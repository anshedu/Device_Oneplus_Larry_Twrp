#
# Copyright (C) 2025 The Android Open Source Project
#
# SPDX-License-Identifier: Apache-2.0
#
DEVICE_PATH := device/larry/boot

# For building with minimal manifest
ALLOW_MISSING_DEPENDENCIES := true

# Architecture
TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_VARIANT := generic
TARGET_CPU_VARIANT_RUNTIME := kryo300

TARGET_2ND_ARCH := arm
TARGET_2ND_ARCH_VARIANT := armv8-a
TARGET_2ND_CPU_ABI := armeabi-v7a
TARGET_2ND_CPU_ABI2 := armeabi
TARGET_2ND_CPU_VARIANT := generic
TARGET_2ND_CPU_VARIANT_RUNTIME := cortex-a75

# Bootloader
TARGET_BOOTLOADER_BOARD_NAME := boot

# Display
TARGET_SCREEN_DENSITY := 450

# Kernel
BOARD_BOOT_HEADER_VERSION := 3
BOARD_KERNEL_BASE := 0x00000000
BOARD_KERNEL_CMDLINE := \
    androidboot.hardware=qcom \
    androidboot.memcg=1 \
    androidboot.usbcontroller=4e00000.dwc3 \
    cgroup.memory=nokmem,nosocket \
    loop.max_part=7 \
    lpm_levels.sleep_disabled=1 \
    msm_rtb.filter=0x237 \
    reboot=panic_warm \
    service_locator.enable=1 \
    swiotlb=0 \
    iptable_raw.raw_before_defrag=1 \
    ip6table_raw.raw_before_defrag=1
BOARD_KERNEL_IMAGE_NAME := Image
BOARD_KERNEL_PAGESIZE := 4096
BOARD_KERNEL_SEPARATED_DTBO := true
BOARD_MKBOOTIMG_ARGS += --header_version $(BOARD_BOOT_HEADER_VERSION)
BOARD_RAMDISK_USE_LZ4 := true
TARGET_KERNEL_SOURCE := kernel/oneplus/sm6375
TARGET_KERNEL_CONFIG := vendor/holi-qgki_defconfig vendor/debugfs.config
TARGET_KERNEL_NO_GCC := true

# Partitions
BOARD_PRODUCTIMAGE_MINIMAL_PARTITION_RESERVED_SIZE := false
-include vendor/lineage/config/BoardConfigReservedSize.mk
BOARD_BOOTIMAGE_PARTITION_SIZE := 167772160
BOARD_DTBOIMG_PARTITION_SIZE := 25165824
BOARD_USERDATAIMAGE_PARTITION_SIZE := 113600311296
BOARD_VENDOR_BOOTIMAGE_PARTITION_SIZE := 167772160
BOARD_ODMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_PRODUCTIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_SYSTEM_EXTIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_VENDOR_DLKMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_ONEPLUS_DYNAMIC_PARTITIONS_PARTITION_LIST := odm product system system_ext vendor vendor_dlkm
BOARD_SUPER_PARTITION_GROUPS := oneplus_dynamic_partitions
BOARD_FLASH_BLOCK_SIZE := 262144 # (BOARD_KERNEL_PAGESIZE * 64)
TARGET_COPY_OUT_ODM := odm
TARGET_COPY_OUT_PRODUCT := product
TARGET_COPY_OUT_SYSTEM_EXT := system_ext
TARGET_COPY_OUT_VENDOR := vendor
TARGET_COPY_OUT_VENDOR_DLKM := vendor_dlkm

# Platform
TARGET_BOARD_PLATFORM := holi

# Recovery
BOARD_INCLUDE_DTB_IN_BOOTIMG := true
BOARD_MOVE_RECOVERY_RESOURCES_TO_VENDOR_BOOT := true
TARGET_RECOVERY_FSTAB := $(COMMON_PATH)/init/fstab.default
TARGET_RECOVERY_PIXEL_FORMAT := RGBX_8888
TARGET_USERIMAGES_USE_EXT4 := true
TARGET_USERIMAGES_USE_F2FS := true
