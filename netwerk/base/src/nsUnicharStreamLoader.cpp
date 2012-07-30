/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicharStreamLoader.h"
#include "nsIInputStream.h"
#include "nsICharsetConverterManager.h"
#include "nsIServiceManager.h"

#define SNIFFING_BUFFER_SIZE 512 // specified in draft-abarth-mime-sniff-06

using mozilla::fallible_t;

NS_IMETHODIMP
nsUnicharStreamLoader::Init(nsIUnicharStreamLoaderObserver *aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);

  mObserver = aObserver;

  if (!mRawData.SetCapacity(SNIFFING_BUFFER_SIZE, fallible_t()))
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

nsresult
nsUnicharStreamLoader::Create(nsISupports *aOuter,
                              REFNSIID aIID,
                              void **aResult)
{
  if (aOuter) return NS_ERROR_NO_AGGREGATION;

  nsUnicharStreamLoader* it = new nsUnicharStreamLoader();
  NS_ADDREF(it);
  nsresult rv = it->QueryInterface(aIID, aResult);
  NS_RELEASE(it);
  return rv;
}

NS_IMPL_ISUPPORTS3(nsUnicharStreamLoader, nsIUnicharStreamLoader,
                   nsIRequestObserver, nsIStreamListener)

/* readonly attribute nsIChannel channel; */
NS_IMETHODIMP
nsUnicharStreamLoader::GetChannel(nsIChannel **aChannel)
{
  NS_IF_ADDREF(*aChannel = mChannel);
  return NS_OK;
}

/* readonly attribute nsACString charset */
NS_IMETHODIMP
nsUnicharStreamLoader::GetCharset(nsACString& aCharset)
{
  aCharset = mCharset;
  return NS_OK;
}

/* nsIRequestObserver implementation */
NS_IMETHODIMP
nsUnicharStreamLoader::OnStartRequest(nsIRequest*, nsISupports*)
{
  return NS_OK;
}

NS_IMETHODIMP
nsUnicharStreamLoader::OnStopRequest(nsIRequest *aRequest,
                                     nsISupports *aContext,
                                     nsresult aStatus)
{
  if (!mObserver) {
    NS_ERROR("nsUnicharStreamLoader::OnStopRequest called before ::Init");
    return NS_ERROR_UNEXPECTED;
  }

  mContext = aContext;
  mChannel = do_QueryInterface(aRequest);

  nsresult rv = NS_OK;
  if (mRawData.Length() > 0 && NS_SUCCEEDED(aStatus)) {
    NS_ABORT_IF_FALSE(mBuffer.Length() == 0,
                      "should not have both decoded and raw data");
    rv = DetermineCharset();
  }

  if (NS_FAILED(rv)) {
    // Call the observer but pass it no data.
    mObserver->OnStreamComplete(this, mContext, rv, EmptyString());
  } else {
    mObserver->OnStreamComplete(this, mContext, aStatus, mBuffer);
  }

  mObserver = nullptr;
  mDecoder = nullptr;
  mContext = nullptr;
  mChannel = nullptr;
  mCharset.Truncate();
  mBuffer.Truncate();
  return rv;
}

