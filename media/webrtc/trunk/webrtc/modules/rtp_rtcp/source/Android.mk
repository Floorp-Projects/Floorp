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

LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE := libwebrtc_rtp_rtcp
LOCAL_MODULE_TAGS := optional
LOCAL_CPP_EXTENSION := .cc
LOCAL_GENERATED_SOURCES :=
LOCAL_SRC_FILES := \
    bitrate.cc \
    rtp_rtcp_impl.cc \
    rtcp_receiver.cc \
    rtcp_receiver_help.cc \
    rtcp_sender.cc \
    rtcp_utility.cc \
    rtp_receiver.cc \
    rtp_sender.cc \
    rtp_utility.cc \
    rtp_header_extension.cc \
    ssrc_database.cc \
    tmmbr_help.cc \
    dtmf_queue.cc \
    rtp_receiver_audio.cc \
    rtp_sender_audio.cc \
    bandwidth_management.cc \
    forward_error_correction.cc \
    forward_error_correction_internal.cc \
    overuse_detector.cc \
    remote_rate_control.cc \
    rtp_packet_history.cc \
    receiver_fec.cc \
    rtp_receiver_video.cc \
    rtp_sender_video.cc \
    rtp_format_vp8.cc \
    transmission_bucket.cc \
    vp8_partition_aggregator.cc

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../../.. \
    $(LOCAL_PATH)/../../interface \
    $(LOCAL_PATH)/../../../system_wrappers/interface 

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libstlport

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_STATIC_LIBRARY)
