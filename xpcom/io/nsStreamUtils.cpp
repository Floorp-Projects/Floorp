/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Mutex.h"
#include "mozilla/Attributes.h"
#include "nsStreamUtils.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIPipe.h"
#include "nsIEventTarget.h"
#include "nsIRunnable.h"
#include "nsISafeOutputStream.h"
#include "nsString.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIBufferedStreams.h"

using namespace mozilla;

//-----------------------------------------------------------------------------

class nsInputStreamReadyEvent MOZ_FINAL
  : public nsIRunnable
  , public nsIInputStreamCallback
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  nsInputStreamReadyEvent(nsIInputStreamCallback* aCallback,
                          nsIEventTarget* aTarget)
    : mCallback(aCallback)
    , mTarget(aTarget)
  {
  }

private:
  ~nsInputStreamReadyEvent()
  {
    if (!mCallback) {
      return;
    }
    //
    // whoa!!  looks like we never posted this event.  take care to
    // release mCallback on the correct thread.  if mTarget lives on the
    // calling thread, then we are ok.  otherwise, we have to try to
    // proxy the Release over the right thread.  if that thread is dead,
    // then there's nothing we can do... better to leak than crash.
    //
    bool val;
    nsresult rv = mTarget->IsOnCurrentThread(&val);
    if (NS_FAILED(rv) || !val) {
      nsCOMPtr<nsIInputStreamCallback> event =
        NS_NewInputStreamReadyEvent(mCallback, mTarget);
      mCallback = nullptr;
      if (event) {
        rv = event->OnInputStreamReady(nullptr);
        if (NS_FAILED(rv)) {
          NS_NOTREACHED("leaking stream event");
          nsISupports* sup = event;
          NS_ADDREF(sup);
        }
      }
    }
  }

