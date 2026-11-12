# Use the Android 14 System SDK for this bring-up image. The stock vendor
# shipped with API 35, so leave PRODUCT_SHIPPING_API_LEVEL unset here.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit_only.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/base.mk)
$(call inherit-product, vendor/twrp/config/common.mk)

PRODUCT_NAME := twrp_pdx256
PRODUCT_BUILD_RECOVERY_IMAGE := true
PRODUCT_DEVICE := pdx256
PRODUCT_BRAND := Sony
PRODUCT_MODEL := XQ-FS72
PRODUCT_MANUFACTURER := Sony

# Required by the Sony ramdisk recipe, including on a fresh product output.
PRODUCT_PACKAGES += \
    xperia_recovery_service_trace.recovery \
    xperia_recovery_backup_sample \
    xperia_backup_volumes
