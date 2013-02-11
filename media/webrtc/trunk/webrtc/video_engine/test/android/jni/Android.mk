# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH := $(call my-dir)

include $(call all-makefiles-under, $(LOCAL_PATH))

# Specify BUILDTYPE=Release on the command line for a release build.
BUILDTYPE ?= Debug

MY_LIBS_PATH := ../../../../../out/$(BUILDTYPE)/obj.target

include $(CLEAR_VARS)
LOCAL_MODULE := libvoice_engine_core
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/voice_engine/libvoice_engine_core.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libvideo_engine_core
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/video_engine/libvideo_engine_core.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libvideo_processing
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libvideo_processing.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libwebrtc_video_coding
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libwebrtc_video_coding.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libvideo_render_module
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libvideo_render_module.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libvideo_capture_module
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libvideo_capture_module.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libaudio_coding_module
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libaudio_coding_module.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libaudio_processing
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libaudio_processing.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libaudio_processing_neon
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libaudio_processing_neon.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libPCM16B
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libPCM16B.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libCNG
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libCNG.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libNetEq
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libNetEq.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libG722
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libG722.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libiSAC
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libiSAC.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libG711
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libG711.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libiLBC
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libiLBC.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libiSACFix
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libiSACFix.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libisac_neon
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libisac_neon.a
include $(PREBUILT_STATIC_LIBRARY)

# Remove the following file existense check when opus is always enabled.
ifneq ($(wildcard jni/$(MY_LIBS_PATH)/third_party/opus/libopus.a),)
include $(CLEAR_VARS)
LOCAL_MODULE := libopus
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/third_party/opus/libopus.a
include $(PREBUILT_STATIC_LIBRARY)
endif

ifneq ($(wildcard jni/$(MY_LIBS_PATH)/webrtc/modules/libwebrtc_opus.a),)
include $(CLEAR_VARS)
LOCAL_MODULE := libwebrtc_opus
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libwebrtc_opus.a
include $(PREBUILT_STATIC_LIBRARY)
endif

include $(CLEAR_VARS)
LOCAL_MODULE := libvad
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/common_audio/libvad.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libbitrate_controller
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libbitrate_controller.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libresampler
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/common_audio/libresampler.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libsignal_processing
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/common_audio/libsignal_processing.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libsignal_processing_neon
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/common_audio/libsignal_processing_neon.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libcommon_video
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/common_video/libcommon_video.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libsystem_wrappers
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/system_wrappers/source/libsystem_wrappers.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libcpu_features_android
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/system_wrappers/source/libcpu_features_android.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libaudio_device
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libaudio_device.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libremote_bitrate_estimator
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libremote_bitrate_estimator.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := librtp_rtcp
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/librtp_rtcp.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libmedia_file
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libmedia_file.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libudp_transport
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libudp_transport.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libwebrtc_utility
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libwebrtc_utility.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libaudio_conference_mixer
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libaudio_conference_mixer.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libyuv
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/third_party/libyuv/libyuv.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libwebrtc_i420
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libwebrtc_i420.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libwebrtc_vp8
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/video_coding/codecs/vp8/libwebrtc_vp8.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libjpeg_turbo
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/third_party/libjpeg_turbo/libjpeg_turbo.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libaudioproc_debug_proto
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libaudioproc_debug_proto.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libprotobuf_lite
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/third_party/protobuf/libprotobuf_lite.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libvpx
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/third_party/libvpx/libvpx.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libvpx_arm_neon
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/third_party/libvpx/libvpx_arm_neon.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libpaced_sender
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/webrtc/modules/libpaced_sender.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE := libwebrtc-video-demo-jni
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES := \
    vie_android_java_api.cc \
    android_media_codec_decoder.cc
LOCAL_CFLAGS := \
    '-DWEBRTC_TARGET_PC' \
    '-DWEBRTC_ANDROID'

LOCAL_C_INCLUDES := \
    external/gtest/include \
    $(LOCAL_PATH)/../../../.. \
    $(LOCAL_PATH)/../../../../.. \
    $(LOCAL_PATH)/../../../include \
    $(LOCAL_PATH)/../../../../voice_engine/include

LOCAL_LDLIBS := \
    -llog \
    -lgcc \
    -lGLESv2 \
    -lOpenSLES \

LOCAL_STATIC_LIBRARIES := \
    libvoice_engine_core \
    libvideo_engine_core \
    libvideo_processing \
    libwebrtc_video_coding \
    libvideo_render_module \
    libvideo_capture_module \
    libaudio_coding_module \
    libaudio_processing \
    libaudio_processing_neon \
    libPCM16B \
    libCNG \
    libNetEq \
    libG722 \
    libiSAC \
    libG711 \
    libiLBC \
    libiSACFix \
    libisac_neon \
    libwebrtc_opus \
    libopus \
    libvad \
    libbitrate_controller \
    libresampler \
    libsignal_processing \
    libsignal_processing_neon \
    libcommon_video \
    libcpu_features_android \
    libaudio_device \
    libremote_bitrate_estimator \
    librtp_rtcp \
    libmedia_file \
    libudp_transport \
    libwebrtc_utility \
    libaudio_conference_mixer \
    libyuv \
    libwebrtc_i420 \
    libwebrtc_vp8 \
    libsystem_wrappers \
    libjpeg_turbo \
    libaudioproc_debug_proto \
    libprotobuf_lite \
    libvpx \
    libvpx_arm_neon \
    libpaced_sender \
    $(MY_SUPPLEMENTAL_LIBS)

include $(BUILD_SHARED_LIBRARY)
