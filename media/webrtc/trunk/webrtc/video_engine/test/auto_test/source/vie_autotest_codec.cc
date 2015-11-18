/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_types.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/video_coding/codecs/i420/main/interface/i420.h"
#include "webrtc/test/channel_transport/include/channel_transport.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_external_codec.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/video_engine/include/vie_render.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest_defines.h"
#include "webrtc/video_engine/test/libvietest/include/tb_I420_codec.h"
#include "webrtc/video_engine/test/libvietest/include/tb_capture_device.h"
#include "webrtc/video_engine/test/libvietest/include/tb_interfaces.h"
#include "webrtc/video_engine/test/libvietest/include/tb_video_channel.h"
#include "webrtc/voice_engine/include/voe_base.h"

class TestCodecObserver : public webrtc::ViEEncoderObserver,
                          public webrtc::ViEDecoderObserver {
 public:
  int incoming_codec_called_;
  int incoming_rate_called_;
  int decoder_timing_called_;
  int outgoing_rate_called_;

  unsigned char last_payload_type_;
  uint16_t last_width_;
  uint16_t last_height_;

  unsigned int last_outgoing_framerate_;
  unsigned int last_outgoing_bitrate_;
  unsigned int last_incoming_framerate_;
  unsigned int last_incoming_bitrate_;
  unsigned int suspend_change_called_;

  webrtc::VideoCodec incoming_codec_;

  TestCodecObserver()
      : incoming_codec_called_(0),
        incoming_rate_called_(0),
        decoder_timing_called_(0),
        outgoing_rate_called_(0),
        last_payload_type_(0),
        last_width_(0),
        last_height_(0),
        last_outgoing_framerate_(0),
        last_outgoing_bitrate_(0),
        last_incoming_framerate_(0),
        last_incoming_bitrate_(0),
        suspend_change_called_(0) {
    memset(&incoming_codec_, 0, sizeof(incoming_codec_));
  }
  virtual void IncomingCodecChanged(const int video_channel,
                                    const webrtc::VideoCodec& video_codec) {
    incoming_codec_called_++;
    last_payload_type_ = video_codec.plType;
    last_width_ = video_codec.width;
    last_height_ = video_codec.height;

    memcpy(&incoming_codec_, &video_codec, sizeof(video_codec));
  }

  virtual void IncomingRate(const int video_channel,
                            const unsigned int framerate,
                            const unsigned int bitrate) {
    incoming_rate_called_++;
    last_incoming_framerate_ += framerate;
    last_incoming_bitrate_ += bitrate;
  }

  virtual void DecoderTiming(int decode_ms,
                             int max_decode_ms,
                             int current_delay_ms,
                             int target_delay_ms,
                             int jitter_buffer_ms,
                             int min_playout_delay_ms,
                             int render_delay_ms) {
    ++decoder_timing_called_;
    // TODO(fischman): anything useful to be done with the data here?
  }

  virtual void OutgoingRate(const int video_channel,
                            const unsigned int framerate,
                            const unsigned int bitrate) {
    outgoing_rate_called_++;
    last_outgoing_framerate_ += framerate;
    last_outgoing_bitrate_ += bitrate;
  }

  void SuspendChange(int video_channel, bool is_suspended) override {
    suspend_change_called_++;
  }

  virtual void RequestNewKeyFrame(const int video_channel) {
  }
};

class RenderFilter : public webrtc::ViEEffectFilter {
 public:
  int num_frames_;
  unsigned int last_render_width_;
  unsigned int last_render_height_;

  RenderFilter()
      : num_frames_(0),
        last_render_width_(0),
        last_render_height_(0) {
  }

  virtual ~RenderFilter() {
  }
  virtual int Transform(size_t size,
                        unsigned char* frame_buffer,
                        int64_t ntp_time_ms,
                        unsigned int timestamp,
                        unsigned int width,
                        unsigned int height)  {
    num_frames_++;
    last_render_width_ = width;
    last_render_height_ = height;
    return 0;
  }
};

