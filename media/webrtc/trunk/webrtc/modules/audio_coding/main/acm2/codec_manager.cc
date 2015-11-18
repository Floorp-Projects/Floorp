/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/acm2/codec_manager.h"

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_coding/main/acm2/audio_coding_module_impl.h"

namespace webrtc {
namespace acm2 {

namespace {
bool IsCodecRED(const CodecInst* codec) {
  return (STR_CASE_CMP(codec->plname, "RED") == 0);
}

bool IsCodecRED(int index) {
  return (IsCodecRED(&ACMCodecDB::database_[index]));
}

bool IsCodecCN(const CodecInst* codec) {
  return (STR_CASE_CMP(codec->plname, "CN") == 0);
}

bool IsCodecCN(int index) {
  return (IsCodecCN(&ACMCodecDB::database_[index]));
}

// Check if the given codec is a valid to be registered as send codec.
int IsValidSendCodec(const CodecInst& send_codec,
                     bool is_primary_encoder,
                     int* mirror_id) {
  int dummy_id = 0;
  if ((send_codec.channels != 1) && (send_codec.channels != 2)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, dummy_id,
                 "Wrong number of channels (%d, only mono and stereo are "
                 "supported) for %s encoder",
                 send_codec.channels,
                 is_primary_encoder ? "primary" : "secondary");
    return -1;
  }

  int codec_id = ACMCodecDB::CodecNumber(send_codec, mirror_id);
  if (codec_id < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, dummy_id,
                 "Invalid codec setting for the send codec.");
    return -1;
  }

  // TODO(tlegrand): Remove this check. Already taken care of in
  // ACMCodecDB::CodecNumber().
  // Check if the payload-type is valid
  if (!ACMCodecDB::ValidPayloadType(send_codec.pltype)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, dummy_id,
                 "Invalid payload-type %d for %s.", send_codec.pltype,
                 send_codec.plname);
    return -1;
  }

  // Telephone-event cannot be a send codec.
  if (!STR_CASE_CMP(send_codec.plname, "telephone-event")) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, dummy_id,
                 "telephone-event cannot be a send codec");
    *mirror_id = -1;
    return -1;
  }

  if (ACMCodecDB::codec_settings_[codec_id].channel_support <
      send_codec.channels) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, dummy_id,
                 "%d number of channels not supportedn for %s.",
                 send_codec.channels, send_codec.plname);
    *mirror_id = -1;
    return -1;
  }

  if (!is_primary_encoder) {
    // If registering the secondary encoder, then RED and CN are not valid
    // choices as encoder.
    if (IsCodecRED(&send_codec)) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, dummy_id,
                   "RED cannot be secondary codec");
      *mirror_id = -1;
      return -1;
    }

    if (IsCodecCN(&send_codec)) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, dummy_id,
                   "DTX cannot be secondary codec");
      *mirror_id = -1;
      return -1;
    }
  }
  return codec_id;
}

const CodecInst kEmptyCodecInst = {-1, "noCodecRegistered", 0, 0, 0, 0};
}  // namespace

CodecManager::CodecManager(AudioCodingModuleImpl* acm)
    : acm_(acm),
      cng_nb_pltype_(255),
      cng_wb_pltype_(255),
      cng_swb_pltype_(255),
      cng_fb_pltype_(255),
      red_nb_pltype_(255),
      stereo_send_(false),
      vad_enabled_(false),
      dtx_enabled_(false),
      vad_mode_(VADNormal),
      current_encoder_(nullptr),
      send_codec_inst_(kEmptyCodecInst),
      red_enabled_(false),
      codec_fec_enabled_(false) {
  for (int i = 0; i < ACMCodecDB::kMaxNumCodecs; i++) {
    codecs_[i] = nullptr;
    mirror_codec_idx_[i] = -1;
  }

  // Register the default payload type for RED and for CNG at sampling rates of
  // 8, 16, 32 and 48 kHz.
  for (int i = (ACMCodecDB::kNumCodecs - 1); i >= 0; i--) {
    if (IsCodecRED(i) && ACMCodecDB::database_[i].plfreq == 8000) {
      red_nb_pltype_ = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
    } else if (IsCodecCN(i)) {
      if (ACMCodecDB::database_[i].plfreq == 8000) {
        cng_nb_pltype_ = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
      } else if (ACMCodecDB::database_[i].plfreq == 16000) {
        cng_wb_pltype_ = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
      } else if (ACMCodecDB::database_[i].plfreq == 32000) {
        cng_swb_pltype_ = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
      } else if (ACMCodecDB::database_[i].plfreq == 48000) {
        cng_fb_pltype_ = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
      }
    }
  }
  thread_checker_.DetachFromThread();
}

