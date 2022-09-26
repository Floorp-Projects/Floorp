/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/tx/stream_scheduler.h"

#include <vector>

#include "net/dcsctp/public/types.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::Return;
using ::testing::StrictMock;

constexpr size_t kMtu = 1000;
constexpr size_t kPayloadSize = 4;

MATCHER_P(HasDataWithMid, mid, "") {
  if (!arg.has_value()) {
    *result_listener << "There was no produced data";
    return false;
  }

  if (arg->data.message_id != mid) {
    *result_listener << "the produced data had mid " << *arg->data.message_id
                     << " and not the expected " << *mid;
    return false;
  }

  return true;
}

std::function<absl::optional<SendQueue::DataToSend>(TimeMs, size_t)>
CreateChunk(StreamID sid, MID mid, size_t payload_size = kPayloadSize) {
  return [sid, mid, payload_size](TimeMs now, size_t max_size) {
    return SendQueue::DataToSend(Data(
        sid, SSN(0), mid, FSN(0), PPID(42), std::vector<uint8_t>(payload_size),
        Data::IsBeginning(true), Data::IsEnd(true), IsUnordered(true)));
  };
}

std::map<StreamID, size_t> GetPacketCounts(StreamScheduler& scheduler,
                                           size_t packets_to_generate) {
  std::map<StreamID, size_t> packet_counts;
  for (size_t i = 0; i < packets_to_generate; ++i) {
    absl::optional<SendQueue::DataToSend> data =
        scheduler.Produce(TimeMs(0), kMtu);
    if (data.has_value()) {
      ++packet_counts[data->data.stream_id];
    }
  }
  return packet_counts;
}

class MockStreamCallback : public StreamScheduler::StreamCallback {
 public:
  MOCK_METHOD(absl::optional<SendQueue::DataToSend>,
              Produce,
              (TimeMs, size_t),
              (override));
  MOCK_METHOD(size_t, bytes_to_send_in_next_message, (), (const, override));
};

class TestStream {
 public:
  TestStream(StreamScheduler& scheduler,
             StreamID stream_id,
             StreamPriority priority,
             size_t packet_size = kPayloadSize) {
    EXPECT_CALL(callback_, Produce)
        .WillRepeatedly(CreateChunk(stream_id, MID(0), packet_size));
    EXPECT_CALL(callback_, bytes_to_send_in_next_message)
        .WillRepeatedly(Return(packet_size));
    stream_ = scheduler.CreateStream(&callback_, stream_id, priority);
    stream_->MaybeMakeActive();
  }

  StreamScheduler::Stream& stream() { return *stream_; }

 private:
  StrictMock<MockStreamCallback> callback_;
  std::unique_ptr<StreamScheduler::Stream> stream_;
};

// A scheduler without active streams doesn't produce data.
TEST(StreamSchedulerTest, HasNoActiveStreams) {
  StreamScheduler scheduler;

  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
}

// Stream properties can be set and retrieved
TEST(StreamSchedulerTest, CanSetAndGetStreamProperties) {
  StreamScheduler scheduler;

  StrictMock<MockStreamCallback> callback;
  auto stream =
      scheduler.CreateStream(&callback, StreamID(1), StreamPriority(2));

  EXPECT_EQ(stream->stream_id(), StreamID(1));
  EXPECT_EQ(stream->priority(), StreamPriority(2));

  stream->set_priority(StreamPriority(0));
  EXPECT_EQ(stream->priority(), StreamPriority(0));
}

// A scheduler with a single stream produced packets from it.
TEST(StreamSchedulerTest, CanProduceFromSingleStream) {
  StreamScheduler scheduler;

  StrictMock<MockStreamCallback> callback;
  EXPECT_CALL(callback, Produce).WillOnce(CreateChunk(StreamID(1), MID(0)));
  EXPECT_CALL(callback, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(0));
  auto stream =
      scheduler.CreateStream(&callback, StreamID(1), StreamPriority(2));
  stream->MaybeMakeActive();

  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(0)));
  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
}

