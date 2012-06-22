# Android makefile for VideoCapture Module

LOCAL_PATH := $(call my-dir)

WEBRTC_INTERFACES_PATH := $(LOCAL_PATH)/../../../../../../../build/interface
WEBRTC_LIBS_PATH := $(LOCAL_PATH)/../../../../../../../build/libraries

include $(CLEAR_VARS)

LOCAL_MODULE     := VideoCaptureModuleAndroidTestJniAPI
LOCAL_SRC_FILES  := video_capture_module_android_test_jni.cc
LOCAL_CFLAGS     := -DWEBRTC_TARGET_PC # For typedefs.h
LOCAL_C_INCLUDES := $(WEBRTC_INTERFACES_PATH)
LOCAL_LDLIBS     := \
    $(WEBRTC_LIBS_PATH)/testVideoCaptureAPI_android_gcc.a \
    $(WEBRTC_LIBS_PATH)/VideoCaptureModuleTestApiLib_android_gcc.a \
    -llog -lgcc -lGLESv2
include $(BUILD_SHARED_LIBRARY)

