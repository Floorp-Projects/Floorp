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

#ifndef nsTreeItem_h___
#define nsTreeItem_h___

class nsTreeColumn;
class nsIDeviceContext;
class nsIImage;

#include "nsColor.h"
#include "nsFont.h"
#include "nsTreeDataModel.h"

//------------------------------------------------------------
// A single item in a hierarchical data model for a tree widget
// with columns.
//------------------------------------------------------------

#include "nsHierarchicalDataItem.h"

class nsTreeItem : public nsHierarchicalDataItem
                  
{
protected:
    nsTreeItem() {}; // Disallow instantiation of abstract class.

public:
    virtual ~nsTreeItem() {};

	virtual void GetItemStyle(nsIDeviceContext* dc, 
							  nsTreeItemStyleInfo& styleInfo) = 0;

	virtual void GetTextForColumn(nsTreeColumn* pColumn, nsString& nodeText) const = 0;

};

#endif /* nsTreeItem_h___ */
