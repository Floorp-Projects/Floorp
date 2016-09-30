/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The MIME stream separates headers and a datastream. It also allows
 * automatic creation of the content-length header.
 */

#include "ipc/IPCMessageUtils.h"

#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIMultiplexInputStream.h"
#include "nsIMIMEInputStream.h"
#include "nsISeekableStream.h"
#include "nsIStringStream.h"
#include "nsString.h"
#include "nsMIMEInputStream.h"
#include "nsIClassInfoImpl.h"
#include "nsIIPCSerializableInputStream.h"
#include "mozilla/ipc/InputStreamUtils.h"

using namespace mozilla::ipc;
using mozilla::Maybe;

class nsMIMEInputStream : public nsIMIMEInputStream,
                          public nsISeekableStream,
                          public nsIIPCSerializableInputStream
{
    virtual ~nsMIMEInputStream();

public:
    nsMIMEInputStream();

    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIMIMEINPUTSTREAM
    NS_DECL_NSISEEKABLESTREAM
    NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM

    nsresult Init();

private:

    void InitStreams();

    struct MOZ_STACK_CLASS ReadSegmentsState {
        nsCOMPtr<nsIInputStream> mThisStream;
        nsWriteSegmentFun mWriter;
        void* mClosure;
    };
    static nsresult ReadSegCb(nsIInputStream* aIn, void* aClosure,
                              const char* aFromRawSegment, uint32_t aToOffset,
                              uint32_t aCount, uint32_t *aWriteCount);

    nsCString mHeaders;
    nsCOMPtr<nsIStringInputStream> mHeaderStream;
    
    nsCString mContentLength;
    nsCOMPtr<nsIStringInputStream> mCLStream;
    
    nsCOMPtr<nsIInputStream> mData;
    nsCOMPtr<nsIMultiplexInputStream> mStream;
    bool mAddContentLength;
    bool mStartedReading;
};

NS_IMPL_ADDREF(nsMIMEInputStream)
NS_IMPL_RELEASE(nsMIMEInputStream)

NS_IMPL_CLASSINFO(nsMIMEInputStream, nullptr, nsIClassInfo::THREADSAFE,
                  NS_MIMEINPUTSTREAM_CID)

NS_IMPL_QUERY_INTERFACE_CI(nsMIMEInputStream,
                           nsIMIMEInputStream,
                           nsIInputStream,
                           nsISeekableStream,
                           nsIIPCSerializableInputStream)
NS_IMPL_CI_INTERFACE_GETTER(nsMIMEInputStream,
                            nsIMIMEInputStream,
                            nsIInputStream,
                            nsISeekableStream)

nsMIMEInputStream::nsMIMEInputStream() : mAddContentLength(false),
                                         mStartedReading(false)
{
}

nsMIMEInputStream::~nsMIMEInputStream()
{
}

