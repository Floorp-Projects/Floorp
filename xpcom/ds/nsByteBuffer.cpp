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
#include "nsIByteBuffer.h"
#include "nsIInputStream.h"
#include "nsCRT.h"

#define MIN_BUFFER_SIZE 32

class ByteBufferImpl : public nsIByteBuffer {
public:
  ByteBufferImpl(PRInt32 aBufferSize);
  ~ByteBufferImpl();

  NS_DECL_ISUPPORTS
  virtual PRInt32 GetLength() const;
  virtual PRInt32 GetBufferSize() const;
  virtual char* GetBuffer() const;
  virtual PRBool Grow(PRInt32 aNewSize);
  virtual PRInt32 Fill(PRInt32* aErrorCode, nsIInputStream* aStream,
                       PRInt32 aKeep);

  char* mBuffer;
  PRInt32 mSpace;
  PRInt32 mLength;
};

ByteBufferImpl::ByteBufferImpl(PRInt32 aBufferSize)
{
  if (PRUint32(aBufferSize) < MIN_BUFFER_SIZE) {
    aBufferSize = MIN_BUFFER_SIZE;
  }
  mSpace = aBufferSize;
  mBuffer = new char[aBufferSize];
  mLength = 0;
  NS_INIT_REFCNT();
}

NS_DEFINE_IID(kByteBufferIID,NS_IBYTE_BUFFER_IID);
NS_IMPL_ISUPPORTS(ByteBufferImpl,kByteBufferIID)

ByteBufferImpl::~ByteBufferImpl()
{
  if (nsnull != mBuffer) {
    delete mBuffer;
    mBuffer = nsnull;
  }
  mLength = 0;
}

PRInt32 ByteBufferImpl::GetLength() const
{
  return mLength;
}

PRInt32 ByteBufferImpl::GetBufferSize() const
{
  return mSpace;
}

char* ByteBufferImpl::GetBuffer() const
{
  return mBuffer;
}

PRBool ByteBufferImpl::Grow(PRInt32 aNewSize)
{
  if (PRUint32(aNewSize) < MIN_BUFFER_SIZE) {
    aNewSize = MIN_BUFFER_SIZE;
  }
  char* newbuf = new char[aNewSize];
  if (nsnull != newbuf) {
    if (0 != mLength) {
      nsCRT::memcpy(newbuf, mBuffer, mLength);
    }
    delete mBuffer;
    mBuffer = newbuf;
    return PR_TRUE;
  }
  return PR_FALSE;
}

PRInt32 ByteBufferImpl::Fill(PRInt32* aErrorCode, nsIInputStream* aStream,
                             PRInt32 aKeep)
{
  NS_PRECONDITION(nsnull != aStream, "null stream");
  NS_PRECONDITION(PRUint32(aKeep) <= PRUint32(mLength), "illegal keep count");
  if ((nsnull == aStream) || (PRUint32(aKeep) > PRUint32(mLength))) {
    // whoops
    *aErrorCode = NS_INPUTSTREAM_ILLEGAL_ARGS;
    return -1;
  }

  if (0 != aKeep) {
    // Slide over kept data
    nsCRT::memmove(mBuffer, mBuffer + (mLength - aKeep), aKeep);
  }

  // Read in some new data
  mLength = aKeep;
  PRInt32 amount = mSpace - aKeep;
  PRInt32 nb = aStream->Read(aErrorCode, mBuffer, aKeep, amount);
  if (nb > 0) {
    mLength += nb;
  }
  return nb;
}

NS_BASE nsresult NS_NewByteBuffer(nsIByteBuffer** aInstancePtrResult,
                                  nsISupports* aOuter,
                                  PRInt32 aBufferSize)
{
  if (nsnull != aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }
  ByteBufferImpl* it = new ByteBufferImpl(aBufferSize);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kByteBufferIID, (void **) aInstancePtrResult);
}
