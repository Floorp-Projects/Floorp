/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is frightening to behold.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com>
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

/**
 * The multiplex stream concatenates a list of input streams into a single
 * stream.
 */

#include "nsMultiplexInputStream.h"
#include "nsIMultiplexInputStream.h"
#include "nsISeekableStream.h"
#include "nsSupportsArray.h"
#include "nsInt64.h"

class nsMultiplexInputStream : public nsIMultiplexInputStream,
                               public nsISeekableStream
{
public:
    nsMultiplexInputStream();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIMULTIPLEXINPUTSTREAM
    NS_DECL_NSISEEKABLESTREAM

    static NS_METHOD Create(nsISupports *outer, REFNSIID iid, void **result);

private:
    ~nsMultiplexInputStream() {}


    struct ReadSegmentsState {
        nsIInputStream* mThisStream;
        PRUint32 mOffset;
        nsWriteSegmentFun mWriter;
        void* mClosure;
        PRBool mDone;
    };

    static NS_METHOD ReadSegCb(nsIInputStream* aIn, void* aClosure,
                               const char* aFromRawSegment, PRUint32 aToOffset,
                               PRUint32 aCount, PRUint32 *aWriteCount);
    
    nsSupportsArray mStreams;
    PRUint32 mCurrentStream;
    PRBool mStartedReadingCurrent;
};


NS_IMPL_THREADSAFE_ISUPPORTS3(nsMultiplexInputStream,
                              nsIMultiplexInputStream,
                              nsIInputStream,
                              nsISeekableStream)

nsMultiplexInputStream::nsMultiplexInputStream()
    : mCurrentStream(0),
      mStartedReadingCurrent(PR_FALSE)
{
}

/* readonly attribute unsigned long count; */
NS_IMETHODIMP
nsMultiplexInputStream::GetCount(PRUint32 *aCount)
{
    mStreams.Count(aCount);
    return NS_OK;
}

/* void appendStream (in nsIInputStream stream); */
NS_IMETHODIMP
nsMultiplexInputStream::AppendStream(nsIInputStream *aStream)
{
    return mStreams.AppendElement(aStream);
}

/* void insertStream (in nsIInputStream stream, in unsigned long index); */
NS_IMETHODIMP
nsMultiplexInputStream::InsertStream(nsIInputStream *aStream, PRUint32 aIndex)
{
    nsresult rv = mStreams.InsertElementAt(aStream, aIndex);
    NS_ENSURE_SUCCESS(rv, rv);
    if (mCurrentStream > aIndex ||
        (mCurrentStream == aIndex && mStartedReadingCurrent))
        ++mCurrentStream;
    return rv;
}

/* void removeStream (in unsigned long index); */
NS_IMETHODIMP
nsMultiplexInputStream::RemoveStream(PRUint32 aIndex)
{
    nsresult rv = mStreams.RemoveElementAt(aIndex);
    NS_ENSURE_SUCCESS(rv, rv);
    if (mCurrentStream > aIndex)
        --mCurrentStream;
    else if (mCurrentStream == aIndex)
        mStartedReadingCurrent = PR_FALSE;

    return rv;
}

/* nsIInputStream getStream (in unsigned long index); */
NS_IMETHODIMP
nsMultiplexInputStream::GetStream(PRUint32 aIndex, nsIInputStream **_retval)
{
    return mStreams.QueryElementAt(aIndex,
                                   NS_GET_IID(nsIInputStream),
                                   (void**)_retval);
}

/* void close (); */
NS_IMETHODIMP
nsMultiplexInputStream::Close()
{
    PRUint32 len, i;
    nsresult rv = NS_OK;

    mStreams.Count(&len);
    for (i = 0; i < len; ++i) {
        nsCOMPtr<nsIInputStream> stream(do_QueryElementAt(&mStreams, i));
        nsresult rv2 = stream->Close();
        // We still want to close all streams, but we should return an error
        if (NS_FAILED(rv2))
            rv = rv2;
    }
    return rv;
}

/* unsigned long available (); */
NS_IMETHODIMP
nsMultiplexInputStream::Available(PRUint32 *_retval)
{
    nsresult rv;
    PRUint32 i, len, avail = 0;

    mStreams.Count(&len);
    for (i = mCurrentStream; i < len; i++) {
        nsCOMPtr<nsIInputStream> stream(do_QueryElementAt(&mStreams, i));
        
        PRUint32 streamAvail;
        rv = stream->Available(&streamAvail);
        NS_ENSURE_SUCCESS(rv, rv);
        avail += streamAvail;
    }
    *_retval = avail;
    return NS_OK;
}

/* [noscript] unsigned long read (in charPtr buf, in unsigned long count); */
NS_IMETHODIMP
nsMultiplexInputStream::Read(char * aBuf, PRUint32 aCount, PRUint32 *_retval)
{
    nsresult rv = NS_OK;
    PRUint32 len, read;

    *_retval = 0;

    mStreams.Count(&len);
    while (mCurrentStream < len && aCount) {
        nsCOMPtr<nsIInputStream> stream(do_QueryElementAt(&mStreams,
                                                          mCurrentStream));
        rv = stream->Read(aBuf, aCount, &read);

        // XXX some streams return NS_BASE_STREAM_CLOSED to indicate EOF.
        if (rv == NS_BASE_STREAM_CLOSED) {
            rv = NS_OK;
            read = 0;
        }
        else if (NS_FAILED(rv))
            break;

        if (read == 0) {
            ++mCurrentStream;
            mStartedReadingCurrent = PR_FALSE;
        }
        else {
            NS_ASSERTION(aCount >= read, "Read more than requested");
            *_retval += read;
            aCount -= read;
            aBuf += read;
            mStartedReadingCurrent = PR_TRUE;
        }
    }
    return *_retval ? NS_OK : rv;
}

