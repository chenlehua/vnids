# VNIDS Makefile - Android Build System
# Focused on Android ARM64 target with Docker-based testing

.PHONY: all clean build suricata test test-unit test-integration \
        docker-build docker-test docker-shell emulator-start emulator-stop \
        push deploy help

# Configuration
BUILD_DIR ?= build-android
ANDROID_NDK ?= $(ANDROID_NDK_HOME)
ANDROID_ABI ?= arm64-v8a
ANDROID_PLATFORM ?= android-31
ANDROID_API ?= 31

# Docker settings
DOCKER_IMAGE ?= vnids-android-test
DOCKER_TAG ?= latest
EMULATOR_NAME ?= vnids-test-avd

# Output directories
OUT_DIR := out/android-arm64
SURICATA_OUT := suricata/out/android-arm64

# Colors
GREEN := \033[0;32m
YELLOW := \033[1;33m
RED := \033[0;31m
NC := \033[0m

#=============================================================================
# Main Targets
#=============================================================================

all: build suricata
	@echo "$(GREEN)[DONE]$(NC) All Android components built"

build: check-ndk
	@echo "$(GREEN)[BUILD]$(NC) Building vnidsd and vnids-cli for Android ARM64..."
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. \
		-DCMAKE_TOOLCHAIN_FILE=$(ANDROID_NDK)/build/cmake/android.toolchain.cmake \
		-DANDROID_ABI=$(ANDROID_ABI) \
		-DANDROID_PLATFORM=$(ANDROID_PLATFORM) \
		-DCMAKE_BUILD_TYPE=Release
	@cd $(BUILD_DIR) && cmake --build . --parallel
	@mkdir -p $(OUT_DIR)/bin
	@cp $(BUILD_DIR)/vnidsd/vnidsd $(OUT_DIR)/bin/ 2>/dev/null || true
	@cp $(BUILD_DIR)/vnids-cli/vnids-cli $(OUT_DIR)/bin/ 2>/dev/null || true
	@echo "$(GREEN)[BUILD]$(NC) Build complete: $(OUT_DIR)/bin/"

suricata: check-ndk
	@echo "$(GREEN)[BUILD]$(NC) Building Suricata for Android ARM64..."
	@./suricata/scripts/build-android.sh
	@echo "$(GREEN)[BUILD]$(NC) Suricata build complete: $(SURICATA_OUT)/bin/"

#=============================================================================
# Testing Targets
#=============================================================================

test: test-unit test-integration
	@echo "$(GREEN)[TEST]$(NC) All tests completed"

test-unit:
	@echo "$(GREEN)[TEST]$(NC) Running unit tests..."
	@cd tests && python3 -m pytest unit/ -v --tb=short

test-integration: docker-test
	@echo "$(GREEN)[TEST]$(NC) Integration tests completed"

# Docker-based testing with Android emulator
docker-build:
	@echo "$(GREEN)[DOCKER]$(NC) Building test environment image..."
	@docker build -t $(DOCKER_IMAGE):$(DOCKER_TAG) -f tests/docker/Dockerfile .

docker-test: docker-build build suricata
	@echo "$(GREEN)[DOCKER]$(NC) Running tests in Docker container..."
	@docker run --rm -it \
		--privileged \
		-v $(PWD):/workspace \
		-v $(ANDROID_NDK):/android-ndk:ro \
		-e ANDROID_NDK=/android-ndk \
		$(DOCKER_IMAGE):$(DOCKER_TAG) \
		/workspace/tests/scripts/run-android-tests.sh

docker-shell: docker-build
	@echo "$(GREEN)[DOCKER]$(NC) Opening interactive shell..."
	@docker run --rm -it \
		--privileged \
		-v $(PWD):/workspace \
		-v $(ANDROID_NDK):/android-ndk:ro \
		-e ANDROID_NDK=/android-ndk \
		$(DOCKER_IMAGE):$(DOCKER_TAG) \
		/bin/bash

# Emulator management (for local testing)
emulator-start:
	@echo "$(GREEN)[EMU]$(NC) Starting Android emulator..."
	@tests/scripts/emulator-start.sh $(EMULATOR_NAME)

emulator-stop:
	@echo "$(GREEN)[EMU]$(NC) Stopping Android emulator..."
	@adb emu kill 2>/dev/null || true

#=============================================================================
# Deployment Targets
#=============================================================================

push: build suricata
	@echo "$(GREEN)[PUSH]$(NC) Pushing binaries to device..."
	@adb wait-for-device
	@adb root || true
	@adb remount || true
	@adb push $(OUT_DIR)/bin/vnidsd /data/local/tmp/
	@adb push $(SURICATA_OUT)/bin/suricata /data/local/tmp/
	@adb shell chmod 755 /data/local/tmp/vnidsd
	@adb shell chmod 755 /data/local/tmp/suricata
	@echo "$(GREEN)[PUSH]$(NC) Binaries pushed to /data/local/tmp/"

deploy: push
	@echo "$(GREEN)[DEPLOY]$(NC) Deploying to system partition..."
	@adb shell "mount -o remount,rw /system" || true
	@adb shell "mkdir -p /data/vnids/bin /data/vnids/etc /data/vnids/rules /data/vnids/var"
	@adb push $(OUT_DIR)/bin/vnidsd /system/bin/
	@adb push $(SURICATA_OUT)/bin/suricata /data/vnids/bin/
	@adb push deploy/android/vnids.conf /data/vnids/etc/
	@adb push suricata/rules/baseline/*.rules /data/vnids/rules/
	@echo "$(GREEN)[DEPLOY]$(NC) Deployment complete"

#=============================================================================
# Utility Targets
#=============================================================================

check-ndk:
	@if [ -z "$(ANDROID_NDK)" ]; then \
		echo "$(RED)[ERROR]$(NC) ANDROID_NDK or ANDROID_NDK_HOME not set"; \
		exit 1; \
	fi
	@if [ ! -d "$(ANDROID_NDK)" ]; then \
		echo "$(RED)[ERROR]$(NC) Android NDK not found: $(ANDROID_NDK)"; \
		exit 1; \
	fi

clean:
	@echo "$(YELLOW)[CLEAN]$(NC) Removing build artifacts..."
	@rm -rf $(BUILD_DIR) build out
	@rm -rf suricata/build-android suricata/out
	@rm -rf tests/__pycache__ tests/.pytest_cache
	@echo "$(YELLOW)[CLEAN]$(NC) Clean complete"

clean-docker:
	@echo "$(YELLOW)[CLEAN]$(NC) Removing Docker images..."
	@docker rmi $(DOCKER_IMAGE):$(DOCKER_TAG) 2>/dev/null || true

format:
	@echo "$(GREEN)[FORMAT]$(NC) Formatting source code..."
	@find vnidsd vnids-cli shared -name "*.c" -o -name "*.h" | xargs clang-format -i 2>/dev/null || true

help:
	@echo "VNIDS Android Build System"
	@echo ""
	@echo "$(GREEN)Build Targets:$(NC)"
	@echo "  all              - Build everything (vnidsd, vnids-cli, suricata)"
	@echo "  build            - Build vnidsd and vnids-cli for Android ARM64"
	@echo "  suricata         - Build Suricata for Android ARM64"
	@echo ""
	@echo "$(GREEN)Test Targets:$(NC)"
	@echo "  test             - Run all tests (unit + integration)"
	@echo "  test-unit        - Run unit tests only"
	@echo "  test-integration - Run integration tests in Docker"
	@echo "  docker-build     - Build Docker test environment"
	@echo "  docker-test      - Run tests in Docker container"
	@echo "  docker-shell     - Open interactive Docker shell"
	@echo ""
	@echo "$(GREEN)Emulator Targets:$(NC)"
	@echo "  emulator-start   - Start Android emulator"
	@echo "  emulator-stop    - Stop Android emulator"
	@echo ""
	@echo "$(GREEN)Deploy Targets:$(NC)"
	@echo "  push             - Push binaries to device via adb"
	@echo "  deploy           - Full deployment to system partition"
	@echo ""
	@echo "$(GREEN)Utility Targets:$(NC)"
	@echo "  clean            - Remove all build artifacts"
	@echo "  clean-docker     - Remove Docker images"
	@echo "  format           - Format source code"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "$(GREEN)Variables:$(NC)"
	@echo "  ANDROID_NDK      - Android NDK path (default: \$$ANDROID_NDK_HOME)"
	@echo "  ANDROID_ABI      - Target ABI (default: arm64-v8a)"
	@echo "  ANDROID_PLATFORM - Target platform (default: android-31)"
	@echo "  DOCKER_IMAGE     - Docker image name (default: vnids-android-test)"
