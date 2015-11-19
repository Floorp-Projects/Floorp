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

#ifndef AUDIO_DECODER_UNITTEST
// If this is compiled as a part of the audio_deoder_unittest, the codec
// selection is made in the gypi file instead of in engine_configurations.h.
#include "webrtc/engine_configurations.h"
#endif
#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_coding/codecs/audio_decoder.h"
#include "webrtc/modules/audio_coding/codecs/cng/include/webrtc_cng.h"
#ifdef WEBRTC_CODEC_G722
#include "webrtc/modules/audio_coding/codecs/g722/include/g722_interface.h"
#endif
#ifdef WEBRTC_CODEC_ILBC
#include "webrtc/modules/audio_coding/codecs/ilbc/interface/ilbc.h"
#endif
#ifdef WEBRTC_CODEC_OPUS
#include "webrtc/modules/audio_coding/codecs/opus/interface/opus_interface.h"
#endif
#include "webrtc/typedefs.h"

namespace webrtc {

class AudioDecoderPcmU : public AudioDecoder {
 public:
  AudioDecoderPcmU() {}
  virtual int Init() { return 0; }
  virtual int PacketDuration(const uint8_t* encoded, size_t encoded_len) const;
  size_t Channels() const override { return 1; }

 protected:
  int DecodeInternal(const uint8_t* encoded,
                     size_t encoded_len,
                     int sample_rate_hz,
                     int16_t* decoded,
                     SpeechType* speech_type) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDecoderPcmU);
};

class AudioDecoderPcmA : public AudioDecoder {
 public:
  AudioDecoderPcmA() {}
  virtual int Init() { return 0; }
  virtual int PacketDuration(const uint8_t* encoded, size_t encoded_len) const;
  size_t Channels() const override { return 1; }

 protected:
  int DecodeInternal(const uint8_t* encoded,
                     size_t encoded_len,
                     int sample_rate_hz,
                     int16_t* decoded,
                     SpeechType* speech_type) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDecoderPcmA);
};

class AudioDecoderPcmUMultiCh : public AudioDecoderPcmU {
 public:
  explicit AudioDecoderPcmUMultiCh(size_t channels)
      : AudioDecoderPcmU(), channels_(channels) {
    assert(channels > 0);
  }
  size_t Channels() const override { return channels_; }

 private:
  const size_t channels_;
  DISALLOW_COPY_AND_ASSIGN(AudioDecoderPcmUMultiCh);
};

class AudioDecoderPcmAMultiCh : public AudioDecoderPcmA {
 public:
  explicit AudioDecoderPcmAMultiCh(size_t channels)
      : AudioDecoderPcmA(), channels_(channels) {
    assert(channels > 0);
  }
  size_t Channels() const override { return channels_; }

 private:
  const size_t channels_;
  DISALLOW_COPY_AND_ASSIGN(AudioDecoderPcmAMultiCh);
};

#ifdef WEBRTC_CODEC_PCM16
// This class handles all four types (i.e., sample rates) of PCM16B codecs.
// The type is specified in the constructor parameter |type|.
class AudioDecoderPcm16B : public AudioDecoder {
 public:
  AudioDecoderPcm16B();
  virtual int Init() { return 0; }
  virtual int PacketDuration(const uint8_t* encoded, size_t encoded_len) const;
  size_t Channels() const override { return 1; }

 protected:
  int DecodeInternal(const uint8_t* encoded,
                     size_t encoded_len,
                     int sample_rate_hz,
                     int16_t* decoded,
                     SpeechType* speech_type) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDecoderPcm16B);
};

// This class handles all four types (i.e., sample rates) of PCM16B codecs.
// The type is specified in the constructor parameter |type|, and the number
// of channels is derived from the type.
class AudioDecoderPcm16BMultiCh : public AudioDecoderPcm16B {
 public:
  explicit AudioDecoderPcm16BMultiCh(int num_channels);
  size_t Channels() const override { return channels_; }

 private:
  const size_t channels_;
  DISALLOW_COPY_AND_ASSIGN(AudioDecoderPcm16BMultiCh);
};
#endif

#ifdef WEBRTC_CODEC_ILBC
class AudioDecoderIlbc : public AudioDecoder {
 public:
  AudioDecoderIlbc();
  virtual ~AudioDecoderIlbc();
  virtual bool HasDecodePlc() const { return true; }
  virtual int DecodePlc(int num_frames, int16_t* decoded);
  virtual int Init();
  size_t Channels() const override { return 1; }

 protected:
  int DecodeInternal(const uint8_t* encoded,
                     size_t encoded_len,
                     int sample_rate_hz,
                     int16_t* decoded,
                     SpeechType* speech_type) override;

 private:
  IlbcDecoderInstance* dec_state_;
  DISALLOW_COPY_AND_ASSIGN(AudioDecoderIlbc);
};
#endif

#ifdef WEBRTC_CODEC_G722
class AudioDecoderG722 : public AudioDecoder {
 public:
  AudioDecoderG722();
  virtual ~AudioDecoderG722();
  virtual bool HasDecodePlc() const { return false; }
  virtual int Init();
  virtual int PacketDuration(const uint8_t* encoded, size_t encoded_len) const;
  size_t Channels() const override { return 1; }

