/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsByteBuffer.h"
#include "nsIInputStream.h"
#include "nsCRT.h"
#include "nsAutoPtr.h"

#define MIN_BUFFER_SIZE 32

ByteBufferImpl::ByteBufferImpl(void)
  : mBuffer(NULL), mSpace(0), mLength(0)
{
}

NS_IMETHODIMP
ByteBufferImpl::Init(uint32_t aBufferSize)
{
  if (aBufferSize < MIN_BUFFER_SIZE) {
    aBufferSize = MIN_BUFFER_SIZE;
  }
  mSpace = aBufferSize;
  mLength = 0;
  mBuffer = new char[aBufferSize];
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(ByteBufferImpl,nsIByteBuffer)

ByteBufferImpl::~ByteBufferImpl()
{
  if (nullptr != mBuffer) {
    delete[] mBuffer;
    mBuffer = nullptr;
  }
  mLength = 0;
}

nsresult
ByteBufferImpl::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsRefPtr<ByteBufferImpl> it = new ByteBufferImpl();

  return it->QueryInterface(aIID, (void**)aResult);
}

NS_IMETHODIMP_(uint32_t)
ByteBufferImpl::GetLength(void) const
{
  return mLength;
}

NS_IMETHODIMP_(uint32_t)
ByteBufferImpl::GetBufferSize(void) const
{
  return mSpace;
}

NS_IMETHODIMP_(char*)
ByteBufferImpl::GetBuffer(void) const
{
  return mBuffer;
}

NS_IMETHODIMP_(bool)
ByteBufferImpl::Grow(uint32_t aNewSize)
{
  if (aNewSize < MIN_BUFFER_SIZE) {
    aNewSize = MIN_BUFFER_SIZE;
  }
  char* newbuf = new char[aNewSize];
  if (0 != mLength) {
    memcpy(newbuf, mBuffer, mLength);
  }
  delete[] mBuffer;
  mBuffer = newbuf;
  return true;
}

NS_IMETHODIMP_(int32_t)
ByteBufferImpl::Fill(nsresult* aErrorCode, nsIInputStream* aStream,
                     uint32_t aKeep)
{
  NS_PRECONDITION(nullptr != aStream, "null stream");
  NS_PRECONDITION(aKeep <= mLength, "illegal keep count");
  if ((nullptr == aStream) || (uint32_t(aKeep) > uint32_t(mLength))) {
    // whoops
    *aErrorCode = NS_BASE_STREAM_ILLEGAL_ARGS;
    return -1;
  }

  if (0 != aKeep) {
    // Slide over kept data
    memmove(mBuffer, mBuffer + (mLength - aKeep), aKeep);
  }

  // Read in some new data
  mLength = aKeep;
  uint32_t nb;
  *aErrorCode = aStream->Read(mBuffer + aKeep, mSpace - aKeep, &nb);
  if (NS_SUCCEEDED(*aErrorCode)) {
    mLength += nb;
  }
  else
    nb = 0;
  return nb;
}

nsresult NS_NewByteBuffer(nsIByteBuffer** aInstancePtrResult,
                                  nsISupports* aOuter,
                                  uint32_t aBufferSize)
{
  nsresult rv;
  nsIByteBuffer* buf;
  rv = ByteBufferImpl::Create(aOuter, NS_GET_IID(nsIByteBuffer), (void**)&buf);
  if (NS_FAILED(rv)) return rv;
    
  rv = buf->Init(aBufferSize);
  if (NS_FAILED(rv)) {
    NS_RELEASE(buf);
    return rv;
  }
  *aInstancePtrResult = buf;
  return rv;
}
