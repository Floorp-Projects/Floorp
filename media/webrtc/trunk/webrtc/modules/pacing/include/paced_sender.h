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

#include "webrtc/modules/interface/module.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class CriticalSectionWrapper;

class PacedSender : public Module {
 public:
  enum Priority {
    kHighPriority = 0,  // Pass through; will be sent immediately.
    kNormalPriority = 2,  // Put in back of the line.
    kLowPriority = 3,  // Put in back of the low priority line.
  };
  class Callback {
   public:
    // Note: packets sent as a result of a callback should not pass by this
    // module again.
    // Called when it's time to send a queued packet.
    virtual void TimeToSendPacket(uint32_t ssrc, uint16_t sequence_number,
                                  int64_t capture_time_ms) = 0;
    // Called when it's a good time to send a padding data.
    virtual void TimeToSendPadding(int bytes) = 0;
   protected:
    virtual ~Callback() {}
  };
  PacedSender(Callback* callback, int target_bitrate_kbps);

  virtual ~PacedSender();

  // Enable/disable pacing.
  void SetStatus(bool enable);

  // Current total estimated bitrate.
  void UpdateBitrate(int target_bitrate_kbps);

  // Returns true if we send the packet now, else it will add the packet
  // information to the queue and call TimeToSendPacket when it's time to send.
  bool SendPacket(Priority priority, uint32_t ssrc, uint16_t sequence_number,
                  int64_t capture_time_ms, int bytes);

  // Returns the number of milliseconds until the module want a worker thread
  // to call Process.
  virtual int32_t TimeUntilNextProcess();

  // Process any pending packets in the queue(s).
  virtual int32_t Process();

 private:
  struct Packet {
    Packet(uint32_t ssrc, uint16_t seq_number, int64_t capture_time_ms,
           int length_in_bytes)
        : ssrc_(ssrc),
          sequence_number_(seq_number),
          capture_time_ms_(capture_time_ms),
          bytes_(length_in_bytes) {
    }
    uint32_t ssrc_;
    uint16_t sequence_number_;
    int64_t capture_time_ms_;
    int bytes_;
  };
  // Checks if next packet in line can be transmitted. Returns true on success.
  bool GetNextPacket(uint32_t* ssrc, uint16_t* sequence_number,
                     int64_t* capture_time_ms);

  // Updates the number of bytes that can be sent for the next time interval.
  void UpdateBytesPerInterval(uint32_t delta_time_in_ms);

  // Updates the buffers with the number of bytes that we sent.
  void UpdateState(int num_bytes);

  Callback* callback_;
  bool enable_;
  scoped_ptr<CriticalSectionWrapper> critsect_;
  int target_bitrate_kbytes_per_s_;
  int bytes_remaining_interval_;
  int padding_bytes_remaining_interval_;
  TickTime time_last_update_;
  TickTime time_last_send_;

  std::list<Packet> normal_priority_packets_;
  std::list<Packet> low_priority_packets_;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_PACED_SENDER_H_
