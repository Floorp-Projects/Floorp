/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PROFILER_BACKTRACE_H
#define __PROFILER_BACKTRACE_H

#include "ProfileBuffer.h"

#include "mozilla/UniquePtrExtensions.h"

class ProfileBuffer;
class ProfilerCodeAddressService;
class SpliceableJSONWriter;
class ThreadInfo;
class UniqueStacks;

namespace mozilla {
class BlocksRingBuffer;
class TimeStamp;
}  // namespace mozilla

// ProfilerBacktrace encapsulates a synchronous sample.
class ProfilerBacktrace {
 public:
  ProfilerBacktrace(const char* aName, int aThreadId,
                    UniquePtr<mozilla::BlocksRingBuffer> aBlocksRingBuffer,
                    mozilla::UniquePtr<ProfileBuffer> aProfileBuffer);
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
  // Used to serialize a ProfilerBacktrace.
  friend struct BlocksRingBuffer::Serializer<ProfilerBacktrace>;
  friend struct BlocksRingBuffer::Deserializer<ProfilerBacktrace>;

  mozilla::UniqueFreePtr<char> mName;
  int mThreadId;
  // `BlocksRingBuffer` in which `mProfileBuffer` stores its data; must be
  // located before `mProfileBuffer` so that it's destroyed after.
  UniquePtr<mozilla::BlocksRingBuffer> mBlocksRingBuffer;
  mozilla::UniquePtr<ProfileBuffer> mProfileBuffer;
};

namespace mozilla {

// Format: [ UniquePtr<BlockRingsBuffer> | threadId | name ]
// Initial len==0 marks a nullptr or empty backtrace.
template <>
struct BlocksRingBuffer::Serializer<ProfilerBacktrace> {
  static Length Bytes(const ProfilerBacktrace& aBacktrace) {
    if (!aBacktrace.mProfileBuffer) {
      return ULEB128Size<Length>(0);
    }
    auto bufferBytes = SumBytes(*aBacktrace.mBlocksRingBuffer);
    if (bufferBytes == 0) {
      return ULEB128Size<Length>(0);
    }
    return bufferBytes +
           SumBytes(aBacktrace.mThreadId,
                    WrapBlocksRingBufferUnownedCString(aBacktrace.mName.get()));
  }
  static void Write(EntryWriter& aEW, const ProfilerBacktrace& aBacktrace) {
    if (!aBacktrace.mProfileBuffer ||
        SumBytes(*aBacktrace.mBlocksRingBuffer) == 0) {
      aEW.WriteULEB128(0u);
      return;
    }
    aEW.WriteObject(*aBacktrace.mBlocksRingBuffer);
    aEW.WriteObject(aBacktrace.mThreadId);
    aEW.WriteObject(WrapBlocksRingBufferUnownedCString(aBacktrace.mName.get()));
  }
};

template <typename Destructor>
struct BlocksRingBuffer::Serializer<UniquePtr<ProfilerBacktrace, Destructor>> {
  static Length Bytes(
      const UniquePtr<ProfilerBacktrace, Destructor>& aBacktrace) {
    if (!aBacktrace) {
      return ULEB128Size<Length>(0);
    }
    return SumBytes(*aBacktrace);
  }
  static void Write(
      EntryWriter& aEW,
      const UniquePtr<ProfilerBacktrace, Destructor>& aBacktrace) {
    if (!aBacktrace) {
      aEW.WriteULEB128(0u);
      return;
    }
    aEW.WriteObject(*aBacktrace);
  }
};
template <typename Destructor>
struct BlocksRingBuffer::Deserializer<
    UniquePtr<ProfilerBacktrace, Destructor>> {
  static void ReadInto(EntryReader& aER,
                       UniquePtr<ProfilerBacktrace, Destructor>& aBacktrace) {
    aBacktrace = Read(aER);
  }
  static UniquePtr<ProfilerBacktrace, Destructor> Read(EntryReader& aER) {
    auto blocksRingBuffer = aER.ReadObject<UniquePtr<BlocksRingBuffer>>();
    if (!blocksRingBuffer) {
      return nullptr;
    }
    MOZ_ASSERT(
        !blocksRingBuffer->IsThreadSafe(),
        "ProfilerBacktrace only stores non-thread-safe BlocksRingBuffers");
    int threadId = aER.ReadObject<int>();
    std::string name = aER.ReadObject<std::string>();
    auto profileBuffer = MakeUnique<ProfileBuffer>(*blocksRingBuffer);
    return UniquePtr<ProfilerBacktrace, Destructor>{new ProfilerBacktrace(
        name.c_str(), threadId, std::move(blocksRingBuffer),
        std::move(profileBuffer))};
  }
};

}  // namespace mozilla

#endif  // __PROFILER_BACKTRACE_H
