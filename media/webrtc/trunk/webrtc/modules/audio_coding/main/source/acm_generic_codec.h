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

#include "webrtc/modules/audio_coding/main/interface/audio_coding_module_typedefs.h"
#include "webrtc/modules/audio_coding/main/source/acm_common_defs.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq.h"
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"

#define MAX_FRAME_SIZE_10MSEC 6

// forward declaration
struct WebRtcVadInst;
struct WebRtcCngEncInst;

namespace webrtc {

// forward declaration
struct CodecInst;
class ACMNetEQ;

class ACMGenericCodec {
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
  //    and encoded by the encoder. The "*encoding_type" will be either
  //    "kActiveNormalEncode" or "kPassiveNormalEncode" if frame is active or
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
  //   -bitstream          : a buffer where bit-stream will be written to.
  //   -bitstream_len_byte : contains the length of the bit-stream in
  //                         bytes.
  //   -timestamp          : contains the RTP timestamp, this is the
  //                         sampling time of the first sample encoded
  //                         (measured in number of samples).
  //   -encoding_type       : contains the type of encoding applied on the
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
  WebRtc_Word16 Encode(WebRtc_UWord8* bitstream,
                       WebRtc_Word16* bitstream_len_byte,
                       WebRtc_UWord32* timestamp,
                       WebRtcACMEncodingType* encoding_type);

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word16 Decode()
  // This function is used to decode a given bit-stream, without engaging
  // NetEQ.
  //
  // This function acquires the appropriate locks and calls DecodeSafe() for
  // the actual processing. Please note that this is not functional yet.
  //
  // Inputs:
  //   -bitstream         : a buffer where bit-stream will be read.
  //   -bitstream_len_byte : the length of the bit-stream in bytes.
  //
  // Outputs:
  //   -audio              : pointer to a buffer where the audio will written.
  //   -audio_samples      : number of audio samples out of decoding the given
  //                         bit-stream.
  //   -speech_type        : speech type (for future use).
  //
  // Return value:
  //   -1 if failed to decode,
  //    0 if succeeded.
  //
  WebRtc_Word16 Decode(WebRtc_UWord8* bitstream,
                       WebRtc_Word16 bitstream_len_byte,
                       WebRtc_Word16* audio,
                       WebRtc_Word16* audio_samples,
                       WebRtc_Word8* speech_type);

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
  //
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
  //   -enc_params         : a buffer where the encoder parameters is
  //                         written to. If the encoder is not
  //                         initialized this buffer is filled with
  //                         invalid values
  // Return value:
  //   -1 if the encoder is not initialized,
  //    0 otherwise.
  //
  WebRtc_Word16 EncoderParams(WebRtcACMCodecParams *enc_params);

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word16 DecoderParams(...)
  // It is called to get decoder parameters. It will call DecoderParamsSafe()
  // in turn.
  //
  // Output:
  //   -dec_params         : a buffer where the decoder parameters is
  //                         written to. If the decoder is not initialized
  //                         this buffer is filled with invalid values
  //
  // Return value:
  //   -1 if the decoder is not initialized,
  //    0 otherwise.
  //
  //
  bool DecoderParams(WebRtcACMCodecParams *dec_params,
                     const WebRtc_UWord8 payload_type);

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word16 InitEncoder(...)
  // This function is called to initialize the encoder with the given
  // parameters.
  //
  // Input:
  //   -codec_params        : parameters of encoder.
  //   -force_initialization: if false the initialization is invoked only if
  //                          the encoder is not initialized. If true the
  //                          encoder is forced to (re)initialize.
  //
  // Return value:
  //   0 if could initialize successfully,
  //  -1 if failed to initialize.
  //
  //
  WebRtc_Word16 InitEncoder(WebRtcACMCodecParams* codec_params,
                            bool force_initialization);

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word16 InitDecoder()
  // This function is called to initialize the decoder with the given
  // parameters. (c.f. acm_common_defs.h & common_types.h for the
  // definition of the structure)
  //
  // Input:
  //   -codec_params        : parameters of decoder.
  //   -force_initialization: if false the initialization is invoked only
  //                          if the decoder is not initialized. If true
  //                          the encoder is forced to(re)initialize.
  //
  // Return value:
  //   0 if could initialize successfully,
  //  -1 if failed to initialize.
  //
  //
  WebRtc_Word16 InitDecoder(WebRtcACMCodecParams* codec_params,
                            bool force_initialization);

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 RegisterInNetEq(...)
  // This function is called to register the decoder in NetEq, with the given
  // payload type.
  //
  // Inputs:
  //   -neteq              : pointer to NetEq Instance
  //   -codec_inst         : instance with of the codec settings of the codec
  //
  // Return values
  //   -1 if failed to register,
  //    0 if successfully initialized.
  //
  WebRtc_Word32 RegisterInNetEq(ACMNetEQ* neteq, const CodecInst& codec_inst);

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 Add10MsData(...)
  // This function is called to add 10 ms of audio to the audio buffer of
  // the codec.
  //
  // Inputs:
  //   -timestamp          : the timestamp of the 10 ms audio. the timestamp
  //                         is the sampling time of the
  //                         first sample measured in number of samples.
  //   -data               : a buffer that contains the audio. The codec
  //                         expects to get the audio in correct sampling
  //                         frequency
  //   -length             : the length of the audio buffer
  //   -audio_channel      : 0 for mono, 1 for stereo (not supported yet)
  //
  // Return values:
  //   -1 if failed
  //    0 otherwise.
  //
  WebRtc_Word32 Add10MsData(const WebRtc_UWord32 timestamp,
                            const WebRtc_Word16* data,
                            const WebRtc_UWord16 length,
                            const WebRtc_UWord8 audio_channel);

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
  //   -bitrate_bps        : encoding rate in bits per second
  //
  // Return value:
  //   -1 if failed to set the rate, due to invalid input or given
  //      codec is not rate-adjustable.
  //    0 if the rate is adjusted successfully
  //
  WebRtc_Word16 SetBitRate(const WebRtc_Word32 bitrate_bps);

