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

#include "nsByteBufferInputStream.h"
#include "prcmon.h"

NS_IMETHODIMP
nsDummyBufferStream::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    NS_NOTREACHED("nsDummyBufferStream::QueryInterface");
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP_(nsrefcnt)
nsDummyBufferStream::AddRef(void)
{
    NS_NOTREACHED("nsDummyBufferStream::AddRef");
    return 1;
}

NS_IMETHODIMP_(nsrefcnt)
nsDummyBufferStream::Release(void)
{
    NS_NOTREACHED("nsDummyBufferStream::Release");
    return 1;
}

////////////////////////////////////////////////////////////////////////////////
// nsByteBufferInputStream methods:
////////////////////////////////////////////////////////////////////////////////

nsByteBufferInputStream::nsByteBufferInputStream(PRBool blocking, PRUint32 size)
    : mBuffer(nsnull), mLength(size), mReadCursor(0), mWriteCursor(0), 
      mFull(PR_FALSE), mClosed(PR_FALSE), mEOF(PR_FALSE), mBlocking(blocking)
{
    NS_INIT_REFCNT();
}

nsresult
nsByteBufferInputStream::Init(void)
{
    mBuffer = new char[mLength];
    if (mBuffer == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

nsByteBufferInputStream::~nsByteBufferInputStream()
{
    (void)Close();
    if (mBuffer) delete mBuffer;
}

NS_IMPL_ISUPPORTS3(nsByteBufferInputStream, nsIByteBufferInputStream, nsIInputStream, nsIBaseStream)

NS_IMETHODIMP
nsByteBufferInputStream::Close(void)
{
    nsresult rv = NS_OK;
    if (mBlocking) 
        PR_CEnterMonitor(this);
    mClosed = PR_TRUE;
    if (mBlocking) {
        PRStatus status = PR_CNotify(this);   // wake up the writer
        if (status != PR_SUCCESS)
            rv = NS_ERROR_FAILURE;
        PR_CExitMonitor(this);
    }
    return rv;
}

NS_IMETHODIMP
nsByteBufferInputStream::GetLength(PRUint32 *aLength)
{
    if (mClosed)
        return NS_BASE_STREAM_CLOSED;

    if (mBlocking) 
        PR_CEnterMonitor(this);
    *aLength = ReadableAmount();
    if (mBlocking) 
        PR_CExitMonitor(this);
    return NS_OK;
}

NS_IMETHODIMP
nsByteBufferInputStream::Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount)
{
    nsresult rv = NS_OK;
    if (mClosed)
        return NS_BASE_STREAM_CLOSED;

    if (AtEOF())
        return NS_BASE_STREAM_EOF;

    if (mBlocking) 
        PR_CEnterMonitor(this);
    *aReadCount = 0;

    /*while (aCount > 0)*/ {
        if (ReadableAmount() == 0) {
            if (mBlocking) {
                PRStatus status = PR_CWait(this, PR_INTERVAL_NO_TIMEOUT);
                if (status != PR_SUCCESS) {
                    rv = NS_ERROR_FAILURE;
                    goto done;
                }
            }
            if (mEOF) {
                rv = NS_BASE_STREAM_EOF;
                goto done;
            }
            else if (!mBlocking) {
                rv = NS_BASE_STREAM_WOULD_BLOCK;
                goto done;
            }
        }

        // wrap-around buffer:
        PRUint32 amt = 0;
        if (mReadCursor >= mWriteCursor || mFull) {
            ReadChunk(aBuf, aCount, mLength, &amt);
            *aReadCount += amt;
            aBuf += amt;
            aCount -= amt;
            if (mReadCursor == mLength) {
                mReadCursor = 0;
                amt = 0;
                ReadChunk(aBuf, aCount, mWriteCursor, &amt);
                *aReadCount += amt;
                aBuf += amt;
                aCount -= amt;
            }
        }
        else {
            ReadChunk(aBuf, aCount, mWriteCursor, &amt);
            *aReadCount += amt;
            aBuf += amt;
            aCount -= amt;
        }
        if (*aReadCount)
            mFull = PR_FALSE;

        if (mBlocking) {
            PRStatus status = PR_CNotify(this);   // tell the writer there's space
            if (status != PR_SUCCESS)
                rv = NS_ERROR_FAILURE;
        }
    }
  done:
    if (mBlocking) 
        PR_CExitMonitor(this);
    return rv;
}

NS_IMETHODIMP
nsByteBufferInputStream::Fill(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount)
{
    nsDummyBufferStream in(aBuf, aCount);
    return Fill(&in, aCount, aWriteCount);
}

NS_IMETHODIMP
nsByteBufferInputStream::Fill(nsIInputStream* stream, PRUint32 aCount, PRUint32 *aWriteCount)
{
    nsresult rv = NS_OK;

    if (mClosed || mEOF)
        return NS_BASE_STREAM_CLOSED;

    *aWriteCount = 0;

    if (mBlocking) 
        PR_CEnterMonitor(this);

    while (aCount > 0) {
        if (WritableAmount() == 0) {
            if (mBlocking) {
                PRStatus status = PR_CWait(this, PR_INTERVAL_NO_TIMEOUT);
                if (status != PR_SUCCESS) {
                    rv = NS_ERROR_FAILURE;
                    goto done;
                }
            }
            if (mClosed) {
                rv = NS_BASE_STREAM_CLOSED;
                goto done;
            }
            else if (!mBlocking) {
                rv = NS_BASE_STREAM_WOULD_BLOCK;
                goto done;
            }
        }

        // wrap-around buffer:
        PRUint32 amt = 0;
        if (mReadCursor <= mWriteCursor && !mFull) {
            rv = WriteChunk(stream, mLength, &amt);
            if (NS_FAILED(rv)) goto done;
            *aWriteCount += amt;
            aCount -= amt;
            if (mWriteCursor == mLength) {
                mWriteCursor = 0;
                amt = 0;
                rv = WriteChunk(stream, mReadCursor, &amt);
                if (NS_FAILED(rv)) goto done;
                *aWriteCount += amt;
                aCount -= amt;
            }
        }
        else {
            rv = WriteChunk(stream, mReadCursor, &amt);
            if (NS_FAILED(rv)) goto done;
            *aWriteCount += amt;
            aCount -= amt;
        }
        if (mWriteCursor == mReadCursor)
            mFull = PR_TRUE;

        if (mBlocking) {
            PRStatus status = PR_CNotify(this);   // tell the reader there's more
            if (status != PR_SUCCESS)
                rv = NS_ERROR_FAILURE;
        }
    }
  done:
    if (mBlocking) 
        PR_CExitMonitor(this);
    return rv;
}

nsresult
nsByteBufferInputStream::SetEOF()
{
    nsresult rv = NS_OK;
    if (mBlocking) 
        PR_CEnterMonitor(this);
    mEOF = PR_TRUE;
    if (mBlocking) {
        PRStatus status = PR_CNotify(this);   // wake up the reader
        if (status != PR_SUCCESS)
            rv = NS_ERROR_FAILURE;
        PR_CExitMonitor(this);
    }
    return rv;
}

nsresult
nsByteBufferInputStream::Drain()
{
    nsresult rv = NS_OK;
    if (mBlocking) {
        PR_CEnterMonitor(this);
        while (ReadableAmount() != 0) {
            PRStatus status = PR_CNotify(this);   // wake up the reader
            if (status != PR_SUCCESS) {
                rv = NS_ERROR_FAILURE;
                break;
            }
            else {
                // wait for the reader to take all the data
                status = PR_CWait(this, PR_INTERVAL_NO_TIMEOUT);
                if (status != PR_SUCCESS) {
                    rv = NS_ERROR_FAILURE;
                    break;
                }
            }
        }
        PR_CExitMonitor(this);
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsByteBufferOutputStream methods:
////////////////////////////////////////////////////////////////////////////////

nsByteBufferOutputStream::nsByteBufferOutputStream(nsByteBufferInputStream* in)
    : mInputStream(in)
{
    NS_INIT_REFCNT();
    NS_ADDREF(mInputStream);
}

nsByteBufferOutputStream::~nsByteBufferOutputStream()
{
    (void)Close();
    NS_IF_RELEASE(mInputStream);
}

NS_IMPL_ISUPPORTS2(nsByteBufferOutputStream, nsIOutputStream, nsIBaseStream)

NS_IMETHODIMP
nsByteBufferOutputStream::Close(void)
{
    mInputStream->SetEOF();
    return NS_OK;
}

NS_IMETHODIMP
nsByteBufferOutputStream::Write(const char* aBuf, PRUint32 aCount,
                                PRUint32 *aWriteCount)
{
    return mInputStream->Fill(aBuf, aCount, aWriteCount);
}

NS_IMETHODIMP
nsByteBufferOutputStream::WriteFrom(nsIInputStream* fromStream, PRUint32 aCount,
                                    PRUint32 *aWriteCount)
{
    return mInputStream->Fill(fromStream, aCount, aWriteCount);
}

NS_IMETHODIMP
nsByteBufferOutputStream::Flush(void)
{
    return mInputStream->Drain();
}

////////////////////////////////////////////////////////////////////////////////

NS_COM nsresult
NS_NewByteBufferInputStream(nsIByteBufferInputStream* *result,
                            PRBool blocking, PRUint32 size)
{
    nsresult rv;
    nsByteBufferInputStream* inStr = nsnull;

    inStr = new nsByteBufferInputStream(blocking, size);
    if (inStr == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    rv = inStr->Init();
    if (NS_FAILED(rv)) {
        delete inStr;
        return rv;
    }
    NS_ADDREF(inStr);
    *result = inStr;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_COM nsresult
NS_NewPipe(nsIInputStream* *inStrResult,
           nsIOutputStream* *outStrResult,
           PRBool blocking, PRUint32 size)
{
    nsresult rv;
    nsIByteBufferInputStream* in;
    nsByteBufferInputStream* inStr;
    nsByteBufferOutputStream* outStr;

    rv = NS_NewByteBufferInputStream(&in, blocking, size);
    if (NS_FAILED(rv)) return rv;
    // this cast is safe, because we know how NS_NewByteBufferInputStream works:
    inStr = NS_STATIC_CAST(nsByteBufferInputStream*, in);
    
    outStr = new nsByteBufferOutputStream(inStr);
    if (outStr == nsnull) {
        NS_RELEASE(inStr);
        return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(outStr);

    *inStrResult = inStr;
    *outStrResult = outStr;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
