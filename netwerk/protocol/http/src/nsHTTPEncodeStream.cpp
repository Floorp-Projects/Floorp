/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsHTTPEncodeStream.h"
#include "nsIHTTPProtocolHandler.h"

#include "prmem.h"

#define MAX_BUF_SIZE 2048

////////////////////////////////////////////////////////////////////////////////
// nsHTTPEncodeStream methods:

nsHTTPEncodeStream::nsHTTPEncodeStream()
    : mInput(nsnull)
    , mFlags(nsIHTTPProtocolHandler::ENCODE_NORMAL)
    , mLastLineComplete(PR_TRUE)
    , mBuf(nsnull)
    , mBufCursor(0)
    , mBufUnread(0)
{
    NS_INIT_ISUPPORTS();
}

nsresult
nsHTTPEncodeStream::Init(nsIInputStream* in, PRUint32 flags)
{
    mFlags = flags;
    mInput = in;
    return NS_OK;
}

nsHTTPEncodeStream::~nsHTTPEncodeStream()
{
    if (mBuf)
        PR_Free(mBuf);
}

NS_IMPL_THREADSAFE_ADDREF(nsHTTPEncodeStream);
NS_IMPL_THREADSAFE_RELEASE(nsHTTPEncodeStream);

NS_IMPL_QUERY_INTERFACE2(nsHTTPEncodeStream, nsIInputStream, nsISeekableStream);

