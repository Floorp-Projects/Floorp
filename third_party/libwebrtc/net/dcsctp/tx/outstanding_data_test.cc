/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "net/dcsctp/tx/outstanding_data.h"

#include <vector>

#include "absl/types/optional.h"
#include "net/dcsctp/common/math.h"
#include "net/dcsctp/common/sequence_numbers.h"
#include "net/dcsctp/packet/chunk/data_chunk.h"
#include "net/dcsctp/packet/chunk/forward_tsn_chunk.h"
#include "net/dcsctp/public/types.h"
#include "net/dcsctp/testing/data_generator.h"
#include "net/dcsctp/testing/testing_macros.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

namespace dcsctp {
namespace {
using ::testing::MockFunction;
using State = ::dcsctp::OutstandingData::State;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::Return;
using ::testing::StrictMock;

constexpr TimeMs kNow(42);

class OutstandingDataTest : public testing::Test {
 protected:
  OutstandingDataTest()
      : gen_(MID(42)),
        buf_(DataChunk::kHeaderSize,
             unwrapper_.Unwrap(TSN(10)),
             unwrapper_.Unwrap(TSN(9)),
             on_discard_.AsStdFunction()) {}

  UnwrappedTSN::Unwrapper unwrapper_;
  DataGenerator gen_;
  StrictMock<MockFunction<bool(IsUnordered, StreamID, MID)>> on_discard_;
  OutstandingData buf_;
};

TEST_F(OutstandingDataTest, HasInitialState) {
  EXPECT_TRUE(buf_.empty());
  EXPECT_EQ(buf_.outstanding_bytes(), 0u);
  EXPECT_EQ(buf_.outstanding_items(), 0u);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(9));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(10));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(9));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked)));
  EXPECT_FALSE(buf_.ShouldSendForwardTsn());
}

TEST_F(OutstandingDataTest, InsertChunk) {
  ASSERT_HAS_VALUE_AND_ASSIGN(
      UnwrappedTSN tsn,
      buf_.Insert(gen_.Ordered({1}, "BE"), MaxRetransmits::NoLimit(), kNow,
                  TimeMs::InfiniteFuture()));

  EXPECT_EQ(tsn.Wrap(), TSN(10));

  EXPECT_EQ(buf_.outstanding_bytes(), DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(buf_.outstanding_items(), 1u);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(9));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(11));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(10));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),
                          Pair(TSN(10), State::kInFlight)));
}

TEST_F(OutstandingDataTest, AcksSingleChunk) {
  buf_.Insert(gen_.Ordered({1}, "BE"), MaxRetransmits::NoLimit(), kNow,
              TimeMs::InfiniteFuture());
  OutstandingData::AckInfo ack =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(10)), {}, false);

  EXPECT_EQ(ack.bytes_acked, DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(ack.highest_tsn_acked.Wrap(), TSN(10));
  EXPECT_FALSE(ack.has_packet_loss);

  EXPECT_EQ(buf_.outstanding_bytes(), 0u);
  EXPECT_EQ(buf_.outstanding_items(), 0u);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(10));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(11));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(10));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(10), State::kAcked)));
}

TEST_F(OutstandingDataTest, AcksPreviousChunkDoesntUpdate) {
  buf_.Insert(gen_.Ordered({1}, "BE"), MaxRetransmits::NoLimit(), kNow,
              TimeMs::InfiniteFuture());
  buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), {}, false);

  EXPECT_EQ(buf_.outstanding_bytes(), DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(buf_.outstanding_items(), 1u);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(9));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(11));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(10));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),
                          Pair(TSN(10), State::kInFlight)));
}

TEST_F(OutstandingDataTest, AcksAndNacksWithGapAckBlocks) {
  buf_.Insert(gen_.Ordered({1}, "B"), MaxRetransmits::NoLimit(), kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, "E"), MaxRetransmits::NoLimit(), kNow,
              TimeMs::InfiniteFuture());

  std::vector<SackChunk::GapAckBlock> gab = {SackChunk::GapAckBlock(2, 2)};
  OutstandingData::AckInfo ack =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab, false);
  EXPECT_EQ(ack.bytes_acked, DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(ack.highest_tsn_acked.Wrap(), TSN(11));
  EXPECT_FALSE(ack.has_packet_loss);

  EXPECT_EQ(buf_.outstanding_bytes(), 0u);
  EXPECT_EQ(buf_.outstanding_items(), 0u);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(9));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(12));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(11));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),    //
                          Pair(TSN(10), State::kNacked),  //
                          Pair(TSN(11), State::kAcked)));
}

TEST_F(OutstandingDataTest, NacksThreeTimesWithSameTsnDoesntRetransmit) {
  buf_.Insert(gen_.Ordered({1}, "B"), MaxRetransmits::NoLimit(), kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, "E"), MaxRetransmits::NoLimit(), kNow,
              TimeMs::InfiniteFuture());

  std::vector<SackChunk::GapAckBlock> gab1 = {SackChunk::GapAckBlock(2, 2)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),    //
                          Pair(TSN(10), State::kNacked),  //
                          Pair(TSN(11), State::kAcked)));
}

