/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "StreamFunctions.h"
#include "MainThreadUtils.h"
#include "nsDeflateConverter.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "nsStringStream.h"
#include "nsComponentManagerUtils.h"
#include "nsCRT.h"
#include "plstr.h"
#include "mozilla/UniquePtr.h"

#define ZLIB_TYPE "deflate"
#define GZIP_TYPE "gzip"
#define X_GZIP_TYPE "x-gzip"

using namespace mozilla;

/**
 * nsDeflateConverter is a stream converter applies the deflate compression
 * method to the data.
 */
NS_IMPL_ISUPPORTS(nsDeflateConverter, nsIStreamConverter, nsIStreamListener,
                  nsIThreadRetargetableStreamListener, nsIRequestObserver)

nsresult nsDeflateConverter::Init() {
  int zerr;

  mOffset = 0;

  mZstream.zalloc = Z_NULL;
  mZstream.zfree = Z_NULL;
  mZstream.opaque = Z_NULL;

  int32_t window = MAX_WBITS;
  switch (mWrapMode) {
    case WRAP_NONE:
      window = -window;
      break;
    case WRAP_GZIP:
      window += 16;
      break;
    default:
      break;
  }

  zerr = deflateInit2(&mZstream, mLevel, Z_DEFLATED, window, 8,
                      Z_DEFAULT_STRATEGY);
  if (zerr != Z_OK) return NS_ERROR_OUT_OF_MEMORY;

  mZstream.next_out = mWriteBuffer;
  mZstream.avail_out = sizeof(mWriteBuffer);

  // mark the input buffer as empty.
  mZstream.avail_in = 0;
  mZstream.next_in = Z_NULL;

  return NS_OK;
}

NS_IMETHODIMP nsDeflateConverter::Convert(nsIInputStream* aFromStream,
                                          const char* aFromType,
                                          const char* aToType,
                                          nsISupports* aCtxt,
                                          nsIInputStream** _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDeflateConverter::AsyncConvertData(const char* aFromType,
                                                   const char* aToType,
                                                   nsIStreamListener* aListener,
                                                   nsISupports* aCtxt) {
  if (mListener) return NS_ERROR_ALREADY_INITIALIZED;

  NS_ENSURE_ARG_POINTER(aListener);

  if (!PL_strncasecmp(aToType, ZLIB_TYPE, sizeof(ZLIB_TYPE) - 1)) {
    mWrapMode = WRAP_ZLIB;
  } else if (!nsCRT::strcasecmp(aToType, GZIP_TYPE) ||
             !nsCRT::strcasecmp(aToType, X_GZIP_TYPE)) {
    mWrapMode = WRAP_GZIP;
  } else {
    mWrapMode = WRAP_NONE;
  }

  nsresult rv = Init();
  NS_ENSURE_SUCCESS(rv, rv);

  mListener = aListener;
  mContext = aCtxt;
  return rv;
}

NS_IMETHODIMP
nsDeflateConverter::GetConvertedType(const nsACString& aFromType,
                                     nsIChannel* aChannel,
                                     nsACString& aToType) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDeflateConverter::OnDataAvailable(nsIRequest* aRequest,
                                                  nsIInputStream* aInputStream,
                                                  uint64_t aOffset,
                                                  uint32_t aCount) {
  if (!mListener) return NS_ERROR_NOT_INITIALIZED;

  auto buffer = MakeUnique<char[]>(aCount);
  NS_ENSURE_TRUE(buffer, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = ZW_ReadData(aInputStream, buffer.get(), aCount);
  NS_ENSURE_SUCCESS(rv, rv);

  // make sure we aren't reading too much
  mZstream.avail_in = aCount;
  mZstream.next_in = (unsigned char*)buffer.get();

  int zerr = Z_OK;
  // deflate loop
  while (mZstream.avail_in > 0 && zerr == Z_OK) {
    zerr = deflate(&mZstream, Z_NO_FLUSH);

    while (mZstream.avail_out == 0) {
      // buffer is full, push the data out to the listener
      rv = PushAvailableData(aRequest);
      NS_ENSURE_SUCCESS(rv, rv);
      zerr = deflate(&mZstream, Z_NO_FLUSH);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsDeflateConverter::OnStartRequest(nsIRequest* aRequest) {
  if (!mListener) return NS_ERROR_NOT_INITIALIZED;

  return mListener->OnStartRequest(aRequest);
}

NS_IMETHODIMP
nsDeflateConverter::OnDataFinished(nsresult aStatus) {
  nsCOMPtr<nsIThreadRetargetableStreamListener> retargetable =
      do_QueryInterface(mListener);

  if (retargetable) {
    return retargetable->OnDataFinished(aStatus);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDeflateConverter::CheckListenerChain() { return NS_ERROR_NO_INTERFACE; }

NS_IMETHODIMP nsDeflateConverter::OnStopRequest(nsIRequest* aRequest,
                                                nsresult aStatusCode) {
  if (!mListener) return NS_ERROR_NOT_INITIALIZED;

  nsresult rv;

  int zerr;
  do {
    zerr = deflate(&mZstream, Z_FINISH);
    rv = PushAvailableData(aRequest);
    NS_ENSURE_SUCCESS(rv, rv);
  } while (zerr == Z_OK);

  deflateEnd(&mZstream);

  return mListener->OnStopRequest(aRequest, aStatusCode);
}

nsresult nsDeflateConverter::PushAvailableData(nsIRequest* aRequest) {
  uint32_t bytesToWrite = sizeof(mWriteBuffer) - mZstream.avail_out;
  // We don't need to do anything if there isn't any data
  if (bytesToWrite == 0) return NS_OK;

  MOZ_ASSERT(bytesToWrite <= INT32_MAX);
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewByteInputStream(getter_AddRefs(stream),
                                      Span((char*)mWriteBuffer, bytesToWrite),
                                      NS_ASSIGNMENT_DEPEND);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mListener->OnDataAvailable(aRequest, stream, mOffset, bytesToWrite);

  // now set the state for 'deflate'
  mZstream.next_out = mWriteBuffer;
  mZstream.avail_out = sizeof(mWriteBuffer);

  mOffset += bytesToWrite;
  return rv;
}