NS_METHOD
nsHTTPEncodeStream::Create(nsIInputStream *rawStream, PRUint32 flags,
                           nsIInputStream **result)
{
    nsHTTPEncodeStream* str = new nsHTTPEncodeStream();
    if (str == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(str);
    nsresult rv = str->Init(rawStream, flags);
    if (NS_FAILED(rv)) {
        NS_RELEASE(str);
        return rv;
    }
    *result = str;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIBaseStream methods:

NS_IMETHODIMP
nsHTTPEncodeStream::Close(void)
{
    return mInput->Close();
}

////////////////////////////////////////////////////////////////////////////////
// nsIInputStream methods:

NS_IMETHODIMP
nsHTTPEncodeStream::Available(PRUint32 *result)
{
    // XXX Ugh! This requires buffering up the translation so that you can
    // count it, because to walk it consumes the input.
    NS_NOTREACHED("Available");
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsHTTPEncodeStream::GetData(char* buf, PRUint32 bufLen, PRUint32 *readCount)
{
    *readCount = 0;
    PRInt32 len = mPushBackBuffer.Length();
    PRUint32 amt = PR_MIN((PRUint32)len, bufLen);
    if (amt > 0) {
        mPushBackBuffer.ToCString(buf,amt,0);
        buf += amt;
        bufLen -= amt;
        *readCount += amt;
        mPushBackBuffer.Cut(0, amt);
    }

    nsresult rv = NS_OK;
    if (bufLen > 0) {
        // get more from the input stream
        rv = mInput->Read(buf, bufLen, &amt);
        *readCount += amt;
    }
    return rv;
}

nsresult
nsHTTPEncodeStream::PushBack(nsString& data)
{
    mPushBackBuffer.Insert(data, 0);
    return NS_OK;
}

nsresult
nsHTTPEncodeStream::ReadNext(char* outBuf, PRUint32 outBufCnt, PRUint32 *result)
{
    nsresult rv;
#define BUF_SIZE 1024
    char readBuf[BUF_SIZE];
    PRUint32 amt = 0;

    *result = 0;

    while (outBufCnt > 0) {
        PRUint32 readCnt = PR_MIN(outBufCnt, BUF_SIZE);
        rv = GetData(readBuf, readCnt, &amt);
        if (NS_FAILED(rv) || (amt == 0)) return rv;

        *result += amt;
        
        if (mFlags & nsIHTTPProtocolHandler::ENCODE_QUOTE_LINES) {
            // If this line begins with "." so we need to quote it
            // by adding another "." to the beginning of the line.
            nsCAutoString str(readBuf);
            while (PR_TRUE) {
                PRInt32 pos = str.Find("\012."); // LF .
                if (pos == -1)
                    break;

                PRUint32 cnt = PR_MIN((PRUint32)pos + 2, // +2 for LF and dot
                                      outBufCnt - 1);    // -1 for extra dot
                nsCRT::memcpy(outBuf, readBuf, cnt);
                outBuf += cnt;
                outBufCnt -= cnt;
                str.Cut(0, cnt);
            }
        }

        nsCRT::memcpy(outBuf, readBuf, amt);
        outBuf += amt;
        outBufCnt -= amt;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPEncodeStream::Read(char *buf, PRUint32 count, PRUint32 *countRead)
{
    NS_ASSERTION(buf, "null pointer");
    NS_ASSERTION(countRead, "null pointer");

    *countRead = 0;

    PRUint32 amt = 0;

    // Read from the buffer if it is not empty
    if (mBufUnread) {
        amt = PR_MIN(count, mBufUnread);
        nsCRT::memcpy(buf, mBuf + mBufCursor, amt);
        mBufCursor += amt;
        mBufUnread -= amt;
        *countRead = amt;
        // If we've read all that was requested then return
        if (count == amt)
            return NS_OK;
        // Fall through...
    }
    // Read from the source data stream
    nsresult rv = ReadNext(buf + amt, count - amt, &amt);
    *countRead += amt;
    return rv;
}

NS_IMETHODIMP
nsHTTPEncodeStream::ReadSegments(nsWriteSegmentFun writer, void * closure,
                                 PRUint32 count, PRUint32 *countRead)
{
    NS_ASSERTION(writer, "null pointer");
    NS_ASSERTION(countRead, "null pointer");

    nsresult rv;
    PRUint32 offset = 0;

    *countRead = 0;

    while (count) {
        // Read from the buffer if it is not empty
        while (mBufUnread) {
            PRUint32 amt = PR_MIN(count, mBufUnread);
            rv = writer(this, closure, mBuf + mBufCursor, offset, amt, &amt);
            if (rv == NS_BASE_STREAM_WOULD_BLOCK)
                return NS_OK; // mask this error
            if (NS_FAILED(rv))
                return rv;
            if (amt == 0)
                return NS_OK;
            mBufCursor += amt;
            mBufUnread -= amt;
            count -= amt;
            *countRead += amt;
            offset += amt;
        }
        // Fill the buffer with more data if the read hasn't been satisfied
        if (count) {
            // Allocate the buffer if not already allocated
            if (!mBuf) {
                mBuf = (char *) PR_Malloc(MAX_BUF_SIZE);
                if (!mBuf)
                    return NS_ERROR_OUT_OF_MEMORY;
            }
            // Reset the cursor to the start of the buffer
            mBufCursor = 0;
            // Fill the buffer 
            rv = ReadNext(mBuf, MAX_BUF_SIZE, &mBufUnread);
            if (NS_FAILED(rv)) return rv;
            // Break out of this loop, if we didn't add anything to the buffer
            if (mBufUnread == 0)
                break;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPEncodeStream::GetNonBlocking(PRBool *aNonBlocking)
{
    NS_NOTREACHED("GetNonBlocking");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTTPEncodeStream::GetObserver(nsIInputStreamObserver * *aObserver)
{
    NS_NOTREACHED("GetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTTPEncodeStream::SetObserver(nsIInputStreamObserver * aObserver)
{
    NS_NOTREACHED("SetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsISeekableStream methods:

NS_IMETHODIMP
nsHTTPEncodeStream::Seek(PRInt32 whence, PRInt32 offset)
{
    nsresult rv;
    nsCOMPtr<nsISeekableStream> ras = do_QueryInterface(mInput, &rv);
    if (NS_FAILED(rv)) return rv;

    // Clear the ReadSegments buffer
    mBufUnread = 0;

    mPushBackBuffer.SetLength(0);
    return ras->Seek(whence, offset);
}

NS_IMETHODIMP
nsHTTPEncodeStream::Tell(PRUint32* outWhere)
{
    nsresult rv;
    nsCOMPtr<nsISeekableStream> ras = do_QueryInterface(mInput, &rv);
    if (NS_FAILED(rv)) return rv;

    // Our current stream position depends on the size of the ReadSegments buffer
    return ras->Tell(outWhere) - mBufUnread;
}

////////////////////////////////////////////////////////////////////////////////
