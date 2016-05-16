/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsIBufferedStreams.h"
#include "nsICloneableInputStream.h"
#include "nsIPipe.h"
#include "nsIEventTarget.h"
#include "nsISeekableStream.h"
#include "mozilla/RefPtr.h"
#include "nsSegmentedBuffer.h"
#include "nsStreamUtils.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "mozilla/Logging.h"
#include "nsIClassInfoImpl.h"
#include "nsAlgorithm.h"
#include "nsMemory.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"

using namespace mozilla;

#ifdef LOG
#undef LOG
#endif
//
// set NSPR_LOG_MODULES=nsPipe:5
//
static LazyLogModule sPipeLog("nsPipe");
#define LOG(args) MOZ_LOG(sPipeLog, mozilla::LogLevel::Debug, args)

#define DEFAULT_SEGMENT_SIZE  4096
#define DEFAULT_SEGMENT_COUNT 16

class nsPipe;
class nsPipeEvents;
class nsPipeInputStream;
class nsPipeOutputStream;
class AutoReadSegment;

namespace {

enum MonitorAction
{
  DoNotNotifyMonitor,
  NotifyMonitor
};

enum SegmentChangeResult
{
  SegmentNotChanged,
  SegmentDeleted
};

} // namespace

//-----------------------------------------------------------------------------

// this class is used to delay notifications until the end of a particular
// scope.  it helps avoid the complexity of issuing callbacks while inside
// a critical section.
class nsPipeEvents
{
public:
  nsPipeEvents() { }
  ~nsPipeEvents();

  inline void NotifyInputReady(nsIAsyncInputStream* aStream,
                               nsIInputStreamCallback* aCallback)
  {
    mInputList.AppendElement(InputEntry(aStream, aCallback));
  }

  inline void NotifyOutputReady(nsIAsyncOutputStream* aStream,
                                nsIOutputStreamCallback* aCallback)
  {
    NS_ASSERTION(!mOutputCallback, "already have an output event");
    mOutputStream = aStream;
    mOutputCallback = aCallback;
  }

private:
  struct InputEntry
  {
    InputEntry(nsIAsyncInputStream* aStream, nsIInputStreamCallback* aCallback)
      : mStream(aStream)
      , mCallback(aCallback)
    {
      MOZ_ASSERT(mStream);
      MOZ_ASSERT(mCallback);
    }

    nsCOMPtr<nsIAsyncInputStream> mStream;
    nsCOMPtr<nsIInputStreamCallback> mCallback;
  };

  nsTArray<InputEntry> mInputList;

  nsCOMPtr<nsIAsyncOutputStream>    mOutputStream;
  nsCOMPtr<nsIOutputStreamCallback> mOutputCallback;
};

//-----------------------------------------------------------------------------

// This class is used to maintain input stream state.  Its broken out from the
// nsPipeInputStream class because generally the nsPipe should be modifying
// this state and not the input stream itself.
struct nsPipeReadState
{
  nsPipeReadState()
    : mReadCursor(nullptr)
    , mReadLimit(nullptr)
    , mSegment(0)
    , mAvailable(0)
    , mActiveRead(false)
    , mNeedDrain(false)
  { }

  char*    mReadCursor;
  char*    mReadLimit;
  int32_t  mSegment;
  uint32_t mAvailable;

  // This flag is managed using the AutoReadSegment RAII stack class.
  bool     mActiveRead;

  // Set to indicate that the input stream has closed and should be drained,
  // but that drain has been delayed due to an active read.  When the read
  // completes, this flag indicate the drain should then be performed.
  bool     mNeedDrain;
};

//-----------------------------------------------------------------------------

// an input end of a pipe (maintained as a list of refs within the pipe)
class nsPipeInputStream final
  : public nsIAsyncInputStream
  , public nsISeekableStream
  , public nsISearchableInputStream
  , public nsICloneableInputStream
  , public nsIClassInfo
  , public nsIBufferedInputStream
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM
  NS_DECL_NSISEARCHABLEINPUTSTREAM
  NS_DECL_NSICLONEABLEINPUTSTREAM
  NS_DECL_NSICLASSINFO
  NS_DECL_NSIBUFFEREDINPUTSTREAM

  explicit nsPipeInputStream(nsPipe* aPipe)
    : mPipe(aPipe)
    , mLogicalOffset(0)
    , mInputStatus(NS_OK)
    , mBlocking(true)
    , mBlocked(false)
    , mCallbackFlags(0)
  { }

  explicit nsPipeInputStream(const nsPipeInputStream& aOther)
    : mPipe(aOther.mPipe)
    , mLogicalOffset(aOther.mLogicalOffset)
    , mInputStatus(aOther.mInputStatus)
    , mBlocking(aOther.mBlocking)
    , mBlocked(false)
    , mCallbackFlags(0)
    , mReadState(aOther.mReadState)
  { }

  nsresult Fill();
  void SetNonBlocking(bool aNonBlocking)
  {
    mBlocking = !aNonBlocking;
  }

  uint32_t Available();

  // synchronously wait for the pipe to become readable.
  nsresult Wait();

  // These two don't acquire the monitor themselves.  Instead they
  // expect their caller to have done so and to pass the monitor as
  // evidence.
  MonitorAction OnInputReadable(uint32_t aBytesWritten, nsPipeEvents&,
                                const ReentrantMonitorAutoEnter& ev);
  MonitorAction OnInputException(nsresult, nsPipeEvents&,
                                 const ReentrantMonitorAutoEnter& ev);

  nsPipeReadState& ReadState()
  {
    return mReadState;
  }

  const nsPipeReadState& ReadState() const
  {
    return mReadState;
  }

  nsresult Status() const;

  // A version of Status() that doesn't acquire the monitor.
  nsresult Status(const ReentrantMonitorAutoEnter& ev) const;

private:
  virtual ~nsPipeInputStream();

  RefPtr<nsPipe>               mPipe;

  int64_t                        mLogicalOffset;
  // Individual input streams can be closed without effecting the rest of the
  // pipe.  So track individual input stream status separately.  |mInputStatus|
  // is protected by |mPipe->mReentrantMonitor|.
  nsresult                       mInputStatus;
  bool                           mBlocking;

  // these variables can only be accessed while inside the pipe's monitor
  bool                           mBlocked;
  nsCOMPtr<nsIInputStreamCallback> mCallback;
  uint32_t                       mCallbackFlags;

  // requires pipe's monitor; usually treat as an opaque token to pass to nsPipe
  nsPipeReadState                mReadState;
};

//-----------------------------------------------------------------------------