  ///////////////////////////////////////////////////////////////////////////
  // DestructEncoderInst()
  // This API is used in conferencing. It will free the memory that is pointed
  // by |ptr_inst|. |ptr_inst| is a pointer to encoder instance, created and
  // filled up by calling EncoderInst(...).
  //
  // Inputs:
  //   -ptr_inst            : pointer to an encoder instance to be deleted.
  //
  //
  void DestructEncoderInst(void* ptr_inst);

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word16 AudioBuffer()
  // This is used when synchronization of codecs is required. There are cases
  // that the audio buffers of two codecs have to be synched. By calling this
  // function on can get the audio buffer and other related parameters, such
  // as timestamps...
  //
  // Output:
  //   -audio_buff         : a pointer to WebRtcACMAudioBuff where the audio
  //                         buffer of this codec will be written to.
  //
  // Return value:
  //   -1 if fails to copy the audio buffer,
  //    0 if succeeded.
  //
  WebRtc_Word16 AudioBuffer(WebRtcACMAudioBuff& audio_buff);

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
  WebRtc_Word16 SetAudioBuffer(WebRtcACMAudioBuff& audio_buff);

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
  //   -enable_dtx         : if true DTX will be enabled otherwise the DTX is
  //                         disabled. If codec has internal DTX that will be
  //                         used, otherwise WebRtc-CNG is used. In the latter
  //                         case VAD is automatically activated.
  //   -enable_vad         : if true WebRtc-VAD is enabled, otherwise VAD is
  //                         disabled, except for the case that DTX is enabled
  //                         but codec doesn't have internal DTX. In this case
  //                         VAD is enabled regardless of the value of
  //                         |enable_vad|.
  //   -mode               : this specifies the aggressiveness of VAD.
  //
  // Return value
  //   -1 if failed to set DTX & VAD as specified,
  //    0 if succeeded.
  //
  WebRtc_Word16 SetVAD(const bool enable_dtx = true,
                       const bool enable_vad = false,
                       const ACMVADMode mode = VADNormal);

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 ReplaceInternalDTX()
  // This is called to replace the codec internal DTX with WebRtc DTX.
  // This is only valid for G729 where the user has possibility to replace
  // AnnexB with WebRtc DTX. For other codecs this function has no effect.
  //
  // Input:
  //   -replace_internal_dtx : if true the internal DTX is replaced with WebRtc.
  //
  // Return value
  //   -1 if failed to replace internal DTX,
  //    0 if succeeded.
  //
  WebRtc_Word32 ReplaceInternalDTX(const bool replace_internal_dtx);

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 IsInternalDTXReplaced()
  // This is called to check if the codec internal DTX is replaced by WebRtc
  // DTX. This is only valid for G729 where the user has possibility to replace
  // AnnexB with WebRtc DTX. For other codecs this function has no effect.
  //
  // Output:
  //   -internal_dtx_replaced: if true the internal DTX is replaced with WebRtc.
  //
  // Return value
  //   -1 if failed to check
  //    0 if succeeded.
  //
  WebRtc_Word32 IsInternalDTXReplaced(bool* internal_dtx_replaced);

