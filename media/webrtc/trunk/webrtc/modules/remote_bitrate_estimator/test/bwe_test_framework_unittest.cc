/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_framework.h"

#include <numeric>

#include "gtest/gtest.h"
#include "webrtc/system_wrappers/interface/constructor_magic.h"
#include "webrtc/test/testsupport/fileutils.h"

using std::vector;

namespace webrtc {
namespace testing {
namespace bwe {

TEST(BweTestFramework_RandomTest, Gaussian) {
  enum {
    kN = 100000,
    kBuckets = 100,
    kMean = 49,
    kStddev = 10
  };

  Random random(0x12345678);

  int buckets[kBuckets] = {0};
  for (int i = 0; i < kN; ++i) {
    int index = random.Gaussian(kMean, kStddev);
    if (index >= 0 && index < kBuckets) {
      buckets[index]++;
    }
  }

  const double kPi = 3.14159265358979323846;
  const double kScale = kN / (kStddev * std::sqrt(2.0 * kPi));
  const double kDiv = -2.0 * kStddev * kStddev;
  double self_corr = 0.0;
  double bucket_corr = 0.0;
  for (int n = 0; n < kBuckets; ++n) {
    double normal_dist = kScale * std::exp((n - kMean) * (n - kMean) / kDiv);
    self_corr += normal_dist * normal_dist;
    bucket_corr += normal_dist * buckets[n];
  }
  printf("Correlation: %f (random sample), %f (self), %f (quotient)\n",
         bucket_corr, self_corr, bucket_corr / self_corr);
  EXPECT_NEAR(1.0, bucket_corr / self_corr, 0.0004);
}

static bool IsSequenceNumberSorted(const Packets& packets) {
  PacketsConstIt last_it = packets.begin();
  for (PacketsConstIt it = last_it; it != packets.end(); ++it) {
    if (IsNewerSequenceNumber(last_it->header().sequenceNumber,
                              it->header().sequenceNumber)) {
      return false;
    }
    last_it = it;
  }
  return true;
}

TEST(BweTestFramework_PacketTest, IsTimeSorted) {
  Packets packets;
  // Insert some packets in order...
  EXPECT_TRUE(IsTimeSorted(packets));

  packets.push_back(Packet(100, 0));
  EXPECT_TRUE(IsTimeSorted(packets));

  packets.push_back(Packet(110, 0));
  EXPECT_TRUE(IsTimeSorted(packets));

  // ...and one out-of-order...
  packets.push_back(Packet(100, 0));
  EXPECT_FALSE(IsTimeSorted(packets));

  // ...remove the out-of-order packet, insert another in-order packet.
  packets.pop_back();
  packets.push_back(Packet(120, 0));
  EXPECT_TRUE(IsTimeSorted(packets));
}

TEST(BweTestFramework_PacketTest, IsSequenceNumberSorted) {
  Packets packets;
  // Insert some packets in order...
  EXPECT_TRUE(IsSequenceNumberSorted(packets));

  packets.push_back(Packet(0, 100));
  EXPECT_TRUE(IsSequenceNumberSorted(packets));

  packets.push_back(Packet(0, 110));
  EXPECT_TRUE(IsSequenceNumberSorted(packets));

  // ...and one out-of-order...
  packets.push_back(Packet(0, 100));
  EXPECT_FALSE(IsSequenceNumberSorted(packets));

  // ...remove the out-of-order packet, insert another in-order packet.
  packets.pop_back();
  packets.push_back(Packet(0, 120));
  EXPECT_TRUE(IsSequenceNumberSorted(packets));
}

TEST(BweTestFramework_StatsTest, Mean) {
  Stats<int32_t> stats;
  EXPECT_EQ(0, stats.GetMean());

  stats.Push(1);
  stats.Push(3);
  EXPECT_EQ(2, stats.GetMean());

  // Integer division rounds (1+3-3)/3 to 0.
  stats.Push(-3);
  EXPECT_EQ(0, stats.GetMean());
}

TEST(BweTestFramework_StatsTest, Variance) {
  Stats<int32_t> stats;
  EXPECT_EQ(0, stats.GetVariance());

  // Mean is 2 ; ((1-2)*(1-2)+(3-2)*(3-2))/2 = (1+1)/2 = 1
  stats.Push(1);
  stats.Push(3);
  EXPECT_EQ(1, stats.GetVariance());

  // Integer division rounds 26/3 to 8
  // Mean is 0 ; (1*1+3*3+(-4)*(-4))/3 = (1+9+16)/3 = 8
  stats.Push(-4);
  EXPECT_EQ(8, stats.GetVariance());
}

TEST(BweTestFramework_StatsTest, StdDev) {
  Stats<int32_t> stats;
  EXPECT_EQ(0, stats.GetStdDev());

  // Variance is 1 ; sqrt(1) = 1
  stats.Push(1);
  stats.Push(3);
  EXPECT_EQ(1, stats.GetStdDev());

  // Variance is 8 ; sqrt(8) = 2 with integers.
  stats.Push(-4);
  EXPECT_EQ(2, stats.GetStdDev());
}

TEST(BweTestFramework_StatsTest, MinMax) {
  Stats<int32_t> stats;
  EXPECT_EQ(0, stats.GetMin());
  EXPECT_EQ(0, stats.GetMax());

  stats.Push(1);
  EXPECT_EQ(1, stats.GetMin());
  EXPECT_EQ(1, stats.GetMax());

  stats.Push(3);
  EXPECT_EQ(1, stats.GetMin());
  EXPECT_EQ(3, stats.GetMax());

  stats.Push(-4);
  EXPECT_EQ(-4, stats.GetMin());
  EXPECT_EQ(3, stats.GetMax());
}

class BweTestFramework_RateCounterFilterTest : public ::testing::Test {
 public:
  BweTestFramework_RateCounterFilterTest()
    : filter_(NULL),
      now_ms_(0) {
  }
  virtual ~BweTestFramework_RateCounterFilterTest() {}

