/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <stdio.h>

#include <algorithm>
#include <iostream>
#include <string>

#include "google/gflags.h"
#include "webrtc/modules/audio_coding/neteq4/interface/neteq.h"
#include "webrtc/modules/audio_coding/neteq4/test/NETEQTEST_RTPpacket.h"
#include "webrtc/modules/audio_coding/neteq4/test/NETEQTEST_DummyRTPpacket.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/typedefs.h"

using webrtc::NetEq;
using webrtc::WebRtcRTPHeader;

// Flag validators.
static bool ValidatePayloadType(const char* flagname, int32_t value) {
  if (value >= 0 && value <= 127)  // Value is ok.
    return true;
  printf("Invalid value for --%s: %d\n", flagname, static_cast<int>(value));
  return false;
}

// Define command line flags.
DEFINE_int32(pcmu, 0, "RTP payload type for PCM-u");
static const bool pcmu_dummy =
    google::RegisterFlagValidator(&FLAGS_pcmu, &ValidatePayloadType);
DEFINE_int32(pcma, 8, "RTP payload type for PCM-a");
static const bool pcma_dummy =
    google::RegisterFlagValidator(&FLAGS_pcma, &ValidatePayloadType);
DEFINE_int32(ilbc, 102, "RTP payload type for iLBC");
static const bool ilbc_dummy =
    google::RegisterFlagValidator(&FLAGS_ilbc, &ValidatePayloadType);
DEFINE_int32(isac, 103, "RTP payload type for iSAC");
static const bool isac_dummy =
    google::RegisterFlagValidator(&FLAGS_isac, &ValidatePayloadType);
DEFINE_int32(isac_swb, 104, "RTP payload type for iSAC-swb (32 kHz)");
static const bool isac_swb_dummy =
    google::RegisterFlagValidator(&FLAGS_isac_swb, &ValidatePayloadType);
DEFINE_int32(pcm16b, 93, "RTP payload type for PCM16b-nb (8 kHz)");
static const bool pcm16b_dummy =
    google::RegisterFlagValidator(&FLAGS_pcm16b, &ValidatePayloadType);
DEFINE_int32(pcm16b_wb, 94, "RTP payload type for PCM16b-wb (16 kHz)");
static const bool pcm16b_wb_dummy =
    google::RegisterFlagValidator(&FLAGS_pcm16b_wb, &ValidatePayloadType);
DEFINE_int32(pcm16b_swb32, 95, "RTP payload type for PCM16b-swb32 (32 kHz)");
static const bool pcm16b_swb32_dummy =
    google::RegisterFlagValidator(&FLAGS_pcm16b_swb32, &ValidatePayloadType);
DEFINE_int32(pcm16b_swb48, 96, "RTP payload type for PCM16b-swb48 (48 kHz)");
static const bool pcm16b_swb48_dummy =
    google::RegisterFlagValidator(&FLAGS_pcm16b_swb48, &ValidatePayloadType);
DEFINE_int32(g722, 9, "RTP payload type for G.722");
static const bool g722_dummy =
    google::RegisterFlagValidator(&FLAGS_g722, &ValidatePayloadType);
DEFINE_int32(avt, 106, "RTP payload type for AVT/DTMF");
static const bool avt_dummy =
    google::RegisterFlagValidator(&FLAGS_avt, &ValidatePayloadType);
DEFINE_int32(red, 117, "RTP payload type for redundant audio (RED)");
static const bool red_dummy =
    google::RegisterFlagValidator(&FLAGS_red, &ValidatePayloadType);
DEFINE_int32(cn_nb, 13, "RTP payload type for comfort noise (8 kHz)");
static const bool cn_nb_dummy =
    google::RegisterFlagValidator(&FLAGS_cn_nb, &ValidatePayloadType);
DEFINE_int32(cn_wb, 98, "RTP payload type for comfort noise (16 kHz)");
static const bool cn_wb_dummy =
    google::RegisterFlagValidator(&FLAGS_cn_wb, &ValidatePayloadType);
