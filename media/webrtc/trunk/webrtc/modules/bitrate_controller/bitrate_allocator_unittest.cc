/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_allocator.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"

namespace webrtc {

class TestBitrateObserver : public BitrateObserver {
 public:
  TestBitrateObserver()
      : last_bitrate_(0), last_fraction_loss_(0), last_rtt_(0) {}

  virtual void OnNetworkChanged(uint32_t bitrate,
                                uint8_t fraction_loss,
                                int64_t rtt) {
    last_bitrate_ = bitrate;
    last_fraction_loss_ = fraction_loss;
    last_rtt_ = rtt;
  }
  uint32_t last_bitrate_;
  uint8_t last_fraction_loss_;
  int64_t last_rtt_;
};

class BitrateAllocatorTest : public ::testing::Test {
 protected:
  BitrateAllocatorTest() : allocator_(new BitrateAllocator()) {
    allocator_->OnNetworkChanged(300000u, 0, 0);
  }
  ~BitrateAllocatorTest() {}

  rtc::scoped_ptr<BitrateAllocator> allocator_;
};

TEST_F(BitrateAllocatorTest, UpdatingBitrateObserver) {
  TestBitrateObserver bitrate_observer;
  int start_bitrate;
  allocator_->AddBitrateObserver(&bitrate_observer, 200000, 100000, 1500000,
                                 &start_bitrate);
  EXPECT_EQ(300000, start_bitrate);
  allocator_->OnNetworkChanged(200000, 0, 0);
  EXPECT_EQ(200000u, bitrate_observer.last_bitrate_);

  allocator_->AddBitrateObserver(&bitrate_observer, 1500000, 100000, 1500000,
                                 &start_bitrate);
  EXPECT_EQ(1500000, start_bitrate);
  allocator_->OnNetworkChanged(1500000, 0, 0);
  EXPECT_EQ(1500000u, bitrate_observer.last_bitrate_);

  allocator_->AddBitrateObserver(&bitrate_observer, 500000, 100000, 1500000,
                                 &start_bitrate);
  EXPECT_EQ(1500000, start_bitrate);
  allocator_->OnNetworkChanged(1500000, 0, 0);
  EXPECT_EQ(1500000u, bitrate_observer.last_bitrate_);
}

TEST_F(BitrateAllocatorTest, TwoBitrateObserversOneRtcpObserver) {
  TestBitrateObserver bitrate_observer_1;
  TestBitrateObserver bitrate_observer_2;
  int start_bitrate;
  allocator_->AddBitrateObserver(&bitrate_observer_1, 200000, 100000, 300000,
                                 &start_bitrate);
  EXPECT_EQ(300000, start_bitrate);
  allocator_->AddBitrateObserver(&bitrate_observer_2, 200000, 200000, 300000,
                                 &start_bitrate);
  EXPECT_EQ(200000, start_bitrate);

  // Test too low start bitrate, hence lower than sum of min. Min bitrates will
  // be allocated to all observers.
  allocator_->OnNetworkChanged(200000, 0, 50);
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer_1.last_fraction_loss_);
  EXPECT_EQ(50, bitrate_observer_1.last_rtt_);
  EXPECT_EQ(200000u, bitrate_observer_2.last_bitrate_);
  EXPECT_EQ(0, bitrate_observer_2.last_fraction_loss_);
  EXPECT_EQ(50, bitrate_observer_2.last_rtt_);

