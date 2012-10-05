/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_GENERIC_CODEC_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_GENERIC_CODEC_H_

#include "acm_common_defs.h"
#include "audio_coding_module_typedefs.h"
#include "rw_lock_wrapper.h"
#include "trace.h"
#include "webrtc_neteq.h"

#define MAX_FRAME_SIZE_10MSEC 6

// forward declaration
struct WebRtcVadInst;
struct WebRtcCngEncInst;

namespace webrtc
{

// forward declaration
struct CodecInst;
class  ACMNetEQ;

class ACMGenericCodec
{
public:
    ///////////////////////////////////////////////////////////////////////////
    // Constructor of the class
    //
    ACMGenericCodec();


    ///////////////////////////////////////////////////////////////////////////
    // Destructor of the class.
    //
    virtual ~ACMGenericCodec();


    ///////////////////////////////////////////////////////////////////////////
    // ACMGenericCodec* CreateInstance();
    // The function will be used for FEC. It is not implemented yet.
    //
    virtual ACMGenericCodec* CreateInstance() = 0;


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 Encode()
    // The function is called to perform an encoding of the audio stored in
    // audio buffer. An encoding is performed only if enough audio, i.e. equal
    // to the frame-size of the codec, exist. The audio frame will be processed
    // by VAD and CN/DTX if required. There are few different cases.
    //
    // A) Neither VAD nor DTX is active; the frame is encoded by the encoder.
    //
    // B) VAD is enabled but not DTX; in this case the audio is processed by VAD
    //    and encoded by the encoder. The "*encodingType" will be either
    //    "activeNormalEncode" or "passiveNormalEncode" if frame is active or
    //    passive, respectively.
    //
    // C) DTX is enabled; if the codec has internal VAD/DTX we just encode the
    //    frame by the encoder. Otherwise, the frame is passed through VAD and
    //    if identified as passive, then it will be processed by CN/DTX. If the
    //    frame is active it will be encoded by the encoder.
    //
    // This function acquires the appropriate locks and calls EncodeSafe() for
    // the actual processing.
    //
    // Outputs:
    //   -bitStream          : a buffer where bit-stream will be written to.
    //   -bitStreamLenByte   : contains the length of the bit-stream in
    //                         bytes.
    //   -timeStamp          : contains the RTP timestamp, this is the
    //                         sampling time of the first sample encoded
    //                         (measured in number of samples).
    //   -encodingType       : contains the type of encoding applied on the
    //                         audio samples. The alternatives are
    //                         (c.f. acm_common_types.h)
    //                         -kNoEncoding:
    //                            there was not enough data to encode. or
    //                            some error has happened that we could
    //                            not do encoding.
    //                         -kActiveNormalEncoded:
    //                            the audio frame is active and encoded by
    //                            the given codec.
    //                         -kPassiveNormalEncoded:
    //                            the audio frame is passive but coded with
    //                            the given codec (NO DTX).
    //                         -kPassiveDTXWB:
    //                            The audio frame is passive and used
    //                            wide-band CN to encode.
    //                         -kPassiveDTXNB:
    //                            The audio frame is passive and used
    //                            narrow-band CN to encode.
    //
    // Return value:
    //   -1 if error is occurred, otherwise the length of the bit-stream in
    //      bytes.
    //
    WebRtc_Word16 Encode(
        WebRtc_UWord8*         bitStream,
        WebRtc_Word16*         bitStreamLenByte,
        WebRtc_UWord32*        timeStamp,
        WebRtcACMEncodingType* encodingType);


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 Decode()
    // This function is used to decode a given bit-stream, without engaging
    // NetEQ.
    //
    // This function acquires the appropriate locks and calls DecodeSafe() for
    // the actual processing. Please note that this is not functional yet.
    //
    // Inputs:
    //   -bitStream          : a buffer where bit-stream will be read.
    //   -bitStreamLenByte   : the length of the bit-stream in bytes.
    //
    // Outputs:
    //   -audio              : pointer to a buffer where the audio will written.
    //   -audioSamples       : number of audio samples out of decoding the given
    //                         bit-stream.
    //   -speechType         : speech type (for future use).
    //
    // Return value:
    //   -1 if failed to decode,
    //    0 if succeeded.
    //
    WebRtc_Word16 Decode(
        WebRtc_UWord8* bitStream,
        WebRtc_Word16  bitStreamLenByte,
        WebRtc_Word16* audio,
        WebRtc_Word16* audioSamples,
        WebRtc_Word8*  speechType);

    ///////////////////////////////////////////////////////////////////////////
    // void SplitStereoPacket()
    // This function is used to split stereo payloads in left and right channel.
    // Codecs which has stereo support has there own implementation of the
    // function.
    //
    // Input/Output:
    //   -payload             : a vector with the received payload data.
    //                          The function will reorder the data so that
    //                          first half holds the left channel data, and the
    //                          second half the right channel data.
    //   -payload_length      : length of payload in bytes. Will be changed to
    //                          twice the input in case of true stereo, where
    //                          we simply copy the data and return it both for
    //                          left channel and right channel decoding.
    virtual void SplitStereoPacket(WebRtc_UWord8* /* payload */,
                                   WebRtc_Word32* /* payload_length */) {}

    ///////////////////////////////////////////////////////////////////////////
    // bool EncoderInitialized();
    //
    // Return value:
    //   True if the encoder is successfully initialized,
    //   false otherwise.
    //
    bool EncoderInitialized();


