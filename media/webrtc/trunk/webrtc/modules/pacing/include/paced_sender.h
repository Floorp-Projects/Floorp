/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_PACED_SENDER_H_
#define WEBRTC_MODULES_PACED_SENDER_H_

#include <list>
#include <set>

#include "webrtc/modules/interface/module.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class CriticalSectionWrapper;
namespace paced_sender {
class IntervalBudget;
struct Packet;
class PacketList;
}  // namespace paced_sender

class PacedSender : public Module {
 public:
  enum Priority {
    kHighPriority = 0,  // Pass through; will be sent immediately.
    kNormalPriority = 2,  // Put in back of the line.
    kLowPriority = 3,  // Put in back of the low priority line.
  };
  // Low priority packets are mixed with the normal priority packets
  // while we are paused.

  class Callback {
   public:
    // Note: packets sent as a result of a callback should not pass by this
    // module again.
    // Called when it's time to send a queued packet.
    // Returns false if packet cannot be sent.
    virtual bool TimeToSendPacket(uint32_t ssrc, uint16_t sequence_number,
                                  int64_t capture_time_ms) = 0;
    // Called when it's a good time to send a padding data.
    virtual int TimeToSendPadding(int bytes) = 0;
   protected:
    virtual ~Callback() {}
  };
  PacedSender(Callback* callback, int target_bitrate_kbps,
              float pace_multiplier);

  virtual ~PacedSender();

  // Enable/disable pacing.
  void SetStatus(bool enable);

  bool Enabled() const;

  // Temporarily pause all sending.
  void Pause();

  // Resume sending packets.
  void Resume();

  // Set the pacing target bitrate and the bitrate up to which we are allowed to
  // pad. We will send padding packets to increase the total bitrate until we
  // reach |pad_up_to_bitrate_kbps|. If the media bitrate is above
  // |pad_up_to_bitrate_kbps| no padding will be sent.
  void UpdateBitrate(int target_bitrate_kbps,
                     int max_padding_bitrate_kbps,
                     int pad_up_to_bitrate_kbps);

  // Returns true if we send the packet now, else it will add the packet
  // information to the queue and call TimeToSendPacket when it's time to send.
  virtual bool SendPacket(Priority priority,
                          uint32_t ssrc,
                          uint16_t sequence_number,
                          int64_t capture_time_ms,
                          int bytes);

  // Returns the time since the oldest queued packet was captured.
  virtual int QueueInMs() const;

  // Returns the number of milliseconds until the module want a worker thread
  // to call Process.
  virtual int32_t TimeUntilNextProcess() OVERRIDE;

  // Process any pending packets in the queue(s).
  virtual int32_t Process() OVERRIDE;

 private:
  // Return true if next packet in line should be transmitted.
  // Return packet list that contains the next packet.
  bool ShouldSendNextPacket(paced_sender::PacketList** packet_list);

  // Local helper function to GetNextPacket.
  void GetNextPacketFromList(paced_sender::PacketList* packets,
      uint32_t* ssrc, uint16_t* sequence_number, int64_t* capture_time_ms);

  // Updates the number of bytes that can be sent for the next time interval.
  void UpdateBytesPerInterval(uint32_t delta_time_in_ms);

  // Updates the buffers with the number of bytes that we sent.
  void UpdateMediaBytesSent(int num_bytes);

  Callback* callback_;
  const float pace_multiplier_;
  bool enabled_;
  bool paused_;
  scoped_ptr<CriticalSectionWrapper> critsect_;
  // This is the media budget, keeping track of how many bits of media
  // we can pace out during the current interval.
  scoped_ptr<paced_sender::IntervalBudget> media_budget_;
  // This is the padding budget, keeping track of how many bits of padding we're
  // allowed to send out during the current interval.
  scoped_ptr<paced_sender::IntervalBudget> padding_budget_;
  // Media and padding share this budget, therefore no padding will be sent if
  // media uses all of this budget. This is used to avoid padding above a given
  // bitrate.
  scoped_ptr<paced_sender::IntervalBudget> pad_up_to_bitrate_budget_;

  TickTime time_last_update_;
  TickTime time_last_send_;
  int64_t capture_time_ms_last_queued_;
  int64_t capture_time_ms_last_sent_;

  scoped_ptr<paced_sender::PacketList> high_priority_packets_;
  scoped_ptr<paced_sender::PacketList> normal_priority_packets_;
  scoped_ptr<paced_sender::PacketList> low_priority_packets_;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_PACED_SENDER_H_
