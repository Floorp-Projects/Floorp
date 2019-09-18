/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseProfiler.h"

#ifdef MOZ_BASE_PROFILER

#  include "ProfilerBacktrace.h"

#  include "ProfileBuffer.h"
#  include "ProfiledThreadData.h"
#  include "BaseProfileJSONWriter.h"
#  include "ThreadInfo.h"

namespace mozilla {
namespace baseprofiler {

ProfilerBacktrace::ProfilerBacktrace(
    const char* aName, int aThreadId,
    UniquePtr<BlocksRingBuffer> aBlocksRingBuffer,
    UniquePtr<ProfileBuffer> aProfileBuffer)
    : mName(strdup(aName)),
      mThreadId(aThreadId),
      mBlocksRingBuffer(std::move(aBlocksRingBuffer)),
      mProfileBuffer(std::move(aProfileBuffer)) {
  MOZ_ASSERT(
      !!mBlocksRingBuffer,
      "ProfilerBacktrace only takes a non-null UniquePtr<BlocksRingBuffer>");
  MOZ_ASSERT(
      !!mProfileBuffer,
      "ProfilerBacktrace only takes a non-null UniquePtr<ProfileBuffer>");
}

ProfilerBacktrace::~ProfilerBacktrace() {}

void ProfilerBacktrace::StreamJSON(SpliceableJSONWriter& aWriter,
                                   const TimeStamp& aProcessStartTime,
                                   UniqueStacks& aUniqueStacks) {
  // Unlike ProfiledThreadData::StreamJSON, we don't need to call
  // ProfileBuffer::AddJITInfoForRange because mProfileBuffer does not contain
  // any JitReturnAddr entries. For synchronous samples, JIT frames get expanded
  // at sample time.
  StreamSamplesAndMarkers(mName.get(), mThreadId, *mProfileBuffer, aWriter, "",
                          aProcessStartTime,
                          /* aRegisterTime */ TimeStamp(),
                          /* aUnregisterTime */ TimeStamp(),
                          /* aSinceTime */ 0, aUniqueStacks);
}

}  // namespace baseprofiler
}  // namespace mozilla

#endif  // MOZ_BASE_PROFILER