// the output end of a pipe (allocated as a member of the pipe).
class nsPipeOutputStream
  : public nsIAsyncOutputStream
  , public nsIClassInfo
{
public:
  // since this class will be allocated as a member of the pipe, we do not
  // need our own ref count.  instead, we share the lifetime (the ref count)
  // of the entire pipe.  this macro is just convenience since it does not
  // declare a mRefCount variable; however, don't let the name fool you...
  // we are not inheriting from nsPipe ;-)
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIOUTPUTSTREAM
  NS_DECL_NSIASYNCOUTPUTSTREAM
  NS_DECL_NSICLASSINFO

  explicit nsPipeOutputStream(nsPipe* aPipe)
    : mPipe(aPipe)
    , mWriterRefCnt(0)
    , mLogicalOffset(0)
    , mBlocking(true)
    , mBlocked(false)
    , mWritable(true)
    , mCallbackFlags(0)
  { }

  void SetNonBlocking(bool aNonBlocking)
  {
    mBlocking = !aNonBlocking;
  }
  void SetWritable(bool aWritable)
  {
    mWritable = aWritable;
  }

  // synchronously wait for the pipe to become writable.
  nsresult Wait();

  MonitorAction OnOutputWritable(nsPipeEvents&);
  MonitorAction OnOutputException(nsresult, nsPipeEvents&);

private:
  nsPipe*                         mPipe;

  // separate refcnt so that we know when to close the producer
  mozilla::ThreadSafeAutoRefCnt   mWriterRefCnt;
  int64_t                         mLogicalOffset;
  bool                            mBlocking;

  // these variables can only be accessed while inside the pipe's monitor
  bool                            mBlocked;
  bool                            mWritable;
  nsCOMPtr<nsIOutputStreamCallback> mCallback;
  uint32_t                        mCallbackFlags;
};

//-----------------------------------------------------------------------------

class nsPipe final : public nsIPipe
{
public:
  friend class nsPipeInputStream;
  friend class nsPipeOutputStream;
  friend class AutoReadSegment;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPIPE

  // nsPipe methods:
  nsPipe();

private:
  ~nsPipe();

  //
  // methods below may only be called while inside the pipe's monitor
  //

  void PeekSegment(const nsPipeReadState& aReadState, uint32_t aIndex,
                   char*& aCursor, char*& aLimit);
  SegmentChangeResult AdvanceReadSegment(nsPipeReadState& aReadState);
  bool ReadSegmentBeingWritten(nsPipeReadState& aReadState);
  uint32_t CountSegmentReferences(int32_t aSegment);
  void SetAllNullReadCursors();
  bool AllReadCursorsMatchWriteCursor();
  void RollBackAllReadCursors(char* aWriteCursor);
  void UpdateAllReadCursors(char* aWriteCursor);
  void ValidateAllReadCursors();

  //
  // methods below may be called while outside the pipe's monitor
  //

  void     DrainInputStream(nsPipeReadState& aReadState, nsPipeEvents& aEvents);

  nsresult GetWriteSegment(char*& aSegment, uint32_t& aSegmentLen);
  void     AdvanceWriteCursor(uint32_t aCount);

  void     OnInputStreamException(nsPipeInputStream* aStream, nsresult aReason);
  void     OnPipeException(nsresult aReason, bool aOutputOnly = false);

  nsresult CloneInputStream(nsPipeInputStream* aOriginal,
                            nsIInputStream** aCloneOut);

  // methods below should only be called by AutoReadSegment
  nsresult GetReadSegment(nsPipeReadState& aReadState, const char*& aSegment,
                          uint32_t& aLength);
  void     ReleaseReadSegment(nsPipeReadState& aReadState,
                              nsPipeEvents& aEvents);
  void     AdvanceReadCursor(nsPipeReadState& aReadState, uint32_t aCount);

  // We can't inherit from both nsIInputStream and nsIOutputStream
  // because they collide on their Close method. Consequently we nest their
  // implementations to avoid the extra object allocation.
  nsPipeOutputStream  mOutput;

  // Since the input stream can be cloned, we may have more than one.  Use
  // a weak reference as the streams will clear their entry here in their
  // destructor.  Using a strong reference would create a reference cycle.
  // Only usable while mReentrantMonitor is locked.
  nsTArray<nsPipeInputStream*> mInputList;

  // But hold a strong ref to our original input stream.  For backward
  // compatibility we need to be able to consistently return this same
  // object from GetInputStream().  Note, mOriginalInput is also stored
  // in mInputList as a weak ref.
  RefPtr<nsPipeInputStream> mOriginalInput;

  ReentrantMonitor    mReentrantMonitor;
  nsSegmentedBuffer   mBuffer;

  int32_t             mWriteSegment;
  char*               mWriteCursor;
  char*               mWriteLimit;

  // |mStatus| is protected by |mReentrantMonitor|.
  nsresult            mStatus;
  bool                mInited;
};

//-----------------------------------------------------------------------------

// RAII class representing an active read segment.  When it goes out of scope
// it automatically updates the read cursor and releases the read segment.
class MOZ_STACK_CLASS AutoReadSegment final
{
public:
  AutoReadSegment(nsPipe* aPipe, nsPipeReadState& aReadState,
                  uint32_t aMaxLength)
    : mPipe(aPipe)
    , mReadState(aReadState)
    , mStatus(NS_ERROR_FAILURE)
    , mSegment(nullptr)
    , mLength(0)
    , mOffset(0)
  {
    MOZ_ASSERT(mPipe);
    MOZ_ASSERT(!mReadState.mActiveRead);
    mStatus = mPipe->GetReadSegment(mReadState, mSegment, mLength);
    if (NS_SUCCEEDED(mStatus)) {
      MOZ_ASSERT(mReadState.mActiveRead);
      MOZ_ASSERT(mSegment);
      mLength = std::min(mLength, aMaxLength);
      MOZ_ASSERT(mLength);
    }
  }

  ~AutoReadSegment()
  {
    if (NS_SUCCEEDED(mStatus)) {
      if (mOffset) {
        mPipe->AdvanceReadCursor(mReadState, mOffset);
      } else {
        nsPipeEvents events;
        mPipe->ReleaseReadSegment(mReadState, events);
      }
    }
    MOZ_ASSERT(!mReadState.mActiveRead);
  }

  nsresult Status() const
  {
    return mStatus;
  }

  const char* Data() const
  {
    MOZ_ASSERT(NS_SUCCEEDED(mStatus));
    MOZ_ASSERT(mSegment);
    return mSegment + mOffset;
  }

  uint32_t Length() const
  {
    MOZ_ASSERT(NS_SUCCEEDED(mStatus));
    MOZ_ASSERT(mLength >= mOffset);
    return mLength - mOffset;
  }

  void
  Advance(uint32_t aCount)
  {
    MOZ_ASSERT(NS_SUCCEEDED(mStatus));
    MOZ_ASSERT(aCount <= (mLength - mOffset));
    mOffset += aCount;
  }

  nsPipeReadState&
  ReadState() const
  {
    return mReadState;
  }

private:
  // guaranteed to remain alive due to limited stack lifetime of AutoReadSegment
  nsPipe* mPipe;
  nsPipeReadState& mReadState;
  nsresult mStatus;
  const char* mSegment;
  uint32_t mLength;
  uint32_t mOffset;
};

//
// NOTES on buffer architecture:
//
//       +-----------------+ - - mBuffer.GetSegment(0)
//       |                 |
//       + - - - - - - - - + - - nsPipeReadState.mReadCursor
//       |/////////////////|
//       |/////////////////|
//       |/////////////////|
//       |/////////////////|
//       +-----------------+ - - nsPipeReadState.mReadLimit
//                |
//       +-----------------+
//       |/////////////////|
//       |/////////////////|
//       |/////////////////|
//       |/////////////////|
//       |/////////////////|
//       |/////////////////|
//       +-----------------+
//                |
//       +-----------------+ - - mBuffer.GetSegment(mWriteSegment)
//       |/////////////////|
//       |/////////////////|
//       |/////////////////|
//       + - - - - - - - - + - - mWriteCursor
//       |                 |
//       |                 |
//       +-----------------+ - - mWriteLimit
//
// (shaded region contains data)
//
// NOTE: Each input stream produced by the nsPipe contains its own, separate
//       nsPipeReadState.  This means there are multiple mReadCursor and
//       mReadLimit values in play.  The pipe cannot discard old data until
//       all mReadCursors have moved beyond that point in the stream.
//
// NOTE: on some systems (notably OS/2), the heap allocator uses an arena for
// small allocations (e.g., 64 byte allocations).  this means that buffers may
// be allocated back-to-back.  in the diagram above, for example, mReadLimit
// would actually be pointing at the beginning of the next segment.  when
// making changes to this file, please keep this fact in mind.
//

