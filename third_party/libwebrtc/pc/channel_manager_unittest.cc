/*
 *  Copyright 2008 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/channel_manager.h"

#include "api/sequence_checker.h"
#include "api/video/builtin_video_bitrate_allocator_factory.h"
#include "media/base/fake_media_engine.h"
#include "media/base/test_utils.h"
#include "media/engine/fake_webrtc_call.h"
#include "p2p/base/fake_dtls_transport.h"
#include "p2p/base/p2p_constants.h"
#include "pc/channel.h"
#include "pc/dtls_srtp_transport.h"
#include "pc/rtp_transport_internal.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/checks.h"
#include "rtc_base/location.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"
#include "test/scoped_key_value_config.h"

namespace cricket {
namespace {
const bool kDefaultSrtpRequired = true;

static const AudioCodec kAudioCodecs[] = {
    AudioCodec(97, "voice", 1, 2, 3),
    AudioCodec(111, "OPUS", 48000, 32000, 2),
};

static const VideoCodec kVideoCodecs[] = {
    VideoCodec(99, "H264"),
    VideoCodec(100, "VP8"),
    VideoCodec(96, "rtx"),
};

std::unique_ptr<MediaEngineInterface> CreateFakeMediaEngine() {
  auto fme = std::make_unique<FakeMediaEngine>();
  fme->SetAudioCodecs(MAKE_VECTOR(kAudioCodecs));
  fme->SetVideoCodecs(MAKE_VECTOR(kVideoCodecs));
  return fme;
}

}  // namespace

class ChannelManagerTest : public ::testing::Test {
 protected:
  ChannelManagerTest()
      : network_(rtc::Thread::CreateWithSocketServer()),
        worker_(rtc::Thread::Current()),
        video_bitrate_allocator_factory_(
            webrtc::CreateBuiltinVideoBitrateAllocatorFactory()),
        media_engine_(CreateFakeMediaEngine()),
        cm_(cricket::ChannelManager::Create(media_engine_.get(),
                                            &ssrc_generator_,
                                            false,
                                            worker_,
                                            network_.get())),
        fake_call_(worker_, network_.get()) {
    network_->SetName("Network", this);
    network_->Start();
  }

  void TestCreateDestroyChannels(webrtc::RtpTransportInternal* rtp_transport) {
    RTC_DCHECK_RUN_ON(worker_);
    std::unique_ptr<cricket::VoiceChannel> voice_channel =
        cm_->CreateVoiceChannel(&fake_call_, cricket::MediaConfig(),
                                cricket::CN_AUDIO, kDefaultSrtpRequired,
                                webrtc::CryptoOptions(), AudioOptions());
    ASSERT_TRUE(voice_channel != nullptr);

    std::unique_ptr<cricket::VideoChannel> video_channel =
        cm_->CreateVideoChannel(&fake_call_, cricket::MediaConfig(),
                                cricket::CN_VIDEO, kDefaultSrtpRequired,
                                webrtc::CryptoOptions(), VideoOptions(),
                                video_bitrate_allocator_factory_.get());
    ASSERT_TRUE(video_channel != nullptr);
    // Destruction is tested by having the owning pointers
    // go out of scope.
  }

  rtc::AutoThread main_thread_;
  std::unique_ptr<rtc::Thread> network_;
  rtc::Thread* const worker_;
  std::unique_ptr<webrtc::VideoBitrateAllocatorFactory>
      video_bitrate_allocator_factory_;
  std::unique_ptr<cricket::MediaEngineInterface> media_engine_;
  rtc::UniqueRandomIdGenerator ssrc_generator_;
  std::unique_ptr<cricket::ChannelManager> cm_;
  cricket::FakeCall fake_call_;
  webrtc::test::ScopedKeyValueConfig field_trials_;
};

TEST_F(ChannelManagerTest, SetVideoRtxEnabled) {
  std::vector<VideoCodec> send_codecs;
  std::vector<VideoCodec> recv_codecs;
  const VideoCodec rtx_codec(96, "rtx");

  // By default RTX is disabled.
  cm_->GetSupportedVideoSendCodecs(&send_codecs);
  EXPECT_FALSE(ContainsMatchingCodec(send_codecs, rtx_codec, &field_trials_));
  cm_->GetSupportedVideoSendCodecs(&recv_codecs);
  EXPECT_FALSE(ContainsMatchingCodec(recv_codecs, rtx_codec, &field_trials_));

  // Enable and check.
  cm_ = cricket::ChannelManager::Create(media_engine_.get(), &ssrc_generator_,
                                        true, worker_, network_.get());
  cm_->GetSupportedVideoSendCodecs(&send_codecs);
  EXPECT_TRUE(ContainsMatchingCodec(send_codecs, rtx_codec, &field_trials_));
  cm_->GetSupportedVideoSendCodecs(&recv_codecs);
  EXPECT_TRUE(ContainsMatchingCodec(recv_codecs, rtx_codec, &field_trials_));

  // Disable and check.
  cm_ = cricket::ChannelManager::Create(media_engine_.get(), &ssrc_generator_,
                                        false, worker_, network_.get());
  cm_->GetSupportedVideoSendCodecs(&send_codecs);
  EXPECT_FALSE(ContainsMatchingCodec(send_codecs, rtx_codec, &field_trials_));
  cm_->GetSupportedVideoSendCodecs(&recv_codecs);
  EXPECT_FALSE(ContainsMatchingCodec(recv_codecs, rtx_codec, &field_trials_));
}

TEST_F(ChannelManagerTest, CreateDestroyChannels) {
  auto rtp_dtls_transport = std::make_unique<FakeDtlsTransport>(
      "fake_dtls_transport", cricket::ICE_CANDIDATE_COMPONENT_RTP,
      network_.get());
  auto dtls_srtp_transport = std::make_unique<webrtc::DtlsSrtpTransport>(
      /*rtcp_mux_required=*/true, field_trials_);

  network_->Invoke<void>(
      RTC_FROM_HERE, [&rtp_dtls_transport, &dtls_srtp_transport] {
        dtls_srtp_transport->SetDtlsTransports(rtp_dtls_transport.get(),
                                               /*rtcp_dtls_transport=*/nullptr);
      });
  TestCreateDestroyChannels(dtls_srtp_transport.get());
}

}  // namespace cricket
