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

#ifndef nsITreeDataModel_h___
#define nsITreeDataModel_h___

#include "prthread.h"
#include "nsString.h"
#include "nsIDataModel.h"

class nsITreeColumn;
class nsITreeDMItem;

// {728D90C2-5B2A-11d2-96ED-00104B7B7DEB}
#define NS_ITREEDATAMODEL_IID \
{ 0x728d90c2, 0x5b2a, 0x11d2, { 0x96, 0xed, 0x0, 0x10, 0x4b, 0x7b, 0x7d, 0xeb } }

class nsITreeDataModel : public nsIDataModel
{

public:
	// Column APIs
	NS_IMETHOD GetVisibleColumnCount(PRUint32& count) const = 0;
	NS_IMETHOD GetColumnCount(PRUint32& count) const = 0;
	NS_IMETHOD GetNthColumn(nsITreeColumn*& pColumn, PRUint32 n) const = 0;
	
	// TreeItem APIs
	NS_IMETHOD GetFirstVisibleItemIndex(PRUint32& index) const = 0;
	NS_IMETHOD GetNthTreeItem(nsITreeDMItem*& pItem, PRUint32 n) const = 0;
	NS_IMETHOD GetItemTextForColumn(nsString& nodeText, nsITreeDMItem* pItem, nsITreeColumn* pColumn) const = 0;
};

#endif /* nsITreeDataModel_h___ */

