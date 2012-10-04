/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_AUDIO_CODING_MODULE_IMPL_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_AUDIO_CODING_MODULE_IMPL_H_

#include "acm_codec_database.h"
#include "acm_neteq.h"
#include "acm_resampler.h"
#include "common_types.h"
#include "engine_configurations.h"

namespace webrtc {

class ACMDTMFDetection;
class ACMGenericCodec;
class CriticalSectionWrapper;
class RWLockWrapper;

#ifdef ACM_QA_TEST
#   include <stdio.h>
#endif

class AudioCodingModuleImpl : public AudioCodingModule {
 public:
  // Constructor
  AudioCodingModuleImpl(const WebRtc_Word32 id);

  // Destructor
  ~AudioCodingModuleImpl();

  // Change the unique identifier of this object.
  virtual WebRtc_Word32 ChangeUniqueId(const WebRtc_Word32 id);

  // Returns the number of milliseconds until the module want a worker thread
  // to call Process.
  WebRtc_Word32 TimeUntilNextProcess();

  // Process any pending tasks such as timeouts.
  WebRtc_Word32 Process();

  /////////////////////////////////////////
  //   Sender
  //

  // Initialize send codec.
  WebRtc_Word32 InitializeSender();

  // Reset send codec.
  WebRtc_Word32 ResetEncoder();

  // Can be called multiple times for Codec, CNG, RED.
  WebRtc_Word32 RegisterSendCodec(const CodecInst& send_codec);

  // Get current send codec.
  WebRtc_Word32 SendCodec(CodecInst& current_codec) const;

  // Get current send frequency.
  WebRtc_Word32 SendFrequency() const;

  // Get encode bitrate.
  // Adaptive rate codecs return their current encode target rate, while other
  // codecs return there longterm avarage or their fixed rate.
  WebRtc_Word32 SendBitrate() const;

  // Set available bandwidth, inform the encoder about the
  // estimated bandwidth received from the remote party.
  virtual WebRtc_Word32 SetReceivedEstimatedBandwidth(const WebRtc_Word32 bw);

  // Register a transport callback which will be
  // called to deliver the encoded buffers.
  WebRtc_Word32 RegisterTransportCallback(
      AudioPacketizationCallback* transport);

  // Used by the module to deliver messages to the codec module/application
  // AVT(DTMF).
  WebRtc_Word32 RegisterIncomingMessagesCallback(
      AudioCodingFeedback* incoming_message, const ACMCountries cpt);

  // Add 10MS of raw (PCM) audio data to the encoder.
  WebRtc_Word32 Add10MsData(const AudioFrame& audio_frame);

  // Set background noise mode for NetEQ, on, off or fade.
  WebRtc_Word32 SetBackgroundNoiseMode(const ACMBackgroundNoiseMode mode);

  // Get current background noise mode.
  WebRtc_Word32 BackgroundNoiseMode(ACMBackgroundNoiseMode& mode);

  /////////////////////////////////////////
  // (FEC) Forward Error Correction
  //

  // Configure FEC status i.e on/off.
  WebRtc_Word32 SetFECStatus(const bool enable_fec);

  // Get FEC status.
  bool FECStatus() const;

  /////////////////////////////////////////
  //   (VAD) Voice Activity Detection
  //   and
  //   (CNG) Comfort Noise Generation
  //

  WebRtc_Word32 SetVAD(const bool enable_dtx = true,
                       const bool enable_vad = false,
                       const ACMVADMode mode = VADNormal);

  WebRtc_Word32 VAD(bool& dtx_enabled, bool& vad_enabled,
                    ACMVADMode& mode) const;

  WebRtc_Word32 RegisterVADCallback(ACMVADCallback* vadCallback);

  // Get VAD aggressiveness on the incoming stream.
  ACMVADMode ReceiveVADMode() const;

  // Configure VAD aggressiveness on the incoming stream.
  WebRtc_Word16 SetReceiveVADMode(const ACMVADMode mode);

  /////////////////////////////////////////
  //   Receiver
  //

  // Initialize receiver, resets codec database etc.
  WebRtc_Word32 InitializeReceiver();

  // Reset the decoder state.
  WebRtc_Word32 ResetDecoder();

  // Get current receive frequency.
  WebRtc_Word32 ReceiveFrequency() const;

