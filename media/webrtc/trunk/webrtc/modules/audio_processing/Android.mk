# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../../android-webrtc.mk

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libwebrtc_apm
LOCAL_MODULE_TAGS := optional
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES := \
    $(call all-proto-files-under, .) \
    audio_buffer.cc \
    audio_processing_impl.cc \
    echo_cancellation_impl.cc \
    echo_control_mobile_impl.cc \
    gain_control_impl.cc \
    high_pass_filter_impl.cc \
    level_estimator_impl.cc \
    noise_suppression_impl.cc \
    splitting_filter.cc \
    processing_component.cc \
    voice_detection_impl.cc

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS) \
    '-DWEBRTC_NS_FIXED' \
    '-DWEBRTC_ANDROID_PLATFORM_BUILD' \
    '-DWEBRTC_AUDIOPROC_DEBUG_DUMP'
#   floating point
#   -DWEBRTC_NS_FLOAT'

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/aec/include \
    $(LOCAL_PATH)/aecm/include \
    $(LOCAL_PATH)/agc/include \
    $(LOCAL_PATH)/ns/include \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../.. \
    $(LOCAL_PATH)/../../common_audio/signal_processing/include \
    $(LOCAL_PATH)/../../common_audio/vad/include \
    $(LOCAL_PATH)/../../system_wrappers/interface \
    external/protobuf/src \
    external/webrtc

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libstlport

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_STATIC_LIBRARY)

# apm process test app

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES:= \
    $(call all-proto-files-under, .) \
    test/process_test.cc

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS) \
    '-DWEBRTC_ANDROID_PLATFORM_BUILD' \
    '-DWEBRTC_AUDIOPROC_DEBUG_DUMP'

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../.. \
    $(LOCAL_PATH)/../../system_wrappers/interface \
    external/gtest/include \
    external/webrtc

LOCAL_STATIC_LIBRARIES := \
    libgtest \
    libwebrtc_test_support \
    libprotobuf-cpp-2.3.0-lite

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libstlport \
    libwebrtc_audio_preprocessing

LOCAL_MODULE:= webrtc_audioproc

ifdef NDK_ROOT
include $(BUILD_EXECUTABLE)
else
include external/stlport/libstlport.mk
include $(BUILD_NATIVE_TEST)
endif

# apm unit test app

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES:= \
    $(call all-proto-files-under, test) \
    test/audio_processing_unittest.cc

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS) \
    '-DWEBRTC_AUDIOPROC_FIXED_PROFILE' \
    '-DWEBRTC_ANDROID_PLATFORM_BUILD' \
    '-DWEBRTC_AUDIOPROC_DEBUG_DUMP'

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../.. \
    $(LOCAL_PATH)/../../../test \
    $(LOCAL_PATH)/../../system_wrappers/interface \
    $(LOCAL_PATH)/../../common_audio/signal_processing/include \
    external/gtest/include \
    external/protobuf/src \
    external/webrtc

LOCAL_STATIC_LIBRARIES := \
    libgtest \
    libwebrtc_test_support \
    libprotobuf-cpp-2.3.0-lite

LOCAL_SHARED_LIBRARIES := \
    libstlport \
    libwebrtc_audio_preprocessing

LOCAL_MODULE:= webrtc_audioproc_unittest

ifdef NDK_ROOT
include $(BUILD_EXECUTABLE)
else
include external/stlport/libstlport.mk
include $(BUILD_NATIVE_TEST)
endif
