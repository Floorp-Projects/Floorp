/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/test/libvietest/include/fake_network_pipe.h"

#include <assert.h>
#include <math.h>
#include <string.h>

#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/tick_util.h"

namespace webrtc {

const int kNetworkProcessMaxWaitTime = 10;
const double kPi = 3.14159265;

static int GaussianRandom(int mean_delay_ms, int standard_deviation_ms) {
  // Creating a Normal distribution variable from two independent uniform
  // variables based on the Box-Muller transform.
  double uniform1 = (rand() + 1.0) / (RAND_MAX + 1.0);  // NOLINT
  double uniform2 = (rand() + 1.0) / (RAND_MAX + 1.0);  // NOLINT
  return static_cast<int>(mean_delay_ms + standard_deviation_ms *
                          sqrt(-2 * log(uniform1)) * cos(2 * kPi * uniform2));
}

class NetworkPacket {
 public:
  NetworkPacket(void* data, int length, int64_t send_time, int64_t arrival_time)
      : data_(NULL),
        data_length_(length),
        send_time_(send_time),
        arrival_time_(arrival_time) {
    data_ = new uint8_t[length];
    memcpy(data_, data, length);
  }
  ~NetworkPacket() {}

  void ReleaseData() {
    delete [] data_;
    data_ = NULL;
  }
  uint8_t* data() const { return data_; }
  int data_length() const { return data_length_; }
  int64_t send_time() const { return send_time_; }
  int64_t arrival_time() const { return arrival_time_; }
  void IncrementArrivalTime(int64_t extra_delay) {
    arrival_time_+= extra_delay;
  }

 private:
  // The packet data.
  uint8_t* data_;
  // Length of data_.
  int data_length_;
  // The time the packet was sent out on the network.
  const int64_t send_time_;
  // The time the packet should arrive at the reciver.
  int64_t arrival_time_;
};

FakeNetworkPipe::FakeNetworkPipe(
    const FakeNetworkPipe::Configuration& configuration)
    : packet_receiver_(configuration.packet_receiver),
      link_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      queue_length_(configuration.queue_length),
      queue_delay_ms_(configuration.queue_delay_ms),
      queue_delay_deviation_ms_(configuration.delay_standard_deviation_ms),
      link_capacity_bytes_ms_(configuration.link_capacity_kbps / 8),
      loss_percent_(configuration.loss_percent),
      dropped_packets_(0),
      sent_packets_(0),
      total_packet_delay_(0) {
  assert(link_capacity_bytes_ms_ > 0);
  assert(packet_receiver_ != NULL);
}

FakeNetworkPipe::~FakeNetworkPipe() {
}

void FakeNetworkPipe::SendPacket(void* data, int data_length) {
  CriticalSectionScoped cs(link_cs_.get());
  if (capacity_link_.size() >= queue_length_) {
    // Too many packet on the link, drop this one.
    ++dropped_packets_;
    return;
  }

  int64_t time_now = TickTime::MillisecondTimestamp();

  // Delay introduced by the link capacity.
  int64_t capacity_delay_ms = data_length / link_capacity_bytes_ms_;
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
  CriticalSectionScoped cs(link_cs_.get());
  if (sent_packets_ == 0)
    return 0;

  return static_cast<float>(dropped_packets_) /
      (sent_packets_ + dropped_packets_);
}

int FakeNetworkPipe::AverageDelay() {
  CriticalSectionScoped cs(link_cs_.get());
  if (sent_packets_ == 0)
    return 0;

  return total_packet_delay_ / sent_packets_;
}

void FakeNetworkPipe::NetworkProcess() {
  CriticalSectionScoped cs(link_cs_.get());
  if (capacity_link_.size() == 0 && delay_link_.size() == 0)
    return;

  int64_t time_now = TickTime::MillisecondTimestamp();

  // Check the capacity link first.
  while (capacity_link_.size() > 0 &&
         time_now >= capacity_link_.front()->arrival_time()) {
    // Time to get this packet.
    NetworkPacket* packet = capacity_link_.front();
    capacity_link_.pop();

    // Add extra delay and jitter, but make sure the arrival time is not earlier
    // than the last packet in the queue.
    int extra_delay = GaussianRandom(queue_delay_ms_,
                                     queue_delay_deviation_ms_);
    if (delay_link_.size() > 0 &&
        packet->arrival_time() + extra_delay <
        delay_link_.back()->arrival_time()) {
      extra_delay = delay_link_.back()->arrival_time() - packet->arrival_time();
    }
    packet->IncrementArrivalTime(extra_delay);
    delay_link_.push(packet);
  }

  // Check the extra delay queue.
  while (delay_link_.size() > 0 &&
         time_now >= delay_link_.front()->arrival_time()) {
    // Deliver this packet.
    NetworkPacket* packet = delay_link_.front();
    delay_link_.pop();
    packet_receiver_->IncomingPacket(packet->data(), packet->data_length());
    ++sent_packets_;

    // |time_now| might be later than when the packet should have arrived, due
    // to NetworkProcess being called too late. For stats, use the time it
    // should have been on the link.
    total_packet_delay_ += packet->arrival_time() - packet->send_time();
    delete packet;
  }
}

}  // namespace webrtc
