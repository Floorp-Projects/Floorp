/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Refer to kUsage below for a description.

#include "gflags/gflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/sleep.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/test/channel_transport/include/channel_transport.h"
#include "webrtc/test/testsupport/trace_to_stderr.h"
#include "webrtc/tools/agc/agc_manager.h"
#include "webrtc/voice_engine/include/voe_audio_processing.h"
#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_external_media.h"
#include "webrtc/voice_engine/include/voe_file.h"
#include "webrtc/voice_engine/include/voe_hardware.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/include/voe_volume_control.h"

DEFINE_bool(codecs, false, "print out available codecs");
DEFINE_int32(pt, 103, "codec payload type (defaults to ISAC/16000/1)");
DEFINE_bool(internal, true, "use the internal AGC in 'serial' mode, or as the "
                            "first voice engine's AGC in parallel mode");
DEFINE_bool(parallel, false, "run internal and public AGCs in parallel, with "
    "left- and right-panning respectively. Not compatible with -aec.");
DEFINE_bool(devices, false, "print out capture devices and indexes to be used "
                            "with the capture flags");
DEFINE_int32(capture1, 0, "capture device index for the first voice engine");
DEFINE_int32(capture2, 0, "capture device index for second voice engine");
DEFINE_int32(render1, 0, "render device index for first voice engine");
DEFINE_int32(render2, 0, "render device index for second voice engine");
DEFINE_bool(aec, false, "runs two voice engines in parallel, with the first "
    "playing out a file and sending its captured signal to the second voice "
    "engine. Also enables echo cancellation.");
DEFINE_bool(ns, true, "enable noise suppression");
DEFINE_bool(highpass, true, "enable high pass filter");
DEFINE_string(filename, "", "filename for the -aec mode");

namespace webrtc {
namespace {

const char kUsage[] =
    "\nWithout additional flags, sets up a simple VoiceEngine loopback call\n"
    "with the default audio devices and runs forever. The internal AGC is\n"
    "enabled and the public disabled.\n\n"

    "It can also run the public AGC in parallel with the internal, panned to\n"
    "opposite stereo channels on the default render device. The capture\n"
    "devices for each can be selected (recommended, because otherwise they\n"
    "will fight for the level on the same device).\n\n"

    "Lastly, it can be used for local AEC testing. In this mode, the first\n"
    "voice engine plays out a file over the selected render device (normally\n"
    "loudspeakers) and records from the selected capture device. The second\n"
    "voice engine receives the capture signal and plays it out over the\n"
    "selected render device (normally headphones). This allows the user to\n"
    "test an echo scenario with the first voice engine, while monitoring the\n"
    "result with the second.";

class AgcVoiceEngine {
 public:
  enum Pan {
    NoPan,
    PanLeft,
    PanRight
  };

  AgcVoiceEngine(bool internal, int tx_port, int rx_port, int capture_idx,
                 int render_idx)
      : voe_(VoiceEngine::Create()),
        base_(VoEBase::GetInterface(voe_)),
        hardware_(VoEHardware::GetInterface(voe_)),
        codec_(VoECodec::GetInterface(voe_)),
        manager_(new AgcManager(voe_)),
        channel_(-1),
        capture_idx_(capture_idx),
        render_idx_(render_idx) {
    SetUp(internal, tx_port, rx_port);
  }

  ~AgcVoiceEngine() {
    TearDown();
  }

  void SetUp(bool internal, int tx_port, int rx_port) {
    ASSERT_TRUE(voe_ != NULL);
    ASSERT_TRUE(base_ != NULL);
    ASSERT_TRUE(hardware_ != NULL);
    ASSERT_TRUE(codec_ != NULL);
    VoEAudioProcessing* audio = VoEAudioProcessing::GetInterface(voe_);
    ASSERT_TRUE(audio != NULL);
    VoENetwork* network = VoENetwork::GetInterface(voe_);
    ASSERT_TRUE(network != NULL);

    ASSERT_EQ(0, base_->Init());
    channel_ = base_->CreateChannel();
    ASSERT_NE(-1, channel_);

    channel_transport_.reset(
        new test::VoiceChannelTransport(network, channel_));
    ASSERT_EQ(0, channel_transport_->SetSendDestination("127.0.0.1", tx_port));
    ASSERT_EQ(0, channel_transport_->SetLocalReceiver(rx_port));

    ASSERT_EQ(0, hardware_->SetRecordingDevice(capture_idx_));
    ASSERT_EQ(0, hardware_->SetPlayoutDevice(render_idx_));

    CodecInst codec_params = {0};
    bool codec_found = false;
    for (int i = 0; i < codec_->NumOfCodecs(); i++) {
      ASSERT_EQ(0, codec_->GetCodec(i, codec_params));
      if (FLAGS_pt == codec_params.pltype) {
        codec_found = true;
        break;
      }
    }
    ASSERT_TRUE(codec_found);
    ASSERT_EQ(0, codec_->SetSendCodec(channel_, codec_params));

    ASSERT_EQ(0, audio->EnableHighPassFilter(FLAGS_highpass));
    ASSERT_EQ(0, audio->SetNsStatus(FLAGS_ns));
    ASSERT_EQ(0, audio->SetEcStatus(FLAGS_aec));

    ASSERT_EQ(0, manager_->Enable(internal));
    ASSERT_EQ(0, audio->SetAgcStatus(!internal));

    audio->Release();
    network->Release();
  }

