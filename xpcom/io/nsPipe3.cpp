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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIPipe.h"
#include "nsIEventQueue.h"
#include "nsISeekableStream.h"
#include "nsSegmentedBuffer.h"
#include "nsStreamUtils.h"
#include "nsAutoLock.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "prlog.h"

#if defined(PR_LOGGING)
//
// set NSPR_LOG_MODULES=nsPipe:5
//
static PRLogModuleInfo *gPipeLog = nsnull;
#define LOG(args) PR_LOG(gPipeLog, PR_LOG_DEBUG, args)
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

class nsPipeEvents
{
public:
    nsPipeEvents() { }
   ~nsPipeEvents();

    inline void NotifyInputReady(nsIAsyncInputStream *stream,
                                 nsIInputStreamNotify *notify)
    {
        NS_ASSERTION(!mInputNotify, "already have an input event");
        mInputStream = stream;
        mInputNotify = notify;
    }

    inline void NotifyOutputReady(nsIAsyncOutputStream *stream,
                                  nsIOutputStreamNotify *notify)
    {
        NS_ASSERTION(!mOutputNotify, "already have an output event");
        mOutputStream = stream;
        mOutputNotify = notify;
    }

private:
    nsCOMPtr<nsIAsyncInputStream>   mInputStream;
    nsCOMPtr<nsIInputStreamNotify>  mInputNotify;
    nsCOMPtr<nsIAsyncOutputStream>  mOutputStream;
    nsCOMPtr<nsIOutputStreamNotify> mOutputNotify;
};

//-----------------------------------------------------------------------------

class nsPipeInputStream : public nsIAsyncInputStream
                        , public nsISeekableStream
                        , public nsISearchableInputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIASYNCINPUTSTREAM
    NS_DECL_NSISEEKABLESTREAM
    NS_DECL_NSISEARCHABLEINPUTSTREAM

    nsPipeInputStream(nsPipe *pipe)
        : mPipe(pipe)
        , mReaderRefCnt(0)
        , mLogicalOffset(0)
        , mBlocking(PR_TRUE)
        , mBlocked(PR_FALSE)
        , mAvailable(0)
        { }
    virtual ~nsPipeInputStream() { }

    nsresult Fill();
    void SetNonBlocking(PRBool aNonBlocking) { mBlocking = !aNonBlocking; }

    PRUint32 Available() { return mAvailable; }
    void     ReduceAvailable(PRUint32 avail) { mAvailable -= avail; }

    nsresult Wait();
    PRBool   OnInputReadable(PRUint32 bytesWritten, nsPipeEvents &);
    PRBool   OnInputException(nsresult, nsPipeEvents &);

private:
    nsPipe                        *mPipe;

    // separate refcnt so that we know when to close the consumer
    nsrefcnt                       mReaderRefCnt;
    PRUint32                       mLogicalOffset;
    PRPackedBool                   mBlocking;

    // these variables can only be accessed while inside the pipe's monitor
    PRPackedBool                   mBlocked;
    PRUint32                       mAvailable;
    nsCOMPtr<nsIInputStreamNotify> mNotify;
};

//-----------------------------------------------------------------------------

class nsPipeOutputStream : public nsIAsyncOutputStream
                         , public nsISeekableStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIOUTPUTSTREAM
    NS_DECL_NSIASYNCOUTPUTSTREAM
    NS_DECL_NSISEEKABLESTREAM

    nsPipeOutputStream(nsPipe *pipe)
        : mPipe(pipe)
        , mWriterRefCnt(0)
        , mLogicalOffset(0)
        , mBlocking(PR_TRUE)
        , mBlocked(PR_FALSE)
        , mWritable(PR_TRUE)
        { }
    virtual ~nsPipeOutputStream() { }

    void SetNonBlocking(PRBool aNonBlocking) { mBlocking = !aNonBlocking; }
    void SetWritable(PRBool writable) { mWritable = writable; }

    nsresult Wait();
    PRBool   OnOutputWritable(nsPipeEvents &);
    PRBool   OnOutputException(nsresult, nsPipeEvents &);

