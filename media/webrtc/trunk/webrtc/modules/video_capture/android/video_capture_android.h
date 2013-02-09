/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_ANDROID_VIDEO_CAPTURE_ANDROID_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_ANDROID_VIDEO_CAPTURE_ANDROID_H_

#include <jni.h>
#include "device_info_android.h"
#include "../video_capture_impl.h"

#define AndroidJavaCaptureClass "org/webrtc/videoengine/VideoCaptureAndroid"

namespace webrtc {
namespace videocapturemodule {

class VideoCaptureAndroid : public VideoCaptureImpl {
 public:
  static WebRtc_Word32 SetAndroidObjects(void* javaVM, void* javaContext);
  static WebRtc_Word32 AttachAndUseAndroidDeviceInfoObjects(
      JNIEnv*& env,
      jclass& javaCmDevInfoClass,
      jobject& javaCmDevInfoObject,
      bool& attached);
  static WebRtc_Word32 ReleaseAndroidDeviceInfoObjects(bool attached);

  VideoCaptureAndroid(const WebRtc_Word32 id);
  virtual WebRtc_Word32 Init(const WebRtc_Word32 id,
                             const char* deviceUniqueIdUTF8);


  virtual WebRtc_Word32 StartCapture(
      const VideoCaptureCapability& capability);
  virtual WebRtc_Word32 StopCapture();
  virtual bool CaptureStarted();
  virtual WebRtc_Word32 CaptureSettings(VideoCaptureCapability& settings);
  virtual WebRtc_Word32 SetCaptureRotation(VideoCaptureRotation rotation);

 protected:
  virtual ~VideoCaptureAndroid();
  static void JNICALL ProvideCameraFrame (JNIEnv * env,
                                          jobject,
                                          jbyteArray javaCameraFrame,
                                          jint length, jlong context);
  DeviceInfoAndroid _capInfo;
  jobject _javaCaptureObj; // Java Camera object.
  VideoCaptureCapability _frameInfo;
  bool _captureStarted;

  static JavaVM* g_jvm;
  static jclass g_javaCmClass;
  static jclass g_javaCmDevInfoClass;
  //Static java object implementing the needed device info functions;
  static jobject g_javaCmDevInfoObject;
};

}  // namespace videocapturemodule
}  // namespace webrtc
#endif // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_SOURCE_ANDROID_VIDEO_CAPTURE_ANDROID_H_