//-----------------------------------------------------------------------------
// nsPipe methods:
//-----------------------------------------------------------------------------

nsPipe::nsPipe()
  : mOutput(this)
  , mOriginalInput(new nsPipeInputStream(this))
  , mReentrantMonitor("nsPipe.mReentrantMonitor")
  , mWriteSegment(-1)
  , mWriteCursor(nullptr)
  , mWriteLimit(nullptr)
  , mStatus(NS_OK)
  , mInited(false)
{
  mInputList.AppendElement(mOriginalInput);
}

nsPipe::~nsPipe()
{
}

NS_IMPL_ADDREF(nsPipe)
NS_IMPL_QUERY_INTERFACE(nsPipe, nsIPipe)

NS_IMETHODIMP_(MozExternalRefCountType)
nsPipe::Release()
{
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
  nsrefcnt count = --mRefCnt;
  NS_LOG_RELEASE(this, count, "nsPipe");
  if (count == 0) {
    delete (this);
    return 0;
  }
  // Avoid racing on |mOriginalInput| by only looking at it when
  // the refcount is 1, that is, we are the only pointer (hence only
  // thread) to access it.
  if (count == 1 && mOriginalInput) {
    mOriginalInput = nullptr;
    return 1;
  }
  return count;
}

NS_IMETHODIMP
nsPipe::Init(bool aNonBlockingIn,
             bool aNonBlockingOut,
             uint32_t aSegmentSize,
             uint32_t aSegmentCount)
{
  mInited = true;

  if (aSegmentSize == 0) {
    aSegmentSize = DEFAULT_SEGMENT_SIZE;
  }
  if (aSegmentCount == 0) {
    aSegmentCount = DEFAULT_SEGMENT_COUNT;
  }

  // protect against overflow
  uint32_t maxCount = uint32_t(-1) / aSegmentSize;
  if (aSegmentCount > maxCount) {
    aSegmentCount = maxCount;
  }

  nsresult rv = mBuffer.Init(aSegmentSize, aSegmentSize * aSegmentCount);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mOutput.SetNonBlocking(aNonBlockingOut);
  mOriginalInput->SetNonBlocking(aNonBlockingIn);

  return NS_OK;
}

NS_IMETHODIMP
nsPipe::GetInputStream(nsIAsyncInputStream** aInputStream)
{
  RefPtr<nsPipeInputStream> ref = mOriginalInput;
  ref.forget(aInputStream);
  return NS_OK;
}

NS_IMETHODIMP
nsPipe::GetOutputStream(nsIAsyncOutputStream** aOutputStream)
{
  if (NS_WARN_IF(!mInited)) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  NS_ADDREF(*aOutputStream = &mOutput);
  return NS_OK;
}

void
nsPipe::PeekSegment(const nsPipeReadState& aReadState, uint32_t aIndex,
                    char*& aCursor, char*& aLimit)
{
  if (aIndex == 0) {
    NS_ASSERTION(!aReadState.mReadCursor || mBuffer.GetSegmentCount(),
                 "unexpected state");
    aCursor = aReadState.mReadCursor;
    aLimit = aReadState.mReadLimit;
  } else {
    uint32_t absoluteIndex = aReadState.mSegment + aIndex;
    uint32_t numSegments = mBuffer.GetSegmentCount();
    if (absoluteIndex >= numSegments) {
      aCursor = aLimit = nullptr;
    } else {
      aCursor = mBuffer.GetSegment(absoluteIndex);
      if (mWriteSegment == (int32_t)absoluteIndex) {
        aLimit = mWriteCursor;
      } else {
        aLimit = aCursor + mBuffer.GetSegmentSize();
      }
    }
  }
}

nsresult
nsPipe::GetReadSegment(nsPipeReadState& aReadState, const char*& aSegment,
                       uint32_t& aLength)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (aReadState.mReadCursor == aReadState.mReadLimit) {
    return NS_FAILED(mStatus) ? mStatus : NS_BASE_STREAM_WOULD_BLOCK;
  }

  // The input stream locks the pipe while getting the buffer to read from,
  // but then unlocks while actual data copying is taking place.  In
  // order to avoid deleting the buffer out from under this lockless read
  // set a flag to indicate a read is active.  This flag is only modified
  // while the lock is held.
  MOZ_ASSERT(!aReadState.mActiveRead);
  aReadState.mActiveRead = true;

  aSegment = aReadState.mReadCursor;
  aLength = aReadState.mReadLimit - aReadState.mReadCursor;

  return NS_OK;
}

void
nsPipe::ReleaseReadSegment(nsPipeReadState& aReadState, nsPipeEvents& aEvents)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  MOZ_ASSERT(aReadState.mActiveRead);
  aReadState.mActiveRead = false;

  // When a read completes and releases the mActiveRead flag, we may have blocked
  // a drain from completing.  This occurs when the input stream is closed during
  // the read.  In these cases, we need to complete the drain as soon as the
  // active read completes.
  if (aReadState.mNeedDrain) {
    aReadState.mNeedDrain = false;
    DrainInputStream(aReadState, aEvents);
  }
}

void
nsPipe::AdvanceReadCursor(nsPipeReadState& aReadState, uint32_t aBytesRead)
{
  NS_ASSERTION(aBytesRead, "don't call if no bytes read");

  nsPipeEvents events;
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    LOG(("III advancing read cursor by %u\n", aBytesRead));
    NS_ASSERTION(aBytesRead <= mBuffer.GetSegmentSize(), "read too much");

    aReadState.mReadCursor += aBytesRead;
    NS_ASSERTION(aReadState.mReadCursor <= aReadState.mReadLimit,
                 "read cursor exceeds limit");

    MOZ_ASSERT(aReadState.mAvailable >= aBytesRead);
    aReadState.mAvailable -= aBytesRead;

    // Check to see if we're at the end of the available read data.  If we
    // are, and this segment is not still being written, then we can possibly
    // free up the segment.
    if (aReadState.mReadCursor == aReadState.mReadLimit &&
        !ReadSegmentBeingWritten(aReadState)) {

      // Check to see if we can free up any segments.  If we can, then notify
      // the output stream that the pipe has room for a new segment.
      if (AdvanceReadSegment(aReadState) == SegmentDeleted &&
          mOutput.OnOutputWritable(events) == NotifyMonitor) {
        mon.NotifyAll();
      }
    }

    ReleaseReadSegment(aReadState, events);
  }
}

