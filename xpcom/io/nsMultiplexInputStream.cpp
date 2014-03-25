/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The multiplex stream concatenates a list of input streams into a single
 * stream.
 */

#include "mozilla/Attributes.h"
#include "mozilla/MathAlgorithms.h"

#include "base/basictypes.h"

#include "nsMultiplexInputStream.h"
#include "nsIMultiplexInputStream.h"
#include "nsISeekableStream.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIClassInfoImpl.h"
#include "nsIIPCSerializableInputStream.h"
#include "mozilla/ipc/InputStreamUtils.h"

using namespace mozilla::ipc;

using mozilla::DeprecatedAbs;

class nsMultiplexInputStream MOZ_FINAL : public nsIMultiplexInputStream,
                                         public nsISeekableStream,
                                         public nsIIPCSerializableInputStream
{
public:
    nsMultiplexInputStream();

    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIMULTIPLEXINPUTSTREAM
    NS_DECL_NSISEEKABLESTREAM
    NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM

private:
    ~nsMultiplexInputStream() {}

    struct ReadSegmentsState {
        nsIInputStream* mThisStream;
        uint32_t mOffset;
        nsWriteSegmentFun mWriter;
        void* mClosure;
        bool mDone;
    };

    static NS_METHOD ReadSegCb(nsIInputStream* aIn, void* aClosure,
                               const char* aFromRawSegment, uint32_t aToOffset,
                               uint32_t aCount, uint32_t *aWriteCount);
    
    nsTArray<nsCOMPtr<nsIInputStream> > mStreams;
    uint32_t mCurrentStream;
    bool mStartedReadingCurrent;
    nsresult mStatus;
};

NS_IMPL_ADDREF(nsMultiplexInputStream)
NS_IMPL_RELEASE(nsMultiplexInputStream)

NS_IMPL_CLASSINFO(nsMultiplexInputStream, nullptr, nsIClassInfo::THREADSAFE,
                  NS_MULTIPLEXINPUTSTREAM_CID)

