/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>  // NOLINT

#include "common_types.h"  // NOLINT
#include "video_engine/include/vie_base.h"
#include "video_engine/include/vie_capture.h"
#include "video_engine/include/vie_codec.h"
#include "video_engine/include/vie_network.h"
#include "video_engine/include/vie_render.h"
#include "video_engine/include/vie_rtp_rtcp.h"
#include "video_engine/test/auto_test/interface/vie_autotest.h"
#include "video_engine/test/auto_test/interface/vie_autotest_defines.h"
#include "video_engine/test/libvietest/include/tb_external_transport.h"
#include "voice_engine/include/voe_base.h"

enum RelayMode {
  kRelayOneStream = 1,
  kRelayAllStreams = 2
};

#define VCM_RED_PAYLOAD_TYPE        96
#define VCM_ULPFEC_PAYLOAD_TYPE     97

const int kNumStreams = 3;

void InitialSingleStreamSettings(webrtc::VideoCodec* video_codec) {
  video_codec->numberOfSimulcastStreams = 0;
  video_codec->width = 1200;
  video_codec->height = 800;
}

void SetSimulcastSettings(webrtc::VideoCodec* video_codec) {
  video_codec->width = 1280;
  video_codec->height = 720;

  // Simulcast settings.
  video_codec->numberOfSimulcastStreams = kNumStreams;
  video_codec->simulcastStream[0].width = 320;
  video_codec->simulcastStream[0].height = 180;
  video_codec->simulcastStream[0].numberOfTemporalLayers = 0;
  video_codec->simulcastStream[0].maxBitrate = 100;
  video_codec->simulcastStream[0].qpMax = video_codec->qpMax;

  video_codec->simulcastStream[1].width = 640;
  video_codec->simulcastStream[1].height = 360;
  video_codec->simulcastStream[1].numberOfTemporalLayers = 0;
  video_codec->simulcastStream[1].maxBitrate = 500;
  video_codec->simulcastStream[1].qpMax = video_codec->qpMax;

  video_codec->simulcastStream[2].width = 1280;
  video_codec->simulcastStream[2].height = 720;
  video_codec->simulcastStream[2].numberOfTemporalLayers = 0;
  video_codec->simulcastStream[2].maxBitrate = 1200;
  video_codec->simulcastStream[2].qpMax = video_codec->qpMax;
}

void RuntimeSingleStreamSettings(webrtc::VideoCodec* video_codec) {
  SetSimulcastSettings(video_codec);
  video_codec->width = 1200;
  video_codec->height = 800;
  video_codec->numberOfSimulcastStreams = kNumStreams;
  video_codec->simulcastStream[0].maxBitrate = 0;
  video_codec->simulcastStream[1].maxBitrate = 0;
  video_codec->simulcastStream[2].maxBitrate = 0;
}