 protected:
  void TestRateCounter(int64_t run_for_ms, uint32_t payload_bits,
                       uint32_t expected_pps, uint32_t expected_bps) {
    Packets packets;
    RTPHeader header = {0};
    // "Send" a packet every 10 ms.
    for (int64_t i = 0; i < run_for_ms; i += 10, now_ms_ += 10) {
      packets.push_back(Packet(now_ms_ * 1000, payload_bits / 8, header));
    }
    filter_.RunFor(run_for_ms, &packets);
    ASSERT_TRUE(IsTimeSorted(packets));
    EXPECT_EQ(expected_pps, filter_.packets_per_second());
    EXPECT_EQ(expected_bps, filter_.bits_per_second());
  }

 private:
  RateCounterFilter filter_;
  int64_t now_ms_;

  DISALLOW_COPY_AND_ASSIGN(BweTestFramework_RateCounterFilterTest);
};

TEST_F(BweTestFramework_RateCounterFilterTest, Short) {
  // 100ms, 100 bytes per packet, should result in 10 pps and 8 kbps. We're
  // generating one packet every 10 ms ; 10 * 800 = 8k
  TestRateCounter(100, 800, 10, 8000);
}

TEST_F(BweTestFramework_RateCounterFilterTest, Medium) {
  // 100ms, like above.
  TestRateCounter(100, 800, 10, 8000);
  // 1000ms, 100 bpp, should result in 100 pps and 80 kbps. We're still
  // generating packets every 10 ms.
  TestRateCounter(900, 800, 100, 80000);
}

TEST_F(BweTestFramework_RateCounterFilterTest, Long) {
  // 100ms, 1000ms, like above.
  TestRateCounter(100, 800, 10, 8000);
  TestRateCounter(900, 800, 100, 80000);
  // 2000ms, should only see rate of last second, so 100 pps, and 40 kbps now.
  TestRateCounter(1000, 400, 100, 40000);
  // 2500ms, half a second with zero payload size. We should get same pps as
  // before, but kbps should drop to half of previous rate.
  TestRateCounter(500, 0, 100, 20000);
  // Another half second with zero payload size. Now the kbps rate should drop
  // to zero.
  TestRateCounter(500, 0, 100, 0);
  // Increate payload size again. 200 * 100 * 0.5 = 10 kbps.
  TestRateCounter(500, 200, 100, 10000);
}

static void TestLossFilter(float loss_percent, bool zero_tolerance) {
  LossFilter filter(NULL);
  filter.SetLoss(loss_percent);
  Packets::size_type sent_packets = 0;
  Packets::size_type remaining_packets = 0;

  // No input should yield no output
  {
    Packets packets;
    sent_packets += packets.size();
    filter.RunFor(0, &packets);
    ASSERT_TRUE(IsTimeSorted(packets));
    ASSERT_TRUE(IsSequenceNumberSorted(packets));
    remaining_packets += packets.size();
    EXPECT_EQ(0u, sent_packets);
    EXPECT_EQ(0u, remaining_packets);
  }

  // Generate and process 10000 packets in different batch sizes (some empty)
  for (int i = 0; i < 2225; ++i) {
    Packets packets;
    packets.insert(packets.end(), i % 10, Packet());
    sent_packets += packets.size();
    filter.RunFor(0, &packets);
    ASSERT_TRUE(IsTimeSorted(packets));
    ASSERT_TRUE(IsSequenceNumberSorted(packets));
    remaining_packets += packets.size();
  }

  float loss_fraction = 0.01f * (100.0f - loss_percent);
  Packets::size_type expected_packets = loss_fraction * sent_packets;
  if (zero_tolerance) {
    EXPECT_EQ(expected_packets, remaining_packets);
  } else {
    // Require within 1% of expected
    EXPECT_NEAR(expected_packets, remaining_packets, 100);
  }
}

TEST(BweTestFramework_LossFilterTest, Loss0) {
  // With 0% loss, the result should be exact (no loss).
  TestLossFilter(0.0f, true);
}

TEST(BweTestFramework_LossFilterTest, Loss10) {
  TestLossFilter(10.0f, false);
}

TEST(BweTestFramework_LossFilterTest, Loss50) {
  TestLossFilter(50.0f, false);
}

TEST(BweTestFramework_LossFilterTest, Loss100) {
  // With 100% loss, the result should be exact (no packets out).
  TestLossFilter(100.0f, true);
}

class BweTestFramework_DelayFilterTest : public ::testing::Test {
 public:
  BweTestFramework_DelayFilterTest()
    : filter_(NULL),
      now_ms_(0),
      sequence_number_(0) {
  }
  virtual ~BweTestFramework_DelayFilterTest() {}

