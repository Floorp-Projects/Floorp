/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/video_coding/frame_buffer3.h"

#include <vector>

#include "api/video/encoded_frame.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/scoped_key_value_config.h"

namespace webrtc {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Matches;

MATCHER_P(FrameWithId, id, "") {
  return Matches(Eq(id))(arg->Id());
}

class FakeEncodedFrame : public EncodedFrame {
 public:
  int64_t ReceivedTime() const override { return 0; }
  int64_t RenderTime() const override { return 0; }
};

class Builder {
 public:
  Builder& Time(uint32_t rtp_timestamp) {
    rtp_timestamp_ = rtp_timestamp;
    return *this;
  }
  Builder& Id(int64_t frame_id) {
    frame_id_ = frame_id;
    return *this;
  }
  Builder& AsLast() {
    last_spatial_layer_ = true;
    return *this;
  }
  Builder& Refs(const std::vector<int64_t>& references) {
    references_ = references;
    return *this;
  }

  std::unique_ptr<FakeEncodedFrame> Build() {
    RTC_CHECK_LE(references_.size(), EncodedFrame::kMaxFrameReferences);
    RTC_CHECK(rtp_timestamp_.has_value());
    RTC_CHECK(frame_id_.has_value());

    auto frame = std::make_unique<FakeEncodedFrame>();
    frame->SetTimestamp(*rtp_timestamp_);
    frame->SetId(*frame_id_);
    frame->is_last_spatial_layer = last_spatial_layer_;

    for (int64_t ref : references_) {
      frame->references[frame->num_references] = ref;
      frame->num_references++;
    }

    return frame;
  }

 private:
  absl::optional<uint32_t> rtp_timestamp_;
  absl::optional<int64_t> frame_id_;
  bool last_spatial_layer_ = false;
  std::vector<int64_t> references_;
};

TEST(FrameBuffer3Test, RejectInvalidRefs) {
  test::ScopedKeyValueConfig field_trials;
  FrameBuffer buffer(/*max_frame_slots=*/10, /*max_decode_history=*/100,
                     field_trials);
  // Ref must be less than the id of this frame.
  buffer.InsertFrame(Builder().Time(0).Id(0).Refs({0}).AsLast().Build());
  EXPECT_THAT(buffer.LastContinuousFrameId(), Eq(absl::nullopt));

  // Duplicate ids are also invalid.
  buffer.InsertFrame(Builder().Time(10).Id(1).AsLast().Build());
  buffer.InsertFrame(Builder().Time(20).Id(2).Refs({1, 1}).AsLast().Build());
  EXPECT_THAT(buffer.LastContinuousFrameId(), Eq(1));
}

TEST(FrameBuffer3Test, LastContinuousUpdatesOnInsertedFrames) {
  test::ScopedKeyValueConfig field_trials;
  FrameBuffer buffer(/*max_frame_slots=*/10, /*max_decode_history=*/100,
                     field_trials);
  EXPECT_THAT(buffer.LastContinuousFrameId(), Eq(absl::nullopt));
  EXPECT_THAT(buffer.LastContinuousTemporalUnitFrameId(), Eq(absl::nullopt));

  buffer.InsertFrame(Builder().Time(10).Id(1).Build());
  EXPECT_THAT(buffer.LastContinuousFrameId(), Eq(1));
  EXPECT_THAT(buffer.LastContinuousTemporalUnitFrameId(), Eq(absl::nullopt));

  buffer.InsertFrame(Builder().Time(10).Id(2).Refs({1}).AsLast().Build());
  EXPECT_THAT(buffer.LastContinuousFrameId(), Eq(2));
  EXPECT_THAT(buffer.LastContinuousTemporalUnitFrameId(), Eq(2));
}

TEST(FrameBuffer3Test, LastContinuousFrameReordering) {
  test::ScopedKeyValueConfig field_trials;
  FrameBuffer buffer(/*max_frame_slots=*/10, /*max_decode_history=*/100,
                     field_trials);

  buffer.InsertFrame(Builder().Time(10).Id(1).AsLast().Build());
  buffer.InsertFrame(Builder().Time(30).Id(3).Refs({2}).AsLast().Build());
  EXPECT_THAT(buffer.LastContinuousFrameId(), Eq(1));

  buffer.InsertFrame(Builder().Time(20).Id(2).Refs({1}).AsLast().Build());
  EXPECT_THAT(buffer.LastContinuousFrameId(), Eq(3));
}

TEST(FrameBuffer3Test, LastContinuousTemporalUnit) {
  test::ScopedKeyValueConfig field_trials;
  FrameBuffer buffer(/*max_frame_slots=*/10, /*max_decode_history=*/100,
                     field_trials);

  buffer.InsertFrame(Builder().Time(10).Id(1).Build());
  EXPECT_THAT(buffer.LastContinuousTemporalUnitFrameId(), Eq(absl::nullopt));
  buffer.InsertFrame(Builder().Time(10).Id(2).Refs({1}).AsLast().Build());
  EXPECT_THAT(buffer.LastContinuousTemporalUnitFrameId(), Eq(2));
}

TEST(FrameBuffer3Test, LastContinuousTemporalUnitReordering) {
  test::ScopedKeyValueConfig field_trials;
  FrameBuffer buffer(/*max_frame_slots=*/10, /*max_decode_history=*/100,
                     field_trials);

  buffer.InsertFrame(Builder().Time(10).Id(1).Build());
  buffer.InsertFrame(Builder().Time(20).Id(3).Refs({1}).Build());
  buffer.InsertFrame(Builder().Time(20).Id(4).Refs({2, 3}).AsLast().Build());
  EXPECT_THAT(buffer.LastContinuousTemporalUnitFrameId(), Eq(absl::nullopt));

  buffer.InsertFrame(Builder().Time(10).Id(2).Refs({1}).AsLast().Build());
  EXPECT_THAT(buffer.LastContinuousTemporalUnitFrameId(), Eq(4));
}

TEST(FrameBuffer3Test, NextDecodable) {
  test::ScopedKeyValueConfig field_trials;
  FrameBuffer buffer(/*max_frame_slots=*/10, /*max_decode_history=*/100,
                     field_trials);

  EXPECT_THAT(buffer.NextDecodableTemporalUnitRtpTimestamp(),
              Eq(absl::nullopt));
  buffer.InsertFrame(Builder().Time(10).Id(1).AsLast().Build());
  EXPECT_THAT(buffer.NextDecodableTemporalUnitRtpTimestamp(), Eq(10U));
}

TEST(FrameBuffer3Test, AdvanceNextDecodableOnExtraction) {
  test::ScopedKeyValueConfig field_trials;
  FrameBuffer buffer(/*max_frame_slots=*/10, /*max_decode_history=*/100,
                     field_trials);

  buffer.InsertFrame(Builder().Time(10).Id(1).AsLast().Build());
  buffer.InsertFrame(Builder().Time(20).Id(2).AsLast().Build());
  buffer.InsertFrame(Builder().Time(30).Id(3).Refs({2}).AsLast().Build());
  EXPECT_THAT(buffer.NextDecodableTemporalUnitRtpTimestamp(), Eq(10U));

  EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(),
              ElementsAre(FrameWithId(1)));
  EXPECT_THAT(buffer.NextDecodableTemporalUnitRtpTimestamp(), Eq(20U));
  EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(),
              ElementsAre(FrameWithId(2)));
  EXPECT_THAT(buffer.NextDecodableTemporalUnitRtpTimestamp(), Eq(30U));
  EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(),
              ElementsAre(FrameWithId(3)));
}

