# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../../android-webrtc.mk

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libwebrtc_system_wrappers
LOCAL_MODULE_TAGS := optional
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES := \
    android/cpu-features.c \
    cpu_features_android.c \
    map.cc \
    sort.cc \
    aligned_malloc.cc \
    atomic32_posix.cc \
    condition_variable.cc \
    cpu_features.cc \
    cpu_info.cc \
    critical_section.cc \
    event.cc \
    event_tracer.cc \
    file_impl.cc \
    list_no_stl.cc \
    rw_lock.cc \
    thread.cc \
    trace_impl.cc \
    condition_variable_posix.cc \
    critical_section_posix.cc \
    event_posix.cc \
    sleep.cc \
    thread_posix.cc \
    tick_util.cc \
    trace_posix.cc \
    rw_lock_posix.cc 

LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../.. \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/spreadsortlib \
    external/webrtc

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libstlport

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_STATIC_LIBRARY)
