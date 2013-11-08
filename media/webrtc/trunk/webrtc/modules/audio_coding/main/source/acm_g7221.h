/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_G7221_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_G7221_H_

#include "webrtc/modules/audio_coding/main/source/acm_generic_codec.h"

// forward declaration
struct G722_1_16_encinst_t_;
struct G722_1_16_decinst_t_;
struct G722_1_24_encinst_t_;
struct G722_1_24_decinst_t_;
struct G722_1_32_encinst_t_;
struct G722_1_32_decinst_t_;
struct G722_1_Inst_t_;

namespace webrtc {

namespace acm1 {

class ACMG722_1: public ACMGenericCodec {
 public:
  explicit ACMG722_1(int16_t codec_id);
  ~ACMG722_1();

  // for FEC
  ACMGenericCodec* CreateInstance(void);

  int16_t InternalEncode(uint8_t* bitstream, int16_t* bitstream_len_byte);

  int16_t InternalInitEncoder(WebRtcACMCodecParams *codec_params);

  int16_t InternalInitDecoder(WebRtcACMCodecParams *codec_params);

 protected:
  int16_t DecodeSafe(uint8_t* bitstream,
                     int16_t bitstream_len_byte,
                     int16_t* audio, int16_t* audio_samples,
                     int8_t* speech_type);

  int32_t CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                   const CodecInst& codec_inst);

  void DestructEncoderSafe();

  void DestructDecoderSafe();

  int16_t InternalCreateEncoder();

  int16_t InternalCreateDecoder();

  void InternalDestructEncoderInst(void* ptr_inst);

  int32_t operational_rate_;

  G722_1_Inst_t_* encoder_inst_ptr_;
  G722_1_Inst_t_* encoder_inst_ptr_right_;  // Used in stereo mode
  G722_1_Inst_t_* decoder_inst_ptr_;

  // Only one set of these pointer is valid at any instance
  G722_1_16_encinst_t_* encoder_inst16_ptr_;
  G722_1_16_encinst_t_* encoder_inst16_ptr_right_;
  G722_1_24_encinst_t_* encoder_inst24_ptr_;
  G722_1_24_encinst_t_* encoder_inst24_ptr_right_;
  G722_1_32_encinst_t_* encoder_inst32_ptr_;
  G722_1_32_encinst_t_* encoder_inst32_ptr_right_;

  // Only one of these pointer is valid at any instance
  G722_1_16_decinst_t_* decoder_inst16_ptr_;
  G722_1_24_decinst_t_* decoder_inst24_ptr_;
  G722_1_32_decinst_t_* decoder_inst32_ptr_;
};

}  // namespace acm1

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_G7221_H_
