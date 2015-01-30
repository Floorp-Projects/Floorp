/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
// vie_autotest_record.cc
//
// This code is also used as sample code for ViE 3.0
//

#include <fstream>
#include <stdio.h>

#include "webrtc/common_types.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/test/channel_transport/include/channel_transport.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_capture.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_network.h"
#include "webrtc/video_engine/include/vie_render.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest_defines.h"
#include "webrtc/video_engine/test/libvietest/include/tb_external_transport.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/include/voe_rtp_rtcp.h"

#define VCM_RED_PAYLOAD_TYPE            96
#define VCM_ULPFEC_PAYLOAD_TYPE         97
#define DEFAULT_AUDIO_PORT              11113
#define DEFAULT_AUDIO_CODEC             "ISAC"
#define DEFAULT_VIDEO_CODEC_WIDTH       640
#define DEFAULT_VIDEO_CODEC_HEIGHT      480
#define DEFAULT_VIDEO_CODEC_START_RATE  1000
#define DEFAULT_RECORDING_FOLDER        "RECORDING"
#define DEFAULT_RECORDING_AUDIO         "/audio_debug.aec"
#define DEFAULT_RECORDING_VIDEO         "/video_debug.yuv"
#define DEFAULT_RECORDING_AUDIO_RTP     "/audio_rtpdump.rtp"
#define DEFAULT_RECORDING_VIDEO_RTP     "/video_rtpdump.rtp"

bool GetAudioDevices(webrtc::VoEBase* voe_base,
                     webrtc::VoEHardware* voe_hardware,
                     char* recording_device_name,
                     int& recording_device_index,
                     char* playbackDeviceName,
                     int& playback_device_index);
bool GetAudioCodecRecord(webrtc::VoECodec* voe_codec,
                         webrtc::CodecInst& audio_codec);

