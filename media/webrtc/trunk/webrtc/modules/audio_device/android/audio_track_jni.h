/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_TRACK_JNI_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_TRACK_JNI_H_

#include <jni.h>

#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/audio_device/android/audio_common.h"
#include "webrtc/modules/audio_device/android/audio_manager.h"
#include "webrtc/modules/audio_device/include/audio_device_defines.h"
#include "webrtc/modules/audio_device/audio_device_generic.h"
#include "webrtc/modules/utility/interface/helpers_android.h"

namespace webrtc {

// Implements 16-bit mono PCM audio output support for Android using the Java
// AudioTrack interface. Most of the work is done by its Java counterpart in
// WebRtcAudioTrack.java. This class is created and lives on a thread in
// C++-land, but decoded audio buffers are requested on a high-priority
// thread managed by the Java class.
//
// An instance must be created and destroyed on one and the same thread.
// All public methods must also be called on the same thread. A thread checker
// will DCHECK if any method is called on an invalid thread.
// It is possible to call the two static methods (SetAndroidAudioDeviceObjects
// and ClearAndroidAudioDeviceObjects) from a different thread but both will
// CHECK that the calling thread is attached to a Java VM.
//
// All methods use AttachThreadScoped to attach to a Java VM if needed and then
// detach when method goes out of scope. We do so because this class does not
// own the thread is is created and called on and other objects on the same
// thread might put us in a detached state at any time.
class AudioTrackJni : public PlayoutDelayProvider {
 public:
  // Use the invocation API to allow the native application to use the JNI
  // interface pointer to access VM features.
  // |jvm| denotes the Java VM and |context| corresponds to
  // android.content.Context in Java.
  // This method also sets a global jclass object, |g_audio_track_class| for
  // the "org/webrtc/voiceengine/WebRtcAudioTrack"-class.
  static void SetAndroidAudioDeviceObjects(void* jvm, void* context);
  // Always call this method after the object has been destructed. It deletes
  // existing global references and enables garbage collection.
  static void ClearAndroidAudioDeviceObjects();

  AudioTrackJni(AudioManager* audio_manager);
  ~AudioTrackJni();

  int32_t Init();
  int32_t Terminate();

  int32_t InitPlayout();
  bool PlayoutIsInitialized() const { return initialized_; }

  int32_t StartPlayout();
  int32_t StopPlayout();
  bool Playing() const { return playing_; }

  int SpeakerVolumeIsAvailable(bool& available);
  int SetSpeakerVolume(uint32_t volume);
  int SpeakerVolume(uint32_t& volume) const;
  int MaxSpeakerVolume(uint32_t& max_volume) const;
  int MinSpeakerVolume(uint32_t& min_volume) const;

  int32_t PlayoutDelay(uint16_t& delayMS) const;
  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer);

 protected:
  // PlayoutDelayProvider implementation.
  virtual int PlayoutDelayMs();

 private:
  // Called from Java side so we can cache the address of the Java-manged
  // |byte_buffer| in |direct_buffer_address_|. The size of the buffer
  // is also stored in |direct_buffer_capacity_in_bytes_|.
  // Called on the same thread as the creating thread.
  static void JNICALL CacheDirectBufferAddress(
    JNIEnv* env, jobject obj, jobject byte_buffer, jlong nativeAudioTrack);
  void OnCacheDirectBufferAddress(JNIEnv* env, jobject byte_buffer);

  // Called periodically by the Java based WebRtcAudioTrack object when
  // playout has started. Each call indicates that |length| new bytes should
  // be written to the memory area |direct_buffer_address_| for playout.
  // This method is called on a high-priority thread from Java. The name of
  // the thread is 'AudioTrackThread'.
  static void JNICALL GetPlayoutData(
    JNIEnv* env, jobject obj, jint length, jlong nativeAudioTrack);
  void OnGetPlayoutData(int length);

  // Returns true if SetAndroidAudioDeviceObjects() has been called
  // successfully.
  bool HasDeviceObjects();

  // Called from the constructor. Defines the |j_audio_track_| member.
  void CreateJavaInstance();

  // Stores thread ID in constructor.
  // We can then use ThreadChecker::CalledOnValidThread() to ensure that
  // other methods are called from the same thread.
  rtc::ThreadChecker thread_checker_;

  // Stores thread ID in first call to OnGetPlayoutData() from high-priority
  // thread in Java. Detached during construction of this object.
  rtc::ThreadChecker thread_checker_java_;

  // Contains audio parameters provided to this class at construction by the
  // AudioManager.
  const AudioParameters audio_parameters_;

  // The Java WebRtcAudioTrack instance.
  jobject j_audio_track_;

  // Cached copy of address to direct audio buffer owned by |j_audio_track_|.
  void* direct_buffer_address_;

  // Number of bytes in the direct audio buffer owned by |j_audio_track_|.
  int direct_buffer_capacity_in_bytes_;

  // Number of audio frames per audio buffer. Each audio frame corresponds to
  // one sample of PCM mono data at 16 bits per sample. Hence, each audio
  // frame contains 2 bytes (given that the Java layer only supports mono).
  // Example: 480 for 48000 Hz or 441 for 44100 Hz.
  int frames_per_buffer_;

  bool initialized_;

  bool playing_;

  // Raw pointer handle provided to us in AttachAudioBuffer(). Owned by the
  // AudioDeviceModuleImpl class and called by AudioDeviceModuleImpl::Create().
  // The AudioDeviceBuffer is a member of the AudioDeviceModuleImpl instance
  // and therefore outlives this object.
  AudioDeviceBuffer* audio_device_buffer_;

  // Estimated playout delay caused by buffering in the Java based audio track.
  // We are using a fixed value here since measurements have shown that the
  // variations are very small (~10ms) and it is not worth the extra complexity
  // to update this estimate on a continuous basis.
  int delay_in_milliseconds_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_TRACK_JNI_H_
