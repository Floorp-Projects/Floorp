/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_G722_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_G722_H_

#include "webrtc/modules/audio_coding/main/source/acm_generic_codec.h"

typedef struct WebRtcG722EncInst G722EncInst;
typedef struct WebRtcG722DecInst G722DecInst;

namespace webrtc {

namespace acm1 {

// forward declaration
struct ACMG722EncStr;
struct ACMG722DecStr;

class ACMG722 : public ACMGenericCodec {
 public:
  explicit ACMG722(int16_t codec_id);
  virtual ~ACMG722();

  // for FEC
  virtual ACMGenericCodec* CreateInstance(void) OVERRIDE;

  virtual int16_t InternalEncode(uint8_t* bitstream,
                                 int16_t* bitstream_len_byte) OVERRIDE;

  virtual int16_t InternalInitEncoder(
      WebRtcACMCodecParams* codec_params) OVERRIDE;

  virtual int16_t InternalInitDecoder(
      WebRtcACMCodecParams* codec_params) OVERRIDE;

 protected:
  virtual int16_t DecodeSafe(uint8_t* bitstream,
                             int16_t bitstream_len_byte,
                             int16_t* audio,
                             int16_t* audio_samples,
                             int8_t* speech_type) OVERRIDE;

  virtual int32_t CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                           const CodecInst& codec_inst) OVERRIDE;

  virtual int32_t Add10MsDataSafe(const uint32_t timestamp,
                                  const int16_t* data,
                                  const uint16_t length_smpl,
                                  const uint8_t audio_channel) OVERRIDE;

  virtual void DestructEncoderSafe() OVERRIDE;

  virtual void DestructDecoderSafe() OVERRIDE;

  virtual int16_t InternalCreateEncoder() OVERRIDE;

  virtual int16_t InternalCreateDecoder() OVERRIDE;

  virtual void InternalDestructEncoderInst(void* ptr_inst) OVERRIDE;

  virtual void SplitStereoPacket(uint8_t* payload,
                                 int32_t* payload_length) OVERRIDE;

  ACMG722EncStr* ptr_enc_str_;
  ACMG722DecStr* ptr_dec_str_;

  G722EncInst* encoder_inst_ptr_;
  G722EncInst* encoder_inst_ptr_right_;  // Prepared for stereo
  G722DecInst* decoder_inst_ptr_;
};

}  // namespace acm1

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_G722_H_