void ViEAutoTest::ViECodecStandardTest() {
  TbInterfaces interfaces("ViECodecStandardTest");

  TbCaptureDevice capture_device = TbCaptureDevice(interfaces);
  int capture_id = capture_device.captureId;

  webrtc::VideoEngine* video_engine = interfaces.video_engine;
  webrtc::ViEBase* base = interfaces.base;
  webrtc::ViECapture* capture = interfaces.capture;
  webrtc::ViERender* render = interfaces.render;
  webrtc::ViECodec* codec = interfaces.codec;
  webrtc::ViERTP_RTCP* rtp_rtcp = interfaces.rtp_rtcp;
  webrtc::ViENetwork* network = interfaces.network;

  int video_channel = -1;
  EXPECT_EQ(0, base->CreateChannel(video_channel));
  EXPECT_EQ(0, capture->ConnectCaptureDevice(capture_id, video_channel));
  EXPECT_EQ(0, rtp_rtcp->SetRTCPStatus(
      video_channel, webrtc::kRtcpCompound_RFC4585));

  EXPECT_EQ(0, rtp_rtcp->SetKeyFrameRequestMethod(
      video_channel, webrtc::kViEKeyFrameRequestPliRtcp));
  EXPECT_EQ(0, rtp_rtcp->SetTMMBRStatus(video_channel, true));
  EXPECT_EQ(0, render->AddRenderer(capture_id, _window1, 0, 0.0, 0.0, 1.0,
                                   1.0));
  EXPECT_EQ(0, render->AddRenderer(video_channel, _window2, 1, 0.0, 0.0, 1.0,
                                   1.0));
  EXPECT_EQ(0, render->StartRender(capture_id));
  EXPECT_EQ(0, render->StartRender(video_channel));

  webrtc::VideoCodec video_codec;
  memset(&video_codec, 0, sizeof(webrtc::VideoCodec));
  for (int idx = 0; idx < codec->NumberOfCodecs(); idx++) {
    EXPECT_EQ(0, codec->GetCodec(idx, video_codec));
    if (video_codec.codecType != webrtc::kVideoCodecI420) {
      video_codec.width = 640;
      video_codec.height = 480;
    }
    if (video_codec.codecType == webrtc::kVideoCodecI420) {
      video_codec.width = 176;
      video_codec.height = 144;
    }
    EXPECT_EQ(0, codec->SetReceiveCodec(video_channel, video_codec));
  }

  for (int idx = 0; idx < codec->NumberOfCodecs(); idx++) {
    EXPECT_EQ(0, codec->GetCodec(idx, video_codec));
    if (video_codec.codecType == webrtc::kVideoCodecVP8) {
      EXPECT_EQ(0, codec->SetSendCodec(video_channel, video_codec));
      break;
    }
  }
  const char* ip_address = "127.0.0.1";
  const uint16_t rtp_port = 6000;

  rtc::scoped_ptr<webrtc::test::VideoChannelTransport> video_channel_transport(
      new webrtc::test::VideoChannelTransport(network, video_channel));

  ASSERT_EQ(0, video_channel_transport->SetSendDestination(ip_address,
                                                           rtp_port));
  ASSERT_EQ(0, video_channel_transport->SetLocalReceiver(rtp_port));

  EXPECT_EQ(0, base->StartReceive(video_channel));
  EXPECT_EQ(0, base->StartSend(video_channel));

  // Make sure all codecs runs
  {
    webrtc::ViEImageProcess* image_process =
        webrtc::ViEImageProcess::GetInterface(video_engine);
    TestCodecObserver codec_observer;
    EXPECT_EQ(0, codec->RegisterDecoderObserver(video_channel, codec_observer));
    ViETest::Log("Loop through all codecs for %d seconds",
                 kAutoTestSleepTimeMs / 1000);

    for (int i = 0; i < codec->NumberOfCodecs() - 2; i++) {
      EXPECT_EQ(0, codec->GetCodec(i, video_codec));
      if (video_codec.codecType == webrtc::kVideoCodecI420) {
        // Lower resolution to sockets keep up.
        video_codec.width = 176;
        video_codec.height = 144;
        video_codec.maxFramerate = 15;
      }
      EXPECT_EQ(0, codec->SetSendCodec(video_channel, video_codec));
      ViETest::Log("\t %d. %s", i, video_codec.plName);

      RenderFilter frame_counter;
      EXPECT_EQ(0, image_process->RegisterRenderEffectFilter(video_channel,
                                                             frame_counter));
      AutoTestSleep(kAutoTestSleepTimeMs);

      // Verify we've received and decoded correct payload.
      EXPECT_EQ(video_codec.codecType,
                codec_observer.incoming_codec_.codecType);

      // This requirement is quite relaxed, but it's hard to say what's an
      // acceptable number of received frames when we take into account the
      // wide variety of devices (and that we run under valgrind).
      EXPECT_GT(frame_counter.num_frames_, 0);

      EXPECT_EQ(0, image_process->DeregisterRenderEffectFilter(
          video_channel));
    }
    image_process->Release();
    EXPECT_EQ(0, codec->DeregisterDecoderObserver(video_channel));
    ViETest::Log("Done!");
  }

  // Test Callbacks
  TestCodecObserver codec_observer;
  EXPECT_EQ(0, codec->RegisterEncoderObserver(video_channel, codec_observer));
  EXPECT_EQ(0, codec->RegisterDecoderObserver(video_channel, codec_observer));

  ViETest::Log("\nTesting codec callbacks...");

  for (int idx = 0; idx < codec->NumberOfCodecs(); idx++) {
    EXPECT_EQ(0, codec->GetCodec(idx, video_codec));
    if (video_codec.codecType == webrtc::kVideoCodecVP8) {
      EXPECT_EQ(0, codec->SetSendCodec(video_channel, video_codec));
      break;
    }
  }
  AutoTestSleep(kAutoTestSleepTimeMs);

  // Verify the delay estimates are larger than 0.
  int avg_send_delay = 0;
  int max_send_delay = 0;
  EXPECT_TRUE(codec->GetSendSideDelay(video_channel, &avg_send_delay,
                                      &max_send_delay));
  EXPECT_GT(avg_send_delay, 0);
  EXPECT_GE(max_send_delay, avg_send_delay);
  int receive_delay_ms = 0;
  EXPECT_EQ(0, codec->GetReceiveSideDelay(video_channel, &receive_delay_ms));
  EXPECT_GT(receive_delay_ms, 0);

  EXPECT_EQ(0, base->StopSend(video_channel));
  EXPECT_EQ(0, codec->DeregisterEncoderObserver(video_channel));
  EXPECT_EQ(0, codec->DeregisterDecoderObserver(video_channel));

  EXPECT_GT(codec_observer.incoming_codec_called_, 0);
  EXPECT_GT(codec_observer.incoming_rate_called_, 0);
  EXPECT_GT(codec_observer.decoder_timing_called_, 0);
  EXPECT_GT(codec_observer.outgoing_rate_called_, 0);

  EXPECT_EQ(0, base->StopReceive(video_channel));
  EXPECT_EQ(0, render->StopRender(video_channel));
  EXPECT_EQ(0, render->RemoveRenderer(capture_id));
  EXPECT_EQ(0, render->RemoveRenderer(video_channel));
  EXPECT_EQ(0, capture->DisconnectCaptureDevice(video_channel));
  EXPECT_EQ(0, base->DeleteChannel(video_channel));
}

