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

#ifndef nsIClipView_h___
#define nsIClipView_h___

#include "nsISupports.h"

// IID for the nsIClipView interface
#define NS_ICLIPVIEW_IID    \
{ 0x4cc36160, 0xd282, 0x11d2, \
{ 0x90, 0x67, 0x00, 0x60, 0xb0, 0xf1, 0x99, 0xa2 } }

/**
 * this is here so that we can query a view to see if it
 * exists for clipping.
 */
class nsIClipView : public nsISupports
{
private:
  NS_IMETHOD_(nsrefcnt) AddRef(void) = 0;
  NS_IMETHOD_(nsrefcnt) Release(void) = 0;
};

#endif
