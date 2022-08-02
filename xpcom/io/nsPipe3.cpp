/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include "mozilla/Attributes.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsIBufferedStreams.h"
#include "nsICloneableInputStream.h"
#include "nsIPipe.h"
#include "nsIEventTarget.h"
#include "nsITellableStream.h"
#include "mozilla/RefPtr.h"
#include "nsSegmentedBuffer.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "mozilla/Logging.h"
#include "nsIClassInfoImpl.h"
#include "nsAlgorithm.h"
#include "nsMemory.h"
#include "nsPipe.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIInputStreamPriority.h"
#include "nsThreadUtils.h"

using namespace mozilla;

#ifdef LOG
#  undef LOG
#endif
//
// set MOZ_LOG=nsPipe:5
//
static LazyLogModule sPipeLog("nsPipe");
#define LOG(args) MOZ_LOG(sPipeLog, mozilla::LogLevel::Debug, args)

#define DEFAULT_SEGMENT_SIZE 4096
#define DEFAULT_SEGMENT_COUNT 16

class nsPipe;
class nsPipeEvents;
class nsPipeInputStream;
class nsPipeOutputStream;
class AutoReadSegment;

namespace {

enum MonitorAction { DoNotNotifyMonitor, NotifyMonitor };

enum SegmentChangeResult { SegmentNotChanged, SegmentAdvanceBufferRead };

}  // namespace

//-----------------------------------------------------------------------------

class CallbackHolder {
 public:
  CallbackHolder() = default;
  MOZ_IMPLICIT CallbackHolder(std::nullptr_t) {}

  CallbackHolder(nsIAsyncInputStream* aStream,
                 nsIInputStreamCallback* aCallback, uint32_t aFlags,
                 nsIEventTarget* aEventTarget)
      : mRunnable(aCallback ? NS_NewCancelableRunnableFunction(
                                  "nsPipeInputStream AsyncWait Callback",
                                  [stream = nsCOMPtr{aStream},
                                   callback = nsCOMPtr{aCallback}]() {
                                    callback->OnInputStreamReady(stream);
                                  })
                            : nullptr),
        mEventTarget(aEventTarget),
        mFlags(aFlags) {}

  CallbackHolder(nsIAsyncOutputStream* aStream,
                 nsIOutputStreamCallback* aCallback, uint32_t aFlags,
                 nsIEventTarget* aEventTarget)
      : mRunnable(aCallback ? NS_NewCancelableRunnableFunction(
                                  "nsPipeOutputStream AsyncWait Callback",
                                  [stream = nsCOMPtr{aStream},
                                   callback = nsCOMPtr{aCallback}]() {
                                    callback->OnOutputStreamReady(stream);
                                  })
                            : nullptr),
        mEventTarget(aEventTarget),
        mFlags(aFlags) {}

  CallbackHolder(const CallbackHolder&) = delete;
  CallbackHolder(CallbackHolder&&) = default;
  CallbackHolder& operator=(const CallbackHolder&) = delete;
  CallbackHolder& operator=(CallbackHolder&&) = default;

  CallbackHolder& operator=(std::nullptr_t) {
    mRunnable = nullptr;
    mEventTarget = nullptr;
    mFlags = 0;
    return *this;
  }

  MOZ_IMPLICIT operator bool() const { return mRunnable; }

  uint32_t Flags() const {
    MOZ_ASSERT(mRunnable, "Should only be called when a callback is present");
    return mFlags;
  }

  void Notify() {
    nsCOMPtr<nsIRunnable> runnable = mRunnable.forget();
    nsCOMPtr<nsIEventTarget> eventTarget = mEventTarget.forget();
    if (runnable) {
      if (eventTarget) {
        eventTarget->Dispatch(runnable.forget());
      } else {
        runnable->Run();
      }
    }
  }

 private:
  nsCOMPtr<nsIRunnable> mRunnable;
  nsCOMPtr<nsIEventTarget> mEventTarget;
  uint32_t mFlags = 0;
};

//-----------------------------------------------------------------------------

// this class is used to delay notifications until the end of a particular
// scope.  it helps avoid the complexity of issuing callbacks while inside
// a critical section.
class nsPipeEvents {
 public:
  nsPipeEvents() = default;
  ~nsPipeEvents();

  inline void NotifyReady(CallbackHolder aCallback) {
    mCallbacks.AppendElement(std::move(aCallback));
  }

 private:
  nsTArray<CallbackHolder> mCallbacks;
};

//-----------------------------------------------------------------------------

// This class is used to maintain input stream state.  Its broken out from the
// nsPipeInputStream class because generally the nsPipe should be modifying
// this state and not the input stream itself.
struct nsPipeReadState {
  nsPipeReadState()
      : mReadCursor(nullptr),
        mReadLimit(nullptr),
        mSegment(0),
        mAvailable(0),
        mActiveRead(false),
        mNeedDrain(false) {}

  // All members of this type are guarded by the pipe monitor, however it cannot
  // be named from this type, so the less-reliable MOZ_GUARDED_VAR is used
  // instead. In the future it would be nice to avoid this, especially as
  // MOZ_GUARDED_VAR is deprecated.
  char* mReadCursor MOZ_GUARDED_VAR;
  char* mReadLimit MOZ_GUARDED_VAR;
  int32_t mSegment MOZ_GUARDED_VAR;
  uint32_t mAvailable MOZ_GUARDED_VAR;

  // This flag is managed using the AutoReadSegment RAII stack class.
  bool mActiveRead MOZ_GUARDED_VAR;

  // Set to indicate that the input stream has closed and should be drained,
  // but that drain has been delayed due to an active read.  When the read
  // completes, this flag indicate the drain should then be performed.
  bool mNeedDrain MOZ_GUARDED_VAR;
};

//-----------------------------------------------------------------------------

