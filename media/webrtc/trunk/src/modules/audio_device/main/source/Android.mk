# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../../../../android-webrtc.mk

LOCAL_ARM_MODE := arm
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE := libwebrtc_audio_device
LOCAL_MODULE_TAGS := optional
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES := \
    audio_device_buffer.cc \
    audio_device_generic.cc \
    audio_device_utility.cc \
    audio_device_impl.cc \
    android/audio_device_android_opensles.cc \
    android/audio_device_utility_android.cc \
    dummy/audio_device_utility_dummy.cc \
    dummy/audio_device_dummy.cc

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS) \
   '-DWEBRTC_ANDROID_OPENSLES'

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/android \
    $(LOCAL_PATH)/dummy \
    $(LOCAL_PATH)/linux \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../../../.. \
    $(LOCAL_PATH)/../../../interface \
    $(LOCAL_PATH)/../../../../common_audio/resampler/include \
    $(LOCAL_PATH)/../../../../common_audio/signal_processing/include \
    $(LOCAL_PATH)/../../../../system_wrappers/interface \
    system/media/wilhelm/include

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libstlport \
    libOpenSLES

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_STATIC_LIBRARY)
