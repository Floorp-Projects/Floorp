/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/include/voe_codec.h"

#include "gtest/gtest.h"
#include "webrtc/modules/audio_device/include/audio_device.h"
#include "webrtc/modules/audio_device/include/audio_device_defines.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_hardware.h"
#include "webrtc/voice_engine/voice_engine_defines.h"

namespace webrtc {
namespace voe {
namespace {


class FakeAudioDeviceModule : public AudioDeviceModule {
 public:
  FakeAudioDeviceModule() {}
  ~FakeAudioDeviceModule() {}
  virtual int32_t AddRef() { return 0; }
  virtual int32_t Release() { return 0; }
  virtual int32_t RegisterEventObserver(AudioDeviceObserver* eventCallback) {
    return 0;
  }
  virtual int32_t RegisterAudioCallback(AudioTransport* audioCallback) {
    return 0;
  }
  virtual int32_t Init() { return 0; }
  virtual int32_t SpeakerIsAvailable(bool* available) {
    *available = true;
    return 0;
  }
  virtual int32_t InitSpeaker() { return 0; }
  virtual int32_t SetPlayoutDevice(uint16_t index) { return 0; }
  virtual int32_t SetPlayoutDevice(WindowsDeviceType device) { return 0; }
  virtual int32_t SetStereoPlayout(bool enable) { return 0; }
  virtual int32_t StopPlayout() { return 0; }
  virtual int32_t MicrophoneIsAvailable(bool* available) {
    *available = true;
    return 0;
  }
  virtual int32_t InitMicrophone() { return 0; }
  virtual int32_t SetRecordingDevice(uint16_t index) { return 0; }
  virtual int32_t SetRecordingDevice(WindowsDeviceType device) { return 0; }
  virtual int32_t SetStereoRecording(bool enable) { return 0; }
  virtual int32_t SetAGC(bool enable) { return 0; }
  virtual int32_t StopRecording() { return 0; }
  virtual int32_t TimeUntilNextProcess() { return 0; }
  virtual int32_t Process() { return 0; }
  virtual int32_t Terminate() { return 0; }