SegmentChangeResult
nsPipe::AdvanceReadSegment(nsPipeReadState& aReadState)
{
  mReentrantMonitor.AssertCurrentThreadIn();

  int32_t currentSegment = aReadState.mSegment;

  // Move to the next segment to read
  aReadState.mSegment += 1;

  SegmentChangeResult result = SegmentNotChanged;

  // If this was the last reference to the first segment, then remove it.
  if (currentSegment == 0 && CountSegmentReferences(currentSegment) == 0) {

    // shift write and read segment index (-1 indicates an empty buffer).
    mWriteSegment -= 1;

    // Directly modify the current read state.  If the associated input
    // stream is closed simultaneous with reading, then it may not be
    // in the mInputList any more.
    aReadState.mSegment -= 1;

    for (uint32_t i = 0; i < mInputList.Length(); ++i) {
      // Skip the current read state structure since we modify it manually
      // before entering this loop.
      if (&mInputList[i]->ReadState() == &aReadState) {
        continue;
      }
      mInputList[i]->ReadState().mSegment -= 1;
    }

    // done with this segment
    mBuffer.DeleteFirstSegment();
    LOG(("III deleting first segment\n"));

    result = SegmentDeleted;
  }

  if (mWriteSegment < aReadState.mSegment) {
    // read cursor has hit the end of written data, so reset it
    MOZ_ASSERT(mWriteSegment == (aReadState.mSegment - 1));
    aReadState.mReadCursor = nullptr;
    aReadState.mReadLimit = nullptr;
    // also, the buffer is completely empty, so reset the write cursor
    if (mWriteSegment == -1) {
      mWriteCursor = nullptr;
      mWriteLimit = nullptr;
    }
  } else {
    // advance read cursor and limit to next buffer segment
    aReadState.mReadCursor = mBuffer.GetSegment(aReadState.mSegment);
    if (mWriteSegment == aReadState.mSegment) {
      aReadState.mReadLimit = mWriteCursor;
    } else {
      aReadState.mReadLimit = aReadState.mReadCursor + mBuffer.GetSegmentSize();
    }
  }

  return result;
}

void
nsPipe::DrainInputStream(nsPipeReadState& aReadState, nsPipeEvents& aEvents)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  // If a segment is actively being read in ReadSegments() for this input
  // stream, then we cannot drain the stream.  This can happen because
  // ReadSegments() does not hold the lock while copying from the buffer.
  // If we detect this condition, simply note that we need a drain once
  // the read completes and return immediately.
  if (aReadState.mActiveRead) {
    MOZ_ASSERT(!aReadState.mNeedDrain);
    aReadState.mNeedDrain = true;
    return;
  }

  aReadState.mAvailable = 0;

  SegmentChangeResult result = SegmentNotChanged;
  while(mWriteSegment >= aReadState.mSegment) {

    // If the last segment to free is still being written to, we're done
    // draining.  We can't free any more.
    if (ReadSegmentBeingWritten(aReadState)) {
      break;
    }

    if (AdvanceReadSegment(aReadState) == SegmentDeleted) {
      result = SegmentDeleted;
    }
  }

  // if we've free'd up a segment, notify output stream that pipe has
  // room for a new segment.
  if (result == SegmentDeleted &&
      mOutput.OnOutputWritable(aEvents) == NotifyMonitor) {
    mon.NotifyAll();
  }
}

bool
nsPipe::ReadSegmentBeingWritten(nsPipeReadState& aReadState)
{
  mReentrantMonitor.AssertCurrentThreadIn();
  bool beingWritten = mWriteSegment == aReadState.mSegment &&
                      mWriteLimit > mWriteCursor;
  NS_ASSERTION(!beingWritten || aReadState.mReadLimit == mWriteCursor,
               "unexpected state");
  return beingWritten;
}

nsresult
nsPipe::GetWriteSegment(char*& aSegment, uint32_t& aSegmentLen)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  // write cursor and limit may both be null indicating an empty buffer.
  if (mWriteCursor == mWriteLimit) {
    char* seg = mBuffer.AppendNewSegment();
    // pipe is full
    if (!seg) {
      return NS_BASE_STREAM_WOULD_BLOCK;
    }
    LOG(("OOO appended new segment\n"));
    mWriteCursor = seg;
    mWriteLimit = mWriteCursor + mBuffer.GetSegmentSize();
    ++mWriteSegment;
  }

  // make sure read cursor is initialized
  SetAllNullReadCursors();

  // check to see if we can roll-back our read and write cursors to the
  // beginning of the current/first segment.  this is purely an optimization.
  if (mWriteSegment == 0 && AllReadCursorsMatchWriteCursor()) {
    char* head = mBuffer.GetSegment(0);
    LOG(("OOO rolling back write cursor %u bytes\n", mWriteCursor - head));
    RollBackAllReadCursors(head);
    mWriteCursor = head;
  }

  aSegment    = mWriteCursor;
  aSegmentLen = mWriteLimit - mWriteCursor;
  return NS_OK;
}

void
nsPipe::AdvanceWriteCursor(uint32_t aBytesWritten)
{
  NS_ASSERTION(aBytesWritten, "don't call if no bytes written");

  nsPipeEvents events;
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    LOG(("OOO advancing write cursor by %u\n", aBytesWritten));

    char* newWriteCursor = mWriteCursor + aBytesWritten;
    NS_ASSERTION(newWriteCursor <= mWriteLimit, "write cursor exceeds limit");

    // update read limit if reading in the same segment
    UpdateAllReadCursors(newWriteCursor);

    mWriteCursor = newWriteCursor;

    ValidateAllReadCursors();

    // update the writable flag on the output stream
    if (mWriteCursor == mWriteLimit) {
      if (mBuffer.GetSize() >= mBuffer.GetMaxSize()) {
        mOutput.SetWritable(false);
      }
    }

    // notify input stream that pipe now contains additional data
    bool needNotify = false;
    for (uint32_t i = 0; i < mInputList.Length(); ++i) {
      if (mInputList[i]->OnInputReadable(aBytesWritten, events, mon)
          == NotifyMonitor) {
        needNotify = true;
      }
    }

    if (needNotify) {
      mon.NotifyAll();
    }
  }
}

void
nsPipe::OnInputStreamException(nsPipeInputStream* aStream, nsresult aReason)
{
  MOZ_ASSERT(NS_FAILED(aReason));

  nsPipeEvents events;
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    // Its possible to re-enter this method when we call OnPipeException() or
    // OnInputExection() below.  If there is a caller stuck in our synchronous
    // Wait() method, then they will get woken up with a failure code which
    // re-enters this method.  Therefore, gracefully handle unknown streams
    // here.

    // If we only have one stream open and it is the given stream, then shut
    // down the entire pipe.
    if (mInputList.Length() == 1) {
      if (mInputList[0] == aStream) {
        OnPipeException(aReason);
      }
      return;
    }

    // Otherwise just close the particular stream that hit an exception.
    for (uint32_t i = 0; i < mInputList.Length(); ++i) {
      if (mInputList[i] != aStream) {
        continue;
      }

      MonitorAction action = mInputList[i]->OnInputException(aReason, events,
                                                             mon);
      mInputList.RemoveElementAt(i);

      // Notify after element is removed in case we re-enter as a result.
      if (action == NotifyMonitor) {
        mon.NotifyAll();
      }

      return;
    }
  }
}

