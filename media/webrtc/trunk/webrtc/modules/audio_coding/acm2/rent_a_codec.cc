/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/acm2/rent_a_codec.h"

#include <utility>

#include "webrtc/base/logging.h"
#include "webrtc/modules/audio_coding/codecs/cng/audio_encoder_cng.h"
#include "webrtc/modules/audio_coding/codecs/g711/audio_encoder_pcm.h"
#ifdef WEBRTC_CODEC_G722
#include "webrtc/modules/audio_coding/codecs/g722/audio_encoder_g722.h"
#endif
#ifdef WEBRTC_CODEC_ILBC
#include "webrtc/modules/audio_coding/codecs/ilbc/audio_encoder_ilbc.h"
#endif
#ifdef WEBRTC_CODEC_ISACFX
#include "webrtc/modules/audio_coding/codecs/isac/fix/include/audio_decoder_isacfix.h"
#include "webrtc/modules/audio_coding/codecs/isac/fix/include/audio_encoder_isacfix.h"
#endif
#ifdef WEBRTC_CODEC_ISAC
#include "webrtc/modules/audio_coding/codecs/isac/main/include/audio_decoder_isac.h"
#include "webrtc/modules/audio_coding/codecs/isac/main/include/audio_encoder_isac.h"
#endif
#ifdef WEBRTC_CODEC_OPUS
#include "webrtc/modules/audio_coding/codecs/opus/audio_encoder_opus.h"
#endif
#include "webrtc/modules/audio_coding/codecs/pcm16b/audio_encoder_pcm16b.h"
#ifdef WEBRTC_CODEC_RED
#include "webrtc/modules/audio_coding/codecs/red/audio_encoder_copy_red.h"
#endif
#include "webrtc/modules/audio_coding/acm2/acm_codec_database.h"
#include "webrtc/modules/audio_coding/acm2/acm_common_defs.h"

