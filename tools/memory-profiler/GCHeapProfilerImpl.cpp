/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GCHeapProfilerImpl.h"

#include "UncensoredAllocator.h"

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

nsTArray<nsCString>
GCHeapProfilerImpl::GetNames() const
{
  return mTraceTable.GetNames();
}

nsTArray<TrieNode>
GCHeapProfilerImpl::GetTraces() const
{
  return mTraceTable.GetTraces();
}

const nsTArray<AllocEvent>&
GCHeapProfilerImpl::GetEvents() const
{
  return mAllocEvents;
}

void
GCHeapProfilerImpl::reset()
{
  mTraceTable.Reset();
  mAllocEvents.Clear();
  mNurseryEntries.Clear();
  mTenuredEntriesFG.Clear();
  mTenuredEntriesBG.Clear();
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
  AutoUseUncensoredAllocator ua;
  AutoMPLock lock(mLock);
  if (!mMarking) {
    mMarking = true;
    mTenuredEntriesFG.SwapElements(mTenuredEntriesBG);
    MOZ_ASSERT(mTenuredEntriesFG.Count() == 0);
  }
}

void
GCHeapProfilerImpl::markTenured(void* addr)
{
  AutoUseUncensoredAllocator ua;
  AutoMPLock lock(mLock);
  if (mMarking) {
    AllocEntry entry;
    if (mTenuredEntriesBG.Get(addr, &entry)) {
      entry.mMarked = true;
      mTenuredEntriesBG.Put(addr, entry);
    }
  }
}

void
GCHeapProfilerImpl::sweepTenured()
{
  AutoUseUncensoredAllocator ua;
  AutoMPLock lock(mLock);
  if (mMarking) {
    mMarking = false;
    for (auto iter = mTenuredEntriesBG.Iter(); !iter.Done(); iter.Next()) {
      if (iter.Data().mMarked) {
        iter.Data().mMarked = false;
        mTenuredEntriesFG.Put(iter.Key(), iter.Data());
      } else {
        AllocEvent& oldEvent = mAllocEvents[iter.Data().mEventIdx];
        AllocEvent newEvent(oldEvent.mTraceIdx, -oldEvent.mSize, TimeStamp::Now());
        mAllocEvents.AppendElement(newEvent);
      }
    }
    mTenuredEntriesBG.Clear();
  }
}

void
GCHeapProfilerImpl::sweepNursery()
{
  AutoUseUncensoredAllocator ua;
  AutoMPLock lock(mLock);
  for (auto iter = mNurseryEntries.Iter(); !iter.Done(); iter.Next()) {
    AllocEvent& oldEvent = mAllocEvents[iter.Data().mEventIdx];
    AllocEvent newEvent(oldEvent.mTraceIdx, -oldEvent.mSize, TimeStamp::Now());
    mAllocEvents.AppendElement(newEvent);
  }
  mNurseryEntries.Clear();
}

void
GCHeapProfilerImpl::moveNurseryToTenured(void* addrOld, void* addrNew)
{
  AutoUseUncensoredAllocator ua;
  AutoMPLock lock(mLock);
  AllocEntry entryOld;
  if (!mNurseryEntries.Get(addrOld, &entryOld)) {
    return;
  }

  // Because the tenured heap is sampled, the address might already be there.
  // If not, the address is inserted with the old event.
  AllocEntry tenuredEntryOld;
  if (!mTenuredEntriesFG.Get(addrNew, &tenuredEntryOld)) {
    mTenuredEntriesFG.Put(addrNew, AllocEntry(entryOld.mEventIdx));
  } else {
    // If it is already inserted, the insertion above will fail and the
    // iterator of the already-inserted element is returned.
    // We choose to ignore the the new event by setting its size zero and point
    // the newly allocated address to the old event.
    // An event of size zero will be skipped when reporting.
    mAllocEvents[entryOld.mEventIdx].mSize = 0;
    tenuredEntryOld.mEventIdx = entryOld.mEventIdx;
    mTenuredEntriesFG.Put(addrNew, tenuredEntryOld);
  }
  mNurseryEntries.Remove(addrOld);
}

void
GCHeapProfilerImpl::SampleInternal(void* aAddr, uint32_t aSize, AllocMap& aTable)
{
  AutoUseUncensoredAllocator ua;
  AutoMPLock lock(mLock);
  size_t nSamples = AddBytesSampled(aSize);
  if (nSamples > 0) {
    nsTArray<nsCString> trace = GetStacktrace();
    AllocEvent ai(mTraceTable.Insert(trace), nSamples * mSampleSize, TimeStamp::Now());
    aTable.Put(aAddr, AllocEntry(mAllocEvents.Length()));
    mAllocEvents.AppendElement(ai);
  }
}

} // namespace mozilla
