/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vie_autotest.h"

#include "base_primitives.h"
#include "general_primitives.h"
#include "tb_interfaces.h"
#include "vie_autotest_defines.h"
#include "video_capture_factory.h"

class BaseObserver : public webrtc::ViEBaseObserver {
 public:
  BaseObserver()
      : cpu_load_(0) {}

  virtual void PerformanceAlarm(const unsigned int cpu_load) {
    cpu_load_ = cpu_load;
  }
  unsigned int cpu_load_;
};

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

  webrtc::VideoCaptureModule* video_capture_module(NULL);
  const unsigned int kMaxDeviceNameLength = 128;
  char device_name[kMaxDeviceNameLength];
  memset(device_name, 0, kMaxDeviceNameLength);
  int capture_id;

  webrtc::ViEBase *base_interface = interfaces.base;
  webrtc::ViERender *render_interface = interfaces.render;
  webrtc::ViECapture *capture_interface = interfaces.capture;

  FindCaptureDeviceOnSystem(capture_interface,
                            device_name,
                            kMaxDeviceNameLength,
                            &capture_id,
                            &video_capture_module);

  EXPECT_EQ(0, capture_interface->ConnectCaptureDevice(capture_id,
                                                       video_channel));
  EXPECT_EQ(0, capture_interface->StartCapture(capture_id));

  ConfigureRtpRtcp(interfaces.rtp_rtcp, video_channel);

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

  StopAndRemoveRenderers(base_interface, render_interface, video_channel,
                         capture_id);

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

    // ***************************************************************
    // Test BaseObserver
    // ***************************************************************
    // TODO(mflodman) Add test for base observer. Cpu load must be over 75%.
//    BaseObserver base_observer;
//    EXPECT_EQ(ptrViEBase->RegisterObserver(base_observer), 0);
//
//    AutoTestSleep(KAutoTestSleepTimeMs);
//
//    EXPECT_EQ(ptrViEBase->DeregisterObserver(), 0);
//    EXPECT_GT(base_observer.cpu_load, 0);
}