    ///////////////////////////////////////////////////////////////////////////
    // bool DecoderInitialized();
    //
    // Return value:
    //   True if the decoder is successfully initialized,
    //   false otherwise.
    //
    bool DecoderInitialized();


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 EncoderParams()
    // It is called to get encoder parameters. It will call
    // EncoderParamsSafe() in turn.
    //
    // Output:
    //   -encParams          : a buffer where the encoder parameters is
    //                         written to. If the encoder is not
    //                         initialized this buffer is filled with
    //                         invalid values
    // Return value:
    //   -1 if the encoder is not initialized,
    //    0 otherwise.
    //
    //
    WebRtc_Word16 EncoderParams(
        WebRtcACMCodecParams *encParams);


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 DecoderParams(...)
    // It is called to get decoder parameters. It will call DecoderParamsSafe()
    // in turn.
    //
    // Output:
    //   -decParams          : a buffer where the decoder parameters is
    //                         written to. If the decoder is not initialized
    //                         this buffer is filled with invalid values
    //
    // Return value:
    //   -1 if the decoder is not initialized,
    //    0 otherwise.
    //
    //
    bool DecoderParams(
        WebRtcACMCodecParams *decParams,
        const WebRtc_UWord8  payloadType);


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 InitEncoder(...)
    // This function is called to initialize the encoder with the given
    // parameters.
    //
    // Input:
    //   -codecParams        : parameters of encoder.
    //   -forceInitialization: if false the initialization is invoked only if
    //                         the encoder is not initialized. If true the
    //                         encoder is forced to (re)initialize.
    //
    // Return value:
    //   0 if could initialize successfully,
    //  -1 if failed to initialize.
    //
    //
    WebRtc_Word16 InitEncoder(
        WebRtcACMCodecParams* codecParams,
        bool                  forceInitialization);


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 InitDecoder()
    // This function is called to initialize the decoder with the given
    // parameters. (c.f. acm_common_defs.h & common_types.h for the
    // definition of the structure)
    //
    // Input:
    //   -codecParams        : parameters of decoder.
    //   -forceInitialization: if false the initialization is invoked only
    //                         if the decoder is not initialized. If true
    //                         the encoder is forced to(re)initialize.
    //
    // Return value:
    //   0 if could initialize successfully,
    //  -1 if failed to initialize.
    //
    //
    WebRtc_Word16 InitDecoder(
        WebRtcACMCodecParams* codecParams,
        bool                 forceInitialization);


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word32 RegisterInNetEq(...)
    // This function is called to register the decoder in NetEq, with the given
    // payload-type.
    //
    // Inputs:
    //   -netEq              : pointer to NetEq Instance
    //   -codecInst          : instance with of the codec settings of the codec
    //
    // Return values
    //   -1 if failed to register,
    //    0 if successfully initialized.
    //
    WebRtc_Word32 RegisterInNetEq(
        ACMNetEQ*             netEq,
        const CodecInst& codecInst);


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word32 Add10MsData(...)
    // This function is called to add 10 ms of audio to the audio buffer of
    // the codec.
    //
    // Inputs:
    //   -timeStamp          : the timestamp of the 10 ms audio. the timestamp
    //                         is the sampling time of the
    //                         first sample measured in number of samples.
    //   -data               : a buffer that contains the audio. The codec
    //                         expects to get the audio in correct sampling
    //                         frequency
    //   -length             : the length of the audio buffer
    //   -audioChannel       : 0 for mono, 1 for stereo (not supported yet)
    //
    // Return values:
    //   -1 if failed
    //    0 otherwise.
    //
    WebRtc_Word32 Add10MsData(
        const WebRtc_UWord32 timeStamp,
        const WebRtc_Word16* data,
        const WebRtc_UWord16 length,
        const WebRtc_UWord8  audioChannel);


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_UWord32 NoMissedSamples()
    // This function returns the number of samples which are overwritten in
    // the audio buffer. The audio samples are overwritten if the input audio
    // buffer is full, but Add10MsData() is called. (We might remove this
    // function if it is not used)
    //
    // Return Value:
    //   Number of samples which are overwritten.
    //
    WebRtc_UWord32 NoMissedSamples() const;


    ///////////////////////////////////////////////////////////////////////////
    // void ResetNoMissedSamples()
    // This function resets the number of overwritten samples to zero.
    // (We might remove this function if we remove NoMissedSamples())
    //
    void ResetNoMissedSamples();


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 SetBitRate()
    // The function is called to set the encoding rate.
    //
    // Input:
    //   -bitRateBPS         : encoding rate in bits per second
    //
    // Return value:
    //   -1 if failed to set the rate, due to invalid input or given
    //      codec is not rate-adjustable.
    //    0 if the rate is adjusted successfully
    //
    WebRtc_Word16 SetBitRate(const WebRtc_Word32 bitRateBPS);


