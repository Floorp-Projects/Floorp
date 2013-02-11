/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_capture/include/video_capture_factory.h"
#include "video_engine/test/auto_test/interface/vie_autotest.h"
#include "video_engine/test/auto_test/interface/vie_autotest_defines.h"
#include "video_engine/test/auto_test/primitives/base_primitives.h"
#include "video_engine/test/auto_test/primitives/general_primitives.h"
#include "video_engine/test/libvietest/include/tb_interfaces.h"

void ViEAutoTest::ViEBaseStandardTest() {
  // ***************************************************************
  // Begin create/initialize WebRTC Video Engine for testing
  // ***************************************************************

  TbInterfaces interfaces("ViEBaseStandardTest");

  // ***************************************************************
  // Engine ready. Set up the test case:
  // ***************************************************************
  int video_channel = -1;
  EXPECT_EQ(0, interfaces.base->CreateChannel(video_channel));

  webrtc::VideoCaptureModule* video_capture_module = NULL;
  const unsigned int kMaxDeviceNameLength = 128;
  char device_name[kMaxDeviceNameLength];
  memset(device_name, 0, kMaxDeviceNameLength);
  int capture_id;

  webrtc::ViEBase* base_interface = interfaces.base;
  webrtc::ViERender* render_interface = interfaces.render;
  webrtc::ViECapture* capture_interface = interfaces.capture;

  FindCaptureDeviceOnSystem(capture_interface,
                            device_name,
                            kMaxDeviceNameLength,
                            &capture_id,
                            &video_capture_module);

  EXPECT_TRUE(video_capture_module);
  if (!video_capture_module)
    return;

  EXPECT_EQ(0, capture_interface->ConnectCaptureDevice(capture_id,
                                                       video_channel));
  EXPECT_EQ(0, capture_interface->StartCapture(capture_id));

  ConfigureRtpRtcp(interfaces.rtp_rtcp, kNack, video_channel);

  EXPECT_EQ(0, render_interface->RegisterVideoRenderModule(*_vrm1));
  EXPECT_EQ(0, render_interface->RegisterVideoRenderModule(*_vrm2));

  RenderInWindow(render_interface, capture_id, _window1, 0);
  RenderInWindow(render_interface, video_channel, _window2, 1);

  // ***************************************************************
  // Run the actual test:
  // ***************************************************************
  ViETest::Log("You should shortly see a local preview from camera %s"
               " in window 1 and the remote video in window 2.", device_name);
  ::TestI420CallSetup(interfaces.codec, interfaces.video_engine,
                      base_interface, interfaces.network, video_channel,
                      device_name);

  // ***************************************************************
  // Testing finished. Tear down Video Engine
  // ***************************************************************
  EXPECT_EQ(0, capture_interface->StopCapture(capture_id));
  EXPECT_EQ(0, base_interface->StopReceive(video_channel));

  EXPECT_EQ(0, render_interface->StopRender(video_channel));
  EXPECT_EQ(0, render_interface->RemoveRenderer(video_channel));
  EXPECT_EQ(0, render_interface->RemoveRenderer(capture_id));

  EXPECT_EQ(0, render_interface->DeRegisterVideoRenderModule(*_vrm1));
  EXPECT_EQ(0, render_interface->DeRegisterVideoRenderModule(*_vrm2));

  EXPECT_EQ(0, capture_interface->ReleaseCaptureDevice(capture_id));

  video_capture_module->Release();
  video_capture_module = NULL;

  EXPECT_EQ(0, base_interface->DeleteChannel(video_channel));
}

void ViEAutoTest::ViEBaseExtendedTest() {
  // Start with standard test
  ViEBaseAPITest();
  ViEBaseStandardTest();
}