TEST(FrameBuffer3Test, AdvanceLastDecodableOnExtraction) {
  test::ScopedKeyValueConfig field_trials;
  FrameBuffer buffer(/*max_frame_slots=*/10, /*max_decode_history=*/100,
                     field_trials);

  buffer.InsertFrame(Builder().Time(10).Id(1).AsLast().Build());
  buffer.InsertFrame(Builder().Time(20).Id(2).Refs({1}).AsLast().Build());
  buffer.InsertFrame(Builder().Time(30).Id(3).Refs({1}).AsLast().Build());
  EXPECT_THAT(buffer.LastDecodableTemporalUnitRtpTimestamp(), Eq(10U));

  EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(),
              ElementsAre(FrameWithId(1)));
  EXPECT_THAT(buffer.LastDecodableTemporalUnitRtpTimestamp(), Eq(30U));
}

TEST(FrameBuffer3Test, FrameUpdatesNextDecodable) {
  test::ScopedKeyValueConfig field_trials;
  FrameBuffer buffer(/*max_frame_slots=*/10, /*max_decode_history=*/100,
                     field_trials);

  buffer.InsertFrame(Builder().Time(20).Id(2).AsLast().Build());
  EXPECT_THAT(buffer.NextDecodableTemporalUnitRtpTimestamp(), Eq(20U));

  buffer.InsertFrame(Builder().Time(10).Id(1).AsLast().Build());
  EXPECT_THAT(buffer.NextDecodableTemporalUnitRtpTimestamp(), Eq(10U));
}

