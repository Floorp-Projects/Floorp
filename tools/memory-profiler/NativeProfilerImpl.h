/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef memory_profiler_NativeProfilerImpl_h
#define memory_profiler_NativeProfilerImpl_h

#include "CompactTraceTable.h"
#include "MemoryProfiler.h"

#include "jsfriendapi.h"

struct PRLock;

namespace mozilla {

class NativeProfilerImpl final : public NativeProfiler
                               , public ProfilerImpl
{
public:
  NativeProfilerImpl();
  ~NativeProfilerImpl() override;

  u_vector<u_string> GetNames() const override;
  u_vector<TrieNode> GetTraces() const override;
  const u_vector<AllocEvent>& GetEvents() const override;

  void reset() override;
  void sampleNative(void* addr, uint32_t size) override;
  void removeNative(void* addr) override;

private:
  PRLock* mLock;
  AllocMap mNativeEntries;
  u_vector<AllocEvent> mAllocEvents;
  CompactTraceTable mTraceTable;
};

} // namespace mozilla

#endif // memory_profiler_NativeProfilerImpl_h
