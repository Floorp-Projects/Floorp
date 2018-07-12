#
# Copyright (c) 2016, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 2 Clause License and
# the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
# was not distributed with this source code in the LICENSE file, you can
# obtain it at www.aomedia.org/license/software. If the Alliance for Open
# Media Patent License 1.0 was not distributed with this source code in the
# PATENTS file, you can obtain it at www.aomedia.org/license/patent.
#
# This make file builds aom_test app for android.
# The test app itself runs on the command line through adb shell
# The paths are really messed up as the libaom make file
# expects to be made from a parent directory.
CUR_WD := $(call my-dir)
BINDINGS_DIR := $(CUR_WD)/../../..
LOCAL_PATH := $(CUR_WD)/../../..

#libwebm
include $(CLEAR_VARS)
include $(BINDINGS_DIR)/libaom/third_party/libwebm/Android.mk
LOCAL_PATH := $(CUR_WD)/../../..

#libaom
include $(CLEAR_VARS)
LOCAL_STATIC_LIBRARIES := libwebm
include $(BINDINGS_DIR)/libaom/build/make/Android.mk
LOCAL_PATH := $(CUR_WD)/../..

#libgtest
include $(CLEAR_VARS)
LOCAL_ARM_MODE := arm
LOCAL_CPP_EXTENSION := .cc
LOCAL_MODULE := gtest
LOCAL_C_INCLUDES := $(LOCAL_PATH)/third_party/googletest/src/googletest/src
LOCAL_C_INCLUDES += $(LOCAL_PATH)/third_party/googletest/src/googletest/include
LOCAL_SRC_FILES := ./third_party/googletest/src/googletest/src/gtest-all.cc
include $(BUILD_STATIC_LIBRARY)

#libaom_test
include $(CLEAR_VARS)
LOCAL_ARM_MODE := arm
LOCAL_MODULE := libaom_test
LOCAL_STATIC_LIBRARIES := gtest libwebm

ifeq ($(ENABLE_SHARED),1)
  LOCAL_SHARED_LIBRARIES := aom
else
  LOCAL_STATIC_LIBRARIES += aom
endif

include $(LOCAL_PATH)/test/test.mk
LOCAL_C_INCLUDES := $(BINDINGS_DIR)
FILTERED_SRC := $(sort $(filter %.cc %.c, $(LIBAOM_TEST_SRCS-yes)))
LOCAL_SRC_FILES := $(addprefix ./test/, $(FILTERED_SRC))
# some test files depend on *_rtcd.h, ensure they're generated first.
$(eval $(call rtcd_dep_template))
include $(BUILD_EXECUTABLE)
