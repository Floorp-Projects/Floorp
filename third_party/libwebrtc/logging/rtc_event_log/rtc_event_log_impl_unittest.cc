/*
 *  Copyright (c) 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/rtc_event_log_impl.h"

#include <utility>
#include <vector>

#include "test/gmock.h"
#include "test/gtest.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace {

using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Property;
using ::testing::Ref;
using ::testing::Return;
using ::testing::StrEq;

class MockEventEncoder : public RtcEventLogEncoder {
 public:
  MOCK_METHOD(std::string,
              EncodeLogStart,
              (int64_t /*timestamp_us*/, int64_t /*utc_time_us*/),
              (override));
  MOCK_METHOD(std::string,
              EncodeLogEnd,
              (int64_t /*timestamp_us*/),
              (override));
  MOCK_METHOD(std::string, OnEncode, (const RtcEvent&), ());
  std::string EncodeBatch(
      std::deque<std::unique_ptr<RtcEvent>>::const_iterator a,
      std::deque<std::unique_ptr<RtcEvent>>::const_iterator b) override {
    std::string result;
    while (a != b) {
      result += OnEncode(**a);
      ++a;
    }
    return result;
  }
};

class FakeOutput : public RtcEventLogOutput {
 public:
  explicit FakeOutput(std::string& written_data)
      : written_data_(written_data) {}
  bool IsActive() const { return true; }
  bool Write(absl::string_view data) override {
    written_data_.append(std::string(data));
    return true;
  }
  void Flush() override {}

 private:
  std::string& written_data_;
};

class FakeEvent : public RtcEvent {
 public:
  Type GetType() const override { return RtcEvent::Type::FakeEvent; }
  bool IsConfigEvent() const override { return false; }
};

class FakeConfigEvent : public RtcEvent {
 public:
  Type GetType() const override { return RtcEvent::Type::FakeEvent; }
  bool IsConfigEvent() const override { return true; }
};

class RtcEventLogImplTest : public ::testing::Test {
 public:
  static constexpr size_t kMaxEventsInHistory = 2;
  static constexpr size_t kMaxEventsInConfigHistory = 3;
  static constexpr TimeDelta kOutputPeriod = TimeDelta::Seconds(2);
  static constexpr Timestamp kStartTime = Timestamp::Seconds(1);

  GlobalSimulatedTimeController time_controller_{kStartTime};
  std::unique_ptr<MockEventEncoder> encoder_ =
      std::make_unique<MockEventEncoder>();
  MockEventEncoder* encoder_ptr_ = encoder_.get();
  std::unique_ptr<FakeOutput> output_ =
      std::make_unique<FakeOutput>(written_data_);
  FakeOutput* output_ptr_ = output_.get();
  RtcEventLogImpl event_log_{std::move(encoder_),
                             time_controller_.GetTaskQueueFactory(),
                             kMaxEventsInHistory, kMaxEventsInConfigHistory};
  std::string written_data_;
};

TEST_F(RtcEventLogImplTest, WritesHeaderAndEventsAndTrailer) {
  EXPECT_CALL(*encoder_ptr_, EncodeLogStart(kStartTime.us(), kStartTime.us()))
      .WillOnce(Return("start"));
  EXPECT_CALL(*encoder_ptr_, EncodeLogEnd(kStartTime.us()))
      .WillOnce(Return("stop"));
  EXPECT_CALL(*encoder_ptr_, OnEncode(Property(&RtcEvent::IsConfigEvent, true)))
      .WillOnce(Return("config"));
  EXPECT_CALL(*encoder_ptr_,
              OnEncode(Property(&RtcEvent::IsConfigEvent, false)))
      .WillOnce(Return("event"));
  event_log_.StartLogging(std::move(output_), kOutputPeriod.ms());
  event_log_.Log(std::make_unique<FakeEvent>());
  event_log_.Log(std::make_unique<FakeConfigEvent>());
  event_log_.StopLogging();
  Mock::VerifyAndClearExpectations(encoder_ptr_);
  EXPECT_TRUE(written_data_ == "startconfigeventstop" ||
              written_data_ == "starteventconfigstop");
}

