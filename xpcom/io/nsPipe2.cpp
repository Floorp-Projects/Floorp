/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIPipe.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsSegmentedBuffer.h"
#include "nsAutoLock.h"
#include "nsIServiceManager.h"
#ifdef PAGE_MANAGER
#include "nsIPageManager.h"
#endif
#include "nsCRT.h"
#include "nsCOMPtr.h"

// define to include temporary code which prevents a common startup crash
// (Bugzilla bug 46267) without addressing the unerlying issue.
#define MASK_PREMATURE_STREAM_RELEASE

////////////////////////////////////////////////////////////////////////////////

#define GET_INPUTSTREAM_PIPE(_this) \
    ((nsPipe*)((char*)(_this) - offsetof(nsPipe, mInput)))

#define GET_OUTPUTSTREAM_PIPE(_this) \
    ((nsPipe*)((char*)(_this) - offsetof(nsPipe, mOutput)))

////////////////////////////////////////////////////////////////////////////////

class nsPipe : public nsIPipe
{
public:
    // We can't inherit from both nsIInputStream and nsIOutputStream
    // because they collide on their Close method. Consequently we nest their
    // implementations to avoid the extra object allocation, and speed up method
    // invocation between them and the nsPipe's buffer  manipulation methods.

    class nsPipeInputStream : public nsIInputStream, public nsISearchableInputStream {
    public:
        NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr); 
        NS_IMETHOD_(nsrefcnt) AddRef(void); 
        NS_IMETHOD_(nsrefcnt) Release(void); 
        // nsIBaseStream methods:
        NS_IMETHOD Close(void);
        // nsIInputStream methods:
        NS_IMETHOD Available(PRUint32 *result);
        NS_IMETHOD Read(char* toBuf, PRUint32 bufLen, PRUint32 *readCount);
        // nsIInputStream methods:
        NS_IMETHOD ReadSegments(nsWriteSegmentFun writer, void* closure, PRUint32 count,
                                PRUint32 *readCount);
        NS_IMETHOD Search(const char *forString, PRBool ignoreCase, PRBool *found,
                          PRUint32 *offsetSearchedTo);
        NS_IMETHOD GetNonBlocking(PRBool *aNonBlocking);
        NS_IMETHOD SetNonBlocking(PRBool aNonBlocking);
        NS_IMETHOD GetObserver(nsIInputStreamObserver* *result) {
            *result = mObserver;
            NS_IF_ADDREF(*result);
            return NS_OK;
        }

        NS_IMETHOD SetObserver(nsIInputStreamObserver* obs) {
            mObserver = obs;
            return NS_OK;
        }

        nsPipeInputStream() : mReaderRefCnt(0), mBlocking(PR_TRUE) {}
        nsresult Fill();