private:
    nsPipe                         *mPipe;

    // separate refcnt so that we know when to close the producer
    nsrefcnt                        mWriterRefCnt;
    PRUint32                        mLogicalOffset;
    PRPackedBool                    mBlocking;

    // these variables can only be accessed while inside the pipe's monitor
    PRPackedBool                    mBlocked;
    PRPackedBool                    mWritable;
    nsCOMPtr<nsIOutputStreamNotify> mNotify;
};

//-----------------------------------------------------------------------------

class nsPipe : public nsIPipe
{
public:
    friend class nsPipeInputStream;
    friend class nsPipeOutputStream;

    NS_DECL_ISUPPORTS
    NS_DECL_NSIPIPE

    // nsPipe methods:
    nsPipe();
    virtual ~nsPipe();

    PRMonitor *Monitor() { return mMonitor; }
    nsPipeInputStream* InputStream() { return &mInput; }
    nsPipeOutputStream* OutputStream() { return &mOutput; }

    //
    // methods below may only be called while inside the pipe's monitor
    //

    nsresult PeekSegment(PRUint32 n, char *&cursor, char *&limit);

    //
    // methods below may be called while outside the pipe's monitor
    //
 
    nsresult GetReadSegment(const char *&segment, PRUint32 &segmentLen);
    void     AdvanceReadCursor(PRUint32 count);

    nsresult GetWriteSegment(char *&segment, PRUint32 &segmentLen);
    void     AdvanceWriteCursor(PRUint32 count);

    void     OnPipeException(nsresult reason, PRBool outputOnly = PR_FALSE);

protected:
    // We can't inherit from both nsIInputStream and nsIOutputStream
    // because they collide on their Close method. Consequently we nest their
    // implementations to avoid the extra object allocation.
    nsPipeInputStream   mInput;
    nsPipeOutputStream  mOutput;

    PRMonitor*          mMonitor;
    nsSegmentedBuffer   mBuffer;

    char*               mReadCursor;
    char*               mReadLimit;

    PRInt32             mWriteSegment;
    char*               mWriteCursor;
    char*               mWriteLimit;

    nsresult            mStatus;
};

//-----------------------------------------------------------------------------
// nsPipe methods:
//-----------------------------------------------------------------------------

nsPipe::nsPipe()
    : mInput(this)
    , mOutput(this)
    , mMonitor(nsnull)
    , mReadCursor(nsnull)
    , mReadLimit(nsnull)
    , mWriteSegment(-1)
    , mWriteCursor(nsnull)
    , mWriteLimit(nsnull)
    , mStatus(NS_OK)
{
    NS_INIT_ISUPPORTS();
}

nsPipe::~nsPipe()
{
    if (mMonitor)
        PR_DestroyMonitor(mMonitor);
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsPipe, nsIPipe)

NS_IMETHODIMP
nsPipe::Init(PRBool nonBlockingIn,
             PRBool nonBlockingOut,
             PRUint32 segmentSize,
             PRUint32 segmentCount,
             nsIMemory *segmentAlloc)
{
    mMonitor = PR_NewMonitor();
    if (!mMonitor)
        return NS_ERROR_OUT_OF_MEMORY;

    if (segmentSize == 0)
        segmentSize = DEFAULT_SEGMENT_SIZE;
    if (segmentCount == 0)
        segmentCount = DEFAULT_SEGMENT_COUNT;

    // protect against overflow
    PRUint32 maxCount = PRUint32(-1) / segmentSize;
    if (segmentCount > maxCount)
        segmentCount = maxCount;

    nsresult rv = mBuffer.Init(segmentSize, segmentSize * segmentCount, segmentAlloc);
    if (NS_FAILED(rv))
        return rv;

    InputStream()->SetNonBlocking(nonBlockingIn);
    OutputStream()->SetNonBlocking(nonBlockingOut);
    return NS_OK;
}

NS_IMETHODIMP
nsPipe::GetInputStream(nsIAsyncInputStream **aInputStream)
{
    NS_ADDREF(*aInputStream = &mInput);
    return NS_OK;
}

NS_IMETHODIMP
nsPipe::GetOutputStream(nsIAsyncOutputStream **aOutputStream)
{
    NS_ADDREF(*aOutputStream = &mOutput);
    return NS_OK;
}

