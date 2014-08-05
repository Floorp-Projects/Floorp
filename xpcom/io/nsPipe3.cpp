/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsIPipe.h"
#include "nsIEventTarget.h"
#include "nsISeekableStream.h"
#include "nsIProgrammingLanguage.h"
#include "nsSegmentedBuffer.h"
#include "nsStreamUtils.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "prlog.h"
#include "nsIClassInfoImpl.h"
#include "nsAlgorithm.h"
#include "nsMemory.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"

using namespace mozilla;

#ifdef LOG
#undef LOG
#endif
#if defined(PR_LOGGING)
//
// set NSPR_LOG_MODULES=nsPipe:5
//
static PRLogModuleInfo*
GetPipeLog()
{
  static PRLogModuleInfo* sLog;
  if (!sLog) {
    sLog = PR_NewLogModule("nsPipe");
  }
  return sLog;
}
#define LOG(args) PR_LOG(GetPipeLog(), PR_LOG_DEBUG, args)
#else
#define LOG(args)
#endif

#define DEFAULT_SEGMENT_SIZE  4096
#define DEFAULT_SEGMENT_COUNT 16

class nsPipe;
class nsPipeEvents;
class nsPipeInputStream;
class nsPipeOutputStream;

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
    NS_ASSERTION(!mInputCallback, "already have an input event");
    mInputStream = aStream;
    mInputCallback = aCallback;
  }

  inline void NotifyOutputReady(nsIAsyncOutputStream* aStream,
                                nsIOutputStreamCallback* aCallback)
  {
    NS_ASSERTION(!mOutputCallback, "already have an output event");
    mOutputStream = aStream;
    mOutputCallback = aCallback;
  }

private:
  nsCOMPtr<nsIAsyncInputStream>     mInputStream;
  nsCOMPtr<nsIInputStreamCallback>  mInputCallback;
  nsCOMPtr<nsIAsyncOutputStream>    mOutputStream;
  nsCOMPtr<nsIOutputStreamCallback> mOutputCallback;
};

//-----------------------------------------------------------------------------

// the input end of a pipe (allocated as a member of the pipe).
class nsPipeInputStream
  : public nsIAsyncInputStream
  , public nsISeekableStream
  , public nsISearchableInputStream
  , public nsIClassInfo
{
public:
  // since this class will be allocated as a member of the pipe, we do not
  // need our own ref count.  instead, we share the lifetime (the ref count)
  // of the entire pipe.  this macro is just convenience since it does not
  // declare a mRefCount variable; however, don't let the name fool you...
  // we are not inheriting from nsPipe ;-)
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM
  NS_DECL_NSISEARCHABLEINPUTSTREAM
  NS_DECL_NSICLASSINFO

  explicit nsPipeInputStream(nsPipe* aPipe)
    : mPipe(aPipe)
    , mReaderRefCnt(0)
    , mLogicalOffset(0)
    , mBlocking(true)
    , mBlocked(false)
    , mAvailable(0)
    , mCallbackFlags(0)
  { }

  nsresult Fill();
  void SetNonBlocking(bool aNonBlocking)
  {
    mBlocking = !aNonBlocking;
  }

  uint32_t Available()
  {
    return mAvailable;
  }
  void ReduceAvailable(uint32_t aAvail)
  {
    mAvailable -= aAvail;
  }

  // synchronously wait for the pipe to become readable.
  nsresult Wait();

  // these functions return true to indicate that the pipe's monitor should
  // be notified, to wake up a blocked reader if any.
  bool OnInputReadable(uint32_t aBytesWritten, nsPipeEvents&);
  bool OnInputException(nsresult, nsPipeEvents&);

private:
  nsPipe*                        mPipe;

  // separate refcnt so that we know when to close the consumer
  mozilla::ThreadSafeAutoRefCnt  mReaderRefCnt;
  int64_t                        mLogicalOffset;
  bool                           mBlocking;

  // these variables can only be accessed while inside the pipe's monitor
  bool                           mBlocked;
  uint32_t                       mAvailable;
  nsCOMPtr<nsIInputStreamCallback> mCallback;
  uint32_t                       mCallbackFlags;
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

  // these functions return true to indicate that the pipe's monitor should
  // be notified, to wake up a blocked writer if any.
  bool OnOutputWritable(nsPipeEvents&);
  bool OnOutputException(nsresult, nsPipeEvents&);

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

class nsPipe MOZ_FINAL : public nsIPipe
{
public:
  friend class nsPipeInputStream;
  friend class nsPipeOutputStream;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPIPE

  // nsPipe methods:
  nsPipe();

private:
  ~nsPipe();

public:
  //
  // methods below may only be called while inside the pipe's monitor
  //

  void PeekSegment(uint32_t aIndex, char*& aCursor, char*& aLimit);

  //
  // methods below may be called while outside the pipe's monitor
  //

  nsresult GetReadSegment(const char*& aSegment, uint32_t& aSegmentLen);
  void     AdvanceReadCursor(uint32_t aCount);

  nsresult GetWriteSegment(char*& aSegment, uint32_t& aSegmentLen);
  void     AdvanceWriteCursor(uint32_t aCount);

  void     OnPipeException(nsresult aReason, bool aOutputOnly = false);

protected:
  // We can't inherit from both nsIInputStream and nsIOutputStream
  // because they collide on their Close method. Consequently we nest their
  // implementations to avoid the extra object allocation.
  nsPipeInputStream   mInput;
  nsPipeOutputStream  mOutput;

  ReentrantMonitor    mReentrantMonitor;
  nsSegmentedBuffer   mBuffer;

  char*               mReadCursor;
  char*               mReadLimit;

  int32_t             mWriteSegment;
  char*               mWriteCursor;
  char*               mWriteLimit;

  nsresult            mStatus;
  bool                mInited;
};

//
// NOTES on buffer architecture:
//
//       +-----------------+ - - mBuffer.GetSegment(0)
//       |                 |
//       + - - - - - - - - + - - mReadCursor
//       |/////////////////|
//       |/////////////////|
//       |/////////////////|
//       |/////////////////|
//       +-----------------+ - - mReadLimit
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
  : mInput(MOZ_THIS_IN_INITIALIZER_LIST())
  , mOutput(MOZ_THIS_IN_INITIALIZER_LIST())
  , mReentrantMonitor("nsPipe.mReentrantMonitor")
  , mReadCursor(nullptr)
  , mReadLimit(nullptr)
  , mWriteSegment(-1)
  , mWriteCursor(nullptr)
  , mWriteLimit(nullptr)
  , mStatus(NS_OK)
  , mInited(false)
{
}

nsPipe::~nsPipe()
{
}

NS_IMPL_ISUPPORTS(nsPipe, nsIPipe)

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

  mInput.SetNonBlocking(aNonBlockingIn);
  mOutput.SetNonBlocking(aNonBlockingOut);
  return NS_OK;
}

