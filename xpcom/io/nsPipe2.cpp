/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsIPipe.h"
#include "nsIBufferInputStream.h"
#include "nsIBufferOutputStream.h"
#include "nsSegmentedBuffer.h"
#include "nsAutoLock.h"
#include "nsIServiceManager.h"
#include "nsIPageManager.h"

////////////////////////////////////////////////////////////////////////////////

class nsPipe : public nsIPipe
{
public:
    // We can't inherit from both nsIBufferInputStream and nsIBufferOutputStream
    // because they collide on their Close method. Consequently we nest their
    // implementations to avoid the extra object allocation, and speed up method
    // invocation between them and the nsPipe's buffer  manipulation methods.

    class nsPipeInputStream : public nsIBufferInputStream {
    public:
        NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr); 
        NS_IMETHOD_(nsrefcnt) AddRef(void); 
        NS_IMETHOD_(nsrefcnt) Release(void); 
        // nsIBaseStream methods:
        NS_IMETHOD Close(void);
        // nsIInputStream methods:
        NS_IMETHOD GetLength(PRUint32 *result);
        NS_IMETHOD Read(char* toBuf, PRUint32 bufLen, PRUint32 *readCount);
        // nsIBufferInputStream methods:
        NS_IMETHOD GetBuffer(nsIBuffer * *aBuffer) {
            return NS_ERROR_NOT_IMPLEMENTED;
        }
        NS_IMETHOD ReadSegments(nsWriteSegmentFun writer, void* closure, PRUint32 count,
                                PRUint32 *readCount);
        NS_IMETHOD Search(const char *forString, PRBool ignoreCase, PRBool *found,
                          PRUint32 *offsetSearchedTo);
        NS_IMETHOD GetNonBlocking(PRBool *aNonBlocking);
        NS_IMETHOD SetNonBlocking(PRBool aNonBlocking);

        nsPipeInputStream() : mReaderRefCnt(0), mBlocking(PR_TRUE) {}
        nsresult Fill();
    protected:
        nsrefcnt        mReaderRefCnt; // separate refcnt so that we know when to close the consumer
        PRBool          mBlocking;
    };

    class nsPipeOutputStream : public nsIBufferOutputStream {
    public:
        NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr); 
        NS_IMETHOD_(nsrefcnt) AddRef(void); 
        NS_IMETHOD_(nsrefcnt) Release(void); 
        // nsIBaseStream methods:
        NS_IMETHOD Close(void);
        NS_IMETHOD Write(const char* fromBuf, PRUint32 bufLen, PRUint32 *writeCount);
        NS_IMETHOD Flush(void);
        // nsIBufferOutputStream methods:
        NS_IMETHOD GetBuffer(nsIBuffer * *aBuffer) {
            return NS_ERROR_NOT_IMPLEMENTED;
        }
        NS_IMETHOD WriteSegments(nsReadSegmentFun reader, void* closure, PRUint32 count,
                                 PRUint32 *writeCount);
        NS_IMETHOD WriteFrom(nsIInputStream* fromStream, PRUint32 count, PRUint32 *writeCount);
        NS_IMETHOD GetNonBlocking(PRBool *aNonBlocking);
        NS_IMETHOD SetNonBlocking(PRBool aNonBlocking);

        nsPipeOutputStream() : mWriterRefCnt(0), mBlocking(PR_TRUE) {}
    protected:
        nsrefcnt        mWriterRefCnt; // separate refcnt so that we know when to close the producer
        PRBool          mBlocking;
    };

    friend class nsPipeInputStream;
    friend class nsPipeOutputStream;

    NS_DECL_ISUPPORTS

    // nsIPipe methods:
    NS_IMETHOD Initialize(PRUint32 segmentSize, PRUint32 maxSize, 
                          nsIPipeObserver *observer, nsIAllocator *segmentAllocator) {
        nsresult rv;
        rv = mBuffer.Init(segmentSize, maxSize, segmentAllocator);
        if (NS_FAILED(rv)) return rv;
        mObserver = observer;
        NS_IF_ADDREF(mObserver);
        return NS_OK;
    }

    NS_IMETHOD GetInputStream(nsIBufferInputStream * *aInputStream) {
        *aInputStream = &mInput;
        NS_IF_ADDREF(*aInputStream);
        return NS_OK;
    }

    NS_IMETHOD GetOutputStream(nsIBufferOutputStream * *aOutputStream) {
        *aOutputStream = &mOutput;
        NS_IF_ADDREF(*aOutputStream);
        return NS_OK;
    }

    NS_IMETHOD GetObserver(nsIPipeObserver* *result) {
        *result = mObserver;
        NS_IF_ADDREF(*result);
        return NS_OK;
    }

    NS_IMETHOD SetObserver(nsIPipeObserver* obs) {
        NS_IF_RELEASE(mObserver);
        mObserver = obs;
        NS_IF_ADDREF(mObserver);
        return NS_OK;
    }

    // nsPipe methods:
    nsPipe();
    virtual ~nsPipe();

    nsPipeInputStream* GetInputStream() { return &mInput; }
    nsPipeOutputStream* GetOutputStream() { return &mOutput; }

    nsresult GetReadSegment(PRUint32 segmentLogicalOffset, 
                            const char* *resultSegment,
                            PRUint32 *resultSegmentLen);
    nsresult GetWriteSegment(char* *resultSegment,
                             PRUint32 *resultSegmentLen);

