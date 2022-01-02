/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <initializer_list>
#include "InputData.h"
#include "Units.h"
#include "gtest/gtest.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#include "TouchResampler.h"

using namespace mozilla;
using widget::TouchResampler;

class TouchResamplerTest : public ::testing::Test {
 protected:
  virtual void SetUp() { baseTimeStamp = TimeStamp::Now(); }

  TimeStamp Time(double aMilliseconds) {
    return baseTimeStamp + TimeDuration::FromMilliseconds(aMilliseconds);
  }

  uint64_t ProcessEvent(
      MultiTouchInput::MultiTouchType aType,
      std::initializer_list<std::pair<TimeStamp, ScreenIntPoint>>
          aHistoricalData,
      const TimeStamp& aTimeStamp, const ScreenIntPoint& aPosition) {
    MultiTouchInput input(aType, 0, aTimeStamp, 0);
    input.mTouches.AppendElement(SingleTouchData(1, aPosition, {}, 0.0f, 0.0f));
    for (const auto& histData : aHistoricalData) {
      input.mTouches[0].mHistoricalData.AppendElement(
          SingleTouchData::HistoricalTouchData{
              histData.first, histData.second, {}, {}, 0.0f, 0.0f});
    }
    return resampler.ProcessEvent(std::move(input));
  }

  void CheckTime(const TimeStamp& aTimeStamp,
                 const TimeStamp& aExpectedTimeStamp) {
    EXPECT_EQ((aTimeStamp - baseTimeStamp).ToMilliseconds(),
              (aExpectedTimeStamp - baseTimeStamp).ToMilliseconds());
  }

  void CheckEvent(const MultiTouchInput& aEvent,
                  MultiTouchInput::MultiTouchType aExpectedType,
                  std::initializer_list<std::pair<TimeStamp, ScreenIntPoint>>
                      aExpectedHistoricalData,
                  const TimeStamp& aExpectedTimeStamp,
                  const ScreenIntPoint& aExpectedPosition) {
    EXPECT_EQ(aEvent.mType, aExpectedType);
    EXPECT_EQ(aEvent.mTouches.Length(), size_t(1));
    EXPECT_EQ(aEvent.mTouches[0].mHistoricalData.Length(),
              aExpectedHistoricalData.size());
    for (size_t i = 0; i < aExpectedHistoricalData.size(); i++) {
      CheckTime(aEvent.mTouches[0].mHistoricalData[i].mTimeStamp,
                aExpectedHistoricalData.begin()[i].first);
      EXPECT_EQ(aEvent.mTouches[0].mHistoricalData[i].mScreenPoint,
                aExpectedHistoricalData.begin()[i].second);
    }
    CheckTime(aEvent.mTimeStamp, aExpectedTimeStamp);
    EXPECT_EQ(aEvent.mTouches[0].mScreenPoint, aExpectedPosition);
  }

  struct ExpectedOutgoingEvent {
    Maybe<uint64_t> mEventId;
    MultiTouchInput::MultiTouchType mType = MultiTouchInput::MULTITOUCH_START;
    std::initializer_list<std::pair<TimeStamp, ScreenIntPoint>> mHistoricalData;
    TimeStamp mTimeStamp;
    ScreenIntPoint mPosition;
  };

  void CheckOutgoingEvents(
      std::initializer_list<ExpectedOutgoingEvent> aExpectedEvents) {
    auto outgoing = resampler.ConsumeOutgoingEvents();
    EXPECT_EQ(outgoing.size(), aExpectedEvents.size());
    for (const auto& expectedEvent : aExpectedEvents) {
      auto outgoingEvent = std::move(outgoing.front());
      outgoing.pop();

      EXPECT_EQ(outgoingEvent.mEventId, expectedEvent.mEventId);
      CheckEvent(outgoingEvent.mEvent, expectedEvent.mType,
                 expectedEvent.mHistoricalData, expectedEvent.mTimeStamp,
                 expectedEvent.mPosition);
    }
  }

  TimeStamp baseTimeStamp;
  TouchResampler resampler;
};

