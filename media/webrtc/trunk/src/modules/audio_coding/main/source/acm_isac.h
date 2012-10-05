/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_ISAC_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_ISAC_H_

#include "acm_generic_codec.h"

namespace webrtc
{

struct ACMISACInst;

enum iSACCodingMode {ADAPTIVE, CHANNEL_INDEPENDENT};


class ACMISAC : public ACMGenericCodec
{
public:
    ACMISAC(WebRtc_Word16 codecID);
    ~ACMISAC();
    // for FEC
    ACMGenericCodec* CreateInstance(void);

    WebRtc_Word16 InternalEncode(
        WebRtc_UWord8* bitstream,
        WebRtc_Word16* bitStreamLenByte);

    WebRtc_Word16 InternalInitEncoder(
        WebRtcACMCodecParams *codecParams);

    WebRtc_Word16 InternalInitDecoder(
        WebRtcACMCodecParams *codecParams);

    WebRtc_Word16 DeliverCachedIsacData(
        WebRtc_UWord8*         bitStream,
        WebRtc_Word16*         bitStreamLenByte,
        WebRtc_UWord32*        timestamp,
        WebRtcACMEncodingType* encodingType,
        const WebRtc_UWord16   isacRate,
        const WebRtc_UWord8    isacBWestimate);

    WebRtc_Word16 DeliverCachedData(
        WebRtc_UWord8*         /* bitStream        */,
        WebRtc_Word16*         /* bitStreamLenByte */,
        WebRtc_UWord32*        /* timestamp        */,
        WebRtcACMEncodingType* /* encodingType     */)
    {
        return -1;
    }

    WebRtc_Word16 UpdateDecoderSampFreq(
        WebRtc_Word16 codecId);

    WebRtc_Word16 UpdateEncoderSampFreq(
        WebRtc_UWord16 sampFreqHz);

    WebRtc_Word16 EncoderSampFreq(
        WebRtc_UWord16& sampFreqHz);

    WebRtc_Word32 ConfigISACBandwidthEstimator(
        const WebRtc_UWord8  initFrameSizeMsec,
        const WebRtc_UWord16 initRateBitPerSec,
        const bool           enforceFrameSize);

    WebRtc_Word32 SetISACMaxPayloadSize(
        const WebRtc_UWord16 maxPayloadLenBytes);

    WebRtc_Word32 SetISACMaxRate(
        const WebRtc_UWord32 maxRateBitPerSec);

    WebRtc_Word16 REDPayloadISAC(
        const WebRtc_Word32  isacRate,
        const WebRtc_Word16  isacBwEstimate,
        WebRtc_UWord8*       payload,
        WebRtc_Word16*       payloadLenBytes);

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

    WebRtc_Word16 SetBitRateSafe(
        const WebRtc_Word32 bitRate);

    WebRtc_Word32 GetEstimatedBandwidthSafe();

    WebRtc_Word32 SetEstimatedBandwidthSafe(WebRtc_Word32 estimatedBandwidth);

    WebRtc_Word32 GetRedPayloadSafe(
        WebRtc_UWord8* redPayload,
        WebRtc_Word16* payloadBytes);

    WebRtc_Word16 InternalCreateEncoder();

    WebRtc_Word16 InternalCreateDecoder();

    void InternalDestructEncoderInst(
        void* ptrInst);

    WebRtc_Word16 Transcode(
        WebRtc_UWord8* bitStream,
        WebRtc_Word16* bitStreamLenByte,
        WebRtc_Word16  qBWE,
        WebRtc_Word32  rate,
        bool           isRED);

    void CurrentRate(WebRtc_Word32& rateBitPerSec);

    void UpdateFrameLen();

    bool DecoderParamsSafe(
        WebRtcACMCodecParams *decParams,
        const WebRtc_UWord8  payloadType);

    void SaveDecoderParamSafe(
        const WebRtcACMCodecParams* codecParams);

    ACMISACInst* _codecInstPtr;

    bool                  _isEncInitialized;
    iSACCodingMode        _isacCodingMode;
    bool                  _enforceFrameSize;
    WebRtc_Word32         _isacCurrentBN;
    WebRtc_UWord16        _samplesIn10MsAudio;
    WebRtcACMCodecParams  _decoderParams32kHz;
};

} //namespace

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_ISAC_H_
