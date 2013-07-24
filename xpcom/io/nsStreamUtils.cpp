/* vim:set ts=4 sw=4 sts=4 et cin: */
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

using namespace mozilla;

//-----------------------------------------------------------------------------

class nsInputStreamReadyEvent MOZ_FINAL : public nsIRunnable
                                        , public nsIInputStreamCallback
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    nsInputStreamReadyEvent(nsIInputStreamCallback *callback,
                            nsIEventTarget *target)
        : mCallback(callback)
        , mTarget(target)
    {
    }

private:
    ~nsInputStreamReadyEvent()
    {
        if (!mCallback)
            return;
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
                    nsISupports *sup = event;
                    NS_ADDREF(sup);
                }
            }
        }
    }

public:
    NS_IMETHOD OnInputStreamReady(nsIAsyncInputStream *stream)
    {
        mStream = stream;

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
            if (mStream)
                mCallback->OnInputStreamReady(mStream);
            mCallback = nullptr;
        }
        return NS_OK;
    }

private:
    nsCOMPtr<nsIAsyncInputStream>    mStream;
    nsCOMPtr<nsIInputStreamCallback> mCallback;
    nsCOMPtr<nsIEventTarget>         mTarget;
};

NS_IMPL_ISUPPORTS2(nsInputStreamReadyEvent, nsIRunnable,
                   nsIInputStreamCallback)

//-----------------------------------------------------------------------------

class nsOutputStreamReadyEvent MOZ_FINAL : public nsIRunnable
                                         , public nsIOutputStreamCallback
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS

    nsOutputStreamReadyEvent(nsIOutputStreamCallback *callback,
                             nsIEventTarget *target)
        : mCallback(callback)
        , mTarget(target)
    {
    }

private:
    ~nsOutputStreamReadyEvent()
    {
        if (!mCallback)
            return;
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
                    nsISupports *sup = event;
                    NS_ADDREF(sup);
                }
            }
        }
    }

public:
    NS_IMETHOD OnOutputStreamReady(nsIAsyncOutputStream *stream)
    {
        mStream = stream;

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
            if (mStream)
                mCallback->OnOutputStreamReady(mStream);
            mCallback = nullptr;
        }
        return NS_OK;
    }

private:
    nsCOMPtr<nsIAsyncOutputStream>    mStream;
    nsCOMPtr<nsIOutputStreamCallback> mCallback;
    nsCOMPtr<nsIEventTarget>          mTarget;
};

NS_IMPL_ISUPPORTS2(nsOutputStreamReadyEvent, nsIRunnable,
                   nsIOutputStreamCallback)

//-----------------------------------------------------------------------------

already_AddRefed<nsIInputStreamCallback>
NS_NewInputStreamReadyEvent(nsIInputStreamCallback *callback,
                            nsIEventTarget *target)
{
    NS_ASSERTION(callback, "null callback");
    NS_ASSERTION(target, "null target");
    nsRefPtr<nsInputStreamReadyEvent> ev =
        new nsInputStreamReadyEvent(callback, target);
    return ev.forget();
}

already_AddRefed<nsIOutputStreamCallback>
NS_NewOutputStreamReadyEvent(nsIOutputStreamCallback *callback,
                             nsIEventTarget *target)
{
    NS_ASSERTION(callback, "null callback");
    NS_ASSERTION(target, "null target");
    nsRefPtr<nsOutputStreamReadyEvent> ev =
        new nsOutputStreamReadyEvent(callback, target);
    return ev.forget();
}

//-----------------------------------------------------------------------------
// NS_AsyncCopy implementation

