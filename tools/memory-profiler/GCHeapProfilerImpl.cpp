/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GCHeapProfilerImpl.h"

#include "mozilla/TimeStamp.h"

#include "prlock.h"

namespace mozilla {

GCHeapProfilerImpl::GCHeapProfilerImpl()
{
  mLock = PR_NewLock();
  mMarking = false;
}

GCHeapProfilerImpl::~GCHeapProfilerImpl()
{
  if (mLock) {
    PR_DestroyLock(mLock);
  }
}

u_vector<u_string>
GCHeapProfilerImpl::GetNames() const
{
  return mTraceTable.GetNames();
}

u_vector<TrieNode>
GCHeapProfilerImpl::GetTraces() const
{
  return mTraceTable.GetTraces();
}

const u_vector<AllocEvent>&
GCHeapProfilerImpl::GetEvents() const
{
  return mAllocEvents;
}

void
GCHeapProfilerImpl::reset()
{
  mTraceTable.Reset();
  mAllocEvents.clear();
  mNurseryEntries.clear();
  mTenuredEntriesFG.clear();
  mTenuredEntriesBG.clear();
}

void
GCHeapProfilerImpl::sampleTenured(void* addr, uint32_t size)
{
  SampleInternal(addr, size, mTenuredEntriesFG);
}

void
GCHeapProfilerImpl::sampleNursery(void* addr, uint32_t size)
{
  SampleInternal(addr, size, mNurseryEntries);
}

void
GCHeapProfilerImpl::markTenuredStart()
{
  AutoMPLock lock(mLock);
  if (!mMarking) {
    mMarking = true;
    Swap(mTenuredEntriesFG, mTenuredEntriesBG);
    MOZ_ASSERT(mTenuredEntriesFG.empty());
  }
}

void
GCHeapProfilerImpl::markTenured(void* addr)
{
  AutoMPLock lock(mLock);
  if (mMarking) {
    auto res = mTenuredEntriesBG.find(addr);
    if (res != mTenuredEntriesBG.end()) {
      res->second.mMarked = true;
    }
  }
}

void
GCHeapProfilerImpl::sweepTenured()
{
  AutoMPLock lock(mLock);
  if (mMarking) {
    mMarking = false;
    for (auto& entry: mTenuredEntriesBG) {
      if (entry.second.mMarked) {
        entry.second.mMarked = false;
        mTenuredEntriesFG.insert(entry);
      } else {
        AllocEvent& oldEvent = mAllocEvents[entry.second.mEventIdx];
        AllocEvent newEvent(oldEvent.mTraceIdx, -oldEvent.mSize, TimeStamp::Now());
        mAllocEvents.push_back(newEvent);
      }
    }
    mTenuredEntriesBG.clear();
  }
}

void
GCHeapProfilerImpl::sweepNursery()
{
  AutoMPLock lock(mLock);
  for (auto& entry: mNurseryEntries) {
    AllocEvent& oldEvent = mAllocEvents[entry.second.mEventIdx];
    AllocEvent newEvent(oldEvent.mTraceIdx, -oldEvent.mSize, TimeStamp::Now());
    mAllocEvents.push_back(newEvent);
  }
  mNurseryEntries.clear();
}

void
GCHeapProfilerImpl::moveNurseryToTenured(void* addrOld, void* addrNew)
{
  AutoMPLock lock(mLock);
  auto iterOld = mNurseryEntries.find(addrOld);
  if (iterOld == mNurseryEntries.end()) {
    return;
  }

  // Because the tenured heap is sampled, the address might already be there.
  // If not, the address is inserted with the old event.
  auto res = mTenuredEntriesFG.insert(
    std::make_pair(addrNew, AllocEntry(iterOld->second.mEventIdx)));
  auto iterNew = res.first;

  // If it is already inserted, the insertion above will fail and the
  // iterator of the already-inserted element is returned.
  // We choose to ignore the the new event by setting its size zero and point
  // the newly allocated address to the old event.
  // An event of size zero will be skipped when reporting.
  if (!res.second) {
    mAllocEvents[iterNew->second.mEventIdx].mSize = 0;
    iterNew->second.mEventIdx = iterOld->second.mEventIdx;
  }
  mNurseryEntries.erase(iterOld);
}

void
GCHeapProfilerImpl::SampleInternal(void* aAddr, uint32_t aSize, AllocMap& aTable)
{
  AutoMPLock lock(mLock);
  size_t nSamples = AddBytesSampled(aSize);
  if (nSamples > 0) {
    u_vector<u_string> trace = GetStacktrace();
    AllocEvent ai(mTraceTable.Insert(trace), nSamples * mSampleSize, TimeStamp::Now());
    aTable.insert(std::make_pair(aAddr, AllocEntry(mAllocEvents.size())));
    mAllocEvents.push_back(ai);
  }
}

} // namespace mozilla
