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
	AddNodesToArray(mContentRoot);
}

void nsHTDataModel::AddNodesToArray(nsIContent* pContent)
{
	// Add this child to the array (unless it is the root node).
	nsHierarchicalDataItem* pDataItem = CreateDataItemWithContentNode(pContent);
	if (pContent != mContentRoot)
		mVisibleItemArray.AppendElement(pDataItem);
	else mRootNode = pDataItem;

	nsHTItem* pItem = (nsHTItem*)(pDataItem->GetImplData());

	nsIContent* pChildrenNode = pItem->FindChildWithName("children");
	if (pChildrenNode)
	{
		// If the node is OPEN, then its children need to be added to the visibility array.
		nsString attrValue;
		nsresult result = pContent->GetAttribute("open", attrValue);
		if ((pContent == mContentRoot) || (result == NS_CONTENT_ATTR_NO_VALUE ||
			(result == NS_CONTENT_ATTR_HAS_VALUE && attrValue=="true")))
		{
			PRInt32 numChildren = 0;
			pChildrenNode->ChildCount(numChildren);
			for (PRInt32 i = 0; i < numChildren; i++)
			{
				nsIContent* child = nsnull;
				pContent->ChildAt(i, child);
				if (child)
				{
					AddNodesToArray(child);
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

void nsHTDataModel::ImageLoaded(nsHierarchicalDataItem* pItem)
{
	if (mListener)
	{
		// Send it on along. Let the view know what happened.
		mListener->HandleDataModelEvent(cDMImageLoaded, pItem);
	}
}