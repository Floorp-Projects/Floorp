/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TEST_FAKE_NETWORK_PIPE_H_
#define WEBRTC_TEST_FAKE_NETWORK_PIPE_H_

#include <queue>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class Clock;
class CriticalSectionWrapper;
class NetworkPacket;
class PacketReceiver;

// Class faking a network link. This is a simple and naive solution just faking
// capacity and adding an extra transport delay in addition to the capacity
// introduced delay.

// TODO(mflodman) Add random and bursty packet loss.
class FakeNetworkPipe {
 public:
  struct Config {
    Config() {}
    // Queue length in number of packets.
    size_t queue_length_packets = 0;
    // Delay in addition to capacity induced delay.
    int queue_delay_ms = 0;
    // Standard deviation of the extra delay.
    int delay_standard_deviation_ms = 0;
    // Link capacity in kbps.
    int link_capacity_kbps = 0;
    // Random packet loss.
    int loss_percent = 0;
  };

  FakeNetworkPipe(Clock* clock, const FakeNetworkPipe::Config& config);
  ~FakeNetworkPipe();

  // Must not be called in parallel with SendPacket or Process.
  void SetReceiver(PacketReceiver* receiver);

  // Sets a new configuration. This won't affect packets already in the pipe.
  void SetConfig(const FakeNetworkPipe::Config& config);

  // Sends a new packet to the link.
  void SendPacket(const uint8_t* packet, size_t packet_length);

  // Processes the network queues and trigger PacketReceiver::IncomingPacket for
  // packets ready to be delivered.
  void Process();
  int64_t TimeUntilNextProcess() const;

  // Get statistics.
  float PercentageLoss();
  int AverageDelay();
  size_t dropped_packets() { return dropped_packets_; }
  size_t sent_packets() { return sent_packets_; }

 private:
  Clock* const clock_;
  mutable rtc::CriticalSection lock_;
  PacketReceiver* packet_receiver_;
  std::queue<NetworkPacket*> capacity_link_;
  std::queue<NetworkPacket*> delay_link_;

  // Link configuration.
  Config config_;

  // Statistics.
  size_t dropped_packets_;
  size_t sent_packets_;
  int64_t total_packet_delay_;

  int64_t next_process_time_;

  RTC_DISALLOW_COPY_AND_ASSIGN(FakeNetworkPipe);
};

}  // namespace webrtc

#endif  // WEBRTC_TEST_FAKE_NETWORK_PIPE_H_