// an input end of a pipe (maintained as a list of refs within the pipe)
class nsPipeInputStream final : public nsIAsyncInputStream,
                                public nsITellableStream,
                                public nsISearchableInputStream,
                                public nsICloneableInputStream,
                                public nsIClassInfo,
                                public nsIBufferedInputStream,
                                public nsIInputStreamPriority {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSIASYNCINPUTSTREAM
  NS_DECL_NSITELLABLESTREAM
  NS_DECL_NSISEARCHABLEINPUTSTREAM
  NS_DECL_NSICLONEABLEINPUTSTREAM
  NS_DECL_NSICLASSINFO
  NS_DECL_NSIBUFFEREDINPUTSTREAM
  NS_DECL_NSIINPUTSTREAMPRIORITY

  explicit nsPipeInputStream(nsPipe* aPipe)
      : mPipe(aPipe),
        mLogicalOffset(0),
        mInputStatus(NS_OK),
        mBlocking(true),
        mBlocked(false),
        mPriority(nsIRunnablePriority::PRIORITY_NORMAL) {}

  nsPipeInputStream(const nsPipeInputStream& aOther)
      : mPipe(aOther.mPipe),
        mLogicalOffset(aOther.mLogicalOffset),
        mInputStatus(aOther.mInputStatus),
        mBlocking(aOther.mBlocking),
        mBlocked(false),
        mReadState(aOther.mReadState),
        mPriority(nsIRunnablePriority::PRIORITY_NORMAL) {}

  void SetNonBlocking(bool aNonBlocking) { mBlocking = !aNonBlocking; }

  uint32_t Available() MOZ_REQUIRES(Monitor());

  // synchronously wait for the pipe to become readable.
  nsresult Wait();

  // These two don't acquire the monitor themselves.  Instead they
  // expect their caller to have done so and to pass the monitor as
  // evidence.
  MonitorAction OnInputReadable(uint32_t aBytesWritten, nsPipeEvents&,
                                const ReentrantMonitorAutoEnter& ev)
      MOZ_REQUIRES(Monitor());
  MonitorAction OnInputException(nsresult, nsPipeEvents&,
                                 const ReentrantMonitorAutoEnter& ev)
      MOZ_REQUIRES(Monitor());

  nsPipeReadState& ReadState() { return mReadState; }

  const nsPipeReadState& ReadState() const { return mReadState; }

  nsresult Status() const;

  // A version of Status() that doesn't acquire the monitor.
  nsresult Status(const ReentrantMonitorAutoEnter& ev) const
      MOZ_REQUIRES(Monitor());

  // The status of this input stream, ignoring the status of the underlying
  // monitor. If this status is errored, the input stream has either already
  // been removed from the pipe, or will be removed from the pipe shortly.
  nsresult InputStatus(const ReentrantMonitorAutoEnter&) const
      MOZ_REQUIRES(Monitor()) {
    return mInputStatus;
  }

  ReentrantMonitor& Monitor() const;

 private:
  virtual ~nsPipeInputStream();

  RefPtr<nsPipe> mPipe;

  int64_t mLogicalOffset;
  // Individual input streams can be closed without effecting the rest of the
  // pipe.  So track individual input stream status separately.  |mInputStatus|
  // is protected by |mPipe->mReentrantMonitor|.
  nsresult mInputStatus MOZ_GUARDED_BY(Monitor());
  bool mBlocking;

  // these variables can only be accessed while inside the pipe's monitor
  bool mBlocked MOZ_GUARDED_BY(Monitor());
  CallbackHolder mCallback MOZ_GUARDED_BY(Monitor());

  // requires pipe's monitor to access members; usually treat as an opaque token
  // to pass to nsPipe
  nsPipeReadState mReadState;
  Atomic<uint32_t, Relaxed> mPriority;
};

//-----------------------------------------------------------------------------

// the output end of a pipe (allocated as a member of the pipe).
class nsPipeOutputStream : public nsIAsyncOutputStream, public nsIClassInfo {
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
      : mPipe(aPipe),
        mWriterRefCnt(0),
        mLogicalOffset(0),
        mBlocking(true),
        mBlocked(false),
        mWritable(true) {}

  void SetNonBlocking(bool aNonBlocking) { mBlocking = !aNonBlocking; }
  void SetWritable(bool aWritable) MOZ_REQUIRES(Monitor()) {
    mWritable = aWritable;
  }

  // synchronously wait for the pipe to become writable.
  nsresult Wait();

  MonitorAction OnOutputWritable(nsPipeEvents&) MOZ_REQUIRES(Monitor());
  MonitorAction OnOutputException(nsresult, nsPipeEvents&)
      MOZ_REQUIRES(Monitor());

  ReentrantMonitor& Monitor() const;

 private:
  nsPipe* mPipe;

  // separate refcnt so that we know when to close the producer
  ThreadSafeAutoRefCnt mWriterRefCnt;
  int64_t mLogicalOffset;
  bool mBlocking;

  // these variables can only be accessed while inside the pipe's monitor
  bool mBlocked MOZ_GUARDED_BY(Monitor());
  bool mWritable MOZ_GUARDED_BY(Monitor());
  CallbackHolder mCallback MOZ_GUARDED_BY(Monitor());
};

//-----------------------------------------------------------------------------

class nsPipe final {
 public:
  friend class nsPipeInputStream;
  friend class nsPipeOutputStream;
  friend class AutoReadSegment;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(nsPipe)

  // public constructor
  friend nsresult NS_NewPipe2(nsIAsyncInputStream**, nsIAsyncOutputStream**,
                              bool, bool, uint32_t, uint32_t);

 private:
  nsPipe(uint32_t aSegmentSize, uint32_t aSegmentCount);
  ~nsPipe();

  //
  // Methods below may only be called while inside the pipe's monitor.  Some
  // of these methods require passing a ReentrantMonitorAutoEnter to prove the
  // monitor is held.
  //

  void PeekSegment(const nsPipeReadState& aReadState, uint32_t aIndex,
                   char*& aCursor, char*& aLimit)
      MOZ_REQUIRES(mReentrantMonitor);
  SegmentChangeResult AdvanceReadSegment(nsPipeReadState& aReadState,
                                         const ReentrantMonitorAutoEnter& ev)
      MOZ_REQUIRES(mReentrantMonitor);
  bool ReadSegmentBeingWritten(nsPipeReadState& aReadState)
      MOZ_REQUIRES(mReentrantMonitor);
  uint32_t CountSegmentReferences(int32_t aSegment)
      MOZ_REQUIRES(mReentrantMonitor);
  void SetAllNullReadCursors() MOZ_REQUIRES(mReentrantMonitor);
  bool AllReadCursorsMatchWriteCursor() MOZ_REQUIRES(mReentrantMonitor);
  void RollBackAllReadCursors(char* aWriteCursor)
      MOZ_REQUIRES(mReentrantMonitor);
  void UpdateAllReadCursors(char* aWriteCursor) MOZ_REQUIRES(mReentrantMonitor);
  void ValidateAllReadCursors() MOZ_REQUIRES(mReentrantMonitor);
  uint32_t GetBufferSegmentCount(const nsPipeReadState& aReadState,
                                 const ReentrantMonitorAutoEnter& ev) const
      MOZ_REQUIRES(mReentrantMonitor);
  bool IsAdvanceBufferFull(const ReentrantMonitorAutoEnter& ev) const
      MOZ_REQUIRES(mReentrantMonitor);

  //
  // methods below may be called while outside the pipe's monitor
  //

  void DrainInputStream(nsPipeReadState& aReadState, nsPipeEvents& aEvents);
  nsresult GetWriteSegment(char*& aSegment, uint32_t& aSegmentLen);
  void AdvanceWriteCursor(uint32_t aCount);

  void OnInputStreamException(nsPipeInputStream* aStream, nsresult aReason);
  void OnPipeException(nsresult aReason, bool aOutputOnly = false);

  nsresult CloneInputStream(nsPipeInputStream* aOriginal,
                            nsIInputStream** aCloneOut);