// abstract stream copier...
class nsAStreamCopier : public nsIInputStreamCallback
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

    // virtual since subclasses call superclass Release()
    virtual ~nsAStreamCopier()
    {
    }

    // kick off the async copy...
    nsresult Start(nsIInputStream *source,
                   nsIOutputStream *sink,
                   nsIEventTarget *target,
                   nsAsyncCopyCallbackFun callback,
                   void *closure,
                   uint32_t chunksize,
                   bool closeSource,
                   bool closeSink,
                   nsAsyncCopyProgressFun progressCallback)
    {
        mSource = source;
        mSink = sink;
        mTarget = target;
        mCallback = callback;
        mClosure = closure;
        mChunkSize = chunksize;
        mCloseSource = closeSource;
        mCloseSink = closeSink;
        mProgressCallback = progressCallback;

        mAsyncSource = do_QueryInterface(mSource);
        mAsyncSink = do_QueryInterface(mSink);

        return PostContinuationEvent();
    }

    // implemented by subclasses, returns number of bytes copied and
    // sets source and sink condition before returning.
    virtual uint32_t DoCopy(nsresult *sourceCondition, nsresult *sinkCondition) = 0;

    void Process()
    {
        if (!mSource || !mSink)
            return;

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
                }
                else if (sinkCondition == NS_BASE_STREAM_WOULD_BLOCK && mAsyncSink) {
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
                        mAsyncSource->CloseWithStatus(canceled ? cancelStatus :
                                                                 sinkCondition);
                    else
                        mSource->Close();
                }
                mAsyncSource = nullptr;
                mSource = nullptr;

                if (mCloseSink) {
                    // close sink
                    if (mAsyncSink)
                        mAsyncSink->CloseWithStatus(canceled ? cancelStatus :
                                                               sourceCondition);
                    else {
                        // If we have an nsISafeOutputStream, and our
                        // sourceCondition and sinkCondition are not set to a
                        // failure state, finish writing.
                        nsCOMPtr<nsISafeOutputStream> sostream =
                            do_QueryInterface(mSink);
                        if (sostream && NS_SUCCEEDED(sourceCondition) &&
                            NS_SUCCEEDED(sinkCondition))
                            sostream->Finish();
                        else
                            mSink->Close();
                    }
                }
                mAsyncSink = nullptr;
                mSink = nullptr;

                // notify state complete...
                if (mCallback) {
                    nsresult status;
                    if (!canceled) {
                        status = sourceCondition;
                        if (NS_SUCCEEDED(status))
                            status = sinkCondition;
                        if (status == NS_BASE_STREAM_CLOSED)
                            status = NS_OK;
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
        if (mCanceled)
            return NS_ERROR_FAILURE;

        if (NS_SUCCEEDED(aReason)) {
            NS_WARNING("cancel with non-failure status code");
            aReason = NS_BASE_STREAM_CLOSED;
        }

        mCanceled = true;
        mCancelStatus = aReason;
        return NS_OK;
    }

    NS_IMETHOD OnInputStreamReady(nsIAsyncInputStream *source)
    {
        PostContinuationEvent();
        return NS_OK;
    }

    NS_IMETHOD OnOutputStreamReady(nsIAsyncOutputStream *sink)
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
        if (mEventInProcess)
            mEventIsPending = true;
        else {
            rv = mTarget->Dispatch(this, NS_DISPATCH_NORMAL);
            if (NS_SUCCEEDED(rv))
                mEventInProcess = true;
            else
                NS_WARNING("unable to post continuation event");
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
    void                          *mClosure;
    uint32_t                       mChunkSize;
    bool                           mEventInProcess;
    bool                           mEventIsPending;
    bool                           mCloseSource;
    bool                           mCloseSink;
    bool                           mCanceled;
    nsresult                       mCancelStatus;
};

NS_IMPL_ISUPPORTS3(nsAStreamCopier,
                   nsIInputStreamCallback,
                   nsIOutputStreamCallback,
                   nsIRunnable)

class nsStreamCopierIB MOZ_FINAL : public nsAStreamCopier
{
public:
    nsStreamCopierIB() : nsAStreamCopier() {}
    virtual ~nsStreamCopierIB() {}

    struct ReadSegmentsState {
        nsIOutputStream *mSink;
        nsresult         mSinkCondition;
    };

    static NS_METHOD ConsumeInputBuffer(nsIInputStream *inStr,
                                        void *closure,
                                        const char *buffer,
                                        uint32_t offset,
                                        uint32_t count,
                                        uint32_t *countWritten)
    {
        ReadSegmentsState *state = (ReadSegmentsState *) closure;

        nsresult rv = state->mSink->Write(buffer, count, countWritten);
        if (NS_FAILED(rv))
            state->mSinkCondition = rv;
        else if (*countWritten == 0)
            state->mSinkCondition = NS_BASE_STREAM_CLOSED;

        return state->mSinkCondition;
    }

    uint32_t DoCopy(nsresult *sourceCondition, nsresult *sinkCondition)
    {
        ReadSegmentsState state;
        state.mSink = mSink;
        state.mSinkCondition = NS_OK;

        uint32_t n;
        *sourceCondition =
            mSource->ReadSegments(ConsumeInputBuffer, &state, mChunkSize, &n);
        *sinkCondition = state.mSinkCondition;
        return n;
    }
};

class nsStreamCopierOB MOZ_FINAL : public nsAStreamCopier
{
public:
    nsStreamCopierOB() : nsAStreamCopier() {}
    virtual ~nsStreamCopierOB() {}

    struct WriteSegmentsState {
        nsIInputStream *mSource;
        nsresult        mSourceCondition;
    };

    static NS_METHOD FillOutputBuffer(nsIOutputStream *outStr,
                                      void *closure,
                                      char *buffer,
                                      uint32_t offset,
                                      uint32_t count,
                                      uint32_t *countRead)
    {
        WriteSegmentsState *state = (WriteSegmentsState *) closure;

        nsresult rv = state->mSource->Read(buffer, count, countRead);
        if (NS_FAILED(rv))
            state->mSourceCondition = rv;
        else if (*countRead == 0)
            state->mSourceCondition = NS_BASE_STREAM_CLOSED;

        return state->mSourceCondition;
    }

    uint32_t DoCopy(nsresult *sourceCondition, nsresult *sinkCondition)
    {
        WriteSegmentsState state;
        state.mSource = mSource;
        state.mSourceCondition = NS_OK;

        uint32_t n;
        *sinkCondition =
            mSink->WriteSegments(FillOutputBuffer, &state, mChunkSize, &n);
        *sourceCondition = state.mSourceCondition;
        return n;
    }
};

//-----------------------------------------------------------------------------

nsresult
NS_AsyncCopy(nsIInputStream         *source,
             nsIOutputStream        *sink,
             nsIEventTarget         *target,
             nsAsyncCopyMode         mode,
             uint32_t                chunkSize,
             nsAsyncCopyCallbackFun  callback,
             void                   *closure,
             bool                    closeSource,
             bool                    closeSink,
             nsISupports           **aCopierCtx,
             nsAsyncCopyProgressFun  progressCallback)
{
    NS_ASSERTION(target, "non-null target required");

    nsresult rv;
    nsAStreamCopier *copier;

    if (mode == NS_ASYNCCOPY_VIA_READSEGMENTS)
        copier = new nsStreamCopierIB();
    else
        copier = new nsStreamCopierOB();

    if (!copier)
        return NS_ERROR_OUT_OF_MEMORY;

    // Start() takes an owning ref to the copier...
    NS_ADDREF(copier);
    rv = copier->Start(source, sink, target, callback, closure, chunkSize,
                       closeSource, closeSink, progressCallback);

    if (aCopierCtx) {
        *aCopierCtx = static_cast<nsISupports*>(
                      static_cast<nsIRunnable*>(copier));
        NS_ADDREF(*aCopierCtx);
    }
    NS_RELEASE(copier);

    return rv;
}

//-----------------------------------------------------------------------------

nsresult
NS_CancelAsyncCopy(nsISupports *aCopierCtx, nsresult aReason)
{
  nsAStreamCopier *copier = static_cast<nsAStreamCopier *>(
                            static_cast<nsIRunnable *>(aCopierCtx));
  return copier->Cancel(aReason);
}

//-----------------------------------------------------------------------------

nsresult
NS_ConsumeStream(nsIInputStream *stream, uint32_t maxCount, nsACString &result)
{
    nsresult rv = NS_OK;
    result.Truncate();

    while (maxCount) {
        uint64_t avail64;
        rv = stream->Available(&avail64);
        if (NS_FAILED(rv)) {
            if (rv == NS_BASE_STREAM_CLOSED)
                rv = NS_OK;
            break;
        }
        if (avail64 == 0)
            break;

        uint32_t avail = (uint32_t)XPCOM_MIN<uint64_t>(avail64, maxCount);

        // resize result buffer
        uint32_t length = result.Length();
        if (avail > UINT32_MAX - length)
            return NS_ERROR_FILE_TOO_BIG;
        
        result.SetLength(length + avail);
        if (result.Length() != (length + avail))
            return NS_ERROR_OUT_OF_MEMORY;
        char *buf = result.BeginWriting() + length;
        
        uint32_t n;
        rv = stream->Read(buf, avail, &n);
        if (NS_FAILED(rv))
            break;
        if (n != avail)
            result.SetLength(length + n);
        if (n == 0)
            break;
        maxCount -= n;
    }

    return rv;
}

//-----------------------------------------------------------------------------

static NS_METHOD
TestInputStream(nsIInputStream *inStr,
                void *closure,
                const char *buffer,
                uint32_t offset,
                uint32_t count,
                uint32_t *countWritten)
{
    bool *result = static_cast<bool *>(closure);
    *result = true;
    return NS_ERROR_ABORT;  // don't call me anymore
}

bool
NS_InputStreamIsBuffered(nsIInputStream *stream)
{
    bool result = false;
    uint32_t n;
    nsresult rv = stream->ReadSegments(TestInputStream,
                                       &result, 1, &n);
    return result || NS_SUCCEEDED(rv);
}

static NS_METHOD
TestOutputStream(nsIOutputStream *outStr,
                 void *closure,
                 char *buffer,
                 uint32_t offset,
                 uint32_t count,
                 uint32_t *countRead)
{
    bool *result = static_cast<bool *>(closure);
    *result = true;
    return NS_ERROR_ABORT;  // don't call me anymore
}

bool
NS_OutputStreamIsBuffered(nsIOutputStream *stream)
{
    bool result = false;
    uint32_t n;
    stream->WriteSegments(TestOutputStream, &result, 1, &n);
    return result;
}

//-----------------------------------------------------------------------------

NS_METHOD
NS_CopySegmentToStream(nsIInputStream *inStr,
                       void *closure,
                       const char *buffer,
                       uint32_t offset,
                       uint32_t count,
                       uint32_t *countWritten)
{
    nsIOutputStream *outStr = static_cast<nsIOutputStream *>(closure);
    *countWritten = 0;
    while (count) {
        uint32_t n;
        nsresult rv = outStr->Write(buffer, count, &n);
        if (NS_FAILED(rv))
            return rv;
        buffer += n;
        count -= n;
        *countWritten += n;
    }
    return NS_OK;
}

NS_METHOD
NS_CopySegmentToBuffer(nsIInputStream *inStr,
                       void *closure,
                       const char *buffer,
                       uint32_t offset,
                       uint32_t count,
                       uint32_t *countWritten)
{
    char *toBuf = static_cast<char *>(closure);
    memcpy(&toBuf[offset], buffer, count);
    *countWritten = count;
    return NS_OK;
}

NS_METHOD
NS_CopySegmentToBuffer(nsIOutputStream *outStr,
                       void *closure,
                       char *buffer,
                       uint32_t offset,
                       uint32_t count,
                       uint32_t *countRead)
{
    const char* fromBuf = static_cast<const char*>(closure);
    memcpy(buffer, &fromBuf[offset], count);
    *countRead = count;
    return NS_OK;
}

NS_METHOD
NS_DiscardSegment(nsIInputStream *inStr,
                  void *closure,
                  const char *buffer,
                  uint32_t offset,
                  uint32_t count,
                  uint32_t *countWritten)
{
    *countWritten = count;
    return NS_OK;
}

//-----------------------------------------------------------------------------

NS_METHOD
NS_WriteSegmentThunk(nsIInputStream *inStr,
                     void *closure,
                     const char *buffer,
                     uint32_t offset,
                     uint32_t count,
                     uint32_t *countWritten)
{
    nsWriteSegmentThunk *thunk = static_cast<nsWriteSegmentThunk *>(closure);
    return thunk->mFun(thunk->mStream, thunk->mClosure, buffer, offset, count,
                       countWritten);
}
