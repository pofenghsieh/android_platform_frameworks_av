LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
        wfd_param_test.cpp              \

LOCAL_SHARED_LIBRARIES:= \
        libstagefright                  \
        libstagefright_foundation       \
        libstagefright_wfd              \
        libutils                        \
        libcutils                       \

LOCAL_C_INCLUDES:= \
        $(TOP)/hardware/ti/domx/omx_core/inc \

LOCAL_MODULE:= wfd_param_test

LOCAL_MODULE_TAGS := debug

include $(BUILD_EXECUTABLE)
