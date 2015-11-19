/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file generates databases with information about all supported audio
 * codecs.
 */

// TODO(tlegrand): Change constant input pointers in all functions to constant
// references, where appropriate.
#include "webrtc/modules/audio_coding/main/acm2/acm_codec_database.h"

#include <assert.h>

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_common_defs.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

namespace acm2 {

// Not yet used payload-types.
// 83,  82,  81, 80, 79,  78,  77,  76,  75,  74,  73,  72,  71,  70,  69, 68,
// 67, 66, 65

const CodecInst ACMCodecDB::database_[] = {
#if (defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX))
  {103, "ISAC", 16000, kIsacPacSize480, 1, kIsacWbDefaultRate},
# if (defined(WEBRTC_CODEC_ISAC))
  {104, "ISAC", 32000, kIsacPacSize960, 1, kIsacSwbDefaultRate},
  {105, "ISAC", 48000, kIsacPacSize1440, 1, kIsacSwbDefaultRate},
# endif
#endif
#ifdef WEBRTC_CODEC_PCM16
  // Mono
  {107, "L16", 8000, 80, 1, 128000},
  {108, "L16", 16000, 160, 1, 256000},
  {109, "L16", 32000, 320, 1, 512000},
  // Stereo
  {111, "L16", 8000, 80, 2, 128000},
  {112, "L16", 16000, 160, 2, 256000},
  {113, "L16", 32000, 320, 2, 512000},
#endif
  // G.711, PCM mu-law and A-law.
  // Mono
  {0, "PCMU", 8000, 160, 1, 64000},
  {8, "PCMA", 8000, 160, 1, 64000},
  // Stereo
  {110, "PCMU", 8000, 160, 2, 64000},
  {118, "PCMA", 8000, 160, 2, 64000},
#ifdef WEBRTC_CODEC_ILBC
  {102, "ILBC", 8000, 240, 1, 13300},
#endif
#ifdef WEBRTC_CODEC_G722
  // Mono
  {9, "G722", 16000, 320, 1, 64000},
  // Stereo
  {119, "G722", 16000, 320, 2, 64000},
#endif
#ifdef WEBRTC_CODEC_OPUS
  // Opus internally supports 48, 24, 16, 12, 8 kHz.
  // Mono and stereo.
  {120, "opus", 48000, 960, 2, 64000},
#endif
  // Comfort noise for four different sampling frequencies.
  {13, "CN", 8000, 240, 1, 0},
  {98, "CN", 16000, 480, 1, 0},
  {99, "CN", 32000, 960, 1, 0},
#ifdef ENABLE_48000_HZ
  {100, "CN", 48000, 1440, 1, 0},
#endif
#ifdef WEBRTC_CODEC_AVT
  {106, "telephone-event", 8000, 240, 1, 0},
#endif
#ifdef WEBRTC_CODEC_RED
  {127, "red", 8000, 0, 1, 0},
#endif
  // To prevent compile errors due to trailing commas.
  {-1, "Null", -1, -1, -1, -1}
};

// Create database with all codec settings at compile time.
// Each entry needs the following parameters in the given order:
// Number of allowed packet sizes, a vector with the allowed packet sizes,
// Basic block samples, max number of channels that are supported.
const ACMCodecDB::CodecSettings ACMCodecDB::codec_settings_[] = {
#if (defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX))
    {2, {kIsacPacSize480, kIsacPacSize960}, 0, 1, true},
# if (defined(WEBRTC_CODEC_ISAC))
    {1, {kIsacPacSize960}, 0, 1, true},
    {1, {kIsacPacSize1440}, 0, 1, true},
# endif
#endif
#ifdef WEBRTC_CODEC_PCM16
    // Mono
    {4, {80, 160, 240, 320}, 0, 2, false},
    {4, {160, 320, 480, 640}, 0, 2, false},
    {2, {320, 640}, 0, 2, false},
    // Stereo
    {4, {80, 160, 240, 320}, 0, 2, false},
    {4, {160, 320, 480, 640}, 0, 2, false},
    {2, {320, 640}, 0, 2},
#endif
    // G.711, PCM mu-law and A-law.
    // Mono
    {6, {80, 160, 240, 320, 400, 480}, 0, 2, false},
    {6, {80, 160, 240, 320, 400, 480}, 0, 2, false},
    // Stereo
    {6, {80, 160, 240, 320, 400, 480}, 0, 2, false},
    {6, {80, 160, 240, 320, 400, 480}, 0, 2, false},
#ifdef WEBRTC_CODEC_ILBC
    {4, {160, 240, 320, 480}, 0, 1, false},
#endif
#ifdef WEBRTC_CODEC_G722
    // Mono
    {6, {160, 320, 480, 640, 800, 960}, 0, 2, false},
    // Stereo
    {6, {160, 320, 480, 640, 800, 960}, 0, 2, false},
#endif
#ifdef WEBRTC_CODEC_OPUS
    // Opus supports frames shorter than 10ms,
    // but it doesn't help us to use them.
    // Mono and stereo.
    {4, {480, 960, 1920, 2880}, 0, 2, false},
#endif
    // Comfort noise for three different sampling frequencies.
    {1, {240}, 240, 1, false},
    {1, {480}, 480, 1, false},
    {1, {960}, 960, 1, false},
#ifdef ENABLE_48000_HZ
    {1, {1440}, 1440, 1, false},
#endif
#ifdef WEBRTC_CODEC_AVT
    {1, {240}, 240, 1, false},
#endif
#ifdef WEBRTC_CODEC_RED
    {1, {0}, 0, 1, false},
#endif
    // To prevent compile errors due to trailing commas.
    {-1, {-1}, -1, -1, false}
};

// Create a database of all NetEQ decoders at compile time.
const NetEqDecoder ACMCodecDB::neteq_decoders_[] = {
#if (defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX))
    kDecoderISAC,
# if (defined(WEBRTC_CODEC_ISAC))
    kDecoderISACswb,
    kDecoderISACfb,
# endif
#endif
#ifdef WEBRTC_CODEC_PCM16
    // Mono
    kDecoderPCM16B,
    kDecoderPCM16Bwb,
    kDecoderPCM16Bswb32kHz,
    // Stereo
    kDecoderPCM16B_2ch,
    kDecoderPCM16Bwb_2ch,
    kDecoderPCM16Bswb32kHz_2ch,
#endif
    // G.711, PCM mu-las and A-law.
    // Mono
    kDecoderPCMu,
    kDecoderPCMa,
    // Stereo
    kDecoderPCMu_2ch,
    kDecoderPCMa_2ch,
#ifdef WEBRTC_CODEC_ILBC
    kDecoderILBC,
#endif
#ifdef WEBRTC_CODEC_G722
    // Mono
    kDecoderG722,
    // Stereo
    kDecoderG722_2ch,
#endif
#ifdef WEBRTC_CODEC_OPUS
    // Mono and stereo.
    kDecoderOpus,
#endif
    // Comfort noise for three different sampling frequencies.
    kDecoderCNGnb,
    kDecoderCNGwb,
    kDecoderCNGswb32kHz
#ifdef ENABLE_48000_HZ
    , kDecoderCNGswb48kHz
#endif
#ifdef WEBRTC_CODEC_AVT
    , kDecoderAVT
#endif
#ifdef WEBRTC_CODEC_RED
    , kDecoderRED
#endif
};

// Get codec information from database.
// TODO(tlegrand): replace memcpy with a pointer to the data base memory.
int ACMCodecDB::Codec(int codec_id, CodecInst* codec_inst) {
  // Error check to see that codec_id is not out of bounds.
  if ((codec_id < 0) || (codec_id >= kNumCodecs)) {
    return -1;
  }

  // Copy database information for the codec to the output.
  memcpy(codec_inst, &database_[codec_id], sizeof(CodecInst));

  return 0;
}

// Enumerator for error codes when asking for codec database id.
enum {
  kInvalidCodec = -10,
  kInvalidPayloadtype = -30,
  kInvalidPacketSize = -40,
  kInvalidRate = -50
};

// Gets the codec id number from the database. If there is some mismatch in
// the codec settings, the function will return an error code.
// NOTE! The first mismatch found will generate the return value.
int ACMCodecDB::CodecNumber(const CodecInst& codec_inst, int* mirror_id) {
  // Look for a matching codec in the database.
  int codec_id = CodecId(codec_inst);

  // Checks if we found a matching codec.
  if (codec_id == -1) {
    return kInvalidCodec;
  }

  // Checks the validity of payload type
  if (!ValidPayloadType(codec_inst.pltype)) {
    return kInvalidPayloadtype;
  }

  // Comfort Noise is special case, packet-size & rate is not checked.
  if (STR_CASE_CMP(database_[codec_id].plname, "CN") == 0) {
    *mirror_id = codec_id;
    return codec_id;
  }

  // RED is special case, packet-size & rate is not checked.
  if (STR_CASE_CMP(database_[codec_id].plname, "red") == 0) {
    *mirror_id = codec_id;
    return codec_id;
  }

  // Checks the validity of packet size.
  if (codec_settings_[codec_id].num_packet_sizes > 0) {
    bool packet_size_ok = false;
    int i;
    int packet_size_samples;
    for (i = 0; i < codec_settings_[codec_id].num_packet_sizes; i++) {
      packet_size_samples =
          codec_settings_[codec_id].packet_sizes_samples[i];
      if (codec_inst.pacsize == packet_size_samples) {
        packet_size_ok = true;
        break;
      }
    }

    if (!packet_size_ok) {
      return kInvalidPacketSize;
    }
  }

  if (codec_inst.pacsize < 1) {
    return kInvalidPacketSize;
  }

  // Check the validity of rate. Codecs with multiple rates have their own
  // function for this.
  *mirror_id = codec_id;
  if (STR_CASE_CMP("isac", codec_inst.plname) == 0) {
    if (IsISACRateValid(codec_inst.rate)) {
      // Set mirrorID to iSAC WB which is only created once to be used both for
      // iSAC WB and SWB, because they need to share struct.
      *mirror_id = kISAC;
      return  codec_id;
    } else {
      return kInvalidRate;
    }
  } else if (STR_CASE_CMP("ilbc", codec_inst.plname) == 0) {
    return IsILBCRateValid(codec_inst.rate, codec_inst.pacsize)
        ? codec_id : kInvalidRate;
  } else if (STR_CASE_CMP("amr", codec_inst.plname) == 0) {
    return IsAMRRateValid(codec_inst.rate)
        ? codec_id : kInvalidRate;
  } else if (STR_CASE_CMP("amr-wb", codec_inst.plname) == 0) {
    return IsAMRwbRateValid(codec_inst.rate)
        ? codec_id : kInvalidRate;
  } else if (STR_CASE_CMP("g7291", codec_inst.plname) == 0) {
    return IsG7291RateValid(codec_inst.rate)
        ? codec_id : kInvalidRate;
  } else if (STR_CASE_CMP("opus", codec_inst.plname) == 0) {
    return IsOpusRateValid(codec_inst.rate)
        ? codec_id : kInvalidRate;
  } else if (STR_CASE_CMP("speex", codec_inst.plname) == 0) {
    return IsSpeexRateValid(codec_inst.rate)
        ? codec_id : kInvalidRate;
  }

  return IsRateValid(codec_id, codec_inst.rate) ?
      codec_id : kInvalidRate;
}

// Looks for a matching payload name, frequency, and channels in the
// codec list. Need to check all three since some codecs have several codec
// entries with different frequencies and/or channels.
// Does not check other codec settings, such as payload type and packet size.
// Returns the id of the codec, or -1 if no match is found.
int ACMCodecDB::CodecId(const CodecInst& codec_inst) {
  return (CodecId(codec_inst.plname, codec_inst.plfreq,
                  codec_inst.channels));
}

int ACMCodecDB::CodecId(const char* payload_name, int frequency, int channels) {
  for (int id = 0; id < kNumCodecs; id++) {
    bool name_match = false;
    bool frequency_match = false;
    bool channels_match = false;

    // Payload name, sampling frequency and number of channels need to match.
    // NOTE! If |frequency| is -1, the frequency is not applicable, and is
    // always treated as true, like for RED.
    name_match = (STR_CASE_CMP(database_[id].plname, payload_name) == 0);
    frequency_match = (frequency == database_[id].plfreq) || (frequency == -1);
    // The number of channels must match for all codecs but Opus.
    if (STR_CASE_CMP(payload_name, "opus") != 0) {
      channels_match = (channels == database_[id].channels);
    } else {
      // For opus we just check that number of channels is valid.
      channels_match = (channels == 1 || channels == 2);
    }

    if (name_match && frequency_match && channels_match) {
      // We have found a matching codec in the list.
      return id;
    }
  }

  // We didn't find a matching codec.
  return -1;
}
// Gets codec id number, and mirror id, from database for the receiver.
int ACMCodecDB::ReceiverCodecNumber(const CodecInst& codec_inst,
                                    int* mirror_id) {
  // Look for a matching codec in the database.
  int codec_id = CodecId(codec_inst);

  // Set |mirror_id| to |codec_id|, except for iSAC. In case of iSAC we always
  // set |mirror_id| to iSAC WB (kISAC) which is only created once to be used
  // both for iSAC WB and SWB, because they need to share struct.
  if (STR_CASE_CMP(codec_inst.plname, "ISAC") != 0) {
    *mirror_id = codec_id;
  } else {
    *mirror_id = kISAC;
  }

  return codec_id;
}

// Returns the codec sampling frequency for codec with id = "codec_id" in
// database.
int ACMCodecDB::CodecFreq(int codec_id) {
  // Error check to see that codec_id is not out of bounds.
  if (codec_id < 0 || codec_id >= kNumCodecs) {
    return -1;
  }

  return database_[codec_id].plfreq;
}

// Returns the codec's basic coding block size in samples.
int ACMCodecDB::BasicCodingBlock(int codec_id) {
  // Error check to see that codec_id is not out of bounds.
  if (codec_id < 0 || codec_id >= kNumCodecs) {
    return -1;
  }

  return codec_settings_[codec_id].basic_block_samples;
}

// Returns the NetEQ decoder database.
const NetEqDecoder* ACMCodecDB::NetEQDecoders() {
  return neteq_decoders_;
}

// Gets mirror id. The Id is used for codecs sharing struct for settings that
// need different payload types.
int ACMCodecDB::MirrorID(int codec_id) {
  if (STR_CASE_CMP(database_[codec_id].plname, "isac") == 0) {
    return kISAC;
  } else {
    return codec_id;
  }
}

// Creates memory/instance for storing codec state.
ACMGenericCodec* ACMCodecDB::CreateCodecInstance(const CodecInst& codec_inst,
                                                 int cng_pt_nb,
                                                 int cng_pt_wb,
                                                 int cng_pt_swb,
                                                 int cng_pt_fb,
                                                 bool enable_red,
                                                 int red_payload_type) {
  // All we have support for right now.
  if (!STR_CASE_CMP(codec_inst.plname, "ISAC")) {
#if (defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX))
    return new ACMGenericCodec(codec_inst, cng_pt_nb, cng_pt_wb, cng_pt_swb,
                               cng_pt_fb, enable_red, red_payload_type);
#endif
  } else if (!STR_CASE_CMP(codec_inst.plname, "PCMU") ||
             !STR_CASE_CMP(codec_inst.plname, "PCMA")) {
    return new ACMGenericCodec(codec_inst, cng_pt_nb, cng_pt_wb, cng_pt_swb,
                               cng_pt_fb, enable_red, red_payload_type);
  } else if (!STR_CASE_CMP(codec_inst.plname, "ILBC")) {
#ifdef WEBRTC_CODEC_ILBC
    return new ACMGenericCodec(codec_inst, cng_pt_nb, cng_pt_wb, cng_pt_swb,
                               cng_pt_fb, enable_red, red_payload_type);
#endif
  } else if (!STR_CASE_CMP(codec_inst.plname, "G722")) {
#ifdef WEBRTC_CODEC_G722
    return new ACMGenericCodec(codec_inst, cng_pt_nb, cng_pt_wb, cng_pt_swb,
                               cng_pt_fb, enable_red, red_payload_type);
#endif
  } else if (!STR_CASE_CMP(codec_inst.plname, "opus")) {
#ifdef WEBRTC_CODEC_OPUS
    return new ACMGenericCodec(codec_inst, cng_pt_nb, cng_pt_wb, cng_pt_swb,
                               cng_pt_fb, enable_red, red_payload_type);
#endif
  } else if (!STR_CASE_CMP(codec_inst.plname, "L16")) {
#ifdef WEBRTC_CODEC_PCM16
    return new ACMGenericCodec(codec_inst, cng_pt_nb, cng_pt_wb, cng_pt_swb,
                               cng_pt_fb, enable_red, red_payload_type);
#endif
  }
  return NULL;
}

// Checks if the bitrate is valid for the codec.
bool ACMCodecDB::IsRateValid(int codec_id, int rate) {
  return database_[codec_id].rate == rate;
}

// Checks if the bitrate is valid for iSAC.
bool ACMCodecDB::IsISACRateValid(int rate) {
  return (rate == -1) || ((rate <= 56000) && (rate >= 10000));
}

// Checks if the bitrate is valid for iLBC.
bool ACMCodecDB::IsILBCRateValid(int rate, int frame_size_samples) {
  if (((frame_size_samples == 240) || (frame_size_samples == 480)) &&
      (rate == 13300)) {
    return true;
  } else if (((frame_size_samples == 160) || (frame_size_samples == 320)) &&
      (rate == 15200)) {
    return true;
  } else {
    return false;
  }
}

// Check if the bitrate is valid for the GSM-AMR.
bool ACMCodecDB::IsAMRRateValid(int rate) {
  switch (rate) {
    case 4750:
    case 5150:
    case 5900:
    case 6700:
    case 7400:
    case 7950:
    case 10200:
    case 12200: {
      return true;
    }
    default: {
      return false;
    }
  }
}

// Check if the bitrate is valid for GSM-AMR-WB.
bool ACMCodecDB::IsAMRwbRateValid(int rate) {
  switch (rate) {
    case 7000:
    case 9000:
    case 12000:
    case 14000:
    case 16000:
    case 18000:
    case 20000:
    case 23000:
    case 24000: {
      return true;
    }
    default: {
      return false;
    }
  }
}

// Check if the bitrate is valid for G.729.1.
bool ACMCodecDB::IsG7291RateValid(int rate) {
  switch (rate) {
    case 8000:
    case 12000:
    case 14000:
    case 16000:
    case 18000:
    case 20000:
    case 22000:
    case 24000:
    case 26000:
    case 28000:
    case 30000:
    case 32000: {
      return true;
    }
    default: {
      return false;
    }
  }
}

// Checks if the bitrate is valid for Speex.
bool ACMCodecDB::IsSpeexRateValid(int rate) {
  return rate > 2000;
}

// Checks if the bitrate is valid for Opus.
bool ACMCodecDB::IsOpusRateValid(int rate) {
  return (rate >= 6000) && (rate <= 510000);
}

// Checks if the payload type is in the valid range.
bool ACMCodecDB::ValidPayloadType(int payload_type) {
  return (payload_type >= 0) && (payload_type <= 127);
}

bool ACMCodecDB::OwnsDecoder(int codec_id) {
  assert(codec_id >= 0 && codec_id < ACMCodecDB::kNumCodecs);
  return ACMCodecDB::codec_settings_[codec_id].owns_decoder;
}

}  // namespace acm2

}  // namespace webrtc
