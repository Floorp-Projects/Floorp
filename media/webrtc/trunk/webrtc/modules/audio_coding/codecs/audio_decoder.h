/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_NETEQ_INTERFACE_AUDIO_DECODER_H_
#define WEBRTC_MODULES_AUDIO_CODING_NETEQ_INTERFACE_AUDIO_DECODER_H_

#include <stdlib.h>  // NULL

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_coding/codecs/cng/include/webrtc_cng.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// This is the interface class for decoders in NetEQ. Each codec type will have
// and implementation of this class.
class AudioDecoder {
 public:
  enum SpeechType {
    kSpeech = 1,
    kComfortNoise = 2
  };

  // Used by PacketDuration below. Save the value -1 for errors.
  enum { kNotImplemented = -2 };

  AudioDecoder() = default;
  virtual ~AudioDecoder() = default;

  // Decodes |encode_len| bytes from |encoded| and writes the result in
  // |decoded|. The maximum bytes allowed to be written into |decoded| is
  // |max_decoded_bytes|. The number of samples from all channels produced is
  // in the return value. If the decoder produced comfort noise, |speech_type|
  // is set to kComfortNoise, otherwise it is kSpeech. The desired output
  // sample rate is provided in |sample_rate_hz|, which must be valid for the
  // codec at hand.
  virtual int Decode(const uint8_t* encoded,
                     size_t encoded_len,
                     int sample_rate_hz,
                     size_t max_decoded_bytes,
                     int16_t* decoded,
                     SpeechType* speech_type);

  // Same as Decode(), but interfaces to the decoders redundant decode function.
  // The default implementation simply calls the regular Decode() method.
  virtual int DecodeRedundant(const uint8_t* encoded,
                              size_t encoded_len,
                              int sample_rate_hz,
                              size_t max_decoded_bytes,
                              int16_t* decoded,
                              SpeechType* speech_type);

  // Indicates if the decoder implements the DecodePlc method.
  virtual bool HasDecodePlc() const;

  // Calls the packet-loss concealment of the decoder to update the state after
  // one or several lost packets.
  virtual int DecodePlc(int num_frames, int16_t* decoded);

  // Initializes the decoder.
  virtual int Init() = 0;

  // Notifies the decoder of an incoming packet to NetEQ.
  virtual int IncomingPacket(const uint8_t* payload,
                             size_t payload_len,
                             uint16_t rtp_sequence_number,
                             uint32_t rtp_timestamp,
                             uint32_t arrival_timestamp);

  // Returns the last error code from the decoder.
  virtual int ErrorCode();

  // Returns the duration in samples of the payload in |encoded| which is
  // |encoded_len| bytes long. Returns kNotImplemented if no duration estimate
  // is available, or -1 in case of an error.
  virtual int PacketDuration(const uint8_t* encoded, size_t encoded_len) const;

  // Returns the duration in samples of the redandant payload in |encoded| which
  // is |encoded_len| bytes long. Returns kNotImplemented if no duration
  // estimate is available, or -1 in case of an error.
  virtual int PacketDurationRedundant(const uint8_t* encoded,
                                      size_t encoded_len) const;

  // Detects whether a packet has forward error correction. The packet is
  // comprised of the samples in |encoded| which is |encoded_len| bytes long.
  // Returns true if the packet has FEC and false otherwise.
  virtual bool PacketHasFec(const uint8_t* encoded, size_t encoded_len) const;

  // If this is a CNG decoder, return the underlying CNG_dec_inst*. If this
  // isn't a CNG decoder, don't call this method.
  virtual CNG_dec_inst* CngDecoderInstance();

  virtual size_t Channels() const = 0;

 protected:
  static SpeechType ConvertSpeechType(int16_t type);

  virtual int DecodeInternal(const uint8_t* encoded,
                             size_t encoded_len,
                             int sample_rate_hz,
                             int16_t* decoded,
                             SpeechType* speech_type);

  virtual int DecodeRedundantInternal(const uint8_t* encoded,
                                      size_t encoded_len,
                                      int sample_rate_hz,
                                      int16_t* decoded,
                                      SpeechType* speech_type);

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioDecoder);
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_NETEQ_INTERFACE_AUDIO_DECODER_H_