CodecManager::~CodecManager() {
  for (int i = 0; i < ACMCodecDB::kMaxNumCodecs; i++) {
    if (codecs_[i] != NULL) {
      // Mirror index holds the address of the codec memory.
      assert(mirror_codec_idx_[i] > -1);
      if (codecs_[mirror_codec_idx_[i]] != NULL) {
        delete codecs_[mirror_codec_idx_[i]];
        codecs_[mirror_codec_idx_[i]] = NULL;
      }

      codecs_[i] = NULL;
    }
  }
}

int CodecManager::RegisterSendCodec(const CodecInst& send_codec) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int mirror_id;
  int codec_id = IsValidSendCodec(send_codec, true, &mirror_id);

  // Check for reported errors from function IsValidSendCodec().
  if (codec_id < 0) {
    return -1;
  }

  int dummy_id = 0;
  // RED can be registered with other payload type. If not registered a default
  // payload type is used.
  if (IsCodecRED(&send_codec)) {
    // TODO(tlegrand): Remove this check. Already taken care of in
    // ACMCodecDB::CodecNumber().
    // Check if the payload-type is valid
    if (!ACMCodecDB::ValidPayloadType(send_codec.pltype)) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, dummy_id,
                   "Invalid payload-type %d for %s.", send_codec.pltype,
                   send_codec.plname);
      return -1;
    }
    // Set RED payload type.
    if (send_codec.plfreq == 8000) {
      red_nb_pltype_ = static_cast<uint8_t>(send_codec.pltype);
    } else {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, dummy_id,
                   "RegisterSendCodec() failed, invalid frequency for RED "
                   "registration");
      return -1;
    }
    SetRedPayloadType(send_codec.plfreq, send_codec.pltype);
    return 0;
  }

  // CNG can be registered with other payload type. If not registered the
  // default payload types from codec database will be used.
  if (IsCodecCN(&send_codec)) {
    // CNG is registered.
    switch (send_codec.plfreq) {
      case 8000: {
        cng_nb_pltype_ = static_cast<uint8_t>(send_codec.pltype);
        break;
      }
      case 16000: {
        cng_wb_pltype_ = static_cast<uint8_t>(send_codec.pltype);
        break;
      }
      case 32000: {
        cng_swb_pltype_ = static_cast<uint8_t>(send_codec.pltype);
        break;
      }
      case 48000: {
        cng_fb_pltype_ = static_cast<uint8_t>(send_codec.pltype);
        break;
      }
      default: {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, dummy_id,
                     "RegisterSendCodec() failed, invalid frequency for CNG "
                     "registration");
        return -1;
      }
    }
    SetCngPayloadType(send_codec.plfreq, send_codec.pltype);
    return 0;
  }

  // Set Stereo, and make sure VAD and DTX is turned off.
  if (send_codec.channels == 2) {
    stereo_send_ = true;
    if (vad_enabled_ || dtx_enabled_) {
      WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, dummy_id,
                   "VAD/DTX is turned off, not supported when sending stereo.");
    }
    vad_enabled_ = false;
    dtx_enabled_ = false;
  } else {
    stereo_send_ = false;
  }

  // Check if the codec is already registered as send codec.
  bool is_send_codec;
  if (current_encoder_) {
    int send_codec_mirror_id;
    int send_codec_id =
        ACMCodecDB::CodecNumber(send_codec_inst_, &send_codec_mirror_id);
    assert(send_codec_id >= 0);
    is_send_codec =
        (send_codec_id == codec_id) || (mirror_id == send_codec_mirror_id);
  } else {
    is_send_codec = false;
  }

  // If new codec, or new settings, register.
  if (!is_send_codec) {
    if (!codecs_[mirror_id]) {
      codecs_[mirror_id] = ACMCodecDB::CreateCodecInstance(
          send_codec, cng_nb_pltype_, cng_wb_pltype_, cng_swb_pltype_,
          cng_fb_pltype_, red_enabled_, red_nb_pltype_);
      if (!codecs_[mirror_id]) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, dummy_id,
                     "Cannot Create the codec");
        return -1;
      }
      mirror_codec_idx_[mirror_id] = mirror_id;
    }

    if (mirror_id != codec_id) {
      codecs_[codec_id] = codecs_[mirror_id];
      mirror_codec_idx_[codec_id] = mirror_id;
    }

    ACMGenericCodec* codec_ptr = codecs_[codec_id];
    WebRtcACMCodecParams codec_params;

    memcpy(&(codec_params.codec_inst), &send_codec, sizeof(CodecInst));
    codec_params.enable_vad = vad_enabled_;
    codec_params.enable_dtx = dtx_enabled_;
    codec_params.vad_mode = vad_mode_;
    // Force initialization.
    if (codec_ptr->InitEncoder(&codec_params, true) < 0) {
      // Could not initialize the encoder.

      // Check if already have a registered codec.
      // Depending on that different messages are logged.
      if (!current_encoder_) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, dummy_id,
                     "Cannot Initialize the encoder No Encoder is registered");
      } else {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, dummy_id,
                     "Cannot Initialize the encoder, continue encoding with "
                     "the previously registered codec");
      }
      return -1;
    }

    // Update states.
    dtx_enabled_ = codec_params.enable_dtx;
    vad_enabled_ = codec_params.enable_vad;
    vad_mode_ = codec_params.vad_mode;

    // Everything is fine so we can replace the previous codec with this one.
    if (current_encoder_) {
      // If we change codec we start fresh with RED.
      // This is not strictly required by the standard.

      if (codec_ptr->SetCopyRed(red_enabled_) < 0) {
        // We tried to preserve the old red status, if failed, it means the
        // red status has to be flipped.
        red_enabled_ = !red_enabled_;
      }

      codec_ptr->SetVAD(&dtx_enabled_, &vad_enabled_, &vad_mode_);

      if (!codec_ptr->HasInternalFEC()) {
        codec_fec_enabled_ = false;
      } else {
        if (codec_ptr->SetFEC(codec_fec_enabled_) < 0) {
          WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, dummy_id,
                       "Cannot set codec FEC");
          return -1;
        }
      }
    }

    current_encoder_ = codecs_[codec_id];
    DCHECK(current_encoder_);
    memcpy(&send_codec_inst_, &send_codec, sizeof(CodecInst));
    return 0;
  } else {
    // If codec is the same as already registered check if any parameters
    // has changed compared to the current values.
    // If any parameter is valid then apply it and record.
    bool force_init = false;

    if (mirror_id != codec_id) {
      codecs_[codec_id] = codecs_[mirror_id];
      mirror_codec_idx_[codec_id] = mirror_id;
    }

    // Check the payload type.
    if (send_codec.pltype != send_codec_inst_.pltype) {
      // At this point check if the given payload type is valid.
      // Record it later when the sampling frequency is changed
      // successfully.
      if (!ACMCodecDB::ValidPayloadType(send_codec.pltype)) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, dummy_id,
                     "Out of range payload type");
        return -1;
      }
    }

    // If there is a codec that ONE instance of codec supports multiple
    // sampling frequencies, then we need to take care of it here.
    // one such a codec is iSAC. Both WB and SWB are encoded and decoded
    // with one iSAC instance. Therefore, we need to update the encoder
    // frequency if required.
    if (send_codec_inst_.plfreq != send_codec.plfreq) {
      force_init = true;
    }

    // If packet size or number of channels has changed, we need to
    // re-initialize the encoder.
    if (send_codec_inst_.pacsize != send_codec.pacsize) {
      force_init = true;
    }
    if (send_codec_inst_.channels != send_codec.channels) {
      force_init = true;
    }

    if (force_init) {
      WebRtcACMCodecParams codec_params;

      memcpy(&(codec_params.codec_inst), &send_codec, sizeof(CodecInst));
      codec_params.enable_vad = vad_enabled_;
      codec_params.enable_dtx = dtx_enabled_;
      codec_params.vad_mode = vad_mode_;

      // Force initialization.
      if (current_encoder_->InitEncoder(&codec_params, true) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, dummy_id,
                     "Could not change the codec packet-size.");
        return -1;
      }

      send_codec_inst_.plfreq = send_codec.plfreq;
      send_codec_inst_.pacsize = send_codec.pacsize;
      send_codec_inst_.channels = send_codec.channels;
    }

    // If the change of sampling frequency has been successful then
    // we store the payload-type.
    send_codec_inst_.pltype = send_codec.pltype;

    // Check if a change in Rate is required.
    if (send_codec.rate != send_codec_inst_.rate) {
      if (codecs_[codec_id]->SetBitRate(send_codec.rate) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, dummy_id,
                     "Could not change the codec rate.");
        return -1;
      }
      send_codec_inst_.rate = send_codec.rate;
    }

    if (!codecs_[codec_id]->HasInternalFEC()) {
      codec_fec_enabled_ = false;
    } else {
      if (codecs_[codec_id]->SetFEC(codec_fec_enabled_) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, dummy_id,
                     "Cannot set codec FEC");
        return -1;
      }
    }

    return 0;
  }
}

