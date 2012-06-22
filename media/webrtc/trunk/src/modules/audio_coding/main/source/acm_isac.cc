/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "acm_codec_database.h"
#include "acm_common_defs.h"
#include "acm_isac.h"
#include "acm_neteq.h"
#include "trace.h"
#include "webrtc_neteq.h"
#include "webrtc_neteq_help_macros.h"


#ifdef WEBRTC_CODEC_ISAC
    #include "acm_isac_macros.h"
    #include "isac.h"
#endif

#ifdef WEBRTC_CODEC_ISACFX
    #include "acm_isac_macros.h"
    #include "isacfix.h"
#endif

namespace webrtc
{

// we need this otherwise we cannot use forward declaration
// in the header file
#if (defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX))
struct ACMISACInst
{
    ACM_ISAC_STRUCT *inst;
};
#endif

#define ISAC_MIN_RATE 10000
#define ISAC_MAX_RATE 56000


// How the scaling is computed. iSAC computes a gain based on the
// bottleneck. It follows the following expression for that
//
// G(BN_kbps) = pow(10, (a + b * BN_kbps + c * BN_kbps * BN_kbps) / 20.0)
//              / 3.4641;
//
// Where for 30 ms framelength we have,
//
// a = -23; b = 0.48; c = 0;
//
// As the default encoder is operating at 32kbps we have the scale as
//
// S(BN_kbps) = G(BN_kbps) / G(32);

#define ISAC_NUM_SUPPORTED_RATES 9
const WebRtc_UWord16 isacSuportedRates[ISAC_NUM_SUPPORTED_RATES] = {
    32000,    30000,    26000,   23000,   21000,
    19000,    17000,   15000,    12000};

const float isacScale[ISAC_NUM_SUPPORTED_RATES] = {
     1.0f,    0.8954f,  0.7178f, 0.6081f, 0.5445f,
     0.4875f, 0.4365f,  0.3908f, 0.3311f};

// Tables for bandwidth estimates
#define NR_ISAC_BANDWIDTHS 24
const WebRtc_Word32 isacRatesWB[NR_ISAC_BANDWIDTHS] =
{
    10000, 11100, 12300, 13700, 15200, 16900,
    18800, 20900, 23300, 25900, 28700, 31900,
    10100, 11200, 12400, 13800, 15300, 17000,
    18900, 21000, 23400, 26000, 28800, 32000};


const WebRtc_Word32 isacRatesSWB[NR_ISAC_BANDWIDTHS] =
{
    10000, 11000, 12400, 13800, 15300, 17000,
    18900, 21000, 23200, 25400, 27600, 29800,
    32000, 34100, 36300, 38500, 40700, 42900,
    45100, 47300, 49500, 51700, 53900, 56000,
};

#if (!defined(WEBRTC_CODEC_ISAC) && !defined(WEBRTC_CODEC_ISACFX))

ACMISAC::ACMISAC(WebRtc_Word16 /* codecID */)
    : _codecInstPtr(NULL),
      _isEncInitialized(false),
      _isacCodingMode(CHANNEL_INDEPENDENT),
      _enforceFrameSize(false),
      _isacCurrentBN(32000),
      _samplesIn10MsAudio(160) {  // Initiates to 16 kHz mode.
  // Initiate decoder parameters for the 32 kHz mode.
  memset(&_decoderParams32kHz, 0, sizeof(WebRtcACMCodecParams));
  _decoderParams32kHz.codecInstant.pltype = -1;

  return;
}


ACMISAC::~ACMISAC()
{
    return;
}


ACMGenericCodec*
ACMISAC::CreateInstance(void)
{
    return NULL;
}


WebRtc_Word16
ACMISAC::InternalEncode(
    WebRtc_UWord8* /* bitstream        */,
    WebRtc_Word16* /* bitStreamLenByte */)
{
    return -1;
}


WebRtc_Word16
ACMISAC::DecodeSafe(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16  /* bitStreamLenByte */,
    WebRtc_Word16* /* audio            */,
    WebRtc_Word16* /* audioSamples     */,
    WebRtc_Word8*  /* speechType       */)
{
    return 0;
}


WebRtc_Word16
ACMISAC::InternalInitEncoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    return -1;
}


WebRtc_Word16
ACMISAC::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */)
{
    return -1;
}


WebRtc_Word16
ACMISAC::InternalCreateDecoder()
{
    return -1;
}


