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

#ifndef nsIDMItem_h___
#define nsIDMItem_h___

#include "nsISupports.h"
#include "prthread.h"
#include "nsString.h"

class nsIImage;
class nsIImageGroup;

// {F2F36B61-5EBB-11d2-96ED-00104B7B7DEB}
#define NS_IDMITEM_IID \
{ 0xf2f36b61, 0x5ebb, 0x11d2, { 0x96, 0xed, 0x0, 0x10, 0x4b, 0x7b, 0x7d, 0xeb } }

class nsIDMItem : public nsISupports
{
public:
	// Inspectors
	NS_IMETHOD GetIconImage(nsIImage*& pImage, nsIImageGroup* pGroup) = 0;
	NS_IMETHOD GetOpenState(PRBool& answer) = 0;

	// Methods for iterating over children.
	NS_IMETHOD GetChildCount(PRUint32& count) = 0;
	NS_IMETHOD GetNthChild(nsIDMItem*& pItem, PRUint32 item) = 0;

	// Parent access
	NS_IMETHOD GetParent(nsIDMItem*& pItem) = 0;

	// Setters

	// Methods to query the data model for a specific item displayed within the widget.
	NS_IMETHOD GetStringPropertyValue(nsString& value, const nsString& itemProperty) = 0;
	NS_IMETHOD GetIntPropertyValue(PRInt32& value, const nsString& itemProperty) = 0;
};

#endif /* nsIDMItem_h___ */