void ViEAutoTest::ViEBaseAPITest() {
  // ***************************************************************
  // Begin create/initialize WebRTC Video Engine for testing
  // ***************************************************************
  // Get the ViEBase API
  webrtc::ViEBase* vie_base = webrtc::ViEBase::GetInterface(NULL);
  EXPECT_EQ(NULL, vie_base) << "Should return null for a bad ViE pointer";

  webrtc::VideoEngine* video_engine = webrtc::VideoEngine::Create();
  EXPECT_TRUE(NULL != video_engine);

  std::string trace_file_path =
    ViETest::GetResultOutputPath() + "ViEBaseAPI_trace.txt";
  EXPECT_EQ(0, video_engine->SetTraceFile(trace_file_path.c_str()));

  vie_base = webrtc::ViEBase::GetInterface(video_engine);
  EXPECT_TRUE(NULL != vie_base);

  webrtc::ViENetwork* vie_network =
      webrtc::ViENetwork::GetInterface(video_engine);
  EXPECT_TRUE(vie_network != NULL);

  webrtc::ViERTP_RTCP* vie_rtp =
      webrtc::ViERTP_RTCP::GetInterface(video_engine);
  EXPECT_TRUE(vie_rtp != NULL);

  // ***************************************************************
  // Engine ready. Begin testing class
  // ***************************************************************
  char version[1024] = "";
  EXPECT_EQ(0, vie_base->GetVersion(version));
  EXPECT_EQ(0, vie_base->LastError());

  // Create without init
  int video_channel = -1;
  EXPECT_NE(0, vie_base->CreateChannel(video_channel)) <<
      "Should fail since Init has not been called yet";
  EXPECT_EQ(0, vie_base->Init());
  EXPECT_EQ(0, vie_base->CreateChannel(video_channel));

  int video_channel2 = -1;
  int video_channel3 = -1;
  EXPECT_EQ(0, vie_base->CreateChannel(video_channel2));
  EXPECT_NE(video_channel, video_channel2) <<
      "Should allocate new number for independent channel";

  EXPECT_EQ(0, vie_base->DeleteChannel(video_channel2));

  EXPECT_EQ(-1, vie_base->CreateChannel(video_channel2, video_channel + 1))
      << "Should fail since neither channel exists (the second must)";

  // Create a receive only channel and a send channel. Verify we can't send on
  // the receive only channel.
  EXPECT_EQ(0, vie_base->CreateReceiveChannel(video_channel2,
                                                  video_channel));
  EXPECT_EQ(0, vie_base->CreateChannel(video_channel3, video_channel));

  const char* ip_address = "127.0.0.1\0";
  const int send_port = 1234;
  EXPECT_EQ(0, vie_rtp->SetLocalSSRC(video_channel, 1));
  EXPECT_EQ(0, vie_network->SetSendDestination(video_channel, ip_address,
                                               send_port));
  EXPECT_EQ(0, vie_rtp->SetLocalSSRC(video_channel, 2));
  EXPECT_EQ(0, vie_network->SetSendDestination(video_channel2, ip_address,
                                               send_port + 2));
  EXPECT_EQ(0, vie_rtp->SetLocalSSRC(video_channel, 3));
  EXPECT_EQ(0, vie_network->SetSendDestination(video_channel3, ip_address,
                                               send_port + 4));

  EXPECT_EQ(0, vie_base->StartSend(video_channel));
  EXPECT_EQ(-1, vie_base->StartSend(video_channel2));
  EXPECT_EQ(0, vie_base->StartSend(video_channel3));
  EXPECT_EQ(0, vie_base->StopSend(video_channel));
  EXPECT_EQ(0, vie_base->StopSend(video_channel3));

  // Test Voice Engine integration with Video Engine.
  webrtc::VoiceEngine* voice_engine = NULL;
  webrtc::VoEBase* voe_base = NULL;
  int audio_channel = -1;

  voice_engine = webrtc::VoiceEngine::Create();
  EXPECT_TRUE(NULL != voice_engine);

  voe_base = webrtc::VoEBase::GetInterface(voice_engine);
  EXPECT_TRUE(NULL != voe_base);
  EXPECT_EQ(0, voe_base->Init());

  audio_channel = voe_base->CreateChannel();
  EXPECT_NE(-1, audio_channel);

  // Connect before setting VoE.
  EXPECT_NE(0, vie_base->ConnectAudioChannel(video_channel, audio_channel))
      << "Should fail since Voice Engine is not set yet.";

  // Then do it right.
  EXPECT_EQ(0, vie_base->SetVoiceEngine(voice_engine));
  EXPECT_EQ(0, vie_base->ConnectAudioChannel(video_channel, audio_channel));

  // ***************************************************************
  // Testing finished. Tear down Video Engine
  // ***************************************************************
  EXPECT_NE(0, vie_base->DisconnectAudioChannel(video_channel + 5)) <<
      "Should fail: disconnecting bogus channel";

  EXPECT_EQ(0, vie_base->DisconnectAudioChannel(video_channel));

  // Clean up voice engine
  EXPECT_EQ(0, vie_rtp->Release());
  EXPECT_EQ(0, vie_network->Release());
  EXPECT_EQ(0, vie_base->SetVoiceEngine(NULL));
  // VoiceEngine reference counting is per object, not per interface, so
  // Release should return != 0.
  EXPECT_NE(0, voe_base->Release());
  EXPECT_TRUE(webrtc::VoiceEngine::Delete(voice_engine));

  webrtc::ViEBase* vie_base2 = webrtc::ViEBase::GetInterface(video_engine);
  EXPECT_TRUE(NULL != vie_base2);

  EXPECT_EQ(1, vie_base->Release()) <<
      "There should be one interface left.";

  EXPECT_FALSE(webrtc::VideoEngine::Delete(video_engine)) <<
      "Should fail since there are interfaces left.";

  EXPECT_EQ(0, vie_base->Release());
  EXPECT_TRUE(webrtc::VideoEngine::Delete(video_engine));
}