TEST_F(TouchResamplerTest, BasicExtrapolation) {
  // Execute the following sequence:
  //
  // 0----------10-------16-----20---------------32------------
  // * touchstart at (10, 10)
  //             * touchmove at (20, 20)
  //                      * frame
  //                             * touchend at (20, 20)
  //                                              * frame
  //
  // And expect the following output:
  //
  // 0----------10-------16-----20---------------32------------
  // * touchstart at (10, 10)
  //                      * touchmove at (26, 26)
  //                      * touchmove at (20, 20)
  //                             * touchend at (20, 20)
  //
  // The first frame should emit an extrapolated touchmove from the position
  // data in the touchstart and touchmove events.
  // The touchend should force a synthesized touchmove that returns back to a
  // non-resampled position.

  EXPECT_FALSE(resampler.InTouchingState());

  auto idStart0 = ProcessEvent(MultiTouchInput::MULTITOUCH_START, {}, Time(0.0),
                               ScreenIntPoint(10, 10));
  EXPECT_TRUE(resampler.InTouchingState());
  auto idMove1 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE, {}, Time(10.0),
                              ScreenIntPoint(20, 20));

  resampler.NotifyFrame(Time(16.0));

  CheckOutgoingEvents({
      {Some(idStart0),
       MultiTouchInput::MULTITOUCH_START,
       {},
       Time(0.0),
       ScreenIntPoint(10, 10)},

      {Some(idMove1),
       MultiTouchInput::MULTITOUCH_MOVE,
       {{Time(10.0), ScreenIntPoint(20, 20)}},
       Time(16.0),
       ScreenIntPoint(26, 26)},
  });

  auto idEnd2 = ProcessEvent(MultiTouchInput::MULTITOUCH_END, {}, Time(20.0),
                             ScreenIntPoint(20, 20));

  EXPECT_FALSE(resampler.InTouchingState());

  CheckOutgoingEvents({
      {Nothing(),
       MultiTouchInput::MULTITOUCH_MOVE,
       {},
       Time(16.0),
       ScreenIntPoint(20, 20)},

      {Some(idEnd2),
       MultiTouchInput::MULTITOUCH_END,
       {},
       Time(20.0),
       ScreenIntPoint(20, 20)},
  });

  // No more events should be produced from here on out.
  resampler.NotifyFrame(Time(32.0));
  auto outgoing = resampler.ConsumeOutgoingEvents();
  EXPECT_TRUE(outgoing.empty());
}

TEST_F(TouchResamplerTest, BasicInterpolation) {
  // Same test as BasicExtrapolation, but with a frame time that's 10ms earlier.
  //
  // Execute the following sequence:
  //
  // 0------6---10-----------20--22------------30-------------
  // * touchstart at (10, 10)
  //             * touchmove at (20, 20)
  //        * frame
  //                          * touchend at (20, 20)
  //                              * frame
  //
  // And expect the following output:
  //
  // 0------6---10-----------20--22------------30-------------
  // * touchstart at (10, 10)
  //        * touchmove (16, 16)
  //             * touchmove (20, 20)
  //                          * touchend at (20, 20)
  //
  // The first frame should emit an interpolated touchmove from the position
  // data in the touchstart and touchmove events.
  // The touchend should create a touchmove that returns back to a non-resampled
  // position.

  auto idStart0 = ProcessEvent(MultiTouchInput::MULTITOUCH_START, {}, Time(0.0),
                               ScreenIntPoint(10, 10));
  EXPECT_TRUE(resampler.InTouchingState());
  auto idMove1 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE, {}, Time(10.0),
                              ScreenIntPoint(20, 20));

  resampler.NotifyFrame(Time(6.0));

  CheckOutgoingEvents({
      {Some(idStart0),
       MultiTouchInput::MULTITOUCH_START,
       {},
       Time(0.0),
       ScreenIntPoint(10, 10)},

      {Some(idMove1),
       MultiTouchInput::MULTITOUCH_MOVE,
       {},
       Time(6.0),
       ScreenIntPoint(16, 16)},
  });

  auto idEnd2 = ProcessEvent(MultiTouchInput::MULTITOUCH_END, {}, Time(20.0),
                             ScreenIntPoint(20, 20));
  EXPECT_FALSE(resampler.InTouchingState());

  CheckOutgoingEvents({
      {Nothing(),
       MultiTouchInput::MULTITOUCH_MOVE,
       {},
       Time(10.0),
       ScreenIntPoint(20, 20)},

      {Some(idEnd2),
       MultiTouchInput::MULTITOUCH_END,
       {},
       Time(20.0),
       ScreenIntPoint(20, 20)},
  });

  // No more events should be produced from here on out.
  resampler.NotifyFrame(Time(22.0));
  auto outgoing = resampler.ConsumeOutgoingEvents();
  EXPECT_TRUE(outgoing.empty());
}

TEST_F(TouchResamplerTest, InterpolationFromHistoricalData) {
  // Interpolate from the historical data in a touch move event.
  //
  // Execute the following sequence:
  //
  // 0----------10-------16-----20-----------30--32------------
  // * touchstart at (10, 10)
  //             * [hist] at (20, 25) for
  //             `---------------* touchmove at (30, 30)
  //                      * frame
  //                                          * touchend at (30, 30)
  //                                              * frame
  //
  // And expect the following output:
  //
  // 0----------10-------16-----20-----------30--32------------
  // * touchstart at (10, 10)
  //             * [hist] at (20, 25) for
  //             `--------* touchmove at (26, 28)
  //                             * touchmove at (30, 30)
  //                                          * touchend at (30, 30)
  //
  // The first frame should emit an interpolated touchmove from the position
  // data in the touchmove event, and integrate the historical data point into
  // the resampled event.
  // The touchend should force a synthesized touchmove that returns back to a
  // non-resampled position.

  // This also tests that interpolation works for both x and y, by giving the
  // historical datapoint different values for x and y.
  // (26, 28) is 60% of the way from (20, 25) to (30, 30).

  auto idStart0 = ProcessEvent(MultiTouchInput::MULTITOUCH_START, {}, Time(0.0),
                               ScreenIntPoint(10, 10));
  auto idMove1 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE,
                              {{Time(10.0), ScreenIntPoint(20, 25)}},
                              Time(20.0), ScreenIntPoint(30, 30));
  resampler.NotifyFrame(Time(16.0));
  auto idEnd2 = ProcessEvent(MultiTouchInput::MULTITOUCH_END, {}, Time(30.0),
                             ScreenIntPoint(30, 30));
  resampler.NotifyFrame(Time(32.0));

  CheckOutgoingEvents({
      {Some(idStart0),
       MultiTouchInput::MULTITOUCH_START,
       {},
       Time(0.0),
       ScreenIntPoint(10, 10)},

      {Some(idMove1),
       MultiTouchInput::MULTITOUCH_MOVE,
       {{Time(10.0), ScreenIntPoint(20, 25)}},
       Time(16.0),
       ScreenIntPoint(26, 28)},

      {Nothing(),
       MultiTouchInput::MULTITOUCH_MOVE,
       {},
       Time(20.0),
       ScreenIntPoint(30, 30)},

      {Some(idEnd2),
       MultiTouchInput::MULTITOUCH_END,
       {},
       Time(30.0),
       ScreenIntPoint(30, 30)},
  });
}