// Switches between two streams after every packet.
TEST(StreamSchedulerTest, WillRoundRobinBetweenStreams) {
  StreamScheduler scheduler;

  StrictMock<MockStreamCallback> callback1;
  EXPECT_CALL(callback1, Produce)
      .WillOnce(CreateChunk(StreamID(1), MID(100)))
      .WillOnce(CreateChunk(StreamID(1), MID(101)))
      .WillOnce(CreateChunk(StreamID(1), MID(102)));
  EXPECT_CALL(callback1, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(0));
  auto stream1 =
      scheduler.CreateStream(&callback1, StreamID(1), StreamPriority(2));
  stream1->MaybeMakeActive();

  StrictMock<MockStreamCallback> callback2;
  EXPECT_CALL(callback2, Produce)
      .WillOnce(CreateChunk(StreamID(2), MID(200)))
      .WillOnce(CreateChunk(StreamID(2), MID(201)))
      .WillOnce(CreateChunk(StreamID(2), MID(202)));
  EXPECT_CALL(callback2, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(0));
  auto stream2 =
      scheduler.CreateStream(&callback2, StreamID(2), StreamPriority(2));
  stream2->MaybeMakeActive();

  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(100)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(200)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(101)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(201)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(102)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(202)));
  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
}

// Switches between two streams after every packet, but keeps producing from the
// same stream when a packet contains of multiple fragments.
TEST(StreamSchedulerTest, WillRoundRobinOnlyWhenFinishedProducingChunk) {
  StreamScheduler scheduler;

  StrictMock<MockStreamCallback> callback1;
  EXPECT_CALL(callback1, Produce)
      .WillOnce(CreateChunk(StreamID(1), MID(100)))
      .WillOnce([](...) {
        return SendQueue::DataToSend(
            Data(StreamID(1), SSN(0), MID(101), FSN(0), PPID(42),
                 std::vector<uint8_t>(4), Data::IsBeginning(true),
                 Data::IsEnd(false), IsUnordered(true)));
      })
      .WillOnce([](...) {
        return SendQueue::DataToSend(
            Data(StreamID(1), SSN(0), MID(101), FSN(0), PPID(42),
                 std::vector<uint8_t>(4), Data::IsBeginning(false),
                 Data::IsEnd(false), IsUnordered(true)));
      })
      .WillOnce([](...) {
        return SendQueue::DataToSend(
            Data(StreamID(1), SSN(0), MID(101), FSN(0), PPID(42),
                 std::vector<uint8_t>(4), Data::IsBeginning(false),
                 Data::IsEnd(true), IsUnordered(true)));
      })
      .WillOnce(CreateChunk(StreamID(1), MID(102)));
  EXPECT_CALL(callback1, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(0));
  auto stream1 =
      scheduler.CreateStream(&callback1, StreamID(1), StreamPriority(2));
  stream1->MaybeMakeActive();

  StrictMock<MockStreamCallback> callback2;
  EXPECT_CALL(callback2, Produce)
      .WillOnce(CreateChunk(StreamID(2), MID(200)))
      .WillOnce(CreateChunk(StreamID(2), MID(201)))
      .WillOnce(CreateChunk(StreamID(2), MID(202)));
  EXPECT_CALL(callback2, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(0));
  auto stream2 =
      scheduler.CreateStream(&callback2, StreamID(2), StreamPriority(2));
  stream2->MaybeMakeActive();

  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(100)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(200)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(101)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(101)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(101)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(201)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(102)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(202)));
  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
}

// Deactivates a stream before it has finished producing all packets.
TEST(StreamSchedulerTest, StreamsCanBeMadeInactive) {
  StreamScheduler scheduler;

  StrictMock<MockStreamCallback> callback1;
  EXPECT_CALL(callback1, Produce)
      .WillOnce(CreateChunk(StreamID(1), MID(100)))
      .WillOnce(CreateChunk(StreamID(1), MID(101)));
  EXPECT_CALL(callback1, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize));  // hints that there is a MID(2) coming.
  auto stream1 =
      scheduler.CreateStream(&callback1, StreamID(1), StreamPriority(2));
  stream1->MaybeMakeActive();

  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(100)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(101)));

  // ... but the stream is made inactive before it can be produced.
  stream1->MakeInactive();
  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
}

// Resumes a paused stream - makes a stream active after inactivating it.
TEST(StreamSchedulerTest, SingleStreamCanBeResumed) {
  StreamScheduler scheduler;

  StrictMock<MockStreamCallback> callback1;
  // Callbacks are setup so that they hint that there is a MID(2) coming...
  EXPECT_CALL(callback1, Produce)
      .WillOnce(CreateChunk(StreamID(1), MID(100)))
      .WillOnce(CreateChunk(StreamID(1), MID(101)))
      .WillOnce(CreateChunk(StreamID(1), MID(102)));
  EXPECT_CALL(callback1, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))  // When making active again
      .WillOnce(Return(0));
  auto stream1 =
      scheduler.CreateStream(&callback1, StreamID(1), StreamPriority(2));
  stream1->MaybeMakeActive();

  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(100)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(101)));

  stream1->MakeInactive();
  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
  stream1->MaybeMakeActive();
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(102)));
  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
}

