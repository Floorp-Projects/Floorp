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
LOCAL_MODULE := libwebrtc_audio_coding
LOCAL_MODULE_TAGS := optional
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES := \
    acm_cng.cc \
    acm_codec_database.cc \
    acm_dtmf_detection.cc \
    acm_dtmf_playout.cc \
    acm_g722.cc \
    acm_generic_codec.cc \
    acm_ilbc.cc \
    acm_isac.cc \
    acm_neteq.cc \
    acm_pcm16b.cc \
    acm_pcma.cc \
    acm_pcmu.cc \
    acm_red.cc \
    acm_resampler.cc \
    audio_coding_module.cc \
    audio_coding_module_impl.cc

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../../codecs/cng/include \
    $(LOCAL_PATH)/../../codecs/g711/include \
    $(LOCAL_PATH)/../../codecs/g722/include \
    $(LOCAL_PATH)/../../codecs/ilbc/interface \
    $(LOCAL_PATH)/../../codecs/iSAC/main/interface \
    $(LOCAL_PATH)/../../codecs/iSAC/fix/interface \
    $(LOCAL_PATH)/../../codecs/pcm16b/include \
    $(LOCAL_PATH)/../../neteq/interface \
    $(LOCAL_PATH)/../../../.. \
    $(LOCAL_PATH)/../../../interface \
    $(LOCAL_PATH)/../../../../common_audio/resampler/include \
    $(LOCAL_PATH)/../../../../common_audio/signal_processing/include \
    $(LOCAL_PATH)/../../../../common_audio/vad/include \
    $(LOCAL_PATH)/../../../../system_wrappers/interface 

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libstlport

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_STATIC_LIBRARY)
