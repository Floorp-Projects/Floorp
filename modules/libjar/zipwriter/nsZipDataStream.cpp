/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "StreamFunctions.h"
#include "nsZipDataStream.h"
#include "nsStringStream.h"
#include "nsISeekableStream.h"
#include "nsDeflateConverter.h"
#include "nsNetUtil.h"
#include "nsComponentManagerUtils.h"
#include "nsMemory.h"

#define ZIP_METHOD_STORE 0
#define ZIP_METHOD_DEFLATE 8

using namespace mozilla;

/**
 * nsZipDataStream handles the writing an entry's into the zip file.
 * It is set up to wither write the data as is, or in the event that compression
 * has been requested to pass it through a stream converter.
 * Currently only the deflate compression method is supported.
 * The CRC checksum for the entry's data is also generated here.
 */
NS_IMPL_ISUPPORTS(nsZipDataStream, nsIStreamListener,
                  nsIRequestObserver)

nsresult nsZipDataStream::Init(nsZipWriter *aWriter,
                               nsIOutputStream *aStream,
                               nsZipHeader *aHeader,
                               int32_t aCompression)
{
    mWriter = aWriter;
    mHeader = aHeader;
    mStream = aStream;
    mHeader->mCRC = crc32(0L, Z_NULL, 0);

    nsresult rv = NS_NewSimpleStreamListener(getter_AddRefs(mOutput), aStream,
                                             nullptr);
    NS_ENSURE_SUCCESS(rv, rv);

    if (aCompression > 0) {
        mHeader->mMethod = ZIP_METHOD_DEFLATE;
        nsCOMPtr<nsIStreamConverter> converter =
                              new nsDeflateConverter(aCompression);
        NS_ENSURE_TRUE(converter, NS_ERROR_OUT_OF_MEMORY);

        rv = converter->AsyncConvertData("uncompressed", "rawdeflate", mOutput,
                                         nullptr);
        NS_ENSURE_SUCCESS(rv, rv);

        mOutput = do_QueryInterface(converter, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        mHeader->mMethod = ZIP_METHOD_STORE;
    }

    return NS_OK;
}

NS_IMETHODIMP nsZipDataStream::OnDataAvailable(nsIRequest *aRequest,
                                               nsISupports *aContext,
                                               nsIInputStream *aInputStream,
                                               uint64_t aOffset,
                                               uint32_t aCount)
{
    if (!mOutput)
        return NS_ERROR_NOT_INITIALIZED;

    auto buffer = MakeUnique<char[]>(aCount);
    NS_ENSURE_TRUE(buffer, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = ZW_ReadData(aInputStream, buffer.get(), aCount);
    NS_ENSURE_SUCCESS(rv, rv);

    return ProcessData(aRequest, aContext, buffer.get(), aOffset, aCount);
}

NS_IMETHODIMP nsZipDataStream::OnStartRequest(nsIRequest *aRequest,
                                              nsISupports *aContext)
{
    if (!mOutput)
        return NS_ERROR_NOT_INITIALIZED;

    return mOutput->OnStartRequest(aRequest, aContext);
}

NS_IMETHODIMP nsZipDataStream::OnStopRequest(nsIRequest *aRequest,
                                             nsISupports *aContext,
                                             nsresult aStatusCode)
{
    if (!mOutput)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = mOutput->OnStopRequest(aRequest, aContext, aStatusCode);
    mOutput = nullptr;
    if (NS_FAILED(rv)) {
        mWriter->EntryCompleteCallback(mHeader, rv);
    }
    else {
        rv = CompleteEntry();
        rv = mWriter->EntryCompleteCallback(mHeader, rv);
    }

    mStream = nullptr;
    mWriter = nullptr;
    mHeader = nullptr;

    return rv;
}

inline nsresult nsZipDataStream::CompleteEntry()
{
    nsresult rv;
    nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mStream, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    int64_t pos;
    rv = seekable->Tell(&pos);
    NS_ENSURE_SUCCESS(rv, rv);

    mHeader->mCSize = pos - mHeader->mOffset - mHeader->GetFileHeaderLength();
    mHeader->mWriteOnClose = true;
    return NS_OK;
}

nsresult nsZipDataStream::ProcessData(nsIRequest *aRequest,
                                      nsISupports *aContext, char *aBuffer,
                                      uint64_t aOffset, uint32_t aCount)
{
    mHeader->mCRC = crc32(mHeader->mCRC,
                          reinterpret_cast<const unsigned char*>(aBuffer),
                          aCount);

    MOZ_ASSERT(aCount <= INT32_MAX);
    nsCOMPtr<nsIInputStream> stream;
    nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream),
					aBuffer, aCount);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mOutput->OnDataAvailable(aRequest, aContext, stream, aOffset, aCount);
    mHeader->mUSize += aCount;

    return rv;
}

nsresult nsZipDataStream::ReadStream(nsIInputStream *aStream)
{
    if (!mOutput)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = OnStartRequest(nullptr, nullptr);
    NS_ENSURE_SUCCESS(rv, rv);

    auto buffer = MakeUnique<char[]>(4096);
    NS_ENSURE_TRUE(buffer, NS_ERROR_OUT_OF_MEMORY);

    uint32_t read = 0;
    uint32_t offset = 0;
    do
    {
        rv = aStream->Read(buffer.get(), 4096, &read);
        if (NS_FAILED(rv)) {
            OnStopRequest(nullptr, nullptr, rv);
            return rv;
        }

        if (read > 0) {
            rv = ProcessData(nullptr, nullptr, buffer.get(), offset, read);
            if (NS_FAILED(rv)) {
                OnStopRequest(nullptr, nullptr, rv);
                return rv;
            }
            offset += read;
        }
    } while (read > 0);

    return OnStopRequest(nullptr, nullptr, NS_OK);
}
