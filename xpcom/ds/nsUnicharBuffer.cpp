/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicharBuffer.h"
#include "nsCRT.h"

#define MIN_BUFFER_SIZE 32

UnicharBufferImpl::UnicharBufferImpl()
  : mBuffer(NULL), mSpace(0), mLength(0)
{
}

NS_METHOD
UnicharBufferImpl::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  UnicharBufferImpl* it = new UnicharBufferImpl();
  if (it == nullptr) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(it);
  nsresult rv = it->QueryInterface(aIID, aResult);
  NS_RELEASE(it);
  return rv;
}

NS_IMETHODIMP
UnicharBufferImpl::Init(PRUint32 aBufferSize)
{
  if (aBufferSize < MIN_BUFFER_SIZE) {
    aBufferSize = MIN_BUFFER_SIZE;
  }
  mSpace = aBufferSize;
  mLength = 0;
  mBuffer = new PRUnichar[aBufferSize];
  return mBuffer ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMPL_ISUPPORTS1(UnicharBufferImpl, nsIUnicharBuffer)

UnicharBufferImpl::~UnicharBufferImpl()
{
  if (nullptr != mBuffer) {
    delete[] mBuffer;
    mBuffer = nullptr;
  }
  mLength = 0;
}

NS_IMETHODIMP_(PRInt32)
UnicharBufferImpl::GetLength() const
{
  return mLength;
}

NS_IMETHODIMP_(PRInt32)
UnicharBufferImpl::GetBufferSize() const
{
  return mSpace;
}

NS_IMETHODIMP_(PRUnichar*)
UnicharBufferImpl::GetBuffer() const
{
  return mBuffer;
}

NS_IMETHODIMP_(bool)
UnicharBufferImpl::Grow(PRInt32 aNewSize)
{
  if (PRUint32(aNewSize) < MIN_BUFFER_SIZE) {
    aNewSize = MIN_BUFFER_SIZE;
  }
  PRUnichar* newbuf = new PRUnichar[aNewSize];
  if (nullptr != newbuf) {
    if (0 != mLength) {
      memcpy(newbuf, mBuffer, mLength * sizeof(PRUnichar));
    }
    delete[] mBuffer;
    mBuffer = newbuf;
    return true;
  }
  return false;
}

nsresult
NS_NewUnicharBuffer(nsIUnicharBuffer** aInstancePtrResult,
                    nsISupports* aOuter,
                    PRUint32 aBufferSize)
{
  nsresult rv;
  nsIUnicharBuffer* buf;
  rv = UnicharBufferImpl::Create(aOuter, NS_GET_IID(nsIUnicharBuffer), 
                                 (void**)&buf);
  if (NS_FAILED(rv)) return rv;
  rv = buf->Init(aBufferSize);
  if (NS_FAILED(rv)) {
    NS_RELEASE(buf);
    return rv;
  }
  *aInstancePtrResult = buf;
  return rv;
}