TEST_F(TouchResamplerTest, MultipleTouches) {
  EXPECT_FALSE(resampler.InTouchingState());

  // Touch start
  MultiTouchInput inputStart0(MultiTouchInput::MULTITOUCH_START, 0, Time(0.0),
                              0);
  inputStart0.mTouches.AppendElement(
      SingleTouchData(1, ScreenIntPoint(10, 10), {}, 0.0f, 0.0f));
  auto idStart0 = resampler.ProcessEvent(std::move(inputStart0));
  EXPECT_TRUE(resampler.InTouchingState());

  // Touch move
  MultiTouchInput inputMove1(MultiTouchInput::MULTITOUCH_MOVE, 0, Time(20.0),
                             0);
  inputMove1.mTouches.AppendElement(
      SingleTouchData(1, ScreenIntPoint(30, 30), {}, 0.0f, 0.0f));
  inputMove1.mTouches[0].mHistoricalData.AppendElement(
      SingleTouchData::HistoricalTouchData{
          Time(10.0), ScreenIntPoint(20, 25), {}, {}, 0.0f, 0.0f});
  auto idMove1 = resampler.ProcessEvent(std::move(inputMove1));
  EXPECT_TRUE(resampler.InTouchingState());

  // Frame
  resampler.NotifyFrame(Time(16.0));

  // Touch move
  MultiTouchInput inputMove2(MultiTouchInput::MULTITOUCH_MOVE, 0, Time(30.0),
                             0);
  inputMove2.mTouches.AppendElement(
      SingleTouchData(1, ScreenIntPoint(30, 40), {}, 0.0f, 0.0f));
  auto idMove2 = resampler.ProcessEvent(std::move(inputMove2));
  EXPECT_TRUE(resampler.InTouchingState());

  // Touch start
  MultiTouchInput inputStart3(MultiTouchInput::MULTITOUCH_START, 0, Time(30.0),
                              0);
  inputStart3.mTouches.AppendElement(
      SingleTouchData(2, ScreenIntPoint(100, 10), {}, 0.0f, 0.0f));
  inputStart3.mTouches.AppendElement(
      SingleTouchData(1, ScreenIntPoint(30, 40), {}, 0.0f, 0.0f));
  auto idStart3 = resampler.ProcessEvent(std::move(inputStart3));
  EXPECT_TRUE(resampler.InTouchingState());

  // Touch move
  MultiTouchInput inputMove4(MultiTouchInput::MULTITOUCH_MOVE, 0, Time(40.0),
                             0);
  inputMove4.mTouches.AppendElement(
      SingleTouchData(1, ScreenIntPoint(30, 50), {}, 0.0f, 0.0f));
  inputMove4.mTouches.AppendElement(
      SingleTouchData(2, ScreenIntPoint(100, 30), {}, 0.0f, 0.0f));
  auto idMove4 = resampler.ProcessEvent(std::move(inputMove4));
  EXPECT_TRUE(resampler.InTouchingState());

  // Frame
  resampler.NotifyFrame(Time(32.0));

  // Touch move
  MultiTouchInput inputMove5(MultiTouchInput::MULTITOUCH_MOVE, 0, Time(50.0),
                             0);
  inputMove5.mTouches.AppendElement(
      SingleTouchData(1, ScreenIntPoint(30, 60), {}, 0.0f, 0.0f));
  inputMove5.mTouches.AppendElement(
      SingleTouchData(2, ScreenIntPoint(100, 40), {}, 0.0f, 0.0f));
  auto idMove5 = resampler.ProcessEvent(std::move(inputMove5));
  EXPECT_TRUE(resampler.InTouchingState());

  // Touch end
  MultiTouchInput inputEnd6(MultiTouchInput::MULTITOUCH_END, 0, Time(50.0), 0);
  // Touch point with identifier 1 is lifted
  inputEnd6.mTouches.AppendElement(
      SingleTouchData(1, ScreenIntPoint(30, 60), {}, 0.0f, 0.0f));
  auto idEnd6 = resampler.ProcessEvent(std::move(inputEnd6));
  EXPECT_TRUE(resampler.InTouchingState());

  // Frame
  resampler.NotifyFrame(Time(48.0));

  // Touch move
  MultiTouchInput inputMove7(MultiTouchInput::MULTITOUCH_MOVE, 0, Time(60.0),
                             0);
  inputMove7.mTouches.AppendElement(
      SingleTouchData(2, ScreenIntPoint(100, 60), {}, 0.0f, 0.0f));
  auto idMove7 = resampler.ProcessEvent(std::move(inputMove7));
  EXPECT_TRUE(resampler.InTouchingState());

  // Frame
  resampler.NotifyFrame(Time(64.0));

  // Touch end
  MultiTouchInput inputEnd8(MultiTouchInput::MULTITOUCH_END, 0, Time(70.0), 0);
  // Touch point with identifier 2 is lifted
  inputEnd8.mTouches.AppendElement(
      SingleTouchData(2, ScreenIntPoint(100, 60), {}, 0.0f, 0.0f));
  auto idEnd8 = resampler.ProcessEvent(std::move(inputEnd8));
  EXPECT_FALSE(resampler.InTouchingState());

  // Check outgoing events
  auto outgoing = resampler.ConsumeOutgoingEvents();
  EXPECT_EQ(outgoing.size(), size_t(9));

  auto outgoingStart0 = std::move(outgoing.front());
  outgoing.pop();
  EXPECT_EQ(outgoingStart0.mEventId, Some(idStart0));
  CheckEvent(outgoingStart0.mEvent, MultiTouchInput::MULTITOUCH_START, {},
             Time(0.0), ScreenIntPoint(10, 10));

  auto outgoingMove1 = std::move(outgoing.front());
  outgoing.pop();
  EXPECT_EQ(outgoingMove1.mEventId, Some(idMove1));
  // (26, 28) is 60% of the way from (20, 25) to (30, 30).
  CheckEvent(outgoingMove1.mEvent, MultiTouchInput::MULTITOUCH_MOVE,
             {{Time(10.0), ScreenIntPoint(20, 25)}}, Time(16.0),
             ScreenIntPoint(26, 28));

  auto outgoingMove2 = std::move(outgoing.front());
  outgoing.pop();
  EXPECT_EQ(outgoingMove2.mEventId, Some(idMove2));
  CheckEvent(outgoingMove2.mEvent, MultiTouchInput::MULTITOUCH_MOVE,
             {{Time(20.0), ScreenIntPoint(30, 30)}}, Time(30.0),
             ScreenIntPoint(30, 40));

  auto outgoingStart3 = std::move(outgoing.front());
  outgoing.pop();
  EXPECT_EQ(outgoingStart3.mEventId, Some(idStart3));
  EXPECT_EQ(outgoingStart3.mEvent.mType, MultiTouchInput::MULTITOUCH_START);
  CheckTime(outgoingStart3.mEvent.mTimeStamp, Time(30.0));
  EXPECT_EQ(outgoingStart3.mEvent.mTouches.Length(), size_t(2));
  // touch order should be taken from the original touch start event
  EXPECT_EQ(outgoingStart3.mEvent.mTouches[0].mIdentifier, 2);
  EXPECT_EQ(outgoingStart3.mEvent.mTouches[0].mScreenPoint,
            ScreenIntPoint(100, 10));
  EXPECT_EQ(outgoingStart3.mEvent.mTouches[1].mIdentifier, 1);
  EXPECT_EQ(outgoingStart3.mEvent.mTouches[1].mScreenPoint,
            ScreenIntPoint(30, 40));

  auto outgoingMove4 = std::move(outgoing.front());
  outgoing.pop();
  EXPECT_EQ(outgoingMove4.mEventId, Some(idMove4));
  EXPECT_EQ(outgoingMove4.mEvent.mType, MultiTouchInput::MULTITOUCH_MOVE);
  CheckTime(outgoingMove4.mEvent.mTimeStamp, Time(32.0));
  EXPECT_EQ(outgoingMove4.mEvent.mTouches.Length(), size_t(2));
  // Touch order should be taken from the original touch move event.
  // Both touches should be resampled.
  EXPECT_EQ(outgoingMove4.mEvent.mTouches[0].mIdentifier, 1);
  EXPECT_EQ(outgoingMove4.mEvent.mTouches[0].mScreenPoint,
            ScreenIntPoint(30, 42));
  EXPECT_EQ(outgoingMove4.mEvent.mTouches[1].mIdentifier, 2);
  EXPECT_EQ(outgoingMove4.mEvent.mTouches[1].mScreenPoint,
            ScreenIntPoint(100, 14));

  auto outgoingMove5 = std::move(outgoing.front());
  outgoing.pop();
  EXPECT_EQ(outgoingMove5.mEventId, Some(idMove5));
  EXPECT_EQ(outgoingMove5.mEvent.mType, MultiTouchInput::MULTITOUCH_MOVE);
  CheckTime(outgoingMove5.mEvent.mTimeStamp, Time(50.0));
  EXPECT_EQ(outgoingMove5.mEvent.mTouches.Length(), size_t(2));
  // touch order should be taken from the original touch move event
  EXPECT_EQ(outgoingMove5.mEvent.mTouches[0].mIdentifier, 1);
  EXPECT_EQ(outgoingMove5.mEvent.mTouches[0].mScreenPoint,
            ScreenIntPoint(30, 60));
  EXPECT_EQ(outgoingMove5.mEvent.mTouches[0].mHistoricalData.Length(),
            size_t(1));
  CheckTime(outgoingMove5.mEvent.mTouches[0].mHistoricalData[0].mTimeStamp,
            Time(40.0));
  EXPECT_EQ(outgoingMove5.mEvent.mTouches[0].mHistoricalData[0].mScreenPoint,
            ScreenIntPoint(30, 50));
  EXPECT_EQ(outgoingMove5.mEvent.mTouches[1].mIdentifier, 2);
  EXPECT_EQ(outgoingMove5.mEvent.mTouches[1].mScreenPoint,
            ScreenIntPoint(100, 40));
  EXPECT_EQ(outgoingMove5.mEvent.mTouches[1].mHistoricalData.Length(),
            size_t(1));
  CheckTime(outgoingMove5.mEvent.mTouches[1].mHistoricalData[0].mTimeStamp,
            Time(40.0));
  EXPECT_EQ(outgoingMove5.mEvent.mTouches[1].mHistoricalData[0].mScreenPoint,
            ScreenIntPoint(100, 30));

  auto outgoingEnd6 = std::move(outgoing.front());
  outgoing.pop();
  EXPECT_EQ(outgoingEnd6.mEventId, Some(idEnd6));
  CheckEvent(outgoingEnd6.mEvent, MultiTouchInput::MULTITOUCH_END, {},
             Time(50.0), ScreenIntPoint(30, 60));

  auto outgoingMove7 = std::move(outgoing.front());
  outgoing.pop();
  EXPECT_EQ(outgoingMove7.mEventId, Some(idMove7));
  // No extrapolation because the frame at 64.0 cleared the data points because
  // there was no pending touch move event at that point
  CheckEvent(outgoingMove7.mEvent, MultiTouchInput::MULTITOUCH_MOVE, {},
             Time(60.0), ScreenIntPoint(100, 60));
  EXPECT_EQ(outgoingMove7.mEvent.mTouches[0].mIdentifier, 2);

  auto outgoingEnd8 = std::move(outgoing.front());
  outgoing.pop();
  EXPECT_EQ(outgoingEnd8.mEventId, Some(idEnd8));
  CheckEvent(outgoingEnd8.mEvent, MultiTouchInput::MULTITOUCH_END, {},
             Time(70.0), ScreenIntPoint(100, 60));
}