int VideoEngineSimulcastTest(void* window1, void* window2) {
  // *******************************************************
  //  Begin create/initialize Video Engine for testing
  // *******************************************************

  int error = 0;
  int receive_channels[kNumStreams];

  // Create a VideoEngine instance.
  webrtc::VideoEngine* video_engine = NULL;
  video_engine = webrtc::VideoEngine::Create();
  if (video_engine == NULL) {
    printf("ERROR in VideoEngine::Create\n");
    return -1;
  }

  error = video_engine->SetTraceFilter(webrtc::kTraceAll);
  if (error == -1) {
    printf("ERROR in VideoEngine::SetTraceLevel\n");
    return -1;
  }

  std::string trace_file =
    ViETest::GetResultOutputPath() + "ViESimulcast_trace.txt";
  error = video_engine->SetTraceFile(trace_file.c_str());
  if (error == -1) {
    printf("ERROR in VideoEngine::SetTraceFile\n");
    return -1;
  }

  // Init VideoEngine and create a channel.
  webrtc::ViEBase* vie_base = webrtc::ViEBase::GetInterface(video_engine);
  if (vie_base == NULL) {
    printf("ERROR in ViEBase::GetInterface\n");
    return -1;
  }

  error = vie_base->Init();
  if (error == -1) {
    printf("ERROR in ViEBase::Init\n");
    return -1;
  }

  RelayMode relay_mode = kRelayOneStream;
  printf("Select relay mode:\n");
  printf("\t1. Relay one stream\n");
  printf("\t2. Relay all streams\n");
  if (scanf("%d", reinterpret_cast<int*>(&relay_mode)) != 1) {
    printf("Error in scanf()\n");
    return -1;
  }
  getchar();

  webrtc::ViERTP_RTCP* vie_rtp_rtcp =
      webrtc::ViERTP_RTCP::GetInterface(video_engine);
  if (vie_rtp_rtcp == NULL) {
    printf("ERROR in ViERTP_RTCP::GetInterface\n");
    return -1;
  }

  printf("Bandwidth estimation modes:\n");
  printf("1. Multi-stream bandwidth estimation\n");
  printf("2. Single-stream bandwidth estimation\n");
  printf("Choose bandwidth estimation mode (default is 1): ");
  std::string str;
  std::getline(std::cin, str);
  int bwe_mode_choice = atoi(str.c_str());
  webrtc::BandwidthEstimationMode bwe_mode;
  switch (bwe_mode_choice) {
    case 1:
      bwe_mode = webrtc::kViEMultiStreamEstimation;
      break;
    case 2:
      bwe_mode = webrtc::kViESingleStreamEstimation;
      break;
    default:
      bwe_mode = webrtc::kViEMultiStreamEstimation;
      break;
  }

  error = vie_rtp_rtcp->SetBandwidthEstimationMode(bwe_mode);
  if (error == -1) {
    printf("ERROR in ViERTP_RTCP::SetBandwidthEstimationMode\n");
    return -1;
  }

  int video_channel = -1;
  error = vie_base->CreateChannel(video_channel);
  if (error == -1) {
    printf("ERROR in ViEBase::CreateChannel\n");
    return -1;
  }

  for (int i = 0; i < kNumStreams; ++i) {
    receive_channels[i] = -1;
    error = vie_base->CreateReceiveChannel(receive_channels[i], video_channel);
    if (error == -1) {
      printf("ERROR in ViEBase::CreateChannel\n");
      return -1;
    }
  }

  // List available capture devices, allocate and connect.
  webrtc::ViECapture* vie_capture =
      webrtc::ViECapture::GetInterface(video_engine);
  if (vie_base == NULL) {
    printf("ERROR in ViECapture::GetInterface\n");
    return -1;
  }

  const unsigned int KMaxDeviceNameLength = 128;
  const unsigned int KMaxUniqueIdLength = 256;
  char device_name[KMaxDeviceNameLength];
  memset(device_name, 0, KMaxDeviceNameLength);
  char unique_id[KMaxUniqueIdLength];
  memset(unique_id, 0, KMaxUniqueIdLength);

  printf("Available capture devices:\n");
  int capture_idx = 0;
  for (capture_idx = 0; capture_idx < vie_capture->NumberOfCaptureDevices();
       capture_idx++) {
    memset(device_name, 0, KMaxDeviceNameLength);
    memset(unique_id, 0, KMaxUniqueIdLength);

    error = vie_capture->GetCaptureDevice(capture_idx, device_name,
                                          KMaxDeviceNameLength, unique_id,
                                          KMaxUniqueIdLength);
    if (error == -1) {
      printf("ERROR in ViECapture::GetCaptureDevice\n");
      return -1;
    }
    printf("\t %d. %s\n", capture_idx + 1, device_name);
  }
  printf("\nChoose capture device: ");
#ifdef WEBRTC_ANDROID
  capture_idx = 0;
  printf("0\n");
#else
  if (scanf("%d", &capture_idx) != 1) {
    printf("Error in scanf()\n");
    return -1;
  }
  getchar();
  // Compensate for idx start at 1.
  capture_idx = capture_idx - 1;
#endif
  error = vie_capture->GetCaptureDevice(capture_idx, device_name,
                                        KMaxDeviceNameLength, unique_id,
                                        KMaxUniqueIdLength);
  if (error == -1) {
    printf("ERROR in ViECapture::GetCaptureDevice\n");
    return -1;
  }

  int capture_id = 0;
  error = vie_capture->AllocateCaptureDevice(unique_id, KMaxUniqueIdLength,
                                             capture_id);
  if (error == -1) {
    printf("ERROR in ViECapture::AllocateCaptureDevice\n");
    return -1;
  }

  error = vie_capture->ConnectCaptureDevice(capture_id, video_channel);
  if (error == -1) {
    printf("ERROR in ViECapture::ConnectCaptureDevice\n");
    return -1;
  }

  error = vie_capture->StartCapture(capture_id);
  if (error == -1) {
    printf("ERROR in ViECapture::StartCapture\n");
    return -1;
  }

  // RTP/RTCP settings.
  error = vie_rtp_rtcp->SetRTCPStatus(video_channel,
                                      webrtc::kRtcpCompound_RFC4585);
  if (error == -1) {
    printf("ERROR in ViERTP_RTCP::SetRTCPStatus\n");
    return -1;
  }

  vie_rtp_rtcp->SetRembStatus(video_channel, true, false);
  if (error == -1) {
    printf("ERROR in ViERTP_RTCP::SetRTCPStatus\n");
    return -1;
  }

  error = vie_rtp_rtcp->SetKeyFrameRequestMethod(
            video_channel, webrtc::kViEKeyFrameRequestPliRtcp);
  if (error == -1) {
    printf("ERROR in ViERTP_RTCP::SetKeyFrameRequestMethod\n");
    return -1;
  }

  for (int i = 0; i < kNumStreams; ++i) {
    error = vie_rtp_rtcp->SetRTCPStatus(receive_channels[i],
                                        webrtc::kRtcpCompound_RFC4585);
    if (error == -1) {
      printf("ERROR in ViERTP_RTCP::SetRTCPStatus\n");
      return -1;
    }

    vie_rtp_rtcp->SetRembStatus(receive_channels[i], false, true);
    if (error == -1) {
      printf("ERROR in ViERTP_RTCP::SetRTCPStatus\n");
      return -1;
    }

    error = vie_rtp_rtcp->SetKeyFrameRequestMethod(
        receive_channels[i], webrtc::kViEKeyFrameRequestPliRtcp);
    if (error == -1) {
      printf("ERROR in ViERTP_RTCP::SetKeyFrameRequestMethod\n");
      return -1;
    }
  }

  // Set up rendering.
  webrtc::ViERender* vie_render = webrtc::ViERender::GetInterface(video_engine);
  if (vie_render == NULL) {
    printf("ERROR in ViERender::GetInterface\n");
    return -1;
  }

  error = vie_render->AddRenderer(capture_id, window1, 0, 0.0, 0.0, 1.0, 1.0);
  if (error == -1) {
    printf("ERROR in ViERender::AddRenderer\n");
    return -1;
  }

  error = vie_render->StartRender(capture_id);
  if (error == -1) {
    printf("ERROR in ViERender::StartRender\n");
    return -1;
  }

  // Only rendering the thumbnail.
  int channel_to_render = video_channel;
  if (relay_mode == kRelayAllStreams) {
    channel_to_render = receive_channels[0];
  }
  error = vie_render->AddRenderer(channel_to_render, window2, 1, 0.0, 0.0, 1.0,
                                  1.0);
  if (error == -1) {
    printf("ERROR in ViERender::AddRenderer\n");
    return -1;
  }

  error = vie_render->StartRender(channel_to_render);
  if (error == -1) {
    printf("ERROR in ViERender::StartRender\n");
    return -1;
  }

  // Setup codecs.
  webrtc::ViECodec* vie_codec = webrtc::ViECodec::GetInterface(video_engine);
  if (vie_codec == NULL) {
    printf("ERROR in ViECodec::GetInterface\n");
    return -1;
  }

  // Check available codecs and prepare receive codecs.
  printf("\nAvailable codecs:\n");
  webrtc::VideoCodec video_codec;
  memset(&video_codec, 0, sizeof(webrtc::VideoCodec));
  int codec_idx = 0;
  for (codec_idx = 0; codec_idx < vie_codec->NumberOfCodecs(); codec_idx++) {
    error = vie_codec->GetCodec(codec_idx, video_codec);
    if (error == -1) {
      printf("ERROR in ViECodec::GetCodec\n");
      return -1;
    }
    // Try to keep the test frame size small when I420.
    if (video_codec.codecType != webrtc::kVideoCodecVP8) {
      continue;
    }
    for (int i = 0; i < kNumStreams; ++i) {
      error = vie_codec->SetReceiveCodec(receive_channels[i], video_codec);
      if (error == -1) {
        printf("ERROR in ViECodec::SetReceiveCodec\n");
        return -1;
      }
    }
    if (video_codec.codecType != webrtc::kVideoCodecRED &&
        video_codec.codecType != webrtc::kVideoCodecULPFEC) {
      printf("\t %d. %s\n", codec_idx + 1, video_codec.plName);
    }
    break;
  }
  error = vie_codec->GetCodec(codec_idx, video_codec);
  if (error == -1) {
    printf("ERROR in ViECodec::GetCodec\n");
    return -1;
  }

  bool simulcast_mode = true;
  int num_streams = 1;
  // Set spatial resolution option.
  if (simulcast_mode) {
    SetSimulcastSettings(&video_codec);
    num_streams = video_codec.numberOfSimulcastStreams;
  } else {
    InitialSingleStreamSettings(&video_codec);
    num_streams = 1;
  }

  // Set start bit rate.
  std::cout << std::endl;
  std::cout << "Choose start rate (in kbps). Press enter for default:  ";
  std::getline(std::cin, str);
  int start_rate = atoi(str.c_str());
  if (start_rate != 0) {
    video_codec.startBitrate = start_rate;
  }

  error = vie_codec->SetSendCodec(video_channel, video_codec);
  if (error == -1) {
    printf("ERROR in ViECodec::SetSendCodec\n");
    return -1;
  }

  // Address settings.
  webrtc::ViENetwork* vie_network =
      webrtc::ViENetwork::GetInterface(video_engine);
  if (vie_network == NULL) {
    printf("ERROR in ViENetwork::GetInterface\n");
    return -1;
  }

  TbExternalTransport::SsrcChannelMap ssrc_channel_map;
  for (int idx = 0; idx < num_streams; idx++) {
    error = vie_rtp_rtcp->SetLocalSSRC(video_channel, idx + 1,  // SSRC
                                       webrtc::kViEStreamTypeNormal, idx);
    ssrc_channel_map[idx + 1] = receive_channels[idx];
    if (error == -1) {
      printf("ERROR in ViERTP_RTCP::SetLocalSSRC(idx:%d)\n",
             idx);
      return -1;
    }
  }

  TbExternalTransport::SsrcChannelMap* channel_map = &ssrc_channel_map;
  if (relay_mode == kRelayOneStream) {
    channel_map = NULL;
  }

  // Setting External transport.
  TbExternalTransport ext_transport(*vie_network, video_channel, channel_map);

  error = vie_network->RegisterSendTransport(video_channel, ext_transport);
  if (error == -1) {
    printf("ERROR in ViECodec::RegisterSendTransport \n");
    return -1;
  }

  for (int i = 0; i < kNumStreams; ++i) {
    error = vie_network->RegisterSendTransport(receive_channels[i],
                                               ext_transport);
    if (error == -1) {
      printf("ERROR in ViECodec::RegisterSendTransport \n");
      return -1;
    }
  }

  // Set network one-way delay value.
  // 10 ms one-way delay.
  NetworkParameters network;
  network.loss_model = kUniformLoss;
  network.mean_one_way_delay = 10;
  ext_transport.SetNetworkParameters(network);

  if (relay_mode == kRelayOneStream) {
    ext_transport.SetSSRCFilter(num_streams);
  }

  error = vie_base->StartSend(video_channel);
  if (error == -1) {
    printf("ERROR in ViENetwork::StartSend\n");
    return -1;
  }
  error = vie_base->StartReceive(video_channel);
  if (error == -1) {
    printf("ERROR in ViENetwork::StartReceive\n");
    return -1;
  }

  for (int i = 0; i < kNumStreams; ++i) {
    error = vie_base->StartReceive(receive_channels[i]);
    if (error == -1) {
      printf("ERROR in ViENetwork::StartReceive\n");
      return -1;
    }
  }

  // Create a receive channel to verify that it doesn't mess up toggling
  // between single stream and simulcast.
  int video_channel2 = -1;
  error = vie_base->CreateReceiveChannel(video_channel2, video_channel);
  if (error == -1) {
    printf("ERROR in ViEBase::CreateReceiveChannel\n");
    return -1;
  }

  // *******************************************************
  //  Engine started
  // *******************************************************

  printf("\nSimulcast call started\n\n");
  do {
    printf("Enter new SSRC filter 1,2 or 3\n");
    printf("... or 0 to switch between simulcast and a single stream\n");
    printf("Press enter to stop...");
    str.clear();
    std::getline(std::cin, str);
    if (!str.empty()) {
      int ssrc = atoi(str.c_str());
      if (ssrc == 0) {
        // Toggle between simulcast and a single stream with different
        // resolution.
        if (simulcast_mode) {
          RuntimeSingleStreamSettings(&video_codec);
          num_streams = 1;
          printf("Disabling simulcast\n");
        } else {
          SetSimulcastSettings(&video_codec);
          num_streams = video_codec.numberOfSimulcastStreams;
          printf("Enabling simulcast\n");
        }
        simulcast_mode = !simulcast_mode;
        if (vie_codec->SetSendCodec(video_channel, video_codec) != 0) {
          printf("ERROR switching between simulcast and single stream\n");
          return -1;
        }
        for (int idx = 0; idx < num_streams; idx++) {
          error = vie_rtp_rtcp->SetLocalSSRC(video_channel, idx + 1,  // SSRC
                                             webrtc::kViEStreamTypeNormal, idx);
          if (error == -1) {
            printf("ERROR in ViERTP_RTCP::SetLocalSSRC(idx:%d)\n", idx);
            return -1;
          }
        }
        if (relay_mode == kRelayOneStream) {
          ext_transport.SetSSRCFilter(num_streams);
        }
      } else if (ssrc > 0 && ssrc < 4) {
        if (relay_mode == kRelayOneStream) {
          ext_transport.SetSSRCFilter(ssrc);
        }
      } else {
        printf("Invalid SSRC\n");
      }
    } else {
      break;
    }
  } while (true);

  // *******************************************************
  //  Testing finished. Tear down Video Engine
  // *******************************************************
  error = vie_base->DeleteChannel(video_channel2);
  if (error == -1) {
    printf("ERROR in ViEBase::DeleteChannel\n");
    return -1;
  }

  for (int i = 0; i < kNumStreams; ++i) {
    error = vie_base->StopReceive(receive_channels[i]);
    if (error == -1) {
      printf("ERROR in ViEBase::StopReceive\n");
      return -1;
    }
  }

  error = vie_base->StopReceive(video_channel);
  if (error == -1) {
    printf("ERROR in ViEBase::StopReceive\n");
    return -1;
  }

  error = vie_base->StopSend(video_channel);
  if (error == -1) {
    printf("ERROR in ViEBase::StopSend\n");
    return -1;
  }

  error = vie_render->StopRender(capture_id);
  if (error == -1) {
    printf("ERROR in ViERender::StopRender\n");
    return -1;
  }

  error = vie_render->RemoveRenderer(capture_id);
  if (error == -1) {
    printf("ERROR in ViERender::RemoveRenderer\n");
    return -1;
  }

  error = vie_render->StopRender(channel_to_render);
  if (error == -1) {
    printf("ERROR in ViERender::StopRender\n");
    return -1;
  }

  error = vie_render->RemoveRenderer(channel_to_render);
  if (error == -1) {
    printf("ERROR in ViERender::RemoveRenderer\n");
    return -1;
  }

  error = vie_capture->StopCapture(capture_id);
  if (error == -1) {
    printf("ERROR in ViECapture::StopCapture\n");
    return -1;
  }

  error = vie_capture->DisconnectCaptureDevice(video_channel);
  if (error == -1) {
    printf("ERROR in ViECapture::DisconnectCaptureDevice\n");
    return -1;
  }

  error = vie_capture->ReleaseCaptureDevice(capture_id);
  if (error == -1) {
    printf("ERROR in ViECapture::ReleaseCaptureDevice\n");
    return -1;
  }

  for (int i = 0; i < kNumStreams; ++i) {
    error = vie_base->DeleteChannel(receive_channels[i]);
    if (error == -1) {
      printf("ERROR in ViEBase::DeleteChannel\n");
      return -1;
    }
  }

  error = vie_base->DeleteChannel(video_channel);
  if (error == -1) {
    printf("ERROR in ViEBase::DeleteChannel\n");
    return -1;
  }

  int remaining_interfaces = 0;
  remaining_interfaces = vie_codec->Release();
  remaining_interfaces += vie_capture->Release();
  remaining_interfaces += vie_rtp_rtcp->Release();
  remaining_interfaces += vie_render->Release();
  remaining_interfaces += vie_network->Release();
  remaining_interfaces += vie_base->Release();
  if (remaining_interfaces > 0) {
    printf("ERROR: Could not release all interfaces\n");
    return -1;
  }

  bool deleted = webrtc::VideoEngine::Delete(video_engine);
  if (deleted == false) {
    printf("ERROR in VideoEngine::Delete\n");
    return -1;
  }
  return 0;
}

int ViEAutoTest::ViESimulcastCall() {
  ViETest::Log(" ");
  ViETest::Log("========================================");
  ViETest::Log(" ViE Autotest Simulcast Call\n");

  if (VideoEngineSimulcastTest(_window1, _window2) == 0) {
    ViETest::Log(" ");
    ViETest::Log(" ViE Autotest Simulcast Call Done");
    ViETest::Log("========================================");
    ViETest::Log(" ");

    return 0;
  }
  ViETest::Log(" ");
  ViETest::Log(" ViE Autotest Simulcast Call Failed");
  ViETest::Log("========================================");
  ViETest::Log(" ");
  return 1;
}
