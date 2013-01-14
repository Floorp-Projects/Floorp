/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_JITTER_BUFFER_H_
#define WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_JITTER_BUFFER_H_

#include <list>

#include "modules/interface/module_common_types.h"
#include "modules/video_coding/main/interface/video_coding_defines.h"
#include "modules/video_coding/main/source/decoding_state.h"
#include "modules/video_coding/main/source/event.h"
#include "modules/video_coding/main/source/inter_frame_delay.h"
#include "modules/video_coding/main/source/jitter_buffer_common.h"
#include "modules/video_coding/main/source/jitter_estimator.h"
#include "system_wrappers/interface/constructor_magic.h"
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "typedefs.h"

namespace webrtc {

enum VCMNackMode {
  kNackInfinite,
  kNackHybrid,
  kNoNack
};

typedef std::list<VCMFrameBuffer*> FrameList;

// forward declarations
class TickTimeBase;
class VCMFrameBuffer;
class VCMPacket;
class VCMEncodedFrame;

struct VCMJitterSample {
  VCMJitterSample() : timestamp(0), frame_size(0), latest_packet_time(-1) {}
  uint32_t timestamp;
  uint32_t frame_size;
  int64_t latest_packet_time;
};

class VCMJitterBuffer {
 public:
  VCMJitterBuffer(TickTimeBase* clock, int vcm_id = -1, int receiver_id = -1,
                  bool master = true);
  virtual ~VCMJitterBuffer();

  // Makes |this| a deep copy of |rhs|.
  void CopyFrom(const VCMJitterBuffer& rhs);

  // Initializes and starts jitter buffer.
  void Start();

  // Signals all internal events and stops the jitter buffer.
  void Stop();

  // Returns true if the jitter buffer is running.
  bool Running() const;

  // Empty the jitter buffer of all its data.
  void Flush();

  // Get the number of received key and delta frames since the jitter buffer
  // was started.
  void FrameStatistics(uint32_t* received_delta_frames,
                       uint32_t* received_key_frames) const;

  // The number of packets discarded by the jitter buffer because the decoder
  // won't be able to decode them.
  int num_not_decodable_packets() const;

  // Gets number of packets discarded by the jitter buffer.
  int num_discarded_packets() const;

  // Statistics, Calculate frame and bit rates.
  void IncomingRateStatistics(unsigned int* framerate,
                              unsigned int* bitrate);

  // Waits for the first packet in the next frame to arrive and then returns
  // the timestamp of that frame. |incoming_frame_type| and |render_time_ms| are
  // set to the frame type and render time of the next frame.
  // Blocks for up to |max_wait_time_ms| ms. Returns -1 if no packet has arrived
  // after |max_wait_time_ms| ms.
  int64_t NextTimestamp(uint32_t max_wait_time_ms,
                        FrameType* incoming_frame_type,
                        int64_t* render_time_ms);

  // Checks if the packet sequence will be complete if the next frame would be
  // grabbed for decoding. That is, if a frame has been lost between the
  // last decoded frame and the next, or if the next frame is missing one
  // or more packets.
  bool CompleteSequenceWithNextFrame();

  // TODO(mikhal/stefan): Merge all GetFrameForDecoding into one.
  // Wait |max_wait_time_ms| for a complete frame to arrive. After timeout NULL
  // is returned.
  VCMEncodedFrame* GetCompleteFrameForDecoding(uint32_t max_wait_time_ms);

  // Get a frame for decoding (even an incomplete) without delay.
  VCMEncodedFrame* GetFrameForDecoding();

  // Releases a frame returned from the jitter buffer, should be called when
  // done with decoding.
  void ReleaseFrame(VCMEncodedFrame* frame);

  // Returns the frame assigned to this timestamp.
  int GetFrame(const VCMPacket& packet, VCMEncodedFrame*&);
  VCMEncodedFrame* GetFrame(const VCMPacket& packet);  // Deprecated.

  // Returns the time in ms when the latest packet was inserted into the frame.
  // Retransmitted is set to true if any of the packets belonging to the frame
  // has been retransmitted.
  int64_t LastPacketTime(VCMEncodedFrame* frame, bool* retransmitted) const;

  // Inserts a packet into a frame returned from GetFrame().
  VCMFrameBufferEnum InsertPacket(VCMEncodedFrame* frame,
                                  const VCMPacket& packet);

  // Returns the estimated jitter in milliseconds.
  uint32_t EstimatedJitterMs();

  // Updates the round-trip time estimate.
  void UpdateRtt(uint32_t rtt_ms);

  // Set the NACK mode. |highRttNackThreshold| is an RTT threshold in ms above
  // which NACK will be disabled if the NACK mode is |kNackHybrid|, -1 meaning
  // that NACK is always enabled in the hybrid mode.
  // |lowRttNackThreshold| is an RTT threshold in ms below which we expect to
  // rely on NACK only, and therefore are using larger buffers to have time to
  // wait for retransmissions.
  void SetNackMode(VCMNackMode mode, int low_rtt_nack_threshold_ms,
                   int high_rtt_nack_threshold_ms);

  // Returns the current NACK mode.
  VCMNackMode nack_mode() const;

