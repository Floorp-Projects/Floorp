/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_DEVICE_FILE_AUDIO_DEVICE_H
#define WEBRTC_AUDIO_DEVICE_FILE_AUDIO_DEVICE_H

#include <stdio.h>

#include <string>

#include "webrtc/modules/audio_device/audio_device_generic.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/file_wrapper.h"
#include "webrtc/system_wrappers/interface/clock.h"

namespace webrtc {
class EventWrapper;
class ThreadWrapper;

// This is a fake audio device which plays audio from a file as its microphone
// and plays out into a file.
class FileAudioDevice : public AudioDeviceGeneric {
 public:
  // Constructs a file audio device with |id|. It will read audio from
  // |inputFilename| and record output audio to |outputFilename|.
  //
  // The input file should be a readable 48k stereo raw file, and the output
  // file should point to a writable location. The output format will also be
  // 48k stereo raw audio.
  FileAudioDevice(const int32_t id,
                  const char* inputFilename,
                  const char* outputFilename);
  virtual ~FileAudioDevice();

  // Retrieve the currently utilized audio layer
  virtual int32_t ActiveAudioLayer(
      AudioDeviceModule::AudioLayer& audioLayer) const OVERRIDE;

  // Main initializaton and termination
  virtual int32_t Init() OVERRIDE;
  virtual int32_t Terminate() OVERRIDE;
  virtual bool Initialized() const OVERRIDE;

  // Device enumeration
  virtual int16_t PlayoutDevices() OVERRIDE;
  virtual int16_t RecordingDevices() OVERRIDE;
  virtual int32_t PlayoutDeviceName(uint16_t index,
                                    char name[kAdmMaxDeviceNameSize],
                                    char guid[kAdmMaxGuidSize]) OVERRIDE;
  virtual int32_t RecordingDeviceName(uint16_t index,
                                      char name[kAdmMaxDeviceNameSize],
                                      char guid[kAdmMaxGuidSize]) OVERRIDE;

  // Device selection
  virtual int32_t SetPlayoutDevice(uint16_t index) OVERRIDE;
  virtual int32_t SetPlayoutDevice(
      AudioDeviceModule::WindowsDeviceType device) OVERRIDE;
  virtual int32_t SetRecordingDevice(uint16_t index) OVERRIDE;
  virtual int32_t SetRecordingDevice(
      AudioDeviceModule::WindowsDeviceType device) OVERRIDE;

  // Audio transport initialization
  virtual int32_t PlayoutIsAvailable(bool& available) OVERRIDE;
  virtual int32_t InitPlayout() OVERRIDE;
  virtual bool PlayoutIsInitialized() const OVERRIDE;
  virtual int32_t RecordingIsAvailable(bool& available) OVERRIDE;
  virtual int32_t InitRecording() OVERRIDE;
  virtual bool RecordingIsInitialized() const OVERRIDE;

  // Audio transport control
  virtual int32_t StartPlayout() OVERRIDE;
  virtual int32_t StopPlayout() OVERRIDE;
  virtual bool Playing() const OVERRIDE;
  virtual int32_t StartRecording() OVERRIDE;
  virtual int32_t StopRecording() OVERRIDE;
  virtual bool Recording() const OVERRIDE;

  // Microphone Automatic Gain Control (AGC)
  virtual int32_t SetAGC(bool enable) OVERRIDE;
  virtual bool AGC() const OVERRIDE;

  // Volume control based on the Windows Wave API (Windows only)
  virtual int32_t SetWaveOutVolume(uint16_t volumeLeft,
                                   uint16_t volumeRight) OVERRIDE;
  virtual int32_t WaveOutVolume(uint16_t& volumeLeft,
                                uint16_t& volumeRight) const OVERRIDE;

  // Audio mixer initialization
  virtual int32_t InitSpeaker() OVERRIDE;
  virtual bool SpeakerIsInitialized() const OVERRIDE;
  virtual int32_t InitMicrophone() OVERRIDE;
  virtual bool MicrophoneIsInitialized() const OVERRIDE;

  // Speaker volume controls
  virtual int32_t SpeakerVolumeIsAvailable(bool& available) OVERRIDE;
  virtual int32_t SetSpeakerVolume(uint32_t volume) OVERRIDE;
  virtual int32_t SpeakerVolume(uint32_t& volume) const OVERRIDE;
  virtual int32_t MaxSpeakerVolume(uint32_t& maxVolume) const OVERRIDE;
  virtual int32_t MinSpeakerVolume(uint32_t& minVolume) const OVERRIDE;
  virtual int32_t SpeakerVolumeStepSize(uint16_t& stepSize) const OVERRIDE;