  // methods below should only be called by AutoReadSegment
  nsresult GetReadSegment(nsPipeReadState& aReadState, const char*& aSegment,
                          uint32_t& aLength);
  void ReleaseReadSegment(nsPipeReadState& aReadState, nsPipeEvents& aEvents);
  void AdvanceReadCursor(nsPipeReadState& aReadState, uint32_t aCount);

  // We can't inherit from both nsIInputStream and nsIOutputStream
  // because they collide on their Close method. Consequently we nest their
  // implementations to avoid the extra object allocation.
  nsPipeOutputStream mOutput;

  // Since the input stream can be cloned, we may have more than one.  Use
  // a weak reference as the streams will clear their entry here in their
  // destructor.  Using a strong reference would create a reference cycle.
  // Only usable while mReentrantMonitor is locked.
  nsTArray<nsPipeInputStream*> mInputList MOZ_GUARDED_BY(mReentrantMonitor);

  ReentrantMonitor mReentrantMonitor;
  nsSegmentedBuffer mBuffer MOZ_GUARDED_BY(mReentrantMonitor);

  // The maximum number of segments to allow to be buffered in advance
  // of the fastest reader.  This is collection of segments is called
  // the "advance buffer".
  uint32_t mMaxAdvanceBufferSegmentCount MOZ_GUARDED_BY(mReentrantMonitor);

  int32_t mWriteSegment MOZ_GUARDED_BY(mReentrantMonitor);
  char* mWriteCursor MOZ_GUARDED_BY(mReentrantMonitor);
  char* mWriteLimit MOZ_GUARDED_BY(mReentrantMonitor);

  // |mStatus| is protected by |mReentrantMonitor|.
  nsresult mStatus MOZ_GUARDED_BY(mReentrantMonitor);
};

//-----------------------------------------------------------------------------

// Declarations of Monitor() methods on the streams.
//
// These must be placed early to provide MOZ_RETURN_CAPABILITY annotations for
// the thread-safety analysis. This couldn't be done at the declaration due to
// nsPipe not yet being defined.

ReentrantMonitor& nsPipeOutputStream::Monitor() const
    MOZ_RETURN_CAPABILITY(mPipe->mReentrantMonitor) {
  return mPipe->mReentrantMonitor;
}

ReentrantMonitor& nsPipeInputStream::Monitor() const
    MOZ_RETURN_CAPABILITY(mPipe->mReentrantMonitor) {
  return mPipe->mReentrantMonitor;
}

//-----------------------------------------------------------------------------

// RAII class representing an active read segment.  When it goes out of scope
// it automatically updates the read cursor and releases the read segment.
class MOZ_STACK_CLASS AutoReadSegment final {
 public:
  AutoReadSegment(nsPipe* aPipe, nsPipeReadState& aReadState,
                  uint32_t aMaxLength)
      : mPipe(aPipe),
        mReadState(aReadState),
        mStatus(NS_ERROR_FAILURE),
        mSegment(nullptr),
        mLength(0),
        mOffset(0) {
    MOZ_DIAGNOSTIC_ASSERT(mPipe);
    MOZ_DIAGNOSTIC_ASSERT(!mReadState.mActiveRead);
    mStatus = mPipe->GetReadSegment(mReadState, mSegment, mLength);
    if (NS_SUCCEEDED(mStatus)) {
      MOZ_DIAGNOSTIC_ASSERT(mReadState.mActiveRead);
      MOZ_DIAGNOSTIC_ASSERT(mSegment);
      mLength = std::min(mLength, aMaxLength);
      MOZ_DIAGNOSTIC_ASSERT(mLength);
    }
  }

  ~AutoReadSegment() {
    if (NS_SUCCEEDED(mStatus)) {
      if (mOffset) {
        mPipe->AdvanceReadCursor(mReadState, mOffset);
      } else {
        nsPipeEvents events;
        mPipe->ReleaseReadSegment(mReadState, events);
      }
    }
    MOZ_DIAGNOSTIC_ASSERT(!mReadState.mActiveRead);
  }

  nsresult Status() const { return mStatus; }

  const char* Data() const {
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(mStatus));
    MOZ_DIAGNOSTIC_ASSERT(mSegment);
    return mSegment + mOffset;
  }

  uint32_t Length() const {
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(mStatus));
    MOZ_DIAGNOSTIC_ASSERT(mLength >= mOffset);
    return mLength - mOffset;
  }

  void Advance(uint32_t aCount) {
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(mStatus));
    MOZ_DIAGNOSTIC_ASSERT(aCount <= (mLength - mOffset));
    mOffset += aCount;
  }

  nsPipeReadState& ReadState() const { return mReadState; }

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
//       Likewise, each input stream reader will have it's own amount of
//       buffered data.  The pipe size threshold, however, is only applied
//       to the input stream that is being read fastest.  We call this
//       the "advance buffer" in that its in advance of all readers.  We
//       allow slower input streams to buffer more data so that we don't
//       stall processing of the faster input stream.
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

nsPipe::nsPipe(uint32_t aSegmentSize, uint32_t aSegmentCount)
    : mOutput(this),
      mReentrantMonitor("nsPipe.mReentrantMonitor"),
      // protect against overflow
      mMaxAdvanceBufferSegmentCount(
          std::min(aSegmentCount, UINT32_MAX / aSegmentSize)),
      mWriteSegment(-1),
      mWriteCursor(nullptr),
      mWriteLimit(nullptr),
      mStatus(NS_OK) {
  // The internal buffer is always "infinite" so that we can allow
  // the size to expand when cloned streams are read at different
  // rates.  We enforce a limit on how much data can be buffered
  // ahead of the fastest reader in GetWriteSegment().
  MOZ_ALWAYS_SUCCEEDS(mBuffer.Init(aSegmentSize, UINT32_MAX));
}

nsPipe::~nsPipe() = default;