  // Creates a list of missing sequence numbers.
  uint16_t* CreateNackList(uint16_t* nack_list_size, bool* list_extended);

  int64_t LastDecodedTimestamp() const;

 private:
  // In NACK-only mode this function doesn't return or release non-complete
  // frames unless we have a complete key frame. In hybrid mode, we may release
  // "decodable", incomplete frames.
  VCMEncodedFrame* GetFrameForDecodingNACK();

  void ReleaseFrameIfNotDecoding(VCMFrameBuffer* frame);

  // Gets an empty frame, creating a new frame if necessary (i.e. increases
  // jitter buffer size).
  VCMFrameBuffer* GetEmptyFrame();

  // Recycles oldest frames until a key frame is found. Used if jitter buffer is
  // completely full. Returns true if a key frame was found.
  bool RecycleFramesUntilKeyFrame();

  // Sets the state of |frame| to complete if it's not too old to be decoded.
  // Also updates the frame statistics. Signals the |frame_event| if this is
  // the next frame to be decoded.
  VCMFrameBufferEnum UpdateFrameState(VCMFrameBuffer* frame);

  // Finds the oldest complete frame, used for getting next frame to decode.
  // Can return a decodable, incomplete frame if |enable_decodable| is true.
  FrameList::iterator FindOldestCompleteContinuousFrame(bool enable_decodable);

  void CleanUpOldFrames();

  // Sets the "decodable" and "frame loss" flags of a frame depending on which
  // packets have been received and which are missing.
  // A frame is "decodable" if enough packets of that frame has been received
  // for it to be usable by the decoder.
  // A frame has the "frame loss" flag set if packets are missing  after the
  // last decoded frame and before |frame|.
  void VerifyAndSetPreviousFrameLost(VCMFrameBuffer* frame);

  // Returns true if |packet| is likely to have been retransmitted.
  bool IsPacketRetransmitted(const VCMPacket& packet) const;

  // The following three functions update the jitter estimate with the
  // payload size, receive time and RTP timestamp of a frame.
  void UpdateJitterEstimate(const VCMJitterSample& sample,
                            bool incomplete_frame);
  void UpdateJitterEstimate(const VCMFrameBuffer& frame, bool incomplete_frame);
  void UpdateJitterEstimate(int64_t latest_packet_time_ms,
                            uint32_t timestamp,
                            unsigned int frame_size,
                            bool incomplete_frame);

  // Returns the lowest and highest known sequence numbers, where the lowest is
  // the last decoded sequence number if a frame has been decoded.
  // -1 is returned if a sequence number cannot be determined.
  void GetLowHighSequenceNumbers(int32_t* low_seq_num,
                                 int32_t* high_seq_num) const;

  // Returns true if we should wait for retransmissions, false otherwise.
  bool WaitForRetransmissions();

  int vcm_id_;
  int receiver_id_;
  TickTimeBase* clock_;
  // If we are running (have started) or not.
  bool running_;
  CriticalSectionWrapper* crit_sect_;
  bool master_;
  // Event to signal when we have a frame ready for decoder.
  VCMEvent frame_event_;
  // Event to signal when we have received a packet.
  VCMEvent packet_event_;
  // Number of allocated frames.
  int max_number_of_frames_;
  // Array of pointers to the frames in jitter buffer.
  VCMFrameBuffer* frame_buffers_[kMaxNumberOfFrames];
  FrameList frame_list_;
  VCMDecodingState last_decoded_state_;
  bool first_packet_;

  // Statistics.
  int num_not_decodable_packets_;
  // Frame counter for each type (key, delta, golden, key-delta).
  unsigned int receive_statistics_[4];
  // Latest calculated frame rates of incoming stream.
  unsigned int incoming_frame_rate_;
  unsigned int incoming_frame_count_;
  int64_t time_last_incoming_frame_count_;
  unsigned int incoming_bit_count_;
  unsigned int incoming_bit_rate_;
  unsigned int drop_count_;  // Frame drop counter.
  // Number of frames in a row that have been too old.
  int num_consecutive_old_frames_;
  // Number of packets in a row that have been too old.
  int num_consecutive_old_packets_;
  // Number of packets discarded by the jitter buffer.
  int num_discarded_packets_;

  // Jitter estimation.
  // Filter for estimating jitter.
  VCMJitterEstimator jitter_estimate_;
  // Calculates network delays used for jitter calculations.
  VCMInterFrameDelay inter_frame_delay_;
  VCMJitterSample waiting_for_completion_;
  WebRtc_UWord32 rtt_ms_;

  // NACK and retransmissions.
  VCMNackMode nack_mode_;
  int low_rtt_nack_threshold_ms_;
  int high_rtt_nack_threshold_ms_;
  // Holds the internal NACK list (the missing sequence numbers).
  int32_t nack_seq_nums_internal_[kNackHistoryLength];
  uint16_t nack_seq_nums_[kNackHistoryLength];
  unsigned int nack_seq_nums_length_;
  bool waiting_for_key_frame_;

  DISALLOW_COPY_AND_ASSIGN(VCMJitterBuffer);
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_MAIN_SOURCE_JITTER_BUFFER_H_