int CodecManager::SendCodec(CodecInst* current_codec) const {
  int dummy_id = 0;
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, dummy_id,
               "SendCodec()");

  if (!current_encoder_) {
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, dummy_id,
                 "SendCodec Failed, no codec is registered");
    return -1;
  }
  WebRtcACMCodecParams encoder_param;
  current_encoder_->EncoderParams(&encoder_param);
  encoder_param.codec_inst.pltype = send_codec_inst_.pltype;
  memcpy(current_codec, &(encoder_param.codec_inst), sizeof(CodecInst));

  return 0;
}

// Register possible receive codecs, can be called multiple times,
// for codecs, CNG (NB, WB and SWB), DTMF, RED.
int CodecManager::RegisterReceiveCodec(const CodecInst& codec) {
  if (codec.channels > 2 || codec.channels < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, 0,
                 "Unsupported number of channels, %d.", codec.channels);
    return -1;
  }

  int mirror_id;
  int codec_id = ACMCodecDB::ReceiverCodecNumber(codec, &mirror_id);

  if (codec_id < 0 || codec_id >= ACMCodecDB::kNumCodecs) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, 0,
                 "Wrong codec params to be registered as receive codec");
    return -1;
  }

  // Check if the payload-type is valid.
  if (!ACMCodecDB::ValidPayloadType(codec.pltype)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, 0,
                 "Invalid payload-type %d for %s.", codec.pltype, codec.plname);
    return -1;
  }

  AudioDecoder* decoder = NULL;
  // Get |decoder| associated with |codec|. |decoder| can be NULL if |codec|
  // does not own its decoder.
  if (GetAudioDecoder(codec, codec_id, mirror_id, &decoder) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, 0,
                 "Wrong codec params to be registered as receive codec");
    return -1;
  }
  uint8_t payload_type = static_cast<uint8_t>(codec.pltype);
  return acm_->RegisterDecoder(codec_id, payload_type, codec.channels, decoder);
}