    ///////////////////////////////////////////////////////////////////////////
    // DestructEncoderInst()
    // This API is used in conferencing. It will free the memory that is pointed
    // by "ptrInst". "ptrInst" is a pointer to encoder instance, created and
    // filled up by calling EncoderInst(...).
    //
    // Inputs:
    //   -ptrInst            : pointer to an encoder instance to be deleted.
    //
    //
    void DestructEncoderInst(
        void* ptrInst);

    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 AudioBuffer()
    // This is used when synchronization of codecs is required. There are cases
    // that the audio buffers of two codecs have to be synched. By calling this
    // function on can get the audio buffer and other related parameters, such
    // as timestamps...
    //
    // Output:
    //   -audioBuff          : a pointer to WebRtcACMAudioBuff where the audio
    //                         buffer of this codec will be written to.
    //
    // Return value:
    //   -1 if fails to copy the audio buffer,
    //    0 if succeeded.
    //
    WebRtc_Word16 AudioBuffer(
        WebRtcACMAudioBuff& audioBuff);


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_UWord32 EarliestTimestamp()
    // Returns the timestamp of the first 10 ms in audio buffer. This is used
    // to identify if a synchronization of two encoders is required.
    //
    // Return value:
    //   timestamp of the first 10 ms audio in the audio buffer.
    //
    WebRtc_UWord32 EarliestTimestamp() const;


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 SetAudioBuffer()
    // This function is called to set the audio buffer and the associated
    // parameters to a given value.
    //
    // Return value:
    //   -1 if fails to copy the audio buffer,
    //    0 if succeeded.
    //
    WebRtc_Word16 SetAudioBuffer(WebRtcACMAudioBuff& audioBuff);



    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 SetVAD()
    // This is called to set VAD & DTX. If the codec has internal DTX that will
    // be used. If DTX is enabled and the codec does not have internal DTX,
    // WebRtc-VAD will be used to decide if the frame is active. If DTX is
    // disabled but VAD is enabled. The audio is passed through VAD to label it
    // as active or passive, but the frame is  encoded normally. However the
    // bit-stream is labeled properly so that ACM::Process() can use this
    // information. In case of failure, the previous states of the VAD & DTX
    // are kept.
    //
    // Inputs:
    //   -enableDTX          : if true DTX will be enabled otherwise the DTX is
    //                         disabled. If codec has internal DTX that will be
    //                         used, otherwise WebRtc-CNG is used. In the latter
    //                         case VAD is automatically activated.
    //   -enableVAD          : if true WebRtc-VAD is enabled, otherwise VAD is
    //                         disabled, except for the case that DTX is enabled
    //                         but codec doesn't have internal DTX. In this case
    //                         VAD is enabled regardless of the value of
    //                         "enableVAD."
    //   -mode               : this specifies the aggressiveness of VAD.
    //
    // Return value
    //   -1 if failed to set DTX & VAD as specified,
    //    0 if succeeded.
    //
    WebRtc_Word16 SetVAD(
        const bool             enableDTX = true,
        const bool             enableVAD = false,
        const ACMVADMode mode      = VADNormal);


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word32 ReplaceInternalDTX()
    // This is called to replace the codec internal DTX with WebRtc DTX.
    // This is only valid for G729 where the user has possibility to replace
    // AnnexB with WebRtc DTX. For other codecs this function has no effect.
    //
    // Input:
    //   -replaceInternalDTX : if true the internal DTX is replaced with WebRtc.
    //
    // Return value
    //   -1 if failed to replace internal DTX,
    //    0 if succeeded.
    //
    WebRtc_Word32 ReplaceInternalDTX(const bool replaceInternalDTX);

    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word32 IsInternalDTXReplaced()
    // This is called to check if the codec internal DTX is replaced by WebRtc DTX.
    // This is only valid for G729 where the user has possibility to replace
    // AnnexB with WebRtc DTX. For other codecs this function has no effect.
    //
    // Output:
    //   -internalDTXReplaced    : if true the internal DTX is replaced with WebRtc.
    //
    // Return value
    //   -1 if failed to check if replace internal DTX or replacement not feasible,
    //    0 if succeeded.
    //
    WebRtc_Word32 IsInternalDTXReplaced(bool* internalDTXReplaced);

    ///////////////////////////////////////////////////////////////////////////
    // void SetNetEqDecodeLock()
    // Passes the NetEq lock to the codec.
    //
    // Input:
    //   -netEqDecodeLock    : pointer to the lock associated with NetEQ of ACM.
    //
    void SetNetEqDecodeLock(
        RWLockWrapper* netEqDecodeLock)
    {
        _netEqDecodeLock = netEqDecodeLock;
    }


    ///////////////////////////////////////////////////////////////////////////
    // bool HasInternalDTX()
    // Used to check if the codec has internal DTX.
    //
    // Return value:
    //   true if the codec has an internal DTX, e.g. G729,
    //   false otherwise.
    //
    bool HasInternalDTX() const
    {
        return _hasInternalDTX;
    }


   ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word32 GetEstimatedBandwidth()
    // Used to get decoder estimated bandwidth. Only iSAC will provide a value.
    //
    //
    // Return value:
    //   -1 if fails to get decoder estimated bandwidth,
    //    >0 estimated bandwidth in bits/sec.
    //
    WebRtc_Word32 GetEstimatedBandwidth();


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word32 SetEstimatedBandwidth()
    // Used to set estiamted bandwidth sent out of band from other side. Only
    // iSAC will have use for the value.
    //
    // Input:
    //       -estimatedBandwidth:    estimated bandwidth in bits/sec
    //
    // Return value:
    //   -1 if fails to set estimated bandwidth,
    //    0 on success.
    //
    WebRtc_Word32 SetEstimatedBandwidth(WebRtc_Word32 estimatedBandwidth);

    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word32 GetRedPayload()
    // Used to get codec specific RED payload (if such is implemented).
    // Currently only done in iSAC.
    //
    // Outputs:
    //   -redPayload        : a pointer to the data for RED payload.
    //   -payloadBytes      : number of bytes in RED payload.
    //
    // Return value:
    //   -1 if fails to get codec specific RED,
    //    0 if succeeded.
    //
    WebRtc_Word32 GetRedPayload(
        WebRtc_UWord8* redPayload,
        WebRtc_Word16* payloadBytes);


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 ResetEncoder()
    // By calling this function you would re-initialize the encoder with the
    // current parameters. All the settings, e.g. VAD/DTX, frame-size... should
    // remain unchanged. (In case of iSAC we don't want to lose BWE history.)
    //
    // Return value
    //   -1 if failed,
    //    0 if succeeded.
    //
    WebRtc_Word16 ResetEncoder();


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 ResetEncoder()
    // By calling this function you would re-initialize the decoder with the
    // current parameters.
    //
    // Return value
    //   -1 if failed,
    //    0 if succeeded.
    //
    WebRtc_Word16 ResetDecoder(
        WebRtc_Word16 payloadType);


