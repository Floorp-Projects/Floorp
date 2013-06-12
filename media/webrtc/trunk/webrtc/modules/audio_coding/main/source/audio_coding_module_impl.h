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

#include "webrtc/common_types.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/audio_coding/main/source/acm_codec_database.h"
#include "webrtc/modules/audio_coding/main/source/acm_neteq.h"
#include "webrtc/modules/audio_coding/main/source/acm_resampler.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

class ACMDTMFDetection;
class ACMGenericCodec;
class CriticalSectionWrapper;
class RWLockWrapper;

class AudioCodingModuleImpl : public AudioCodingModule {
 public:
  // Constructor
  explicit AudioCodingModuleImpl(const WebRtc_Word32 id);

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

  // Register Secondary codec for dual-streaming. Dual-streaming is activated
  // right after the secondary codec is registered.
  int RegisterSecondarySendCodec(const CodecInst& send_codec);

  // Unregister the secondary codec. Dual-streaming is deactivated right after
  // deregistering secondary codec.
  void UnregisterSecondarySendCodec();

  // Get the secondary codec.
  int SecondarySendCodec(CodecInst* secondary_codec) const;

  // Get current send codec.
  WebRtc_Word32 SendCodec(CodecInst& current_codec) const;

  // Get current send frequency.
  WebRtc_Word32 SendFrequency() const;

  // Get encode bit-rate.
  // Adaptive rate codecs return their current encode target rate, while other
  // codecs return there long-term average or their fixed rate.
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

  // Add 10 ms of raw (PCM) audio data to the encoder.
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

  WebRtc_Word32 RegisterVADCallback(ACMVADCallback* vad_callback);

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

  // Register possible receive codecs, can be called multiple times,
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

  // Minimum playout delay (used for lip-sync).
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
                                       ACMNetEQ::JitterBuffer jitter_buffer);

  // Set VAD/DTX status. This function does not acquire a lock, and it is
  // created to be called only from inside a critical section.
  int SetVADSafe(bool enable_dtx, bool enable_vad, ACMVADMode mode);

  // Process buffered audio when dual-streaming is not enabled (When RED is
  // enabled still this function is used.)
  int ProcessSingleStream();

  // Process buffered audio when dual-streaming is enabled, i.e. secondary send
  // codec is registered.
  int ProcessDualStream();

  // Preprocessing of input audio, including resampling and down-mixing if
  // required, before pushing audio into encoder's buffer.
  //
  // in_frame: input audio-frame
  // ptr_out: pointer to output audio_frame. If no preprocessing is required
  //          |ptr_out| will be pointing to |in_frame|, otherwise pointing to
  //          |preprocess_frame_|.
  //
  // Return value:
  //   -1: if encountering an error.
  //    0: otherwise.
  int PreprocessToAddData(const AudioFrame& in_frame,
                          const AudioFrame** ptr_out);

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

  int EncodeFragmentation(int fragmentation_index, int payload_type,
                          uint32_t current_timestamp,
                          ACMGenericCodec* encoder,
                          uint8_t* stream);

  void ResetFragmentation(int vector_size);

  AudioPacketizationCallback* packetization_callback_;
  WebRtc_Word32 id_;
  WebRtc_UWord32 last_timestamp_;
  WebRtc_UWord32 last_in_timestamp_;
  CodecInst send_codec_inst_;
  uint8_t cng_nb_pltype_;
  uint8_t cng_wb_pltype_;
  uint8_t cng_swb_pltype_;
  uint8_t cng_fb_pltype_;
  uint8_t red_pltype_;
  bool vad_enabled_;
  bool dtx_enabled_;
  ACMVADMode vad_mode_;
  ACMGenericCodec* codecs_[ACMCodecDB::kMaxNumCodecs];
  ACMGenericCodec* slave_codecs_[ACMCodecDB::kMaxNumCodecs];
  WebRtc_Word16 mirror_codec_idx_[ACMCodecDB::kMaxNumCodecs];
  bool stereo_receive_[ACMCodecDB::kMaxNumCodecs];
  bool stereo_receive_registered_;
  bool stereo_send_;
  int prev_received_channel_;
  int expected_channels_;
  WebRtc_Word32 current_send_codec_idx_;
  int current_receive_codec_idx_;
  bool send_codec_registered_;
  ACMResampler input_resampler_;
  ACMResampler output_resampler_;
  ACMNetEQ neteq_;
  CriticalSectionWrapper* acm_crit_sect_;
  ACMVADCallback* vad_callback_;
  WebRtc_UWord8 last_recv_audio_codec_pltype_;

  // RED/FEC.
  bool is_first_red_;
  bool fec_enabled_;
  // TODO(turajs): |red_buffer_| is allocated in constructor, why having them
  // as pointers and not an array. If concerned about the memory, then make a
  // set-up function to allocate them only when they are going to be used, i.e.
  // FEC or Dual-streaming is enabled.
  WebRtc_UWord8* red_buffer_;
  // TODO(turajs): we actually don't need |fragmentation_| as a member variable.
  // It is sufficient to keep the length & payload type of previous payload in
  // member variables.
  RTPFragmentationHeader fragmentation_;
  WebRtc_UWord32 last_fec_timestamp_;
  // If no RED is registered as receive codec this
  // will have an invalid value.
  WebRtc_UWord8 receive_red_pltype_;

  // This is to keep track of CN instances where we can send DTMFs.
  WebRtc_UWord8 previous_pltype_;

  // This keeps track of payload types associated with codecs_[].
  // We define it as signed variable and initialize with -1 to indicate
  // unused elements.
  WebRtc_Word16 registered_pltypes_[ACMCodecDB::kMaxNumCodecs];

  // Used when payloads are pushed into ACM without any RTP info
  // One example is when pre-encoded bit-stream is pushed from
  // a file.
  WebRtcRTPHeader* dummy_rtp_header_;
  WebRtc_UWord16 recv_pl_frame_size_smpls_;

  bool receiver_initialized_;
  ACMDTMFDetection* dtmf_detector_;

  AudioCodingFeedback* dtmf_callback_;
  WebRtc_Word16 last_detected_tone_;
  CriticalSectionWrapper* callback_crit_sect_;

  AudioFrame audio_frame_;
  AudioFrame preprocess_frame_;
  CodecInst secondary_send_codec_inst_;
  scoped_ptr<ACMGenericCodec> secondary_encoder_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_AUDIO_CODING_MODULE_IMPL_H_
