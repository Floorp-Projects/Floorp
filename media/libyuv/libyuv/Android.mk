# This is the Android makefile for libyuv for both platform and NDK.
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_CPP_EXTENSION := .cc

LOCAL_SRC_FILES := \
    source/compare.cc           \
    source/compare_common.cc    \
    source/compare_gcc.cc       \
    source/compare_neon.cc      \
    source/compare_neon64.cc    \
    source/convert.cc           \
    source/convert_argb.cc      \
    source/convert_from.cc      \
    source/convert_from_argb.cc \
    source/convert_jpeg.cc      \
    source/convert_to_argb.cc   \
    source/convert_to_i420.cc   \
    source/cpu_id.cc            \
    source/mjpeg_decoder.cc     \
    source/mjpeg_validate.cc    \
    source/planar_functions.cc  \
    source/rotate.cc            \
    source/rotate_any.cc        \
    source/rotate_argb.cc       \
    source/rotate_common.cc     \
    source/rotate_dspr2.cc      \
    source/rotate_gcc.cc        \
    source/rotate_msa.cc        \
    source/rotate_neon.cc       \
    source/rotate_neon64.cc     \
    source/row_any.cc           \
    source/row_common.cc        \
    source/row_dspr2.cc         \
    source/row_gcc.cc           \
    source/row_msa.cc           \
    source/row_neon.cc          \
    source/row_neon64.cc        \
    source/scale.cc             \
    source/scale_any.cc         \
    source/scale_argb.cc        \
    source/scale_common.cc      \
    source/scale_dspr2.cc       \
    source/scale_gcc.cc         \
    source/scale_msa.cc         \
    source/scale_neon.cc        \
    source/scale_neon64.cc      \
    source/video_common.cc

common_CFLAGS := -Wall -fexceptions -DHAVE_JPEG -Wno-unused-parameter
LOCAL_CFLAGS += $(common_CFLAGS)
LOCAL_SHARED_LIBRARIES := libjpeg
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/include

LOCAL_MODULE := libyuv_static
LOCAL_MODULE_TAGS := optional

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_WHOLE_STATIC_LIBRARIES := libyuv_static
LOCAL_MODULE := libyuv
LOCAL_SHARED_LIBRARIES := libjpeg

include $(BUILD_SHARED_LIBRARY)
