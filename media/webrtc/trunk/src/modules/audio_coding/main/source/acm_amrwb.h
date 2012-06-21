/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_AMRWB_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_AMRWB_H_

#include "acm_generic_codec.h"

// forward declaration
struct AMRWB_encinst_t_;
struct AMRWB_decinst_t_;

namespace webrtc {

enum ACMAMRPackingFormat;

class ACMAMRwb: public ACMGenericCodec {
 public:
  ACMAMRwb(WebRtc_Word16 codecID);
  ~ACMAMRwb();
  // for FEC
  ACMGenericCodec* CreateInstance(void);

  WebRtc_Word16 InternalEncode(WebRtc_UWord8* bitstream,
                               WebRtc_Word16* bitStreamLenByte);

  WebRtc_Word16 InternalInitEncoder(WebRtcACMCodecParams* codecParams);

  WebRtc_Word16 InternalInitDecoder(WebRtcACMCodecParams* codecParams);

  WebRtc_Word16 SetAMRwbEncoderPackingFormat(
      const ACMAMRPackingFormat packingFormat);

  ACMAMRPackingFormat AMRwbEncoderPackingFormat() const;

  WebRtc_Word16 SetAMRwbDecoderPackingFormat(
      const ACMAMRPackingFormat packingFormat);

  ACMAMRPackingFormat AMRwbDecoderPackingFormat() const;

 protected:
  WebRtc_Word16 DecodeSafe(WebRtc_UWord8* bitStream,
                           WebRtc_Word16 bitStreamLenByte,
                           WebRtc_Word16* audio, WebRtc_Word16* audioSamples,
                           WebRtc_Word8* speechType);

  WebRtc_Word32 CodecDef(WebRtcNetEQ_CodecDef& codecDef,
                         const CodecInst& codecInst);

  void DestructEncoderSafe();

  void DestructDecoderSafe();

  WebRtc_Word16 InternalCreateEncoder();

  WebRtc_Word16 InternalCreateDecoder();

  void InternalDestructEncoderInst(void* ptrInst);

  WebRtc_Word16 SetBitRateSafe(const WebRtc_Word32 rate);

  WebRtc_Word16 EnableDTX();

  WebRtc_Word16 DisableDTX();

  WebRtc_Word16 UnregisterFromNetEqSafe(ACMNetEQ* netEq,
                                        WebRtc_Word16 payloadType);

  AMRWB_encinst_t_* _encoderInstPtr;
  AMRWB_decinst_t_* _decoderInstPtr;

  WebRtc_Word16 _encodingMode;
  WebRtc_Word16 _encodingRate;
  ACMAMRPackingFormat _encoderPackingFormat;
  ACMAMRPackingFormat _decoderPackingFormat;
};

} // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_AMRWB_H_
