/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_BackgroundHangTelemetry_h
#define mozilla_BackgroundHangTelemetry_h

#include "mozilla/Array.h"
#include "mozilla/Assertions.h"
#include "mozilla/HangAnnotations.h"
#include "mozilla/Move.h"
#include "mozilla/Mutex.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Vector.h"
#include "mozilla/CombinedStacks.h"

#include "nsString.h"
#include "prinrval.h"

namespace mozilla {
namespace Telemetry {

// This variable controls the maximum number of native hang stacks which may be
// attached to a ping. This is due to how large native stacks can be. We want to
// reduce the chance of a ping being discarded due to it exceeding the maximum
// ping size.
static const uint32_t kMaximumNativeHangStacks = 300;

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

/* A native stack is a simple list of pointers, so rather than building a
   wrapper type, we typdef the type here. */
typedef std::vector<uintptr_t> NativeHangStack;

/* HangStack stores an array of const char pointers,
   with optional internal storage for strings. */
class HangStack
{
public:
  static const size_t sMaxInlineStorage = 8;

  // The maximum depth for the native stack frames that we might collect.
  // XXX: Consider moving this to a different object?
  static const size_t sMaxNativeFrames = 150;

private:
  typedef mozilla::Vector<const char*, sMaxInlineStorage> Impl;
  Impl mImpl;

  // Stack entries can either be a static const char*
  // or a pointer to within this buffer.
  mozilla::Vector<char, 0> mBuffer;

public:
  HangStack() {}

  HangStack(HangStack&& aOther)
    : mImpl(mozilla::Move(aOther.mImpl))
    , mBuffer(mozilla::Move(aOther.mBuffer))
  {
  }

  HangStack& operator=(HangStack&& aOther) {
    mImpl = mozilla::Move(aOther.mImpl);
    mBuffer = mozilla::Move(aOther.mBuffer);
    return *this;
  }

