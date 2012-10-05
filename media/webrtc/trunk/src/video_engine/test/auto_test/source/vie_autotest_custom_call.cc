/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/test/auto_test/interface/vie_autotest.h"
#include "video_engine/test/auto_test/interface/vie_autotest_defines.h"

#define VCM_RED_PAYLOAD_TYPE                            96
#define VCM_ULPFEC_PAYLOAD_TYPE                         97
#define DEFAULT_SEND_IP                                 "127.0.0.1"
#define DEFAULT_VIDEO_PORT                              11111
#define DEFAULT_VIDEO_CODEC                             "vp8"
#define DEFAULT_VIDEO_CODEC_WIDTH                       640
#define DEFAULT_VIDEO_CODEC_HEIGHT                      480
#define DEFAULT_VIDEO_CODEC_BITRATE                     300
#define DEFAULT_VIDEO_CODEC_MIN_BITRATE                 100
#define DEFAULT_VIDEO_CODEC_MAX_BITRATE                 1000
#define DEFAULT_AUDIO_PORT                              11113
#define DEFAULT_AUDIO_CODEC                             "ISAC"
#define DEFAULT_INCOMING_FILE_NAME                      "IncomingFile.avi"
#define DEFAULT_OUTGOING_FILE_NAME                      "OutgoingFile.avi"
#define DEFAULT_VIDEO_CODEC_MAX_FRAMERATE               30
#define DEFAULT_VIDEO_PROTECTION_METHOD                 0
#define DEFAULT_TEMPORAL_LAYER                          0

enum StatisticsType {
  kSendStatistic,
  kReceivedStatistic
};

class ViEAutotestFileObserver : public webrtc::ViEFileObserver {
 public:
  ViEAutotestFileObserver() {}
  ~ViEAutotestFileObserver() {}

  void PlayFileEnded(const WebRtc_Word32 file_id) {
    ViETest::Log("PlayFile ended");
  }
};

class ViEAutotestEncoderObserver : public webrtc::ViEEncoderObserver {
 public:
  ViEAutotestEncoderObserver() {}
  ~ViEAutotestEncoderObserver() {}

  void OutgoingRate(const int video_channel,
                    const unsigned int framerate,
                    const unsigned int bitrate) {
    std::cout << "Send FR: " << framerate
              << " BR: " << bitrate << std::endl;
  }
};

class ViEAutotestDecoderObserver : public webrtc::ViEDecoderObserver {
 public:
  ViEAutotestDecoderObserver() {}
  ~ViEAutotestDecoderObserver() {}

  void IncomingRate(const int video_channel,
                    const unsigned int framerate,
                    const unsigned int bitrate) {
    std::cout << "Received FR: " << framerate
              << " BR: " << bitrate << std::endl;
  }
  void IncomingCodecChanged(const int video_channel,
                            const webrtc::VideoCodec& codec) {}
  void RequestNewKeyFrame(const int video_channel) {
    std::cout << "Decoder requesting a new key frame." << std::endl;
  }
};

// The following are general helper functions.
bool GetVideoDevice(webrtc::ViEBase* vie_base,
                    webrtc::ViECapture* vie_capture,
                    char* capture_device_name, char* capture_device_unique_id);
bool GetIPAddress(char* IP);
bool ValidateIP(std::string i_str);

// The following are Print to stdout functions.
void PrintCallInformation(char* IP,
                          char* video_capture_device_name,
                          char* video_capture_unique_id,
                          webrtc::VideoCodec video_codec,
                          int video_tx_port,
                          int video_rx_port,
                          char* audio_capture_device_name,
                          char* audio_playbackDeviceName,
                          webrtc::CodecInst audio_codec,
                          int audio_tx_port,
                          int audio_rx_port,
                          int protection_method);
void PrintRTCCPStatistics(webrtc::ViERTP_RTCP* vie_rtp_rtcp,
                          int video_channel,
                          StatisticsType stat_type);
void PrintRTPStatistics(webrtc::ViERTP_RTCP* vie_rtp_rtcp,
                        int video_channel);
void PrintBandwidthUsage(webrtc::ViERTP_RTCP* vie_rtp_rtcp,
                         int video_channel);
void PrintCodecStatistics(webrtc::ViECodec* vie_codec,
                          int video_channel,
                          StatisticsType stat_type);
void PrintGetDiscardedPackets(webrtc::ViECodec* vie_codec,
                              int video_channel);
void PrintVideoStreamInformation(webrtc::ViECodec* vie_codec,
                                 int video_channel);
void PrintVideoCodec(webrtc::VideoCodec video_codec);

// The following are video functions.
// TODO(amyfong): change to pointers as input arguments
// instead of references
bool SetVideoPorts(int* tx_port, int* rx_port);
bool SetVideoCodecType(webrtc::ViECodec* vie_codec,
                       webrtc::VideoCodec* video_codec);
bool SetVideoCodecResolution(webrtc::ViECodec* vie_codec,
                             webrtc::VideoCodec* video_codec);
bool SetVideoCodecSize(webrtc::ViECodec* vie_codec,
                       webrtc::VideoCodec* video_codec);
bool SetVideoCodecBitrate(webrtc::ViECodec* vie_codec,
                          webrtc::VideoCodec* video_codec);
bool SetVideoCodecMinBitrate(webrtc::ViECodec* vie_codec,
                             webrtc::VideoCodec* video_codec);
bool SetVideoCodecMaxBitrate(webrtc::ViECodec* vie_codec,
                             webrtc::VideoCodec* video_codec);
bool SetVideoCodecMaxFramerate(webrtc::ViECodec* vie_codec,
                               webrtc::VideoCodec* video_codec);
bool SetVideoCodecTemporalLayer(webrtc::VideoCodec* video_codec);
int GetVideoProtection();
bool SetVideoProtection(webrtc::ViECodec* vie_codec,
                        webrtc::ViERTP_RTCP* vie_rtp_rtcp,
                        int video_channel, int protection_method);
bool GetBitrateSignaling();

// The following are audio helper functions.
bool GetAudioDevices(webrtc::VoEBase* voe_base,
                     webrtc::VoEHardware* voe_hardware,
                     char* recording_device_name, int& recording_device_index,
                     char* playbackDeviceName, int& playback_device_index);
bool GetAudioDevices(webrtc::VoEBase* voe_base,
                     webrtc::VoEHardware* voe_hardware,
                     int& recording_device_index, int& playback_device_index);
bool GetAudioPorts(int* tx_port, int* rx_port);
bool GetAudioCodec(webrtc::VoECodec* voe_codec,
                   webrtc::CodecInst& audio_codec);