void
nsPipe::OnPipeException(nsresult aReason, bool aOutputOnly)
{
  LOG(("PPP nsPipe::OnPipeException [reason=%x output-only=%d]\n",
       aReason, aOutputOnly));

  nsPipeEvents events;
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    // if we've already hit an exception, then ignore this one.
    if (NS_FAILED(mStatus)) {
      return;
    }

    mStatus = aReason;

    bool needNotify = false;

    nsTArray<nsPipeInputStream*> tmpInputList;
    for (uint32_t i = 0; i < mInputList.Length(); ++i) {
      // an output-only exception applies to the input end if the pipe has
      // zero bytes available.
      if (aOutputOnly && mInputList[i]->Available()) {
        tmpInputList.AppendElement(mInputList[i]);
        continue;
      }

      if (mInputList[i]->OnInputException(aReason, events, mon)
          == NotifyMonitor) {
        needNotify = true;
      }
    }
    mInputList = tmpInputList;

    if (mOutput.OnOutputException(aReason, events) == NotifyMonitor) {
      needNotify = true;
    }

    // Notify after we have removed any input streams from mInputList
    if (needNotify) {
      mon.NotifyAll();
    }
  }
}

nsresult
nsPipe::CloneInputStream(nsPipeInputStream* aOriginal,
                         nsIInputStream** aCloneOut)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  RefPtr<nsPipeInputStream> ref = new nsPipeInputStream(*aOriginal);
  mInputList.AppendElement(ref);
  nsCOMPtr<nsIAsyncInputStream> downcast = ref.forget();
  downcast.forget(aCloneOut);
  return NS_OK;
}

uint32_t
nsPipe::CountSegmentReferences(int32_t aSegment)
{
  mReentrantMonitor.AssertCurrentThreadIn();
  uint32_t count = 0;
  for (uint32_t i = 0; i < mInputList.Length(); ++i) {
    if (aSegment >= mInputList[i]->ReadState().mSegment) {
      count += 1;
    }
  }
  return count;
}

void
nsPipe::SetAllNullReadCursors()
{
  mReentrantMonitor.AssertCurrentThreadIn();
  for (uint32_t i = 0; i < mInputList.Length(); ++i) {
    nsPipeReadState& readState = mInputList[i]->ReadState();
    if (!readState.mReadCursor) {
      NS_ASSERTION(mWriteSegment == readState.mSegment,
                   "unexpected null read cursor");
      readState.mReadCursor = readState.mReadLimit = mWriteCursor;
    }
  }
}

bool
nsPipe::AllReadCursorsMatchWriteCursor()
{
  mReentrantMonitor.AssertCurrentThreadIn();
  for (uint32_t i = 0; i < mInputList.Length(); ++i) {
    const nsPipeReadState& readState = mInputList[i]->ReadState();
    if (readState.mSegment != mWriteSegment ||
        readState.mReadCursor != mWriteCursor) {
      return false;
    }
  }
  return true;
}

void
nsPipe::RollBackAllReadCursors(char* aWriteCursor)
{
  mReentrantMonitor.AssertCurrentThreadIn();
  for (uint32_t i = 0; i < mInputList.Length(); ++i) {
    nsPipeReadState& readState = mInputList[i]->ReadState();
    MOZ_ASSERT(mWriteSegment == readState.mSegment);
    MOZ_ASSERT(mWriteCursor == readState.mReadCursor);
    MOZ_ASSERT(mWriteCursor == readState.mReadLimit);
    readState.mReadCursor = aWriteCursor;
    readState.mReadLimit = aWriteCursor;
  }
}

void
nsPipe::UpdateAllReadCursors(char* aWriteCursor)
{
  mReentrantMonitor.AssertCurrentThreadIn();
  for (uint32_t i = 0; i < mInputList.Length(); ++i) {
    nsPipeReadState& readState = mInputList[i]->ReadState();
    if (mWriteSegment == readState.mSegment &&
        readState.mReadLimit == mWriteCursor) {
      readState.mReadLimit = aWriteCursor;
    }
  }
}

void
nsPipe::ValidateAllReadCursors()
{
  mReentrantMonitor.AssertCurrentThreadIn();
  // The only way mReadCursor == mWriteCursor is if:
  //
  // - mReadCursor is at the start of a segment (which, based on how
  //   nsSegmentedBuffer works, means that this segment is the "first"
  //   segment)
  // - mWriteCursor points at the location past the end of the current
  //   write segment (so the current write filled the current write
  //   segment, so we've incremented mWriteCursor to point past the end
  //   of it)
  // - the segment to which data has just been written is located
  //   exactly one segment's worth of bytes before the first segment
  //   where mReadCursor is located
  //
  // Consequently, the byte immediately after the end of the current
  // write segment is the first byte of the first segment, so
  // mReadCursor == mWriteCursor.  (Another way to think about this is
  // to consider the buffer architecture diagram above, but consider it
  // with an arena allocator which allocates from the *end* of the
  // arena to the *beginning* of the arena.)
#ifdef DEBUG
  for (uint32_t i = 0; i < mInputList.Length(); ++i) {
    const nsPipeReadState& state = mInputList[i]->ReadState();
    NS_ASSERTION(state.mReadCursor != mWriteCursor ||
                 (mBuffer.GetSegment(state.mSegment) == state.mReadCursor &&
                  mWriteCursor == mWriteLimit),
                 "read cursor is bad");
  }
#endif
}

//-----------------------------------------------------------------------------
// nsPipeEvents methods:
//-----------------------------------------------------------------------------

nsPipeEvents::~nsPipeEvents()
{
  // dispatch any pending events

  for (uint32_t i = 0; i < mInputList.Length(); ++i) {
    mInputList[i].mCallback->OnInputStreamReady(mInputList[i].mStream);
  }
  mInputList.Clear();

  if (mOutputCallback) {
    mOutputCallback->OnOutputStreamReady(mOutputStream);
    mOutputCallback = nullptr;
    mOutputStream = nullptr;
  }
}

//-----------------------------------------------------------------------------
// nsPipeInputStream methods:
//-----------------------------------------------------------------------------

NS_IMPL_ADDREF(nsPipeInputStream);
NS_IMPL_RELEASE(nsPipeInputStream);

NS_INTERFACE_TABLE_HEAD(nsPipeInputStream)
  NS_INTERFACE_TABLE_BEGIN
    NS_INTERFACE_TABLE_ENTRY(nsPipeInputStream, nsIAsyncInputStream)
    NS_INTERFACE_TABLE_ENTRY(nsPipeInputStream, nsISeekableStream)
    NS_INTERFACE_TABLE_ENTRY(nsPipeInputStream, nsISearchableInputStream)
    NS_INTERFACE_TABLE_ENTRY(nsPipeInputStream, nsICloneableInputStream)
    NS_INTERFACE_TABLE_ENTRY(nsPipeInputStream, nsIBufferedInputStream)
    NS_INTERFACE_TABLE_ENTRY(nsPipeInputStream, nsIClassInfo)
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(nsPipeInputStream, nsIInputStream,
                                       nsIAsyncInputStream)
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(nsPipeInputStream, nsISupports,
                                       nsIAsyncInputStream)
  NS_INTERFACE_TABLE_END
NS_INTERFACE_TABLE_TAIL

NS_IMPL_CI_INTERFACE_GETTER(nsPipeInputStream,
                            nsIInputStream,
                            nsIAsyncInputStream,
                            nsISeekableStream,
                            nsISearchableInputStream,
                            nsICloneableInputStream,
                            nsIBufferedInputStream)

NS_IMPL_THREADSAFE_CI(nsPipeInputStream)