    ///////////////////////////////////////////////////////////////////////////
    // void DestructEncoder()
    // This function is called to delete the encoder instance, if possible, to
    // have a fresh start. For codecs where encoder and decoder share the same
    // instance we cannot delete the encoder and instead we will initialize the
    // encoder. We also delete VAD and DTX if they have been created.
    //
    void DestructEncoder();


    ///////////////////////////////////////////////////////////////////////////
    // void DestructDecoder()
    // This function is called to delete the decoder instance, if possible, to
    // have a fresh start. For codecs where encoder and decoder share the same
    // instance we cannot delete the encoder and instead we will initialize the
    // decoder. Before deleting decoder instance it has to be removed from the
    // NetEq list.
    //
    void DestructDecoder();


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 SamplesLeftToEncode()
    // Returns the number of samples required to be able to do encoding.
    //
    // Return value:
    //   Number of samples.
    //
    WebRtc_Word16 SamplesLeftToEncode();


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_UWord32 LastEncodedTimestamp()
    // Returns the timestamp of the last frame it encoded.
    //
    // Return value:
    //   Timestamp.
    //
    WebRtc_UWord32 LastEncodedTimestamp() const;


    ///////////////////////////////////////////////////////////////////////////
    // SetUniqueID()
    // Set a unique ID for the codec to be used for tracing and debuging
    //
    // Input
    //   -id                 : A number to identify the codec.
    //
    void SetUniqueID(
        const WebRtc_UWord32 id);


    ///////////////////////////////////////////////////////////////////////////
    // IsAudioBufferFresh()
    // Specifies if ever audio is injected to this codec.
    //
    // Return value
    //   -true; no audio is feed into this codec
    //   -false; audio has already been  fed to the codec.
    //
    bool IsAudioBufferFresh() const;


    ///////////////////////////////////////////////////////////////////////////
    // UpdateDecoderSampFreq()
    // For most of the codecs this function does nothing. It must be
    // implemented for those codecs that one codec instance serves as the
    // decoder for different flavers of the codec. One example is iSAC. there,
    // iSAC 16 kHz and iSAC 32 kHz are treated as two different codecs with
    // different payload types, however, there is only one iSAC instance to
    // decode. The reason for that is we would like to decode and encode with
    // the same codec instance for bandwidth estimator to work.
    //
    // Each time that we receive a new payload type, we call this funtion to
    // prepare the decoder associated with the new payload. Normally, decoders
    // doesn't have to do anything. For iSAC the decoder has to change it's
    // sampling rate. The input parameter specifies the current flaver of the
    // codec in codec database. For instance, if we just got a SWB payload then
    // the input parameter is ACMCodecDB::isacswb.
    //
    // Input:
    //   -codecId            : the ID of the codec associated with the
    //                         payload type that we just received.
    //
    // Return value:
    //    0 if succeeded in updating the decoder.
    //   -1 if failed to update.
    //
    virtual WebRtc_Word16 UpdateDecoderSampFreq(
        WebRtc_Word16 /* codecId */)
    {
        return 0;
    }


    ///////////////////////////////////////////////////////////////////////////
    // UpdateEncoderSampFreq()
    // Call this function to update the encoder sampling frequency. This
    // is for codecs where one payload-name supports several encoder sampling
    // frequencies. Otherwise, to change the sampling frequency we need to
    // register new codec. ACM will consider that as registration of a new
    // codec, not a change in parameter. For iSAC, switching from WB to SWB
    // is treated as a change in parameter. Therefore, we need this function.
    //
    // Input:
    //   -encoderSampFreqHz  : encoder sampling frequency.
    //
    // Return value:
    //   -1 if failed, or if this is meaningless for the given codec.
    //    0 if succeeded.
    //
    virtual WebRtc_Word16 UpdateEncoderSampFreq(
        WebRtc_UWord16 encoderSampFreqHz);