TEST_F(TouchResamplerTest, MovingPauses) {
  auto idStart0 = ProcessEvent(MultiTouchInput::MULTITOUCH_START, {}, Time(0.0),
                               ScreenIntPoint(10, 10));
  auto idMove1 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE, {}, Time(10.0),
                              ScreenIntPoint(20, 20));
  resampler.NotifyFrame(Time(16.0));
  auto idMove2 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE, {}, Time(30.0),
                              ScreenIntPoint(40, 40));
  resampler.NotifyFrame(Time(32.0));
  auto idMove3 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE, {}, Time(40.0),
                              ScreenIntPoint(50, 40));
  resampler.NotifyFrame(Time(48.0));
  resampler.NotifyFrame(Time(64.0));
  auto idEnd4 = ProcessEvent(MultiTouchInput::MULTITOUCH_END, {}, Time(70.0),
                             ScreenIntPoint(50, 40));

  CheckOutgoingEvents({
      {Some(idStart0),
       MultiTouchInput::MULTITOUCH_START,
       {},
       Time(0.0),
       ScreenIntPoint(10, 10)},

      {Some(idMove1),
       MultiTouchInput::MULTITOUCH_MOVE,
       {{Time(10.0), ScreenIntPoint(20, 20)}},
       Time(16.0),
       ScreenIntPoint(26, 26)},

      {Some(idMove2),
       MultiTouchInput::MULTITOUCH_MOVE,
       {{Time(30.0), ScreenIntPoint(40, 40)}},
       Time(32.0),
       ScreenIntPoint(42, 42)},

      {Some(idMove3),
       MultiTouchInput::MULTITOUCH_MOVE,
       {{Time(40.0), ScreenIntPoint(50, 40)}},
       Time(48.0),
       ScreenIntPoint(58, 40)},

      // There was no event between two frames here, so we expect a reset event,
      // so that we pause at a non-resampled position.
      {Nothing(),
       MultiTouchInput::MULTITOUCH_MOVE,
       {},
       Time(48.0),
       ScreenIntPoint(50, 40)},

      {Some(idEnd4),
       MultiTouchInput::MULTITOUCH_END,
       {},
       Time(70.0),
       ScreenIntPoint(50, 40)},
  });
}