/* nsIStreamListener implementation */
NS_IMETHODIMP
nsUnicharStreamLoader::OnDataAvailable(nsIRequest *aRequest,
                                       nsISupports *aContext,
                                       nsIInputStream *aInputStream,
                                       PRUint32 aSourceOffset,
                                       PRUint32 aCount)
{
  if (!mObserver) {
    NS_ERROR("nsUnicharStreamLoader::OnDataAvailable called before ::Init");
    return NS_ERROR_UNEXPECTED;
  }

  mContext = aContext;
  mChannel = do_QueryInterface(aRequest);

  nsresult rv = NS_OK;
  if (mDecoder) {
    // process everything we've got
    PRUint32 dummy;
    aInputStream->ReadSegments(WriteSegmentFun, this, aCount, &dummy);
  } else {
    // no decoder yet.  Read up to SNIFFING_BUFFER_SIZE octets into
    // mRawData (this is the cutoff specified in
    // draft-abarth-mime-sniff-06).  If we can get that much, then go
    // ahead and fire charset detection and read the rest.  Otherwise
    // wait for more data.

    PRUint32 haveRead = mRawData.Length();
    PRUint32 toRead = NS_MIN(SNIFFING_BUFFER_SIZE - haveRead, aCount);
    PRUint32 n;
    char *here = mRawData.BeginWriting() + haveRead;

    rv = aInputStream->Read(here, toRead, &n);
    if (NS_SUCCEEDED(rv)) {
      mRawData.SetLength(haveRead + n);
      if (mRawData.Length() == SNIFFING_BUFFER_SIZE) {
        rv = DetermineCharset();
        if (NS_SUCCEEDED(rv)) {
          // process what's left
          PRUint32 dummy;
          aInputStream->ReadSegments(WriteSegmentFun, this, aCount - n, &dummy);
        }
      } else {
        NS_ABORT_IF_FALSE(n == aCount, "didn't read as much as was available");
      }
    }
  }

  mContext = nullptr;
  mChannel = nullptr;
  return rv;
}

/* internal */
static NS_DEFINE_CID(kCharsetConverterManagerCID,
                     NS_ICHARSETCONVERTERMANAGER_CID);

nsresult
nsUnicharStreamLoader::DetermineCharset()
{
  nsresult rv = mObserver->OnDetermineCharset(this, mContext,
                                              mRawData, mCharset);
  if (NS_FAILED(rv) || mCharset.IsEmpty()) {
    // The observer told us nothing useful
    mCharset.AssignLiteral("UTF-8");
  }

  // Create the decoder for this character set
  nsCOMPtr<nsICharsetConverterManager> ccm =
    do_GetService(kCharsetConverterManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = ccm->GetUnicodeDecoder(mCharset.get(), getter_AddRefs(mDecoder));
  if (NS_FAILED(rv)) return rv;

  // Process the data into mBuffer
  PRUint32 dummy;
  rv = WriteSegmentFun(nullptr, this,
                       mRawData.BeginReading(),
                       0, mRawData.Length(),
                       &dummy);
  mRawData.Truncate();
  return rv;
}

NS_METHOD
nsUnicharStreamLoader::WriteSegmentFun(nsIInputStream *,
                                       void *aClosure,
                                       const char *aSegment,
                                       PRUint32,
                                       PRUint32 aCount,
                                       PRUint32 *aWriteCount)
{
  nsUnicharStreamLoader* self = static_cast<nsUnicharStreamLoader*>(aClosure);

  PRUint32 haveRead = self->mBuffer.Length();
  PRUint32 consumed = 0;
  nsresult rv;
  do {
    PRInt32 srcLen = aCount - consumed;
    PRInt32 dstLen;
    self->mDecoder->GetMaxLength(aSegment + consumed, srcLen, &dstLen);

    PRUint32 capacity = haveRead + dstLen;
    if (!self->mBuffer.SetCapacity(capacity, fallible_t())) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    rv = self->mDecoder->Convert(aSegment + consumed,
                                 &srcLen,
                                 self->mBuffer.BeginWriting() + haveRead,
                                 &dstLen);
    haveRead += dstLen;
    // XXX if srcLen is negative, we want to drop the _first_ byte in
    // the erroneous byte sequence and try again.  This is not quite
    // possible right now -- see bug 160784
    consumed += srcLen;
    if (NS_FAILED(rv)) {
      NS_ASSERTION(0 < capacity - haveRead,
                   "Decoder returned an error but filled the output buffer! "
                   "Should not happen.");
      self->mBuffer.BeginWriting()[haveRead++] = 0xFFFD;
      ++consumed;
      // XXX this is needed to make sure we don't underrun our buffer;
      // bug 160784 again
      consumed = NS_MAX<PRUint32>(consumed, 0);
      self->mDecoder->Reset();
    }
  } while (consumed < aCount);

  self->mBuffer.SetLength(haveRead);
  *aWriteCount = aCount;
  return NS_OK;
}