DEFINE_int32(cn_swb32, 99, "RTP payload type for comfort noise (32 kHz)");
static const bool cn_swb32_dummy =
    google::RegisterFlagValidator(&FLAGS_cn_swb32, &ValidatePayloadType);
DEFINE_int32(cn_swb48, 100, "RTP payload type for comfort noise (48 kHz)");
static const bool cn_swb48_dummy =
    google::RegisterFlagValidator(&FLAGS_cn_swb48, &ValidatePayloadType);
DEFINE_bool(codec_map, false, "Prints the mapping between RTP payload type and "
    "codec");
DEFINE_bool(dummy_rtp, false, "The input file contains ""dummy"" RTP data, "
            "i.e., only headers");

// Declaring helper functions (defined further down in this file).
std::string CodecName(webrtc::NetEqDecoder codec);
void RegisterPayloadTypes(NetEq* neteq);
void PrintCodecMapping();

int main(int argc, char* argv[]) {
  static const int kMaxChannels = 5;
  static const int kMaxSamplesPerMs = 48000 / 1000;
  static const int kOutputBlockSizeMs = 10;

  std::string program_name = argv[0];
  std::string usage = "Tool for decoding an RTP dump file using NetEq.\n"
      "Run " + program_name + " --helpshort for usage.\n"
      "Example usage:\n" + program_name +
      " input.rtp output.pcm\n";
  google::SetUsageMessage(usage);
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_codec_map) {
    PrintCodecMapping();
  }

  if (argc != 3) {
    if (FLAGS_codec_map) {
      // We have already printed the codec map. Just end the program.
      return 0;
    }
    // Print usage information.
    std::cout << google::ProgramUsage();
    return 0;
  }

  FILE* in_file = fopen(argv[1], "rb");
  if (!in_file) {
    std::cerr << "Cannot open input file " << argv[1] << std::endl;
    exit(1);
  }
  std::cout << "Input file: " << argv[1] << std::endl;

  FILE* out_file = fopen(argv[2], "wb");
  if (!in_file) {
    std::cerr << "Cannot open output file " << argv[2] << std::endl;
    exit(1);
  }
  std::cout << "Output file: " << argv[2] << std::endl;

  // Read RTP file header.
  if (NETEQTEST_RTPpacket::skipFileHeader(in_file) != 0) {
    std::cerr << "Wrong format in RTP file" << std::endl;
    exit(1);
  }

  // Enable tracing.
  webrtc::Trace::CreateTrace();
  webrtc::Trace::SetTraceFile((webrtc::test::OutputPath() +
      "neteq_trace.txt").c_str());
  webrtc::Trace::set_level_filter(webrtc::kTraceAll);

  // Initialize NetEq instance.
  int sample_rate_hz = 16000;
  NetEq* neteq = NetEq::Create(sample_rate_hz);
  RegisterPayloadTypes(neteq);

  // Read first packet.
  NETEQTEST_RTPpacket *rtp;
  if (!FLAGS_dummy_rtp) {
    rtp = new NETEQTEST_RTPpacket();
  } else {
    rtp = new NETEQTEST_DummyRTPpacket();
  }
  rtp->readFromFile(in_file);
  if (!rtp) {
    std::cout  << "Warning: RTP file is empty" << std::endl;
  }

  // This is the main simulation loop.
  int time_now_ms = rtp->time();  // Start immediately with the first packet.
  int next_input_time_ms = rtp->time();
  int next_output_time_ms = time_now_ms;
  if (time_now_ms % kOutputBlockSizeMs != 0) {
    // Make sure that next_output_time_ms is rounded up to the next multiple
    // of kOutputBlockSizeMs. (Legacy bit-exactness.)
    next_output_time_ms +=
        kOutputBlockSizeMs - time_now_ms % kOutputBlockSizeMs;
  }
  while (rtp->dataLen() >= 0) {
    // Check if it is time to insert packet.
    while (time_now_ms >= next_input_time_ms && rtp->dataLen() >= 0) {
      if (rtp->dataLen() > 0) {
        // Parse RTP header.
        WebRtcRTPHeader rtp_header;
        rtp->parseHeader(&rtp_header);
        int error = neteq->InsertPacket(rtp_header, rtp->payload(),
                                         rtp->payloadLen(),
                                         rtp->time() * sample_rate_hz / 1000);
        if (error != NetEq::kOK) {
          std::cerr << "InsertPacket returned error code " <<
              neteq->LastError() << std::endl;
        }
      }
      // Get next packet from file.
      rtp->readFromFile(in_file);
      next_input_time_ms = rtp->time();
    }

    // Check if it is time to get output audio.
    if (time_now_ms >= next_output_time_ms) {
      static const int kOutDataLen = kOutputBlockSizeMs * kMaxSamplesPerMs *
          kMaxChannels;
      int16_t out_data[kOutDataLen];
      int num_channels;
      int samples_per_channel;
      int error = neteq->GetAudio(kOutDataLen, out_data, &samples_per_channel,
                                   &num_channels, NULL);
      if (error != NetEq::kOK) {
        std::cerr << "GetAudio returned error code " <<
            neteq->LastError() << std::endl;
      } else {
        // Calculate sample rate from output size.
        sample_rate_hz = 1000 * samples_per_channel / kOutputBlockSizeMs;
      }

      // Write to file.
      size_t write_len = samples_per_channel * num_channels;
      if (fwrite(out_data, sizeof(out_data[0]), write_len, out_file) !=
          write_len) {
        std::cerr << "Error while writing to file" << std::endl;
        webrtc::Trace::ReturnTrace();
        exit(1);
      }
      next_output_time_ms += kOutputBlockSizeMs;
    }
    // Advance time to next event.
    time_now_ms = std::min(next_input_time_ms, next_output_time_ms);
  }

  std::cout << "Simulation done" << std::endl;

  fclose(in_file);
  fclose(out_file);
  delete neteq;
  webrtc::Trace::ReturnTrace();
  return 0;
}