  ///////////////////////////////////////////////////////////////////////////
  // void SetNetEqDecodeLock()
  // Passes the NetEq lock to the codec.
  //
  // Input:
  //   -neteq_decode_lock  : pointer to the lock associated with NetEQ of ACM.
  //
  void SetNetEqDecodeLock(RWLockWrapper* neteq_decode_lock) {
    neteq_decode_lock_ = neteq_decode_lock;
  }

  ///////////////////////////////////////////////////////////////////////////
  // bool HasInternalDTX()
  // Used to check if the codec has internal DTX.
  //
  // Return value:
  //   true if the codec has an internal DTX, e.g. G729,
  //   false otherwise.
  //
  bool HasInternalDTX() const {
    return has_internal_dtx_;
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
  //       -estimated_bandwidth:    estimated bandwidth in bits/sec
  //
  // Return value:
  //   -1 if fails to set estimated bandwidth,
  //    0 on success.
  //
  WebRtc_Word32 SetEstimatedBandwidth(WebRtc_Word32 estimated_bandwidth);

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 GetRedPayload()
  // Used to get codec specific RED payload (if such is implemented).
  // Currently only done in iSAC.
  //
  // Outputs:
  //   -red_payload       : a pointer to the data for RED payload.
  //   -payload_bytes     : number of bytes in RED payload.
  //
  // Return value:
  //   -1 if fails to get codec specific RED,
  //    0 if succeeded.
  //
  WebRtc_Word32 GetRedPayload(WebRtc_UWord8* red_payload,
                              WebRtc_Word16* payload_bytes);

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
  WebRtc_Word16 ResetDecoder(WebRtc_Word16 payload_type);

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
  // Set a unique ID for the codec to be used for tracing and debugging
  //
  // Input
  //   -id                 : A number to identify the codec.
  //
  void SetUniqueID(const WebRtc_UWord32 id);

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
  // decoder for different flavors of the codec. One example is iSAC. there,
  // iSAC 16 kHz and iSAC 32 kHz are treated as two different codecs with
  // different payload types, however, there is only one iSAC instance to
  // decode. The reason for that is we would like to decode and encode with
  // the same codec instance for bandwidth estimator to work.
  //
  // Each time that we receive a new payload type, we call this function to
  // prepare the decoder associated with the new payload. Normally, decoders
  // doesn't have to do anything. For iSAC the decoder has to change it's
  // sampling rate. The input parameter specifies the current flavor of the
  // codec in codec database. For instance, if we just got a SWB payload then
  // the input parameter is ACMCodecDB::isacswb.
  //
  // Input:
  //   -codec_id           : the ID of the codec associated with the
  //                         payload type that we just received.
  //
  // Return value:
  //    0 if succeeded in updating the decoder.
  //   -1 if failed to update.
  //
  virtual WebRtc_Word16 UpdateDecoderSampFreq(WebRtc_Word16 /* codec_id */) {
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
  //   -samp_freq_hz        : encoder sampling frequency.
  //
  // Return value:
  //   -1 if failed, or if this is meaningless for the given codec.
  //    0 if succeeded.
  //
  virtual WebRtc_Word16 UpdateEncoderSampFreq(
      WebRtc_UWord16 samp_freq_hz);

  ///////////////////////////////////////////////////////////////////////////
  // EncoderSampFreq()
  // Get the sampling frequency that the encoder (WebRtc wrapper) expects.
  //
  // Output:
  //   -samp_freq_hz       : sampling frequency, in Hertz, which the encoder
  //                         should be fed with.
  //
  // Return value:
  //   -1 if failed to output sampling rate.
  //    0 if the sample rate is returned successfully.
  //
  virtual WebRtc_Word16 EncoderSampFreq(WebRtc_UWord16& samp_freq_hz);

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word32 ConfigISACBandwidthEstimator()
  // Call this function to configure the bandwidth estimator of ISAC.
  // During the adaptation of bit-rate, iSAC automatically adjusts the
  // frame-size (either 30 or 60 ms) to save on RTP header. The initial
  // frame-size can be specified by the first argument. The configuration also
  // regards the initial estimate of bandwidths. The estimator starts from
  // this point and converges to the actual bottleneck. This is given by the
  // second parameter. Furthermore, it is also possible to control the
  // adaptation of frame-size. This is specified by the last parameter.
  //
  // Input:
  //   -init_frame_fize_ms : initial frame-size in milliseconds. For iSAC-wb
  //                         30 ms and 60 ms (default) are acceptable values,
  //                         and for iSAC-swb 30 ms is the only acceptable
  //                         value. Zero indicates default value.
  //   -init_rate_bps      : initial estimate of the bandwidth. Values
  //                         between 10000 and 58000 are acceptable.
  //   -enforce_frame_size : if true, the frame-size will not be adapted.
  //
  // Return value:
  //   -1 if failed to configure the bandwidth estimator,
  //    0 if the configuration was successfully applied.
  //
  virtual WebRtc_Word32 ConfigISACBandwidthEstimator(
      const WebRtc_UWord8 init_frame_size_msec,
      const WebRtc_UWord16 init_rate_bps,
      const bool enforce_frame_size);

  ///////////////////////////////////////////////////////////////////////////
  // SetISACMaxPayloadSize()
  // Set the maximum payload size of iSAC packets. No iSAC payload,
  // regardless of its frame-size, may exceed the given limit. For
  // an iSAC payload of size B bits and frame-size T sec we have;
  // (B < max_payload_len_bytes * 8) and (B/T < max_rate_bit_per_sec), c.f.
  // SetISACMaxRate().
  //
  // Input:
  //   -max_payload_len_bytes : maximum payload size in bytes.
  //
  // Return value:
  //   -1 if failed to set the maximum  payload-size.
  //    0 if the given length is set successfully.
  //
  virtual WebRtc_Word32 SetISACMaxPayloadSize(
      const WebRtc_UWord16 max_payload_len_bytes);

  ///////////////////////////////////////////////////////////////////////////
  // SetISACMaxRate()
  // Set the maximum instantaneous rate of iSAC. For a payload of B bits
  // with a frame-size of T sec the instantaneous rate is B/T bits per
  // second. Therefore, (B/T < max_rate_bit_per_sec) and
  // (B < max_payload_len_bytes * 8) are always satisfied for iSAC payloads,
  // c.f SetISACMaxPayloadSize().
  //
  // Input:
  //   -max_rate_bps       : maximum instantaneous bit-rate given in bits/sec.
  //
  // Return value:
  //   -1 if failed to set the maximum rate.
  //    0 if the maximum rate is set successfully.
  //
  virtual WebRtc_Word32 SetISACMaxRate(const WebRtc_UWord32 max_rate_bps);

  ///////////////////////////////////////////////////////////////////////////
  // SaveDecoderParamS()
  // Save the parameters of decoder.
  //
  // Input:
  //   -codec_params       : pointer to a structure where the parameters of
  //                         decoder is stored in.
  //
  void SaveDecoderParam(const WebRtcACMCodecParams* codec_params);

  WebRtc_Word32 FrameSize() {
    return frame_len_smpl_;
  }

  void SetIsMaster(bool is_master);

  ///////////////////////////////////////////////////////////////////////////
  // REDPayloadISAC()
  // This is an iSAC-specific function. The function is called to get RED
  // payload from a default-encoder.
  //
  // Inputs:
  //   -isac_rate          : the target rate of the main payload. A RED
  //                         payload is generated according to the rate of
  //                         main payload. Note that we are not specifying the
  //                         rate of RED payload, but the main payload.
  //   -isac_bw_estimate   : bandwidth information should be inserted in
  //                         RED payload.
  //
  // Output:
  //   -payload            : pointer to a buffer where the RED payload will
  //                         written to.
  //   -payload_len_bytes  : a place-holder to write the length of the RED
  //                         payload in Bytes.
  //
  // Return value:
  //   -1 if an error occurs, otherwise the length of the payload (in Bytes)
  //   is returned.
  //
  virtual WebRtc_Word16 REDPayloadISAC(const WebRtc_Word32 isac_rate,
                                       const WebRtc_Word16 isac_bw_estimate,
                                       WebRtc_UWord8* payload,
                                       WebRtc_Word16* payload_len_bytes);

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

  ///////////////////////////////////////////////////////////////////////////
  // HasFrameToEncode()
  // Returns true if there is enough audio buffered for encoding, such that
  // calling Encode() will return a payload.
  //
  bool HasFrameToEncode() const;

 protected:
  ///////////////////////////////////////////////////////////////////////////
  // All the functions with FunctionNameSafe(...) contain the actual
  // implementation of FunctionName(...). FunctionName() acquires an
  // appropriate lock and calls FunctionNameSafe() to do the actual work.
  // Therefore, for the description of functionality, input/output arguments
  // and return value we refer to FunctionName()
  //

  ///////////////////////////////////////////////////////////////////////////
  // See Decode() for the description of function, input(s)/output(s) and
  // return value.
  //
  virtual WebRtc_Word16 DecodeSafe(WebRtc_UWord8* bitstream,
                                   WebRtc_Word16 bitstream_len_byte,
                                   WebRtc_Word16* audio,
                                   WebRtc_Word16* audio_samples,
                                   WebRtc_Word8* speech_type) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // See Add10MsSafe() for the description of function, input(s)/output(s)
  // and return value.
  //
  virtual WebRtc_Word32 Add10MsDataSafe(const WebRtc_UWord32 timestamp,
                                        const WebRtc_Word16* data,
                                        const WebRtc_UWord16 length,
                                        const WebRtc_UWord8 audio_channel);

  ///////////////////////////////////////////////////////////////////////////
  // See RegisterInNetEq() for the description of function,
  // input(s)/output(s) and  return value.
  //
  virtual WebRtc_Word32 CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                                 const CodecInst& codec_inst) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // See EncoderParam() for the description of function, input(s)/output(s)
  // and return value.
  //
  WebRtc_Word16 EncoderParamsSafe(WebRtcACMCodecParams *enc_params);

  ///////////////////////////////////////////////////////////////////////////
  // See DecoderParam for the description of function, input(s)/output(s)
  // and return value.
  //
  // Note:
  // Any Class where a single instance handle several flavors of the
  // same codec, therefore, several payload types are associated with
  // the same instance have to implement this function.
  //
  // Currently only iSAC is implementing it. A single iSAC instance is
  // used for decoding both WB & SWB stream. At one moment both WB & SWB
  // can be registered as receive codec. Hence two payloads are associated
  // with a single codec instance.
  //
  virtual bool DecoderParamsSafe(WebRtcACMCodecParams *dec_params,
                                 const WebRtc_UWord8 payload_type);

  ///////////////////////////////////////////////////////////////////////////
  // See ResetEncoder() for the description of function, input(s)/output(s)
  // and return value.
  //
  WebRtc_Word16 ResetEncoderSafe();

  ///////////////////////////////////////////////////////////////////////////
  // See InitEncoder() for the description of function, input(s)/output(s)
  // and return value.
  //
  WebRtc_Word16 InitEncoderSafe(WebRtcACMCodecParams *codec_params,
                                bool force_initialization);

  ///////////////////////////////////////////////////////////////////////////
  // See InitDecoder() for the description of function, input(s)/output(s)
  // and return value.
  //
  WebRtc_Word16 InitDecoderSafe(WebRtcACMCodecParams *codec_params,
                                bool force_initialization);

  ///////////////////////////////////////////////////////////////////////////
  // See ResetDecoder() for the description of function, input(s)/output(s)
  // and return value.
  //
  WebRtc_Word16 ResetDecoderSafe(WebRtc_Word16 payload_type);

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
  virtual WebRtc_Word16 SetBitRateSafe(const WebRtc_Word32 bitrate_bps);

  ///////////////////////////////////////////////////////////////////////////
  // See GetEstimatedBandwidth() for the description of function,
  // input(s)/output(s) and return value.
  //
  virtual WebRtc_Word32 GetEstimatedBandwidthSafe();

  ///////////////////////////////////////////////////////////////////////////
  // See SetEstimatedBandwidth() for the description of function,
  // input(s)/output(s) and return value.
  //
  virtual WebRtc_Word32 SetEstimatedBandwidthSafe(
      WebRtc_Word32 estimated_bandwidth);

  ///////////////////////////////////////////////////////////////////////////
  // See GetRedPayload() for the description of function, input(s)/output(s)
  // and return value.
  //
  virtual WebRtc_Word32 GetRedPayloadSafe(WebRtc_UWord8* red_payload,
                                          WebRtc_Word16* payload_bytes);

  ///////////////////////////////////////////////////////////////////////////
  // See SetVAD() for the description of function, input(s)/output(s) and
  // return value.
  //
  WebRtc_Word16 SetVADSafe(const bool enable_dtx = true,
                           const bool enable_vad = false,
                           const ACMVADMode mode = VADNormal);

  ///////////////////////////////////////////////////////////////////////////
  // See ReplaceInternalDTX() for the description of function, input and
  // return value.
  //
  virtual WebRtc_Word32 ReplaceInternalDTXSafe(const bool replace_internal_dtx);

  ///////////////////////////////////////////////////////////////////////////
  // See IsInternalDTXReplaced() for the description of function, input and
  // return value.
  //
  virtual WebRtc_Word32 IsInternalDTXReplacedSafe(bool* internal_dtx_replaced);

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
  //   -bitstream          : pointer to a buffer where the bit-stream is
  //                         written to.
  //   -bitstream_len_byte : the length of the bit-stream in bytes,
  //                         a negative value indicates error.
  //
  // Return value:
  //   -1 if failed,
  //   otherwise the length of the bit-stream is returned.
  //
  virtual WebRtc_Word16 InternalEncode(WebRtc_UWord8* bitstream,
                                       WebRtc_Word16* bitstream_len_byte) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word16 InternalInitEncoder()
  // This is a codec-specific function called in InitEncoderSafe(), it has to
  // do all codec-specific operation to initialize the encoder given the
  // encoder parameters.
  //
  // Input:
  //   -codec_params       : pointer to a structure that contains parameters to
  //                         initialize encoder.
  //                         Set codec_params->codec_inst.rate to -1 for
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
      WebRtcACMCodecParams *codec_params) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // WebRtc_Word16 InternalInitDecoder()
  // This is a codec-specific function called in InitDecoderSafe(), it has to
  // do all codec-specific operation to initialize the decoder given the
  // decoder parameters.
  //
  // Input:
  //   -codec_params       : pointer to a structure that contains parameters to
  //                         initialize encoder.
  //
  // Return value:
  //   -1 if failed,
  //    0 if succeeded.
  //
  virtual WebRtc_Word16 InternalInitDecoder(
      WebRtcACMCodecParams *codec_params) = 0;