void ViEAutoTest::ViECodecExtendedTest() {
  {
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViECodec Extended Test\n");

    ViECodecExternalCodecTest();

    TbInterfaces interfaces("ViECodecExtendedTest");
    webrtc::ViEBase* base = interfaces.base;
    webrtc::ViECapture* capture = interfaces.capture;
    webrtc::ViERender* render = interfaces.render;
    webrtc::ViECodec* codec = interfaces.codec;
    webrtc::ViERTP_RTCP* rtp_rtcp = interfaces.rtp_rtcp;
    webrtc::ViENetwork* network = interfaces.network;

    TbCaptureDevice capture_device = TbCaptureDevice(interfaces);
    int capture_id = capture_device.captureId;

    int video_channel = -1;
    EXPECT_EQ(0, base->CreateChannel(video_channel));
    EXPECT_EQ(0, capture->ConnectCaptureDevice(capture_id, video_channel));
    EXPECT_EQ(0, rtp_rtcp->SetRTCPStatus(
                video_channel, webrtc::kRtcpCompound_RFC4585));
    EXPECT_EQ(0, rtp_rtcp->SetKeyFrameRequestMethod(
                video_channel, webrtc::kViEKeyFrameRequestPliRtcp));
    EXPECT_EQ(0, rtp_rtcp->SetTMMBRStatus(video_channel, true));
    EXPECT_EQ(0, render->AddRenderer(capture_id, _window1, 0, 0.0, 0.0, 1.0,
                                     1.0));

    EXPECT_EQ(0, render->AddRenderer(video_channel, _window2, 1, 0.0, 0.0, 1.0,
                                     1.0));
    EXPECT_EQ(0, render->StartRender(capture_id));
    EXPECT_EQ(0, render->StartRender(video_channel));

    webrtc::VideoCodec video_codec;
    memset(&video_codec, 0, sizeof(webrtc::VideoCodec));
    for (int idx = 0; idx < codec->NumberOfCodecs(); idx++) {
      EXPECT_EQ(0, codec->GetCodec(idx, video_codec));
      if (video_codec.codecType != webrtc::kVideoCodecI420) {
        video_codec.width = 640;
        video_codec.height = 480;
      }
      EXPECT_EQ(0, codec->SetReceiveCodec(video_channel, video_codec));
    }

    const char* ip_address = "127.0.0.1";
    const uint16_t rtp_port = 6000;

    rtc::scoped_ptr<webrtc::test::VideoChannelTransport>
        video_channel_transport(
            new webrtc::test::VideoChannelTransport(network, video_channel));

    ASSERT_EQ(0, video_channel_transport->SetSendDestination(ip_address,
                                                             rtp_port));
    ASSERT_EQ(0, video_channel_transport->SetLocalReceiver(rtp_port));

    EXPECT_EQ(0, base->StartSend(video_channel));
    EXPECT_EQ(0, base->StartReceive(video_channel));

    // Codec specific tests
    memset(&video_codec, 0, sizeof(webrtc::VideoCodec));
    EXPECT_EQ(0, base->StopSend(video_channel));

    TestCodecObserver codec_observer;
    EXPECT_EQ(0, codec->RegisterEncoderObserver(video_channel, codec_observer));
    EXPECT_EQ(0, codec->RegisterDecoderObserver(video_channel, codec_observer));
    EXPECT_EQ(0, base->StopReceive(video_channel));

    EXPECT_EQ(0, render->StopRender(video_channel));
    EXPECT_EQ(0, render->RemoveRenderer(capture_id));
    EXPECT_EQ(0, render->RemoveRenderer(video_channel));
    EXPECT_EQ(0, capture->DisconnectCaptureDevice(video_channel));
    EXPECT_EQ(0, base->DeleteChannel(video_channel));
  }

  // Multiple send channels.
  {
    // Create two channels, where the second channel is created from the
    // first channel. Send different resolutions on the channels and verify
    // the received streams.
    TbInterfaces video_engine("ViECodecExtendedTest2");
    TbCaptureDevice tb_capture(video_engine);
    webrtc::ViENetwork* network = video_engine.network;

    // Create channel 1.
    int video_channel_1 = -1;
    EXPECT_EQ(0, video_engine.base->CreateChannel(video_channel_1));

    // Create channel 2 based on the first channel.
    int video_channel_2 = -1;
    EXPECT_EQ(0, video_engine.base->CreateChannel(
        video_channel_2, video_channel_1));
    EXPECT_NE(video_channel_1, video_channel_2)
        << "Channel 2 should be unique.";

    const char* ip_address = "127.0.0.1";
    uint16_t rtp_port_1 = 12000;
    uint16_t rtp_port_2 = 13000;

    rtc::scoped_ptr<webrtc::test::VideoChannelTransport>
        video_channel_transport_1(
            new webrtc::test::VideoChannelTransport(network, video_channel_1));

    ASSERT_EQ(0, video_channel_transport_1->SetSendDestination(ip_address,
                                                               rtp_port_1));
    ASSERT_EQ(0, video_channel_transport_1->SetLocalReceiver(rtp_port_1));

    rtc::scoped_ptr<webrtc::test::VideoChannelTransport>
        video_channel_transport_2(
            new webrtc::test::VideoChannelTransport(network, video_channel_2));

    ASSERT_EQ(0, video_channel_transport_2->SetSendDestination(ip_address,
                                                               rtp_port_2));
    ASSERT_EQ(0, video_channel_transport_2->SetLocalReceiver(rtp_port_2));

    EXPECT_EQ(0, video_engine.rtp_rtcp->SetLocalSSRC(video_channel_1, 1));
    EXPECT_EQ(0, video_engine.rtp_rtcp->SetLocalSSRC(video_channel_2, 2));
    tb_capture.ConnectTo(video_channel_1);
    tb_capture.ConnectTo(video_channel_2);
    EXPECT_EQ(0, video_engine.rtp_rtcp->SetKeyFrameRequestMethod(
        video_channel_1, webrtc::kViEKeyFrameRequestPliRtcp));
    EXPECT_EQ(0, video_engine.rtp_rtcp->SetKeyFrameRequestMethod(
        video_channel_2, webrtc::kViEKeyFrameRequestPliRtcp));
    EXPECT_EQ(0, video_engine.render->AddRenderer(video_channel_1, _window1, 0,
                                                  0.0, 0.0, 1.0, 1.0));
    EXPECT_EQ(0, video_engine.render->StartRender(video_channel_1));
    EXPECT_EQ(0, video_engine.render->AddRenderer(video_channel_2, _window2, 0,
                                                  0.0, 0.0, 1.0, 1.0));
    EXPECT_EQ(0, video_engine.render->StartRender(video_channel_2));

    // Set Send codec.
    uint16_t codec_width = 320;
    uint16_t codec_height = 240;
    bool codec_set = false;
    webrtc::VideoCodec video_codec;
    webrtc::VideoCodec send_codec1;
    webrtc::VideoCodec send_codec2;
    for (int idx = 0; idx < video_engine.codec->NumberOfCodecs(); idx++) {
      EXPECT_EQ(0, video_engine.codec->GetCodec(idx, video_codec));
      EXPECT_EQ(0, video_engine.codec->SetReceiveCodec(video_channel_1,
                                                       video_codec));
      if (video_codec.codecType == webrtc::kVideoCodecVP8) {
        memcpy(&send_codec1, &video_codec, sizeof(video_codec));
        send_codec1.width = codec_width;
        send_codec1.height = codec_height;
        EXPECT_EQ(0, video_engine.codec->SetSendCodec(
                    video_channel_1, send_codec1));
        memcpy(&send_codec2, &video_codec, sizeof(video_codec));
        send_codec2.width = 2 * codec_width;
        send_codec2.height = 2 * codec_height;
        EXPECT_EQ(0, video_engine.codec->SetSendCodec(
                    video_channel_2, send_codec2));
        codec_set = true;
        break;
      }
    }
    EXPECT_TRUE(codec_set);

    // We need to verify using render effect filter since we won't trigger
    // a decode reset in loopback (due to using the same SSRC).
    RenderFilter filter1;
    RenderFilter filter2;
    EXPECT_EQ(0, video_engine.image_process->RegisterRenderEffectFilter(
        video_channel_1, filter1));
    EXPECT_EQ(0, video_engine.image_process->RegisterRenderEffectFilter(
        video_channel_2, filter2));

    EXPECT_EQ(0, video_engine.base->StartReceive(video_channel_1));
    EXPECT_EQ(0, video_engine.base->StartSend(video_channel_1));
    EXPECT_EQ(0, video_engine.base->StartReceive(video_channel_2));
    EXPECT_EQ(0, video_engine.base->StartSend(video_channel_2));

    AutoTestSleep(kAutoTestSleepTimeMs);

    EXPECT_EQ(0, video_engine.base->StopReceive(video_channel_1));
    EXPECT_EQ(0, video_engine.base->StopSend(video_channel_1));
    EXPECT_EQ(0, video_engine.base->StopReceive(video_channel_2));
    EXPECT_EQ(0, video_engine.base->StopSend(video_channel_2));

    EXPECT_EQ(0, video_engine.image_process->DeregisterRenderEffectFilter(
        video_channel_1));
    EXPECT_EQ(0, video_engine.image_process->DeregisterRenderEffectFilter(
        video_channel_2));
    EXPECT_EQ(send_codec1.width, filter1.last_render_width_);
    EXPECT_EQ(send_codec1.height, filter1.last_render_height_);
    EXPECT_EQ(send_codec2.width, filter2.last_render_width_);
    EXPECT_EQ(send_codec2.height, filter2.last_render_height_);

    EXPECT_EQ(0, video_engine.base->DeleteChannel(video_channel_1));
    EXPECT_EQ(0, video_engine.base->DeleteChannel(video_channel_2));
  }
}