nsresult
nsPipe::PeekSegment(PRUint32 index, char *&cursor, char *&limit)
{
    if (mReadCursor == mWriteCursor)
        return NS_FAILED(mStatus) ? mStatus : NS_BASE_STREAM_WOULD_BLOCK;
    
    if (index == 0 && mReadCursor) {
        cursor = mReadCursor;
        limit = mReadLimit;
    }
    else {
        PRUint32 numSegments = mBuffer.GetSegmentCount();
        if (index >= numSegments)
            return NS_FAILED(mStatus) ? mStatus : NS_BASE_STREAM_WOULD_BLOCK;
        cursor = mBuffer.GetSegment(index);
        if (mWriteSegment == (PRInt32) index)
            limit = mWriteCursor;
        else
            limit = cursor + mBuffer.GetSegmentSize();
    }
    return NS_OK;
}

nsresult
nsPipe::GetReadSegment(const char *&segment, PRUint32 &segmentLen)
{
    nsAutoMonitor mon(mMonitor);

    char *cursor, *limit;
    nsresult rv = PeekSegment(0, cursor, limit);
    if (NS_FAILED(rv)) return rv;

    if (mReadCursor == nsnull) {
        mReadCursor = cursor;
        mReadLimit = limit;
    }

    NS_ASSERTION(mReadCursor && mReadLimit, "WTF");

    segment    = mReadCursor;
    segmentLen = mReadLimit - mReadCursor;
    return NS_OK;
}

void
nsPipe::AdvanceReadCursor(PRUint32 bytesRead)
{
    NS_ASSERTION(bytesRead, "dont call if no bytes read");

    nsPipeEvents events;
    {
        nsAutoMonitor mon(mMonitor);

        LOG(("III advancing read cursor by %u\n", bytesRead));

        mReadCursor += bytesRead;
        NS_ASSERTION(mReadCursor <= mReadLimit, "read cursor exceeds limit");

        if ((mWriteSegment == 0) && (mReadCursor > mWriteCursor))
            NS_ERROR("read cursor exceeds write cursor");

        InputStream()->ReduceAvailable(bytesRead);

        if (mReadCursor == mReadLimit) {
            // if still writing in this segment then bail
            if (mWriteSegment == 0 && mWriteLimit > mWriteCursor) {
                // reset write cursor to start of segment
                mWriteCursor = mBuffer.GetSegment(0);
                mReadCursor = nsnull;
                mReadLimit = nsnull;
                return;
            }

            // done with this segment
            mBuffer.DeleteFirstSegment();
            LOG(("III deleting first segment\n"));

            mReadCursor = nsnull;
            mReadLimit = nsnull;
            
            --mWriteSegment; // can become -1

            // notify output stream that pipe is writable
            if (OutputStream()->OnOutputWritable(events))
                mon.Notify();
        }
    }
}

nsresult
nsPipe::GetWriteSegment(char *&segment, PRUint32 &segmentLen)
{
    nsAutoMonitor mon(mMonitor);

    if (NS_FAILED(mStatus))
        return mStatus;

    if (mWriteCursor == mWriteLimit) {
        char *seg = mBuffer.AppendNewSegment();
        // pipe is full
        if (seg == nsnull)
            return NS_BASE_STREAM_WOULD_BLOCK;
        LOG(("OOO appended new segment\n"));
        mWriteCursor = seg;
        mWriteLimit = mWriteCursor + mBuffer.GetSegmentSize();
        ++mWriteSegment;
    }
    segment    = mWriteCursor;
    segmentLen = mWriteLimit - mWriteCursor;
    return NS_OK;
}

void
nsPipe::AdvanceWriteCursor(PRUint32 bytesWritten)
{
    NS_ASSERTION(bytesWritten, "dont call if no bytes written");

    nsPipeEvents events;
    {
        nsAutoMonitor mon(mMonitor);

        LOG(("OOO advancing write cursor by %u\n", bytesWritten));

        char *newWriteCursor = mWriteCursor + bytesWritten;
        NS_ASSERTION(newWriteCursor <= mWriteLimit, "write cursor exceeds limit");

        // update read limit
        if (mReadLimit == mWriteCursor)
            mReadLimit = newWriteCursor;

        mWriteCursor = newWriteCursor;

        if (mReadCursor == mWriteCursor)
            NS_ERROR("read cursor is bad");

        // update the writable flag on the output stream
        if (mWriteCursor == mWriteLimit) {
            if (mBuffer.GetSize() >= mBuffer.GetMaxSize())
                OutputStream()->SetWritable(PR_FALSE);
        }

        // notify input stream that pipe now contains additional data
        if (InputStream()->OnInputReadable(bytesWritten, events))
            mon.Notify();
    }
}

