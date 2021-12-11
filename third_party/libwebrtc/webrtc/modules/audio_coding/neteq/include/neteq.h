/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_INCLUDE_NETEQ_H_
#define MODULES_AUDIO_CODING_NETEQ_INCLUDE_NETEQ_H_

#include <string.h>  // Provide access to size_t.

#include <string>
#include <vector>

#include "api/audio_codecs/audio_decoder.h"
#include "api/optional.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/audio_coding/neteq/neteq_decoder_enum.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

// Forward declarations.
class AudioFrame;
class AudioDecoderFactory;

struct NetEqNetworkStatistics {
  uint16_t current_buffer_size_ms;  // Current jitter buffer size in ms.
  uint16_t preferred_buffer_size_ms;  // Target buffer size in ms.
  uint16_t jitter_peaks_found;  // 1 if adding extra delay due to peaky
                                // jitter; 0 otherwise.
  uint16_t packet_loss_rate;  // Loss rate (network + late) in Q14.
  uint16_t expand_rate;  // Fraction (of original stream) of synthesized
                         // audio inserted through expansion (in Q14).
  uint16_t speech_expand_rate;  // Fraction (of original stream) of synthesized
                                // speech inserted through expansion (in Q14).
  uint16_t preemptive_rate;  // Fraction of data inserted through pre-emptive
                             // expansion (in Q14).
  uint16_t accelerate_rate;  // Fraction of data removed through acceleration
                             // (in Q14).
  uint16_t secondary_decoded_rate;  // Fraction of data coming from FEC/RED
                                    // decoding (in Q14).
  uint16_t secondary_discarded_rate;  // Fraction of discarded FEC/RED data (in
                                      // Q14).
  int32_t clockdrift_ppm;  // Average clock-drift in parts-per-million
                           // (positive or negative).
  size_t added_zero_samples;  // Number of zero samples added in "off" mode.
  // Statistics for packet waiting times, i.e., the time between a packet
  // arrives until it is decoded.
  int mean_waiting_time_ms;
  int median_waiting_time_ms;
  int min_waiting_time_ms;
  int max_waiting_time_ms;
};

// NetEq statistics that persist over the lifetime of the class.
// These metrics are never reset.
struct NetEqLifetimeStatistics {
  // Stats below correspond to similarly-named fields in the WebRTC stats spec.
  // https://w3c.github.io/webrtc-stats/#dom-rtcmediastreamtrackstats
  uint64_t total_samples_received = 0;
  uint64_t concealed_samples = 0;
  uint64_t concealment_events = 0;
  uint64_t jitter_buffer_delay_ms = 0;
};

enum NetEqPlayoutMode {
  kPlayoutOn,
  kPlayoutOff,
  kPlayoutFax,
  kPlayoutStreaming
};

// This is the interface class for NetEq.
class NetEq {
 public:
  enum BackgroundNoiseMode {
    kBgnOn,    // Default behavior with eternal noise.
    kBgnFade,  // Noise fades to zero after some time.
    kBgnOff    // Background noise is always zero.
  };

  struct Config {
    Config()
        : sample_rate_hz(16000),
          enable_post_decode_vad(false),
          max_packets_in_buffer(50),
          // |max_delay_ms| has the same effect as calling SetMaximumDelay().
          max_delay_ms(2000),
          background_noise_mode(kBgnOff),
          playout_mode(kPlayoutOn),
          enable_fast_accelerate(false) {}

    std::string ToString() const;

    int sample_rate_hz;  // Initial value. Will change with input data.
    bool enable_post_decode_vad;
    size_t max_packets_in_buffer;
    int max_delay_ms;
    BackgroundNoiseMode background_noise_mode;
    NetEqPlayoutMode playout_mode;
    bool enable_fast_accelerate;
    bool enable_muted_state = false;
  };

  enum ReturnCodes {
    kOK = 0,
    kFail = -1,
    kNotImplemented = -2
  };

  // Creates a new NetEq object, with parameters set in |config|. The |config|
  // object will only have to be valid for the duration of the call to this
  // method.
  static NetEq* Create(
      const NetEq::Config& config,
      const rtc::scoped_refptr<AudioDecoderFactory>& decoder_factory);

  virtual ~NetEq() {}

