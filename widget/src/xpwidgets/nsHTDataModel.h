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

#ifndef nsHTDataModel_h___
#define nsHTDataModel_h___

#include "nsWindow.h"
#include "nsHierarchicalDataModel.h"
#include "nsIImageGroup.h"

class nsDataModelWidget;
class nsHTItem;
class nsIDocument;
class nsIContent;

//------------------------------------------------------------
// The HyperTree Base Class
// This class is a simplified API that wraps calls into the DOM
// interfaces. It is shared by the HT Tree Widget and the HT
// Toolbar.
// -----------------------------------------------------------

class nsHTDataModel
                  
{
public:
    nsHTDataModel();
    virtual ~nsHTDataModel();

	// Set the content root
	void SetContentRootDelegate(nsIContent* pContent);

	// Retrieve the root node of the data model.
	virtual nsHierarchicalDataItem* GetRootDelegate() const;

	// A visibility hint can be stored and retrieved (e.g., the leftmost or topmost
	// item in the current scrolled view).
	virtual PRUint32 GetFirstVisibleItemIndexDelegate() const;
	virtual void SetFirstVisibleItemIndexDelegate(PRUint32 index);
	virtual nsHierarchicalDataItem* GetNthItemDelegate(PRUint32 n) const;

	virtual void SetDataModelListenerDelegate(nsDataModelWidget* pListener);

	// Selection APIs
	virtual void SetSelectionDelegate(nsHierarchicalDataItem* pDataItem);
		// Sets the selection to be only the node passed in. All other nodes are deselected
		// prior to the selection of the new node.
	virtual void ToggleSelectionDelegate(nsHierarchicalDataItem* pDataItem);
		// Toggles the selection on the item specified without affecting other selected nodes.
	virtual void RangedSelectionDelegate(PRUint32 n, PRUint32 count);
		// Selects the number of items specified by count, starting from position n.
	virtual void ClearSelectionDelegate();
		// Deselects all items.

public:
	virtual nsHierarchicalDataItem* CreateDataItemWithContentNode(nsIContent* pContent) = 0;

	static void GetChildTextForNode(nsIContent* pChildNode, nsString& text);
	static nsIContent* FindChildWithName(nsIContent* pNode, const nsString& name);

	void ImageLoaded(nsHierarchicalDataItem* pItem);
	nsIImageGroup* GetImageGroup() const { NS_ADDREF(mImageGroup); return mImageGroup; }

public:
	nsVoidArray* GetVisibilityArray() { return &mVisibleItemArray; };
	nsVoidArray* GetSelectionArray() { return &mSelectedItemArray; };

protected:
	void AddNodesToArray(nsIContent* pContent, PRUint32 indentLevel); 
		// This recursive function is called to add nodes to the visibility array.

	enum { cDMImageLoaded = 0 } ;

	nsDataModelWidget* mListener; // Events are sent to the listening widget.
	nsIImageGroup* mImageGroup; // Image group used for loading all images in the model.

protected:
	// The document being observed (and the content node that serves as the root for the
	// widget attached to the model).
	nsIDocument* mDocument;
	nsIContent* mContentRoot;
	
	nsHierarchicalDataItem* mRootNode;
	nsVoidArray mVisibleItemArray;  // A flat view of the hierarchy, i.e., a list of all currently exposed items.
	nsVoidArray mSelectedItemArray;  // A list of all nodes in the hierarchy that are currently selected.
};

#endif /* nsToolbar_h___ */
