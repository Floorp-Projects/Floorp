/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


// This file includes unit tests for RemoteBitrateEstimator.

#include <gtest/gtest.h>
#include <list>

#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

enum { kMtu = 1200 };

class TestBitrateObserver : public RemoteBitrateObserver {
 public:
  TestBitrateObserver() : updated_(false), latest_bitrate_(0) {}

  void OnReceiveBitrateChanged(unsigned int ssrc, unsigned int bitrate) {
    latest_bitrate_ = bitrate;
    updated_ = true;
  }

  void Reset() {
    updated_ = false;
  }

  bool updated() const {
    return updated_;
  }

  unsigned int latest_bitrate() const {
    return latest_bitrate_;
  }

 private:
  bool updated_;
  unsigned int latest_bitrate_;
};

class StreamGenerator {
 public:
  struct Packet {
    int64_t send_time;
    int64_t arrival_time;
    uint32_t rtp_timestamp;
    unsigned int size;
  };

  typedef std::list<Packet*> PacketList;

  StreamGenerator(int fps, int bitrate_bps, int capacity, int64_t time_now)
      : fps_(fps),
        bitrate_bps_(bitrate_bps),
        capacity_(capacity),
        time_now_(time_now),
        prev_arrival_time_(time_now),
        rtp_timestamp_offset_(0xFFFFF000) {}

  void SetCapacity(int capacity_bps) {
    ASSERT_GT(capacity_bps, 0);
    capacity_ = capacity_bps;
  }

  void SetBitrate(int bitrate_bps) {
    ASSERT_GE(bitrate_bps, 0);
    bitrate_bps_ = bitrate_bps;
  }

  void SetRtpTimestampOffset(uint32_t offset) {
    rtp_timestamp_offset_ = offset;
  }

  void GenerateFrame(PacketList* packets) {
    ASSERT_FALSE(packets == NULL);
    ASSERT_TRUE(packets->empty());
    ASSERT_GT(fps_, 0);
    int bits_per_frame = bitrate_bps_ / fps_;
    int n_packets = std::max(bits_per_frame / (8 * kMtu), 1);
    int packet_size = bits_per_frame / (8 * n_packets);
    ASSERT_GE(n_packets, 0);
    for (int i = 0; i < n_packets; ++i) {
      Packet* packet = new Packet;
      packet->send_time = time_now_ + kSendSideOffsetMs;
      ASSERT_GT(capacity_, 0);
      packet->arrival_time = std::max(
          prev_arrival_time_ + 8 * 1000 * packet_size / capacity_,
          time_now_);
      packet->size = packet_size;
      packet->rtp_timestamp = rtp_timestamp_offset_ + 90 * packet->send_time;
      prev_arrival_time_ = packet->arrival_time;
      packets->push_back(packet);
    }
    time_now_ = time_now_ + 1000 / fps_;
  }

  int64_t TimeNow() const {
    return time_now_;
  }

 private:
  enum { kSendSideOffsetMs = 1000 };

  int fps_;
  int bitrate_bps_;
  int capacity_;
  int64_t time_now_;
  int64_t prev_arrival_time_;
  uint32_t rtp_timestamp_offset_;
};

class RemoteBitrateEstimatorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    bitrate_observer_.reset(new TestBitrateObserver);
    bitrate_estimator_.reset(new RemoteBitrateEstimator(
        bitrate_observer_.get(), over_use_detector_options_));
    // Framerate: 30 fps; Start bitrate: 300 kbps; Link capacity: 1000 kbps,
    // Start time: 0.
    stream_generator_.reset(new StreamGenerator(30, 3e5, 1e6, 0));
  }

  // Generates a frame of packets belonging to a stream at a given bitrate and
  // with a given ssrc. The stream is pushed through a very simple simulated
  // network, and is then given to the receive-side bandwidth estimator.
  // Returns true if an over-use was seen, false otherwise.
  // The StreamGenerator::updated() should be used to check for any changes in
  // target bitrate after the call to this function.
  bool GenerateAndProcessFrame(unsigned int ssrc, unsigned int bitrate_bps) {
    stream_generator_->SetBitrate(bitrate_bps);
    StreamGenerator::PacketList packets;
    stream_generator_->GenerateFrame(&packets);
    int64_t last_arrival_time = -1;
    bool prev_was_decrease = false;
    bool overuse = false;
    while (!packets.empty()) {
      StreamGenerator::Packet* packet = packets.front();
      bitrate_estimator_->IncomingPacket(ssrc,
                                         packet->size,
                                         packet->arrival_time,
                                         packet->rtp_timestamp,
                                         -1);
      if (bitrate_observer_->updated()) {
        // Verify that new estimates only are triggered by an overuse and a
        // rate decrease.
        overuse = true;
        EXPECT_LE(bitrate_observer_->latest_bitrate(), bitrate_bps);
        EXPECT_FALSE(prev_was_decrease);
        prev_was_decrease = true;
      } else {
        prev_was_decrease = false;
      }
      bitrate_observer_->Reset();
      last_arrival_time = packet->arrival_time;
      delete packet;
      packets.pop_front();
    }
    EXPECT_GT(last_arrival_time, -1);
    bitrate_estimator_->UpdateEstimate(ssrc, last_arrival_time);
    return overuse;
  }

  // Run the bandwidth estimator with a stream of |number_of_frames| frames.
  // Can for instance be used to run the estimator for some time to get it
  // into a steady state.
  unsigned int SteadyStateRun(unsigned int ssrc,
                              int number_of_frames,
                              unsigned int start_bitrate,
                              unsigned int min_bitrate,
                              unsigned int max_bitrate) {
    unsigned int bitrate_bps = start_bitrate;
    bool bitrate_update_seen = false;
    // Produce |number_of_frames| frames and give them to the estimator.
    for (int i = 0; i < number_of_frames; ++i) {
      bool overuse = GenerateAndProcessFrame(ssrc, bitrate_bps);
      if (overuse) {
        EXPECT_LT(bitrate_observer_->latest_bitrate(), max_bitrate);
        EXPECT_GT(bitrate_observer_->latest_bitrate(), min_bitrate);
        bitrate_bps = bitrate_observer_->latest_bitrate();
        bitrate_update_seen = true;
      } else if (bitrate_observer_->updated()) {
        bitrate_bps = bitrate_observer_->latest_bitrate();
        bitrate_observer_->Reset();
      }
    }
    EXPECT_TRUE(bitrate_update_seen);
    return bitrate_bps;
  }

  OverUseDetectorOptions over_use_detector_options_;
  scoped_ptr<RemoteBitrateEstimator> bitrate_estimator_;
  scoped_ptr<TestBitrateObserver> bitrate_observer_;
  scoped_ptr<StreamGenerator> stream_generator_;
};

TEST_F(RemoteBitrateEstimatorTest, TestInitialBehavior) {
  unsigned int bitrate_bps = 0;
  unsigned int ssrc = 0;
  int64_t time_now = 0;
  uint32_t timestamp = 0;
  EXPECT_FALSE(bitrate_estimator_->LatestEstimate(ssrc, &bitrate_bps));
  bitrate_estimator_->UpdateEstimate(ssrc, time_now);
  EXPECT_FALSE(bitrate_estimator_->LatestEstimate(ssrc, &bitrate_bps));
  EXPECT_FALSE(bitrate_observer_->updated());
  bitrate_observer_->Reset();
  // Inserting a packet. Still no valid estimate. We need to wait 1 second.
  bitrate_estimator_->IncomingPacket(ssrc, kMtu, time_now,
                                     timestamp, -1);
  bitrate_estimator_->UpdateEstimate(ssrc, time_now);
  EXPECT_FALSE(bitrate_estimator_->LatestEstimate(ssrc, &bitrate_bps));
  EXPECT_FALSE(bitrate_observer_->updated());
  bitrate_observer_->Reset();
  // Waiting more than one second gives us a valid estimate.
  time_now += 1001;
  bitrate_estimator_->UpdateEstimate(ssrc, time_now);
  EXPECT_TRUE(bitrate_estimator_->LatestEstimate(ssrc, &bitrate_bps));
  EXPECT_EQ(bitrate_bps, 10734u);
  EXPECT_TRUE(bitrate_observer_->updated());
  bitrate_observer_->Reset();
  EXPECT_EQ(bitrate_observer_->latest_bitrate(), bitrate_bps);
}

