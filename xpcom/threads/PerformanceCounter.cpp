/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"
#include "mozilla/PerformanceCounter.h"

using mozilla::DispatchCategory;
using mozilla::DispatchCounter;
using mozilla::PerformanceCounter;

static mozilla::LazyLogModule sPerformanceCounter("PerformanceCounter");
#ifdef LOG
#  undef LOG
#endif
#define LOG(args) MOZ_LOG(sPerformanceCounter, mozilla::LogLevel::Debug, args)

// Global counter used by PerformanceCounter CTOR via NextCounterID().
static mozilla::Atomic<uint64_t> gNextCounterID(0);

static uint64_t NextCounterID() {
  // This can return the same value on different processes but
  // we're fine with this behavior because consumers can use a (pid, counter_id)
  // tuple to make instances globally unique in a browser session.
  return ++gNextCounterID;
}

// this instance is the extension for the worker
const DispatchCategory DispatchCategory::Worker =
    DispatchCategory((uint32_t)TaskCategory::Count);

PerformanceCounter::PerformanceCounter(const nsACString& aName)
    : mExecutionDuration(0),
      mTotalDispatchCount(0),
      mDispatchCounter(),
      mName(aName),
      mID(NextCounterID()) {
  LOG(("PerformanceCounter created with ID %" PRIu64, mID));
}

void PerformanceCounter::IncrementDispatchCounter(DispatchCategory aCategory) {
  mDispatchCounter[aCategory.GetValue()] += 1;
  mTotalDispatchCount += 1;
  LOG(("[%s][%" PRIu64 "] Total dispatch %" PRIu64, mName.get(), GetID(),
       uint64_t(mTotalDispatchCount)));
}

void PerformanceCounter::IncrementExecutionDuration(uint32_t aMicroseconds) {
  mExecutionDuration += aMicroseconds;
  LOG(("[%s][%" PRIu64 "] Total duration %" PRIu64, mName.get(), GetID(),
       uint64_t(mExecutionDuration)));
}

const DispatchCounter& PerformanceCounter::GetDispatchCounter() const {
  return mDispatchCounter;
}

uint64_t PerformanceCounter::GetExecutionDuration() const {
  return mExecutionDuration;
}

uint64_t PerformanceCounter::GetTotalDispatchCount() const {
  return mTotalDispatchCount;
}

uint32_t PerformanceCounter::GetDispatchCount(
    DispatchCategory aCategory) const {
  return mDispatchCounter[aCategory.GetValue()];
}

uint64_t PerformanceCounter::GetID() const { return mID; }
