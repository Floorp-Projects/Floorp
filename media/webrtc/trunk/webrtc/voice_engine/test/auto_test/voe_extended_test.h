/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_EXTENDED_TEST_H
#define WEBRTC_VOICE_ENGINE_VOE_EXTENDED_TEST_H

#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/event_wrapper.h"
#include "webrtc/system_wrappers/interface/ref_count.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/system_wrappers/interface/thread_wrapper.h"
#include "webrtc/test/channel_transport/include/channel_transport.h"
#include "webrtc/voice_engine/test/auto_test/voe_standard_test.h"

namespace voetest {

class VoETestManager;

// ----------------------------------------------------------------------------
//  AudioDeviceModule
//
//  Implementation of the ADM to be used as external ADM in VoiceEngine.
//  This implementation is only a mock class, i.e., it does not provide
//  any real audio support.
// ----------------------------------------------------------------------------

class AudioDeviceModuleImpl : public AudioDeviceModule {
 public:
  // Factory methods
  static AudioDeviceModuleImpl* Create();
  static bool Destroy(AudioDeviceModuleImpl* adm);

  // Helper methods which allows us to get some handy information about
  // this mock implementation.
  int32_t ReferenceCounter() const {
    return _ref_count;
  }

  // RefCountedModule implementation (mocks default implementation)
  virtual int32_t AddRef();
  virtual int32_t Release();

  // Module implementation
  virtual int32_t Version(char* version,
                          uint32_t& remaining_buffer_in_bytes,
                          uint32_t& position) const {
    return 0;
  }
  virtual int32_t ChangeUniqueId(int32_t id) {
    return 0;
  }
  virtual int32_t TimeUntilNextProcess() {
    return -1;
  }
  virtual int32_t Process() {
    return 0;
  }

  // AudioDeviceModule implementation
  virtual int32_t ActiveAudioLayer(AudioLayer* audioLayer) const {
    return 0;
  }

  virtual ErrorCode LastError() const {
    return static_cast<ErrorCode> (0);
  }
  virtual int32_t RegisterEventObserver(AudioDeviceObserver* eventCallback) {
    return 0;
  }

  virtual int32_t RegisterAudioCallback(AudioTransport* audioCallback) {
    return 0;
  }

  virtual int32_t Init() {
    return 0;
  }
  virtual int32_t Terminate() {
    return 0;
  }
  virtual bool Initialized() const {
    return true;
  }

  virtual int16_t PlayoutDevices() {
    return -1;
  }
  virtual int16_t RecordingDevices() {
    return -1;
  }
  virtual int32_t PlayoutDeviceName(uint16_t index,
                                    char name[kAdmMaxDeviceNameSize],
                                    char guid[kAdmMaxGuidSize]) {
    return -1;
  }
  virtual int32_t RecordingDeviceName(uint16_t index,
                                      char name[kAdmMaxDeviceNameSize],
                                      char guid[kAdmMaxGuidSize]) {
    return -1;
  }

  virtual int32_t SetPlayoutDevice(uint16_t index) {
    return 0;
  }
  virtual int32_t SetPlayoutDevice(WindowsDeviceType device) {
    return 0;
  }
  virtual int32_t SetRecordingDevice(uint16_t index) {
    return 0;
  }
  virtual int32_t SetRecordingDevice(WindowsDeviceType device) {
    return 0;
  }

  virtual int32_t PlayoutIsAvailable(bool* available) {
    *available = true;
    return 0;
  }
  virtual int32_t InitPlayout() {
    return 0;
  }
  virtual bool PlayoutIsInitialized() const {
    return true;
  }
  virtual int32_t RecordingIsAvailable(bool* available) {
    *available = true;
    return 0;
  }
  virtual int32_t InitRecording() {
    return 0;
  }
  virtual bool RecordingIsInitialized() const {
    return true;
  }

  virtual int32_t StartPlayout() {
    return 0;
  }
  virtual int32_t StopPlayout() {
    return 0;
  }
  virtual bool Playing() const {
    return true;
  }
  virtual int32_t StartRecording() {
    return 0;
  }
  virtual int32_t StopRecording() {
    return 0;
  }
  virtual bool Recording() const {
    return true;
  }

  virtual int32_t SetAGC(bool enable) {
    return -1;
  }
  virtual bool AGC() const {
    return false;
  }