void
ACMISAC::DestructDecoderSafe()
{
    return;
}


WebRtc_Word16
ACMISAC::InternalCreateEncoder()
{
    return -1;
}


void
ACMISAC::DestructEncoderSafe()
{
    return;
}


WebRtc_Word32
ACMISAC::CodecDef(
    WebRtcNetEQ_CodecDef& /* codecDef  */,
    const CodecInst&      /* codecInst */)
{
    return -1;
}


void
ACMISAC::InternalDestructEncoderInst(
    void* /* ptrInst */)
{
    return;
}

WebRtc_Word16
ACMISAC::DeliverCachedIsacData(
    WebRtc_UWord8*         /* bitStream        */,
    WebRtc_Word16*         /* bitStreamLenByte */,
    WebRtc_UWord32*        /* timestamp        */,
    WebRtcACMEncodingType* /* encodingType     */,
    const WebRtc_UWord16   /* isacRate         */,
    const WebRtc_UWord8    /* isacBWestimate   */)
{
    return -1;
}


WebRtc_Word16
ACMISAC::Transcode(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16* /* bitStreamLenByte */,
    WebRtc_Word16  /* qBWE             */,
    WebRtc_Word32  /* scale            */,
    bool           /* isRED            */)
{
    return -1;
}

WebRtc_Word16
ACMISAC::SetBitRateSafe(
    WebRtc_Word32 /* bitRate */)
{
    return -1;
}

WebRtc_Word32
ACMISAC::GetEstimatedBandwidthSafe()
{
    return -1;
}

WebRtc_Word32
ACMISAC::SetEstimatedBandwidthSafe(
    WebRtc_Word32 /* estimatedBandwidth */)
{
    return -1;
}

WebRtc_Word32
ACMISAC::GetRedPayloadSafe(
    WebRtc_UWord8* /* redPayload   */,
    WebRtc_Word16* /* payloadBytes */)
{
    return -1;
}


WebRtc_Word16
ACMISAC::UnregisterFromNetEqSafe(
    ACMNetEQ*     /* netEq       */,
    WebRtc_Word16 /* payloadType */)
{
    return -1;
}


WebRtc_Word16
ACMISAC::UpdateDecoderSampFreq(
    WebRtc_Word16 /* codecId */)
{
    return -1;
}


WebRtc_Word16
ACMISAC::UpdateEncoderSampFreq(
    WebRtc_UWord16 /* encoderSampFreqHz */)
{
    return -1;
}

WebRtc_Word16
ACMISAC::EncoderSampFreq(
        WebRtc_UWord16& /* sampFreqHz */)
{
    return -1;
}

WebRtc_Word32
ACMISAC::ConfigISACBandwidthEstimator(
    const WebRtc_UWord8  /* initFrameSizeMsec */,
    const WebRtc_UWord16 /* initRateBitPerSec */,
    const bool           /* enforceFrameSize  */)
{
    return -1;
}

WebRtc_Word32
ACMISAC::SetISACMaxPayloadSize(
    const WebRtc_UWord16 /* maxPayloadLenBytes */)
{
    return -1;
}

WebRtc_Word32
ACMISAC::SetISACMaxRate(
        const WebRtc_UWord32 /* maxRateBitPerSec */)
{
    return -1;
}


void
ACMISAC::UpdateFrameLen()
{
    return;
}

void
ACMISAC::CurrentRate(
    WebRtc_Word32& /*rateBitPerSec */)
{
    return;
}

bool
ACMISAC::DecoderParamsSafe(
    WebRtcACMCodecParams* /* decParams   */,
    const WebRtc_UWord8   /* payloadType */)
{
    return false;
}

void
ACMISAC::SaveDecoderParamSafe(
    const WebRtcACMCodecParams* /* codecParams */)
{
    return;
}

WebRtc_Word16
ACMISAC::REDPayloadISAC(
        const WebRtc_Word32 /* isacRate        */,
        const WebRtc_Word16 /* isacBwEstimate  */,
        WebRtc_UWord8*      /* payload         */,
        WebRtc_Word16*      /* payloadLenBytes */)
{
    return -1;
}


#else     //===================== Actual Implementation =======================



#ifdef WEBRTC_CODEC_ISACFX

enum IsacSamplingRate
{
    kIsacWideband = 16,
    kIsacSuperWideband = 32
};