TEST(FrameBuffer3Test, KeyframeClearsFullBuffer) {
  test::ScopedKeyValueConfig field_trials;
  FrameBuffer buffer(/*max_frame_slots=*/5, /*max_decode_history=*/10,
                     field_trials);
  buffer.InsertFrame(Builder().Time(10).Id(1).AsLast().Build());
  buffer.InsertFrame(Builder().Time(20).Id(2).Refs({1}).AsLast().Build());
  buffer.InsertFrame(Builder().Time(30).Id(3).Refs({2}).AsLast().Build());
  buffer.InsertFrame(Builder().Time(40).Id(4).Refs({3}).AsLast().Build());
  buffer.InsertFrame(Builder().Time(50).Id(5).Refs({4}).AsLast().Build());
  EXPECT_THAT(buffer.LastContinuousFrameId(), Eq(5));

  // Frame buffer is full
  buffer.InsertFrame(Builder().Time(60).Id(6).Refs({5}).AsLast().Build());
  EXPECT_THAT(buffer.LastContinuousFrameId(), Eq(5));

  buffer.InsertFrame(Builder().Time(70).Id(7).AsLast().Build());
  EXPECT_THAT(buffer.LastContinuousFrameId(), Eq(7));
}

TEST(FrameBuffer3Test, DropNextDecodableTemporalUnit) {
  test::ScopedKeyValueConfig field_trials;
  FrameBuffer buffer(/*max_frame_slots=*/10, /*max_decode_history=*/100,
                     field_trials);
  buffer.InsertFrame(Builder().Time(10).Id(1).AsLast().Build());
  buffer.InsertFrame(Builder().Time(20).Id(2).Refs({1}).AsLast().Build());
  buffer.InsertFrame(Builder().Time(30).Id(3).Refs({1}).AsLast().Build());

  buffer.ExtractNextDecodableTemporalUnit();
  buffer.DropNextDecodableTemporalUnit();
  EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(),
              ElementsAre(FrameWithId(3)));
}

TEST(FrameBuffer3Test, OldFramesAreIgnored) {
  test::ScopedKeyValueConfig field_trials;
  FrameBuffer buffer(/*max_frame_slots=*/10, /*max_decode_history=*/100,
                     field_trials);
  buffer.InsertFrame(Builder().Time(10).Id(1).AsLast().Build());
  buffer.InsertFrame(Builder().Time(20).Id(2).Refs({1}).AsLast().Build());

  buffer.ExtractNextDecodableTemporalUnit();
  buffer.ExtractNextDecodableTemporalUnit();

  buffer.InsertFrame(Builder().Time(10).Id(1).AsLast().Build());
  buffer.InsertFrame(Builder().Time(20).Id(2).Refs({1}).AsLast().Build());
  buffer.InsertFrame(Builder().Time(30).Id(3).Refs({1}).AsLast().Build());

  EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(),
              ElementsAre(FrameWithId(3)));
}

TEST(FrameBuffer3Test, ReturnFullTemporalUnitKSVC) {
  test::ScopedKeyValueConfig field_trials;
  FrameBuffer buffer(/*max_frame_slots=*/10, /*max_decode_history=*/100,
                     field_trials);
  buffer.InsertFrame(Builder().Time(10).Id(1).Build());
  buffer.InsertFrame(Builder().Time(10).Id(2).Refs({1}).Build());
  buffer.InsertFrame(Builder().Time(10).Id(3).Refs({2}).AsLast().Build());
  EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(),
              ElementsAre(FrameWithId(1), FrameWithId(2), FrameWithId(3)));

  buffer.InsertFrame(Builder().Time(20).Id(4).Refs({3}).AsLast().Build());
  EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(),
              ElementsAre(FrameWithId(4)));
}

TEST(FrameBuffer3Test, InterleavedStream) {
  test::ScopedKeyValueConfig field_trials;
  FrameBuffer buffer(/*max_frame_slots=*/10, /*max_decode_history=*/100,
                     field_trials);
  buffer.InsertFrame(Builder().Time(10).Id(1).AsLast().Build());
  buffer.InsertFrame(Builder().Time(20).Id(2).Refs({1}).AsLast().Build());
  buffer.InsertFrame(Builder().Time(30).Id(3).Refs({1}).AsLast().Build());
  buffer.InsertFrame(Builder().Time(40).Id(4).Refs({2}).AsLast().Build());
  buffer.InsertFrame(Builder().Time(50).Id(5).Refs({3}).AsLast().Build());

  EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(),
              ElementsAre(FrameWithId(1)));
  EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(),
              ElementsAre(FrameWithId(2)));
  EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(),
              ElementsAre(FrameWithId(3)));
  EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(),
              ElementsAre(FrameWithId(4)));
  EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(),
              ElementsAre(FrameWithId(5)));

  buffer.InsertFrame(Builder().Time(70).Id(7).Refs({5}).AsLast().Build());
  EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(),
              ElementsAre(FrameWithId(7)));
  buffer.InsertFrame(Builder().Time(60).Id(6).Refs({4}).AsLast().Build());
  EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(), IsEmpty());
  buffer.InsertFrame(Builder().Time(90).Id(9).Refs({7}).AsLast().Build());
  EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(),
              ElementsAre(FrameWithId(9)));
}

