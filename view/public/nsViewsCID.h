/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsViewsCID_h__
#define nsViewsCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"

#define NS_VIEW_MANAGER_CID \
{ 0xc95f1831, 0xc376, 0x11d1, \
    {0xb7, 0x21, 0x0, 0x60, 0x8, 0x91, 0xd8, 0xc9}}

#define NS_VIEW_CID \
{ 0xc95f1832, 0xc376, 0x11d1, \
    {0xb7, 0x21, 0x0, 0x60, 0x8, 0x91, 0xd8, 0xc9}}

#define NS_SCROLLING_VIEW_CID \
{ 0xc95f1833, 0xc376, 0x11d1, \
    {0xb7, 0x21, 0x0, 0x60, 0x8, 0x91, 0xd8, 0xc9}}

#define NS_SCROLL_PORT_VIEW_CID \
{ 0x3b733c91, 0x7223, 0x11d3, \
    { 0xb3, 0x61, 0x0, 0xa0, 0xcc, 0x3c, 0x1c, 0xde }}

#endif // nsViewsCID_h__