public:
  NS_IMETHOD OnInputStreamReady(nsIAsyncInputStream* aStream)
  {
    mStream = aStream;

    nsresult rv =
      mTarget->Dispatch(this, NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      NS_WARNING("Dispatch failed");
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

  NS_IMETHOD Run()
  {
    if (mCallback) {
      if (mStream) {
        mCallback->OnInputStreamReady(mStream);
      }
      mCallback = nullptr;
    }
    return NS_OK;
  }

private:
  nsCOMPtr<nsIAsyncInputStream>    mStream;
  nsCOMPtr<nsIInputStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget>         mTarget;
};

NS_IMPL_ISUPPORTS(nsInputStreamReadyEvent, nsIRunnable,
                  nsIInputStreamCallback)

//-----------------------------------------------------------------------------

class nsOutputStreamReadyEvent MOZ_FINAL
  : public nsIRunnable
  , public nsIOutputStreamCallback
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  nsOutputStreamReadyEvent(nsIOutputStreamCallback* aCallback,
                           nsIEventTarget* aTarget)
    : mCallback(aCallback)
    , mTarget(aTarget)
  {
  }

private:
  ~nsOutputStreamReadyEvent()
  {
    if (!mCallback) {
      return;
    }
    //
    // whoa!!  looks like we never posted this event.  take care to
    // release mCallback on the correct thread.  if mTarget lives on the
    // calling thread, then we are ok.  otherwise, we have to try to
    // proxy the Release over the right thread.  if that thread is dead,
    // then there's nothing we can do... better to leak than crash.
    //
    bool val;
    nsresult rv = mTarget->IsOnCurrentThread(&val);
    if (NS_FAILED(rv) || !val) {
      nsCOMPtr<nsIOutputStreamCallback> event =
        NS_NewOutputStreamReadyEvent(mCallback, mTarget);
      mCallback = nullptr;
      if (event) {
        rv = event->OnOutputStreamReady(nullptr);
        if (NS_FAILED(rv)) {
          NS_NOTREACHED("leaking stream event");
          nsISupports* sup = event;
          NS_ADDREF(sup);
        }
      }
    }
  }

public:
  NS_IMETHOD OnOutputStreamReady(nsIAsyncOutputStream* aStream)
  {
    mStream = aStream;

    nsresult rv =
      mTarget->Dispatch(this, NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      NS_WARNING("PostEvent failed");
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

  NS_IMETHOD Run()
  {
    if (mCallback) {
      if (mStream) {
        mCallback->OnOutputStreamReady(mStream);
      }
      mCallback = nullptr;
    }
    return NS_OK;
  }

private:
  nsCOMPtr<nsIAsyncOutputStream>    mStream;
  nsCOMPtr<nsIOutputStreamCallback> mCallback;
  nsCOMPtr<nsIEventTarget>          mTarget;
};

NS_IMPL_ISUPPORTS(nsOutputStreamReadyEvent, nsIRunnable,
                  nsIOutputStreamCallback)

//-----------------------------------------------------------------------------

already_AddRefed<nsIInputStreamCallback>
NS_NewInputStreamReadyEvent(nsIInputStreamCallback* aCallback,
                            nsIEventTarget* aTarget)
{
  NS_ASSERTION(aCallback, "null callback");
  NS_ASSERTION(aTarget, "null target");
  nsRefPtr<nsInputStreamReadyEvent> ev =
    new nsInputStreamReadyEvent(aCallback, aTarget);
  return ev.forget();
}

already_AddRefed<nsIOutputStreamCallback>
NS_NewOutputStreamReadyEvent(nsIOutputStreamCallback* aCallback,
                             nsIEventTarget* aTarget)
{
  NS_ASSERTION(aCallback, "null callback");
  NS_ASSERTION(aTarget, "null target");
  nsRefPtr<nsOutputStreamReadyEvent> ev =
    new nsOutputStreamReadyEvent(aCallback, aTarget);
  return ev.forget();
}

//-----------------------------------------------------------------------------
// NS_AsyncCopy implementation

// abstract stream copier...
class nsAStreamCopier
  : public nsIInputStreamCallback
  , public nsIOutputStreamCallback
  , public nsIRunnable
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

  nsAStreamCopier()
    : mLock("nsAStreamCopier.mLock")
    , mCallback(nullptr)
    , mProgressCallback(nullptr)
    , mClosure(nullptr)
    , mChunkSize(0)
    , mEventInProcess(false)
    , mEventIsPending(false)
    , mCloseSource(true)
    , mCloseSink(true)
    , mCanceled(false)
    , mCancelStatus(NS_OK)
  {
  }

  // kick off the async copy...
  nsresult Start(nsIInputStream* aSource,
                 nsIOutputStream* aSink,
                 nsIEventTarget* aTarget,
                 nsAsyncCopyCallbackFun aCallback,
                 void* aClosure,
                 uint32_t aChunksize,
                 bool aCloseSource,
                 bool aCloseSink,
                 nsAsyncCopyProgressFun aProgressCallback)
  {
    mSource = aSource;
    mSink = aSink;
    mTarget = aTarget;
    mCallback = aCallback;
    mClosure = aClosure;
    mChunkSize = aChunksize;
    mCloseSource = aCloseSource;
    mCloseSink = aCloseSink;
    mProgressCallback = aProgressCallback;

    mAsyncSource = do_QueryInterface(mSource);
    mAsyncSink = do_QueryInterface(mSink);

    return PostContinuationEvent();
  }

  // implemented by subclasses, returns number of bytes copied and
  // sets source and sink condition before returning.
  virtual uint32_t DoCopy(nsresult* aSourceCondition,
                          nsresult* aSinkCondition) = 0;

  void Process()
  {
    if (!mSource || !mSink) {
      return;
    }

    nsresult sourceCondition, sinkCondition;
    nsresult cancelStatus;
    bool canceled;
    {
      MutexAutoLock lock(mLock);
      canceled = mCanceled;
      cancelStatus = mCancelStatus;
    }

    // Copy data from the source to the sink until we hit failure or have
    // copied all the data.
    for (;;) {
      // Note: copyFailed will be true if the source or the sink have
      //       reported an error, or if we failed to write any bytes
      //       because we have consumed all of our data.
      bool copyFailed = false;
      if (!canceled) {
        uint32_t n = DoCopy(&sourceCondition, &sinkCondition);
        if (n > 0 && mProgressCallback) {
          mProgressCallback(mClosure, n);
        }
        copyFailed = NS_FAILED(sourceCondition) ||
                     NS_FAILED(sinkCondition) || n == 0;

        MutexAutoLock lock(mLock);
        canceled = mCanceled;
        cancelStatus = mCancelStatus;
      }
      if (copyFailed && !canceled) {
        if (sourceCondition == NS_BASE_STREAM_WOULD_BLOCK && mAsyncSource) {
          // need to wait for more data from source.  while waiting for
          // more source data, be sure to observe failures on output end.
          mAsyncSource->AsyncWait(this, 0, 0, nullptr);

          if (mAsyncSink)
            mAsyncSink->AsyncWait(this,
                                  nsIAsyncOutputStream::WAIT_CLOSURE_ONLY,
                                  0, nullptr);
          break;
        } else if (sinkCondition == NS_BASE_STREAM_WOULD_BLOCK && mAsyncSink) {
          // need to wait for more room in the sink.  while waiting for
          // more room in the sink, be sure to observer failures on the
          // input end.
          mAsyncSink->AsyncWait(this, 0, 0, nullptr);

          if (mAsyncSource)
            mAsyncSource->AsyncWait(this,
                                    nsIAsyncInputStream::WAIT_CLOSURE_ONLY,
                                    0, nullptr);
          break;
        }
      }
      if (copyFailed || canceled) {
        if (mCloseSource) {
          // close source
          if (mAsyncSource)
            mAsyncSource->CloseWithStatus(
              canceled ? cancelStatus : sinkCondition);
          else {
            mSource->Close();
          }
        }
        mAsyncSource = nullptr;
        mSource = nullptr;

        if (mCloseSink) {
          // close sink
          if (mAsyncSink)
            mAsyncSink->CloseWithStatus(
              canceled ? cancelStatus : sourceCondition);
          else {
            // If we have an nsISafeOutputStream, and our
            // sourceCondition and sinkCondition are not set to a
            // failure state, finish writing.
            nsCOMPtr<nsISafeOutputStream> sostream =
              do_QueryInterface(mSink);
            if (sostream && NS_SUCCEEDED(sourceCondition) &&
                NS_SUCCEEDED(sinkCondition)) {
              sostream->Finish();
            } else {
              mSink->Close();
            }
          }
        }
        mAsyncSink = nullptr;
        mSink = nullptr;

        // notify state complete...
        if (mCallback) {
          nsresult status;
          if (!canceled) {
            status = sourceCondition;
            if (NS_SUCCEEDED(status)) {
              status = sinkCondition;
            }
            if (status == NS_BASE_STREAM_CLOSED) {
              status = NS_OK;
            }
          } else {
            status = cancelStatus;
          }
          mCallback(mClosure, status);
        }
        break;
      }
    }
  }

  nsresult Cancel(nsresult aReason)
  {
    MutexAutoLock lock(mLock);
    if (mCanceled) {
      return NS_ERROR_FAILURE;
    }

    if (NS_SUCCEEDED(aReason)) {
      NS_WARNING("cancel with non-failure status code");
      aReason = NS_BASE_STREAM_CLOSED;
    }

    mCanceled = true;
    mCancelStatus = aReason;
    return NS_OK;
  }

  NS_IMETHOD OnInputStreamReady(nsIAsyncInputStream* aSource)
  {
    PostContinuationEvent();
    return NS_OK;
  }

  NS_IMETHOD OnOutputStreamReady(nsIAsyncOutputStream* aSink)
  {
    PostContinuationEvent();
    return NS_OK;
  }

  // continuation event handler
  NS_IMETHOD Run()
  {
    Process();

    // clear "in process" flag and post any pending continuation event
    MutexAutoLock lock(mLock);
    mEventInProcess = false;
    if (mEventIsPending) {
      mEventIsPending = false;
      PostContinuationEvent_Locked();
    }

    return NS_OK;
  }

  nsresult PostContinuationEvent()
  {
    // we cannot post a continuation event if there is currently
    // an event in process.  doing so could result in Process being
    // run simultaneously on multiple threads, so we mark the event
    // as pending, and if an event is already in process then we
    // just let that existing event take care of posting the real
    // continuation event.

    MutexAutoLock lock(mLock);
    return PostContinuationEvent_Locked();
  }

  nsresult PostContinuationEvent_Locked()
  {
    nsresult rv = NS_OK;
    if (mEventInProcess) {
      mEventIsPending = true;
    } else {
      rv = mTarget->Dispatch(this, NS_DISPATCH_NORMAL);
      if (NS_SUCCEEDED(rv)) {
        mEventInProcess = true;
      } else {
        NS_WARNING("unable to post continuation event");
      }
    }
    return rv;
  }

protected:
  nsCOMPtr<nsIInputStream>       mSource;
  nsCOMPtr<nsIOutputStream>      mSink;
  nsCOMPtr<nsIAsyncInputStream>  mAsyncSource;
  nsCOMPtr<nsIAsyncOutputStream> mAsyncSink;
  nsCOMPtr<nsIEventTarget>       mTarget;
  Mutex                          mLock;
  nsAsyncCopyCallbackFun         mCallback;
  nsAsyncCopyProgressFun         mProgressCallback;
  void*                          mClosure;
  uint32_t                       mChunkSize;
  bool                           mEventInProcess;
  bool                           mEventIsPending;
  bool                           mCloseSource;
  bool                           mCloseSink;
  bool                           mCanceled;
  nsresult                       mCancelStatus;

  // virtual since subclasses call superclass Release()
  virtual ~nsAStreamCopier()
  {
  }
};

NS_IMPL_ISUPPORTS(nsAStreamCopier,
                  nsIInputStreamCallback,
                  nsIOutputStreamCallback,
                  nsIRunnable)

class nsStreamCopierIB MOZ_FINAL : public nsAStreamCopier
{
public:
  nsStreamCopierIB() : nsAStreamCopier()
  {
  }
  virtual ~nsStreamCopierIB()
  {
  }

  struct ReadSegmentsState
  {
    nsIOutputStream* mSink;
    nsresult         mSinkCondition;
  };

  static NS_METHOD ConsumeInputBuffer(nsIInputStream* aInStr,
                                      void* aClosure,
                                      const char* aBuffer,
                                      uint32_t aOffset,
                                      uint32_t aCount,
                                      uint32_t* aCountWritten)
  {
    ReadSegmentsState* state = (ReadSegmentsState*)aClosure;

    nsresult rv = state->mSink->Write(aBuffer, aCount, aCountWritten);
    if (NS_FAILED(rv)) {
      state->mSinkCondition = rv;
    } else if (*aCountWritten == 0) {
      state->mSinkCondition = NS_BASE_STREAM_CLOSED;
    }

    return state->mSinkCondition;
  }

  uint32_t DoCopy(nsresult* aSourceCondition, nsresult* aSinkCondition)
  {
    ReadSegmentsState state;
    state.mSink = mSink;
    state.mSinkCondition = NS_OK;

    uint32_t n;
    *aSourceCondition =
      mSource->ReadSegments(ConsumeInputBuffer, &state, mChunkSize, &n);
    *aSinkCondition = state.mSinkCondition;
    return n;
  }
};

class nsStreamCopierOB MOZ_FINAL : public nsAStreamCopier
{
public:
  nsStreamCopierOB() : nsAStreamCopier()
  {
  }
  virtual ~nsStreamCopierOB()
  {
  }

  struct WriteSegmentsState
  {
    nsIInputStream* mSource;
    nsresult        mSourceCondition;
  };

  static NS_METHOD FillOutputBuffer(nsIOutputStream* aOutStr,
                                    void* aClosure,
                                    char* aBuffer,
                                    uint32_t aOffset,
                                    uint32_t aCount,
                                    uint32_t* aCountRead)
  {
    WriteSegmentsState* state = (WriteSegmentsState*)aClosure;

    nsresult rv = state->mSource->Read(aBuffer, aCount, aCountRead);
    if (NS_FAILED(rv)) {
      state->mSourceCondition = rv;
    } else if (*aCountRead == 0) {
      state->mSourceCondition = NS_BASE_STREAM_CLOSED;
    }

    return state->mSourceCondition;
  }

  uint32_t DoCopy(nsresult* aSourceCondition, nsresult* aSinkCondition)
  {
    WriteSegmentsState state;
    state.mSource = mSource;
    state.mSourceCondition = NS_OK;

    uint32_t n;
    *aSinkCondition =
      mSink->WriteSegments(FillOutputBuffer, &state, mChunkSize, &n);
    *aSourceCondition = state.mSourceCondition;
    return n;
  }
};

//-----------------------------------------------------------------------------

nsresult
NS_AsyncCopy(nsIInputStream*         aSource,
             nsIOutputStream*        aSink,
             nsIEventTarget*         aTarget,
             nsAsyncCopyMode         aMode,
             uint32_t                aChunkSize,
             nsAsyncCopyCallbackFun  aCallback,
             void*                   aClosure,
             bool                    aCloseSource,
             bool                    aCloseSink,
             nsISupports**           aCopierCtx,
             nsAsyncCopyProgressFun  aProgressCallback)
{
  NS_ASSERTION(aTarget, "non-null target required");

  nsresult rv;
  nsAStreamCopier* copier;

  if (aMode == NS_ASYNCCOPY_VIA_READSEGMENTS) {
    copier = new nsStreamCopierIB();
  } else {
    copier = new nsStreamCopierOB();
  }

  if (!copier) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Start() takes an owning ref to the copier...
  NS_ADDREF(copier);
  rv = copier->Start(aSource, aSink, aTarget, aCallback, aClosure, aChunkSize,
                     aCloseSource, aCloseSink, aProgressCallback);

  if (aCopierCtx) {
    *aCopierCtx = static_cast<nsISupports*>(static_cast<nsIRunnable*>(copier));
    NS_ADDREF(*aCopierCtx);
  }
  NS_RELEASE(copier);

  return rv;
}

//-----------------------------------------------------------------------------

nsresult
NS_CancelAsyncCopy(nsISupports* aCopierCtx, nsresult aReason)
{
  nsAStreamCopier* copier =
    static_cast<nsAStreamCopier*>(static_cast<nsIRunnable *>(aCopierCtx));
  return copier->Cancel(aReason);
}

//-----------------------------------------------------------------------------

nsresult
NS_ConsumeStream(nsIInputStream* aStream, uint32_t aMaxCount,
                 nsACString& aResult)
{
  nsresult rv = NS_OK;
  aResult.Truncate();

  while (aMaxCount) {
    uint64_t avail64;
    rv = aStream->Available(&avail64);
    if (NS_FAILED(rv)) {
      if (rv == NS_BASE_STREAM_CLOSED) {
        rv = NS_OK;
      }
      break;
    }
    if (avail64 == 0) {
      break;
    }

    uint32_t avail = (uint32_t)XPCOM_MIN<uint64_t>(avail64, aMaxCount);

    // resize aResult buffer
    uint32_t length = aResult.Length();
    if (avail > UINT32_MAX - length) {
      return NS_ERROR_FILE_TOO_BIG;
    }

    aResult.SetLength(length + avail);
    if (aResult.Length() != (length + avail)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    char* buf = aResult.BeginWriting() + length;

    uint32_t n;
    rv = aStream->Read(buf, avail, &n);
    if (NS_FAILED(rv)) {
      break;
    }
    if (n != avail) {
      aResult.SetLength(length + n);
    }
    if (n == 0) {
      break;
    }
    aMaxCount -= n;
  }

  return rv;
}

//-----------------------------------------------------------------------------

static NS_METHOD
TestInputStream(nsIInputStream* aInStr,
                void* aClosure,
                const char* aBuffer,
                uint32_t aOffset,
                uint32_t aCount,
                uint32_t* aCountWritten)
{
  bool* result = static_cast<bool*>(aClosure);
  *result = true;
  return NS_ERROR_ABORT;  // don't call me anymore
}

bool
NS_InputStreamIsBuffered(nsIInputStream* aStream)
{
  nsCOMPtr<nsIBufferedInputStream> bufferedIn = do_QueryInterface(aStream);
  if (bufferedIn) {
    return true;
  }

  bool result = false;
  uint32_t n;
  nsresult rv = aStream->ReadSegments(TestInputStream, &result, 1, &n);
  return result || NS_SUCCEEDED(rv);
}

static NS_METHOD
TestOutputStream(nsIOutputStream* aOutStr,
                 void* aClosure,
                 char* aBuffer,
                 uint32_t aOffset,
                 uint32_t aCount,
                 uint32_t* aCountRead)
{
  bool* result = static_cast<bool*>(aClosure);
  *result = true;
  return NS_ERROR_ABORT;  // don't call me anymore
}

bool
NS_OutputStreamIsBuffered(nsIOutputStream* aStream)
{
  nsCOMPtr<nsIBufferedOutputStream> bufferedOut = do_QueryInterface(aStream);
  if (bufferedOut) {
    return true;
  }

  bool result = false;
  uint32_t n;
  aStream->WriteSegments(TestOutputStream, &result, 1, &n);
  return result;
}

//-----------------------------------------------------------------------------

NS_METHOD
NS_CopySegmentToStream(nsIInputStream* aInStr,
                       void* aClosure,
                       const char* aBuffer,
                       uint32_t aOffset,
                       uint32_t aCount,
                       uint32_t* aCountWritten)
{
  nsIOutputStream* outStr = static_cast<nsIOutputStream*>(aClosure);
  *aCountWritten = 0;
  while (aCount) {
    uint32_t n;
    nsresult rv = outStr->Write(aBuffer, aCount, &n);
    if (NS_FAILED(rv)) {
      return rv;
    }
    aBuffer += n;
    aCount -= n;
    *aCountWritten += n;
  }
  return NS_OK;
}

NS_METHOD
NS_CopySegmentToBuffer(nsIInputStream* aInStr,
                       void* aClosure,
                       const char* aBuffer,
                       uint32_t aOffset,
                       uint32_t aCount,
                       uint32_t* aCountWritten)
{
  char* toBuf = static_cast<char*>(aClosure);
  memcpy(&toBuf[aOffset], aBuffer, aCount);
  *aCountWritten = aCount;
  return NS_OK;
}

NS_METHOD
NS_CopySegmentToBuffer(nsIOutputStream* aOutStr,
                       void* aClosure,
                       char* aBuffer,
                       uint32_t aOffset,
                       uint32_t aCount,
                       uint32_t* aCountRead)
{
  const char* fromBuf = static_cast<const char*>(aClosure);
  memcpy(aBuffer, &fromBuf[aOffset], aCount);
  *aCountRead = aCount;
  return NS_OK;
}

NS_METHOD
NS_DiscardSegment(nsIInputStream* aInStr,
                  void* aClosure,
                  const char* aBuffer,
                  uint32_t aOffset,
                  uint32_t aCount,
                  uint32_t* aCountWritten)
{
  *aCountWritten = aCount;
  return NS_OK;
}

//-----------------------------------------------------------------------------

NS_METHOD
NS_WriteSegmentThunk(nsIInputStream* aInStr,
                     void* aClosure,
                     const char* aBuffer,
                     uint32_t aOffset,
                     uint32_t aCount,
                     uint32_t* aCountWritten)
{
  nsWriteSegmentThunk* thunk = static_cast<nsWriteSegmentThunk*>(aClosure);
  return thunk->mFun(thunk->mStream, thunk->mClosure, aBuffer, aOffset, aCount,
                     aCountWritten);
}

NS_METHOD
NS_FillArray(FallibleTArray<char>& aDest, nsIInputStream* aInput,
             uint32_t aKeep, uint32_t* aNewBytes)
{
  MOZ_ASSERT(aInput, "null stream");
  MOZ_ASSERT(aKeep <= aDest.Length(), "illegal keep count");

  char* aBuffer = aDest.Elements();
  int64_t keepOffset = int64_t(aDest.Length()) - aKeep;
  if (aKeep != 0 && keepOffset > 0) {
    memmove(aBuffer, aBuffer + keepOffset, aKeep);
  }

  nsresult rv =
    aInput->Read(aBuffer + aKeep, aDest.Capacity() - aKeep, aNewBytes);
  if (NS_FAILED(rv)) {
    *aNewBytes = 0;
  }
  // NOTE: we rely on the fact that the new slots are NOT initialized by
  // SetLengthAndRetainStorage here, see nsTArrayElementTraits::Construct()
  // in nsTArray.h:
  aDest.SetLengthAndRetainStorage(aKeep + *aNewBytes);

  MOZ_ASSERT(aDest.Length() <= aDest.Capacity(), "buffer overflow");
  return rv;
}