  virtual int32_t ActiveAudioLayer(AudioLayer* audioLayer) const {
    assert(false);
    return 0;
  }
  virtual ErrorCode LastError() const {
    assert(false);
    return  kAdmErrNone;
  }
  virtual bool Initialized() const {
    assert(false);
    return true;
  }
  virtual int16_t PlayoutDevices() {
    assert(false);
    return 0;
  }
  virtual int16_t RecordingDevices() {
    assert(false);
    return 0;
  }
  virtual int32_t PlayoutDeviceName(uint16_t index,
                            char name[kAdmMaxDeviceNameSize],
                            char guid[kAdmMaxGuidSize]) {
    assert(false);
    return 0;
  }
  virtual int32_t RecordingDeviceName(uint16_t index,
                              char name[kAdmMaxDeviceNameSize],
                              char guid[kAdmMaxGuidSize]) {
    assert(false);
    return 0;
  }
  virtual int32_t PlayoutIsAvailable(bool* available) {
    assert(false);
    return 0;
  }
  virtual int32_t InitPlayout() {
    assert(false);
    return 0;
  }
  virtual bool PlayoutIsInitialized() const {
    assert(false);
    return true;
  }
  virtual int32_t RecordingIsAvailable(bool* available) {
    assert(false);
    return 0;
  }
  virtual int32_t InitRecording() {
    assert(false);
    return 0;
  }
  virtual bool RecordingIsInitialized() const {
    assert(false);
    return true;
  }
  virtual int32_t StartPlayout() {
    assert(false);
    return 0;
  }
  virtual bool Playing() const {
    assert(false);
    return false;
  }
  virtual int32_t StartRecording() {
    assert(false);
    return 0;
  }
  virtual bool Recording() const {
    assert(false);
    return false;
  }
  virtual bool AGC() const {
    assert(false);
    return true;
  }
  virtual int32_t SetWaveOutVolume(uint16_t volumeLeft,
                           uint16_t volumeRight) {
    assert(false);
    return 0;
  }
  virtual int32_t WaveOutVolume(uint16_t* volumeLeft,
                        uint16_t* volumeRight) const {
    assert(false);
    return 0;
  }
  virtual bool SpeakerIsInitialized() const {
    assert(false);
    return true;
  }
  virtual bool MicrophoneIsInitialized() const {
    assert(false);
    return true;
  }
  virtual int32_t SpeakerVolumeIsAvailable(bool* available) {
    assert(false);
    return 0;
  }
  virtual int32_t SetSpeakerVolume(uint32_t volume) {
    assert(false);
    return 0;
  }
  virtual int32_t SpeakerVolume(uint32_t* volume) const {
    assert(false);
    return 0;
  }
  virtual int32_t MaxSpeakerVolume(uint32_t* maxVolume) const {
    assert(false);
    return 0;
  }
  virtual int32_t MinSpeakerVolume(uint32_t* minVolume) const {
    assert(false);
    return 0;
  }
  virtual int32_t SpeakerVolumeStepSize(uint16_t* stepSize) const {
    assert(false);
    return 0;
  }
  virtual int32_t MicrophoneVolumeIsAvailable(bool* available) {
    assert(false);
    return 0;
  }
  virtual int32_t SetMicrophoneVolume(uint32_t volume) {
    assert(false);
    return 0;
  }
  virtual int32_t MicrophoneVolume(uint32_t* volume) const {
    assert(false);
    return 0;
  }
  virtual int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const {
    assert(false);
    return 0;
  }
  virtual int32_t MinMicrophoneVolume(uint32_t* minVolume) const {
    assert(false);
    return 0;
  }
  virtual int32_t MicrophoneVolumeStepSize(uint16_t* stepSize) const {
    assert(false);
    return 0;
  }
  virtual int32_t SpeakerMuteIsAvailable(bool* available) {
    assert(false);
    return 0;
  }
  virtual int32_t SetSpeakerMute(bool enable) {
    assert(false);
    return 0;
  }
  virtual int32_t SpeakerMute(bool* enabled) const {
    assert(false);
    return 0;
  }
  virtual int32_t MicrophoneMuteIsAvailable(bool* available) {
    assert(false);
    return 0;
  }
  virtual int32_t SetMicrophoneMute(bool enable) {
    assert(false);
    return 0;
  }
  virtual int32_t MicrophoneMute(bool* enabled) const {
    assert(false);
    return 0;
  }
  virtual int32_t MicrophoneBoostIsAvailable(bool* available) {
    assert(false);
    return 0;
  }
  virtual int32_t SetMicrophoneBoost(bool enable) {
    assert(false);
    return 0;
  }
  virtual int32_t MicrophoneBoost(bool* enabled) const {
    assert(false);
    return 0;
  }
  virtual int32_t StereoPlayoutIsAvailable(bool* available) const {
    *available = false;
    return 0;
  }
  virtual int32_t StereoPlayout(bool* enabled) const {
    assert(false);
    return 0;
  }
  virtual int32_t StereoRecordingIsAvailable(bool* available) const {
    *available = false;
    return 0;
  }
  virtual int32_t StereoRecording(bool* enabled) const {
    assert(false);
    return 0;
  }
  virtual int32_t SetRecordingChannel(const ChannelType channel) {
    assert(false);
    return 0;
  }
  virtual int32_t RecordingChannel(ChannelType* channel) const {
    assert(false);
    return 0;
  }
  virtual int32_t SetPlayoutBuffer(const BufferType type,
                           uint16_t sizeMS = 0) {
    assert(false);
    return 0;
  }
  virtual int32_t PlayoutBuffer(BufferType* type, uint16_t* sizeMS) const {
    assert(false);
    return 0;
  }
  virtual int32_t PlayoutDelay(uint16_t* delayMS) const {
    assert(false);
    return 0;
  }
  virtual int32_t RecordingDelay(uint16_t* delayMS) const {
    assert(false);
    return 0;
  }
  virtual int32_t CPULoad(uint16_t* load) const {
    assert(false);
    return 0;
  }
  virtual int32_t StartRawOutputFileRecording(
      const char pcmFileNameUTF8[kAdmMaxFileNameSize]) {
    assert(false);
    return 0;
  }
  virtual int32_t StopRawOutputFileRecording() {
    assert(false);
    return 0;
  }
  virtual int32_t StartRawInputFileRecording(
      const char pcmFileNameUTF8[kAdmMaxFileNameSize]) {
    assert(false);
    return 0;
  }
  virtual int32_t StopRawInputFileRecording() {
    assert(false);
    return 0;
  }
  virtual int32_t SetRecordingSampleRate(const uint32_t samplesPerSec) {
    assert(false);
    return 0;
  }
  virtual int32_t RecordingSampleRate(uint32_t* samplesPerSec) const {
    assert(false);
    return 0;
  }
  virtual int32_t SetPlayoutSampleRate(const uint32_t samplesPerSec) {
    assert(false);
    return 0;
  }
  virtual int32_t PlayoutSampleRate(uint32_t* samplesPerSec) const {
    assert(false);
    return 0;
  }
  virtual int32_t ResetAudioDevice() {
    assert(false);
    return 0;
  }
  virtual int32_t SetLoudspeakerStatus(bool enable) {
    assert(false);
    return 0;
  }
  virtual int32_t GetLoudspeakerStatus(bool* enabled) const {
    assert(false);
    return 0;
  }
  virtual int32_t EnableBuiltInAEC(bool enable) {
    assert(false);
    return -1;
  }
  virtual bool BuiltInAECIsEnabled() const {
    assert(false);
    return false;
  }
};

class VoECodecTest : public ::testing::Test {
 protected:
  VoECodecTest()
      : voe_(VoiceEngine::Create()),
        base_(VoEBase::GetInterface(voe_)),
        voe_codec_(VoECodec::GetInterface(voe_)),
        channel_(-1),
        adm_(new FakeAudioDeviceModule),
        red_payload_type_(-1) {
  }

