#include "zigbee.h"
#include "esp_zigbee_core.h"
#include "esp_zigbee_secur.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "ZIGBEE"

#define ENDPOINT           10
#define MANUFACTURER_NAME  "\x06""kshpin"
#define MODEL_IDENTIFIER   "\x0D""DimmableLight"
#define STEERING_MAX_RETRIES  5
#define STEERING_RETRY_DELAY_MS 5000

static zigbee_on_off_cb_t on_off_callback;
static zigbee_level_cb_t  level_callback;

static bool steering_in_progress = false;
static int steering_retry_count = 0;
static void steering_cb(uint8_t param);

static void configure_reporting(void)
{
    esp_zb_zcl_reporting_info_t on_off_report = {
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .ep = ENDPOINT,
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
        .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        .attr_id = ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
        .u.send_info.min_interval = 0,
        .u.send_info.max_interval = 300,
        .u.send_info.delta.u8 = 1,
        .u.send_info.def_min_interval = 0,
        .u.send_info.def_max_interval = 300,
        .dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC,
    };
    esp_zb_zcl_update_reporting_info(&on_off_report);

    esp_zb_zcl_reporting_info_t level_report = {
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .ep = ENDPOINT,
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL,
        .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        .attr_id = ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID,
        .u.send_info.min_interval = 1,
        .u.send_info.max_interval = 300,
        .u.send_info.delta.u8 = 1,
        .u.send_info.def_min_interval = 1,
        .u.send_info.def_max_interval = 300,
        .dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC,
    };
    esp_zb_zcl_update_reporting_info(&level_report);
}

static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    if (message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
        return ESP_ERR_INVALID_ARG;
    }

    if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
        if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID) {
            bool on = *(bool *)message->attribute.data.value;
            if (on_off_callback) on_off_callback(on);
            ESP_LOGI(TAG, "On/Off set to %s", on ? "ON" : "OFF");
        }
    } else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL) {
        if (message->attribute.id == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID) {
            uint8_t level = *(uint8_t *)message->attribute.data.value;
            if (level_callback) level_callback(level);
            ESP_LOGI(TAG, "Level set to %d", level);
        }
    }

    return ESP_OK;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        return zb_attribute_handler((const esp_zb_zcl_set_attr_value_message_t *)message);
    default:
        ESP_LOGW(TAG, "Unhandled Zigbee action callback: 0x%x", callback_id);
        return ESP_OK;
    }
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;

    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Zigbee stack initialized");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Factory new device, starting steering...");
                steering_in_progress = true;
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device rebooted, already on network");
                configure_reporting();
            }
        } else {
            ESP_LOGW(TAG, "BDB init failed (status: %s), starting steering...",
                     esp_err_to_name(err_status));
            steering_in_progress = true;
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        }
        break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
        steering_in_progress = false;
        if (err_status == ESP_OK) {
            steering_retry_count = 0;
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network (Extended PAN ID: "
                     "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, "
                     "PAN ID: 0x%04hx, Channel: %d)",
                     extended_pan_id[7], extended_pan_id[6],
                     extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2],
                     extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel());
            configure_reporting();
        } else {
            steering_retry_count++;
            ESP_LOGW(TAG, "Steering failed (attempt %d/%d), %s",
                     steering_retry_count, STEERING_MAX_RETRIES,
                     steering_retry_count < STEERING_MAX_RETRIES ? "retrying..." : "giving up");
            if (steering_retry_count < STEERING_MAX_RETRIES) {
                steering_in_progress = true;
                esp_zb_scheduler_alarm(steering_cb, 0, STEERING_RETRY_DELAY_MS);
            }
        }
        break;

    default:
        ESP_LOGI(TAG, "ZDO signal: 0x%x, status: %s", sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}