static float
ACMISACFixTranscodingScale(
    WebRtc_UWord16 rate)
{
    // find the scale for transcoding, the scale is rounded
    // downward
    float  scale = -1;
    for(WebRtc_Word16 n=0; n < ISAC_NUM_SUPPORTED_RATES; n++)
    {
        if(rate >= isacSuportedRates[n])
        {
            scale = isacScale[n];
            break;
        }
    }
    return scale;
}

static void
ACMISACFixGetSendBitrate(
    ACM_ISAC_STRUCT* inst,
    WebRtc_Word32*   bottleNeck)
{
    *bottleNeck = WebRtcIsacfix_GetUplinkBw(inst);
}

static WebRtc_Word16
ACMISACFixGetNewBitstream(
    ACM_ISAC_STRUCT* inst,
    WebRtc_Word16    BWEIndex,
    WebRtc_Word16    /* jitterIndex */,
    WebRtc_Word32    rate,
    WebRtc_Word16*   bitStream,
    bool             isRED)
{
    if (isRED)
    {
        // RED not supported with iSACFIX
        return -1;
    }
    float scale = ACMISACFixTranscodingScale((WebRtc_UWord16)rate);
    return WebRtcIsacfix_GetNewBitStream(inst, BWEIndex, scale, bitStream);
}


static WebRtc_Word16
ACMISACFixGetSendBWE(
    ACM_ISAC_STRUCT* inst,
    WebRtc_Word16*   rateIndex,
    WebRtc_Word16*   /* dummy */)
{
    WebRtc_Word16 localRateIndex;
    WebRtc_Word16 status = WebRtcIsacfix_GetDownLinkBwIndex(inst, &localRateIndex);
    if(status < 0)
    {
        return -1;
    }
    else
    {
        *rateIndex = localRateIndex;
        return 0;
    }
}

static WebRtc_Word16
ACMISACFixControlBWE(
    ACM_ISAC_STRUCT* inst,
    WebRtc_Word32    rateBPS,
    WebRtc_Word16    frameSizeMs,
    WebRtc_Word16    enforceFrameSize)
{
    return WebRtcIsacfix_ControlBwe(inst, (WebRtc_Word16)rateBPS,
        frameSizeMs, enforceFrameSize);
}

static WebRtc_Word16
ACMISACFixControl(
    ACM_ISAC_STRUCT* inst,
    WebRtc_Word32    rateBPS,
    WebRtc_Word16    frameSizeMs)
{
    return WebRtcIsacfix_Control(inst, (WebRtc_Word16)rateBPS,
        frameSizeMs);
}

static IsacSamplingRate
ACMISACFixGetEncSampRate(
    ACM_ISAC_STRUCT* /* inst */)
{
    return kIsacWideband;
}


static IsacSamplingRate
ACMISACFixGetDecSampRate(
    ACM_ISAC_STRUCT* /* inst */)
{
    return kIsacWideband;
}

#endif






ACMISAC::ACMISAC(WebRtc_Word16 codecID)
    : _isEncInitialized(false),
      _isacCodingMode(CHANNEL_INDEPENDENT),
      _enforceFrameSize(false),
      _isacCurrentBN(32000),
      _samplesIn10MsAudio(160) {  // Initiates to 16 kHz mode.
  _codecID = codecID;

  // Create codec instance.
  _codecInstPtr = new ACMISACInst;
  if (_codecInstPtr == NULL) {
    return;
  }
  _codecInstPtr->inst = NULL;

  // Initiate decoder parameters for the 32 kHz mode.
  memset(&_decoderParams32kHz, 0, sizeof(WebRtcACMCodecParams));
  _decoderParams32kHz.codecInstant.pltype = -1;

  // TODO(tlegrand): Check if the following is really needed, now that
  // ACMGenericCodec has been updated to initialize this value.
  // Initialize values that can be used uninitialized otherwise
  _decoderParams.codecInstant.pltype = -1;
}


ACMISAC::~ACMISAC()
{
    if (_codecInstPtr != NULL)
    {
        if(_codecInstPtr->inst != NULL)
        {
            ACM_ISAC_FREE(_codecInstPtr->inst);
            _codecInstPtr->inst = NULL;
        }
        delete _codecInstPtr;
        _codecInstPtr = NULL;
    }
    return;
}


