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

#ifndef nsClipboard_h__
#define nsClipboard_h__

#include "nsIClipboard.h"
#include "nsITransferable.h"

class nsITransferable;
class nsDataObj;
class nsIClipboardOwner;
class nsIWidget;

/**
 * Native Win32 Clipboard wrapper
 */

class nsClipboard : public nsIClipboard
{

public:
  nsClipboard();
  virtual ~nsClipboard();

  //nsISupports
  NS_DECL_ISUPPORTS
  

  // nsIClipboard  
  NS_IMETHOD SetTransferable(nsITransferable * aTransferable, nsIClipboardOwner * anOwner);
  NS_IMETHOD GetTransferable(nsITransferable ** aTransferable);

  NS_IMETHOD GetClipboard();
  NS_IMETHOD SetClipboard();
  NS_IMETHOD IsDataFlavorSupported(nsIDataFlavor * aDataFlavor);
  NS_IMETHOD EmptyClipboard();
  NS_IMETHOD ForceDataToClipboard();


protected:
  PRBool              mIgnoreEmptyNotification;

  nsIClipboardOwner * mClipboardOwner;
  nsITransferable   * mTransferable;
  nsIWidget         * mWindow;

  nsDataObj         * mDataObj;

};

#endif // nsClipboard_h__
