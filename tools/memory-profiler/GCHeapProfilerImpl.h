/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef memory_profiler_GCHeapProfilerImpl_h
#define memory_profiler_GCHeapProfilerImpl_h

#include "CompactTraceTable.h"
#include "MemoryProfiler.h"

#include "jsfriendapi.h"

namespace mozilla {

class GCHeapProfilerImpl final : public GCHeapProfiler
                               , public ProfilerImpl
{
public:
  GCHeapProfilerImpl();
  ~GCHeapProfilerImpl() override;

  u_vector<u_string> GetNames() const override;
  u_vector<TrieNode> GetTraces() const override;
  const u_vector<AllocEvent>& GetEvents() const override;

  void reset() override;
  void sampleTenured(void* addr, uint32_t size) override;
  void sampleNursery(void* addr, uint32_t size) override;
  void markTenuredStart() override;
  void markTenured(void* addr) override;
  void sweepTenured() override;
  void sweepNursery() override;
  void moveNurseryToTenured(void* addrOld, void* addrNew) override;

private:
  void SampleInternal(void* addr, uint32_t size, AllocMap& table);

  PRLock* mLock;
  bool mMarking;

  AllocMap mNurseryEntries;
  AllocMap mTenuredEntriesFG;
  AllocMap mTenuredEntriesBG;

  u_vector<AllocEvent> mAllocEvents;
  CompactTraceTable mTraceTable;
};

} // namespace mozilla

#endif // memory_profiler_GCHeapProfilerImpl_h