NS_IMETHODIMP
nsPipeInputStream::Init(nsIInputStream*, uint32_t)
{
  MOZ_CRASH("nsPipeInputStream should never be initialized with "
            "nsIBufferedInputStream::Init!\n");
}

uint32_t
nsPipeInputStream::Available()
{
  mPipe->mReentrantMonitor.AssertCurrentThreadIn();
  return mReadState.mAvailable;
}

nsresult
nsPipeInputStream::Wait()
{
  NS_ASSERTION(mBlocking, "wait on non-blocking pipe input stream");

  ReentrantMonitorAutoEnter mon(mPipe->mReentrantMonitor);

  while (NS_SUCCEEDED(Status(mon)) && (mReadState.mAvailable == 0)) {
    LOG(("III pipe input: waiting for data\n"));

    mBlocked = true;
    mon.Wait();
    mBlocked = false;

    LOG(("III pipe input: woke up [status=%x available=%u]\n",
         Status(mon), mReadState.mAvailable));
  }

  return Status(mon) == NS_BASE_STREAM_CLOSED ? NS_OK : Status(mon);
}

MonitorAction
nsPipeInputStream::OnInputReadable(uint32_t aBytesWritten,
                                   nsPipeEvents& aEvents,
                                   const ReentrantMonitorAutoEnter& ev)
{
  MonitorAction result = DoNotNotifyMonitor;

  mPipe->mReentrantMonitor.AssertCurrentThreadIn();
  mReadState.mAvailable += aBytesWritten;

  if (mCallback && !(mCallbackFlags & WAIT_CLOSURE_ONLY)) {
    aEvents.NotifyInputReady(this, mCallback);
    mCallback = 0;
    mCallbackFlags = 0;
  } else if (mBlocked) {
    result = NotifyMonitor;
  }

  return result;
}

MonitorAction
nsPipeInputStream::OnInputException(nsresult aReason, nsPipeEvents& aEvents,
                                    const ReentrantMonitorAutoEnter& ev)
{
  LOG(("nsPipeInputStream::OnInputException [this=%x reason=%x]\n",
       this, aReason));

  MonitorAction result = DoNotNotifyMonitor;

  NS_ASSERTION(NS_FAILED(aReason), "huh? successful exception");

  if (NS_SUCCEEDED(mInputStatus)) {
    mInputStatus = aReason;
  }

  // force count of available bytes to zero.
  mPipe->DrainInputStream(mReadState, aEvents);

  if (mCallback) {
    aEvents.NotifyInputReady(this, mCallback);
    mCallback = 0;
    mCallbackFlags = 0;
  } else if (mBlocked) {
    result = NotifyMonitor;
  }

  return result;
}

NS_IMETHODIMP
nsPipeInputStream::CloseWithStatus(nsresult aReason)
{
  LOG(("III CloseWithStatus [this=%x reason=%x]\n", this, aReason));

  ReentrantMonitorAutoEnter mon(mPipe->mReentrantMonitor);

  if (NS_FAILED(mInputStatus)) {
    return NS_OK;
  }

  if (NS_SUCCEEDED(aReason)) {
    aReason = NS_BASE_STREAM_CLOSED;
  }

  mPipe->OnInputStreamException(this, aReason);
  return NS_OK;
}

NS_IMETHODIMP
nsPipeInputStream::Close()
{
  return CloseWithStatus(NS_BASE_STREAM_CLOSED);
}

NS_IMETHODIMP
nsPipeInputStream::Available(uint64_t* aResult)
{
  // nsPipeInputStream supports under 4GB stream only
  ReentrantMonitorAutoEnter mon(mPipe->mReentrantMonitor);

  // return error if closed
  if (!mReadState.mAvailable && NS_FAILED(Status(mon))) {
    return Status(mon);
  }

  *aResult = (uint64_t)mReadState.mAvailable;
  return NS_OK;
}

