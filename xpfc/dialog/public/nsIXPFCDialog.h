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

#ifndef nsIXPFCDialog_h___
#define nsIXPFCDialog_h___

#include "nsISupports.h"
#include "nsString.h"

//52ae6d80-3b81-11d2-bee0-00805f8a8dbd
#define NS_IXPFC_DIALOG_IID      \
 { 0x52ae6d80, 0x3b81, 0x11d2, \
   {0xbe, 0xe0, 0x00, 0x80, 0x5f, 0x8a, 0x8d, 0xbd} }

class nsIXPFCDialog : public nsISupports
{

public:

  NS_IMETHOD Init() = 0;
};

#endif /* nsIXPFCDialog_h___ */

