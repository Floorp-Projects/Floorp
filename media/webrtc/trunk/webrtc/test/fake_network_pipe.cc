/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/test/fake_network_pipe.h"

#include <assert.h>
#include <math.h>
#include <string.h>
#include <algorithm>

#include "webrtc/call.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/tick_util.h"

namespace webrtc {

const double kPi = 3.14159265;
const int kDefaultProcessIntervalMs = 30;

static int GaussianRandom(int mean_delay_ms, int standard_deviation_ms) {
  // Creating a Normal distribution variable from two independent uniform
  // variables based on the Box-Muller transform.
  double uniform1 = (rand() + 1.0) / (RAND_MAX + 1.0);  // NOLINT
  double uniform2 = (rand() + 1.0) / (RAND_MAX + 1.0);  // NOLINT
  return static_cast<int>(mean_delay_ms + standard_deviation_ms *
                          sqrt(-2 * log(uniform1)) * cos(2 * kPi * uniform2));
}

static bool UniformLoss(int loss_percent) {
  int outcome = rand() % 100;
  return outcome < loss_percent;
}

class NetworkPacket {
 public:
  NetworkPacket(const uint8_t* data, size_t length, int64_t send_time,
      int64_t arrival_time)
      : data_(NULL),
        data_length_(length),
        send_time_(send_time),
        arrival_time_(arrival_time) {
    data_ = new uint8_t[length];
    memcpy(data_, data, length);
  }
  ~NetworkPacket() {
    delete [] data_;
  }

  uint8_t* data() const { return data_; }
  size_t data_length() const { return data_length_; }
  int64_t send_time() const { return send_time_; }
  int64_t arrival_time() const { return arrival_time_; }
  void IncrementArrivalTime(int64_t extra_delay) {
    arrival_time_+= extra_delay;
  }

 private:
  // The packet data.
  uint8_t* data_;
  // Length of data_.
  size_t data_length_;
  // The time the packet was sent out on the network.
  const int64_t send_time_;
  // The time the packet should arrive at the reciver.
  int64_t arrival_time_;
};

FakeNetworkPipe::FakeNetworkPipe(
    const FakeNetworkPipe::Config& config)
    : lock_(CriticalSectionWrapper::CreateCriticalSection()),
      packet_receiver_(NULL),
      config_(config),
      dropped_packets_(0),
      sent_packets_(0),
      total_packet_delay_(0),
      next_process_time_(TickTime::MillisecondTimestamp()) {
}

FakeNetworkPipe::~FakeNetworkPipe() {
  while (!capacity_link_.empty()) {
    delete capacity_link_.front();
    capacity_link_.pop();
  }
  while (!delay_link_.empty()) {
    delete delay_link_.front();
    delay_link_.pop();
  }
}

void FakeNetworkPipe::SetReceiver(PacketReceiver* receiver) {
  packet_receiver_ = receiver;
}

void FakeNetworkPipe::SetConfig(const FakeNetworkPipe::Config& config) {
  CriticalSectionScoped crit(lock_.get());
  config_ = config;  // Shallow copy of the struct.
}

void FakeNetworkPipe::SendPacket(const uint8_t* data, size_t data_length) {
  // A NULL packet_receiver_ means that this pipe will terminate the flow of
  // packets.
  if (packet_receiver_ == NULL)
    return;
  CriticalSectionScoped crit(lock_.get());
  if (config_.queue_length_packets > 0 &&
      capacity_link_.size() >= config_.queue_length_packets) {
    // Too many packet on the link, drop this one.
    ++dropped_packets_;
    return;
  }

  int64_t time_now = TickTime::MillisecondTimestamp();

  // Delay introduced by the link capacity.
  int64_t capacity_delay_ms = 0;
  if (config_.link_capacity_kbps > 0)
    capacity_delay_ms = data_length / (config_.link_capacity_kbps / 8);
  int64_t network_start_time = time_now;

  // Check if there already are packets on the link and change network start
  // time if there is.
  if (capacity_link_.size() > 0)
    network_start_time = capacity_link_.back()->arrival_time();

  int64_t arrival_time = network_start_time + capacity_delay_ms;
  NetworkPacket* packet = new NetworkPacket(data, data_length, time_now,
                                            arrival_time);
  capacity_link_.push(packet);
}

float FakeNetworkPipe::PercentageLoss() {
  CriticalSectionScoped crit(lock_.get());
  if (sent_packets_ == 0)
    return 0;

  return static_cast<float>(dropped_packets_) /
      (sent_packets_ + dropped_packets_);
}

int FakeNetworkPipe::AverageDelay() {
  CriticalSectionScoped crit(lock_.get());
  if (sent_packets_ == 0)
    return 0;

  return total_packet_delay_ / static_cast<int>(sent_packets_);
}

void FakeNetworkPipe::Process() {
  int64_t time_now = TickTime::MillisecondTimestamp();
  std::queue<NetworkPacket*> packets_to_deliver;
  {
    CriticalSectionScoped crit(lock_.get());
    // Check the capacity link first.
    while (capacity_link_.size() > 0 &&
           time_now >= capacity_link_.front()->arrival_time()) {
      // Time to get this packet.
      NetworkPacket* packet = capacity_link_.front();
      capacity_link_.pop();

      // Packets are randomly dropped after being affected by the bottleneck.
      if (UniformLoss(config_.loss_percent)) {
        delete packet;
        continue;
      }

      // Add extra delay and jitter, but make sure the arrival time is not
      // earlier than the last packet in the queue.
      int extra_delay = GaussianRandom(config_.queue_delay_ms,
                                       config_.delay_standard_deviation_ms);
      if (delay_link_.size() > 0 &&
          packet->arrival_time() + extra_delay <
          delay_link_.back()->arrival_time()) {
        extra_delay = delay_link_.back()->arrival_time() -
            packet->arrival_time();
      }
      packet->IncrementArrivalTime(extra_delay);
      if (packet->arrival_time() < next_process_time_)
        next_process_time_ = packet->arrival_time();
      delay_link_.push(packet);
    }

    // Check the extra delay queue.
    while (delay_link_.size() > 0 &&
           time_now >= delay_link_.front()->arrival_time()) {
      // Deliver this packet.
      NetworkPacket* packet = delay_link_.front();
      packets_to_deliver.push(packet);
      delay_link_.pop();
      // |time_now| might be later than when the packet should have arrived, due
      // to NetworkProcess being called too late. For stats, use the time it
      // should have been on the link.
      total_packet_delay_ += packet->arrival_time() - packet->send_time();
    }
    sent_packets_ += packets_to_deliver.size();
  }
  while (!packets_to_deliver.empty()) {
    NetworkPacket* packet = packets_to_deliver.front();
    packets_to_deliver.pop();
    packet_receiver_->DeliverPacket(packet->data(), packet->data_length());
    delete packet;
  }
}

int FakeNetworkPipe::TimeUntilNextProcess() const {
  CriticalSectionScoped crit(lock_.get());
  if (capacity_link_.size() == 0 || delay_link_.size() == 0)
    return kDefaultProcessIntervalMs;
  return std::max(static_cast<int>(next_process_time_ -
      TickTime::MillisecondTimestamp()), 0);
}

}  // namespace webrtc
