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

#ifndef nsIImageButtonListener_h___
#define nsIImageButtonListener_h___

#include "nsISupports.h"
#include "nsGUIEvent.h"

class nsIImageButton;

// {D3C3B8B2-55B5-11d2-9A2A-000000000000}
#define NS_IIMAGEBUTTONLISTENER_IID      \
{ 0xd3c3b8b2, 0x55b5, 0x11d2, \
  { 0x9a, 0x2a, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }

class nsIImageButtonListener : public nsISupports
{

public:

 /**
  * Notifies 
  *
  */
  NS_IMETHOD NotifyImageButtonEvent(nsIImageButton * aImgBtn, nsGUIEvent* anEvent) = 0;


};

#endif /* nsIImageButtonListener_h___ */

