/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PROFILER_BACKTRACE_H
#define __PROFILER_BACKTRACE_H

#include "mozilla/UniquePtrExtensions.h"

class ProfileBuffer;
class SpliceableJSONWriter;
class ThreadInfo;
class UniqueStacks;

namespace mozilla {
class TimeStamp;
}

// ProfilerBacktrace encapsulates a synchronous sample.
class ProfilerBacktrace
{
public:
  ProfilerBacktrace(const char* aName, int aThreadId,
                    mozilla::UniquePtr<ProfileBuffer> aBuffer);
  ~ProfilerBacktrace();

  // ProfilerBacktraces' stacks are deduplicated in the context of the
  // profile that contains the backtrace as a marker payload.
  //
  // That is, markers that contain backtraces should not need their own stack,
  // frame, and string tables. They should instead reuse their parent
  // profile's tables.
  void StreamJSON(SpliceableJSONWriter& aWriter,
                  const mozilla::TimeStamp& aProcessStartTime,
                  UniqueStacks& aUniqueStacks);

private:
  ProfilerBacktrace(const ProfilerBacktrace&);
  ProfilerBacktrace& operator=(const ProfilerBacktrace&);

  mozilla::UniqueFreePtr<char> mName;
  int mThreadId;
  mozilla::UniquePtr<ProfileBuffer> mBuffer;
};

#endif // __PROFILER_BACKTRACE_H

