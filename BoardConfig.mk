# Recovery target for XQ-FS72 stock 71.2.A.2.35; Sony FBE integration is in patches/.
DEVICE_PATH := device/sony/pdx256

TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_VARIANT := generic
TARGET_BOARD_PLATFORM := sun
TARGET_BOOTLOADER_BOARD_NAME := XQ-FS
TARGET_NO_BOOTLOADER := true

TARGET_NO_KERNEL := true
BOARD_BOOT_HEADER_VERSION := 4
BOARD_KERNEL_PAGESIZE := 4096
BOARD_MKBOOTIMG_ARGS := --header_version $(BOARD_BOOT_HEADER_VERSION)
BOARD_RAMDISK_USE_LZ4 := true
BOARD_USES_GENERIC_KERNEL_IMAGE := true
BOARD_EXCLUDE_KERNEL_FROM_RECOVERY_IMAGE := true
BOARD_USES_RECOVERY_AS_BOOT := false
BOARD_MOVE_RECOVERY_RESOURCES_TO_VENDOR_BOOT := false
AB_OTA_UPDATER := true
AB_OTA_PARTITIONS := boot init_boot vendor_boot recovery dtbo vbmeta vbmeta_system

BOARD_BOOTIMAGE_PARTITION_SIZE := 100663296
BOARD_INIT_BOOT_IMAGE_PARTITION_SIZE := 8388608
BOARD_VENDOR_BOOTIMAGE_PARTITION_SIZE := 100663296
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 104857600
BOARD_DTBOIMG_PARTITION_SIZE := 52428800
TARGET_COPY_OUT_VENDOR := vendor

# The initial image is for offline inspection. Do not alter device AVB settings.
BOARD_AVB_ENABLE := false
TARGET_RECOVERY_FSTAB := $(DEVICE_PATH)/recovery.fstab
# Do not import userdata and metadata from the vendor fstab during bring-up.
TW_SKIP_ADDITIONAL_FSTAB := true
# This device uses file-based encryption and has no legacy FDE footer.
TW_EXCLUDE_FDE := true
# Match Sony's recovery ro.minui.pixel_format.
TARGET_RECOVERY_PIXEL_FORMAT := RGBX_8888
TW_THEME := portrait_hdpi
# TWRP 14.1 copies this file even when crypto is disabled.
TARGET_RECOVERY_DEVICE_MODULES += task_profiles.json
TW_INCLUDE_CRYPTO := true

# Qualcomm CPU subsystem thermal sensor, resolved by type.
TW_CUSTOM_CPU_TEMP_SENSOR := cpuss-0-0

TW_USE_SERIALNO_PROPERTY_FOR_DEVICE_ID := true

TW_BRIGHTNESS_PATH := /sys/class/backlight/panel0-backlight/brightness

# Match the user-selected 10% brightness on this 4095-step panel.
TW_DEFAULT_BRIGHTNESS := 409

# Sony security services keep vendor libraries mapped during recovery.
TW_USES_VENDOR_LIBS := true

# Internal storage is emulated Data media, including while CE is locked.
RECOVERY_SDCARD_ON_DATA := true

TW_EXTRA_LANGUAGES := true

TW_INCLUDE_FASTBOOTD := true

TARGET_RECOVERY_QCOM_RTC_FIX := true