    ///////////////////////////////////////////////////////////////////////////
    // EncoderSampFreq()
    // Get the sampling frequency that the encoder (WebRtc wrapper) expects.
    //
    // Output:
    //   -sampFreqHz         : sampling frequency, in Hertz, which the encoder
    //                         should be fed with.
    //
    // Return value:
    //   -1 if failed to output sampling rate.
    //    0 if the sample rate is returned successfully.
    //
    virtual WebRtc_Word16 EncoderSampFreq(
        WebRtc_UWord16& sampFreqHz);


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word32 ConfigISACBandwidthEstimator()
    // Call this function to configure the bandwidth estimator of ISAC.
    // During the adaptation of bit-rate, iSAC atomatically adjusts the
    // frame-size (either 30 or 60 ms) to save on RTP header. The initial
    // frame-size can be specified by the first argument. The configuration also
    // regards the initial estimate of bandwidths. The estimator starts from
    // this point and converges to the actual bottleneck. This is given by the
    // second parameter. Furthermore, it is also possible to control the
    // adaptation of frame-size. This is specified by the last parameter.
    //
    // Input:
    //   -initFrameSizeMsec  : initial frame-size in milisecods. For iSAC-wb
    //                         30 ms and 60 ms (default) are acceptable values,
    //                         and for iSAC-swb 30 ms is the only acceptable
    //                         value. Zero indiates default value.
    //   -initRateBitPerSec  : initial estimate of the bandwidth. Values
    //                         between 10000 and 58000 are acceptable.
    //   -enforceFrameSize   : if true, the frame-size will not be adapted.
    //
    // Return value:
    //   -1 if failed to configure the bandwidth estimator,
    //    0 if the configuration was successfully applied.
    //
    virtual WebRtc_Word32 ConfigISACBandwidthEstimator(
        const WebRtc_UWord8  initFrameSizeMsec,
        const WebRtc_UWord16 initRateBitPerSec,
        const bool           enforceFrameSize);


    ///////////////////////////////////////////////////////////////////////////
    // SetISACMaxPayloadSize()
    // Set the maximum payload size of iSAC packets. No iSAC payload,
    // regardless of its frame-size, may exceed the given limit. For
    // an iSAC payload of size B bits and frame-size T sec we have;
    // (B < maxPayloadLenBytes * 8) and (B/T < maxRateBitPerSec), c.f.
    // SetISACMaxRate().
    //
    // Input:
    //   -maxPayloadLenBytes : maximum payload size in bytes.
    //
    // Return value:
    //   -1 if failed to set the maximm  payload-size.
    //    0 if the given linit is seet successfully.
    //
    virtual WebRtc_Word32 SetISACMaxPayloadSize(
        const WebRtc_UWord16 maxPayloadLenBytes);


    ///////////////////////////////////////////////////////////////////////////
    // SetISACMaxRate()
    // Set the maximum instantaneous rate of iSAC. For a payload of B bits
    // with a frame-size of T sec the instantaneous rate is B/T bist per
    // second. Therefore, (B/T < maxRateBitPerSec) and
    // (B < maxPayloadLenBytes * 8) are always satisfied for iSAC payloads,
    // c.f SetISACMaxPayloadSize().
    //
    // Input:
    //   -maxRateBitPerSec   : maximum instantaneous bit-rate given in bits/sec.
    //
    // Return value:
    //   -1 if failed to set the maximum rate.
    //    0 if the maximum rate is set successfully.
    //
    virtual WebRtc_Word32 SetISACMaxRate(
        const WebRtc_UWord32 maxRateBitPerSec);


    ///////////////////////////////////////////////////////////////////////////
    // SaveDecoderParamS()
    // Save the parameters of decoder.
    //
    // Input:
    //   -codecParams        : pointer to a struct where the parameters of
    //                         decoder is stored in.
    //
    void SaveDecoderParam(
        const WebRtcACMCodecParams* codecParams);


    WebRtc_Word32 FrameSize()
    {
        return _frameLenSmpl;
    }

    void SetIsMaster(bool isMaster);




    ///////////////////////////////////////////////////////////////////////////
    // REDPayloadISAC()
    // This is an iSAC-specific function. The function is called to get RED
    // paylaod from a default-encoder.
    //
    // Inputs:
    //   -isacRate           : the target rate of the main payload. A RED
    //                         paylaod is generated according to the rate of
    //                         main paylaod. Note that we are not specifying the
    //                         rate of RED payload, but the main payload.
    //   -isacBwEstimate     : bandwidth information should be inserted in
    //                         RED payload.
    //
    // Output:
    //   -payload            : pointer to a buffer where the RED paylaod will
    //                         written to.
    //   -paylaodLenBytes    : a place-holder to write the length of the RED
    //                         payload in Bytes.
    //
    // Return value:
    //   -1 if an error occures, otherwise the length of the payload (in Bytes)
    //   is returned.
    //
    //
    virtual WebRtc_Word16 REDPayloadISAC(
        const WebRtc_Word32 isacRate,
        const WebRtc_Word16 isacBwEstimate,
        WebRtc_UWord8*      payload,
        WebRtc_Word16*      payloadLenBytes);

    ///////////////////////////////////////////////////////////////////////////
    // IsTrueStereoCodec()
    // Call to see if current encoder is a true stereo codec. This function
    // should be overwritten for codecs which are true stereo codecs
    // Return value:
    //   -true  if stereo codec
    //   -false if not stereo codec.
    //
    virtual bool IsTrueStereoCodec() {
      return false;
    }

protected:
    ///////////////////////////////////////////////////////////////////////////
    // All the functions with FunctionNameSafe(...) contain the actual
    // implementation of FunctionName(...). FunctionName() acquires an
    // appropriate lock and calls FunctionNameSafe() to do the actual work.
    // Therefore, for the description of functionality, input/output arguments
    // and return value we refer to FunctionName()
    //

    ///////////////////////////////////////////////////////////////////////////
    // See Encode() for the description of function, input(s)/output(s) and
    // return value.
    //
    WebRtc_Word16 EncodeSafe(
        WebRtc_UWord8*         bitStream,
        WebRtc_Word16*         bitStreamLenByte,
        WebRtc_UWord32*        timeStamp,
        WebRtcACMEncodingType* encodingType);