/* [noscript] unsigned long readSegments (in nsWriteSegmentFun writer,
 *                                        in voidPtr closure,
 *                                        in unsigned long count); */
NS_IMETHODIMP
nsMultiplexInputStream::ReadSegments(nsWriteSegmentFun aWriter, void *aClosure,
                                     PRUint32 aCount, PRUint32 *_retval)
{
    NS_ASSERTION(aWriter, "missing aWriter");

    nsresult rv = NS_OK;
    ReadSegmentsState state;
    state.mThisStream = this;
    state.mOffset = 0;
    state.mWriter = aWriter;
    state.mClosure = aClosure;
    state.mDone = PR_FALSE;
    
    PRUint32 len;
    mStreams.Count(&len);
    while (mCurrentStream < len && aCount) {
        nsCOMPtr<nsIInputStream> stream(do_QueryElementAt(&mStreams,
                                                          mCurrentStream));
        PRUint32 read;
        rv = stream->ReadSegments(ReadSegCb, &state, aCount, &read);

        // XXX some streams return NS_BASE_STREAM_CLOSED to indicate EOF.
        if (rv == NS_BASE_STREAM_CLOSED) {
            rv = NS_OK;
            read = 0;
        }

        // if |aWriter| decided to stop reading segments...
        if (state.mDone || NS_FAILED(rv))
            break;

        // if stream is empty, then advance to the next stream.
        if (read == 0) {
            ++mCurrentStream;
            mStartedReadingCurrent = PR_FALSE;
        }
        else {
            NS_ASSERTION(aCount >= read, "Read more than requested");
            state.mOffset += read;
            aCount -= read;
            mStartedReadingCurrent = PR_TRUE;
        }
    }

    // if we successfully read some data, then this call succeeded.
    *_retval = state.mOffset;
    return state.mOffset ? NS_OK : rv;
}

NS_METHOD
nsMultiplexInputStream::ReadSegCb(nsIInputStream* aIn, void* aClosure,
                                  const char* aFromRawSegment,
                                  PRUint32 aToOffset, PRUint32 aCount,
                                  PRUint32 *aWriteCount)
{
    nsresult rv;
    ReadSegmentsState* state = (ReadSegmentsState*)aClosure;
    rv = (state->mWriter)(state->mThisStream,
                          state->mClosure,
                          aFromRawSegment,
                          aToOffset + state->mOffset,
                          aCount,
                          aWriteCount);
    if (NS_FAILED(rv))
        state->mDone = PR_TRUE;
    return rv;
}

/* readonly attribute boolean nonBlocking; */
NS_IMETHODIMP
nsMultiplexInputStream::IsNonBlocking(PRBool *aNonBlocking)
{
    nsresult rv;
    PRUint32 i, len;

    mStreams.Count(&len);
    for (i = 0; i < len; ++i) {
        nsCOMPtr<nsIInputStream> stream(do_QueryElementAt(&mStreams, i));
        rv = stream->IsNonBlocking(aNonBlocking);
        NS_ENSURE_SUCCESS(rv, rv);
        // If one is non-blocking the entire stream becomes non-blocking
        if (*aNonBlocking)
            return NS_OK;
    }
    return NS_OK;
}

/* void seek (in PRInt32 whence, in PRInt32 offset); */
NS_IMETHODIMP
nsMultiplexInputStream::Seek(PRInt32 aWhence, PRInt64 aOffset)
{
    nsresult rv;

    // rewinding to start is easy, and should be the most common case
    if (aWhence == NS_SEEK_SET && aOffset == 0)
    {
        PRUint32 i, last;
        last = mStartedReadingCurrent ? mCurrentStream+1 : mCurrentStream;
        for (i = 0; i < last; ++i) {
            nsCOMPtr<nsISeekableStream> stream(do_QueryElementAt(&mStreams, i));
            NS_ENSURE_TRUE(stream, NS_ERROR_NO_INTERFACE);

            rv = stream->Seek(NS_SEEK_SET, 0);
            NS_ENSURE_SUCCESS(rv, rv);
        }
        mCurrentStream = 0;
        mStartedReadingCurrent = PR_FALSE;
        return NS_OK;
    }

    // other Seeks not implemented yet
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRUint32 tell (); */
NS_IMETHODIMP
nsMultiplexInputStream::Tell(PRInt64 *_retval)
{
    nsresult rv;
    nsInt64 ret64 = 0;
    PRUint32 i, last;
    last = mStartedReadingCurrent ? mCurrentStream+1 : mCurrentStream;
    for (i = 0; i < last; ++i) {
        nsCOMPtr<nsISeekableStream> stream(do_QueryElementAt(&mStreams, i));
        NS_ENSURE_TRUE(stream, NS_ERROR_NO_INTERFACE);

        PRInt64 pos;
        rv = stream->Tell(&pos);
        NS_ENSURE_SUCCESS(rv, rv);
        ret64 += pos;
    }
    *_retval =  ret64;

    return NS_OK;
}

/* void setEOF (); */
NS_IMETHODIMP
nsMultiplexInputStream::SetEOF()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsMultiplexInputStreamConstructor(nsISupports *outer,
                                  REFNSIID iid,
                                  void **result)
{
    *result = nsnull;

    if (outer)
        return NS_ERROR_NO_AGGREGATION;

    nsMultiplexInputStream *inst;
    NS_NEWXPCOM(inst, nsMultiplexInputStream);
    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(inst);
    nsresult rv = inst->QueryInterface(iid, result);
    NS_RELEASE(inst);

    return rv;
}
