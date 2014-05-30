/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_RECEIVER_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_RECEIVER_H_

#include <vector>

#include "webrtc/common_audio/vad/include/webrtc_vad.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_codec_database.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_resampler.h"
#include "webrtc/modules/audio_coding/main/acm2/call_statistics.h"
#include "webrtc/modules/audio_coding/main/acm2/initial_delay_manager.h"
#include "webrtc/modules/audio_coding/neteq4/interface/neteq.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

struct CodecInst;
class CriticalSectionWrapper;
class RWLockWrapper;
class NetEq;

namespace acm2 {

class Nack;

class AcmReceiver {
 public:
  struct Decoder {
    bool registered;
    uint8_t payload_type;
    // This field is meaningful for codecs where both mono and
    // stereo versions are registered under the same ID.
    int channels;
  };

  // Constructor of the class
  AcmReceiver();

  explicit AcmReceiver(NetEq* neteq);

  // Destructor of the class.
  ~AcmReceiver();

  //
  // Inserts a payload with its associated RTP-header into NetEq.
  //
  // Input:
  //   - rtp_header           : RTP header for the incoming payload containing
  //                            information about payload type, sequence number,
  //                            timestamp, SSRC and marker bit.
  //   - incoming_payload     : Incoming audio payload.
  //   - length_payload       : Length of incoming audio payload in bytes.
  //
  // Return value             : 0 if OK.
  //                           <0 if NetEq returned an error.
  //
  int InsertPacket(const WebRtcRTPHeader& rtp_header,
                   const uint8_t* incoming_payload,
                   int length_payload);

  //
  // Asks NetEq for 10 milliseconds of decoded audio.
  //
  // Input:
  //   -desired_freq_hz       : specifies the sampling rate [Hz] of the output
  //                            audio. If set -1 indicates to resampling is
  //                            is required and the audio returned at the
  //                            sampling rate of the decoder.
  //
  // Output:
  //   -audio_frame           : an audio frame were output data and
  //                            associated parameters are written to.
  //
  // Return value             : 0 if OK.
  //                           -1 if NetEq returned an error.
  //
  int GetAudio(int desired_freq_hz, AudioFrame* audio_frame);

  //
  // Adds a new codec to the NetEq codec database.
  //
  // Input:
  //   - acm_codec_id        : ACM codec ID.
  //   - payload_type        : payload type.
  //   - audio_decoder       : pointer to a decoder object. If it is NULL
  //                           then NetEq will internally create the decoder
  //                           object. Otherwise, NetEq will store this pointer
  //                           as the decoder corresponding with the given
  //                           payload type. NetEq won't acquire the ownership
  //                           of this pointer. It is up to the client of this
  //                           class (ACM) to delete it. By providing
  //                           |audio_decoder| ACM will have control over the
  //                           decoder instance of the codec. This is essential
  //                           for a codec like iSAC which encoder/decoder
  //                           encoder has to know about decoder (bandwidth
  //                           estimator that is updated at decoding time).
  //
  // Return value             : 0 if OK.
  //                           <0 if NetEq returned an error.
  //
  int AddCodec(int acm_codec_id,
               uint8_t payload_type,
               int channels,
               AudioDecoder* audio_decoder);

  //
  // Sets a minimum delay for packet buffer. The given delay is maintained,
  // unless channel condition dictates a higher delay.
  //
  // Input:
  //   - delay_ms             : minimum delay in milliseconds.
  //
  // Return value             : 0 if OK.
  //                           <0 if NetEq returned an error.
  //
  int SetMinimumDelay(int delay_ms);

  //
  // Sets a maximum delay [ms] for the packet buffer. The target delay does not
  // exceed the given value, even if channel condition requires so.
  //
  // Input:
  //   - delay_ms             : maximum delay in milliseconds.
  //
  // Return value             : 0 if OK.
  //                           <0 if NetEq returned an error.
  //
  int SetMaximumDelay(int delay_ms);

  //
  // Get least required delay computed based on channel conditions. Note that
  // this is before applying any user-defined limits (specified by calling
  // (SetMinimumDelay() and/or SetMaximumDelay()).
  //
  int LeastRequiredDelayMs() const;

  //
  // Sets an initial delay of |delay_ms| milliseconds. This introduces a playout
  // delay. Silence (zero signal) is played out until equivalent of |delay_ms|
  // millisecond of audio is buffered. Then, NetEq maintains the delay.
  //
  // Input:
  //   - delay_ms             : initial delay in milliseconds.
  //
  // Return value             : 0 if OK.
  //                           <0 if NetEq returned an error.
  //
  int SetInitialDelay(int delay_ms);

  //
  // Resets the initial delay to zero.
  //
  void ResetInitialDelay();

  //
  // Get the current sampling frequency in Hz.
  //
  // Return value             : Sampling frequency in Hz.
  //
  int current_sample_rate_hz() const;

  //
  // Sets the playout mode.
  //
  // Input:
  //   - mode                 : an enumerator specifying the playout mode.
  //
  void SetPlayoutMode(AudioPlayoutMode mode);

  //
  // Get the current playout mode.
  //
  // Return value             : The current playout mode.
  //
  AudioPlayoutMode PlayoutMode() const;

  //
  // Get the current network statistics from NetEq.
  //
  // Output:
  //   - statistics           : The current network statistics.
  //
  void NetworkStatistics(ACMNetworkStatistics* statistics);

  //
  // Enable post-decoding VAD.
  //
  void EnableVad();

  //
  // Disable post-decoding VAD.
  //
  void DisableVad();