ACMGenericCodec*
ACMISAC::CreateInstance(void)
{
    return NULL;
}


WebRtc_Word16
ACMISAC::InternalEncode(
    WebRtc_UWord8* bitstream,
    WebRtc_Word16* bitStreamLenByte)
{
    // ISAC takes 10ms audio everytime we call encoder, therefor,
    // it should be treated like codecs with 'basic coding block'
    // non-zero, and the following 'while-loop' should not be necessary.
    // However, due to a mistake in the codec the frame-size might change
    // at the first 10ms pushed in to iSAC if the bit-rate is low, this is
    // sort of a bug in iSAC. to address this we treat iSAC as the
    // following.

    if (_codecInstPtr == NULL)
    {
        return -1;
    }
    *bitStreamLenByte = 0;
    while((*bitStreamLenByte == 0) && (_inAudioIxRead < _frameLenSmpl))
    {
        if(_inAudioIxRead > _inAudioIxWrite)
        {
            // something is wrong.
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                "The actual fram-size of iSAC appears to be larger that expected. All audio \
pushed in but no bit-stream is generated.");
            return -1;
        }
        *bitStreamLenByte = ACM_ISAC_ENCODE(_codecInstPtr->inst,
            &_inAudio[_inAudioIxRead], (WebRtc_Word16*)bitstream);
        // increment the read index this tell the caller that how far
        // we have gone forward in reading the audio buffer
        _inAudioIxRead += _samplesIn10MsAudio;
    }
    if(*bitStreamLenByte == 0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, _uniqueID,
            "ISAC Has encoded the whole frame but no bit-stream is generated.");
    }

    // a packet is generated iSAC, is set in adaptive mode may change
    // the frame length and we like to update the bottleneck value as
    // well, although updating bottleneck is not crucial
    if((*bitStreamLenByte > 0) && (_isacCodingMode == ADAPTIVE))
    {
        //_frameLenSmpl = ACM_ISAC_GETNEWFRAMELEN(_codecInstPtr->inst);
        ACM_ISAC_GETSENDBITRATE(_codecInstPtr->inst, &_isacCurrentBN);
    }
    UpdateFrameLen();
    return *bitStreamLenByte;
}


WebRtc_Word16
ACMISAC::DecodeSafe(
    WebRtc_UWord8* /* bitStream        */,
    WebRtc_Word16  /* bitStreamLenByte */,
    WebRtc_Word16* /* audio            */,
    WebRtc_Word16* /* audioSamples     */,
    WebRtc_Word8*  /* speechType       */)
{
    return 0;
}


WebRtc_Word16
ACMISAC::InternalInitEncoder(
    WebRtcACMCodecParams* codecParams)
{
    // if rate is set to -1 then iSAC has to be in adaptive mode
    if(codecParams->codecInstant.rate == -1)
    {
        _isacCodingMode = ADAPTIVE;
    }

    // sanity check that rate is in acceptable range
    else if((codecParams->codecInstant.rate >= ISAC_MIN_RATE) &&
        (codecParams->codecInstant.rate <= ISAC_MAX_RATE))
    {
        _isacCodingMode = CHANNEL_INDEPENDENT;
        _isacCurrentBN = codecParams->codecInstant.rate;
    }
    else
    {
        return -1;
    }

    // we need to set the encoder sampling frequency.
    if(UpdateEncoderSampFreq((WebRtc_UWord16)codecParams->codecInstant.plfreq) < 0)
    {
        return -1;
    }
    if(ACM_ISAC_ENCODERINIT(_codecInstPtr->inst, _isacCodingMode) < 0)
    {
        return -1;
    }

    // apply the frame-size and rate if operating in
    // channel-independent mode
    if(_isacCodingMode == CHANNEL_INDEPENDENT)
    {
        if(ACM_ISAC_CONTROL(_codecInstPtr->inst,
            codecParams->codecInstant.rate,
            codecParams->codecInstant.pacsize /
            (codecParams->codecInstant.plfreq / 1000)) < 0)
        {
            return -1;
        }
    }
    else
    {
        // We need this for adaptive case and has to be called
        // after initialization
        ACM_ISAC_GETSENDBITRATE(
            _codecInstPtr->inst, &_isacCurrentBN);
    }
    _frameLenSmpl = ACM_ISAC_GETNEWFRAMELEN(_codecInstPtr->inst);
    return 0;
}

