/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsByteBuffer.h"
#include "nsIInputStream.h"
#include "nsCRT.h"

#define MIN_BUFFER_SIZE 32

ByteBufferImpl::ByteBufferImpl(void)
  : mBuffer(NULL), mSpace(0), mLength(0)
{
}

NS_IMETHODIMP
ByteBufferImpl::Init(PRUint32 aBufferSize)
{
  if (aBufferSize < MIN_BUFFER_SIZE) {
    aBufferSize = MIN_BUFFER_SIZE;
  }
  mSpace = aBufferSize;
  mLength = 0;
  mBuffer = new char[aBufferSize];
  return mBuffer ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMPL_ISUPPORTS1(ByteBufferImpl,nsIByteBuffer)

ByteBufferImpl::~ByteBufferImpl()
{
  if (nsnull != mBuffer) {
    delete[] mBuffer;
    mBuffer = nsnull;
  }
  mLength = 0;
}

nsresult
ByteBufferImpl::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  ByteBufferImpl* it = new ByteBufferImpl();
  if (nsnull == it) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(it);
  nsresult rv = it->QueryInterface(aIID, (void**)aResult);
  NS_RELEASE(it);
  return rv;
}

NS_IMETHODIMP_(PRUint32)
ByteBufferImpl::GetLength(void) const
{
  return mLength;
}

NS_IMETHODIMP_(PRUint32)
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
ByteBufferImpl::Grow(PRUint32 aNewSize)
{
  if (aNewSize < MIN_BUFFER_SIZE) {
    aNewSize = MIN_BUFFER_SIZE;
  }
  char* newbuf = new char[aNewSize];
  if (nsnull != newbuf) {
    if (0 != mLength) {
      memcpy(newbuf, mBuffer, mLength);
    }
    delete[] mBuffer;
    mBuffer = newbuf;
    return true;
  }
  return false;
}

NS_IMETHODIMP_(PRInt32)
ByteBufferImpl::Fill(nsresult* aErrorCode, nsIInputStream* aStream,
                     PRUint32 aKeep)
{
  NS_PRECONDITION(nsnull != aStream, "null stream");
  NS_PRECONDITION(aKeep <= mLength, "illegal keep count");
  if ((nsnull == aStream) || (PRUint32(aKeep) > PRUint32(mLength))) {
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
  PRUint32 nb;
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
                                  PRUint32 aBufferSize)
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
