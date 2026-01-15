# VNIDS Android Test Environment
# Docker image with Android emulator for integration testing

FROM ubuntu:22.04

LABEL maintainer="VNIDS Authors"
LABEL description="Android emulator environment for VNIDS integration testing"

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Android SDK settings
ENV ANDROID_SDK_ROOT=/opt/android-sdk
ENV ANDROID_HOME=/opt/android-sdk
ENV PATH="${PATH}:${ANDROID_SDK_ROOT}/cmdline-tools/latest/bin:${ANDROID_SDK_ROOT}/platform-tools:${ANDROID_SDK_ROOT}/emulator"

# Install dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    # Base tools
    ca-certificates \
    curl \
    wget \
    unzip \
    git \
    # Build tools
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    # Python for tests
    python3 \
    python3-pip \
    python3-venv \
    # Android emulator dependencies
    openjdk-11-jdk-headless \
    libx11-6 \
    libpulse0 \
    libgl1 \
    libnss3 \
    libxcomposite1 \
    libxcursor1 \
    libxi6 \
    libxtst6 \
    libglu1-mesa \
    # KVM for emulator acceleration
    qemu-kvm \
    libvirt-daemon-system \
    # Network tools for testing
    tcpdump \
    tcpreplay \
    iproute2 \
    iptables \
    # Utilities
    procps \
    htop \
    vim \
    && rm -rf /var/lib/apt/lists/*

# Install Android command line tools
RUN mkdir -p ${ANDROID_SDK_ROOT}/cmdline-tools && \
    cd ${ANDROID_SDK_ROOT}/cmdline-tools && \
    wget -q https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip -O cmdline-tools.zip && \
    unzip -q cmdline-tools.zip && \
    mv cmdline-tools latest && \
    rm cmdline-tools.zip

# Accept licenses and install SDK components
RUN yes | sdkmanager --licenses > /dev/null 2>&1 || true && \
    sdkmanager --update && \
    sdkmanager \
        "platform-tools" \
        "platforms;android-31" \
        "system-images;android-31;google_apis;arm64-v8a" \
        "emulator"

# Create AVD for testing
RUN echo "no" | avdmanager create avd \
    --name vnids-test-avd \
    --package "system-images;android-31;google_apis;arm64-v8a" \
    --device "pixel_4" \
    --force

# Install Python test dependencies
COPY tests/requirements.txt /tmp/requirements.txt
RUN pip3 install --no-cache-dir -r /tmp/requirements.txt || \
    pip3 install --no-cache-dir pytest pytest-timeout scapy dpkt requests pyyaml

# Create working directories
RUN mkdir -p /workspace /data/vnids

# Copy test configuration
COPY tests/docker/emulator-config.ini /root/.android/avd/vnids-test-avd.avd/config.ini

WORKDIR /workspace

# Healthcheck
HEALTHCHECK --interval=30s --timeout=10s --retries=3 \
    CMD adb devices | grep -q "emulator" || exit 1

# Default command
CMD ["/bin/bash"]