TEST_F(RtcEventLogImplTest, KeepsMostRecentEventsOnStart) {
  auto e2 = std::make_unique<FakeEvent>();
  RtcEvent* e2_ptr = e2.get();
  auto e3 = std::make_unique<FakeEvent>();
  RtcEvent* e3_ptr = e3.get();
  event_log_.Log(std::make_unique<FakeEvent>());
  event_log_.Log(std::move(e2));
  event_log_.Log(std::move(e3));
  InSequence s;
  EXPECT_CALL(*encoder_ptr_, OnEncode(Ref(*e2_ptr)));
  EXPECT_CALL(*encoder_ptr_, OnEncode(Ref(*e3_ptr)));
  event_log_.StartLogging(std::move(output_), kOutputPeriod.ms());
  time_controller_.AdvanceTime(TimeDelta::Zero());
  Mock::VerifyAndClearExpectations(encoder_ptr_);
}

TEST_F(RtcEventLogImplTest, EncodesEventsOnHistoryFullPostponesLastEncode) {
  auto e1 = std::make_unique<FakeEvent>();
  RtcEvent* e1_ptr = e1.get();
  auto e2 = std::make_unique<FakeEvent>();
  RtcEvent* e2_ptr = e2.get();
  auto e3 = std::make_unique<FakeEvent>();
  RtcEvent* e3_ptr = e3.get();
  event_log_.StartLogging(std::move(output_), kOutputPeriod.ms());
  time_controller_.AdvanceTime(TimeDelta::Zero());
  event_log_.Log(std::move(e1));
  event_log_.Log(std::move(e2));
  // Overflows and causes immediate output, and eventual output after the output
  // period has passed.
  event_log_.Log(std::move(e3));
  InSequence s;
  EXPECT_CALL(*encoder_ptr_, OnEncode(Ref(*e1_ptr)));
  EXPECT_CALL(*encoder_ptr_, OnEncode(Ref(*e2_ptr)));
  time_controller_.AdvanceTime(TimeDelta::Zero());
  EXPECT_CALL(*encoder_ptr_, OnEncode(Ref(*e3_ptr)));
  time_controller_.AdvanceTime(kOutputPeriod);
  Mock::VerifyAndClearExpectations(encoder_ptr_);
}

TEST_F(RtcEventLogImplTest, RewritesConfigEventsOnlyOnRestart) {
  event_log_.StartLogging(std::make_unique<FakeOutput>(written_data_),
                          kOutputPeriod.ms());
  event_log_.Log(std::make_unique<FakeConfigEvent>());
  event_log_.Log(std::make_unique<FakeEvent>());
  event_log_.StopLogging();
  EXPECT_CALL(*encoder_ptr_,
              OnEncode(Property(&RtcEvent::IsConfigEvent, true)));
  EXPECT_CALL(*encoder_ptr_,
              OnEncode(Property(&RtcEvent::IsConfigEvent, false)))
      .Times(0);
  event_log_.StartLogging(std::move(output_), kOutputPeriod.ms());
  time_controller_.AdvanceTime(TimeDelta::Zero());
  Mock::VerifyAndClearExpectations(encoder_ptr_);
}

TEST_F(RtcEventLogImplTest, SchedulesWriteAfterOutputDurationPassed) {
  event_log_.StartLogging(std::make_unique<FakeOutput>(written_data_),
                          kOutputPeriod.ms());
  event_log_.Log(std::make_unique<FakeConfigEvent>());
  event_log_.Log(std::make_unique<FakeEvent>());
  EXPECT_CALL(*encoder_ptr_,
              OnEncode(Property(&RtcEvent::IsConfigEvent, true)));
  EXPECT_CALL(*encoder_ptr_,
              OnEncode(Property(&RtcEvent::IsConfigEvent, false)));
  time_controller_.AdvanceTime(kOutputPeriod);
  Mock::VerifyAndClearExpectations(encoder_ptr_);
}

}  // namespace
}  // namespace webrtc
