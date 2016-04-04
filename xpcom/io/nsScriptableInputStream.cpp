/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsScriptableInputStream.h"
#include "nsMemory.h"
#include "nsString.h"

NS_IMPL_ISUPPORTS(nsScriptableInputStream, nsIScriptableInputStream)

// nsIScriptableInputStream methods
NS_IMETHODIMP
nsScriptableInputStream::Close()
{
  if (!mInputStream) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return mInputStream->Close();
}

NS_IMETHODIMP
nsScriptableInputStream::Init(nsIInputStream* aInputStream)
{
  if (!aInputStream) {
    return NS_ERROR_NULL_POINTER;
  }
  mInputStream = aInputStream;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptableInputStream::Available(uint64_t* aResult)
{
  if (!mInputStream) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  return mInputStream->Available(aResult);
}

NS_IMETHODIMP
nsScriptableInputStream::Read(uint32_t aCount, char** aResult)
{
  nsresult rv = NS_OK;
  uint64_t count64 = 0;
  char* buffer = nullptr;

  if (!mInputStream) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  rv = mInputStream->Available(&count64);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // bug716556 - Ensure count+1 doesn't overflow
  uint32_t count =
    XPCOM_MIN((uint32_t)XPCOM_MIN<uint64_t>(count64, aCount), UINT32_MAX - 1);
  buffer = (char*)malloc(count + 1);  // make room for '\0'
  if (!buffer) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = ReadHelper(buffer, count);
  if (NS_FAILED(rv)) {
    free(buffer);
    return rv;
  }

  buffer[count] = '\0';
  *aResult = buffer;
  return NS_OK;
}

NS_IMETHODIMP
nsScriptableInputStream::ReadBytes(uint32_t aCount, nsACString& aResult)
{
  if (!mInputStream) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (!aResult.SetLength(aCount, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  MOZ_ASSERT(aResult.Length() == aCount);
  char* ptr = aResult.BeginWriting();
  nsresult rv = ReadHelper(ptr, aCount);
  if (NS_FAILED(rv)) {
    aResult.Truncate();
  }
  return rv;
}

nsresult
nsScriptableInputStream::ReadHelper(char* aBuffer, uint32_t aCount)
{
  uint32_t totalBytesRead = 0;
  while (1) {
    uint32_t bytesRead;
    nsresult rv = mInputStream->Read(aBuffer + totalBytesRead,
                                     aCount - totalBytesRead,
                                     &bytesRead);
    if (NS_FAILED(rv)) {
      return rv;
    }

    totalBytesRead += bytesRead;
    if (totalBytesRead == aCount) {
      break;
    }

    // If we have read zero bytes, we have hit EOF.
    if (bytesRead == 0) {
      return NS_ERROR_FAILURE;
    }

  }
  return NS_OK;
}

nsresult
nsScriptableInputStream::Create(nsISupports* aOuter, REFNSIID aIID,
                                void** aResult)
{
  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }

  RefPtr<nsScriptableInputStream> sis = new nsScriptableInputStream();
  return sis->QueryInterface(aIID, aResult);
}