  // Get current playout frequency.
  WebRtc_Word32 PlayoutFrequency() const;

  // Register possible reveive codecs, can be called multiple times,
  // for codecs, CNG, DTMF, RED.
  WebRtc_Word32 RegisterReceiveCodec(const CodecInst& receive_codec);

  // Get current received codec.
  WebRtc_Word32 ReceiveCodec(CodecInst& current_codec) const;

  // Incoming packet from network parsed and ready for decode.
  WebRtc_Word32 IncomingPacket(const WebRtc_UWord8* incoming_payload,
                               const WebRtc_Word32 payload_length,
                               const WebRtcRTPHeader& rtp_info);

  // Incoming payloads, without rtp-info, the rtp-info will be created in ACM.
  // One usage for this API is when pre-encoded files are pushed in ACM.
  WebRtc_Word32 IncomingPayload(const WebRtc_UWord8* incoming_payload,
                                const WebRtc_Word32 payload_length,
                                const WebRtc_UWord8 payload_type,
                                const WebRtc_UWord32 timestamp = 0);

  // Minimum playout dealy (used for lip-sync).
  WebRtc_Word32 SetMinimumPlayoutDelay(const WebRtc_Word32 time_ms);

  // Configure Dtmf playout status i.e on/off playout the incoming outband Dtmf
  // tone.
  WebRtc_Word32 SetDtmfPlayoutStatus(const bool enable);

  // Get Dtmf playout status.
  bool DtmfPlayoutStatus() const;

  // Estimate the Bandwidth based on the incoming stream, needed
  // for one way audio where the RTCP send the BW estimate.
  // This is also done in the RTP module .
  WebRtc_Word32 DecoderEstimatedBandwidth() const;

  // Set playout mode voice, fax.
  WebRtc_Word32 SetPlayoutMode(const AudioPlayoutMode mode);

  // Get playout mode voice, fax.
  AudioPlayoutMode PlayoutMode() const;

  // Get playout timestamp.
  WebRtc_Word32 PlayoutTimestamp(WebRtc_UWord32& timestamp);

  // Get 10 milliseconds of raw audio data to play out, and
  // automatic resample to the requested frequency if > 0.
  WebRtc_Word32 PlayoutData10Ms(const WebRtc_Word32 desired_freq_hz,
                                AudioFrame &audio_frame);

  /////////////////////////////////////////
  //   Statistics
  //

  WebRtc_Word32 NetworkStatistics(ACMNetworkStatistics& statistics) const;

  void DestructEncoderInst(void* inst);

  WebRtc_Word16 AudioBuffer(WebRtcACMAudioBuff& buffer);

  // GET RED payload for iSAC. The method id called when 'this' ACM is
  // the default ACM.
  WebRtc_Word32 REDPayloadISAC(const WebRtc_Word32 isac_rate,
                               const WebRtc_Word16 isac_bw_estimate,
                               WebRtc_UWord8* payload,
                               WebRtc_Word16* length_bytes);

  WebRtc_Word16 SetAudioBuffer(WebRtcACMAudioBuff& buffer);

  WebRtc_UWord32 EarliestTimestamp() const;

  WebRtc_Word32 LastEncodedTimestamp(WebRtc_UWord32& timestamp) const;

  WebRtc_Word32 ReplaceInternalDTXWithWebRtc(const bool use_webrtc_dtx);

  WebRtc_Word32 IsInternalDTXReplacedWithWebRtc(bool& uses_webrtc_dtx);

  WebRtc_Word32 SetISACMaxRate(const WebRtc_UWord32 max_bit_per_sec);

  WebRtc_Word32 SetISACMaxPayloadSize(const WebRtc_UWord16 max_size_bytes);

  WebRtc_Word32 ConfigISACBandwidthEstimator(
      const WebRtc_UWord8 frame_size_ms,
      const WebRtc_UWord16 rate_bit_per_sec,
      const bool enforce_frame_size = false);

  WebRtc_Word32 UnregisterReceiveCodec(const WebRtc_Word16 payload_type);

 protected:
  void UnregisterSendCodec();

  WebRtc_Word32 UnregisterReceiveCodecSafe(const WebRtc_Word16 id);

  ACMGenericCodec* CreateCodec(const CodecInst& codec);

