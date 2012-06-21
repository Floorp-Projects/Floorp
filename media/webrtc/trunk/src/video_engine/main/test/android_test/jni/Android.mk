# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH := $(call my-dir)

MY_LIBS_PATH := $(LOCAL_PATH)/../../../../../../out/Release/obj.target

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
    $(LOCAL_PATH)/../../../../../voice_engine/main/interface

LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := \
    libstlport_shared

LOCAL_LDLIBS := \
    -llog \
    -lgcc \
    -lGLESv2 \
    -lOpenSLES \
    $(MY_LIBS_PATH)/src/voice_engine/libvoice_engine_core.a \
    $(MY_LIBS_PATH)/src/video_engine/libvideo_engine_core.a \
    $(MY_LIBS_PATH)/src/modules/libvideo_processing.a \
    $(MY_LIBS_PATH)/src/modules/libwebrtc_video_coding.a \
    $(MY_LIBS_PATH)/src/modules/libvideo_render_module.a \
    $(MY_LIBS_PATH)/src/modules/libvideo_capture_module.a \
    $(MY_LIBS_PATH)/src/modules/libaudio_coding_module.a \
    $(MY_LIBS_PATH)/src/modules/libaudio_processing.a \
    $(MY_LIBS_PATH)/src/modules/libPCM16B.a \
    $(MY_LIBS_PATH)/src/modules/libCNG.a \
    $(MY_LIBS_PATH)/src/modules/libNetEq.a \
    $(MY_LIBS_PATH)/src/modules/libG722.a \
    $(MY_LIBS_PATH)/src/modules/libiSAC.a \
    $(MY_LIBS_PATH)/src/modules/libG711.a \
    $(MY_LIBS_PATH)/src/modules/libiLBC.a \
    $(MY_LIBS_PATH)/src/modules/libiSACFix.a \
    $(MY_LIBS_PATH)/src/common_audio/libvad.a \
    $(MY_LIBS_PATH)/src/modules/libns.a \
    $(MY_LIBS_PATH)/src/modules/libagc.a \
    $(MY_LIBS_PATH)/src/modules/libaec.a \
    $(MY_LIBS_PATH)/src/modules/libaecm.a \
    $(MY_LIBS_PATH)/src/common_audio/libresampler.a \
    $(MY_LIBS_PATH)/src/common_audio/libsignal_processing.a \
    $(MY_LIBS_PATH)/src/modules/libapm_util.a \
    $(MY_LIBS_PATH)/src/system_wrappers/source/libsystem_wrappers.a \
    $(MY_LIBS_PATH)/src/modules/libaudio_device.a \
    $(MY_LIBS_PATH)/src/modules/librtp_rtcp.a \
    $(MY_LIBS_PATH)/src/modules/libmedia_file.a \
    $(MY_LIBS_PATH)/src/modules/libudp_transport.a \
    $(MY_LIBS_PATH)/src/modules/libwebrtc_utility.a \
    $(MY_LIBS_PATH)/src/modules/libaudio_conference_mixer.a \
    $(MY_LIBS_PATH)/src/common_video/libwebrtc_libyuv.a \
    $(MY_LIBS_PATH)/third_party/libyuv/libyuv.a \
    $(MY_LIBS_PATH)/src/modules/libwebrtc_i420.a \
    $(MY_LIBS_PATH)/src/modules/libwebrtc_vp8.a \
    $(MY_LIBS_PATH)/src/common_video/libwebrtc_jpeg.a \
    $(MY_LIBS_PATH)/third_party/libjpeg_turbo/libjpeg_turbo.a \
    $(MY_LIBS_PATH)/src/modules/libaudioproc_debug_proto.a \
    $(MY_LIBS_PATH)/third_party/protobuf/libprotobuf_lite.a \
    $(MY_LIBS_PATH)/third_party/libvpx/libvpx.a

include $(BUILD_SHARED_LIBRARY)

