LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CPP_EXTENSION := .cpp
LOCAL_SRC_FILES := mkvmuxer.cpp \
                   mkvmuxerutil.cpp \
                   mkvparser.cpp \
                   mkvreader.cpp \
                   mkvwriter.cpp
LOCAL_MODULE := libwebm
include $(BUILD_STATIC_LIBRARY)
