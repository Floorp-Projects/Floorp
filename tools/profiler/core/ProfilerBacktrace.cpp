/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerBacktrace.h"

#include "ProfileJSONWriter.h"
#include "ThreadInfo.h"

ProfilerBacktrace::ProfilerBacktrace(const char* aName, int aThreadId,
                                     UniquePtr<ProfileBuffer> aBuffer)
  : mName(strdup(aName))
  , mThreadId(aThreadId)
  , mBuffer(std::move(aBuffer))
{
  MOZ_COUNT_CTOR(ProfilerBacktrace);
}

ProfilerBacktrace::~ProfilerBacktrace()
{
  MOZ_COUNT_DTOR(ProfilerBacktrace);
}

void
ProfilerBacktrace::StreamJSON(SpliceableJSONWriter& aWriter,
                              const TimeStamp& aProcessStartTime,
                              UniqueStacks& aUniqueStacks)
{
  // Unlike ProfiledThreadData::StreamJSON, we don't need to call
  // ProfileBuffer::AddJITInfoForRange because mBuffer does not contain any
  // JitReturnAddr entries. For synchronous samples, JIT frames get expanded
  // at sample time.
  StreamSamplesAndMarkers(mName.get(), mThreadId,
                          *mBuffer.get(), aWriter, aProcessStartTime,
                          /* aRegisterTime */ TimeStamp(),
                          /* aUnregisterTime */ TimeStamp(),
                          /* aSinceTime */ 0,
                          aUniqueStacks);
}