int VideoEngineSampleRecordCode(void* window1, void* window2) {
  int error = 0;
  // Audio settings.
  int audio_tx_port = DEFAULT_AUDIO_PORT;
  int audio_rx_port = DEFAULT_AUDIO_PORT;
  webrtc::CodecInst audio_codec;
  int audio_channel = -1;
  int audio_capture_device_index = -1;
  int audio_playback_device_index = -1;
  const unsigned int KMaxDeviceNameLength = 128;
  const unsigned int KMaxUniqueIdLength = 256;
  char deviceName[KMaxDeviceNameLength];
  char audio_capture_device_name[KMaxUniqueIdLength] = "";
  char audio_playbackDeviceName[KMaxUniqueIdLength] = "";

  // Network settings.
  const char* ipAddress = "127.0.0.1";
  const int rtpPort = 6000;

  //
  // Create a VideoEngine instance
  //
  webrtc::VideoEngine* ptrViE = NULL;
  ptrViE = webrtc::VideoEngine::Create();
  if (ptrViE == NULL) {
    printf("ERROR in VideoEngine::Create\n");
    return -1;
  }

  error = ptrViE->SetTraceFilter(webrtc::kTraceAll);
  if (error == -1) {
    printf("ERROR in VideoEngine::SetTraceLevel\n");
    return -1;
  }

  std::string trace_file =
    ViETest::GetResultOutputPath() + "ViERecordCall_trace.txt";
  error = ptrViE->SetTraceFile(trace_file.c_str());
  if (error == -1) {
    printf("ERROR in VideoEngine::SetTraceFile\n");
    return -1;
  }

  //
  // Create a VoE instance
  //
  webrtc::VoiceEngine* voe = webrtc::VoiceEngine::Create();
  //
  // Init VideoEngine and create a channel
  //
  webrtc::ViEBase* ptrViEBase = webrtc::ViEBase::GetInterface(ptrViE);
  if (ptrViEBase == NULL) {
    printf("ERROR in ViEBase::GetInterface\n");
    return -1;
  }

  error = ptrViEBase->Init();
  if (error == -1) {
    printf("ERROR in ViEBase::Init\n");
    return -1;
  }

  webrtc::VoEBase* voe_base = webrtc::VoEBase::GetInterface(voe);
  if (voe_base == NULL) {
    printf("ERROR in VoEBase::GetInterface\n");
    return -1;
  }
  error = voe_base->Init();
  if (error == -1) {
    printf("ERROR in VoEBase::Init\n");
    return -1;
  }

  int videoChannel = -1;
  error = ptrViEBase->CreateChannel(videoChannel);
  if (error == -1) {
    printf("ERROR in ViEBase::CreateChannel\n");
    return -1;
  }

  webrtc::VoEHardware* voe_hardware =
    webrtc::VoEHardware::GetInterface(voe);
  webrtc::VoECodec* voe_codec = webrtc::VoECodec::GetInterface(voe);
  webrtc::VoEAudioProcessing* voe_apm =
       webrtc::VoEAudioProcessing::GetInterface(voe);
  webrtc::VoENetwork* voe_network =
    webrtc::VoENetwork::GetInterface(voe);

  // Get the audio device for the call.
  memset(audio_capture_device_name, 0, KMaxUniqueIdLength);
  memset(audio_playbackDeviceName, 0, KMaxUniqueIdLength);
  GetAudioDevices(voe_base, voe_hardware, audio_capture_device_name,
                  audio_capture_device_index, audio_playbackDeviceName,
                  audio_playback_device_index);

  // Get the audio codec for the call.
  memset(static_cast<void*>(&audio_codec), 0, sizeof(audio_codec));
  GetAudioCodecRecord(voe_codec, audio_codec);

  audio_channel = voe_base->CreateChannel();

  webrtc::scoped_ptr<webrtc::test::VoiceChannelTransport>
      voice_channel_transport(
          new webrtc::test::VoiceChannelTransport(voe_network, audio_channel));

  voice_channel_transport->SetSendDestination(ipAddress, audio_tx_port);
  voice_channel_transport->SetLocalReceiver(audio_rx_port);

  voe_hardware->SetRecordingDevice(audio_capture_device_index);
  voe_hardware->SetPlayoutDevice(audio_playback_device_index);
  voe_codec->SetSendCodec(audio_channel, audio_codec);
  voe_apm->SetAgcStatus(true, webrtc::kAgcDefault);
  voe_apm->SetNsStatus(true, webrtc::kNsHighSuppression);

  //
  // List available capture devices, allocate and connect.
  //
  webrtc::ViECapture* ptrViECapture =
      webrtc::ViECapture::GetInterface(ptrViE);
  if (ptrViECapture == NULL) {
    printf("ERROR in ViECapture::GetInterface\n");
    return -1;
  }

  webrtc::VoERTP_RTCP* ptrVoERtpRtcp =
    webrtc::VoERTP_RTCP::GetInterface(voe);
  if (ptrVoERtpRtcp == NULL) {
    printf("ERROR in VoERTP_RTCP::GetInterface\n");
    return -1;
  }

  memset(deviceName, 0, KMaxDeviceNameLength);
  char uniqueId[KMaxUniqueIdLength];
  memset(uniqueId, 0, KMaxUniqueIdLength);

  printf("Available capture devices:\n");
  int captureIdx = 0;
  for (captureIdx = 0;
       captureIdx < ptrViECapture->NumberOfCaptureDevices();
       captureIdx++) {
    memset(deviceName, 0, KMaxDeviceNameLength);
    memset(uniqueId, 0, KMaxUniqueIdLength);

    error = ptrViECapture->GetCaptureDevice(captureIdx, deviceName,
                                            KMaxDeviceNameLength, uniqueId,
                                            KMaxUniqueIdLength);
    if (error == -1) {
      printf("ERROR in ViECapture::GetCaptureDevice\n");
      return -1;
    }
    printf("\t %d. %s\n", captureIdx + 1, deviceName);
  }
  printf("\nChoose capture device: ");
#ifdef WEBRTC_ANDROID
  captureIdx = 0;
  printf("0\n");
#else
  if (scanf("%d", &captureIdx) != 1) {
    printf("Error in scanf()\n");
    return -1;
  }
  getc(stdin);
  captureIdx = captureIdx - 1;  // Compensate for idx start at 1.
#endif
  error = ptrViECapture->GetCaptureDevice(captureIdx, deviceName,
                                          KMaxDeviceNameLength, uniqueId,
                                          KMaxUniqueIdLength);
  if (error == -1) {
    printf("ERROR in ViECapture::GetCaptureDevice\n");
    return -1;
  }

  int captureId = 0;
  error = ptrViECapture->AllocateCaptureDevice(uniqueId, KMaxUniqueIdLength,
                                               captureId);
  if (error == -1) {
    printf("ERROR in ViECapture::AllocateCaptureDevice\n");
    return -1;
  }

  error = ptrViECapture->ConnectCaptureDevice(captureId, videoChannel);
  if (error == -1) {
    printf("ERROR in ViECapture::ConnectCaptureDevice\n");
    return -1;
  }

  error = ptrViECapture->StartCapture(captureId);
  if (error == -1) {
    printf("ERROR in ViECapture::StartCapture\n");
    return -1;
  }

  //
  // RTP/RTCP settings
  //
  webrtc::ViERTP_RTCP* ptrViERtpRtcp =
      webrtc::ViERTP_RTCP::GetInterface(ptrViE);
  if (ptrViERtpRtcp == NULL) {
    printf("ERROR in ViERTP_RTCP::GetInterface\n");
    return -1;
  }

  error = ptrViERtpRtcp->SetRTCPStatus(videoChannel,
                                       webrtc::kRtcpCompound_RFC4585);
  if (error == -1) {
    printf("ERROR in ViERTP_RTCP::SetRTCPStatus\n");
    return -1;
  }

  error = ptrViERtpRtcp->SetKeyFrameRequestMethod(
      videoChannel, webrtc::kViEKeyFrameRequestPliRtcp);
  if (error == -1) {
    printf("ERROR in ViERTP_RTCP::SetKeyFrameRequestMethod\n");
    return -1;
  }

  error = ptrViERtpRtcp->SetRembStatus(videoChannel, true, true);
  if (error == -1) {
    printf("ERROR in ViERTP_RTCP::SetTMMBRStatus\n");
    return -1;
  }

  //
  // Set up rendering
  //
  webrtc::ViERender* ptrViERender = webrtc::ViERender::GetInterface(ptrViE);
  if (ptrViERender == NULL) {
    printf("ERROR in ViERender::GetInterface\n");
    return -1;
  }

  error = ptrViERender->AddRenderer(captureId, window1, 0, 0.0, 0.0, 1.0, 1.0);
  if (error == -1) {
    printf("ERROR in ViERender::AddRenderer\n");
    return -1;
  }

  error = ptrViERender->StartRender(captureId);
  if (error == -1) {
    printf("ERROR in ViERender::StartRender\n");
    return -1;
  }

  error = ptrViERender->AddRenderer(videoChannel, window2, 1, 0.0, 0.0, 1.0,
                                    1.0);
  if (error == -1) {
    printf("ERROR in ViERender::AddRenderer\n");
    return -1;
  }

  error = ptrViERender->StartRender(videoChannel);
  if (error == -1) {
    printf("ERROR in ViERender::StartRender\n");
    return -1;
  }

  //
  // Setup codecs
  //
  webrtc::ViECodec* ptrViECodec = webrtc::ViECodec::GetInterface(ptrViE);
  if (ptrViECodec == NULL) {
    printf("ERROR in ViECodec::GetInterface\n");
    return -1;
  }

  webrtc::VideoCodec videoCodec;
  memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));
  int codecIdx = 0;