 protected:
  int DecodeInternal(const uint8_t* encoded,
                     size_t encoded_len,
                     int sample_rate_hz,
                     int16_t* decoded,
                     SpeechType* speech_type) override;

 private:
  G722DecInst* dec_state_;
  DISALLOW_COPY_AND_ASSIGN(AudioDecoderG722);
};

class AudioDecoderG722Stereo : public AudioDecoder {
 public:
  AudioDecoderG722Stereo();
  virtual ~AudioDecoderG722Stereo();
  virtual int Init();

 protected:
  int DecodeInternal(const uint8_t* encoded,
                     size_t encoded_len,
                     int sample_rate_hz,
                     int16_t* decoded,
                     SpeechType* speech_type) override;
  size_t Channels() const override { return 2; }

 private:
  // Splits the stereo-interleaved payload in |encoded| into separate payloads
  // for left and right channels. The separated payloads are written to
  // |encoded_deinterleaved|, which must hold at least |encoded_len| samples.
  // The left channel starts at offset 0, while the right channel starts at
  // offset encoded_len / 2 into |encoded_deinterleaved|.
  void SplitStereoPacket(const uint8_t* encoded, size_t encoded_len,
                         uint8_t* encoded_deinterleaved);

  G722DecInst* dec_state_left_;
  G722DecInst* dec_state_right_;

  DISALLOW_COPY_AND_ASSIGN(AudioDecoderG722Stereo);
};
#endif

#ifdef WEBRTC_CODEC_OPUS
class AudioDecoderOpus : public AudioDecoder {
 public:
  explicit AudioDecoderOpus(int num_channels);
  virtual ~AudioDecoderOpus();

  virtual int Init();
  virtual int PacketDuration(const uint8_t* encoded, size_t encoded_len) const;
  virtual int PacketDurationRedundant(const uint8_t* encoded,
                                      size_t encoded_len) const;
  virtual bool PacketHasFec(const uint8_t* encoded, size_t encoded_len) const;
  size_t Channels() const override { return channels_; }

 protected:
  int DecodeInternal(const uint8_t* encoded,
                     size_t encoded_len,
                     int sample_rate_hz,
                     int16_t* decoded,
                     SpeechType* speech_type) override;
  int DecodeRedundantInternal(const uint8_t* encoded,
                              size_t encoded_len,
                              int sample_rate_hz,
                              int16_t* decoded,
                              SpeechType* speech_type) override;

 private:
  OpusDecInst* dec_state_;
  const size_t channels_;
  DISALLOW_COPY_AND_ASSIGN(AudioDecoderOpus);
};
#endif

// AudioDecoderCng is a special type of AudioDecoder. It inherits from
// AudioDecoder just to fit in the DecoderDatabase. None of the class methods
// should be used, except constructor, destructor, and accessors.
// TODO(hlundin): Consider the possibility to create a super-class to
// AudioDecoder that is stored in DecoderDatabase. Then AudioDecoder and a
// specific CngDecoder class could both inherit from that class.
class AudioDecoderCng : public AudioDecoder {
 public:
  explicit AudioDecoderCng();
  virtual ~AudioDecoderCng();
  virtual int Init();
  virtual int IncomingPacket(const uint8_t* payload,
                             size_t payload_len,
                             uint16_t rtp_sequence_number,
                             uint32_t rtp_timestamp,
                             uint32_t arrival_timestamp) { return -1; }

  CNG_dec_inst* CngDecoderInstance() override { return dec_state_; }
  size_t Channels() const override { return 1; }

 protected:
  int DecodeInternal(const uint8_t* encoded,
                     size_t encoded_len,
                     int sample_rate_hz,
                     int16_t* decoded,
                     SpeechType* speech_type) override  {
    return -1;
  }

 private:
  CNG_dec_inst* dec_state_;
  DISALLOW_COPY_AND_ASSIGN(AudioDecoderCng);
};

enum NetEqDecoder {
  kDecoderPCMu,
  kDecoderPCMa,
  kDecoderPCMu_2ch,
  kDecoderPCMa_2ch,
  kDecoderILBC,
  kDecoderISAC,
  kDecoderISACswb,
  kDecoderISACfb,
  kDecoderPCM16B,
  kDecoderPCM16Bwb,
  kDecoderPCM16Bswb32kHz,
  kDecoderPCM16Bswb48kHz,
  kDecoderPCM16B_2ch,
  kDecoderPCM16Bwb_2ch,
  kDecoderPCM16Bswb32kHz_2ch,
  kDecoderPCM16Bswb48kHz_2ch,
  kDecoderPCM16B_5ch,
  kDecoderG722,
  kDecoderG722_2ch,
  kDecoderRED,
  kDecoderAVT,
  kDecoderCNGnb,
  kDecoderCNGwb,
  kDecoderCNGswb32kHz,
  kDecoderCNGswb48kHz,
  kDecoderArbitrary,
  kDecoderOpus,
  kDecoderOpus_2ch,
};

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
