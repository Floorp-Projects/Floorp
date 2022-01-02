/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PROFILER_BACKTRACE_H
#define __PROFILER_BACKTRACE_H

#include "mozilla/ProfileChunkedBuffer.h"
#include "mozilla/UniquePtr.h"

#include <string>

namespace mozilla {

class TimeStamp;

namespace baseprofiler {

class ProfileBuffer;
class SpliceableJSONWriter;
class ThreadInfo;
class UniqueStacks;

// ProfilerBacktrace encapsulates a synchronous sample.
// It can work with a ProfileBuffer and/or a ProfileChunkedBuffer (if both, they
// must already be linked together). The ProfileChunkedBuffer contains all the
// data; the ProfileBuffer is not strictly needed, only provide it if it is
// already available at the call site.
// And these buffers can either be:
// - owned here, so that the ProfilerBacktrace object can be kept for later
//   use), OR
// - referenced through pointers (in cases where the backtrace is immediately
//   streamed out, so we only need temporary references to external buffers);
//   these pointers may be null for empty backtraces.
class ProfilerBacktrace {
 public:
  // Take ownership of external buffers and use them to keep, and to stream a
  // backtrace. If a ProfileBuffer is given, its underlying chunked buffer must
  // be provided as well.
  explicit ProfilerBacktrace(
      const char* aName,
      UniquePtr<ProfileChunkedBuffer> aProfileChunkedBufferStorage,
      UniquePtr<ProfileBuffer> aProfileBufferStorageOrNull = nullptr);

  // Take pointers to external buffers and use them to stream a backtrace.
  // If null, the backtrace is effectively empty.
  // If both are provided, they must already be connected.
  explicit ProfilerBacktrace(
      const char* aName,
      ProfileChunkedBuffer* aExternalProfileChunkedBufferOrNull = nullptr,
      ProfileBuffer* aExternalProfileBufferOrNull = nullptr);

  ~ProfilerBacktrace();

  [[nodiscard]] bool IsEmpty() const {
    return !mProfileChunkedBuffer ||
           ProfileBufferEntryWriter::Serializer<ProfileChunkedBuffer>::Bytes(
               *mProfileChunkedBuffer) <= ULEB128Size(0u);
  }

  // ProfilerBacktraces' stacks are deduplicated in the context of the
  // profile that contains the backtrace as a marker payload.
  //
  // That is, markers that contain backtraces should not need their own stack,
  // frame, and string tables. They should instead reuse their parent
  // profile's tables.
  BaseProfilerThreadId StreamJSON(SpliceableJSONWriter& aWriter,
                                  const TimeStamp& aProcessStartTime,
                                  UniqueStacks& aUniqueStacks);

 private:
  // Used to de/serialize a ProfilerBacktrace.
  friend ProfileBufferEntryWriter::Serializer<ProfilerBacktrace>;
  friend ProfileBufferEntryReader::Deserializer<ProfilerBacktrace>;

  std::string mName;

  // `ProfileChunkedBuffer` in which `mProfileBuffer` stores its data; must be
  // located before `mProfileBuffer` so that it's destroyed after.
  UniquePtr<ProfileChunkedBuffer> mOptionalProfileChunkedBufferStorage;
  // If null, there is no need to check mProfileBuffer's (if present) underlying
  // buffer because this is done when constructed.
  ProfileChunkedBuffer* mProfileChunkedBuffer;

  UniquePtr<ProfileBuffer> mOptionalProfileBufferStorage;
  ProfileBuffer* mProfileBuffer;
};

}  // namespace baseprofiler

// Format: [ UniquePtr<BlockRingsBuffer> | name ]
// Initial len==0 marks a nullptr or empty backtrace.
template <>
struct ProfileBufferEntryWriter::Serializer<baseprofiler::ProfilerBacktrace> {
  static Length Bytes(const baseprofiler::ProfilerBacktrace& aBacktrace) {
    if (!aBacktrace.mProfileChunkedBuffer) {
      // No buffer.
      return ULEB128Size(0u);
    }
    auto bufferBytes = SumBytes(*aBacktrace.mProfileChunkedBuffer);
    if (bufferBytes <= ULEB128Size(0u)) {
      // Empty buffer.
      return ULEB128Size(0u);
    }
    return bufferBytes + SumBytes(aBacktrace.mName);
  }

  static void Write(ProfileBufferEntryWriter& aEW,
                    const baseprofiler::ProfilerBacktrace& aBacktrace) {
    if (!aBacktrace.mProfileChunkedBuffer ||
        SumBytes(*aBacktrace.mProfileChunkedBuffer) <= ULEB128Size(0u)) {
      // No buffer, or empty buffer.
      aEW.WriteULEB128(0u);
      return;
    }
    aEW.WriteObject(*aBacktrace.mProfileChunkedBuffer);
    aEW.WriteObject(aBacktrace.mName);
  }
};

template <typename Destructor>
struct ProfileBufferEntryWriter::Serializer<
    UniquePtr<baseprofiler::ProfilerBacktrace, Destructor>> {
  static Length Bytes(const UniquePtr<baseprofiler::ProfilerBacktrace,
                                      Destructor>& aBacktrace) {
    if (!aBacktrace) {
      // Null backtrace pointer (treated like an empty backtrace).
      return ULEB128Size(0u);
    }
    return SumBytes(*aBacktrace);
  }

  static void Write(ProfileBufferEntryWriter& aEW,
                    const UniquePtr<baseprofiler::ProfilerBacktrace,
                                    Destructor>& aBacktrace) {
    if (!aBacktrace) {
      // Null backtrace pointer (treated like an empty backtrace).
      aEW.WriteULEB128(0u);
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
