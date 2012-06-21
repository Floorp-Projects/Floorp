#  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH := $(call my-dir)

# the follow two lines are for NDK build
INTERFACES_PATH := $(LOCAL_PATH)/../../../../../../build/interface
LIBS_PATH := $(LOCAL_PATH)/../../../../../../build/libraries

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests
LOCAL_MODULE := libwebrtc-video-autotest-jni
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES := \
    vie_autotest_jni.cc \
    ../../source/vie_autotest_android.cc \
    ../../source/vie_autotest.cc \
    ../../source/vie_autotest_base.cc \
    ../../source/vie_autotest_capture.cc \
    ../../source/vie_autotest_codec.cc \
    ../../source/vie_autotest_encryption.cc \
    ../../source/vie_autotest_file.cc \
    ../../source/vie_autotest_image_process.cc \
    ../../source/vie_autotest_loopback.cc \
    ../../source/vie_autotest_network.cc \
    ../../source/vie_autotest_render.cc \
    ../../source/vie_autotest_rtp_rtcp.cc \
    ../../source/tb_I420_codec.cc \
    ../../source/tb_capture_device.cc \
    ../../source/tb_external_transport.cc \
    ../../source/tb_interfaces.cc \
    ../../source/tb_video_channel.cc 

LOCAL_CFLAGS := \
    '-DWEBRTC_TARGET_PC' \
    '-DWEBRTC_ANDROID' \
    '-DWEBRTC_ANDROID_OPENSLES'

LOCAL_C_INCLUDES := \
    external/gtest/include \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../../interface \
    $(LOCAL_PATH)/../../../interface \
    $(LOCAL_PATH)/../../.. \
    $(LOCAL_PATH)/../../../../.. \
    $(LOCAL_PATH)/../../../../../common_video/interface \
    $(LOCAL_PATH)/../../../../../common_video/vplib/main/interface \
    $(LOCAL_PATH)/../../../../../modules/interface \
    $(LOCAL_PATH)/../../../../../modules/video_capture/main/interface \
    $(LOCAL_PATH)/../../../../../modules/video_capture/main/source \
    $(LOCAL_PATH)/../../../../../modules/video_coding/codecs/interface \
    $(LOCAL_PATH)/../../../../../modules/video_render/main/interface \
    $(LOCAL_PATH)/../../../../../voice_engine/main/interface \
    $(LOCAL_PATH)/../../../../../system_wrappers/interface 

LOCAL_PRELINK_MODULE := false

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libstlport \
    libandroid \
    libwebrtc \
    libGLESv2

# the following line is for NDK build
LOCAL_LDLIBS     := $(LIBS_PATH)/VideoEngine_android_gcc.a -llog -lgcc 

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_SHARED_LIBRARY)