NS_IMETHODIMP
nsPipeInputStream::ReadSegments(nsWriteSegmentFun aWriter,
                                void* aClosure,
                                uint32_t aCount,
                                uint32_t* aReadCount)
{
  LOG(("III ReadSegments [this=%x count=%u]\n", this, aCount));

  nsresult rv = NS_OK;

  *aReadCount = 0;
  while (aCount) {
    AutoReadSegment segment(mPipe, mReadState, aCount);
    rv = segment.Status();
    if (NS_FAILED(rv)) {
      // ignore this error if we've already read something.
      if (*aReadCount > 0) {
        rv = NS_OK;
        break;
      }
      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        // pipe is empty
        if (!mBlocking) {
          break;
        }
        // wait for some data to be written to the pipe
        rv = Wait();
        if (NS_SUCCEEDED(rv)) {
          continue;
        }
      }
      // ignore this error, just return.
      if (rv == NS_BASE_STREAM_CLOSED) {
        rv = NS_OK;
        break;
      }
      mPipe->OnInputStreamException(this, rv);
      break;
    }

    uint32_t writeCount;
    while (segment.Length()) {
      writeCount = 0;

      rv = aWriter(static_cast<nsIAsyncInputStream*>(this), aClosure,
                   segment.Data(), *aReadCount, segment.Length(), &writeCount);

      if (NS_FAILED(rv) || writeCount == 0) {
        aCount = 0;
        // any errors returned from the writer end here: do not
        // propagate to the caller of ReadSegments.
        rv = NS_OK;
        break;
      }

      NS_ASSERTION(writeCount <= segment.Length(), "wrote more than expected");
      segment.Advance(writeCount);
      aCount -= writeCount;
      *aReadCount += writeCount;
      mLogicalOffset += writeCount;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsPipeInputStream::Read(char* aToBuf, uint32_t aBufLen, uint32_t* aReadCount)
{
  return ReadSegments(NS_CopySegmentToBuffer, aToBuf, aBufLen, aReadCount);
}

NS_IMETHODIMP
nsPipeInputStream::IsNonBlocking(bool* aNonBlocking)
{
  *aNonBlocking = !mBlocking;
  return NS_OK;
}

NS_IMETHODIMP
nsPipeInputStream::AsyncWait(nsIInputStreamCallback* aCallback,
                             uint32_t aFlags,
                             uint32_t aRequestedCount,
                             nsIEventTarget* aTarget)
{
  LOG(("III AsyncWait [this=%x]\n", this));

  nsPipeEvents pipeEvents;
  {
    ReentrantMonitorAutoEnter mon(mPipe->mReentrantMonitor);

    // replace a pending callback
    mCallback = 0;
    mCallbackFlags = 0;

    if (!aCallback) {
      return NS_OK;
    }

    nsCOMPtr<nsIInputStreamCallback> proxy;
    if (aTarget) {
      proxy = NS_NewInputStreamReadyEvent(aCallback, aTarget);
      aCallback = proxy;
    }

    if (NS_FAILED(Status(mon)) ||
       (mReadState.mAvailable && !(aFlags & WAIT_CLOSURE_ONLY))) {
      // stream is already closed or readable; post event.
      pipeEvents.NotifyInputReady(this, aCallback);
    } else {
      // queue up callback object to be notified when data becomes available
      mCallback = aCallback;
      mCallbackFlags = aFlags;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPipeInputStream::Seek(int32_t aWhence, int64_t aOffset)
{
  NS_NOTREACHED("nsPipeInputStream::Seek");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPipeInputStream::Tell(int64_t* aOffset)
{
  ReentrantMonitorAutoEnter mon(mPipe->mReentrantMonitor);

  // return error if closed
  if (!mReadState.mAvailable && NS_FAILED(Status(mon))) {
    return Status(mon);
  }

  *aOffset = mLogicalOffset;
  return NS_OK;
}

NS_IMETHODIMP
nsPipeInputStream::SetEOF()
{
  NS_NOTREACHED("nsPipeInputStream::SetEOF");
  return NS_ERROR_NOT_IMPLEMENTED;
}

static bool strings_equal(bool aIgnoreCase,
                          const char* aS1, const char* aS2, uint32_t aLen)
{
  return aIgnoreCase
    ? !nsCRT::strncasecmp(aS1, aS2, aLen) : !nsCRT::strncmp(aS1, aS2, aLen);
}

NS_IMETHODIMP
nsPipeInputStream::Search(const char* aForString,
                          bool aIgnoreCase,
                          bool* aFound,
                          uint32_t* aOffsetSearchedTo)
{
  LOG(("III Search [for=%s ic=%u]\n", aForString, aIgnoreCase));

  ReentrantMonitorAutoEnter mon(mPipe->mReentrantMonitor);

  char* cursor1;
  char* limit1;
  uint32_t index = 0, offset = 0;
  uint32_t strLen = strlen(aForString);

  mPipe->PeekSegment(mReadState, 0, cursor1, limit1);
  if (cursor1 == limit1) {
    *aFound = false;
    *aOffsetSearchedTo = 0;
    LOG(("  result [aFound=%u offset=%u]\n", *aFound, *aOffsetSearchedTo));
    return NS_OK;
  }

  while (true) {
    uint32_t i, len1 = limit1 - cursor1;

    // check if the string is in the buffer segment
    for (i = 0; i < len1 - strLen + 1; i++) {
      if (strings_equal(aIgnoreCase, &cursor1[i], aForString, strLen)) {
        *aFound = true;
        *aOffsetSearchedTo = offset + i;
        LOG(("  result [aFound=%u offset=%u]\n", *aFound, *aOffsetSearchedTo));
        return NS_OK;
      }
    }

    // get the next segment
    char* cursor2;
    char* limit2;
    uint32_t len2;

    index++;
    offset += len1;

    mPipe->PeekSegment(mReadState, index, cursor2, limit2);
    if (cursor2 == limit2) {
      *aFound = false;
      *aOffsetSearchedTo = offset - strLen + 1;
      LOG(("  result [aFound=%u offset=%u]\n", *aFound, *aOffsetSearchedTo));
      return NS_OK;
    }
    len2 = limit2 - cursor2;

    // check if the string is straddling the next buffer segment
    uint32_t lim = XPCOM_MIN(strLen, len2 + 1);
    for (i = 0; i < lim; ++i) {
      uint32_t strPart1Len = strLen - i - 1;
      uint32_t strPart2Len = strLen - strPart1Len;
      const char* strPart2 = &aForString[strLen - strPart2Len];
      uint32_t bufSeg1Offset = len1 - strPart1Len;
      if (strings_equal(aIgnoreCase, &cursor1[bufSeg1Offset], aForString, strPart1Len) &&
          strings_equal(aIgnoreCase, cursor2, strPart2, strPart2Len)) {
        *aFound = true;
        *aOffsetSearchedTo = offset - strPart1Len;
        LOG(("  result [aFound=%u offset=%u]\n", *aFound, *aOffsetSearchedTo));
        return NS_OK;
      }
    }

    // finally continue with the next buffer
    cursor1 = cursor2;
    limit1 = limit2;
  }

  NS_NOTREACHED("can't get here");
  return NS_ERROR_UNEXPECTED;    // keep compiler happy
}

NS_IMETHODIMP
nsPipeInputStream::GetCloneable(bool* aCloneableOut)
{
  *aCloneableOut = true;
  return NS_OK;
}

NS_IMETHODIMP
nsPipeInputStream::Clone(nsIInputStream** aCloneOut)
{
  return mPipe->CloneInputStream(this, aCloneOut);
}

nsresult
nsPipeInputStream::Status(const ReentrantMonitorAutoEnter& ev) const
{
  return NS_FAILED(mInputStatus) ? mInputStatus : mPipe->mStatus;
}

nsresult
nsPipeInputStream::Status() const
{
  ReentrantMonitorAutoEnter mon(mPipe->mReentrantMonitor);
  return Status(mon);
}

nsPipeInputStream::~nsPipeInputStream()
{
  Close();
}

//-----------------------------------------------------------------------------
// nsPipeOutputStream methods:
//-----------------------------------------------------------------------------

NS_IMPL_QUERY_INTERFACE(nsPipeOutputStream,
                        nsIOutputStream,
                        nsIAsyncOutputStream,
                        nsIClassInfo)

NS_IMPL_CI_INTERFACE_GETTER(nsPipeOutputStream,
                            nsIOutputStream,
                            nsIAsyncOutputStream)

NS_IMPL_THREADSAFE_CI(nsPipeOutputStream)

nsresult
nsPipeOutputStream::Wait()
{
  NS_ASSERTION(mBlocking, "wait on non-blocking pipe output stream");

  ReentrantMonitorAutoEnter mon(mPipe->mReentrantMonitor);

  if (NS_SUCCEEDED(mPipe->mStatus) && !mWritable) {
    LOG(("OOO pipe output: waiting for space\n"));
    mBlocked = true;
    mon.Wait();
    mBlocked = false;
    LOG(("OOO pipe output: woke up [pipe-status=%x writable=%u]\n",
         mPipe->mStatus, mWritable));
  }

  return mPipe->mStatus == NS_BASE_STREAM_CLOSED ? NS_OK : mPipe->mStatus;
}

MonitorAction
nsPipeOutputStream::OnOutputWritable(nsPipeEvents& aEvents)
{
  MonitorAction result = DoNotNotifyMonitor;

  mWritable = true;

  if (mCallback && !(mCallbackFlags & WAIT_CLOSURE_ONLY)) {
    aEvents.NotifyOutputReady(this, mCallback);
    mCallback = 0;
    mCallbackFlags = 0;
  } else if (mBlocked) {
    result = NotifyMonitor;
  }

  return result;
}

MonitorAction
nsPipeOutputStream::OnOutputException(nsresult aReason, nsPipeEvents& aEvents)
{
  LOG(("nsPipeOutputStream::OnOutputException [this=%x reason=%x]\n",
       this, aReason));

  MonitorAction result = DoNotNotifyMonitor;

  NS_ASSERTION(NS_FAILED(aReason), "huh? successful exception");
  mWritable = false;

  if (mCallback) {
    aEvents.NotifyOutputReady(this, mCallback);
    mCallback = 0;
    mCallbackFlags = 0;
  } else if (mBlocked) {
    result = NotifyMonitor;
  }

  return result;
}


NS_IMETHODIMP_(MozExternalRefCountType)
nsPipeOutputStream::AddRef()
{
  ++mWriterRefCnt;
  return mPipe->AddRef();
}

NS_IMETHODIMP_(MozExternalRefCountType)
nsPipeOutputStream::Release()
{
  if (--mWriterRefCnt == 0) {
    Close();
  }
  return mPipe->Release();
}

NS_IMETHODIMP
nsPipeOutputStream::CloseWithStatus(nsresult aReason)
{
  LOG(("OOO CloseWithStatus [this=%x reason=%x]\n", this, aReason));

  if (NS_SUCCEEDED(aReason)) {
    aReason = NS_BASE_STREAM_CLOSED;
  }

  // input stream may remain open
  mPipe->OnPipeException(aReason, true);
  return NS_OK;
}

NS_IMETHODIMP
nsPipeOutputStream::Close()
{
  return CloseWithStatus(NS_BASE_STREAM_CLOSED);
}

NS_IMETHODIMP
nsPipeOutputStream::WriteSegments(nsReadSegmentFun aReader,
                                  void* aClosure,
                                  uint32_t aCount,
                                  uint32_t* aWriteCount)
{
  LOG(("OOO WriteSegments [this=%x count=%u]\n", this, aCount));

  nsresult rv = NS_OK;

  char* segment;
  uint32_t segmentLen;

  *aWriteCount = 0;
  while (aCount) {
    rv = mPipe->GetWriteSegment(segment, segmentLen);
    if (NS_FAILED(rv)) {
      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        // pipe is full
        if (!mBlocking) {
          // ignore this error if we've already written something
          if (*aWriteCount > 0) {
            rv = NS_OK;
          }
          break;
        }
        // wait for the pipe to have an empty segment.
        rv = Wait();
        if (NS_SUCCEEDED(rv)) {
          continue;
        }
      }
      mPipe->OnPipeException(rv);
      break;
    }

    // write no more than aCount
    if (segmentLen > aCount) {
      segmentLen = aCount;
    }

    uint32_t readCount, originalLen = segmentLen;
    while (segmentLen) {
      readCount = 0;

      rv = aReader(this, aClosure, segment, *aWriteCount, segmentLen, &readCount);

      if (NS_FAILED(rv) || readCount == 0) {
        aCount = 0;
        // any errors returned from the aReader end here: do not
        // propagate to the caller of WriteSegments.
        rv = NS_OK;
        break;
      }

      NS_ASSERTION(readCount <= segmentLen, "read more than expected");
      segment += readCount;
      segmentLen -= readCount;
      aCount -= readCount;
      *aWriteCount += readCount;
      mLogicalOffset += readCount;
    }

    if (segmentLen < originalLen) {
      mPipe->AdvanceWriteCursor(originalLen - segmentLen);
    }
  }

  return rv;
}

static NS_METHOD
nsReadFromRawBuffer(nsIOutputStream* aOutStr,
                    void* aClosure,
                    char* aToRawSegment,
                    uint32_t aOffset,
                    uint32_t aCount,
                    uint32_t* aReadCount)
{
  const char* fromBuf = (const char*)aClosure;
  memcpy(aToRawSegment, &fromBuf[aOffset], aCount);
  *aReadCount = aCount;
  return NS_OK;
}

NS_IMETHODIMP
nsPipeOutputStream::Write(const char* aFromBuf,
                          uint32_t aBufLen,
                          uint32_t* aWriteCount)
{
  return WriteSegments(nsReadFromRawBuffer, (void*)aFromBuf, aBufLen, aWriteCount);
}

NS_IMETHODIMP
nsPipeOutputStream::Flush(void)
{
  // nothing to do
  return NS_OK;
}

static NS_METHOD
nsReadFromInputStream(nsIOutputStream* aOutStr,
                      void* aClosure,
                      char* aToRawSegment,
                      uint32_t aOffset,
                      uint32_t aCount,
                      uint32_t* aReadCount)
{
  nsIInputStream* fromStream = (nsIInputStream*)aClosure;
  return fromStream->Read(aToRawSegment, aCount, aReadCount);
}

NS_IMETHODIMP
nsPipeOutputStream::WriteFrom(nsIInputStream* aFromStream,
                              uint32_t aCount,
                              uint32_t* aWriteCount)
{
  return WriteSegments(nsReadFromInputStream, aFromStream, aCount, aWriteCount);
}

NS_IMETHODIMP
nsPipeOutputStream::IsNonBlocking(bool* aNonBlocking)
{
  *aNonBlocking = !mBlocking;
  return NS_OK;
}

NS_IMETHODIMP
nsPipeOutputStream::AsyncWait(nsIOutputStreamCallback* aCallback,
                              uint32_t aFlags,
                              uint32_t aRequestedCount,
                              nsIEventTarget* aTarget)
{
  LOG(("OOO AsyncWait [this=%x]\n", this));

  nsPipeEvents pipeEvents;
  {
    ReentrantMonitorAutoEnter mon(mPipe->mReentrantMonitor);

    // replace a pending callback
    mCallback = 0;
    mCallbackFlags = 0;

    if (!aCallback) {
      return NS_OK;
    }

    nsCOMPtr<nsIOutputStreamCallback> proxy;
    if (aTarget) {
      proxy = NS_NewOutputStreamReadyEvent(aCallback, aTarget);
      aCallback = proxy;
    }

    if (NS_FAILED(mPipe->mStatus) ||
        (mWritable && !(aFlags & WAIT_CLOSURE_ONLY))) {
      // stream is already closed or writable; post event.
      pipeEvents.NotifyOutputReady(this, aCallback);
    } else {
      // queue up callback object to be notified when data becomes available
      mCallback = aCallback;
      mCallbackFlags = aFlags;
    }
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewPipe(nsIInputStream** aPipeIn,
           nsIOutputStream** aPipeOut,
           uint32_t aSegmentSize,
           uint32_t aMaxSize,
           bool aNonBlockingInput,
           bool aNonBlockingOutput)
{
  if (aSegmentSize == 0) {
    aSegmentSize = DEFAULT_SEGMENT_SIZE;
  }

  // Handle aMaxSize of UINT32_MAX as a special case
  uint32_t segmentCount;
  if (aMaxSize == UINT32_MAX) {
    segmentCount = UINT32_MAX;
  } else {
    segmentCount = aMaxSize / aSegmentSize;
  }

  nsIAsyncInputStream* in;
  nsIAsyncOutputStream* out;
  nsresult rv = NS_NewPipe2(&in, &out, aNonBlockingInput, aNonBlockingOutput,
                            aSegmentSize, segmentCount);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aPipeIn = in;
  *aPipeOut = out;
  return NS_OK;
}

nsresult
NS_NewPipe2(nsIAsyncInputStream** aPipeIn,
            nsIAsyncOutputStream** aPipeOut,
            bool aNonBlockingInput,
            bool aNonBlockingOutput,
            uint32_t aSegmentSize,
            uint32_t aSegmentCount)
{
  nsresult rv;

  nsPipe* pipe = new nsPipe();
  rv = pipe->Init(aNonBlockingInput,
                  aNonBlockingOutput,
                  aSegmentSize,
                  aSegmentCount);
  if (NS_FAILED(rv)) {
    NS_ADDREF(pipe);
    NS_RELEASE(pipe);
    return rv;
  }

  pipe->GetInputStream(aPipeIn);
  pipe->GetOutputStream(aPipeOut);
  return NS_OK;
}

nsresult
nsPipeConstructor(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }
  nsPipe* pipe = new nsPipe();
  NS_ADDREF(pipe);
  nsresult rv = pipe->QueryInterface(aIID, aResult);
  NS_RELEASE(pipe);
  return rv;
}

////////////////////////////////////////////////////////////////////////////////
