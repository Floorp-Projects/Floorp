/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>

#include <algorithm>

#include "gflags/gflags.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/test/channel_transport/include/channel_transport.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest.h"
#include "webrtc/video_engine/test/auto_test/interface/vie_autotest_defines.h"
#include "webrtc/video_engine/test/auto_test/primitives/choice_helpers.h"
#include "webrtc/video_engine/test/auto_test/primitives/general_primitives.h"
#include "webrtc/video_engine/test/auto_test/primitives/input_helpers.h"
#include "webrtc/video_engine/test/libvietest/include/vie_to_file_renderer.h"
#include "webrtc/voice_engine/include/voe_network.h"

#define VCM_RED_PAYLOAD_TYPE                            96
#define VCM_ULPFEC_PAYLOAD_TYPE                         97
#define DEFAULT_SEND_IP                                 "127.0.0.1"
#define DEFAULT_VIDEO_PORT                              "11111"
#define DEFAULT_VIDEO_CODEC                             "VP8"
#define DEFAULT_VIDEO_CODEC_WIDTH                       "640"
#define DEFAULT_VIDEO_CODEC_HEIGHT                      "480"
#define DEFAULT_VIDEO_CODEC_BITRATE                     "300"
#define DEFAULT_VIDEO_CODEC_MIN_BITRATE                 "100"
#define DEFAULT_VIDEO_CODEC_MAX_BITRATE                 "1000"
#define DEFAULT_AUDIO_PORT                              "11113"
#define DEFAULT_AUDIO_CODEC                             "ISAC"
#define DEFAULT_VIDEO_CODEC_MAX_FRAMERATE               "30"
#define DEFAULT_VIDEO_PROTECTION_METHOD                 "None"
#define DEFAULT_TEMPORAL_LAYER                          "0"
#define DEFAULT_BUFFERING_DELAY_MS                      "0"

DEFINE_string(render_custom_call_remote_to, "", "Specify to render the remote "
    "stream of a custom call to the provided filename instead of showing it in "
    "window 2. The file will end up in the default output directory (out/).");

enum StatisticsType {
  kSendStatistic,
  kReceivedStatistic
};

enum VideoProtectionMethod {
  kProtectionMethodNone = 1,
  kProtectionMethodFecOnly,
  kProtectionMethodNackOnly,
  kProtectionMethodHybridNackAndFec,
};

