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

#ifndef nsTransferable_h__
#define nsTransferable_h__

#include "nsITransferable.h"

class nsISupportsArray;
class nsIDataFlavor;
class nsDataObj;

#define MAX_FORMATS 32

/**
 * Native Win32 Transferable wrapper
 */

class nsTransferable : public nsITransferable
{

public:
  nsTransferable();
  virtual ~nsTransferable();

  //nsISupports
  NS_DECL_ISUPPORTS
  
  //nsITransferable
  NS_IMETHOD GetTransferDataFlavors(nsISupportsArray ** aDataFlavorList);
  NS_IMETHOD IsDataFlavorSupported(nsIDataFlavor * aFlavor);

  NS_IMETHOD GetTransferData(nsIDataFlavor * aFlavor, void ** aData, PRUint32 * aDataLen);
  NS_IMETHOD SetTransferData(nsIDataFlavor * aFlavor, void * aData, PRUint32 aDataLen);

  NS_IMETHOD SetTransferString(const nsString & aStr);
  NS_IMETHOD GetTransferString(nsString & aStr);

  NS_IMETHOD AddDataFlavor(const nsString & aMimeType, const nsString & aHumanPresentableName);
  NS_IMETHOD SetNativeClipboard();

  // Used for native implementation
  nsDataObj * GetDataObj() { return mDataObj; }

protected:

  nsISupportsArray * mDFList;
  nsDataObj * mDataObj;

  nsString * mStrCache;
  void     * mDataPtr;
};

#endif // nsTransferable_h__
