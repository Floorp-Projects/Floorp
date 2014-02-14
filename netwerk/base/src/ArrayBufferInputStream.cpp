/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include "ArrayBufferInputStream.h"
#include "nsStreamUtils.h"
#include "jsapi.h"
#include "jsfriendapi.h"

NS_IMPL_ISUPPORTS2(ArrayBufferInputStream, nsIArrayBufferInputStream, nsIInputStream);

ArrayBufferInputStream::ArrayBufferInputStream()
: mRt(nullptr)
, mArrayBuffer(JSVAL_VOID)
, mBuffer(nullptr)
, mBufferLength(0)
, mOffset(0)
, mPos(0)
, mClosed(false)
{
}

ArrayBufferInputStream::~ArrayBufferInputStream()
{
  if (mRt) {
    JS_RemoveValueRootRT(mRt, &mArrayBuffer);
  }
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

  mRt = JS_GetRuntime(aCx);
  mArrayBuffer = aBuffer;
  JS_AddNamedValueRootRT(mRt, &mArrayBuffer, "mArrayBuffer");

  uint32_t buflen = JS_GetArrayBufferByteLength(arrayBuffer);
  mOffset = std::min(buflen, aByteOffset);
  mBufferLength = std::min(buflen - mOffset, aLength);
  mBuffer = JS_GetStableArrayBufferData(aCx, arrayBuffer);
  if (!mBuffer) {
      return NS_ERROR_FAILURE;
  }
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
  *aCount = mBufferLength - mPos;
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

  uint32_t remaining = mBufferLength - mPos;
  if (!remaining) {
    *result = 0;
    return NS_OK;
  }

  if (aCount > remaining) {
    aCount = remaining;
  }
  nsresult rv = writer(this, closure, (char*)(mBuffer + mOffset) + mPos,
                       0, aCount, result);
  if (NS_SUCCEEDED(rv)) {
    NS_ASSERTION(*result <= aCount,
                 "writer should not write more than we asked it to write");
    mPos += *result;
  }

  return NS_OK;
}

NS_IMETHODIMP
ArrayBufferInputStream::IsNonBlocking(bool *aNonBlocking)
{
  *aNonBlocking = true;
  return NS_OK;
}