#ifdef WEBRTC_ANDROID
  codecIdx = 0;
  printf("0\n");
#else
  codecIdx = 0;  // Compensate for idx start at 1.
#endif

  error = ptrViECodec->GetCodec(codecIdx, videoCodec);
  if (error == -1) {
     printf("ERROR in ViECodec::GetCodec\n");
     return -1;
  }

  // Set spatial resolution option
  videoCodec.width = DEFAULT_VIDEO_CODEC_WIDTH;
  videoCodec.height = DEFAULT_VIDEO_CODEC_HEIGHT;

  // Set start bit rate
  videoCodec.startBitrate = DEFAULT_VIDEO_CODEC_START_RATE;

  error = ptrViECodec->SetSendCodec(videoChannel, videoCodec);
  if (error == -1) {
    printf("ERROR in ViECodec::SetSendCodec\n");
    return -1;
  }

  //
  // Address settings
  //
  webrtc::ViENetwork* ptrViENetwork =
      webrtc::ViENetwork::GetInterface(ptrViE);
  if (ptrViENetwork == NULL) {
    printf("ERROR in ViENetwork::GetInterface\n");
    return -1;
  }
  webrtc::test::VideoChannelTransport* video_channel_transport =
      new webrtc::test::VideoChannelTransport(ptrViENetwork, videoChannel);

  error = video_channel_transport->SetSendDestination(ipAddress, rtpPort);
  if (error == -1) {
    printf("ERROR in SetSendDestination\n");
    return -1;
  }
  error = video_channel_transport->SetLocalReceiver(rtpPort);
  if (error == -1) {
    printf("ERROR in SetLocalReceiver\n");
    return -1;
  }

  std::string str;
  int enable_labeling = 0;
  std::cout << std::endl;
  std::cout << "Do you want to label this recording?" << std::endl;
  std::cout << "0. No (default)." << std::endl;
  std::cout << "1. This call will be labeled on the fly." << std::endl;
  std::getline(std::cin, str);
  enable_labeling = atoi(str.c_str());

  uint32_t folder_time = static_cast<uint32_t>
    (webrtc::TickTime::MillisecondTimestamp());
  std::stringstream folder_time_str;
  folder_time_str <<  folder_time;
  const std::string folder_name = "recording" + folder_time_str.str();
  printf("recording name = %s\n", folder_name.c_str());
  // TODO(mikhal): use file_utils.
