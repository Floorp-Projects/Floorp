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

#ifndef nsShellCIID_h__
#define nsShellCIID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsRepository.h"

#define NS_SHELLINSTANCE_CID      \
 { 0x90487580, 0xfefe, 0x11d1, \
   {0xbe, 0xcd, 0x00, 0x80, 0x5f, 0x8a, 0x8d, 0xbd} }

// 74222650-2d76-11d2-9246-00805f8a7ab6
#define NS_MENUBAR_CID      \
 { 0x74222650, 0x2d76, 0x11d2, \
   {0x92, 0x46, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6} }

// 7a01c6d0-2d76-11d2-9246-00805f8a7ab6
#define NS_MENUITEM_CID      \
 { 0x7a01c6d0, 0x2d76, 0x11d2, \
   {0x92, 0x46, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6} }

#define NS_IAPPLICATIONSHELL_CID      \
 { 0x2293d960, 0xdeff, 0x11d1, \
   {0x92, 0x44, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6} }

#endif
