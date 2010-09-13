/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include "prlog.h"

#include "nsIInputStreamTee.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIEventTarget.h"
#include "nsThreadUtils.h"
#include <prlock.h>
#include "nsAutoLock.h"

#ifdef PR_LOGGING
static PRLogModuleInfo* gInputStreamTeeLog = PR_NewLogModule("nsInputStreamTee");
#define LOG(args) PR_LOG(gInputStreamTeeLog, PR_LOG_DEBUG, args)
#else
#define LOG(args)
#endif

class nsInputStreamTee : public nsIInputStreamTee
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIINPUTSTREAMTEE

    nsInputStreamTee();
    bool SinkIsValid();
    void InvalidateSink();

private:
    ~nsInputStreamTee() { if (mLock) PR_DestroyLock(mLock); }

    nsresult TeeSegment(const char *buf, PRUint32 count);
    
    static NS_METHOD WriteSegmentFun(nsIInputStream *, void *, const char *,
                                     PRUint32, PRUint32, PRUint32 *);

private:
    nsCOMPtr<nsIInputStream>  mSource;
    nsCOMPtr<nsIOutputStream> mSink;
    nsCOMPtr<nsIEventTarget>  mEventTarget;
    nsWriteSegmentFun         mWriter;  // for implementing ReadSegments
    void                      *mClosure; // for implementing ReadSegments
    PRLock                    *mLock; // synchronize access to mSinkIsValid
    bool                      mSinkIsValid; // False if TeeWriteEvent fails 
};

class nsInputStreamTeeWriteEvent : public nsRunnable {
public:
    // aTee's lock is held across construction of this object
    nsInputStreamTeeWriteEvent(const char *aBuf, PRUint32 aCount,
                               nsIOutputStream  *aSink, 
                               nsInputStreamTee *aTee)
    {
        // copy the buffer - will be free'd by dtor
        mBuf = (char *)malloc(aCount);
        if (mBuf) memcpy(mBuf, (char *)aBuf, aCount);
        mCount = aCount;
        mSink = aSink;
        PRBool isNonBlocking;
        mSink->IsNonBlocking(&isNonBlocking);
        NS_ASSERTION(isNonBlocking == PR_FALSE, "mSink is nonblocking");
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

        PRUint32 totalBytesWritten = 0;
        while (mCount) {
            nsresult rv;
            PRUint32 bytesWritten = 0;
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
        mBuf = nsnull;
    }
    
private:
    char *mBuf;
    PRUint32 mCount;
    nsCOMPtr<nsIOutputStream> mSink;
    // back pointer to the tee that created this runnable
    nsRefPtr<nsInputStreamTee> mTee;
};

nsInputStreamTee::nsInputStreamTee(): mLock(nsnull)
                                    , mSinkIsValid(true)
{
}

bool
nsInputStreamTee::SinkIsValid()
{
    nsAutoLock lock(mLock); 
    return mSinkIsValid; 
}

void
nsInputStreamTee::InvalidateSink()
{
    nsAutoLock lock(mLock);
    mSinkIsValid = false;
}

nsresult
nsInputStreamTee::TeeSegment(const char *buf, PRUint32 count)
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
        PRUint32 totalBytesWritten = 0;
        while (count) {
            PRUint32 bytesWritten = 0;
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
                                  PRUint32 offset, PRUint32 count, PRUint32 *writeCount)
{
    nsInputStreamTee *tee = reinterpret_cast<nsInputStreamTee *>(closure);

    nsresult rv = tee->mWriter(in, tee->mClosure, fromSegment, offset, count, writeCount);
    if (NS_FAILED(rv) || (*writeCount == 0)) {
        NS_ASSERTION((NS_FAILED(rv) ? (*writeCount == 0) : PR_TRUE),
                "writer returned an error with non-zero writeCount");
        return rv;
    }

    return tee->TeeSegment(fromSegment, *writeCount);
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsInputStreamTee,
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
nsInputStreamTee::Available(PRUint32 *avail)
{
    NS_ENSURE_TRUE(mSource, NS_ERROR_NOT_INITIALIZED);
    return mSource->Available(avail);
}

NS_IMETHODIMP
nsInputStreamTee::Read(char *buf, PRUint32 count, PRUint32 *bytesRead)
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
                               PRUint32 count,
                               PRUint32 *bytesRead)
{
    NS_ENSURE_TRUE(mSource, NS_ERROR_NOT_INITIALIZED);

    mWriter = writer;
    mClosure = closure;

    return mSource->ReadSegments(WriteSegmentFun, this, count, bytesRead);
}

NS_IMETHODIMP
nsInputStreamTee::IsNonBlocking(PRBool *result)
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
        PRBool nonBlocking;
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
        mLock = PR_NewLock();
        if (!mLock) {
            NS_ERROR("Failed to allocate lock for nsInputStreamTee");
            return NS_ERROR_OUT_OF_MEMORY;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsInputStreamTee::GetEventTarget(nsIEventTarget **anEventTarget)
{
    NS_IF_ADDREF(*anEventTarget = mEventTarget);
    return NS_OK;
}


NS_COM nsresult
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

NS_COM nsresult
NS_NewInputStreamTee(nsIInputStream **result,
                     nsIInputStream *source,
                     nsIOutputStream *sink)
{
    return NS_NewInputStreamTeeAsync(result, source, sink, nsnull);
}