#ifdef WIN32
  _mkdir(folder_name.c_str());
#else
  mkdir(folder_name.c_str(), 0777);
#endif
  const std::string audio_filename =  folder_name + DEFAULT_RECORDING_AUDIO;
  const std::string video_filename =  folder_name + DEFAULT_RECORDING_VIDEO;
  const std::string audio_rtp_filename = folder_name +
    DEFAULT_RECORDING_AUDIO_RTP;
  const std::string video_rtp_filename = folder_name +
    DEFAULT_RECORDING_VIDEO_RTP;
  std::fstream timing;
  if (enable_labeling == 1) {
    std::cout << "Press enter to stamp current time."<< std::endl;
    std::string timing_file = folder_name + "/labeling.txt";
    timing.open(timing_file.c_str(), std::fstream::out | std::fstream::app);
  }
  printf("\nPress enter to start recording\n");
  std::getline(std::cin, str);
  printf("\nRecording started\n\n");

  error = ptrViEBase->StartReceive(videoChannel);
  if (error == -1) {
    printf("ERROR in ViENetwork::StartReceive\n");
    return -1;
  }

  error = ptrViEBase->StartSend(videoChannel);
  if (error == -1) {
    printf("ERROR in ViENetwork::StartSend\n");
    return -1;
  }
  error = voe_base->StartSend(audio_channel);
  if (error == -1) {
    printf("ERROR in VoENetwork::StartSend\n");
    return -1;
  }

  //  Engine started

  voe_apm->StartDebugRecording(audio_filename.c_str());
  ptrViECodec->StartDebugRecording(videoChannel, video_filename.c_str());
  ptrViERtpRtcp->StartRTPDump(videoChannel,
                              video_rtp_filename.c_str(), webrtc::kRtpOutgoing);
  ptrVoERtpRtcp->StartRTPDump(audio_channel,
                              audio_rtp_filename.c_str(), webrtc::kRtpOutgoing);
  printf("Press s + enter to stop...");
  int64_t clock_time;
  if (enable_labeling == 1) {
    clock_time = webrtc::TickTime::MillisecondTimestamp();
    timing << clock_time << std::endl;
  }
  char c = getc(stdin);
  fflush(stdin);
  while (c != 's') {
    if (c == '\n' && enable_labeling == 1) {
      clock_time = webrtc::TickTime::MillisecondTimestamp();
      timing << clock_time << std::endl;
    }
    c = getc(stdin);
  }
  if (enable_labeling == 1) {
    clock_time = webrtc::TickTime::MillisecondTimestamp();
    timing << clock_time << std::endl;
  }

  ptrViERtpRtcp->StopRTPDump(videoChannel, webrtc::kRtpOutgoing);
  ptrVoERtpRtcp->StopRTPDump(audio_channel, webrtc::kRtpOutgoing);
  voe_apm->StopDebugRecording();
  ptrViECodec->StopDebugRecording(videoChannel);
  if (enable_labeling == 1)
    timing.close();

  //  Recording finished. Tear down Video Engine.

  error = ptrViEBase->StopReceive(videoChannel);
  if (error == -1) {
    printf("ERROR in ViEBase::StopReceive\n");
    return -1;
  }

  error = ptrViEBase->StopSend(videoChannel);
  if (error == -1) {
    printf("ERROR in ViEBase::StopSend\n");
    return -1;
  }
  error = voe_base->StopSend(audio_channel);

  error = ptrViERender->StopRender(captureId);
  if (error == -1) {
    printf("ERROR in ViERender::StopRender\n");
    return -1;
  }

  error = ptrViERender->RemoveRenderer(captureId);
  if (error == -1) {
    printf("ERROR in ViERender::RemoveRenderer\n");
    return -1;
  }

  error = ptrViERender->StopRender(videoChannel);
  if (error == -1) {
    printf("ERROR in ViERender::StopRender\n");
    return -1;
  }

  error = ptrViERender->RemoveRenderer(videoChannel);
  if (error == -1) {
    printf("ERROR in ViERender::RemoveRenderer\n");
    return -1;
  }

  error = ptrViECapture->StopCapture(captureId);
  if (error == -1) {
    printf("ERROR in ViECapture::StopCapture\n");
    return -1;
  }

  error = ptrViECapture->DisconnectCaptureDevice(videoChannel);
  if (error == -1) {
    printf("ERROR in ViECapture::DisconnectCaptureDevice\n");
    return -1;
  }

  error = ptrViECapture->ReleaseCaptureDevice(captureId);
  if (error == -1) {
    printf("ERROR in ViECapture::ReleaseCaptureDevice\n");
    return -1;
  }

  error = ptrViEBase->DeleteChannel(videoChannel);
  if (error == -1) {
    printf("ERROR in ViEBase::DeleteChannel\n");
    return -1;
  }
  delete video_channel_transport;

  int remainingInterfaces = 0;
  remainingInterfaces = ptrViECodec->Release();
  remainingInterfaces += ptrViECapture->Release();
  remainingInterfaces += ptrViERtpRtcp->Release();
  remainingInterfaces += ptrViERender->Release();
  remainingInterfaces += ptrViENetwork->Release();
  remainingInterfaces += ptrViEBase->Release();
  if (remainingInterfaces > 0) {
    printf("ERROR: Could not release all interfaces\n");
    return -1;
  }
  bool deleted = webrtc::VideoEngine::Delete(ptrViE);
  if (deleted == false) {
    printf("ERROR in VideoEngine::Delete\n");
    return -1;
  }
  return 0;
}


