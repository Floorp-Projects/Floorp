/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "acm_codec_database.h"
#include "acm_common_defs.h"
#include "acm_neteq.h"
#include "acm_pcm16b.h"
#include "trace.h"
#include "webrtc_neteq.h"
#include "webrtc_neteq_help_macros.h"

#ifdef WEBRTC_CODEC_PCM16
    #include "pcm16b.h"
#endif

namespace webrtc
{

#ifndef WEBRTC_CODEC_PCM16

ACMPCM16B::ACMPCM16B(
    WebRtc_Word16 /* codecID */)
{
    return;
}


ACMPCM16B::~ACMPCM16B()
{
    return;
}


WebRtc_Word16
ACMPCM16B::InternalEncode(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16* /* bitStreamLenByte */)
{
    return -1;
}


WebRtc_Word16
ACMPCM16B::DecodeSafe(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16  /* bitStreamLenByte */,
    WebRtc_Word16* /* audio            */,
    WebRtc_Word16* /* audioSamples     */,
    WebRtc_Word8*  /* speechType       */)
{
    return -1;
}


WebRtc_Word16
ACMPCM16B::InternalInitEncoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    return -1;
}


WebRtc_Word16
ACMPCM16B::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */)
{
   return -1;
}


WebRtc_Word32
ACMPCM16B::CodecDef(
    WebRtcNetEQ_CodecDef& /* codecDef  */,
    const CodecInst&      /* codecInst */)
{
    return -1;
}


ACMGenericCodec*
ACMPCM16B::CreateInstance(void)
{
    return NULL;
}


WebRtc_Word16
ACMPCM16B::InternalCreateEncoder()
{
    return -1;
}


WebRtc_Word16
ACMPCM16B::InternalCreateDecoder()
{
    return -1;
}


void
ACMPCM16B::InternalDestructEncoderInst(
    void* /* ptrInst */)
{
    return;
}


void
ACMPCM16B::DestructEncoderSafe()
{
    return;
}

void
ACMPCM16B::DestructDecoderSafe()
{
    return;
}


WebRtc_Word16
ACMPCM16B::UnregisterFromNetEqSafe(
    ACMNetEQ*     /* netEq       */,
    WebRtc_Word16 /* payloadType */)
{
    return -1;
}


void ACMPCM16B::SplitStereoPacket(uint8_t* /*payload*/,
                                  int32_t* /*payload_length*/) {}

#else     //===================== Actual Implementation =======================


ACMPCM16B::ACMPCM16B(
    WebRtc_Word16 codecID)
{
    _codecID = codecID;
    _samplingFreqHz = ACMCodecDB::CodecFreq(_codecID);
}


ACMPCM16B::~ACMPCM16B()
{
    return;
}


WebRtc_Word16
ACMPCM16B::InternalEncode(
    WebRtc_UWord8* bitStream,
    WebRtc_Word16* bitStreamLenByte)
{
    *bitStreamLenByte = WebRtcPcm16b_Encode(&_inAudio[_inAudioIxRead],
                                            _frameLenSmpl*_noChannels,
                                            bitStream);
    // increment the read index to tell the caller that how far
    // we have gone forward in reading the audio buffer
    _inAudioIxRead += _frameLenSmpl*_noChannels;
    return *bitStreamLenByte;
}


WebRtc_Word16
ACMPCM16B::DecodeSafe(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16  /* bitStreamLenByte */,
    WebRtc_Word16* /* audio            */,
    WebRtc_Word16* /* audioSamples     */,
    WebRtc_Word8*  /* speechType       */)
{
    return 0;
}


WebRtc_Word16
ACMPCM16B::InternalInitEncoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    // This codec does not need initialization,
    // PCM has no instance
    return 0;
}


WebRtc_Word16
ACMPCM16B::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */)
{
   // This codec does not need initialization,
   // PCM has no instance
   return 0;
}


WebRtc_Word32
ACMPCM16B::CodecDef(
    WebRtcNetEQ_CodecDef& codecDef,
    const CodecInst&      codecInst)
{
    // Fill up the structure by calling
    // "SET_CODEC_PAR" & "SET_PCMU_FUNCTION."
    // Then call NetEQ to add the codec to it's
    // database.
    switch(_samplingFreqHz)
    {
    case 8000:
        {
            SET_CODEC_PAR((codecDef), kDecoderPCM16B, codecInst.pltype,
                NULL, 8000);
            SET_PCM16B_FUNCTIONS((codecDef));
            break;
        }
    case 16000:
        {
            SET_CODEC_PAR((codecDef), kDecoderPCM16Bwb, codecInst.pltype,
                NULL, 16000);
            SET_PCM16B_WB_FUNCTIONS((codecDef));
            break;
        }
    case 32000:
        {
            SET_CODEC_PAR((codecDef), kDecoderPCM16Bswb32kHz,
                codecInst.pltype, NULL, 32000);
            SET_PCM16B_SWB32_FUNCTIONS((codecDef));
            break;
        }
    default:
        {
            return -1;
        }
    }
    return 0;
}


ACMGenericCodec*
ACMPCM16B::CreateInstance(void)
{
    return NULL;
}


WebRtc_Word16
ACMPCM16B::InternalCreateEncoder()
{
    // PCM has no instance
    return 0;
}


WebRtc_Word16
ACMPCM16B::InternalCreateDecoder()
{
    // PCM has no instance
    return 0;
}


void
ACMPCM16B::InternalDestructEncoderInst(
    void* /* ptrInst */)
{
    // PCM has no instance
   return;
}


void
ACMPCM16B::DestructEncoderSafe()
{
    // PCM has no instance
    _encoderExist = false;
    _encoderInitialized = false;
     return;
}

void
ACMPCM16B::DestructDecoderSafe()
{
    // PCM has no instance
    _decoderExist = false;
    _decoderInitialized = false;
    return;
}


WebRtc_Word16
ACMPCM16B::UnregisterFromNetEqSafe(
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

    switch(_samplingFreqHz)
    {
    case 8000:
        {
            return netEq->RemoveCodec(kDecoderPCM16B);
        }
    case 16000:
        {
            return netEq->RemoveCodec(kDecoderPCM16Bwb);
        }
    case 32000:
        {
            return netEq->RemoveCodec(kDecoderPCM16Bswb32kHz);
        }
    default:
        {
            return -1;
        }
    }
}

// Split the stereo packet and place left and right channel after each other
// in the payload vector.
void ACMPCM16B::SplitStereoPacket(uint8_t* payload, int32_t* payload_length) {
  uint8_t right_byte_msb;
  uint8_t right_byte_lsb;

  // Check for valid inputs.
  assert(payload != NULL);
  assert(*payload_length > 0);

  // Move two bytes representing right channel each loop, and place it at the
  // end of the bytestream vector. After looping the data is reordered to:
  // l1 l2 l3 l4 ... l(N-1) lN r1 r2 r3 r4 ... r(N-1) r(N),
  // where N is the total number of samples.

  for (int i = 0; i < *payload_length / 2; i += 2) {
    right_byte_msb = payload[i + 2];
    right_byte_lsb = payload[i + 3];
    memmove(&payload[i + 2], &payload[i + 4], *payload_length - i - 4);
    payload[*payload_length - 2] = right_byte_msb;
    payload[*payload_length - 1] = right_byte_lsb;
  }
}
#endif

} // namespace webrtc
