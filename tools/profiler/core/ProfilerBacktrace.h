/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PROFILER_BACKTRACE_H
#define __PROFILER_BACKTRACE_H

#include "ProfileBuffer.h"

#include "mozilla/ProfileBufferEntrySerialization.h"
#include "mozilla/UniquePtrExtensions.h"

#include <string>

class ProfileBuffer;
class ProfilerCodeAddressService;
class ThreadInfo;
class UniqueStacks;

namespace mozilla {
class ProfileChunkedBuffer;
class TimeStamp;
namespace baseprofiler {
class SpliceableJSONWriter;
}  // namespace baseprofiler
}  // namespace mozilla

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
      mozilla::UniquePtr<mozilla::ProfileChunkedBuffer>
          aProfileChunkedBufferStorage,
      mozilla::UniquePtr<ProfileBuffer> aProfileBufferStorageOrNull = nullptr);

  // Take pointers to external buffers and use them to stream a backtrace.
  // If null, the backtrace is effectively empty.
  // If both are provided, they must already be connected.
  explicit ProfilerBacktrace(
      const char* aName,
      mozilla::ProfileChunkedBuffer* aExternalProfileChunkedBufferOrNull =
          nullptr,
      ProfileBuffer* aExternalProfileBufferOrNull = nullptr);

  ~ProfilerBacktrace();

  [[nodiscard]] bool IsEmpty() const {
    return !mProfileChunkedBuffer ||
           mozilla::ProfileBufferEntryWriter::Serializer<
               mozilla::ProfileChunkedBuffer>::Bytes(*mProfileChunkedBuffer) <=
               mozilla::ULEB128Size(0u);
  }

  // ProfilerBacktraces' stacks are deduplicated in the context of the
  // profile that contains the backtrace as a marker payload.
  //
  // That is, markers that contain backtraces should not need their own stack,
  // frame, and string tables. They should instead reuse their parent
  // profile's tables.
  ProfilerThreadId StreamJSON(
      mozilla::baseprofiler::SpliceableJSONWriter& aWriter,
      const mozilla::TimeStamp& aProcessStartTime, UniqueStacks& aUniqueStacks);

 private:
  // Used to serialize a ProfilerBacktrace.
  friend struct mozilla::ProfileBufferEntryWriter::Serializer<
      ProfilerBacktrace>;
  friend struct mozilla::ProfileBufferEntryReader::Deserializer<
      ProfilerBacktrace>;

  std::string mName;

  // `ProfileChunkedBuffer` in which `mProfileBuffer` stores its data; must be
  // located before `mProfileBuffer` so that it's destroyed after.
  mozilla::UniquePtr<mozilla::ProfileChunkedBuffer>
      mOptionalProfileChunkedBufferStorage;
  // If null, there is no need to check mProfileBuffer's (if present) underlying
  // buffer because this is done when constructed.
  mozilla::ProfileChunkedBuffer* mProfileChunkedBuffer;

  mozilla::UniquePtr<ProfileBuffer> mOptionalProfileBufferStorage;
  ProfileBuffer* mProfileBuffer;
};

namespace mozilla {

// Format: [ UniquePtr<BlockRingsBuffer> | name ]
// Initial len==0 marks a nullptr or empty backtrace.
template <>
struct mozilla::ProfileBufferEntryWriter::Serializer<ProfilerBacktrace> {
  static Length Bytes(const ProfilerBacktrace& aBacktrace) {
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

  static void Write(mozilla::ProfileBufferEntryWriter& aEW,
                    const ProfilerBacktrace& aBacktrace) {
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
struct mozilla::ProfileBufferEntryWriter::Serializer<
    mozilla::UniquePtr<ProfilerBacktrace, Destructor>> {
  static Length Bytes(
      const mozilla::UniquePtr<ProfilerBacktrace, Destructor>& aBacktrace) {
    if (!aBacktrace) {
      // Null backtrace pointer (treated like an empty backtrace).
      return ULEB128Size(0u);
    }
    return SumBytes(*aBacktrace);
  }

  static void Write(
      mozilla::ProfileBufferEntryWriter& aEW,
      const mozilla::UniquePtr<ProfilerBacktrace, Destructor>& aBacktrace) {
    if (!aBacktrace) {
      // Null backtrace pointer (treated like an empty backtrace).
      aEW.WriteULEB128(0u);
      return;
    }
    aEW.WriteObject(*aBacktrace);
  }
};

template <typename Destructor>
struct mozilla::ProfileBufferEntryReader::Deserializer<
    mozilla::UniquePtr<ProfilerBacktrace, Destructor>> {
  static void ReadInto(
      mozilla::ProfileBufferEntryReader& aER,
      mozilla::UniquePtr<ProfilerBacktrace, Destructor>& aBacktrace) {
    aBacktrace = Read(aER);
  }

  static mozilla::UniquePtr<ProfilerBacktrace, Destructor> Read(
      mozilla::ProfileBufferEntryReader& aER) {
    auto profileChunkedBuffer =
        aER.ReadObject<UniquePtr<ProfileChunkedBuffer>>();
    if (!profileChunkedBuffer) {
      return nullptr;
    }
    MOZ_ASSERT(
        !profileChunkedBuffer->IsThreadSafe(),
        "ProfilerBacktrace only stores non-thread-safe ProfileChunkedBuffers");
    std::string name = aER.ReadObject<std::string>();
    return UniquePtr<ProfilerBacktrace, Destructor>{
        new ProfilerBacktrace(name.c_str(), std::move(profileChunkedBuffer))};
  }
};

}  // namespace mozilla

#endif  // __PROFILER_BACKTRACE_H
