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

#ifndef nsIClipboardOwner_h__
#define nsIClipboardOwner_h__

#include "nsISupports.h"

class nsITransferable;

// {5A31C7A1-E122-11d2-9A57-000064657374}
#define NS_ICLIPBOARDOWNER_IID      \
{ 0x5a31c7a1, 0xe122, 0x11d2, { 0x9a, 0x57, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } };

class nsIClipboardOwner : public nsISupports {

  public:

   /**
    * Notifies the owner of the clipboard transferable that the
    * transferable is being removed from the clipboard
    *
    * @param  aTransferable The transferable
    * @result NS_Ok if no errors
    */
  
    NS_IMETHOD LosingOwnership(nsITransferable * aTransferable) = 0;

};

#endif
