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

#include "nsBufferedStreams.h"
#include "nsCRT.h"

#ifdef DEBUG_brendan
# define METERING
#endif

#ifdef METERING
# define METER(x)       x
# define MAX_BIG_SEEKS  20

static struct {
    PRUint32            mSeeksWithinBuffer;
    PRUint32            mSeeksOutsideBuffer;
    PRUint32            mBufferReadUponSeek;
    PRUint32            mBufferUnreadUponSeek;
    PRUint32            mBytesReadFromBuffer;
    PRUint32            mBigSeekIndex;
    struct {
        PRUint32        mOldOffset;
        PRUint32        mNewOffset;
    } mBigSeek[MAX_BIG_SEEKS];
} bufstats;
#else
# define METER(x)       /* nothing */
#endif

////////////////////////////////////////////////////////////////////////////////
// nsBufferedStream

nsBufferedStream::nsBufferedStream()
    : mBuffer(nsnull),
      mBufferStartOffset(0),
      mCursor(0), 
      mFillPoint(0),
      mStream(nsnull),
      mBufferDisabled(PR_FALSE),
      mGetBufferCount(0)
{
    NS_INIT_REFCNT();
}

nsBufferedStream::~nsBufferedStream()
{
    Close();
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsBufferedStream, nsISeekableStream);