TEST_F(TouchResamplerTest, MixedInterAndExtrapolation) {
  auto idStart0 = ProcessEvent(MultiTouchInput::MULTITOUCH_START, {}, Time(0.0),
                               ScreenIntPoint(0, 0));
  auto idMove1 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE, {}, Time(10.0),
                              ScreenIntPoint(0, 10));
  resampler.NotifyFrame(Time(11.0));  // 16 - 5
  auto idMove2 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE,
                              {{Time(20.0), ScreenIntPoint(0, 20)}}, Time(30.0),
                              ScreenIntPoint(0, 30));
  resampler.NotifyFrame(Time(27.0));  // 32 - 5
  auto idMove3 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE, {}, Time(40.0),
                              ScreenIntPoint(0, 40));
  resampler.NotifyFrame(Time(43.0));  // 48 - 5
  auto idMove4 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE,
                              {{Time(50.0), ScreenIntPoint(0, 50)}}, Time(60.0),
                              ScreenIntPoint(0, 60));
  resampler.NotifyFrame(Time(59.0));  // 64 - 5
  auto idEnd5 = ProcessEvent(MultiTouchInput::MULTITOUCH_END, {}, Time(70.0),
                             ScreenIntPoint(0, 60));

  CheckOutgoingEvents({
      {Some(idStart0),
       MultiTouchInput::MULTITOUCH_START,
       {},
       Time(0.0),
       ScreenIntPoint(0, 0)},

      {Some(idMove1),
       MultiTouchInput::MULTITOUCH_MOVE,
       {{Time(10.0), ScreenIntPoint(0, 10)}},
       Time(11.0),
       ScreenIntPoint(0, 11)},

      {Some(idMove2),
       MultiTouchInput::MULTITOUCH_MOVE,
       {{Time(20.0), ScreenIntPoint(0, 20)}},
       Time(27.0),
       ScreenIntPoint(0, 27)},

      {Some(idMove3),
       MultiTouchInput::MULTITOUCH_MOVE,
       {{Time(30.0), ScreenIntPoint(0, 30)},
        {Time(40.0), ScreenIntPoint(0, 40)}},
       Time(43.0),
       ScreenIntPoint(0, 43)},

      {Some(idMove4),
       MultiTouchInput::MULTITOUCH_MOVE,
       {{Time(50.0), ScreenIntPoint(0, 50)}},
       Time(59.0),
       ScreenIntPoint(0, 59)},

      {Nothing(),
       MultiTouchInput::MULTITOUCH_MOVE,
       {},
       Time(60.0),
       ScreenIntPoint(0, 60)},

      {Some(idEnd5),
       MultiTouchInput::MULTITOUCH_END,
       {},
       Time(70.0),
       ScreenIntPoint(0, 60)},
  });
}