// Help functions.

// Maps a codec type to a printable name string.
std::string CodecName(webrtc::NetEqDecoder codec) {
  switch (codec) {
    case webrtc::kDecoderPCMu:
      return "PCM-u";
    case webrtc::kDecoderPCMa:
      return "PCM-a";
    case webrtc::kDecoderILBC:
      return "iLBC";
    case webrtc::kDecoderISAC:
      return "iSAC";
    case webrtc::kDecoderISACswb:
      return "iSAC-swb (32 kHz)";
    case webrtc::kDecoderPCM16B:
      return "PCM16b-nb (8 kHz)";
    case webrtc::kDecoderPCM16Bwb:
      return "PCM16b-wb (16 kHz)";
    case webrtc::kDecoderPCM16Bswb32kHz:
      return "PCM16b-swb32 (32 kHz)";
    case webrtc::kDecoderPCM16Bswb48kHz:
      return "PCM16b-swb48 (48 kHz)";
    case webrtc::kDecoderG722:
      return "G.722";
    case webrtc::kDecoderRED:
      return "redundant audio (RED)";
    case webrtc::kDecoderAVT:
      return "AVT/DTMF";
    case webrtc::kDecoderCNGnb:
      return "comfort noise (8 kHz)";
    case webrtc::kDecoderCNGwb:
      return "comfort noise (16 kHz)";
    case webrtc::kDecoderCNGswb32kHz:
      return "comfort noise (32 kHz)";
    case webrtc::kDecoderCNGswb48kHz:
      return "comfort noise (48 kHz)";
    default:
      assert(false);
      return "undefined";
  }
}

