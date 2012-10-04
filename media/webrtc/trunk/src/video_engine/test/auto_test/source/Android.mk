#  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH:= $(call my-dir)

# voice engine test app

include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../../../../android-webrtc.mk

LOCAL_MODULE_TAGS := tests
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES:= \
    vie_autotest.cc \
    vie_autotest_android.cc \
    vie_autotest_base.cc \
    vie_autotest_capture.cc \
    vie_autotest_codec.cc \
    vie_autotest_encryption.cc \
    vie_autotest_file.cc \
    vie_autotest_image_process.cc \
    vie_autotest_loopback.cc \
    vie_autotest_network.cc \
    vie_autotest_render.cc \
    vie_autotest_rtp_rtcp.cc \
    vie_comparison_tests.cc

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    '-DWEBRTC_TARGET_PC' \
    '-DWEBRTC_ANDROID' \
    '-DWEBRTC_ANDROID_OPENSLES'

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../helpers \
    $(LOCAL_PATH)/../primitives \
    $(LOCAL_PATH)/../../../.. \
    $(LOCAL_PATH)/../../../include \
    $(LOCAL_PATH)/../../.. \
    $(LOCAL_PATH)/../../../../modules/video_coding/codecs/interface \
    $(LOCAL_PATH)/../../../../system_wrappers/interface \
    $(LOCAL_PATH)/../../../../modules/video_render/main/interface \
    $(LOCAL_PATH)/../../../../modules/interface \
    $(LOCAL_PATH)/../../../../modules/video_capture/main/interface \
    $(LOCAL_PATH)/../../../../common_video/vplib/main/interface \
    $(LOCAL_PATH)/../../../../voice_engine/include

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libstlport \
    libwebrtc

LOCAL_MODULE:= webrtc_video_test

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_EXECUTABLE)
