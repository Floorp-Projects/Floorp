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
#ifndef nsIUnicharBuffer_h___
#define nsIUnicharBuffer_h___

#include "nscore.h"
#include "nsISupports.h"
class nsIUnicharInputStream;

#define NS_IUNICHAR_BUFFER_IID \
{ 0x14cf6970, 0x93b5, 0x11d1, \
  {0x89, 0x5b, 0x00, 0x60, 0x08, 0x91, 0x1b, 0x81} }

/// Interface to a buffer that holds unicode characters
class nsIUnicharBuffer : public nsISupports {
public:
  virtual PRInt32 GetLength() const = 0;
  virtual PRInt32 GetBufferSize() const = 0;
  virtual PRUnichar* GetBuffer() const = 0;
  virtual PRBool Grow(PRInt32 aNewSize) = 0;
  virtual PRInt32 Fill(PRInt32* aErrorCode, nsIUnicharInputStream* aStream,
                       PRInt32 aKeep) = 0;
};

/// Factory method for nsIUnicharBuffer.
extern NS_BASE nsresult
  NS_NewUnicharBuffer(nsIUnicharBuffer** aInstancePtrResult,
                      nsISupports* aOuter,
                      PRInt32 aBufferSize = 0);

#endif /* nsIUnicharBuffer_h___ */
