LOCAL_PATH := $(call my-dir)

ifeq ($(TARGET_DEVICE),pdx256)
include $(CLEAR_VARS)
LOCAL_MODULE := xperia_recovery_backup_sample
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/system/bin
LOCAL_SRC_FILES := recovery/backup_sample.c
LOCAL_C_INCLUDES := bootable/recovery/libtar external/zlib
LOCAL_SHARED_LIBRARIES := libtar
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := xperia_backup_volumes
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_RECOVERY_ROOT_OUT)/system/bin
LOCAL_SRC_FILES := recovery/backup_volumes.cpp
LOCAL_CPP_STD := c++17
LOCAL_SHARED_LIBRARIES := libcrypto
include $(BUILD_EXECUTABLE)
endif
