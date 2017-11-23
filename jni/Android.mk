LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := com_nativeflow_root_Root
LOCAL_SRC_FILES := com_nativeflow_root_Root.c client.c root.c kernel_rw.c restore_execution.c
LOCAL_LDLIBS += -llog

include $(BUILD_SHARED_LIBRARY)
