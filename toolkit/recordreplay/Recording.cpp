/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Recording.h"

#include "ipc/ChildInternal.h"
#include "mozilla/Compression.h"
#include "mozilla/Sprintf.h"
#include "ProcessRewind.h"
#include "SpinLock.h"

#include <algorithm>

namespace mozilla {
namespace recordreplay {

///////////////////////////////////////////////////////////////////////////////
// Stream
///////////////////////////////////////////////////////////////////////////////

void Stream::ReadBytes(void* aData, size_t aSize) {
  MOZ_RELEASE_ASSERT(mRecording->IsReading());

  size_t totalRead = 0;

  while (true) {
    // Read what we can from the data buffer.
    MOZ_RELEASE_ASSERT(mBufferPos <= mBufferLength);
    size_t bufAvailable = mBufferLength - mBufferPos;
    size_t bufRead = std::min(bufAvailable, aSize);
    if (aData) {
      memcpy(aData, &mBuffer[mBufferPos], bufRead);
      aData = (char*)aData + bufRead;
    }
    mBufferPos += bufRead;
    mStreamPos += bufRead;
    totalRead += bufRead;
    aSize -= bufRead;

    if (!aSize) {
      return;
    }

    MOZ_RELEASE_ASSERT(mBufferPos == mBufferLength);
    MOZ_RELEASE_ASSERT(mChunkIndex < mChunks.length());

    const StreamChunkLocation& chunk = mChunks[mChunkIndex++];
    MOZ_RELEASE_ASSERT(chunk.mStreamPos == mStreamPos);

    EnsureMemory(&mBallast, &mBallastSize, chunk.mCompressedSize,
                 BallastMaxSize(), DontCopyExistingData);
    mRecording->ReadChunk(mBallast.get(), chunk);

    EnsureMemory(&mBuffer, &mBufferSize, chunk.mDecompressedSize, BUFFER_MAX,
                 DontCopyExistingData);

    size_t bytesWritten;
    if (!Compression::LZ4::decompress(mBallast.get(), chunk.mCompressedSize,
                                      mBuffer.get(), chunk.mDecompressedSize,
                                      &bytesWritten) ||
        bytesWritten != chunk.mDecompressedSize) {
      MOZ_CRASH();
    }

    mBufferPos = 0;
    mBufferLength = chunk.mDecompressedSize;
  }
}

bool Stream::AtEnd() {
  MOZ_RELEASE_ASSERT(mRecording->IsReading());

  return mBufferPos == mBufferLength && mChunkIndex == mChunks.length();
}

void Stream::WriteBytes(const void* aData, size_t aSize) {
  MOZ_RELEASE_ASSERT(mRecording->IsWriting());
  MOZ_RELEASE_ASSERT(mName != StreamName::Event || mInRecordingEventSection);

  // Prevent the recording from being flushed while we write this data.
  AutoReadSpinLock streamLock(mRecording->mStreamLock);

  while (true) {
    // Fill up the data buffer first.
    MOZ_RELEASE_ASSERT(mBufferPos <= mBufferSize);
    size_t bufAvailable = mBufferSize - mBufferPos;
    size_t bufWrite = (bufAvailable < aSize) ? bufAvailable : aSize;
    memcpy(&mBuffer[mBufferPos], aData, bufWrite);
    mBufferPos += bufWrite;
    mStreamPos += bufWrite;
    if (bufWrite == aSize) {
      return;
    }
    aData = (char*)aData + bufWrite;
    aSize -= bufWrite;

    // Grow the stream's buffer if it is not at its maximum size.
    if (mBufferSize < BUFFER_MAX) {
      EnsureMemory(&mBuffer, &mBufferSize, mBufferSize + 1, BUFFER_MAX,
                   CopyExistingData);
      continue;
    }

    Flush(/* aTakeLock = */ true);
  }
}

size_t Stream::ReadScalar() {
  // Read back a pointer sized value using the same encoding as WriteScalar.
  size_t value = 0, shift = 0;
  while (true) {
    uint8_t bits;
    ReadBytes(&bits, 1);
    value |= (size_t)(bits & 127) << shift;
    if (!(bits & 128)) {
      break;
    }
    shift += 7;
  }
  return value;
}

void Stream::WriteScalar(size_t aValue) {
  // Pointer sized values are written out as unsigned values with an encoding
  // optimized for small values. Each written byte successively captures 7 bits
  // of data from the value, starting at the low end, with the high bit in the
  // byte indicating whether there are any more non-zero bits in the value.
  //
  // With this encoding, values less than 2^7 (128) require one byte, values
  // less than 2^14 (16384) require two bytes, and so forth, but negative
  // numbers end up requiring ten bytes on a 64 bit architecture.
  do {
    uint8_t bits = aValue & 127;
    aValue = aValue >> 7;
    if (aValue) {
      bits |= 128;
    }
    WriteBytes(&bits, 1);
  } while (aValue);
}

// Workaround arc4random being called from jemalloc when recording on macOS but
// not when replaying on linux.
bool Stream::ReadMismatchedEventData(ThreadEvent aEvent) {
  if (!strcmp(ThreadEventName(aEvent), "arc4random")) {
    if (mNameIndex == MainThreadId) {
      // For execution progress counter.
      ReadScalar();
    }

    size_t value;
    RecordOrReplayValue(&value);
    return true;
  }
  return false;
}

void Stream::RecordOrReplayThreadEvent(ThreadEvent aEvent, const char* aExtra) {
  if (IsRecording()) {
    WriteScalar((size_t)aEvent);
  } else {
    ThreadEvent oldEvent = (ThreadEvent)ReadScalar();
    while (oldEvent != aEvent) {
      if (ReadMismatchedEventData(oldEvent)) {
        oldEvent = (ThreadEvent)ReadScalar();
        continue;
      }

      DumpEvents();
      const char* extra = "";
      if (oldEvent == ThreadEvent::Assert) {
        // Include the asserted string in the error. This must match up with
        // the writes in RecordReplayAssert.
        if (mNameIndex == MainThreadId) {
          (void)ReadScalar();  // For the ExecutionProgressCounter write below.
        }
        extra = ReadInputString();
      }
      child::ReportFatalError(
          "Event Mismatch: Recorded %s %s Replayed %s %s",
          ThreadEventName(oldEvent), extra,
          ThreadEventName(aEvent), aExtra ? aExtra : "");
    }
    mLastEvent = aEvent;
    PushEvent(ThreadEventName(aEvent));
  }

  // Check the execution progress counter for events executing on the main
  // thread.
  if (mNameIndex == MainThreadId) {
    CheckInput(*ExecutionProgressCounter());
  }
}

ThreadEvent Stream::ReplayThreadEvent() {
  ThreadEvent event = (ThreadEvent)ReadScalar();
  if (mNameIndex == MainThreadId) {
    CheckInput(*ExecutionProgressCounter());
  }
  return event;
}

void Stream::CheckInput(size_t aValue) {
  if (IsRecording()) {
    WriteScalar(aValue);
  } else {
    size_t oldValue = ReadScalar();
    if (oldValue != aValue) {
      DumpEvents();
      child::ReportFatalError("Input Mismatch: %s Recorded %llu Replayed %llu",
                              ThreadEventName(mLastEvent), oldValue, aValue);
    }
  }
}

const char* Stream::ReadInputString() {
  size_t len = ReadScalar();
  EnsureInputBallast(len + 1);
  ReadBytes(mInputBallast.get(), len);
  mInputBallast[len] = 0;
  return mInputBallast.get();
}

void Stream::CheckInput(const char* aValue) {
  size_t len = strlen(aValue);
  if (IsRecording()) {
    WriteScalar(len);
    WriteBytes(aValue, len);
  } else {
    const char* oldInput = ReadInputString();
    if (strcmp(oldInput, aValue) != 0) {
      DumpEvents();
      child::ReportFatalError("Input Mismatch: %s Recorded %s Replayed %s",
                              ThreadEventName(mLastEvent), oldInput, aValue);
    }
    PushEvent(aValue);
  }
}

void Stream::CheckInput(const void* aData, size_t aSize) {
  CheckInput(aSize);
  if (IsRecording()) {
    WriteBytes(aData, aSize);
  } else {
    EnsureInputBallast(aSize);
    ReadBytes(mInputBallast.get(), aSize);

    if (memcmp(aData, mInputBallast.get(), aSize) != 0) {
      DumpEvents();
      child::ReportFatalError("Input Buffer Mismatch: %s",
                              ThreadEventName(mLastEvent));
    }
  }
}

void Stream::EnsureMemory(UniquePtr<char[]>* aBuf, size_t* aSize,
                          size_t aNeededSize, size_t aMaxSize,
                          ShouldCopy aCopy) {
  // Once a stream buffer grows, it never shrinks again. Buffers start out
  // small because most streams are very small.
  MOZ_RELEASE_ASSERT(!!*aBuf == !!*aSize);
  MOZ_RELEASE_ASSERT(aNeededSize <= aMaxSize);
  if (*aSize < aNeededSize) {
    size_t newSize = std::min(std::max<size_t>(256, aNeededSize * 2), aMaxSize);
    char* newBuf = new char[newSize];
    if (*aBuf && aCopy == CopyExistingData) {
      memcpy(newBuf, aBuf->get(), *aSize);
    }
    aBuf->reset(newBuf);
    *aSize = newSize;
  }
}

void Stream::EnsureInputBallast(size_t aSize) {
  EnsureMemory(&mInputBallast, &mInputBallastSize, aSize, (size_t)-1,
               DontCopyExistingData);
}

void Stream::Flush(bool aTakeLock) {
  MOZ_RELEASE_ASSERT(mRecording->IsWriting());

  if (!mBufferPos) {
    return;
  }

  size_t bound = Compression::LZ4::maxCompressedSize(mBufferPos);
  EnsureMemory(&mBallast, &mBallastSize, bound, BallastMaxSize(),
               DontCopyExistingData);

  size_t compressedSize =
      Compression::LZ4::compress(mBuffer.get(), mBufferPos, mBallast.get());
  MOZ_RELEASE_ASSERT(compressedSize != 0);
  MOZ_RELEASE_ASSERT((size_t)compressedSize <= bound);

  StreamChunkLocation chunk =
      mRecording->WriteChunk(mName, mNameIndex,
                             mBallast.get(), compressedSize, mBufferPos,
                             mStreamPos - mBufferPos, aTakeLock);
  mChunks.append(chunk);
  MOZ_ALWAYS_TRUE(++mChunkIndex == mChunks.length());

  mBufferPos = 0;
}

/* static */
size_t Stream::BallastMaxSize() {
  return Compression::LZ4::maxCompressedSize(BUFFER_MAX);
}

static bool gDumpEvents;

void Stream::PushEvent(const char* aEvent) {
  if (gDumpEvents) {
    mEvents.append(strdup(aEvent));
  }
}

void Stream::DumpEvents() {
  if (gDumpEvents) {
    Print("Thread Events: %d\n", Thread::Current()->Id());
    for (char* ev : mEvents) {
      Print("Event: %s\n", ev);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Recording
///////////////////////////////////////////////////////////////////////////////

Recording::Recording() : mMode(IsRecording() ? WRITE : READ) {
  PodZero(&mLock);
  PodZero(&mStreamLock);

  if (IsReplaying()) {
    gDumpEvents = TestEnv("MOZ_REPLAYING_DUMP_EVENTS");
  }
}

// The recording format is a series of chunks. Each chunk is a ChunkDescriptor
// followed by the compressed contents of the chunk itself.
struct ChunkDescriptor {
  uint32_t /* StreamName */ mName;
  uint32_t mNameIndex;
  StreamChunkLocation mChunk;

  ChunkDescriptor() { PodZero(this); }

  ChunkDescriptor(StreamName aName, uint32_t aNameIndex,
                  const StreamChunkLocation& aChunk)
      : mName((uint32_t)aName), mNameIndex(aNameIndex), mChunk(aChunk) {}
};

void Recording::NewContents(const uint8_t* aContents, size_t aSize,
                            InfallibleVector<Stream*>* aUpdatedStreams) {
  // All other recorded threads are idle when adding new contents, so we don't
  // have to worry about thread safety here.
  MOZ_RELEASE_ASSERT(Thread::CurrentIsMainThread());
  MOZ_RELEASE_ASSERT(IsReading());

  mContents.append(aContents, aSize);

  size_t offset = 0;
  while (offset < aSize) {
    MOZ_RELEASE_ASSERT(offset + sizeof(ChunkDescriptor) <= aSize);
    ChunkDescriptor* desc = (ChunkDescriptor*)(aContents + offset);
    offset += sizeof(ChunkDescriptor);

    Stream* stream = OpenStream((StreamName)desc->mName, desc->mNameIndex);
    stream->mChunks.append(desc->mChunk);
    if (aUpdatedStreams) {
      aUpdatedStreams->append(stream);
    }

    MOZ_RELEASE_ASSERT(offset + desc->mChunk.mCompressedSize <= aSize);
    offset += desc->mChunk.mCompressedSize;
  }
}

void Recording::Flush() {
  AutoSpinLock lock(mLock);

  for (auto& vector : mStreams) {
    for (const UniquePtr<Stream>& stream : vector) {
      if (stream) {
        stream->Flush(/* aTakeLock = */ false);
      }
    }
  }
}

StreamChunkLocation Recording::WriteChunk(StreamName aName, size_t aNameIndex,
                                          const char* aStart,
                                          size_t aCompressedSize,
                                          size_t aDecompressedSize,
                                          uint64_t aStreamPos, bool aTakeLock) {
  Maybe<AutoSpinLock> lock;
  if (aTakeLock) {
    lock.emplace(mLock);
  }

  StreamChunkLocation chunk;
  chunk.mOffset = mContents.length() + sizeof(ChunkDescriptor);
  chunk.mCompressedSize = aCompressedSize;
  chunk.mDecompressedSize = aDecompressedSize;
  chunk.mHash = HashBytes(aStart, aCompressedSize);
  chunk.mStreamPos = aStreamPos;

  ChunkDescriptor desc;
  desc.mName = (uint32_t) aName;
  desc.mNameIndex = aNameIndex;
  desc.mChunk = chunk;

  mContents.append((const uint8_t*)&desc, sizeof(ChunkDescriptor));
  mContents.append(aStart, aCompressedSize);

  return chunk;
}

void Recording::ReadChunk(char* aDest, const StreamChunkLocation& aChunk) {
  AutoSpinLock lock(mLock);
  MOZ_RELEASE_ASSERT(aChunk.mOffset + aChunk.mCompressedSize <= mContents.length());
  memcpy(aDest, mContents.begin() + aChunk.mOffset, aChunk.mCompressedSize);
  MOZ_RELEASE_ASSERT(HashBytes(aDest, aChunk.mCompressedSize) == aChunk.mHash);
}

Stream* Recording::OpenStream(StreamName aName, size_t aNameIndex) {
  AutoSpinLock lock(mLock);

  auto& vector = mStreams[(size_t)aName];

  while (aNameIndex >= vector.length()) {
    vector.emplaceBack();
  }

  UniquePtr<Stream>& stream = vector[aNameIndex];
  if (!stream) {
    stream.reset(new Stream(this, aName, aNameIndex));
  }
  return stream.get();
}

}  // namespace recordreplay
}  // namespace mozilla