TEST_F(TouchResamplerTest, MultipleMoveEvents) {
  // Test what happens if multiple touch move events appear between two frames.
  // This scenario shouldn't occur on Android but we should be able to deal with
  // it anyway. Check that we don't discard any event IDs.
  auto idStart0 = ProcessEvent(MultiTouchInput::MULTITOUCH_START, {}, Time(0.0),
                               ScreenIntPoint(0, 0));
  auto idMove1 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE, {}, Time(10.0),
                              ScreenIntPoint(0, 10));
  resampler.NotifyFrame(Time(11.0));  // 16 - 5
  auto idMove2 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE,
                              {{Time(20.0), ScreenIntPoint(0, 20)}}, Time(30.0),
                              ScreenIntPoint(0, 30));
  auto idMove3 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE, {}, Time(40.0),
                              ScreenIntPoint(0, 40));
  auto idMove4 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE,
                              {{Time(45.0), ScreenIntPoint(0, 45)}}, Time(50.0),
                              ScreenIntPoint(0, 50));
  auto idMove5 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE, {}, Time(55.0),
                              ScreenIntPoint(0, 55));
  resampler.NotifyFrame(Time(43.0));  // 48 - 5
  resampler.NotifyFrame(Time(59.0));  // 64 - 5
  auto idEnd5 = ProcessEvent(MultiTouchInput::MULTITOUCH_END, {}, Time(70.0),
                             ScreenIntPoint(0, 60));

  CheckOutgoingEvents({
      {Some(idStart0),
       MultiTouchInput::MULTITOUCH_START,
       {},
       Time(0.0),
       ScreenIntPoint(0, 0)},

      {Some(idMove1),
       MultiTouchInput::MULTITOUCH_MOVE,
       {{Time(10.0), ScreenIntPoint(0, 10)}},
       Time(11.0),
       ScreenIntPoint(0, 11)},

      {Some(idMove2),
       MultiTouchInput::MULTITOUCH_MOVE,
       {{Time(20.0), ScreenIntPoint(0, 20)}},
       Time(30.0),
       ScreenIntPoint(0, 30)},

      {Some(idMove3),
       MultiTouchInput::MULTITOUCH_MOVE,
       {},
       Time(40.0),
       ScreenIntPoint(0, 40)},

      {Some(idMove4),
       MultiTouchInput::MULTITOUCH_MOVE,
       {},
       Time(43.0),
       ScreenIntPoint(0, 43)},

      {Some(idMove5),
       MultiTouchInput::MULTITOUCH_MOVE,
       {{Time(45.0), ScreenIntPoint(0, 45)},
        {Time(50.0), ScreenIntPoint(0, 50)},
        {Time(55.0), ScreenIntPoint(0, 55)}},
       Time(59.0),
       ScreenIntPoint(0, 59)},

      {Nothing(),
       MultiTouchInput::MULTITOUCH_MOVE,
       {},
       Time(59.0),
       ScreenIntPoint(0, 55)},

      {Some(idEnd5),
       MultiTouchInput::MULTITOUCH_END,
       {},
       Time(70.0),
       ScreenIntPoint(0, 60)},
  });
}

