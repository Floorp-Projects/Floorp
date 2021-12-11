/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_FAKE_NETWORK_PIPE_H_
#define TEST_FAKE_NETWORK_PIPE_H_

#include <string.h>
#include <map>
#include <memory>
#include <queue>
#include <set>

#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/random.h"
#include "typedefs.h"  // NOLINT(build/include)

namespace webrtc {

class Clock;
class PacketReceiver;
enum class MediaType;

class NetworkPacket {
 public:
  NetworkPacket(const uint8_t* data,
                size_t length,
                int64_t send_time,
                int64_t arrival_time)
      : data_(new uint8_t[length]),
        data_length_(length),
        send_time_(send_time),
        arrival_time_(arrival_time) {
    memcpy(data_.get(), data, length);
  }

  uint8_t* data() const { return data_.get(); }
  size_t data_length() const { return data_length_; }
  int64_t send_time() const { return send_time_; }
  int64_t arrival_time() const { return arrival_time_; }
  void IncrementArrivalTime(int64_t extra_delay) {
    arrival_time_ += extra_delay;
  }

 private:
  // The packet data.
  std::unique_ptr<uint8_t[]> data_;
  // Length of data_.
  size_t data_length_;
  // The time the packet was sent out on the network.
  const int64_t send_time_;
  // The time the packet should arrive at the receiver.
  int64_t arrival_time_;
};

class Demuxer {
 public:
  virtual ~Demuxer() = default;
  virtual void SetReceiver(PacketReceiver* receiver) = 0;
  virtual void DeliverPacket(const NetworkPacket* packet,
                             const PacketTime& packet_time) = 0;
};

class DemuxerImpl final : public Demuxer {
 public:
  explicit DemuxerImpl(const std::map<uint8_t, MediaType>& payload_type_map);

  void SetReceiver(PacketReceiver* receiver) override;
  void DeliverPacket(const NetworkPacket* packet,
                     const PacketTime& packet_time) override;

 private:
  PacketReceiver* packet_receiver_;
  const std::map<uint8_t, MediaType> payload_type_map_;
  RTC_DISALLOW_COPY_AND_ASSIGN(DemuxerImpl);
};

// Class faking a network link. This is a simple and naive solution just faking
// capacity and adding an extra transport delay in addition to the capacity
// introduced delay.

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
    // If packets are allowed to be reordered.
    bool allow_reordering = false;
    // The average length of a burst of lost packets.
    int avg_burst_loss_length = -1;
  };

  FakeNetworkPipe(Clock* clock,
                  const FakeNetworkPipe::Config& config,
                  std::unique_ptr<Demuxer> demuxer);
  FakeNetworkPipe(Clock* clock,
                  const FakeNetworkPipe::Config& config,
                  std::unique_ptr<Demuxer> demuxer,
                  uint64_t seed);
  virtual ~FakeNetworkPipe();

  // Sets a new configuration. This won't affect packets already in the pipe.
  void SetConfig(const FakeNetworkPipe::Config& config);

  // Sends a new packet to the link.
  virtual void SendPacket(const uint8_t* packet, size_t packet_length);

  // Must not be called in parallel with SendPacket or Process.
  void SetReceiver(PacketReceiver* receiver);

  // Processes the network queues and trigger PacketReceiver::IncomingPacket for
  // packets ready to be delivered.
  virtual void Process();
  int64_t TimeUntilNextProcess() const;

  // Get statistics.
  float PercentageLoss();
  int AverageDelay();
  size_t dropped_packets() { return dropped_packets_; }
  size_t sent_packets() { return sent_packets_; }

 protected:
  Clock* const clock_;
  rtc::CriticalSection lock_;
  const std::unique_ptr<Demuxer> demuxer_;
  std::queue<NetworkPacket*> capacity_link_;
  Random random_;

  // Since we need to access both the packet with the earliest and latest
  // arrival time we need to use a multiset to keep all packets sorted,
  // hence, we cannot use a priority queue.
  struct PacketArrivalTimeComparator {
    bool operator()(const NetworkPacket* p1, const NetworkPacket* p2) {
      return p1->arrival_time() < p2->arrival_time();
    }
  };
  std::multiset<NetworkPacket*, PacketArrivalTimeComparator> delay_link_;

  // Link configuration.
  Config config_;

  // Statistics.
  size_t dropped_packets_;
  size_t sent_packets_;
  int64_t total_packet_delay_;

  // Are we currently dropping a burst of packets?
  bool bursting_;

  // The probability to drop the packet if we are currently dropping a
  // burst of packet
  double prob_loss_bursting_;

  // The probability to drop a burst of packets.
  double prob_start_bursting_;

  int64_t next_process_time_;

  int64_t last_log_time_;

  int64_t capacity_delay_error_bytes_ = 0;

  RTC_DISALLOW_COPY_AND_ASSIGN(FakeNetworkPipe);
};

}  // namespace webrtc

#endif  // TEST_FAKE_NETWORK_PIPE_H_