NS_IMPL_QUERY_INTERFACE4_CI(nsMultiplexInputStream,
                            nsIMultiplexInputStream,
                            nsIInputStream,
                            nsISeekableStream,
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
nsMultiplexInputStream::GetCount(uint32_t *aCount)
{
    *aCount = mStreams.Length();
    return NS_OK;
}

#ifdef DEBUG
static bool
SeekableStreamAtBeginning(nsIInputStream *aStream)
{
    int64_t streamPos;
    nsCOMPtr<nsISeekableStream> stream = do_QueryInterface(aStream);
    if (stream && NS_SUCCEEDED(stream->Tell(&streamPos)) && streamPos != 0) {
        return false;
    }
    return true;
}
#endif

/* void appendStream (in nsIInputStream stream); */
NS_IMETHODIMP
nsMultiplexInputStream::AppendStream(nsIInputStream *aStream)
{
    NS_ASSERTION(SeekableStreamAtBeginning(aStream), "Appended stream not at beginning.");
    return mStreams.AppendElement(aStream) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

/* void insertStream (in nsIInputStream stream, in unsigned long index); */
NS_IMETHODIMP
nsMultiplexInputStream::InsertStream(nsIInputStream *aStream, uint32_t aIndex)
{
    NS_ASSERTION(SeekableStreamAtBeginning(aStream), "Inserted stream not at beginning.");
    mStreams.InsertElementAt(aIndex, aStream);
    if (mCurrentStream > aIndex ||
        (mCurrentStream == aIndex && mStartedReadingCurrent))
        ++mCurrentStream;
    return NS_OK;
}

/* void removeStream (in unsigned long index); */
NS_IMETHODIMP
nsMultiplexInputStream::RemoveStream(uint32_t aIndex)
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
nsMultiplexInputStream::GetStream(uint32_t aIndex, nsIInputStream **_retval)
{
    *_retval = mStreams.SafeElementAt(aIndex, nullptr);
    if (NS_WARN_IF(!*_retval))
        return NS_ERROR_NOT_AVAILABLE;

    NS_ADDREF(*_retval);
    return NS_OK;
}

/* void close (); */
NS_IMETHODIMP
nsMultiplexInputStream::Close()
{
    mStatus = NS_BASE_STREAM_CLOSED;

    nsresult rv = NS_OK;

    uint32_t len = mStreams.Length();
    for (uint32_t i = 0; i < len; ++i) {
        nsresult rv2 = mStreams[i]->Close();
        // We still want to close all streams, but we should return an error
        if (NS_FAILED(rv2))
            rv = rv2;
    }
    return rv;
}

/* unsigned long long available (); */
NS_IMETHODIMP
nsMultiplexInputStream::Available(uint64_t *_retval)
{
    if (NS_FAILED(mStatus))
        return mStatus;

    nsresult rv;
    uint64_t avail = 0;

    uint32_t len = mStreams.Length();
    for (uint32_t i = mCurrentStream; i < len; i++) {
        uint64_t streamAvail;
        rv = mStreams[i]->Available(&streamAvail);
        if (NS_WARN_IF(NS_FAILED(rv)))
            return rv;
        avail += streamAvail;
    }
    *_retval = avail;
    return NS_OK;
}

/* [noscript] unsigned long read (in charPtr buf, in unsigned long count); */
NS_IMETHODIMP
nsMultiplexInputStream::Read(char * aBuf, uint32_t aCount, uint32_t *_retval)
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

    uint32_t len = mStreams.Length();
    while (mCurrentStream < len && aCount) {
        uint32_t read;
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
                                     uint32_t aCount, uint32_t *_retval)
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
    
    uint32_t len = mStreams.Length();
    while (mCurrentStream < len && aCount) {
        uint32_t read;
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
                                  uint32_t aToOffset, uint32_t aCount,
                                  uint32_t *aWriteCount)
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
    uint32_t len = mStreams.Length();
    if (len == 0) {
        // Claim to be non-blocking, since we won't block the caller.
        // On the other hand we'll never return NS_BASE_STREAM_WOULD_BLOCK,
        // so maybe we should claim to be blocking?  It probably doesn't
        // matter in practice.
        *aNonBlocking = true;
        return NS_OK;
    }
    for (uint32_t i = 0; i < len; ++i) {
        nsresult rv = mStreams[i]->IsNonBlocking(aNonBlocking);
        if (NS_WARN_IF(NS_FAILED(rv)))
            return rv;
        // If one is non-blocking the entire stream becomes non-blocking
        // (except that we don't implement nsIAsyncInputStream, so there's
        //  not much for the caller to do if Read returns "would block")
        if (*aNonBlocking)
            return NS_OK;
    }
    return NS_OK;
}

/* void seek (in int32_t whence, in int32_t offset); */
NS_IMETHODIMP
nsMultiplexInputStream::Seek(int32_t aWhence, int64_t aOffset)
{
    if (NS_FAILED(mStatus))
        return mStatus;

    nsresult rv;

    uint32_t oldCurrentStream = mCurrentStream;
    bool oldStartedReadingCurrent = mStartedReadingCurrent;

    if (aWhence == NS_SEEK_SET) {
        int64_t remaining = aOffset;
        if (aOffset == 0) {
            mCurrentStream = 0;
        }
        for (uint32_t i = 0; i < mStreams.Length(); ++i) {
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
                    if (NS_WARN_IF(NS_FAILED(rv)))
                        return rv;
                    continue;
                }
                else {
                    break;
                }
            }

            // Get position in current stream
            int64_t streamPos;
            if (i > oldCurrentStream ||
                (i == oldCurrentStream && !oldStartedReadingCurrent)) {
                streamPos = 0;
            }
            else {
                rv = stream->Tell(&streamPos);
                if (NS_WARN_IF(NS_FAILED(rv)))
                    return rv;
            }

            // See if we need to seek current stream forward or backward
            if (remaining < streamPos) {
                rv = stream->Seek(NS_SEEK_SET, remaining);
                if (NS_WARN_IF(NS_FAILED(rv)))
                    return rv;

                mCurrentStream = i;
                mStartedReadingCurrent = remaining != 0;

                remaining = 0;
            }
            else if (remaining > streamPos) {
                if (i < oldCurrentStream) {
                    // We're already at end so no need to seek this stream
                    remaining -= streamPos;
                    NS_ASSERTION(remaining >= 0, "Remaining invalid");
                }
                else {
                    uint64_t avail;
                    rv = mStreams[i]->Available(&avail);
                    if (NS_WARN_IF(NS_FAILED(rv)))
                        return rv;

                    int64_t newPos = XPCOM_MIN(remaining, streamPos + (int64_t)avail);

                    rv = stream->Seek(NS_SEEK_SET, newPos);
                    if (NS_WARN_IF(NS_FAILED(rv)))
                        return rv;

                    mCurrentStream = i;
                    mStartedReadingCurrent = true;

                    remaining -= newPos;
                    NS_ASSERTION(remaining >= 0, "Remaining invalid");
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
        int64_t remaining = aOffset;
        for (uint32_t i = mCurrentStream; remaining && i < mStreams.Length(); ++i) {
            nsCOMPtr<nsISeekableStream> stream =
                do_QueryInterface(mStreams[i]);

            uint64_t avail;
            rv = mStreams[i]->Available(&avail);
            if (NS_WARN_IF(NS_FAILED(rv)))
                return rv;

            int64_t seek = XPCOM_MIN((int64_t)avail, remaining);

            rv = stream->Seek(NS_SEEK_CUR, seek);
            if (NS_WARN_IF(NS_FAILED(rv)))
                return rv;

            mCurrentStream = i;
            mStartedReadingCurrent = true;

            remaining -= seek;
        }

        return NS_OK;
    }

    if (aWhence == NS_SEEK_CUR && aOffset < 0) {
        int64_t remaining = -aOffset;
        for (uint32_t i = mCurrentStream; remaining && i != (uint32_t)-1; --i) {
            nsCOMPtr<nsISeekableStream> stream =
                do_QueryInterface(mStreams[i]);

            int64_t pos;
            rv = stream->Tell(&pos);
            if (NS_WARN_IF(NS_FAILED(rv)))
                return rv;

            int64_t seek = XPCOM_MIN(pos, remaining);

            rv = stream->Seek(NS_SEEK_CUR, -seek);
            if (NS_WARN_IF(NS_FAILED(rv)))
                return rv;

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
        int64_t remaining = aOffset;
        for (uint32_t i = mStreams.Length() - 1; i != (uint32_t)-1; --i) {
            nsCOMPtr<nsISeekableStream> stream =
                do_QueryInterface(mStreams[i]);

            // See if all remaining streams should be seeked to end
            if (remaining == 0) {
                if (i >= oldCurrentStream) {
                    rv = stream->Seek(NS_SEEK_END, 0);
                    if (NS_WARN_IF(NS_FAILED(rv)))
                        return rv;
                }
                else {
                    break;
                }
            }

            // Get position in current stream
            int64_t streamPos;
            if (i < oldCurrentStream) {
                streamPos = 0;
            } else {
                uint64_t avail;
                rv = mStreams[i]->Available(&avail);
                if (NS_WARN_IF(NS_FAILED(rv)))
                    return rv;

                streamPos = avail;
            }

            // See if we have enough data in the current stream.
            if (DeprecatedAbs(remaining) < streamPos) {
                rv = stream->Seek(NS_SEEK_END, remaining);
                if (NS_WARN_IF(NS_FAILED(rv)))
                    return rv;

                mCurrentStream = i;
                mStartedReadingCurrent = true;

                remaining = 0;
            } else if (DeprecatedAbs(remaining) > streamPos) {
                if (i > oldCurrentStream ||
                    (i == oldCurrentStream && !oldStartedReadingCurrent)) {
                    // We're already at start so no need to seek this stream
                    remaining += streamPos;
                } else {
                    int64_t avail;
                    rv = stream->Tell(&avail);
                    if (NS_WARN_IF(NS_FAILED(rv)))
                        return rv;

                    int64_t newPos = streamPos + XPCOM_MIN(avail, DeprecatedAbs(remaining));

                    rv = stream->Seek(NS_SEEK_END, -newPos);
                    if (NS_WARN_IF(NS_FAILED(rv)))
                        return rv;

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

/* uint32_t tell (); */
NS_IMETHODIMP
nsMultiplexInputStream::Tell(int64_t *_retval)
{
    if (NS_FAILED(mStatus))
        return mStatus;

    nsresult rv;
    int64_t ret64 = 0;
    uint32_t i, last;
    last = mStartedReadingCurrent ? mCurrentStream+1 : mCurrentStream;
    for (i = 0; i < last; ++i) {
        nsCOMPtr<nsISeekableStream> stream = do_QueryInterface(mStreams[i]);
        if (NS_WARN_IF(!stream))
            return NS_ERROR_NO_INTERFACE;

        int64_t pos;
        rv = stream->Tell(&pos);
        if (NS_WARN_IF(NS_FAILED(rv)))
            return rv;
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

void
nsMultiplexInputStream::Serialize(InputStreamParams& aParams,
                                  FileDescriptorArray& aFileDescriptors)
{
    MultiplexInputStreamParams params;

    uint32_t streamCount = mStreams.Length();

    if (streamCount) {
        InfallibleTArray<InputStreamParams>& streams = params.streams();

        streams.SetCapacity(streamCount);
        for (uint32_t index = 0; index < streamCount; index++) {
            InputStreamParams childStreamParams;
            SerializeInputStream(mStreams[index], childStreamParams,
                                 aFileDescriptors);

            streams.AppendElement(childStreamParams);
        }
    }

    params.currentStream() = mCurrentStream;
    params.status() = mStatus;
    params.startedReadingCurrent() = mStartedReadingCurrent;

    aParams = params;
}

bool
nsMultiplexInputStream::Deserialize(const InputStreamParams& aParams,
                                    const FileDescriptorArray& aFileDescriptors)
{
    if (aParams.type() !=
            InputStreamParams::TMultiplexInputStreamParams) {
        NS_ERROR("Received unknown parameters from the other process!");
        return false;
    }

    const MultiplexInputStreamParams& params =
        aParams.get_MultiplexInputStreamParams();

    const InfallibleTArray<InputStreamParams>& streams = params.streams();

    uint32_t streamCount = streams.Length();
    for (uint32_t index = 0; index < streamCount; index++) {
        nsCOMPtr<nsIInputStream> stream =
            DeserializeInputStream(streams[index], aFileDescriptors);
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
