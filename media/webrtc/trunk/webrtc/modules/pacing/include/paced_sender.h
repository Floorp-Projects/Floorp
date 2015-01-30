/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_PACING_INCLUDE_PACED_SENDER_H_
#define WEBRTC_MODULES_PACING_INCLUDE_PACED_SENDER_H_

#include <list>
#include <set>

#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/interface/module.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class BitrateProber;
class Clock;
class CriticalSectionWrapper;

namespace paced_sender {
class IntervalBudget;
struct Packet;
class PacketQueue;
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
    virtual bool TimeToSendPacket(uint32_t ssrc,
                                  uint16_t sequence_number,
                                  int64_t capture_time_ms,
                                  bool retransmission) = 0;
    // Called when it's a good time to send a padding data.
    // Returns the number of bytes sent.
    virtual int TimeToSendPadding(int bytes) = 0;

   protected:
    virtual ~Callback() {}
  };

  static const int kDefaultMaxQueueLengthMs = 2000;
  // Pace in kbits/s until we receive first estimate.
  static const int kDefaultInitialPaceKbps = 2000;
  // Pacing-rate relative to our target send rate.
  // Multiplicative factor that is applied to the target bitrate to calculate
  // the number of bytes that can be transmitted per interval.
  // Increasing this factor will result in lower delays in cases of bitrate
  // overshoots from the encoder.
  static const float kDefaultPaceMultiplier;

  PacedSender(Clock* clock,
              Callback* callback,
              int bitrate_kbps,
              int max_bitrate_kbps,
              int min_bitrate_kbps);

  virtual ~PacedSender();

  // Enable/disable pacing.
  void SetStatus(bool enable);

  bool Enabled() const;

  // Temporarily pause all sending.
  void Pause();

  // Resume sending packets.
  void Resume();

  // Set target bitrates for the pacer.
  // We will pace out bursts of packets at a bitrate of |max_bitrate_kbps|.
  // |bitrate_kbps| is our estimate of what we are allowed to send on average.
  // Padding packets will be utilized to reach |min_bitrate| unless enough media
  // packets are available.
  void UpdateBitrate(int bitrate_kbps,
                     int max_bitrate_kbps,
                     int min_bitrate_kbps);

  // Returns true if we send the packet now, else it will add the packet
  // information to the queue and call TimeToSendPacket when it's time to send.
  virtual bool SendPacket(Priority priority,
                          uint32_t ssrc,
                          uint16_t sequence_number,
                          int64_t capture_time_ms,
                          int bytes,
                          bool retransmission);

  // Returns the time since the oldest queued packet was enqueued.
  virtual int QueueInMs() const;

  virtual size_t QueueSizePackets() const;

  // Returns the number of milliseconds it will take to send the current
  // packets in the queue, given the current size and bitrate, ignoring prio.
  virtual int ExpectedQueueTimeMs() const;

  // Returns the number of milliseconds until the module want a worker thread
  // to call Process.
  virtual int32_t TimeUntilNextProcess() OVERRIDE;

  // Process any pending packets in the queue(s).
  virtual int32_t Process() OVERRIDE;

 protected:
  virtual bool ProbingExperimentIsEnabled() const;

 private:
  // Updates the number of bytes that can be sent for the next time interval.
  void UpdateBytesPerInterval(uint32_t delta_time_in_ms)
      EXCLUSIVE_LOCKS_REQUIRED(critsect_);

  bool SendPacket(const paced_sender::Packet& packet)
      EXCLUSIVE_LOCKS_REQUIRED(critsect_);
  void SendPadding(int padding_needed) EXCLUSIVE_LOCKS_REQUIRED(critsect_);

  Clock* const clock_;
  Callback* const callback_;

  scoped_ptr<CriticalSectionWrapper> critsect_;
  bool enabled_ GUARDED_BY(critsect_);
  bool paused_ GUARDED_BY(critsect_);
  // This is the media budget, keeping track of how many bits of media
  // we can pace out during the current interval.
  scoped_ptr<paced_sender::IntervalBudget> media_budget_ GUARDED_BY(critsect_);
  // This is the padding budget, keeping track of how many bits of padding we're
  // allowed to send out during the current interval. This budget will be
  // utilized when there's no media to send.
  scoped_ptr<paced_sender::IntervalBudget> padding_budget_
      GUARDED_BY(critsect_);

  scoped_ptr<BitrateProber> prober_ GUARDED_BY(critsect_);
  int bitrate_bps_ GUARDED_BY(critsect_);

  int64_t time_last_update_us_ GUARDED_BY(critsect_);

  scoped_ptr<paced_sender::PacketQueue> packets_ GUARDED_BY(critsect_);
  uint64_t packet_counter_ GUARDED_BY(critsect_);
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_PACING_INCLUDE_PACED_SENDER_H_
