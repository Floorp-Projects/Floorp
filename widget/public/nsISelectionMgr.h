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

#ifndef nsISelectionMgr_h__
#define nsISelectionMgr_h__

#include "nsISupports.h"

// {a6cf90ea-15b3-11d2-932e-00805f8add32}
#define NS_ISELECTIONMGR_IID \
{ 0xa6cf90ea, 0x15b3, 0x11d2, \
{ 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }

// We'd like to declare this as a forwarded class,
// but for some reason the Mac won't accept that here.
#include <iostream.h>

class nsString;

/**
 * Selection Manager interface.
 * Owns the copied text, listens for selection request events.
 */

class nsISelectionMgr : public nsISupports
{
public:
  static const nsIID& GetIID()
    { static nsIID iid = NS_ISELECTIONMGR_IID; return iid; }
    
  NS_IMETHOD GetCopyOStream(ostream** aStream) = 0;

  NS_IMETHOD CopyToClipboard() = 0;

  NS_IMETHOD PasteTextBlocking(nsString* aPastedText) = 0;
};

#endif // nsISelectionMgr_h__