nsresult nsMIMEInputStream::Init()
{
    nsresult rv = NS_OK;
    mStream = do_CreateInstance("@mozilla.org/io/multiplex-input-stream;1",
                                &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    mHeaderStream = do_CreateInstance("@mozilla.org/io/string-input-stream;1",
                                      &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    mCLStream = do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStream->AppendStream(mHeaderStream);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mStream->AppendStream(mCLStream);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
}


NS_IMETHODIMP
nsMIMEInputStream::GetAddContentLength(bool *aAddContentLength)
{
    *aAddContentLength = mAddContentLength;
    return NS_OK;
}
NS_IMETHODIMP
nsMIMEInputStream::SetAddContentLength(bool aAddContentLength)
{
    NS_ENSURE_FALSE(mStartedReading, NS_ERROR_FAILURE);
    mAddContentLength = aAddContentLength;
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInputStream::AddHeader(const char *aName, const char *aValue)
{
    NS_ENSURE_FALSE(mStartedReading, NS_ERROR_FAILURE);
    mHeaders.Append(aName);
    mHeaders.AppendLiteral(": ");
    mHeaders.Append(aValue);
    mHeaders.AppendLiteral("\r\n");

    // Just in case someone somehow uses our stream, lets at least
    // let the stream have a valid pointer. The stream will be properly
    // initialized in nsMIMEInputStream::InitStreams
    mHeaderStream->ShareData(mHeaders.get(), 0);

    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInputStream::SetData(nsIInputStream *aStream)
{
    NS_ENSURE_FALSE(mStartedReading, NS_ERROR_FAILURE);
    // Remove the old stream if there is one
    if (mData)
        mStream->RemoveStream(2);

    mData = aStream;
    if (aStream)
        mStream->AppendStream(mData);
    return NS_OK;
}

NS_IMETHODIMP
nsMIMEInputStream::GetData(nsIInputStream **aStream)
{
  NS_ENSURE_ARG_POINTER(aStream);
  *aStream = mData;
  NS_IF_ADDREF(*aStream);
  return NS_OK;
}

// set up the internal streams
void nsMIMEInputStream::InitStreams()
{
    NS_ASSERTION(!mStartedReading,
                 "Don't call initStreams twice without rewinding");

    mStartedReading = true;

    // We'll use the content-length stream to add the final \r\n
    if (mAddContentLength) {
        uint64_t cl = 0;
        if (mData) {
            mData->Available(&cl);
        }
        mContentLength.AssignLiteral("Content-Length: ");
        mContentLength.AppendInt(cl);
        mContentLength.AppendLiteral("\r\n\r\n");
    }
    else {
        mContentLength.AssignLiteral("\r\n");
    }
    mCLStream->ShareData(mContentLength.get(), -1);
    mHeaderStream->ShareData(mHeaders.get(), -1);
}



#define INITSTREAMS         \
if (!mStartedReading) {     \
    InitStreams();          \
}

// Reset mStartedReading when Seek-ing to start
NS_IMETHODIMP
nsMIMEInputStream::Seek(int32_t whence, int64_t offset)
{
    nsresult rv;
    nsCOMPtr<nsISeekableStream> stream = do_QueryInterface(mStream);
    if (whence == NS_SEEK_SET && offset == 0) {
        rv = stream->Seek(whence, offset);
        if (NS_SUCCEEDED(rv))
            mStartedReading = false;
    }
    else {
        INITSTREAMS;
        rv = stream->Seek(whence, offset);
    }

    return rv;
}

// Proxy ReadSegments since we need to be a good little nsIInputStream
NS_IMETHODIMP nsMIMEInputStream::ReadSegments(nsWriteSegmentFun aWriter,
                                              void *aClosure, uint32_t aCount,
                                              uint32_t *_retval)
{
    INITSTREAMS;
    ReadSegmentsState state;
    state.mThisStream = this;
    state.mWriter = aWriter;
    state.mClosure = aClosure;
    return mStream->ReadSegments(ReadSegCb, &state, aCount, _retval);
}

nsresult
nsMIMEInputStream::ReadSegCb(nsIInputStream* aIn, void* aClosure,
                             const char* aFromRawSegment,
                             uint32_t aToOffset, uint32_t aCount,
                             uint32_t *aWriteCount)
{
    ReadSegmentsState* state = (ReadSegmentsState*)aClosure;
    return  (state->mWriter)(state->mThisStream,
                             state->mClosure,
                             aFromRawSegment,
                             aToOffset,
                             aCount,
                             aWriteCount);
}

/**
 * Forward everything else to the mStream after calling InitStreams()
 */

// nsIInputStream
NS_IMETHODIMP nsMIMEInputStream::Close(void) { INITSTREAMS; return mStream->Close(); }
NS_IMETHODIMP nsMIMEInputStream::Available(uint64_t *_retval) { INITSTREAMS; return mStream->Available(_retval); }
NS_IMETHODIMP nsMIMEInputStream::Read(char * buf, uint32_t count, uint32_t *_retval) { INITSTREAMS; return mStream->Read(buf, count, _retval); }
NS_IMETHODIMP nsMIMEInputStream::IsNonBlocking(bool *aNonBlocking) { INITSTREAMS; return mStream->IsNonBlocking(aNonBlocking); }

// nsISeekableStream
NS_IMETHODIMP nsMIMEInputStream::Tell(int64_t *_retval)
{
    INITSTREAMS;
    nsCOMPtr<nsISeekableStream> stream = do_QueryInterface(mStream);
    return stream->Tell(_retval);
}
NS_IMETHODIMP nsMIMEInputStream::SetEOF(void) {
    INITSTREAMS;
    nsCOMPtr<nsISeekableStream> stream = do_QueryInterface(mStream);
    return stream->SetEOF();
}


/**
 * Factory method used by do_CreateInstance
 */

nsresult
nsMIMEInputStreamConstructor(nsISupports *outer, REFNSIID iid, void **result)
{
    *result = nullptr;

    if (outer)
        return NS_ERROR_NO_AGGREGATION;

    nsMIMEInputStream *inst = new nsMIMEInputStream();
    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(inst);

    nsresult rv = inst->Init();
    if (NS_FAILED(rv)) {
        NS_RELEASE(inst);
        return rv;
    }

    rv = inst->QueryInterface(iid, result);
    NS_RELEASE(inst);

    return rv;
}

void
nsMIMEInputStream::Serialize(InputStreamParams& aParams,
                             FileDescriptorArray& aFileDescriptors)
{
    MIMEInputStreamParams params;

    if (mData) {
        nsCOMPtr<nsIInputStream> stream = do_QueryInterface(mData);
        MOZ_ASSERT(stream);

        InputStreamParams wrappedParams;
        SerializeInputStream(stream, wrappedParams, aFileDescriptors);

        NS_ASSERTION(wrappedParams.type() != InputStreamParams::T__None,
                     "Wrapped stream failed to serialize!");

        params.optionalStream() = wrappedParams;
    }
    else {
        params.optionalStream() = mozilla::void_t();
    }

    params.headers() = mHeaders;
    params.contentLength() = mContentLength;
    params.startedReading() = mStartedReading;
    params.addContentLength() = mAddContentLength;

    aParams = params;
}

bool
nsMIMEInputStream::Deserialize(const InputStreamParams& aParams,
                               const FileDescriptorArray& aFileDescriptors)
{
    if (aParams.type() != InputStreamParams::TMIMEInputStreamParams) {
        NS_ERROR("Received unknown parameters from the other process!");
        return false;
    }

    const MIMEInputStreamParams& params =
        aParams.get_MIMEInputStreamParams();
    const OptionalInputStreamParams& wrappedParams = params.optionalStream();

    mHeaders = params.headers();
    mContentLength = params.contentLength();
    mStartedReading = params.startedReading();

    // nsMIMEInputStream::Init() already appended mHeaderStream & mCLStream
    mHeaderStream->ShareData(mHeaders.get(),
                             mStartedReading ? mHeaders.Length() : 0);
    mCLStream->ShareData(mContentLength.get(),
                         mStartedReading ? mContentLength.Length() : 0);

    nsCOMPtr<nsIInputStream> stream;
    if (wrappedParams.type() == OptionalInputStreamParams::TInputStreamParams) {
        stream = DeserializeInputStream(wrappedParams.get_InputStreamParams(),
                                        aFileDescriptors);
        if (!stream) {
            NS_WARNING("Failed to deserialize wrapped stream!");
            return false;
        }

        mData = stream;

        if (NS_FAILED(mStream->AppendStream(mData))) {
            NS_WARNING("Failed to append stream!");
            return false;
        }
    }
    else {
        NS_ASSERTION(wrappedParams.type() == OptionalInputStreamParams::Tvoid_t,
                     "Unknown type for OptionalInputStreamParams!");
    }

    mAddContentLength = params.addContentLength();

    return true;
}

Maybe<uint64_t>
nsMIMEInputStream::ExpectedSerializedLength()
{
    nsCOMPtr<nsIIPCSerializableInputStream> serializable = do_QueryInterface(mStream);
    return serializable ? serializable->ExpectedSerializedLength() : Nothing();
}

