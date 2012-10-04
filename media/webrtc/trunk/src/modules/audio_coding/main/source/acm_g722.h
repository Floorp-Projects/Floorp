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

#include "acm_generic_codec.h"

typedef struct WebRtcG722EncInst G722EncInst;
typedef struct WebRtcG722DecInst G722DecInst;

namespace webrtc {

// forward declaration
struct ACMG722EncStr;
struct ACMG722DecStr;

class ACMG722: public ACMGenericCodec {
 public:
  ACMG722(WebRtc_Word16 codecID);
  ~ACMG722();
  // for FEC
  ACMGenericCodec* CreateInstance(void);

  WebRtc_Word16 InternalEncode(WebRtc_UWord8* bitstream,
                               WebRtc_Word16* bitStreamLenByte);

  WebRtc_Word16 InternalInitEncoder(WebRtcACMCodecParams *codecParams);

  WebRtc_Word16 InternalInitDecoder(WebRtcACMCodecParams *codecParams);

 protected:
  WebRtc_Word16 DecodeSafe(WebRtc_UWord8* bitStream,
                           WebRtc_Word16 bitStreamLenByte,
                           WebRtc_Word16* audio, WebRtc_Word16* audioSamples,
                           WebRtc_Word8* speechType);

  WebRtc_Word32 CodecDef(WebRtcNetEQ_CodecDef& codecDef,
                         const CodecInst& codecInst);

  WebRtc_Word32 Add10MsDataSafe(const WebRtc_UWord32 timestamp,
                                const WebRtc_Word16* data,
                                const WebRtc_UWord16 lengthSmpl,
                                const WebRtc_UWord8 audioChannel);

  void DestructEncoderSafe();

  void DestructDecoderSafe();

  WebRtc_Word16 InternalCreateEncoder();

  WebRtc_Word16 InternalCreateDecoder();

  void InternalDestructEncoderInst(void* ptrInst);

  void SplitStereoPacket(uint8_t* payload, int32_t* payload_length);

  ACMG722EncStr* _ptrEncStr;
  ACMG722DecStr* _ptrDecStr;

  G722EncInst* _encoderInstPtr;
  G722EncInst* _encoderInstPtrRight; // Prepared for stereo
  G722DecInst* _decoderInstPtr;
};

} // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_G722_H_
