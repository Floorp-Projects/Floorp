/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vie_autotest_defines.h"
#include "vie_autotest.h"
#include "engine_configurations.h"

#include "common_types.h"
#include "tb_capture_device.h"
#include "tb_I420_codec.h"
#include "tb_interfaces.h"
#include "tb_video_channel.h"
#include "vie_base.h"
#include "vie_capture.h"
#include "vie_codec.h"
#include "vie_network.h"
#include "vie_render.h"
#include "vie_rtp_rtcp.h"
#include "voe_base.h"

class TestCodecObserver : public webrtc::ViEEncoderObserver,
    public webrtc::ViEDecoderObserver {
public:
  int incoming_codec_called_;
  int incoming_rate_called_;
  int outgoing_rate_called_;

  unsigned char last_payload_type_;
  unsigned short last_width_;
  unsigned short last_height_;

  unsigned int last_outgoing_framerate_;
  unsigned int last_outgoing_bitrate_;
  unsigned int last_incoming_framerate_;
  unsigned int last_incoming_bitrate_;

  webrtc::VideoCodec incoming_codec_;

  TestCodecObserver() {
    incoming_codec_called_ = 0;
    incoming_rate_called_ = 0;
    outgoing_rate_called_ = 0;
    last_payload_type_ = 0;
    last_width_ = 0;
    last_height_ = 0;
    last_outgoing_framerate_ = 0;
    last_outgoing_bitrate_ = 0;
    last_incoming_framerate_ = 0;
    last_incoming_bitrate_ = 0;
    memset(&incoming_codec_, 0, sizeof(incoming_codec_));
  }
  virtual void IncomingCodecChanged(const int videoChannel,
      const webrtc::VideoCodec& videoCodec) {
    incoming_codec_called_++;
    last_payload_type_ = videoCodec.plType;
    last_width_ = videoCodec.width;
    last_height_ = videoCodec.height;

    memcpy(&incoming_codec_, &videoCodec, sizeof(videoCodec));
  }

  virtual void IncomingRate(const int videoChannel,
      const unsigned int framerate, const unsigned int bitrate) {
    incoming_rate_called_++;
    last_incoming_framerate_ += framerate;
    last_incoming_bitrate_ += bitrate;
  }

  virtual void OutgoingRate(const int videoChannel,
      const unsigned int framerate, const unsigned int bitrate) {
    outgoing_rate_called_++;
    last_outgoing_framerate_ += framerate;
    last_outgoing_bitrate_ += bitrate;
  }

  virtual void RequestNewKeyFrame(const int videoChannel) {
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

  virtual int Transform(int size, unsigned char* frameBuffer,
      unsigned int timeStamp90KHz, unsigned int width, unsigned int height) {
    num_frames_++;
    last_render_width_ = width;
    last_render_height_ = height;
    return 0;
  }
};

void ViEAutoTest::ViECodecStandardTest() {
  TbInterfaces interfaces = TbInterfaces("ViECodecStandardTest");

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
  EXPECT_EQ( 0, rtp_rtcp->SetRTCPStatus(
      video_channel, webrtc::kRtcpCompound_RFC4585));

  EXPECT_EQ(0, rtp_rtcp->SetKeyFrameRequestMethod(
      video_channel, webrtc::kViEKeyFrameRequestPliRtcp));
  EXPECT_EQ(0, rtp_rtcp->SetTMMBRStatus(video_channel, true));
  EXPECT_EQ(0, render->AddRenderer(
      capture_id, _window1, 0, 0.0, 0.0, 1.0, 1.0));
  EXPECT_EQ(0, render->AddRenderer(
      video_channel, _window2, 1, 0.0, 0.0, 1.0, 1.0));
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
  const unsigned short rtp_port = 6000;
  EXPECT_EQ(0, network->SetLocalReceiver(video_channel, rtp_port));
  EXPECT_EQ(0, base->StartReceive(video_channel));
  EXPECT_EQ(0, network->SetSendDestination(
      video_channel, ip_address, rtp_port));
  EXPECT_EQ(0, base->StartSend(video_channel));

  //
  // Make sure all codecs runs
  //
  {
    webrtc::ViEImageProcess* image_process =
        webrtc::ViEImageProcess::GetInterface(video_engine);
    TestCodecObserver codec_observer;
    EXPECT_EQ(0, codec->RegisterDecoderObserver(
        video_channel, codec_observer));
    ViETest::Log("Loop through all codecs for %d seconds",
        KAutoTestSleepTimeMs / 1000);

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
      EXPECT_EQ(0, image_process->RegisterRenderEffectFilter(
          video_channel, frame_counter));
      AutoTestSleep(KAutoTestSleepTimeMs);

      // Verify we've received and decoded correct payload.
      EXPECT_EQ(video_codec.codecType,
          codec_observer.incoming_codec_.codecType);

      int max_number_of_possible_frames = video_codec.maxFramerate
          * KAutoTestSleepTimeMs / 1000;

      if (video_codec.codecType == webrtc::kVideoCodecI420) {
        // Don't expect too much from I420, it requires a lot of bandwidth.
        EXPECT_GT(frame_counter.num_frames_, 0);
      } else {
#ifdef WEBRTC_ANDROID
        // To get the autotest to pass on some slow devices
        EXPECT_GT(
            frame_counter.num_frames_, max_number_of_possible_frames / 6);
#else
        EXPECT_GT(frame_counter.num_frames_, max_number_of_possible_frames / 4);
#endif
      }

      EXPECT_EQ(0, image_process->DeregisterRenderEffectFilter(
          video_channel));
    }
    image_process->Release();
    EXPECT_EQ(0, codec->DeregisterDecoderObserver(video_channel));
    ViETest::Log("Done!");
  }

  //
  // Test Callbacks
  //

  TestCodecObserver codecObserver;
  EXPECT_EQ(0, codec->RegisterEncoderObserver(
      video_channel, codecObserver));
  EXPECT_EQ(0, codec->RegisterDecoderObserver(
      video_channel, codecObserver));

  ViETest::Log("\nTesting codec callbacks...");

  for (int idx = 0; idx < codec->NumberOfCodecs(); idx++) {
    EXPECT_EQ(0, codec->GetCodec(idx, video_codec));
    if (video_codec.codecType == webrtc::kVideoCodecVP8) {
      EXPECT_EQ(0, codec->SetSendCodec(video_channel, video_codec));
      break;
    }
  }
  AutoTestSleep(KAutoTestSleepTimeMs);

  EXPECT_EQ(0, base->StopSend(video_channel));
  EXPECT_EQ(0, codec->DeregisterEncoderObserver(video_channel));
  EXPECT_EQ(0, codec->DeregisterDecoderObserver(video_channel));

  EXPECT_GT(codecObserver.incoming_codec_called_, 0);
  EXPECT_GT(codecObserver.incoming_rate_called_, 0);
  EXPECT_GT(codecObserver.outgoing_rate_called_, 0);

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

    ViECodecAPITest();
    ViECodecStandardTest();
    ViECodecExternalCodecTest();

    TbInterfaces interfaces = TbInterfaces("ViECodecExtendedTest");
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
    EXPECT_EQ(0, render->AddRenderer(
        capture_id, _window1, 0, 0.0, 0.0, 1.0, 1.0));

    EXPECT_EQ(0, render->AddRenderer(
        video_channel, _window2, 1, 0.0, 0.0, 1.0, 1.0));
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
    const unsigned short rtp_port = 6000;
    EXPECT_EQ(0, network->SetLocalReceiver(video_channel, rtp_port));
    EXPECT_EQ(0, base->StartReceive(video_channel));
    EXPECT_EQ(0, network->SetSendDestination(
        video_channel, ip_address, rtp_port));
    EXPECT_EQ(0, base->StartSend(video_channel));

    //
    // Codec specific tests
    //
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

  //
  // Multiple send channels.
  //
  {
    // Create two channels, where the second channel is created from the
    // first channel. Send different resolutions on the channels and verify
    // the received streams.

    TbInterfaces video_engine("ViECodecExtendedTest2");
    TbCaptureDevice tbCapture(video_engine);

    // Create channel 1.
    int video_channel_1 = -1;
    EXPECT_EQ(0, video_engine.base->CreateChannel(video_channel_1));

    // Create channel 2 based on the first channel.
    int video_channel_2 = -1;
    EXPECT_EQ(0, video_engine.base->CreateChannel(
        video_channel_2, video_channel_1));
    EXPECT_NE(video_channel_1, video_channel_2)
      << "Channel 2 should be unique.";

    unsigned short rtp_port_1 = 12000;
    unsigned short rtp_port_2 = 13000;
    EXPECT_EQ(0, video_engine.network->SetLocalReceiver(
        video_channel_1, rtp_port_1));
    EXPECT_EQ(0, video_engine.network->SetSendDestination(
        video_channel_1, "127.0.0.1", rtp_port_1));
    EXPECT_EQ(0, video_engine.network->SetLocalReceiver(
        video_channel_2, rtp_port_2));
    EXPECT_EQ(0, video_engine.network->SetSendDestination(
        video_channel_2, "127.0.0.1", rtp_port_2));

    tbCapture.ConnectTo(video_channel_1);
    tbCapture.ConnectTo(video_channel_2);

    EXPECT_EQ(0, video_engine.rtp_rtcp->SetKeyFrameRequestMethod(
        video_channel_1, webrtc::kViEKeyFrameRequestPliRtcp));
    EXPECT_EQ(0, video_engine.rtp_rtcp->SetKeyFrameRequestMethod(
        video_channel_2, webrtc::kViEKeyFrameRequestPliRtcp));
    EXPECT_EQ(0, video_engine.render->AddRenderer(
        video_channel_1, _window1, 0, 0.0, 0.0, 1.0, 1.0));
    EXPECT_EQ(0, video_engine.render->StartRender(video_channel_1));
    EXPECT_EQ(0, video_engine.render->AddRenderer(
        video_channel_2, _window2, 0, 0.0, 0.0, 1.0, 1.0));
    EXPECT_EQ(0, video_engine.render->StartRender(video_channel_2));

    // Set Send codec
    unsigned short codecWidth = 320;
    unsigned short codecHeight = 240;
    bool codecSet = false;
    webrtc::VideoCodec videoCodec;
    webrtc::VideoCodec sendCodec1;
    webrtc::VideoCodec sendCodec2;
    for (int idx = 0; idx < video_engine.codec->NumberOfCodecs(); idx++) {
      EXPECT_EQ(0, video_engine.codec->GetCodec(idx, videoCodec));
      EXPECT_EQ(0, video_engine.codec->SetReceiveCodec(
          video_channel_1, videoCodec));
      if (videoCodec.codecType == webrtc::kVideoCodecVP8) {
        memcpy(&sendCodec1, &videoCodec, sizeof(videoCodec));
        sendCodec1.width = codecWidth;
        sendCodec1.height = codecHeight;
        EXPECT_EQ(0, video_engine.codec->SetSendCodec(
            video_channel_1, sendCodec1));
        memcpy(&sendCodec2, &videoCodec, sizeof(videoCodec));
        sendCodec2.width = 2 * codecWidth;
        sendCodec2.height = 2 * codecHeight;
        EXPECT_EQ(0, video_engine.codec->SetSendCodec(
            video_channel_2, sendCodec2));
        codecSet = true;
        break;
      }
    }EXPECT_TRUE(codecSet);

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

    AutoTestSleep(KAutoTestSleepTimeMs);

    EXPECT_EQ(0, video_engine.base->StopReceive(video_channel_1));
    EXPECT_EQ(0, video_engine.base->StopSend(video_channel_1));
    EXPECT_EQ(0, video_engine.base->StopReceive(video_channel_2));
    EXPECT_EQ(0, video_engine.base->StopSend(video_channel_2));

    EXPECT_EQ(0, video_engine.image_process->DeregisterRenderEffectFilter(
        video_channel_1));
    EXPECT_EQ(0, video_engine.image_process->DeregisterRenderEffectFilter(
        video_channel_2));
    EXPECT_EQ(sendCodec1.width, filter1.last_render_width_);
    EXPECT_EQ(sendCodec1.height, filter1.last_render_height_);
    EXPECT_EQ(sendCodec2.width, filter2.last_render_width_);
    EXPECT_EQ(sendCodec2.height, filter2.last_render_height_);

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
      EXPECT_EQ(0, codec->SetSendCodec(video_channel, video_codec));
      break;
    }
  }
  memset(&video_codec, 0, sizeof(video_codec));
  EXPECT_EQ(0, codec->GetSendCodec(video_channel, video_codec));
  EXPECT_EQ(webrtc::kVideoCodecVP8, video_codec.codecType);

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

  EXPECT_EQ(0, base->DeleteChannel(video_channel));

  EXPECT_EQ(0, codec->Release());
  EXPECT_EQ(0, base->Release());
  EXPECT_TRUE(webrtc::VideoEngine::Delete(video_engine));
}

