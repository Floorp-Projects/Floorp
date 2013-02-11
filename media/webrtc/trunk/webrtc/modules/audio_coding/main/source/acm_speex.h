/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_SPEEX_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_SPEEX_H_

#include "webrtc/modules/audio_coding/main/source/acm_generic_codec.h"

// forward declaration
struct SPEEX_encinst_t_;
struct SPEEX_decinst_t_;

namespace webrtc {

class ACMSPEEX : public ACMGenericCodec {
 public:
  explicit ACMSPEEX(WebRtc_Word16 codec_id);
  ~ACMSPEEX();

  // for FEC
  ACMGenericCodec* CreateInstance(void);

  WebRtc_Word16 InternalEncode(WebRtc_UWord8* bitstream,
                               WebRtc_Word16* bitstream_len_byte);

  WebRtc_Word16 InternalInitEncoder(WebRtcACMCodecParams *codec_params);

  WebRtc_Word16 InternalInitDecoder(WebRtcACMCodecParams *codec_params);

 protected:
  WebRtc_Word16 DecodeSafe(WebRtc_UWord8* bitstream,
                           WebRtc_Word16 bitstream_len_byte,
                           WebRtc_Word16* audio,
                           WebRtc_Word16* audio_samples,
                           WebRtc_Word8* speech_type);

  WebRtc_Word32 CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                         const CodecInst& codec_inst);

  void DestructEncoderSafe();

  void DestructDecoderSafe();

  WebRtc_Word16 InternalCreateEncoder();

  WebRtc_Word16 InternalCreateDecoder();

  void InternalDestructEncoderInst(void* ptr_inst);

  WebRtc_Word16 SetBitRateSafe(const WebRtc_Word32 rate);

  WebRtc_Word16 EnableDTX();

  WebRtc_Word16 DisableDTX();

#ifdef UNUSEDSPEEX
  WebRtc_Word16 EnableVBR();

  WebRtc_Word16 DisableVBR();

  WebRtc_Word16 SetComplMode(WebRtc_Word16 mode);
#endif

  SPEEX_encinst_t_* encoder_inst_ptr_;
  SPEEX_decinst_t_* decoder_inst_ptr_;
  WebRtc_Word16 compl_mode_;
  bool vbr_enabled_;
  WebRtc_Word32 encoding_rate_;
  WebRtc_Word16 sampling_frequency_;
  WebRtc_UWord16 samples_in_20ms_audio_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_SPEEX_H_
