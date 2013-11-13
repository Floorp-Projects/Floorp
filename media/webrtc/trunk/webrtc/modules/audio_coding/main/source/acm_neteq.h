/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_NETEQ_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_NETEQ_H_

#include "webrtc/common_audio/vad/include/webrtc_vad.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module_typedefs.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class CriticalSectionWrapper;
class RWLockWrapper;
struct CodecInst;

namespace acm1 {

#define MAX_NUM_SLAVE_NETEQ 1

class ACMNetEQ {
 public:
  enum JitterBuffer {
    kMasterJb = 0,
    kSlaveJb = 1
  };

  // Constructor of the class
  ACMNetEQ();

  // Destructor of the class.
  ~ACMNetEQ();

  //
  // Init()
  // Allocates memory for NetEQ and VAD and initializes them.
  //
  // Return value             : 0 if ok.
  //                           -1 if NetEQ or VAD returned an error or
  //                           if out of memory.
  //
  int32_t Init();

  //
  // RecIn()
  // Gives the payload to NetEQ.
  //
  // Input:
  //   - incoming_payload     : Incoming audio payload.
  //   - length_payload       : Length of incoming audio payload.
  //   - rtp_info             : RTP header for the incoming payload containing
  //                            information about payload type, sequence number,
  //                            timestamp, SSRC and marker bit.
  //   - receive_timestamp    : received timestamp.
  //
  // Return value             : 0 if ok.
  //                           <0 if NetEQ returned an error.
  //
  int32_t RecIn(const uint8_t* incoming_payload,
                const int32_t length_payload,
                const WebRtcRTPHeader& rtp_info,
                uint32_t receive_timestamp);

  //
  // RecIn()
  // Insert a sync payload to NetEq. Should only be called if |av_sync_| is
  // enabled;
  //
  // Input:
  //   - rtp_info             : RTP header for the incoming payload containing
  //                            information about payload type, sequence number,
  //                            timestamp, SSRC and marker bit.
  //   - receive_timestamp    : received timestamp.
  //
  // Return value             : 0 if ok.
  //                           <0 if NetEQ returned an error.
  //
  int RecIn(const WebRtcRTPHeader& rtp_info, uint32_t receive_timestamp);

  //
  // RecOut()
  // Asks NetEQ for 10 ms of decoded audio.
  //
  // Input:
  //   -audio_frame           : an audio frame were output data and
  //                            associated parameters are written to.
  //
  // Return value             : 0 if ok.
  //                           -1 if NetEQ returned an error.
  //
  int32_t RecOut(AudioFrame& audio_frame);

  //
  // AddCodec()
  // Adds a new codec to the NetEQ codec database.
  //
  // Input:
  //   - codec_def            : The codec to be added.
  //   - to_master            : true if the codec has to be added to Master
  //                            NetEq, otherwise will be added to the Slave
  //                            NetEQ.
  //
  // Return value             : 0 if ok.
  //                           <0 if NetEQ returned an error.
  //
  int32_t AddCodec(WebRtcNetEQ_CodecDef *codec_def,
                   bool to_master = true);

  //
  // AllocatePacketBuffer()
  // Allocates the NetEQ packet buffer.
  //
  // Input:
  //   - used_codecs          : An array of the codecs to be used by NetEQ.
  //   - num_codecs           : Number of codecs in used_codecs.
  //
  // Return value             : 0 if ok.
  //                           <0 if NetEQ returned an error.
  //
  int32_t AllocatePacketBuffer(const WebRtcNetEQDecoder* used_codecs,
                               int16_t num_codecs);

  //
  // SetAVTPlayout()
  // Enable/disable playout of AVT payloads.
  //
  // Input:
  //   - enable               : Enable if true, disable if false.
  //
  // Return value             : 0 if ok.
  //                           <0 if NetEQ returned an error.
  //
  int32_t SetAVTPlayout(const bool enable);

  //
  // AVTPlayout()
  // Get the current AVT playout state.
  //
  // Return value             : True if AVT playout is enabled.
  //                            False if AVT playout is disabled.
  //
  bool avt_playout() const;

  //
  // CurrentSampFreqHz()
  // Get the current sampling frequency in Hz.
  //
  // Return value             : Sampling frequency in Hz.
  //
  int32_t CurrentSampFreqHz() const;

  //
  // SetPlayoutMode()
  // Sets the playout mode to voice or fax.
  //
  // Input:
  //   - mode                 : The playout mode to be used, voice,
  //                            fax, or streaming.
  //
  // Return value             : 0 if ok.
  //                           <0 if NetEQ returned an error.
  //
  int32_t SetPlayoutMode(const AudioPlayoutMode mode);

  //
  // PlayoutMode()
  // Get the current playout mode.
  //
  // Return value             : The current playout mode.
  //
  AudioPlayoutMode playout_mode() const;

  //
  // NetworkStatistics()
  // Get the current network statistics from NetEQ.
  //
  // Output:
  //   - statistics           : The current network statistics.
  //
  // Return value             : 0 if ok.
  //                           <0 if NetEQ returned an error.
  //
  int32_t NetworkStatistics(ACMNetworkStatistics* statistics) const;

  //
  // VADMode()
  // Get the current VAD Mode.
  //
  // Return value             : The current VAD mode.
  //
  ACMVADMode vad_mode() const;

  //
  // SetVADMode()
  // Set the VAD mode.
  //
  // Input:
  //   - mode                 : The new VAD mode.
  //
  // Return value             : 0 if ok.
  //                           -1 if an error occurred.
  //
  int16_t SetVADMode(const ACMVADMode mode);

