#  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

MY_CAPTURE_FOLDER := ../../../../modules/video_capture/main/source
MY_CAPTURE_JAVA_FOLDER := Android/java/org/webrtc/videoengine
MY_CAPTURE_PATH := $(MY_CAPTURE_FOLDER)/$(MY_CAPTURE_JAVA_FOLDER)

MY_RENDER_FOLDER := ../../../../modules/video_render/main/source
MY_RENDER_JAVA_FOLDER := Android/java/org/webrtc/videoengine
MY_RENDER_PATH := $(MY_RENDER_FOLDER)/$(MY_RENDER_JAVA_FOLDER)

LOCAL_MODULE_TAGS := tests

LOCAL_SRC_FILES := \
    src/org/webrtc/vieautotest/ViEAutotest.java \
    $(MY_CAPTURE_PATH)/CaptureCapabilityAndroid.java \
    $(MY_CAPTURE_PATH)/VideoCaptureAndroid.java \
    $(MY_CAPTURE_PATH)/VideoCaptureDeviceInfoAndroid.java \
    $(MY_RENDER_PATH)/ViEAndroidGLES20.java \
    $(MY_RENDER_PATH)/ViERenderer.java \
    $(MY_RENDER_PATH)/ViESurfaceRenderer.java 

LOCAL_PACKAGE_NAME := webrtc-video-autotest
LOCAL_CERTIFICATE := platform

LOCAL_JNI_SHARED_LIBRARIES := libwebrtc-video-autotest-jni

include $(BUILD_PACKAGE)

include $(call all-makefiles-under,$(LOCAL_PATH))