#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
#include "vie_external_codec.h"
#endif
void ViEAutoTest::ViECodecExternalCodecTest() {
  ViETest::Log(" ");
  ViETest::Log("========================================");
  ViETest::Log(" ViEExternalCodec Test\n");

  //***************************************************************
  //  Begin create/initialize WebRTC Video Engine for testing
  //***************************************************************

  //***************************************************************
  //  Engine ready. Begin testing class
  //***************************************************************

#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
  int numberOfErrors=0;
  {
    int error=0;
    TbInterfaces ViE("ViEExternalCodec");
    TbCaptureDevice captureDevice(ViE);
    TbVideoChannel channel(ViE, webrtc::kVideoCodecI420,
        352,288,30,(352*288*3*8*30)/(2*1000));

    captureDevice.ConnectTo(channel.videoChannel);

    error = ViE.render->AddRenderer(channel.videoChannel, _window1, 0,
        0.0, 0.0, 1.0, 1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
        __FUNCTION__, __LINE__);
    error = ViE.render->StartRender(channel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
        __FUNCTION__, __LINE__);

    channel.StartReceive();
    channel.StartSend();

    ViETest::Log("Using internal I420 codec");
    AutoTestSleep(KAutoTestSleepTimeMs/2);

    webrtc::ViEExternalCodec* ptrViEExtCodec =
      webrtc::ViEExternalCodec::GetInterface(ViE.video_engine);
    numberOfErrors += ViETest::TestError(ptrViEExtCodec != NULL,
        "ERROR: %s at line %d",
        __FUNCTION__, __LINE__);

    webrtc::VideoCodec codecStruct;

    error=ViE.codec->GetSendCodec(channel.videoChannel,codecStruct);
    numberOfErrors += ViETest::TestError(ptrViEExtCodec != NULL,
        "ERROR: %s at line %d",
        __FUNCTION__, __LINE__);

    // Use external encoder instead
    {
      TbI420Encoder extEncoder;

      // Test to register on wrong channel
      error = ptrViEExtCodec->RegisterExternalSendCodec(
          channel.videoChannel+5,codecStruct.plType,&extEncoder);
      numberOfErrors += ViETest::TestError(error == -1,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
      numberOfErrors += ViETest::TestError(
          ViE.LastError() == kViECodecInvalidArgument,
          "ERROR: %s at line %d", __FUNCTION__, __LINE__);

      error = ptrViEExtCodec->RegisterExternalSendCodec(
          channel.videoChannel,codecStruct.plType,&extEncoder);
      numberOfErrors += ViETest::TestError(error == 0,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);

      // Use new external encoder
      error = ViE.codec->SetSendCodec(channel.videoChannel,codecStruct);
      numberOfErrors += ViETest::TestError(error == 0,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);

      TbI420Decoder extDecoder;
      error = ptrViEExtCodec->RegisterExternalReceiveCodec(
          channel.videoChannel,codecStruct.plType,&extDecoder);
      numberOfErrors += ViETest::TestError(error == 0,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);

      error = ViE.codec->SetReceiveCodec(channel.videoChannel,
          codecStruct);
      numberOfErrors += ViETest::TestError(error == 0,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);

      ViETest::Log("Using external I420 codec");
      AutoTestSleep(KAutoTestSleepTimeMs);

      // Test to deregister on wrong channel
      error = ptrViEExtCodec->DeRegisterExternalSendCodec(
          channel.videoChannel+5,codecStruct.plType);
      numberOfErrors += ViETest::TestError(error == -1,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
      numberOfErrors += ViETest::TestError(
          ViE.LastError() == kViECodecInvalidArgument,
          "ERROR: %s at line %d", __FUNCTION__, __LINE__);

      // Test to deregister wrong payload type.
      error = ptrViEExtCodec->DeRegisterExternalSendCodec(
          channel.videoChannel,codecStruct.plType-1);
      numberOfErrors += ViETest::TestError(error == -1,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);

      // Deregister external send codec
      error = ptrViEExtCodec->DeRegisterExternalSendCodec(
          channel.videoChannel,codecStruct.plType);
      numberOfErrors += ViETest::TestError(error == 0,
          "ERROR: %s at line %d", __FUNCTION__, __LINE__);

      error = ptrViEExtCodec->DeRegisterExternalReceiveCodec(
          channel.videoChannel,codecStruct.plType);
      numberOfErrors += ViETest::TestError(error == 0,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);

      // Verify that the encoder and decoder has been used
      TbI420Encoder::FunctionCalls encodeCalls =
        extEncoder.GetFunctionCalls();
      numberOfErrors += ViETest::TestError(encodeCalls.InitEncode == 1,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
      numberOfErrors += ViETest::TestError(encodeCalls.Release == 1,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
      numberOfErrors += ViETest::TestError(encodeCalls.Encode > 30,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
      numberOfErrors += ViETest::TestError(
          encodeCalls.RegisterEncodeCompleteCallback ==1,
          "ERROR: %s at line %d", __FUNCTION__, __LINE__);
      numberOfErrors += ViETest::TestError(encodeCalls.SetRates > 1,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
      numberOfErrors += ViETest::TestError(encodeCalls.SetPacketLoss > 1,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);

      TbI420Decoder::FunctionCalls decodeCalls =
        extDecoder.GetFunctionCalls();
      numberOfErrors += ViETest::TestError(decodeCalls.InitDecode == 1,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
      numberOfErrors += ViETest::TestError(decodeCalls.Release == 1,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
      numberOfErrors += ViETest::TestError(decodeCalls.Decode > 30,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
      numberOfErrors += ViETest::TestError(
          decodeCalls.RegisterDecodeCompleteCallback ==1,
          "ERROR: %s at line %d", __FUNCTION__, __LINE__);

      ViETest::Log("Changing payload type Using external I420 codec");

      codecStruct.plType=codecStruct.plType-1;
      error = ptrViEExtCodec->RegisterExternalReceiveCodec(
          channel.videoChannel, codecStruct.plType, &extDecoder);
      numberOfErrors += ViETest::TestError(error == 0,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);

      error = ViE.codec->SetReceiveCodec(channel.videoChannel,
          codecStruct);
      numberOfErrors += ViETest::TestError(error == 0,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);

      error = ptrViEExtCodec->RegisterExternalSendCodec(
          channel.videoChannel, codecStruct.plType, &extEncoder);
      numberOfErrors += ViETest::TestError(error == 0,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);

      // Use new external encoder
      error = ViE.codec->SetSendCodec(channel.videoChannel,
          codecStruct);
      numberOfErrors += ViETest::TestError(error == 0,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);

      AutoTestSleep(KAutoTestSleepTimeMs/2);

      //***************************************************************
      //  Testing finished. Tear down Video Engine
      //***************************************************************

      error = ptrViEExtCodec->DeRegisterExternalSendCodec(
          channel.videoChannel,codecStruct.plType);
      numberOfErrors += ViETest::TestError(error == 0,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
      error = ptrViEExtCodec->DeRegisterExternalReceiveCodec(
          channel.videoChannel,codecStruct.plType);
      numberOfErrors += ViETest::TestError(error == 0,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);

      // Verify that the encoder and decoder has been used
      encodeCalls = extEncoder.GetFunctionCalls();
      numberOfErrors += ViETest::TestError(encodeCalls.InitEncode == 2,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
      numberOfErrors += ViETest::TestError(encodeCalls.Release == 2,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
      numberOfErrors += ViETest::TestError(encodeCalls.Encode > 30,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
      numberOfErrors += ViETest::TestError(
          encodeCalls.RegisterEncodeCompleteCallback == 2,
          "ERROR: %s at line %d", __FUNCTION__, __LINE__);
      numberOfErrors += ViETest::TestError(encodeCalls.SetRates > 1,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
      numberOfErrors += ViETest::TestError(encodeCalls.SetPacketLoss > 1,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);

      decodeCalls = extDecoder.GetFunctionCalls();
      numberOfErrors += ViETest::TestError(decodeCalls.InitDecode == 2,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
      numberOfErrors += ViETest::TestError(decodeCalls.Release == 2,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
      numberOfErrors += ViETest::TestError(decodeCalls.Decode > 30,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
      numberOfErrors += ViETest::TestError(
          decodeCalls.RegisterDecodeCompleteCallback == 2,
          "ERROR: %s at line %d", __FUNCTION__, __LINE__);

      int remainingInterfaces = ptrViEExtCodec->Release();
      numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
          "ERROR: %s at line %d",
          __FUNCTION__, __LINE__);
    } // tbI420Encoder and extDecoder goes out of scope

    ViETest::Log("Using internal I420 codec");
    AutoTestSleep(KAutoTestSleepTimeMs/2);

  }
  if (numberOfErrors > 0)
  {
    // Test failed
    ViETest::Log(" ");
    ViETest::Log(" ERROR ViEExternalCodec Test FAILED!");
    ViETest::Log(" Number of errors: %d", numberOfErrors);
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
