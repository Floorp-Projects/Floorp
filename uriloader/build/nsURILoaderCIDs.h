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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsURILoaderCID_h__
#define nsURILoaderCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

#define NS_URIDISPATCHER_CID				              \
{ /* EBBBBFE1-8BE8-11d3-989D-001083010E9B */      \
 0xebbbbfe1, 0x8be8, 0x11d3,                      \
 {0x98, 0x9d, 0x0, 0x10, 0x83, 0x1, 0xe, 0x9b}}

#define NS_URIDISPATCHER_PROGID \
  "component://netscape/network/uri-dispatcher-service"


#endif // nsURILoaderCID_h__
