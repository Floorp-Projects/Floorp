/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/acm2/acm_generic_codec.h"

#include <assert.h>
#include <string.h>
#include <algorithm>
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/common_audio/vad/include/webrtc_vad.h"
#include "webrtc/modules/audio_coding/codecs/cng/include/audio_encoder_cng.h"
#include "webrtc/modules/audio_coding/codecs/cng/include/webrtc_cng.h"
#include "webrtc/modules/audio_coding/codecs/g711/include/audio_encoder_pcm.h"
#include "webrtc/modules/audio_coding/codecs/g722/include/audio_encoder_g722.h"
#include "webrtc/modules/audio_coding/codecs/ilbc/interface/audio_encoder_ilbc.h"
#include "webrtc/modules/audio_coding/codecs/isac/fix/interface/audio_encoder_isacfix.h"
#include "webrtc/modules/audio_coding/codecs/isac/main/interface/audio_encoder_isac.h"
#include "webrtc/modules/audio_coding/codecs/opus/interface/audio_encoder_opus.h"
#include "webrtc/modules/audio_coding/codecs/pcm16b/include/audio_encoder_pcm16b.h"
#include "webrtc/modules/audio_coding/codecs/red/audio_encoder_copy_red.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_codec_database.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_common_defs.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

namespace {
static const int kInvalidPayloadType = 255;

std::map<int, int>::iterator FindSampleRateInMap(std::map<int, int>* cng_pt_map,
                                                 int sample_rate_hz) {
  return find_if(cng_pt_map->begin(), cng_pt_map->end(),
                 [sample_rate_hz](decltype(*cng_pt_map->begin()) p) {
                   return p.second == sample_rate_hz;
                 });
}

void SetPtInMap(std::map<int, int>* pt_map,
                int sample_rate_hz,
                int payload_type) {
  if (payload_type == kInvalidPayloadType)
    return;
  CHECK_GE(payload_type, 0);
  CHECK_LT(payload_type, 128);
  auto pt_iter = FindSampleRateInMap(pt_map, sample_rate_hz);
  if (pt_iter != pt_map->end()) {
    // Remove item in map with sample_rate_hz.
    pt_map->erase(pt_iter);
  }
  (*pt_map)[payload_type] = sample_rate_hz;
}
}  // namespace