    ///////////////////////////////////////////////////////////////////////////
    // See Decode() for the description of function, input(s)/output(s) and
    // return value.
    //
    virtual WebRtc_Word16 DecodeSafe(
        WebRtc_UWord8* bitStream,
        WebRtc_Word16  bitStreamLenByte,
        WebRtc_Word16* audio,
        WebRtc_Word16* audioSamples,
        WebRtc_Word8*  speechType) = 0;

    ///////////////////////////////////////////////////////////////////////////
    // See Add10MsSafe() for the description of function, input(s)/output(s)
    // and return value.
    //
    virtual WebRtc_Word32 Add10MsDataSafe(
        const WebRtc_UWord32 timeStamp,
        const WebRtc_Word16* data,
        const WebRtc_UWord16 length,
        const WebRtc_UWord8  audioChannel);

    ///////////////////////////////////////////////////////////////////////////
    // See RegisterInNetEq() for the description of function,
    // input(s)/output(s) and  return value.
    //
    virtual WebRtc_Word32 CodecDef(
        WebRtcNetEQ_CodecDef& codecDef,
        const CodecInst&  codecInst) = 0;

    ///////////////////////////////////////////////////////////////////////////
    // See EncoderParam() for the description of function, input(s)/output(s)
    // and return value.
    //
    WebRtc_Word16 EncoderParamsSafe(
        WebRtcACMCodecParams *encParams);

    ///////////////////////////////////////////////////////////////////////////
    // See DecoderParam for the description of function, input(s)/output(s)
    // and return value.
    //
    // Note:
    // Any Class where a single instance handle several flavers of the
    // same codec, therefore, several payload types are associated with
    // the same instance have to implement this function.
    //
    // Currently only iSAC is implementing it. A single iSAC instance is
    // used for decoding both WB & SWB stream. At one moment both WB & SWB
    // can be registered as receive codec. Hence two payloads are associated
    // with a single codec instance.
    //
    virtual bool  DecoderParamsSafe(
        WebRtcACMCodecParams *decParams,
        const WebRtc_UWord8  payloadType);

    ///////////////////////////////////////////////////////////////////////////
    // See ResetEncoder() for the description of function, input(s)/output(s)
    // and return value.
    //
    WebRtc_Word16 ResetEncoderSafe();

    ///////////////////////////////////////////////////////////////////////////
    // See InitEncoder() for the description of function, input(s)/output(s)
    // and return value.
    //
    WebRtc_Word16 InitEncoderSafe(
        WebRtcACMCodecParams *codecParams,
        bool                 forceInitialization);

    ///////////////////////////////////////////////////////////////////////////
    // See InitDecoder() for the description of function, input(s)/output(s)
    // and return value.
    //
    WebRtc_Word16 InitDecoderSafe(
        WebRtcACMCodecParams *codecParams,
        bool                 forceInitialization);

    ///////////////////////////////////////////////////////////////////////////
    // See ResetDecoder() for the description of function, input(s)/output(s)
    // and return value.
    //
    WebRtc_Word16 ResetDecoderSafe(
        WebRtc_Word16 payloadType);

    ///////////////////////////////////////////////////////////////////////////
    // See DestructEncoder() for the description of function,
    // input(s)/output(s) and return value.
    //
    virtual void DestructEncoderSafe() = 0;

    ///////////////////////////////////////////////////////////////////////////
    // See DestructDecoder() for the description of function,
    // input(s)/output(s) and return value.
    //
    virtual void DestructDecoderSafe() = 0;

    ///////////////////////////////////////////////////////////////////////////
    // See SetBitRate() for the description of function, input(s)/output(s)
    // and return value.
    //
    // Any codec that can change the bit-rate has to implement this.
    //
    virtual WebRtc_Word16 SetBitRateSafe(
        const WebRtc_Word32 bitRateBPS);

    ///////////////////////////////////////////////////////////////////////////
    // See GetEstimatedBandwidth() for the description of function, input(s)/output(s)
    // and return value.
    //
    virtual WebRtc_Word32 GetEstimatedBandwidthSafe();

    ///////////////////////////////////////////////////////////////////////////
    // See SetEstimatedBandwidth() for the description of function, input(s)/output(s)
    // and return value.
    //
    virtual WebRtc_Word32 SetEstimatedBandwidthSafe(WebRtc_Word32 estimatedBandwidth);

    ///////////////////////////////////////////////////////////////////////////
    // See GetRedPayload() for the description of function, input(s)/output(s)
    // and return value.
    //
    virtual WebRtc_Word32 GetRedPayloadSafe(
        WebRtc_UWord8* redPayload,
        WebRtc_Word16* payloadBytes);

    ///////////////////////////////////////////////////////////////////////////
    // See SetVAD() for the description of function, input(s)/output(s) and
    // return value.
    //
    WebRtc_Word16 SetVADSafe(
        const bool       enableDTX = true,
        const bool       enableVAD = false,
        const ACMVADMode mode      = VADNormal);

    ///////////////////////////////////////////////////////////////////////////
    // See ReplaceInternalDTX() for the description of function, input and
    // return value.
    //
    virtual WebRtc_Word32 ReplaceInternalDTXSafe(
        const bool replaceInternalDTX);

