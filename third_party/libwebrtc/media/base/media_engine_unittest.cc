/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/media_engine.h"

#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Return;
using ::testing::StrEq;
using ::webrtc::RtpExtension;
using ::webrtc::RtpHeaderExtensionCapability;
using ::webrtc::RtpTransceiverDirection;

namespace cricket {
namespace {

class MockRtpHeaderExtensionQueryInterface
    : public RtpHeaderExtensionQueryInterface {
 public:
  MOCK_METHOD(std::vector<RtpHeaderExtensionCapability>,
              GetRtpHeaderExtensions,
              (),
              (const, override));
};

}  // namespace

TEST(MediaEngineTest, ReturnsNotStoppedHeaderExtensions) {
  MockRtpHeaderExtensionQueryInterface mock;
  std::vector<RtpHeaderExtensionCapability> extensions(
      {RtpHeaderExtensionCapability("uri1", 1,
                                    RtpTransceiverDirection::kInactive),
       RtpHeaderExtensionCapability("uri2", 2,
                                    RtpTransceiverDirection::kSendRecv),
       RtpHeaderExtensionCapability("uri3", 3,
                                    RtpTransceiverDirection::kStopped),
       RtpHeaderExtensionCapability("uri4", 4,
                                    RtpTransceiverDirection::kSendOnly),
       RtpHeaderExtensionCapability("uri5", 5,
                                    RtpTransceiverDirection::kRecvOnly)});
  EXPECT_CALL(mock, GetRtpHeaderExtensions).WillOnce(Return(extensions));
  EXPECT_THAT(GetDefaultEnabledRtpHeaderExtensions(mock),
              ElementsAre(Field(&RtpExtension::uri, StrEq("uri1")),
                          Field(&RtpExtension::uri, StrEq("uri2")),
                          Field(&RtpExtension::uri, StrEq("uri4")),
                          Field(&RtpExtension::uri, StrEq("uri5"))));
}

// This class mocks methods declared as pure virtual in the interface.
// Since the tests are aiming to check the patterns of overrides, the
// functions with default implementations are not mocked.
class MostlyMockVoiceEngineInterface : public VoiceEngineInterface {
 public:
  MOCK_METHOD(std::vector<webrtc::RtpHeaderExtensionCapability>,
              GetRtpHeaderExtensions,
              (),
              (const, override));
  MOCK_METHOD(void, Init, (), (override));
  MOCK_METHOD(rtc::scoped_refptr<webrtc::AudioState>,
              GetAudioState,
              (),
              (const, override));
  MOCK_METHOD(std::vector<AudioCodec>&, send_codecs, (), (const, override));
  MOCK_METHOD(std::vector<AudioCodec>&, recv_codecs, (), (const, override));
  MOCK_METHOD(bool,
              StartAecDump,
              (webrtc::FileWrapper file, int64_t max_size_bytes),
              (override));
  MOCK_METHOD(void, StopAecDump, (), (override));
  MOCK_METHOD(absl::optional<webrtc::AudioDeviceModule::Stats>,
              GetAudioDeviceStats,
              (),
              (override));
};

class OldStyleVoiceEngineInterface : public MostlyMockVoiceEngineInterface {
 public:
  using MostlyMockVoiceEngineInterface::CreateMediaChannel;
  // Old style overrides the deprecated API only.
  VoiceMediaChannel* CreateMediaChannel(
      webrtc::Call* call,
      const MediaConfig& config,
      const AudioOptions& options,
      const webrtc::CryptoOptions& crypto_options) override {
    ++call_count;
    return nullptr;
  }
  int call_count = 0;
};

class NewStyleVoiceEngineInterface : public MostlyMockVoiceEngineInterface {
  // New style overrides the non-deprecated API.
  VoiceMediaChannel* CreateMediaChannel(
      MediaChannel::Role role,
      webrtc::Call* call,
      const MediaConfig& config,
      const AudioOptions& options,
      const webrtc::CryptoOptions& crypto_options,
      webrtc::AudioCodecPairId codec_pair_id) override {
    return nullptr;
  }
};

TEST(MediaEngineTest, NewStyleApiCallsOldIfOverridden) {
  OldStyleVoiceEngineInterface implementation_under_test;
  MediaConfig config;
  AudioOptions options;
  webrtc::CryptoOptions crypto_options;
  // Calling the old-style interface.
  implementation_under_test.CreateMediaChannel(nullptr, config, options,
                                               crypto_options);
  EXPECT_EQ(implementation_under_test.call_count, 1);
  // Calling the new-style interface redirects to the old-style interface.
  implementation_under_test.CreateMediaChannel(
      MediaChannel::Role::kBoth, nullptr, config, options, crypto_options,
      webrtc::AudioCodecPairId::Create());
  EXPECT_EQ(implementation_under_test.call_count, 2);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(MediaEngineTest, NoOverrideOfCreateCausesCrash) {
  MostlyMockVoiceEngineInterface implementation_under_test;
  MediaConfig config;
  AudioOptions options;
  webrtc::CryptoOptions crypto_options;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

  EXPECT_DEATH(implementation_under_test.CreateMediaChannel(
                   nullptr, config, options, crypto_options),
               "Check failed: !recursion_guard_");
#pragma clang diagnostic pop
  EXPECT_DEATH(implementation_under_test.CreateMediaChannel(
                   MediaChannel::Role::kBoth, nullptr, config, options,
                   crypto_options, webrtc::AudioCodecPairId::Create()),
               "Check failed: !recursion_guard_");
}
#endif

}  // namespace cricket
