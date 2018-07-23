/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "File.h"

#include "ipc/ChildIPC.h"
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

void
Stream::ReadBytes(void* aData, size_t aSize)
{
  MOZ_RELEASE_ASSERT(mFile->OpenForReading());

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

    // If we try to read off the end of a stream then we must have hit the end
    // of the replay for this thread.
    while (mChunkIndex == mChunks.length()) {
      MOZ_RELEASE_ASSERT(mName == StreamName::Event || mName == StreamName::Assert);
      HitEndOfRecording();
    }

    const StreamChunkLocation& chunk = mChunks[mChunkIndex++];

    EnsureMemory(&mBallast, &mBallastSize, chunk.mCompressedSize, BallastMaxSize(),
                 DontCopyExistingData);
    mFile->ReadChunk(mBallast.get(), chunk);

    EnsureMemory(&mBuffer, &mBufferSize, chunk.mDecompressedSize, BUFFER_MAX,
                 DontCopyExistingData);

    size_t bytesWritten;
    if (!Compression::LZ4::decompress(mBallast.get(), chunk.mCompressedSize,
                                      mBuffer.get(), chunk.mDecompressedSize, &bytesWritten) ||
        bytesWritten != chunk.mDecompressedSize)
    {
      MOZ_CRASH();
    }

    mBufferPos = 0;
    mBufferLength = chunk.mDecompressedSize;
  }
}

bool
Stream::AtEnd()
{
  MOZ_RELEASE_ASSERT(mFile->OpenForReading());

  return mBufferPos == mBufferLength && mChunkIndex == mChunks.length();
}

void
Stream::WriteBytes(const void* aData, size_t aSize)
{
  MOZ_RELEASE_ASSERT(mFile->OpenForWriting());

  // Prevent the entire file from being flushed while we write this data.
  AutoReadSpinLock streamLock(mFile->mStreamLock);

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

    // Grow the file's buffer if it is not at its maximum size.
    if (mBufferSize < BUFFER_MAX) {
      EnsureMemory(&mBuffer, &mBufferSize, mBufferSize + 1, BUFFER_MAX, CopyExistingData);
      continue;
    }

    Flush(/* aTakeLock = */ true);
  }
}

