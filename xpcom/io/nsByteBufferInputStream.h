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

#ifndef nsByteBufferInputStream_h__
#define nsByteBufferInputStream_h__

#include "nsIByteBufferInputStream.h"
#include "nsCRT.h"

class nsByteBufferInputStream;

////////////////////////////////////////////////////////////////////////////////

class nsByteBufferOutputStream : public nsIOutputStream
{
public:
    NS_DECL_ISUPPORTS

    // nsIBaseStream methods:
    NS_DECL_NSIBASESTREAM

    // nsIOutputStream methods:
    NS_DECL_NSIOUTPUTSTREAM

    // This method isn't in nsIOutputStream...
    NS_IMETHOD WriteFrom(nsIInputStream* fromStream, PRUint32 aCount,
                         PRUint32 *aWriteCount);

    // nsByteBufferOutputStream methods:
    nsByteBufferOutputStream(nsByteBufferInputStream* in);
    virtual ~nsByteBufferOutputStream();

protected:
    nsByteBufferInputStream*  mInputStream;
};

////////////////////////////////////////////////////////////////////////////////

class nsByteBufferInputStream : public nsIByteBufferInputStream
{
public:
    NS_DECL_ISUPPORTS

    // nsIBaseStream methods:
    NS_DECL_NSIBASESTREAM

    // nsIInputStream methods:
    NS_DECL_NSIINPUTSTREAM

    // nsIByteBufferInputStream methods:
    NS_IMETHOD Fill(nsIInputStream* stream, PRUint32 aCount, PRUint32 *aWriteCount);
    NS_IMETHOD Fill(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount);

    // nsByteBufferInputStream methods:
    nsByteBufferInputStream(PRBool blocking, PRUint32 size);
    virtual ~nsByteBufferInputStream();

    friend class nsByteBufferOutputStream;

    nsresult Init(void);
    nsresult SetEOF(void);
    nsresult Drain(void);

    PRBool AtEOF() { return mEOF && (mReadCursor == mWriteCursor) && !mFull; }

    PRUint32 ReadableAmount() {
        if (mReadCursor < mWriteCursor) 
            return mWriteCursor - mReadCursor;
        else if (mReadCursor == mWriteCursor && !mFull)
            return 0;
        else
            return (mLength - mReadCursor) + mWriteCursor;
    }

    PRUint32 WritableAmount() {
        if (mWriteCursor < mReadCursor)
            return mWriteCursor - mReadCursor;
        else if (mReadCursor == mWriteCursor && mFull)
            return 0;
        else
            return (mLength - mWriteCursor) + mReadCursor;
    }

    void ReadChunk(char* toBuf, PRUint32 max,
                   PRUint32 end, PRUint32 *totalRef)
    {
        NS_ASSERTION(mReadCursor <= end, "bad range");
        PRUint32 diff = end - mReadCursor;
        PRInt32 amt = PR_MIN(max, diff);
        if (amt > 0) {
            nsCRT::memcpy(toBuf, &mBuffer[mReadCursor], amt);
#ifdef DEBUG_warren
//            nsCRT::memset(&mBuffer[mReadCursor], 0xDD, amt);
#endif
            mReadCursor += amt;
            *totalRef += amt;
        }
    }

    nsresult WriteChunk(nsIInputStream* in,
                        PRUint32 end, PRUint32 *totalRef)
    {
        NS_ASSERTION(mWriteCursor <= end, "bad range");
        PRUint32 amt = end - mWriteCursor;
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
    char*               mBuffer;
    PRUint32            mLength;
    PRUint32            mReadCursor;
    PRUint32            mWriteCursor;
    PRBool              mFull;
    PRBool              mClosed;
    PRBool              mEOF;
    PRBool              mBlocking;
};

////////////////////////////////////////////////////////////////////////////////

class nsDummyBufferStream : public nsIInputStream
{
public:
    NS_DECL_ISUPPORTS

    // nsIBaseStream methods:
    NS_IMETHOD Close(void) {
        NS_NOTREACHED("nsDummyBufferStream::Close");
        return NS_ERROR_FAILURE;
    }

    // nsIInputStream methods:
    NS_IMETHOD GetLength(PRUint32 *aLength) { 
        *aLength = mLength;
        return NS_OK;
    }
    NS_IMETHOD Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount) {
        PRUint32 amt = PR_MIN(aCount, mLength);
        if (amt > 0) {
            nsCRT::memcpy(aBuf, mBuffer, amt);
            mBuffer += amt;
            mLength -= amt;
        }
        *aReadCount = amt;
        return NS_OK;
    } 

    // nsDummyBufferStream methods:
    nsDummyBufferStream(const char* buffer, PRUint32 length)
        : mBuffer(buffer), mLength(length) {}
    virtual ~nsDummyBufferStream() {}

protected:
    const char* mBuffer;
    PRUint32    mLength;
};

#endif // nsByteBufferInputStream_h__