    ///////////////////////////////////////////////////////////////////////////
    // See IsInternalDTXReplaced() for the description of function, input and
    // return value.
    //
    virtual WebRtc_Word32 IsInternalDTXReplacedSafe(
        bool* internalDTXReplaced);

    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 CreateEncoder()
    // Creates the encoder instance.
    //
    // Return value:
    //   -1 if failed,
    //    0 if succeeded.
    //
    WebRtc_Word16 CreateEncoder();


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 CreateDecoder()
    // Creates the decoder instance.
    //
    // Return value:
    //   -1 if failed,
    //    0 if succeeded.
    //
    WebRtc_Word16 CreateDecoder();


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 EnableVAD();
    // Enables VAD with the given mode. The VAD instance will be created if
    // it does not exists.
    //
    // Input:
    //   -mode               : VAD mode c.f. audio_coding_module_typedefs.h for
    //                         the options.
    //
    // Return value:
    //   -1 if failed,
    //    0 if succeeded.
    //
    WebRtc_Word16 EnableVAD(ACMVADMode mode);


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 DisableVAD()
    // Disables VAD.
    //
    // Return value:
    //   -1 if failed,
    //    0 if succeeded.
    //
    WebRtc_Word16 DisableVAD();


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 EnableDTX()
    // Enables DTX. This method should be overwritten for codecs which have
    // internal DTX.
    //
    // Return value:
    //   -1 if failed,
    //    0 if succeeded.
    //
    virtual WebRtc_Word16 EnableDTX();


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 DisableDTX()
    // Disables usage of DTX. This method should be overwritten for codecs which
    // have internal DTX.
    //
    // Return value:
    //   -1 if failed,
    //    0 if succeeded.
    //
    virtual WebRtc_Word16 DisableDTX();


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 InternalEncode()
    // This is a codec-specific function called in EncodeSafe() to actually
    // encode a frame of audio.
    //
    // Outputs:
    //   -bitStream          : pointer to a buffer where the bit-stream is
    //                         written to.
    //   -bitStreamLenByte   : the length of the bit-stream in bytes,
    //                         a negative value indicates error.
    //
    // Return value:
    //   -1 if failed,
    //   otherwise the length of the bit-stream is returned.
    //
    virtual WebRtc_Word16 InternalEncode(
        WebRtc_UWord8* bitStream,
        WebRtc_Word16* bitStreamLenByte) = 0;


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 InternalInitEncoder()
    // This is a codec-specific function called in InitEncoderSafe(), it has to
    // do all codec-specific operation to initialize the encoder given the
    // encoder parameters.
    //
    // Input:
    //   -codecParams        : pointer to a structure that contains parameters to
    //                         initialize encoder.
    //                         Set codecParam->CodecInst.rate to -1 for
    //                         iSAC to operate in adaptive mode.
    //                         (to do: if frame-length is -1 frame-length will be
    //                         automatically adjusted, otherwise, given
    //                         frame-length is forced)
    //
    // Return value:
    //   -1 if failed,
    //    0 if succeeded.
    //
    virtual WebRtc_Word16 InternalInitEncoder(
        WebRtcACMCodecParams *codecParams) = 0;


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 InternalInitDecoder()
    // This is a codec-specific function called in InitDecoderSafe(), it has to
    // do all codec-specific operation to initialize the decoder given the
    // decoder parameters.
    //
    // Input:
    //   -codecParams        : pointer to a structure that contains parameters to
    //                         initialize encoder.
    //
    // Return value:
    //   -1 if failed,
    //    0 if succeeded.
    //
    virtual WebRtc_Word16 InternalInitDecoder(
        WebRtcACMCodecParams *codecParams) = 0;


    ///////////////////////////////////////////////////////////////////////////
    // void IncreaseNoMissedSamples()
    // This method is called to increase the number of samples that are
    // overwritten in the audio buffer.
    //
    // Input:
    //   -noSamples          : the number of overwritten samples is incremented
    //                         by this value.
    //
    void IncreaseNoMissedSamples(
        const WebRtc_Word16 noSamples);


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 InternalCreateEncoder()
    // This is a codec-specific method called in CreateEncoderSafe() it is
    // supposed to perform all codec-specific operations to create encoder
    // instance.
    //
    // Return value:
    //   -1 if failed,
    //    0 if succeeded.
    //
    virtual WebRtc_Word16 InternalCreateEncoder() = 0;


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 InternalCreateDecoder()
    // This is a codec-specific method called in CreateDecoderSafe() it is
    // supposed to perform all codec-specific operations to create decoder
    // instance.
    //
    // Return value:
    //   -1 if failed,
    //    0 if succeeded.
    //
    virtual WebRtc_Word16 InternalCreateDecoder() = 0;


    ///////////////////////////////////////////////////////////////////////////
    // void InternalDestructEncoderInst()
    // This is a codec-specific method, used in conferencing, called from
    // DestructEncoderInst(). The input argument is pointer to encoder instance
    // (codec instance for codecs that encoder and decoder share the same
    // instance). This method is called to free the memory that "ptrInst" is
    // pointing to.
    //
    // Input:
    //   -ptrInst            : pointer to encoder instance.
    //
    // Return value:
    //   -1 if failed,
    //    0 if succeeded.
    //
    virtual void InternalDestructEncoderInst(
        void* ptrInst) = 0;


    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 InternalResetEncoder()
    // This method is called to reset the states of encoder. However, the
    // current parameters, e.g. frame-length, should remain as they are. For
    // most of the codecs a re-initialization of the encoder is what needs to
    // be down. But for iSAC we like to keep the BWE history so we cannot
    // re-initialize. As soon as such an API is implemented in iSAC this method
    // has to be overwritten in ACMISAC class.
    //
    // Return value:
    //   -1 if failed,
    //    0 if succeeded.
    //
    virtual WebRtc_Word16 InternalResetEncoder();



