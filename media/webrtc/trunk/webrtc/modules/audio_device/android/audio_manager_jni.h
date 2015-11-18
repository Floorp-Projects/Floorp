/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Android APIs used to access Java functionality needed to enable low latency
// audio.

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_MANAGER_JNI_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_MANAGER_JNI_H_

#include <jni.h>

namespace webrtc {

class AudioManagerJni {
 public:
  AudioManagerJni();
  ~AudioManagerJni() {}

  // SetAndroidAudioDeviceObjects must only be called once unless there has
  // been a successive call to ClearAndroidAudioDeviceObjects. For each
  // call to ClearAndroidAudioDeviceObjects, SetAndroidAudioDeviceObjects may be
  // called once.
  // This function must be called by a Java thread as calling it from a thread
  // created by the native application will prevent FindClass from working. See
  // http://developer.android.com/training/articles/perf-jni.html#faq_FindClass
  // for more details.
  // It has to be called for this class' APIs to be successful. Calling
  // ClearAndroidAudioDeviceObjects will prevent this class' APIs to be called
  // successfully if SetAndroidAudioDeviceObjects is not called after it.
  static void SetAndroidAudioDeviceObjects(void* jvm, void* context);
  // This function must be called when the AudioManagerJni class is no
  // longer needed. It frees up the global references acquired in
  // SetAndroidAudioDeviceObjects.
  static void ClearAndroidAudioDeviceObjects();

  bool low_latency_supported() const { return low_latency_supported_; }
  int native_output_sample_rate() const { return native_output_sample_rate_; }
  int native_buffer_size() const { return native_buffer_size_; }

 private:
  bool HasDeviceObjects();

  // Following functions assume that the calling thread has been attached.
  void SetLowLatencySupported(JNIEnv* env);
  void SetNativeOutputSampleRate(JNIEnv* env);
  void SetNativeFrameSize(JNIEnv* env);

  jmethodID LookUpMethodId(JNIEnv* env, const char* method_name,
                           const char* method_signature);

  void CreateInstance(JNIEnv* env);

  // Whether or not low latency audio is supported, the native output sample
  // rate and the audio buffer size do not change. I.e the values might as well
  // just be cached when initializing.
  bool low_latency_supported_;
  int native_output_sample_rate_;
  int native_buffer_size_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_MANAGER_JNI_H_