  // Microphone volume controls
  virtual int32_t MicrophoneVolumeIsAvailable(bool& available) OVERRIDE;
  virtual int32_t SetMicrophoneVolume(uint32_t volume) OVERRIDE;
  virtual int32_t MicrophoneVolume(uint32_t& volume) const OVERRIDE;
  virtual int32_t MaxMicrophoneVolume(uint32_t& maxVolume) const OVERRIDE;
  virtual int32_t MinMicrophoneVolume(uint32_t& minVolume) const OVERRIDE;
  virtual int32_t MicrophoneVolumeStepSize(uint16_t& stepSize) const OVERRIDE;

  // Speaker mute control
  virtual int32_t SpeakerMuteIsAvailable(bool& available) OVERRIDE;
  virtual int32_t SetSpeakerMute(bool enable) OVERRIDE;
  virtual int32_t SpeakerMute(bool& enabled) const OVERRIDE;

  // Microphone mute control
  virtual int32_t MicrophoneMuteIsAvailable(bool& available) OVERRIDE;
  virtual int32_t SetMicrophoneMute(bool enable) OVERRIDE;
  virtual int32_t MicrophoneMute(bool& enabled) const OVERRIDE;

  // Microphone boost control
  virtual int32_t MicrophoneBoostIsAvailable(bool& available) OVERRIDE;
  virtual int32_t SetMicrophoneBoost(bool enable) OVERRIDE;
  virtual int32_t MicrophoneBoost(bool& enabled) const OVERRIDE;

  // Stereo support
  virtual int32_t StereoPlayoutIsAvailable(bool& available) OVERRIDE;
  virtual int32_t SetStereoPlayout(bool enable) OVERRIDE;
  virtual int32_t StereoPlayout(bool& enabled) const OVERRIDE;
  virtual int32_t StereoRecordingIsAvailable(bool& available) OVERRIDE;
  virtual int32_t SetStereoRecording(bool enable) OVERRIDE;
  virtual int32_t StereoRecording(bool& enabled) const OVERRIDE;

  // Delay information and control
  virtual int32_t SetPlayoutBuffer(const AudioDeviceModule::BufferType type,
                                   uint16_t sizeMS) OVERRIDE;
  virtual int32_t PlayoutBuffer(AudioDeviceModule::BufferType& type,
                                uint16_t& sizeMS) const OVERRIDE;
  virtual int32_t PlayoutDelay(uint16_t& delayMS) const OVERRIDE;
  virtual int32_t RecordingDelay(uint16_t& delayMS) const OVERRIDE;

  // CPU load
  virtual int32_t CPULoad(uint16_t& load) const OVERRIDE;

  virtual bool PlayoutWarning() const OVERRIDE;
  virtual bool PlayoutError() const OVERRIDE;
  virtual bool RecordingWarning() const OVERRIDE;
  virtual bool RecordingError() const OVERRIDE;
  virtual void ClearPlayoutWarning() OVERRIDE;
  virtual void ClearPlayoutError() OVERRIDE;
  virtual void ClearRecordingWarning() OVERRIDE;
  virtual void ClearRecordingError() OVERRIDE;

  virtual void AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) OVERRIDE;

 private:
  static bool RecThreadFunc(void*);
  static bool PlayThreadFunc(void*);
  bool RecThreadProcess();
  bool PlayThreadProcess();

  int32_t _playout_index;
  int32_t _record_index;
  AudioDeviceModule::BufferType _playBufType;
  AudioDeviceBuffer* _ptrAudioBuffer;
  int8_t* _recordingBuffer;  // In bytes.
  int8_t* _playoutBuffer;  // In bytes.
  uint32_t _recordingFramesLeft;
  uint32_t _playoutFramesLeft;
  CriticalSectionWrapper& _critSect;

  uint32_t _recordingBufferSizeIn10MS;
  uint32_t _recordingFramesIn10MS;
  uint32_t _playoutFramesIn10MS;

  ThreadWrapper* _ptrThreadRec;
  ThreadWrapper* _ptrThreadPlay;
  uint32_t _recThreadID;
  uint32_t _playThreadID;

  bool _playing;
  bool _recording;
  uint64_t _lastCallPlayoutMillis;
  uint64_t _lastCallRecordMillis;

  FileWrapper& _outputFile;
  FileWrapper& _inputFile;
  std::string _outputFilename;
  std::string _inputFilename;

  Clock* _clock;
};

}  // namespace webrtc

#endif  // WEBRTC_AUDIO_DEVICE_FILE_AUDIO_DEVICE_H
