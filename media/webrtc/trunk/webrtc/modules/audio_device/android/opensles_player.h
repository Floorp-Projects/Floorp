/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_OPENSLES_PLAYER_H_
#define WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_OPENSLES_PLAYER_H_

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <SLES/OpenSLES_AndroidConfiguration.h>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/audio_device/android/audio_common.h"
#include "webrtc/modules/audio_device/android/audio_manager.h"
#include "webrtc/modules/audio_device/android/opensles_common.h"
#include "webrtc/modules/audio_device/include/audio_device_defines.h"
#include "webrtc/modules/audio_device/audio_device_generic.h"
#include "webrtc/modules/utility/include/helpers_android.h"

namespace webrtc {

class FineAudioBuffer;

// Implements 16-bit mono PCM audio output support for Android using the
// C based OpenSL ES API. No calls from C/C++ to Java using JNI is done.
//
// An instance must be created and destroyed on one and the same thread.
// All public methods must also be called on the same thread. A thread checker
// will RTC_DCHECK if any method is called on an invalid thread. Decoded audio
// buffers are requested on a dedicated internal thread managed by the OpenSL
// ES layer.
//
// The existing design forces the user to call InitPlayout() after Stoplayout()
// to be able to call StartPlayout() again. This is inline with how the Java-
// based implementation works.
//
// OpenSL ES is a native C API which have no Dalvik-related overhead such as
// garbage collection pauses and it supports reduced audio output latency.
// If the device doesn't claim this feature but supports API level 9 (Android
// platform version 2.3) or later, then we can still use the OpenSL ES APIs but
// the output latency may be higher.
class OpenSLESPlayer {
 public:
  // The lower output latency path is used only if the application requests a
  // buffer count of 2 or more, and a buffer size and sample rate that are
  // compatible with the device's native output configuration provided via the
  // audio manager at construction.
  static const int kNumOfOpenSLESBuffers = 4;

  // There is no need for this class to use JNI.
  static int32_t SetAndroidAudioDeviceObjects(void* javaVM, void* context) {
    return 0;
  }
  static void ClearAndroidAudioDeviceObjects() {}

  explicit OpenSLESPlayer(AudioManager* audio_manager);
  ~OpenSLESPlayer();

  int Init();
  int Terminate();

  int InitPlayout();
  bool PlayoutIsInitialized() const { return initialized_; }

  int StartPlayout();
  int StopPlayout();
  bool Playing() const { return playing_; }

  int SpeakerVolumeIsAvailable(bool& available);
  int SetSpeakerVolume(uint32_t volume);
  int SpeakerVolume(uint32_t& volume) const;
  int MaxSpeakerVolume(uint32_t& maxVolume) const;
  int MinSpeakerVolume(uint32_t& minVolume) const;

  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer);

 private:
  // These callback methods are called when data is required for playout.
  // They are both called from an internal "OpenSL ES thread" which is not
  // attached to the Dalvik VM.
  static void SimpleBufferQueueCallback(SLAndroidSimpleBufferQueueItf caller,
                                        void* context);
  void FillBufferQueue();
  // Reads audio data in PCM format using the AudioDeviceBuffer.
  // Can be called both on the main thread (during Start()) and from the
  // internal audio thread while output streaming is active.
  void EnqueuePlayoutData();

  // Configures the SL_DATAFORMAT_PCM structure.
  SLDataFormat_PCM CreatePCMConfiguration(size_t channels,
                                          int sample_rate,
                                          size_t bits_per_sample);

  // Allocate memory for audio buffers which will be used to render audio
  // via the SLAndroidSimpleBufferQueueItf interface.
  void AllocateDataBuffers();

  // Creates/destroys the main engine object and the SLEngineItf interface.
  bool CreateEngine();
  void DestroyEngine();

  // Creates/destroys the output mix object.
  bool CreateMix();
  void DestroyMix();

  // Creates/destroys the audio player and the simple-buffer object.
  // Also creates the volume object.
  bool CreateAudioPlayer();
  void DestroyAudioPlayer();

  SLuint32 GetPlayState() const;

  // Ensures that methods are called from the same thread as this object is
  // created on.
  rtc::ThreadChecker thread_checker_;