  ~VoECodecTest() {}

  void TearDown() {
    base_->DeleteChannel(channel_);
    base_->Terminate();
    base_->Release();
    voe_codec_->Release();
    VoiceEngine::Delete(voe_);
  }

  void SetUp() {
    // Check if all components are valid.
    ASSERT_TRUE(voe_ != NULL);
    ASSERT_TRUE(base_ != NULL);
    ASSERT_TRUE(voe_codec_ != NULL);
    ASSERT_TRUE(adm_.get() != NULL);
    ASSERT_EQ(0, base_->Init(adm_.get()));
    channel_ = base_->CreateChannel();
    ASSERT_NE(-1, channel_);

    CodecInst my_codec;

    bool primary_found = false;
    bool valid_secondary_found = false;
    bool invalid_secondary_found = false;

    // Find primary and secondary codecs.
    int num_codecs = voe_codec_->NumOfCodecs();
    int n = 0;
    while (n < num_codecs && (!primary_found || !valid_secondary_found ||
        !invalid_secondary_found || red_payload_type_ < 0)) {
      EXPECT_EQ(0, voe_codec_->GetCodec(n, my_codec));
      if (!STR_CASE_CMP(my_codec.plname, "isac") && my_codec.plfreq == 16000) {
        memcpy(&valid_secondary_, &my_codec, sizeof(my_codec));
        valid_secondary_found = true;
      } else if (!STR_CASE_CMP(my_codec.plname, "isac") &&
          my_codec.plfreq == 32000) {
        memcpy(&invalid_secondary_, &my_codec, sizeof(my_codec));
        invalid_secondary_found = true;
      } else if (!STR_CASE_CMP(my_codec.plname, "L16") &&
          my_codec.plfreq == 16000) {
        memcpy(&primary_, &my_codec, sizeof(my_codec));
        primary_found = true;
      } else if (!STR_CASE_CMP(my_codec.plname, "RED")) {
        red_payload_type_ = my_codec.pltype;
      }
      n++;
    }

    EXPECT_TRUE(primary_found);
    EXPECT_TRUE(valid_secondary_found);
    EXPECT_TRUE(invalid_secondary_found);
    EXPECT_NE(-1, red_payload_type_);
  }