// Registers all decoders in |neteq|.
void RegisterPayloadTypes(NetEq* neteq) {
  assert(neteq);
  int error;
  error = neteq->RegisterPayloadType(webrtc::kDecoderPCMu, FLAGS_pcmu);
  if (error) {
    std::cerr << "Cannot register payload type " << FLAGS_pcmu <<
        " as " << CodecName(webrtc::kDecoderPCMu).c_str() << std::endl;
    exit(1);
  }
  error = neteq->RegisterPayloadType(webrtc::kDecoderPCMa, FLAGS_pcma);
  if (error) {
    std::cerr << "Cannot register payload type " << FLAGS_pcma <<
        " as " << CodecName(webrtc::kDecoderPCMa).c_str() << std::endl;
    exit(1);
  }
  error = neteq->RegisterPayloadType(webrtc::kDecoderILBC, FLAGS_ilbc);
  if (error) {
    std::cerr << "Cannot register payload type " << FLAGS_ilbc <<
        " as " << CodecName(webrtc::kDecoderILBC).c_str() << std::endl;
    exit(1);
  }
  error = neteq->RegisterPayloadType(webrtc::kDecoderISAC, FLAGS_isac);
  if (error) {
    std::cerr << "Cannot register payload type " << FLAGS_isac <<
        " as " << CodecName(webrtc::kDecoderISAC).c_str() << std::endl;
    exit(1);
  }
  error = neteq->RegisterPayloadType(webrtc::kDecoderISACswb, FLAGS_isac_swb);
  if (error) {
    std::cerr << "Cannot register payload type " << FLAGS_isac_swb <<
        " as " << CodecName(webrtc::kDecoderISACswb).c_str() << std::endl;
    exit(1);
  }
  error = neteq->RegisterPayloadType(webrtc::kDecoderPCM16B, FLAGS_pcm16b);
  if (error) {
    std::cerr << "Cannot register payload type " << FLAGS_pcm16b <<
        " as " << CodecName(webrtc::kDecoderPCM16B).c_str() << std::endl;
    exit(1);
  }
  error = neteq->RegisterPayloadType(webrtc::kDecoderPCM16Bwb,
                                      FLAGS_pcm16b_wb);
  if (error) {
    std::cerr << "Cannot register payload type " << FLAGS_pcm16b_wb <<
        " as " << CodecName(webrtc::kDecoderPCM16Bwb).c_str() << std::endl;
    exit(1);
  }
  error = neteq->RegisterPayloadType(webrtc::kDecoderPCM16Bswb32kHz,
                                      FLAGS_pcm16b_swb32);
  if (error) {
    std::cerr << "Cannot register payload type " << FLAGS_pcm16b_swb32 <<
        " as " << CodecName(webrtc::kDecoderPCM16Bswb32kHz).c_str() <<
        std::endl;
    exit(1);
  }
  error = neteq->RegisterPayloadType(webrtc::kDecoderPCM16Bswb48kHz,
                                      FLAGS_pcm16b_swb48);
  if (error) {
    std::cerr << "Cannot register payload type " << FLAGS_pcm16b_swb48 <<
        " as " << CodecName(webrtc::kDecoderPCM16Bswb48kHz).c_str() <<
        std::endl;
    exit(1);
  }
  error = neteq->RegisterPayloadType(webrtc::kDecoderG722, FLAGS_g722);
  if (error) {
    std::cerr << "Cannot register payload type " << FLAGS_g722 <<
        " as " << CodecName(webrtc::kDecoderG722).c_str() << std::endl;
    exit(1);
  }
  error = neteq->RegisterPayloadType(webrtc::kDecoderAVT, FLAGS_avt);
  if (error) {
    std::cerr << "Cannot register payload type " << FLAGS_avt <<
        " as " << CodecName(webrtc::kDecoderAVT).c_str() << std::endl;
    exit(1);
  }
  error = neteq->RegisterPayloadType(webrtc::kDecoderRED, FLAGS_red);
  if (error) {
    std::cerr << "Cannot register payload type " << FLAGS_red <<
        " as " << CodecName(webrtc::kDecoderRED).c_str() << std::endl;
    exit(1);
  }
  error = neteq->RegisterPayloadType(webrtc::kDecoderCNGnb, FLAGS_cn_nb);
  if (error) {
    std::cerr << "Cannot register payload type " << FLAGS_cn_nb <<
        " as " << CodecName(webrtc::kDecoderCNGnb).c_str() << std::endl;
    exit(1);
  }
  error = neteq->RegisterPayloadType(webrtc::kDecoderCNGwb, FLAGS_cn_wb);
  if (error) {
    std::cerr << "Cannot register payload type " << FLAGS_cn_wb <<
        " as " << CodecName(webrtc::kDecoderCNGwb).c_str() << std::endl;
    exit(1);
  }
  error = neteq->RegisterPayloadType(webrtc::kDecoderCNGswb32kHz,
                                      FLAGS_cn_swb32);
  if (error) {
    std::cerr << "Cannot register payload type " << FLAGS_cn_swb32 <<
        " as " << CodecName(webrtc::kDecoderCNGswb32kHz).c_str() << std::endl;
    exit(1);
  }
  error = neteq->RegisterPayloadType(webrtc::kDecoderCNGswb48kHz,
                                     FLAGS_cn_swb48);
  if (error) {
    std::cerr << "Cannot register payload type " << FLAGS_cn_swb48 <<
        " as " << CodecName(webrtc::kDecoderCNGswb48kHz).c_str() << std::endl;
    exit(1);
  }
}

