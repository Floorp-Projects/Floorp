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

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/modules/audio_device/include/audio_device_defines.h"
#include "webrtc/modules/audio_device/audio_device_generic.h"

namespace webrtc {

class EventWrapper;
class ThreadWrapper;
class PlayoutDelayProvider;

class AudioRecordJni {
 public:
  static int32_t SetAndroidAudioDeviceObjects(void* javaVM, void* env,
                                              void* context);

  static int32_t SetAndroidAudioDeviceObjects(void* javaVM,
                                              void* context);

  static void ClearAndroidAudioDeviceObjects();

  AudioRecordJni(const int32_t id, PlayoutDelayProvider* delay_provider);
  ~AudioRecordJni();

  // Main initializaton and termination
  int32_t Init();
  int32_t Terminate();
  bool Initialized() const { return _initialized; }

  // Device enumeration
  int16_t RecordingDevices() { return 1; }  // There is one device only
  int32_t RecordingDeviceName(uint16_t index,
                              char name[kAdmMaxDeviceNameSize],
                              char guid[kAdmMaxGuidSize]);

  // Device selection
  int32_t SetRecordingDevice(uint16_t index);
  int32_t SetRecordingDevice(
      AudioDeviceModule::WindowsDeviceType device);

  // Audio transport initialization
  int32_t RecordingIsAvailable(bool& available);  // NOLINT
  int32_t InitRecording();
  bool RecordingIsInitialized() const { return _recIsInitialized; }

  // Audio transport control
  int32_t StartRecording();
  int32_t StopRecording();
  bool Recording() const { return _recording; }

  // Microphone Automatic Gain Control (AGC)
  int32_t SetAGC(bool enable);
  bool AGC() const { return _AGC; }

  // Audio mixer initialization
  int32_t InitMicrophone();
  bool MicrophoneIsInitialized() const { return _micIsInitialized; }

  // Microphone volume controls
  int32_t MicrophoneVolumeIsAvailable(bool& available);  // NOLINT
  // TODO(leozwang): Add microphone volume control when OpenSL APIs
  // are available.
  int32_t SetMicrophoneVolume(uint32_t volume);
  int32_t MicrophoneVolume(uint32_t& volume) const;  // NOLINT
  int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const;  // NOLINT
  int32_t MinMicrophoneVolume(uint32_t& minVolume) const;  // NOLINT
  int32_t MicrophoneVolumeStepSize(
      uint16_t& stepSize) const;  // NOLINT

  // Microphone mute control
  int32_t MicrophoneMuteIsAvailable(bool& available);  // NOLINT
  int32_t SetMicrophoneMute(bool enable);
  int32_t MicrophoneMute(bool& enabled) const;  // NOLINT

  // Microphone boost control
  int32_t MicrophoneBoostIsAvailable(bool& available);  // NOLINT
  int32_t SetMicrophoneBoost(bool enable);
  int32_t MicrophoneBoost(bool& enabled) const;  // NOLINT

  // Stereo support
  int32_t StereoRecordingIsAvailable(bool& available);  // NOLINT
  int32_t SetStereoRecording(bool enable);
  int32_t StereoRecording(bool& enabled) const;  // NOLINT

  // Delay information and control
  int32_t RecordingDelay(uint16_t& delayMS) const;  // NOLINT

  bool RecordingWarning() const;
  bool RecordingError() const;
  void ClearRecordingWarning();
  void ClearRecordingError();

  // Attach audio buffer
  void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer);

  int32_t SetRecordingSampleRate(const uint32_t samplesPerSec);

  static const uint32_t N_REC_SAMPLES_PER_SEC = 16000; // Default is 16 kHz
  static const uint32_t N_REC_CHANNELS = 1; // default is mono recording
  static const uint32_t REC_BUF_SIZE_IN_SAMPLES = 480; // Handle max 10 ms @ 48 kHz

 private:
  void Lock() EXCLUSIVE_LOCK_FUNCTION(_critSect) {
    _critSect.Enter();
  }
  void UnLock() UNLOCK_FUNCTION(_critSect) {
    _critSect.Leave();
  }

  int32_t InitJavaResources();
  int32_t InitSampleRate();

  static bool RecThreadFunc(void*);
  bool RecThreadProcess();

  // TODO(leozwang): Android holds only one JVM, all these jni handling
  // will be consolidated into a single place to make it consistant and
  // reliable. Chromium has a good example at base/android.
  static JavaVM* globalJvm;
  static JNIEnv* globalJNIEnv;
  static jobject globalContext;
  static jclass globalScClass;

  JavaVM* _javaVM; // denotes a Java VM
  JNIEnv* _jniEnvRec; // The JNI env for recording thread
  jclass _javaScClass; // AudioDeviceAndroid class
  jobject _javaScObj; // AudioDeviceAndroid object
  jobject _javaRecBuffer;
  void* _javaDirectRecBuffer; // Direct buffer pointer to rec buffer
  jmethodID _javaMidRecAudio; // Method ID of rec in AudioDeviceAndroid

  AudioDeviceBuffer* _ptrAudioBuffer;
  CriticalSectionWrapper& _critSect;
  int32_t _id;
  PlayoutDelayProvider* _delay_provider;
  bool _initialized;

  EventWrapper& _timeEventRec;
  EventWrapper& _recStartStopEvent;
  ThreadWrapper* _ptrThreadRec;
  uint32_t _recThreadID;
  bool _recThreadIsInitialized;
  bool _shutdownRecThread;

  int8_t _recBuffer[2 * REC_BUF_SIZE_IN_SAMPLES];
  bool _recordingDeviceIsSpecified;

  bool _recording;
  bool _recIsInitialized;
  bool _micIsInitialized;

  bool _startRec;

  uint16_t _recWarning;
  uint16_t _recError;

  uint16_t _delayRecording;

  bool _AGC;

  uint16_t _samplingFreqIn; // Sampling frequency for Mic
  int _recAudioSource;

};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_DEVICE_ANDROID_AUDIO_RECORD_JNI_H_
