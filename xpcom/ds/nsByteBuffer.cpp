/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsByteBuffer.h"
#include "nsIInputStream.h"
#include "nsCRT.h"

#define MIN_BUFFER_SIZE 32

ByteBufferImpl::ByteBufferImpl(void)
  : mBuffer(NULL), mSpace(0), mLength(0)
{
  NS_INIT_REFCNT();
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

NS_METHOD
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

NS_IMETHODIMP_(PRBool)
ByteBufferImpl::Grow(PRUint32 aNewSize)
{
  if (aNewSize < MIN_BUFFER_SIZE) {
    aNewSize = MIN_BUFFER_SIZE;
  }
  char* newbuf = new char[aNewSize];
  if (nsnull != newbuf) {
    if (0 != mLength) {
      nsCRT::memcpy(newbuf, mBuffer, mLength);
    }
    delete[] mBuffer;
    mBuffer = newbuf;
    return PR_TRUE;
  }
  return PR_FALSE;
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
    nsCRT::memmove(mBuffer, mBuffer + (mLength - aKeep), aKeep);
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

NS_COM nsresult NS_NewByteBuffer(nsIByteBuffer** aInstancePtrResult,
                                  nsISupports* aOuter,
                                  PRUint32 aBufferSize)
{
  nsresult rv;
  nsIByteBuffer* buf;
  rv = ByteBufferImpl::Create(aOuter, nsIByteBuffer::GetIID(), (void**)&buf);
  if (NS_FAILED(rv)) return rv;
    
  rv = buf->Init(aBufferSize);
  if (NS_FAILED(rv)) {
    NS_RELEASE(buf);
    return rv;
  }
  *aInstancePtrResult = buf;
  return rv;
}
