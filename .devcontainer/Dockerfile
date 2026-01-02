FROM ubuntu:22.04

ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=1000

ENV DEBIAN_FRONTEND=noninteractive
ENV IDF_PATH=/opt/esp-idf

# ---- system deps ----
RUN apt-get update && apt-get install -y \
    git \
    curl \
    wget \
    ca-certificates \
    flex \
    bison \
    gperf \
    python3 \
    python3-pip \
    python3-venv \
    python-is-python3 \
    cmake \
    ninja-build \
    ccache \
    libffi-dev \
    libssl-dev \
    dfu-util \
    usbutils \
    udev \
    bash \
    && rm -rf /var/lib/apt/lists/*

# Add udev rule for ESP32 devices
RUN echo 'KERNEL=="ttyACM*", MODE="0666", GROUP="dialout"' > /etc/udev/rules.d/99-esp32.rules && \
    echo 'KERNEL=="ttyUSB*", MODE="0666", GROUP="dialout"' >> /etc/udev/rules.d/99-esp32.rules

# ---- non-root user with sudo support ----
RUN apt-get update && apt-get install -y sudo \
    && groupadd --gid ${USER_GID} ${USERNAME} \
    && useradd --uid ${USER_UID} --gid ${USER_GID} -m ${USERNAME} \
    && usermod -aG dialout ${USERNAME} \
    && usermod -aG sudo ${USERNAME} \
    && echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# Create ESP-IDF directory and set ownership before switching to non-root user
RUN mkdir -p ${IDF_PATH} && chown -R ${USERNAME}:${USERNAME} ${IDF_PATH}

USER ${USERNAME}
WORKDIR /workspaces/dimming-light-switch

# ---- ESP-IDF source ----
RUN git clone --branch v5.2 --depth 1 https://github.com/espressif/esp-idf.git ${IDF_PATH}

# ---- ESP-IDF install ----
RUN ${IDF_PATH}/tools/idf_tools.py install --targets esp32c6 \
    && ${IDF_PATH}/tools/idf_tools.py install-python-env

# Make ESP-IDF environment available in shells
RUN echo ". ${IDF_PATH}/export.sh" >> ~/.bashrc