  ///////////////////////////////////////////////////////////////////////////
  // void IncreaseNoMissedSamples()
  // This method is called to increase the number of samples that are
  // overwritten in the audio buffer.
  //
  // Input:
  //   -num_samples        : the number of overwritten samples is incremented
  //                         by this value.
  //
  void IncreaseNoMissedSamples(const WebRtc_Word16 num_samples);

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
  // instance). This method is called to free the memory that |ptr_inst| is
  // pointing to.
  //
  // Input:
  //   -ptr_inst           : pointer to encoder instance.
  //
  // Return value:
  //   -1 if failed,
  //    0 if succeeded.
  //
  virtual void InternalDestructEncoderInst(void* ptr_inst) = 0;

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
  // '*samples_processed' set to zero. There, the audio frame will be encoded
  // by the encoder. Second, the first block is inactive and is processed by
  // CN/DTX, then we stop processing the next block and return to the caller
  // which is EncodeSafe(), with "*samples_processed" equal to the number of
  // samples in first block.
  //
  // Output:
  //   -bitstream          : pointer to a buffer where DTX frame, if
  //                         generated, will be written to.
  //   -bitstream_len_byte : contains the length of bit-stream in bytes, if
  //                         generated. Zero if no bit-stream is generated.
  //   -samples_processed  : contains no of samples that actually CN has
  //                         processed. Those samples processed by CN will not
  //                         be encoded by the encoder, obviously. If
  //                         contains zero, it means that the frame has been
  //                         identified as active by VAD. Note that
  //                         "*samples_processed" might be non-zero but
  //                         "*bitstream_len_byte" be zero.
  //
  // Return value:
  //   -1 if failed,
  //    0 if succeeded.
  //
  WebRtc_Word16 ProcessFrameVADDTX(WebRtc_UWord8* bitstream,
                                   WebRtc_Word16* bitstream_len_byte,
                                   WebRtc_Word16* samples_processed);