 protected:
  void TestDelayFilter(int64_t run_for_ms, uint32_t in_packets,
                       uint32_t out_packets) {
    Packets packets;
    for (uint32_t i = 0; i < in_packets; ++i) {
      packets.push_back(Packet(now_ms_ * 1000 + (sequence_number_ >> 4),
                                  sequence_number_));
      sequence_number_++;
    }
    filter_.RunFor(run_for_ms, &packets);
    ASSERT_TRUE(IsTimeSorted(packets));
    ASSERT_TRUE(IsSequenceNumberSorted(packets));
    for (PacketsConstIt it = packets.begin(); it != packets.end(); ++it) {
      EXPECT_LE(now_ms_ * 1000, it->send_time_us());
    }
    EXPECT_EQ(out_packets, packets.size());
    accumulated_packets_.splice(accumulated_packets_.end(), packets);
    now_ms_ += run_for_ms;
  }

  void TestDelayFilter(int64_t delay_ms) {
    filter_.SetDelay(delay_ms);
    TestDelayFilter(1, 0, 0);    // No input should yield no output

    // Single packet
    TestDelayFilter(0, 1, 1);
    TestDelayFilter(delay_ms, 0, 0);

    for (int i = 0; i < delay_ms; ++i) {
      filter_.SetDelay(i);
      TestDelayFilter(1, 10, 10);
    }
    TestDelayFilter(0, 0, 0);
    TestDelayFilter(delay_ms, 0, 0);

    // Wait a little longer - should still see no output
    TestDelayFilter(delay_ms, 0, 0);

    for (int i = 1; i < delay_ms + 1; ++i) {
      filter_.SetDelay(i);
      TestDelayFilter(1, 5, 5);
    }
    TestDelayFilter(0, 0, 0);
    filter_.SetDelay(2 * delay_ms);
    TestDelayFilter(1, 0, 0);
    TestDelayFilter(delay_ms, 13, 13);
    TestDelayFilter(delay_ms, 0, 0);

    // Wait a little longer - should still see no output
    TestDelayFilter(delay_ms, 0, 0);

    for (int i = 0; i < 2 * delay_ms; ++i) {
      filter_.SetDelay(2 * delay_ms - i - 1);
      TestDelayFilter(1, 5, 5);
    }
    TestDelayFilter(0, 0, 0);
    filter_.SetDelay(0);
    TestDelayFilter(0, 7, 7);

    ASSERT_TRUE(IsTimeSorted(accumulated_packets_));
    ASSERT_TRUE(IsSequenceNumberSorted(accumulated_packets_));
  }

  DelayFilter filter_;
  Packets accumulated_packets_;

 private:
  int64_t now_ms_;
  uint32_t sequence_number_;

