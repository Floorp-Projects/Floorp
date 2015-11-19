/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_RECORD_JNI_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_RECORD_JNI_H_

#include <jni.h>

#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/audio_device/android/audio_manager.h"
#include "webrtc/modules/audio_device/include/audio_device_defines.h"
#include "webrtc/modules/audio_device/audio_device_generic.h"
#include "webrtc/modules/utility/interface/helpers_android.h"

namespace webrtc {

class PlayoutDelayProvider;

// Implements 16-bit mono PCM audio input support for Android using the Java
// AudioRecord interface. Most of the work is done by its Java counterpart in
// WebRtcAudioRecord.java. This class is created and lives on a thread in
// C++-land, but recorded audio buffers are delivered on a high-priority
// thread managed by the Java class.
//
// The Java class makes use of AudioEffect features (mainly AEC) which are
// first available in Jelly Bean. If it is instantiated running against earlier
// SDKs, the AEC provided by the APM in WebRTC must be used and enabled
// separately instead.
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
class AudioRecordJni {
 public:
  // Use the invocation API to allow the native application to use the JNI
  // interface pointer to access VM features.
  // |jvm| denotes the Java VM and |context| corresponds to
  // android.content.Context in Java.
  // This method also sets a global jclass object, |g_audio_record_class| for
  // the "org/webrtc/voiceengine/WebRtcAudioRecord"-class.
  static void SetAndroidAudioDeviceObjects(void* jvm, void* context);
  // Always call this method after the object has been destructed. It deletes
  // existing global references and enables garbage collection.
  static void ClearAndroidAudioDeviceObjects();

  AudioRecordJni(
      PlayoutDelayProvider* delay_provider, AudioManager* audio_manager);
  ~AudioRecordJni();

  int32_t Init();
  int32_t Terminate();

  int32_t InitRecording();
  bool RecordingIsInitialized() const { return initialized_; }

  int32_t StartRecording();
  int32_t StopRecording ();
  bool Recording() const { return recording_; }

  int32_t RecordingDelay(uint16_t& delayMS) const;

  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer);

  bool BuiltInAECIsAvailable() const;
  int32_t EnableBuiltInAEC(bool enable);
  int32_t RecordingDeviceName(uint16_t index,
                              char name[kAdmMaxDeviceNameSize],
                              char guid[kAdmMaxGuidSize]);

 private:
  // Called from Java side so we can cache the address of the Java-manged
  // |byte_buffer| in |direct_buffer_address_|. The size of the buffer
  // is also stored in |direct_buffer_capacity_in_bytes_|.
  // This method will be called by the WebRtcAudioRecord constructor, i.e.,
  // on the same thread that this object is created on.
  static void JNICALL CacheDirectBufferAddress(
    JNIEnv* env, jobject obj, jobject byte_buffer, jlong nativeAudioRecord);
  void OnCacheDirectBufferAddress(JNIEnv* env, jobject byte_buffer);

  // Called periodically by the Java based WebRtcAudioRecord object when
  // recording has started. Each call indicates that there are |length| new
  // bytes recorded in the memory area |direct_buffer_address_| and it is
  // now time to send these to the consumer.
  // This method is called on a high-priority thread from Java. The name of
  // the thread is 'AudioRecordThread'.
  static void JNICALL DataIsRecorded(
    JNIEnv* env, jobject obj, jint length, jlong nativeAudioRecord);
  void OnDataIsRecorded(int length);

  // Returns true if SetAndroidAudioDeviceObjects() has been called
  // successfully.
  bool HasDeviceObjects();

  // Called from the constructor. Defines the |j_audio_record_| member.
  void CreateJavaInstance();

  // Stores thread ID in constructor.
  // We can then use ThreadChecker::CalledOnValidThread() to ensure that
  // other methods are called from the same thread.
  // Currently only does DCHECK(thread_checker_.CalledOnValidThread()).
  rtc::ThreadChecker thread_checker_;

  // Stores thread ID in first call to OnDataIsRecorded() from high-priority
  // thread in Java. Detached during construction of this object.
  rtc::ThreadChecker thread_checker_java_;

  // Returns the current playout delay.
  // TODO(henrika): this value is currently fixed since initial tests have
  // shown that the estimated delay varies very little over time. It might be
  // possible to make improvements in this area.
  PlayoutDelayProvider* delay_provider_;

  // Contains audio parameters provided to this class at construction by the
  // AudioManager.
  const AudioParameters audio_parameters_;

  // The Java WebRtcAudioRecord instance.
  jobject j_audio_record_;

  // Cached copy of address to direct audio buffer owned by |j_audio_record_|.
  void* direct_buffer_address_;

  // Number of bytes in the direct audio buffer owned by |j_audio_record_|.
  int direct_buffer_capacity_in_bytes_;

  // Number audio frames per audio buffer. Each audio frame corresponds to
  // one sample of PCM mono data at 16 bits per sample. Hence, each audio
  // frame contains 2 bytes (given that the Java layer only supports mono).
  // Example: 480 for 48000 Hz or 441 for 44100 Hz.
  int frames_per_buffer_;

  bool initialized_;

  bool recording_;

  // Raw pointer handle provided to us in AttachAudioBuffer(). Owned by the
  // AudioDeviceModuleImpl class and called by AudioDeviceModuleImpl::Create().
  AudioDeviceBuffer* audio_device_buffer_;

  // Contains a delay estimate from the playout side given by |delay_provider_|.
  int playout_delay_in_milliseconds_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_RECORD_JNI_H_