  //
  // Returns whether post-decoding VAD is enabled (true) or disabled (false).
  //
  bool vad_enabled() const { return vad_enabled_; }

  //
  // Get the decode lock used to protect decoder instances while decoding.
  //
  // Return value             : Pointer to the decode lock.
  //
  RWLockWrapper* DecodeLock() const { return decode_lock_; }

  //
  // Flushes the NetEq packet and speech buffers.
  //
  void FlushBuffers();

  //
  // Removes a payload-type from the NetEq codec database.
  //
  // Input:
  //   - payload_type         : the payload-type to be removed.
  //
  // Return value             : 0 if OK.
  //                           -1 if an error occurred.
  //
  int RemoveCodec(uint8_t payload_type);

  //
  // Remove all registered codecs.
  //
  int RemoveAllCodecs();

  //
  // Set ID.
  //
  void set_id(int id);  // TODO(turajs): can be inline.

  //
  // Returns the RTP timestamp of the last sample delivered by GetAudio().
  //
  uint32_t PlayoutTimestamp();

  //
  // Return the index of the codec associated with the last non-CNG/non-DTMF
  // received payload. If no non-CNG/non-DTMF payload is received -1 is
  // returned.
  //
  int last_audio_codec_id() const;  // TODO(turajs): can be inline.

  //
  // Return the payload-type of the last non-CNG/non-DTMF RTP packet. If no
  // non-CNG/non-DTMF packet is received -1 is returned.
  //
  int last_audio_payload_type() const;  // TODO(turajs): can be inline.

  //
  // Get the audio codec associated with the last non-CNG/non-DTMF received
  // payload. If no non-CNG/non-DTMF packet is received -1 is returned,
  // otherwise return 0.
  //
  int LastAudioCodec(CodecInst* codec) const;

  //
  // Return payload type of RED if it is registered, otherwise return -1;
  //
  int RedPayloadType() const;

  //
  // Get a decoder given its registered payload-type.
  //
  // Input:
  //    -payload_type         : the payload-type of the codec to be retrieved.
  //
  // Output:
  //    -codec                : codec associated with the given payload-type.
  //
  // Return value             : 0 if succeeded.
  //                           -1 if failed, e.g. given payload-type is not
  //                              registered.
  //
  int DecoderByPayloadType(uint8_t payload_type,
                           CodecInst* codec) const;

  //
  // Enable NACK and set the maximum size of the NACK list. If NACK is already
  // enabled then the maximum NACK list size is modified accordingly.
  //
  // Input:
  //    -max_nack_list_size  : maximum NACK list size
  //                           should be positive (none zero) and less than or
  //                           equal to |Nack::kNackListSizeLimit|
  // Return value
  //                         : 0 if succeeded.
  //                          -1 if failed
  //
  int EnableNack(size_t max_nack_list_size);

  // Disable NACK.
  void DisableNack();

  //
  // Get a list of packets to be retransmitted.
  //
  // Input:
  //    -round_trip_time_ms : estimate of the round-trip-time (in milliseconds).
  // Return value           : list of packets to be retransmitted.
  //
  std::vector<uint16_t> GetNackList(int round_trip_time_ms) const;

  //
  // Returns the background noise mode. This is only for testing and ACM is not
  // calling this function. Used in acm_receiver_unittest.cc.
  //
  NetEqBackgroundNoiseMode BackgroundNoiseModeForTest() const;

  //
  // Get statistics of calls to GetAudio().
  void GetDecodingCallStatistics(AudioDecodingCallStats* stats) const;

 private:
  int PayloadType2CodecIndex(uint8_t payload_type) const;

  bool GetSilence(int desired_sample_rate_hz, AudioFrame* frame);

  int GetNumSyncPacketToInsert(uint16_t received_squence_number);

  int RtpHeaderToCodecIndex(
      const RTPHeader& rtp_header, const uint8_t* payload) const;

  uint32_t NowInTimestamp(int decoder_sampling_rate) const;

  void InsertStreamOfSyncPackets(InitialDelayManager::SyncStream* sync_stream);

  int id_;
  NetEq* neteq_;
  Decoder decoders_[ACMCodecDB::kMaxNumCodecs];
  int last_audio_decoder_;
  RWLockWrapper* decode_lock_;
  CriticalSectionWrapper* neteq_crit_sect_;
  bool vad_enabled_;
  AudioFrame::VADActivity previous_audio_activity_;
  int current_sample_rate_hz_;
  ACMResampler resampler_;
  // Used in GetAudio, declared as member to avoid allocating every 10ms.
  int16_t audio_buffer_[AudioFrame::kMaxDataSizeSamples];
  scoped_ptr<Nack> nack_;
  bool nack_enabled_;

  // Indicates if a non-zero initial delay is set, and the receiver is in
  // AV-sync mode.
  bool av_sync_;
  scoped_ptr<InitialDelayManager> initial_delay_manager_;

  // The following are defined as members to avoid creating them in every
  // iteration. |missing_packets_sync_stream_| is *ONLY* used in InsertPacket().
  // |late_packets_sync_stream_| is only used in GetAudio(). Both of these
  // member variables are allocated only when we AV-sync is enabled, i.e.
  // initial delay is set.
  scoped_ptr<InitialDelayManager::SyncStream> missing_packets_sync_stream_;
  scoped_ptr<InitialDelayManager::SyncStream> late_packets_sync_stream_;

  CallStatistics call_stats_;
};

}  // namespace acm2

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_RECEIVER_H_
