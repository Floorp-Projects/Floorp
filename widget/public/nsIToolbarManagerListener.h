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

#ifndef nsIToolbarManagerListener_h___
#define nsIToolbarManagerListener_h___

#include "nsISupports.h"

class nsIToolbarManager;

// {ECF9C251-5194-11d2-8DC1-00609703C14E}
#define NS_ITOOLBARMANAGERLISTENER_IID      \
{ 0xecf9c251, 0x5194, 0x11d2, \
  { 0x8d, 0xc1, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

class nsIToolbarManagerListener : public nsISupports
{

public:
  static const nsIID& IID() { static nsIID iid = NS_ITOOLBARMANAGERLISTENER_IID; return iid; }

 /**
  * Notifies 
  *
  */
  NS_IMETHOD NotifyToolbarManagerChangedSize(nsIToolbarManager* aToolbarMgr) = 0;


};

#endif /* nsIToolbarManagerListener_h___ */