  ///////////////////////////////////////////////////////////////////////////
  // CanChangeEncodingParam()
  // Check if the codec parameters can be changed. In conferencing normally
  // codec parameters cannot be changed. The exception is bit-rate of isac.
  //
  // return value:
  //   -true  if codec parameters are allowed to change.
  //   -false otherwise.
  //
  virtual bool CanChangeEncodingParam(CodecInst& codec_inst);

  ///////////////////////////////////////////////////////////////////////////
  // CurrentRate()
  // Call to get the current encoding rate of the encoder. This function
  // should be overwritten for codecs which automatically change their
  // target rate. One example is iSAC. The output of the function is the
  // current target rate.
  //
  // Output:
  //   -rate_bps           : the current target rate of the codec.
  //
  virtual void CurrentRate(WebRtc_Word32& /* rate_bps */) {
    return;
  }

  virtual void SaveDecoderParamSafe(const WebRtcACMCodecParams* codec_params);

  // &in_audio_[in_audio_ix_write_] always point to where new audio can be
  // written to
  WebRtc_Word16 in_audio_ix_write_;

  // &in_audio_[in_audio_ix_read_] points to where audio has to be read from
  WebRtc_Word16 in_audio_ix_read_;

  WebRtc_Word16 in_timestamp_ix_write_;