  WebRtc_Word16 DecoderParamByPlType(const WebRtc_UWord8 payload_type,
                                     WebRtcACMCodecParams& codec_params) const;

  WebRtc_Word16 DecoderListIDByPlName(
      const char* name, const WebRtc_UWord16 frequency = 0) const;

  WebRtc_Word32 InitializeReceiverSafe();

  bool HaveValidEncoder(const char* caller_name) const;

  WebRtc_Word32 RegisterRecCodecMSSafe(const CodecInst& receive_codec,
                                       WebRtc_Word16 codec_id,
                                       WebRtc_Word16 mirror_id,
                                       ACMNetEQ::JB jitter_buffer);

 private:
  // Change required states after starting to receive the codec corresponding
  // to |index|.
  int UpdateUponReceivingCodec(int index);

  // Remove all slaves and initialize a stereo slave with required codecs
  // from the master.
  int InitStereoSlave();

  // Returns true if the codec's |index| is registered with the master and
  // is a stereo codec, RED or CN.
  bool IsCodecForSlave(int index) const;

  // Returns true if the |codec| is RED.
  bool IsCodecRED(const CodecInst* codec) const;
  // ...or if its |index| is RED.
  bool IsCodecRED(int index) const;

  // Returns true if the |codec| is CN.
  bool IsCodecCN(int index) const;
  // ...or if its |index| is CN.
  bool IsCodecCN(const CodecInst* codec) const;

  AudioPacketizationCallback* _packetizationCallback;
  WebRtc_Word32 _id;
  WebRtc_UWord32 _lastTimestamp;
  WebRtc_UWord32 _lastInTimestamp;
  CodecInst _sendCodecInst;
  uint8_t _cng_nb_pltype;
  uint8_t _cng_wb_pltype;
  uint8_t _cng_swb_pltype;
  uint8_t _red_pltype;
  bool _vadEnabled;
  bool _dtxEnabled;
  ACMVADMode _vadMode;
  ACMGenericCodec* _codecs[ACMCodecDB::kMaxNumCodecs];
  ACMGenericCodec* _slaveCodecs[ACMCodecDB::kMaxNumCodecs];
  WebRtc_Word16 _mirrorCodecIdx[ACMCodecDB::kMaxNumCodecs];
  bool _stereoReceive[ACMCodecDB::kMaxNumCodecs];
  bool _stereoReceiveRegistered;
  bool _stereoSend;
  int _prev_received_channel;
  int _expected_channels;
  WebRtc_Word32 _currentSendCodecIdx;
  int _current_receive_codec_idx;
  bool _sendCodecRegistered;
  ACMResampler _inputResampler;
  ACMResampler _outputResampler;
  ACMNetEQ _netEq;
  CriticalSectionWrapper* _acmCritSect;
  ACMVADCallback* _vadCallback;
  WebRtc_UWord8 _lastRecvAudioCodecPlType;

  // RED/FEC.
  bool _isFirstRED;
  bool _fecEnabled;
  WebRtc_UWord8* _redBuffer;
  RTPFragmentationHeader* _fragmentation;
  WebRtc_UWord32 _lastFECTimestamp;
  // If no RED is registered as receive codec this
  // will have an invalid value.
  WebRtc_UWord8 _receiveREDPayloadType;

  // This is to keep track of CN instances where we can send DTMFs.
  WebRtc_UWord8 _previousPayloadType;

  // This keeps track of payload types associated with _codecs[].
  // We define it as signed variable and initialize with -1 to indicate
  // unused elements.
  WebRtc_Word16 _registeredPlTypes[ACMCodecDB::kMaxNumCodecs];

  // Used when payloads are pushed into ACM without any RTP info
  // One example is when pre-encoded bit-stream is pushed from
  // a file.
  WebRtcRTPHeader* _dummyRTPHeader;
  WebRtc_UWord16 _recvPlFrameSizeSmpls;

  bool _receiverInitialized;
  ACMDTMFDetection* _dtmfDetector;

  AudioCodingFeedback* _dtmfCallback;
  WebRtc_Word16 _lastDetectedTone;
  CriticalSectionWrapper* _callbackCritSect;

  AudioFrame _audioFrame;

#ifdef ACM_QA_TEST
  FILE* _outgoingPL;
  FILE* _incomingPL;
#endif

};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_AUDIO_CODING_MODULE_IMPL_H_