  // Test a bitrate which should be distributed equally.
  allocator_->OnNetworkChanged(500000, 0, 50);
  const uint32_t kBitrateToShare = 500000 - 200000 - 100000;
  EXPECT_EQ(100000u + kBitrateToShare / 2, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(200000u + kBitrateToShare / 2, bitrate_observer_2.last_bitrate_);

  // Limited by 2x max bitrates since we leave room for FEC and retransmissions.
  allocator_->OnNetworkChanged(1500000, 0, 50);
  EXPECT_EQ(600000u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(600000u, bitrate_observer_2.last_bitrate_);
}

class BitrateAllocatorTestNoEnforceMin : public ::testing::Test {
 protected:
  BitrateAllocatorTestNoEnforceMin() : allocator_(new BitrateAllocator()) {
    allocator_->EnforceMinBitrate(false);
    allocator_->OnNetworkChanged(300000u, 0, 0);
  }
  ~BitrateAllocatorTestNoEnforceMin() {}

  rtc::scoped_ptr<BitrateAllocator> allocator_;
};

// The following three tests verify that the EnforceMinBitrate() method works
// as intended.
TEST_F(BitrateAllocatorTestNoEnforceMin, OneBitrateObserver) {
  TestBitrateObserver bitrate_observer_1;
  int start_bitrate;
  allocator_->AddBitrateObserver(&bitrate_observer_1, 200000, 100000, 400000,
                                 &start_bitrate);
  EXPECT_EQ(300000, start_bitrate);

  // High REMB.
  allocator_->OnNetworkChanged(150000, 0, 0);
  EXPECT_EQ(150000u, bitrate_observer_1.last_bitrate_);

  // Low REMB.
  allocator_->OnNetworkChanged(10000, 0, 0);
  EXPECT_EQ(10000u, bitrate_observer_1.last_bitrate_);

  allocator_->RemoveBitrateObserver(&bitrate_observer_1);
}

TEST_F(BitrateAllocatorTestNoEnforceMin, ThreeBitrateObservers) {
  TestBitrateObserver bitrate_observer_1;
  TestBitrateObserver bitrate_observer_2;
  TestBitrateObserver bitrate_observer_3;
  // Set up the observers with min bitrates at 100000, 200000, and 300000.
  int start_bitrate;
  allocator_->AddBitrateObserver(&bitrate_observer_1, 200000, 100000, 400000,
                                 &start_bitrate);
  EXPECT_EQ(300000, start_bitrate);

  allocator_->AddBitrateObserver(&bitrate_observer_2, 200000, 200000, 400000,
                                 &start_bitrate);
  EXPECT_EQ(200000, start_bitrate);
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_);

  allocator_->AddBitrateObserver(&bitrate_observer_3, 200000, 300000, 400000,
                                 &start_bitrate);
  EXPECT_EQ(0, start_bitrate);
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(200000u, bitrate_observer_2.last_bitrate_);

  // High REMB. Make sure the controllers get a fair share of the surplus
  // (i.e., what is left after each controller gets its min rate).
  allocator_->OnNetworkChanged(690000, 0, 0);
  // Verify that each observer gets its min rate (sum of min rates is 600000),
  // and that the remaining 90000 is divided equally among the three.
  uint32_t bitrate_to_share = 690000u - 100000u - 200000u - 300000u;
  EXPECT_EQ(100000u + bitrate_to_share / 3, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(200000u + bitrate_to_share / 3, bitrate_observer_2.last_bitrate_);
  EXPECT_EQ(300000u + bitrate_to_share / 3, bitrate_observer_3.last_bitrate_);

  // High REMB, but below the sum of min bitrates.
  allocator_->OnNetworkChanged(500000, 0, 0);
  // Verify that the first and second observers get their min bitrates, and the
  // third gets the remainder.
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_);  // Min bitrate.
  EXPECT_EQ(200000u, bitrate_observer_2.last_bitrate_);  // Min bitrate.
  EXPECT_EQ(200000u, bitrate_observer_3.last_bitrate_);  // Remainder.

  // Low REMB.
  allocator_->OnNetworkChanged(10000, 0, 0);
  // Verify that the first observer gets all the rate, and the rest get zero.
  EXPECT_EQ(10000u, bitrate_observer_1.last_bitrate_);
  EXPECT_EQ(0u, bitrate_observer_2.last_bitrate_);
  EXPECT_EQ(0u, bitrate_observer_3.last_bitrate_);

  allocator_->RemoveBitrateObserver(&bitrate_observer_1);
  allocator_->RemoveBitrateObserver(&bitrate_observer_2);
  allocator_->RemoveBitrateObserver(&bitrate_observer_3);
}

TEST_F(BitrateAllocatorTest, ThreeBitrateObserversLowRembEnforceMin) {
  TestBitrateObserver bitrate_observer_1;
  TestBitrateObserver bitrate_observer_2;
  TestBitrateObserver bitrate_observer_3;
  int start_bitrate;
  allocator_->AddBitrateObserver(&bitrate_observer_1, 200000, 100000, 400000,
                                 &start_bitrate);
  EXPECT_EQ(300000, start_bitrate);

  allocator_->AddBitrateObserver(&bitrate_observer_2, 200000, 200000, 400000,
                                 &start_bitrate);
  EXPECT_EQ(200000, start_bitrate);
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_);

  allocator_->AddBitrateObserver(&bitrate_observer_3, 200000, 300000, 400000,
                                 &start_bitrate);
  EXPECT_EQ(300000, start_bitrate);
  EXPECT_EQ(100000, static_cast<int>(bitrate_observer_1.last_bitrate_));
  EXPECT_EQ(200000, static_cast<int>(bitrate_observer_2.last_bitrate_));

  // Low REMB. Verify that all observers still get their respective min bitrate.
  allocator_->OnNetworkChanged(1000, 0, 0);
  EXPECT_EQ(100000u, bitrate_observer_1.last_bitrate_);  // Min cap.
  EXPECT_EQ(200000u, bitrate_observer_2.last_bitrate_);  // Min cap.
  EXPECT_EQ(300000u, bitrate_observer_3.last_bitrate_);  // Min cap.

  allocator_->RemoveBitrateObserver(&bitrate_observer_1);
  allocator_->RemoveBitrateObserver(&bitrate_observer_2);
  allocator_->RemoveBitrateObserver(&bitrate_observer_3);
}
}  // namespace webrtc
