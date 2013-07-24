/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include "prlog.h"

#include "mozilla/Mutex.h"
#include "mozilla/Attributes.h"
#include "nsIInputStreamTee.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIEventTarget.h"
#include "nsThreadUtils.h"

using namespace mozilla;

#ifdef PR_LOGGING
static PRLogModuleInfo*
GetTeeLog()
{
    static PRLogModuleInfo *sLog;
    if (!sLog)
        sLog = PR_NewLogModule("nsInputStreamTee");
    return sLog;
}
#define LOG(args) PR_LOG(GetTeeLog(), PR_LOG_DEBUG, args)
#else
#define LOG(args)
#endif

class nsInputStreamTee MOZ_FINAL : public nsIInputStreamTee
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIINPUTSTREAMTEE

    nsInputStreamTee();
    bool SinkIsValid();
    void InvalidateSink();

private:
    ~nsInputStreamTee() {}

    nsresult TeeSegment(const char *buf, uint32_t count);
    
    static NS_METHOD WriteSegmentFun(nsIInputStream *, void *, const char *,
                                     uint32_t, uint32_t, uint32_t *);

private:
    nsCOMPtr<nsIInputStream>  mSource;
    nsCOMPtr<nsIOutputStream> mSink;
    nsCOMPtr<nsIEventTarget>  mEventTarget;
    nsWriteSegmentFun         mWriter;  // for implementing ReadSegments
    void                      *mClosure; // for implementing ReadSegments
    nsAutoPtr<Mutex>          mLock; // synchronize access to mSinkIsValid
    bool                      mSinkIsValid; // False if TeeWriteEvent fails 
};

class nsInputStreamTeeWriteEvent : public nsRunnable {
public:
    // aTee's lock is held across construction of this object
    nsInputStreamTeeWriteEvent(const char *aBuf, uint32_t aCount,
                               nsIOutputStream  *aSink, 
                               nsInputStreamTee *aTee)
    {
        // copy the buffer - will be free'd by dtor
        mBuf = (char *)malloc(aCount);
        if (mBuf) memcpy(mBuf, (char *)aBuf, aCount);
        mCount = aCount;
        mSink = aSink;
        bool isNonBlocking;
        mSink->IsNonBlocking(&isNonBlocking);
        NS_ASSERTION(isNonBlocking == false, "mSink is nonblocking");
        mTee = aTee;
    }

    NS_IMETHOD Run()
    {
        if (!mBuf) {
            NS_WARNING("nsInputStreamTeeWriteEvent::Run() "
                       "memory not allocated\n");
            return NS_OK;
        }
        NS_ABORT_IF_FALSE(mSink, "mSink is null!");

        //  The output stream could have been invalidated between when 
        //  this event was dispatched and now, so check before writing.
        if (!mTee->SinkIsValid()) {
            return NS_OK; 
        }

        LOG(("nsInputStreamTeeWriteEvent::Run() [%p]"
             "will write %u bytes to %p\n",
              this, mCount, mSink.get()));

        uint32_t totalBytesWritten = 0;
        while (mCount) {
            nsresult rv;
            uint32_t bytesWritten = 0;
            rv = mSink->Write(mBuf + totalBytesWritten, mCount, &bytesWritten);
            if (NS_FAILED(rv)) {
                LOG(("nsInputStreamTeeWriteEvent::Run[%p] error %x in writing",
                     this,rv));
                mTee->InvalidateSink();
                break;
            }
            totalBytesWritten += bytesWritten;
            NS_ASSERTION(bytesWritten <= mCount, "wrote too much");
            mCount -= bytesWritten;
        }
        return NS_OK;
    }

protected:
    virtual ~nsInputStreamTeeWriteEvent()
    {
        if (mBuf) free(mBuf);
        mBuf = nullptr;
    }
    
private:
    char *mBuf;
    uint32_t mCount;
    nsCOMPtr<nsIOutputStream> mSink;
    // back pointer to the tee that created this runnable
    nsRefPtr<nsInputStreamTee> mTee;
};

nsInputStreamTee::nsInputStreamTee(): mLock(nullptr)
                                    , mSinkIsValid(true)
{
}

bool
nsInputStreamTee::SinkIsValid()
{
    MutexAutoLock lock(*mLock); 
    return mSinkIsValid; 
}

void
nsInputStreamTee::InvalidateSink()
{
    MutexAutoLock lock(*mLock);
    mSinkIsValid = false;
}

nsresult
nsInputStreamTee::TeeSegment(const char *buf, uint32_t count)
{
    if (!mSink) return NS_OK; // nothing to do
    if (mLock) { // asynchronous case
        NS_ASSERTION(mEventTarget, "mEventTarget is null, mLock is not null.");
        if (!SinkIsValid()) {
            return NS_OK; // nothing to do
        }
        nsRefPtr<nsIRunnable> event =
            new nsInputStreamTeeWriteEvent(buf, count, mSink, this);
        NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);
        LOG(("nsInputStreamTee::TeeSegment [%p] dispatching write %u bytes\n",
              this, count));
        return mEventTarget->Dispatch(event, NS_DISPATCH_NORMAL);
    } else { // synchronous case
        NS_ASSERTION(!mEventTarget, "mEventTarget is not null, mLock is null.");
        nsresult rv;
        uint32_t totalBytesWritten = 0;
        while (count) {
            uint32_t bytesWritten = 0;
            rv = mSink->Write(buf + totalBytesWritten, count, &bytesWritten);
            if (NS_FAILED(rv)) {
                // ok, this is not a fatal error... just drop our reference to mSink
                // and continue on as if nothing happened.
                NS_WARNING("Write failed (non-fatal)");
                // catch possible misuse of the input stream tee
                NS_ASSERTION(rv != NS_BASE_STREAM_WOULD_BLOCK, "sink must be a blocking stream");
                mSink = 0;
                break;
            }
            totalBytesWritten += bytesWritten;
            NS_ASSERTION(bytesWritten <= count, "wrote too much");
            count -= bytesWritten;
        }
        return NS_OK;
    }
}

