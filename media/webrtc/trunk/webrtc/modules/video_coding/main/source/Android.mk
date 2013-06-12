# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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
LOCAL_MODULE := libwebrtc_video_coding
LOCAL_MODULE_TAGS := optional
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES := \
    codec_database.cc \
    codec_timer.cc \
    content_metrics_processing.cc \
    decoding_state.cc \
    encoded_frame.cc \
    frame_buffer.cc \
    generic_decoder.cc \
    generic_encoder.cc \
    inter_frame_delay.cc \
    jitter_buffer.cc \
    jitter_buffer_common.cc \
    jitter_estimator.cc \
    media_opt_util.cc \
    media_optimization.cc \
    packet.cc \
    qm_select.cc \
    receiver.cc \
    rtt_filter.cc \
    session_info.cc \
    timestamp_extrapolator.cc \
    timestamp_map.cc \
    timing.cc \
    video_coding_impl.cc

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../../codecs/interface \
    $(LOCAL_PATH)/../../codecs/i420/main/interface \
    $(LOCAL_PATH)/../../codecs/vp8/main/interface \
    $(LOCAL_PATH)/../../../interface \
    $(LOCAL_PATH)/../../../.. \
    $(LOCAL_PATH)/../../../../common_video/vplib/main/interface \
    $(LOCAL_PATH)/../../../../common_video/interface \
    $(LOCAL_PATH)/../../utility/include \
    $(LOCAL_PATH)/../../../../system_wrappers/interface 

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libstlport

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_STATIC_LIBRARY)
