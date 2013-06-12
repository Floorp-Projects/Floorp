/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_NETEQ4_NETEQ_IMPL_H_
#define WEBRTC_MODULES_AUDIO_CODING_NETEQ4_NETEQ_IMPL_H_

#include <vector>

#include "webrtc/modules/audio_coding/neteq4/audio_multi_vector.h"
#include "webrtc/modules/audio_coding/neteq4/defines.h"
#include "webrtc/modules/audio_coding/neteq4/interface/neteq.h"
#include "webrtc/modules/audio_coding/neteq4/packet.h"  // Declare PacketList.
#include "webrtc/modules/audio_coding/neteq4/random_vector.h"
#include "webrtc/modules/audio_coding/neteq4/rtcp.h"
#include "webrtc/modules/audio_coding/neteq4/statistics_calculator.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Forward declarations.
class BackgroundNoise;
class BufferLevelFilter;
class ComfortNoise;
class CriticalSectionWrapper;
class DecisionLogic;
class DecoderDatabase;
class DelayManager;
class DelayPeakDetector;
class DtmfBuffer;
class DtmfToneGenerator;
class Expand;
class PacketBuffer;
class PayloadSplitter;
class PostDecodeVad;
class RandomVector;
class SyncBuffer;
class TimestampScaler;
struct DtmfEvent;

class NetEqImpl : public webrtc::NetEq {
 public:
  // Creates a new NetEqImpl object. The object will assume ownership of all
  // injected dependencies, and will delete them when done.
  NetEqImpl(int fs,
            BufferLevelFilter* buffer_level_filter,
            DecoderDatabase* decoder_database,
            DelayManager* delay_manager,
            DelayPeakDetector* delay_peak_detector,
            DtmfBuffer* dtmf_buffer,
            DtmfToneGenerator* dtmf_tone_generator,
            PacketBuffer* packet_buffer,
            PayloadSplitter* payload_splitter,
            TimestampScaler* timestamp_scaler);

  virtual ~NetEqImpl();

  // Inserts a new packet into NetEq. The |receive_timestamp| is an indication
  // of the time when the packet was received, and should be measured with
  // the same tick rate as the RTP timestamp of the current payload.
  // Returns 0 on success, -1 on failure.
  virtual int InsertPacket(const WebRtcRTPHeader& rtp_header,
                           const uint8_t* payload,
                           int length_bytes,
                           uint32_t receive_timestamp);

  // Instructs NetEq to deliver 10 ms of audio data. The data is written to
  // |output_audio|, which can hold (at least) |max_length| elements.
  // The number of channels that were written to the output is provided in
  // the output variable |num_channels|, and each channel contains
  // |samples_per_channel| elements. If more than one channel is written,
  // the samples are interleaved.
  // The speech type is written to |type|, if |type| is not NULL.
  // Returns kOK on success, or kFail in case of an error.
  virtual int GetAudio(size_t max_length, int16_t* output_audio,
                       int* samples_per_channel, int* num_channels,
                       NetEqOutputType* type);

  // Associates |rtp_payload_type| with |codec| and stores the information in
  // the codec database. Returns kOK on success, kFail on failure.
  virtual int RegisterPayloadType(enum NetEqDecoder codec,
                                  uint8_t rtp_payload_type);

  // Provides an externally created decoder object |decoder| to insert in the
  // decoder database. The decoder implements a decoder of type |codec| and
  // associates it with |rtp_payload_type|. The decoder operates at the
  // frequency |sample_rate_hz|. Returns kOK on success, kFail on failure.
  virtual int RegisterExternalDecoder(AudioDecoder* decoder,
                                      enum NetEqDecoder codec,
                                      int sample_rate_hz,
                                      uint8_t rtp_payload_type);

  // Removes |rtp_payload_type| from the codec database. Returns 0 on success,
  // -1 on failure.
  virtual int RemovePayloadType(uint8_t rtp_payload_type);

  // Sets the desired extra delay on top of what NetEq already applies due to
  // current network situation. Used for synchronization with video. Returns
  // true if successful, otherwise false.
  virtual bool SetExtraDelay(int extra_delay_ms);

  virtual int SetTargetDelay() { return kNotImplemented; }

  virtual int TargetDelay() { return kNotImplemented; }

  virtual int CurrentDelay() { return kNotImplemented; }

  // Enables playout of DTMF tones.
  virtual int EnableDtmf();

  // Sets the playout mode to |mode|.
  virtual void SetPlayoutMode(NetEqPlayoutMode mode);

  // Returns the current playout mode.
  virtual NetEqPlayoutMode PlayoutMode() const;

