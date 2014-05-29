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

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/modules/audio_device/android/audio_common.h"
#include "webrtc/modules/audio_device/include/audio_device_defines.h"
#include "webrtc/modules/audio_device/audio_device_generic.h"

namespace webrtc {

class EventWrapper;
class ThreadWrapper;

const uint32_t N_PLAY_SAMPLES_PER_SEC = 16000; // Default is 16 kHz
const uint32_t N_PLAY_CHANNELS = 1; // default is mono playout

class AudioTrackJni : public PlayoutDelayProvider {
 public:
  static int32_t SetAndroidAudioDeviceObjects(void* javaVM, void* env,
                                              void* context);
  static void ClearAndroidAudioDeviceObjects();
  explicit AudioTrackJni(const int32_t id);
  virtual ~AudioTrackJni();

  // Main initializaton and termination
  int32_t Init();
  int32_t Terminate();
  bool Initialized() const { return _initialized; }

  // Device enumeration
  int16_t PlayoutDevices() { return 1; }  // There is one device only.

  int32_t PlayoutDeviceName(uint16_t index,
                            char name[kAdmMaxDeviceNameSize],
                            char guid[kAdmMaxGuidSize]);

  // Device selection
  int32_t SetPlayoutDevice(uint16_t index);
  int32_t SetPlayoutDevice(
      AudioDeviceModule::WindowsDeviceType device);

  // Audio transport initialization
  int32_t PlayoutIsAvailable(bool& available);  // NOLINT
  int32_t InitPlayout();
  bool PlayoutIsInitialized() const { return _playIsInitialized; }

  // Audio transport control
  int32_t StartPlayout();
  int32_t StopPlayout();
  bool Playing() const { return _playing; }

  // Audio mixer initialization
  int32_t SpeakerIsAvailable(bool& available);  // NOLINT
  int32_t InitSpeaker();
  bool SpeakerIsInitialized() const { return _speakerIsInitialized; }

  // Speaker volume controls
  int32_t SpeakerVolumeIsAvailable(bool& available);  // NOLINT
  int32_t SetSpeakerVolume(uint32_t volume);
  int32_t SpeakerVolume(uint32_t& volume) const;  // NOLINT
  int32_t MaxSpeakerVolume(uint32_t& maxVolume) const;  // NOLINT
  int32_t MinSpeakerVolume(uint32_t& minVolume) const;  // NOLINT
  int32_t SpeakerVolumeStepSize(uint16_t& stepSize) const;  // NOLINT

  // Speaker mute control
  int32_t SpeakerMuteIsAvailable(bool& available);  // NOLINT
  int32_t SetSpeakerMute(bool enable);
  int32_t SpeakerMute(bool& enabled) const;  // NOLINT


  // Stereo support
  int32_t StereoPlayoutIsAvailable(bool& available);  // NOLINT
  int32_t SetStereoPlayout(bool enable);
  int32_t StereoPlayout(bool& enabled) const;  // NOLINT

  // Delay information and control
  int32_t SetPlayoutBuffer(const AudioDeviceModule::BufferType type,
                           uint16_t sizeMS);
  int32_t PlayoutBuffer(AudioDeviceModule::BufferType& type,  // NOLINT
                        uint16_t& sizeMS) const;
  int32_t PlayoutDelay(uint16_t& delayMS) const;  // NOLINT

  // Attach audio buffer
  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer);

  int32_t SetPlayoutSampleRate(const uint32_t samplesPerSec);

  // Error and warning information
  bool PlayoutWarning() const;
  bool PlayoutError() const;
  void ClearPlayoutWarning();
  void ClearPlayoutError();

  // Speaker audio routing
  int32_t SetLoudspeakerStatus(bool enable);
  int32_t GetLoudspeakerStatus(bool& enable) const;  // NOLINT

 protected:
  virtual int PlayoutDelayMs() { return 0; }

 private:
  void Lock() EXCLUSIVE_LOCK_FUNCTION(_critSect) {
    _critSect.Enter();
  }
  void UnLock() UNLOCK_FUNCTION(_critSect) {
    _critSect.Leave();
  }

  int32_t InitJavaResources();
  int32_t InitSampleRate();

  static bool PlayThreadFunc(void*);
  bool PlayThreadProcess();

  // TODO(leozwang): Android holds only one JVM, all these jni handling
  // will be consolidated into a single place to make it consistant and
  // reliable. Chromium has a good example at base/android.
  static JavaVM* globalJvm;
  static JNIEnv* globalJNIEnv;
  static jobject globalContext;
  static jclass globalScClass;

  JavaVM* _javaVM; // denotes a Java VM
  JNIEnv* _jniEnvPlay; // The JNI env for playout thread
  jclass _javaScClass; // AudioDeviceAndroid class
  jobject _javaScObj; // AudioDeviceAndroid object
  jobject _javaPlayBuffer;
  void* _javaDirectPlayBuffer; // Direct buffer pointer to play buffer
  jmethodID _javaMidPlayAudio; // Method ID of play in AudioDeviceAndroid

  AudioDeviceBuffer* _ptrAudioBuffer;
  CriticalSectionWrapper& _critSect;
  int32_t _id;
  bool _initialized;

  EventWrapper& _timeEventPlay;
  EventWrapper& _playStartStopEvent;
  ThreadWrapper* _ptrThreadPlay;
  uint32_t _playThreadID;
  bool _playThreadIsInitialized;
  bool _shutdownPlayThread;
  bool _playoutDeviceIsSpecified;

  bool _playing;
  bool _playIsInitialized;
  bool _speakerIsInitialized;

  bool _startPlay;

  uint16_t _playWarning;
  uint16_t _playError;

  uint16_t _delayPlayout;

  uint16_t _samplingFreqOut; // Sampling frequency for Speaker
  uint32_t _maxSpeakerVolume; // The maximum speaker volume value
  bool _loudSpeakerOn;

};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_TRACK_JNI_H_
