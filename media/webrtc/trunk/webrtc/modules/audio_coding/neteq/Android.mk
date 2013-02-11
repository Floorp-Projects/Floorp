# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../../../android-webrtc.mk

LOCAL_ARM_MODE := arm
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE := libwebrtc_neteq
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    accelerate.c \
    automode.c \
    bgn_update.c \
    bufstats_decision.c \
    cng_internal.c \
    codec_db.c \
    correlator.c \
    dsp.c \
    dsp_helpfunctions.c \
    dtmf_buffer.c \
    dtmf_tonegen.c \
    expand.c \
    mcu_address_init.c \
    mcu_dsp_common.c \
    mcu_reset.c \
    merge.c \
    min_distortion.c \
    mix_voice_unvoice.c \
    mute_signal.c \
    normal.c \
    packet_buffer.c \
    peak_detection.c \
    preemptive_expand.c \
    random_vector.c \
    recin.c \
    recout.c \
    rtcp.c \
    rtp.c \
    set_fs.c \
    signal_mcu.c \
    split_and_insert.c \
    unmute_signal.c \
    webrtc_neteq.c

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS) \
    '-DNETEQ_VOICEENGINE_CODECS'

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/interface \
    $(LOCAL_PATH)/../codecs/cng/include \
    $(LOCAL_PATH)/../../.. \
    $(LOCAL_PATH)/../../../common_audio/signal_processing/include 

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libstlport

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_STATIC_LIBRARY)
