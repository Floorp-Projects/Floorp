/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_GSMFR_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_GSMFR_H_

#include "acm_generic_codec.h"

// forward declaration
struct GSMFR_encinst_t_;
struct GSMFR_decinst_t_;

namespace webrtc {

class ACMGSMFR : public ACMGenericCodec
{
public:
    ACMGSMFR(WebRtc_Word16 codecID);
    ~ACMGSMFR();
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
        const CodecInst&      codecInst);

    void DestructEncoderSafe();

    void DestructDecoderSafe();

    WebRtc_Word16 InternalCreateEncoder();

    WebRtc_Word16 InternalCreateDecoder();

    void InternalDestructEncoderInst(
        void* ptrInst);

    WebRtc_Word16 EnableDTX();

    WebRtc_Word16 DisableDTX();

    WebRtc_Word16 UnregisterFromNetEqSafe(
        ACMNetEQ*     netEq,
        WebRtc_Word16 payloadType);

    GSMFR_encinst_t_* _encoderInstPtr;
    GSMFR_decinst_t_* _decoderInstPtr;
};

} // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_GSMFR_H_
