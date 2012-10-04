# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH := $(call my-dir)

MY_LIBS_PATH := ../../../../../../out/Release/obj.target

include $(CLEAR_VARS)
LOCAL_MODULE := libvoice_engine_core
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/voice_engine/libvoice_engine_core.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libvideo_engine_core
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/video_engine/libvideo_engine_core.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libvideo_processing
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libvideo_processing.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libwebrtc_video_coding
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libwebrtc_video_coding.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libvideo_render_module
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libvideo_render_module.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libvideo_capture_module
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libvideo_capture_module.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libaudio_coding_module
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libaudio_coding_module.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libaudio_processing
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libaudio_processing.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libPCM16B
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libPCM16B.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libCNG
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libCNG.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libNetEq
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libNetEq.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libG722
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libG722.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libiSAC
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libiSAC.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libG711
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libG711.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libiLBC
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libiLBC.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libiSACFix
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libiSACFix.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libisac_neon
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libisac_neon.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libvad
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/common_audio/libvad.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libns_fix
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libns_fix.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libns_neon
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libns_neon.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libagc
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libagc.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libaec
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libaec.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libaecm
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libaecm.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libaecm_neon
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libaecm_neon.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libbitrate_controller
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libbitrate_controller.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libresampler
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/common_audio/libresampler.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libsignal_processing
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/common_audio/libsignal_processing.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libapm_util
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libapm_util.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libsystem_wrappers
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/system_wrappers/source/libsystem_wrappers.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libcpu_features_android
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/system_wrappers/source/libcpu_features_android.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libaudio_device
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libaudio_device.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libremote_bitrate_estimator
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libremote_bitrate_estimator.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := librtp_rtcp
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/librtp_rtcp.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libmedia_file
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libmedia_file.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libudp_transport
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libudp_transport.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libwebrtc_utility
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libwebrtc_utility.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libaudio_conference_mixer
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libaudio_conference_mixer.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libwebrtc_libyuv
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/common_video/libwebrtc_libyuv.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libyuv
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/third_party/libyuv/libyuv.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libwebrtc_i420
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libwebrtc_i420.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libwebrtc_vp8
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/video_coding/codecs/vp8/libwebrtc_vp8.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libwebrtc_jpeg
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/common_video/libwebrtc_jpeg.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libjpeg_turbo
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/third_party/libjpeg_turbo/libjpeg_turbo.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libaudioproc_debug_proto
LOCAL_SRC_FILES := \
    $(MY_LIBS_PATH)/src/modules/libaudioproc_debug_proto.a
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
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE := libwebrtc-video-demo-jni
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES := vie_android_java_api.cc
LOCAL_CFLAGS := \
    '-DWEBRTC_TARGET_PC' \
    '-DWEBRTC_ANDROID'

LOCAL_C_INCLUDES := \
    external/gtest/include \
    $(LOCAL_PATH)/../../../../.. \
    $(LOCAL_PATH)/../../../../include \
    $(LOCAL_PATH)/../../../../../voice_engine/include

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
    libPCM16B \
    libCNG \
    libNetEq \
    libG722 \
    libiSAC \
    libG711 \
    libiLBC \
    libiSACFix \
    libisac_neon \
    libvad \
    libns_fix \
    libns_neon \
    libagc \
    libaec \
    libaecm \
    libaecm_neon \
    libbitrate_controller \
    libresampler \
    libsignal_processing \
    libapm_util \
    libsystem_wrappers \
    libcpu_features_android \
    libaudio_device \
    libremote_bitrate_estimator \
    librtp_rtcp \
    libmedia_file \
    libudp_transport \
    libwebrtc_utility \
    libaudio_conference_mixer \
    libwebrtc_libyuv \
    libyuv \
    libwebrtc_i420 \
    libwebrtc_vp8 \
    libwebrtc_jpeg \
    libjpeg_turbo \
    libaudioproc_debug_proto \
    libprotobuf_lite \
    libvpx

include $(BUILD_SHARED_LIBRARY)