WebRtc_Word16
ACMISAC::InternalInitDecoder(
    WebRtcACMCodecParams*  codecParams)
{
    if (_codecInstPtr == NULL)
    {
        return -1;
    }

    // set decoder sampling frequency.
    if(codecParams->codecInstant.plfreq == 32000)
    {
        UpdateDecoderSampFreq(ACMCodecDB::kISACSWB);
    }
    else
    {
        UpdateDecoderSampFreq(ACMCodecDB::kISAC);
    }

    // in a one-way communication we may never register send-codec.
    // However we like that the BWE to work properly so it has to
    // be initialized. The BWE is initialized when iSAC encoder is initialized.
    // Therefore, we need this.
    if(!_encoderInitialized)
    {
        // Since we don't require a valid rate or a valid packet size when initializing
        // the decoder, we set valid values before initializing encoder
        codecParams->codecInstant.rate = kIsacWbDefaultRate;
        codecParams->codecInstant.pacsize = kIsacPacSize960;
        if(InternalInitEncoder(codecParams) < 0)
        {
            return -1;
        }
        _encoderInitialized = true;
    }

    return ACM_ISAC_DECODERINIT(_codecInstPtr->inst);
}


WebRtc_Word16
ACMISAC::InternalCreateDecoder()
{
    if (_codecInstPtr == NULL)
    {
        return -1;
    }
    WebRtc_Word16 status = ACM_ISAC_CREATE (&(_codecInstPtr->inst));

    // specific to codecs with one instance for encoding and decoding
    _encoderInitialized = false;
    if(status < 0)
    {
        _encoderExist = false;
    }
    else
    {
        _encoderExist = true;
    }
    return status;
}


void
ACMISAC::DestructDecoderSafe()
{
    // codec with shared instance cannot delete.
    _decoderInitialized = false;
    return;
}


WebRtc_Word16
ACMISAC::InternalCreateEncoder()
{
    if (_codecInstPtr == NULL)
    {
        return -1;
    }
    WebRtc_Word16 status = ACM_ISAC_CREATE(&(_codecInstPtr->inst));

    // specific to codecs with one instance for encoding and decoding
    _decoderInitialized = false;
    if(status < 0)
    {
        _decoderExist = false;
    }
    else
    {
        _decoderExist = true;
    }
    return status;
}


void
ACMISAC::DestructEncoderSafe()
{
    // codec with shared instance cannot delete.
    _encoderInitialized = false;
    return;
}


WebRtc_Word32
ACMISAC::CodecDef(
    WebRtcNetEQ_CodecDef& codecDef,
    const CodecInst&      codecInst)
{
    // Sanity checks
    if (_codecInstPtr == NULL)
    {
        return -1;
    }
    if (!_decoderInitialized || !_decoderExist)
    {
        // Todo:
        // log error
        return -1;
    }
    // Fill up the structure by calling
    // "SET_CODEC_PAR" & "SET_ISAC_FUNCTION."
    // Then call NetEQ to add the codec to it's
    // database.
    if(codecInst.plfreq == 16000)
    {
        SET_CODEC_PAR((codecDef), kDecoderISAC, codecInst.pltype,
            _codecInstPtr->inst, 16000);
#ifdef WEBRTC_CODEC_ISAC
        SET_ISAC_FUNCTIONS((codecDef));
#else
        SET_ISACfix_FUNCTIONS((codecDef));
#endif
    }
    else
    {
#ifdef WEBRTC_CODEC_ISAC
        SET_CODEC_PAR((codecDef), kDecoderISACswb, codecInst.pltype,
            _codecInstPtr->inst, 32000);
        SET_ISACSWB_FUNCTIONS((codecDef));
#else
        return -1;
#endif
    }

    return 0;
}


void
ACMISAC::InternalDestructEncoderInst(
    void* ptrInst)
{
    if(ptrInst != NULL)
    {
        ACM_ISAC_FREE((ACM_ISAC_STRUCT *)ptrInst);
    }
    return;
}