  virtual int32_t SetWaveOutVolume(uint16_t volumeLeft,
                                   uint16_t volumeRight) {
    return -1;
  }
  virtual int32_t WaveOutVolume(uint16_t* volumeLeft,
                                uint16_t* volumeRight) const {
    return -1;
  }

  virtual int32_t SpeakerIsAvailable(bool* available) {
    *available = true;
    return 0;
  }
  virtual int32_t InitSpeaker() {
    return 0;
  }
  virtual bool SpeakerIsInitialized() const {
    return true;
  }
  virtual int32_t MicrophoneIsAvailable(bool* available) {
    *available = true;
    return 0;
  }
  virtual int32_t InitMicrophone() {
    return 0;
  }
  virtual bool MicrophoneIsInitialized() const {
    return true;
  }

  virtual int32_t SpeakerVolumeIsAvailable(bool* available) {
    return -1;
  }
  virtual int32_t SetSpeakerVolume(uint32_t volume) {
    return -1;
  }
  virtual int32_t SpeakerVolume(uint32_t* volume) const {
    return -1;
  }
  virtual int32_t MaxSpeakerVolume(uint32_t* maxVolume) const {
    return -1;
  }
  virtual int32_t MinSpeakerVolume(uint32_t* minVolume) const {
    return -1;
  }
  virtual int32_t SpeakerVolumeStepSize(uint16_t* stepSize) const {
    return -1;
  }

  virtual int32_t MicrophoneVolumeIsAvailable(bool* available) {
    return -1;
  }
  virtual int32_t SetMicrophoneVolume(uint32_t volume) {
    return -1;
  }
  virtual int32_t MicrophoneVolume(uint32_t* volume) const {
    return -1;
  }
  virtual int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const {
    return -1;
  }
  virtual int32_t MinMicrophoneVolume(uint32_t* minVolume) const {
    return -1;
  }
  virtual int32_t MicrophoneVolumeStepSize(uint16_t* stepSize) const {
    return -1;
  }

  virtual int32_t SpeakerMuteIsAvailable(bool* available) {
    return -1;
  }
  virtual int32_t SetSpeakerMute(bool enable) {
    return -1;
  }
  virtual int32_t SpeakerMute(bool* enabled) const {
    return -1;
  }

  virtual int32_t MicrophoneMuteIsAvailable(bool* available) {
    return -1;
  }
  virtual int32_t SetMicrophoneMute(bool enable) {
    return -1;
  }
  virtual int32_t MicrophoneMute(bool* enabled) const {
    return -1;
  }

  virtual int32_t MicrophoneBoostIsAvailable(bool* available) {
    return -1;
  }
  virtual int32_t SetMicrophoneBoost(bool enable) {
    return -1;
  }
  virtual int32_t MicrophoneBoost(bool* enabled) const {
    return -1;
  }

  virtual int32_t StereoPlayoutIsAvailable(bool* available) const {
    return -1;
  }
  virtual int32_t SetStereoPlayout(bool enable) {
    return -1;
  }
  virtual int32_t StereoPlayout(bool* enabled) const {
    return -1;
  }
  virtual int32_t StereoRecordingIsAvailable(bool* available) const {
    return -1;
  }
  virtual int32_t SetStereoRecording(bool enable) {
    return -1;
  }
  virtual int32_t StereoRecording(bool* enabled) const {
    return -1;
  }
  virtual int32_t SetRecordingChannel(ChannelType channel) {
    return -1;
  }
  virtual int32_t RecordingChannel(ChannelType* channel) const {
    return -1;
  }

  virtual int32_t SetPlayoutBuffer(BufferType type, uint16_t sizeMS = 0) {
    return -1;
  }
  virtual int32_t PlayoutBuffer(BufferType* type, uint16_t* sizeMS) const {
    return -1;
  }
  virtual int32_t PlayoutDelay(uint16_t* delayMS) const {
    return -1;
  }
  virtual int32_t RecordingDelay(uint16_t* delayMS) const {
    return -1;
  }

  virtual int32_t CPULoad(uint16_t* load) const {
    return -1;
  }

  virtual int32_t StartRawOutputFileRecording(
      const char pcmFileNameUTF8[kAdmMaxFileNameSize]) {
    return -1;
  }
  virtual int32_t StopRawOutputFileRecording() {
    return -1;
  }
  virtual int32_t StartRawInputFileRecording(
      const char pcmFileNameUTF8[kAdmMaxFileNameSize]) {
    return -1;
  }
  virtual int32_t StopRawInputFileRecording() {
    return -1;
  }