nsresult
nsBufferedStream::Init(nsISupports* stream, PRUint32 bufferSize)
{
    NS_ASSERTION(stream, "need to supply a stream");
    NS_ASSERTION(mStream == nsnull, "already inited");
    mStream = stream;
    NS_ADDREF(mStream);
    mBufferSize = bufferSize;
    mBufferStartOffset = 0;
    mCursor = 0;
    mBuffer = new char[bufferSize];
    if (mBuffer == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

nsresult
nsBufferedStream::Close()
{
    nsresult rv = NS_OK;
    if (mBuffer) {
        delete[] mBuffer;
        mBuffer = nsnull;
        mBufferSize = 0;
        mBufferStartOffset = 0;
        mCursor = 0;
    }
    return rv;
}

NS_IMETHODIMP
nsBufferedStream::Seek(PRInt32 whence, PRInt32 offset)
{
    if (mStream == nsnull)
        return NS_BASE_STREAM_CLOSED;
    
    // If the underlying stream isn't a random access store, then fail early.
    // We could possibly succeed for the case where the seek position denotes
    // something that happens to be read into the buffer, but that would make
    // the failure data-dependent.
    nsresult rv;
    nsCOMPtr<nsISeekableStream> ras = do_QueryInterface(mStream, &rv);
    if (NS_FAILED(rv)) return rv;

    PRInt32 absPos;
    switch (whence) {
      case nsISeekableStream::NS_SEEK_SET:
        absPos = offset;
        break;
      case nsISeekableStream::NS_SEEK_CUR:
        absPos = mBufferStartOffset + mCursor + offset;
        break;
      case nsISeekableStream::NS_SEEK_END:
        absPos = -1;
        break;
      default:
        NS_NOTREACHED("bogus seek whence parameter");
        return NS_ERROR_UNEXPECTED;
    }

    // Let mCursor point into the existing buffer if the new position is
    // between the current cursor and the mFillPoint "fencepost" -- the
    // client may never get around to a Read or Write after this Seek.
    // Read and Write worry about flushing and filling in that event.
    PRUint32 offsetInBuffer = PRUint32(absPos - mBufferStartOffset);
    if (offsetInBuffer <= mFillPoint) {
        METER(bufstats.mSeeksWithinBuffer++);
        mCursor = offsetInBuffer;
        return NS_OK;
    }

    METER(bufstats.mSeeksOutsideBuffer++);
    METER(bufstats.mBufferReadUponSeek += mCursor);
    METER(bufstats.mBufferUnreadUponSeek += mFillPoint - mCursor);
    rv = Flush();
    if (NS_FAILED(rv)) return rv;

    rv = ras->Seek(whence, offset);
    if (NS_FAILED(rv)) return rv;

    METER(if (bufstats.mBigSeekIndex < MAX_BIG_SEEKS)
              bufstats.mBigSeek[bufstats.mBigSeekIndex].mOldOffset =
                  mBufferStartOffset + mCursor);
    if (absPos == -1) {
        // then we had the SEEK_END case, above
        rv = ras->Tell(&mBufferStartOffset);
        if (NS_FAILED(rv)) return rv;
    }
    else {
        mBufferStartOffset = absPos;
    }
    METER(if (bufstats.mBigSeekIndex < MAX_BIG_SEEKS)
              bufstats.mBigSeek[bufstats.mBigSeekIndex++].mNewOffset =
                  mBufferStartOffset);

    mFillPoint = mCursor = 0;
    return Fill();
}

NS_IMETHODIMP
nsBufferedStream::Tell(PRUint32 *result)
{
    if (mStream == nsnull)
        return NS_BASE_STREAM_CLOSED;
    
    *result = mBufferStartOffset + mCursor;
    return NS_OK;
}

NS_IMETHODIMP
nsBufferedStream::SetEOF()
{
    if (mStream == nsnull)
        return NS_BASE_STREAM_CLOSED;
    
    nsresult rv;
    nsCOMPtr<nsISeekableStream> ras = do_QueryInterface(mStream, &rv);
    if (NS_FAILED(rv)) return rv;

    return ras->SetEOF();
}

////////////////////////////////////////////////////////////////////////////////
// nsBufferedInputStream

NS_IMPL_ISUPPORTS_INHERITED3(nsBufferedInputStream, 
                             nsBufferedStream,
                             nsIInputStream,
                             nsIBufferedInputStream,
                             nsIStreamBufferAccess)

NS_METHOD
nsBufferedInputStream::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    NS_ENSURE_NO_AGGREGATION(aOuter);

    nsBufferedInputStream* stream = new nsBufferedInputStream();
    if (stream == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(stream);
    nsresult rv = stream->QueryInterface(aIID, aResult);
    NS_RELEASE(stream);
    return rv;
}

NS_IMETHODIMP
nsBufferedInputStream::Init(nsIInputStream* stream, PRUint32 bufferSize)
{
    return nsBufferedStream::Init(stream, bufferSize);
}

NS_IMETHODIMP
nsBufferedInputStream::Close()
{
    nsresult rv1 = NS_OK, rv2;
    if (mStream) {
        rv1 = Source()->Close();
        NS_RELEASE(mStream);
    }
    rv2 = nsBufferedStream::Close();
    if (NS_FAILED(rv1)) return rv1;
    return rv2;
}

NS_IMETHODIMP
nsBufferedInputStream::Available(PRUint32 *result)
{
    *result = mFillPoint - mCursor;
    return NS_OK;
}

NS_IMETHODIMP
nsBufferedInputStream::Read(char * buf, PRUint32 count, PRUint32 *result)
{
    nsresult rv;
    if (mBufferDisabled) {
        rv = Source()->Read(buf, count, result);
        if (NS_SUCCEEDED(rv))
            mBufferStartOffset += *result;  // so nsBufferedStream::Tell works
        return rv;
    }

    rv = NS_OK;
    PRUint32 read = 0;
    while (count > 0) {
        PRUint32 amt = PR_MIN(count, mFillPoint - mCursor);
        if (amt > 0) {
            nsCRT::memcpy(buf + read, mBuffer + mCursor, amt);
            read += amt;
            count -= amt;
            mCursor += amt;
        }
        else {
            rv = Fill();
            if (NS_FAILED(rv) || mFillPoint == mCursor)
                break;
        }
    }
    METER(if (read > 0) bufstats.mBytesReadFromBuffer += read);
    *result = read;
    return (read > 0) ? NS_OK : rv;
}

NS_IMETHODIMP
nsBufferedInputStream::ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *result)
{
    nsresult rv = NS_OK;
    *result = 0;
    while (count > 0) {
        PRUint32 amt = PR_MIN(count, mFillPoint - mCursor);
        if (amt > 0) {
            PRUint32 read = 0;
            rv = writer(this, closure, mBuffer + mCursor, mCursor, amt, &read);
            if (NS_FAILED(rv)) break;
            *result += read;
            count -= read;
            mCursor += read;
        }
        else {
            rv = Fill();
            if (NS_FAILED(rv) || mFillPoint == mCursor)
                break;
        }
    }
    return (*result > 0) ? NS_OK : rv;
}

NS_IMETHODIMP
nsBufferedInputStream::GetNonBlocking(PRBool *aNonBlocking)
{
    NS_NOTREACHED("GetNonBlocking");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBufferedInputStream::GetObserver(nsIInputStreamObserver * *aObserver)
{
    NS_NOTREACHED("GetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBufferedInputStream::SetObserver(nsIInputStreamObserver * aObserver)
{
    NS_NOTREACHED("SetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBufferedInputStream::Fill()
{
    if (mBufferDisabled)
        return NS_OK;

    nsresult rv;
    PRInt32 rem = PRInt32(mFillPoint - mCursor);
    if (rem > 0) {
        // slide the remainder down to the start of the buffer
        // |<------------->|<--rem-->|<--->|
        // b               c         f     s
        nsCRT::memcpy(mBuffer, mBuffer + mCursor, rem);
    }
    mBufferStartOffset += mCursor;
    mFillPoint = rem;
    mCursor = 0;

    PRUint32 amt;
    rv = Source()->Read(mBuffer + mFillPoint, mBufferSize - mFillPoint, &amt);
    if (NS_FAILED(rv)) return rv;

    mFillPoint += amt;
    return NS_OK;
}

NS_IMETHODIMP_(char*)
nsBufferedInputStream::GetBuffer(PRUint32 aLength, PRUint32 aAlignMask)
{
    NS_ASSERTION(mGetBufferCount == 0, "nested GetBuffer!");
    if (mGetBufferCount != 0)
        return nsnull;

    if (mBufferDisabled)
        return nsnull;

    char* buf = mBuffer + mCursor;
    PRUint32 rem = mFillPoint - mCursor;
    if (rem == 0) {
        if (NS_FAILED(Fill()))
            return nsnull;
        buf = mBuffer + mCursor;
        rem = mFillPoint - mCursor;
    }

    PRUint32 mod = (NS_PTR_TO_INT32(buf) & aAlignMask);
    if (mod) {
        PRUint32 pad = aAlignMask + 1 - mod;
        if (pad > rem)
            return nsnull;

        memset(buf, 0, pad);
        mCursor += pad;
        buf += pad;
        rem -= pad;
    }

    if (aLength > rem)
        return nsnull;
    mGetBufferCount++;
    return buf;
}

NS_IMETHODIMP_(void)
nsBufferedInputStream::PutBuffer(char* aBuffer, PRUint32 aLength)
{
    NS_ASSERTION(mGetBufferCount == 1, "stray PutBuffer!");
    if (--mGetBufferCount != 0)
        return;

    NS_ASSERTION(mCursor + aLength <= mFillPoint, "PutBuffer botch");
    mCursor += aLength;
}

NS_IMETHODIMP
nsBufferedInputStream::DisableBuffering()
{
    NS_ASSERTION(!mBufferDisabled, "redundant call to DisableBuffering!");
    NS_ASSERTION(mGetBufferCount == 0,
                 "DisableBuffer call between GetBuffer and PutBuffer!");
    if (mGetBufferCount != 0)
        return NS_ERROR_UNEXPECTED;

    // Empty the buffer so nsBufferedStream::Tell works.
    mBufferStartOffset += mCursor;
    mFillPoint = mCursor = 0;
    mBufferDisabled = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsBufferedInputStream::EnableBuffering()
{
    NS_ASSERTION(mBufferDisabled, "gratuitous call to EnableBuffering!");
    mBufferDisabled = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsBufferedInputStream::GetUnbufferedStream(nsISupports* *aStream)
{
    // Empty the buffer so subsequent i/o trumps any buffered data.
    mBufferStartOffset += mCursor;
    mFillPoint = mCursor = 0;

    *aStream = mStream;
    NS_IF_ADDREF(*aStream);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsBufferedOutputStream

NS_IMPL_ISUPPORTS_INHERITED3(nsBufferedOutputStream, 
                             nsBufferedStream,
                             nsIOutputStream,
                             nsIBufferedOutputStream,
                             nsIStreamBufferAccess)
 
NS_METHOD
nsBufferedOutputStream::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    NS_ENSURE_NO_AGGREGATION(aOuter);

    nsBufferedOutputStream* stream = new nsBufferedOutputStream();
    if (stream == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(stream);
    nsresult rv = stream->QueryInterface(aIID, aResult);
    NS_RELEASE(stream);
    return rv;
}

NS_IMETHODIMP
nsBufferedOutputStream::Init(nsIOutputStream* stream, PRUint32 bufferSize)
{
    return nsBufferedStream::Init(stream, bufferSize);
}

NS_IMETHODIMP
nsBufferedOutputStream::Close()
{
    nsresult rv1, rv2 = NS_OK, rv3;
    rv1 = Flush();
    // If we fail to Flush all the data, then we close anyway and drop the
    // remaining data in the buffer. We do this because it's what Unix does
    // for fclose and close. However, we report the error from Flush anyway.
    if (mStream) {
        rv2 = Sink()->Close();
        NS_RELEASE(mStream);
    }
    rv3 = nsBufferedStream::Close();
    if (NS_FAILED(rv1)) return rv1;
    if (NS_FAILED(rv2)) return rv2;
    return rv3;
}

NS_IMETHODIMP
nsBufferedOutputStream::Write(const char *buf, PRUint32 count, PRUint32 *result)
{
    nsresult rv = NS_OK;
    PRUint32 written = 0;
    while (count > 0) {
        PRUint32 amt = PR_MIN(count, mBufferSize - mCursor);
        if (amt > 0) {
            nsCRT::memcpy(mBuffer + mCursor, buf + written, amt);
            written += amt;
            count -= amt;
            mCursor += amt;
            if (mFillPoint < mCursor)
                mFillPoint = mCursor;
        }
        else {
            NS_ASSERTION(mFillPoint, "iloop in nsBufferedOutputStream::Write!");
            rv = Flush();
            if (NS_FAILED(rv)) break;
        }
    }
    *result = written;
    return (written > 0) ? NS_OK : rv;
}

NS_IMETHODIMP
nsBufferedOutputStream::Flush()
{
    nsresult rv;
    PRUint32 amt;
    if (!mStream) {
        // Stream already cancelled/flushed; probably because of error.
        return NS_OK;
    }
    rv = Sink()->Write(mBuffer, mFillPoint, &amt);
    if (NS_FAILED(rv)) return rv;
    mBufferStartOffset += amt;
    if (amt == mFillPoint) {
        mFillPoint = mCursor = 0;
        return NS_OK;   // flushed everything
    }

    // slide the remainder down to the start of the buffer
    // |<-------------->|<---|----->|
    // b                a    c      s
    PRUint32 rem = mFillPoint - amt;
    nsCRT::memcpy(mBuffer, mBuffer + amt, rem);
    mFillPoint = mCursor = rem;
    return NS_ERROR_FAILURE;        // didn't flush all
}
    
NS_IMETHODIMP
nsBufferedOutputStream::WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval)
{
    NS_NOTREACHED("WriteFrom");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBufferedOutputStream::WriteSegments(nsReadSegmentFun reader, void * closure, PRUint32 count, PRUint32 *_retval)
{
    NS_NOTREACHED("WriteSegments");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBufferedOutputStream::GetNonBlocking(PRBool *aNonBlocking)
{
    if (mStream)
        return Sink()->GetNonBlocking(aNonBlocking);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBufferedOutputStream::SetNonBlocking(PRBool aNonBlocking)
{
    if (mStream)
        return Sink()->SetNonBlocking(aNonBlocking);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBufferedOutputStream::GetObserver(nsIOutputStreamObserver * *aObserver)
{
    NS_NOTREACHED("GetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBufferedOutputStream::SetObserver(nsIOutputStreamObserver * aObserver)
{
    NS_NOTREACHED("SetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(char*)
nsBufferedOutputStream::GetBuffer(PRUint32 aLength, PRUint32 aAlignMask)
{
    NS_ASSERTION(mGetBufferCount == 0, "nested GetBuffer!");
    if (mGetBufferCount != 0)
        return nsnull;

    if (mBufferDisabled)
        return nsnull;

    char* buf = mBuffer + mCursor;
    PRUint32 rem = mBufferSize - mCursor;
    if (rem == 0) {
        if (NS_FAILED(Flush()))
            return nsnull;
        buf = mBuffer + mCursor;
        rem = mBufferSize - mCursor;
    }

    PRUint32 mod = (NS_PTR_TO_INT32(buf) & aAlignMask);
    if (mod) {
        PRUint32 pad = aAlignMask + 1 - mod;
        if (pad > rem)
            return nsnull;

        memset(buf, 0, pad);
        mCursor += pad;
        buf += pad;
        rem -= pad;
    }

    if (aLength > rem)
        return nsnull;
    mGetBufferCount++;
    return buf;
}

NS_IMETHODIMP_(void)
nsBufferedOutputStream::PutBuffer(char* aBuffer, PRUint32 aLength)
{
    NS_ASSERTION(mGetBufferCount == 1, "stray PutBuffer!");
    if (--mGetBufferCount != 0)
        return;

    NS_ASSERTION(mCursor + aLength <= mBufferSize, "PutBuffer botch");
    mCursor += aLength;
    if (mFillPoint < mCursor)
        mFillPoint = mCursor;
}

NS_IMETHODIMP
nsBufferedOutputStream::DisableBuffering()
{
    NS_ASSERTION(!mBufferDisabled, "redundant call to DisableBuffering!");
    NS_ASSERTION(mGetBufferCount == 0,
                 "DisableBuffer call between GetBuffer and PutBuffer!");
    if (mGetBufferCount != 0)
        return NS_ERROR_UNEXPECTED;

    // Empty the buffer so nsBufferedStream::Tell works.
    nsresult rv = Flush();
    if (NS_FAILED(rv))
        return rv;

    mBufferDisabled = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsBufferedOutputStream::EnableBuffering()
{
    NS_ASSERTION(mBufferDisabled, "gratuitous call to EnableBuffering!");
    mBufferDisabled = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsBufferedOutputStream::GetUnbufferedStream(nsISupports* *aStream)
{
    // Empty the buffer so subsequent i/o trumps any buffered data.
    if (mFillPoint) {
        nsresult rv = Flush();
        if (NS_FAILED(rv))
            return rv;
    }

    *aStream = mStream;
    NS_IF_ADDREF(*aStream);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
