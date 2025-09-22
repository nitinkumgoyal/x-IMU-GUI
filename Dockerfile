# STM32 Build Environment Docker Image
# Based on Ubuntu 22.04 with ARM GNU Toolchain and STM32 tools

FROM ubuntu:22.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Install basic dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    wget \
    curl \
    unzip \
    python3 \
    python3-pip \
    libusb-1.0-0-dev \
    libncurses5 \
    libc6-dev-i386 \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /opt

# Download and install ARM GNU Toolchain (latest LTS version)
ARG ARM_TOOLCHAIN_VERSION=13.2.Rel1
RUN wget -q https://developer.arm.com/-/media/Files/downloads/gnu/${ARM_TOOLCHAIN_VERSION}/binrel/arm-gnu-toolchain-${ARM_TOOLCHAIN_VERSION}-x86_64-arm-none-eabi.tar.xz \
    && tar -xf arm-gnu-toolchain-${ARM_TOOLCHAIN_VERSION}-x86_64-arm-none-eabi.tar.xz \
    && rm arm-gnu-toolchain-${ARM_TOOLCHAIN_VERSION}-x86_64-arm-none-eabi.tar.xz \
    && mv arm-gnu-toolchain-${ARM_TOOLCHAIN_VERSION}-x86_64-arm-none-eabi arm-toolchain

# Add ARM toolchain to PATH
ENV PATH="/opt/arm-toolchain/bin:${PATH}"

# Install OpenOCD for debugging/flashing (optional)
RUN apt-get update && apt-get install -y openocd && rm -rf /var/lib/apt/lists/*

# Install STM32 utilities
RUN pip3 install --no-cache-dir \
    stm32pio \
    pyocd

# Download and install STM32CubeMX (for code generation if needed)
# Note: This requires accepting ST's license agreement
RUN mkdir -p /opt/stm32cubemx && \
    cd /opt/stm32cubemx && \
    wget -q https://sw-center.st.com/packs/resource/library/stm32cube_mx_v6100-lin.zip || echo "STM32CubeMX download requires manual intervention"

# Create workspace directory
WORKDIR /workspace

# Verify installation
RUN arm-none-eabi-gcc --version && \
    arm-none-eabi-g++ --version && \
    cmake --version && \
    make --version

# Set default command
CMD ["/bin/bash"]