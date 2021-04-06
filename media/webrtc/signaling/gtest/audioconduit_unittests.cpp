/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

#include "AudioConduit.h"
#include "WebrtcCallWrapper.h"

#include "MockCall.h"

using namespace mozilla;
using namespace testing;
using namespace webrtc;

namespace test {

class AudioConduitTest : public ::testing::Test {
 public:
  AudioConduitTest()
      : mCallWrapper(MockCallWrapper::Create()),
        mAudioConduit(MakeRefPtr<WebrtcAudioConduit>(
            mCallWrapper, GetCurrentSerialEventTarget())) {}

  ~AudioConduitTest() override {
    mAudioConduit->DeleteStreams();
    mCallWrapper->Destroy();
  }

  MockCall* Call() { return mCallWrapper->GetMockCall(); }

  const RefPtr<MockCallWrapper> mCallWrapper;
  const RefPtr<WebrtcAudioConduit> mAudioConduit;
};

TEST_F(AudioConduitTest, TestConfigureSendMediaCodec) {
  MediaConduitErrorCode ec;

  // defaults
  AudioCodecConfig codecConfig(114, "opus", 48000, 2, false);
  ec = mAudioConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mAudioConduit->StartTransmitting();
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioSendConfig.send_codec_spec->format;
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_NE(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxaveragebitrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("usedtx"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("cbr"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("ptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("minptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxptime"), f.parameters.end());
  }

  // null codec
  ec = mAudioConduit->ConfigureSendMediaCodec(nullptr);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);

  // empty codec name
  codecConfig = AudioCodecConfig(114, "", 48000, 2, false);
  ec = mAudioConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);
}

TEST_F(AudioConduitTest, TestConfigureSendOpusMono) {
  MediaConduitErrorCode ec;

  // opus mono
  AudioCodecConfig codecConfig = AudioCodecConfig(114, "opus", 48000, 1, false);
  ec = mAudioConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mAudioConduit->StartTransmitting();
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioSendConfig.send_codec_spec->format;
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 1UL);
    ASSERT_EQ(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxaveragebitrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("usedtx"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("cbr"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("ptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("minptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxptime"), f.parameters.end());
  }
}

TEST_F(AudioConduitTest, TestConfigureSendOpusFEC) {
  MediaConduitErrorCode ec;

  // opus with inband Forward Error Correction
  AudioCodecConfig codecConfig = AudioCodecConfig(114, "opus", 48000, 2, true);
  ec = mAudioConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mAudioConduit->StartTransmitting();
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioSendConfig.send_codec_spec->format;
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_NE(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_NE(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("useinbandfec"), "1");
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxaveragebitrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("usedtx"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("cbr"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("ptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("minptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxptime"), f.parameters.end());
  }
}

TEST_F(AudioConduitTest, TestConfigureSendOpusMaxPlaybackRate) {
  MediaConduitErrorCode ec;

  AudioCodecConfig codecConfig = AudioCodecConfig(114, "opus", 48000, 2, false);
  codecConfig.mMaxPlaybackRate = 1234;
  ec = mAudioConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mAudioConduit->StartTransmitting();
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioSendConfig.send_codec_spec->format;
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_NE(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_NE(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("maxplaybackrate"), "1234");
    ASSERT_EQ(f.parameters.find("maxaveragebitrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("usedtx"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("cbr"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("ptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("minptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxptime"), f.parameters.end());
  }
}

TEST_F(AudioConduitTest, TestConfigureSendOpusMaxAverageBitrate) {
  MediaConduitErrorCode ec;

  AudioCodecConfig codecConfig = AudioCodecConfig(114, "opus", 48000, 2, false);
  codecConfig.mMaxAverageBitrate = 12345;
  ec = mAudioConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mAudioConduit->StartTransmitting();
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioSendConfig.send_codec_spec->format;
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_NE(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_NE(f.parameters.find("maxaveragebitrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("maxaveragebitrate"), "12345");
    ASSERT_EQ(f.parameters.find("usedtx"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("cbr"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("ptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("minptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxptime"), f.parameters.end());
  }
}

TEST_F(AudioConduitTest, TestConfigureSendOpusDtx) {
  MediaConduitErrorCode ec;

  AudioCodecConfig codecConfig = AudioCodecConfig(114, "opus", 48000, 2, false);
  codecConfig.mDTXEnabled = true;
  ec = mAudioConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mAudioConduit->StartTransmitting();
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioSendConfig.send_codec_spec->format;
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_NE(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxaveragebitrate"), f.parameters.end());
    ASSERT_NE(f.parameters.find("usedtx"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("usedtx"), "1");
    ASSERT_EQ(f.parameters.find("cbr"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("ptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("minptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxptime"), f.parameters.end());
  }
}

TEST_F(AudioConduitTest, TestConfigureSendOpusCbr) {
  MediaConduitErrorCode ec;

  AudioCodecConfig codecConfig = AudioCodecConfig(114, "opus", 48000, 2, false);
  codecConfig.mCbrEnabled = true;
  ec = mAudioConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mAudioConduit->StartTransmitting();
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioSendConfig.send_codec_spec->format;
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_NE(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxaveragebitrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("usedtx"), f.parameters.end());
    ASSERT_NE(f.parameters.find("cbr"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("cbr"), "1");
    ASSERT_EQ(f.parameters.find("ptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("minptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxptime"), f.parameters.end());
  }
}

TEST_F(AudioConduitTest, TestConfigureSendOpusPtime) {
  MediaConduitErrorCode ec;

  AudioCodecConfig codecConfig = AudioCodecConfig(114, "opus", 48000, 2, false);
  codecConfig.mFrameSizeMs = 100;
  ec = mAudioConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mAudioConduit->StartTransmitting();
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioSendConfig.send_codec_spec->format;
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_NE(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxaveragebitrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("usedtx"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("cbr"), f.parameters.end());
    ASSERT_NE(f.parameters.find("ptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("ptime"), "100");
    ASSERT_EQ(f.parameters.find("minptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxptime"), f.parameters.end());
  }
}

TEST_F(AudioConduitTest, TestConfigureSendOpusMinPtime) {
  MediaConduitErrorCode ec;

  AudioCodecConfig codecConfig = AudioCodecConfig(114, "opus", 48000, 2, false);
  codecConfig.mMinFrameSizeMs = 201;
  ec = mAudioConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mAudioConduit->StartTransmitting();
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioSendConfig.send_codec_spec->format;
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_NE(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxaveragebitrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("usedtx"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("cbr"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("ptime"), f.parameters.end());
    ASSERT_NE(f.parameters.find("minptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("minptime"), "201");
    ASSERT_EQ(f.parameters.find("maxptime"), f.parameters.end());
  }
}

TEST_F(AudioConduitTest, TestConfigureSendOpusMaxPtime) {
  MediaConduitErrorCode ec;

  AudioCodecConfig codecConfig = AudioCodecConfig(114, "opus", 48000, 2, false);
  codecConfig.mMaxFrameSizeMs = 321;
  ec = mAudioConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mAudioConduit->StartTransmitting();
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioSendConfig.send_codec_spec->format;
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_NE(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxaveragebitrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("usedtx"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("cbr"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("ptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("minptime"), f.parameters.end());
    ASSERT_NE(f.parameters.find("maxptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("maxptime"), "321");
  }
}

TEST_F(AudioConduitTest, TestConfigureSendOpusAllParams) {
  MediaConduitErrorCode ec;

  AudioCodecConfig codecConfig = AudioCodecConfig(114, "opus", 48000, 2, true);
  codecConfig.mMaxPlaybackRate = 5432;
  codecConfig.mMaxAverageBitrate = 54321;
  codecConfig.mDTXEnabled = true;
  codecConfig.mCbrEnabled = true;
  codecConfig.mFrameSizeMs = 999;
  codecConfig.mMinFrameSizeMs = 123;
  codecConfig.mMaxFrameSizeMs = 789;
  ec = mAudioConduit->ConfigureSendMediaCodec(&codecConfig);
  ASSERT_EQ(ec, kMediaConduitNoError);
  mAudioConduit->StartTransmitting();
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioSendConfig.send_codec_spec->format;
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_NE(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_NE(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("useinbandfec"), "1");
    ASSERT_NE(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("maxplaybackrate"), "5432");
    ASSERT_NE(f.parameters.find("maxaveragebitrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("maxaveragebitrate"), "54321");
    ASSERT_NE(f.parameters.find("usedtx"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("usedtx"), "1");
    ASSERT_NE(f.parameters.find("cbr"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("cbr"), "1");
    ASSERT_NE(f.parameters.find("ptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("ptime"), "999");
    ASSERT_NE(f.parameters.find("minptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("minptime"), "123");
    ASSERT_NE(f.parameters.find("maxptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("maxptime"), "789");
  }
}

TEST_F(AudioConduitTest, TestConfigureReceiveMediaCodecs) {
  MediaConduitErrorCode ec;

  // just default opus stereo
  std::vector<UniquePtr<mozilla::AudioCodecConfig>> codecs;
  codecs.emplace_back(new AudioCodecConfig(114, "opus", 48000, 2, false));
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(Call()->mAudioReceiveConfig.sync_group, "");
  ASSERT_EQ(Call()->mAudioReceiveConfig.decoder_map.size(), 1U);
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioReceiveConfig.decoder_map.at(114);
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxaveragebitrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("usedtx"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("cbr"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("ptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("minptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxptime"), f.parameters.end());
  }

  // multiple codecs
  codecs.clear();
  codecs.emplace_back(new AudioCodecConfig(9, "g722", 16000, 2, false));
  codecs.emplace_back(new AudioCodecConfig(114, "opus", 48000, 2, false));
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(Call()->mAudioReceiveConfig.sync_group, "");
  ASSERT_EQ(Call()->mAudioReceiveConfig.decoder_map.size(), 2U);
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioReceiveConfig.decoder_map.at(9);
    ASSERT_EQ(f.name, "g722");
    ASSERT_EQ(f.clockrate_hz, 16000);
    ASSERT_EQ(f.num_channels, 2U);
    ASSERT_EQ(f.parameters.size(), 0U);
  }
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioReceiveConfig.decoder_map.at(114);
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2U);
    ASSERT_EQ(f.parameters.at("stereo"), "1");
  }

  // no codecs
  codecs.clear();
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);

  // null codec
  codecs.clear();
  codecs.push_back(nullptr);
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);

  // invalid codec name
  codecs.clear();
  codecs.emplace_back(new AudioCodecConfig(114, "", 48000, 2, false));
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);

  // invalid number of channels
  codecs.clear();
  codecs.emplace_back(new AudioCodecConfig(114, "opus", 48000, 42, false));
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);
}

TEST_F(AudioConduitTest, TestConfigureReceiveOpusMono) {
  MediaConduitErrorCode ec;

  // opus mono
  std::vector<UniquePtr<mozilla::AudioCodecConfig>> codecs;
  codecs.emplace_back(new AudioCodecConfig(114, "opus", 48000, 1, false));
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(Call()->mAudioReceiveConfig.sync_group, "");
  ASSERT_EQ(Call()->mAudioReceiveConfig.decoder_map.size(), 1U);
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioReceiveConfig.decoder_map.at(114);
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 1UL);
    ASSERT_EQ(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxaveragebitrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("usedtx"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("cbr"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("ptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("minptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxptime"), f.parameters.end());
  }
}

TEST_F(AudioConduitTest, TestConfigureReceiveOpusDtx) {
  MediaConduitErrorCode ec;

  // opus mono
  std::vector<UniquePtr<mozilla::AudioCodecConfig>> codecs;
  codecs.emplace_back(new AudioCodecConfig(114, "opus", 48000, 2, false));
  codecs[0]->mDTXEnabled = true;
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(Call()->mAudioReceiveConfig.sync_group, "");
  ASSERT_EQ(Call()->mAudioReceiveConfig.decoder_map.size(), 1U);
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioReceiveConfig.decoder_map.at(114);
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_NE(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxaveragebitrate"), f.parameters.end());
    ASSERT_NE(f.parameters.find("usedtx"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("usedtx"), "1");
    ASSERT_EQ(f.parameters.find("cbr"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("ptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("minptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxptime"), f.parameters.end());
  }
}

TEST_F(AudioConduitTest, TestConfigureReceiveOpusFEC) {
  MediaConduitErrorCode ec;

  // opus with inband Forward Error Correction
  std::vector<UniquePtr<mozilla::AudioCodecConfig>> codecs;
  codecs.emplace_back(new AudioCodecConfig(114, "opus", 48000, 2, true));
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(Call()->mAudioReceiveConfig.sync_group, "");
  ASSERT_EQ(Call()->mAudioReceiveConfig.decoder_map.size(), 1U);
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioReceiveConfig.decoder_map.at(114);
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_NE(f.parameters.find("stereo"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_NE(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("useinbandfec"), "1");
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxaveragebitrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("usedtx"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("cbr"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("ptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("minptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxptime"), f.parameters.end());
  }
}

TEST_F(AudioConduitTest, TestConfigureReceiveOpusMaxPlaybackRate) {
  MediaConduitErrorCode ec;

  std::vector<UniquePtr<mozilla::AudioCodecConfig>> codecs;
  codecs.emplace_back(new AudioCodecConfig(114, "opus", 48000, 2, false));

  codecs[0]->mMaxPlaybackRate = 0;
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(Call()->mAudioReceiveConfig.decoder_map.size(), 1U);
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioReceiveConfig.decoder_map.at(114);
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.count("maxplaybackrate"), 0U);
    ASSERT_EQ(f.parameters.find("maxaveragebitrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("usedtx"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("cbr"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("ptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("minptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxptime"), f.parameters.end());
  }

  codecs[0]->mMaxPlaybackRate = 8000;
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(Call()->mAudioReceiveConfig.decoder_map.size(), 1U);
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioReceiveConfig.decoder_map.at(114);
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.at("maxplaybackrate"), "8000");
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxaveragebitrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("usedtx"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("cbr"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("ptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("minptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxptime"), f.parameters.end());
  }
}

TEST_F(AudioConduitTest, TestConfigureReceiveOpusMaxAverageBitrate) {
  MediaConduitErrorCode ec;

  std::vector<UniquePtr<mozilla::AudioCodecConfig>> codecs;
  codecs.emplace_back(new AudioCodecConfig(114, "opus", 48000, 2, false));

  codecs[0]->mMaxAverageBitrate = 0;
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(Call()->mAudioReceiveConfig.decoder_map.size(), 1U);
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioReceiveConfig.decoder_map.at(114);
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.count("maxaveragebitrate"), 0U);
    ASSERT_EQ(f.parameters.find("usedtx"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("cbr"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("ptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("minptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxptime"), f.parameters.end());
  }

  codecs[0]->mMaxAverageBitrate = 8000;
  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(Call()->mAudioReceiveConfig.decoder_map.size(), 1U);
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioReceiveConfig.decoder_map.at(114);
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.find("useinbandfec"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxplaybackrate"), f.parameters.end());
    ASSERT_EQ(f.parameters.at("maxaveragebitrate"), "8000");
    ASSERT_EQ(f.parameters.find("usedtx"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("cbr"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("ptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("minptime"), f.parameters.end());
    ASSERT_EQ(f.parameters.find("maxptime"), f.parameters.end());
  }
}

TEST_F(AudioConduitTest, TestConfigureReceiveOpusAllParameters) {
  MediaConduitErrorCode ec;

  std::vector<UniquePtr<mozilla::AudioCodecConfig>> codecs;
  codecs.emplace_back(new AudioCodecConfig(114, "opus", 48000, 2, true));

  codecs[0]->mMaxPlaybackRate = 8000;
  codecs[0]->mMaxAverageBitrate = 9000;
  codecs[0]->mDTXEnabled = true;
  codecs[0]->mCbrEnabled = true;
  codecs[0]->mFrameSizeMs = 10;
  codecs[0]->mMinFrameSizeMs = 20;
  codecs[0]->mMaxFrameSizeMs = 30;

  ec = mAudioConduit->ConfigureRecvMediaCodecs(codecs);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(Call()->mAudioReceiveConfig.decoder_map.size(), 1U);
  {
    const webrtc::SdpAudioFormat& f =
        Call()->mAudioReceiveConfig.decoder_map.at(114);
    ASSERT_EQ(f.name, "opus");
    ASSERT_EQ(f.clockrate_hz, 48000);
    ASSERT_EQ(f.num_channels, 2UL);
    ASSERT_EQ(f.parameters.at("stereo"), "1");
    ASSERT_EQ(f.parameters.at("useinbandfec"), "1");
    ASSERT_EQ(f.parameters.at("maxplaybackrate"), "8000");
    ASSERT_EQ(f.parameters.at("maxaveragebitrate"), "9000");
    ASSERT_EQ(f.parameters.at("usedtx"), "1");
    ASSERT_EQ(f.parameters.at("cbr"), "1");
    ASSERT_EQ(f.parameters.at("ptime"), "10");
    ASSERT_EQ(f.parameters.at("minptime"), "20");
    ASSERT_EQ(f.parameters.at("maxptime"), "30");
  }
}

TEST_F(AudioConduitTest, TestSetLocalRTPExtensions) {
  MediaConduitErrorCode ec;

  using LocalDirection = MediaSessionConduitLocalDirection;

  RtpExtList extensions;

  // Empty extensions
  ec = mAudioConduit->SetLocalRTPExtensions(LocalDirection::kRecv, extensions);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->SetLocalRTPExtensions(LocalDirection::kSend, extensions);
  ASSERT_EQ(ec, kMediaConduitNoError);

  // Audio level
  webrtc::RtpExtension extension;
  extension.uri = webrtc::RtpExtension::kAudioLevelUri;
  extensions.emplace_back(extension);

  ec = mAudioConduit->SetLocalRTPExtensions(LocalDirection::kRecv, extensions);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->StartReceiving();
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->StopReceiving();
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(Call()->mAudioReceiveConfig.rtp.extensions.back().uri,
            webrtc::RtpExtension::kAudioLevelUri);

  ec = mAudioConduit->SetLocalRTPExtensions(LocalDirection::kSend, extensions);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->StartTransmitting();
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->StopTransmitting();
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(Call()->mAudioSendConfig.rtp.extensions.back().uri,
            webrtc::RtpExtension::kAudioLevelUri);

  // Contributing sources audio level
  extensions.clear();
  extension.uri = webrtc::RtpExtension::kCsrcAudioLevelUri;
  extensions.emplace_back(extension);

  ec = mAudioConduit->SetLocalRTPExtensions(LocalDirection::kRecv, extensions);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->StartReceiving();
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->StopReceiving();
  ASSERT_EQ(ec, kMediaConduitNoError);

  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(Call()->mAudioReceiveConfig.rtp.extensions.back().uri,
            webrtc::RtpExtension::kCsrcAudioLevelUri);

  ec = mAudioConduit->SetLocalRTPExtensions(LocalDirection::kSend, extensions);
  ASSERT_EQ(ec, kMediaConduitMalformedArgument);

  // Mid
  extensions.clear();
  extension.uri = webrtc::RtpExtension::kMidUri;
  extensions.emplace_back(extension);

  // We do not support configuring receiving MId, but do not return an error
  // in this case.
  ec = mAudioConduit->SetLocalRTPExtensions(LocalDirection::kRecv, extensions);
  ASSERT_EQ(ec, kMediaConduitNoError);

  ec = mAudioConduit->SetLocalRTPExtensions(LocalDirection::kSend, extensions);
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->StartTransmitting();
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->StopTransmitting();
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(Call()->mAudioSendConfig.rtp.extensions.back().uri,
            webrtc::RtpExtension::kMidUri);
}

TEST_F(AudioConduitTest, TestSetSyncGroup) {
  MediaConduitErrorCode ec;

  mAudioConduit->SetSyncGroup("test");
  ec = mAudioConduit->StartReceiving();
  ASSERT_EQ(ec, kMediaConduitNoError);
  ec = mAudioConduit->StopReceiving();
  ASSERT_EQ(ec, kMediaConduitNoError);
  ASSERT_EQ(Call()->mAudioReceiveConfig.sync_group, "test");
}

}  // End namespace test.
