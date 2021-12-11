/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_TouchResampler_h
#define mozilla_widget_TouchResampler_h

#include <queue>
#include <unordered_map>

#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#include "InputData.h"

namespace mozilla {
namespace widget {

/**
 * De-jitters touch motions by resampling (interpolating or extrapolating) touch
 * positions for the vsync timestamp.
 *
 * Touch resampling improves the touch panning experience on devices where touch
 * positions are sampled at a rate that's not an integer multiple of the display
 * refresh rate, for example 100Hz touch sampling on a 60Hz display: Without
 * resampling, we would alternate between taking one touch sample or two touch
 * samples into account each frame, creating a jittery motion ("small step, big
 * step, small step, big step").
 * Intended for use on Android, where both touch events and vsync notifications
 * arrive on the same thread, the Java UI thread.
 * This class is not thread safe.
 *
 * TouchResampler operates in the following way:
 *
 * Original events are fed into ProcessEvent().
 * Outgoing events (potentially resampled for resampling) are added to a queue
 * and can be consumed by calling ConsumeOutgoingEvents(). Touch events which
 * are not touch move events are forwarded instantly and not resampled. Only
 * touch move events are resampled. Whenever a touch move event is received, it
 * gets delayed until NotifyFrame() is called, at which point it is resampled
 * into a resampled version for the given frame timestamp, and added to the
 * outgoing queue. If no touch move event is received between two consecutive
 * frames, this is treated as a stop in the touch motion. If the last outgoing
 * event was an resampled touch move event, we return back to the non-resampled
 * state by emitting a copy of the last original touch move event, which has
 * unmodified position data. Touch events which are not touch move events also
 * force a return to the non-resampled state before they are moved to the
 * outgoing queue.
 */
class TouchResampler final {
 public:
  // Feed a touch event into the interpolater. Returns an ID that can be used to
  // match outgoing events to this incoming event, to track data associated with
  // this event.
  uint64_t ProcessEvent(MultiTouchInput&& aInput);

  // Emit events, potentially resampled, for this timestamp. The events are put
  // into the outgoing queue. May not emit any events if there's no update.
  void NotifyFrame(const TimeStamp& aTimeStamp);

  // Returns true between the start and the end of a touch gesture. During this
  // time, the caller should keep itself registered with the system frame
  // callback mechanism, so that NotifyFrame() can be called on every frame.
  // (Otherwise, if we only registered the callback after receiving a touch move
  // event, the frame callback might be delayed by a full frame.)
  bool InTouchingState() const { return mCurrentTouches.HasTouch(); }

  struct OutgoingEvent {
    // The event, potentially modified from the original for resampling.
    MultiTouchInput mEvent;

    // Some(eventId) if this event is a modified version of an original event,
    // Nothing() if this is an extra event.
    Maybe<uint64_t> mEventId;
  };

  // Returns the outgoing events that were produced since the last call.
  // No event IDs will be skipped. Returns at least one outgoing event for each
  // incoming event (possibly after a delay), and potential extra events with
  // no originating event ID.
  // Outgoing events should be consumed after every call to ProcessEvent() and
  // after every call to NotifyFrame().
  std::queue<OutgoingEvent> ConsumeOutgoingEvents() {
    return std::move(mOutgoingEvents);
  }

 private:
  // Add the event to the outgoing queue.
  void EmitEvent(MultiTouchInput&& aInput, uint64_t aEventId) {
    mLastEmittedEventTime = aInput.mTimeStamp;
    mOutgoingEvents.push(OutgoingEvent{std::move(aInput), Some(aEventId)});
  }

  // Emit an event that does not correspond to an incoming event.
  void EmitExtraEvent(MultiTouchInput&& aInput) {
    mLastEmittedEventTime = aInput.mTimeStamp;
    mOutgoingEvents.push(OutgoingEvent{std::move(aInput), Nothing()});
  }

  // Move any touch move events that we deferred for resampling to the outgoing
  // queue unmodified, leaving mDeferredTouchMoveEvents empty.
  void FlushDeferredTouchMoveEventsUnresampled();

  // Must only be called if mInResampledState is true and
  // mDeferredTouchMoveEvents is empty. Emits mOriginalOfResampledTouchMove,
  // with a potentially adjusted timestamp for correct ordering.
  void ReturnToNonResampledState();

  // Takes historical touch data from mRemainingTouchData and prepends it to the
  // data in aInput.
  void PrependLeftoverHistoricalData(MultiTouchInput* aInput);

  struct DataPoint {
    TimeStamp mTimeStamp;
    ScreenIntPoint mPosition;
  };

  struct TouchInfo {
    void Update(const SingleTouchData& aTouch, const TimeStamp& aEventTime);
    ScreenIntPoint ResampleAtTime(const ScreenIntPoint& aLastObservedPosition,
                                  const TimeStamp& aTimeStamp);

    int32_t mIdentifier = 0;
    Maybe<DataPoint> mBaseDataPoint;
    Maybe<DataPoint> mLatestDataPoint;
  };

  struct CurrentTouches {
    void UpdateFromEvent(const MultiTouchInput& aInput);
    bool HasTouch() const { return !mTouches.IsEmpty(); }
    TimeStamp LatestDataPointTime() { return mLatestDataPointTime; }

    ScreenIntPoint ResampleTouchPositionAtTime(
        int32_t aIdentifier, const ScreenIntPoint& aLastObservedPosition,
        const TimeStamp& aTimeStamp);

    void ClearDataPoints() {
      for (auto& touch : mTouches) {
        touch.mBaseDataPoint = Nothing();
        touch.mLatestDataPoint = Nothing();
      }
    }

   private:
    nsTArray<TouchInfo>::iterator TouchByIdentifier(int32_t aIdentifier);

    nsTArray<TouchInfo> mTouches;
    TimeStamp mLatestDataPointTime;
  };

  // The current touch positions with historical data points. This data only
  // contains original non-resampled positions from the incoming touch events.
  CurrentTouches mCurrentTouches;

  // Incoming touch move events are stored here until NotifyFrame is called.
  std::queue<std::pair<MultiTouchInput, uint64_t>> mDeferredTouchMoveEvents;

  // Stores any touch samples that were not included in the last emitted touch
  // move event because they were in the future compared to the emitted event's
  // timestamp. These data points should be prepended to the historical data of
  // the next emitted touch move evnt.
  // Can only be non-empty if mInResampledState is true.
  std::unordered_map<int32_t, nsTArray<SingleTouchData::HistoricalTouchData>>
      mRemainingTouchData;

  // If we're in an resampled state, because the last outgoing event was a
  // resampled touch move event, then this contains a copy of the unresampled,
  // original touch move event.
  // Some() iff mInResampledState is true.
  Maybe<MultiTouchInput> mOriginalOfResampledTouchMove;

  // The stream of outgoing events that can be consumed by our caller.
  std::queue<OutgoingEvent> mOutgoingEvents;

  // The timestamp of the event that was emitted most recently, or the null
  // timestamp if no event has been emitted yet.
  TimeStamp mLastEmittedEventTime;

  uint64_t mNextEventId = 0;

  // True if the last outgoing event was a touch move event with an resampled
  // position. We only want to stay in this state as long as a continuous stream
  // of touch move events is coming in.
  bool mInResampledState = false;
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_TouchResampler_h