int ViEAutoTest::ViECustomCall() {
  ViETest::Log(" ");
  ViETest::Log("========================================");
  ViETest::Log(" Enter values to use custom settings\n");

  int error = 0;
  int number_of_errors = 0;
  std::string str;

  // Create the VoE and get the VoE interfaces.
  webrtc::VoiceEngine* voe = webrtc::VoiceEngine::Create();
  number_of_errors += ViETest::TestError(voe != NULL, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

  webrtc::VoEBase* voe_base = webrtc::VoEBase::GetInterface(voe);
  number_of_errors += ViETest::TestError(voe_base != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

  error = voe_base->Init();
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

  webrtc::VoECodec* voe_codec = webrtc::VoECodec::GetInterface(voe);
  number_of_errors += ViETest::TestError(voe_codec != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

  webrtc::VoEHardware* voe_hardware =
      webrtc::VoEHardware::GetInterface(voe);
  number_of_errors += ViETest::TestError(voe_hardware != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

  webrtc::VoEAudioProcessing* voe_apm =
      webrtc::VoEAudioProcessing::GetInterface(voe);
  number_of_errors += ViETest::TestError(voe_apm != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

  // Create the ViE and get the ViE Interfaces.
  webrtc::VideoEngine* vie = webrtc::VideoEngine::Create();
  number_of_errors += ViETest::TestError(vie != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

  webrtc::ViEBase* vie_base = webrtc::ViEBase::GetInterface(vie);
  number_of_errors += ViETest::TestError(vie_base != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

  error = vie_base->Init();
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

  webrtc::ViECapture* vie_capture =
      webrtc::ViECapture::GetInterface(vie);
  number_of_errors += ViETest::TestError(vie_capture != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

  webrtc::ViERender* vie_renderer = webrtc::ViERender::GetInterface(vie);
  number_of_errors += ViETest::TestError(vie_renderer != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

  webrtc::ViECodec* vie_codec = webrtc::ViECodec::GetInterface(vie);
  number_of_errors += ViETest::TestError(vie_codec != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

  webrtc::ViENetwork* vie_network = webrtc::ViENetwork::GetInterface(vie);
  number_of_errors += ViETest::TestError(vie_network != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

  webrtc::ViEFile* vie_file = webrtc::ViEFile::GetInterface(vie);
  number_of_errors += ViETest::TestError(vie_file != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

  bool start_call = false;
  const unsigned int kMaxIPLength = 16;
  char ip_address[kMaxIPLength] = "";
  const unsigned int KMaxUniqueIdLength = 256;
  char unique_id[KMaxUniqueIdLength] = "";
  char device_name[KMaxUniqueIdLength] = "";
  int video_tx_port = 0;
  int video_rx_port = 0;
  int video_channel = -1;
  webrtc::VideoCodec video_send_codec;
  char audio_capture_device_name[KMaxUniqueIdLength] = "";
  char audio_playbackDeviceName[KMaxUniqueIdLength] = "";
  int audio_capture_device_index = -1;
  int audio_playback_device_index = -1;
  int audio_tx_port = 0;
  int audio_rx_port = 0;
  webrtc::CodecInst audio_codec;
  int audio_channel = -1;
  bool is_image_scale_enabled = false;
  int protection_method = DEFAULT_VIDEO_PROTECTION_METHOD;
  bool remb = true;

  while (!start_call) {
    // Get the IP address to use from call.
    memset(ip_address, 0, kMaxIPLength);
    GetIPAddress(ip_address);

    // Get the video device to use for call.
    memset(device_name, 0, KMaxUniqueIdLength);
    memset(unique_id, 0, KMaxUniqueIdLength);
    GetVideoDevice(vie_base, vie_capture, device_name, unique_id);

    // Get and set the video ports for the call.
    video_tx_port = 0;
    video_rx_port = 0;
    SetVideoPorts(&video_tx_port, &video_rx_port);

    // Get and set the video codec parameters for the call.
    memset(&video_send_codec, 0, sizeof(webrtc::VideoCodec));
    SetVideoCodecType(vie_codec, &video_send_codec);
    SetVideoCodecSize(vie_codec, &video_send_codec);
    SetVideoCodecBitrate(vie_codec, &video_send_codec);
    SetVideoCodecMinBitrate(vie_codec, &video_send_codec);
    SetVideoCodecMaxBitrate(vie_codec, &video_send_codec);
    SetVideoCodecMaxFramerate(vie_codec, &video_send_codec);
    SetVideoCodecTemporalLayer(&video_send_codec);
    remb = GetBitrateSignaling();

    // Get the video protection method for the call.
    protection_method = GetVideoProtection();

    // Get the audio device for the call.
    memset(audio_capture_device_name, 0, KMaxUniqueIdLength);
    memset(audio_playbackDeviceName, 0, KMaxUniqueIdLength);
    GetAudioDevices(voe_base, voe_hardware, audio_capture_device_name,
                    audio_capture_device_index, audio_playbackDeviceName,
                    audio_playback_device_index);

    // Get the audio port for the call.
    audio_tx_port = 0;
    audio_rx_port = 0;
    GetAudioPorts(&audio_tx_port, &audio_rx_port);

    // Get the audio codec for the call.
    memset(static_cast<void*>(&audio_codec), 0, sizeof(audio_codec));
    GetAudioCodec(voe_codec, audio_codec);

    // Now ready to start the call.  Check user wants to continue.
    PrintCallInformation(ip_address, device_name, unique_id, video_send_codec,
                         video_tx_port, video_rx_port,
                         audio_capture_device_name, audio_playbackDeviceName,
                         audio_codec, audio_tx_port, audio_rx_port,
                         protection_method);

    std::cout << std::endl;
    std::cout << "1. Start the call" << std::endl;
    std::cout << "2. Reconfigure call settings" << std::endl;
    std::cout << "What do you want to do? Press enter for default "
              << "(Start the call): ";

    std::getline(std::cin, str);
    int selection = 0;
    selection = atoi(str.c_str());

    switch (selection) {
      case 0:
        start_call = true;
        break;
      case 1:
        start_call = true;
        break;
      case 2:
        start_call = false;
        break;
      default:
        // Invalid selection gets error mesage.
        std::cout << "ERROR: Code=" << error
                  << " Invalid selection" << std::endl;
        continue;
    }
  }
  /// **************************************************************
  // Begin create/initialize WebRTC Video Engine for testing.
  /// **************************************************************
  if (start_call == true) {
    // Configure audio channel first.
    audio_channel = voe_base->CreateChannel();
    error = voe_base->SetSendDestination(audio_channel, audio_tx_port,
                                         ip_address);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = voe_base->SetLocalReceiver(audio_channel, audio_rx_port);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = voe_hardware->SetRecordingDevice(audio_capture_device_index);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = voe_hardware->SetPlayoutDevice(audio_playback_device_index);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = voe_codec->SetSendCodec(audio_channel, audio_codec);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = voe_apm->SetAgcStatus(true, webrtc::kAgcDefault);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = voe_apm->SetNsStatus(true, webrtc::kNsHighSuppression);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
    // Now configure the video channel.
    error = vie->SetTraceFilter(webrtc::kTraceAll);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    std::string trace_file =
        ViETest::GetResultOutputPath() + "ViECustomCall_trace.txt";
    error = vie->SetTraceFile(trace_file.c_str());
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_base->SetVoiceEngine(voe);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_base->CreateChannel(video_channel);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_base->ConnectAudioChannel(video_channel, audio_channel);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    int capture_id = 0;
    error = vie_capture->AllocateCaptureDevice(unique_id,
                                               KMaxUniqueIdLength,
                                               capture_id);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_capture->ConnectCaptureDevice(capture_id, video_channel);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_capture->StartCapture(capture_id);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    webrtc::ViERTP_RTCP* vie_rtp_rtcp =
        webrtc::ViERTP_RTCP::GetInterface(vie);
    number_of_errors += ViETest::TestError(vie != NULL,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_rtp_rtcp->SetRTCPStatus(video_channel,
                                        webrtc::kRtcpCompound_RFC4585);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_rtp_rtcp->SetKeyFrameRequestMethod(
        video_channel, webrtc::kViEKeyFrameRequestPliRtcp);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    if (remb) {
      error = vie_rtp_rtcp->SetRembStatus(video_channel, true, true);
      number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    } else  {
      error = vie_rtp_rtcp->SetTMMBRStatus(video_channel, true);
      number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    }

    error = vie_renderer->AddRenderer(capture_id, _window1, 0, 0.0, 0.0, 1.0,
                                      1.0);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_renderer->AddRenderer(video_channel, _window2, 1, 0.0, 0.0, 1.0,
                                      1.0);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
    error = vie_network->SetSendDestination(video_channel, ip_address,
                                                video_tx_port);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_network->SetLocalReceiver(video_channel, video_rx_port);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_codec->SetSendCodec(video_channel, video_send_codec);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_codec->SetReceiveCodec(video_channel, video_send_codec);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
    // Set the Video Protection before start send and receive.
    SetVideoProtection(vie_codec, vie_rtp_rtcp,
                       video_channel, protection_method);

    // Start Voice Playout and Receive.
    error = voe_base->StartReceive(audio_channel);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = voe_base->StartPlayout(audio_channel);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = voe_base->StartSend(audio_channel);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    // Now start the Video Send & Receive.
    error = vie_base->StartSend(video_channel);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_base->StartReceive(video_channel);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_renderer->StartRender(capture_id);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_renderer->StartRender(video_channel);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
    ViEAutotestFileObserver file_observer;
    int file_id;

    ViEAutotestEncoderObserver* codec_encoder_observer = NULL;
    ViEAutotestDecoderObserver* codec_decoder_observer = NULL;

    //  Engine ready, wait for input.

    // Call started.
    std::cout << std::endl;
    std::cout << "Custom call started" << std::endl;
    std::cout << std::endl << std::endl;

    // Modify call or stop call.

    std::cout << "Custom call in progress, would you like do?" << std::endl;
    std::cout << "  0. Stop the call" << std::endl;
    std::cout << "  1. Modify the call" << std::endl;
    std::cout << "What do you want to do? "
              << "Press enter for default (Stop the call): ";

    std::getline(std::cin, str);
    int selection = 0;
    selection = atoi(str.c_str());

    // Keep on modifying the call until user selects finish modify call.
    bool modify_call = false;

    while (selection == 1) {
      std::cout << "Modify Custom Call" << std::endl;
      std::cout << "  0. Finished modifying custom call" << std::endl;
      std::cout << "  1. Change Video Send Codec" << std::endl;
      std::cout << "  2. Change Video Send Size by Common Resolutions"
                << std::endl;
      std::cout << "  3. Change Video Send Size by Width & Height" << std::endl;
      std::cout << "  4. Change Video Capture Device" << std::endl;
      std::cout << "  5. Record Incoming Call" << std::endl;
      std::cout << "  6. Record Outgoing Call" << std::endl;
      std::cout << "  7. Play File on Video Channel"
                << "(Assumes you recorded incoming & outgoing call)"
                << std::endl;
      std::cout << "  8. Change Video Protection Method" << std::endl;
      std::cout << "  9. Toggle Encoder Observer" << std::endl;
      std::cout << " 10. Toggle Decoder Observer" << std::endl;
      std::cout << " 11. Print Call Information" << std::endl;
      std::cout << " 12. Print Call Statistics" << std::endl;
      std::cout << " 13. Toggle Image Scaling "
                << "(Warning high CPU usage when enabled)"
                << std::endl;
      std::cout << "What do you want to do? ";
      std::cout << "Press enter for default "
                << "(Finished modifying custom call): ";

      std::getline(std::cin, str);
      int modify_selection = 0;
      int file_selection = 0;

      modify_selection = atoi(str.c_str());

      switch (modify_selection) {
        case 0:
          std::cout << "Finished modifying custom call." << std::endl;
          modify_call = false;
          break;
        case 1:
          // Change video codec.
          SetVideoCodecType(vie_codec, &video_send_codec);
          SetVideoCodecSize(vie_codec, &video_send_codec);
          SetVideoCodecBitrate(vie_codec, &video_send_codec);
          SetVideoCodecMinBitrate(vie_codec, &video_send_codec);
          SetVideoCodecMaxBitrate(vie_codec, &video_send_codec);
          SetVideoCodecMaxFramerate(vie_codec, &video_send_codec);
          SetVideoCodecTemporalLayer(&video_send_codec);
          PrintCallInformation(ip_address, device_name,
                               unique_id, video_send_codec,
                               video_tx_port, video_rx_port,
                               audio_capture_device_name,
                               audio_playbackDeviceName, audio_codec,
                               audio_tx_port, audio_rx_port, protection_method);
          error = vie_codec->SetSendCodec(video_channel, video_send_codec);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
          error = vie_codec->SetReceiveCodec(video_channel, video_send_codec);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
          modify_call = true;
          break;
        case 2:
          // Change Video codec size by common resolution.
          SetVideoCodecResolution(vie_codec, &video_send_codec);
          PrintCallInformation(ip_address, device_name,
                               unique_id, video_send_codec,
                               video_tx_port, video_rx_port,
                               audio_capture_device_name,
                               audio_playbackDeviceName, audio_codec,
                               audio_tx_port, audio_rx_port, protection_method);
          error = vie_codec->SetSendCodec(video_channel, video_send_codec);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
          error = vie_codec->SetReceiveCodec(video_channel, video_send_codec);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
          modify_call = true;
          break;
        case 3:
          // Change video codec by size height and width.
          SetVideoCodecSize(vie_codec, &video_send_codec);
          PrintCallInformation(ip_address, device_name,
                               unique_id, video_send_codec,
                               video_tx_port, video_rx_port,
                               audio_capture_device_name,
                               audio_playbackDeviceName, audio_codec,
                               audio_tx_port, audio_rx_port, protection_method);
          error = vie_codec->SetSendCodec(video_channel, video_send_codec);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
          error = vie_codec->SetReceiveCodec(video_channel, video_send_codec);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
          modify_call = true;
          break;
        case 4:
          error = vie_renderer->StopRender(capture_id);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
          error = vie_renderer->RemoveRenderer(capture_id);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
          error = vie_capture->StopCapture(capture_id);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
          error = vie_capture->DisconnectCaptureDevice(video_channel);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
          error = vie_capture->ReleaseCaptureDevice(capture_id);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
          memset(device_name, 0, KMaxUniqueIdLength);
          memset(unique_id, 0, KMaxUniqueIdLength);
          GetVideoDevice(vie_base, vie_capture, device_name, unique_id);
          capture_id = 0;
          error = vie_capture->AllocateCaptureDevice(unique_id,
                                                     KMaxUniqueIdLength,
                                                     capture_id);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
          error = vie_capture->ConnectCaptureDevice(capture_id,
                                                    video_channel);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
          error = vie_capture->StartCapture(capture_id);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
          error = vie_renderer->AddRenderer(capture_id, _window1, 0, 0.0, 0.0,
                                            1.0, 1.0);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
          error = vie_renderer->StartRender(capture_id);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
          modify_call = true;
          break;
        case 5:
          // Record the incoming call.
          std::cout << "Start Recording Incoming Video "
                    << DEFAULT_INCOMING_FILE_NAME <<  std::endl;
          error = vie_file->StartRecordIncomingVideo(
                    video_channel, DEFAULT_INCOMING_FILE_NAME,
                    webrtc::NO_AUDIO, audio_codec, video_send_codec);
          std::cout << "Press enter to stop...";
          std::getline(std::cin, str);
          error = vie_file->StopRecordIncomingVideo(video_channel);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 vie_base->LastError(),
                                                 __FUNCTION__, __LINE__);
          modify_call = true;
          break;
        case 6:
          // Record the outgoing call.
          std::cout << "Start Recording Outgoing Video "
                    << DEFAULT_OUTGOING_FILE_NAME <<  std::endl;
          error = vie_file->StartRecordOutgoingVideo(
                    video_channel, DEFAULT_OUTGOING_FILE_NAME,
                    webrtc::NO_AUDIO, audio_codec, video_send_codec);
          std::cout << "Press enter to stop...";
          std::getline(std::cin, str);
          error = vie_file->StopRecordOutgoingVideo(video_channel);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 vie_base->LastError(),
                                                 __FUNCTION__, __LINE__);
          modify_call = true;
          break;
        case 7:
          // Send the file on the video_channel.
          file_selection = 0;
          std::cout << "Available files to play" << std::endl;
          std::cout << "  0. " << DEFAULT_INCOMING_FILE_NAME <<  std::endl;
          std::cout << "  1. " << DEFAULT_OUTGOING_FILE_NAME <<  std::endl;
          std::cout << "Press enter for default ("
                    << DEFAULT_INCOMING_FILE_NAME << "): ";
          std::getline(std::cin, str);
          file_selection = atoi(str.c_str());
          // Disconnect the camera first.
          error = vie_capture->DisconnectCaptureDevice(video_channel);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 vie_base->LastError(),
                                                 __FUNCTION__, __LINE__);
          if (file_selection == 1)
            error = vie_file->StartPlayFile(DEFAULT_OUTGOING_FILE_NAME,
                                            file_id, true);
          else
            error = vie_file->StartPlayFile(DEFAULT_INCOMING_FILE_NAME,
                                            file_id, true);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 vie_base->LastError(),
                                                 __FUNCTION__, __LINE__);
          ViETest::Log("Registering file observer");
          error = vie_file->RegisterObserver(file_id, file_observer);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 vie_base->LastError(),
                                                 __FUNCTION__, __LINE__);
          std::cout << std::endl;
          std::cout << "Start sending the file that is played in a loop "
                    << std::endl;
          error = vie_file->SendFileOnChannel(file_id, video_channel);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 vie_base->LastError(),
                                                 __FUNCTION__, __LINE__);
          std::cout << "Press enter to stop...";
          std::getline(std::cin, str);
          ViETest::Log("Stopped sending video on channel");
          error = vie_file->StopSendFileOnChannel(video_channel);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 vie_base->LastError(),
                                                 __FUNCTION__, __LINE__);
          ViETest::Log("Stop playing the file.");
          error = vie_file->StopPlayFile(file_id);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 vie_base->LastError(),
                                                 __FUNCTION__, __LINE__);
          error = vie_capture->ConnectCaptureDevice(capture_id,
                                                        video_channel);
          number_of_errors += ViETest::TestError(error == 0,
                                                 "ERROR:%d %s at line %d",
                                                 vie_base->LastError(),
                                                 __FUNCTION__, __LINE__);
          error = vie_file->DeregisterObserver(file_id, file_observer);
          number_of_errors += ViETest::TestError(error == -1,
                                                 "ERROR:%d %s at line %d",
                                                 vie_base->LastError(),
                                                 __FUNCTION__, __LINE__);
          modify_call = true;
          break;
        case 8:
          // Change the Video Protection.
          protection_method = GetVideoProtection();
          SetVideoProtection(vie_codec, vie_rtp_rtcp,
                             video_channel, protection_method);
          modify_call = true;
          break;
        case 9:
          // Toggle Encoder Observer.
          if (!codec_encoder_observer) {
            std::cout << "Registering Encoder Observer" << std::endl;
            codec_encoder_observer = new ViEAutotestEncoderObserver();
            error = vie_codec->RegisterEncoderObserver(video_channel,
                                                       *codec_encoder_observer);
            number_of_errors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                   __FUNCTION__, __LINE__);
          } else {
            std::cout << "Deregistering Encoder Observer" << std::endl;
            error = vie_codec->DeregisterEncoderObserver(video_channel);
            delete codec_encoder_observer;
            codec_encoder_observer = NULL;
            number_of_errors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                   __FUNCTION__, __LINE__);
          }
          modify_call = true;
          break;
        case 10:
          // Toggle Decoder Observer.
          if (!codec_decoder_observer) {
            std::cout << "Registering Decoder Observer" << std::endl;
            codec_decoder_observer = new ViEAutotestDecoderObserver();
            error = vie_codec->RegisterDecoderObserver(video_channel,
                                                       *codec_decoder_observer);
            number_of_errors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                   __FUNCTION__, __LINE__);
          } else {
            std::cout << "Deregistering Decoder Observer" << std::endl;
            error = vie_codec->DeregisterDecoderObserver(video_channel);
            delete codec_decoder_observer;
            codec_decoder_observer = NULL;
            number_of_errors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                   __FUNCTION__, __LINE__);
          }
          modify_call = true;
          break;
        case 11:
          // Print Call information..
          PrintCallInformation(ip_address, device_name,
                               unique_id, video_send_codec,
                               video_tx_port, video_rx_port,
                               audio_capture_device_name,
                               audio_playbackDeviceName,
                               audio_codec, audio_tx_port,
                               audio_rx_port, protection_method);
          PrintVideoStreamInformation(vie_codec,
                                      video_channel);
          modify_call = true;
          break;
        case 12:
          // Print Call statistics.
          PrintRTCCPStatistics(vie_rtp_rtcp, video_channel,
                               kSendStatistic);
          PrintRTCCPStatistics(vie_rtp_rtcp, video_channel,
                               kReceivedStatistic);
          PrintRTPStatistics(vie_rtp_rtcp, video_channel);
          PrintBandwidthUsage(vie_rtp_rtcp, video_channel);
          PrintCodecStatistics(vie_codec, video_channel,
                               kSendStatistic);
          PrintCodecStatistics(vie_codec, video_channel,
                               kReceivedStatistic);
          PrintGetDiscardedPackets(vie_codec, video_channel);
          modify_call = true;
          break;
        case 13:
          is_image_scale_enabled = !is_image_scale_enabled;
          vie_codec->SetImageScaleStatus(video_channel, is_image_scale_enabled);
          if (is_image_scale_enabled) {
            std::cout << "Image Scale is now enabled" << std::endl;
          } else {
            std::cout << "Image Scale is now disabled" << std::endl;
          }
          modify_call = true;
          break;
        default:
          // Invalid selection, shows options menu again.
          std::cout << "Invalid selection. Select Again." << std::endl;
          break;
      }
      // Modify_call is false if user does not select one of the modify options.
      if (modify_call == false) {
        selection = 0;
      }
    }

    // Stop the Call
    std::cout << "Press enter to stop...";
    std::getline(std::cin, str);

    // Testing finished. Tear down Voice and Video Engine.
    // Tear down the VoE first.
    error = voe_base->StopReceive(audio_channel);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = voe_base->StopPlayout(audio_channel);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = voe_base->DeleteChannel(audio_channel);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
    // Now tear down the ViE engine.
    error = vie_base->DisconnectAudioChannel(video_channel);

    // If Encoder/Decoder Observer is running, delete them.
    if (codec_encoder_observer) {
      error = vie_codec->DeregisterEncoderObserver(video_channel);
      delete codec_encoder_observer;
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    }
    if (codec_decoder_observer) {
      error = vie_codec->DeregisterDecoderObserver(video_channel);
      delete codec_decoder_observer;
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    }

    error = vie_base->StopReceive(video_channel);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_base->StopSend(video_channel);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_renderer->StopRender(capture_id);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_renderer->StopRender(video_channel);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_renderer->RemoveRenderer(capture_id);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_renderer->RemoveRenderer(video_channel);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_capture->StopCapture(capture_id);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_capture->DisconnectCaptureDevice(video_channel);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_capture->ReleaseCaptureDevice(capture_id);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = vie_base->DeleteChannel(video_channel);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    int remaining_interfaces = 0;
    remaining_interfaces = vie_file->Release();
    number_of_errors += ViETest::TestError(remaining_interfaces == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
    remaining_interfaces = vie_codec->Release();
    number_of_errors += ViETest::TestError(remaining_interfaces == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    remaining_interfaces = vie_capture->Release();
    number_of_errors += ViETest::TestError(remaining_interfaces == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    remaining_interfaces = vie_rtp_rtcp->Release();
    number_of_errors += ViETest::TestError(remaining_interfaces == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    remaining_interfaces = vie_renderer->Release();
    number_of_errors += ViETest::TestError(remaining_interfaces == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    remaining_interfaces = vie_network->Release();
    number_of_errors += ViETest::TestError(remaining_interfaces == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    remaining_interfaces = vie_base->Release();
    number_of_errors += ViETest::TestError(remaining_interfaces == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    bool deleted = webrtc::VideoEngine::Delete(vie);
    number_of_errors += ViETest::TestError(deleted == true,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    ViETest::Log(" ");
    ViETest::Log(" ViE Autotest Custom Call Started");
    ViETest::Log("========================================");
    ViETest::Log(" ");
  }
  return number_of_errors;
}

bool GetVideoDevice(webrtc::ViEBase* vie_base,
                    webrtc::ViECapture* vie_capture,
                    char* capture_device_name,
                    char* capture_device_unique_id) {
  int error = 0;
  int number_of_errors = 0;
  int capture_device_index = 0;
  std::string str;

  const unsigned int KMaxDeviceNameLength = 128;
  const unsigned int KMaxUniqueIdLength = 256;
  char device_name[KMaxDeviceNameLength];
  char unique_id[KMaxUniqueIdLength];

  while (1) {
    memset(device_name, 0, KMaxDeviceNameLength);
    memset(unique_id, 0, KMaxUniqueIdLength);

    std::cout << std::endl;
    std::cout << "Available video capture devices:" << std::endl;
    int capture_idx = 0;
    for (capture_idx = 0;
         capture_idx < vie_capture->NumberOfCaptureDevices();
         capture_idx++) {
      memset(device_name, 0, KMaxDeviceNameLength);
      memset(unique_id, 0, KMaxUniqueIdLength);

      error = vie_capture->GetCaptureDevice(capture_idx, device_name,
                                            KMaxDeviceNameLength,
                                            unique_id,
                                            KMaxUniqueIdLength);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      std::cout << "   " << capture_idx + 1 << ". " << device_name
                << "/" << unique_id
                << std::endl;
    }
    //  Get the dev_name of the default (or first) camera for display.
    error = vie_capture->GetCaptureDevice(0, device_name,
                                          KMaxDeviceNameLength,
                                          unique_id,
                                          KMaxUniqueIdLength);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    std::cout << "Choose a video capture device. Press enter for default ("
              << device_name << "/" << unique_id << "): ";
    std::getline(std::cin, str);
    capture_device_index = atoi(str.c_str());

    if (capture_device_index == 0) {
      // Use the default (or first) camera.
      error = vie_capture->GetCaptureDevice(0, device_name,
                                            KMaxDeviceNameLength,
                                            unique_id,
                                            KMaxUniqueIdLength);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      strcpy(capture_device_unique_id, unique_id);
      strcpy(capture_device_name, device_name);
      return true;
    } else if (
        capture_device_index < 0 ||
        (capture_device_index >
            static_cast<int>(vie_capture->NumberOfCaptureDevices()))) {
      // invalid selection
      continue;
    } else {
      error = vie_capture->GetCaptureDevice(capture_device_index - 1,
                                            device_name,
                                            KMaxDeviceNameLength,
                                            unique_id,
                                            KMaxUniqueIdLength);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      strcpy(capture_device_unique_id, unique_id);
      strcpy(capture_device_name, device_name);
      return true;
    }
  }
}

bool GetAudioDevices(webrtc::VoEBase* voe_base,
                     webrtc::VoEHardware* voe_hardware,
                     char* recording_device_name,
                     int& recording_device_index,
                     char* playbackDeviceName,
                     int& playback_device_index) {
  int error = 0;
  int number_of_errors = 0;
  std::string str;

  const unsigned int KMaxDeviceNameLength = 128;
  const unsigned int KMaxUniqueIdLength = 128;
  char recording_device_unique_name[KMaxDeviceNameLength];
  char playbackDeviceUniqueName[KMaxUniqueIdLength];

  int number_of_recording_devices = -1;
  error = voe_hardware->GetNumOfRecordingDevices(number_of_recording_devices);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

  while (1) {
    recording_device_index = -1;
    std::cout << std::endl;
    std::cout << "Available audio capture devices:" << std::endl;
    int capture_idx = 0;

    for (capture_idx = 0; capture_idx < number_of_recording_devices;
         capture_idx++) {
      memset(recording_device_name, 0, KMaxDeviceNameLength);
      memset(recording_device_unique_name, 0, KMaxDeviceNameLength);
      error = voe_hardware->GetRecordingDeviceName(
          capture_idx, recording_device_name, recording_device_unique_name);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      std::cout << "   " << capture_idx + 1 << ". " << recording_device_name
                << std::endl;
    }

    std::cout << "Choose an audio capture device. Press enter for default("
              << recording_device_name << "): ";
    std::getline(std::cin, str);
    int capture_device_index = atoi(str.c_str());

    if (capture_device_index == 0) {
      // Use the default (or first) recording device.
      recording_device_index = 0;
      error = voe_hardware->GetRecordingDeviceName(
          recording_device_index, recording_device_name,
          recording_device_unique_name);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      break;
    } else if (capture_device_index < 0 ||
               capture_device_index > number_of_recording_devices) {
      // Invalid selection.
      continue;
    } else {
      recording_device_index = capture_device_index - 1;
      error = voe_hardware->GetRecordingDeviceName(
          recording_device_index, recording_device_name,
          recording_device_unique_name);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      break;
    }
  }

  int number_of_playbackDevices = -1;
  error = voe_hardware->GetNumOfPlayoutDevices(number_of_playbackDevices);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

  while (1) {
    playback_device_index = -1;
    std::cout << std::endl;
    std::cout << "Available audio playout devices:" << std::endl;
    int capture_idx = 0;

    for (capture_idx = 0; capture_idx < number_of_playbackDevices;
         capture_idx++) {
      memset(playbackDeviceName, 0, KMaxDeviceNameLength);
      memset(playbackDeviceUniqueName, 0, KMaxDeviceNameLength);
      error = voe_hardware->GetPlayoutDeviceName(capture_idx,
                                                 playbackDeviceName,
                                                 playbackDeviceUniqueName);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      std::cout << "   " << capture_idx + 1 << ". " << playbackDeviceName
                << std::endl;
    }

    std::cout << "Choose an audio playback device. Press enter for default ("
              << playbackDeviceName << "): ";
    std::getline(std::cin, str);
    int capture_device_index = atoi(str.c_str());

    if (capture_device_index == 0) {
      // Use the default (or first) playout device.
      playback_device_index = 0;
      error = voe_hardware->GetPlayoutDeviceName(
                playback_device_index, playbackDeviceName,
                playbackDeviceUniqueName);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      return true;
    } else if (capture_device_index < 0
               || capture_device_index > number_of_playbackDevices) {
      // Invalid selection.
      continue;
    } else {
      playback_device_index = capture_device_index - 1;
      error = voe_hardware->GetPlayoutDeviceName(playback_device_index,
                                                 playbackDeviceName,
                                                 playbackDeviceUniqueName);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      return true;
    }
  }
}

// General helper functions.

bool GetIPAddress(char* i_ip) {
  char o_ip[16] = DEFAULT_SEND_IP;
  std::string str;

  while (1) {
    std::cout << std::endl;
    std::cout << "Enter destination IP. Press enter for default ("
              << o_ip << "): ";
    std::getline(std::cin, str);

    if (str.compare("") == 0) {
      // use default value;
      strcpy(i_ip, o_ip);
      return true;
    }
    if (ValidateIP(str) == false) {
      std::cout << "Invalid entry. Try again." << std::endl;
      continue;
    }
    // Done, copy std::string to c_string and return.
    strcpy(i_ip, str.c_str());
    return true;
  }
  assert(false);
  return false;
}

bool ValidateIP(std::string i_str) {
  if (0 == i_str.compare("")) {
    return false;
  }
  return true;
}

// Video settings functions.

bool SetVideoPorts(int* tx_port, int* rx_port) {
  std::string str;
  int port = 0;

  // Set to default values.
  *tx_port = DEFAULT_VIDEO_PORT;
  *rx_port = DEFAULT_VIDEO_PORT;

  while (1) {
    std::cout << "Enter video send port. Press enter for default ("
              << *tx_port << "):  ";
    std::getline(std::cin, str);
    port = atoi(str.c_str());

    if (port == 0) {
      // Default value.
      break;
    } else {
      // User selection.
      if (port <= 0 || port > 63556) {
        // Invalid selection.
        continue;
      } else {
        *tx_port = port;
        break;  // Move on to rx_port.
      }
    }
  }

  while (1) {
    std::cout << "Enter video receive port. Press enter for default ("
              << *rx_port << "):  ";
    std::getline(std::cin, str);
    port = atoi(str.c_str());

    if (port == 0) {
      // Default value
      return true;
    } else {
      // User selection.
      if (port <= 0 || port > 63556) {
        // Invalid selection.
        continue;
      } else {
        *rx_port = port;
        return true;
      }
    }
  }
  assert(false);
  return false;
}

// Audio settings functions.

bool GetAudioPorts(int* tx_port, int* rx_port) {
  int port = 0;
  std::string str;

  // set to default values.
  *tx_port = DEFAULT_AUDIO_PORT;
  *rx_port = DEFAULT_AUDIO_PORT;

  while (1) {
    std::cout << "Enter audio send port. Press enter for default ("
              << *tx_port << "):  ";
    std::getline(std::cin, str);
    port = atoi(str.c_str());

    if (port == 0) {
      // Default value.
      break;
    } else {
      // User selection.
      if (port <= 0 || port > 63556) {
        // Invalid selection.
        continue;
      } else {
        *tx_port = port;
        break;  // Move on to rx_port.
      }
    }
  }

  while (1) {
    std::cout << "Enter audio receive port. Press enter for default ("
              << *rx_port << "):  ";
    std::getline(std::cin, str);
    port = atoi(str.c_str());

    if (port == 0) {
      // Default value.
      return true;
    } else {
      // User selection.
      if (port <= 0 || port > 63556) {
        // Invalid selection.
        continue;
      } else {
        *rx_port = port;
        return true;
      }
    }
  }
  assert(false);
  return false;
}

bool GetAudioCodec(webrtc::VoECodec* voe_codec,
                   webrtc::CodecInst& audio_codec) {
  int error = 0;
  int number_of_errors = 0;
  int codec_selection = 0;
  std::string str;
  memset(&audio_codec, 0, sizeof(webrtc::CodecInst));

  while (1) {
    std::cout << std::endl;
    std::cout << "Available audio codecs:" << std::endl;
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
      std::cout << "   " << codec_idx + 1 << ". " << audio_codec.plname
                << " type:" << audio_codec.pltype
                << " freq:" << audio_codec.plfreq
                << " chan:" << audio_codec.channels
                << std::endl;
    }
    std::cout << std::endl;
    std::cout << "Choose audio codec. Press enter for default ("
              << DEFAULT_AUDIO_CODEC << "):  ";
    std::getline(std::cin, str);
    codec_selection = atoi(str.c_str());

    if (codec_selection == 0) {
      // Use default.
      error = voe_codec->GetCodec(default_codec_idx, audio_codec);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      return true;
    } else {
      // User selection.
      codec_selection = atoi(str.c_str()) - 1;
      error = voe_codec->GetCodec(codec_selection, audio_codec);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      if (error != 0) {
        std::cout << "ERROR: Code = " << error << " Invalid selection"
                  << std::endl;
        continue;
      }
      return true;
    }
  }
  assert(false);
  return false;
}

void PrintCallInformation(char* IP, char* video_capture_device_name,
                          char* video_capture_unique_id,
                          webrtc::VideoCodec video_codec,
                          int video_tx_port, int video_rx_port,
                          char* audio_capture_device_name,
                          char* audio_playbackDeviceName,
                          webrtc::CodecInst audio_codec,
                          int audio_tx_port, int audio_rx_port,
                          int protection_method) {
  std::string str;

  std::cout << "************************************************"
            << std::endl;
  std::cout << "The call has the following settings: " << std::endl;
  std::cout << "\tIP: " << IP << std::endl;
  std::cout << "\tVideo Capture Device: " << video_capture_device_name
            << std::endl;
  std::cout << "\t\tName: " << video_capture_device_name << std::endl;
  std::cout << "\t\tUniqueId: " << video_capture_unique_id << std::endl;
  PrintVideoCodec(video_codec);
  std::cout << "\t Video Tx Port: " << video_tx_port << std::endl;
  std::cout << "\t Video Rx Port: " << video_rx_port << std::endl;
  std::cout << "\t Video Protection Method: " << protection_method
            << std::endl;
  std::cout << "\tAudio Capture Device: " << audio_capture_device_name
            << std::endl;
  std::cout << "\tAudio Playback Device: " << audio_playbackDeviceName
            << std::endl;
  std::cout << "\tAudio Codec: " << std::endl;
  std::cout << "\t\tplname: " << audio_codec.plname << std::endl;
  std::cout << "\t\tpltype: " << static_cast<int>(audio_codec.pltype)
            << std::endl;
  std::cout << "\t Audio Tx Port: " << audio_tx_port << std::endl;
  std::cout << "\t Audio Rx Port: " << audio_rx_port << std::endl;
  std::cout << "************************************************"
            << std::endl;
}

bool SetVideoCodecType(webrtc::ViECodec* vie_codec,
                       webrtc::VideoCodec* video_codec) {
  int error = 0;
  int number_of_errors = 0;
  int codec_selection = 0;
  std::string str;
  memset(video_codec, 0, sizeof(webrtc::VideoCodec));

  bool exit_loop = false;
  while (!exit_loop) {
    std::cout << std::endl;
    std::cout << "Available video codecs:" << std::endl;
    int codec_idx = 0;
    int default_codec_idx = 0;
    // Print out all the codecs available to set Codec to.
    for (codec_idx = 0; codec_idx < vie_codec->NumberOfCodecs(); codec_idx++) {
      error = vie_codec->GetCodec(codec_idx, *video_codec);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      // Test for default codec index.
      if (strcmp(video_codec->plName, DEFAULT_VIDEO_CODEC) == 0) {
        default_codec_idx = codec_idx;
      }
      std::cout << "   " << codec_idx + 1 << ". " << video_codec->plName
                << std::endl;
    }
    std::cout << std::endl;
    std::cout << "Choose video codec. Press enter for default ("
              << DEFAULT_VIDEO_CODEC << "):  ";
    std::getline(std::cin, str);
    codec_selection = atoi(str.c_str());
    if (codec_selection == 0) {
      // Use default.
      error = vie_codec->GetCodec(default_codec_idx, *video_codec);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      exit_loop = true;
    } else {
      // User selection.
      codec_selection = atoi(str.c_str()) - 1;
      error = vie_codec->GetCodec(codec_selection, *video_codec);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      if (error != 0) {
        std::cout << "ERROR: Code=" << error << " Invalid selection"
                  << std::endl;
        continue;
      }
      exit_loop = true;
    }
  }
  if (video_codec->codecType == webrtc::kVideoCodecI420) {
    video_codec->width = 176;
    video_codec->height = 144;
  }
  return true;
}

bool SetVideoCodecResolution(webrtc::ViECodec* vie_codec,
                             webrtc::VideoCodec* video_codec) {
  std::string str;
  int size_option = 5;

  if (video_codec->codecType == webrtc::kVideoCodecVP8) {
    std::cout << std::endl;
    std::cout << "Available Common Resolutions : " << std::endl;
    std::cout << "  1. SQCIF (128X96) " << std::endl;
    std::cout << "  2. QQVGA (160X120) " << std::endl;
    std::cout << "  3. QCIF (176X144) " << std::endl;
    std::cout << "  4. CIF  (352X288) " << std::endl;
    std::cout << "  5. VGA  (640X480) " << std::endl;
    std::cout << "  6. WVGA (800x480) " << std::endl;
    std::cout << "  7. 4CIF (704X576) " << std::endl;
    std::cout << "  8. SVGA (800X600) " << std::endl;
    std::cout << "  9. HD   (1280X720) " << std::endl;
    std::cout << " 10. XGA  (1024x768) " << std::endl;
    std::cout << "Enter frame size option: " << std::endl;

    std::getline(std::cin, str);
    size_option = atoi(str.c_str());

    switch (size_option) {
      case 1:
        video_codec->width = 128;
        video_codec->height = 96;
        break;
      case 2:
        video_codec->width = 160;
        video_codec->height = 120;
        break;
      case 3:
        video_codec->width = 176;
        video_codec->height = 144;
        break;
      case 4:
        video_codec->width = 352;
        video_codec->height = 288;
        break;
      case 5:
        video_codec->width = 640;
        video_codec->height = 480;
        break;
      case 6:
        video_codec->width = 800;
        video_codec->height = 480;
        break;
      case 7:
        video_codec->width = 704;
        video_codec->height = 576;
        break;
      case 8:
        video_codec->width = 800;
        video_codec->height = 600;
        break;
      case 9:
        video_codec->width = 1280;
        video_codec->height = 720;
        break;
      case 10:
        video_codec->width = 1024;
        video_codec->height = 768;
        break;
    }
  } else {
    std::cout << "Can Only change codec size if it's VP8" << std::endl;
  }
  return true;
}

bool SetVideoCodecSize(webrtc::ViECodec* vie_codec,
                       webrtc::VideoCodec* video_codec) {
  if (video_codec->codecType == webrtc::kVideoCodecVP8) {
    std::string str;
    video_codec->width = DEFAULT_VIDEO_CODEC_WIDTH;
    video_codec->height = DEFAULT_VIDEO_CODEC_HEIGHT;
    std::cout << "Choose video width. Press enter for default ("
              << DEFAULT_VIDEO_CODEC_WIDTH << "):  ";
    std::getline(std::cin, str);
    int size_selection = atoi(str.c_str());
    if (size_selection != 0) {
      video_codec->width = size_selection;
    }
    std::cout << "Choose video height. Press enter for default ("
              << DEFAULT_VIDEO_CODEC_HEIGHT << "):  ";
    std::getline(std::cin, str);
    size_selection = atoi(str.c_str());
    if (size_selection != 0) {
      video_codec->height = size_selection;
    }
  } else {
    std::cout << "Can Only change codec size if it's VP8" << std::endl;
  }
  return true;
}

bool SetVideoCodecBitrate(webrtc::ViECodec* vie_codec,
                          webrtc::VideoCodec* video_codec) {
  std::string str;
  std::cout << std::endl;
  std::cout << "Choose start rate (in kbps). Press enter for default ("
            << DEFAULT_VIDEO_CODEC_BITRATE << "):  ";
  std::getline(std::cin, str);
  int start_rate = atoi(str.c_str());
  video_codec->startBitrate = DEFAULT_VIDEO_CODEC_BITRATE;
  if (start_rate != 0) {
    video_codec->startBitrate = start_rate;
  }
  return true;
}

bool SetVideoCodecMaxBitrate(webrtc::ViECodec* vie_codec,
                             webrtc::VideoCodec* video_codec) {
  std::string str;
  std::cout << std::endl;
  std::cout << "Choose max bitrate (in kbps). Press enter for default ("
            << DEFAULT_VIDEO_CODEC_MAX_BITRATE << "):  ";
  std::getline(std::cin, str);
  int max_rate = atoi(str.c_str());
  video_codec->maxBitrate = DEFAULT_VIDEO_CODEC_MAX_BITRATE;
  if (max_rate != 0) {
    video_codec->maxBitrate = max_rate;
  }
  return true;
}

bool SetVideoCodecMinBitrate(webrtc::ViECodec* vie_codec,
                             webrtc::VideoCodec* video_codec) {
  std::string str;
  std::cout << std::endl;
  std::cout << "Choose min bitrate (in fps). Press enter for default ("
            << DEFAULT_VIDEO_CODEC_MIN_BITRATE << "):  ";
  std::getline(std::cin, str);
  char min_bit_rate = atoi(str.c_str());
  video_codec->minBitrate = DEFAULT_VIDEO_CODEC_MIN_BITRATE;
  if (min_bit_rate != 0) {
    video_codec->minBitrate = min_bit_rate;
  }
  return true;
}

bool SetVideoCodecMaxFramerate(webrtc::ViECodec* vie_codec,
                               webrtc::VideoCodec* video_codec) {
  std::string str;
  std::cout << std::endl;
  std::cout << "Choose max framerate (in fps). Press enter for default ("
            << DEFAULT_VIDEO_CODEC_MAX_FRAMERATE << "):  ";
  std::getline(std::cin, str);
  char max_frame_rate = atoi(str.c_str());
  video_codec->maxFramerate = DEFAULT_VIDEO_CODEC_MAX_FRAMERATE;
  if (max_frame_rate != 0) {
    video_codec->maxFramerate = max_frame_rate;
  }
  return true;
}

bool SetVideoCodecTemporalLayer(webrtc::VideoCodec* video_codec) {
  if (video_codec->codecType == webrtc::kVideoCodecVP8) {
    std::string str;
    std::cout << std::endl;
    std::cout << "Choose number of temporal layers (1 to 4). "
              << "Press enter for default ("
              << DEFAULT_TEMPORAL_LAYER << "):  ";
    std::getline(std::cin, str);
    char num_temporal_layers = atoi(str.c_str());
    video_codec->codecSpecific.VP8.numberOfTemporalLayers =
        DEFAULT_TEMPORAL_LAYER;
    if (num_temporal_layers != 0) {
      video_codec->codecSpecific.VP8.numberOfTemporalLayers =
          num_temporal_layers;
    }
  }
  return true;
}

// GetVideoProtection only prints the prompt to get a number
// that SetVideoProtection method uses
// 0 = None
// 1 = FEC
// 2 = NACK
// 3 = NACK + FEC (aka Hybrid)
// Default = DEFAULT_VIDEO_PROTECTION METHOD
int GetVideoProtection() {
  int protection_method = DEFAULT_VIDEO_PROTECTION_METHOD;

  std::cout << "Available Video Protection Method." << std::endl;
  std::cout << "  0. None" << std::endl;
  std::cout << "  1. FEC" << std::endl;
  std::cout << "  2. NACK" << std::endl;
  std::cout << "  3. NACK+FEC" << std::endl;
  std::cout << "Enter Video Protection Method. "
            << "Press enter for default (" << protection_method << "):"
            << std::endl;
  std::string method;
  std::getline(std::cin, method);
  protection_method = atoi(method.c_str());

  return protection_method;
}

bool SetVideoProtection(webrtc::ViECodec* vie_codec,
                        webrtc::ViERTP_RTCP* vie_rtp_rtcp,
                        int video_channel, int protection_method) {
  int error = 0;
  int number_of_errors = 0;
  webrtc::VideoCodec video_codec;

  memset(&video_codec, 0, sizeof(webrtc::VideoCodec));

  // Set all video protection to false initially
  error = vie_rtp_rtcp->SetHybridNACKFECStatus(video_channel, false,
                                                   VCM_RED_PAYLOAD_TYPE,
                                                   VCM_ULPFEC_PAYLOAD_TYPE);
  number_of_errors += ViETest::TestError(error == 0,
                                         "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = vie_rtp_rtcp->SetFECStatus(video_channel, false,
                                     VCM_RED_PAYLOAD_TYPE,
                                     VCM_ULPFEC_PAYLOAD_TYPE);
  number_of_errors += ViETest::TestError(error == 0,
                                         "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = vie_rtp_rtcp->SetNACKStatus(video_channel, false);
  number_of_errors += ViETest::TestError(error == 0,
                                         "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  // Set video protection for FEC, NACK or Hybrid.
  switch (protection_method) {
    case 0:  // None.
      // No protection selected, all protection already at false.
      std::cout << "Call using None protection Method"
                << std::endl;
      break;
    case 1:  // FEC only.
      std::cout << "Call using FEC protection Method"
                << std::endl;
      error = vie_rtp_rtcp->SetFECStatus(video_channel, true,
                                         VCM_RED_PAYLOAD_TYPE,
                                         VCM_ULPFEC_PAYLOAD_TYPE);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      break;
    case 2:  // NACK only.
      std::cout << "Call using NACK protection Method"
                << std::endl;
      error = vie_rtp_rtcp->SetNACKStatus(video_channel, true);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      break;
    case 3:  // Hybrid NACK and FEC.
      std::cout << "Call using Hybrid NACK and FEC protection Method"
                << std::endl;
      error = vie_rtp_rtcp->SetHybridNACKFECStatus(video_channel, true,
                                                   VCM_RED_PAYLOAD_TYPE,
                                                   VCM_ULPFEC_PAYLOAD_TYPE);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      break;
  }

  // Set receive codecs for FEC and hybrid NACK/FEC.
  if (protection_method == 1 || protection_method == 3) {
    // RED.
    error = vie_codec->GetCodec(vie_codec->NumberOfCodecs() - 2,
                                video_codec);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
    video_codec.plType = VCM_RED_PAYLOAD_TYPE;
    error = vie_codec->SetReceiveCodec(video_channel, video_codec);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
    std::cout << "RED Codec Information:" << std::endl;
    PrintVideoCodec(video_codec);
    // ULPFEC.
    error = vie_codec->GetCodec(vie_codec->NumberOfCodecs() - 1,
                                video_codec);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
    video_codec.plType = VCM_ULPFEC_PAYLOAD_TYPE;
    error = vie_codec->SetReceiveCodec(video_channel, video_codec);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
    std::cout << "ULPFEC Codec Information:" << std::endl;
    PrintVideoCodec(video_codec);
  }

  return true;
}

bool GetBitrateSignaling() {
  std::cout << std::endl;
  std::cout << "Available bitrate signaling methods." << std::endl;
  std::cout << "  0. REMB" << std::endl;
  std::cout << "  1. TMMBR" << std::endl;
  std::cout << "Enter bitrate signaling methods. "
            << "Press enter for default (REMB): " << std::endl;
  std::string method;
  std::getline(std::cin, method);
  if (atoi(method.c_str()) == 1) {
    return false;
  }
  return true;
}

void PrintRTCCPStatistics(webrtc::ViERTP_RTCP* vie_rtp_rtcp,
                          int video_channel,
                          StatisticsType stat_type) {
  int error = 0;
  int number_of_errors = 0;
  uint16_t fraction_lost = 0;
  unsigned int cumulative_lost = 0;
  unsigned int extended_max = 0;
  unsigned int jitter = 0;
  int rtt_ms = 0;

  switch (stat_type) {
    case kReceivedStatistic:
      std::cout << "RTCP Received statistics"
                << std::endl;
      // Get and print the Received RTCP Statistics
      error = vie_rtp_rtcp->GetReceivedRTCPStatistics(video_channel,
                                                      fraction_lost,
                                                      cumulative_lost,
                                                      extended_max,
                                                      jitter, rtt_ms);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      break;
    case kSendStatistic:
      std::cout << "RTCP Sent statistics"
                << std::endl;
      // Get and print the Sent RTCP Statistics
      error = vie_rtp_rtcp->GetSentRTCPStatistics(video_channel, fraction_lost,
                                                  cumulative_lost, extended_max,
                                                  jitter, rtt_ms);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      break;
  }
  std::cout << "\tRTCP fraction of lost packets: "
            << fraction_lost << std::endl;
  std::cout << "\tRTCP cumulative number of lost packets: "
            << cumulative_lost << std::endl;
  std::cout << "\tRTCP max received sequence number "
            << extended_max << std::endl;
  std::cout << "\tRTCP jitter: "
            << jitter << std::endl;
  std::cout << "\tRTCP round trip (ms): "
            << rtt_ms << std::endl;
}

void PrintRTPStatistics(webrtc::ViERTP_RTCP* vie_rtp_rtcp,
                        int video_channel) {
  int error = 0;
  int number_of_errors = 0;
  unsigned int bytes_sent = 0;
  unsigned int packets_sent = 0;
  unsigned int bytes_received = 0;
  unsigned int packets_received = 0;

  std::cout << "RTP statistics"
            << std::endl;

  // Get and print the RTP Statistics
  error = vie_rtp_rtcp->GetRTPStatistics(video_channel, bytes_sent,
                                         packets_sent, bytes_received,
                                         packets_received);
  number_of_errors += ViETest::TestError(error == 0,
                                         "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  std::cout << "\tRTP bytes sent: "
            << bytes_sent << std::endl;
  std::cout << "\tRTP packets sent: "
            << packets_sent << std::endl;
  std::cout << "\tRTP bytes received: "
            << bytes_received << std::endl;
  std::cout << "\tRTP packets received: "
            << packets_received << std::endl;
}

void PrintBandwidthUsage(webrtc::ViERTP_RTCP* vie_rtp_rtcp,
                         int video_channel) {
  int error = 0;
  int number_of_errors = 0;
  unsigned int total_bitrate_sent = 0;
  unsigned int video_bitrate_sent = 0;
  unsigned int fec_bitrate_sent = 0;
  unsigned int nack_bitrate_sent = 0;
  double percentage_fec = 0;
  double percentage_nack = 0;

  std::cout << "Bandwidth Usage" << std::endl;

  // Get and print Bandwidth usage
  error = vie_rtp_rtcp->GetBandwidthUsage(video_channel, total_bitrate_sent,
                                          video_bitrate_sent, fec_bitrate_sent,
                                          nack_bitrate_sent);
  number_of_errors += ViETest::TestError(error == 0,
                                         "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  std::cout << "\tTotal bitrate sent (Kbit/s): "
            << total_bitrate_sent << std::endl;
  std::cout << "\tVideo bitrate sent (Kbit/s): "
            << video_bitrate_sent << std::endl;
  std::cout << "\tFEC bitrate sent (Kbit/s): "
            << fec_bitrate_sent << std::endl;
  percentage_fec =
      (static_cast<double>(fec_bitrate_sent) /
      static_cast<double>(total_bitrate_sent)) * 100;
  std::cout << "\tPercentage FEC bitrate sent from total bitrate: "
            << percentage_fec << std::endl;
  std::cout << "\tNACK bitrate sent (Kbit/s): "
            << nack_bitrate_sent << std::endl;
  percentage_nack =
      (static_cast<double>(nack_bitrate_sent) /
      static_cast<double>(total_bitrate_sent)) * 100;
  std::cout << "\tPercentage NACK bitrate sent from total bitrate: "
            << percentage_nack << std::endl;
}

void PrintCodecStatistics(webrtc::ViECodec* vie_codec,
                          int video_channel,
                          StatisticsType stat_type) {
  int error = 0;
  int number_of_errors = 0;
  unsigned int key_frames = 0;
  unsigned int delta_frames = 0;
  switch (stat_type) {
    case kReceivedStatistic:
      std::cout << "Codec Receive statistics"
                << std::endl;
      // Get and print the Receive Codec Statistics
      error = vie_codec->GetReceiveCodecStastistics(video_channel, key_frames,
                                                    delta_frames);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      break;
    case kSendStatistic:
      std::cout << "Codec Send statistics"
                << std::endl;
      // Get and print the Send Codec Statistics
      error = vie_codec->GetSendCodecStastistics(video_channel, key_frames,
                                                 delta_frames);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      break;
  }
  std::cout << "\tNumber of encoded key frames: "
            << key_frames << std::endl;
  std::cout << "\tNumber of encoded delta frames: "
            << delta_frames << std::endl;
}

void PrintGetDiscardedPackets(webrtc::ViECodec* vie_codec, int video_channel) {
  std::cout << "Discarded Packets" << std::endl;
  int discarded_packets = 0;
  discarded_packets = vie_codec->GetDiscardedPackets(video_channel);
  std::cout << "\tNumber of discarded packets: "
            << discarded_packets << std::endl;
}

void PrintVideoStreamInformation(webrtc::ViECodec* vie_codec,
                                 int video_channel) {
  webrtc::VideoCodec outgoing_codec;
  webrtc::VideoCodec incoming_codec;

  memset(&outgoing_codec, 0, sizeof(webrtc::VideoCodec));
  memset(&incoming_codec, 0, sizeof(webrtc::VideoCodec));

  vie_codec->GetSendCodec(video_channel, outgoing_codec);
  vie_codec->GetReceiveCodec(video_channel, incoming_codec);

  std::cout << "************************************************"
            << std::endl;
  std::cout << "ChannelId: " << video_channel << std::endl;
  std::cout << "Outgoing Stream information:" << std::endl;
  PrintVideoCodec(outgoing_codec);
  std::cout << "Incoming Stream information:" << std::endl;
  PrintVideoCodec(incoming_codec);
  std::cout << "************************************************"
            << std::endl;
}

void PrintVideoCodec(webrtc::VideoCodec video_codec) {
  std::cout << "\t\tplName: " << video_codec.plName << std::endl;
  std::cout << "\t\tplType: " << static_cast<int>(video_codec.plType)
            << std::endl;
  std::cout << "\t\twidth: " << video_codec.width << std::endl;
  std::cout << "\t\theight: " << video_codec.height << std::endl;
  std::cout << "\t\tstartBitrate: " << video_codec.startBitrate
            << std::endl;
  std::cout << "\t\tminBitrate: " << video_codec.minBitrate
            << std::endl;
  std::cout << "\t\tmaxBitrate: " << video_codec.maxBitrate
            << std::endl;
  std::cout << "\t\tmaxFramerate: "
            << static_cast<int>(video_codec.maxFramerate) << std::endl;
  if (video_codec.codecType == webrtc::kVideoCodecVP8) {
    int number_of_layers =
        static_cast<int>(video_codec.codecSpecific.VP8.numberOfTemporalLayers);
    std::cout << "\t\tVP8 Temporal Layer: " << number_of_layers << std::endl;
  }
}