  // Stores thread ID in first call to SimpleBufferQueueCallback() from internal
  // non-application thread which is not attached to the Dalvik JVM.
  // Detached during construction of this object.
  rtc::ThreadChecker thread_checker_opensles_;

  // Contains audio parameters provided to this class at construction by the
  // AudioManager.
  const AudioParameters audio_parameters_;

  // Raw pointer handle provided to us in AttachAudioBuffer(). Owned by the
  // AudioDeviceModuleImpl class and called by AudioDeviceModuleImpl::Create().
  AudioDeviceBuffer* audio_device_buffer_;

  bool initialized_;
  bool playing_;

  // PCM-type format definition.
  // TODO(henrika): add support for SLAndroidDataFormat_PCM_EX (android-21) if
  // 32-bit float representation is needed.
  SLDataFormat_PCM pcm_format_;

  // Number of bytes per audio buffer in each |audio_buffers_[i]|.
  // Typical sizes are 480 or 512 bytes corresponding to native output buffer
  // sizes of 240 or 256 audio frames respectively.
  size_t bytes_per_buffer_;

  // Queue of audio buffers to be used by the player object for rendering
  // audio. They will be used in a Round-robin way and the size of each buffer
  // is given by FineAudioBuffer::RequiredBufferSizeBytes().
  rtc::scoped_ptr<SLint8[]> audio_buffers_[kNumOfOpenSLESBuffers];

  // FineAudioBuffer takes an AudioDeviceBuffer which delivers audio data
  // in chunks of 10ms. It then allows for this data to be pulled in
  // a finer or coarser granularity. I.e. interacting with this class instead
  // of directly with the AudioDeviceBuffer one can ask for any number of
  // audio data samples.
  // Example: native buffer size is 240 audio frames at 48kHz sample rate.
  // WebRTC will provide 480 audio frames per 10ms but OpenSL ES asks for 240
  // in each callback (one every 5ms). This class can then ask for 240 and the
  // FineAudioBuffer will ask WebRTC for new data only every second callback
  // and also cach non-utilized audio.
  rtc::scoped_ptr<FineAudioBuffer> fine_buffer_;

  // Keeps track of active audio buffer 'n' in the audio_buffers_[n] queue.
  // Example (kNumOfOpenSLESBuffers = 2): counts 0, 1, 0, 1, ...
  int buffer_index_;

  // The engine object which provides the SLEngineItf interface.
  // Created by the global Open SL ES constructor slCreateEngine().
  SLObjectItf engine_object_;

  // This interface exposes creation methods for all the OpenSL ES object types.
  // It is the OpenSL ES API entry point.
  SLEngineItf engine_;

  // Output mix object to be used by the player object.
  webrtc::ScopedSLObjectItf output_mix_;

  // The audio player media object plays out audio to the speakers. It also
  // supports volume control.
  webrtc::ScopedSLObjectItf player_object_;

  // This interface is supported on the audio player and it controls the state
  // of the audio player.
  SLPlayItf player_;

  // The Android Simple Buffer Queue interface is supported on the audio player
  // and it provides methods to send audio data from the source to the audio
  // player for rendering.
  SLAndroidSimpleBufferQueueItf simple_buffer_queue_;

  // This interface exposes controls for manipulating the object’s audio volume
  // properties. This interface is supported on the Audio Player object.
  SLVolumeItf volume_;

  // Last time the OpenSL ES layer asked for audio data to play out.
  uint32_t last_play_time_;

  void *opensles_lib_;
  typedef SLresult (*slCreateEngine_t)(SLObjectItf *,
                                       SLuint32,
                                       const SLEngineOption *,
                                       SLuint32,
                                       const SLInterfaceID *,
                                       const SLboolean *);
  slCreateEngine_t slCreateEngine_;
  SLInterfaceID SL_IID_ENGINE_;
  SLInterfaceID SL_IID_ANDROIDCONFIGURATION_;
  SLInterfaceID SL_IID_BUFFERQUEUE_;
  SLInterfaceID SL_IID_VOLUME_;
  SLInterfaceID SL_IID_PLAY_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_OPENSLES_PLAYER_H_