TEST_F(OutstandingDataTest, NacksThreeTimesResultsInRetransmission) {
  buf_.Insert(gen_.Ordered({1}, "B"), MaxRetransmits::NoLimit(), kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, ""), MaxRetransmits::NoLimit(), kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, ""), MaxRetransmits::NoLimit(), kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, "E"), MaxRetransmits::NoLimit(), kNow,
              TimeMs::InfiniteFuture());

  std::vector<SackChunk::GapAckBlock> gab1 = {SackChunk::GapAckBlock(2, 2)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab2 = {SackChunk::GapAckBlock(2, 3)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab2, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab3 = {SackChunk::GapAckBlock(2, 4)};
  OutstandingData::AckInfo ack =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab3, false);
  EXPECT_EQ(ack.bytes_acked, DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(ack.highest_tsn_acked.Wrap(), TSN(13));
  EXPECT_TRUE(ack.has_packet_loss);

  EXPECT_TRUE(buf_.has_data_to_be_retransmitted());

  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),               //
                          Pair(TSN(10), State::kToBeRetransmitted),  //
                          Pair(TSN(11), State::kAcked),              //
                          Pair(TSN(12), State::kAcked),              //
                          Pair(TSN(13), State::kAcked)));

  EXPECT_THAT(buf_.GetChunksToBeRetransmitted(1000),
              ElementsAre(Pair(TSN(10), _)));
}

TEST_F(OutstandingDataTest, NacksThreeTimesResultsInAbandoning) {
  static constexpr MaxRetransmits kMaxRetransmissions(0);
  buf_.Insert(gen_.Ordered({1}, "B"), kMaxRetransmissions, kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, ""), kMaxRetransmissions, kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, ""), kMaxRetransmissions, kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, "E"), kMaxRetransmissions, kNow,
              TimeMs::InfiniteFuture());

  std::vector<SackChunk::GapAckBlock> gab1 = {SackChunk::GapAckBlock(2, 2)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab2 = {SackChunk::GapAckBlock(2, 3)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab2, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  EXPECT_CALL(on_discard_, Call(IsUnordered(false), StreamID(1), MID(42)))
      .WillOnce(Return(false));
  std::vector<SackChunk::GapAckBlock> gab3 = {SackChunk::GapAckBlock(2, 4)};
  OutstandingData::AckInfo ack =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab3, false);
  EXPECT_EQ(ack.bytes_acked, DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(ack.highest_tsn_acked.Wrap(), TSN(13));
  EXPECT_TRUE(ack.has_packet_loss);

  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(14));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),       //
                          Pair(TSN(10), State::kAbandoned),  //
                          Pair(TSN(11), State::kAbandoned),  //
                          Pair(TSN(12), State::kAbandoned),  //
                          Pair(TSN(13), State::kAbandoned)));
}

TEST_F(OutstandingDataTest, NacksThreeTimesResultsInAbandoningWithPlaceholder) {
  static constexpr MaxRetransmits kMaxRetransmissions(0);
  buf_.Insert(gen_.Ordered({1}, "B"), kMaxRetransmissions, kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, ""), kMaxRetransmissions, kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, ""), kMaxRetransmissions, kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, ""), kMaxRetransmissions, kNow,
              TimeMs::InfiniteFuture());

  std::vector<SackChunk::GapAckBlock> gab1 = {SackChunk::GapAckBlock(2, 2)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab1, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  std::vector<SackChunk::GapAckBlock> gab2 = {SackChunk::GapAckBlock(2, 3)};
  EXPECT_FALSE(
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab2, false).has_packet_loss);
  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());

  EXPECT_CALL(on_discard_, Call(IsUnordered(false), StreamID(1), MID(42)))
      .WillOnce(Return(true));
  std::vector<SackChunk::GapAckBlock> gab3 = {SackChunk::GapAckBlock(2, 4)};
  OutstandingData::AckInfo ack =
      buf_.HandleSack(unwrapper_.Unwrap(TSN(9)), gab3, false);
  EXPECT_EQ(ack.bytes_acked, DataChunk::kHeaderSize + RoundUpTo4(1));
  EXPECT_EQ(ack.highest_tsn_acked.Wrap(), TSN(13));
  EXPECT_TRUE(ack.has_packet_loss);

  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(15));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),       //
                          Pair(TSN(10), State::kAbandoned),  //
                          Pair(TSN(11), State::kAbandoned),  //
                          Pair(TSN(12), State::kAbandoned),  //
                          Pair(TSN(13), State::kAbandoned),  //
                          Pair(TSN(14), State::kAbandoned)));
}

