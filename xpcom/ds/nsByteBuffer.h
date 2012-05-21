/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsByteBuffer_h__
#define nsByteBuffer_h__

#include "mozilla/Attributes.h"

#include "nsIByteBuffer.h"

class ByteBufferImpl MOZ_FINAL : public nsIByteBuffer {
public:
  ByteBufferImpl(void);

  NS_DECL_ISUPPORTS

  static nsresult
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  NS_IMETHOD Init(PRUint32 aBufferSize);
  NS_IMETHOD_(PRUint32) GetLength(void) const;
  NS_IMETHOD_(PRUint32) GetBufferSize(void) const;
  NS_IMETHOD_(char*) GetBuffer() const;
  NS_IMETHOD_(bool) Grow(PRUint32 aNewSize);
  NS_IMETHOD_(PRInt32) Fill(nsresult* aErrorCode, nsIInputStream* aStream,
                            PRUint32 aKeep);

  char* mBuffer;
  PRUint32 mSpace;
  PRUint32 mLength;
private:
  ~ByteBufferImpl();
};

#endif // nsByteBuffer_h__
