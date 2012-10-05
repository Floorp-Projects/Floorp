#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests

LOCAL_SRC_FILES := \
    src/org/webrtc/videoengineapp/WebRTCDemo.java \
    src/org/webrtc/videoengineapp/ViEAndroidJavaAPI.java \
    src/org/webrtc/videoengineapp/IViEAndroidCallback.java \
    src/org/webrtc/videoengine/CaptureCapabilityAndroid.java \
    src/org/webrtc/videoengine/VideoCaptureAndroid.java \
    src/org/webrtc/videoengine/VideoCaptureDeviceInfoAndroid.java \
    src/org/webrtc/videoengine/ViEAndroidGLES20.java \
    src/org/webrtc/videoengine/ViERenderer.java \
    src/org/webrtc/videoengine/ViESurfaceRenderer.java

LOCAL_PACKAGE_NAME := webrtc-video-demo
LOCAL_CERTIFICATE := platform

LOCAL_JNI_SHARED_LIBRARIES := libwebrtc-video-demo-jni

include $(BUILD_PACKAGE)

include $(call all-makefiles-under,$(LOCAL_PATH))