  DISALLOW_COPY_AND_ASSIGN(BweTestFramework_DelayFilterTest);
};

TEST_F(BweTestFramework_DelayFilterTest, Delay0) {
  TestDelayFilter(1, 0, 0);    // No input should yield no output
  TestDelayFilter(1, 10, 10);  // Expect no delay (delay time is zero)
  TestDelayFilter(1, 0, 0);    // Check no packets are still in buffer
  filter_.SetDelay(0);
  TestDelayFilter(1, 5, 5);    // Expect no delay (delay time is zero)
  TestDelayFilter(1, 0, 0);    // Check no packets are still in buffer
}

TEST_F(BweTestFramework_DelayFilterTest, Delay1) {
  TestDelayFilter(1);
}

TEST_F(BweTestFramework_DelayFilterTest, Delay2) {
  TestDelayFilter(2);
}

TEST_F(BweTestFramework_DelayFilterTest, Delay20) {
  TestDelayFilter(20);
}

TEST_F(BweTestFramework_DelayFilterTest, Delay100) {
  TestDelayFilter(100);
}

TEST_F(BweTestFramework_DelayFilterTest, JumpToZeroDelay) {
  DelayFilter delay(NULL);
  Packets acc;
  Packets packets;

  // Delay a bunch of packets, accumulate them to the 'acc' list.
  delay.SetDelay(100.0f);
  for (uint32_t i = 0; i < 10; ++i) {
    packets.push_back(Packet(i * 100, i));
  }
  delay.RunFor(1000, &packets);
  acc.splice(acc.end(), packets);
  ASSERT_TRUE(IsTimeSorted(acc));
  ASSERT_TRUE(IsSequenceNumberSorted(acc));

  // Drop delay to zero, send a few more packets through the delay, append them
  // to the 'acc' list and verify that it is all sorted.
  delay.SetDelay(0.0f);
  for (uint32_t i = 10; i < 50; ++i) {
    packets.push_back(Packet(i * 100, i));
  }
  delay.RunFor(1000, &packets);
  acc.splice(acc.end(), packets);
  ASSERT_TRUE(IsTimeSorted(acc));
  ASSERT_TRUE(IsSequenceNumberSorted(acc));
}

TEST_F(BweTestFramework_DelayFilterTest, IncreasingDelay) {
  // Gradually increase delay.
  for (int i = 1; i < 50; i += 4) {
    TestDelayFilter(i);
  }
  // Reach a steady state.
  filter_.SetDelay(100);
  TestDelayFilter(1, 20, 20);
  TestDelayFilter(2, 0, 0);
  TestDelayFilter(99, 20, 20);
  // Drop delay back down to zero.
  filter_.SetDelay(0);
  TestDelayFilter(1, 100, 100);
  TestDelayFilter(23010, 0, 0);
  ASSERT_TRUE(IsTimeSorted(accumulated_packets_));
  ASSERT_TRUE(IsSequenceNumberSorted(accumulated_packets_));
}

static void TestJitterFilter(int64_t stddev_jitter_ms) {
  JitterFilter filter(NULL);
  filter.SetJitter(stddev_jitter_ms);

  int64_t now_ms = 0;
  uint32_t sequence_number = 0;

  // Generate packets, add jitter to them, accumulate the altered packets.
  Packets original;
  Packets jittered;
  for (uint32_t i = 0; i < 1000; ++i) {
    Packets packets;
    for (uint32_t j = 0; j < i % 100; ++j) {
      packets.push_back(Packet(now_ms * 1000, sequence_number++));
      now_ms += 5 * stddev_jitter_ms;
    }
    original.insert(original.end(), packets.begin(), packets.end());
    filter.RunFor(stddev_jitter_ms, &packets);
    jittered.splice(jittered.end(), packets);
  }

  // Jittered packets should still be in order.
  ASSERT_TRUE(IsTimeSorted(original));
  ASSERT_TRUE(IsTimeSorted(jittered));
  ASSERT_TRUE(IsSequenceNumberSorted(original));
  ASSERT_TRUE(IsSequenceNumberSorted(jittered));
  EXPECT_EQ(original.size(), jittered.size());

  // Make sure jittered and original packets are in same order. Collect time
  // difference (jitter) in stats, then check that mean jitter is close to zero
  // and standard deviation of jitter is what we set it to.
  Stats<double> jitter_us;
  for (PacketsIt it1 = original.begin(), it2 = jittered.begin();
       it1 != original.end() && it2 != jittered.end(); ++it1, ++it2) {
    EXPECT_EQ(it1->header().sequenceNumber, it2->header().sequenceNumber);
    jitter_us.Push(it2->send_time_us() - it1->send_time_us());
  }
  EXPECT_NEAR(0.0, jitter_us.GetMean(), stddev_jitter_ms * 1000.0 * 0.008);
  EXPECT_NEAR(stddev_jitter_ms * 1000.0, jitter_us.GetStdDev(),
              stddev_jitter_ms * 1000.0 * 0.02);
}

TEST(BweTestFramework_JitterFilterTest, Jitter0) {
  TestJitterFilter(0);
}

TEST(BweTestFramework_JitterFilterTest, Jitter1) {
  TestJitterFilter(1);
}

TEST(BweTestFramework_JitterFilterTest, Jitter5) {
  TestJitterFilter(5);
}

TEST(BweTestFramework_JitterFilterTest, Jitter10) {
  TestJitterFilter(10);
}

TEST(BweTestFramework_JitterFilterTest, Jitter1031) {
  TestJitterFilter(1031);
}

static void TestReorderFilter(uint32_t reorder_percent, uint32_t near) {
  const uint32_t kPacketCount = 10000;

  // Generate packets with 10 ms interval.
  Packets packets;
  int64_t now_ms = 0;
  uint32_t sequence_number = 1;
  for (uint32_t i = 0; i < kPacketCount; ++i, now_ms += 10) {
    packets.push_back(Packet(now_ms * 1000, sequence_number++));
  }
  ASSERT_TRUE(IsTimeSorted(packets));
  ASSERT_TRUE(IsSequenceNumberSorted(packets));

  // Reorder packets, verify that send times are still in order.
  ReorderFilter filter(NULL);
  filter.SetReorder(reorder_percent);
  filter.RunFor(now_ms, &packets);
  ASSERT_TRUE(IsTimeSorted(packets));

  // We measure the amount of reordering by summing the distance by which out-
  // of-order packets have been moved in the stream.
  uint32_t distance = 0;
  uint32_t last_sequence_number = 0;
  for (PacketsIt it = packets.begin(); it != packets.end(); ++it) {
    uint32_t sequence_number = it->header().sequenceNumber;
    if (sequence_number < last_sequence_number) {
      distance += last_sequence_number - sequence_number;
    }
    last_sequence_number = sequence_number;
  }

  // Because reordering is random, we allow a threshold when comparing. The
  // maximum distance a packet can be moved is PacketCount - 1.
  EXPECT_NEAR(((kPacketCount - 1) * reorder_percent) / 100, distance, near);
}

TEST(BweTestFramework_ReorderFilterTest, Reorder0) {
  // For 0% reordering, no packets should have been moved, so result is exact.
  TestReorderFilter(0, 0);
}

TEST(BweTestFramework_ReorderFilterTest, Reorder10) {
  TestReorderFilter(10, 30);
}

TEST(BweTestFramework_ReorderFilterTest, Reorder20) {
  TestReorderFilter(20, 20);
}

TEST(BweTestFramework_ReorderFilterTest, Reorder50) {
  TestReorderFilter(50, 20);
}

TEST(BweTestFramework_ReorderFilterTest, Reorder70) {
  TestReorderFilter(70, 20);
}

TEST(BweTestFramework_ReorderFilterTest, Reorder100) {
  // Note that because the implementation works by optionally swapping two
  // adjacent packets, when the likelihood of a swap is 1.0, a swap will always
  // occur, so the stream will be in order except for the first packet, which
  // has been moved to the end. Therefore we expect the result to be exact here.
  TestReorderFilter(100.0, 0);
}

class BweTestFramework_ChokeFilterTest : public ::testing::Test {
 public:
  BweTestFramework_ChokeFilterTest()
    : now_ms_(0),
      sequence_number_(0),
      output_packets_(),
      send_times_us_() {
  }
  virtual ~BweTestFramework_ChokeFilterTest() {}