  virtual int32_t SetRecordingSampleRate(uint32_t samplesPerSec) {
    return -1;
  }
  virtual int32_t RecordingSampleRate(uint32_t* samplesPerSec) const {
    return -1;
  }
  virtual int32_t SetPlayoutSampleRate(uint32_t samplesPerSec) {
    return -1;
  }
  virtual int32_t PlayoutSampleRate(uint32_t* samplesPerSec) const {
    return -1;
  }

  virtual int32_t ResetAudioDevice() {
    return -1;
  }
  virtual int32_t SetLoudspeakerStatus(bool enable) {
    return -1;
  }
  virtual int32_t GetLoudspeakerStatus(bool* enabled) const {
    return -1;
  }

 protected:
  AudioDeviceModuleImpl();
  ~AudioDeviceModuleImpl();

 private:
  volatile int32_t _ref_count;
};

// ----------------------------------------------------------------------------
//	Transport
// ----------------------------------------------------------------------------

class ExtendedTestTransport : public Transport {
 public:
  ExtendedTestTransport(VoENetwork* ptr);
  ~ExtendedTestTransport();
  VoENetwork* myNetw;

 protected:
  virtual int SendPacket(int channel, const void *data, int len);
  virtual int SendRTCPPacket(int channel, const void *data, int len);

 private:
  static bool Run(void* ptr);
  bool Process();

 private:
  ThreadWrapper* _thread;
  CriticalSectionWrapper* _lock;
  EventWrapper* _event;

 private:
  unsigned char _packetBuffer[1612];
  int _length;
  int _channel;
};

class XTransport : public Transport {
 public:
  XTransport(VoENetwork* netw, VoEFile* file);
  VoENetwork* _netw;
  VoEFile* _file;

 public:
  virtual int SendPacket(int channel, const void *data, int len);
  virtual int SendRTCPPacket(int channel, const void *data, int len);
};

class XRTPObserver : public VoERTPObserver {
 public:
  XRTPObserver();
  ~XRTPObserver();
  virtual void OnIncomingCSRCChanged(int channel,
                                     unsigned int CSRC,
                                     bool added);
  virtual void OnIncomingSSRCChanged(int channel,
                                     unsigned int SSRC);
 public:
  unsigned int _SSRC;
};

// ----------------------------------------------------------------------------
//	VoEExtendedTest
// ----------------------------------------------------------------------------

class VoEExtendedTest : public VoiceEngineObserver,
                        public VoEConnectionObserver {
 public:
  VoEExtendedTest(VoETestManager& mgr);
  ~VoEExtendedTest();
  int PrepareTest(const char* str) const;
  int TestPassed(const char* str) const;
  int TestBase();
  int TestCallReport();
  int TestCodec();
  int TestDtmf();
  int TestEncryption();
  int TestExternalMedia();
  int TestFile();
  int TestMixing();
  int TestHardware();
  int TestNetEqStats();
  int TestNetwork();
  int TestRTP_RTCP();
  int TestVideoSync();
  int TestVolumeControl();

  int ErrorCode() const {
    return _errCode;
  }
  void ClearErrorCode() {
    _errCode = 0;
  }

 protected:
  // from VoiceEngineObserver
  void CallbackOnError(int errCode, int channel);
  void CallbackOnTrace(TraceLevel level, const char* message,
                       int length);

  // from VoEConnectionObserver
  void OnPeriodicDeadOrAlive(int channel, bool alive);

 private:
  void Play(int channel, unsigned int timeMillisec,
            bool addFileAsMicrophone = false, bool addTimeMarker = false);
  void Sleep(unsigned int timeMillisec, bool addMarker = false);
  void StartMedia(int channel, int rtpPort, bool listen, bool playout,
                  bool send);
  void StopMedia(int channel);
  int RunMixingTest(int num_remote_channels, int num_local_channels,
                    int16_t input_value, int16_t max_output_value,
                    int16_t min_output_value);

  VoETestManager& _mgr;
  int _errCode;
  bool _alive;
  bool _listening[32];
  scoped_ptr<webrtc::test::VoiceChannelTransport> voice_channel_transports_[32];
  bool _playing[32];
  bool _sending[32];
};

} //  namespace voetest
#endif // WEBRTC_VOICE_ENGINE_VOE_EXTENDED_TEST_H