void
nsPipe::OnPipeException(nsresult reason, PRBool outputOnly)
{
    LOG(("PPP nsPipe::OnPipeException [reason=%x output-only=%d]\n",
        reason, outputOnly));

    nsPipeEvents events;
    {
        nsAutoMonitor mon(mMonitor);

        // if we've already hit an exception, then ignore this one.
        if (NS_FAILED(mStatus))
            return;

        mStatus = reason;

        // an output-only exception applies to the input end if the pipe has
        // zero bytes available.
        if (outputOnly && !InputStream()->Available())
            outputOnly = PR_FALSE;

        if (!outputOnly)
            if (InputStream()->OnInputException(reason, events))
                mon.Notify();

        if (OutputStream()->OnOutputException(reason, events))
            mon.Notify();
    }
}

//-----------------------------------------------------------------------------
// nsPipeEvents methods:
//-----------------------------------------------------------------------------

nsPipeEvents::~nsPipeEvents()
{
    // dispatch any pending events

    if (mInputNotify) {
        mInputNotify->OnInputStreamReady(mInputStream);
        mInputNotify = 0;
        mInputStream = 0;
    }
    if (mOutputNotify) {
        mOutputNotify->OnOutputStreamReady(mOutputStream);
        mOutputNotify = 0;
        mOutputStream = 0;
    }
}

//-----------------------------------------------------------------------------
// nsPipeInputStream methods:
//-----------------------------------------------------------------------------

nsresult
nsPipeInputStream::Wait()
{
    NS_ASSERTION(mBlocking, "wait on non-blocking pipe input stream");

    nsAutoMonitor mon(mPipe->Monitor());

    while (NS_SUCCEEDED(mPipe->mStatus) && (mAvailable == 0)) {
        LOG(("III pipe input: waiting for data\n"));

        mBlocked = PR_TRUE;
        mon.Wait();
        mBlocked = PR_FALSE;

        LOG(("III pipe input: woke up [pipe-status=%x available=%u]\n",
            mPipe->mStatus, mAvailable));
    }

    return mPipe->mStatus == NS_BASE_STREAM_CLOSED ? NS_OK : mPipe->mStatus;
}

PRBool
nsPipeInputStream::OnInputReadable(PRUint32 bytesWritten, nsPipeEvents &events)
{
    PRBool result = PR_FALSE;

    mAvailable += bytesWritten;

    if (mNotify) {
        events.NotifyInputReady(this, mNotify);
        mNotify = 0;
    }
    else if (mBlocked)
        result = PR_TRUE;

    return result;
}

PRBool
nsPipeInputStream::OnInputException(nsresult reason, nsPipeEvents &events)
{
    LOG(("nsPipeInputStream::OnInputException [this=%x reason=%x]\n",
        this, reason));

    PRBool result = PR_FALSE;

    NS_ASSERTION(NS_FAILED(reason), "huh? successful exception");

    if (mNotify) {
        events.NotifyInputReady(this, mNotify);
        mNotify = 0;
    }
    else if (mBlocked)
        result = PR_TRUE;

    return result;
}

NS_IMETHODIMP_(nsrefcnt)
nsPipeInputStream::AddRef(void)
{
    ++mReaderRefCnt;
    return mPipe->AddRef();
}

NS_IMETHODIMP_(nsrefcnt)
nsPipeInputStream::Release(void)
{
    if (--mReaderRefCnt == 0)
        Close();
    return mPipe->Release();
}

NS_IMPL_QUERY_INTERFACE4(nsPipeInputStream,
                         nsIInputStream,
                         nsIAsyncInputStream,
                         nsISeekableStream,
                         nsISearchableInputStream)