void PrintCodecMapping() {
  std::cout << CodecName(webrtc::kDecoderPCMu).c_str() << ": " << FLAGS_pcmu <<
      std::endl;
  std::cout << CodecName(webrtc::kDecoderPCMa).c_str() << ": " << FLAGS_pcma <<
      std::endl;
  std::cout << CodecName(webrtc::kDecoderILBC).c_str() << ": " << FLAGS_ilbc <<
      std::endl;
  std::cout << CodecName(webrtc::kDecoderISAC).c_str() << ": " << FLAGS_isac <<
      std::endl;
  std::cout << CodecName(webrtc::kDecoderISACswb).c_str() << ": " <<
      FLAGS_isac_swb << std::endl;
  std::cout << CodecName(webrtc::kDecoderPCM16B).c_str() << ": " <<
      FLAGS_pcm16b << std::endl;
  std::cout << CodecName(webrtc::kDecoderPCM16Bwb).c_str() << ": " <<
      FLAGS_pcm16b_wb << std::endl;
  std::cout << CodecName(webrtc::kDecoderPCM16Bswb32kHz).c_str() << ": " <<
      FLAGS_pcm16b_swb32 << std::endl;
  std::cout << CodecName(webrtc::kDecoderPCM16Bswb48kHz).c_str() << ": " <<
      FLAGS_pcm16b_swb48 << std::endl;
  std::cout << CodecName(webrtc::kDecoderG722).c_str() << ": " << FLAGS_g722 <<
      std::endl;
  std::cout << CodecName(webrtc::kDecoderAVT).c_str() << ": " << FLAGS_avt <<
      std::endl;
  std::cout << CodecName(webrtc::kDecoderRED).c_str() << ": " << FLAGS_red <<
      std::endl;
  std::cout << CodecName(webrtc::kDecoderCNGnb).c_str() << ": " <<
      FLAGS_cn_nb << std::endl;
  std::cout << CodecName(webrtc::kDecoderCNGwb).c_str() << ": " <<
      FLAGS_cn_wb << std::endl;
  std::cout << CodecName(webrtc::kDecoderCNGswb32kHz).c_str() << ": " <<
      FLAGS_cn_swb32 << std::endl;
  std::cout << CodecName(webrtc::kDecoderCNGswb48kHz).c_str() << ": " <<
      FLAGS_cn_swb48 << std::endl;
}
