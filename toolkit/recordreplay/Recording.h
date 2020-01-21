/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_recordreplay_Recording_h
#define mozilla_recordreplay_Recording_h

#include "InfallibleVector.h"
#include "ProcessRecordReplay.h"
#include "SpinLock.h"

#include "mozilla/PodOperations.h"
#include "mozilla/RecordReplay.h"
#include "mozilla/UniquePtr.h"
#include "nsString.h"

namespace mozilla {
namespace recordreplay {

// Representation of the recording which is written to by recording processes
// and read from by replaying processes. The recording encapsulates a set of
// streams of data. While recording, these streams grow independently from one
// another, and when the recording is flushed the streams contents are collated
// into a single stream of bytes which can be saved to disk or sent to other
// processes via IPC or network connections.
//
// Data in the recording is automatically compressed with LZ4. The Recording
// object is threadsafe for simultaneous read/read and write/write accesses.
// Stream is not threadsafe.

// A location of a chunk of a stream within a recording.
struct StreamChunkLocation {
  // Offset into the recording of the start of the chunk.
  uint64_t mOffset;

  // Compressed size of the chunk, as stored in the recording.
  uint32_t mCompressedSize;

  // Decompressed size of the chunk.
  uint32_t mDecompressedSize;

  // Hash of the compressed chunk data.
  uint32_t mHash;

  // Position in the stream of the start of this chunk.
  uint64_t mStreamPos;
};

enum class StreamName {
  // Per-thread list of events.
  Event,

  // Per-lock list of threads in acquire order.
  Lock,

  // Single stream containing endpoints of recording after flushing.
  Endpoint,

  // Single stream describing recording sections to skip for local replay;.
  LocalReplaySkip,

  Count
};

class Recording;
class RecordingEventSection;

class Stream {
  friend class Recording;
  friend class RecordingEventSection;

  // Recording this stream belongs to.
  Recording* mRecording;

  // Prefix name for this stream.
  StreamName mName;

  // Index which, when combined with mName, uniquely identifies this stream in
  // the recording.
  size_t mNameIndex;

  // All chunks of data in the stream.
  InfallibleVector<StreamChunkLocation> mChunks;

  // Data buffer.
  UniquePtr<char[]> mBuffer;

  // The maximum number of bytes to buffer before compressing and writing to
  // the recording, and the maximum number of bytes that can be decompressed at
  // once.
  static const size_t BUFFER_MAX = 1024 * 1024;

  // The capacity of mBuffer, at most BUFFER_MAX.
  size_t mBufferSize;

  // During reading, the number of accessible bytes in mBuffer.
  size_t mBufferLength;

  // The number of bytes read or written from mBuffer.
  size_t mBufferPos;

  // The number of uncompressed bytes read or written from the stream.
  size_t mStreamPos;

  // Any buffer available for use when decompressing or compressing data.
  UniquePtr<char[]> mBallast;
  size_t mBallastSize;

  // Any buffer available to check for input mismatches.
  UniquePtr<char[]> mInputBallast;
  size_t mInputBallastSize;

  // The last event in this stream, in case of an input mismatch.
  ThreadEvent mLastEvent;

  // The number of chunks that have been completely read or written. When
  // writing, this equals mChunks.length().
  size_t mChunkIndex;

  // Whether there is a RecordingEventSection instance active for this stream.
  bool mInRecordingEventSection;

  // When replaying and MOZ_REPLAYING_DUMP_EVENTS is set, this describes all
  // events in the stream we have replayed so far.
  InfallibleVector<char*> mEvents;

  Stream(Recording* aRecording, StreamName aName, size_t aNameIndex)
      : mRecording(aRecording),
        mName(aName),
        mNameIndex(aNameIndex),
        mBuffer(nullptr),
        mBufferSize(0),
        mBufferLength(0),
        mBufferPos(0),
        mStreamPos(0),
        mBallast(nullptr),
        mBallastSize(0),
        mInputBallast(nullptr),
        mInputBallastSize(0),
        mLastEvent((ThreadEvent)0),
        mChunkIndex(0),
        mInRecordingEventSection(false) {}

 public:
  StreamName Name() const { return mName; }
  size_t NameIndex() const { return mNameIndex; }

  void ReadBytes(void* aData, size_t aSize);
  void WriteBytes(const void* aData, size_t aSize);
  size_t ReadScalar();
  void WriteScalar(size_t aValue);
  bool AtEnd();

