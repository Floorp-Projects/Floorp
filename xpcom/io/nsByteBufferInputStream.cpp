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

#include "nsIByteBufferInputStream.h"
#include "nsCRT.h"

class nsByteBufferInputStream : public nsIByteBufferInputStream
{
public:
    NS_DECL_ISUPPORTS

    // nsIBaseStream methods:
    NS_IMETHOD Close(void);

    // nsIInputStream methods:
    NS_IMETHOD GetLength(PRUint32 *aLength);
    NS_IMETHOD Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount); 

    // nsIByteBufferInputStream methods:
    NS_IMETHOD Fill(nsIInputStream* stream, PRUint32 *filledAmount);
    
    // nsByteBufferInputStream methods:
    nsByteBufferInputStream();
    virtual ~nsByteBufferInputStream();

    nsresult Init(PRUint32 size);

    void ReadChunk(char* toBuf, PRUint32 max,
                   PRUint32 end, PRUint32 *totalRef)
    {
        NS_ASSERTION(mReadCursor <= end, "bad range");
        PRUint32 diff = end - mReadCursor;
        PRInt32 amt = PR_MIN(max, diff);
        if (amt > 0) {
            nsCRT::memcpy(toBuf, &mBuffer[mReadCursor], amt);
            mReadCursor += amt;
            *totalRef += amt;
        }
    }

    nsresult WriteChunk(nsIInputStream* in, PRUint32 end, PRUint32 *totalRef)
    {
        NS_ASSERTION(mWriteCursor <= end, "bad range");
        PRInt32 amt = end - mWriteCursor;
        if (amt > 0) {
            PRUint32 readAmt;
            nsresult rv = in->Read(&mBuffer[mWriteCursor], amt, &readAmt);
            if (NS_FAILED(rv)) return rv;
            mWriteCursor += readAmt;
            *totalRef += readAmt;
        }
        return NS_OK;
    }

protected:
    char*       mBuffer;
    PRUint32    mLength;
    PRUint32    mReadCursor;
    PRUint32    mWriteCursor;
    PRBool      mFull;
};

////////////////////////////////////////////////////////////////////////////////

nsByteBufferInputStream::nsByteBufferInputStream()
    : mBuffer(nsnull), mLength(0), mReadCursor(0), mWriteCursor(0), mFull(PR_FALSE)
{
    NS_INIT_REFCNT();
}

nsresult
nsByteBufferInputStream::Init(PRUint32 size)
{
    mBuffer = new char[size];
    if (mBuffer == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    mLength = size;
    return NS_OK;
}

nsByteBufferInputStream::~nsByteBufferInputStream()
{
    Close();
}

NS_IMPL_ADDREF(nsByteBufferInputStream);
NS_IMPL_RELEASE(nsByteBufferInputStream);

NS_IMETHODIMP
nsByteBufferInputStream::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (aInstancePtr == nsnull)
        return NS_ERROR_NULL_POINTER;
    if (aIID.Equals(nsIByteBufferInputStream::GetIID()) ||
        aIID.Equals(nsIInputStream::GetIID()) ||
        aIID.Equals(nsIBaseStream::GetIID())) {
        *aInstancePtr = this;
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsByteBufferInputStream::Close(void)
{
    if (mBuffer == nsnull)
        return NS_BASE_STREAM_CLOSED;

    delete mBuffer;
    mBuffer = nsnull;
    mLength = 0;
    mReadCursor = 0;
    mWriteCursor = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsByteBufferInputStream::GetLength(PRUint32 *aLength)
{
    if (mBuffer == nsnull)
        return NS_BASE_STREAM_CLOSED;

    if (mReadCursor < mWriteCursor) 
        *aLength = mWriteCursor - mReadCursor;
    else
        *aLength = (mLength - mReadCursor) + mWriteCursor;
    return NS_OK;
}

NS_IMETHODIMP
nsByteBufferInputStream::Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount)
{
    if (mBuffer == nsnull)
        return NS_BASE_STREAM_CLOSED;

    // wrap-around buffer:
    if (mReadCursor == mWriteCursor && !mFull) {
        return NS_BASE_STREAM_EOF;
    }

    *aReadCount = 0;
    if (mReadCursor >= mWriteCursor || mFull) {
        PRUint32 amt = 0;
        ReadChunk(aBuf, aCount, mLength, &amt);
        *aReadCount += amt;
        if (mReadCursor == mLength) {
            mReadCursor = 0;
            aBuf += amt;
            aCount -= amt;
            ReadChunk(aBuf, aCount, mWriteCursor, aReadCount);
            if (mReadCursor == mWriteCursor)
                mFull = PR_FALSE;
        }
    }
    else {
        ReadChunk(aBuf, aCount, mWriteCursor, aReadCount);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsByteBufferInputStream::Fill(nsIInputStream* stream, PRUint32 *filledAmount)
{
    if (mBuffer == nsnull)
        return NS_BASE_STREAM_CLOSED;

    nsresult rv;

    // wrap-around buffer:
    *filledAmount = 0;
    if (mReadCursor <= mWriteCursor && !mFull) {
        rv = WriteChunk(stream, mLength, filledAmount);
        if (NS_FAILED(rv)) return rv;
        if (mWriteCursor == mLength) {
            mWriteCursor = 0;
            rv = WriteChunk(stream, mReadCursor, filledAmount);
            if (NS_FAILED(rv)) return rv;
            if (mWriteCursor == mReadCursor)
                mFull = PR_TRUE;
        }
    }
    else {
        rv = WriteChunk(stream, mReadCursor, filledAmount);
        if (NS_FAILED(rv)) return rv;
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_BASE nsresult
NS_NewByteBufferInputStream(PRUint32 size, nsIByteBufferInputStream* *result)
{
    nsByteBufferInputStream* str =
        new nsByteBufferInputStream();
    if (str == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    nsresult rv = str->Init(size);
    if (NS_FAILED(rv)) {
        delete str;
        return rv;
    }
    NS_ADDREF(str);
    *result = str;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