WebRtc_Word16
ACMISAC::Transcode(
    WebRtc_UWord8* bitStream,
    WebRtc_Word16* bitStreamLenByte,
    WebRtc_Word16  qBWE,
    WebRtc_Word32  rate,
    bool           isRED)
{
    WebRtc_Word16 jitterInfo = 0;
    // transcode from a higher rate to lower rate
    // sanity check
    if (_codecInstPtr == NULL)
    {
        return -1;
    }

    *bitStreamLenByte = ACM_ISAC_GETNEWBITSTREAM(_codecInstPtr->inst,
        qBWE, jitterInfo, rate, (WebRtc_Word16*)bitStream, (isRED)? 1:0);

    if(*bitStreamLenByte < 0)
    {
        // error happened
        *bitStreamLenByte = 0;
        return -1;
    }
    else
    {
        return *bitStreamLenByte;
    }
}

WebRtc_Word16
ACMISAC::SetBitRateSafe(
    WebRtc_Word32 bitRate)
{
    if (_codecInstPtr == NULL)
    {
        return -1;
    }
    WebRtc_UWord16 encoderSampFreq;
    EncoderSampFreq(encoderSampFreq);
    bool reinit = false;
    // change the BN of iSAC
    if(bitRate == -1)
    {
        // ADAPTIVE MODE
        // Check if it was already in adaptive mode
        if(_isacCodingMode != ADAPTIVE)
        {
            // was not in adaptive, then set the mode to adaptive
            // and flag for re-initialization
            _isacCodingMode = ADAPTIVE;
            reinit = true;
        }
    }
    // Sanity check if the rate valid
    else if((bitRate >= ISAC_MIN_RATE) &&
        (bitRate <= ISAC_MAX_RATE))
    {
        //check if it was in channel-independent mode before
        if(_isacCodingMode != CHANNEL_INDEPENDENT)
        {
            // was not in channel independent, set the mode to
            // channel-independent and flag for re-initialization
            _isacCodingMode = CHANNEL_INDEPENDENT;
            reinit = true;
        }
        // store the bottleneck
        _isacCurrentBN = (WebRtc_UWord16)bitRate;
    }
    else
    {
        // invlaid rate
        return -1;
    }

    WebRtc_Word16 status = 0;
    if(reinit)
    {
        // initialize and check if it is successful
        if(ACM_ISAC_ENCODERINIT(_codecInstPtr->inst, _isacCodingMode) < 0)
        {
            // failed initialization
            return -1;
        }
    }
    if(_isacCodingMode == CHANNEL_INDEPENDENT)
    {

        status = ACM_ISAC_CONTROL(_codecInstPtr->inst, _isacCurrentBN,
            (encoderSampFreq == 32000)? 30:(_frameLenSmpl / 16));
        if(status < 0)
        {
            status = -1;
        }
    }

    // Update encoder parameters
    _encoderParams.codecInstant.rate = bitRate;

    UpdateFrameLen();
    return status;
}


WebRtc_Word32
ACMISAC::GetEstimatedBandwidthSafe()
{
    WebRtc_Word16 bandwidthIndex;
    WebRtc_Word16 delayIndex;
    IsacSamplingRate sampRate;

    // Get bandwidth information
    ACM_ISAC_GETSENDBWE(_codecInstPtr->inst, &bandwidthIndex, &delayIndex);

    // Validy check of index
    if ((bandwidthIndex < 0) || (bandwidthIndex >= NR_ISAC_BANDWIDTHS))
    {
        return -1;
    }

    // Check sample frequency
    sampRate = ACM_ISAC_GETDECSAMPRATE(_codecInstPtr->inst);
    if(sampRate == kIsacWideband)
    {
        return isacRatesWB[bandwidthIndex];
    }
    else
    {
        return isacRatesSWB[bandwidthIndex];
    }
}

WebRtc_Word32
ACMISAC::SetEstimatedBandwidthSafe(
    WebRtc_Word32 estimatedBandwidth)
{
    IsacSamplingRate sampRate;
    WebRtc_Word16 bandwidthIndex;

    // Check sample frequency and choose appropriate table
    sampRate = ACM_ISAC_GETENCSAMPRATE(_codecInstPtr->inst);

    if(sampRate == kIsacWideband)
    {
        // Search through the WB rate table to find the index

        bandwidthIndex = NR_ISAC_BANDWIDTHS/2 - 1;
        for (int i=0; i<(NR_ISAC_BANDWIDTHS/2); i++)
        {
            if (estimatedBandwidth == isacRatesWB[i])
            {
                bandwidthIndex = i;
                break;
            } else if (estimatedBandwidth == isacRatesWB[i+NR_ISAC_BANDWIDTHS/2])
            {
                bandwidthIndex = i + NR_ISAC_BANDWIDTHS/2;
                break;
            } else if (estimatedBandwidth < isacRatesWB[i])
            {
                bandwidthIndex = i;
                break;
             }
        }
    }
    else
    {
        // Search through the SWB rate table to find the index
        bandwidthIndex = NR_ISAC_BANDWIDTHS - 1;
        for (int i=0; i<NR_ISAC_BANDWIDTHS; i++)
        {
            if(estimatedBandwidth <= isacRatesSWB[i])
            {
                bandwidthIndex = i;
                break;
            }
        }
    }

    // Set iSAC Bandwidth Estimate
    ACM_ISAC_SETBWE(_codecInstPtr->inst, bandwidthIndex);

    return 0;
}

