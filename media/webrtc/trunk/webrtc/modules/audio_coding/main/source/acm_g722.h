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

// forward declaration
struct ACMG722EncStr;
struct ACMG722DecStr;

class ACMG722: public ACMGenericCodec {
 public:
  explicit ACMG722(WebRtc_Word16 codec_id);
  ~ACMG722();

  // for FEC
  ACMGenericCodec* CreateInstance(void);

  WebRtc_Word16 InternalEncode(WebRtc_UWord8* bitstream,
                               WebRtc_Word16* bitstream_len_byte);

  WebRtc_Word16 InternalInitEncoder(WebRtcACMCodecParams *codec_params);

  WebRtc_Word16 InternalInitDecoder(WebRtcACMCodecParams *codec_params);

 protected:
  WebRtc_Word16 DecodeSafe(WebRtc_UWord8* bitstream,
                           WebRtc_Word16 bitstream_len_byte,
                           WebRtc_Word16* audio, WebRtc_Word16* audio_samples,
                           WebRtc_Word8* speech_type);

  WebRtc_Word32 CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                         const CodecInst& codec_inst);

  WebRtc_Word32 Add10MsDataSafe(const WebRtc_UWord32 timestamp,
                                const WebRtc_Word16* data,
                                const WebRtc_UWord16 length_smpl,
                                const WebRtc_UWord8 audio_channel);

  void DestructEncoderSafe();

  void DestructDecoderSafe();

  WebRtc_Word16 InternalCreateEncoder();

  WebRtc_Word16 InternalCreateDecoder();

  void InternalDestructEncoderInst(void* ptr_inst);

  void SplitStereoPacket(uint8_t* payload, int32_t* payload_length);

  ACMG722EncStr* ptr_enc_str_;
  ACMG722DecStr* ptr_dec_str_;

  G722EncInst* encoder_inst_ptr_;
  G722EncInst* encoder_inst_ptr_right_; // Prepared for stereo
  G722DecInst* decoder_inst_ptr_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_G722_H_