  VoiceEngine* voe_;
  VoEBase* base_;
  VoECodec* voe_codec_;
  int channel_;
  CodecInst primary_;
  CodecInst valid_secondary_;
  scoped_ptr<FakeAudioDeviceModule> adm_;

  // A codec which is not valid to be registered as secondary codec.
  CodecInst invalid_secondary_;
  int red_payload_type_;
};


TEST_F(VoECodecTest, DualStreamSetSecondaryBeforePrimaryFails) {
  // Setting secondary before a primary is registered should fail.
  EXPECT_EQ(-1, voe_codec_->SetSecondarySendCodec(channel_, valid_secondary_,
                                                  red_payload_type_));
  red_payload_type_ = 1;
}

TEST_F(VoECodecTest, DualStreamRegisterWithWrongInputsFails) {
  // Register primary codec.
  EXPECT_EQ(0, voe_codec_->SetSendCodec(channel_, primary_));

  // Wrong secondary.
  EXPECT_EQ(-1, voe_codec_->SetSecondarySendCodec(channel_, invalid_secondary_,
                                                  red_payload_type_));

  // Wrong paylaod.
  EXPECT_EQ(-1, voe_codec_->SetSecondarySendCodec(channel_, valid_secondary_,
                                                  -1));
  // Wrong channel.
  EXPECT_EQ(-1, voe_codec_->SetSecondarySendCodec(channel_ + 1,
                                                  valid_secondary_,
                                                  red_payload_type_));
}

TEST_F(VoECodecTest, DualStreamGetSecodaryEncoder) {
  // Register primary codec.
  EXPECT_EQ(0, voe_codec_->SetSendCodec(channel_, primary_));

  // Register a valid codec.
  EXPECT_EQ(0, voe_codec_->SetSecondarySendCodec(channel_, valid_secondary_,
                                                 red_payload_type_));
  CodecInst my_codec;

  // Get secondary codec from wrong channel.
  EXPECT_EQ(-1, voe_codec_->GetSecondarySendCodec(channel_ + 1, my_codec));

  // Get secondary and compare.
  memset(&my_codec, 0, sizeof(my_codec));
  EXPECT_EQ(0, voe_codec_->GetSecondarySendCodec(channel_, my_codec));

  EXPECT_EQ(valid_secondary_.plfreq, my_codec.plfreq);
  EXPECT_EQ(valid_secondary_.channels, my_codec.channels);
  EXPECT_EQ(valid_secondary_.pacsize, my_codec.pacsize);
  EXPECT_EQ(valid_secondary_.rate, my_codec.rate);
  EXPECT_EQ(valid_secondary_.pltype, my_codec.pltype);
  EXPECT_EQ(0, STR_CASE_CMP(valid_secondary_.plname, my_codec.plname));
}

TEST_F(VoECodecTest, DualStreamRemoveSecondaryCodec) {
  // Register primary codec.
  EXPECT_EQ(0, voe_codec_->SetSendCodec(channel_, primary_));

  // Register a valid codec.
  EXPECT_EQ(0, voe_codec_->SetSecondarySendCodec(channel_, valid_secondary_,
                                                 red_payload_type_));
  // Remove from wrong channel.
  EXPECT_EQ(-1, voe_codec_->RemoveSecondarySendCodec(channel_ + 1));
  EXPECT_EQ(0, voe_codec_->RemoveSecondarySendCodec(channel_));

  CodecInst my_codec;

  // Get should fail, if secondary is removed.
  EXPECT_EQ(-1, voe_codec_->GetSecondarySendCodec(channel_, my_codec));
}

}  // namespace
}  // namespace voe
}  // namespace webrtc
