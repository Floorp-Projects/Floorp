/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BackgroundHangTelemetry_h
#define mozilla_BackgroundHangTelemetry_h

#include "mozilla/Array.h"
#include "mozilla/Assertions.h"
#include "mozilla/Move.h"
#include "mozilla/Mutex.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Vector.h"

#include "nsString.h"
#include "prinrval.h"

namespace mozilla {
namespace Telemetry {

static const size_t kTimeHistogramBuckets = 8 * sizeof(PRIntervalTime);

/* TimeHistogram is an efficient histogram that puts time durations into
   exponential (base 2) buckets; times are accepted in PRIntervalTime and
   stored in milliseconds. */
class TimeHistogram : public mozilla::Array<uint32_t, kTimeHistogramBuckets>
{
public:
  TimeHistogram()
  {
    mozilla::PodArrayZero(*this);
  }
  // Get minimum (inclusive) range of bucket in milliseconds
  uint32_t GetBucketMin(size_t aBucket) const {
    MOZ_ASSERT(aBucket < ArrayLength(*this));
    return (1u << aBucket) & ~1u; // Bucket 0 starts at 0, not 1
  }
  // Get maximum (inclusive) range of bucket in milliseconds
  uint32_t GetBucketMax(size_t aBucket) const {
    MOZ_ASSERT(aBucket < ArrayLength(*this));
    return (1u << (aBucket + 1u)) - 1u;
  }
  void Add(PRIntervalTime aTime);
};

/* A hang histogram consists of a stack associated with the
   hang, along with a time histogram of the hang times. */
class HangHistogram : public TimeHistogram
{
public:
  typedef mozilla::Vector<const char*, 8> Stack;

private:
  static uint32_t GetHash(const Stack& aStack);

  Stack mStack;
  // Use a hash to speed comparisons
  const uint32_t mHash;

public:
  explicit HangHistogram(Stack&& aStack)
    : mStack(mozilla::Move(aStack))
    , mHash(GetHash(mStack))
  {
  }
  HangHistogram(HangHistogram&& aOther)
    : TimeHistogram(mozilla::Move(aOther))
    , mStack(mozilla::Move(aOther.mStack))
    , mHash(mozilla::Move(aOther.mHash))
  {
  }
  bool operator==(const HangHistogram& aOther) const;
  bool operator!=(const HangHistogram& aOther) const
  {
    return !operator==(aOther);
  }
  const Stack& GetStack() const {
    return mStack;
  }
};

/* Thread hang stats consist of
 - thread name
 - time histogram of all task run times
 - hang histograms of individual hangs. */
class ThreadHangStats
{
private:
  nsAutoCString mName;

public:
  TimeHistogram mActivity;
  mozilla::Vector<HangHistogram, 4> mHangs;

  explicit ThreadHangStats(const char* aName)
    : mName(aName)
  {
  }
  ThreadHangStats(ThreadHangStats&& aOther)
    : mName(mozilla::Move(aOther.mName))
    , mActivity(mozilla::Move(aOther.mActivity))
    , mHangs(mozilla::Move(aOther.mHangs))
  {
  }
  const char* GetName() const {
    return mName.get();
  }
};

} // namespace Telemetry
} // namespace mozilla
#endif // mozilla_BackgroundHangTelemetry_h
