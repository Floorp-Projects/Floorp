/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBase64Encoder.h"

#include "plbase64.h"
#include "prmem.h"

NS_IMPL_ISUPPORTS(nsBase64Encoder, nsIOutputStream)

NS_IMETHODIMP
nsBase64Encoder::Close()
{
  return NS_OK;
}

NS_IMETHODIMP
nsBase64Encoder::Flush()
{
  return NS_OK;
}

NS_IMETHODIMP
nsBase64Encoder::Write(const char* aBuf, uint32_t aCount, uint32_t* _retval)
{
  mData.Append(aBuf, aCount);
  *_retval = aCount;
  return NS_OK;
}

NS_IMETHODIMP
nsBase64Encoder::WriteFrom(nsIInputStream* aStream, uint32_t aCount,
                           uint32_t* _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBase64Encoder::WriteSegments(nsReadSegmentFun aReader,
                               void* aClosure,
                               uint32_t aCount,
                               uint32_t* _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBase64Encoder::IsNonBlocking(bool* aNonBlocking)
{
  *aNonBlocking = false;
  return NS_OK;
}

nsresult
nsBase64Encoder::Finish(nsACString& result)
{
  char* b64 = PL_Base64Encode(mData.get(), mData.Length(), nullptr);
  if (!b64)
    return NS_ERROR_OUT_OF_MEMORY;

  result.Assign(b64);
  PR_Free(b64); // PL_Base64Encode() uses PR_MALLOC().
  // Free unneeded memory and allow reusing the object
  mData.Truncate();
  return NS_OK;
}