  // Writes the current network statistics to |stats|. The statistics are reset
  // after the call.
  virtual int NetworkStatistics(NetEqNetworkStatistics* stats);

  // Writes the last packet waiting times (in ms) to |waiting_times|. The number
  // of values written is no more than 100, but may be smaller if the interface
  // is polled again before 100 packets has arrived.
  virtual void WaitingTimes(std::vector<int>* waiting_times);

  // Writes the current RTCP statistics to |stats|. The statistics are reset
  // and a new report period is started with the call.
  virtual void GetRtcpStatistics(RtcpStatistics* stats);

  // Same as RtcpStatistics(), but does not reset anything.
  virtual void GetRtcpStatisticsNoReset(RtcpStatistics* stats);

  // Enables post-decode VAD. When enabled, GetAudio() will return
  // kOutputVADPassive when the signal contains no speech.
  virtual void EnableVad();

  // Disables post-decode VAD.
  virtual void DisableVad();

  // Returns the RTP timestamp for the last sample delivered by GetAudio().
  virtual uint32_t PlayoutTimestamp();

  virtual int SetTargetNumberOfChannels() { return kNotImplemented; }

  virtual int SetTargetSampleRate() { return kNotImplemented; }

  // Returns the error code for the last occurred error. If no error has
  // occurred, 0 is returned.
  virtual int LastError();

  // Returns the error code last returned by a decoder (audio or comfort noise).
  // When LastError() returns kDecoderErrorCode or kComfortNoiseErrorCode, check
  // this method to get the decoder's error code.
  virtual int LastDecoderError();

  // Flushes both the packet buffer and the sync buffer.
  virtual void FlushBuffers();

 private:
  static const int kOutputSizeMs = 10;
  static const int kMaxFrameSize = 2880;  // 60 ms @ 48 kHz.
  // TODO(hlundin): Provide a better value for kSyncBufferSize.
  static const int kSyncBufferSize = 2 * kMaxFrameSize;

  // Inserts a new packet into NetEq. This is used by the InsertPacket method
  // above. Returns 0 on success, otherwise an error code.
  // TODO(hlundin): Merge this with InsertPacket above?
  int InsertPacketInternal(const WebRtcRTPHeader& rtp_header,
                           const uint8_t* payload,
                           int length_bytes,
                           uint32_t receive_timestamp);


  // Delivers 10 ms of audio data. The data is written to |output|, which can
  // hold (at least) |max_length| elements. The number of channels that were
  // written to the output is provided in the output variable |num_channels|,
  // and each channel contains |samples_per_channel| elements. If more than one
  // channel is written, the samples are interleaved.
  // Returns 0 on success, otherwise an error code.
  int GetAudioInternal(size_t max_length, int16_t* output,
                       int* samples_per_channel, int* num_channels);


  // Provides a decision to the GetAudioInternal method. The decision what to
  // do is written to |operation|. Packets to decode are written to
  // |packet_list|, and a DTMF event to play is written to |dtmf_event|. When
  // DTMF should be played, |play_dtmf| is set to true by the method.
  // Returns 0 on success, otherwise an error code.
  int GetDecision(Operations* operation,
                  PacketList* packet_list,
                  DtmfEvent* dtmf_event,
                  bool* play_dtmf);

  // Decodes the speech packets in |packet_list|, and writes the results to
  // |decoded_buffer|, which is allocated to hold |decoded_buffer_length|
  // elements. The length of the decoded data is written to |decoded_length|.
  // The speech type -- speech or (codec-internal) comfort noise -- is written
  // to |speech_type|. If |packet_list| contains any SID frames for RFC 3389
  // comfort noise, those are not decoded.
  int Decode(PacketList* packet_list, Operations* operation,
             int* decoded_length, AudioDecoder::SpeechType* speech_type);

  // Sub-method to Decode(). Performs the actual decoding.
  int DecodeLoop(PacketList* packet_list, Operations* operation,
                 AudioDecoder* decoder, int* decoded_length,
                 AudioDecoder::SpeechType* speech_type);

  // Sub-method which calls the Normal class to perform the normal operation.
  void DoNormal(const int16_t* decoded_buffer, size_t decoded_length,
                AudioDecoder::SpeechType speech_type, bool play_dtmf,
                AudioMultiVector<int16_t>* algorithm_buffer);

  // Sub-method which calls the Merge class to perform the merge operation.
  void DoMerge(int16_t* decoded_buffer, size_t decoded_length,
               AudioDecoder::SpeechType speech_type, bool play_dtmf,
               AudioMultiVector<int16_t>* algorithm_buffer);

