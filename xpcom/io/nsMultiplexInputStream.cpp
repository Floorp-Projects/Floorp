/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The multiplex stream concatenates a list of input streams into a single
 * stream.
 */

#include "IPC/IPCMessageUtils.h"
#include "mozilla/net/NeckoMessageUtils.h"
#include "mozilla/Attributes.h"

#include "nsMultiplexInputStream.h"
#include "nsIMultiplexInputStream.h"
#include "nsISeekableStream.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIIPCSerializableObsolete.h"
#include "nsIClassInfoImpl.h"
#include "nsIIPCSerializableInputStream.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/IPCSerializableParams.h"

using namespace mozilla::ipc;

class nsMultiplexInputStream MOZ_FINAL : public nsIMultiplexInputStream,
                                         public nsISeekableStream,
                                         public nsIIPCSerializableObsolete,
                                         public nsIIPCSerializableInputStream
{
public:
    nsMultiplexInputStream();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIMULTIPLEXINPUTSTREAM
    NS_DECL_NSISEEKABLESTREAM
    NS_DECL_NSIIPCSERIALIZABLEOBSOLETE
    NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM

private:
    ~nsMultiplexInputStream() {}

    struct ReadSegmentsState {
        nsIInputStream* mThisStream;
        PRUint32 mOffset;
        nsWriteSegmentFun mWriter;
        void* mClosure;
        bool mDone;
    };

    static NS_METHOD ReadSegCb(nsIInputStream* aIn, void* aClosure,
                               const char* aFromRawSegment, PRUint32 aToOffset,
                               PRUint32 aCount, PRUint32 *aWriteCount);
    
    nsTArray<nsCOMPtr<nsIInputStream> > mStreams;
    PRUint32 mCurrentStream;
    bool mStartedReadingCurrent;
    nsresult mStatus;
};

NS_IMPL_THREADSAFE_ADDREF(nsMultiplexInputStream)
NS_IMPL_THREADSAFE_RELEASE(nsMultiplexInputStream)

NS_IMPL_CLASSINFO(nsMultiplexInputStream, NULL, nsIClassInfo::THREADSAFE,
                  NS_MULTIPLEXINPUTSTREAM_CID)

NS_IMPL_QUERY_INTERFACE5_CI(nsMultiplexInputStream,
                            nsIMultiplexInputStream,
                            nsIInputStream,
                            nsISeekableStream,
                            nsIIPCSerializableObsolete,
                            nsIIPCSerializableInputStream)
NS_IMPL_CI_INTERFACE_GETTER3(nsMultiplexInputStream,
                             nsIMultiplexInputStream,
                             nsIInputStream,
                             nsISeekableStream)

nsMultiplexInputStream::nsMultiplexInputStream()
    : mCurrentStream(0),
      mStartedReadingCurrent(false),
      mStatus(NS_OK)
{
}

/* readonly attribute unsigned long count; */
NS_IMETHODIMP
nsMultiplexInputStream::GetCount(PRUint32 *aCount)
{
    *aCount = mStreams.Length();
    return NS_OK;
}

/* void appendStream (in nsIInputStream stream); */
NS_IMETHODIMP
nsMultiplexInputStream::AppendStream(nsIInputStream *aStream)
{
    return mStreams.AppendElement(aStream) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* void insertStream (in nsIInputStream stream, in unsigned long index); */
NS_IMETHODIMP
nsMultiplexInputStream::InsertStream(nsIInputStream *aStream, PRUint32 aIndex)
{
    bool result = mStreams.InsertElementAt(aIndex, aStream);
    NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);
    if (mCurrentStream > aIndex ||
        (mCurrentStream == aIndex && mStartedReadingCurrent))
        ++mCurrentStream;
    return NS_OK;
}

/* void removeStream (in unsigned long index); */
NS_IMETHODIMP
nsMultiplexInputStream::RemoveStream(PRUint32 aIndex)
{
    mStreams.RemoveElementAt(aIndex);
    if (mCurrentStream > aIndex)
        --mCurrentStream;
    else if (mCurrentStream == aIndex)
        mStartedReadingCurrent = false;

    return NS_OK;
}

