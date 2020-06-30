/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerBacktrace.h"

#include "ProfileBuffer.h"
#include "ProfiledThreadData.h"
#include "ProfileJSONWriter.h"
#include "ThreadInfo.h"

ProfilerBacktrace::ProfilerBacktrace(
    const char* aName, int aThreadId,
    UniquePtr<mozilla::ProfileChunkedBuffer> aProfileChunkedBuffer,
    mozilla::UniquePtr<ProfileBuffer> aProfileBuffer)
    : mName(strdup(aName)),
      mThreadId(aThreadId),
      mProfileChunkedBuffer(std::move(aProfileChunkedBuffer)),
      mProfileBuffer(std::move(aProfileBuffer)) {
  MOZ_COUNT_CTOR(ProfilerBacktrace);
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

ProfilerBacktrace::~ProfilerBacktrace() { MOZ_COUNT_DTOR(ProfilerBacktrace); }

void ProfilerBacktrace::StreamJSON(SpliceableJSONWriter& aWriter,
                                   const mozilla::TimeStamp& aProcessStartTime,
                                   UniqueStacks& aUniqueStacks) {
  // Unlike ProfiledThreadData::StreamJSON, we don't need to call
  // ProfileBuffer::AddJITInfoForRange because mProfileBuffer does not contain
  // any JitReturnAddr entries. For synchronous samples, JIT frames get expanded
  // at sample time.
  StreamSamplesAndMarkers(mName.get(), mThreadId, *mProfileBuffer, aWriter,
                          NS_LITERAL_CSTRING(""), aProcessStartTime,
                          /* aRegisterTime */ mozilla::TimeStamp(),
                          /* aUnregisterTime */ mozilla::TimeStamp(),
                          /* aSinceTime */ 0, aUniqueStacks);
}