size_t
Stream::ReadScalar()
{
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

void
Stream::WriteScalar(size_t aValue)
{
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

void
Stream::CheckInput(size_t aValue)
{
  size_t oldValue = aValue;
  RecordOrReplayScalar(&oldValue);
  if (oldValue != aValue) {
    child::ReportFatalError("Input Mismatch: Recorded: %zu Replayed %zu\n", oldValue, aValue);
    Unreachable();
  }
}

void
Stream::EnsureMemory(UniquePtr<char[]>* aBuf, size_t* aSize,
                     size_t aNeededSize, size_t aMaxSize, ShouldCopy aCopy)
{
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

void
Stream::Flush(bool aTakeLock)
{
  MOZ_RELEASE_ASSERT(mFile && mFile->OpenForWriting());

  if (!mBufferPos) {
    return;
  }

  size_t bound = Compression::LZ4::maxCompressedSize(mBufferPos);
  EnsureMemory(&mBallast, &mBallastSize, bound, BallastMaxSize(),
               DontCopyExistingData);

  size_t compressedSize = Compression::LZ4::compress(mBuffer.get(), mBufferPos, mBallast.get());
  MOZ_RELEASE_ASSERT(compressedSize != 0);
  MOZ_RELEASE_ASSERT((size_t)compressedSize <= bound);

  StreamChunkLocation chunk =
    mFile->WriteChunk(mBallast.get(), compressedSize, mBufferPos, aTakeLock);
  mChunks.append(chunk);
  MOZ_ALWAYS_TRUE(++mChunkIndex == mChunks.length());

  mBufferPos = 0;
}

/* static */ size_t
Stream::BallastMaxSize()
{
  return Compression::LZ4::maxCompressedSize(BUFFER_MAX);
}

///////////////////////////////////////////////////////////////////////////////
// File
///////////////////////////////////////////////////////////////////////////////

// Information in a file index about a chunk.
struct FileIndexChunk
{
  uint32_t /* StreamName */ mName;
  uint32_t mNameIndex;
  StreamChunkLocation mChunk;

  FileIndexChunk()
  {
    PodZero(this);
  }

  FileIndexChunk(StreamName aName, uint32_t aNameIndex, const StreamChunkLocation& aChunk)
    : mName((uint32_t) aName), mNameIndex(aNameIndex), mChunk(aChunk)
  {}
};

// We expect to find this at every index in a file.
static const uint64_t MagicValue = 0xd3e7f5fae445b3ac;

// Index of chunks in a file. There is an index at the start of the file
// (which is always empty) and at various places within the file itself.
struct FileIndex
{
  // This should match MagicValue.
  uint64_t mMagic;

  // How many FileIndexChunk instances follow this structure.
  uint32_t mNumChunks;

  // The location of the next index in the file, or zero.
  uint64_t mNextIndexOffset;

  explicit FileIndex(uint32_t aNumChunks)
    : mMagic(MagicValue), mNumChunks(aNumChunks), mNextIndexOffset(0)
  {}
};

bool
File::Open(const char* aName, Mode aMode)
{
  MOZ_RELEASE_ASSERT(!mFd);
  MOZ_RELEASE_ASSERT(aName);

  mMode = aMode;
  mFd = DirectOpenFile(aName, mMode == WRITE);

  if (OpenForWriting()) {
    // Write an empty index at the start of the file.
    FileIndex index(0);
    DirectWrite(mFd, &index, sizeof(index));
    mWriteOffset += sizeof(index);
    return true;
  }

  // Read in every index in the file.
  ReadIndexResult result;
  do {
    result = ReadNextIndex(nullptr);
    if (result == ReadIndexResult::InvalidFile) {
      return false;
    }
  } while (result == ReadIndexResult::FoundIndex);

  return true;
}

void
File::Close()
{
  if (!mFd) {
    return;
  }

  if (OpenForWriting()) {
    Flush();
  }

  Clear();
}

File::ReadIndexResult
File::ReadNextIndex(InfallibleVector<Stream*>* aUpdatedStreams)
{
  // Unlike in the Flush() case, we don't have to worry about other threads
  // attempting to read data from streams in this file while we are reading
  // the new index.
  MOZ_ASSERT(OpenForReading());

  // Read in the last index to see if there is another one.
  DirectSeekFile(mFd, mLastIndexOffset + offsetof(FileIndex, mNextIndexOffset));
  uint64_t nextIndexOffset;
  if (DirectRead(mFd, &nextIndexOffset, sizeof(nextIndexOffset)) != sizeof(nextIndexOffset)) {
    return ReadIndexResult::InvalidFile;
  }
  if (!nextIndexOffset) {
    return ReadIndexResult::EndOfFile;
  }

  mLastIndexOffset = nextIndexOffset;

  FileIndex index(0);
  DirectSeekFile(mFd, nextIndexOffset);
  if (DirectRead(mFd, &index, sizeof(index)) != sizeof(index)) {
    return ReadIndexResult::InvalidFile;
  }
  if (index.mMagic != MagicValue) {
    return ReadIndexResult::InvalidFile;
  }

  MOZ_RELEASE_ASSERT(index.mNumChunks);

  size_t indexBytes = index.mNumChunks * sizeof(FileIndexChunk);
  FileIndexChunk* chunks = new FileIndexChunk[index.mNumChunks];
  if (DirectRead(mFd, chunks, indexBytes) != indexBytes) {
    return ReadIndexResult::InvalidFile;
  }
  for (size_t i = 0; i < index.mNumChunks; i++) {
    const FileIndexChunk& indexChunk = chunks[i];
    Stream* stream = OpenStream((StreamName) indexChunk.mName, indexChunk.mNameIndex);
    stream->mChunks.append(indexChunk.mChunk);
    if (aUpdatedStreams) {
      aUpdatedStreams->append(stream);
    }
  }
  delete[] chunks;

  return ReadIndexResult::FoundIndex;
}

bool
File::Flush()
{
  MOZ_ASSERT(OpenForWriting());
  AutoSpinLock lock(mLock);

  InfallibleVector<FileIndexChunk> newChunks;
  for (auto& vector : mStreams) {
    for (const UniquePtr<Stream>& stream : vector) {
      if (stream) {
        stream->Flush(/* aTakeLock = */ false);
        for (size_t i = stream->mFlushedChunks; i < stream->mChunkIndex; i++) {
          newChunks.emplaceBack(stream->mName, stream->mNameIndex, stream->mChunks[i]);
        }
        stream->mFlushedChunks = stream->mChunkIndex;
      }
    }
  }

  if (newChunks.empty()) {
    return false;
  }

  // Write the new index information at the end of the file.
  uint64_t indexOffset = mWriteOffset;
  size_t indexBytes = newChunks.length() * sizeof(FileIndexChunk);
  FileIndex index(newChunks.length());
  DirectWrite(mFd, &index, sizeof(index));
  DirectWrite(mFd, newChunks.begin(), indexBytes);
  mWriteOffset += sizeof(index) + indexBytes;

  // Update the next index offset for the last index written.
  MOZ_RELEASE_ASSERT(sizeof(index.mNextIndexOffset) == sizeof(indexOffset));
  DirectSeekFile(mFd, mLastIndexOffset + offsetof(FileIndex, mNextIndexOffset));
  DirectWrite(mFd, &indexOffset, sizeof(indexOffset));
  DirectSeekFile(mFd, mWriteOffset);

  mLastIndexOffset = indexOffset;

  return true;
}

StreamChunkLocation
File::WriteChunk(const char* aStart,
                 size_t aCompressedSize, size_t aDecompressedSize,
                 bool aTakeLock)
{
  Maybe<AutoSpinLock> lock;
  if (aTakeLock) {
    lock.emplace(mLock);
  }

  StreamChunkLocation chunk;
  chunk.mOffset = mWriteOffset;
  chunk.mCompressedSize = aCompressedSize;
  chunk.mDecompressedSize = aDecompressedSize;

  DirectWrite(mFd, aStart, aCompressedSize);
  mWriteOffset += aCompressedSize;

  return chunk;
}

void
File::ReadChunk(char* aDest, const StreamChunkLocation& aChunk)
{
  AutoSpinLock lock(mLock);
  DirectSeekFile(mFd, aChunk.mOffset);
  size_t res = DirectRead(mFd, aDest, aChunk.mCompressedSize);
  if (res != aChunk.mCompressedSize) {
    MOZ_CRASH();
  }
}

Stream*
File::OpenStream(StreamName aName, size_t aNameIndex)
{
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

} // namespace recordreplay
} // namespace mozilla