NS_IMETHODIMP
nsPipeInputStream::CloseEx(nsresult reason)
{
    LOG(("III CloseEx [this=%x reason=%x]\n", this, reason));

    if (NS_SUCCEEDED(reason))
        reason = NS_BASE_STREAM_CLOSED;

    mPipe->OnPipeException(reason);
    return NS_OK;
}

NS_IMETHODIMP
nsPipeInputStream::Close()
{
    return CloseEx(NS_BASE_STREAM_CLOSED);
}

NS_IMETHODIMP
nsPipeInputStream::Available(PRUint32 *result)
{
    nsAutoMonitor mon(mPipe->Monitor());

    // return error if pipe closed
    if (!mAvailable && NS_FAILED(mPipe->mStatus))
        return mPipe->mStatus;

    *result = mAvailable;
    return NS_OK;
}

NS_IMETHODIMP
nsPipeInputStream::ReadSegments(nsWriteSegmentFun writer, 
                                void *closure,  
                                PRUint32 count,
                                PRUint32 *readCount)
{
    LOG(("III ReadSegments [this=%x count=%u]\n", this, count));

    nsresult rv = NS_OK;

    const char *segment;
    PRUint32 segmentLen;

    *readCount = 0;
    while (count) {
        rv = mPipe->GetReadSegment(segment, segmentLen);
        if (NS_FAILED(rv)) {
            // ignore this error if we've already read something.
            if (*readCount > 0) {
                rv = NS_OK;
                break;
            }
            if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
                // pipe is empty
                if (!mBlocking)
                    break;
                // wait for some data to be written to the pipe
                rv = Wait();
                if (NS_SUCCEEDED(rv))
                    continue;
            }
            // ignore this error, just return.
            if (rv == NS_BASE_STREAM_CLOSED) {
                rv = NS_OK;
                break;
            }
            mPipe->OnPipeException(rv);
            break;
        }

        // read no more than count
        if (segmentLen > count)
            segmentLen = count;

        PRUint32 writeCount, originalLen = segmentLen;
        while (segmentLen) {
            writeCount = 0;

            rv = writer(this, closure, segment, *readCount, segmentLen, &writeCount);

            if (NS_FAILED(rv) || (writeCount == 0)) {
                count = 0;
                // any errors returned from the writer end here: do not
                // propogate to the caller of ReadSegments.
                rv = NS_OK;
                break;
            }

            segment += writeCount;
            segmentLen -= writeCount;
            count -= writeCount;
            *readCount += writeCount;
            mLogicalOffset += writeCount;
        }

        if (segmentLen < originalLen)
            mPipe->AdvanceReadCursor(originalLen - segmentLen);
    }

    return rv;
}

static NS_METHOD
nsWriteToRawBuffer(nsIInputStream* inStr,
                   void *closure,
                   const char *fromRawSegment,
                   PRUint32 offset,
                   PRUint32 count,
                   PRUint32 *writeCount)
{
    char *toBuf = (char*)closure;
    memcpy(&toBuf[offset], fromRawSegment, count);
    *writeCount = count;
    return NS_OK;
}

NS_IMETHODIMP
nsPipeInputStream::Read(char* toBuf, PRUint32 bufLen, PRUint32 *readCount)
{
    return ReadSegments(nsWriteToRawBuffer, toBuf, bufLen, readCount);
}

NS_IMETHODIMP
nsPipeInputStream::IsNonBlocking(PRBool *aNonBlocking)
{
    *aNonBlocking = !mBlocking;
    return NS_OK;
}

