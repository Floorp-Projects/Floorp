/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerBacktrace.h"

#include "ProfileBuffer.h"
#include "ProfiledThreadData.h"

#include "mozilla/ProfileJSONWriter.h"

ProfilerBacktrace::ProfilerBacktrace(
    const char* aName,
    mozilla::UniquePtr<mozilla::ProfileChunkedBuffer>
        aProfileChunkedBufferStorage,
    mozilla::UniquePtr<ProfileBuffer>
        aProfileBufferStorageOrNull /* = nullptr */)
    : mName(aName),
      mOptionalProfileChunkedBufferStorage(
          std::move(aProfileChunkedBufferStorage)),
      mProfileChunkedBuffer(mOptionalProfileChunkedBufferStorage.get()),
      mOptionalProfileBufferStorage(std::move(aProfileBufferStorageOrNull)),
      mProfileBuffer(mOptionalProfileBufferStorage.get()) {
  MOZ_COUNT_CTOR(ProfilerBacktrace);
  if (mProfileBuffer) {
    MOZ_RELEASE_ASSERT(mProfileChunkedBuffer,
                       "If we take ownership of a ProfileBuffer, we must also "
                       "receive ownership of a ProfileChunkedBuffer");
    MOZ_RELEASE_ASSERT(
        mProfileChunkedBuffer == &mProfileBuffer->UnderlyingChunkedBuffer(),
        "If we take ownership of a ProfileBuffer, we must also receive "
        "ownership of its ProfileChunkedBuffer");
  }
  MOZ_ASSERT(
      !mProfileChunkedBuffer || !mProfileChunkedBuffer->IsThreadSafe(),
      "ProfilerBacktrace only takes a non-thread-safe ProfileChunkedBuffer");
}

ProfilerBacktrace::ProfilerBacktrace(
    const char* aName,
    mozilla::ProfileChunkedBuffer* aExternalProfileChunkedBuffer,
    ProfileBuffer* aExternalProfileBuffer)
    : mName(aName),
      mProfileChunkedBuffer(aExternalProfileChunkedBuffer),
      mProfileBuffer(aExternalProfileBuffer) {
  MOZ_COUNT_CTOR(ProfilerBacktrace);
  if (!mProfileChunkedBuffer) {
    if (mProfileBuffer) {
      // We don't have a ProfileChunkedBuffer but we have a ProfileBuffer, use
      // the latter's ProfileChunkedBuffer.
      mProfileChunkedBuffer = &mProfileBuffer->UnderlyingChunkedBuffer();
      MOZ_ASSERT(!mProfileChunkedBuffer->IsThreadSafe(),
                 "ProfilerBacktrace only takes a non-thread-safe "
                 "ProfileChunkedBuffer");
    }
  } else {
    if (mProfileBuffer) {
      MOZ_RELEASE_ASSERT(
          mProfileChunkedBuffer == &mProfileBuffer->UnderlyingChunkedBuffer(),
          "If we reference both ProfileChunkedBuffer and ProfileBuffer, they "
          "must already be connected");
    }
    MOZ_ASSERT(!mProfileChunkedBuffer->IsThreadSafe(),
               "ProfilerBacktrace only takes a non-thread-safe "
               "ProfileChunkedBuffer");
  }
}

ProfilerBacktrace::~ProfilerBacktrace() { MOZ_COUNT_DTOR(ProfilerBacktrace); }

ProfilerThreadId ProfilerBacktrace::StreamJSON(
    SpliceableJSONWriter& aWriter, const mozilla::TimeStamp& aProcessStartTime,
    UniqueStacks& aUniqueStacks) {
  ProfilerThreadId processedThreadId;

  // Unlike ProfiledThreadData::StreamJSON, we don't need to call
  // ProfileBuffer::AddJITInfoForRange because ProfileBuffer does not contain
  // any JitReturnAddr entries. For synchronous samples, JIT frames get expanded
  // at sample time.
  if (mProfileBuffer) {
    processedThreadId = StreamSamplesAndMarkers(
        mName.c_str(), ProfilerThreadId{}, *mProfileBuffer, aWriter, ""_ns,
        ""_ns, aProcessStartTime,
        /* aRegisterTime */ mozilla::TimeStamp(),
        /* aUnregisterTime */ mozilla::TimeStamp(),
        /* aSinceTime */ 0, aUniqueStacks);
  } else if (mProfileChunkedBuffer) {
    ProfileBuffer profileBuffer(*mProfileChunkedBuffer);
    processedThreadId = StreamSamplesAndMarkers(
        mName.c_str(), ProfilerThreadId{}, profileBuffer, aWriter, ""_ns, ""_ns,
        aProcessStartTime,
        /* aRegisterTime */ mozilla::TimeStamp(),
        /* aUnregisterTime */ mozilla::TimeStamp(),
        /* aSinceTime */ 0, aUniqueStacks);
  }
  // If there are no buffers, the backtrace is empty and nothing is streamed.

  return processedThreadId;
}