  // Inserts a new packet into NetEq. The |receive_timestamp| is an indication
  // of the time when the packet was received, and should be measured with
  // the same tick rate as the RTP timestamp of the current payload.
  // Returns 0 on success, -1 on failure.
  virtual int InsertPacket(const RTPHeader& rtp_header,
                           rtc::ArrayView<const uint8_t> payload,
                           uint32_t receive_timestamp) = 0;

  // Lets NetEq know that a packet arrived with an empty payload. This typically
  // happens when empty packets are used for probing the network channel, and
  // these packets use RTP sequence numbers from the same series as the actual
  // audio packets.
  virtual void InsertEmptyPacket(const RTPHeader& rtp_header) = 0;

  // Instructs NetEq to deliver 10 ms of audio data. The data is written to
  // |audio_frame|. All data in |audio_frame| is wiped; |data_|, |speech_type_|,
  // |num_channels_|, |sample_rate_hz_|, |samples_per_channel_|, and
  // |vad_activity_| are updated upon success. If an error is returned, some
  // fields may not have been updated, or may contain inconsistent values.
  // If muted state is enabled (through Config::enable_muted_state), |muted|
  // may be set to true after a prolonged expand period. When this happens, the
  // |data_| in |audio_frame| is not written, but should be interpreted as being
  // all zeros.
  // Returns kOK on success, or kFail in case of an error.
  virtual int GetAudio(AudioFrame* audio_frame, bool* muted) = 0;

  // Replaces the current set of decoders with the given one.
  virtual void SetCodecs(const std::map<int, SdpAudioFormat>& codecs) = 0;

  // Associates |rtp_payload_type| with |codec| and |codec_name|, and stores the
  // information in the codec database. Returns 0 on success, -1 on failure.
  // The name is only used to provide information back to the caller about the
  // decoders. Hence, the name is arbitrary, and may be empty.
  virtual int RegisterPayloadType(NetEqDecoder codec,
                                  const std::string& codec_name,
                                  uint8_t rtp_payload_type) = 0;

  // Provides an externally created decoder object |decoder| to insert in the
  // decoder database. The decoder implements a decoder of type |codec| and
  // associates it with |rtp_payload_type| and |codec_name|. Returns kOK on
  // success, kFail on failure. The name is only used to provide information
  // back to the caller about the decoders. Hence, the name is arbitrary, and
  // may be empty.
  virtual int RegisterExternalDecoder(AudioDecoder* decoder,
                                      NetEqDecoder codec,
                                      const std::string& codec_name,
                                      uint8_t rtp_payload_type) = 0;

  // Associates |rtp_payload_type| with the given codec, which NetEq will
  // instantiate when it needs it. Returns true iff successful.
  virtual bool RegisterPayloadType(int rtp_payload_type,
                                   const SdpAudioFormat& audio_format) = 0;

  // Removes |rtp_payload_type| from the codec database. Returns 0 on success,
  // -1 on failure. Removing a payload type that is not registered is ok and
  // will not result in an error.
  virtual int RemovePayloadType(uint8_t rtp_payload_type) = 0;

  // Removes all payload types from the codec database.
  virtual void RemoveAllPayloadTypes() = 0;

  // Sets a minimum delay in millisecond for packet buffer. The minimum is
  // maintained unless a higher latency is dictated by channel condition.
  // Returns true if the minimum is successfully applied, otherwise false is
  // returned.
  virtual bool SetMinimumDelay(int delay_ms) = 0;

  // Sets a maximum delay in milliseconds for packet buffer. The latency will
  // not exceed the given value, even required delay (given the channel
  // conditions) is higher. Calling this method has the same effect as setting
  // the |max_delay_ms| value in the NetEq::Config struct.
  virtual bool SetMaximumDelay(int delay_ms) = 0;

  // The smallest latency required. This is computed bases on inter-arrival
  // time and internal NetEq logic. Note that in computing this latency none of
  // the user defined limits (applied by calling setMinimumDelay() and/or
  // SetMaximumDelay()) are applied.
  virtual int LeastRequiredDelayMs() const = 0;

  // Not implemented.
  virtual int SetTargetDelay() = 0;

