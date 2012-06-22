/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "acm_red.h"
#include "acm_neteq.h"
#include "acm_common_defs.h"
#include "trace.h"
#include "webrtc_neteq.h"
#include "webrtc_neteq_help_macros.h"

namespace webrtc
{

ACMRED::ACMRED(WebRtc_Word16 codecID)
{
    _codecID = codecID;
}


ACMRED::~ACMRED()
{
    return;
}


WebRtc_Word16
ACMRED::InternalEncode(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16* /* bitStreamLenByte */)
{
    // RED is never used as an encoder
    // RED has no instance
    return 0;
}


WebRtc_Word16
ACMRED::DecodeSafe(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16  /* bitStreamLenByte */,
    WebRtc_Word16* /* audio            */,
    WebRtc_Word16* /* audioSamples     */,
    WebRtc_Word8*  /* speechType       */)
{
    return 0;
}


WebRtc_Word16
ACMRED::InternalInitEncoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    // This codec does not need initialization,
    // RED has no instance
    return 0;
}


WebRtc_Word16
ACMRED::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */)
{
   // This codec does not need initialization,
   // RED has no instance
   return 0;
}


WebRtc_Word32
ACMRED::CodecDef(
    WebRtcNetEQ_CodecDef& codecDef,
    const CodecInst&      codecInst)
{
    if (!_decoderInitialized)
    {
        // Todo:
        // log error
        return -1;
    }

    // Fill up the structure by calling
    // "SET_CODEC_PAR" & "SET_PCMU_FUNCTION."
    // Then call NetEQ to add the codec to it's
    // database.
    SET_CODEC_PAR((codecDef), kDecoderRED, codecInst.pltype, NULL, 8000);
    SET_RED_FUNCTIONS((codecDef));
    return 0;
}


ACMGenericCodec*
ACMRED::CreateInstance(void)
{
    return NULL;
}


WebRtc_Word16
ACMRED::InternalCreateEncoder()
{
    // RED has no instance
    return 0;
}


WebRtc_Word16
ACMRED::InternalCreateDecoder()
{
    // RED has no instance
    return 0;
}


void
ACMRED::InternalDestructEncoderInst(
    void* /* ptrInst */)
{
    // RED has no instance
    return;
}


void
ACMRED::DestructEncoderSafe()
{
    // RED has no instance
    return;
}

void ACMRED::DestructDecoderSafe()
{
    // RED has no instance
    return;
}


WebRtc_Word16
ACMRED::UnregisterFromNetEqSafe(
    ACMNetEQ*     netEq,
    WebRtc_Word16 payloadType)
{
    if(payloadType != _decoderParams.codecInstant.pltype)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "Cannot unregister codec %s given payload-type %d does not match \
the stored payload type",
            _decoderParams.codecInstant.plname,
            payloadType,
            _decoderParams.codecInstant.pltype);
        return -1;
    }

    return netEq->RemoveCodec(kDecoderRED);
}

} // namespace webrtc
