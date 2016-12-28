/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/neteq/audio_decoder_impl.h"

#include <assert.h>

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_coding/codecs/cng/webrtc_cng.h"
#include "webrtc/modules/audio_coding/codecs/g711/audio_decoder_pcm.h"
#ifdef WEBRTC_CODEC_G722
#include "webrtc/modules/audio_coding/codecs/g722/audio_decoder_g722.h"
#endif
#ifdef WEBRTC_CODEC_ILBC
#include "webrtc/modules/audio_coding/codecs/ilbc/audio_decoder_ilbc.h"
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
#include "webrtc/modules/audio_coding/codecs/opus/audio_decoder_opus.h"
#endif
#include "webrtc/modules/audio_coding/codecs/pcm16b/audio_decoder_pcm16b.h"

namespace webrtc {

AudioDecoderCng::AudioDecoderCng() {
  RTC_CHECK_EQ(0, WebRtcCng_CreateDec(&dec_state_));
  WebRtcCng_InitDec(dec_state_);
}

AudioDecoderCng::~AudioDecoderCng() {
  WebRtcCng_FreeDec(dec_state_);
}

void AudioDecoderCng::Reset() {
  WebRtcCng_InitDec(dec_state_);
}

int AudioDecoderCng::IncomingPacket(const uint8_t* payload,
                                    size_t payload_len,
                                    uint16_t rtp_sequence_number,
                                    uint32_t rtp_timestamp,
                                    uint32_t arrival_timestamp) {
  return -1;
}

CNG_dec_inst* AudioDecoderCng::CngDecoderInstance() {
  return dec_state_;
}

size_t AudioDecoderCng::Channels() const {
  return 1;
}

int AudioDecoderCng::DecodeInternal(const uint8_t* encoded,
                                    size_t encoded_len,
                                    int sample_rate_hz,
                                    int16_t* decoded,
                                    SpeechType* speech_type) {
  return -1;
}

bool CodecSupported(NetEqDecoder codec_type) {
  switch (codec_type) {
    case NetEqDecoder::kDecoderPCMu:
    case NetEqDecoder::kDecoderPCMa:
    case NetEqDecoder::kDecoderPCMu_2ch:
    case NetEqDecoder::kDecoderPCMa_2ch:
#ifdef WEBRTC_CODEC_ILBC
    case NetEqDecoder::kDecoderILBC:
#endif
#if defined(WEBRTC_CODEC_ISACFX) || defined(WEBRTC_CODEC_ISAC)
    case NetEqDecoder::kDecoderISAC:
#endif
#ifdef WEBRTC_CODEC_ISAC
    case NetEqDecoder::kDecoderISACswb:
#endif
    case NetEqDecoder::kDecoderPCM16B:
    case NetEqDecoder::kDecoderPCM16Bwb:
    case NetEqDecoder::kDecoderPCM16Bswb32kHz:
    case NetEqDecoder::kDecoderPCM16Bswb48kHz:
    case NetEqDecoder::kDecoderPCM16B_2ch:
    case NetEqDecoder::kDecoderPCM16Bwb_2ch:
    case NetEqDecoder::kDecoderPCM16Bswb32kHz_2ch:
    case NetEqDecoder::kDecoderPCM16Bswb48kHz_2ch:
    case NetEqDecoder::kDecoderPCM16B_5ch:
#ifdef WEBRTC_CODEC_G722
    case NetEqDecoder::kDecoderG722:
    case NetEqDecoder::kDecoderG722_2ch:
#endif
#ifdef WEBRTC_CODEC_OPUS
    case NetEqDecoder::kDecoderOpus:
    case NetEqDecoder::kDecoderOpus_2ch:
#endif
    case NetEqDecoder::kDecoderRED:
    case NetEqDecoder::kDecoderAVT:
    case NetEqDecoder::kDecoderCNGnb:
    case NetEqDecoder::kDecoderCNGwb:
    case NetEqDecoder::kDecoderCNGswb32kHz:
    case NetEqDecoder::kDecoderCNGswb48kHz:
    case NetEqDecoder::kDecoderArbitrary: {
      return true;
    }
    default: {
      return false;
    }
  }
}

int CodecSampleRateHz(NetEqDecoder codec_type) {
  switch (codec_type) {
    case NetEqDecoder::kDecoderPCMu:
    case NetEqDecoder::kDecoderPCMa:
    case NetEqDecoder::kDecoderPCMu_2ch:
    case NetEqDecoder::kDecoderPCMa_2ch:
#ifdef WEBRTC_CODEC_ILBC
    case NetEqDecoder::kDecoderILBC:
#endif
    case NetEqDecoder::kDecoderPCM16B:
    case NetEqDecoder::kDecoderPCM16B_2ch:
    case NetEqDecoder::kDecoderPCM16B_5ch:
    case NetEqDecoder::kDecoderCNGnb: {
      return 8000;
    }
#if defined(WEBRTC_CODEC_ISACFX) || defined(WEBRTC_CODEC_ISAC)
    case NetEqDecoder::kDecoderISAC:
#endif
    case NetEqDecoder::kDecoderPCM16Bwb:
    case NetEqDecoder::kDecoderPCM16Bwb_2ch:
#ifdef WEBRTC_CODEC_G722
    case NetEqDecoder::kDecoderG722:
    case NetEqDecoder::kDecoderG722_2ch:
#endif
    case NetEqDecoder::kDecoderCNGwb: {
      return 16000;
    }
#ifdef WEBRTC_CODEC_ISAC
    case NetEqDecoder::kDecoderISACswb:
#endif
    case NetEqDecoder::kDecoderPCM16Bswb32kHz:
    case NetEqDecoder::kDecoderPCM16Bswb32kHz_2ch:
    case NetEqDecoder::kDecoderCNGswb32kHz: {
      return 32000;
    }
    case NetEqDecoder::kDecoderPCM16Bswb48kHz:
    case NetEqDecoder::kDecoderPCM16Bswb48kHz_2ch: {
      return 48000;
    }
#ifdef WEBRTC_CODEC_OPUS
    case NetEqDecoder::kDecoderOpus:
    case NetEqDecoder::kDecoderOpus_2ch: {
      return 48000;
    }
#endif
    case NetEqDecoder::kDecoderCNGswb48kHz: {
      // TODO(tlegrand): Remove limitation once ACM has full 48 kHz support.
      return 32000;
    }
    default: {
      return -1;  // Undefined sample rate.
    }
  }
}

AudioDecoder* CreateAudioDecoder(NetEqDecoder codec_type) {
  if (!CodecSupported(codec_type)) {
    return NULL;
  }
  switch (codec_type) {
    case NetEqDecoder::kDecoderPCMu:
      return new AudioDecoderPcmU(1);
    case NetEqDecoder::kDecoderPCMa:
      return new AudioDecoderPcmA(1);
    case NetEqDecoder::kDecoderPCMu_2ch:
      return new AudioDecoderPcmU(2);
    case NetEqDecoder::kDecoderPCMa_2ch:
      return new AudioDecoderPcmA(2);
#ifdef WEBRTC_CODEC_ILBC
    case NetEqDecoder::kDecoderILBC:
      return new AudioDecoderIlbc;
#endif
#if defined(WEBRTC_CODEC_ISACFX)
    case NetEqDecoder::kDecoderISAC:
      return new AudioDecoderIsacFix();
#elif defined(WEBRTC_CODEC_ISAC)
    case NetEqDecoder::kDecoderISAC:
    case NetEqDecoder::kDecoderISACswb:
      return new AudioDecoderIsac();
#endif
    case NetEqDecoder::kDecoderPCM16B:
    case NetEqDecoder::kDecoderPCM16Bwb:
    case NetEqDecoder::kDecoderPCM16Bswb32kHz:
    case NetEqDecoder::kDecoderPCM16Bswb48kHz:
      return new AudioDecoderPcm16B(1);
    case NetEqDecoder::kDecoderPCM16B_2ch:
    case NetEqDecoder::kDecoderPCM16Bwb_2ch:
    case NetEqDecoder::kDecoderPCM16Bswb32kHz_2ch:
    case NetEqDecoder::kDecoderPCM16Bswb48kHz_2ch:
      return new AudioDecoderPcm16B(2);
    case NetEqDecoder::kDecoderPCM16B_5ch:
      return new AudioDecoderPcm16B(5);
#ifdef WEBRTC_CODEC_G722
    case NetEqDecoder::kDecoderG722:
      return new AudioDecoderG722;
    case NetEqDecoder::kDecoderG722_2ch:
      return new AudioDecoderG722Stereo;
#endif
#ifdef WEBRTC_CODEC_OPUS
    case NetEqDecoder::kDecoderOpus:
      return new AudioDecoderOpus(1);
    case NetEqDecoder::kDecoderOpus_2ch:
      return new AudioDecoderOpus(2);
#endif
    case NetEqDecoder::kDecoderCNGnb:
    case NetEqDecoder::kDecoderCNGwb:
    case NetEqDecoder::kDecoderCNGswb32kHz:
    case NetEqDecoder::kDecoderCNGswb48kHz:
      return new AudioDecoderCng;
    case NetEqDecoder::kDecoderRED:
    case NetEqDecoder::kDecoderAVT:
    case NetEqDecoder::kDecoderArbitrary:
    default: {
      return NULL;
    }
  }
}

}  // namespace webrtc