    protected:
        nsrefcnt        mReaderRefCnt; // separate refcnt so that we know when to close the consumer
        PRBool          mBlocking;
        nsCOMPtr<nsIInputStreamObserver> mObserver;
    };

    class nsPipeOutputStream : public nsIOutputStream {
    public:
        NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr); 
        NS_IMETHOD_(nsrefcnt) AddRef(void); 
        NS_IMETHOD_(nsrefcnt) Release(void); 
        // nsIBaseStream methods:
        NS_IMETHOD Close(void);
        NS_IMETHOD Write(const char* fromBuf, PRUint32 bufLen, PRUint32 *writeCount);
        NS_IMETHOD Flush(void);
        // nsIOutputStream methods:
        NS_IMETHOD WriteSegments(nsReadSegmentFun reader, void* closure, PRUint32 count,
                                 PRUint32 *writeCount);
        NS_IMETHOD WriteFrom(nsIInputStream* fromStream, PRUint32 count, PRUint32 *writeCount);
        NS_IMETHOD GetNonBlocking(PRBool *aNonBlocking);
        NS_IMETHOD SetNonBlocking(PRBool aNonBlocking);
        NS_IMETHOD GetObserver(nsIOutputStreamObserver* *result) {
            *result = mObserver;
            NS_IF_ADDREF(*result);
            return NS_OK;
        }

        NS_IMETHOD SetObserver(nsIOutputStreamObserver* obs) {
            mObserver = obs;
            return NS_OK;
        }

        nsPipeOutputStream() : mWriterRefCnt(0), mBlocking(PR_TRUE) {}

    protected:
        nsrefcnt        mWriterRefCnt; // separate refcnt so that we know when to close the producer
        PRBool          mBlocking;
        nsCOMPtr<nsIOutputStreamObserver> mObserver;
    };

    friend class nsPipeInputStream;
    friend class nsPipeOutputStream;

    NS_DECL_ISUPPORTS

    // nsIPipe methods:
    NS_IMETHOD Initialize(PRUint32 segmentSize, PRUint32 maxSize,
                          PRBool nonBlockingIn, PRBool nonBlockingOut,
                          nsIMemory *segmentAllocator) {
        nsresult rv;
        mMonitor = PR_NewMonitor();
        if (mMonitor == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        rv = mBuffer.Init(segmentSize, maxSize, segmentAllocator);
        GetInputStream()->SetNonBlocking(nonBlockingIn);
        GetOutputStream()->SetNonBlocking(nonBlockingOut);
        return NS_OK;
    }

    NS_IMETHOD GetInputStream(nsIInputStream * *aInputStream) {
        *aInputStream = &mInput;
        NS_IF_ADDREF(*aInputStream);
        return NS_OK;
    }

    NS_IMETHOD GetOutputStream(nsIOutputStream * *aOutputStream) {
        *aOutputStream = &mOutput;
        NS_IF_ADDREF(*aOutputStream);
        return NS_OK;
    }

    // nsPipe methods:
    nsPipe();
    virtual ~nsPipe();

    PRMonitor* GetMonitor() { return mMonitor; }
    nsPipeInputStream* GetInputStream() { return &mInput; }
    nsPipeOutputStream* GetOutputStream() { return &mOutput; }

    nsresult GetReadSegment(PRUint32 segmentLogicalOffset, 
                            const char* *resultSegment,
                            PRUint32 *resultSegmentLen);
    nsresult GetWriteSegment(char* *resultSegment,
                             PRUint32 *resultSegmentLen);

protected:
    PRMonitor*          mMonitor;
    nsPipeInputStream   mInput;
    nsPipeOutputStream  mOutput;

    nsSegmentedBuffer   mBuffer;

    char*               mReadCursor;
    char*               mReadLimit;

    char*               mWriteCursor;
    char*               mWriteLimit;

    nsresult            mCondition;
};

////////////////////////////////////////////////////////////////////////////////
// nsPipe methods:

nsPipe::nsPipe()
    : mMonitor(nsnull),
      mReadCursor(nsnull),
      mReadLimit(nsnull),
      mWriteCursor(nsnull),
      mWriteLimit(nsnull),
      mCondition(NS_OK)
{
    NS_INIT_REFCNT();
}

nsPipe::~nsPipe()
{
    if (mMonitor) PR_DestroyMonitor(mMonitor);
}

NS_IMPL_THREADSAFE_ADDREF(nsPipe);
NS_IMPL_THREADSAFE_RELEASE(nsPipe);

