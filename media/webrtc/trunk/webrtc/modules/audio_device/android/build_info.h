/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_BUILD_INFO_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_BUILD_INFO_H_

#include <jni.h>
#include <string>

#include "webrtc/modules/utility/include/jvm_android.h"

namespace webrtc {

// Utility class used to query the Java class (org/webrtc/voiceengine/BuildInfo)
// for device and Android build information.
// The calling thread is attached to the JVM at construction if needed and a
// valid Java environment object is also created.
// All Get methods must be called on the creating thread. If not, the code will
// hit RTC_DCHECKs when calling JNIEnvironment::JavaToStdString().
class BuildInfo {
 public:
  BuildInfo();
  ~BuildInfo() {}

  // End-user-visible name for the end product (e.g. "Nexus 6").
  std::string GetDeviceModel();
  // Consumer-visible brand (e.g. "google").
  std::string GetBrand();
  // Manufacturer of the product/hardware (e.g. "motorola").
  std::string GetDeviceManufacturer();
  // Android build ID (e.g. LMY47D).
  std::string GetAndroidBuildId();
  // The type of build (e.g. "user" or "eng").
  std::string GetBuildType();
  // The user-visible version string (e.g. "5.1").
  std::string GetBuildRelease();
  // The user-visible SDK version of the framework (e.g. 21).
  std::string GetSdkVersion();

 private:
  // Helper method which calls a static getter method with |name| and returns
  // a string from Java.
  std::string GetStringFromJava(const char* name);

  // Ensures that this class can access a valid JNI interface pointer even
  // if the creating thread was not attached to the JVM.
  AttachCurrentThreadIfNeeded attach_thread_if_needed_;

  // Provides access to the JNIEnv interface pointer and the JavaToStdString()
  // method which is used to translate Java strings to std strings.
  rtc::scoped_ptr<JNIEnvironment> j_environment_;

  // Holds the jclass object and provides access to CallStaticObjectMethod().
  // Used by GetStringFromJava() during construction only.
  JavaClass j_build_info_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_BUILD_INFO_H_
