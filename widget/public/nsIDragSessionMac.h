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

#ifndef nsIDragSessionMac_h__
#define nsIDragSessionMac_h__

#include "nsISupports.h"
#include <Drag.h>


#define NS_IDRAGSESSIONMAC_IID      \
{ 0x36c4c380, 0x09e2, 0x11d3, { 0xb0, 0x33, 0xa4, 0x20, 0xf4, 0x2c, 0xfd, 0x7c } };


class nsIDragSessionMac : public nsISupports {

  public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDRAGSESSIONMAC_IID)

  /**
    * Since the drag may originate in an external application, we need some way of
    * communicating the DragManager's DragRef to the session so it can use it
    * when filling in data requests.
    *
    * @param  aDragRef the MacOS DragManager's ref number for the current drag
    */

   NS_IMETHOD SetDragReference ( DragReference aDragRef ) = 0; 

};

#endif
