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

#ifndef nsBaseClipboard_h__
#define nsBaseClipboard_h__

#include "nsIClipboard.h"
#include "nsITransferable.h"

class nsITransferable;
class nsDataObj;
class nsIClipboardOwner;
class nsIWidget;

/**
 * Native Win32 BaseClipboard wrapper
 */

class nsBaseClipboard : public nsIClipboard
{

public:
  nsBaseClipboard();
  virtual ~nsBaseClipboard();

  //nsISupports
  NS_DECL_ISUPPORTS

  // nsIBaseClipboard  
  NS_IMETHOD SetData(nsITransferable * aTransferable, nsIClipboardOwner * anOwner);
  NS_IMETHOD GetData(nsITransferable * aTransferable);

  NS_IMETHOD IsDataFlavorSupported(nsIDataFlavor * aDataFlavor);
  NS_IMETHOD EmptyClipboard();
  NS_IMETHOD ForceDataToClipboard();


protected:

  NS_IMETHOD SetNativeClipboardData() = 0;
  NS_IMETHOD GetNativeClipboardData(nsITransferable * aTransferable) = 0;

  PRBool              mIgnoreEmptyNotification;
  nsIClipboardOwner * mClipboardOwner;
  nsITransferable   * mTransferable;

};

#endif // nsBaseClipboard_h__

