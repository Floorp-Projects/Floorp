/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NativeProfilerImpl.h"

#include "mozilla/TimeStamp.h"

#include "prlock.h"

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

u_vector<u_string>
NativeProfilerImpl::GetNames() const
{
  return mTraceTable.GetNames();
}

u_vector<TrieNode>
NativeProfilerImpl::GetTraces() const
{
  return mTraceTable.GetTraces();
}

const u_vector<AllocEvent>&
NativeProfilerImpl::GetEvents() const
{
  return mAllocEvents;
}

void
NativeProfilerImpl::reset()
{
  mTraceTable.Reset();
  mAllocEvents.clear();
  mNativeEntries.clear();
}

void
NativeProfilerImpl::sampleNative(void* addr, uint32_t size)
{
  AutoMPLock lock(mLock);
  size_t nSamples = AddBytesSampled(size);
  if (nSamples > 0) {
    u_vector<u_string> trace = GetStacktrace();
    AllocEvent ai(mTraceTable.Insert(trace), nSamples * mSampleSize, TimeStamp::Now());
    mNativeEntries.insert(std::make_pair(addr, AllocEntry(mAllocEvents.size())));
    mAllocEvents.push_back(ai);
  }
}

void
NativeProfilerImpl::removeNative(void* addr)
{
  AutoMPLock lock(mLock);

  auto res = mNativeEntries.find(addr);
  if (res == mNativeEntries.end()) {
    return;
  }

  AllocEvent& oldEvent = mAllocEvents[res->second.mEventIdx];
  AllocEvent newEvent(oldEvent.mTraceIdx, -oldEvent.mSize, TimeStamp::Now());
  mAllocEvents.push_back(newEvent);
  mNativeEntries.erase(res);
}

} // namespace mozilla
