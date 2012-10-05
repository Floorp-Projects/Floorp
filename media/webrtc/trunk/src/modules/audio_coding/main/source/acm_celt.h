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

#include "acm_generic_codec.h"

// forward declaration
struct CELT_encinst_t_;
struct CELT_decinst_t_;

namespace webrtc {

class ACMCELT : public ACMGenericCodec {
 public:
  ACMCELT(int16_t codecID);
  ~ACMCELT();

  ACMGenericCodec* CreateInstance(void);

  int16_t InternalEncode(uint8_t* bitstream, int16_t* bitStreamLenByte);

  int16_t InternalInitEncoder(WebRtcACMCodecParams *codecParams);

  int16_t InternalInitDecoder(WebRtcACMCodecParams *codecParams);

 protected:

  WebRtc_Word16 DecodeSafe(
      uint8_t* /* bitStream */,
      int16_t /* bitStreamLenByte */,
      int16_t* /* audio */,
      int16_t* /* audioSamples */,
      // TODO(leozwang): use int8_t here when WebRtc_Word8 is properly typed.
      // http://code.google.com/p/webrtc/issues/detail?id=311
      WebRtc_Word8* /* speechType */);

  int32_t CodecDef(WebRtcNetEQ_CodecDef& codecDef, const CodecInst& codecInst);

  void DestructEncoderSafe();

  void DestructDecoderSafe();

  int16_t InternalCreateEncoder();

  int16_t InternalCreateDecoder();

  void InternalDestructEncoderInst(void* ptrInst);

  bool IsTrueStereoCodec();

  int16_t SetBitRateSafe(const int32_t rate);

  void SplitStereoPacket(uint8_t* payload, int32_t* payload_length);

  CELT_encinst_t_* enc_inst_ptr_;
  CELT_decinst_t_* dec_inst_ptr_;
  uint16_t sampling_freq_;
  int32_t bitrate_;
  uint16_t channels_;
  uint16_t dec_channels_;
};

} // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_CELT_H_