 protected:
  void TestChoke(PacketProcessor* filter,
                 int64_t run_for_ms,
                 uint32_t packets_to_generate,
                 uint32_t expected_kbit_transmitted) {
    // Generate a bunch of packets, apply choke, verify output is ordered.
    Packets packets;
    RTPHeader header = {0};
    for (uint32_t i = 0; i < packets_to_generate; ++i) {
      int64_t send_time_ms = now_ms_ + (i * run_for_ms) / packets_to_generate;
      header.sequenceNumber = sequence_number_++;
      // Payload is 1000 bits.
      packets.push_back(Packet(send_time_ms * 1000, 125, header));
      send_times_us_.push_back(send_time_ms * 1000);
    }
    ASSERT_TRUE(IsTimeSorted(packets));
    filter->RunFor(run_for_ms, &packets);
    now_ms_ += run_for_ms;
    output_packets_.splice(output_packets_.end(), packets);
    ASSERT_TRUE(IsTimeSorted(output_packets_));
    ASSERT_TRUE(IsSequenceNumberSorted(output_packets_));

    // Sum up the transmitted bytes up until the current time.
    uint32_t bytes_transmitted = 0;
    while (!output_packets_.empty()) {
      const Packet& packet = output_packets_.front();
      if (packet.send_time_us() > now_ms_ * 1000) {
        break;
      }
      bytes_transmitted += packet.payload_size();
      output_packets_.pop_front();
    }
    EXPECT_EQ(expected_kbit_transmitted, (bytes_transmitted * 8) / 1000);
  }

  void CheckMaxDelay(int64_t max_delay_ms) {
    for (PacketsIt it = output_packets_.begin(); it != output_packets_.end();
        ++it) {
      const Packet& packet = *it;
      int64_t delay_us = packet.send_time_us() -
          send_times_us_[packet.header().sequenceNumber];
      EXPECT_GE(max_delay_ms * 1000, delay_us);
    }
  }