static void esp_zb_task(void *pvParameters)
{
    esp_zb_cfg_t zb_nwk_cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER,
        .install_code_policy = false,
        .nwk_cfg.zczr_cfg.max_children = 10,
    };
    esp_zb_init(&zb_nwk_cfg);
    esp_zb_set_primary_network_channel_set(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK);
    esp_zb_set_secondary_network_channel_set(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK);
    esp_zb_secur_network_min_join_lqi_set(0);
    esp_zb_set_tx_power(20);

    /* Basic cluster */
    esp_zb_basic_cluster_cfg_t basic_cfg = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = 0x01,
    };
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&basic_cfg);
    esp_zb_basic_cluster_add_attr(basic_cluster,
                                  ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID,
                                  (void *)MANUFACTURER_NAME);
    esp_zb_basic_cluster_add_attr(basic_cluster,
                                  ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID,
                                  (void *)MODEL_IDENTIFIER);

    /* Identify cluster */
    esp_zb_identify_cluster_cfg_t identify_cfg = { .identify_time = 0 };
    esp_zb_attribute_list_t *identify_cluster =
        esp_zb_identify_cluster_create(&identify_cfg);

    /* Groups cluster */
    esp_zb_groups_cluster_cfg_t groups_cfg = { .groups_name_support_id = 0 };
    esp_zb_attribute_list_t *groups_cluster =
        esp_zb_groups_cluster_create(&groups_cfg);

    /* Scenes cluster */
    esp_zb_scenes_cluster_cfg_t scenes_cfg = {
        .scenes_count = 0,
        .current_scene = 0,
        .current_group = 0,
        .scene_valid = false,
        .name_support = 0,
    };
    esp_zb_attribute_list_t *scenes_cluster =
        esp_zb_scenes_cluster_create(&scenes_cfg);

    /* On/Off cluster */
    esp_zb_on_off_cluster_cfg_t on_off_cfg = { .on_off = false };
    esp_zb_attribute_list_t *on_off_cluster =
        esp_zb_on_off_cluster_create(&on_off_cfg);

    /* Level Control cluster */
    esp_zb_level_cluster_cfg_t level_cfg = { .current_level = 254 };
    esp_zb_attribute_list_t *level_cluster =
        esp_zb_level_cluster_create(&level_cfg);

    /* Assemble cluster list */
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster,
                                          ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(cluster_list, identify_cluster,
                                             ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_groups_cluster(cluster_list, groups_cluster,
                                           ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_scenes_cluster(cluster_list, scenes_cluster,
                                           ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_on_off_cluster(cluster_list, on_off_cluster,
                                           ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_level_cluster(cluster_list, level_cluster,
                                          ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

    /* Create endpoint 10 as HA Dimmable Light */
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_DIMMABLE_LIGHT_DEVICE_ID,
        .app_device_version = 0,
    };
    esp_zb_ep_list_add_ep(ep_list, cluster_list, endpoint_config);

    esp_zb_device_register(ep_list);
    esp_zb_core_action_handler_register(zb_action_handler);

    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}

void zigbee_init(zigbee_on_off_cb_t on_off_cb, zigbee_level_cb_t level_cb)
{
    on_off_callback = on_off_cb;
    level_callback  = level_cb;

    esp_zb_platform_config_t config = {
        .radio_config = { .radio_mode = ZB_RADIO_MODE_NATIVE },
        .host_config = { .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE },
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    xTaskCreate(esp_zb_task, "zigbee_main", 8192, NULL, 5, NULL);
}

void zigbee_on_off_updated(bool on)
{
    esp_zb_lock_acquire(portMAX_DELAY);
    uint8_t value = on ? 1 : 0;
    esp_zb_zcl_set_attribute_val(ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF,
                                 ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                                 ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID,
                                 &value, false);
    esp_zb_lock_release();
}

static void steering_cb(uint8_t param)
{
    if (steering_in_progress) {
        ESP_LOGW(TAG, "Steering already in progress, ignoring request");
        return;
    }
    ESP_LOGI(TAG, "Starting network steering...");
    steering_in_progress = true;
    esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
}

void zigbee_start_steering(void)
{
    esp_zb_scheduler_alarm(steering_cb, 0, 0);
}

static void factory_reset_cb(uint8_t param)
{
    ESP_LOGW(TAG, "Factory reset!");
    esp_zb_factory_reset();
}

void zigbee_factory_reset(void)
{
    esp_zb_scheduler_alarm(factory_reset_cb, 0, 0);
}
