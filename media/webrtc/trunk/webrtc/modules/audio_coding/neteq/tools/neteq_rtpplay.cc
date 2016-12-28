/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO(hlundin): The functionality in this file should be moved into one or
// several classes.

#include <assert.h>
#include <errno.h>
#include <limits.h>  // For ULONG_MAX returned by strtoul.
#include <stdio.h>
#include <stdlib.h>  // For strtoul.

#include <algorithm>
#include <iostream>
#include <limits>
#include <string>

#include "google/gflags.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_coding/codecs/pcm16b/pcm16b.h"
#include "webrtc/modules/audio_coding/neteq/include/neteq.h"
#include "webrtc/modules/audio_coding/neteq/tools/input_audio_file.h"
#include "webrtc/modules/audio_coding/neteq/tools/output_audio_file.h"
#include "webrtc/modules/audio_coding/neteq/tools/output_wav_file.h"
#include "webrtc/modules/audio_coding/neteq/tools/packet.h"
#include "webrtc/modules/audio_coding/neteq/tools/rtc_event_log_source.h"
#include "webrtc/modules/audio_coding/neteq/tools/rtp_file_source.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/system_wrappers/include/trace.h"
#include "webrtc/test/rtp_file_reader.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/typedefs.h"

using webrtc::NetEq;
using webrtc::WebRtcRTPHeader;

