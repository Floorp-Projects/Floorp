/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "acm_common_defs.h"
#include "acm_neteq.h"
#include "acm_pcmu.h"
#include "trace.h"
#include "webrtc_neteq.h"
#include "webrtc_neteq_help_macros.h"

// Codec interface
#include "g711_interface.h"

namespace webrtc
{

ACMPCMU::ACMPCMU(WebRtc_Word16 codecID)
{
    _codecID = codecID;
}


ACMPCMU::~ACMPCMU()
{
    return;
}


WebRtc_Word16
ACMPCMU::InternalEncode(
    WebRtc_UWord8* bitStream,
    WebRtc_Word16* bitStreamLenByte)
{
    *bitStreamLenByte = WebRtcG711_EncodeU(NULL, &_inAudio[_inAudioIxRead],
        _frameLenSmpl*_noChannels, (WebRtc_Word16*)bitStream);
    // increment the read index this tell the caller that how far
    // we have gone forward in reading the audio buffer
    _inAudioIxRead += _frameLenSmpl*_noChannels;
    return *bitStreamLenByte;
}


WebRtc_Word16
ACMPCMU::DecodeSafe(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16  /* bitStreamLenByte */,
    WebRtc_Word16* /* audio            */,
    WebRtc_Word16* /* audioSamples     */,
    WebRtc_Word8*  /* speechType       */)
{
    return 0;
}


WebRtc_Word16
ACMPCMU::InternalInitEncoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    // This codec does not need initialization,
    // PCM has no instance
    return 0;
}


WebRtc_Word16
ACMPCMU::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */)
{
   // This codec does not need initialization,
   // PCM has no instance
   return 0;
}


WebRtc_Word32
ACMPCMU::CodecDef(
    WebRtcNetEQ_CodecDef& codecDef,
    const CodecInst&      codecInst)
{
    // Fill up the structure by calling
    // "SET_CODEC_PAR" & "SET_PCMU_FUNCTION."
    // Then call NetEQ to add the codec to it's
    // database.
    SET_CODEC_PAR((codecDef), kDecoderPCMu, codecInst.pltype, NULL, 8000);
    SET_PCMU_FUNCTIONS((codecDef));
    return 0;
}


ACMGenericCodec*
ACMPCMU::CreateInstance(void)
{
    return NULL;
}


WebRtc_Word16
ACMPCMU::InternalCreateEncoder()
{
    // PCM has no instance
    return 0;
}


WebRtc_Word16
ACMPCMU::InternalCreateDecoder()
{
    // PCM has no instance
    return 0;
}


void
ACMPCMU::InternalDestructEncoderInst(
    void* /* ptrInst */)
{
    // PCM has no instance
    return;
}


void
ACMPCMU::DestructEncoderSafe()
{
    // PCM has no instance
    _encoderExist = false;
    _encoderInitialized = false;
    return;
}

void ACMPCMU::DestructDecoderSafe()
{
    // PCM has no instance
    _decoderInitialized = false;
    _decoderExist = false;
    return;
}


WebRtc_Word16
ACMPCMU::UnregisterFromNetEqSafe(
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

    return netEq->RemoveCodec(kDecoderPCMu);
}

// Split the stereo packet and place left and right channel after each other
// in the payload vector.
void ACMPCMU::SplitStereoPacket(uint8_t* payload, int32_t* payload_length) {
  uint8_t right_byte;

  // Check for valid inputs.
  assert(payload != NULL);
  assert(*payload_length > 0);

  // Move one bytes representing right channel each loop, and place it at the
  // end of the bytestream vector. After looping the data is reordered to:
  // l1 l2 l3 l4 ... l(N-1) lN r1 r2 r3 r4 ... r(N-1) r(N),
  // where N is the total number of samples.
  for (int i = 0; i < *payload_length / 2; i ++) {
    right_byte = payload[i + 1];
    memmove(&payload[i + 1], &payload[i + 2], *payload_length - i - 2);
    payload[*payload_length - 1] = right_byte;
  }
}

} // namespace webrtc
