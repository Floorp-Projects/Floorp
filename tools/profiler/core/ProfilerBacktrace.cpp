/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProfilerBacktrace.h"

#include "ProfileJSONWriter.h"
#include "ThreadInfo.h"

ProfilerBacktrace::ProfilerBacktrace(ProfileBuffer* aBuffer,
                                     ThreadInfo* aThreadInfo)
  : mBuffer(aBuffer)
  , mThreadInfo(aThreadInfo)
{
  MOZ_COUNT_CTOR(ProfilerBacktrace);
  MOZ_ASSERT(aThreadInfo && aThreadInfo->HasProfile());
}

ProfilerBacktrace::~ProfilerBacktrace()
{
  MOZ_COUNT_DTOR(ProfilerBacktrace);
  delete mBuffer;
  delete mThreadInfo;
}

void
ProfilerBacktrace::StreamJSON(SpliceableJSONWriter& aWriter,
                              const TimeStamp& aStartTime,
                              UniqueStacks& aUniqueStacks)
{
  mozilla::MutexAutoLock lock(mThreadInfo->GetMutex());
  mThreadInfo->StreamSamplesAndMarkers(mBuffer, aWriter, aStartTime,
                                       /* aSinceTime */ 0, aUniqueStacks);
}
