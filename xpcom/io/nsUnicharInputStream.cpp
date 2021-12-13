/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicharInputStream.h"
#include "nsIInputStream.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsCRT.h"
#include "nsStreamUtils.h"
#include "nsConverterInputStream.h"
#include "mozilla/Attributes.h"
#include <fcntl.h>
#if defined(XP_WIN)
#  include <io.h>
#else
#  include <unistd.h>
#endif

#define STRING_BUFFER_SIZE 8192

class StringUnicharInputStream final : public nsIUnicharInputStream {
 public:
  explicit StringUnicharInputStream(const nsAString& aString)
      : mString(aString), mPos(0), mLen(aString.Length()) {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIUNICHARINPUTSTREAM

  nsString mString;
  uint32_t mPos;
  uint32_t mLen;

 private:
  ~StringUnicharInputStream() = default;
};

NS_IMETHODIMP
StringUnicharInputStream::Read(char16_t* aBuf, uint32_t aCount,
                               uint32_t* aReadCount) {
  if (mPos >= mLen) {
    *aReadCount = 0;
    return NS_OK;
  }
  nsAString::const_iterator iter;
  mString.BeginReading(iter);
  const char16_t* us = iter.get();
  uint32_t amount = mLen - mPos;
  if (amount > aCount) {
    amount = aCount;
  }
  memcpy(aBuf, us + mPos, sizeof(char16_t) * amount);
  mPos += amount;
  *aReadCount = amount;
  return NS_OK;
}

NS_IMETHODIMP
StringUnicharInputStream::ReadSegments(nsWriteUnicharSegmentFun aWriter,
                                       void* aClosure, uint32_t aCount,
                                       uint32_t* aReadCount) {
  uint32_t bytesWritten;
  uint32_t totalBytesWritten = 0;

  nsresult rv;
  aCount = XPCOM_MIN<uint32_t>(mString.Length() - mPos, aCount);

  nsAString::const_iterator iter;
  mString.BeginReading(iter);

  while (aCount) {
    rv = aWriter(this, aClosure, iter.get() + mPos, totalBytesWritten, aCount,
                 &bytesWritten);

    if (NS_FAILED(rv)) {
      // don't propagate errors to the caller
      break;
    }

    aCount -= bytesWritten;
    totalBytesWritten += bytesWritten;
    mPos += bytesWritten;
  }

  *aReadCount = totalBytesWritten;

  return NS_OK;
}

NS_IMETHODIMP
StringUnicharInputStream::ReadString(uint32_t aCount, nsAString& aString,
                                     uint32_t* aReadCount) {
  if (mPos >= mLen) {
    *aReadCount = 0;
    return NS_OK;
  }
  uint32_t amount = mLen - mPos;
  if (amount > aCount) {
    amount = aCount;
  }
  aString = Substring(mString, mPos, amount);
  mPos += amount;
  *aReadCount = amount;
  return NS_OK;
}

nsresult StringUnicharInputStream::Close() {
  mPos = mLen;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(StringUnicharInputStream, nsIUnicharInputStream)

//----------------------------------------------------------------------

nsresult NS_NewUnicharInputStream(nsIInputStream* aStreamToWrap,
                                  nsIUnicharInputStream** aResult) {
  *aResult = nullptr;

  // Create converter input stream
  RefPtr<nsConverterInputStream> it = new nsConverterInputStream();
  nsresult rv = it->Init(aStreamToWrap, "UTF-8", STRING_BUFFER_SIZE,
                         nsIConverterInputStream::ERRORS_ARE_FATAL);
  if (NS_FAILED(rv)) {
    return rv;
  }

  it.forget(aResult);
  return NS_OK;
}