  // Sub-method which calls the Expand class to perform the expand operation.
  int DoExpand(bool play_dtmf, AudioMultiVector<int16_t>* algorithm_buffer);

  // Sub-method which calls the Accelerate class to perform the accelerate
  // operation.
  int DoAccelerate(int16_t* decoded_buffer, size_t decoded_length,
                   AudioDecoder::SpeechType speech_type, bool play_dtmf,
                   AudioMultiVector<int16_t>* algorithm_buffer);

  // Sub-method which calls the PreemptiveExpand class to perform the
  // preemtive expand operation.
  int DoPreemptiveExpand(int16_t* decoded_buffer, size_t decoded_length,
                         AudioDecoder::SpeechType speech_type, bool play_dtmf,
                         AudioMultiVector<int16_t>* algorithm_buffer);

  // Sub-method which calls the ComfortNoise class to generate RFC 3389 comfort
  // noise. |packet_list| can either contain one SID frame to update the
  // noise parameters, or no payload at all, in which case the previously
  // received parameters are used.
  int DoRfc3389Cng(PacketList* packet_list, bool play_dtmf,
                   AudioMultiVector<int16_t>* algorithm_buffer);

  // Calls the audio decoder to generate codec-internal comfort noise when
  // no packet was received.
  void DoCodecInternalCng(AudioMultiVector<int16_t>* algorithm_buffer);

  // Calls the DtmfToneGenerator class to generate DTMF tones.
  int DoDtmf(const DtmfEvent& dtmf_event, bool* play_dtmf,
             AudioMultiVector<int16_t>* algorithm_buffer);

  // Produces packet-loss concealment using alternative methods. If the codec
  // has an internal PLC, it is called to generate samples. Otherwise, the
  // method performs zero-stuffing.
  void DoAlternativePlc(bool increase_timestamp,
                        AudioMultiVector<int16_t>* algorithm_buffer);

  // Overdub DTMF on top of |output|.
  int DtmfOverdub(const DtmfEvent& dtmf_event, size_t num_channels,
                  int16_t* output) const;

  // Extracts packets from |packet_buffer_| to produce at least
  // |required_samples| samples. The packets are inserted into |packet_list|.
  // Returns the number of samples that the packets in the list will produce, or
  // -1 in case of an error.
  int ExtractPackets(int required_samples, PacketList* packet_list);

  // Resets various variables and objects to new values based on the sample rate
  // |fs_hz| and |channels| number audio channels.
  void SetSampleRateAndChannels(int fs_hz, size_t channels);

  // Returns the output type for the audio produced by the latest call to
  // GetAudio().
  NetEqOutputType LastOutputType();

  BackgroundNoise* background_noise_;
  scoped_ptr<BufferLevelFilter> buffer_level_filter_;
  scoped_ptr<DecoderDatabase> decoder_database_;
  scoped_ptr<DelayManager> delay_manager_;
  scoped_ptr<DelayPeakDetector> delay_peak_detector_;
  scoped_ptr<DtmfBuffer> dtmf_buffer_;
  scoped_ptr<DtmfToneGenerator> dtmf_tone_generator_;
  scoped_ptr<PacketBuffer> packet_buffer_;
  scoped_ptr<PayloadSplitter> payload_splitter_;
  scoped_ptr<TimestampScaler> timestamp_scaler_;
  scoped_ptr<DecisionLogic> decision_logic_;
  scoped_ptr<PostDecodeVad> vad_;
  SyncBuffer* sync_buffer_;
  Expand* expand_;
  RandomVector random_vector_;
  ComfortNoise* comfort_noise_;
  Rtcp rtcp_;
  StatisticsCalculator stats_;
  int fs_hz_;
  int fs_mult_;
  int output_size_samples_;
  int decoder_frame_length_;
  Modes last_mode_;
  scoped_array<int16_t> mute_factor_array_;
  size_t decoded_buffer_length_;
  scoped_array<int16_t> decoded_buffer_;
  uint32_t playout_timestamp_;
  bool new_codec_;
  uint32_t timestamp_;
  bool reset_decoder_;
  uint8_t current_rtp_payload_type_;
  uint8_t current_cng_rtp_payload_type_;
  uint32_t ssrc_;
  bool first_packet_;
  bool dtmf_enabled_;
  int error_code_;  // Store last error code.
  int decoder_error_code_;
  CriticalSectionWrapper* crit_sect_;

  DISALLOW_COPY_AND_ASSIGN(NetEqImpl);
};

}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_NETEQ4_NETEQ_IMPL_H_