// Iterates between streams, where one is suddenly paused and later resumed.
TEST(StreamSchedulerTest, WillRoundRobinWithPausedStream) {
  StreamScheduler scheduler;

  StrictMock<MockStreamCallback> callback1;
  EXPECT_CALL(callback1, Produce)
      .WillOnce(CreateChunk(StreamID(1), MID(100)))
      .WillOnce(CreateChunk(StreamID(1), MID(101)))
      .WillOnce(CreateChunk(StreamID(1), MID(102)));
  EXPECT_CALL(callback1, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(0));
  auto stream1 =
      scheduler.CreateStream(&callback1, StreamID(1), StreamPriority(2));
  stream1->MaybeMakeActive();

  StrictMock<MockStreamCallback> callback2;
  EXPECT_CALL(callback2, Produce)
      .WillOnce(CreateChunk(StreamID(2), MID(200)))
      .WillOnce(CreateChunk(StreamID(2), MID(201)))
      .WillOnce(CreateChunk(StreamID(2), MID(202)));
  EXPECT_CALL(callback2, bytes_to_send_in_next_message)
      .WillOnce(Return(kPayloadSize))  // When making active
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(kPayloadSize))
      .WillOnce(Return(0));
  auto stream2 =
      scheduler.CreateStream(&callback2, StreamID(2), StreamPriority(2));
  stream2->MaybeMakeActive();

  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(100)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(200)));
  stream1->MakeInactive();
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(201)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(202)));
  stream1->MaybeMakeActive();
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(101)));
  EXPECT_THAT(scheduler.Produce(TimeMs(0), kMtu), HasDataWithMid(MID(102)));
  EXPECT_EQ(scheduler.Produce(TimeMs(0), kMtu), absl::nullopt);
}

// Verifies that packet counts are evenly distributed in round robin scheduling.
TEST(StreamSchedulerTest, WillDistributeRoundRobinPacketsEvenlyTwoStreams) {
  StreamScheduler scheduler;
  TestStream stream1(scheduler, StreamID(1), StreamPriority(1));
  TestStream stream2(scheduler, StreamID(2), StreamPriority(1));

  std::map<StreamID, size_t> packet_counts = GetPacketCounts(scheduler, 10);
  EXPECT_EQ(packet_counts[StreamID(1)], 5U);
  EXPECT_EQ(packet_counts[StreamID(2)], 5U);
}

// Verifies that packet counts are evenly distributed among active streams,
// where a stream is suddenly made inactive, two are added, and then the paused
// stream is resumed.
TEST(StreamSchedulerTest, WillDistributeEvenlyWithPausedAndAddedStreams) {
  StreamScheduler scheduler;
  TestStream stream1(scheduler, StreamID(1), StreamPriority(1));
  TestStream stream2(scheduler, StreamID(2), StreamPriority(1));

  std::map<StreamID, size_t> packet_counts = GetPacketCounts(scheduler, 10);
  EXPECT_EQ(packet_counts[StreamID(1)], 5U);
  EXPECT_EQ(packet_counts[StreamID(2)], 5U);

  stream2.stream().MakeInactive();

  TestStream stream3(scheduler, StreamID(3), StreamPriority(1));
  TestStream stream4(scheduler, StreamID(4), StreamPriority(1));

  std::map<StreamID, size_t> counts2 = GetPacketCounts(scheduler, 15);
  EXPECT_EQ(counts2[StreamID(1)], 5U);
  EXPECT_EQ(counts2[StreamID(2)], 0U);
  EXPECT_EQ(counts2[StreamID(3)], 5U);
  EXPECT_EQ(counts2[StreamID(4)], 5U);

  stream2.stream().MaybeMakeActive();

  std::map<StreamID, size_t> counts3 = GetPacketCounts(scheduler, 20);
  EXPECT_EQ(counts3[StreamID(1)], 5U);
  EXPECT_EQ(counts3[StreamID(2)], 5U);
  EXPECT_EQ(counts3[StreamID(3)], 5U);
  EXPECT_EQ(counts3[StreamID(4)], 5U);
}

}  // namespace
}  // namespace dcsctp
