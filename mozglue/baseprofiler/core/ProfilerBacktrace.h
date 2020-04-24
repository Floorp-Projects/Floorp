/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PROFILER_BACKTRACE_H
#define __PROFILER_BACKTRACE_H

#include "mozilla/UniquePtrExtensions.h"

namespace mozilla {

class ProfileChunkedBuffer;
class TimeStamp;

namespace baseprofiler {

class ProfileBuffer;
class SpliceableJSONWriter;
class ThreadInfo;
class UniqueStacks;

// ProfilerBacktrace encapsulates a synchronous sample.
class ProfilerBacktrace {
 public:
  ProfilerBacktrace(const char* aName, int aThreadId,
                    UniquePtr<ProfileChunkedBuffer> aProfileChunkedBuffer,
                    UniquePtr<ProfileBuffer> aProfileBuffer);
  ~ProfilerBacktrace();

  // ProfilerBacktraces' stacks are deduplicated in the context of the
  // profile that contains the backtrace as a marker payload.
  //
  // That is, markers that contain backtraces should not need their own stack,
  // frame, and string tables. They should instead reuse their parent
  // profile's tables.
  void StreamJSON(SpliceableJSONWriter& aWriter,
                  const TimeStamp& aProcessStartTime,
                  UniqueStacks& aUniqueStacks);

 private:
  // Used to de/serialize a ProfilerBacktrace.
  friend struct ProfileBufferEntryWriter::Serializer<ProfilerBacktrace>;
  friend struct ProfileBufferEntryReader::Deserializer<ProfilerBacktrace>;

  UniqueFreePtr<char> mName;
  int mThreadId;
  // `ProfileChunkedBuffer` in which `mProfileBuffer` stores its data; must be
  // located before `mProfileBuffer` so that it's destroyed after.
  UniquePtr<ProfileChunkedBuffer> mProfileChunkedBuffer;
  UniquePtr<ProfileBuffer> mProfileBuffer;
};

}  // namespace baseprofiler

// Format: [ UniquePtr<BlockRingsBuffer> | threadId | name ]
// Initial len==0 marks a nullptr or empty backtrace.
template <>
struct ProfileBufferEntryWriter::Serializer<baseprofiler::ProfilerBacktrace> {
  static Length Bytes(const baseprofiler::ProfilerBacktrace& aBacktrace) {
    if (!aBacktrace.mProfileBuffer) {
      // No backtrace buffer.
      return ULEB128Size<Length>(0);
    }
    auto bufferBytes = SumBytes(*aBacktrace.mProfileChunkedBuffer);
    if (bufferBytes == 0) {
      // Empty backtrace buffer.
      return ULEB128Size<Length>(0);
    }
    return bufferBytes +
           SumBytes(aBacktrace.mThreadId,
                    WrapProfileBufferUnownedCString(aBacktrace.mName.get()));
  }

  static void Write(ProfileBufferEntryWriter& aEW,
                    const baseprofiler::ProfilerBacktrace& aBacktrace) {
    if (!aBacktrace.mProfileBuffer ||
        SumBytes(aBacktrace.mProfileChunkedBuffer) == 0) {
      // No backtrace buffer, or it is empty.
      aEW.WriteULEB128<Length>(0);
      return;
    }
    aEW.WriteObject(aBacktrace.mProfileChunkedBuffer);
    aEW.WriteObject(aBacktrace.mThreadId);
    aEW.WriteObject(WrapProfileBufferUnownedCString(aBacktrace.mName.get()));
  }
};

template <typename Destructor>
struct ProfileBufferEntryWriter::Serializer<
    UniquePtr<baseprofiler::ProfilerBacktrace, Destructor>> {
  static Length Bytes(const UniquePtr<baseprofiler::ProfilerBacktrace,
                                      Destructor>& aBacktrace) {
    if (!aBacktrace) {
      // Null backtrace pointer (treated like an empty backtrace).
      return ULEB128Size<Length>(0);
    }
    return SumBytes(*aBacktrace);
  }

  static void Write(ProfileBufferEntryWriter& aEW,
                    const UniquePtr<baseprofiler::ProfilerBacktrace,
                                    Destructor>& aBacktrace) {
    if (!aBacktrace) {
      // Null backtrace pointer (treated like an empty backtrace).
      aEW.WriteULEB128<Length>(0);
      return;
    }
    aEW.WriteObject(*aBacktrace);
  }
};

template <typename Destructor>
struct ProfileBufferEntryReader::Deserializer<
    UniquePtr<baseprofiler::ProfilerBacktrace, Destructor>> {
  static void ReadInto(
      ProfileBufferEntryReader& aER,
      UniquePtr<baseprofiler::ProfilerBacktrace, Destructor>& aBacktrace) {
    aBacktrace = Read(aER);
  }

  static UniquePtr<baseprofiler::ProfilerBacktrace, Destructor> Read(
      ProfileBufferEntryReader& aER);
};

}  // namespace mozilla

#endif  // __PROFILER_BACKTRACE_H