void nsPipe::PeekSegment(const nsPipeReadState& aReadState, uint32_t aIndex,
                         char*& aCursor, char*& aLimit) {
  if (aIndex == 0) {
    MOZ_DIAGNOSTIC_ASSERT(!aReadState.mReadCursor || mBuffer.GetSegmentCount());
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

nsresult nsPipe::GetReadSegment(nsPipeReadState& aReadState,
                                const char*& aSegment, uint32_t& aLength) {
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (aReadState.mReadCursor == aReadState.mReadLimit) {
    return NS_FAILED(mStatus) ? mStatus : NS_BASE_STREAM_WOULD_BLOCK;
  }

  // The input stream locks the pipe while getting the buffer to read from,
  // but then unlocks while actual data copying is taking place.  In
  // order to avoid deleting the buffer out from under this lockless read
  // set a flag to indicate a read is active.  This flag is only modified
  // while the lock is held.
  MOZ_DIAGNOSTIC_ASSERT(!aReadState.mActiveRead);
  aReadState.mActiveRead = true;

  aSegment = aReadState.mReadCursor;
  aLength = aReadState.mReadLimit - aReadState.mReadCursor;
  MOZ_DIAGNOSTIC_ASSERT(aLength <= aReadState.mAvailable);

  return NS_OK;
}

void nsPipe::ReleaseReadSegment(nsPipeReadState& aReadState,
                                nsPipeEvents& aEvents) {
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  MOZ_DIAGNOSTIC_ASSERT(aReadState.mActiveRead);
  aReadState.mActiveRead = false;

  // When a read completes and releases the mActiveRead flag, we may have
  // blocked a drain from completing.  This occurs when the input stream is
  // closed during the read.  In these cases, we need to complete the drain as
  // soon as the active read completes.
  if (aReadState.mNeedDrain) {
    aReadState.mNeedDrain = false;
    DrainInputStream(aReadState, aEvents);
  }
}

void nsPipe::AdvanceReadCursor(nsPipeReadState& aReadState,
                               uint32_t aBytesRead) {
  MOZ_DIAGNOSTIC_ASSERT(aBytesRead > 0);

  nsPipeEvents events;
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    LOG(("III advancing read cursor by %u\n", aBytesRead));
    MOZ_DIAGNOSTIC_ASSERT(aBytesRead <= mBuffer.GetSegmentSize());

    aReadState.mReadCursor += aBytesRead;
    MOZ_DIAGNOSTIC_ASSERT(aReadState.mReadCursor <= aReadState.mReadLimit);

    MOZ_DIAGNOSTIC_ASSERT(aReadState.mAvailable >= aBytesRead);
    aReadState.mAvailable -= aBytesRead;

    // Check to see if we're at the end of the available read data.  If we
    // are, and this segment is not still being written, then we can possibly
    // free up the segment.
    if (aReadState.mReadCursor == aReadState.mReadLimit &&
        !ReadSegmentBeingWritten(aReadState)) {
      // Advance the segment position.  If we have read any segments from the
      // advance buffer then we can potentially notify blocked writers.
      mOutput.Monitor().AssertCurrentThreadIn();
      if (AdvanceReadSegment(aReadState, mon) == SegmentAdvanceBufferRead &&
          mOutput.OnOutputWritable(events) == NotifyMonitor) {
        mon.NotifyAll();
      }
    }

    ReleaseReadSegment(aReadState, events);
  }
}

SegmentChangeResult nsPipe::AdvanceReadSegment(
    nsPipeReadState& aReadState, const ReentrantMonitorAutoEnter& ev) {
  // Calculate how many segments are buffered for this stream to start.
  uint32_t startBufferSegments = GetBufferSegmentCount(aReadState, ev);

  int32_t currentSegment = aReadState.mSegment;

  // Move to the next segment to read
  aReadState.mSegment += 1;

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
  }

  if (mWriteSegment < aReadState.mSegment) {
    // read cursor has hit the end of written data, so reset it
    MOZ_DIAGNOSTIC_ASSERT(mWriteSegment == (aReadState.mSegment - 1));
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

  // Calculate how many segments are buffered for the stream after
  // reading.
  uint32_t endBufferSegments = GetBufferSegmentCount(aReadState, ev);

  // If the stream has read a segment out of the set of advanced buffer
  // segments, then the writer may advance.
  if (startBufferSegments >= mMaxAdvanceBufferSegmentCount &&
      endBufferSegments < mMaxAdvanceBufferSegmentCount) {
    return SegmentAdvanceBufferRead;
  }

  // Otherwise there are no significant changes to the segment structure.
  return SegmentNotChanged;
}

void nsPipe::DrainInputStream(nsPipeReadState& aReadState,
                              nsPipeEvents& aEvents) {
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  // If a segment is actively being read in ReadSegments() for this input
  // stream, then we cannot drain the stream.  This can happen because
  // ReadSegments() does not hold the lock while copying from the buffer.
  // If we detect this condition, simply note that we need a drain once
  // the read completes and return immediately.
  if (aReadState.mActiveRead) {
    MOZ_DIAGNOSTIC_ASSERT(!aReadState.mNeedDrain);
    aReadState.mNeedDrain = true;
    return;
  }

  while (mWriteSegment >= aReadState.mSegment) {
    // If the last segment to free is still being written to, we're done
    // draining.  We can't free any more.
    if (ReadSegmentBeingWritten(aReadState)) {
      break;
    }

    // Don't bother checking if this results in an advance buffer segment
    // read.  Since we are draining the entire stream we will read an
    // advance buffer segment no matter what.
    AdvanceReadSegment(aReadState, mon);
  }

  // Force the stream into an empty state.  Make sure mAvailable, mCursor, and
  // mReadLimit are consistent with one another.
  aReadState.mAvailable = 0;
  aReadState.mReadCursor = nullptr;
  aReadState.mReadLimit = nullptr;

  // Remove the input stream from the pipe's list of streams.  This will
  // prevent the pipe from holding the stream alive or trying to update
  // its read state any further.
  DebugOnly<uint32_t> numRemoved = 0;
  mInputList.RemoveElementsBy([&](nsPipeInputStream* aEntry) {
    bool result = &aReadState == &aEntry->ReadState();
    numRemoved += result ? 1 : 0;
    return result;
  });
  MOZ_ASSERT(numRemoved == 1);

  // If we have read any segments from the advance buffer then we can
  // potentially notify blocked writers.
  mOutput.Monitor().AssertCurrentThreadIn();
  if (!IsAdvanceBufferFull(mon) &&
      mOutput.OnOutputWritable(aEvents) == NotifyMonitor) {
    mon.NotifyAll();
  }
}

bool nsPipe::ReadSegmentBeingWritten(nsPipeReadState& aReadState) {
  mReentrantMonitor.AssertCurrentThreadIn();
  bool beingWritten =
      mWriteSegment == aReadState.mSegment && mWriteLimit > mWriteCursor;
  MOZ_DIAGNOSTIC_ASSERT(!beingWritten || aReadState.mReadLimit == mWriteCursor);
  return beingWritten;
}

nsresult nsPipe::GetWriteSegment(char*& aSegment, uint32_t& aSegmentLen) {
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);

  if (NS_FAILED(mStatus)) {
    return mStatus;
  }

  // write cursor and limit may both be null indicating an empty buffer.
  if (mWriteCursor == mWriteLimit) {
    // The pipe is full if we have hit our limit on advance data buffering.
    // This means the fastest reader is still reading slower than data is
    // being written into the pipe.
    if (IsAdvanceBufferFull(mon)) {
      return NS_BASE_STREAM_WOULD_BLOCK;
    }

    // The nsSegmentedBuffer is configured to be "infinite", so this
    // should never return nullptr here.
    char* seg = mBuffer.AppendNewSegment();
    if (!seg) {
      return NS_ERROR_OUT_OF_MEMORY;
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
    LOG(("OOO rolling back write cursor %" PRId64 " bytes\n",
         static_cast<int64_t>(mWriteCursor - head)));
    RollBackAllReadCursors(head);
    mWriteCursor = head;
  }

  aSegment = mWriteCursor;
  aSegmentLen = mWriteLimit - mWriteCursor;
  return NS_OK;
}

void nsPipe::AdvanceWriteCursor(uint32_t aBytesWritten) {
  MOZ_DIAGNOSTIC_ASSERT(aBytesWritten > 0);

  nsPipeEvents events;
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    LOG(("OOO advancing write cursor by %u\n", aBytesWritten));

    char* newWriteCursor = mWriteCursor + aBytesWritten;
    MOZ_DIAGNOSTIC_ASSERT(newWriteCursor <= mWriteLimit);

    // update read limit if reading in the same segment
    UpdateAllReadCursors(newWriteCursor);

    mWriteCursor = newWriteCursor;

    ValidateAllReadCursors();

    // update the writable flag on the output stream
    if (mWriteCursor == mWriteLimit) {
      mOutput.Monitor().AssertCurrentThreadIn();
      mOutput.SetWritable(!IsAdvanceBufferFull(mon));
    }

    // notify input stream that pipe now contains additional data
    bool needNotify = false;
    for (uint32_t i = 0; i < mInputList.Length(); ++i) {
      mInputList[i]->Monitor().AssertCurrentThreadIn();
      if (mInputList[i]->OnInputReadable(aBytesWritten, events, mon) ==
          NotifyMonitor) {
        needNotify = true;
      }
    }

    if (needNotify) {
      mon.NotifyAll();
    }
  }
}