// TODO(mikhal): Place above functionality under this class.
int ViEAutoTest::ViERecordCall() {
  ViETest::Log(" ");
  ViETest::Log("========================================");
  ViETest::Log(" ViE Record Call\n");

  if (VideoEngineSampleRecordCode(_window1, _window2) == 0) {
    ViETest::Log(" ");
    ViETest::Log(" ViE Autotest Record Call Done");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
  }

  ViETest::Log(" ");
  ViETest::Log(" ViE Autotest Record Call Failed");
  ViETest::Log("========================================");
  ViETest::Log(" ");
  return 1;
}

bool GetAudioCodecRecord(webrtc::VoECodec* voe_codec,
                         webrtc::CodecInst& audio_codec) {
  int error = 0;
  int number_of_errors = 0;
  memset(&audio_codec, 0, sizeof(webrtc::CodecInst));

  while (1) {
    int codec_idx = 0;
    int default_codec_idx = 0;
    for (codec_idx = 0; codec_idx < voe_codec->NumOfCodecs(); codec_idx++) {
      error = voe_codec->GetCodec(codec_idx, audio_codec);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

      // Test for default codec index.
      if (strcmp(audio_codec.plname, DEFAULT_AUDIO_CODEC) == 0) {
        default_codec_idx = codec_idx;
      }
    }
    error = voe_codec->GetCodec(default_codec_idx, audio_codec);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
    return true;
  }
  assert(false);
  return false;
}