TEST_F(OutstandingDataTest, ExpiresChunkBeforeItIsInserted) {
  static constexpr TimeMs kExpiresAt = kNow + DurationMs(1);
  EXPECT_TRUE(buf_.Insert(gen_.Ordered({1}, "B"), MaxRetransmits::NoLimit(),
                          kNow, kExpiresAt)
                  .has_value());
  EXPECT_TRUE(buf_.Insert(gen_.Ordered({1}, ""), MaxRetransmits::NoLimit(),
                          kNow + DurationMs(0), kExpiresAt)
                  .has_value());

  EXPECT_CALL(on_discard_, Call(IsUnordered(false), StreamID(1), MID(42)))
      .WillOnce(Return(false));
  EXPECT_FALSE(buf_.Insert(gen_.Ordered({1}, "E"), MaxRetransmits::NoLimit(),
                           kNow + DurationMs(1), kExpiresAt)
                   .has_value());

  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_EQ(buf_.last_cumulative_tsn_ack().Wrap(), TSN(9));
  EXPECT_EQ(buf_.next_tsn().Wrap(), TSN(13));
  EXPECT_EQ(buf_.highest_outstanding_tsn().Wrap(), TSN(12));
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),       //
                          Pair(TSN(10), State::kAbandoned),  //
                          Pair(TSN(11), State::kAbandoned),
                          Pair(TSN(12), State::kAbandoned)));
}

TEST_F(OutstandingDataTest, CanGenerateForwardTsn) {
  static constexpr MaxRetransmits kMaxRetransmissions(0);
  buf_.Insert(gen_.Ordered({1}, "B"), kMaxRetransmissions, kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, ""), kMaxRetransmissions, kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, "E"), kMaxRetransmissions, kNow,
              TimeMs::InfiniteFuture());

  EXPECT_CALL(on_discard_, Call(IsUnordered(false), StreamID(1), MID(42)))
      .WillOnce(Return(false));
  buf_.NackAll();

  EXPECT_FALSE(buf_.has_data_to_be_retransmitted());
  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(9), State::kAcked),       //
                          Pair(TSN(10), State::kAbandoned),  //
                          Pair(TSN(11), State::kAbandoned),
                          Pair(TSN(12), State::kAbandoned)));

  EXPECT_TRUE(buf_.ShouldSendForwardTsn());
  ForwardTsnChunk chunk = buf_.CreateForwardTsn();
  EXPECT_EQ(chunk.new_cumulative_tsn(), TSN(12));
}

TEST_F(OutstandingDataTest, AckWithGapBlocksFromRFC4960Section334) {
  buf_.Insert(gen_.Ordered({1}, "B"), MaxRetransmits::NoLimit(), kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, ""), MaxRetransmits::NoLimit(), kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, ""), MaxRetransmits::NoLimit(), kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, ""), MaxRetransmits::NoLimit(), kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, ""), MaxRetransmits::NoLimit(), kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, ""), MaxRetransmits::NoLimit(), kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, ""), MaxRetransmits::NoLimit(), kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, "E"), MaxRetransmits::NoLimit(), kNow,
              TimeMs::InfiniteFuture());

  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              testing::ElementsAre(Pair(TSN(9), State::kAcked),      //
                                   Pair(TSN(10), State::kInFlight),  //
                                   Pair(TSN(11), State::kInFlight),  //
                                   Pair(TSN(12), State::kInFlight),  //
                                   Pair(TSN(13), State::kInFlight),  //
                                   Pair(TSN(14), State::kInFlight),  //
                                   Pair(TSN(15), State::kInFlight),  //
                                   Pair(TSN(16), State::kInFlight),  //
                                   Pair(TSN(17), State::kInFlight)));

  std::vector<SackChunk::GapAckBlock> gab = {SackChunk::GapAckBlock(2, 3),
                                             SackChunk::GapAckBlock(5, 5)};
  buf_.HandleSack(unwrapper_.Unwrap(TSN(12)), gab, false);

  EXPECT_THAT(buf_.GetChunkStatesForTesting(),
              ElementsAre(Pair(TSN(12), State::kAcked),   //
                          Pair(TSN(13), State::kNacked),  //
                          Pair(TSN(14), State::kAcked),   //
                          Pair(TSN(15), State::kAcked),   //
                          Pair(TSN(16), State::kNacked),  //
                          Pair(TSN(17), State::kAcked)));
}

TEST_F(OutstandingDataTest, MeasureRTT) {
  buf_.Insert(gen_.Ordered({1}, "BE"), MaxRetransmits::NoLimit(), kNow,
              TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, "BE"), MaxRetransmits::NoLimit(),
              kNow + DurationMs(1), TimeMs::InfiniteFuture());
  buf_.Insert(gen_.Ordered({1}, "BE"), MaxRetransmits::NoLimit(),
              kNow + DurationMs(2), TimeMs::InfiniteFuture());

  static constexpr DurationMs kDuration(123);
  ASSERT_HAS_VALUE_AND_ASSIGN(
      DurationMs duration,
      buf_.MeasureRTT(kNow + kDuration, unwrapper_.Unwrap(TSN(11))));

  EXPECT_EQ(duration, kDuration - DurationMs(1));
}

}  // namespace
}  // namespace dcsctp