void nsPipe::OnInputStreamException(nsPipeInputStream* aStream,
                                    nsresult aReason) {
  MOZ_DIAGNOSTIC_ASSERT(NS_FAILED(aReason));

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

      mInputList[i]->Monitor().AssertCurrentThreadIn();
      MonitorAction action =
          mInputList[i]->OnInputException(aReason, events, mon);

      // Notify after element is removed in case we re-enter as a result.
      if (action == NotifyMonitor) {
        mon.NotifyAll();
      }

      return;
    }
  }
}

void nsPipe::OnPipeException(nsresult aReason, bool aOutputOnly) {
  LOG(("PPP nsPipe::OnPipeException [reason=%" PRIx32 " output-only=%d]\n",
       static_cast<uint32_t>(aReason), aOutputOnly));

  nsPipeEvents events;
  {
    ReentrantMonitorAutoEnter mon(mReentrantMonitor);

    // if we've already hit an exception, then ignore this one.
    if (NS_FAILED(mStatus)) {
      return;
    }

    mStatus = aReason;

    bool needNotify = false;

    // OnInputException() can drain the stream and remove it from
    // mInputList.  So iterate over a temp list instead.
    nsTArray<nsPipeInputStream*> list = mInputList.Clone();
    for (uint32_t i = 0; i < list.Length(); ++i) {
      // an output-only exception applies to the input end if the pipe has
      // zero bytes available.
      list[i]->Monitor().AssertCurrentThreadIn();
      if (aOutputOnly && list[i]->Available()) {
        continue;
      }

      if (list[i]->OnInputException(aReason, events, mon) == NotifyMonitor) {
        needNotify = true;
      }
    }

    mOutput.Monitor().AssertCurrentThreadIn();
    if (mOutput.OnOutputException(aReason, events) == NotifyMonitor) {
      needNotify = true;
    }

    // Notify after we have removed any input streams from mInputList
    if (needNotify) {
      mon.NotifyAll();
    }
  }
}

nsresult nsPipe::CloneInputStream(nsPipeInputStream* aOriginal,
                                  nsIInputStream** aCloneOut) {
  ReentrantMonitorAutoEnter mon(mReentrantMonitor);
  RefPtr<nsPipeInputStream> ref = new nsPipeInputStream(*aOriginal);
  // don't add clones of closed pipes to mInputList.
  ref->Monitor().AssertCurrentThreadIn();
  if (NS_SUCCEEDED(ref->InputStatus(mon))) {
    mInputList.AppendElement(ref);
  }
  nsCOMPtr<nsIAsyncInputStream> upcast = std::move(ref);
  upcast.forget(aCloneOut);
  return NS_OK;
}

uint32_t nsPipe::CountSegmentReferences(int32_t aSegment) {
  mReentrantMonitor.AssertCurrentThreadIn();
  uint32_t count = 0;
  for (uint32_t i = 0; i < mInputList.Length(); ++i) {
    if (aSegment >= mInputList[i]->ReadState().mSegment) {
      count += 1;
    }
  }
  return count;
}

void nsPipe::SetAllNullReadCursors() {
  mReentrantMonitor.AssertCurrentThreadIn();
  for (uint32_t i = 0; i < mInputList.Length(); ++i) {
    nsPipeReadState& readState = mInputList[i]->ReadState();
    if (!readState.mReadCursor) {
      MOZ_DIAGNOSTIC_ASSERT(mWriteSegment == readState.mSegment);
      readState.mReadCursor = readState.mReadLimit = mWriteCursor;
    }
  }
}

bool nsPipe::AllReadCursorsMatchWriteCursor() {
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

void nsPipe::RollBackAllReadCursors(char* aWriteCursor) {
  mReentrantMonitor.AssertCurrentThreadIn();
  for (uint32_t i = 0; i < mInputList.Length(); ++i) {
    nsPipeReadState& readState = mInputList[i]->ReadState();
    MOZ_DIAGNOSTIC_ASSERT(mWriteSegment == readState.mSegment);
    MOZ_DIAGNOSTIC_ASSERT(mWriteCursor == readState.mReadCursor);
    MOZ_DIAGNOSTIC_ASSERT(mWriteCursor == readState.mReadLimit);
    readState.mReadCursor = aWriteCursor;
    readState.mReadLimit = aWriteCursor;
  }
}

void nsPipe::UpdateAllReadCursors(char* aWriteCursor) {
  mReentrantMonitor.AssertCurrentThreadIn();
  for (uint32_t i = 0; i < mInputList.Length(); ++i) {
    nsPipeReadState& readState = mInputList[i]->ReadState();
    if (mWriteSegment == readState.mSegment &&
        readState.mReadLimit == mWriteCursor) {
      readState.mReadLimit = aWriteCursor;
    }
  }
}

void nsPipe::ValidateAllReadCursors() {
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
    MOZ_ASSERT(state.mReadCursor != mWriteCursor ||
               (mBuffer.GetSegment(state.mSegment) == state.mReadCursor &&
                mWriteCursor == mWriteLimit));
  }
#endif
}

uint32_t nsPipe::GetBufferSegmentCount(
    const nsPipeReadState& aReadState,
    const ReentrantMonitorAutoEnter& ev) const {
  // The write segment can be smaller than the current reader position
  // in some cases.  For example, when the first write segment has not
  // been allocated yet mWriteSegment is negative.  In these cases
  // the stream is effectively using zero segments.
  if (mWriteSegment < aReadState.mSegment) {
    return 0;
  }

  MOZ_DIAGNOSTIC_ASSERT(mWriteSegment >= 0);
  MOZ_DIAGNOSTIC_ASSERT(aReadState.mSegment >= 0);

  // Otherwise at least one segment is being used.  We add one here
  // since a single segment is being used when the write and read
  // segment indices are the same.
  return 1 + mWriteSegment - aReadState.mSegment;
}