  //
  // DecodeLock()
  // Get the decode lock used to protect decoder instances while decoding.
  //
  // Return value             : Pointer to the decode lock.
  //
  RWLockWrapper* DecodeLock() const {
    return decode_lock_;
  }

  //
  // FlushBuffers()
  // Flushes the NetEQ packet and speech buffers.
  //
  // Return value             : 0 if ok.
  //                           -1 if NetEQ returned an error.
  //
  int32_t FlushBuffers();

  //
  // RemoveCodec()
  // Removes a codec from the NetEQ codec database.
  //
  // Input:
  //   - codec_idx            : Codec to be removed.
  //
  // Return value             : 0 if ok.
  //                           -1 if an error occurred.
  //
  int16_t RemoveCodec(WebRtcNetEQDecoder codec_idx,
                      bool is_stereo = false);

  //
  // SetBackgroundNoiseMode()
  // Set the mode of the background noise.
  //
  // Input:
  //   - mode                 : an enumerator specifying the mode of the
  //                            background noise.
  //
  // Return value             : 0 if succeeded,
  //                           -1 if failed to set the mode.
  //
  int16_t SetBackgroundNoiseMode(const ACMBackgroundNoiseMode mode);

  //
  // BackgroundNoiseMode()
  // return the mode of the background noise.
  //
  // Return value             : The mode of background noise.
  //
  int16_t BackgroundNoiseMode(ACMBackgroundNoiseMode& mode);

  void set_id(int32_t id);

  int32_t PlayoutTimestamp(uint32_t& timestamp);

  void set_received_stereo(bool received_stereo);

  uint8_t num_slaves();

  // Delete all slaves.
  void RemoveSlaves();

  int16_t AddSlave(const WebRtcNetEQDecoder* used_codecs,
                   int16_t num_codecs);

  void BufferSpec(int& num_packets, int& size_bytes, int& overhead_bytes) {
    num_packets = min_of_max_num_packets_;
    size_bytes = min_of_buffer_size_bytes_;
    overhead_bytes = per_packet_overhead_bytes_;
  }

  //
  // Set AV-sync mode.
  //
  void EnableAVSync(bool enable);

  //
  // Get sequence number and timestamp of the last decoded RTP.
  //
  bool DecodedRtpInfo(int* sequence_number, uint32_t* timestamp) const;

  //
  // Set a minimum delay in NetEq. Unless channel condition dictates a longer
  // delay, the given delay is maintained by NetEq.
  //
  int SetMinimumDelay(int minimum_delay_ms);

  //
  // Set a maximum delay in NetEq.
  //
  int SetMaximumDelay(int maximum_delay_ms);

  //
  // The shortest latency, in milliseconds, required by jitter buffer. This
  // is computed based on inter-arrival times and playout mode of NetEq. The
  // actual delay is the maximum of least-required-delay and the minimum-delay
  // specified by SetMinumumPlayoutDelay() API.
  //
  int LeastRequiredDelayMs() const ;

 private:
  //
  // RTPPack()
  // Creates a Word16 RTP packet out of the payload data in Word16 and
  // a WebRtcRTPHeader.
  //
  // Input:
  //   - payload              : Payload to be packetized.
  //   - payload_length_bytes : Length of the payload in bytes.
  //   - rtp_info             : RTP header structure.
  //
  // Output:
  //   - rtp_packet           : The RTP packet.
  //
  static void RTPPack(int16_t* rtp_packet, const int8_t* payload,
                      const int32_t payload_length_bytes,
                      const WebRtcRTPHeader& rtp_info);

  void LogError(const char* neteq_func_name, const int16_t idx) const;

  int16_t InitByIdxSafe(const int16_t idx);

  //
  // EnableVAD()
  // Enable VAD.
  //
  // Return value             : 0 if ok.
  //                           -1 if an error occurred.
  //
  int16_t EnableVAD();

  int16_t EnableVADByIdxSafe(const int16_t idx);

  int16_t AllocatePacketBufferByIdxSafe(
      const WebRtcNetEQDecoder* used_codecs,
      int16_t num_codecs,
      const int16_t idx);

  // Delete the NetEQ corresponding to |index|.
  void RemoveNetEQSafe(int index);

  void RemoveSlavesSafe();

  void* inst_[MAX_NUM_SLAVE_NETEQ + 1];
  void* inst_mem_[MAX_NUM_SLAVE_NETEQ + 1];

  int16_t* neteq_packet_buffer_[MAX_NUM_SLAVE_NETEQ + 1];

  int32_t id_;
  float current_samp_freq_khz_;
  bool avt_playout_;
  AudioPlayoutMode playout_mode_;
  CriticalSectionWrapper* neteq_crit_sect_;

  WebRtcVadInst* ptr_vadinst_[MAX_NUM_SLAVE_NETEQ + 1];

  bool vad_status_;
  ACMVADMode vad_mode_;
  RWLockWrapper* decode_lock_;
  bool is_initialized_[MAX_NUM_SLAVE_NETEQ + 1];
  uint8_t num_slaves_;
  bool received_stereo_;
  void* master_slave_info_;
  AudioFrame::VADActivity previous_audio_activity_;

  CriticalSectionWrapper* callback_crit_sect_;
  // Minimum of "max number of packets," among all NetEq instances.
  int min_of_max_num_packets_;
  // Minimum of buffer-size among all NetEq instances.
  int min_of_buffer_size_bytes_;
  int per_packet_overhead_bytes_;

  // Keep track of AV-sync. Just used to set the slave when a slave is added.
  bool av_sync_;

  int minimum_delay_ms_;
  int maximum_delay_ms_;
};

}  // namespace acm1

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_NETEQ_H_
