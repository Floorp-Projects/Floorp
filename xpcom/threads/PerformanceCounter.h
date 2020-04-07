/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_PerformanceCounter_h
#define mozilla_PerformanceCounter_h

#include "mozilla/Array.h"
#include "mozilla/Atomics.h"
#include "mozilla/TaskCategory.h"
#include "nsISupportsImpl.h"
#include "nsString.h"

namespace mozilla {

/*
 * The DispatchCategory class is used to fake the inheritance
 * of the TaskCategory enum so we can extend it to hold
 * one more value corresponding to the category
 * we use when a worker dispatches a call.
 *
 */
class DispatchCategory final {
 public:
  explicit DispatchCategory(uint32_t aValue) : mValue(aValue) {
    // Since DispatchCategory is adding one single value to the
    // TaskCategory enum, we can check here that the value is
    // the next index e.g. TaskCategory::Count
    MOZ_ASSERT(aValue == (uint32_t)TaskCategory::Count);
  }

  constexpr explicit DispatchCategory(TaskCategory aValue)
      : mValue((uint32_t)aValue) {}

  uint32_t GetValue() const { return mValue; }

  static const DispatchCategory Worker;

 private:
  uint32_t mValue;
};

typedef Array<Atomic<uint32_t>, (uint32_t)TaskCategory::Count + 1>
    DispatchCounter;

// PerformanceCounter is a class that can be used to keep track of
// runnable execution times and dispatch counts.
//
// - runnable execution time: time spent in a runnable when called
//   in nsThread::ProcessNextEvent (not counting recursive calls)
// - dispatch counts: number of times a tracked runnable is dispatched
//   in nsThread. Useful to measure the activity of a tab or worker.
//
// The PerformanceCounter class is currently instantiated in DocGroup
// and WorkerPrivate in order to count how many scheduler dispatches
// are done through them, and how long the execution lasts.
//
// The execution time is calculated by the nsThread class (and its
// inherited WorkerThread class) in its ProcessNextEvent method.
//
// For each processed runnable, nsThread will reach out the
// PerformanceCounter attached to the runnable via its DocGroup
// or WorkerPrivate and call IncrementExecutionDuration()
//
// Notice that the execution duration counting takes into account
// recursivity. If an event triggers a recursive call to
// nsThread::ProcessNextEVent, the counter will discard the time
// spent in sub events.
class PerformanceCounter final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PerformanceCounter)

  explicit PerformanceCounter(const nsACString& aName);

  /**
   * This is called everytime a runnable is dispatched.
   *
   * aCategory can be used to distinguish counts per TaskCategory
   *
   * Note that an overflow will simply reset the counter.
   */
  void IncrementDispatchCounter(DispatchCategory aCategory);

  /**
   * This is called via nsThread::ProcessNextEvent to measure runnable
   * execution duration.
   *
   * Note that an overflow will simply reset the counter.
   */
  void IncrementExecutionDuration(uint32_t aMicroseconds);

  /**
   * Returns a category/counter array of all dispatches.
   */
  const DispatchCounter& GetDispatchCounter();

  /**
   * Returns the total execution duration.
   */
  uint64_t GetExecutionDuration();

  /**
   * Returns the number of dispatches per TaskCategory.
   */
  uint32_t GetDispatchCount(DispatchCategory aCategory);

  /**
   * Returns the total number of dispatches.
   */
  uint64_t GetTotalDispatchCount();

  /**
   * Returns the unique id for the instance.
   *
   * Used to distinguish instances since the lifespan of
   * a PerformanceCounter can be shorter than the
   * host it's tracking. That leads to edge cases
   * where a counter appears to have values that go
   * backwards. Having this id let the consumers
   * detect that they are dealing with a new counter
   * when it happens.
   */
  uint64_t GetID() const;

 private:
  ~PerformanceCounter() = default;

  Atomic<uint64_t> mExecutionDuration;
  Atomic<uint64_t> mTotalDispatchCount;
  DispatchCounter mDispatchCounter;
  nsCString mName;
  const uint64_t mID;
};

}  // namespace mozilla

#endif  // mozilla_PerformanceCounter_h
