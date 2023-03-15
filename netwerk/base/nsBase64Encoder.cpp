/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBase64Encoder.h"

#include "mozilla/Base64.h"

NS_IMPL_ISUPPORTS(nsBase64Encoder, nsIOutputStream)

NS_IMETHODIMP
nsBase64Encoder::Close() { return NS_OK; }

NS_IMETHODIMP
nsBase64Encoder::Flush() { return NS_OK; }

NS_IMETHODIMP
nsBase64Encoder::StreamStatus() { return NS_OK; }

NS_IMETHODIMP
nsBase64Encoder::Write(const char* aBuf, uint32_t aCount, uint32_t* _retval) {
  mData.Append(aBuf, aCount);
  *_retval = aCount;
  return NS_OK;
}

NS_IMETHODIMP
nsBase64Encoder::WriteFrom(nsIInputStream* aStream, uint32_t aCount,
                           uint32_t* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBase64Encoder::WriteSegments(nsReadSegmentFun aReader, void* aClosure,
                               uint32_t aCount, uint32_t* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBase64Encoder::IsNonBlocking(bool* aNonBlocking) {
  *aNonBlocking = false;
  return NS_OK;
}

nsresult nsBase64Encoder::Finish(nsACString& result) {
  nsresult rv = mozilla::Base64Encode(mData, result);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Free unneeded memory and allow reusing the object
  mData.Truncate();
  return NS_OK;
}
