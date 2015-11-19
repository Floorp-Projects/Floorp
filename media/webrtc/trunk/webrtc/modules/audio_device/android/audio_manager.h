/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_MANAGER_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_MANAGER_H_

#if !defined(MOZ_WIDGET_GONK)
#include <jni.h>
#endif

#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/audio_device/android/audio_common.h"
#include "webrtc/modules/audio_device/include/audio_device_defines.h"
#include "webrtc/modules/audio_device/audio_device_generic.h"
#if !defined(MOZ_WIDGET_GONK)
#include "webrtc/modules/utility/interface/helpers_android.h"
#endif

namespace webrtc {

class AudioParameters {
 public:
  enum { kBitsPerSample = 16 };
  AudioParameters()
      : sample_rate_(0),
        channels_(0),
        frames_per_buffer_(0),
        bits_per_sample_(kBitsPerSample) {}
  AudioParameters(int sample_rate, int channels)
      : sample_rate_(sample_rate),
        channels_(channels),
        frames_per_buffer_(sample_rate / 100),
        bits_per_sample_(kBitsPerSample) {}
  void reset(int sample_rate, int channels) {
    sample_rate_ = sample_rate;
    channels_ = channels;
    // WebRTC uses a fixed buffer size equal to 10ms.
    frames_per_buffer_ = (sample_rate / 100);
  }
  int sample_rate() const { return sample_rate_; }
  int channels() const { return channels_; }
  int frames_per_buffer() const { return frames_per_buffer_; }
  bool is_valid() const {
    return ((sample_rate_ > 0) && (channels_ > 0) && (frames_per_buffer_ > 0));
  }
  int GetBytesPerFrame() const { return channels_ * bits_per_sample_ / 8; }
  int GetBytesPerBuffer() const {
    return frames_per_buffer_ * GetBytesPerFrame();
  }

 private:
  int sample_rate_;
  int channels_;
  int frames_per_buffer_;
  const int bits_per_sample_;
};

// Implements support for functions in the WebRTC audio stack for Android that
// relies on the AudioManager in android.media. It also populates an
// AudioParameter structure with native audio parameters detected at
// construction. This class does not make any audio-related modifications
// unless Init() is called. Caching audio parameters makes no changes but only
// reads data from the Java side.
// TODO(henrika): expand this class when adding support for low-latency
// OpenSL ES. Currently, it only contains very basic functionality.
class AudioManager {
 public:
  // Use the invocation API to allow the native application to use the JNI
  // interface pointer to access VM features. |jvm| denotes the Java VM and
  // |context| corresponds to android.content.Context in Java.
  // This method also sets a global jclass object, |g_audio_manager_class| for
  // the "org/webrtc/voiceengine/WebRtcAudioManager"-class.
  static void SetAndroidAudioDeviceObjects(void* jvm, void* context);
  // Always call this method after the object has been destructed. It deletes
  // existing global references and enables garbage collection.
  static void ClearAndroidAudioDeviceObjects();

  AudioManager();
  ~AudioManager();

  // Initializes the audio manager (changes mode to MODE_IN_COMMUNICATION,
  // request audio focus etc.).
  // It is possible to use this class without calling Init() if the calling
  // application prefers to set up the audio environment on its own instead.
  bool Init();
  // Revert any setting done by Init().
  bool Close();

  // Native audio parameters stored during construction.
  AudioParameters GetPlayoutAudioParameters() const;
  AudioParameters GetRecordAudioParameters() const;

  bool initialized() const { return initialized_; }

 private:
#if !defined(MOZ_WIDGET_GONK)
  // Called from Java side so we can cache the native audio parameters.
  // This method will be called by the WebRtcAudioManager constructor, i.e.
  // on the same thread that this object is created on.
  static void JNICALL CacheAudioParameters(JNIEnv* env, jobject obj,
      jint sample_rate, jint channels, jlong nativeAudioManager);
  void OnCacheAudioParameters(JNIEnv* env, jint sample_rate, jint channels);
#endif
  
  // Returns true if SetAndroidAudioDeviceObjects() has been called
  // successfully.
  bool HasDeviceObjects();

  // Called from the constructor. Defines the |j_audio_manager_| member.
  void CreateJavaInstance();

  // Stores thread ID in the constructor.
  // We can then use ThreadChecker::CalledOnValidThread() to ensure that
  // other methods are called from the same thread.
  rtc::ThreadChecker thread_checker_;

#if !defined(MOZ_WIDGET_GONK)
  // The Java WebRtcAudioManager instance.
  jobject j_audio_manager_;
#endif

  // Set to true by Init() and false by Close().
  bool initialized_;

  // Contains native parameters (e.g. sample rate, channel configuration).
  // Set at construction in OnCacheAudioParameters() which is called from
  // Java on the same thread as this object is created on.
  AudioParameters playout_parameters_;
  AudioParameters record_parameters_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_MANAGER_H_
