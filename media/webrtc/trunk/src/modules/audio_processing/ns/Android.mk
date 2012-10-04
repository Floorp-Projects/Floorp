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

include $(LOCAL_PATH)/../../../../android-webrtc.mk

LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE := libwebrtc_ns
LOCAL_MODULE_TAGS := optional
LOCAL_GENERATED_SOURCES :=
LOCAL_SRC_FILES := \
    noise_suppression_x.c \
    nsx_core.c

# Files for floating point.
# noise_suppression.c ns_core.c

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := $(MY_WEBRTC_COMMON_DEFS)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/../utility \
    $(LOCAL_PATH)/../../.. \
    $(LOCAL_PATH)/../../../common_audio/signal_processing/include \
    $(LOCAL_PATH)/../../../system_wrappers/interface

LOCAL_STATIC_LIBRARIES += libwebrtc_system_wrappers

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libstlport

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_STATIC_LIBRARY)

#############################
# Build the neon library.
ifeq ($(WEBRTC_BUILD_NEON_LIBS),true)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE_CLASS := STATIC_LIBRARIES
LOCAL_MODULE := libwebrtc_ns_neon
LOCAL_MODULE_TAGS := optional
GEN := $(LOCAL_PATH)/nsx_core_neon_offsets.h

# Generate a header file nsx_core_neon_offsets.h which will be included in
# assembly file nsx_core_neon.S, from file nsx_core_neon_offsets.c.
$(GEN): $(LOCAL_PATH)/../../../../src/build/generate_asm_header.py \
            $(intermediates)/nsx_core_neon_offsets.S
	@python $^ $@ offset_nsx_

$(intermediates)/nsx_core_neon_offsets.S: $(LOCAL_PATH)/nsx_core_neon_offsets.c
	@$(TARGET_CC) $(addprefix -I, $(LOCAL_INCLUDES)) $(addprefix -isystem ,\
            $(TARGET_C_INCLUDES)) -S -o $@ $^

LOCAL_GENERATED_SOURCES := $(GEN)
LOCAL_SRC_FILES := nsx_core_neon.S

# Flags passed to both C and C++ files.
LOCAL_CFLAGS := \
    $(MY_WEBRTC_COMMON_DEFS) \
    -mfpu=neon \
    -mfloat-abi=softfp \
    -flax-vector-conversions

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/../../.. \
    $(LOCAL_PATH)/../../../common_audio/signal_processing/include

LOCAL_INCLUDES := $(LOCAL_C_INCLUDES)

ifndef NDK_ROOT
include external/stlport/libstlport.mk
endif
include $(BUILD_STATIC_LIBRARY)
endif # ifeq ($(WEBRTC_BUILD_NEON_LIBS),true)