  // Where the audio is stored before encoding,
  // To save memory the following buffer can be allocated
  // dynamically for 80 ms depending on the sampling frequency
  // of the codec.
  WebRtc_Word16* in_audio_;
  WebRtc_UWord32* in_timestamp_;

  WebRtc_Word16 frame_len_smpl_;
  WebRtc_UWord16 num_channels_;

  // This will point to a static database of the supported codecs
  WebRtc_Word16 codec_id_;

  // This will account for the number of samples  were not encoded
  // the case is rare, either samples are missed due to overwrite
  // at input buffer or due to encoding error
  WebRtc_UWord32 num_missed_samples_;

  // True if the encoder instance created
  bool encoder_exist_;
  bool decoder_exist_;
  // True if the encoder instance initialized
  bool encoder_initialized_;
  bool decoder_initialized_;

  bool registered_in_neteq_;

  // VAD/DTX
  bool has_internal_dtx_;
  WebRtcVadInst* ptr_vad_inst_;
  bool vad_enabled_;
  ACMVADMode vad_mode_;
  WebRtc_Word16 vad_label_[MAX_FRAME_SIZE_10MSEC];
  bool dtx_enabled_;
  WebRtcCngEncInst* ptr_dtx_inst_;
  WebRtc_UWord8 num_lpc_params_;
  bool sent_cn_previous_;
  bool is_master_;
  int16_t prev_frame_cng_;

  WebRtcACMCodecParams encoder_params_;
  WebRtcACMCodecParams decoder_params_;

  // Used as a global lock for all available decoders
  // so that no decoder is used when NetEQ decodes.
  RWLockWrapper* neteq_decode_lock_;
  // Used to lock wrapper internal data
  // such as buffers and state variables.
  RWLockWrapper& codec_wrapper_lock_;

  WebRtc_UWord32 last_encoded_timestamp_;
  WebRtc_UWord32 last_timestamp_;
  bool is_audio_buff_fresh_;
  WebRtc_UWord32 unique_id_;
};

}  // namespace webrt

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_GENERIC_CODEC_H_