bool nsPipe::IsAdvanceBufferFull(const ReentrantMonitorAutoEnter& ev) const {
  // If we have fewer total segments than the limit we can immediately
  // determine we are not full.  Note, we must add one to mWriteSegment
  // to convert from a index to a count.
  MOZ_DIAGNOSTIC_ASSERT(mWriteSegment >= -1);
  MOZ_DIAGNOSTIC_ASSERT(mWriteSegment < INT32_MAX);
  uint32_t totalWriteSegments = mWriteSegment + 1;
  if (totalWriteSegments < mMaxAdvanceBufferSegmentCount) {
    return false;
  }

  // Otherwise we must inspect all of our reader streams.  We need
  // to determine the buffer depth of the fastest reader.
  uint32_t minBufferSegments = UINT32_MAX;
  for (uint32_t i = 0; i < mInputList.Length(); ++i) {
    // Only count buffer segments from input streams that are open.
    mInputList[i]->Monitor().AssertCurrentThreadIn();
    if (NS_FAILED(mInputList[i]->Status(ev))) {
      continue;
    }
    const nsPipeReadState& state = mInputList[i]->ReadState();
    uint32_t bufferSegments = GetBufferSegmentCount(state, ev);
    minBufferSegments = std::min(minBufferSegments, bufferSegments);
    // We only care if any reader has fewer segments buffered than
    // our threshold.  We can stop once we hit that threshold.
    if (minBufferSegments < mMaxAdvanceBufferSegmentCount) {
      return false;
    }
  }

  // Note, its possible for minBufferSegments to exceed our
  // mMaxAdvanceBufferSegmentCount here.  This happens when a cloned
  // reader gets far behind, but then the fastest reader stream is
  // closed.  This leaves us with a single stream that is buffered
  // beyond our max.  Naturally we continue to indicate the pipe
  // is full at this point.

  return true;
}

//-----------------------------------------------------------------------------
// nsPipeEvents methods:
//-----------------------------------------------------------------------------

nsPipeEvents::~nsPipeEvents() {
  // dispatch any pending events
  for (auto& callback : mCallbacks) {
    callback.Notify();
  }
  mCallbacks.Clear();
}

//-----------------------------------------------------------------------------
// nsPipeInputStream methods:
//-----------------------------------------------------------------------------

NS_IMPL_ADDREF(nsPipeInputStream);
NS_IMPL_RELEASE(nsPipeInputStream);

NS_INTERFACE_TABLE_HEAD(nsPipeInputStream)
  NS_INTERFACE_TABLE_BEGIN
    NS_INTERFACE_TABLE_ENTRY(nsPipeInputStream, nsIAsyncInputStream)
    NS_INTERFACE_TABLE_ENTRY(nsPipeInputStream, nsITellableStream)
    NS_INTERFACE_TABLE_ENTRY(nsPipeInputStream, nsISearchableInputStream)
    NS_INTERFACE_TABLE_ENTRY(nsPipeInputStream, nsICloneableInputStream)
    NS_INTERFACE_TABLE_ENTRY(nsPipeInputStream, nsIBufferedInputStream)
    NS_INTERFACE_TABLE_ENTRY(nsPipeInputStream, nsIClassInfo)
    NS_INTERFACE_TABLE_ENTRY(nsPipeInputStream, nsIInputStreamPriority)
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(nsPipeInputStream, nsIInputStream,
                                       nsIAsyncInputStream)
    NS_INTERFACE_TABLE_ENTRY_AMBIGUOUS(nsPipeInputStream, nsISupports,
                                       nsIAsyncInputStream)
  NS_INTERFACE_TABLE_END
NS_INTERFACE_TABLE_TAIL

NS_IMPL_CI_INTERFACE_GETTER(nsPipeInputStream, nsIInputStream,
                            nsIAsyncInputStream, nsITellableStream,
                            nsISearchableInputStream, nsICloneableInputStream,
                            nsIBufferedInputStream)

NS_IMPL_THREADSAFE_CI(nsPipeInputStream)

NS_IMETHODIMP
nsPipeInputStream::Init(nsIInputStream*, uint32_t) {
  MOZ_CRASH(
      "nsPipeInputStream should never be initialized with "
      "nsIBufferedInputStream::Init!\n");
}

NS_IMETHODIMP
nsPipeInputStream::GetData(nsIInputStream** aResult) {
  // as this was not created with init() we are not
  // wrapping anything
  return NS_ERROR_NOT_IMPLEMENTED;
}

uint32_t nsPipeInputStream::Available() {
  mPipe->mReentrantMonitor.AssertCurrentThreadIn();
  return mReadState.mAvailable;
}

nsresult nsPipeInputStream::Wait() {
  MOZ_DIAGNOSTIC_ASSERT(mBlocking);

  ReentrantMonitorAutoEnter mon(mPipe->mReentrantMonitor);

  while (NS_SUCCEEDED(Status(mon)) && (mReadState.mAvailable == 0)) {
    LOG(("III pipe input: waiting for data\n"));

    mBlocked = true;
    mon.Wait();
    mBlocked = false;

    LOG(("III pipe input: woke up [status=%" PRIx32 " available=%u]\n",
         static_cast<uint32_t>(Status(mon)), mReadState.mAvailable));
  }

  return Status(mon) == NS_BASE_STREAM_CLOSED ? NS_OK : Status(mon);
}

MonitorAction nsPipeInputStream::OnInputReadable(
    uint32_t aBytesWritten, nsPipeEvents& aEvents,
    const ReentrantMonitorAutoEnter& ev) {
  MonitorAction result = DoNotNotifyMonitor;

  mPipe->mReentrantMonitor.AssertCurrentThreadIn();
  mReadState.mAvailable += aBytesWritten;

  if (mCallback && !(mCallback.Flags() & WAIT_CLOSURE_ONLY)) {
    aEvents.NotifyReady(std::move(mCallback));
  } else if (mBlocked) {
    result = NotifyMonitor;
  }

  return result;
}

MonitorAction nsPipeInputStream::OnInputException(
    nsresult aReason, nsPipeEvents& aEvents,
    const ReentrantMonitorAutoEnter& ev) {
  LOG(("nsPipeInputStream::OnInputException [this=%p reason=%" PRIx32 "]\n",
       this, static_cast<uint32_t>(aReason)));

  MonitorAction result = DoNotNotifyMonitor;

  MOZ_DIAGNOSTIC_ASSERT(NS_FAILED(aReason));

  if (NS_SUCCEEDED(mInputStatus)) {
    mInputStatus = aReason;
  }

  // force count of available bytes to zero.
  mPipe->DrainInputStream(mReadState, aEvents);

  if (mCallback) {
    aEvents.NotifyReady(std::move(mCallback));
  } else if (mBlocked) {
    result = NotifyMonitor;
  }

  return result;
}

