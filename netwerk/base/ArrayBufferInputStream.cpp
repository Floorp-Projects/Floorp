/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include "ArrayBufferInputStream.h"
#include "nsStreamUtils.h"
#include "jsapi.h"
#include "jsfriendapi.h"

NS_IMPL_ISUPPORTS(ArrayBufferInputStream, nsIArrayBufferInputStream, nsIInputStream);

ArrayBufferInputStream::ArrayBufferInputStream()
: mBufferLength(0)
, mPos(0)
, mClosed(false)
{
}

NS_IMETHODIMP
ArrayBufferInputStream::SetData(JS::Handle<JS::Value> aBuffer,
                                uint32_t aByteOffset,
                                uint32_t aLength,
                                JSContext* aCx)
{
  if (!aBuffer.isObject()) {
    return NS_ERROR_FAILURE;
  }
  JS::RootedObject arrayBuffer(aCx, &aBuffer.toObject());
  if (!JS_IsArrayBufferObject(arrayBuffer)) {
    return NS_ERROR_FAILURE;
  }

  uint32_t buflen = JS_GetArrayBufferByteLength(arrayBuffer);
  uint32_t offset = std::min(buflen, aByteOffset);
  mBufferLength = std::min(buflen - offset, aLength);

  mArrayBuffer = mozilla::MakeUnique<char[]>(mBufferLength);

  JS::AutoCheckCannotGC nogc;
  bool isShared;
  char* src = (char*) JS_GetArrayBufferData(arrayBuffer, &isShared, nogc) + offset;
  memcpy(&mArrayBuffer[0], src, mBufferLength);
  return NS_OK;
}

NS_IMETHODIMP
ArrayBufferInputStream::Close()
{
  mClosed = true;
  return NS_OK;
}

NS_IMETHODIMP
ArrayBufferInputStream::Available(uint64_t* aCount)
{
  if (mClosed) {
    return NS_BASE_STREAM_CLOSED;
  }
  if (mArrayBuffer) {
    *aCount = mBufferLength ? mBufferLength - mPos : 0;
  } else {
    *aCount = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
ArrayBufferInputStream::Read(char* aBuf, uint32_t aCount, uint32_t *aReadCount)
{
  return ReadSegments(NS_CopySegmentToBuffer, aBuf, aCount, aReadCount);
}

NS_IMETHODIMP
ArrayBufferInputStream::ReadSegments(nsWriteSegmentFun writer, void *closure,
                                     uint32_t aCount, uint32_t *result)
{
  NS_ASSERTION(result, "null ptr");
  NS_ASSERTION(mBufferLength >= mPos, "bad stream state");

  if (mClosed) {
    return NS_BASE_STREAM_CLOSED;
  }

  MOZ_ASSERT(mArrayBuffer || (mPos == mBufferLength), "stream inited incorrectly");

  *result = 0;
  while (mPos < mBufferLength) {
    uint32_t remaining = mBufferLength - mPos;
    MOZ_ASSERT(mArrayBuffer);

    uint32_t count = std::min(aCount, remaining);
    if (count == 0) {
      break;
    }

    uint32_t written;
    nsresult rv = writer(this, closure, &mArrayBuffer[0] + mPos, *result, count, &written);
    if (NS_FAILED(rv)) {
      // InputStreams do not propagate errors to caller.
      return NS_OK;
    }

    NS_ASSERTION(written <= count,
                 "writer should not write more than we asked it to write");
    mPos += written;
    *result += written;
    aCount -= written;
  }

  return NS_OK;
}

NS_IMETHODIMP
ArrayBufferInputStream::IsNonBlocking(bool *aNonBlocking)
{
  *aNonBlocking = true;
  return NS_OK;
}