  inline void RecordOrReplayBytes(void* aData, size_t aSize) {
    if (IsRecording()) {
      WriteBytes(aData, aSize);
    } else {
      ReadBytes(aData, aSize);
    }
  }

  template <typename T>
  inline void RecordOrReplayScalar(T* aPtr) {
    if (IsRecording()) {
      WriteScalar((size_t)*aPtr);
    } else {
      *aPtr = (T)ReadScalar();
    }
  }

  template <typename T>
  inline void RecordOrReplayValue(T* aPtr) {
    RecordOrReplayBytes(aPtr, sizeof(T));
  }

  // Note a new thread event for this stream, and make sure it is the same
  // while replaying as it was while recording.
  void RecordOrReplayThreadEvent(ThreadEvent aEvent,
                                 const char* aExtra = nullptr);

  // Replay a thread event without requiring it to be a specific event.
  ThreadEvent ReplayThreadEvent();

  // Make sure that a value or buffer is the same while replaying as it was
  // while recording.
  void CheckInput(size_t aValue);
  void CheckInput(const char* aValue);
  void CheckInput(const void* aData, size_t aSize);

  inline size_t StreamPosition() { return mStreamPos; }

 private:
  enum ShouldCopy { DontCopyExistingData, CopyExistingData };

  void EnsureMemory(UniquePtr<char[]>* aBuf, size_t* aSize, size_t aNeededSize,
                    size_t aMaxSize, ShouldCopy aCopy);
  void EnsureInputBallast(size_t aSize);
  void Flush(bool aTakeLock);
  const char* ReadInputString();

  static size_t BallastMaxSize();

  void PushEvent(const char* aEvent);
  void DumpEvents();

  bool ReadMismatchedEventData(ThreadEvent aEvent);
};

// All information about the platform and build where a recording was made.
struct BuildId {
  char mContents[128];

  BuildId() { PodZero(this); }
  bool Matches(const BuildId& aOther) {
    return !memcmp(this, &aOther, sizeof(*this));
  }
};

// Get the build ID for the currently running process.
void GetCurrentBuildId(BuildId* aBuildId);

class Recording {
 public:
  enum Mode { WRITE, READ };

  friend class Stream;
  friend class RecordingEventSection;

 private:
  // Whether this recording is for writing or reading.
  Mode mMode = READ;

  // When writing, all contents that have been flushed so far. When reading,
  // all known contents. When writing, existing parts of the recording are not
  // modified: the recording can only grow.
  InfallibleVector<uint8_t> mContents;

  // All streams in this recording, indexed by stream name and name index.
  typedef InfallibleVector<UniquePtr<Stream>> StreamVector;
  StreamVector mStreams[(size_t)StreamName::Count];

  // Lock protecting access to this recording.
  SpinLock mLock;

  // When writing, lock for synchronizing flushes (writer) with other threads
  // writing to streams in this recording (readers).
  ReadWriteSpinLock mStreamLock;

 public:
  Recording();

  bool IsWriting() const { return mMode == WRITE; }
  bool IsReading() const { return mMode == READ; }

  const uint8_t* Data() const { return mContents.begin(); }
  size_t Size() const { return mContents.length(); }

  // Get or create a stream in this recording.
  Stream* OpenStream(StreamName aName, size_t aNameIndex);

  // When reading, append additional contents to this recording.
  // aUpdatedStreams is optional and filled in with streams whose contents have
  // changed, and may have duplicates.
  void NewContents(const uint8_t* aContents, size_t aSize,
                   InfallibleVector<Stream*>* aUpdatedStreams);

  // Prevent/allow other threads to write to streams in this recording.
  void PreventStreamWrites() { mStreamLock.WriteLock(); }
  void AllowStreamWrites() { mStreamLock.WriteUnlock(); }

  // Flush all streams to the recording.
  void Flush();

  // Get the build ID embedded in a recording.
  static void ExtractBuildId(const char* aContents, size_t aLength,
                             BuildId* aBuildId);

 private:
  StreamChunkLocation WriteChunk(StreamName aName, size_t aNameIndex,
                                 const char* aStart, size_t aCompressedSize,
                                 size_t aDecompressedSize, uint64_t aStreamPos,
                                 bool aTakeLock);
  void ReadChunk(char* aDest, const StreamChunkLocation& aChunk);
};

}  // namespace recordreplay
}  // namespace mozilla

#endif  // mozilla_recordreplay_Recording_h
