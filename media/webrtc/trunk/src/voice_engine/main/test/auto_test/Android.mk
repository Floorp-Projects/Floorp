#  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
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
    automated_mode.cc \
    voe_cpu_test.cc \
    voe_standard_test.cc \
    voe_stress_test.cc \
    voe_unit_test.cc \
    voe_extended_test.cc \
    voe_standard_integration_test.cc

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    '-DWEBRTC_TARGET_PC' \
    '-DWEBRTC_LINUX' \
    '-DWEBRTC_ANDROID'

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../../interface \
    $(LOCAL_PATH)/../../../.. \
    $(LOCAL_PATH)/../../../../modules/audio_device/main/interface \
    $(LOCAL_PATH)/../../../../modules/interface \
    $(LOCAL_PATH)/../../../../system_wrappers/interface \
    $(LOCAL_PATH)/../../../../../test \
    external/gtest/include \

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libstlport \
    libwebrtc

LOCAL_MODULE:= webrtc_voe_autotest

ifdef NDK_ROOT
include $(BUILD_EXECUTABLE)
else
include external/stlport/libstlport.mk
include $(BUILD_NATIVE_TEST)
endif

