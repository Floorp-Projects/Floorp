/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "TestStereo.h"

#include <cassert>
#include <iostream>

#include "gtest/gtest.h"

#include "audio_coding_module_typedefs.h"
#include "common_types.h"
#include "engine_configurations.h"
#include "testsupport/fileutils.h"
#include "trace.h"
#include "utility.h"

namespace webrtc {

// Class for simulating packet handling
TestPackStereo::TestPackStereo()
    : receiver_acm_(NULL),
      seq_no_(0),
      timestamp_diff_(0),
      last_in_timestamp_(0),
      total_bytes_(0),
      payload_size_(0),
      codec_mode_(kNotSet),
      lost_packet_(false) {}

TestPackStereo::~TestPackStereo() {}

void TestPackStereo::RegisterReceiverACM(AudioCodingModule* acm) {
  receiver_acm_ = acm;
  return;
}

WebRtc_Word32 TestPackStereo::SendData(
    const FrameType frame_type,
    const WebRtc_UWord8 payload_type,
    const WebRtc_UWord32 timestamp,
    const WebRtc_UWord8* payload_data,
    const WebRtc_UWord16 payload_size,
    const RTPFragmentationHeader* fragmentation) {
  WebRtcRTPHeader rtp_info;
  WebRtc_Word32 status = 0;

  rtp_info.header.markerBit = false;
  rtp_info.header.ssrc = 0;
  rtp_info.header.sequenceNumber = seq_no_++;
  rtp_info.header.payloadType = payload_type;
  rtp_info.header.timestamp = timestamp;
  if (frame_type == kFrameEmpty) {
    // Skip this frame
    return 0;
  }

  if (lost_packet_ == false) {
    if (frame_type != kAudioFrameCN) {
      rtp_info.type.Audio.isCNG = false;
      rtp_info.type.Audio.channel = (int) codec_mode_;
    } else {
      rtp_info.type.Audio.isCNG = true;
      rtp_info.type.Audio.channel = (int) kMono;
    }
    status = receiver_acm_->IncomingPacket(payload_data, payload_size,
                                           rtp_info);

    if (frame_type != kAudioFrameCN) {
      payload_size_ = payload_size;
    } else {
      payload_size_ = -1;
    }

    timestamp_diff_ = timestamp - last_in_timestamp_;
    last_in_timestamp_ = timestamp;
    total_bytes_ += payload_size;
  }
  return status;
}

WebRtc_UWord16 TestPackStereo::payload_size() {
  return payload_size_;
}

WebRtc_UWord32 TestPackStereo::timestamp_diff() {
  return timestamp_diff_;
}

void TestPackStereo::reset_payload_size() {
  payload_size_ = 0;
}

void TestPackStereo::set_codec_mode(enum StereoMonoMode mode) {
  codec_mode_ = mode;
}

void TestPackStereo::set_lost_packet(bool lost) {
  lost_packet_ = lost;
}

TestStereo::TestStereo(int test_mode)
    : acm_a_(NULL),
      acm_b_(NULL),
      channel_a2b_(NULL),
      test_cntr_(0),
      pack_size_samp_(0),
      pack_size_bytes_(0),
      counter_(0),
      g722_pltype_(0),
      l16_8khz_pltype_(-1),
      l16_16khz_pltype_(-1),
      l16_32khz_pltype_(-1),
      pcma_pltype_(-1),
      pcmu_pltype_(-1),
      celt_pltype_(-1),
      cn_8khz_pltype_(-1),
      cn_16khz_pltype_(-1),
      cn_32khz_pltype_(-1) {
  // test_mode = 0 for silent test (auto test)
  test_mode_ = test_mode;
}

TestStereo::~TestStereo() {
  if (acm_a_ != NULL) {
    AudioCodingModule::Destroy(acm_a_);
    acm_a_ = NULL;
  }
  if (acm_b_ != NULL) {
    AudioCodingModule::Destroy(acm_b_);
    acm_b_ = NULL;
  }
  if (channel_a2b_ != NULL) {
    delete channel_a2b_;
    channel_a2b_ = NULL;
  }
}

void TestStereo::Perform() {
  WebRtc_UWord16 frequency_hz;
  int audio_channels;
  int codec_channels;
  bool dtx;
  bool vad;
  ACMVADMode vad_mode;

  if (test_mode_ == 0) {
    printf("Running Stereo Test");
    WEBRTC_TRACE(kTraceStateInfo, kTraceAudioCoding, -1,
                 "---------- TestStereo ----------");
  }

  // Open both mono and stereo test files in 32 kHz.
  const std::string file_name_stereo =
      webrtc::test::ResourcePath("audio_coding/teststereo32kHz", "pcm");
  const std::string file_name_mono =
      webrtc::test::ResourcePath("audio_coding/testfile32kHz", "pcm");
  frequency_hz = 32000;
  in_file_stereo_ = new PCMFile();
  in_file_mono_ = new PCMFile();
  in_file_stereo_->Open(file_name_stereo, frequency_hz, "rb");
  in_file_stereo_->ReadStereo(true);
  in_file_mono_->Open(file_name_mono, frequency_hz, "rb");
  in_file_mono_->ReadStereo(false);

  // Create and initialize two ACMs, one for each side of a one-to-one call.
  acm_a_ = AudioCodingModule::Create(0);
  acm_b_ = AudioCodingModule::Create(1);
  ASSERT_TRUE((acm_a_ != NULL) && (acm_b_ != NULL));
  EXPECT_EQ(0, acm_a_->InitializeReceiver());
  EXPECT_EQ(0, acm_b_->InitializeReceiver());

  // Register all available codes as receiving codecs.
  WebRtc_UWord8 num_encoders = acm_a_->NumberOfCodecs();
  CodecInst my_codec_param;
  for (WebRtc_UWord8 n = 0; n < num_encoders; n++) {
    EXPECT_EQ(0, acm_b_->Codec(n, my_codec_param));
    EXPECT_EQ(0, acm_b_->RegisterReceiveCodec(my_codec_param));
  }

  // Test that unregister all receive codecs works.
  for (WebRtc_UWord8 n = 0; n < num_encoders; n++) {
    EXPECT_EQ(0, acm_b_->Codec(n, my_codec_param));
    EXPECT_EQ(0, acm_b_->UnregisterReceiveCodec(my_codec_param.pltype));
  }

  // Register all available codes as receiving codecs once more.
  for (WebRtc_UWord8 n = 0; n < num_encoders; n++) {
    EXPECT_EQ(0, acm_b_->Codec(n, my_codec_param));
    EXPECT_EQ(0, acm_b_->RegisterReceiveCodec(my_codec_param));
  }

  // TODO(tlegrand): Take care of return values of all function calls.

  // TODO(tlegrand): Re-register all stereo codecs needed in the test,
  // with new payload numbers.
  // g722_pltype_ = 117;
  // l16_8khz_pltype_ = 120;
  // l16_16khz_pltype_ = 121;
  // l16_32khz_pltype_ = 122;
  // pcma_pltype_ = 110;
  // pcmu_pltype_ = 118;
  // celt_pltype_ = 119;
  // cn_8khz_pltype_ = 123;
  // cn_16khz_pltype_ = 124;
  // cn_32khz_pltype_ = 125;

  // Create and connect the channel.
  channel_a2b_ = new TestPackStereo;
  EXPECT_EQ(0, acm_a_->RegisterTransportCallback(channel_a2b_));
  channel_a2b_->RegisterReceiverACM(acm_b_);

  // Start with setting VAD/DTX, before we know we will send stereo.
  // Continue with setting a stereo codec as send codec and verify that
  // VAD/DTX gets turned off.
  EXPECT_EQ(0, acm_a_->SetVAD(true, true, VADNormal));
  EXPECT_EQ(0, acm_a_->VAD(dtx, vad, vad_mode));
  EXPECT_TRUE(dtx);
  EXPECT_TRUE(vad);
  char codec_pcma_temp[] = "PCMA";
  RegisterSendCodec('A', codec_pcma_temp, 8000, 64000, 80, 2, pcma_pltype_);
  EXPECT_EQ(0, acm_a_->VAD(dtx, vad, vad_mode));
  EXPECT_FALSE(dtx);
  EXPECT_FALSE(vad);
  if(test_mode_ != 0) {
    printf("\n");
  }

  //
  // Test Stereo-To-Stereo for all codecs.
  //
  audio_channels = 2;
  codec_channels = 2;

  // All codecs are tested for all allowed sampling frequencies, rates and
  // packet sizes.
#ifdef WEBRTC_CODEC_G722
  if(test_mode_ != 0) {
    printf("===========================================================\n");
    printf("Test number: %d\n",test_cntr_ + 1);
    printf("Test type: Stereo-to-stereo\n");
  }
  channel_a2b_->set_codec_mode(kStereo);
  test_cntr_++;
  OpenOutFile(test_cntr_);
  char codec_g722[] = "G722";
  RegisterSendCodec('A', codec_g722, 16000, 64000, 160, codec_channels,
      g722_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_g722, 16000, 64000, 320, codec_channels,
      g722_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_g722, 16000, 64000, 480, codec_channels,
      g722_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_g722, 16000, 64000, 640, codec_channels,
      g722_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_g722, 16000, 64000, 800, codec_channels,
      g722_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_g722, 16000, 64000, 960, codec_channels,
      g722_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  out_file_.Close();
#endif
#ifdef WEBRTC_CODEC_PCM16
  if(test_mode_ != 0) {
    printf("===========================================================\n");
    printf("Test number: %d\n",test_cntr_ + 1);
    printf("Test type: Stereo-to-stereo\n");
  }
  channel_a2b_->set_codec_mode(kStereo);
  test_cntr_++;
  OpenOutFile(test_cntr_);
  char codec_l16[] = "L16";
  RegisterSendCodec('A', codec_l16, 8000, 128000, 80, codec_channels,
      l16_8khz_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_l16, 8000, 128000, 160, codec_channels,
      l16_8khz_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_l16, 8000, 128000, 240, codec_channels,
      l16_8khz_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_l16, 8000, 128000, 320, codec_channels,
      l16_8khz_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  out_file_.Close();

  if(test_mode_ != 0) {
    printf("===========================================================\n");
    printf("Test number: %d\n",test_cntr_ + 1);
    printf("Test type: Stereo-to-stereo\n");
  }
  test_cntr_++;
  OpenOutFile(test_cntr_);
  RegisterSendCodec('A', codec_l16, 16000, 256000, 160, codec_channels,
      l16_16khz_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_l16, 16000, 256000, 320, codec_channels,
      l16_16khz_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_l16, 16000, 256000, 480, codec_channels,
      l16_16khz_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_l16, 16000, 256000, 640, codec_channels,
      l16_16khz_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  out_file_.Close();

  if(test_mode_ != 0) {
    printf("===========================================================\n");
    printf("Test number: %d\n",test_cntr_ + 1);
    printf("Test type: Stereo-to-stereo\n");
  }
  test_cntr_++;
  OpenOutFile(test_cntr_);
  RegisterSendCodec('A', codec_l16, 32000, 512000, 320, codec_channels,
      l16_32khz_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_l16, 32000, 512000, 640, codec_channels,
      l16_32khz_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  out_file_.Close();
#endif
#define PCMA_AND_PCMU
#ifdef PCMA_AND_PCMU
  if (test_mode_ != 0) {
    printf("===========================================================\n");
    printf("Test number: %d\n", test_cntr_ + 1);
    printf("Test type: Stereo-to-stereo\n");
  }
  channel_a2b_->set_codec_mode(kStereo);
  audio_channels = 2;
  codec_channels = 2;
  test_cntr_++;
  OpenOutFile(test_cntr_);
  char codec_pcma[] = "PCMA";
  RegisterSendCodec('A', codec_pcma, 8000, 64000, 80, codec_channels,
                    pcma_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_pcma, 8000, 64000, 160, codec_channels,
                    pcma_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_pcma, 8000, 64000, 240, codec_channels,
                    pcma_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_pcma, 8000, 64000, 320, codec_channels,
                    pcma_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_pcma, 8000, 64000, 400, codec_channels,
                    pcma_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_pcma, 8000, 64000, 480, codec_channels,
                    pcma_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);

  // Test that VAD/DTX cannot be turned on while sending stereo.
  EXPECT_EQ(-1, acm_a_->SetVAD(true, true, VADNormal));
  EXPECT_EQ(0, acm_a_->VAD(dtx, vad, vad_mode));
  EXPECT_FALSE(dtx);
  EXPECT_FALSE(vad);
  EXPECT_EQ(-1, acm_a_->SetVAD(true, false, VADNormal));
  EXPECT_EQ(0, acm_a_->VAD(dtx, vad, vad_mode));
  EXPECT_FALSE(dtx);
  EXPECT_FALSE(vad);
  EXPECT_EQ(-1, acm_a_->SetVAD(false, true, VADNormal));
  EXPECT_EQ(0, acm_a_->VAD(dtx, vad, vad_mode));
  EXPECT_FALSE(dtx);
  EXPECT_FALSE(vad);
  EXPECT_EQ(0, acm_a_->SetVAD(false, false, VADNormal));
  EXPECT_EQ(0, acm_a_->VAD(dtx, vad, vad_mode));
  EXPECT_FALSE(dtx);
  EXPECT_FALSE(vad);

  out_file_.Close();
  if (test_mode_ != 0) {
    printf("===========================================================\n");
    printf("Test number: %d\n", test_cntr_ + 1);
    printf("Test type: Stereo-to-stereo\n");
  }
  test_cntr_++;
  OpenOutFile(test_cntr_);
  char codec_pcmu[] = "PCMU";
  RegisterSendCodec('A', codec_pcmu, 8000, 64000, 80, codec_channels,
                    pcmu_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_pcmu, 8000, 64000, 160, codec_channels,
                    pcmu_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_pcmu, 8000, 64000, 240, codec_channels,
                    pcmu_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_pcmu, 8000, 64000, 320, codec_channels,
                    pcmu_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_pcmu, 8000, 64000, 400, codec_channels,
                    pcmu_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_pcmu, 8000, 64000, 480, codec_channels,
                    pcmu_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  out_file_.Close();
#endif
#ifdef WEBRTC_CODEC_CELT
  if(test_mode_ != 0) {
    printf("===========================================================\n");
    printf("Test number: %d\n",test_cntr_ + 1);
    printf("Test type: Stereo-to-stereo\n");
  }
  channel_a2b_->set_codec_mode(kStereo);
  audio_channels = 2;
  codec_channels = 2;
  test_cntr_++;
  OpenOutFile(test_cntr_);
  char codec_celt[] = "CELT";
  RegisterSendCodec('A', codec_celt, 32000, 48000, 640, codec_channels,
      celt_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_celt, 32000, 64000, 640, codec_channels,
      celt_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_celt, 32000, 128000, 640, codec_channels,
      celt_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  out_file_.Close();
#endif
  //
  // Test Mono-To-Stereo for all codecs.
  //
  audio_channels = 1;
  codec_channels = 2;

#ifdef WEBRTC_CODEC_G722
  if(test_mode_ != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",test_cntr_ + 1);
    printf("Test type: Mono-to-stereo\n");
  }
  test_cntr_++;
  channel_a2b_->set_codec_mode(kStereo);
  OpenOutFile(test_cntr_);
  RegisterSendCodec('A', codec_g722, 16000, 64000, 160, codec_channels,
                    g722_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  out_file_.Close();
#endif
#ifdef WEBRTC_CODEC_PCM16
  if(test_mode_ != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",test_cntr_ + 1);
    printf("Test type: Mono-to-stereo\n");
  }
  test_cntr_++;
  channel_a2b_->set_codec_mode(kStereo);
  OpenOutFile(test_cntr_);
  RegisterSendCodec('A', codec_l16, 8000, 128000, 80, codec_channels,
                    l16_8khz_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  out_file_.Close();
  if(test_mode_ != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",test_cntr_ + 1);
    printf("Test type: Mono-to-stereo\n");
  }
  test_cntr_++;
  OpenOutFile(test_cntr_);
  RegisterSendCodec('A', codec_l16, 16000, 256000, 160, codec_channels,
                    l16_16khz_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  out_file_.Close();
  if(test_mode_ != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",test_cntr_ + 1);
    printf("Test type: Mono-to-stereo\n");
  }
  test_cntr_++;
  OpenOutFile(test_cntr_);
  RegisterSendCodec('A', codec_l16, 32000, 512000, 320, codec_channels,
                    l16_32khz_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  out_file_.Close();
#endif
#ifdef PCMA_AND_PCMU
  if(test_mode_ != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",test_cntr_ + 1);
    printf("Test type: Mono-to-stereo\n");
  }
  test_cntr_++;
  channel_a2b_->set_codec_mode(kStereo);
  OpenOutFile(test_cntr_);
  RegisterSendCodec('A', codec_pcmu, 8000, 64000, 80, codec_channels,
                    pcmu_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_pcma, 8000, 64000, 80, codec_channels,
                    pcma_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  out_file_.Close();
#endif
#ifdef WEBRTC_CODEC_CELT
  if(test_mode_ != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",test_cntr_ + 1);
    printf("Test type: Mono-to-stereo\n");
  }
  test_cntr_++;
  channel_a2b_->set_codec_mode(kStereo);
  OpenOutFile(test_cntr_);
  RegisterSendCodec('A', codec_celt, 32000, 64000, 640, codec_channels,
                    celt_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  out_file_.Close();
#endif

  //
  // Test Stereo-To-Mono for all codecs.
  //
  audio_channels = 2;
  codec_channels = 1;
  channel_a2b_->set_codec_mode(kMono);

#ifdef WEBRTC_CODEC_G722
  // Run stereo audio and mono codec.
  if(test_mode_ != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",test_cntr_ + 1);
    printf("Test type: Stereo-to-mono\n");
  }
  test_cntr_++;
  OpenOutFile(test_cntr_);
  RegisterSendCodec('A', codec_g722, 16000, 64000, 160, codec_channels,
                    g722_pltype_);


  // Make sure it is possible to set VAD/CNG, now that we are sending mono
  // again.
  EXPECT_EQ(0, acm_a_->SetVAD(true, true, VADNormal));
  EXPECT_EQ(0, acm_a_->VAD(dtx, vad, vad_mode));
  EXPECT_TRUE(dtx);
  EXPECT_TRUE(vad);
  EXPECT_EQ(0, acm_a_->SetVAD(false, false, VADNormal));
  Run(channel_a2b_, audio_channels, codec_channels);
  out_file_.Close();
#endif
#ifdef WEBRTC_CODEC_PCM16
  if(test_mode_ != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",test_cntr_ + 1);
    printf("Test type: Stereo-to-mono\n");
  }
  test_cntr_++;
  OpenOutFile(test_cntr_);
  RegisterSendCodec('A', codec_l16, 8000, 128000, 80, codec_channels,
                    l16_8khz_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  out_file_.Close();
  if(test_mode_ != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",test_cntr_ + 1);
    printf("Test type: Stereo-to-mono\n");
   }
  test_cntr_++;
  OpenOutFile(test_cntr_);
  RegisterSendCodec('A', codec_l16, 16000, 256000, 160, codec_channels,
                    l16_16khz_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  out_file_.Close();
  if(test_mode_ != 0) {
     printf("==============================================================\n");
     printf("Test number: %d\n",test_cntr_ + 1);
     printf("Test type: Stereo-to-mono\n");
   }
   test_cntr_++;
   OpenOutFile(test_cntr_);
   RegisterSendCodec('A', codec_l16, 32000, 512000, 320, codec_channels,
                     l16_32khz_pltype_);
   Run(channel_a2b_, audio_channels, codec_channels);
   out_file_.Close();
#endif
#ifdef PCMA_AND_PCMU
  if(test_mode_ != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",test_cntr_ + 1);
    printf("Test type: Stereo-to-mono\n");
  }
  test_cntr_++;
  OpenOutFile(test_cntr_);
  RegisterSendCodec('A', codec_pcmu, 8000, 64000, 80, codec_channels,
                    pcmu_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  RegisterSendCodec('A', codec_pcma, 8000, 64000, 80, codec_channels,
                    pcma_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  out_file_.Close();
#endif
#ifdef WEBRTC_CODEC_CELT
  if(test_mode_ != 0) {
    printf("===============================================================\n");
    printf("Test number: %d\n",test_cntr_ + 1);
    printf("Test type: Stereo-to-mono\n");
  }
  test_cntr_++;
  OpenOutFile(test_cntr_);
  RegisterSendCodec('A', codec_celt, 32000, 64000, 640, codec_channels,
                    celt_pltype_);
  Run(channel_a2b_, audio_channels, codec_channels);
  out_file_.Close();
#endif

  // Print out which codecs were tested, and which were not, in the run.
  if (test_mode_ != 0) {
    printf("\nThe following codecs was INCLUDED in the test:\n");
#ifdef WEBRTC_CODEC_G722
    printf("   G.722\n");
#endif
#ifdef WEBRTC_CODEC_PCM16
    printf("   PCM16\n");
#endif
    printf("   G.711\n");
#ifdef WEBRTC_CODEC_CELT
    printf("   CELT\n");
#endif
    printf("\nTo complete the test, listen to the %d number of output "
           "files.\n",
           test_cntr_);
  }

  // Delete the file pointers.
  delete in_file_stereo_;
  delete in_file_mono_;
}

// Register Codec to use in the test
//
// Input:   side             - which ACM to use, 'A' or 'B'
//          codec_name       - name to use when register the codec
//          sampling_freq_hz - sampling frequency in Herz
//          rate             - bitrate in bytes
//          pack_size        - packet size in samples
//          channels         - number of channels; 1 for mono, 2 for stereo
//          payload_type     - payload type for the codec
void TestStereo::RegisterSendCodec(char side, char* codec_name,
                                   WebRtc_Word32 sampling_freq_hz, int rate,
                                   int pack_size, int channels,
                                   int payload_type) {
  if (test_mode_ != 0) {
    // Print out codec and settings
    printf("Codec: %s Freq: %d Rate: %d PackSize: %d\n", codec_name,
           sampling_freq_hz, rate, pack_size);
  }

  // Store packet size in samples, used to validate the received packet
  pack_size_samp_ = pack_size;

  // Store the expected packet size in bytes, used to validate the received
  // packet. Add 0.875 to always round up to a whole byte.
  // For Celt the packet size in bytes is already counting the stereo part.
  if (!strcmp(codec_name, "CELT")) {
    pack_size_bytes_ = (WebRtc_UWord16)(
        (float) (pack_size * rate) / (float) (sampling_freq_hz * 8) + 0.875)
        / channels;
  } else {
    pack_size_bytes_ = (WebRtc_UWord16)(
        (float) (pack_size * rate) / (float) (sampling_freq_hz * 8) + 0.875);
  }

  // Set pointer to the ACM where to register the codec
  AudioCodingModule* my_acm = NULL;
  switch (side) {
    case 'A': {
      my_acm = acm_a_;
      break;
    }
    case 'B': {
      my_acm = acm_b_;
      break;
    }
    default:
      break;
  }
  ASSERT_TRUE(my_acm != NULL);

  CodecInst my_codec_param;
  // Get all codec parameters before registering
  CHECK_ERROR(AudioCodingModule::Codec(codec_name, my_codec_param,
                                       sampling_freq_hz, channels));
  my_codec_param.rate = rate;
  my_codec_param.pacsize = pack_size;
  CHECK_ERROR(my_acm->RegisterSendCodec(my_codec_param));
}

void TestStereo::Run(TestPackStereo* channel, int in_channels, int out_channels,
                     int percent_loss) {
  AudioFrame audio_frame;

  WebRtc_Word32 out_freq_hz_b = out_file_.SamplingFrequency();
  WebRtc_UWord16 rec_size;
  WebRtc_UWord32 time_stamp_diff;
  channel->reset_payload_size();
  int error_count = 0;

  while (1) {
    // Simulate packet loss by setting |packet_loss_| to "true" in
    // |percent_loss| percent of the loops.
    if (percent_loss > 0) {
      if (counter_ == floor((100 / percent_loss) + 0.5)) {
        counter_ = 0;
        channel->set_lost_packet(true);
      } else {
        channel->set_lost_packet(false);
      }
      counter_++;
    }

    // Add 10 msec to ACM
    if (in_channels == 1) {
      if (in_file_mono_->EndOfFile()) {
        break;
      }
      in_file_mono_->Read10MsData(audio_frame);
    } else {
      if (in_file_stereo_->EndOfFile()) {
        break;
      }
      in_file_stereo_->Read10MsData(audio_frame);
    }
    CHECK_ERROR(acm_a_->Add10MsData(audio_frame));

    // Run sender side of ACM
    CHECK_ERROR(acm_a_->Process());

    // Verify that the received packet size matches the settings
    rec_size = channel->payload_size();
    if ((0 < rec_size) & (rec_size < 65535)) {
      if ((rec_size != pack_size_bytes_ * out_channels)
          && (pack_size_bytes_ < 65535)) {
        error_count++;
      }

      // Verify that the timestamp is updated with expected length
      time_stamp_diff = channel->timestamp_diff();
      if ((counter_ > 10) && (time_stamp_diff != pack_size_samp_)) {
        error_count++;
      }
    }

    // Run received side of ACM
    CHECK_ERROR(acm_b_->PlayoutData10Ms(out_freq_hz_b, audio_frame));

    // Write output speech to file
    out_file_.Write10MsData(
        audio_frame.data_,
        audio_frame.samples_per_channel_ * audio_frame.num_channels_);
  }

  EXPECT_EQ(0, error_count);

  if (in_file_mono_->EndOfFile()) {
    in_file_mono_->Rewind();
  }
  if (in_file_stereo_->EndOfFile()) {
    in_file_stereo_->Rewind();
  }
  // Reset in case we ended with a lost packet
  channel->set_lost_packet(false);
}

void TestStereo::OpenOutFile(WebRtc_Word16 test_number) {
  std::string file_name;
  std::stringstream file_stream;
  file_stream << webrtc::test::OutputPath() << "teststereo_out_"
      << test_number << ".pcm";
  file_name = file_stream.str();
  out_file_.Open(file_name, 32000, "wb");
}

void TestStereo::DisplaySendReceiveCodec() {
  CodecInst my_codec_param;
  acm_a_->SendCodec(my_codec_param);
  if (test_mode_ != 0) {
    printf("%s -> ", my_codec_param.plname);
  }
  acm_b_->ReceiveCodec(my_codec_param);
  if (test_mode_ != 0) {
    printf("%s\n", my_codec_param.plname);
  }
}

}  // namespace webrtc