TEST(FrameBuffer3Test, LegacyFrameIdJumpBehavior) {
  {
    test::ScopedKeyValueConfig field_trials(
        "WebRTC-LegacyFrameIdJumpBehavior/Disabled/");
    FrameBuffer buffer(/*max_frame_slots=*/10, /*max_decode_history=*/100,
                       field_trials);

    buffer.InsertFrame(Builder().Time(20).Id(3).AsLast().Build());
    EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(),
                ElementsAre(FrameWithId(3)));
    buffer.InsertFrame(Builder().Time(30).Id(2).AsLast().Build());
    EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(), IsEmpty());
  }

  {
    // WebRTC-LegacyFrameIdJumpBehavior is disabled by default.
    test::ScopedKeyValueConfig field_trials;
    FrameBuffer buffer(/*max_frame_slots=*/10, /*max_decode_history=*/100,
                       field_trials);

    buffer.InsertFrame(Builder().Time(20).Id(3).AsLast().Build());
    EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(),
                ElementsAre(FrameWithId(3)));
    buffer.InsertFrame(Builder().Time(30).Id(2).Refs({1}).AsLast().Build());
    EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(), IsEmpty());
    buffer.InsertFrame(Builder().Time(40).Id(1).AsLast().Build());
    EXPECT_THAT(buffer.ExtractNextDecodableTemporalUnit(),
                ElementsAre(FrameWithId(1)));
  }
}

TEST(FrameBuffer3Test, TotalNumberOfContinuousTemporalUnits) {
  test::ScopedKeyValueConfig field_trials;
  FrameBuffer buffer(/*max_frame_slots=*/10, /*max_decode_history=*/100,
                     field_trials);
  EXPECT_THAT(buffer.GetTotalNumberOfContinuousTemporalUnits(), Eq(0));

  buffer.InsertFrame(Builder().Time(10).Id(1).AsLast().Build());
  EXPECT_THAT(buffer.GetTotalNumberOfContinuousTemporalUnits(), Eq(1));

  buffer.InsertFrame(Builder().Time(20).Id(2).Refs({1}).Build());
  EXPECT_THAT(buffer.GetTotalNumberOfContinuousTemporalUnits(), Eq(1));

  buffer.InsertFrame(Builder().Time(40).Id(4).Refs({2}).Build());
  buffer.InsertFrame(Builder().Time(40).Id(5).Refs({3, 4}).AsLast().Build());
  EXPECT_THAT(buffer.GetTotalNumberOfContinuousTemporalUnits(), Eq(1));

  // Reordered
  buffer.InsertFrame(Builder().Time(20).Id(3).Refs({2}).AsLast().Build());
  EXPECT_THAT(buffer.GetTotalNumberOfContinuousTemporalUnits(), Eq(3));
}

TEST(FrameBuffer3Test, TotalNumberOfDroppedFrames) {
  test::ScopedKeyValueConfig field_trials;
  FrameBuffer buffer(/*max_frame_slots=*/10, /*max_decode_history=*/100,
                     field_trials);
  EXPECT_THAT(buffer.GetTotalNumberOfDroppedFrames(), Eq(0));

  buffer.InsertFrame(Builder().Time(10).Id(1).AsLast().Build());
  buffer.InsertFrame(Builder().Time(20).Id(2).Refs({1}).Build());
  buffer.InsertFrame(Builder().Time(20).Id(3).Refs({2}).AsLast().Build());
  buffer.InsertFrame(Builder().Time(40).Id(4).Refs({1}).Build());
  buffer.InsertFrame(Builder().Time(40).Id(5).Refs({4}).AsLast().Build());

  buffer.ExtractNextDecodableTemporalUnit();
  EXPECT_THAT(buffer.GetTotalNumberOfDroppedFrames(), Eq(0));

  buffer.DropNextDecodableTemporalUnit();
  EXPECT_THAT(buffer.GetTotalNumberOfDroppedFrames(), Eq(2));

  buffer.ExtractNextDecodableTemporalUnit();
  EXPECT_THAT(buffer.GetTotalNumberOfDroppedFrames(), Eq(2));
}

}  // namespace
}  // namespace webrtc
