/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsByteBuffer_h__
#define nsByteBuffer_h__

#include "nsIByteBuffer.h"

class ByteBufferImpl : public nsIByteBuffer {
public:
  ByteBufferImpl(void);
  virtual ~ByteBufferImpl();

  NS_DECL_ISUPPORTS

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  NS_IMETHOD Init(PRUint32 aBufferSize);
  NS_IMETHOD_(PRUint32) GetLength(void) const;
  NS_IMETHOD_(PRUint32) GetBufferSize(void) const;
  NS_IMETHOD_(char*) GetBuffer() const;
  NS_IMETHOD_(PRBool) Grow(PRUint32 aNewSize);
  NS_IMETHOD_(PRInt32) Fill(nsresult* aErrorCode, nsIInputStream* aStream,
                            PRUint32 aKeep);

  char* mBuffer;
  PRUint32 mSpace;
  PRUint32 mLength;
};

#endif // nsByteBuffer_h__
