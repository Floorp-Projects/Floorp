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

#include "nspr.h"
#include "nsString.h"
#include "nsFont.h"
#include "nsHTTreeDataModel.h"
#include "nsWidgetsCID.h"
#include "nsDataModelWidget.h"
#include "nsTreeColumn.h"
#include "nsHTTreeItem.h"
#include "nsIDeviceContext.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsVoidArray.h"
#include "nsIDOMNode.h"
#include "nsINameSpaceManager.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsHTDataModel::nsHTDataModel() 
{
	mListener = nsnull;
	mImageGroup = nsnull;
	mContentRoot = nsnull;
	mDocument = nsnull;
}

//--------------------------------------------------------------------
nsHTDataModel::~nsHTDataModel()
{
	if (mImageGroup)
	{
		mImageGroup->Interrupt();
		NS_RELEASE(mImageGroup);
	}

	// TODO: Destroy visibility array
}

void nsHTDataModel::SetDataModelListenerDelegate(nsDataModelWidget* pWidget)
{
	NS_IF_RELEASE(mImageGroup);
	mListener = pWidget;
	if (pWidget != nsnull && NS_NewImageGroup(&mImageGroup) == NS_OK) 
	{
		nsIDeviceContext * deviceCtx = pWidget->GetDeviceContext();
		mImageGroup->Init(deviceCtx, nsnull);
		NS_RELEASE(deviceCtx);
	}
}

// Hierarchical Data Model Implementation ---------------------

void nsHTDataModel::SetContentRootDelegate(nsIContent* pContent)
{
	NS_IF_RELEASE(mDocument);
	NS_IF_RELEASE(mContentRoot);

	mContentRoot = pContent;
	pContent->GetDocument(mDocument); // I'm assuming this addrefs the document.
	NS_ADDREF(mContentRoot);

	// Destroy our old visibility list.
	// TODO

	// Reconstruct our visibility list (so that all items that are visible 
	// are instantiated).  Need to only look for folder and item children.  All other children should be ignored.
	AddNodesToArray(mContentRoot, 0);
}

void nsHTDataModel::AddNodesToArray(nsIContent* pContent, PRUint32 indentLevel)
{
	// Add this child to the array (unless it is the root node).
	nsHierarchicalDataItem* pDataItem = CreateDataItemWithContentNode(pContent);
	if (pContent != mContentRoot)
	{
		// Add to our array
		mVisibleItemArray.AppendElement(pDataItem);
		// Set the correct indent level for the item.
		pDataItem->SetIndentationLevel(indentLevel);
		indentLevel++;
	}
	else mRootNode = pDataItem;

	if (pContent == mContentRoot || pDataItem->IsExpanded())
	{
		nsHTItem* pItem = NS_STATIC_CAST(nsHTItem*, pDataItem->GetImplData());
		nsIContent* pChildrenNode = nsHTDataModel::FindChildWithName(pItem->GetContentNode(), "children");
		if (pChildrenNode)
		{
			PRInt32 numChildren = 0;
			pChildrenNode->ChildCount(numChildren);
			for (PRInt32 i = 0; i < numChildren; i++)
			{
				nsIContent* child = nsnull;
				pChildrenNode->ChildAt(i, child);
				if (child)
				{
					AddNodesToArray(child, indentLevel);
				}

				NS_IF_RELEASE(child);
			}
		}
	}
}

nsHierarchicalDataItem* nsHTDataModel::GetRootDelegate() const
{
	return mRootNode;
}


PRUint32 nsHTDataModel::GetFirstVisibleItemIndexDelegate() const
{
	return 0;
}

void nsHTDataModel::SetFirstVisibleItemIndexDelegate(PRUint32 n)
{
}

nsHierarchicalDataItem* nsHTDataModel::GetNthItemDelegate(PRUint32 n) const
{
	PRUint32 itemCount = (PRUint32)(mVisibleItemArray.Count());

	if (n < itemCount)
		return (nsHierarchicalDataItem*)(mVisibleItemArray[n]);
	else return nsnull;
}


void nsHTDataModel::SetSelectionDelegate(nsHierarchicalDataItem* pDataItem)
{
	ClearSelectionDelegate(); // First clear our selection.
	
	ToggleSelectionDelegate(pDataItem); // Add this node to the selection.
}
	
void nsHTDataModel::ToggleSelectionDelegate(nsHierarchicalDataItem* pDataItem)
{
	nsString attrValue;
	// Need to set the attribute's value.
	if (pDataItem->IsSelected())
		attrValue = "false";
	else attrValue = "true";

	// Set it and wait for the callback.
	nsHTItem* pItem = NS_STATIC_CAST(nsHTItem*, pDataItem->GetImplData());
  nsIAtom* selectedAtom = NS_NewAtom("selected");
	pItem->GetContentNode()->SetAttribute(kNameSpaceID_None, selectedAtom, attrValue, PR_TRUE);
  NS_RELEASE(selectedAtom);

	// TODO: Remove this and put it in the callback instead.
	pItem->FinishSelectionChange();
}

void nsHTDataModel::RangedSelectionDelegate(PRUint32 n, PRUint32 count)
{
}

void nsHTDataModel::ClearSelectionDelegate()
{
	// Iterate over our array and clear the selection.
	PRInt32 count = mSelectedItemArray.Count();
	for (PRInt32 i = 0; i < count; i++)
	{
		nsHierarchicalDataItem* pItem = (nsHierarchicalDataItem*)mSelectedItemArray[0];
		ToggleSelectionDelegate(pItem);
	}
}

void nsHTDataModel::ImageLoaded(nsHierarchicalDataItem* pItem)
{
	if (mListener)
	{
		// Send it on along. Let the view know what happened.
		mListener->HandleDataModelEvent(cDMImageLoaded, pItem);
	}
}

// Static Helper functions
void nsHTDataModel::GetChildTextForNode(nsIContent* pChildNode, nsString& text)
{
	nsIContent* pChild;
	pChildNode->ChildAt(0, pChild);
	nsIDOMNode* pTextChild = nsnull;

static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);

	if (NS_SUCCEEDED(pChild->QueryInterface(kIDOMNodeIID, (void**)&pTextChild)))
	{
		pTextChild->GetNodeValue(text);
		NS_IF_RELEASE(pTextChild);
	}
	else text = "null";

	NS_IF_RELEASE(pChild);
}

nsIContent* nsHTDataModel::FindChildWithName(nsIContent* pNode, const nsString& name) 
{
	PRInt32 count;
	pNode->ChildCount(count);
	for (PRInt32 i = 0; i < count; i++)
	{
		nsIAtom* pAtom = nsnull;
		nsIContent* pChild = nsnull;
		pNode->ChildAt(i, pChild);
		pChild->GetTag(pAtom);
		nsString answer;
		pAtom->ToString(answer);
		NS_IF_RELEASE(pAtom);
		if (answer.EqualsIgnoreCase(name))
			return pChild;
		else NS_IF_RELEASE(pChild);
	}

	return nsnull;
}