NS_IMETHODIMP
nsPipe::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (aInstancePtr == nsnull)
        return NS_ERROR_NULL_POINTER;
    if (aIID.Equals(NS_GET_IID(nsIInputStream))) {
        nsIInputStream* in = GetInputStream();
        NS_ADDREF(in);
        *aInstancePtr = in;
        return NS_OK;
    }
    if (aIID.Equals(NS_GET_IID(nsIOutputStream))) {
        nsIOutputStream* out = GetOutputStream();
        NS_ADDREF(out);
        *aInstancePtr = out;
        return NS_OK;
    }
    if (aIID.Equals(NS_GET_IID(nsIPipe)) ||
        aIID.Equals(NS_GET_IID(nsISupports))) {
        *aInstancePtr = this;
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

nsresult
nsPipe::GetReadSegment(PRUint32 segmentLogicalOffset, 
                       const char* *resultSegment,
                       PRUint32 *resultSegmentLen)
{
    nsAutoMonitor mon(mMonitor);

    PRInt32 offset = (PRInt32)segmentLogicalOffset;
    PRInt32 segCount = mBuffer.GetSegmentCount();
    for (PRInt32 i = 0; i < segCount; i++) {
        char* segStart = mBuffer.GetSegment(i);
        char* segEnd = segStart + mBuffer.GetSegmentSize();
        if (mReadCursor == nsnull) {
            mReadCursor = segStart;
            mReadLimit = segEnd;
        }
        else if (segStart <= mReadCursor && mReadCursor < segEnd) {
            segStart = mReadCursor;
            NS_ASSERTION(i == 0, "read cursor not in first segment");
        }
        if (segStart <= mWriteCursor && mWriteCursor < segEnd) {
            segEnd = mWriteCursor;
            NS_ASSERTION(i == segCount - 1, "write cursor not in last segment");
        }

        PRInt32 amt = segEnd - segStart;
        if (offset < amt) {
            // segmentLogicalOffset is in this segment, so read up to its end
            *resultSegmentLen = amt - offset;
            *resultSegment = segStart + offset;
            return *resultSegmentLen == 0 ? mCondition : NS_OK;
        }
        offset -= amt;
    }
    *resultSegmentLen = 0;
    *resultSegment = nsnull;
    return *resultSegmentLen == 0 ? mCondition : NS_OK;
}

nsresult
nsPipe::GetWriteSegment(char* *resultSegment,
                        PRUint32 *resultSegmentLen)
{
    nsAutoMonitor mon(mMonitor);

    *resultSegment = nsnull;
    *resultSegmentLen = 0;
    if (mWriteCursor == nsnull ||
        mWriteCursor == mWriteLimit) {
        char* seg = mBuffer.AppendNewSegment();
        if (seg == nsnull) {
            // buffer is full
            return NS_OK;
        }
        mWriteCursor = seg;
        mWriteLimit = seg + mBuffer.GetSegmentSize();
    }

    *resultSegment = mWriteCursor;
    *resultSegmentLen = mWriteLimit - mWriteCursor;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsPipeInputStream methods:

NS_IMETHODIMP
nsPipe::nsPipeInputStream::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (aIID.Equals(NS_GET_IID(nsISearchableInputStream))) 
  {
    nsISearchableInputStream* in = NS_STATIC_CAST(nsISearchableInputStream *, this);
    NS_ADDREF(in);
    *aInstancePtr = in;
    return NS_OK;
  }
  else
    return GET_INPUTSTREAM_PIPE(this)->QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP_(nsrefcnt)
nsPipe::nsPipeInputStream::AddRef(void)
{
    mReaderRefCnt++;
    return GET_INPUTSTREAM_PIPE(this)->AddRef();
}

NS_IMETHODIMP_(nsrefcnt)
nsPipe::nsPipeInputStream::Release(void)
{
    if (--mReaderRefCnt == 0)
        Close();
    return GET_INPUTSTREAM_PIPE(this)->Release();
}

NS_IMETHODIMP
nsPipe::nsPipeInputStream::Close(void)
{
    nsPipe* pipe = GET_INPUTSTREAM_PIPE(this);
    nsAutoMonitor mon(pipe->GetMonitor());

    pipe->mCondition = NS_BASE_STREAM_CLOSED;
    pipe->mBuffer.Empty();
    pipe->mWriteCursor = nsnull;
    pipe->mWriteLimit = nsnull;
    if (mObserver) {
        nsCOMPtr<nsIInputStreamObserver> obs = mObserver;
        mon.Exit();     // XXXbe avoid deadlock better
        nsresult rv = obs->OnClose(this);
        mon.Enter();
        NS_ASSERTION(NS_SUCCEEDED(rv), "OnClose failed");
        // don't return error from OnClose -- its not our problem
        mObserver = nsnull;       // so we don't call OnClose again
    }
    return NS_OK;
}

NS_IMETHODIMP
nsPipe::nsPipeInputStream::Available(PRUint32 *result)
{
    nsPipe* pipe = GET_INPUTSTREAM_PIPE(this);
    nsAutoMonitor mon(pipe->GetMonitor());

    PRUint32 len = pipe->mBuffer.GetSize();
    if (pipe->mReadCursor) 
        len -= pipe->mBuffer.GetSegmentSize() - (pipe->mReadLimit - pipe->mReadCursor);
    if (pipe->mWriteCursor)
        len -= pipe->mWriteLimit - pipe->mWriteCursor;

    *result = len;
    return NS_OK;
}

NS_IMETHODIMP
nsPipe::nsPipeInputStream::ReadSegments(nsWriteSegmentFun writer, 
                                        void* closure,  
                                        PRUint32 count,
                                        PRUint32 *readCount)
{
    nsPipe* pipe = GET_INPUTSTREAM_PIPE(this);
    nsAutoMonitor mon(pipe->GetMonitor());

    nsresult rv = NS_OK;
    PRUint32 readBufferLen;
    const char* readBuffer;

    *readCount = 0;
    PRUint32 amt = count;
    while (amt > 0) {
        rv = pipe->GetReadSegment(0, &readBuffer, &readBufferLen);
        if (NS_FAILED(rv))
            goto done;
        if (readBufferLen == 0) {
            rv = pipe->mCondition;
            if (*readCount > 0 || NS_FAILED(rv))
                goto done;      // don't Fill if we've got something
            if (mObserver) {
                mon.Exit();     // XXXbe avoid deadlock better
                rv = mObserver->OnEmpty(this);
                mon.Enter();
                if (NS_FAILED(rv)) goto done;
            }
            rv = Fill();
            if (/*rv == NS_BASE_STREAM_WOULD_BLOCK || */NS_FAILED(rv)) 
                goto done;
            // else we filled the pipe, so go around again
            continue;
        }

        readBufferLen = PR_MIN(readBufferLen, amt);
        while (readBufferLen > 0) {
            PRUint32 writeCount = 0;
            mon.Exit(); // XXX avoid deadlock better
            rv = writer(this, closure, readBuffer, *readCount, readBufferLen, &writeCount);
            mon.Enter();
            if (NS_FAILED(rv) && rv != NS_BASE_STREAM_WOULD_BLOCK)
                goto done;
            NS_ASSERTION(writeCount <= readBufferLen, "writer returned bad writeCount");
#ifdef DEBUG
            if (writeCount > 0 && rv == NS_BASE_STREAM_WOULD_BLOCK)
                NS_WARNING("Invalid writer implementation: cannot write data and return WOULD_BLOCK");
#endif
            readBuffer += writeCount;
            readBufferLen -= writeCount;
            *readCount += writeCount;
            amt -= writeCount;
            pipe->mReadCursor += writeCount;
            if (writeCount == 0 || rv == NS_BASE_STREAM_WOULD_BLOCK) {
                rv = pipe->mCondition;
                if (*readCount > 0 || NS_FAILED(rv))
                    goto done;      // don't Fill if we've got something
                rv = Fill();
                if (/*rv == NS_BASE_STREAM_WOULD_BLOCK || */NS_FAILED(rv))
                    goto done;
                // else we filled the pipe, so go around again
                continue;
            }
        }
        if (pipe->mReadCursor == pipe->mReadLimit) {
            pipe->mReadCursor = nsnull;
            pipe->mReadLimit = nsnull;
            PRBool empty = pipe->mBuffer.DeleteFirstSegment();
            if (empty && mObserver) {
                mon.Exit();     // XXXbe avoid deadlock better
                rv = mObserver->OnEmpty(this);
                mon.Enter();
                mon.Notify();   // wake up writer
                if (NS_FAILED(rv))
                    goto done;
            }
        }
        else if (pipe->mReadCursor == pipe->mWriteCursor) {
            if (mObserver) {
                mon.Exit();     // XXXbe avoid deadlock better
                rv = mObserver->OnEmpty(this);
                mon.Enter();
                if (NS_FAILED(rv))
                    goto done;
            }
        }
    }
  done:

    if (mBlocking && rv == NS_BASE_STREAM_WOULD_BLOCK && *readCount > 0) {
        mon.Notify();   // wake up writer
    }

    if (rv == NS_BASE_STREAM_CLOSED) {    // EOF
        if (mObserver) {
            nsCOMPtr<nsIInputStreamObserver> obs = mObserver;
            mon.Exit();     // XXXbe avoid deadlock better
            nsresult rv2 = obs->OnClose(this);
            mon.Enter();
            NS_ASSERTION(NS_SUCCEEDED(rv2), "OnClose failed");
            // don't return error from OnClose -- its not our problem
            mObserver = nsnull;       // so we don't call OnClose again
        }
        rv = NS_OK;
    }
    NS_ASSERTION(*readCount <= count, "read more than expected");
    return *readCount == 0 ? rv : NS_OK;
}

nsresult 
nsPipe::nsPipeInputStream::Fill()
{
    nsPipe* pipe = GET_INPUTSTREAM_PIPE(this);
    nsAutoMonitor mon(pipe->GetMonitor());

    nsresult rv;
    while (PR_TRUE) {
        // check read buffer again while in the monitor
        PRUint32 amt;
        const char* buf;
        rv = pipe->GetReadSegment(0, &buf, &amt);
        if (NS_FAILED(rv) || amt > 0) return rv;

        // else notify the writer and wait
        rv = mon.Notify();
        if (NS_FAILED(rv)) return rv;   // interrupted
        if (mBlocking) {
            rv = mon.Wait();
            if (NS_FAILED(rv)) return rv;   // interrupted
        }
        else {
            return NS_BASE_STREAM_WOULD_BLOCK;
        }
        // loop again so that we end up exiting on EOF with
        // the right error
    }
    return NS_OK;
}

static NS_METHOD
nsWriteToRawBuffer(nsIInputStream* inStr,
                   void* closure,
                   const char* fromRawSegment,
                   PRUint32 offset,
                   PRUint32 count,
                   PRUint32 *writeCount)
{
    char* toBuf = (char*)closure;
    nsCRT::memcpy(&toBuf[offset], fromRawSegment, count);
    *writeCount = count;
    return NS_OK;
}

NS_IMETHODIMP
nsPipe::nsPipeInputStream::Read(char* toBuf, PRUint32 bufLen, PRUint32 *readCount)
{
    return ReadSegments(nsWriteToRawBuffer, toBuf, bufLen, readCount);
}

#define COMPARE(s1, s2, i)                                                 \
    (ignoreCase                                                            \
     ? nsCRT::strncasecmp((const char *)s1, (const char *)s2, (PRUint32)i) \
     : nsCRT::strncmp((const char *)s1, (const char *)s2, (PRUint32)i))

NS_IMETHODIMP
nsPipe::nsPipeInputStream::Search(const char *forString, 
                                  PRBool ignoreCase,
                                  PRBool *found,
                                  PRUint32 *offsetSearchedTo)
{
    nsPipe* pipe = GET_INPUTSTREAM_PIPE(this);

    nsresult rv;
    const char* bufSeg1;
    PRUint32 bufSegLen1;
    PRUint32 segmentPos = 0;
    PRUint32 strLen = nsCRT::strlen(forString);

    rv = pipe->GetReadSegment(segmentPos, &bufSeg1, &bufSegLen1);
    if (NS_FAILED(rv) || bufSegLen1 == 0) {
        *found = PR_FALSE;
        *offsetSearchedTo = segmentPos;
        return NS_OK;
    }

    while (PR_TRUE) {
        PRUint32 i;
        // check if the string is in the buffer segment
        for (i = 0; i < bufSegLen1 - strLen + 1; i++) {
            if (COMPARE(&bufSeg1[i], forString, strLen) == 0) {
                *found = PR_TRUE;
                *offsetSearchedTo = segmentPos + i;
                return NS_OK;
            }
        }

        // get the next segment
        const char* bufSeg2;
        PRUint32 bufSegLen2;
        segmentPos += bufSegLen1;
        rv = pipe->GetReadSegment(segmentPos, &bufSeg2, &bufSegLen2);
        if (NS_FAILED(rv) || bufSegLen2 == 0) {
            *found = PR_FALSE;
            if (NS_FAILED(pipe->mCondition))
                *offsetSearchedTo = segmentPos;
            else
                *offsetSearchedTo = segmentPos - strLen + 1;
            return NS_OK;
        }

        // check if the string is straddling the next buffer segment
        PRUint32 limit = PR_MIN(strLen, bufSegLen2 + 1);
        for (i = 0; i < limit; i++) {
            PRUint32 strPart1Len = strLen - i - 1;
            PRUint32 strPart2Len = strLen - strPart1Len;
            const char* strPart2 = &forString[strLen - strPart2Len];
            PRUint32 bufSeg1Offset = bufSegLen1 - strPart1Len;
            if (COMPARE(&bufSeg1[bufSeg1Offset], forString, strPart1Len) == 0 &&
                COMPARE(bufSeg2, strPart2, strPart2Len) == 0) {
                *found = PR_TRUE;
                *offsetSearchedTo = segmentPos - strPart1Len;
                return NS_OK;
            }
        }

        // finally continue with the next buffer
        bufSeg1 = bufSeg2;
        bufSegLen1 = bufSegLen2;
    }
    NS_NOTREACHED("can't get here");
    return NS_ERROR_FAILURE;    // keep compiler happy
}

NS_IMETHODIMP
nsPipe::nsPipeInputStream::GetNonBlocking(PRBool *aNonBlocking)
{
    *aNonBlocking = !mBlocking;
    return NS_OK;
}

NS_IMETHODIMP
nsPipe::nsPipeInputStream::SetNonBlocking(PRBool aNonBlocking)
{
    mBlocking = !aNonBlocking;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsPipeOutputStream methods:

NS_IMETHODIMP
nsPipe::nsPipeOutputStream::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    return GET_OUTPUTSTREAM_PIPE(this)->QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP_(nsrefcnt)
nsPipe::nsPipeOutputStream::AddRef(void)
{
    mWriterRefCnt++;
    return GET_OUTPUTSTREAM_PIPE(this)->AddRef();
}

NS_IMETHODIMP_(nsrefcnt)
nsPipe::nsPipeOutputStream::Release(void)
{
    if (--mWriterRefCnt == 0)
        Close();
    return GET_OUTPUTSTREAM_PIPE(this)->Release();
}

NS_IMETHODIMP
nsPipe::nsPipeOutputStream::Close(void)
{
    nsPipe* pipe = GET_OUTPUTSTREAM_PIPE(this);
    nsAutoMonitor mon(pipe->GetMonitor());
    if (pipe->mCondition == NS_OK)
        pipe->mCondition = NS_BASE_STREAM_CLOSED;
    nsresult rv = mon.Notify();   // wake up the writer
    if (NS_FAILED(rv))
        return rv;
    return NS_OK;
}

NS_IMETHODIMP
nsPipe::nsPipeOutputStream::WriteSegments(nsReadSegmentFun reader,
                                          void* closure,
                                          PRUint32 count,
                                          PRUint32 *writeCount)
{
    nsresult rv = NS_OK;
#ifdef MASK_PREMATURE_STREAM_RELEASE
    nsCOMPtr<nsIOutputStream> kungFuDeathGrip(this);
#endif
    nsPipe* pipe = GET_OUTPUTSTREAM_PIPE(this);
    PRUint32 amt = count;
    {
        nsAutoMonitor mon(pipe->GetMonitor());

        *writeCount = 0;
        if (NS_FAILED(pipe->mCondition)
            && pipe->mCondition != NS_BASE_STREAM_CLOSED) {
            rv = pipe->mCondition;
            goto done;
        }

        while (amt > 0) {
            PRUint32 writeBufLen;
            char* writeBuf;
            rv = pipe->GetWriteSegment(&writeBuf, &writeBufLen);
            if (NS_FAILED(rv))
                goto done;
            if (writeBufLen == 0) {
                if (*writeCount > 0 && !mBlocking)
                    goto done;
                if (mObserver/* && *writeCount == 0*/) {
                    mon.Exit();     // XXXbe avoid deadlock better
                    rv = mObserver->OnFull(this);
                    mon.Enter();
                    if (NS_FAILED(rv)) goto done;
                }
                rv = Flush();
                if (/*rv == NS_BASE_STREAM_WOULD_BLOCK || */NS_FAILED(rv))
                    goto done;
                // else we flushed, so go around again
                continue;
            }

            writeBufLen = PR_MIN(writeBufLen, amt);
            while (writeBufLen > 0) {
                PRUint32 readCount = 0;
                mon.Exit(); // XXX avoid deadlock better
                rv = reader(this, closure, writeBuf, *writeCount, writeBufLen, &readCount);
                mon.Enter();
                if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
                    NS_ASSERTION(readCount <= writeBufLen, "reader returned bad readCount");
                    // XXX should not update counters if reader returned WOULD_BLOCK!!
                    writeBuf += readCount;
                    writeBufLen -= readCount;
                    *writeCount += readCount;
                    amt -= readCount;
                    pipe->mWriteCursor += readCount;

                    // The reader didn't have anything else to put in the buffer, so
                    // call flush to notify the guy downstream, hoping that he'll somehow
                    // wake up the guy upstream to eventually produce more data for us.
                    nsresult rv2 = Flush();
                    if (NS_FAILED(rv2)) {
                        if (rv2 == NS_BASE_STREAM_WOULD_BLOCK)
                            rv = pipe->mCondition;
                        goto done;
                    }
                    // else we flushed, so go around again
                    continue;
                }
                if (NS_FAILED(rv)) {
                    // save the failure condition so that we can get it again later
                    pipe->mCondition = rv;
                    goto done;
                }
                if (readCount == 0) { // EOF
                    pipe->mCondition = NS_BASE_STREAM_CLOSED;
                    goto done;
                }
                NS_ASSERTION(readCount <= writeBufLen, "reader returned bad readCount");
                writeBuf += readCount;
                writeBufLen -= readCount;
                *writeCount += readCount;
                amt -= readCount;
                pipe->mWriteCursor += readCount;
            }
            if (pipe->mWriteCursor == pipe->mWriteLimit) {
                pipe->mWriteCursor =  nsnull;
                pipe->mWriteLimit =  nsnull;
            }
        }
      done:

        if (mBlocking && rv == NS_BASE_STREAM_WOULD_BLOCK && *writeCount > 0) {
            mon.Notify();       // wake up reader
        }

        ;
    }   // exit monitor

    if (mObserver && *writeCount > 0) {
        mObserver->OnWrite(this, *writeCount);
    }
    NS_ASSERTION(*writeCount <= count, "wrote more than expected");
    return *writeCount == 0 ? rv : NS_OK;
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
    nsCRT::memcpy(toRawSegment, &fromBuf[offset], count);
    *readCount = count;
    return NS_OK;
}

NS_IMETHODIMP
nsPipe::nsPipeOutputStream::Write(const char* fromBuf,
                                  PRUint32 bufLen, 
                                  PRUint32 *writeCount)
{
    return WriteSegments(nsReadFromRawBuffer, (void*)fromBuf, bufLen, writeCount);
}

NS_IMETHODIMP
nsPipe::nsPipeOutputStream::Flush(void)
{
    nsPipe* pipe = GET_OUTPUTSTREAM_PIPE(this);
    nsAutoMonitor mon(pipe->GetMonitor());
    nsresult rv = NS_OK;
    PRBool firstTime = PR_TRUE;
    while (PR_TRUE) {
        // check write buffer again while in the monitor
        PRUint32 amt;
        const char* buf;
        rv = pipe->GetReadSegment(0, &buf, &amt);
        if (firstTime && amt == 0) {
            // If we think we needed to flush, yet there's nothing
            // in the buffer to read, we must have not been able to 
            // allocate any segments. 
            return NS_BASE_STREAM_WOULD_BLOCK;
        }
        if (NS_FAILED(rv) || amt == 0) return rv;
        firstTime = PR_FALSE;

        // else notify the reader and wait
        rv = mon.Notify();
        if (NS_FAILED(rv)) return rv;   // interrupted
        if (mBlocking) {
            rv = mon.Wait();
            if (NS_FAILED(rv)) return rv;   // interrupted
        }
        else {
            return NS_BASE_STREAM_WOULD_BLOCK;
        }
    }
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
nsPipe::nsPipeOutputStream::WriteFrom(nsIInputStream* fromStream,
                                      PRUint32 count,
                                      PRUint32 *writeCount)
{
    return WriteSegments(nsReadFromInputStream, fromStream, count, writeCount);
}

NS_IMETHODIMP
nsPipe::nsPipeOutputStream::GetNonBlocking(PRBool *aNonBlocking)
{
    *aNonBlocking = !mBlocking;
    return NS_OK;
}

NS_IMETHODIMP
nsPipe::nsPipeOutputStream::SetNonBlocking(PRBool aNonBlocking)
{
    mBlocking = !aNonBlocking;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

#ifdef PAGE_MANAGER
static NS_DEFINE_CID(kPageManagerCID, NS_PAGEMANAGER_CID);
#endif
static NS_DEFINE_CID(kMemoryCID, NS_MEMORY_CID);

NS_COM nsresult
NS_NewPipe(nsIInputStream* *inStrResult,
           nsIOutputStream* *outStrResult,
           PRUint32 segmentSize,
           PRUint32 maxSize,
           PRBool nonBlockingInput,
           PRBool nonBlockingOutput,
           nsIMemory* memory)
{
    nsresult rv;
    NS_ASSERTION(segmentSize > 0, "need to supply segmentSize for buffer");
    NS_ASSERTION(maxSize > 0, "need to supply maxSize for buffer");
    const nsCID* cid = &kMemoryCID;
#ifdef PAGE_MANAGER
    // Take the page manager out altogether because some unices don't
    // know how to reserve VM -- only preallocate it which takes up a lot 
    // of space.
#ifndef XP_MAC
    // Don't use page buffers on the mac because we don't really have
    // VM there, and they end up being more wasteful:
    if (segmentSize >= NS_PAGEMGR_PAGE_SIZE) {
        cid = &kPageManagerCID;
    }
#endif
#endif
    nsCOMPtr<nsIMemory> alloc = memory;
    if (alloc == nsnull) {
        alloc = do_GetService(*cid, &rv);
        if (NS_FAILED(rv)) return rv;
    }

    nsPipe* pipe = new nsPipe();
    if (pipe == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    rv = pipe->Initialize(segmentSize, maxSize, 
                          nonBlockingInput, nonBlockingOutput,
                          alloc);
    if (NS_FAILED(rv)) {
        delete pipe;
        return rv;
    }
    *inStrResult = pipe->GetInputStream();
    *outStrResult = pipe->GetOutputStream();
    NS_ADDREF(*inStrResult);
    NS_ADDREF(*outStrResult);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