void ViEAutoTest::ViEBaseAPITest() {
  // ***************************************************************
  // Begin create/initialize WebRTC Video Engine for testing
  // ***************************************************************
  // Get the ViEBase API
  webrtc::ViEBase* ptrViEBase = webrtc::ViEBase::GetInterface(NULL);
  EXPECT_EQ(NULL, ptrViEBase) << "Should return null for a bad ViE pointer";

  webrtc::VideoEngine* ptrViE = webrtc::VideoEngine::Create();
  EXPECT_TRUE(NULL != ptrViE);

  std::string trace_file_path =
      ViETest::GetResultOutputPath() + "ViEBaseAPI_trace.txt";
  EXPECT_EQ(0, ptrViE->SetTraceFile(trace_file_path.c_str()));

  ptrViEBase = webrtc::ViEBase::GetInterface(ptrViE);
  EXPECT_TRUE(NULL != ptrViEBase);

  webrtc::ViENetwork* ptrVieNetwork = webrtc::ViENetwork::GetInterface(ptrViE);
  EXPECT_TRUE(ptrVieNetwork != NULL);

  // ***************************************************************
  // Engine ready. Begin testing class
  // ***************************************************************
  char version[1024] = "";
  EXPECT_EQ(0, ptrViEBase->GetVersion(version));
  EXPECT_EQ(0, ptrViEBase->LastError());

  // Create without init
  int videoChannel = -1;
  EXPECT_NE(0, ptrViEBase->CreateChannel(videoChannel)) <<
      "Should fail since Init has not been called yet";
  EXPECT_EQ(0, ptrViEBase->Init());
  EXPECT_EQ(0, ptrViEBase->CreateChannel(videoChannel));

  int videoChannel2 = -1;
  int videoChannel3 = -1;
  EXPECT_EQ(0, ptrViEBase->CreateChannel(videoChannel2));
  EXPECT_NE(videoChannel, videoChannel2) <<
      "Should allocate new number for independent channel";

  EXPECT_EQ(0, ptrViEBase->DeleteChannel(videoChannel2));

  EXPECT_EQ(-1, ptrViEBase->CreateChannel(videoChannel2, videoChannel + 1))
    << "Should fail since neither channel exists (the second must)";

  // Create a receive only channel and a send channel. Verify we can't send on
  // the receive only channel.
  EXPECT_EQ(0, ptrViEBase->CreateReceiveChannel(videoChannel2, videoChannel));
  EXPECT_EQ(0, ptrViEBase->CreateChannel(videoChannel3, videoChannel));

  const char* ipAddress = "127.0.0.1\0";
  const int sendPort = 1234;
  EXPECT_EQ(0, ptrVieNetwork->SetSendDestination(videoChannel, ipAddress,
                                                 sendPort));
  EXPECT_EQ(0, ptrVieNetwork->SetSendDestination(videoChannel2,ipAddress,
                                                 sendPort + 2));
  EXPECT_EQ(0, ptrVieNetwork->SetSendDestination(videoChannel3,ipAddress,
                                                 sendPort + 4));

  EXPECT_EQ(0, ptrViEBase->StartSend(videoChannel));
  EXPECT_EQ(-1, ptrViEBase->StartSend(videoChannel2));
  EXPECT_EQ(0, ptrViEBase->StartSend(videoChannel3));
  EXPECT_EQ(0, ptrViEBase->StopSend(videoChannel));
  EXPECT_EQ(0, ptrViEBase->StopSend(videoChannel3));

  // Test Voice Engine integration with Video Engine.
  webrtc::VoiceEngine* ptrVoE = NULL;
  webrtc::VoEBase* ptrVoEBase = NULL;
  int audioChannel = -1;

  ptrVoE = webrtc::VoiceEngine::Create();
  EXPECT_TRUE(NULL != ptrVoE);

  ptrVoEBase = webrtc::VoEBase::GetInterface(ptrVoE);
  EXPECT_TRUE(NULL != ptrVoEBase);
  EXPECT_EQ(0, ptrVoEBase->Init());

  audioChannel = ptrVoEBase->CreateChannel();
  EXPECT_NE(-1, audioChannel);

  // Connect before setting VoE.
  EXPECT_NE(0, ptrViEBase->ConnectAudioChannel(videoChannel, audioChannel)) <<
      "Should fail since Voice Engine is not set yet.";

  // Then do it right.
  EXPECT_EQ(0, ptrViEBase->SetVoiceEngine(ptrVoE));
  EXPECT_EQ(0, ptrViEBase->ConnectAudioChannel(videoChannel, audioChannel));

  // ***************************************************************
  // Testing finished. Tear down Video Engine
  // ***************************************************************
  EXPECT_NE(0, ptrViEBase->DisconnectAudioChannel(videoChannel + 5)) <<
      "Should fail: disconnecting bogus channel";

  EXPECT_EQ(0, ptrViEBase->DisconnectAudioChannel(videoChannel));

  // Clean up voice engine
  EXPECT_EQ(0, ptrVieNetwork->Release());
  EXPECT_EQ(0, ptrViEBase->SetVoiceEngine(NULL));
  EXPECT_EQ(0, ptrVoEBase->Release());
  EXPECT_TRUE(webrtc::VoiceEngine::Delete(ptrVoE));

  webrtc::ViEBase* ptrViEBase2 = webrtc::ViEBase::GetInterface(ptrViE);
  EXPECT_TRUE(NULL != ptrViEBase2);

  EXPECT_EQ(1, ptrViEBase->Release()) << "There should be one interface left.";

  EXPECT_FALSE(webrtc::VideoEngine::Delete(ptrViE)) <<
      "Should fail since there are interfaces left.";

  EXPECT_EQ(0, ptrViEBase->Release());
  EXPECT_TRUE(webrtc::VideoEngine::Delete(ptrViE));
}
