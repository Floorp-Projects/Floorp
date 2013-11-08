# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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
LOCAL_MODULE := libwebrtc_voe_core
LOCAL_MODULE_TAGS := optional
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES := \
    audio_frame_operations.cc \
    channel.cc \
    channel_manager.cc \
    dtmf_inband.cc \
    dtmf_inband_queue.cc \
    level_indicator.cc \
    monitor_module.cc \
    output_mixer.cc \
    ref_count.cc \
    shared_data.cc \
    statistics.cc \
    transmit_mixer.cc \
    utility.cc \
    voe_audio_processing_impl.cc \
    voe_base_impl.cc \
    voe_call_report_impl.cc \
    voe_codec_impl.cc \
    voe_dtmf_impl.cc \
    voe_encryption_impl.cc \
    voe_external_media_impl.cc \
    voe_file_impl.cc \
    voe_hardware_impl.cc \
    voe_neteq_stats_impl.cc \
    voe_network_impl.cc \
    voe_rtp_rtcp_impl.cc \
    voe_video_sync_impl.cc \
    voe_volume_control_impl.cc \
    voice_engine_impl.cc

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS) \
   '-DWEBRTC_ANDROID_OPENSLES'

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../../.. \
    $(LOCAL_PATH)/../../../common_audio/resampler/include \
    $(LOCAL_PATH)/../../../common_audio/signal_processing/include \
    $(LOCAL_PATH)/../../../modules/interface \
    $(LOCAL_PATH)/../../../modules/audio_coding/main/interface \
    $(LOCAL_PATH)/../../../modules/audio_conference_mixer/interface \
    $(LOCAL_PATH)/../../../modules/audio_device/main/interface \
    $(LOCAL_PATH)/../../../modules/audio_device/main/source \
    $(LOCAL_PATH)/../../../modules/audio_processing/include \
    $(LOCAL_PATH)/../../../modules/media_file/interface \
    $(LOCAL_PATH)/../../../modules/rtp_rtcp/interface \
    $(LOCAL_PATH)/../../../modules/udp_transport/interface \
    $(LOCAL_PATH)/../../../modules/utility/interface \
    $(LOCAL_PATH)/../../../system_wrappers/interface 

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libstlport 

ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
LOCAL_LDLIBS += -ldl -lpthread
endif

ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_STATIC_LIBRARY)