    ///////////////////////////////////////////////////////////////////////////
    // WebRtc_Word16 ProcessFrameVADDTX()
    // This function is called when a full frame of audio is available. It will
    // break the audio frame into blocks such that each block could be processed
    // by VAD & CN/DTX. If a frame is divided into two blocks then there are two
    // cases. First, the first block is active, the second block will not be
    // processed by CN/DTX but only by VAD and return to caller with
    // '*samplesProcessed' set to zero. There, the audio frame will be encoded
    // by the encoder. Second, the first block is inactive and is processed by
    // CN/DTX, then we stop processing the next block and return to the caller
    // which is EncodeSafe(), with "*samplesProcessed" equal to the number of
    // samples in first block.
    //
    // Output:
    //   -bitStream          : pointer to a buffer where DTX frame, if
    //                         generated, will be written to.
    //   -bitStreamLenByte   : contains the length of bit-stream in bytes, if
    //                         generated. Zero if no bit-stream is generated.
    //   -noSamplesProcessed : contains no of samples that actually CN has
    //                         processed. Those samples processed by CN will not
    //                         be encoded by the encoder, obviously. If
    //                         contains zero, it means that the frame has been
    //                         identified as active by VAD. Note that
    //                         "*noSamplesProcessed" might be non-zero but
    //                         "*bitStreamLenByte" be zero.
    //
    // Return value:
    //   -1 if failed,
    //    0 if succeeded.
    //
    WebRtc_Word16 ProcessFrameVADDTX(
        WebRtc_UWord8* bitStream,
        WebRtc_Word16* bitStreamLenByte,
        WebRtc_Word16* samplesProcessed);


    ///////////////////////////////////////////////////////////////////////////
    // CanChangeEncodingParam()
    // Check if the codec parameters can be changed. In conferencing normally
    // codec parametrs cannot be changed. The exception is bit-rate of isac.
    //
    // return value:
    //   -true  if codec parameters are allowed to change.
    //   -flase otherwise.
    //
    virtual bool CanChangeEncodingParam(CodecInst& codecInst);


    ///////////////////////////////////////////////////////////////////////////
    // CurrentRate()
    // Call to get the current encoding rate of the encoder. This function
    // should be overwritten for codecs whic automatically change their
    // target rate. One example is iSAC. The output of the function is the
    // current target rate.
    //
    // Output:
    //   -rateBitPerSec      : the current target rate of the codec.
    //
    virtual void CurrentRate(
        WebRtc_Word32& /* rateBitPerSec */)
    {
        return;
    }

    virtual void SaveDecoderParamSafe(
        const WebRtcACMCodecParams* codecParams);

    // &_inAudio[_inAudioIxWrite] always point to where new audio can be
    // written to
    WebRtc_Word16         _inAudioIxWrite;

    // &_inAudio[_inAudioIxRead] points to where audio has to be read from
    WebRtc_Word16         _inAudioIxRead;

    WebRtc_Word16         _inTimestampIxWrite;

    // Where the audio is stored before encoding,
    // To save memory the following buffer can be allocated
    // dynamically for 80ms depending on the sampling frequency
    // of the codec.
    WebRtc_Word16*        _inAudio;
    WebRtc_UWord32*       _inTimestamp;

    WebRtc_Word16         _frameLenSmpl;
    WebRtc_UWord16        _noChannels;

    // This will point to a static database of the supported codecs
    WebRtc_Word16         _codecID;

    // This will account for the No of samples  were not encoded
    // the case is rare, either samples are missed due to overwite
    // at input buffer or due to encoding error
    WebRtc_UWord32        _noMissedSamples;

    // True if the encoder instance created
    bool                  _encoderExist;
    bool                  _decoderExist;
    // True if the ecncoder instance initialized
    bool                  _encoderInitialized;
    bool                  _decoderInitialized;

    bool                  _registeredInNetEq;

    // VAD/DTX
    bool                  _hasInternalDTX;
    WebRtcVadInst*        _ptrVADInst;
    bool                  _vadEnabled;
    ACMVADMode            _vadMode;
    WebRtc_Word16         _vadLabel[MAX_FRAME_SIZE_10MSEC];
    bool                  _dtxEnabled;
    WebRtcCngEncInst*     _ptrDTXInst;
    WebRtc_UWord8         _numLPCParams;
    bool                  _sentCNPrevious;
    bool                  _isMaster;

    WebRtcACMCodecParams  _encoderParams;
    WebRtcACMCodecParams  _decoderParams;

    // Used as a global lock for all avaiable decoders
    // so that no decoder is used when NetEQ decodes.
    RWLockWrapper*        _netEqDecodeLock;
    // Used to lock wrapper internal data
    // such as buffers and state variables.
    RWLockWrapper&        _codecWrapperLock;

    WebRtc_UWord32        _lastEncodedTimestamp;
    WebRtc_UWord32        _lastTimestamp;
    bool                  _isAudioBuffFresh;
    WebRtc_UWord32        _uniqueID;
};

} // namespace webrt

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_GENERIC_CODEC_H_