 private:
  int64_t now_ms_;
  uint32_t sequence_number_;
  Packets output_packets_;
  std::vector<int64_t> send_times_us_;

  DISALLOW_COPY_AND_ASSIGN(BweTestFramework_ChokeFilterTest);
};

TEST_F(BweTestFramework_ChokeFilterTest, Short) {
  // 100ms, 100 packets, 10 kbps choke -> 1 kbit of data should have propagated.
  // That is actually just a single packet, since each packet has 1000 bits of
  // payload.
  ChokeFilter filter(NULL);
  filter.SetCapacity(10);
  TestChoke(&filter, 100, 100, 1);
}

TEST_F(BweTestFramework_ChokeFilterTest, Medium) {
  // 100ms, 10 packets, 10 kbps choke -> 1 packet through, or 1 kbit.
  ChokeFilter filter(NULL);
  filter.SetCapacity(10);
  TestChoke(&filter, 100, 10, 1);
  // 200ms, no new packets -> another packet through.
  TestChoke(&filter, 100, 0, 1);
  // 1000ms, no new packets -> 8 more packets.
  TestChoke(&filter, 800, 0, 8);
  // 2000ms, no new packets -> queue is empty so no output.
  TestChoke(&filter, 1000, 0, 0);
}

TEST_F(BweTestFramework_ChokeFilterTest, Long) {
  // 100ms, 100 packets in queue, 10 kbps choke -> 1 packet through, or 1 kbit.
  ChokeFilter filter(NULL);
  filter.SetCapacity(10);
  TestChoke(&filter, 100, 100, 1);
  // 200ms, no input, another packet through.
  TestChoke(&filter, 100, 0, 1);
  // 1000ms, no input, 8 packets through.
  TestChoke(&filter, 800, 0, 8);
  // 10000ms, no input, raise choke to 100 kbps. Remaining 90 packets in queue
  // should be propagated, for a total of 90 kbps.
  filter.SetCapacity(100);
  TestChoke(&filter, 9000, 0, 90);
  // 10100ms, 20 more packets -> 10 packets or 10 kbit through.
  TestChoke(&filter, 100, 20, 10);
  // 10300ms, 10 more packets -> 20 packets out.
  TestChoke(&filter, 200, 10, 20);
  // 11300ms, no input, queue should be empty.
  filter.SetCapacity(10);
  TestChoke(&filter, 1000, 0, 0);
}

TEST_F(BweTestFramework_ChokeFilterTest, MaxDelay) {
  // 10 kbps choke, 500 ms delay cap
  ChokeFilter filter(NULL);
  filter.SetCapacity(10);
  filter.SetMaxDelay(500);
  // 100ms, 100 packets in queue, 10 kbps choke -> 1 packet through, or 1 kbit.
  TestChoke(&filter, 100, 100, 1);
  CheckMaxDelay(500);
  // 500ms, no input, 4 more packets through.
  TestChoke(&filter, 400, 0, 4);
  // 10000ms, no input, remaining packets should have been dropped.
  TestChoke(&filter, 9500, 0, 0);

  // 100 ms delay cap
  filter.SetMaxDelay(100);
  // 10100ms, 50 more packets -> 2 packets or 2 kbit through.
  TestChoke(&filter, 100, 50, 2);
  CheckMaxDelay(100);
  // 20000ms, no input, remaining packets in queue should have been dropped.
  TestChoke(&filter, 9900, 0, 0);

  // Reset delay cap (0 is no cap) and verify no packets are dropped.
  filter.SetCapacity(10);
  filter.SetMaxDelay(0);
  TestChoke(&filter, 100, 100, 2);
  TestChoke(&filter, 9900, 0, 98);
}

TEST_F(BweTestFramework_ChokeFilterTest, ShortTrace) {
  // According to the input file 6 packets should be transmitted within
  // 100 milliseconds.
  TraceBasedDeliveryFilter filter(NULL);
  ASSERT_TRUE(filter.Init(test::ResourcePath("synthetic-trace", "rx")));
  TestChoke(&filter, 100, 100, 6);
}

TEST_F(BweTestFramework_ChokeFilterTest, ShortTraceWrap) {
  // According to the input file 10 packets should be transmitted within
  // 140 milliseconds (at the wrapping point two packets are sent back to back).
  TraceBasedDeliveryFilter filter(NULL);
  ASSERT_TRUE(filter.Init(test::ResourcePath("synthetic-trace", "rx")));
  TestChoke(&filter, 140, 100, 10);
}

void TestVideoSender(VideoSender* sender, int64_t run_for_ms,
                     uint32_t expected_packets,
                     uint32_t expected_payload_size,
                     uint32_t expected_total_payload_size) {
  assert(sender);
  Packets packets;
  sender->RunFor(run_for_ms, &packets);
  ASSERT_TRUE(IsTimeSorted(packets));
  ASSERT_TRUE(IsSequenceNumberSorted(packets));
  EXPECT_EQ(expected_packets, packets.size());
  int64_t send_time_us = -1;
  uint32_t total_payload_size = 0;
  uint32_t absolute_send_time = 0;
  uint32_t absolute_send_time_wraps = 0;
  uint32_t rtp_timestamp = 0;
  uint32_t rtp_timestamp_wraps = 0;
  for (PacketsIt it = packets.begin(); it != packets.end(); ++it) {
    EXPECT_LE(send_time_us, it->send_time_us());
    send_time_us = it->send_time_us();
    if (sender->max_payload_size_bytes() != it->payload_size()) {
      EXPECT_EQ(expected_payload_size, it->payload_size());
    }
    total_payload_size += it->payload_size();
    if (absolute_send_time > it->header().extension.absoluteSendTime) {
      absolute_send_time_wraps++;
    }
    absolute_send_time = it->header().extension.absoluteSendTime;
    if (rtp_timestamp > it->header().timestamp) {
      rtp_timestamp_wraps++;
    }
    rtp_timestamp = it->header().timestamp;
  }
  EXPECT_EQ(expected_total_payload_size, total_payload_size);
  EXPECT_GE(1u, absolute_send_time_wraps);
  EXPECT_GE(1u, rtp_timestamp_wraps);
}

TEST(BweTestFramework_VideoSenderTest, Fps1Kpbs80_1s) {
  // 1 fps, 80 kbps
  VideoSender sender(NULL, 1.0f, 80, 0x1234, 0);
  EXPECT_EQ(10000u, sender.bytes_per_second());
  // We're at 1 fps, so all packets should be generated on first call, giving 10
  // packets of each 1000 bytes, total 10000 bytes.
  TestVideoSender(&sender, 1, 10, 1000, 10000);
  // 999ms, should see no output here.
  TestVideoSender(&sender, 998, 0, 0, 0);
  // 1999ms, should get data for one more frame.
  TestVideoSender(&sender, 1000, 10, 1000, 10000);
  // 2000ms, one more frame.
  TestVideoSender(&sender, 1, 10, 1000, 10000);
  // 2999ms, should see nothing.
  TestVideoSender(&sender, 999, 0, 0, 0);
}

TEST(BweTestFramework_VideoSenderTest, Fps1Kpbs80_1s_Offset) {
  // 1 fps, 80 kbps, offset 0.5 of a frame period, ==0.5s in this case.
  VideoSender sender(NULL, 1.0f, 80, 0x1234, 0.5f);
  EXPECT_EQ(10000u, sender.bytes_per_second());
  // 499ms, no output.
  TestVideoSender(&sender, 499, 0, 0, 0);
  // 500ms, first frame (this is the offset we set), 10 packets of 1000 bytes.
  TestVideoSender(&sender, 1, 10, 1000, 10000);
  // 1499ms, nothing.
  TestVideoSender(&sender, 999, 0, 0, 0);
  // 1999ms, second frame.
  TestVideoSender(&sender, 500, 10, 1000, 10000);
  // 2499ms, nothing.
  TestVideoSender(&sender, 500, 0, 0, 0);
  // 2500ms, third frame.
  TestVideoSender(&sender, 1, 10, 1000, 10000);
  // 3499ms, nothing.
  TestVideoSender(&sender, 999, 0, 0, 0);
}

TEST(BweTestFramework_VideoSenderTest, Fps50Kpbs80_11s) {
  // 50 fps, 80 kbps.
  VideoSender sender(NULL, 50.0f, 80, 0x1234, 0);
  EXPECT_EQ(10000u, sender.bytes_per_second());
  // 9998ms, should see 500 frames, 200 byte payloads, total 100000 bytes.
  TestVideoSender(&sender, 9998, 500, 200, 100000);
  // 9999ms, nothing.
  TestVideoSender(&sender, 1, 0, 0, 0);
  // 10000ms, 501st frame as a single packet.
  TestVideoSender(&sender, 1, 1, 200, 200);
  // 10998ms, 49 more frames.
  TestVideoSender(&sender, 998, 49, 200, 9800);
  // 10999ms, nothing.
  TestVideoSender(&sender, 1, 0, 0, 0);
}

TEST(BweTestFramework_VideoSenderTest, Fps10Kpbs120_1s) {
  // 20 fps, 120 kbps.
  VideoSender sender(NULL, 20.0f, 120, 0x1234, 0);
  EXPECT_EQ(15000u, sender.bytes_per_second());
  // 498ms, 10 frames with 750 byte payloads, total 7500 bytes.
  TestVideoSender(&sender, 498, 10, 750, 7500);
  // 499ms, nothing.
  TestVideoSender(&sender, 1, 0, 0, 0);
  // 500ms, one more frame.
  TestVideoSender(&sender, 1, 1, 750, 750);
  // 998ms, 9 more frames.
  TestVideoSender(&sender, 498, 9, 750, 6750);
  // 999ms, nothing.
  TestVideoSender(&sender, 1, 0, 0, 0);
}

TEST(BweTestFramework_VideoSenderTest, Fps30Kpbs800_20s) {
  // 20 fps, 820 kbps.
  VideoSender sender(NULL, 25.0f, 820, 0x1234, 0);
  EXPECT_EQ(102500u, sender.bytes_per_second());
  // 9998ms, 250 frames. 820 kbps = 102500 bytes/s, so total should be 1025000.
  // Each frame is 102500/25=4100 bytes, or 5 packets (4 @1000 bytes, 1 @100),
  // so packet count should be 5*250=1250 and last packet of each frame has
  // 100 bytes of payload.
  TestVideoSender(&sender, 9998, 1250, 100, 1025000);
  // 9999ms, nothing.
  TestVideoSender(&sender, 1, 0, 0, 0);
  // 19998ms, 250 more frames.
  TestVideoSender(&sender, 9999, 1250, 100, 1025000);
  // 19999ms, nothing.
  TestVideoSender(&sender, 1, 0, 0, 0);
  // 20038ms, one more frame, as described above (25fps == 40ms/frame).
  TestVideoSender(&sender, 39, 5, 100, 4100);
  // 20039ms, nothing.
  TestVideoSender(&sender, 1, 0, 0, 0);
}

TEST(BweTestFramework_VideoSenderTest, TestAppendInOrder) {
  // 1 fps, 80 kbps, 250ms offset.
  VideoSender sender1(NULL, 1.0f, 80, 0x1234, 0.25f);
  EXPECT_EQ(10000u, sender1.bytes_per_second());
  Packets packets;
  // Generate some packets, verify they are sorted.
  sender1.RunFor(999, &packets);
  ASSERT_TRUE(IsTimeSorted(packets));
  ASSERT_TRUE(IsSequenceNumberSorted(packets));
  EXPECT_EQ(10u, packets.size());
  // Generate some more packets and verify they are appended to end of list.
  sender1.RunFor(1000, &packets);
  ASSERT_TRUE(IsTimeSorted(packets));
  ASSERT_TRUE(IsSequenceNumberSorted(packets));
  EXPECT_EQ(20u, packets.size());

  // Another sender, 2 fps, 160 kpbs, 150ms offset
  VideoSender sender2(NULL, 2.0f, 160, 0x2234, 0.30f);
  EXPECT_EQ(20000u, sender2.bytes_per_second());
  // Generate some packets, verify that they are merged with the packets already
  // on the list.
  sender2.RunFor(999, &packets);
  ASSERT_TRUE(IsTimeSorted(packets));
  EXPECT_EQ(40u, packets.size());
  // Generate some more.
  sender2.RunFor(1000, &packets);
  ASSERT_TRUE(IsTimeSorted(packets));
  EXPECT_EQ(60u, packets.size());
}

TEST(BweTestFramework_VideoSenderTest, FeedbackIneffective) {
  VideoSender sender(NULL, 25.0f, 820, 0x1234, 0);
  EXPECT_EQ(102500u, sender.bytes_per_second());
  TestVideoSender(&sender, 9998, 1250, 100, 1025000);

  // Make sure feedback has no effect on a regular video sender.
  PacketSender::Feedback feedback = { 512000 };
  sender.GiveFeedback(feedback);
  EXPECT_EQ(102500u, sender.bytes_per_second());
  TestVideoSender(&sender, 9998, 1250, 100, 1025000);
}

TEST(BweTestFramework_AdaptiveVideoSenderTest, FeedbackChangesBitrate) {
  AdaptiveVideoSender sender(NULL, 25.0f, 820, 0x1234, 0);
  EXPECT_EQ(102500u, sender.bytes_per_second());
  TestVideoSender(&sender, 9998, 1250, 100, 1025000);

  // Make sure we can reduce the bitrate.
  PacketSender::Feedback feedback = { 512000 };
  sender.GiveFeedback(feedback);
  EXPECT_EQ(64000u, sender.bytes_per_second());
  TestVideoSender(&sender, 9998, 750, 560, 640000);

  // Increase the bitrate to the initial bitrate and verify that the output is
  // the same.
  feedback.estimated_bps = 820000;
  sender.GiveFeedback(feedback);
  EXPECT_EQ(102500u, sender.bytes_per_second());
  TestVideoSender(&sender, 9998, 1250, 100, 1025000);
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