namespace {

// Parses the input string for a valid SSRC (at the start of the string). If a
// valid SSRC is found, it is written to the output variable |ssrc|, and true is
// returned. Otherwise, false is returned.
bool ParseSsrc(const std::string& str, uint32_t* ssrc) {
  if (str.empty())
    return true;
  int base = 10;
  // Look for "0x" or "0X" at the start and change base to 16 if found.
  if ((str.compare(0, 2, "0x") == 0) || (str.compare(0, 2, "0X") == 0))
    base = 16;
  errno = 0;
  char* end_ptr;
  unsigned long value = strtoul(str.c_str(), &end_ptr, base);
  if (value == ULONG_MAX && errno == ERANGE)
    return false;  // Value out of range for unsigned long.
  if (sizeof(unsigned long) > sizeof(uint32_t) && value > 0xFFFFFFFF)
    return false;  // Value out of range for uint32_t.
  if (end_ptr - str.c_str() < static_cast<ptrdiff_t>(str.length()))
    return false;  // Part of the string was not parsed.
  *ssrc = static_cast<uint32_t>(value);
  return true;
}

// Flag validators.
bool ValidatePayloadType(const char* flagname, int32_t value) {
  if (value >= 0 && value <= 127)  // Value is ok.
    return true;
  printf("Invalid value for --%s: %d\n", flagname, static_cast<int>(value));
  return false;
}

bool ValidateSsrcValue(const char* flagname, const std::string& str) {
  uint32_t dummy_ssrc;
  return ParseSsrc(str, &dummy_ssrc);
}

// Define command line flags.
DEFINE_int32(pcmu, 0, "RTP payload type for PCM-u");
const bool pcmu_dummy =
    google::RegisterFlagValidator(&FLAGS_pcmu, &ValidatePayloadType);
DEFINE_int32(pcma, 8, "RTP payload type for PCM-a");
const bool pcma_dummy =
    google::RegisterFlagValidator(&FLAGS_pcma, &ValidatePayloadType);
DEFINE_int32(ilbc, 102, "RTP payload type for iLBC");
const bool ilbc_dummy =
    google::RegisterFlagValidator(&FLAGS_ilbc, &ValidatePayloadType);
DEFINE_int32(isac, 103, "RTP payload type for iSAC");
const bool isac_dummy =
    google::RegisterFlagValidator(&FLAGS_isac, &ValidatePayloadType);
DEFINE_int32(isac_swb, 104, "RTP payload type for iSAC-swb (32 kHz)");
const bool isac_swb_dummy =
    google::RegisterFlagValidator(&FLAGS_isac_swb, &ValidatePayloadType);
DEFINE_int32(opus, 111, "RTP payload type for Opus");
const bool opus_dummy =
    google::RegisterFlagValidator(&FLAGS_opus, &ValidatePayloadType);
DEFINE_int32(pcm16b, 93, "RTP payload type for PCM16b-nb (8 kHz)");
const bool pcm16b_dummy =
    google::RegisterFlagValidator(&FLAGS_pcm16b, &ValidatePayloadType);
DEFINE_int32(pcm16b_wb, 94, "RTP payload type for PCM16b-wb (16 kHz)");
const bool pcm16b_wb_dummy =
    google::RegisterFlagValidator(&FLAGS_pcm16b_wb, &ValidatePayloadType);
DEFINE_int32(pcm16b_swb32, 95, "RTP payload type for PCM16b-swb32 (32 kHz)");
const bool pcm16b_swb32_dummy =
    google::RegisterFlagValidator(&FLAGS_pcm16b_swb32, &ValidatePayloadType);
DEFINE_int32(pcm16b_swb48, 96, "RTP payload type for PCM16b-swb48 (48 kHz)");
const bool pcm16b_swb48_dummy =
    google::RegisterFlagValidator(&FLAGS_pcm16b_swb48, &ValidatePayloadType);
DEFINE_int32(g722, 9, "RTP payload type for G.722");
const bool g722_dummy =
    google::RegisterFlagValidator(&FLAGS_g722, &ValidatePayloadType);
DEFINE_int32(avt, 106, "RTP payload type for AVT/DTMF");
const bool avt_dummy =
    google::RegisterFlagValidator(&FLAGS_avt, &ValidatePayloadType);
DEFINE_int32(red, 117, "RTP payload type for redundant audio (RED)");
const bool red_dummy =
    google::RegisterFlagValidator(&FLAGS_red, &ValidatePayloadType);
DEFINE_int32(cn_nb, 13, "RTP payload type for comfort noise (8 kHz)");
const bool cn_nb_dummy =
    google::RegisterFlagValidator(&FLAGS_cn_nb, &ValidatePayloadType);
DEFINE_int32(cn_wb, 98, "RTP payload type for comfort noise (16 kHz)");
const bool cn_wb_dummy =
    google::RegisterFlagValidator(&FLAGS_cn_wb, &ValidatePayloadType);
DEFINE_int32(cn_swb32, 99, "RTP payload type for comfort noise (32 kHz)");
const bool cn_swb32_dummy =
    google::RegisterFlagValidator(&FLAGS_cn_swb32, &ValidatePayloadType);
DEFINE_int32(cn_swb48, 100, "RTP payload type for comfort noise (48 kHz)");
const bool cn_swb48_dummy =
    google::RegisterFlagValidator(&FLAGS_cn_swb48, &ValidatePayloadType);
DEFINE_bool(codec_map, false, "Prints the mapping between RTP payload type and "
    "codec");
DEFINE_string(replacement_audio_file, "",
              "A PCM file that will be used to populate ""dummy"" RTP packets");
DEFINE_string(ssrc,
              "",
              "Only use packets with this SSRC (decimal or hex, the latter "
              "starting with 0x)");
const bool hex_ssrc_dummy =
    google::RegisterFlagValidator(&FLAGS_ssrc, &ValidateSsrcValue);

// Maps a codec type to a printable name string.
std::string CodecName(webrtc::NetEqDecoder codec) {
  switch (codec) {
    case webrtc::NetEqDecoder::kDecoderPCMu:
      return "PCM-u";
    case webrtc::NetEqDecoder::kDecoderPCMa:
      return "PCM-a";
    case webrtc::NetEqDecoder::kDecoderILBC:
      return "iLBC";
    case webrtc::NetEqDecoder::kDecoderISAC:
      return "iSAC";
    case webrtc::NetEqDecoder::kDecoderISACswb:
      return "iSAC-swb (32 kHz)";
    case webrtc::NetEqDecoder::kDecoderOpus:
      return "Opus";
    case webrtc::NetEqDecoder::kDecoderPCM16B:
      return "PCM16b-nb (8 kHz)";
    case webrtc::NetEqDecoder::kDecoderPCM16Bwb:
      return "PCM16b-wb (16 kHz)";
    case webrtc::NetEqDecoder::kDecoderPCM16Bswb32kHz:
      return "PCM16b-swb32 (32 kHz)";
    case webrtc::NetEqDecoder::kDecoderPCM16Bswb48kHz:
      return "PCM16b-swb48 (48 kHz)";
    case webrtc::NetEqDecoder::kDecoderG722:
      return "G.722";
    case webrtc::NetEqDecoder::kDecoderRED:
      return "redundant audio (RED)";
    case webrtc::NetEqDecoder::kDecoderAVT:
      return "AVT/DTMF";
    case webrtc::NetEqDecoder::kDecoderCNGnb:
      return "comfort noise (8 kHz)";
    case webrtc::NetEqDecoder::kDecoderCNGwb:
      return "comfort noise (16 kHz)";
    case webrtc::NetEqDecoder::kDecoderCNGswb32kHz:
      return "comfort noise (32 kHz)";
    case webrtc::NetEqDecoder::kDecoderCNGswb48kHz:
      return "comfort noise (48 kHz)";
    default:
      assert(false);
      return "undefined";
  }
}

void RegisterPayloadType(NetEq* neteq,
                         webrtc::NetEqDecoder codec,
                         const std::string& name,
                         google::int32 flag) {
  if (neteq->RegisterPayloadType(codec, name, static_cast<uint8_t>(flag))) {
    std::cerr << "Cannot register payload type " << flag << " as "
              << CodecName(codec) << std::endl;
    exit(1);
  }
}

// Registers all decoders in |neteq|.
void RegisterPayloadTypes(NetEq* neteq) {
  assert(neteq);
  RegisterPayloadType(neteq, webrtc::NetEqDecoder::kDecoderPCMu, "pcmu",
                      FLAGS_pcmu);
  RegisterPayloadType(neteq, webrtc::NetEqDecoder::kDecoderPCMa, "pcma",
                      FLAGS_pcma);
  RegisterPayloadType(neteq, webrtc::NetEqDecoder::kDecoderILBC, "ilbc",
                      FLAGS_ilbc);
  RegisterPayloadType(neteq, webrtc::NetEqDecoder::kDecoderISAC, "isac",
                      FLAGS_isac);
  RegisterPayloadType(neteq, webrtc::NetEqDecoder::kDecoderISACswb, "isac-swb",
                      FLAGS_isac_swb);
  RegisterPayloadType(neteq, webrtc::NetEqDecoder::kDecoderOpus, "opus",
                      FLAGS_opus);
  RegisterPayloadType(neteq, webrtc::NetEqDecoder::kDecoderPCM16B, "pcm16-nb",
                      FLAGS_pcm16b);
  RegisterPayloadType(neteq, webrtc::NetEqDecoder::kDecoderPCM16Bwb, "pcm16-wb",
                      FLAGS_pcm16b_wb);
  RegisterPayloadType(neteq, webrtc::NetEqDecoder::kDecoderPCM16Bswb32kHz,
                      "pcm16-swb32", FLAGS_pcm16b_swb32);
  RegisterPayloadType(neteq, webrtc::NetEqDecoder::kDecoderPCM16Bswb48kHz,
                      "pcm16-swb48", FLAGS_pcm16b_swb48);
  RegisterPayloadType(neteq, webrtc::NetEqDecoder::kDecoderG722, "g722",
                      FLAGS_g722);
  RegisterPayloadType(neteq, webrtc::NetEqDecoder::kDecoderAVT, "avt",
                      FLAGS_avt);
  RegisterPayloadType(neteq, webrtc::NetEqDecoder::kDecoderRED, "red",
                      FLAGS_red);
  RegisterPayloadType(neteq, webrtc::NetEqDecoder::kDecoderCNGnb, "cng-nb",
                      FLAGS_cn_nb);
  RegisterPayloadType(neteq, webrtc::NetEqDecoder::kDecoderCNGwb, "cng-wb",
                      FLAGS_cn_wb);
  RegisterPayloadType(neteq, webrtc::NetEqDecoder::kDecoderCNGswb32kHz,
                      "cng-swb32", FLAGS_cn_swb32);
  RegisterPayloadType(neteq, webrtc::NetEqDecoder::kDecoderCNGswb48kHz,
                      "cng-swb48", FLAGS_cn_swb48);
}

void PrintCodecMappingEntry(webrtc::NetEqDecoder codec, google::int32 flag) {
  std::cout << CodecName(codec) << ": " << flag << std::endl;
}

void PrintCodecMapping() {
  PrintCodecMappingEntry(webrtc::NetEqDecoder::kDecoderPCMu, FLAGS_pcmu);
  PrintCodecMappingEntry(webrtc::NetEqDecoder::kDecoderPCMa, FLAGS_pcma);
  PrintCodecMappingEntry(webrtc::NetEqDecoder::kDecoderILBC, FLAGS_ilbc);
  PrintCodecMappingEntry(webrtc::NetEqDecoder::kDecoderISAC, FLAGS_isac);
  PrintCodecMappingEntry(webrtc::NetEqDecoder::kDecoderISACswb, FLAGS_isac_swb);
  PrintCodecMappingEntry(webrtc::NetEqDecoder::kDecoderOpus, FLAGS_opus);
  PrintCodecMappingEntry(webrtc::NetEqDecoder::kDecoderPCM16B, FLAGS_pcm16b);
  PrintCodecMappingEntry(webrtc::NetEqDecoder::kDecoderPCM16Bwb,
                         FLAGS_pcm16b_wb);
  PrintCodecMappingEntry(webrtc::NetEqDecoder::kDecoderPCM16Bswb32kHz,
                         FLAGS_pcm16b_swb32);
  PrintCodecMappingEntry(webrtc::NetEqDecoder::kDecoderPCM16Bswb48kHz,
                         FLAGS_pcm16b_swb48);
  PrintCodecMappingEntry(webrtc::NetEqDecoder::kDecoderG722, FLAGS_g722);
  PrintCodecMappingEntry(webrtc::NetEqDecoder::kDecoderAVT, FLAGS_avt);
  PrintCodecMappingEntry(webrtc::NetEqDecoder::kDecoderRED, FLAGS_red);
  PrintCodecMappingEntry(webrtc::NetEqDecoder::kDecoderCNGnb, FLAGS_cn_nb);
  PrintCodecMappingEntry(webrtc::NetEqDecoder::kDecoderCNGwb, FLAGS_cn_wb);
  PrintCodecMappingEntry(webrtc::NetEqDecoder::kDecoderCNGswb32kHz,
                         FLAGS_cn_swb32);
  PrintCodecMappingEntry(webrtc::NetEqDecoder::kDecoderCNGswb48kHz,
                         FLAGS_cn_swb48);
}

bool IsComfortNoise(uint8_t payload_type) {
  return payload_type == FLAGS_cn_nb || payload_type == FLAGS_cn_wb ||
      payload_type == FLAGS_cn_swb32 || payload_type == FLAGS_cn_swb48;
}

int CodecSampleRate(uint8_t payload_type) {
  if (payload_type == FLAGS_pcmu || payload_type == FLAGS_pcma ||
      payload_type == FLAGS_ilbc || payload_type == FLAGS_pcm16b ||
      payload_type == FLAGS_cn_nb)
    return 8000;
  if (payload_type == FLAGS_isac || payload_type == FLAGS_pcm16b_wb ||
      payload_type == FLAGS_g722 || payload_type == FLAGS_cn_wb)
    return 16000;
  if (payload_type == FLAGS_isac_swb || payload_type == FLAGS_pcm16b_swb32 ||
      payload_type == FLAGS_cn_swb32)
    return 32000;
  if (payload_type == FLAGS_opus || payload_type == FLAGS_pcm16b_swb48 ||
      payload_type == FLAGS_cn_swb48)
    return 48000;
  if (payload_type == FLAGS_avt || payload_type == FLAGS_red)
    return 0;
  return -1;
}

int CodecTimestampRate(uint8_t payload_type) {
  return (payload_type == FLAGS_g722) ? 8000 : CodecSampleRate(payload_type);
}

size_t ReplacePayload(webrtc::test::InputAudioFile* replacement_audio_file,
                      rtc::scoped_ptr<int16_t[]>* replacement_audio,
                      rtc::scoped_ptr<uint8_t[]>* payload,
                      size_t* payload_mem_size_bytes,
                      size_t* frame_size_samples,
                      WebRtcRTPHeader* rtp_header,
                      const webrtc::test::Packet* next_packet) {
  size_t payload_len = 0;
  // Check for CNG.
  if (IsComfortNoise(rtp_header->header.payloadType)) {
    // If CNG, simply insert a zero-energy one-byte payload.
    if (*payload_mem_size_bytes < 1) {
      (*payload).reset(new uint8_t[1]);
      *payload_mem_size_bytes = 1;
    }
    (*payload)[0] = 127;  // Max attenuation of CNG.
    payload_len = 1;
  } else {
    assert(next_packet->virtual_payload_length_bytes() > 0);
    // Check if payload length has changed.
    if (next_packet->header().sequenceNumber ==
        rtp_header->header.sequenceNumber + 1) {
      if (*frame_size_samples !=
          next_packet->header().timestamp - rtp_header->header.timestamp) {
        *frame_size_samples =
            next_packet->header().timestamp - rtp_header->header.timestamp;
        (*replacement_audio).reset(
            new int16_t[*frame_size_samples]);
        *payload_mem_size_bytes = 2 * *frame_size_samples;
        (*payload).reset(new uint8_t[*payload_mem_size_bytes]);
      }
    }
    // Get new speech.
    assert((*replacement_audio).get());
    if (CodecTimestampRate(rtp_header->header.payloadType) !=
        CodecSampleRate(rtp_header->header.payloadType) ||
        rtp_header->header.payloadType == FLAGS_red ||
        rtp_header->header.payloadType == FLAGS_avt) {
      // Some codecs have different sample and timestamp rates. And neither
      // RED nor DTMF is supported for replacement.
      std::cerr << "Codec not supported for audio replacement." <<
          std::endl;
      webrtc::Trace::ReturnTrace();
      exit(1);
    }
    assert(*frame_size_samples > 0);
    if (!replacement_audio_file->Read(*frame_size_samples,
                                      (*replacement_audio).get())) {
      std::cerr << "Could not read replacement audio file." << std::endl;
      webrtc::Trace::ReturnTrace();
      exit(1);
    }
    // Encode it as PCM16.
    assert((*payload).get());
    payload_len = WebRtcPcm16b_Encode((*replacement_audio).get(),
                                      *frame_size_samples,
                                      (*payload).get());
    assert(payload_len == 2 * *frame_size_samples);
    // Change payload type to PCM16.
    switch (CodecSampleRate(rtp_header->header.payloadType)) {
      case 8000:
        rtp_header->header.payloadType = static_cast<uint8_t>(FLAGS_pcm16b);
        break;
      case 16000:
        rtp_header->header.payloadType = static_cast<uint8_t>(FLAGS_pcm16b_wb);
        break;
      case 32000:
        rtp_header->header.payloadType =
            static_cast<uint8_t>(FLAGS_pcm16b_swb32);
        break;
      case 48000:
        rtp_header->header.payloadType =
            static_cast<uint8_t>(FLAGS_pcm16b_swb48);
        break;
      default:
        std::cerr << "Payload type " <<
            static_cast<int>(rtp_header->header.payloadType) <<
            " not supported or unknown." << std::endl;
        webrtc::Trace::ReturnTrace();
        exit(1);
    }
  }
  return payload_len;
}

}  // namespace