NS_IMETHODIMP
nsPipeInputStream::AsyncWait(nsIInputStreamNotify *notify,
                             PRUint32 aRequestedCount,
                             nsIEventQueue *eventQ)
{
    LOG(("III AsyncWait [this=%x]\n", this));

    nsPipeEvents pipeEvents;
    {
        nsAutoMonitor mon(mPipe->Monitor());

        // replace a pending notify
        mNotify = 0;

        nsCOMPtr<nsIInputStreamNotify> proxy;
        if (eventQ) {
            nsresult rv = NS_NewInputStreamReadyEvent(getter_AddRefs(proxy),
                                                      notify, eventQ);
            if (NS_FAILED(rv)) return rv;
            notify = proxy;
        }

        if (NS_FAILED(mPipe->mStatus) || mAvailable) {
            // stream is already closed or readable; post event.
            pipeEvents.NotifyInputReady(this, notify);
        }
        else {
            // queue up notify object to be notified when data becomes available
            mNotify = notify;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsPipeInputStream::Seek(PRInt32 whence, PRInt32 offset)
{
    NS_NOTREACHED("nsPipeInputStream::Seek");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPipeInputStream::Tell(PRUint32 *offset)
{
    *offset = mLogicalOffset;
    return NS_OK;
}

NS_IMETHODIMP
nsPipeInputStream::SetEOF()
{
    NS_NOTREACHED("nsPipeInputStream::SetEOF");
    return NS_ERROR_NOT_IMPLEMENTED;
}

#define COMPARE(s1, s2, i)                                                 \
    (ignoreCase                                                            \
     ? nsCRT::strncasecmp((const char *)s1, (const char *)s2, (PRUint32)i) \
     : nsCRT::strncmp((const char *)s1, (const char *)s2, (PRUint32)i))

NS_IMETHODIMP
nsPipeInputStream::Search(const char *forString, 
                          PRBool ignoreCase,
                          PRBool *found,
                          PRUint32 *offsetSearchedTo)
{
    LOG(("III Search [for=%s ic=%u]\n", forString, ignoreCase));

    nsAutoMonitor mon(mPipe->Monitor());

    nsresult rv;
    char *cursor1, *limit1;
    PRUint32 index = 0, offset = 0;
    PRUint32 strLen = strlen(forString);

    rv = mPipe->PeekSegment(0, cursor1, limit1);
    if (NS_FAILED(rv) || cursor1 == limit1) {
        *found = PR_FALSE;
        *offsetSearchedTo = 0;
        LOG(("  result [found=%u offset=%u]\n", *found, *offsetSearchedTo));
        return NS_OK;
    }

    while (PR_TRUE) {
        PRUint32 i, len1 = limit1 - cursor1;

        // check if the string is in the buffer segment
        for (i = 0; i < len1 - strLen + 1; i++) {
            if (COMPARE(&cursor1[i], forString, strLen) == 0) {
                *found = PR_TRUE;
                *offsetSearchedTo = offset + i;
                LOG(("  result [found=%u offset=%u]\n", *found, *offsetSearchedTo));
                return NS_OK;
            }
        }

        // get the next segment
        char *cursor2, *limit2;
        PRUint32 len2;

        index++;
        offset += len1;

        rv = mPipe->PeekSegment(index, cursor2, limit2);
        if (NS_FAILED(rv) || cursor2 == limit2) {
            *found = PR_FALSE;
            // if we're unable to get the next segment because of some pipe
            // exception, then we should just inform that caller that all
            // data has been searched (even though it hasn't).
            if (rv != NS_BASE_STREAM_WOULD_BLOCK)
                *offsetSearchedTo = offset;
            else
                *offsetSearchedTo = offset - strLen + 1;
            LOG(("  result [found=%u offset=%u]\n", *found, *offsetSearchedTo));
            return NS_OK;
        }
        len2 = limit2 - cursor2;

        // check if the string is straddling the next buffer segment
        PRUint32 lim = PR_MIN(strLen, len2 + 1);
        for (i = 0; i < lim; ++i) {
            PRUint32 strPart1Len = strLen - i - 1;
            PRUint32 strPart2Len = strLen - strPart1Len;
            const char* strPart2 = &forString[strLen - strPart2Len];
            PRUint32 bufSeg1Offset = len1 - strPart1Len;
            if (COMPARE(&cursor1[bufSeg1Offset], forString, strPart1Len) == 0 &&
                COMPARE(cursor2, strPart2, strPart2Len) == 0) {
                *found = PR_TRUE;
                *offsetSearchedTo = offset - strPart1Len;
                LOG(("  result [found=%u offset=%u]\n", *found, *offsetSearchedTo));
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

nsresult
nsPipeOutputStream::Wait()
{
    NS_ASSERTION(mBlocking, "wait on non-blocking pipe output stream");

    nsAutoMonitor mon(mPipe->Monitor());

    if (NS_SUCCEEDED(mPipe->mStatus) && !mWritable) {
        LOG(("OOO pipe output: waiting for space\n"));
        mBlocked = PR_TRUE;
        mon.Wait();
        mBlocked = PR_FALSE;
        LOG(("OOO pipe output: woke up [pipe-status=%x writable=%u]\n",
            mPipe->mStatus, mWritable == PR_TRUE));
    }

    return mPipe->mStatus == NS_BASE_STREAM_CLOSED ? NS_OK : mPipe->mStatus;
}

PRBool
nsPipeOutputStream::OnOutputWritable(nsPipeEvents &events)
{
    PRBool result = PR_FALSE;

    mWritable = PR_TRUE;

    if (mNotify) {
        events.NotifyOutputReady(this, mNotify);
        mNotify = 0;
    }
    else if (mBlocked)
        result = PR_TRUE;

    return result;
}

PRBool
nsPipeOutputStream::OnOutputException(nsresult reason, nsPipeEvents &events)
{
    LOG(("nsPipeOutputStream::OnOutputException [this=%x reason=%x]\n",
        this, reason));

    nsresult result = PR_FALSE;

    NS_ASSERTION(NS_FAILED(reason), "huh? successful exception");
    mWritable = PR_FALSE;

    if (mNotify) {
        events.NotifyOutputReady(this, mNotify);
        mNotify = 0;
    }
    else if (mBlocked)
        result = PR_TRUE;

    return result;
}


NS_IMETHODIMP_(nsrefcnt)
nsPipeOutputStream::AddRef()
{
    mWriterRefCnt++;
    return mPipe->AddRef();
}

NS_IMETHODIMP_(nsrefcnt)
nsPipeOutputStream::Release()
{
    if (--mWriterRefCnt == 0)
        Close();
    return mPipe->Release();
}

NS_IMPL_QUERY_INTERFACE2(nsPipeOutputStream,
                         nsIOutputStream,
                         nsIAsyncOutputStream)

NS_IMETHODIMP
nsPipeOutputStream::CloseEx(nsresult reason)
{
    LOG(("OOO CloseEx [this=%x reason=%x]\n", this, reason));

    if (NS_SUCCEEDED(reason))
        reason = NS_BASE_STREAM_CLOSED;

    // input stream may remain open
    mPipe->OnPipeException(reason, PR_TRUE);
    return NS_OK;
}

NS_IMETHODIMP
nsPipeOutputStream::Close()
{
    return CloseEx(NS_BASE_STREAM_CLOSED);
}

NS_IMETHODIMP
nsPipeOutputStream::WriteSegments(nsReadSegmentFun reader,
                                  void* closure,
                                  PRUint32 count,
                                  PRUint32 *writeCount)
{
    LOG(("OOO WriteSegments [this=%x count=%u]\n", this, count));

    nsresult rv = NS_OK;

    char *segment;
    PRUint32 segmentLen;

    *writeCount = 0;
    while (count) {
        rv = mPipe->GetWriteSegment(segment, segmentLen);
        if (NS_FAILED(rv)) {
            if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
                // pipe is full
                if (!mBlocking) {
                    // ignore this error if we've already written something
                    if (*writeCount > 0)
                        rv = NS_OK;
                    break;
                }
                // wait for the pipe to have an empty segment.
                rv = Wait();
                if (NS_SUCCEEDED(rv))
                    continue;
            }
            mPipe->OnPipeException(rv);
            break;
        }

        // write no more than count
        if (segmentLen > count)
            segmentLen = count;

        PRUint32 readCount, originalLen = segmentLen;
        while (segmentLen) {
            readCount = 0;

            rv = reader(this, closure, segment, *writeCount, segmentLen, &readCount);

            if (NS_FAILED(rv) || (readCount == 0)) {
                count = 0;
                // any errors returned from the reader end here: do not
                // propogate to the caller of WriteSegments.
                rv = NS_OK;
                break;
            }

            segment += readCount;
            segmentLen -= readCount;
            count -= readCount;
            *writeCount += readCount;
            mLogicalOffset += readCount;
        }

        if (segmentLen < originalLen)
            mPipe->AdvanceWriteCursor(originalLen - segmentLen);
    }

    return rv;
}

static NS_METHOD
nsReadFromRawBuffer(nsIOutputStream* outStr,
                    void* closure,
                    char* toRawSegment,
                    PRUint32 offset,
                    PRUint32 count,
                    PRUint32 *readCount)
{
    const char* fromBuf = (const char*)closure;
    memcpy(toRawSegment, &fromBuf[offset], count);
    *readCount = count;
    return NS_OK;
}

NS_IMETHODIMP
nsPipeOutputStream::Write(const char* fromBuf,
                          PRUint32 bufLen, 
                          PRUint32 *writeCount)
{
    return WriteSegments(nsReadFromRawBuffer, (void*)fromBuf, bufLen, writeCount);
}

NS_IMETHODIMP
nsPipeOutputStream::Flush(void)
{
    // nothing to do
    return NS_OK;
}

static NS_METHOD
nsReadFromInputStream(nsIOutputStream* outStr,
                      void* closure,
                      char* toRawSegment, 
                      PRUint32 offset,
                      PRUint32 count,
                      PRUint32 *readCount)
{
    nsIInputStream* fromStream = (nsIInputStream*)closure;
    return fromStream->Read(toRawSegment, count, readCount);
}

NS_IMETHODIMP
nsPipeOutputStream::WriteFrom(nsIInputStream* fromStream,
                                      PRUint32 count,
                                      PRUint32 *writeCount)
{
    return WriteSegments(nsReadFromInputStream, fromStream, count, writeCount);
}

NS_IMETHODIMP
nsPipeOutputStream::IsNonBlocking(PRBool *aNonBlocking)
{
    *aNonBlocking = !mBlocking;
    return NS_OK;
}

NS_IMETHODIMP
nsPipeOutputStream::AsyncWait(nsIOutputStreamNotify *notify,
                              PRUint32 aRequestedCount,
                              nsIEventQueue *eventQ)
{
    LOG(("OOO AsyncWait [this=%x]\n", this));

    nsPipeEvents pipeEvents;
    {
        nsAutoMonitor mon(mPipe->Monitor());

        // replace a pending notify
        mNotify = 0;

        nsCOMPtr<nsIOutputStreamNotify> proxy;
        if (eventQ) {
            nsresult rv = NS_NewOutputStreamReadyEvent(getter_AddRefs(proxy),
                                                       notify, eventQ);
            if (NS_FAILED(rv)) return rv;
            notify = proxy;
        }

        if (NS_FAILED(mPipe->mStatus) || mWritable) {
            // stream is already closed or writable; post event.
            pipeEvents.NotifyOutputReady(this, notify);
        }
        else {
            // queue up notify object to be notified when data becomes available
            mNotify = notify;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsPipeOutputStream::Seek(PRInt32 whence, PRInt32 offset)
{
    NS_NOTREACHED("nsPipeOutputStream::Seek");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPipeOutputStream::Tell(PRUint32 *offset)
{
    *offset = mLogicalOffset;
    return NS_OK;
}

NS_IMETHODIMP
nsPipeOutputStream::SetEOF()
{
    NS_NOTREACHED("nsPipeOutputStream::SetEOF");
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////

NS_COM nsresult
NS_NewPipe2(nsIAsyncInputStream **pipeIn,
            nsIAsyncOutputStream **pipeOut,
            PRBool nonBlockingInput,
            PRBool nonBlockingOutput,
            PRUint32 segmentSize,
            PRUint32 segmentCount,
            nsIMemory *segmentAlloc)
{
    nsresult rv;

#if defined(PR_LOGGING)
    if (!gPipeLog)
        gPipeLog = PR_NewLogModule("nsPipe");
#endif

    nsPipe *pipe = new nsPipe();
    if (!pipe)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = pipe->Init(nonBlockingInput,
                    nonBlockingOutput,
                    segmentSize,
                    segmentCount,
                    segmentAlloc);
    if (NS_FAILED(rv)) {
        delete pipe;
        return rv;
    }

    pipe->GetInputStream(pipeIn);
    pipe->GetOutputStream(pipeOut);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
