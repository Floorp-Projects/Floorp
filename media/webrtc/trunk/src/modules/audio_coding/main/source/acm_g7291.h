/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_G729_1_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_G729_1_H_

#include "acm_generic_codec.h"

// forward declaration
struct G729_1_inst_t_;
struct G729_1_inst_t_;

namespace webrtc {

class ACMG729_1: public ACMGenericCodec
{
public:
    ACMG729_1(WebRtc_Word16 codecID);
    ~ACMG729_1();
    // for FEC
    ACMGenericCodec* CreateInstance(void);

    WebRtc_Word16 InternalEncode(
        WebRtc_UWord8* bitstream,
        WebRtc_Word16* bitStreamLenByte);

    WebRtc_Word16 InternalInitEncoder(
        WebRtcACMCodecParams *codecParams);

    WebRtc_Word16 InternalInitDecoder(
        WebRtcACMCodecParams *codecParams);

protected:
    WebRtc_Word16 DecodeSafe(
        WebRtc_UWord8* bitStream,
        WebRtc_Word16  bitStreamLenByte,
        WebRtc_Word16* audio,
        WebRtc_Word16* audioSamples,
        WebRtc_Word8*  speechType);

    WebRtc_Word32 CodecDef(
        WebRtcNetEQ_CodecDef& codecDef,
        const CodecInst& codecInst);

    void DestructEncoderSafe();

    void DestructDecoderSafe();

    WebRtc_Word16 InternalCreateEncoder();

    WebRtc_Word16 InternalCreateDecoder();

    void InternalDestructEncoderInst(
        void* ptrInst);

  WebRtc_Word16 SetBitRateSafe(
    const WebRtc_Word32 rate);

    G729_1_inst_t_* _encoderInstPtr;
    G729_1_inst_t_* _decoderInstPtr;

    WebRtc_UWord16     _myRate;
  WebRtc_Word16     _flag8kHz;
    WebRtc_Word16     _flagG729mode;

};

} // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_G729_1_H_