int main(int argc, char* argv[]) {
  static const int kMaxChannels = 5;
  static const size_t kMaxSamplesPerMs = 48000 / 1000;
  static const int kOutputBlockSizeMs = 10;

  std::string program_name = argv[0];
  std::string usage = "Tool for decoding an RTP dump file using NetEq.\n"
      "Run " + program_name + " --helpshort for usage.\n"
      "Example usage:\n" + program_name +
      " input.rtp output.{pcm, wav}\n";
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

  printf("Input file: %s\n", argv[1]);

  bool is_rtp_dump = false;
  rtc::scoped_ptr<webrtc::test::PacketSource> file_source;
  webrtc::test::RtcEventLogSource* event_log_source = nullptr;
  if (webrtc::test::RtpFileSource::ValidRtpDump(argv[1]) ||
      webrtc::test::RtpFileSource::ValidPcap(argv[1])) {
    is_rtp_dump = true;
    file_source.reset(webrtc::test::RtpFileSource::Create(argv[1]));
  } else {
    event_log_source = webrtc::test::RtcEventLogSource::Create(argv[1]);
    file_source.reset(event_log_source);
  }

  assert(file_source.get());

  // Check if an SSRC value was provided.
  if (!FLAGS_ssrc.empty()) {
    uint32_t ssrc;
    RTC_CHECK(ParseSsrc(FLAGS_ssrc, &ssrc)) << "Flag verification has failed.";
    file_source->SelectSsrc(ssrc);
  }

  // Check if a replacement audio file was provided, and if so, open it.
  bool replace_payload = false;
  rtc::scoped_ptr<webrtc::test::InputAudioFile> replacement_audio_file;
  if (!FLAGS_replacement_audio_file.empty()) {
    replacement_audio_file.reset(
        new webrtc::test::InputAudioFile(FLAGS_replacement_audio_file));
    replace_payload = true;
  }

  // Read first packet.
  rtc::scoped_ptr<webrtc::test::Packet> packet(file_source->NextPacket());
  if (!packet) {
    printf(
        "Warning: input file is empty, or the filters did not match any "
        "packets\n");
    webrtc::Trace::ReturnTrace();
    return 0;
  }
  if (packet->payload_length_bytes() == 0 && !replace_payload) {
    std::cerr << "Warning: input file contains header-only packets, but no "
              << "replacement file is specified." << std::endl;
    webrtc::Trace::ReturnTrace();
    return -1;
  }

  // Check the sample rate.
  int sample_rate_hz = CodecSampleRate(packet->header().payloadType);
  if (sample_rate_hz <= 0) {
    printf("Warning: Invalid sample rate from RTP packet.\n");
    webrtc::Trace::ReturnTrace();
    return 0;
  }

  // Open the output file now that we know the sample rate. (Rate is only needed
  // for wav files.)
  // Check output file type.
  std::string output_file_name = argv[2];
  rtc::scoped_ptr<webrtc::test::AudioSink> output;
  if (output_file_name.size() >= 4 &&
      output_file_name.substr(output_file_name.size() - 4) == ".wav") {
    // Open a wav file.
    output.reset(
        new webrtc::test::OutputWavFile(output_file_name, sample_rate_hz));
  } else {
    // Open a pcm file.
    output.reset(new webrtc::test::OutputAudioFile(output_file_name));
  }

  std::cout << "Output file: " << argv[2] << std::endl;

  // Enable tracing.
  webrtc::Trace::CreateTrace();
  webrtc::Trace::SetTraceFile((webrtc::test::OutputPath() +
      "neteq_trace.txt").c_str());
  webrtc::Trace::set_level_filter(webrtc::kTraceAll);

  // Initialize NetEq instance.
  NetEq::Config config;
  config.sample_rate_hz = sample_rate_hz;
  NetEq* neteq = NetEq::Create(config);
  RegisterPayloadTypes(neteq);


  // Set up variables for audio replacement if needed.
  rtc::scoped_ptr<webrtc::test::Packet> next_packet;
  bool next_packet_available = false;
  size_t input_frame_size_timestamps = 0;
  rtc::scoped_ptr<int16_t[]> replacement_audio;
  rtc::scoped_ptr<uint8_t[]> payload;
  size_t payload_mem_size_bytes = 0;
  if (replace_payload) {
    // Initially assume that the frame size is 30 ms at the initial sample rate.
    // This value will be replaced with the correct one as soon as two
    // consecutive packets are found.
    input_frame_size_timestamps = 30 * sample_rate_hz / 1000;
    replacement_audio.reset(new int16_t[input_frame_size_timestamps]);
    payload_mem_size_bytes = 2 * input_frame_size_timestamps;
    payload.reset(new uint8_t[payload_mem_size_bytes]);
    next_packet.reset(file_source->NextPacket());
    assert(next_packet);
    next_packet_available = true;
  }

  // This is the main simulation loop.
  // Set the simulation clock to start immediately with the first packet.
  int64_t start_time_ms = rtc::checked_cast<int64_t>(packet->time_ms());
  int64_t time_now_ms = start_time_ms;
  int64_t next_input_time_ms = time_now_ms;
  int64_t next_output_time_ms = time_now_ms;
  if (time_now_ms % kOutputBlockSizeMs != 0) {
    // Make sure that next_output_time_ms is rounded up to the next multiple
    // of kOutputBlockSizeMs. (Legacy bit-exactness.)
    next_output_time_ms +=
        kOutputBlockSizeMs - time_now_ms % kOutputBlockSizeMs;
  }

  bool packet_available = true;
  bool output_event_available = true;
  if (!is_rtp_dump) {
    next_output_time_ms = event_log_source->NextAudioOutputEventMs();
    if (next_output_time_ms == std::numeric_limits<int64_t>::max())
      output_event_available = false;
    start_time_ms = time_now_ms =
        std::min(next_input_time_ms, next_output_time_ms);
  }
  while (packet_available || output_event_available) {
    // Advance time to next event.
    time_now_ms = std::min(next_input_time_ms, next_output_time_ms);
    // Check if it is time to insert packet.
    while (time_now_ms >= next_input_time_ms && packet_available) {
      assert(packet->virtual_payload_length_bytes() > 0);
      // Parse RTP header.
      WebRtcRTPHeader rtp_header;
      packet->ConvertHeader(&rtp_header);
      const uint8_t* payload_ptr = packet->payload();
      size_t payload_len = packet->payload_length_bytes();
      if (replace_payload) {
        payload_len = ReplacePayload(replacement_audio_file.get(),
                                     &replacement_audio,
                                     &payload,
                                     &payload_mem_size_bytes,
                                     &input_frame_size_timestamps,
                                     &rtp_header,
                                     next_packet.get());
        payload_ptr = payload.get();
      }
      int error = neteq->InsertPacket(
          rtp_header, rtc::ArrayView<const uint8_t>(payload_ptr, payload_len),
          static_cast<uint32_t>(packet->time_ms() * sample_rate_hz / 1000));
      if (error != NetEq::kOK) {
        if (neteq->LastError() == NetEq::kUnknownRtpPayloadType) {
          std::cerr << "RTP Payload type "
                    << static_cast<int>(rtp_header.header.payloadType)
                    << " is unknown." << std::endl;
          std::cerr << "Use --codec_map to view default mapping." << std::endl;
          std::cerr << "Use --helpshort for information on how to make custom "
                       "mappings." << std::endl;
        } else {
          std::cerr << "InsertPacket returned error code " << neteq->LastError()
                    << std::endl;
          std::cerr << "Header data:" << std::endl;
          std::cerr << "  PT = "
                    << static_cast<int>(rtp_header.header.payloadType)
                    << std::endl;
          std::cerr << "  SN = " << rtp_header.header.sequenceNumber
                    << std::endl;
          std::cerr << "  TS = " << rtp_header.header.timestamp << std::endl;
        }
      }

      // Get next packet from file.
      webrtc::test::Packet* temp_packet = file_source->NextPacket();
      if (temp_packet) {
        packet.reset(temp_packet);
        if (replace_payload) {
          // At this point |packet| contains the packet *after* |next_packet|.
          // Swap Packet objects between |packet| and |next_packet|.
          packet.swap(next_packet);
          // Swap the status indicators unless they're already the same.
          if (packet_available != next_packet_available) {
            packet_available = !packet_available;
            next_packet_available = !next_packet_available;
          }
        }
        next_input_time_ms = rtc::checked_cast<int64_t>(packet->time_ms());
      } else {
        // Set next input time to the maximum value of int64_t to prevent the
        // time_now_ms from becoming stuck at the final value.
        next_input_time_ms = std::numeric_limits<int64_t>::max();
        packet_available = false;
      }
    }

    // Check if it is time to get output audio.
    while (time_now_ms >= next_output_time_ms && output_event_available) {
      static const size_t kOutDataLen =
          kOutputBlockSizeMs * kMaxSamplesPerMs * kMaxChannels;
      int16_t out_data[kOutDataLen];
      size_t num_channels;
      size_t samples_per_channel;
      int error = neteq->GetAudio(kOutDataLen, out_data, &samples_per_channel,
                                   &num_channels, NULL);
      if (error != NetEq::kOK) {
        std::cerr << "GetAudio returned error code " <<
            neteq->LastError() << std::endl;
      } else {
        // Calculate sample rate from output size.
        sample_rate_hz = rtc::checked_cast<int>(
            1000 * samples_per_channel / kOutputBlockSizeMs);
      }

      // Write to file.
      // TODO(hlundin): Make writing to file optional.
      size_t write_len = samples_per_channel * num_channels;
      if (!output->WriteArray(out_data, write_len)) {
        std::cerr << "Error while writing to file" << std::endl;
        webrtc::Trace::ReturnTrace();
        exit(1);
      }
      if (is_rtp_dump) {
        next_output_time_ms += kOutputBlockSizeMs;
        if (!packet_available)
          output_event_available = false;
      } else {
        next_output_time_ms = event_log_source->NextAudioOutputEventMs();
        if (next_output_time_ms == std::numeric_limits<int64_t>::max())
          output_event_available = false;
      }
    }
  }
  printf("Simulation done\n");
  printf("Produced %i ms of audio\n",
         static_cast<int>(time_now_ms - start_time_ms));

  delete neteq;
  webrtc::Trace::ReturnTrace();
  return 0;
}