NS_METHOD
nsInputStreamTee::WriteSegmentFun(nsIInputStream *in, void *closure, const char *fromSegment,
                                  uint32_t offset, uint32_t count, uint32_t *writeCount)
{
    nsInputStreamTee *tee = reinterpret_cast<nsInputStreamTee *>(closure);

    nsresult rv = tee->mWriter(in, tee->mClosure, fromSegment, offset, count, writeCount);
    if (NS_FAILED(rv) || (*writeCount == 0)) {
        NS_ASSERTION((NS_FAILED(rv) ? (*writeCount == 0) : true),
                "writer returned an error with non-zero writeCount");
        return rv;
    }

    return tee->TeeSegment(fromSegment, *writeCount);
}

NS_IMPL_ISUPPORTS2(nsInputStreamTee,
                   nsIInputStreamTee,
                   nsIInputStream)
NS_IMETHODIMP
nsInputStreamTee::Close()
{
    NS_ENSURE_TRUE(mSource, NS_ERROR_NOT_INITIALIZED);
    nsresult rv = mSource->Close();
    mSource = 0;
    mSink = 0;
    return rv;
}

NS_IMETHODIMP
nsInputStreamTee::Available(uint64_t *avail)
{
    NS_ENSURE_TRUE(mSource, NS_ERROR_NOT_INITIALIZED);
    return mSource->Available(avail);
}

NS_IMETHODIMP
nsInputStreamTee::Read(char *buf, uint32_t count, uint32_t *bytesRead)
{
    NS_ENSURE_TRUE(mSource, NS_ERROR_NOT_INITIALIZED);

    nsresult rv = mSource->Read(buf, count, bytesRead);
    if (NS_FAILED(rv) || (*bytesRead == 0))
        return rv;

    return TeeSegment(buf, *bytesRead);
}

NS_IMETHODIMP
nsInputStreamTee::ReadSegments(nsWriteSegmentFun writer, 
                               void *closure,
                               uint32_t count,
                               uint32_t *bytesRead)
{
    NS_ENSURE_TRUE(mSource, NS_ERROR_NOT_INITIALIZED);

    mWriter = writer;
    mClosure = closure;

    return mSource->ReadSegments(WriteSegmentFun, this, count, bytesRead);
}

NS_IMETHODIMP
nsInputStreamTee::IsNonBlocking(bool *result)
{
    NS_ENSURE_TRUE(mSource, NS_ERROR_NOT_INITIALIZED);
    return mSource->IsNonBlocking(result);
}

NS_IMETHODIMP
nsInputStreamTee::SetSource(nsIInputStream *source)
{
    mSource = source;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamTee::GetSource(nsIInputStream **source)
{
    NS_IF_ADDREF(*source = mSource);
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamTee::SetSink(nsIOutputStream *sink)
{
#ifdef DEBUG
    if (sink) {
        bool nonBlocking;
        nsresult rv = sink->IsNonBlocking(&nonBlocking);
        if (NS_FAILED(rv) || nonBlocking)
            NS_ERROR("sink should be a blocking stream");
    }
#endif
    mSink = sink;
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamTee::GetSink(nsIOutputStream **sink)
{
    NS_IF_ADDREF(*sink = mSink);
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamTee::SetEventTarget(nsIEventTarget *anEventTarget)
{
    mEventTarget = anEventTarget;
    if (mEventTarget) {
        // Only need synchronization if this is an async tee
        mLock = new Mutex("nsInputStreamTee.mLock");
    }
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamTee::GetEventTarget(nsIEventTarget **anEventTarget)
{
    NS_IF_ADDREF(*anEventTarget = mEventTarget);
    return NS_OK;
}


nsresult
NS_NewInputStreamTeeAsync(nsIInputStream **result,
                          nsIInputStream *source,
                          nsIOutputStream *sink,
                          nsIEventTarget *anEventTarget)
{
    nsresult rv;
    
    nsCOMPtr<nsIInputStreamTee> tee = new nsInputStreamTee();
    if (!tee)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = tee->SetSource(source);
    if (NS_FAILED(rv)) return rv;

    rv = tee->SetSink(sink);
    if (NS_FAILED(rv)) return rv;

    rv = tee->SetEventTarget(anEventTarget);
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*result = tee);
    return rv;
}

nsresult
NS_NewInputStreamTee(nsIInputStream **result,
                     nsIInputStream *source,
                     nsIOutputStream *sink)
{
    return NS_NewInputStreamTeeAsync(result, source, sink, nullptr);
}