WebRtc_Word32
ACMISAC::GetRedPayloadSafe(
#if (!defined(WEBRTC_CODEC_ISAC))
    WebRtc_UWord8* /* redPayload   */,
    WebRtc_Word16* /* payloadBytes */)
{
    return -1;
#else
    WebRtc_UWord8* redPayload,
    WebRtc_Word16* payloadBytes)
{

    WebRtc_Word16 bytes = WebRtcIsac_GetRedPayload(_codecInstPtr->inst, (WebRtc_Word16*)redPayload);
    if (bytes < 0)
    {
        return -1;
    }
    *payloadBytes = bytes;
    return 0;
#endif
}


WebRtc_Word16
ACMISAC::UnregisterFromNetEqSafe(
    ACMNetEQ*     netEq,
    WebRtc_Word16 payloadType)
{
    if(payloadType == _decoderParams.codecInstant.pltype)
    {
        return netEq->RemoveCodec(kDecoderISAC);
    }
    else if(payloadType == _decoderParams32kHz.codecInstant.pltype)
    {
        return netEq->RemoveCodec(kDecoderISACswb);
    }
    else
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "Cannot unregister codec %s given payload-type %d does not match \
the stored payload type %d or %d",
            _decoderParams.codecInstant.plname,
            payloadType,
            _decoderParams.codecInstant.pltype,
            _decoderParams32kHz.codecInstant.pltype);

        return -1;
    }
}


WebRtc_Word16
ACMISAC::UpdateDecoderSampFreq(
#ifdef WEBRTC_CODEC_ISAC
    WebRtc_Word16 codecId)
{
    if(ACMCodecDB::kISAC == codecId)
    {
        return WebRtcIsac_SetDecSampRate(_codecInstPtr->inst, kIsacWideband);
    }
    else if(ACMCodecDB::kISACSWB == codecId)
    {
        return WebRtcIsac_SetDecSampRate(_codecInstPtr->inst, kIsacSuperWideband);
    }
    else
    {
        return -1;
    }

#else
    WebRtc_Word16 /* codecId */)
{
    return 0;
#endif
}


WebRtc_Word16
ACMISAC::UpdateEncoderSampFreq(
#ifdef WEBRTC_CODEC_ISAC
    WebRtc_UWord16 encoderSampFreqHz)
{
    WebRtc_UWord16 currentSampRateHz;
    EncoderSampFreq(currentSampRateHz);

    if(currentSampRateHz != encoderSampFreqHz)
    {
        if((encoderSampFreqHz != 16000) && (encoderSampFreqHz != 32000))
        {
            return -1;
        }
        else
        {
            _inAudioIxRead = 0;
            _inAudioIxWrite = 0;
            _inTimestampIxWrite = 0;
            if(encoderSampFreqHz == 16000)
            {
                if(WebRtcIsac_SetEncSampRate(_codecInstPtr->inst, kIsacWideband) < 0)
                {
                    return -1;
                }
                _samplesIn10MsAudio = 160;
            }
            else
            {

                if(WebRtcIsac_SetEncSampRate(_codecInstPtr->inst, kIsacSuperWideband) < 0)
                {
                    return -1;
                }
                _samplesIn10MsAudio = 320;
            }
            _frameLenSmpl = ACM_ISAC_GETNEWFRAMELEN(_codecInstPtr->inst);
            _encoderParams.codecInstant.pacsize = _frameLenSmpl;
            _encoderParams.codecInstant.plfreq = encoderSampFreqHz;
            return 0;
        }
    }
#else
    WebRtc_UWord16 /* codecId */)
{
#endif
    return 0;
}

