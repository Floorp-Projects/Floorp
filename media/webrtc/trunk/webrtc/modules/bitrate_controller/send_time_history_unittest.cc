/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <limits>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/modules/bitrate_controller/send_time_history.h"
#include "webrtc/system_wrappers/interface/clock.h"

namespace webrtc {

static const int kDefaultHistoryLengthMs = 1000;

class SendTimeHistoryTest : public ::testing::Test {
 protected:
  SendTimeHistoryTest() : history_(kDefaultHistoryLengthMs), clock_(0) {}
  ~SendTimeHistoryTest() {}

  virtual void SetUp() {}

  virtual void TearDown() {}

  SendTimeHistory history_;
  webrtc::SimulatedClock clock_;
};

TEST_F(SendTimeHistoryTest, AddRemoveOne) {
  const uint16_t kSeqNo = 1;
  const int64_t kTimestamp = 2;
  history_.AddAndRemoveOldSendTimes(kSeqNo, kTimestamp);

  int64_t time = 0;
  EXPECT_TRUE(history_.GetSendTime(kSeqNo, &time, false));
  EXPECT_EQ(kTimestamp, time);

  time = 0;
  EXPECT_TRUE(history_.GetSendTime(kSeqNo, &time, true));
  EXPECT_EQ(kTimestamp, time);

  time = 0;
  EXPECT_FALSE(history_.GetSendTime(kSeqNo, &time, true));
}

TEST_F(SendTimeHistoryTest, AddThenRemoveOutOfOrder) {
  struct Timestamp {
    Timestamp(uint16_t sequence_number, int64_t timestamp)
        : sequence_number(sequence_number), timestamp(timestamp) {}
    uint16_t sequence_number;
    int64_t timestamp;
  };
  std::vector<Timestamp> timestamps;
  const size_t num_items = 100;
  for (size_t i = 0; i < num_items; ++i) {
    timestamps.push_back(
        Timestamp(static_cast<uint16_t>(i), static_cast<int64_t>(i)));
  }
  std::vector<Timestamp> randomized_timestamps = timestamps;
  std::random_shuffle(randomized_timestamps.begin(),
                      randomized_timestamps.end());
  for (size_t i = 0; i < num_items; ++i) {
    history_.AddAndRemoveOldSendTimes(timestamps[i].sequence_number,
                                      timestamps[i].timestamp);
  }
  for (size_t i = 0; i < num_items; ++i) {
    int64_t timestamp;
    EXPECT_TRUE(history_.GetSendTime(randomized_timestamps[i].sequence_number,
                                     &timestamp, false));
    EXPECT_EQ(randomized_timestamps[i].timestamp, timestamp);
    EXPECT_TRUE(history_.GetSendTime(randomized_timestamps[i].sequence_number,
                                     &timestamp, true));
  }
  for (size_t i = 0; i < num_items; ++i) {
    int64_t timestamp;
    EXPECT_FALSE(
        history_.GetSendTime(timestamps[i].sequence_number, &timestamp, false));
  }
}

TEST_F(SendTimeHistoryTest, HistorySize) {
  const int kItems = kDefaultHistoryLengthMs / 100;
  for (int i = 0; i < kItems; ++i) {
    history_.AddAndRemoveOldSendTimes(i, i * 100);
  }
  int64_t timestamp;
  for (int i = 0; i < kItems; ++i) {
    EXPECT_TRUE(history_.GetSendTime(i, &timestamp, false));
    EXPECT_EQ(i * 100, timestamp);
  }
  history_.AddAndRemoveOldSendTimes(kItems, kItems * 100);
  EXPECT_FALSE(history_.GetSendTime(0, &timestamp, false));
  for (int i = 1; i < (kItems + 1); ++i) {
    EXPECT_TRUE(history_.GetSendTime(i, &timestamp, false));
    EXPECT_EQ(i * 100, timestamp);
  }
}

TEST_F(SendTimeHistoryTest, HistorySizeWithWraparound) {
  const int kMaxSeqNo = std::numeric_limits<uint16_t>::max();
  history_.AddAndRemoveOldSendTimes(kMaxSeqNo - 2, 0);
  history_.AddAndRemoveOldSendTimes(kMaxSeqNo - 1, 100);
  history_.AddAndRemoveOldSendTimes(kMaxSeqNo, 200);
  history_.AddAndRemoveOldSendTimes(0, 1000);
  int64_t timestamp;
  EXPECT_FALSE(history_.GetSendTime(kMaxSeqNo - 2, &timestamp, false));
  EXPECT_TRUE(history_.GetSendTime(kMaxSeqNo - 1, &timestamp, false));
  EXPECT_TRUE(history_.GetSendTime(kMaxSeqNo, &timestamp, false));
  EXPECT_TRUE(history_.GetSendTime(0, &timestamp, false));

  // Create a gap (kMaxSeqNo - 1) -> 0.
  EXPECT_TRUE(history_.GetSendTime(kMaxSeqNo, &timestamp, true));

  history_.AddAndRemoveOldSendTimes(1, 1100);

  EXPECT_FALSE(history_.GetSendTime(kMaxSeqNo - 2, &timestamp, false));
  EXPECT_FALSE(history_.GetSendTime(kMaxSeqNo - 1, &timestamp, false));
  EXPECT_FALSE(history_.GetSendTime(kMaxSeqNo, &timestamp, false));
  EXPECT_TRUE(history_.GetSendTime(0, &timestamp, false));
  EXPECT_TRUE(history_.GetSendTime(1, &timestamp, false));
}

TEST_F(SendTimeHistoryTest, InterlievedGetAndRemove) {
  const uint16_t kSeqNo = 1;
  const int64_t kTimestamp = 2;

  history_.AddAndRemoveOldSendTimes(kSeqNo, kTimestamp);
  history_.AddAndRemoveOldSendTimes(kSeqNo + 1, kTimestamp + 1);

  int64_t time = 0;
  EXPECT_TRUE(history_.GetSendTime(kSeqNo, &time, true));
  EXPECT_EQ(kTimestamp, time);

  history_.AddAndRemoveOldSendTimes(kSeqNo + 2, kTimestamp + 2);

  EXPECT_TRUE(history_.GetSendTime(kSeqNo + 1, &time, true));
  EXPECT_EQ(kTimestamp + 1, time);
  EXPECT_TRUE(history_.GetSendTime(kSeqNo + 2, &time, true));
  EXPECT_EQ(kTimestamp + 2, time);
}

}  // namespace webrtc
