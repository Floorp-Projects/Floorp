# Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../../../../../../android-webrtc.mk

LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE := libwebrtc_vp8
LOCAL_MODULE_TAGS := optional
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES := \
    reference_picture_selection.cc \
    vp8_impl.cc

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS)
# TODO(leozwang) Enable WEBRTC_LIBVPX_VERSION after libvpx is updateed
# to a new version and also add temporal_layers.cc

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../../../interface \
    $(LOCAL_PATH)/../../../../../.. \
    $(LOCAL_PATH)/../../../../../../common_video/interface \
    $(LOCAL_PATH)/../../../../../../common_video/vplib/main/interface \
    $(LOCAL_PATH)/../../../../../../modules/interface \
    $(LOCAL_PATH)/../../../../../../system_wrappers/interface \
    external/libvpx 

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libstlport

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_STATIC_LIBRARY)