bool CodecManager::SetCopyRed(bool enable) {
  if (enable && codec_fec_enabled_) {
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, 0,
                 "Codec internal FEC and RED cannot be co-enabled.");
    return false;
  }
  if (current_encoder_ && current_encoder_->SetCopyRed(enable) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, 0,
                 "SetCopyRed failed");
    return false;
  }
  red_enabled_ = enable;
  return true;
}

int CodecManager::SetVAD(bool enable_dtx, bool enable_vad, ACMVADMode mode) {
  // Sanity check of the mode.
  if ((mode != VADNormal) && (mode != VADLowBitrate) && (mode != VADAggr) &&
      (mode != VADVeryAggr)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, 0,
                 "Invalid VAD Mode %d, no change is made to VAD/DTX status",
                 mode);
    return -1;
  }

  // Check that the send codec is mono. We don't support VAD/DTX for stereo
  // sending.
  if ((enable_dtx || enable_vad) && stereo_send_) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, 0,
                 "VAD/DTX not supported for stereo sending");
    dtx_enabled_ = false;
    vad_enabled_ = false;
    vad_mode_ = mode;
    return -1;
  }

  // Store VAD/DTX settings. Values can be changed in the call to "SetVAD"
  // below.
  dtx_enabled_ = enable_dtx;
  vad_enabled_ = enable_vad;
  vad_mode_ = mode;

  // If a send codec is registered, set VAD/DTX for the codec.
  if (current_encoder_ &&
      current_encoder_->SetVAD(&dtx_enabled_, &vad_enabled_, &vad_mode_) < 0) {
    // SetVAD failed.
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, 0,
                 "SetVAD failed");
    vad_enabled_ = false;
    dtx_enabled_ = false;
    return -1;
  }
  return 0;
}