  bool operator==(const HangStack& aOther) const {
    for (size_t i = 0; i < length(); i++) {
      if (!IsSameAsEntry(operator[](i), aOther[i])) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(const HangStack& aOther) const {
    return !operator==(aOther);
  }

  const char*& operator[](size_t aIndex) {
    return mImpl[aIndex];
  }

  const char* const& operator[](size_t aIndex) const {
    return mImpl[aIndex];
  }

  size_t capacity() const { return mImpl.capacity(); }
  size_t length() const { return mImpl.length(); }
  bool empty() const { return mImpl.empty(); }
  bool canAppendWithoutRealloc(size_t aNeeded) const {
    return mImpl.canAppendWithoutRealloc(aNeeded);
  }
  void infallibleAppend(const char* aEntry) { mImpl.infallibleAppend(aEntry); }
  bool reserve(size_t aRequest) { return mImpl.reserve(aRequest); }
  const char** begin() { return mImpl.begin(); }
  const char* const* begin() const { return mImpl.begin(); }
  const char** end() { return mImpl.end(); }
  const char* const* end() const { return mImpl.end(); }
  const char*& back() { return mImpl.back(); }
  void erase(const char** aEntry) { mImpl.erase(aEntry); }
  void erase(const char** aBegin, const char** aEnd) {
    mImpl.erase(aBegin, aEnd);
  }

  void clear() {
    mImpl.clear();
    mBuffer.clear();
  }

  bool IsInBuffer(const char* aEntry) const {
    return aEntry >= mBuffer.begin() && aEntry < mBuffer.end();
  }

  bool IsSameAsEntry(const char* aEntry, const char* aOther) const {
    // If the entry came from the buffer, we need to compare its content;
    // otherwise we only need to compare its pointer.
    return IsInBuffer(aEntry) ? !strcmp(aEntry, aOther) : (aEntry == aOther);
  }

  size_t AvailableBufferSize() const {
    return mBuffer.capacity() - mBuffer.length();
  }

  bool EnsureBufferCapacity(size_t aCapacity) {
    // aCapacity is the minimal capacity and Vector may make the actual
    // capacity larger, in which case we want to use up all the space.
    return mBuffer.reserve(aCapacity) &&
           mBuffer.reserve(mBuffer.capacity());
  }

  const char* InfallibleAppendViaBuffer(const char* aText, size_t aLength);
  const char* AppendViaBuffer(const char* aText, size_t aLength);
};

/* A hang histogram consists of a stack associated with the
   hang, along with a time histogram of the hang times. */
class HangHistogram : public TimeHistogram
{
public:
  // Value used for mNativeStackIndex to represent the absence of a cached
  // native stack.
  static const uint32_t NO_NATIVE_STACK_INDEX = UINT32_MAX;

private:
  static uint32_t GetHash(const HangStack& aStack);

  HangStack mStack;
  // Cached index of the native stack in the mCombinedStacks list in the owning
  // ThreadHangStats object. A default value of NO_NATIVE_STACK_INDEX means that
  // the ThreadHangStats object which owns this HangHistogram doesn't have a
  // cached CombinedStacks with this HangHistogram in it.
  uint32_t mNativeStackIndex;
  // Use a hash to speed comparisons
  const uint32_t mHash;
  // Annotations attributed to this stack
  HangMonitor::HangAnnotationsVector mAnnotations;

public:
  explicit HangHistogram(HangStack&& aStack)
    : mStack(mozilla::Move(aStack))
    , mNativeStackIndex(NO_NATIVE_STACK_INDEX)
    , mHash(GetHash(mStack))
  {
  }

  HangHistogram(HangHistogram&& aOther)
    : TimeHistogram(mozilla::Move(aOther))
    , mStack(mozilla::Move(aOther.mStack))
    , mNativeStackIndex(mozilla::Move(aOther.mNativeStackIndex))
    , mHash(mozilla::Move(aOther.mHash))
    , mAnnotations(mozilla::Move(aOther.mAnnotations))
  {
  }
  bool operator==(const HangHistogram& aOther) const;
  bool operator!=(const HangHistogram& aOther) const
  {
    return !operator==(aOther);
  }
  const HangStack& GetStack() const {
    return mStack;
  }
  uint32_t GetNativeStackIndex() const {
    return mNativeStackIndex;
  }
  void SetNativeStackIndex(uint32_t aIndex) {
    MOZ_ASSERT(aIndex != NO_NATIVE_STACK_INDEX);
    mNativeStackIndex = aIndex;
  }
  const HangMonitor::HangAnnotationsVector& GetAnnotations() const {
    return mAnnotations;
  }
  void Add(PRIntervalTime aTime, HangMonitor::HangAnnotationsPtr aAnnotations) {
    TimeHistogram::Add(aTime);
    if (aAnnotations) {
      if (!mAnnotations.append(Move(aAnnotations))) {
        MOZ_CRASH();
      }
    }
  }
};

/* Thread hang stats consist of
 - thread name
 - time histogram of all task run times
 - hang histograms of individual hangs
 - annotations for each hang
 - combined native stacks for all hangs
*/
class ThreadHangStats
{
private:
  nsCString mName;

public:
  TimeHistogram mActivity;
  mozilla::Vector<HangHistogram, 4> mHangs;
  uint32_t mNativeStackCnt;
  CombinedStacks mCombinedStacks;

  explicit ThreadHangStats(const char* aName)
    : mName(aName)
    , mNativeStackCnt(0)
    , mCombinedStacks(Telemetry::kMaximumNativeHangStacks)
  {
  }
  ThreadHangStats(ThreadHangStats&& aOther)
    : mName(mozilla::Move(aOther.mName))
    , mActivity(mozilla::Move(aOther.mActivity))
    , mHangs(mozilla::Move(aOther.mHangs))
    , mNativeStackCnt(aOther.mNativeStackCnt)
    , mCombinedStacks(mozilla::Move(aOther.mCombinedStacks))
  {
    aOther.mNativeStackCnt = 0;
  }
  const char* GetName() const {
    return mName.get();
  }
};

} // namespace Telemetry
} // namespace mozilla

#endif // mozilla_BackgroundHangTelemetry_h
