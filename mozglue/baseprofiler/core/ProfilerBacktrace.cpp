/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerBacktrace.h"

#include "BaseProfiler.h"
#include "BaseProfileJSONWriter.h"
#include "ProfileBuffer.h"
#include "ProfiledThreadData.h"
#include "ThreadInfo.h"

namespace mozilla {
namespace baseprofiler {

ProfilerBacktrace::ProfilerBacktrace(
    const char* aName, int aThreadId,
    UniquePtr<ProfileChunkedBuffer> aProfileChunkedBuffer,
    UniquePtr<ProfileBuffer> aProfileBuffer)
    : mName(strdup(aName)),
      mThreadId(aThreadId),
      mProfileChunkedBuffer(std::move(aProfileChunkedBuffer)),
      mProfileBuffer(std::move(aProfileBuffer)) {
  MOZ_ASSERT(!!mProfileChunkedBuffer,
             "ProfilerBacktrace only takes a non-null "
             "UniquePtr<ProfileChunkedBuffer>");
  MOZ_ASSERT(
      !!mProfileBuffer,
      "ProfilerBacktrace only takes a non-null UniquePtr<ProfileBuffer>");
  MOZ_ASSERT(
      !mProfileChunkedBuffer->IsThreadSafe(),
      "ProfilerBacktrace only takes a non-thread-safe ProfileChunkedBuffer");
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

// static
template <typename Destructor>
UniquePtr<baseprofiler::ProfilerBacktrace, Destructor>
ProfileBufferEntryReader::
    Deserializer<UniquePtr<baseprofiler::ProfilerBacktrace, Destructor>>::Read(
        ProfileBufferEntryReader& aER) {
  auto profileChunkedBuffer = aER.ReadObject<UniquePtr<ProfileChunkedBuffer>>();
  if (!profileChunkedBuffer) {
    return nullptr;
  }
  MOZ_ASSERT(
      !profileChunkedBuffer->IsThreadSafe(),
      "ProfilerBacktrace only stores non-thread-safe ProfileChunkedBuffers");
  int threadId = aER.ReadObject<int>();
  std::string name = aER.ReadObject<std::string>();
  auto profileBuffer =
      MakeUnique<baseprofiler::ProfileBuffer>(*profileChunkedBuffer);
  return UniquePtr<baseprofiler::ProfilerBacktrace, Destructor>{
      new baseprofiler::ProfilerBacktrace(name.c_str(), threadId,
                                          std::move(profileChunkedBuffer),
                                          std::move(profileBuffer))};
};

}  // namespace mozilla