WebRtc_Word16
ACMISAC::EncoderSampFreq(
    WebRtc_UWord16& sampFreqHz)
{
    IsacSamplingRate sampRate;
    sampRate = ACM_ISAC_GETENCSAMPRATE(_codecInstPtr->inst);
    if(sampRate == kIsacSuperWideband)
    {
        sampFreqHz = 32000;
    }
    else
    {
        sampFreqHz = 16000;
    }
    return 0;
}

WebRtc_Word32
ACMISAC::ConfigISACBandwidthEstimator(
    const WebRtc_UWord8  initFrameSizeMsec,
    const WebRtc_UWord16 initRateBitPerSec,
    const bool           enforceFrameSize)
{
    WebRtc_Word16 status;
    {
        WebRtc_UWord16 sampFreqHz;
        EncoderSampFreq(sampFreqHz);
        // @TODO: at 32kHz we hardcode calling with 30ms and enforce
        // the frame-size otherwise we might get error. Revise if
        // control-bwe is changed.
        if(sampFreqHz == 32000)
        {
            status = ACM_ISAC_CONTROL_BWE(_codecInstPtr->inst,
                initRateBitPerSec, 30, 1);
        }
        else
        {
            status = ACM_ISAC_CONTROL_BWE(_codecInstPtr->inst,
                initRateBitPerSec, initFrameSizeMsec, enforceFrameSize? 1:0);
        }
    }
    if(status < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
            "Coutn't config iSAC BWE.");
        return -1;
    }
    UpdateFrameLen();
    ACM_ISAC_GETSENDBITRATE(_codecInstPtr->inst, &_isacCurrentBN);
    return 0;
}

WebRtc_Word32
ACMISAC::SetISACMaxPayloadSize(
    const WebRtc_UWord16 maxPayloadLenBytes)
{
    return ACM_ISAC_SETMAXPAYLOADSIZE(_codecInstPtr->inst, maxPayloadLenBytes);
}

WebRtc_Word32
ACMISAC::SetISACMaxRate(
    const WebRtc_UWord32 maxRateBitPerSec)
{
    return ACM_ISAC_SETMAXRATE(_codecInstPtr->inst, maxRateBitPerSec);
}


void
ACMISAC::UpdateFrameLen()
{
    _frameLenSmpl = ACM_ISAC_GETNEWFRAMELEN(_codecInstPtr->inst);
    _encoderParams.codecInstant.pacsize = _frameLenSmpl;
}

void
ACMISAC::CurrentRate(WebRtc_Word32& rateBitPerSec)
{
    if(_isacCodingMode == ADAPTIVE)
    {
        ACM_ISAC_GETSENDBITRATE(_codecInstPtr->inst, &rateBitPerSec);
    }
}


bool
ACMISAC::DecoderParamsSafe(
    WebRtcACMCodecParams* decParams,
    const WebRtc_UWord8   payloadType)
{
    if(_decoderInitialized)
    {
        if(payloadType == _decoderParams.codecInstant.pltype)
        {
            memcpy(decParams, &_decoderParams, sizeof(WebRtcACMCodecParams));
            return true;
        }
        if(payloadType == _decoderParams32kHz.codecInstant.pltype)
        {
            memcpy(decParams, &_decoderParams32kHz,
                sizeof(WebRtcACMCodecParams));
            return true;
        }
    }
    return false;
}

void
ACMISAC::SaveDecoderParamSafe(
    const WebRtcACMCodecParams* codecParams)
{
    // set decoder sampling frequency.
    if(codecParams->codecInstant.plfreq == 32000)
    {
        memcpy(&_decoderParams32kHz, codecParams, sizeof(WebRtcACMCodecParams));
    }
    else
    {
        memcpy(&_decoderParams, codecParams, sizeof(WebRtcACMCodecParams));
    }
}


WebRtc_Word16
ACMISAC::REDPayloadISAC(
    const WebRtc_Word32  isacRate,
    const WebRtc_Word16  isacBwEstimate,
    WebRtc_UWord8*       payload,
    WebRtc_Word16*       payloadLenBytes)
{
    WebRtc_Word16 status;
    ReadLockScoped rl(_codecWrapperLock);
    status = Transcode(payload, payloadLenBytes, isacBwEstimate, isacRate, true);
    return status;
}

#endif

} // namespace webrtc
