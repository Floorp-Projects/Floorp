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

#ifndef nsITreeItem_h___
#define nsITreeItem_h___

#include "nsIDMItem.h"
#include "prthread.h"
#include "nsString.h"

class nsIImage;
class nsIImageGroup;

// {2FC8FD02-789F-11d2-96ED-00104B7B7DEB}
#define NS_ITREEITEM_IID \
{ 0x2fc8fd02, 0x789f, 0x11d2, { 0x96, 0xed, 0x0, 0x10, 0x4b, 0x7b, 0x7d, 0xeb } }

class nsITreeItem : public nsIDMItem
{
public:
	// Inspectors
	NS_IMETHOD GetTriggerImage(nsIImage*& pImage, nsIImageGroup* pGroup) = 0;
	NS_IMETHOD GetIndentationLevel(int& indentation) = 0;
	
	// Setters

};

#endif /* nsITreeItem_h___ */