  void TearDown() {
    Stop();
    channel_transport_.reset(NULL);
    ASSERT_EQ(0, base_->DeleteChannel(channel_));
    ASSERT_EQ(0, base_->Terminate());
    // Don't test; the manager hasn't released its interfaces.
    hardware_->Release();
    base_->Release();
    codec_->Release();
    delete manager_;
    ASSERT_TRUE(VoiceEngine::Delete(voe_));
  }

  void PrintDevices() {
    int num_devices = 0;
    char device_name[128] = {0};
    char guid[128] = {0};
    ASSERT_EQ(0, hardware_->GetNumOfRecordingDevices(num_devices));
    printf("Capture devices:\n");
    for (int i = 0; i < num_devices; i++) {
      ASSERT_EQ(0, hardware_->GetRecordingDeviceName(i, device_name, guid));
      printf("%d: %s\n", i, device_name);
    }
    ASSERT_EQ(0, hardware_->GetNumOfPlayoutDevices(num_devices));
    printf("Render devices:\n");
    for (int i = 0; i < num_devices; i++) {
      ASSERT_EQ(0, hardware_->GetPlayoutDeviceName(i, device_name, guid));
      printf("%d: %s\n", i, device_name);
    }
  }

  void PrintCodecs() {
    CodecInst params = {0};
    printf("Codecs:\n");
    for (int i = 0; i < codec_->NumOfCodecs(); i++) {
      ASSERT_EQ(0, codec_->GetCodec(i, params));
      printf("%d %s/%d/%d\n", params.pltype, params.plname, params.plfreq,
             params.channels);
    }
  }

  void StartSending() {
    ASSERT_EQ(0, base_->StartSend(channel_));
  }

  void StartPlaying(Pan pan, const std::string& filename) {
    VoEVolumeControl* volume = VoEVolumeControl::GetInterface(voe_);
    VoEFile* file = VoEFile::GetInterface(voe_);
    ASSERT_TRUE(volume != NULL);
    ASSERT_TRUE(file != NULL);
    if (pan == PanLeft) {
      volume->SetOutputVolumePan(channel_, 1, 0);
    } else if (pan == PanRight) {
      volume->SetOutputVolumePan(channel_, 0, 1);
    }
    if (filename != "") {
      printf("playing file\n");
      ASSERT_EQ(0, file->StartPlayingFileLocally(channel_, filename.c_str(),
          true, kFileFormatPcm16kHzFile, 1.0, 0, 0));
    }
    ASSERT_EQ(0, base_->StartReceive(channel_));
    ASSERT_EQ(0, base_->StartPlayout(channel_));
    volume->Release();
    file->Release();
  }

  void Stop() {
    ASSERT_EQ(0, base_->StopSend(channel_));
    ASSERT_EQ(0, base_->StopPlayout(channel_));
  }

 private:
  VoiceEngine* voe_;
  VoEBase* base_;
  VoEHardware* hardware_;
  VoECodec* codec_;
  AgcManager* manager_;
  int channel_;
  int capture_idx_;
  int render_idx_;
  rtc::scoped_ptr<test::VoiceChannelTransport> channel_transport_;
};

void RunHarness() {
  rtc::scoped_ptr<AgcVoiceEngine> voe1(new AgcVoiceEngine(
      FLAGS_internal, 2000, 2000, FLAGS_capture1, FLAGS_render1));
  rtc::scoped_ptr<AgcVoiceEngine> voe2;
  if (FLAGS_parallel) {
    voe2.reset(new AgcVoiceEngine(!FLAGS_internal, 3000, 3000, FLAGS_capture2,
                                  FLAGS_render2));
    voe1->StartPlaying(AgcVoiceEngine::PanLeft, "");
    voe1->StartSending();
    voe2->StartPlaying(AgcVoiceEngine::PanRight, "");
    voe2->StartSending();
  } else if (FLAGS_aec) {
    voe1.reset(new AgcVoiceEngine(FLAGS_internal, 2000, 4242, FLAGS_capture1,
                                  FLAGS_render1));
    voe2.reset(new AgcVoiceEngine(!FLAGS_internal, 4242, 2000, FLAGS_capture2,
                                  FLAGS_render2));
    voe1->StartPlaying(AgcVoiceEngine::NoPan, FLAGS_filename);
    voe1->StartSending();
    voe2->StartPlaying(AgcVoiceEngine::NoPan, "");
  } else {
    voe1->StartPlaying(AgcVoiceEngine::NoPan, "");
    voe1->StartSending();
  }

  // Run forever...
  SleepMs(0x7fffffff);
}

void PrintDevices() {
  AgcVoiceEngine device_voe(false, 4242, 4242, 0, 0);
  device_voe.PrintDevices();
}

void PrintCodecs() {
  AgcVoiceEngine codec_voe(false, 4242, 4242, 0, 0);
  codec_voe.PrintCodecs();
}

}  // namespace
}  // namespace webrtc

int main(int argc, char** argv) {
  google::SetUsageMessage(webrtc::kUsage);
  google::ParseCommandLineFlags(&argc, &argv, true);
  webrtc::test::TraceToStderr trace_to_stderr;

  if (FLAGS_parallel && FLAGS_aec) {
    printf("-parallel and -aec are not compatible\n");
    return 1;
  }
  if (FLAGS_devices) {
    webrtc::PrintDevices();
  }
  if (FLAGS_codecs) {
    webrtc::PrintCodecs();
  }
  if (!FLAGS_devices && !FLAGS_codecs) {
    webrtc::RunHarness();
  }
  return 0;
}