/* nsIInputStream getStream (in unsigned long index); */
NS_IMETHODIMP
nsMultiplexInputStream::GetStream(PRUint32 aIndex, nsIInputStream **_retval)
{
    *_retval = mStreams.SafeElementAt(aIndex, nullptr);
    NS_ENSURE_TRUE(*_retval, NS_ERROR_NOT_AVAILABLE);

    NS_ADDREF(*_retval);
    return NS_OK;
}

/* void close (); */
NS_IMETHODIMP
nsMultiplexInputStream::Close()
{
    mStatus = NS_BASE_STREAM_CLOSED;

    nsresult rv = NS_OK;

    PRUint32 len = mStreams.Length();
    for (PRUint32 i = 0; i < len; ++i) {
        nsresult rv2 = mStreams[i]->Close();
        // We still want to close all streams, but we should return an error
        if (NS_FAILED(rv2))
            rv = rv2;
    }
    return rv;
}

/* unsigned long long available (); */
NS_IMETHODIMP
nsMultiplexInputStream::Available(PRUint64 *_retval)
{
    if (NS_FAILED(mStatus))
        return mStatus;

    nsresult rv;
    PRUint64 avail = 0;

    PRUint32 len = mStreams.Length();
    for (PRUint32 i = mCurrentStream; i < len; i++) {
        PRUint64 streamAvail;
        rv = mStreams[i]->Available(&streamAvail);
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
    // It is tempting to implement this method in terms of ReadSegments, but
    // that would prevent this class from being used with streams that only
    // implement Read (e.g., file streams).
 
    *_retval = 0;

    if (mStatus == NS_BASE_STREAM_CLOSED)
        return NS_OK;
    if (NS_FAILED(mStatus))
        return mStatus;
 
    nsresult rv = NS_OK;

    PRUint32 len = mStreams.Length();
    while (mCurrentStream < len && aCount) {
        PRUint32 read;
        rv = mStreams[mCurrentStream]->Read(aBuf, aCount, &read);

        // XXX some streams return NS_BASE_STREAM_CLOSED to indicate EOF.
        // (This is a bug in those stream implementations)
        if (rv == NS_BASE_STREAM_CLOSED) {
            NS_NOTREACHED("Input stream's Read method returned NS_BASE_STREAM_CLOSED");
            rv = NS_OK;
            read = 0;
        }
        else if (NS_FAILED(rv))
            break;

        if (read == 0) {
            ++mCurrentStream;
            mStartedReadingCurrent = false;
        }
        else {
            NS_ASSERTION(aCount >= read, "Read more than requested");
            *_retval += read;
            aCount -= read;
            aBuf += read;
            mStartedReadingCurrent = true;
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
    if (mStatus == NS_BASE_STREAM_CLOSED) {
        *_retval = 0;
        return NS_OK;
    }
    if (NS_FAILED(mStatus))
        return mStatus;

    NS_ASSERTION(aWriter, "missing aWriter");

    nsresult rv = NS_OK;
    ReadSegmentsState state;
    state.mThisStream = this;
    state.mOffset = 0;
    state.mWriter = aWriter;
    state.mClosure = aClosure;
    state.mDone = false;
    
    PRUint32 len = mStreams.Length();
    while (mCurrentStream < len && aCount) {
        PRUint32 read;
        rv = mStreams[mCurrentStream]->ReadSegments(ReadSegCb, &state, aCount, &read);

        // XXX some streams return NS_BASE_STREAM_CLOSED to indicate EOF.
        // (This is a bug in those stream implementations)
        if (rv == NS_BASE_STREAM_CLOSED) {
            NS_NOTREACHED("Input stream's Read method returned NS_BASE_STREAM_CLOSED");
            rv = NS_OK;
            read = 0;
        }

        // if |aWriter| decided to stop reading segments...
        if (state.mDone || NS_FAILED(rv))
            break;

        // if stream is empty, then advance to the next stream.
        if (read == 0) {
            ++mCurrentStream;
            mStartedReadingCurrent = false;
        }
        else {
            NS_ASSERTION(aCount >= read, "Read more than requested");
            state.mOffset += read;
            aCount -= read;
            mStartedReadingCurrent = true;
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
        state->mDone = true;
    return rv;
}

/* readonly attribute boolean nonBlocking; */
NS_IMETHODIMP
nsMultiplexInputStream::IsNonBlocking(bool *aNonBlocking)
{
    PRUint32 len = mStreams.Length();
    if (len == 0) {
        // Claim to be non-blocking, since we won't block the caller.
        // On the other hand we'll never return NS_BASE_STREAM_WOULD_BLOCK,
        // so maybe we should claim to be blocking?  It probably doesn't
        // matter in practice.
        *aNonBlocking = true;
        return NS_OK;
    }
    for (PRUint32 i = 0; i < len; ++i) {
        nsresult rv = mStreams[i]->IsNonBlocking(aNonBlocking);
        NS_ENSURE_SUCCESS(rv, rv);
        // If one is non-blocking the entire stream becomes non-blocking
        // (except that we don't implement nsIAsyncInputStream, so there's
        //  not much for the caller to do if Read returns "would block")
        if (*aNonBlocking)
            return NS_OK;
    }
    return NS_OK;
}

/* void seek (in PRInt32 whence, in PRInt32 offset); */
NS_IMETHODIMP
nsMultiplexInputStream::Seek(PRInt32 aWhence, PRInt64 aOffset)
{
    if (NS_FAILED(mStatus))
        return mStatus;

    nsresult rv;

    PRUint32 oldCurrentStream = mCurrentStream;
    bool oldStartedReadingCurrent = mStartedReadingCurrent;

    if (aWhence == NS_SEEK_SET) {
        PRInt64 remaining = aOffset;
        if (aOffset == 0) {
            mCurrentStream = 0;
        }
        for (PRUint32 i = 0; i < mStreams.Length(); ++i) {
            nsCOMPtr<nsISeekableStream> stream =
                do_QueryInterface(mStreams[i]);
            if (!stream) {
              return NS_ERROR_FAILURE;
            }

            // See if all remaining streams should be rewound
            if (remaining == 0) {
                if (i < oldCurrentStream ||
                    (i == oldCurrentStream && oldStartedReadingCurrent)) {
                    rv = stream->Seek(NS_SEEK_SET, 0);
                    NS_ENSURE_SUCCESS(rv, rv);
                    continue;
                }
                else {
                    break;
                }
            }

            // Get position in current stream
            PRInt64 streamPos;
            if (i > oldCurrentStream ||
                (i == oldCurrentStream && !oldStartedReadingCurrent)) {
                streamPos = 0;
            }
            else {
                rv = stream->Tell(&streamPos);
                NS_ENSURE_SUCCESS(rv, rv);
            }

            // See if we need to seek current stream forward or backward
            if (remaining < streamPos) {
                rv = stream->Seek(NS_SEEK_SET, remaining);
                NS_ENSURE_SUCCESS(rv, rv);

                mCurrentStream = i;
                mStartedReadingCurrent = remaining != 0;

                remaining = 0;
            }
            else if (remaining > streamPos) {
                if (i < oldCurrentStream) {
                    // We're already at end so no need to seek this stream
                    remaining -= streamPos;
                }
                else {
                    PRUint64 avail;
                    rv = mStreams[i]->Available(&avail);
                    NS_ENSURE_SUCCESS(rv, rv);

                    PRInt64 newPos = streamPos +
                                     NS_MIN((PRInt64)avail, remaining);

                    rv = stream->Seek(NS_SEEK_SET, newPos);
                    NS_ENSURE_SUCCESS(rv, rv);

                    mCurrentStream = i;
                    mStartedReadingCurrent = true;

                    remaining -= newPos;
                }
            }
            else {
                NS_ASSERTION(remaining == streamPos, "Huh?");
                remaining = 0;
            }
        }

        return NS_OK;
    }

    if (aWhence == NS_SEEK_CUR && aOffset > 0) {
        PRInt64 remaining = aOffset;
        for (PRUint32 i = mCurrentStream; remaining && i < mStreams.Length(); ++i) {
            nsCOMPtr<nsISeekableStream> stream =
                do_QueryInterface(mStreams[i]);

            PRUint64 avail;
            rv = mStreams[i]->Available(&avail);
            NS_ENSURE_SUCCESS(rv, rv);

            PRInt64 seek = NS_MIN((PRInt64)avail, remaining);

            rv = stream->Seek(NS_SEEK_CUR, seek);
            NS_ENSURE_SUCCESS(rv, rv);

            mCurrentStream = i;
            mStartedReadingCurrent = true;

            remaining -= seek;
        }

        return NS_OK;
    }

    if (aWhence == NS_SEEK_CUR && aOffset < 0) {
        PRInt64 remaining = -aOffset;
        for (PRUint32 i = mCurrentStream; remaining && i != (PRUint32)-1; --i) {
            nsCOMPtr<nsISeekableStream> stream =
                do_QueryInterface(mStreams[i]);

            PRInt64 pos;
            rv = stream->Tell(&pos);
            NS_ENSURE_SUCCESS(rv, rv);

            PRInt64 seek = NS_MIN(pos, remaining);

            rv = stream->Seek(NS_SEEK_CUR, -seek);
            NS_ENSURE_SUCCESS(rv, rv);

            mCurrentStream = i;
            mStartedReadingCurrent = seek != -pos;

            remaining -= seek;
        }

        return NS_OK;
    }

    if (aWhence == NS_SEEK_CUR) {
        NS_ASSERTION(aOffset == 0, "Should have handled all non-zero values");

        return NS_OK;
    }

    if (aWhence == NS_SEEK_END) {
        if (aOffset > 0) {
          return NS_ERROR_INVALID_ARG;
        }
        PRInt64 remaining = aOffset;
        for (PRUint32 i = mStreams.Length() - 1; i != (PRUint32)-1; --i) {
            nsCOMPtr<nsISeekableStream> stream =
                do_QueryInterface(mStreams[i]);

            // See if all remaining streams should be seeked to end
            if (remaining == 0) {
                if (i >= oldCurrentStream) {
                    rv = stream->Seek(NS_SEEK_END, 0);
                    NS_ENSURE_SUCCESS(rv, rv);
                }
                else {
                    break;
                }
            }

            // Get position in current stream
            PRInt64 streamPos;
            if (i < oldCurrentStream) {
                streamPos = 0;
            } else {
                PRUint64 avail;
                rv = mStreams[i]->Available(&avail);
                NS_ENSURE_SUCCESS(rv, rv);

                streamPos = avail;
            }

            // See if we have enough data in the current stream.
            if (NS_ABS(remaining) < streamPos) {
                rv = stream->Seek(NS_SEEK_END, remaining);
                NS_ENSURE_SUCCESS(rv, rv);

                mCurrentStream = i;
                mStartedReadingCurrent = true;

                remaining = 0;
            } else if (NS_ABS(remaining) > streamPos) {
                if (i > oldCurrentStream ||
                    (i == oldCurrentStream && !oldStartedReadingCurrent)) {
                    // We're already at start so no need to seek this stream
                    remaining += streamPos;
                } else {
                    PRInt64 avail;
                    rv = stream->Tell(&avail);
                    NS_ENSURE_SUCCESS(rv, rv);

                    PRInt64 newPos = streamPos + NS_MIN(avail, NS_ABS(remaining));

                    rv = stream->Seek(NS_SEEK_END, -newPos);
                    NS_ENSURE_SUCCESS(rv, rv);

                    mCurrentStream = i;
                    mStartedReadingCurrent = true;

                    remaining += newPos;
                }
            }
            else {
                NS_ASSERTION(remaining == streamPos, "Huh?");
                remaining = 0;
            }
        }

        return NS_OK;
    }

    // other Seeks not implemented yet
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRUint32 tell (); */
NS_IMETHODIMP
nsMultiplexInputStream::Tell(PRInt64 *_retval)
{
    if (NS_FAILED(mStatus))
        return mStatus;

    nsresult rv;
    PRInt64 ret64 = 0;
    PRUint32 i, last;
    last = mStartedReadingCurrent ? mCurrentStream+1 : mCurrentStream;
    for (i = 0; i < last; ++i) {
        nsCOMPtr<nsISeekableStream> stream = do_QueryInterface(mStreams[i]);
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

nsresult
nsMultiplexInputStreamConstructor(nsISupports *outer,
                                  REFNSIID iid,
                                  void **result)
{
    *result = nullptr;

    if (outer)
        return NS_ERROR_NO_AGGREGATION;

    nsMultiplexInputStream *inst = new nsMultiplexInputStream();
    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(inst);
    nsresult rv = inst->QueryInterface(iid, result);
    NS_RELEASE(inst);

    return rv;
}

bool
nsMultiplexInputStream::Read(const IPC::Message *aMsg, void **aIter)
{
    using IPC::ReadParam;

    PRUint32 count;
    if (!ReadParam(aMsg, aIter, &count))
        return false;

    for (PRUint32 i = 0; i < count; i++) {
        IPC::InputStream inputStream;
        if (!ReadParam(aMsg, aIter, &inputStream))
            return false;

        nsCOMPtr<nsIInputStream> stream(inputStream);
        nsresult rv = AppendStream(stream);
        if (NS_FAILED(rv))
            return false;
    }

    if (!ReadParam(aMsg, aIter, &mCurrentStream) ||
        !ReadParam(aMsg, aIter, &mStartedReadingCurrent) ||
        !ReadParam(aMsg, aIter, &mStatus))
        return false;

    return true;
}

void
nsMultiplexInputStream::Write(IPC::Message *aMsg)
{
    using IPC::WriteParam;

    PRUint32 count = mStreams.Length();
    WriteParam(aMsg, count);

    for (PRUint32 i = 0; i < count; i++) {
        IPC::InputStream inputStream(mStreams[i]);
        WriteParam(aMsg, inputStream);
    }

    WriteParam(aMsg, mCurrentStream);
    WriteParam(aMsg, mStartedReadingCurrent);
    WriteParam(aMsg, mStatus);
}

void
nsMultiplexInputStream::Serialize(InputStreamParams& aParams)
{
    MultiplexInputStreamParams params;

    PRUint32 streamCount = mStreams.Count();

    if (streamCount) {
        InfallibleTArray<InputStreamParams>& streams = params.streams();

        streams.SetCapacity(streamCount);
        for (PRUint32 index = 0; index < streamCount; index++) {
            nsCOMPtr<nsIIPCSerializableInputStream> serializable =
                do_QueryInterface(mStreams.ObjectAt(index));
            NS_ASSERTION(serializable, "Child stream isn't serializable!");

            if (serializable) {
                InputStreamParams childStreamParams;
                serializable->Serialize(childStreamParams);

                NS_ASSERTION(childStreamParams.type() !=
                                 InputStreamParams::T__None,
                             "Serialize failed!");

                streams.AppendElement(childStreamParams);
            }
        }
    }

    params.currentStream() = mCurrentStream;
    params.status() = mStatus;
    params.startedReadingCurrent() = mStartedReadingCurrent;

    aParams = params;
}

bool
nsMultiplexInputStream::Deserialize(const InputStreamParams& aParams)
{
    if (aParams.type() !=
            InputStreamParams::TMultiplexInputStreamParams) {
        NS_ERROR("Received unknown parameters from the other process!");
        return false;
    }

    const MultiplexInputStreamParams& params =
        aParams.get_MultiplexInputStreamParams();

    const InfallibleTArray<InputStreamParams>& streams = params.streams();

    PRUint32 streamCount = streams.Length();
    for (PRUint32 index = 0; index < streamCount; index++) {
        nsCOMPtr<nsIInputStream> stream =
            DeserializeInputStream(streams[index]);
        if (!stream) {
            NS_WARNING("Deserialize failed!");
            return false;
        }

        if (NS_FAILED(AppendStream(stream))) {
            NS_WARNING("AppendStream failed!");
            return false;
        }
    }

    mCurrentStream = params.currentStream();
    mStatus = params.status();
    mStartedReadingCurrent = params.startedReadingCurrent();

    return true;
}