TEST_F(TouchResamplerTest, LimitFuturePrediction) {
  auto idStart0 = ProcessEvent(MultiTouchInput::MULTITOUCH_START, {}, Time(0.0),
                               ScreenIntPoint(0, 0));
  // Fingers move until time 44, then pause. UI thread is occupied until 64.
  auto idMove1 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE,
                              {{Time(20.0), ScreenIntPoint(0, 20)},
                               {Time(32.0), ScreenIntPoint(0, 32)}},
                              Time(44.0), ScreenIntPoint(0, 44));
  resampler.NotifyFrame(Time(59.0));  // 64 - 5
  auto idEnd2 = ProcessEvent(MultiTouchInput::MULTITOUCH_END, {}, Time(70.0),
                             ScreenIntPoint(0, 44));

  CheckOutgoingEvents({
      {Some(idStart0),
       MultiTouchInput::MULTITOUCH_START,
       {},
       Time(0.0),
       ScreenIntPoint(0, 0)},

      // kTouchResampleMaxPredictMs == 8
      // Refuse to predict more than 8ms into the future, the fingers might have
      // paused. Make an event for time 52 (= 44 + 8) instead of 59.
      {Some(idMove1),
       MultiTouchInput::MULTITOUCH_MOVE,
       {{Time(20.0), ScreenIntPoint(0, 20)},
        {Time(32.0), ScreenIntPoint(0, 32)},
        {Time(44.0), ScreenIntPoint(0, 44)}},
       Time(52.0),
       ScreenIntPoint(0, 52)},

      {Nothing(),
       MultiTouchInput::MULTITOUCH_MOVE,
       {},
       Time(52.0),
       ScreenIntPoint(0, 44)},

      {Some(idEnd2),
       MultiTouchInput::MULTITOUCH_END,
       {},
       Time(70.0),
       ScreenIntPoint(0, 44)},
  });
}

TEST_F(TouchResamplerTest, LimitBacksampling) {
  auto idStart0 = ProcessEvent(MultiTouchInput::MULTITOUCH_START, {}, Time(0.0),
                               ScreenIntPoint(0, 0));
  // Fingers move until time 44, then pause. UI thread is occupied until 64.
  // Then we get a frame callback with a very outdated frametime.
  auto idMove1 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE,
                              {{Time(20.0), ScreenIntPoint(0, 20)},
                               {Time(32.0), ScreenIntPoint(0, 32)}},
                              Time(44.0), ScreenIntPoint(0, 44));
  resampler.NotifyFrame(Time(11.0));  // 16 - 5
  auto idEnd2 = ProcessEvent(MultiTouchInput::MULTITOUCH_END, {}, Time(70.0),
                             ScreenIntPoint(0, 44));

  CheckOutgoingEvents({
      {Some(idStart0),
       MultiTouchInput::MULTITOUCH_START,
       {},
       Time(0.0),
       ScreenIntPoint(0, 0)},

      // kTouchResampleMaxBacksampleMs == 20
      // Refuse to sample further back than 20ms before the last data point.
      // Make an event for time 24 (= 44 - 20) instead of time 11.
      {Some(idMove1),
       MultiTouchInput::MULTITOUCH_MOVE,
       {{Time(20.0), ScreenIntPoint(0, 20)}},
       Time(24.0),
       ScreenIntPoint(0, 24)},

      {Nothing(),
       MultiTouchInput::MULTITOUCH_MOVE,
       {{Time(32.0), ScreenIntPoint(0, 32)}},
       Time(44.0),
       ScreenIntPoint(0, 44)},

      {Some(idEnd2),
       MultiTouchInput::MULTITOUCH_END,
       {},
       Time(70.0),
       ScreenIntPoint(0, 44)},
  });
}

TEST_F(TouchResamplerTest, DontExtrapolateFromOldTouch) {
  auto idStart0 = ProcessEvent(MultiTouchInput::MULTITOUCH_START, {}, Time(0.0),
                               ScreenIntPoint(0, 0));
  // Fingers move until time 40, then pause. UI thread is occupied until 64.
  auto idMove1 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE,
                              {{Time(20.0), ScreenIntPoint(0, 20)},
                               {Time(30.0), ScreenIntPoint(0, 30)}},
                              Time(40.0), ScreenIntPoint(0, 40));
  resampler.NotifyFrame(Time(59.0));  // 64 - 5
  auto idEnd2 = ProcessEvent(MultiTouchInput::MULTITOUCH_END, {}, Time(70.0),
                             ScreenIntPoint(0, 44));

  CheckOutgoingEvents({
      {Some(idStart0),
       MultiTouchInput::MULTITOUCH_START,
       {},
       Time(0.0),
       ScreenIntPoint(0, 0)},

      // kTouchResampleOldTouchThresholdMs == 17
      // Refuse to extrapolate from a data point that's more than 17ms older
      // than the frame time.
      {Some(idMove1),
       MultiTouchInput::MULTITOUCH_MOVE,
       {{Time(20.0), ScreenIntPoint(0, 20)},
        {Time(30.0), ScreenIntPoint(0, 30)}},
       Time(40.0),
       ScreenIntPoint(0, 40)},

      {Some(idEnd2),
       MultiTouchInput::MULTITOUCH_END,
       {},
       Time(70.0),
       ScreenIntPoint(0, 44)},
  });
}

