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

#ifndef nsHierarchicalDataModel_h___
#define nsHierarchicalDataModel_h___

#include "nsFont.h"
#include "nsColor.h"

class nsHierarchicalDataItem;
class nsDataModelWidget;
class nsIContent;
class nsIImage;

// Style info helper struct shared by most widgets.
struct nsBasicStyleInfo
{
	nsFont font;
	nscolor foregroundColor;
	nscolor backgroundColor;
	nsIImage* pBackgroundImage;   //*** com_auto_ptr?

	nsIImage* BackgroundImage ( ) const { return pBackgroundImage; }
	
	nsBasicStyleInfo(const nsFont& aFont)
		:font(aFont), pBackgroundImage(nsnull) { }

};

// -----------------------------------------------------------------
// An abstract API for communication with a hierarchical store of
// information. Iteration over children in the model is provided.
// The model also provides a flattened view of the tree (a list
// of visible nodes).
//------------------------------------------------------------

class nsHierarchicalDataModel
                  
{
protected:
    nsHierarchicalDataModel() {}; // Disallow instantiation of abstract class.

public:
    virtual ~nsHierarchicalDataModel() {};

	// Retrieve the root node of the data model.
	// Setting the Content Root for the tree
	virtual void SetContentRoot(nsIContent* pContent) = 0;
	virtual nsHierarchicalDataItem* GetRoot() const = 0;

	// A visibility hint can be stored and retrieved (e.g., the leftmost or topmost
	// item in the current scrolled view).
	virtual PRUint32 GetFirstVisibleItemIndex() const = 0;
	virtual void SetFirstVisibleItemIndex(PRUint32 index) = 0;
	virtual nsHierarchicalDataItem* GetNthItem(PRUint32 n) const = 0;

	virtual void SetDataModelListener(nsDataModelWidget* pListener) = 0;
};

#endif /* nsHierarchicalDataModel_h___ */