namespace acm2 {

// Enum for CNG
enum {
  kMaxPLCParamsCNG = WEBRTC_CNG_MAX_LPC_ORDER,
  kNewCNGNumLPCParams = 8
};

// Interval for sending new CNG parameters (SID frames) is 100 msec.
enum {
  kCngSidIntervalMsec = 100
};

// We set some of the variables to invalid values as a check point
// if a proper initialization has happened. Another approach is
// to initialize to a default codec that we are sure is always included.
ACMGenericCodec::ACMGenericCodec(const CodecInst& codec_inst,
                                 int cng_pt_nb,
                                 int cng_pt_wb,
                                 int cng_pt_swb,
                                 int cng_pt_fb,
                                 bool enable_red,
                                 int red_pt_nb)
    : has_internal_fec_(false),
      copy_red_enabled_(enable_red),
      encoder_(NULL),
      bitrate_bps_(0),
      fec_enabled_(false),
      loss_rate_(0),
      max_playback_rate_hz_(48000),
      max_payload_size_bytes_(-1),
      max_rate_bps_(-1),
      opus_dtx_enabled_(false),
      is_opus_(false),
      is_isac_(false),
      opus_application_set_(false) {
  acm_codec_params_.codec_inst = codec_inst;
  acm_codec_params_.enable_dtx = false;
  acm_codec_params_.enable_vad = false;
  acm_codec_params_.vad_mode = VADNormal;
  SetPtInMap(&red_pt_, 8000, red_pt_nb);
  SetPtInMap(&cng_pt_, 8000, cng_pt_nb);
  SetPtInMap(&cng_pt_, 16000, cng_pt_wb);
  SetPtInMap(&cng_pt_, 32000, cng_pt_swb);
  SetPtInMap(&cng_pt_, 48000, cng_pt_fb);
  ResetAudioEncoder();
  CHECK(encoder_);
}

ACMGenericCodec::~ACMGenericCodec() {
}

AudioDecoderProxy::AudioDecoderProxy()
    : decoder_lock_(CriticalSectionWrapper::CreateCriticalSection()),
      decoder_(nullptr) {
}

void AudioDecoderProxy::SetDecoder(AudioDecoder* decoder) {
  CriticalSectionScoped decoder_lock(decoder_lock_.get());
  decoder_ = decoder;
  CHECK_EQ(decoder_->Init(), 0);
}

bool AudioDecoderProxy::IsSet() const {
  CriticalSectionScoped decoder_lock(decoder_lock_.get());
  return (decoder_ != nullptr);
}

int AudioDecoderProxy::Decode(const uint8_t* encoded,
                              size_t encoded_len,
                              int sample_rate_hz,
                              size_t max_decoded_bytes,
                              int16_t* decoded,
                              SpeechType* speech_type) {
  CriticalSectionScoped decoder_lock(decoder_lock_.get());
  return decoder_->Decode(encoded, encoded_len, sample_rate_hz,
                          max_decoded_bytes, decoded, speech_type);
}

int AudioDecoderProxy::DecodeRedundant(const uint8_t* encoded,
                                       size_t encoded_len,
                                       int sample_rate_hz,
                                       size_t max_decoded_bytes,
                                       int16_t* decoded,
                                       SpeechType* speech_type) {
  CriticalSectionScoped decoder_lock(decoder_lock_.get());
  return decoder_->DecodeRedundant(encoded, encoded_len, sample_rate_hz,
                                   max_decoded_bytes, decoded, speech_type);
}

bool AudioDecoderProxy::HasDecodePlc() const {
  CriticalSectionScoped decoder_lock(decoder_lock_.get());
  return decoder_->HasDecodePlc();
}

int AudioDecoderProxy::DecodePlc(int num_frames, int16_t* decoded) {
  CriticalSectionScoped decoder_lock(decoder_lock_.get());
  return decoder_->DecodePlc(num_frames, decoded);
}

int AudioDecoderProxy::Init() {
  CriticalSectionScoped decoder_lock(decoder_lock_.get());
  return decoder_->Init();
}

int AudioDecoderProxy::IncomingPacket(const uint8_t* payload,
                                      size_t payload_len,
                                      uint16_t rtp_sequence_number,
                                      uint32_t rtp_timestamp,
                                      uint32_t arrival_timestamp) {
  CriticalSectionScoped decoder_lock(decoder_lock_.get());
  return decoder_->IncomingPacket(payload, payload_len, rtp_sequence_number,
                                  rtp_timestamp, arrival_timestamp);
}

int AudioDecoderProxy::ErrorCode() {
  CriticalSectionScoped decoder_lock(decoder_lock_.get());
  return decoder_->ErrorCode();
}

int AudioDecoderProxy::PacketDuration(const uint8_t* encoded,
                                      size_t encoded_len) const {
  CriticalSectionScoped decoder_lock(decoder_lock_.get());
  return decoder_->PacketDuration(encoded, encoded_len);
}

int AudioDecoderProxy::PacketDurationRedundant(const uint8_t* encoded,
                                               size_t encoded_len) const {
  CriticalSectionScoped decoder_lock(decoder_lock_.get());
  return decoder_->PacketDurationRedundant(encoded, encoded_len);
}

bool AudioDecoderProxy::PacketHasFec(const uint8_t* encoded,
                                     size_t encoded_len) const {
  CriticalSectionScoped decoder_lock(decoder_lock_.get());
  return decoder_->PacketHasFec(encoded, encoded_len);
}

CNG_dec_inst* AudioDecoderProxy::CngDecoderInstance() {
  CriticalSectionScoped decoder_lock(decoder_lock_.get());
  return decoder_->CngDecoderInstance();
}

size_t AudioDecoderProxy::Channels() const {
  CriticalSectionScoped decoder_lock(decoder_lock_.get());
  return decoder_->Channels();
}

int16_t ACMGenericCodec::EncoderParams(WebRtcACMCodecParams* enc_params) const {
  *enc_params = acm_codec_params_;
  return 0;
}

int16_t ACMGenericCodec::InitEncoder(WebRtcACMCodecParams* codec_params,
                                     bool force_initialization) {
  bitrate_bps_ = 0;
  loss_rate_ = 0;
  opus_dtx_enabled_ = false;
  acm_codec_params_ = *codec_params;
  if (force_initialization)
    opus_application_set_ = false;
  opus_application_ = GetOpusApplication(codec_params->codec_inst.channels,
                                         opus_dtx_enabled_);
  opus_application_set_ = true;
  ResetAudioEncoder();
  return 0;
}

void ACMGenericCodec::ResetAudioEncoder() {
  const CodecInst& codec_inst = acm_codec_params_.codec_inst;
  if (!STR_CASE_CMP(codec_inst.plname, "PCMU")) {
    AudioEncoderPcmU::Config config;
    config.num_channels = codec_inst.channels;
    config.frame_size_ms = codec_inst.pacsize / 8;
    config.payload_type = codec_inst.pltype;
    audio_encoder_.reset(new AudioEncoderPcmU(config));
  } else if (!STR_CASE_CMP(codec_inst.plname, "PCMA")) {
    AudioEncoderPcmA::Config config;
    config.num_channels = codec_inst.channels;
    config.frame_size_ms = codec_inst.pacsize / 8;
    config.payload_type = codec_inst.pltype;
    audio_encoder_.reset(new AudioEncoderPcmA(config));
#ifdef WEBRTC_CODEC_PCM16
  } else if (!STR_CASE_CMP(codec_inst.plname, "L16")) {
    AudioEncoderPcm16B::Config config;
    config.num_channels = codec_inst.channels;
    config.sample_rate_hz = codec_inst.plfreq;
    config.frame_size_ms = codec_inst.pacsize / (config.sample_rate_hz / 1000);
    config.payload_type = codec_inst.pltype;
    audio_encoder_.reset(new AudioEncoderPcm16B(config));
#endif
#ifdef WEBRTC_CODEC_ILBC
  } else if (!STR_CASE_CMP(codec_inst.plname, "ILBC")) {
    AudioEncoderIlbc::Config config;
    config.frame_size_ms = codec_inst.pacsize / 8;
    config.payload_type = codec_inst.pltype;
    audio_encoder_.reset(new AudioEncoderIlbc(config));
#endif
#ifdef WEBRTC_CODEC_OPUS
  } else if (!STR_CASE_CMP(codec_inst.plname, "opus")) {
    is_opus_ = true;
    has_internal_fec_ = true;
    AudioEncoderOpus::Config config;
    config.frame_size_ms = codec_inst.pacsize / 48;
    config.num_channels = codec_inst.channels;
    config.fec_enabled = fec_enabled_;
    config.bitrate_bps = codec_inst.rate;
    config.max_playback_rate_hz = max_playback_rate_hz_;
    config.dtx_enabled = opus_dtx_enabled_;
    config.payload_type = codec_inst.pltype;
    switch (GetOpusApplication(config.num_channels, config.dtx_enabled)) {
      case kVoip:
        config.application = AudioEncoderOpus::ApplicationMode::kVoip;
        break;
      case kAudio:
        config.application = AudioEncoderOpus::ApplicationMode::kAudio;
        break;
    }
    audio_encoder_.reset(new AudioEncoderOpus(config));
#endif
#ifdef WEBRTC_CODEC_G722
  } else if (!STR_CASE_CMP(codec_inst.plname, "G722")) {
    AudioEncoderG722::Config config;
    config.num_channels = codec_inst.channels;
    config.frame_size_ms = codec_inst.pacsize / 16;
    config.payload_type = codec_inst.pltype;
    audio_encoder_.reset(new AudioEncoderG722(config));
#endif
#ifdef WEBRTC_CODEC_ISACFX
  } else if (!STR_CASE_CMP(codec_inst.plname, "ISAC")) {
    DCHECK_EQ(codec_inst.plfreq, 16000);
    is_isac_ = true;
    AudioEncoderDecoderIsacFix* enc_dec;
    if (codec_inst.rate == -1) {
      // Adaptive mode.
      AudioEncoderDecoderIsacFix::ConfigAdaptive config;
      config.payload_type = codec_inst.pltype;
      enc_dec = new AudioEncoderDecoderIsacFix(config);
    } else {
      // Channel independent mode.
      AudioEncoderDecoderIsacFix::Config config;
      config.bit_rate = codec_inst.rate;
      config.frame_size_ms = codec_inst.pacsize / 16;
      config.payload_type = codec_inst.pltype;
      enc_dec = new AudioEncoderDecoderIsacFix(config);
    }
    decoder_proxy_.SetDecoder(enc_dec);
    audio_encoder_.reset(enc_dec);
#endif
#ifdef WEBRTC_CODEC_ISAC
  } else if (!STR_CASE_CMP(codec_inst.plname, "ISAC")) {
    is_isac_ = true;
    AudioEncoderDecoderIsac* enc_dec;
    if (codec_inst.rate == -1) {
      // Adaptive mode.
      AudioEncoderDecoderIsac::ConfigAdaptive config;
      config.sample_rate_hz = codec_inst.plfreq;
      config.initial_frame_size_ms = rtc::CheckedDivExact(
          1000 * codec_inst.pacsize, config.sample_rate_hz);
      config.max_payload_size_bytes = max_payload_size_bytes_;
      config.max_bit_rate = max_rate_bps_;
      config.payload_type = codec_inst.pltype;
      enc_dec = new AudioEncoderDecoderIsac(config);
    } else {
      // Channel independent mode.
      AudioEncoderDecoderIsac::Config config;
      config.sample_rate_hz = codec_inst.plfreq;
      config.bit_rate = codec_inst.rate;
      config.frame_size_ms = rtc::CheckedDivExact(1000 * codec_inst.pacsize,
                                                  config.sample_rate_hz);
      config.max_payload_size_bytes = max_payload_size_bytes_;
      config.max_bit_rate = max_rate_bps_;
      config.payload_type = codec_inst.pltype;
      enc_dec = new AudioEncoderDecoderIsac(config);
    }
    decoder_proxy_.SetDecoder(enc_dec);
    audio_encoder_.reset(enc_dec);
#endif
  } else {
    FATAL();
  }
  if (bitrate_bps_ != 0)
    audio_encoder_->SetTargetBitrate(bitrate_bps_);
  audio_encoder_->SetProjectedPacketLossRate(loss_rate_ / 100.0);
  encoder_ = audio_encoder_.get();

  // Attach RED if needed.
  auto pt_iter =
      FindSampleRateInMap(&red_pt_, audio_encoder_->SampleRateHz());
  if (copy_red_enabled_ && pt_iter != red_pt_.end()) {
    CHECK_NE(pt_iter->first, kInvalidPayloadType);
    AudioEncoderCopyRed::Config config;
    config.payload_type = pt_iter->first;
    config.speech_encoder = encoder_;
    red_encoder_.reset(new AudioEncoderCopyRed(config));
    encoder_ = red_encoder_.get();
  } else {
    red_encoder_.reset();
    copy_red_enabled_ = false;
  }

  // Attach CNG if needed.
  // Reverse-lookup from sample rate to complete key-value pair.
  pt_iter =
      FindSampleRateInMap(&cng_pt_, audio_encoder_->SampleRateHz());
  if (acm_codec_params_.enable_dtx && pt_iter != cng_pt_.end()) {
    AudioEncoderCng::Config config;
    config.num_channels = acm_codec_params_.codec_inst.channels;
    config.payload_type = pt_iter->first;
    config.speech_encoder = encoder_;
    switch (acm_codec_params_.vad_mode) {
      case VADNormal:
        config.vad_mode = Vad::kVadNormal;
        break;
      case VADLowBitrate:
        config.vad_mode = Vad::kVadLowBitrate;
        break;
      case VADAggr:
        config.vad_mode = Vad::kVadAggressive;
        break;
      case VADVeryAggr:
        config.vad_mode = Vad::kVadVeryAggressive;
        break;
      default:
        FATAL();
    }
    cng_encoder_.reset(new AudioEncoderCng(config));
    encoder_ = cng_encoder_.get();
  } else {
    cng_encoder_.reset();
  }
}

OpusApplicationMode ACMGenericCodec::GetOpusApplication(
    int num_channels, bool enable_dtx) const {
  if (opus_application_set_)
    return opus_application_;
  return num_channels == 1 || enable_dtx ? kVoip : kAudio;
}

int16_t ACMGenericCodec::SetBitRate(const int32_t bitrate_bps) {
  encoder_->SetTargetBitrate(bitrate_bps);
  bitrate_bps_ = bitrate_bps;
  return 0;
}

int16_t ACMGenericCodec::SetVAD(bool* enable_dtx,
                                bool* enable_vad,
                                ACMVADMode* mode) {
  if (is_opus_) {
    *enable_dtx = false;
    *enable_vad = false;
    return 0;
  }
  // Note: |enable_vad| is not used; VAD is enabled based on the DTX setting and
  // the |enable_vad| is set equal to |enable_dtx|.
  // The case when VAD is enabled but DTX is disabled may result in a
  // kPassiveNormalEncoded frame type, but this is not a case that VoE
  // distinguishes from the cases where DTX is in fact used. In the case where
  // DTX is enabled but VAD is disabled, the comment in the ACM interface states
  // that VAD will be enabled anyway.
  DCHECK_EQ(*enable_dtx, *enable_vad);
  *enable_vad = *enable_dtx;
  acm_codec_params_.enable_dtx = *enable_dtx;
  acm_codec_params_.enable_vad = *enable_vad;
  acm_codec_params_.vad_mode = *mode;
  if (acm_codec_params_.enable_dtx && !cng_encoder_) {
    ResetAudioEncoder();
  } else if (!acm_codec_params_.enable_dtx && cng_encoder_) {
    cng_encoder_.reset();
    encoder_ = audio_encoder_.get();
  }
  return 0;
}

void ACMGenericCodec::SetCngPt(int sample_rate_hz, int payload_type) {
  SetPtInMap(&cng_pt_, sample_rate_hz, payload_type);
  ResetAudioEncoder();
}

void ACMGenericCodec::SetRedPt(int sample_rate_hz, int payload_type) {
  SetPtInMap(&red_pt_, sample_rate_hz, payload_type);
  ResetAudioEncoder();
}

int32_t ACMGenericCodec::SetISACMaxPayloadSize(
    const uint16_t max_payload_len_bytes) {
  if (!is_isac_)
    return -1;  // Needed for tests to pass.
  max_payload_size_bytes_ = max_payload_len_bytes;
  ResetAudioEncoder();
  return 0;
}

int32_t ACMGenericCodec::SetISACMaxRate(const uint32_t max_rate_bps) {
  if (!is_isac_)
    return -1;  // Needed for tests to pass.
  max_rate_bps_ = max_rate_bps;
  ResetAudioEncoder();
  return 0;
}

int ACMGenericCodec::SetOpusMaxPlaybackRate(int frequency_hz) {
  if (!is_opus_)
    return -1;  // Needed for tests to pass.
  max_playback_rate_hz_ = frequency_hz;
  ResetAudioEncoder();
  return 0;
}

AudioDecoder* ACMGenericCodec::Decoder() {
  return decoder_proxy_.IsSet() ? &decoder_proxy_ : nullptr;
}

int ACMGenericCodec::EnableOpusDtx(bool force_voip) {
  if (!is_opus_)
    return -1;  // Needed for tests to pass.
  if (!force_voip &&
      GetOpusApplication(encoder_->NumChannels(), true) != kVoip) {
      // Opus DTX can only be enabled when application mode is KVoip.
      return -1;
  }
  opus_application_ = kVoip;
  opus_application_set_ = true;
  opus_dtx_enabled_ = true;
  ResetAudioEncoder();
  return 0;
}

int ACMGenericCodec::DisableOpusDtx() {
  if (!is_opus_)
    return -1;  // Needed for tests to pass.
  opus_dtx_enabled_ = false;
  ResetAudioEncoder();
  return 0;
}

int ACMGenericCodec::SetFEC(bool enable_fec) {
  if (!HasInternalFEC())
    return enable_fec ? -1 : 0;
  if (fec_enabled_ != enable_fec) {
    fec_enabled_ = enable_fec;
    ResetAudioEncoder();
  }
  return 0;
}

int ACMGenericCodec::SetOpusApplication(OpusApplicationMode application,
                                        bool disable_dtx_if_needed) {
  if (opus_dtx_enabled_ && application == kAudio) {
    if (disable_dtx_if_needed) {
      opus_dtx_enabled_ = false;
    } else {
      // Opus can only be set to kAudio when DTX is off.
      return -1;
    }
  }
  opus_application_ = application;
  opus_application_set_ = true;
  ResetAudioEncoder();
  return 0;
}

int ACMGenericCodec::SetPacketLossRate(int loss_rate) {
  encoder_->SetProjectedPacketLossRate(loss_rate / 100.0);
  loss_rate_ = loss_rate;
  return 0;
}

int ACMGenericCodec::SetCopyRed(bool enable) {
  copy_red_enabled_ = enable;
  ResetAudioEncoder();
  return copy_red_enabled_ == enable ? 0 : -1;
}

AudioEncoder* ACMGenericCodec::GetAudioEncoder() {
  return encoder_;
}

const AudioEncoder* ACMGenericCodec::GetAudioEncoder() const {
  return encoder_;
}

}  // namespace acm2

}  // namespace webrtc
