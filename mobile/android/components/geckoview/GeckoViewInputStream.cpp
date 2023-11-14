/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoViewInputStream.h"
#include "nsStreamUtils.h"

NS_IMPL_ISUPPORTS(GeckoViewInputStream, nsIInputStream);

NS_IMETHODIMP
GeckoViewInputStream::Close() {
  mClosed = true;
  mInstance->Close();

  return NS_OK;
}

NS_IMETHODIMP
GeckoViewInputStream::Available(uint64_t* aCount) {
  if (mClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  *aCount = static_cast<uint64_t>(mInstance->Available());
  return NS_OK;
}

NS_IMETHODIMP
GeckoViewInputStream::StreamStatus() {
  return mClosed ? NS_BASE_STREAM_CLOSED : NS_OK;
}

NS_IMETHODIMP
GeckoViewInputStream::Read(char* aBuf, uint32_t aCount, uint32_t* aReadCount) {
  return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, aReadCount);
}

NS_IMETHODIMP
GeckoViewInputStream::ReadSegments(nsWriteSegmentFun writer, void* aClosure,
                                   uint32_t aCount, uint32_t* result) {
  NS_ASSERTION(result, "null ptr");

  if (mClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  auto bufferAddress =
      static_cast<const char*>(mInstance->MBuffer()->Address());
  uint32_t segmentPos = static_cast<uint32_t>(mInstance->MPos());
  int32_t dataLength = 0;
  nsresult rv;

  *result = 0;
  while (aCount) {
    rv = mInstance->Read(static_cast<int64_t>(aCount), &dataLength);
    if (NS_FAILED(rv)) {
      return NS_BASE_STREAM_OSERROR;
    }

    if (dataLength == -1) {
      break;
    }

    uint32_t uDataLength = static_cast<uint32_t>(dataLength);
    uint32_t written;
    rv = writer(this, aClosure, bufferAddress + segmentPos, *result,
                uDataLength, &written);

    if (NS_FAILED(rv)) {
      // InputStreams do not propagate errors to caller.
      break;
    }

    NS_ASSERTION(written > 0, "Must have written something");

    *result += written;
    aCount -= written;

    segmentPos = static_cast<uint32_t>(mInstance->ConsumedData(written));
  }

  return NS_OK;
}

NS_IMETHODIMP
GeckoViewInputStream::IsNonBlocking(bool* aNonBlocking) {
  *aNonBlocking = true;
  return NS_OK;
}

bool GeckoViewInputStream::isClosed() const { return mInstance->IsClosed(); }

bool GeckoViewContentInputStream::isReadable(const nsAutoCString& aUri) {
  return mozilla::java::ContentInputStream::IsReadable(
      mozilla::jni::StringParam(aUri));
}

nsresult GeckoViewContentInputStream::getInstance(const nsAutoCString& aUri,
                                                  nsIInputStream** aInstance) {
  RefPtr<GeckoViewContentInputStream> instance =
      new GeckoViewContentInputStream(aUri);
  if (instance->isClosed()) {
    return NS_ERROR_FILE_NOT_FOUND;
  }
  *aInstance = instance.forget().take();

  return NS_OK;
}
