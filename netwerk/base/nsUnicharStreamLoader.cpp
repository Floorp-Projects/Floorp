/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "nsUnicharStreamLoader.h"
#include "nsIInputStream.h"
#include <algorithm>
#include "mozilla/Encoding.h"

// 1024 bytes is specified in
// http://www.whatwg.org/specs/web-apps/current-work/#charset for HTML; for
// other resource types (e.g. CSS) typically fewer bytes are fine too, since
// they only look at things right at the beginning of the data.
#define SNIFFING_BUFFER_SIZE 1024

using namespace mozilla;

NS_IMETHODIMP
nsUnicharStreamLoader::Init(nsIUnicharStreamLoaderObserver *aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);

  mObserver = aObserver;

  if (!mRawData.SetCapacity(SNIFFING_BUFFER_SIZE, fallible))
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

NS_IMPL_ISUPPORTS(nsUnicharStreamLoader, nsIUnicharStreamLoader,
                  nsIRequestObserver, nsIStreamListener)

NS_IMETHODIMP
nsUnicharStreamLoader::GetChannel(nsIChannel **aChannel)
{
  NS_IF_ADDREF(*aChannel = mChannel);
  return NS_OK;
}

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
    MOZ_ASSERT(mBuffer.Length() == 0,
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
  mRawData.Truncate();
  mRawBuffer.Truncate();
  mBuffer.Truncate();
  return rv;
}

NS_IMETHODIMP
nsUnicharStreamLoader::GetRawBuffer(nsACString& aRawBuffer)
{
  aRawBuffer = mRawBuffer;
  return NS_OK;
}

/* nsIStreamListener implementation */
NS_IMETHODIMP
nsUnicharStreamLoader::OnDataAvailable(nsIRequest *aRequest,
                                       nsISupports *aContext,
                                       nsIInputStream *aInputStream,
                                       uint64_t aSourceOffset,
                                       uint32_t aCount)
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
    uint32_t dummy;
    aInputStream->ReadSegments(WriteSegmentFun, this, aCount, &dummy);
  } else {
    // no decoder yet.  Read up to SNIFFING_BUFFER_SIZE octets into
    // mRawData (this is the cutoff specified in
    // draft-abarth-mime-sniff-06).  If we can get that much, then go
    // ahead and fire charset detection and read the rest.  Otherwise
    // wait for more data.

    uint32_t haveRead = mRawData.Length();
    uint32_t toRead = std::min(SNIFFING_BUFFER_SIZE - haveRead, aCount);
    uint32_t n;
    char *here = mRawData.BeginWriting() + haveRead;

    rv = aInputStream->Read(here, toRead, &n);
    if (NS_SUCCEEDED(rv)) {
      mRawData.SetLength(haveRead + n);
      if (mRawData.Length() == SNIFFING_BUFFER_SIZE) {
        rv = DetermineCharset();
        if (NS_SUCCEEDED(rv)) {
          // process what's left
          uint32_t dummy;
          aInputStream->ReadSegments(WriteSegmentFun, this, aCount - n, &dummy);
        }
      } else {
        MOZ_ASSERT(n == aCount, "didn't read as much as was available");
      }
    }
  }

  mContext = nullptr;
  mChannel = nullptr;
  return rv;
}

nsresult
nsUnicharStreamLoader::DetermineCharset()
{
  nsresult rv = mObserver->OnDetermineCharset(this, mContext,
                                              mRawData, mCharset);
  if (NS_FAILED(rv) || mCharset.IsEmpty()) {
    // The observer told us nothing useful
    mCharset.AssignLiteral("UTF-8");
  }

  // Sadly, nsIUnicharStreamLoader is exposed to extensions, so we can't
  // assume mozilla::css::Loader to be the only caller. Special-casing
  // replacement, since it's not invariant under a second label resolution
  // operation.
  if (mCharset.EqualsLiteral("replacement")) {
    mDecoder = REPLACEMENT_ENCODING->NewDecoderWithBOMRemoval();
  } else {
    const Encoding* encoding = Encoding::ForLabelNoReplacement(mCharset);
    if (!encoding) {
      // If we got replacement here, the caller was not mozilla::css::Loader
      // but an extension.
      return NS_ERROR_UCONV_NOCONV;
    }
    mDecoder = encoding->NewDecoderWithBOMRemoval();
  }

  // Process the data into mBuffer
  uint32_t dummy;
  rv = WriteSegmentFun(nullptr, this,
                       mRawData.BeginReading(),
                       0, mRawData.Length(),
                       &dummy);
  mRawData.Truncate();
  return rv;
}

nsresult
nsUnicharStreamLoader::WriteSegmentFun(nsIInputStream *,
                                       void *aClosure,
                                       const char *aSegment,
                                       uint32_t,
                                       uint32_t aCount,
                                       uint32_t *aWriteCount)
{
  nsUnicharStreamLoader* self = static_cast<nsUnicharStreamLoader*>(aClosure);

  nsAString::size_type haveRead(self->mBuffer.Length());

  CheckedInt<size_t> needed = self->mDecoder->MaxUTF16BufferLength(aCount);
  if (!needed.isValid()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  CheckedInt<nsAString::size_type> capacity(needed.value());
  capacity += haveRead;
  if (!capacity.isValid()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!self->mBuffer.SetCapacity(capacity.value(), fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!self->mRawBuffer.Append(aSegment, aCount, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  uint32_t result;
  size_t read;
  size_t written;
  bool hadErrors;

  Tie(result, read, written, hadErrors) = self->mDecoder->DecodeToUTF16(
    AsBytes(MakeSpan(aSegment, aCount)),
    MakeSpan(self->mBuffer.BeginWriting() + haveRead, needed.value()),
    false);
  MOZ_ASSERT(result == kInputEmpty);
  MOZ_ASSERT(read == aCount);
  Unused << hadErrors;

  CheckedInt<nsAString::size_type> newLen(written);
  newLen += haveRead;
  if (!newLen.isValid()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  self->mBuffer.SetLength(newLen.value());
  *aWriteCount = aCount;
  return NS_OK;
}