namespace webrtc {
namespace acm2 {

rtc::Optional<RentACodec::CodecId> RentACodec::CodecIdByParams(
    const char* payload_name,
    int sampling_freq_hz,
    size_t channels) {
  return CodecIdFromIndex(
      ACMCodecDB::CodecId(payload_name, sampling_freq_hz, channels));
}

rtc::Optional<CodecInst> RentACodec::CodecInstById(CodecId codec_id) {
  rtc::Optional<int> mi = CodecIndexFromId(codec_id);
  return mi ? rtc::Optional<CodecInst>(Database()[*mi])
            : rtc::Optional<CodecInst>();
}

rtc::Optional<RentACodec::CodecId> RentACodec::CodecIdByInst(
    const CodecInst& codec_inst) {
  return CodecIdFromIndex(ACMCodecDB::CodecNumber(codec_inst));
}

rtc::Optional<CodecInst> RentACodec::CodecInstByParams(const char* payload_name,
                                                       int sampling_freq_hz,
                                                       size_t channels) {
  rtc::Optional<CodecId> codec_id =
      CodecIdByParams(payload_name, sampling_freq_hz, channels);
  if (!codec_id)
    return rtc::Optional<CodecInst>();
  rtc::Optional<CodecInst> ci = CodecInstById(*codec_id);
  RTC_DCHECK(ci);

  // Keep the number of channels from the function call. For most codecs it
  // will be the same value as in default codec settings, but not for all.
  ci->channels = channels;

  return ci;
}

bool RentACodec::IsCodecValid(const CodecInst& codec_inst) {
  return ACMCodecDB::CodecNumber(codec_inst) >= 0;
}

rtc::Optional<bool> RentACodec::IsSupportedNumChannels(CodecId codec_id,
                                                       size_t num_channels) {
  auto i = CodecIndexFromId(codec_id);
  return i ? rtc::Optional<bool>(
                 ACMCodecDB::codec_settings_[*i].channel_support >=
                 num_channels)
           : rtc::Optional<bool>();
}

rtc::ArrayView<const CodecInst> RentACodec::Database() {
  return rtc::ArrayView<const CodecInst>(ACMCodecDB::database_,
                                         NumberOfCodecs());
}

rtc::Optional<NetEqDecoder> RentACodec::NetEqDecoderFromCodecId(
    CodecId codec_id,
    size_t num_channels) {
  rtc::Optional<int> i = CodecIndexFromId(codec_id);
  if (!i)
    return rtc::Optional<NetEqDecoder>();
  const NetEqDecoder ned = ACMCodecDB::neteq_decoders_[*i];
  return rtc::Optional<NetEqDecoder>(
      (ned == NetEqDecoder::kDecoderOpus && num_channels == 2)
          ? NetEqDecoder::kDecoderOpus_2ch
          : ned);
}

RentACodec::RegistrationResult RentACodec::RegisterCngPayloadType(
    std::map<int, int>* pt_map,
    const CodecInst& codec_inst) {
  if (STR_CASE_CMP(codec_inst.plname, "CN") != 0)
    return RegistrationResult::kSkip;
  switch (codec_inst.plfreq) {
    case 8000:
    case 16000:
    case 32000:
    case 48000:
      (*pt_map)[codec_inst.plfreq] = codec_inst.pltype;
      return RegistrationResult::kOk;
    default:
      return RegistrationResult::kBadFreq;
  }
}

RentACodec::RegistrationResult RentACodec::RegisterRedPayloadType(
    std::map<int, int>* pt_map,
    const CodecInst& codec_inst) {
  if (STR_CASE_CMP(codec_inst.plname, "RED") != 0)
    return RegistrationResult::kSkip;
  switch (codec_inst.plfreq) {
    case 8000:
      (*pt_map)[codec_inst.plfreq] = codec_inst.pltype;
      return RegistrationResult::kOk;
    default:
      return RegistrationResult::kBadFreq;
  }
}

namespace {

// Returns a new speech encoder, or null on error.
// TODO(kwiberg): Don't handle errors here (bug 5033)
rtc::scoped_ptr<AudioEncoder> CreateEncoder(
    const CodecInst& speech_inst,
    LockedIsacBandwidthInfo* bwinfo) {
#if defined(WEBRTC_CODEC_ISACFX)
  if (STR_CASE_CMP(speech_inst.plname, "isac") == 0)
    return rtc_make_scoped_ptr(new AudioEncoderIsacFix(speech_inst, bwinfo));
#endif
#if defined(WEBRTC_CODEC_ISAC)
  if (STR_CASE_CMP(speech_inst.plname, "isac") == 0)
    return rtc_make_scoped_ptr(new AudioEncoderIsac(speech_inst, bwinfo));
#endif
#ifdef WEBRTC_CODEC_OPUS
  if (STR_CASE_CMP(speech_inst.plname, "opus") == 0)
    return rtc_make_scoped_ptr(new AudioEncoderOpus(speech_inst));
#endif
  if (STR_CASE_CMP(speech_inst.plname, "pcmu") == 0)
    return rtc_make_scoped_ptr(new AudioEncoderPcmU(speech_inst));
  if (STR_CASE_CMP(speech_inst.plname, "pcma") == 0)
    return rtc_make_scoped_ptr(new AudioEncoderPcmA(speech_inst));
  if (STR_CASE_CMP(speech_inst.plname, "l16") == 0)
    return rtc_make_scoped_ptr(new AudioEncoderPcm16B(speech_inst));
#ifdef WEBRTC_CODEC_ILBC
  if (STR_CASE_CMP(speech_inst.plname, "ilbc") == 0)
    return rtc_make_scoped_ptr(new AudioEncoderIlbc(speech_inst));
#endif
#ifdef WEBRTC_CODEC_G722
  if (STR_CASE_CMP(speech_inst.plname, "g722") == 0)
    return rtc_make_scoped_ptr(new AudioEncoderG722(speech_inst));
#endif
  LOG_F(LS_ERROR) << "Could not create encoder of type " << speech_inst.plname;
  return rtc::scoped_ptr<AudioEncoder>();
}

rtc::scoped_ptr<AudioEncoder> CreateRedEncoder(AudioEncoder* encoder,
                                               int red_payload_type) {
#ifdef WEBRTC_CODEC_RED
  AudioEncoderCopyRed::Config config;
  config.payload_type = red_payload_type;
  config.speech_encoder = encoder;
  return rtc::scoped_ptr<AudioEncoder>(new AudioEncoderCopyRed(config));
#else
  return rtc::scoped_ptr<AudioEncoder>();
#endif
}

rtc::scoped_ptr<AudioEncoder> CreateCngEncoder(AudioEncoder* encoder,
                                               int payload_type,
                                               ACMVADMode vad_mode) {
  AudioEncoderCng::Config config;
  config.num_channels = encoder->NumChannels();
  config.payload_type = payload_type;
  config.speech_encoder = encoder;
  switch (vad_mode) {
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
  return rtc::scoped_ptr<AudioEncoder>(new AudioEncoderCng(config));
}

rtc::scoped_ptr<AudioDecoder> CreateIsacDecoder(
    LockedIsacBandwidthInfo* bwinfo) {
#if defined(WEBRTC_CODEC_ISACFX)
  return rtc_make_scoped_ptr(new AudioDecoderIsacFix(bwinfo));
#elif defined(WEBRTC_CODEC_ISAC)
  return rtc_make_scoped_ptr(new AudioDecoderIsac(bwinfo));
#else
  FATAL() << "iSAC is not supported.";
  return rtc::scoped_ptr<AudioDecoder>();
#endif
}

}  // namespace

RentACodec::RentACodec() = default;
RentACodec::~RentACodec() = default;

AudioEncoder* RentACodec::RentEncoder(const CodecInst& codec_inst) {
  rtc::scoped_ptr<AudioEncoder> enc =
      CreateEncoder(codec_inst, &isac_bandwidth_info_);
  if (!enc)
    return nullptr;
  speech_encoder_ = std::move(enc);
  return speech_encoder_.get();
}

RentACodec::StackParameters::StackParameters() {
  // Register the default payload types for RED and CNG.
  for (const CodecInst& ci : RentACodec::Database()) {
    RentACodec::RegisterCngPayloadType(&cng_payload_types, ci);
    RentACodec::RegisterRedPayloadType(&red_payload_types, ci);
  }
}

RentACodec::StackParameters::~StackParameters() = default;

AudioEncoder* RentACodec::RentEncoderStack(StackParameters* param) {
  RTC_DCHECK(param->speech_encoder);

  if (param->use_codec_fec) {
    // Switch FEC on. On failure, remember that FEC is off.
    if (!param->speech_encoder->SetFec(true))
      param->use_codec_fec = false;
  } else {
    // Switch FEC off. This shouldn't fail.
    const bool success = param->speech_encoder->SetFec(false);
    RTC_DCHECK(success);
  }

  auto pt = [&param](const std::map<int, int>& m) {
    auto it = m.find(param->speech_encoder->SampleRateHz());
    return it == m.end() ? rtc::Optional<int>()
                         : rtc::Optional<int>(it->second);
  };
  auto cng_pt = pt(param->cng_payload_types);
  param->use_cng =
      param->use_cng && cng_pt && param->speech_encoder->NumChannels() == 1;
  auto red_pt = pt(param->red_payload_types);
  param->use_red = param->use_red && red_pt;

  if (param->use_cng || param->use_red) {
    // The RED and CNG encoders need to be in sync with the speech encoder, so
    // reset the latter to ensure its buffer is empty.
    param->speech_encoder->Reset();
  }
  encoder_stack_ = param->speech_encoder;
  if (param->use_red) {
    red_encoder_ = CreateRedEncoder(encoder_stack_, *red_pt);
    if (red_encoder_)
      encoder_stack_ = red_encoder_.get();
  } else {
    red_encoder_.reset();
  }
  if (param->use_cng) {
    cng_encoder_ = CreateCngEncoder(encoder_stack_, *cng_pt, param->vad_mode);
    encoder_stack_ = cng_encoder_.get();
  } else {
    cng_encoder_.reset();
  }
  return encoder_stack_;
}

AudioDecoder* RentACodec::RentIsacDecoder() {
  if (!isac_decoder_)
    isac_decoder_ = CreateIsacDecoder(&isac_bandwidth_info_);
  return isac_decoder_.get();
}

}  // namespace acm2
}  // namespace webrtc
