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

#ifndef nsIXPItem_h___
#define nsIXPItem_h___

#include "nsISupports.h"

//b9a92c40-4a95-11d2-bee2-00805f8a8dbd
#define NS_IXP_ITEM_IID      \
 { 0xb9a92c40, 0x4a95, 0x11d2, \
   {0xbe, 0xe2, 0x00, 0x80, 0x5f, 0x8a, 0x8d, 0xbd} }

class nsIXPItem : public nsISupports
{

public:

  NS_IMETHOD Init() = 0;
};

#endif /* nsIXPItem_h___ */