using webrtc::FromChoices;
using webrtc::TypedInput;

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

  void SuspendChange(int video_channel, bool is_suspended) override {
    std::cout << "SuspendChange: " << is_suspended << std::endl;
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

  virtual void DecoderTiming(int decode_ms,
                             int max_decode_ms,
                             int current_delay_ms,
                             int target_delay_ms,
                             int jitter_buffer_ms,
                             int min_playout_delay_ms,
                             int render_delay_ms) {
    std::cout << "Decoder timing: DecodeMS: " << decode_ms
              << ", MaxDecodeMS: " << max_decode_ms
              << ", CurrentDelayMS: " << current_delay_ms
              << ", TargetDelayMS: " << target_delay_ms
              << ", JitterBufferMS: " << jitter_buffer_ms
              << ", MinPlayoutDelayMS: " << min_playout_delay_ms
              << ", RenderDelayMS: " << render_delay_ms;
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
std::string GetIPAddress();
bool ValidateIP(std::string i_str);

// The following are Print to stdout functions.
void PrintCallInformation(const char* IP,
                          const char* video_capture_device_name,
                          const char* video_capture_unique_id,
                          webrtc::VideoCodec video_codec,
                          int video_tx_port,
                          int video_rx_port,
                          const char* audio_capture_device_name,
                          const char* audio_playbackDeviceName,
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
void GetVideoPorts(int* tx_port, int* rx_port);
void SetVideoCodecType(webrtc::ViECodec* vie_codec,
                       webrtc::VideoCodec* video_codec);
void SetVideoCodecResolution(webrtc::VideoCodec* video_codec);
void SetVideoCodecSize(webrtc::VideoCodec* video_codec);
void SetVideoCodecBitrate(webrtc::VideoCodec* video_codec);
void SetVideoCodecMinBitrate(webrtc::VideoCodec* video_codec);
void SetVideoCodecMaxBitrate(webrtc::VideoCodec* video_codec);
void SetVideoCodecMaxFramerate(webrtc::VideoCodec* video_codec);
void SetVideoCodecTemporalLayer(webrtc::VideoCodec* video_codec);
VideoProtectionMethod GetVideoProtection();
bool SetVideoProtection(webrtc::ViECodec* vie_codec,
                        webrtc::ViERTP_RTCP* vie_rtp_rtcp,
                        int video_channel,
                        VideoProtectionMethod protection_method);
bool GetBitrateSignaling();
int GetBufferingDelay();

// The following are audio helper functions.
bool GetAudioDevices(webrtc::VoEBase* voe_base,
                     webrtc::VoEHardware* voe_hardware,
                     char* recording_device_name, int& recording_device_index,
                     char* playback_device_name, int& playback_device_index);
bool GetAudioDevices(webrtc::VoEBase* voe_base,
                     webrtc::VoEHardware* voe_hardware,
                     int& recording_device_index, int& playback_device_index);
void GetAudioPorts(int* tx_port, int* rx_port);
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

  webrtc::VoENetwork* voe_network=
      webrtc::VoENetwork::GetInterface(voe);
  number_of_errors += ViETest::TestError(voe_network != NULL,
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

  bool start_call = false;
  std::string ip_address;
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
  VideoProtectionMethod protection_method = kProtectionMethodNone;
  int buffer_delay_ms = 0;
  bool is_image_scale_enabled = false;
  bool remb = true;
  rtc::scoped_ptr<webrtc::test::VideoChannelTransport> video_channel_transport;
  rtc::scoped_ptr<webrtc::test::VoiceChannelTransport> voice_channel_transport;

  while (!start_call) {
    // Get the IP address to use from call.
    ip_address = GetIPAddress();

    // Get the video device to use for call.
    memset(device_name, 0, KMaxUniqueIdLength);
    memset(unique_id, 0, KMaxUniqueIdLength);
    if (!GetVideoDevice(vie_base, vie_capture, device_name, unique_id))
      return number_of_errors;

    // Get and set the video ports for the call.
    video_tx_port = 0;
    video_rx_port = 0;
    GetVideoPorts(&video_tx_port, &video_rx_port);

    // Get and set the video codec parameters for the call.
    memset(&video_send_codec, 0, sizeof(webrtc::VideoCodec));
    SetVideoCodecType(vie_codec, &video_send_codec);
    SetVideoCodecSize(&video_send_codec);
    SetVideoCodecBitrate(&video_send_codec);
    SetVideoCodecMinBitrate(&video_send_codec);
    SetVideoCodecMaxBitrate(&video_send_codec);
    SetVideoCodecMaxFramerate(&video_send_codec);
    SetVideoCodecTemporalLayer(&video_send_codec);
    remb = GetBitrateSignaling();

    // Get the video protection method for the call.
    protection_method = GetVideoProtection();

    // Get the call mode (Real-Time/Buffered).
    buffer_delay_ms = GetBufferingDelay();

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
    PrintCallInformation(ip_address.c_str(), device_name, unique_id,
                         video_send_codec, video_tx_port, video_rx_port,
                         audio_capture_device_name, audio_playbackDeviceName,
                         audio_codec, audio_tx_port, audio_rx_port,
                         protection_method);

    printf("\n");
    int selection =
        FromChoices("Ready to start:",
                    "Start the call\n"
                    "Reconfigure call settings\n")
                        .WithDefault("Start the call").Choose();
    start_call = (selection == 1);
  }
  /// **************************************************************
  // Begin create/initialize WebRTC Video Engine for testing.
  /// **************************************************************
  if (start_call == true) {
    // Configure audio channel first.
    audio_channel = voe_base->CreateChannel();

    voice_channel_transport.reset(
        new webrtc::test::VoiceChannelTransport(voe_network, audio_channel));

    error = voice_channel_transport->SetSendDestination(ip_address.c_str(),
                                                        audio_tx_port);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = voice_channel_transport->SetLocalReceiver(audio_rx_port);
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


    ViEToFileRenderer file_renderer;
    if (FLAGS_render_custom_call_remote_to == "") {
      error = vie_renderer->AddRenderer(video_channel, _window2, 1, 0.0, 0.0,
                                        1.0, 1.0);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    } else {
      std::string output_path = ViETest::GetResultOutputPath();
      std::string filename = FLAGS_render_custom_call_remote_to;
      ViETest::Log("Rendering remote stream to %s: you will not see any output "
          "in the second window.", (output_path + filename).c_str());

      file_renderer.PrepareForRendering(output_path, filename);
      RenderToFile(vie_renderer, video_channel, &file_renderer);
    }

    video_channel_transport.reset(
        new webrtc::test::VideoChannelTransport(vie_network, video_channel));

    error = video_channel_transport->SetSendDestination(ip_address.c_str(),
                                                        video_tx_port);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    error = video_channel_transport->SetLocalReceiver(video_rx_port);
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

    // Set the call mode (conferencing/buffering)
    error = vie_rtp_rtcp->SetSenderBufferingMode(video_channel,
                                                    buffer_delay_ms);
    number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
    error = vie_rtp_rtcp->SetReceiverBufferingMode(video_channel,
                                                      buffer_delay_ms);
    number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
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

    ViEAutotestEncoderObserver* codec_encoder_observer = NULL;
    ViEAutotestDecoderObserver* codec_decoder_observer = NULL;

    //  Engine ready, wait for input.

    // Call started.
    std::cout << std::endl;
    std::cout << "Custom call started" << std::endl;

    // Modify call or stop call.
    printf("\n");
    int selection = FromChoices(
        "And now?",
        "Stop the call\n"
        "Modify the call\n"
        "Keep the call running indefinitely\n")
            .WithDefault("Keep the call running indefinitely").Choose();
    if (selection == 3) {
        AutoTestSleep(std::numeric_limits<int>::max());
    }

    while (selection == 2) {
      // Keep on modifying the call until user stops the call.
      int modify_selection = FromChoices(
          "Modify the call:",
          "Stop call\n"
          "Change Video Send Codec\n"
          "Change Video Send Size by Common Resolutions\n"
          "Change Video Send Size by Width & Height\n"
          "Change Video Capture Device\n"
          "Change Video Protection Method\n"
          "Toggle Encoder Observer\n"
          "Toggle Decoder Observer\n"
          "Print Call Information\n"
          "Print Call Statistics\n"
          "Toggle Image Scaling (Warning: high CPU usage when enabled)\n")
              .WithDefault("Stop call")
              .Choose();

      switch (modify_selection) {
        case 1:
          selection = 1;
          break;
        case 2:
          // Change video codec.
          SetVideoCodecType(vie_codec, &video_send_codec);
          SetVideoCodecSize(&video_send_codec);
          SetVideoCodecBitrate(&video_send_codec);
          SetVideoCodecMinBitrate(&video_send_codec);
          SetVideoCodecMaxBitrate(&video_send_codec);
          SetVideoCodecMaxFramerate(&video_send_codec);
          SetVideoCodecTemporalLayer(&video_send_codec);
          PrintCallInformation(ip_address.c_str(), device_name,
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
          break;
        case 3:
          // Change Video codec size by common resolution.
          SetVideoCodecResolution(&video_send_codec);
          PrintCallInformation(ip_address.c_str(), device_name,
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
          break;
        case 4:
          // Change video codec by size height and width.
          SetVideoCodecSize(&video_send_codec);
          PrintCallInformation(ip_address.c_str(), device_name,
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
          break;
        case 5:
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
          if (!GetVideoDevice(vie_base, vie_capture, device_name, unique_id))
            return number_of_errors;
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

          assert(FLAGS_render_custom_call_remote_to == "" &&
                 "Not implemented to change video capture device when "
                 "rendering to file!");

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
          break;
        case 6:
          // Change the Video Protection.
          protection_method = GetVideoProtection();
          SetVideoProtection(vie_codec, vie_rtp_rtcp,
                             video_channel, protection_method);
          break;
        case 7:
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
          break;
        case 8:
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
          break;
        case 9:
          // Print Call information..
          PrintCallInformation(ip_address.c_str(), device_name,
                               unique_id, video_send_codec,
                               video_tx_port, video_rx_port,
                               audio_capture_device_name,
                               audio_playbackDeviceName,
                               audio_codec, audio_tx_port,
                               audio_rx_port, protection_method);
          PrintVideoStreamInformation(vie_codec,
                                      video_channel);
          break;
        case 10:
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
          break;
        case 11:
          is_image_scale_enabled = !is_image_scale_enabled;
          vie_codec->SetImageScaleStatus(video_channel, is_image_scale_enabled);
          if (is_image_scale_enabled) {
            std::cout << "Image Scale is now enabled" << std::endl;
          } else {
            std::cout << "Image Scale is now disabled" << std::endl;
          }
          break;
        default:
          assert(false);
          break;
      }
    }

    if (FLAGS_render_custom_call_remote_to != "")
      file_renderer.StopRendering();

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

    voice_channel_transport.reset(NULL);

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

    video_channel_transport.reset(NULL);

    error = vie_base->DeleteChannel(video_channel);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    int remaining_interfaces = 0;
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

  const unsigned int KMaxDeviceNameLength = 128;
  const unsigned int KMaxUniqueIdLength = 256;
  char device_name[KMaxDeviceNameLength];
  char unique_id[KMaxUniqueIdLength];

  if (vie_capture->NumberOfCaptureDevices() == 0) {
    printf("You have no capture devices plugged into your system.\n");
    return false;
  }

  std::string capture_choices;
  std::string first_device;
  for (int i = 0; i < vie_capture->NumberOfCaptureDevices(); i++) {
    memset(device_name, 0, KMaxDeviceNameLength);
    memset(unique_id, 0, KMaxUniqueIdLength);

    error = vie_capture->GetCaptureDevice(i, device_name,
        KMaxDeviceNameLength,
        unique_id,
        KMaxUniqueIdLength);
    number_of_errors += ViETest::TestError(error == 0,
        "ERROR: %s at line %d",
        __FUNCTION__, __LINE__);
    const int kCaptureLineLength =
        KMaxDeviceNameLength + KMaxUniqueIdLength + 8;
    char capture_line[kCaptureLineLength];
    sprintf(capture_line, "%s (%s)", device_name, unique_id);
    capture_choices += capture_line;
    capture_choices += "\n";
    if (first_device.empty())
      first_device = capture_line;
  }

  int choice = FromChoices("Available Video Capture Devices", capture_choices)
      .WithDefault(first_device)
      .Choose();

  error = vie_capture->GetCaptureDevice(
      choice - 1, device_name, KMaxDeviceNameLength, unique_id,
      KMaxUniqueIdLength);
  number_of_errors += ViETest::TestError(error == 0,
      "ERROR: %s at line %d",
      __FUNCTION__, __LINE__);
  strcpy(capture_device_unique_id, unique_id);
  strcpy(capture_device_name, device_name);
  return true;
}

bool GetAudioDevices(webrtc::VoEBase* voe_base,
                     webrtc::VoEHardware* voe_hardware,
                     char* recording_device_name,
                     int& recording_device_index,
                     char* playback_device_name,
                     int& playback_device_index) {
  int error = 0;
  int number_of_errors = 0;

  const unsigned int KMaxDeviceNameLength = 128;
  const unsigned int KMaxUniqueIdLength = 128;
  char recording_device_unique_name[KMaxDeviceNameLength];
  char playback_device_unique_name[KMaxUniqueIdLength];

  int number_of_recording_devices = -1;
  error = voe_hardware->GetNumOfRecordingDevices(number_of_recording_devices);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

  recording_device_index = -1;
  playback_device_index = -1;

  std::string device_choices;
  std::string default_recording_line;
  for (int i = 0; i < number_of_recording_devices; ++i) {
    error = voe_hardware->GetRecordingDeviceName(
        i, recording_device_name, recording_device_unique_name);
    number_of_errors += ViETest::TestError(error == 0,
        "ERROR: %s at line %d",
        __FUNCTION__, __LINE__);

    device_choices += recording_device_name;
    device_choices += "\n";
    if (default_recording_line.empty())
      default_recording_line = recording_device_name;
  }

  int choice = FromChoices("Available audio capture devices:", device_choices)
      .WithDefault(default_recording_line)
      .Choose();

  recording_device_index = choice - 1;
  error = voe_hardware->GetRecordingDeviceName(
      recording_device_index, recording_device_name,
      recording_device_unique_name);
  number_of_errors += ViETest::TestError(
      error == 0, "ERROR: %s at line %d", __FUNCTION__, __LINE__);

  int number_of_playback_devices = -1;
  error = voe_hardware->GetNumOfPlayoutDevices(number_of_playback_devices);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);


  std::string playback_choices;
  std::string default_playback_line;
  for (int i = 0; i < number_of_playback_devices; i++) {
    error = voe_hardware->GetPlayoutDeviceName(i,
        playback_device_name,
        playback_device_unique_name);
    number_of_errors += ViETest::TestError(
        error == 0, "ERROR: %s at line %d", __FUNCTION__, __LINE__);
    playback_choices += playback_device_name;
    playback_choices += "\n";
    if (default_playback_line.empty())
      default_playback_line = playback_device_name;
  }

  choice = FromChoices("Available audio playout devices:", playback_choices)
      .WithDefault(default_playback_line)
      .Choose();

  playback_device_index = choice - 1;
  error = voe_hardware->GetPlayoutDeviceName(playback_device_index,
      playback_device_name,
      playback_device_unique_name);
  number_of_errors += ViETest::TestError(error == 0,
      "ERROR: %s at line %d",
      __FUNCTION__, __LINE__);
  return true;
}

// General helper functions.

std::string GetIPAddress() {
  class IpValidator : public webrtc::InputValidator {
   public:
    bool InputOk(const std::string& input) const {
      // Just check quickly that it's on the form x.y.z.w
      return std::count(input.begin(), input.end(), '.') == 3;
    }
  };
  return TypedInput("Enter destination IP.")
      .WithDefault(DEFAULT_SEND_IP)
      .WithInputValidator(new IpValidator())
      .AskForInput();
}

// Video settings functions.

void GetVideoPorts(int* tx_port, int* rx_port) {
  std::string tx_input = TypedInput("Enter video send port.")
      .WithDefault(DEFAULT_VIDEO_PORT)
      .WithInputValidator(new webrtc::IntegerWithinRangeValidator(1, 65536))
      .AskForInput();
  *tx_port = atoi(tx_input.c_str());

  std::string rx_input = TypedInput("Enter video receive port.")
      .WithDefault(DEFAULT_VIDEO_PORT)
      .WithInputValidator(new webrtc::IntegerWithinRangeValidator(1, 65536))
      .AskForInput();
  *rx_port = atoi(rx_input.c_str());
}

// Audio settings functions.

void GetAudioPorts(int* tx_port, int* rx_port) {
  std::string tx_input = TypedInput("Enter audio send port.")
      .WithDefault(DEFAULT_AUDIO_PORT)
      .WithInputValidator(new webrtc::IntegerWithinRangeValidator(1, 65536))
      .AskForInput();
  *tx_port = atoi(tx_input.c_str());

  std::string rx_input = TypedInput("Enter audio receive port.")
      .WithDefault(DEFAULT_AUDIO_PORT)
      .WithInputValidator(new webrtc::IntegerWithinRangeValidator(1, 65536))
      .AskForInput();
  *rx_port = atoi(rx_input.c_str());
}

bool GetAudioCodec(webrtc::VoECodec* voe_codec,
                   webrtc::CodecInst& audio_codec) {
  int error = 0;
  memset(&audio_codec, 0, sizeof(webrtc::CodecInst));

  std::string default_codec_line;
  std::string codec_choices;
  for (int codec_idx = 0; codec_idx < voe_codec->NumOfCodecs(); codec_idx++) {
    error = voe_codec->GetCodec(codec_idx, audio_codec);
    ViETest::TestError(error == 0,
        "ERROR: %s at line %d",
        __FUNCTION__, __LINE__);

    char codec_line[128];
    sprintf(codec_line, "%s type: %d freq: %d chan: %d",
        audio_codec.plname, audio_codec.pltype, audio_codec.plfreq,
        audio_codec.channels);
    codec_choices += codec_line;
    codec_choices += "\n";

    if (strcmp(audio_codec.plname, DEFAULT_AUDIO_CODEC) == 0) {
      default_codec_line = codec_line;
    }
  }
  assert(!default_codec_line.empty() && "Default codec doesn't exist.");

  int codec_selection = FromChoices("Available Audio Codecs:", codec_choices)
            .WithDefault(default_codec_line)
            .Choose();

  error = voe_codec->GetCodec(codec_selection - 1, audio_codec);
  ViETest::TestError(error == 0,
      "ERROR: %s at line %d",
      __FUNCTION__, __LINE__);
  return true;
}

void PrintCallInformation(const char* IP, const char* video_capture_device_name,
                          const char* video_capture_unique_id,
                          webrtc::VideoCodec video_codec,
                          int video_tx_port, int video_rx_port,
                          const char* audio_capture_device_name,
                          const char* audio_playbackDeviceName,
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
  std::cout << "\t Video Protection Method (NOTE: Starts at 1 now): "
            << protection_method  << std::endl;
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

void SetVideoCodecType(webrtc::ViECodec* vie_codec,
                       webrtc::VideoCodec* video_codec) {
  int error = 0;
  int number_of_errors = 0;
  memset(video_codec, 0, sizeof(webrtc::VideoCodec));

  std::string codec_choices;
  std::string default_codec_line;
  for (int i = 0; i < vie_codec->NumberOfCodecs(); i++) {
    error = vie_codec->GetCodec(i, *video_codec);
    number_of_errors += ViETest::TestError(
        error == 0, "ERROR: %s at line %d", __FUNCTION__, __LINE__);

    codec_choices += video_codec->plName;
    codec_choices += "\n";
    if (strcmp(video_codec->plName, DEFAULT_VIDEO_CODEC) == 0)
      default_codec_line = video_codec->plName;
  }
  assert(!default_codec_line.empty() && "Default does not exist.");

  int choice = FromChoices("Available Video Codecs", codec_choices)
      .WithDefault(default_codec_line)
      .Choose();
  error = vie_codec->GetCodec(choice - 1, *video_codec);
  number_of_errors += ViETest::TestError(
      error == 0, "ERROR: %s at line %d", __FUNCTION__, __LINE__);

  if (video_codec->codecType == webrtc::kVideoCodecI420) {
    video_codec->width = 176;
    video_codec->height = 144;
  }
}

void SetVideoCodecResolution(webrtc::VideoCodec* video_codec) {
  if (video_codec->codecType != webrtc::kVideoCodecVP8) {
    printf("Can only change codec size if it's VP8\n");
    return;
  }

  int choice = FromChoices(
      "Available Common Resolutions:",
      "SQCIF (128X96)\n"
      "QQVGA (160X120)\n"
      "QCIF  (176X144)\n"
      "CIF   (352X288)\n"
      "VGA   (640X480)\n"
      "WVGA  (800x480)\n"
      "4CIF  (704X576)\n"
      "SVGA  (800X600)\n"
      "HD    (1280X720)\n"
      "XGA   (1024x768)\n")
          .Choose();

  switch (choice) {
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
}

void SetVideoCodecSize(webrtc::VideoCodec* video_codec) {
  if (video_codec->codecType != webrtc::kVideoCodecVP8) {
    printf("Can only change codec size if it's VP8\n");
    return;
  }

  std::string input = TypedInput("Choose video width.")
      .WithDefault(DEFAULT_VIDEO_CODEC_WIDTH)
      .WithInputValidator(new webrtc::IntegerWithinRangeValidator(1, INT_MAX))
      .AskForInput();
  video_codec->width = atoi(input.c_str());

  input = TypedInput("Choose video height.")
      .WithDefault(DEFAULT_VIDEO_CODEC_HEIGHT)
      .WithInputValidator(new webrtc::IntegerWithinRangeValidator(1, INT_MAX))
      .AskForInput();
  video_codec->height = atoi(input.c_str());
}

void SetVideoCodecBitrate(webrtc::VideoCodec* video_codec) {
  std::string input = TypedInput("Choose start rate (in kbps).")
      .WithDefault(DEFAULT_VIDEO_CODEC_BITRATE)
      .WithInputValidator(new webrtc::IntegerWithinRangeValidator(1, INT_MAX))
      .AskForInput();

  video_codec->startBitrate = atoi(input.c_str());
}

void SetVideoCodecMaxBitrate(webrtc::VideoCodec* video_codec) {
  std::string input = TypedInput("Choose max bitrate (in kbps).")
      .WithDefault(DEFAULT_VIDEO_CODEC_MAX_BITRATE)
      .WithInputValidator(new webrtc::IntegerWithinRangeValidator(1, INT_MAX))
      .AskForInput();

  video_codec->maxBitrate = atoi(input.c_str());
}

void SetVideoCodecMinBitrate(webrtc::VideoCodec* video_codec) {
  std::string input = TypedInput("Choose min bitrate (in kbps).")
      .WithDefault(DEFAULT_VIDEO_CODEC_MIN_BITRATE)
      .WithInputValidator(new webrtc::IntegerWithinRangeValidator(1, INT_MAX))
      .AskForInput();

  video_codec->minBitrate = atoi(input.c_str());
}

void SetVideoCodecMaxFramerate(webrtc::VideoCodec* video_codec) {
  std::string input = TypedInput("Choose max framerate (in fps).")
      .WithDefault(DEFAULT_VIDEO_CODEC_MAX_FRAMERATE)
      .WithInputValidator(new webrtc::IntegerWithinRangeValidator(1, INT_MAX))
      .AskForInput();
  video_codec->maxFramerate = atoi(input.c_str());
}

void SetVideoCodecTemporalLayer(webrtc::VideoCodec* video_codec) {
  if (video_codec->codecType != webrtc::kVideoCodecVP8)
    return;

  std::string input = TypedInput("Choose number of temporal layers (0 to 4).")
      .WithDefault(DEFAULT_TEMPORAL_LAYER)
      .WithInputValidator(new webrtc::IntegerWithinRangeValidator(0, 4))
      .AskForInput();
  video_codec->codecSpecific.VP8.numberOfTemporalLayers = atoi(input.c_str());
}

// GetVideoProtection only prints the prompt to get a number
// that SetVideoProtection method uses.
VideoProtectionMethod GetVideoProtection() {
  int choice = FromChoices(
      "Available Video Protection Methods:",
      "None\n"
      "FEC\n"
      "NACK\n"
      "NACK+FEC\n")
          .WithDefault(DEFAULT_VIDEO_PROTECTION_METHOD)
          .Choose();

  assert(choice >= kProtectionMethodNone &&
         choice <= kProtectionMethodHybridNackAndFec);
  return static_cast<VideoProtectionMethod>(choice);
}

bool SetVideoProtection(webrtc::ViECodec* vie_codec,
                        webrtc::ViERTP_RTCP* vie_rtp_rtcp,
                        int video_channel,
                        VideoProtectionMethod protection_method) {
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
    case kProtectionMethodNone:
      // No protection selected, all protection already at false.
      std::cout << "Call using None protection Method" << std::endl;
      break;
    case kProtectionMethodFecOnly:
      std::cout << "Call using FEC protection Method" << std::endl;
      error = vie_rtp_rtcp->SetFECStatus(video_channel, true,
                                         VCM_RED_PAYLOAD_TYPE,
                                         VCM_ULPFEC_PAYLOAD_TYPE);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      break;
    case kProtectionMethodNackOnly:
      std::cout << "Call using NACK protection Method" << std::endl;
      error = vie_rtp_rtcp->SetNACKStatus(video_channel, true);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      break;
    case kProtectionMethodHybridNackAndFec:
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
  if (protection_method == kProtectionMethodFecOnly ||
      protection_method == kProtectionMethodHybridNackAndFec) {
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

// Returns true if REMB, false if TMMBR.
bool GetBitrateSignaling() {
  int choice = FromChoices(
      "Available Bitrate Signaling Methods:",
      "REMB\n"
      "TMMBR\n")
          .WithDefault("REMB")
          .Choose();
  return choice == 1;
}

int GetBufferingDelay() {
  std::string input = TypedInput("Choose buffering delay (mS).")
      .WithDefault(DEFAULT_BUFFERING_DELAY_MS)
      .WithInputValidator(new webrtc::IntegerWithinRangeValidator(0, 10000))
      .AskForInput();
  std::string delay_ms = input;
  return atoi(delay_ms.c_str());
}

void PrintRTCCPStatistics(webrtc::ViERTP_RTCP* vie_rtp_rtcp,
                          int video_channel,
                          StatisticsType stat_type) {
  int error = 0;
  int number_of_errors = 0;
  webrtc::RtcpStatistics rtcp_stats;
  int64_t rtt_ms = 0;

  switch (stat_type) {
    case kReceivedStatistic:
      std::cout << "RTCP Received statistics"
                << std::endl;
      // Get and print the Received RTCP Statistics
      error = vie_rtp_rtcp->GetReceiveChannelRtcpStatistics(video_channel,
                                                      rtcp_stats,
                                                      rtt_ms);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      break;
    case kSendStatistic:
      std::cout << "RTCP Sent statistics"
                << std::endl;
      // Get and print the Sent RTCP Statistics
      error = vie_rtp_rtcp->GetSendChannelRtcpStatistics(video_channel,
                                                  rtcp_stats,
                                                  rtt_ms);
      number_of_errors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
      break;
  }
  std::cout << "\tRTCP fraction of lost packets: "
            << rtcp_stats.fraction_lost << std::endl;
  std::cout << "\tRTCP cumulative number of lost packets: "
            << rtcp_stats.cumulative_lost << std::endl;
  std::cout << "\tRTCP max received sequence number "
            << rtcp_stats.extended_max_sequence_number << std::endl;
  std::cout << "\tRTCP jitter: "
            << rtcp_stats.jitter << std::endl;
  std::cout << "\tRTCP round trip (ms): "
            << rtt_ms << std::endl;
}

void PrintRTPStatistics(webrtc::ViERTP_RTCP* vie_rtp_rtcp,
                        int video_channel) {
  int error = 0;
  int number_of_errors = 0;
  webrtc::StreamDataCounters sent;
  webrtc::StreamDataCounters received;

  std::cout << "RTP statistics"
            << std::endl;

  // Get and print the RTP Statistics
  error = vie_rtp_rtcp->GetRtpStatistics(video_channel, sent, received);
  number_of_errors += ViETest::TestError(error == 0,
                                         "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  std::cout << "\tRTP bytes sent: "
            << sent.transmitted.payload_bytes << std::endl;
  std::cout << "\tRTP packets sent: "
            << sent.transmitted.packets << std::endl;
  std::cout << "\tRTP bytes received: "
            << received.transmitted.payload_bytes << std::endl;
  std::cout << "\tRTP packets received: "
            << received.transmitted.packets << std::endl;
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
  discarded_packets = vie_codec->GetNumDiscardedPackets(video_channel);
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
