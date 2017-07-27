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
  , mBuffer(Move(aBuffer))
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
  // This call to StreamSamplesAndMarkers() can safely pass in a non-null
  // JSContext. That's because StreamSamplesAndMarkers() only accesses the
  // JSContext when streaming JitReturnAddress entries, and such entries
  // never appear in synchronous samples.
  double firstSampleTimeIgnored;
  StreamSamplesAndMarkers(mName.get(), mThreadId,
                          *mBuffer.get(), aWriter, aProcessStartTime,
                          /* aSinceTime */ 0, &firstSampleTimeIgnored,
                          /* aContext */ nullptr,
                          /* aSavedStreamedSamples */ nullptr,
                          /* aFirstSavedStreamedSampleTime */ 0.0,
                          /* aSavedStreamedMarkers */ nullptr,
                          aUniqueStacks);
}
