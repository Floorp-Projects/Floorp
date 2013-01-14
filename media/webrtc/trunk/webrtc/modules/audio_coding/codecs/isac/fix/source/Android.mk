# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

#############################
# Build the non-neon library.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

include $(LOCAL_PATH)/../../../../../../../android-webrtc.mk

LOCAL_ARM_MODE := arm
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE := libwebrtc_isacfix
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    arith_routines.c \
    arith_routines_hist.c \
    arith_routines_logist.c \
    bandwidth_estimator.c \
    decode.c \
    decode_bwe.c \
    decode_plc.c \
    encode.c \
    entropy_coding.c \
    fft.c \
    filterbank_tables.c \
    filterbanks.c \
    filters.c \
    initialize.c \
    isacfix.c \
    lattice.c \
    lpc_masking_model.c \
    lpc_tables.c \
    pitch_estimator.c \
    pitch_filter.c \
    pitch_gain_tables.c \
    pitch_lag_tables.c \
    spectrum_ar_model_tables.c \
    transform.c

ifeq ($(ARCH_ARM_HAVE_ARMV7A),true)
# Using .S (instead of .s) extention is to include a C header file in assembly.
LOCAL_SRC_FILES += \
    lattice_armv7.S \
    pitch_filter_armv6.S
else
LOCAL_SRC_FILES += \
    lattice_c.c
endif

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../../../../../.. \
    $(LOCAL_PATH)/../../../../../../common_audio/signal_processing/include

LOCAL_STATIC_LIBRARIES += libwebrtc_system_wrappers

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libstlport

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_STATIC_LIBRARY)

#########################
# Build the neon library.
ifeq ($(WEBRTC_BUILD_NEON_LIBS),true)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE := libwebrtc_isacfix_neon
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := \
    filterbanks_neon.S \
    filters_neon.S \
    lattice_neon.S \
    lpc_masking_model_neon.S \
    transform_neon.S

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS) \
    -mfpu=neon \
    -mfloat-abi=softfp \
    -flax-vector-conversions

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../../../../../.. \
    $(LOCAL_PATH)/../../../../../../common_audio/signal_processing/include


ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_STATIC_LIBRARY)

endif # ifeq ($(WEBRTC_BUILD_NEON_LIBS),true)

###########################
# isac test app

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests
LOCAL_CPP_EXTENSION := .cc
LOCAL_SRC_FILES:= ../test/kenny.cc

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := $(MY_WEBRTC_COMMON_DEFS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../interface \
    $(LOCAL_PATH)/../../../../../..

LOCAL_STATIC_LIBRARIES := \
    libwebrtc_isacfix \
    libwebrtc_spl \
    libwebrtc_system_wrappers \
    libwebrtc_test_support

ifeq ($(WEBRTC_BUILD_NEON_LIBS),true)
LOCAL_STATIC_LIBRARIES += \
    libwebrtc_isacfix_neon
endif

LOCAL_SHARED_LIBRARIES := \
    libutils

LOCAL_MODULE:= webrtc_isac_test

ifdef NDK_ROOT
include $(BUILD_EXECUTABLE)
else
include $(BUILD_NATIVE_TEST)
endif