void ViEAutoTest::ViECodecAPITest() {
  webrtc::VideoEngine* video_engine = NULL;
  video_engine = webrtc::VideoEngine::Create();
  EXPECT_TRUE(video_engine != NULL);

  webrtc::ViEBase* base = webrtc::ViEBase::GetInterface(video_engine);
  EXPECT_EQ(0, base->Init());

  int video_channel = -1;
  EXPECT_EQ(0, base->CreateChannel(video_channel));

  webrtc::ViECodec* codec = webrtc::ViECodec::GetInterface(video_engine);
  EXPECT_TRUE(codec != NULL);

  webrtc::VideoCodec video_codec;
  memset(&video_codec, 0, sizeof(webrtc::VideoCodec));

  const int number_of_codecs = codec->NumberOfCodecs();

  for (int i = 0; i < number_of_codecs; i++) {
    EXPECT_EQ(0, codec->GetCodec(i, video_codec));
    if (video_codec.codecType == webrtc::kVideoCodecVP8) {
      video_codec.codecSpecific.VP8.automaticResizeOn = true;
      video_codec.codecSpecific.VP8.frameDroppingOn = true;
      video_codec.codecSpecific.VP8.keyFrameInterval = 300;
      EXPECT_EQ(0, codec->SetSendCodec(video_channel, video_codec));
      break;
    }
  }
  const unsigned int kMinBitrate = 123;
  video_codec.minBitrate = kMinBitrate;
  video_codec.startBitrate = 50;
  EXPECT_EQ(0, codec->SetSendCodec(video_channel, video_codec));
  EXPECT_EQ(0, codec->GetSendCodec(video_channel, video_codec));
  // We don't allow allocated start bitrate to be decreased via SetSendCodec,
  // and the default bitrate available in the allocator is 300.
  EXPECT_EQ(300u, video_codec.startBitrate);

  memset(&video_codec, 0, sizeof(video_codec));
  EXPECT_EQ(0, codec->GetSendCodec(video_channel, video_codec));
  EXPECT_EQ(webrtc::kVideoCodecVP8, video_codec.codecType);
  EXPECT_TRUE(video_codec.codecSpecific.VP8.automaticResizeOn);
  EXPECT_TRUE(video_codec.codecSpecific.VP8.frameDroppingOn);
  EXPECT_EQ(300, video_codec.codecSpecific.VP8.keyFrameInterval);

  for (int i = 0; i < number_of_codecs; i++) {
    EXPECT_EQ(0, codec->GetCodec(i, video_codec));
    if (video_codec.codecType == webrtc::kVideoCodecI420) {
      EXPECT_EQ(0, codec->SetSendCodec(video_channel, video_codec));
      break;
    }
  }

  memset(&video_codec, 0, sizeof(video_codec));
  EXPECT_EQ(0, codec->GetSendCodec(video_channel, video_codec));
  EXPECT_EQ(webrtc::kVideoCodecI420, video_codec.codecType);

  // Register a generic codec
  memset(&video_codec, 0, sizeof(video_codec));
  video_codec.codecType = webrtc::kVideoCodecGeneric;
  strcpy(video_codec.plName, "generic-codec");
  uint8_t payload_type = 127;
  video_codec.plType = payload_type;
  video_codec.minBitrate = 100;
  video_codec.startBitrate = 500;
  video_codec.maxBitrate = 10000;
  video_codec.width = 1920;
  video_codec.height = 1080;
  video_codec.maxFramerate = 30;
  video_codec.qpMax = 50;

  webrtc::ViEExternalCodec* external_codec =
      webrtc::ViEExternalCodec::GetInterface(video_engine);
  EXPECT_TRUE(external_codec != NULL);

  // Any encoder will do.
  webrtc::I420Encoder encoder;
  EXPECT_EQ(0, external_codec->RegisterExternalSendCodec(video_channel,
                                                         payload_type, &encoder,
                                                         false));
  EXPECT_EQ(0, codec->SetSendCodec(video_channel, video_codec));

  memset(&video_codec, 0, sizeof(video_codec));
  EXPECT_EQ(0, codec->GetSendCodec(video_channel, video_codec));
  EXPECT_EQ(webrtc::kVideoCodecGeneric, video_codec.codecType);

  EXPECT_EQ(0, base->DeleteChannel(video_channel));

  EXPECT_EQ(0, external_codec->Release());
  EXPECT_EQ(0, codec->Release());
  EXPECT_EQ(0, base->Release());
  EXPECT_TRUE(webrtc::VideoEngine::Delete(video_engine));
}