protected:
    nsPipeInputStream   mInput;
    nsPipeOutputStream  mOutput;

    nsSegmentedBuffer   mBuffer;
    nsIPipeObserver*    mObserver;

    char*               mReadCursor;
    char*               mReadLimit;

    char*               mWriteCursor;
    char*               mWriteLimit;

    nsresult            mCondition;
};

#define GET_INPUTSTREAM_PIPE(_this) \
    ((nsPipe*)((char*)(_this) - offsetof(nsPipe, mInput)))

#define GET_OUTPUTSTREAM_PIPE(_this) \
    ((nsPipe*)((char*)(_this) - offsetof(nsPipe, mOutput)))

////////////////////////////////////////////////////////////////////////////////
// nsPipe methods:

nsPipe::nsPipe()
    : mObserver(nsnull),
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
    NS_IF_RELEASE(mObserver);
}

NS_IMPL_THREADSAFE_ADDREF(nsPipe);
NS_IMPL_THREADSAFE_RELEASE(nsPipe);

NS_IMETHODIMP
nsPipe::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (aInstancePtr == nsnull)
        return NS_ERROR_NULL_POINTER;
    if (aIID.Equals(nsIBufferInputStream::GetIID()) ||
        aIID.Equals(nsIInputStream::GetIID()) ||
        aIID.Equals(nsIBaseStream::GetIID())) {
        *aInstancePtr = GetInputStream();
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIBufferOutputStream::GetIID()) ||
        aIID.Equals(nsIOutputStream::GetIID()) ||
        aIID.Equals(nsIBaseStream::GetIID())) {
        *aInstancePtr = GetOutputStream();
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIPipe::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
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
    nsAutoCMonitor mon(this);

    PRInt32 offset = (PRInt32)segmentLogicalOffset;
    PRInt32 segCount = mBuffer.GetSegmentCount();
    PRBool atEnd = PR_FALSE;
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
    nsAutoCMonitor mon(this);

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
    nsAutoCMonitor mon(pipe);

    pipe->mCondition = NS_BASE_STREAM_CLOSED;
    pipe->mBuffer.Empty();
    pipe->mWriteCursor = nsnull;
    pipe->mWriteLimit = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsPipe::nsPipeInputStream::GetLength(PRUint32 *result)
{
    nsPipe* pipe = GET_INPUTSTREAM_PIPE(this);
    nsAutoCMonitor mon(pipe);

    PRUint32 len = pipe->mBuffer.GetSize();
    if (pipe->mReadCursor) {
        char* end = (pipe->mReadLimit == pipe->mWriteLimit)
            ? pipe->mWriteCursor
            : pipe->mReadLimit;
        len -= pipe->mBuffer.GetSegmentSize() - (end - pipe->mReadCursor);
    }
    if (pipe->mWriteCursor)
        len -= pipe->mWriteLimit - pipe->mWriteCursor;
    return len;
}

NS_IMETHODIMP
nsPipe::nsPipeInputStream::ReadSegments(nsWriteSegmentFun writer, 
                                        void* closure, 
                                        PRUint32 count,
                                        PRUint32 *readCount)
{
    nsPipe* pipe = GET_INPUTSTREAM_PIPE(this);
    nsAutoCMonitor mon(pipe);

    nsresult rv = NS_OK;
    PRUint32 readBufferLen;
    const char* readBuffer;

    *readCount = 0;
    while (count > 0) {
        rv = pipe->GetReadSegment(0, &readBuffer, &readBufferLen);
        if (NS_FAILED(rv))
            goto done;
        if (readBufferLen == 0) {
            rv = pipe->mCondition;
            if (*readCount > 0 || NS_FAILED(rv))
                goto done;      // don't Fill if we've got something
            if (pipe->mObserver) {
                rv = pipe->mObserver->OnEmpty(pipe);
                if (NS_FAILED(rv)) goto done;
            }
            rv = Fill();
            if (rv == NS_BASE_STREAM_WOULD_BLOCK || NS_FAILED(rv)) 
                goto done;
            // else we filled the pipe, so go around again
            continue;
        }

        readBufferLen = PR_MIN(readBufferLen, count);
        while (readBufferLen > 0) {
            PRUint32 writeCount;
            rv = writer(closure, readBuffer, *readCount, readBufferLen, &writeCount);
            NS_ASSERTION(rv != NS_BASE_STREAM_EOF, "Write should not return EOF");
            if (NS_FAILED(rv)) 
                goto done;
            if (writeCount == 0 || rv == NS_BASE_STREAM_WOULD_BLOCK) {
                rv = pipe->mCondition;
                if (*readCount > 0 || NS_FAILED(rv))
                    goto done;      // don't Fill if we've got something
                rv = Fill();
                if (rv == NS_BASE_STREAM_WOULD_BLOCK || NS_FAILED(rv))
                    goto done;
                // else we filled the pipe, so go around again
                continue;
            }
            NS_ASSERTION(writeCount <= readBufferLen, "writer returned bad writeCount");
            readBuffer += writeCount;
            readBufferLen -= writeCount;
            *readCount += writeCount;
            count -= writeCount;
            pipe->mReadCursor += writeCount;
        }
        if (pipe->mReadCursor == pipe->mReadLimit) {
            pipe->mReadCursor = nsnull;
            pipe->mReadLimit = nsnull;
            PRBool empty = pipe->mBuffer.DeleteFirstSegment();
#if 0
            if (empty && pipe->mObserver && *readCount == 0) {
                rv = pipe->mObserver->OnEmpty(pipe);
                if (NS_FAILED(rv))
                    goto done;
            }
#endif
        }
    }
  done:
    if (mBlocking && rv == NS_BASE_STREAM_WOULD_BLOCK && *readCount > 0) {
        mon.Notify();   // wake up writer
    }
    return *readCount == 0 ? rv : NS_OK;
}

nsresult 
nsPipe::nsPipeInputStream::Fill()
{
    nsPipe* pipe = GET_INPUTSTREAM_PIPE(this);
    nsAutoCMonitor mon(pipe);

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
nsWriteToRawBuffer(void* closure,
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
    if (NS_FAILED(pipe->mCondition)) {
        return pipe->mCondition;
    }

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
            if (pipe->mCondition != NS_OK)    // XXX NS_FAILED?
                *offsetSearchedTo = segmentPos - bufSegLen1;
            else
                *offsetSearchedTo = segmentPos - bufSegLen1 - strLen + 1;
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
    nsAutoCMonitor mon(pipe);
    pipe->mCondition = NS_BASE_STREAM_EOF;
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
    nsresult rv;
    nsPipe* pipe = GET_OUTPUTSTREAM_PIPE(this);
    {
        nsAutoCMonitor mon(pipe);

        *writeCount = 0;
        if (NS_FAILED(pipe->mCondition)) {
            rv = pipe->mCondition;
            goto done;
        }

        while (count > 0) {
            PRUint32 writeBufLen;
            char* writeBuf;
            rv = pipe->GetWriteSegment(&writeBuf, &writeBufLen);
            if (NS_FAILED(rv))
                goto done;
            if (writeBufLen == 0) {
                if (pipe->mObserver && *writeCount == 0) {
                    rv = pipe->mObserver->OnFull(pipe);
                    if (NS_FAILED(rv)) goto done;
                }
                rv = Flush();
                if (rv == NS_BASE_STREAM_WOULD_BLOCK || NS_FAILED(rv))
                    goto done;
                // else we flushed, so go around again
                continue;
            }

            writeBufLen = PR_MIN(writeBufLen, count);
            while (writeBufLen > 0) {
                PRUint32 readCount = 0;
                rv = reader(closure, writeBuf, *writeCount, writeBufLen, &readCount);
                if (NS_FAILED(rv)) {
                    // save the failure condition so that we can get it again later
                    pipe->mCondition = rv;
                    goto done;
                }
                if (readCount == 0) {
                    // The reader didn't have anything else to put in the buffer, so
                    // call flush to notify the guy downstream, hoping that he'll somehow
                    // wake up the guy upstream to eventually produce more data for us.
                    nsresult rv2 = Flush();
                    if (rv2 == NS_BASE_STREAM_WOULD_BLOCK || NS_FAILED(rv2))
                        goto done;
                    // else we flushed, so go around again
                    continue;
                }
                NS_ASSERTION(readCount <= writeBufLen, "reader returned bad readCount");
                writeBuf += readCount;
                writeBufLen -= readCount;
                *writeCount += readCount;
                count -= readCount;
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
    }   // exit monitor

    if (pipe->mObserver && *writeCount > 0) {
        pipe->mObserver->OnWrite(pipe, *writeCount);
    }
    return *writeCount == 0 ? rv : NS_OK;
}

static NS_METHOD
nsReadFromRawBuffer(void* closure,
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
    nsAutoCMonitor mon(pipe);
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
nsReadFromInputStream(void* closure,
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

static NS_DEFINE_CID(kPageManagerCID, NS_PAGEMANAGER_CID);
static NS_DEFINE_CID(kAllocatorCID, NS_ALLOCATOR_CID);

NS_COM nsresult
NS_NewPipe(nsIBufferInputStream* *inStrResult,
           nsIBufferOutputStream* *outStrResult,
           nsIPipeObserver* observer,
           PRUint32 segmentSize,
           PRUint32 maxSize)
{
    nsresult rv;
#ifdef XP_MAC
    // Don't use page buffers on the mac because we don't really have
    // VM there, and they end up being more wasteful:
    NS_WITH_SERVICE(nsIAllocator, alloc, kAllocatorCID, &rv);
#else
    NS_WITH_SERVICE(nsIAllocator, alloc, kPageManagerCID, &rv);
#endif
    if (NS_FAILED(rv)) return rv;

    nsPipe* pipe = new nsPipe();
    if (pipe == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    rv = pipe->Initialize(segmentSize, maxSize, observer, alloc);
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
