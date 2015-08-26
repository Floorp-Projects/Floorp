/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeProfilerImpl.h"

#include "UncensoredAllocator.h"

namespace mozilla {

NativeProfilerImpl::NativeProfilerImpl()
{
  mLock = PR_NewLock();
}

NativeProfilerImpl::~NativeProfilerImpl()
{
  if (mLock) {
    PR_DestroyLock(mLock);
  }
}

nsTArray<nsCString>
NativeProfilerImpl::GetNames() const
{
  return mTraceTable.GetNames();
}

nsTArray<TrieNode>
NativeProfilerImpl::GetTraces() const
{
  return mTraceTable.GetTraces();
}

const nsTArray<AllocEvent>&
NativeProfilerImpl::GetEvents() const
{
  return mAllocEvents;
}

void
NativeProfilerImpl::reset()
{
  mTraceTable.Reset();
  mAllocEvents.Clear();
  mNativeEntries.Clear();
}

void
NativeProfilerImpl::sampleNative(void* addr, uint32_t size)
{
  AutoUseUncensoredAllocator ua;
  AutoMPLock lock(mLock);
  size_t nSamples = AddBytesSampled(size);
  if (nSamples > 0) {
    nsTArray<nsCString> trace = GetStacktrace();
    AllocEvent ai(mTraceTable.Insert(trace), nSamples * mSampleSize, TimeStamp::Now());
    mNativeEntries.Put(addr, AllocEntry(mAllocEvents.Length()));
    mAllocEvents.AppendElement(ai);
  }
}

void
NativeProfilerImpl::removeNative(void* addr)
{
  AutoUseUncensoredAllocator ua;
  AutoMPLock lock(mLock);

  AllocEntry entry;
  if (!mNativeEntries.Get(addr, &entry)) {
    return;
  }

  AllocEvent& oldEvent = mAllocEvents[entry.mEventIdx];
  AllocEvent newEvent(oldEvent.mTraceIdx, -oldEvent.mSize, TimeStamp::Now());
  mAllocEvents.AppendElement(newEvent);
  mNativeEntries.Remove(addr);
}

} // namespace mozilla