TEST_F(TouchResamplerTest, DontExtrapolateIfTooOld) {
  auto idStart0 = ProcessEvent(MultiTouchInput::MULTITOUCH_START, {}, Time(0.0),
                               ScreenIntPoint(0, 0));
  // Fingers move until time 10, pause, and move again at 55.
  // UI thread is occupied until 64.
  auto idMove1 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE,
                              {{Time(10.0), ScreenIntPoint(0, 10)}}, Time(55.0),
                              ScreenIntPoint(0, 55));
  resampler.NotifyFrame(Time(59.0));  // 64 - 5
  auto idEnd2 = ProcessEvent(MultiTouchInput::MULTITOUCH_END, {}, Time(70.0),
                             ScreenIntPoint(0, 60));

  CheckOutgoingEvents({
      {Some(idStart0),
       MultiTouchInput::MULTITOUCH_START,
       {},
       Time(0.0),
       ScreenIntPoint(0, 0)},

      // kTouchResampleWindowSize == 40
      // Refuse to resample between two data points that are more than 40ms
      // apart.
      {Some(idMove1),
       MultiTouchInput::MULTITOUCH_MOVE,
       {{Time(10.0), ScreenIntPoint(0, 10)}},
       Time(55.0),
       ScreenIntPoint(0, 55)},

      {Some(idEnd2),
       MultiTouchInput::MULTITOUCH_END,
       {},
       Time(70.0),
       ScreenIntPoint(0, 60)},
  });
}

TEST_F(TouchResamplerTest, DontInterpolateIfTooOld) {
  auto idStart0 = ProcessEvent(MultiTouchInput::MULTITOUCH_START, {}, Time(0.0),
                               ScreenIntPoint(0, 0));
  // Fingers move until time 10, pause, and move again at 60.
  // UI thread is occupied until 64.
  auto idMove1 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE,
                              {{Time(10.0), ScreenIntPoint(0, 10)}}, Time(60.0),
                              ScreenIntPoint(0, 60));
  resampler.NotifyFrame(Time(59.0));  // 64 - 5
  auto idEnd2 = ProcessEvent(MultiTouchInput::MULTITOUCH_END, {}, Time(70.0),
                             ScreenIntPoint(0, 60));

  CheckOutgoingEvents({
      {Some(idStart0),
       MultiTouchInput::MULTITOUCH_START,
       {},
       Time(0.0),
       ScreenIntPoint(0, 0)},

      // kTouchResampleWindowSize == 40
      // Refuse to resample between two data points that are more than 40ms
      // apart.
      {Some(idMove1),
       MultiTouchInput::MULTITOUCH_MOVE,
       {{Time(10.0), ScreenIntPoint(0, 10)}},
       Time(60.0),
       ScreenIntPoint(0, 60)},

      {Some(idEnd2),
       MultiTouchInput::MULTITOUCH_END,
       {},
       Time(70.0),
       ScreenIntPoint(0, 60)},
  });
}

TEST_F(TouchResamplerTest, DiscardOutdatedHistoricalData) {
  auto idStart0 = ProcessEvent(MultiTouchInput::MULTITOUCH_START, {}, Time(0.0),
                               ScreenIntPoint(0, 0));
  auto idMove1 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE,
                              {{Time(10.0), ScreenIntPoint(0, 10)}}, Time(16.0),
                              ScreenIntPoint(0, 16));
  resampler.NotifyFrame(Time(20.0));
  auto idMove2 = ProcessEvent(MultiTouchInput::MULTITOUCH_MOVE,
                              {{Time(18.0), ScreenIntPoint(0, 18)}}, Time(25.0),
                              ScreenIntPoint(0, 25));
  auto idEnd3 = ProcessEvent(MultiTouchInput::MULTITOUCH_END, {}, Time(35.0),
                             ScreenIntPoint(0, 25));

  CheckOutgoingEvents({
      {Some(idStart0),
       MultiTouchInput::MULTITOUCH_START,
       {},
       Time(0.0),
       ScreenIntPoint(0, 0)},

      {Some(idMove1),
       MultiTouchInput::MULTITOUCH_MOVE,
       {{Time(10.0), ScreenIntPoint(0, 10)},
        {Time(16.0), ScreenIntPoint(0, 16)}},
       Time(20.0),
       ScreenIntPoint(0, 20)},

      // Discard the historical data point from time 18, because we've already
      // sent out an event with time 20 and don't want to go back before that.
      {Some(idMove2),
       MultiTouchInput::MULTITOUCH_MOVE,
       {},
       Time(25.0),
       ScreenIntPoint(0, 25)},

      {Some(idEnd3),
       MultiTouchInput::MULTITOUCH_END,
       {},
       Time(35.0),
       ScreenIntPoint(0, 25)},
  });
}