// Make sure we initially increase the bitrate as expected.
TEST_F(RemoteBitrateEstimatorTest, TestRateIncreaseRtpTimestamps) {
  const int kExpectedIterations = 323;
  unsigned int bitrate_bps = 30000;
  unsigned int ssrc = 0;
  int iterations = 0;
  // Feed the estimator with a stream of packets and verify that it reaches
  // 500 kbps at the expected time.
  while (bitrate_bps < 5e5) {
    bool overuse = GenerateAndProcessFrame(ssrc, bitrate_bps);
    if (overuse) {
      EXPECT_GT(bitrate_observer_->latest_bitrate(), bitrate_bps);
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    } else if (bitrate_observer_->updated()) {
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    }
    ++iterations;
    ASSERT_LE(iterations, kExpectedIterations);
  }
  ASSERT_EQ(iterations, kExpectedIterations);
}

// Verify that the time it takes for the estimator to reduce the bitrate when
// the capacity is tightened stays the same.
TEST_F(RemoteBitrateEstimatorTest, TestCapacityDropRtpTimestamps) {
  const unsigned int kSsrc = 0;
  const int kNumberOfFrames= 300;
  const int kStartBitrate = 900e3;
  const int kMinExpectedBitrate = 800e3;
  const int kMaxExpectedBitrate = 1500e3;
  // Run in steady state to make the estimator converge.
  stream_generator_->SetCapacity(1000e3);
  unsigned int bitrate_bps = SteadyStateRun(kSsrc, kNumberOfFrames,
                                            kStartBitrate, kMinExpectedBitrate,
                                            kMaxExpectedBitrate);
  // Reduce the capacity and verify the decrease time.
  stream_generator_->SetCapacity(500e3);
  int64_t bitrate_drop_time = 0;
  for (int i = 0; i < 1000; ++i) {
    GenerateAndProcessFrame(kSsrc, bitrate_bps);
    // Check for either increase or decrease.
    if (bitrate_observer_->updated()) {
      if (bitrate_observer_->latest_bitrate() <= 500e3) {
        bitrate_drop_time = stream_generator_->TimeNow();
      }
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    }
  }
  EXPECT_EQ(42900, bitrate_drop_time);
}

// Verify that the time it takes for the estimator to reduce the bitrate when
// the capacity is tightened stays the same. This test also verifies that we
// handle wrap-arounds in this scenario.
TEST_F(RemoteBitrateEstimatorTest, TestCapacityDropRtpTimestampsWrap) {
  const unsigned int kSsrc = 0;
  const int kFramerate= 30;
  const int kStartBitrate = 900e3;
  const int kMinExpectedBitrate = 800e3;
  const int kMaxExpectedBitrate = 1500e3;
  const int kSteadyStateTime = 10;  // Seconds.
  // Trigger wrap right after the steady state run.
  stream_generator_->SetRtpTimestampOffset(
      std::numeric_limits<uint32_t>::max() - kSteadyStateTime * 90000);
  // Run in steady state to make the estimator converge.
  unsigned int bitrate_bps = SteadyStateRun(kSsrc,
                                            kSteadyStateTime * kFramerate,
                                            kStartBitrate,
                                            kMinExpectedBitrate,
                                            kMaxExpectedBitrate);
  // Reduce the capacity and verify the decrease time.
  stream_generator_->SetCapacity(500e3);
  int64_t bitrate_drop_time = 0;
  for (int i = 0; i < 1000; ++i) {
    GenerateAndProcessFrame(kSsrc, bitrate_bps);
    // Check for either increase or decrease.
    if (bitrate_observer_->updated()) {
      if (bitrate_observer_->latest_bitrate() <= 500e3) {
        bitrate_drop_time = stream_generator_->TimeNow();
      }
      bitrate_bps = bitrate_observer_->latest_bitrate();
      bitrate_observer_->Reset();
    }
  }
  EXPECT_EQ(42900, bitrate_drop_time);
}

}  // namespace webrtc
