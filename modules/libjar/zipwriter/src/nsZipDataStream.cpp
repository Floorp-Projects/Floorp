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
 * The Original Code is Zip Writer Component.
 *
 * The Initial Developer of the Original Code is
 * Dave Townsend <dtownsend@oxymoronical.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Mook <mook.moz+random.code@gmail.com>
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
 * ***** END LICENSE BLOCK *****
 */

#include "StreamFunctions.h"
#include "nsZipDataStream.h"
#include "nsIStringStream.h"
#include "nsISeekableStream.h"
#include "nsDeflateConverter.h"
#include "nsNetUtil.h"
#include "nsComponentManagerUtils.h"
#include "nsMemory.h"

#define ZIP_METHOD_STORE 0
#define ZIP_METHOD_DEFLATE 8

/**
 * nsZipDataStream handles the writing an entry's into the zip file.
 * It is set up to wither write the data as is, or in the event that compression
 * has been requested to pass it through a stream converter.
 * Currently only the deflate compression method is supported.
 * The CRC checksum for the entry's data is also generated here.
 */
NS_IMPL_THREADSAFE_ISUPPORTS2(nsZipDataStream, nsIStreamListener,
                                               nsIRequestObserver)

nsresult nsZipDataStream::Init(nsZipWriter *aWriter,
                               nsIOutputStream *aStream,
                               nsZipHeader *aHeader,
                               PRInt32 aCompression)
{
    mWriter = aWriter;
    mHeader = aHeader;
    mStream = aStream;
    mHeader->mCRC = crc32(0L, Z_NULL, 0);

    nsresult rv = NS_NewSimpleStreamListener(getter_AddRefs(mOutput), aStream,
                                             nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    if (aCompression > 0) {
        mHeader->mMethod = ZIP_METHOD_DEFLATE;
        nsCOMPtr<nsIStreamConverter> converter =
                              new nsDeflateConverter(aCompression);
        NS_ENSURE_TRUE(converter, NS_ERROR_OUT_OF_MEMORY);

        rv = converter->AsyncConvertData("uncompressed", "rawdeflate", mOutput,
                                         nsnull);
        NS_ENSURE_SUCCESS(rv, rv);

        mOutput = do_QueryInterface(converter, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        mHeader->mMethod = ZIP_METHOD_STORE;
    }

    return NS_OK;
}

/* void onDataAvailable (in nsIRequest aRequest, in nsISupports aContext,
 *                       in nsIInputStream aInputStream,
 *                       in unsigned long aOffset, in unsigned long aCount); */
NS_IMETHODIMP nsZipDataStream::OnDataAvailable(nsIRequest *aRequest,
                                               nsISupports *aContext,
                                               nsIInputStream *aInputStream,
                                               PRUint32 aOffset,
                                               PRUint32 aCount)
{
    if (!mOutput)
        return NS_ERROR_NOT_INITIALIZED;

    nsAutoArrayPtr<char> buffer(new char[aCount]);
    NS_ENSURE_TRUE(buffer, NS_ERROR_OUT_OF_MEMORY);

    nsresult rv = ZW_ReadData(aInputStream, buffer.get(), aCount);
    NS_ENSURE_SUCCESS(rv, rv);

    return ProcessData(aRequest, aContext, buffer.get(), aOffset, aCount);
}

/* void onStartRequest (in nsIRequest aRequest, in nsISupports aContext); */
NS_IMETHODIMP nsZipDataStream::OnStartRequest(nsIRequest *aRequest,
                                              nsISupports *aContext)
{
    if (!mOutput)
        return NS_ERROR_NOT_INITIALIZED;

    return mOutput->OnStartRequest(aRequest, aContext);
}

/* void onStopRequest (in nsIRequest aRequest, in nsISupports aContext,
 *                     in nsresult aStatusCode); */
NS_IMETHODIMP nsZipDataStream::OnStopRequest(nsIRequest *aRequest,
                                             nsISupports *aContext,
                                             nsresult aStatusCode)
{
    if (!mOutput)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = mOutput->OnStopRequest(aRequest, aContext, aStatusCode);
    mOutput = nsnull;
    if (NS_FAILED(rv)) {
        mWriter->EntryCompleteCallback(mHeader, rv);
    }
    else {
        rv = CompleteEntry();
        rv = mWriter->EntryCompleteCallback(mHeader, rv);
    }

    mStream = nsnull;
    mWriter = nsnull;
    mHeader = nsnull;

    return rv;
}

inline nsresult nsZipDataStream::CompleteEntry()
{
    nsresult rv;
    nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mStream, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    PRInt64 pos;
    rv = seekable->Tell(&pos);
    NS_ENSURE_SUCCESS(rv, rv);

    mHeader->mCSize = pos - mHeader->mOffset - mHeader->GetFileHeaderLength();
    mHeader->mWriteOnClose = PR_TRUE;
    return NS_OK;
}

nsresult nsZipDataStream::ProcessData(nsIRequest *aRequest,
                                      nsISupports *aContext, char *aBuffer,
                                      PRUint32 aOffset, PRUint32 aCount)
{
    mHeader->mCRC = crc32(mHeader->mCRC,
                          reinterpret_cast<const unsigned char*>(aBuffer),
                          aCount);

    nsresult rv;
    nsCOMPtr<nsIStringInputStream> stream =
             do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    stream->ShareData(aBuffer, aCount);
    rv = mOutput->OnDataAvailable(aRequest, aContext, stream, aOffset, aCount);
    mHeader->mUSize += aCount;

    return rv;
}

nsresult nsZipDataStream::ReadStream(nsIInputStream *aStream)
{
    if (!mOutput)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = OnStartRequest(nsnull, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoArrayPtr<char> buffer(new char[4096]);
    NS_ENSURE_TRUE(buffer, NS_ERROR_OUT_OF_MEMORY);

    PRUint32 read = 0;
    PRUint32 offset = 0;
    do
    {
        rv = aStream->Read(buffer.get(), 4096, &read);
        if (NS_FAILED(rv)) {
            OnStopRequest(nsnull, nsnull, rv);
            return rv;
        }

        if (read > 0) {
            rv = ProcessData(nsnull, nsnull, buffer.get(), offset, read);
            if (NS_FAILED(rv)) {
                OnStopRequest(nsnull, nsnull, rv);
                return rv;
            }
            offset += read;
        }
    } while (read > 0);

    return OnStopRequest(nsnull, nsnull, NS_OK);
}
