/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_CELT_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_CELT_H_

#include "webrtc/modules/audio_coding/main/source/acm_generic_codec.h"

// forward declaration
struct CELT_encinst_t_;
struct CELT_decinst_t_;

namespace webrtc {

namespace acm1 {

class ACMCELT : public ACMGenericCodec {
 public:
  explicit ACMCELT(int16_t codec_id);
  virtual ~ACMCELT();

  virtual ACMGenericCodec* CreateInstance(void) OVERRIDE;

  virtual int16_t InternalEncode(uint8_t* bitstream,
                                 int16_t* bitstream_len_byte) OVERRIDE;

  virtual int16_t InternalInitEncoder(
      WebRtcACMCodecParams* codec_params) OVERRIDE;

  virtual int16_t InternalInitDecoder(
      WebRtcACMCodecParams* codec_params) OVERRIDE;

 protected:
  virtual int16_t DecodeSafe(uint8_t* /* bitstream */,
                             int16_t /* bitstream_len_byte */,
                             int16_t* /* audio */,
                             int16_t* /* audio_samples */,
                             int8_t* /* speech_type */) OVERRIDE;

  virtual int32_t CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                           const CodecInst& codec_inst) OVERRIDE;

  virtual void DestructEncoderSafe() OVERRIDE;

  virtual void DestructDecoderSafe() OVERRIDE;

  virtual int16_t InternalCreateEncoder() OVERRIDE;

  virtual int16_t InternalCreateDecoder() OVERRIDE;

  virtual void InternalDestructEncoderInst(void* ptr_inst) OVERRIDE;

  virtual bool IsTrueStereoCodec() OVERRIDE;

  virtual int16_t SetBitRateSafe(const int32_t rate) OVERRIDE;

  virtual void SplitStereoPacket(uint8_t* payload,
                                 int32_t* payload_length) OVERRIDE;

  CELT_encinst_t_* enc_inst_ptr_;
  CELT_decinst_t_* dec_inst_ptr_;
  uint16_t sampling_freq_;
  int32_t bitrate_;
  uint16_t channels_;
  uint16_t dec_channels_;
};

}  // namespace acm1

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_CELT_H_