void CodecManager::VAD(bool* dtx_enabled,
                       bool* vad_enabled,
                       ACMVADMode* mode) const {
  *dtx_enabled = dtx_enabled_;
  *vad_enabled = vad_enabled_;
  *mode = vad_mode_;
}

int CodecManager::SetCodecFEC(bool enable_codec_fec) {
  if (enable_codec_fec == true && red_enabled_ == true) {
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, 0,
                 "Codec internal FEC and RED cannot be co-enabled.");
    return -1;
  }

  // Set codec FEC.
  if (current_encoder_ && current_encoder_->SetFEC(enable_codec_fec) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, 0,
                 "Set codec internal FEC failed.");
    return -1;
  }
  codec_fec_enabled_ = enable_codec_fec;
  return 0;
}

void CodecManager::SetCngPayloadType(int sample_rate_hz, int payload_type) {
  for (auto* codec : codecs_) {
    if (codec) {
      codec->SetCngPt(sample_rate_hz, payload_type);
    }
  }
}

void CodecManager::SetRedPayloadType(int sample_rate_hz, int payload_type) {
  for (auto* codec : codecs_) {
    if (codec) {
      codec->SetRedPt(sample_rate_hz, payload_type);
    }
  }
}

int CodecManager::GetAudioDecoder(const CodecInst& codec,
                                  int codec_id,
                                  int mirror_id,
                                  AudioDecoder** decoder) {
  if (ACMCodecDB::OwnsDecoder(codec_id)) {
    // This codec has to own its own decoder. Therefore, it should create the
    // corresponding AudioDecoder class and insert it into NetEq. If the codec
    // does not exist create it.
    //
    // TODO(turajs): this part of the code is common with RegisterSendCodec(),
    //               make a method for it.
    if (codecs_[mirror_id] == NULL) {
      codecs_[mirror_id] = ACMCodecDB::CreateCodecInstance(
          codec, cng_nb_pltype_, cng_wb_pltype_, cng_swb_pltype_,
          cng_fb_pltype_, red_enabled_, red_nb_pltype_);
      if (codecs_[mirror_id] == NULL) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, 0,
                     "Cannot Create the codec");
        return -1;
      }
      mirror_codec_idx_[mirror_id] = mirror_id;
    }

    if (mirror_id != codec_id) {
      codecs_[codec_id] = codecs_[mirror_id];
      mirror_codec_idx_[codec_id] = mirror_id;
    }
    *decoder = codecs_[codec_id]->Decoder();
    if (!*decoder) {
      assert(false);
      return -1;
    }
  } else {
    *decoder = NULL;
  }

  return 0;
}

}  // namespace acm2
}  // namespace webrtc