NS_IMETHODIMP
nsPipeInputStream::CloseWithStatus(nsresult aReason) {
  LOG(("III CloseWithStatus [this=%p reason=%" PRIx32 "]\n", this,
       static_cast<uint32_t>(aReason)));

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
nsPipeInputStream::SetPriority(uint32_t priority) {
  mPriority = priority;
  return NS_OK;
}

NS_IMETHODIMP
nsPipeInputStream::GetPriority(uint32_t* priority) {
  *priority = mPriority;
  return NS_OK;
}

NS_IMETHODIMP
nsPipeInputStream::Close() { return CloseWithStatus(NS_BASE_STREAM_CLOSED); }

NS_IMETHODIMP
nsPipeInputStream::Available(uint64_t* aResult) {
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
nsPipeInputStream::ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                                uint32_t aCount, uint32_t* aReadCount) {
  LOG(("III ReadSegments [this=%p count=%u]\n", this, aCount));

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

      MOZ_DIAGNOSTIC_ASSERT(writeCount <= segment.Length());
      segment.Advance(writeCount);
      aCount -= writeCount;
      *aReadCount += writeCount;
      mLogicalOffset += writeCount;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsPipeInputStream::Read(char* aToBuf, uint32_t aBufLen, uint32_t* aReadCount) {
  return ReadSegments(NS_CopySegmentToBuffer, aToBuf, aBufLen, aReadCount);
}

NS_IMETHODIMP
nsPipeInputStream::IsNonBlocking(bool* aNonBlocking) {
  *aNonBlocking = !mBlocking;
  return NS_OK;
}

NS_IMETHODIMP
nsPipeInputStream::AsyncWait(nsIInputStreamCallback* aCallback, uint32_t aFlags,
                             uint32_t aRequestedCount,
                             nsIEventTarget* aTarget) {
  LOG(("III AsyncWait [this=%p]\n", this));

  nsPipeEvents pipeEvents;
  {
    ReentrantMonitorAutoEnter mon(mPipe->mReentrantMonitor);

    // replace a pending callback
    mCallback = nullptr;

    if (!aCallback) {
      return NS_OK;
    }

    CallbackHolder callback(this, aCallback, aFlags, aTarget);

    if (NS_FAILED(Status(mon)) ||
        (mReadState.mAvailable && !(aFlags & WAIT_CLOSURE_ONLY))) {
      // stream is already closed or readable; post event.
      pipeEvents.NotifyReady(std::move(callback));
    } else {
      // queue up callback object to be notified when data becomes available
      mCallback = std::move(callback);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPipeInputStream::Tell(int64_t* aOffset) {
  ReentrantMonitorAutoEnter mon(mPipe->mReentrantMonitor);

  // return error if closed
  if (!mReadState.mAvailable && NS_FAILED(Status(mon))) {
    return Status(mon);
  }

  *aOffset = mLogicalOffset;
  return NS_OK;
}

static bool strings_equal(bool aIgnoreCase, const char* aS1, const char* aS2,
                          uint32_t aLen) {
  return aIgnoreCase ? !nsCRT::strncasecmp(aS1, aS2, aLen)
                     : !strncmp(aS1, aS2, aLen);
}

NS_IMETHODIMP
nsPipeInputStream::Search(const char* aForString, bool aIgnoreCase,
                          bool* aFound, uint32_t* aOffsetSearchedTo) {
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
      if (strings_equal(aIgnoreCase, &cursor1[bufSeg1Offset], aForString,
                        strPart1Len) &&
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

  MOZ_ASSERT_UNREACHABLE("can't get here");
  return NS_ERROR_UNEXPECTED;  // keep compiler happy
}

NS_IMETHODIMP
nsPipeInputStream::GetCloneable(bool* aCloneableOut) {
  *aCloneableOut = true;
  return NS_OK;
}

NS_IMETHODIMP
nsPipeInputStream::Clone(nsIInputStream** aCloneOut) {
  return mPipe->CloneInputStream(this, aCloneOut);
}

nsresult nsPipeInputStream::Status(const ReentrantMonitorAutoEnter& ev) const {
  if (NS_FAILED(mInputStatus)) {
    return mInputStatus;
  }

  if (mReadState.mAvailable) {
    // Still something to read and this input stream state is OK.
    return NS_OK;
  }

  // Nothing to read, just fall through to the pipe's state that
  // may reflect state of its output stream side (already closed).
  return mPipe->mStatus;
}

nsresult nsPipeInputStream::Status() const {
  ReentrantMonitorAutoEnter mon(mPipe->mReentrantMonitor);
  return Status(mon);
}

nsPipeInputStream::~nsPipeInputStream() { Close(); }

//-----------------------------------------------------------------------------
// nsPipeOutputStream methods:
//-----------------------------------------------------------------------------

NS_IMPL_QUERY_INTERFACE(nsPipeOutputStream, nsIOutputStream,
                        nsIAsyncOutputStream, nsIClassInfo)

NS_IMPL_CI_INTERFACE_GETTER(nsPipeOutputStream, nsIOutputStream,
                            nsIAsyncOutputStream)

NS_IMPL_THREADSAFE_CI(nsPipeOutputStream)

nsresult nsPipeOutputStream::Wait() {
  MOZ_DIAGNOSTIC_ASSERT(mBlocking);

  ReentrantMonitorAutoEnter mon(mPipe->mReentrantMonitor);

  if (NS_SUCCEEDED(mPipe->mStatus) && !mWritable) {
    LOG(("OOO pipe output: waiting for space\n"));
    mBlocked = true;
    mon.Wait();
    mBlocked = false;
    LOG(("OOO pipe output: woke up [pipe-status=%" PRIx32 " writable=%u]\n",
         static_cast<uint32_t>(mPipe->mStatus), mWritable));
  }

  return mPipe->mStatus == NS_BASE_STREAM_CLOSED ? NS_OK : mPipe->mStatus;
}

MonitorAction nsPipeOutputStream::OnOutputWritable(nsPipeEvents& aEvents) {
  MonitorAction result = DoNotNotifyMonitor;

  mWritable = true;

  if (mCallback && !(mCallback.Flags() & WAIT_CLOSURE_ONLY)) {
    aEvents.NotifyReady(std::move(mCallback));
  } else if (mBlocked) {
    result = NotifyMonitor;
  }

  return result;
}

MonitorAction nsPipeOutputStream::OnOutputException(nsresult aReason,
                                                    nsPipeEvents& aEvents) {
  LOG(("nsPipeOutputStream::OnOutputException [this=%p reason=%" PRIx32 "]\n",
       this, static_cast<uint32_t>(aReason)));

  MonitorAction result = DoNotNotifyMonitor;

  MOZ_DIAGNOSTIC_ASSERT(NS_FAILED(aReason));
  mWritable = false;

  if (mCallback) {
    aEvents.NotifyReady(std::move(mCallback));
  } else if (mBlocked) {
    result = NotifyMonitor;
  }

  return result;
}

NS_IMETHODIMP_(MozExternalRefCountType)
nsPipeOutputStream::AddRef() {
  ++mWriterRefCnt;
  return mPipe->AddRef();
}

NS_IMETHODIMP_(MozExternalRefCountType)
nsPipeOutputStream::Release() {
  if (--mWriterRefCnt == 0) {
    Close();
  }
  return mPipe->Release();
}

NS_IMETHODIMP
nsPipeOutputStream::CloseWithStatus(nsresult aReason) {
  LOG(("OOO CloseWithStatus [this=%p reason=%" PRIx32 "]\n", this,
       static_cast<uint32_t>(aReason)));

  if (NS_SUCCEEDED(aReason)) {
    aReason = NS_BASE_STREAM_CLOSED;
  }

  // input stream may remain open
  mPipe->OnPipeException(aReason, true);
  return NS_OK;
}

NS_IMETHODIMP
nsPipeOutputStream::Close() { return CloseWithStatus(NS_BASE_STREAM_CLOSED); }

NS_IMETHODIMP
nsPipeOutputStream::WriteSegments(nsReadSegmentFun aReader, void* aClosure,
                                  uint32_t aCount, uint32_t* aWriteCount) {
  LOG(("OOO WriteSegments [this=%p count=%u]\n", this, aCount));

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

      rv = aReader(this, aClosure, segment, *aWriteCount, segmentLen,
                   &readCount);

      if (NS_FAILED(rv) || readCount == 0) {
        aCount = 0;
        // any errors returned from the aReader end here: do not
        // propagate to the caller of WriteSegments.
        rv = NS_OK;
        break;
      }

      MOZ_DIAGNOSTIC_ASSERT(readCount <= segmentLen);
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

NS_IMETHODIMP
nsPipeOutputStream::Write(const char* aFromBuf, uint32_t aBufLen,
                          uint32_t* aWriteCount) {
  return WriteSegments(NS_CopyBufferToSegment, (void*)aFromBuf, aBufLen,
                       aWriteCount);
}

NS_IMETHODIMP
nsPipeOutputStream::Flush(void) {
  // nothing to do
  return NS_OK;
}

NS_IMETHODIMP
nsPipeOutputStream::WriteFrom(nsIInputStream* aFromStream, uint32_t aCount,
                              uint32_t* aWriteCount) {
  return WriteSegments(NS_CopyStreamToSegment, aFromStream, aCount,
                       aWriteCount);
}

NS_IMETHODIMP
nsPipeOutputStream::IsNonBlocking(bool* aNonBlocking) {
  *aNonBlocking = !mBlocking;
  return NS_OK;
}

NS_IMETHODIMP
nsPipeOutputStream::AsyncWait(nsIOutputStreamCallback* aCallback,
                              uint32_t aFlags, uint32_t aRequestedCount,
                              nsIEventTarget* aTarget) {
  LOG(("OOO AsyncWait [this=%p]\n", this));

  nsPipeEvents pipeEvents;
  {
    ReentrantMonitorAutoEnter mon(mPipe->mReentrantMonitor);

    // replace a pending callback
    mCallback = nullptr;

    if (!aCallback) {
      return NS_OK;
    }

    CallbackHolder callback(this, aCallback, aFlags, aTarget);

    if (NS_FAILED(mPipe->mStatus) ||
        (mWritable && !(aFlags & WAIT_CLOSURE_ONLY))) {
      // stream is already closed or writable; post event.
      pipeEvents.NotifyReady(std::move(callback));
    } else {
      // queue up callback object to be notified when data becomes available
      mCallback = std::move(callback);
    }
  }
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsresult NS_NewPipe(nsIInputStream** aPipeIn, nsIOutputStream** aPipeOut,
                    uint32_t aSegmentSize, uint32_t aMaxSize,
                    bool aNonBlockingInput, bool aNonBlockingOutput) {
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

// Disable thread safety analysis as this is logically a constructor, and no
// additional threads can observe these objects yet.
nsresult NS_NewPipe2(nsIAsyncInputStream** aPipeIn,
                     nsIAsyncOutputStream** aPipeOut, bool aNonBlockingInput,
                     bool aNonBlockingOutput, uint32_t aSegmentSize,
                     uint32_t aSegmentCount) MOZ_NO_THREAD_SAFETY_ANALYSIS {
  RefPtr<nsPipe> pipe =
      new nsPipe(aSegmentSize ? aSegmentSize : DEFAULT_SEGMENT_SIZE,
                 aSegmentCount ? aSegmentCount : DEFAULT_SEGMENT_COUNT);

  RefPtr<nsPipeInputStream> pipeIn = new nsPipeInputStream(pipe);
  pipe->mInputList.AppendElement(pipeIn);
  RefPtr<nsPipeOutputStream> pipeOut = &pipe->mOutput;

  pipeIn->SetNonBlocking(aNonBlockingInput);
  pipeOut->SetNonBlocking(aNonBlockingOutput);

  pipeIn.forget(aPipeIn);
  pipeOut.forget(aPipeOut);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

// Thin nsIPipe implementation for consumers of the component manager interface
// for creating pipes. Acts as a thin wrapper around NS_NewPipe2 for JS callers.
class nsPipeHolder final : public nsIPipe {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIPIPE

 private:
  ~nsPipeHolder() = default;

  nsCOMPtr<nsIAsyncInputStream> mInput;
  nsCOMPtr<nsIAsyncOutputStream> mOutput;
};

NS_IMPL_ISUPPORTS(nsPipeHolder, nsIPipe)

NS_IMETHODIMP
nsPipeHolder::Init(bool aNonBlockingInput, bool aNonBlockingOutput,
                   uint32_t aSegmentSize, uint32_t aSegmentCount) {
  if (mInput || mOutput) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  return NS_NewPipe2(getter_AddRefs(mInput), getter_AddRefs(mOutput),
                     aNonBlockingInput, aNonBlockingOutput, aSegmentSize,
                     aSegmentCount);
}

NS_IMETHODIMP
nsPipeHolder::GetInputStream(nsIAsyncInputStream** aInputStream) {
  if (mInput) {
    *aInputStream = do_AddRef(mInput).take();
    return NS_OK;
  }
  return NS_ERROR_NOT_INITIALIZED;
}

NS_IMETHODIMP
nsPipeHolder::GetOutputStream(nsIAsyncOutputStream** aOutputStream) {
  if (mOutput) {
    *aOutputStream = do_AddRef(mOutput).take();
    return NS_OK;
  }
  return NS_ERROR_NOT_INITIALIZED;
}

nsresult nsPipeConstructor(REFNSIID aIID, void** aResult) {
  RefPtr<nsPipeHolder> pipe = new nsPipeHolder();
  nsresult rv = pipe->QueryInterface(aIID, aResult);
  return rv;
}

////////////////////////////////////////////////////////////////////////////////