void ViEAutoTest::ViECodecExternalCodecTest() {
  ViETest::Log(" ");
  ViETest::Log("========================================");
  ViETest::Log(" ViEExternalCodec Test\n");

  /// **************************************************************
  //  Begin create/initialize WebRTC Video Engine for testing
  /// **************************************************************

  /// **************************************************************
  //  Engine ready. Begin testing class
  /// **************************************************************

#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
  int number_of_errors = 0;
  {
    int error = 0;
    TbInterfaces ViE("ViEExternalCodec");
    TbCaptureDevice capture_device(ViE);
    TbVideoChannel channel(ViE, webrtc::kVideoCodecI420, 352, 288, 30,
                           (352 * 288 * 3 * 8 * 30) / (2 * 1000));
    capture_device.ConnectTo(channel.videoChannel);

    error = ViE.render->AddRenderer(channel.videoChannel, _window1, 0, 0.0, 0.0,
                                    1.0, 1.0);
    number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
    error = ViE.render->StartRender(channel.videoChannel);
    number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    channel.StartReceive();
    channel.StartSend();

    ViETest::Log("Using internal I420 codec");
    AutoTestSleep(kAutoTestSleepTimeMs / 2);

    webrtc::ViEExternalCodec* vie_external_codec =
        webrtc::ViEExternalCodec::GetInterface(ViE.video_engine);
    number_of_errors += ViETest::TestError(vie_external_codec != NULL,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
    webrtc::VideoCodec codec;
    error = ViE.codec->GetSendCodec(channel.videoChannel, codec);
    number_of_errors += ViETest::TestError(vie_external_codec != NULL,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    // Use external encoder instead.
    {
      TbI420Encoder ext_encoder;

      // Test to register on wrong channel.
      error = vie_external_codec->RegisterExternalSendCodec(
          channel.videoChannel + 5, codec.plType, &ext_encoder, false);
      number_of_errors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      number_of_errors += ViETest::TestError(
          ViE.LastError() == kViECodecInvalidArgument,
          "ERROR: %s at line %d", __FUNCTION__, __LINE__);

      error = vie_external_codec->RegisterExternalSendCodec(
                channel.videoChannel, codec.plType, &ext_encoder, false);
      number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

      // Use new external encoder
      error = ViE.codec->SetSendCodec(channel.videoChannel, codec);
      number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

      TbI420Decoder ext_decoder;
      error = vie_external_codec->RegisterExternalReceiveCodec(
          channel.videoChannel, codec.plType, &ext_decoder);
      number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

      error = ViE.codec->SetReceiveCodec(channel.videoChannel, codec);
      number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

      ViETest::Log("Using external I420 codec");
      AutoTestSleep(kAutoTestSleepTimeMs);

      // Test to deregister on wrong channel
      error = vie_external_codec->DeRegisterExternalSendCodec(
          channel.videoChannel + 5, codec.plType);
      number_of_errors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      number_of_errors += ViETest::TestError(
          ViE.LastError() == kViECodecInvalidArgument, "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);

      // Test to deregister wrong payload type.
      error = vie_external_codec->DeRegisterExternalSendCodec(
          channel.videoChannel, codec.plType - 1);
      number_of_errors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

      // Deregister external send codec
      error = vie_external_codec->DeRegisterExternalSendCodec(
          channel.videoChannel, codec.plType);
      number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

      error = vie_external_codec->DeRegisterExternalReceiveCodec(
          channel.videoChannel, codec.plType);
      number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

      // Verify that the encoder and decoder has been used
      TbI420Encoder::FunctionCalls encode_calls =
          ext_encoder.GetFunctionCalls();
      number_of_errors += ViETest::TestError(encode_calls.InitEncode == 1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      number_of_errors += ViETest::TestError(encode_calls.Release == 1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      number_of_errors += ViETest::TestError(encode_calls.Encode > 30,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      number_of_errors += ViETest::TestError(
          encode_calls.RegisterEncodeCompleteCallback == 1,
          "ERROR: %s at line %d", __FUNCTION__, __LINE__);
      number_of_errors += ViETest::TestError(
          encode_calls.SetChannelParameters > 1, "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
      number_of_errors += ViETest::TestError(encode_calls.SetRates > 1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

      TbI420Decoder::FunctionCalls decode_calls =
          ext_decoder.GetFunctionCalls();
      number_of_errors += ViETest::TestError(decode_calls.InitDecode == 1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      number_of_errors += ViETest::TestError(decode_calls.Release == 1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      number_of_errors += ViETest::TestError(decode_calls.Decode > 30,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      number_of_errors += ViETest::TestError(
          decode_calls.RegisterDecodeCompleteCallback == 1,
          "ERROR: %s at line %d", __FUNCTION__, __LINE__);

      ViETest::Log("Changing payload type Using external I420 codec");

      codec.plType = codec.plType - 1;
      error = vie_external_codec->RegisterExternalReceiveCodec(
          channel.videoChannel, codec.plType, &ext_decoder);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

      error = ViE.codec->SetReceiveCodec(channel.videoChannel,
                                         codec);
      number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

      error = vie_external_codec->RegisterExternalSendCodec(
                channel.videoChannel, codec.plType, &ext_encoder, false);
      number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

      // Use new external encoder
      error = ViE.codec->SetSendCodec(channel.videoChannel,
                                      codec);
      number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

      AutoTestSleep(kAutoTestSleepTimeMs / 2);

      /// **************************************************************
      //  Testing finished. Tear down Video Engine
      /// **************************************************************

      error = vie_external_codec->DeRegisterExternalSendCodec(
                channel.videoChannel, codec.plType);
      number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      error = vie_external_codec->DeRegisterExternalReceiveCodec(
                channel.videoChannel, codec.plType);
      number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

      // Verify that the encoder and decoder has been used
      encode_calls = ext_encoder.GetFunctionCalls();
      number_of_errors += ViETest::TestError(encode_calls.InitEncode == 2,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      number_of_errors += ViETest::TestError(encode_calls.Release == 2,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      number_of_errors += ViETest::TestError(encode_calls.Encode > 30,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      number_of_errors += ViETest::TestError(
          encode_calls.RegisterEncodeCompleteCallback == 2,
          "ERROR: %s at line %d", __FUNCTION__, __LINE__);
      number_of_errors += ViETest::TestError(
          encode_calls.SetChannelParameters > 1, "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
      number_of_errors += ViETest::TestError(encode_calls.SetRates > 1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      decode_calls = ext_decoder.GetFunctionCalls();
      number_of_errors += ViETest::TestError(decode_calls.InitDecode == 2,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      number_of_errors += ViETest::TestError(decode_calls.Release == 2,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      number_of_errors += ViETest::TestError(decode_calls.Decode > 30,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      number_of_errors += ViETest::TestError(
          decode_calls.RegisterDecodeCompleteCallback == 2,
          "ERROR: %s at line %d", __FUNCTION__, __LINE__);

      int remaining_interfaces = vie_external_codec->Release();
      number_of_errors += ViETest::TestError(remaining_interfaces == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    }  // tbI420Encoder and ext_decoder goes out of scope.

    ViETest::Log("Using internal I420 codec");
    AutoTestSleep(kAutoTestSleepTimeMs / 2);
  }
  if (number_of_errors > 0) {
    // Test failed
    ViETest::Log(" ");
    ViETest::Log(" ERROR ViEExternalCodec Test FAILED!");
    ViETest::Log(" Number of errors: %d", number_of_errors);
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return;
  }

  ViETest::Log(" ");
  ViETest::Log(" ViEExternalCodec Test PASSED!");
  ViETest::Log("========================================");
  ViETest::Log(" ");
  return;

#else
  ViETest::Log(" ViEExternalCodec not enabled\n");
  return;
#endif
}
