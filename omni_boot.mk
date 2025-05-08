#
# Copyright (C) 2025 The Android Open Source Project
#
# SPDX-License-Identifier: Apache-2.0
#
# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)

# Inherit some common Omni stuff.
$(call inherit-product, vendor/omni/config/common.mk)

# Inherit from boot device
$(call inherit-product, device/generic/boot/device.mk)

PRODUCT_DEVICE := boot
PRODUCT_NAME := omni_boot
PRODUCT_BRAND := Oneplus
PRODUCT_MODEL := larry
PRODUCT_MANUFACTURER := Oneplus

PRODUCT_GMS_CLIENTID_BASE := android-generic

PRODUCT_BUILD_PROP_OVERRIDES += \
    PRIVATE_BUILD_DESC=""

BUILD_FINGERPRINT := 
