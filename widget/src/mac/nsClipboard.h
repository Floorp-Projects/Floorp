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

//
// Mike Pinkerton
// Netscape Communications
//
// Native MacOS Clipboard Implementation of nsIClipboard. This contains
// the FE-specific implementations of the pure virtuals in nsBaseClipboard
//

#ifndef nsClipboard_h__
#define nsClipboard_h__

#include "nsBaseClipboard.h"

class nsITransferable;


class nsClipboard : public nsBaseClipboard
{

public:
  nsClipboard();
  virtual ~nsClipboard();

  //nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIClipboard  
  //NS_IMETHOD ForceDataToClipboard();

protected:

  // impelement the native clipboard behavior
  NS_IMETHOD SetNativeClipboardData();
  NS_IMETHOD GetNativeClipboardData(nsITransferable * aTransferable);

  static ResType MapMimeTypeToMacOSType ( const nsString & aMimeStr ) ;

}; // nsClipboard

#endif // nsClipboard_h__
