/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_NETEQ_AUDIO_DECODER_IMPL_H_
#define WEBRTC_MODULES_AUDIO_CODING_NETEQ_AUDIO_DECODER_IMPL_H_

#include <assert.h>

#include "webrtc/engine_configurations.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_coding/codecs/audio_decoder.h"
#include "webrtc/modules/audio_coding/codecs/cng/webrtc_cng.h"
#ifdef WEBRTC_CODEC_G722
#include "webrtc/modules/audio_coding/codecs/g722/g722_interface.h"
#endif
#include "webrtc/modules/audio_coding/acm2/rent_a_codec.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// AudioDecoderCng is a special type of AudioDecoder. It inherits from
// AudioDecoder just to fit in the DecoderDatabase. None of the class methods
// should be used, except constructor, destructor, and accessors.
// TODO(hlundin): Consider the possibility to create a super-class to
// AudioDecoder that is stored in DecoderDatabase. Then AudioDecoder and a
// specific CngDecoder class could both inherit from that class.
class AudioDecoderCng : public AudioDecoder {
 public:
  explicit AudioDecoderCng();
  ~AudioDecoderCng() override;
  void Reset() override;
  int IncomingPacket(const uint8_t* payload,
                     size_t payload_len,
                     uint16_t rtp_sequence_number,
                     uint32_t rtp_timestamp,
                     uint32_t arrival_timestamp) override;

  CNG_dec_inst* CngDecoderInstance() override;
  size_t Channels() const override;

 protected:
  int DecodeInternal(const uint8_t* encoded,
                     size_t encoded_len,
                     int sample_rate_hz,
                     int16_t* decoded,
                     SpeechType* speech_type) override;

 private:
  CNG_dec_inst* dec_state_;
  RTC_DISALLOW_COPY_AND_ASSIGN(AudioDecoderCng);
};

using NetEqDecoder = acm2::RentACodec::NetEqDecoder;

// Returns true if |codec_type| is supported.
bool CodecSupported(NetEqDecoder codec_type);

// Returns the sample rate for |codec_type|.
int CodecSampleRateHz(NetEqDecoder codec_type);

// Creates an AudioDecoder object of type |codec_type|. Returns NULL for for
// unsupported codecs, and when creating an AudioDecoder is not applicable
// (e.g., for RED and DTMF/AVT types).
AudioDecoder* CreateAudioDecoder(NetEqDecoder codec_type);

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_NETEQ_AUDIO_DECODER_IMPL_H_