  // Returns the current target delay in ms. This includes any extra delay
  // requested through SetMinimumDelay.
  virtual int TargetDelayMs() const = 0;

  // Returns the current total delay (packet buffer and sync buffer) in ms.
  virtual int CurrentDelayMs() const = 0;

  // Returns the current total delay (packet buffer and sync buffer) in ms,
  // with smoothing applied to even out short-time fluctuations due to jitter.
  // The packet buffer part of the delay is not updated during DTX/CNG periods.
  virtual int FilteredCurrentDelayMs() const = 0;

  // Sets the playout mode to |mode|.
  // Deprecated. Set the mode in the Config struct passed to the constructor.
  // TODO(henrik.lundin) Delete.
  virtual void SetPlayoutMode(NetEqPlayoutMode mode) = 0;

  // Returns the current playout mode.
  // Deprecated.
  // TODO(henrik.lundin) Delete.
  virtual NetEqPlayoutMode PlayoutMode() const = 0;

  // Writes the current network statistics to |stats|. The statistics are reset
  // after the call.
  virtual int NetworkStatistics(NetEqNetworkStatistics* stats) = 0;

  // Returns a copy of this class's lifetime statistics. These statistics are
  // never reset.
  virtual NetEqLifetimeStatistics GetLifetimeStatistics() const = 0;

  // Writes the current RTCP statistics to |stats|. The statistics are reset
  // and a new report period is started with the call.
  virtual void GetRtcpStatistics(RtcpStatistics* stats) = 0;

  // Same as RtcpStatistics(), but does not reset anything.
  virtual void GetRtcpStatisticsNoReset(RtcpStatistics* stats) = 0;

  // Enables post-decode VAD. When enabled, GetAudio() will return
  // kOutputVADPassive when the signal contains no speech.
  virtual void EnableVad() = 0;

  // Disables post-decode VAD.
  virtual void DisableVad() = 0;

  // Returns the RTP timestamp for the last sample delivered by GetAudio().
  // The return value will be empty if no valid timestamp is available.
  virtual rtc::Optional<uint32_t> GetPlayoutTimestamp() const = 0;

  // Returns the sample rate in Hz of the audio produced in the last GetAudio
  // call. If GetAudio has not been called yet, the configured sample rate
  // (Config::sample_rate_hz) is returned.
  virtual int last_output_sample_rate_hz() const = 0;

  // Returns info about the decoder for the given payload type, or an empty
  // value if we have no decoder for that payload type.
  virtual rtc::Optional<CodecInst> GetDecoder(int payload_type) const = 0;

  // Returns the decoder format for the given payload type. Returns empty if no
  // such payload type was registered.
  virtual rtc::Optional<SdpAudioFormat> GetDecoderFormat(
      int payload_type) const = 0;

  // Not implemented.
  virtual int SetTargetNumberOfChannels() = 0;

  // Not implemented.
  virtual int SetTargetSampleRate() = 0;

  // Flushes both the packet buffer and the sync buffer.
  virtual void FlushBuffers() = 0;

  // Current usage of packet-buffer and it's limits.
  virtual void PacketBufferStatistics(int* current_num_packets,
                                      int* max_num_packets) const = 0;

  // Enables NACK and sets the maximum size of the NACK list, which should be
  // positive and no larger than Nack::kNackListSizeLimit. If NACK is already
  // enabled then the maximum NACK list size is modified accordingly.
  virtual void EnableNack(size_t max_nack_list_size) = 0;

  virtual void DisableNack() = 0;

  // Returns a list of RTP sequence numbers corresponding to packets to be
  // retransmitted, given an estimate of the round-trip time in milliseconds.
  virtual std::vector<uint16_t> GetNackList(
      int64_t round_trip_time_ms) const = 0;

  // Returns a vector containing the timestamps of the packets that were decoded
  // in the last GetAudio call. If no packets were decoded in the last call, the
  // vector is empty.
  // Mainly intended for testing.
  virtual std::vector<uint32_t> LastDecodedTimestamps() const = 0;

  // Returns the length of the audio yet to play in the sync buffer.
  // Mainly intended for testing.
  virtual int SyncBufferSizeMs() const = 0;

 protected:
  NetEq() {}

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(NetEq);
};

}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_INCLUDE_NETEQ_H_
