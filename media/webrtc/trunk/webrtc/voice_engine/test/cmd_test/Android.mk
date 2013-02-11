#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH:= $(call my-dir)

# voice engine test app

include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../../../../android-webrtc.mk

LOCAL_MODULE_TAGS := tests
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES:= \
    voe_cmd_test.cc

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    '-DWEBRTC_TARGET_PC' \
    '-DWEBRTC_ANDROID' \
    '-DDEBUG'

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../../interface \
    $(LOCAL_PATH)/../../../.. \
    $(LOCAL_PATH)/../../../../.. \
    external/gtest/include \
    frameworks/base/include

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libmedia \
    libcamera_client \
    libgui \
    libhardware \
    libandroid_runtime \
    libbinder

#libwilhelm.so libDunDef-Android.so libbinder.so libsystem_server.so 

LOCAL_MODULE:= webrtc_voe_cmd

ifdef NDK_ROOT
LOCAL_SHARED_LIBRARIES += \
    libstlport_shared \
    libwebrtc-voice-jni \
    libwebrtc_audio_preprocessing
include $(BUILD_EXECUTABLE)
else
LOCAL_SHARED_LIBRARIES += \
    libstlport \
    libwebrtc
include external/stlport/libstlport.mk
include $(BUILD_NATIVE_TEST)
endif