NS_IMETHODIMP
nsPipe::GetInputStream(nsIAsyncInputStream** aInputStream)
{
  NS_ADDREF(*aInputStream = &mInput);
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
nsPipe::PeekSegment(uint32_t aIndex, char*& aCursor, char*& aLimit)
{
  if (aIndex == 0) {
    NS_ASSERTION(!mReadCursor || mBuffer.GetSegmentCount(), "unexpected state");
    aCursor = mReadCursor;
    aLimit = mReadLimit;
  } else {
    uint32_t numSegments = mBuffer.GetSegmentCount();
    if (aIndex >= numSegments) {
      aCursor = aLimit = nullptr;
    } else {
      aCursor = mBuffer.GetSegment(aIndex);
      if (mWriteSegment == (int32_t)aIndex) {
        aLimit = mWriteCursor;
      } else {
        aLimit = aCursor + mBuffer.GetSegmentSize();
      }
    }
  }
}

nsresult
nsPipe::GetReadSegment(const char*& aSegment, uint32_t& aSegmentLen)
{
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (mReadCursor == mReadLimit) {
    return NS_FAILED(mStatus) ? mStatus : NS_BASE_STREAM_WOULD_BLOCK;
  }

  aSegment    = mReadCursor;
  aSegmentLen = mReadLimit - mReadCursor;
  return NS_OK;
}

void
nsPipe::AdvanceReadCursor(uint32_t aBytesRead)
{
  NS_ASSERTION(aBytesRead, "don't call if no bytes read");

  nsPipeEvents events;
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    LOG(("III advancing read cursor by %u\n", aBytesRead));
    NS_ASSERTION(aBytesRead <= mBuffer.GetSegmentSize(), "read too much");

    mReadCursor += aBytesRead;
    NS_ASSERTION(mReadCursor <= mReadLimit, "read cursor exceeds limit");

    mInput.ReduceAvailable(aBytesRead);

    if (mReadCursor == mReadLimit) {
      // we've reached the limit of how much we can read from this segment.
      // if at the end of this segment, then we must discard this segment.

      // if still writing in this segment then bail because we're not done
      // with the segment and have to wait for now...
      if (mWriteSegment == 0 && mWriteLimit > mWriteCursor) {
        NS_ASSERTION(mReadLimit == mWriteCursor, "unexpected state");
        return;
      }

      // shift write segment index (-1 indicates an empty buffer).
      --mWriteSegment;

      // done with this segment
      mBuffer.DeleteFirstSegment();
      LOG(("III deleting first segment\n"));

      if (mWriteSegment == -1) {
        // buffer is completely empty
        mReadCursor = nullptr;
        mReadLimit = nullptr;
        mWriteCursor = nullptr;
        mWriteLimit = nullptr;
      } else {
        // advance read cursor and limit to next buffer segment
        mReadCursor = mBuffer.GetSegment(0);
        if (mWriteSegment == 0) {
          mReadLimit = mWriteCursor;
        } else {
          mReadLimit = mReadCursor + mBuffer.GetSegmentSize();
        }
      }

      // we've free'd up a segment, so notify output stream that pipe has
      // room for a new segment.
      if (mOutput.OnOutputWritable(events)) {
        mon.Notify();
      }
    }
  }
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
  if (!mReadCursor) {
    NS_ASSERTION(mWriteSegment == 0, "unexpected null read cursor");
    mReadCursor = mReadLimit = mWriteCursor;
  }

  // check to see if we can roll-back our read and write cursors to the
  // beginning of the current/first segment.  this is purely an optimization.
  if (mReadCursor == mWriteCursor && mWriteSegment == 0) {
    char* head = mBuffer.GetSegment(0);
    LOG(("OOO rolling back write cursor %u bytes\n", mWriteCursor - head));
    mWriteCursor = mReadCursor = mReadLimit = head;
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
    if (mWriteSegment == 0 && mReadLimit == mWriteCursor) {
      mReadLimit = newWriteCursor;
    }

    mWriteCursor = newWriteCursor;

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
    NS_ASSERTION(mReadCursor != mWriteCursor ||
                 (mBuffer.GetSegment(0) == mReadCursor &&
                  mWriteCursor == mWriteLimit),
                 "read cursor is bad");

    // update the writable flag on the output stream
    if (mWriteCursor == mWriteLimit) {
      if (mBuffer.GetSize() >= mBuffer.GetMaxSize()) {
        mOutput.SetWritable(false);
      }
    }

    // notify input stream that pipe now contains additional data
    if (mInput.OnInputReadable(aBytesWritten, events)) {
      mon.Notify();
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

    // an output-only exception applies to the input end if the pipe has
    // zero bytes available.
    if (aOutputOnly && !mInput.Available()) {
      aOutputOnly = false;
    }

    if (!aOutputOnly)
      if (mInput.OnInputException(aReason, events)) {
        mon.Notify();
      }

    if (mOutput.OnOutputException(aReason, events)) {
      mon.Notify();
    }
  }
}

//-----------------------------------------------------------------------------
// nsPipeEvents methods:
//-----------------------------------------------------------------------------

nsPipeEvents::~nsPipeEvents()
{
  // dispatch any pending events

  if (mInputCallback) {
    mInputCallback->OnInputStreamReady(mInputStream);
    mInputCallback = 0;
    mInputStream = 0;
  }
  if (mOutputCallback) {
    mOutputCallback->OnOutputStreamReady(mOutputStream);
    mOutputCallback = 0;
    mOutputStream = 0;
  }
}

//-----------------------------------------------------------------------------
// nsPipeInputStream methods:
//-----------------------------------------------------------------------------

NS_IMPL_QUERY_INTERFACE(nsPipeInputStream,
                        nsIInputStream,
                        nsIAsyncInputStream,
                        nsISeekableStream,
                        nsISearchableInputStream,
                        nsIClassInfo)

NS_IMPL_CI_INTERFACE_GETTER(nsPipeInputStream,
                            nsIInputStream,
                            nsIAsyncInputStream,
                            nsISeekableStream,
                            nsISearchableInputStream)

NS_IMPL_THREADSAFE_CI(nsPipeInputStream)

nsresult
nsPipeInputStream::Wait()
{
  NS_ASSERTION(mBlocking, "wait on non-blocking pipe input stream");

  ReentrantMonitorAutoEnter mon(mPipe->mReentrantMonitor);

  while (NS_SUCCEEDED(mPipe->mStatus) && (mAvailable == 0)) {
    LOG(("III pipe input: waiting for data\n"));

    mBlocked = true;
    mon.Wait();
    mBlocked = false;

    LOG(("III pipe input: woke up [pipe-status=%x available=%u]\n",
         mPipe->mStatus, mAvailable));
  }

  return mPipe->mStatus == NS_BASE_STREAM_CLOSED ? NS_OK : mPipe->mStatus;
}

bool
nsPipeInputStream::OnInputReadable(uint32_t aBytesWritten, nsPipeEvents& aEvents)
{
  bool result = false;

  mAvailable += aBytesWritten;

  if (mCallback && !(mCallbackFlags & WAIT_CLOSURE_ONLY)) {
    aEvents.NotifyInputReady(this, mCallback);
    mCallback = 0;
    mCallbackFlags = 0;
  } else if (mBlocked) {
    result = true;
  }

  return result;
}

bool
nsPipeInputStream::OnInputException(nsresult aReason, nsPipeEvents& aEvents)
{
  LOG(("nsPipeInputStream::OnInputException [this=%x reason=%x]\n",
       this, aReason));

  bool result = false;

  NS_ASSERTION(NS_FAILED(aReason), "huh? successful exception");

  // force count of available bytes to zero.
  mAvailable = 0;

  if (mCallback) {
    aEvents.NotifyInputReady(this, mCallback);
    mCallback = 0;
    mCallbackFlags = 0;
  } else if (mBlocked) {
    result = true;
  }

  return result;
}

NS_IMETHODIMP_(MozExternalRefCountType)
nsPipeInputStream::AddRef(void)
{
  ++mReaderRefCnt;
  return mPipe->AddRef();
}

NS_IMETHODIMP_(MozExternalRefCountType)
nsPipeInputStream::Release(void)
{
  if (--mReaderRefCnt == 0) {
    Close();
  }
  return mPipe->Release();
}

NS_IMETHODIMP
nsPipeInputStream::CloseWithStatus(nsresult aReason)
{
  LOG(("III CloseWithStatus [this=%x reason=%x]\n", this, aReason));

  if (NS_SUCCEEDED(aReason)) {
    aReason = NS_BASE_STREAM_CLOSED;
  }

  mPipe->OnPipeException(aReason);
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

  // return error if pipe closed
  if (!mAvailable && NS_FAILED(mPipe->mStatus)) {
    return mPipe->mStatus;
  }

  *aResult = (uint64_t)mAvailable;
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

  const char* segment;
  uint32_t segmentLen;

  *aReadCount = 0;
  while (aCount) {
    rv = mPipe->GetReadSegment(segment, segmentLen);
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
      mPipe->OnPipeException(rv);
      break;
    }

    // read no more than aCount
    if (segmentLen > aCount) {
      segmentLen = aCount;
    }

    uint32_t writeCount, originalLen = segmentLen;
    while (segmentLen) {
      writeCount = 0;

      rv = aWriter(this, aClosure, segment, *aReadCount, segmentLen, &writeCount);

      if (NS_FAILED(rv) || writeCount == 0) {
        aCount = 0;
        // any errors returned from the writer end here: do not
        // propagate to the caller of ReadSegments.
        rv = NS_OK;
        break;
      }

      NS_ASSERTION(writeCount <= segmentLen, "wrote more than expected");
      segment += writeCount;
      segmentLen -= writeCount;
      aCount -= writeCount;
      *aReadCount += writeCount;
      mLogicalOffset += writeCount;
    }

    if (segmentLen < originalLen) {
      mPipe->AdvanceReadCursor(originalLen - segmentLen);
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

    if (NS_FAILED(mPipe->mStatus) ||
        (mAvailable && !(aFlags & WAIT_CLOSURE_ONLY))) {
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

  // return error if pipe closed
  if (!mAvailable && NS_FAILED(mPipe->mStatus)) {
    return mPipe->mStatus;
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

#define COMPARE(s1, s2, i)                                               \
  (aIgnoreCase                                                           \
   ? nsCRT::strncasecmp((const char *)s1, (const char *)s2, (uint32_t)i) \
   : nsCRT::strncmp((const char *)s1, (const char *)s2, (uint32_t)i))

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

  mPipe->PeekSegment(0, cursor1, limit1);
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
      if (COMPARE(&cursor1[i], aForString, strLen) == 0) {
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

    mPipe->PeekSegment(index, cursor2, limit2);
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
      if (COMPARE(&cursor1[bufSeg1Offset], aForString, strPart1Len) == 0 &&
          COMPARE(cursor2, strPart2, strPart2Len) == 0) {
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

bool
nsPipeOutputStream::OnOutputWritable(nsPipeEvents& aEvents)
{
  bool result = false;

  mWritable = true;

  if (mCallback && !(mCallbackFlags & WAIT_CLOSURE_ONLY)) {
    aEvents.NotifyOutputReady(this, mCallback);
    mCallback = 0;
    mCallbackFlags = 0;
  } else if (mBlocked) {
    result = true;
  }

  return result;
}

bool
nsPipeOutputStream::OnOutputException(nsresult aReason, nsPipeEvents& aEvents)
{
  LOG(("nsPipeOutputStream::OnOutputException [this=%x reason=%x]\n",
       this, aReason));

  bool result = false;

  NS_ASSERTION(NS_FAILED(aReason), "huh? successful exception");
  mWritable = false;

  if (mCallback) {
    aEvents.NotifyOutputReady(this, mCallback);
    mCallback = 0;
    mCallbackFlags = 0;
  } else if (mBlocked) {
    result = true;
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
  if (!pipe) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

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
  if (!pipe) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(pipe);
  nsresult rv = pipe->QueryInterface(aIID, aResult);
  NS_RELEASE(pipe);
  return rv;
}

////////////////////////////////////////////////////////////////////////////////
