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

#ifndef nsUnicharBuffer_h__
#define nsUnicharBuffer_h__

#include "nsIUnicharBuffer.h"

class UnicharBufferImpl : public nsIUnicharBuffer {
public:
  UnicharBufferImpl();
  virtual ~UnicharBufferImpl();

  static NS_METHOD
  Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  NS_DECL_ISUPPORTS
  NS_IMETHOD Init(PRUint32 aBufferSize);
  NS_IMETHOD_(PRInt32) GetLength() const;
  NS_IMETHOD_(PRInt32) GetBufferSize() const;
  NS_IMETHOD_(PRUnichar*) GetBuffer() const;
  NS_IMETHOD_(PRBool) Grow(PRInt32 aNewSize);
  NS_IMETHOD_(PRInt32) Fill(nsresult* aErrorCode, nsIUnicharInputStream* aStream,
                            PRInt32 aKeep);

  PRUnichar* mBuffer;
  PRUint32 mSpace;
  PRUint32 mLength;
};

#endif // nsUnicharBuffer_h__
