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
#include "nsHierarchicalDataItem.h"
#include "nsHTDataModel.h"
#include "nsHTItem.h"
#include "nsIContent.h"
#include "nsVoidArray.h"
#include "nsINameSpaceManager.h"

static nsIAtom* kOpenAtom;
static nsIAtom* kSelectedAtom;

nsHTItem::nsHTItem(nsIContent* pContent, nsHierarchicalDataModel* pDataModel)
{
	NS_ADDREF(pContent);
    mContentNode = pContent;
	mDataModel = pDataModel;
	mIndentationLevel = 0;

  if (nsnull == kOpenAtom) {
    kOpenAtom = NS_NewAtom("OPEN");
    kSelectedAtom = NS_NewAtom("OPEN");
  }
  else {
    NS_ADDREF(kOpenAtom);
    NS_ADDREF(kSelectedAtom);
  }
}

//--------------------------------------------------------------------
nsHTItem::~nsHTItem()
{
	NS_IF_RELEASE(mContentNode);

  nsrefcnt refCnt;
  NS_RELEASE2(kOpenAtom, refCnt);
  NS_RELEASE2(kSelectedAtom, refCnt);
}

PRBool nsHTItem::IsExpandedDelegate() const
{
	nsString attrValue;
	nsresult result = mContentNode->GetAttribute(kNameSpaceID_None, kOpenAtom, attrValue);
    attrValue.ToLowerCase();
	return (result == NS_CONTENT_ATTR_NO_VALUE ||
	        (result == NS_CONTENT_ATTR_HAS_VALUE && attrValue=="true"));	
}

void nsHTItem::ToggleOpenStateDelegate() 
{
	nsString attrValue;
	// Need to set the attribute's value.
	if (IsExpandedDelegate())
		attrValue = "false";
	else attrValue = "true";

	// Set it and wait for the callback.
	mContentNode->SetAttribute(kNameSpaceID_None, kOpenAtom, attrValue, PR_TRUE);

	// TODO: Remove this and put it in the callback instead.
	FinishToggle();
}

void nsHTItem::FinishToggle()
{
	nsHTDataModel* pDataModel = NS_STATIC_CAST(nsHTDataModel*, mDataModel->GetImplData());
	nsVoidArray* pArray = pDataModel->GetVisibilityArray();

	nsHierarchicalDataItem* self = GetDataItem();
	PRInt32 index = pArray->IndexOf((void*)self) + 1;
	
	PRUint32 childCount = self->GetChildCount(); 

	if (IsExpandedDelegate())
	{
		// We just opened the node. Add the children to our visibility array.
		for (PRUint32 i = 0; i < childCount; i++)
		{
			nsIContent* child = GetNthChildContentNode(i);
			nsHierarchicalDataItem* pDataItem = pDataModel->CreateDataItemWithContentNode(child);
			pDataItem->SetIndentationLevel(mIndentationLevel + 1);
			pArray->InsertElementAt((void*)pDataItem, index);
			index++;

			NS_IF_RELEASE(child);
		}
	}
	else
	{
		// We just closed the node. Remove the children from our visibility array.
		for (PRUint32 i = 0; i < childCount; i++)
		{
			nsHierarchicalDataItem* pItem = (nsHierarchicalDataItem*)((*pArray)[index]);
			pArray->RemoveElementAt(index);
			delete pItem;
		}
	}
}

PRUint32 nsHTItem::GetChildCountDelegate() const
{
	PRInt32 childCount;
	mContentNode->ChildCount(childCount);

	return childCount;
}

nsIContent* nsHTItem::GetNthChildContentNode(PRUint32 n) const
{
	nsIContent* childNode;
	mContentNode->ChildAt(n, childNode);
	
	return childNode;
}

PRBool nsHTItem::IsSelectedDelegate() const
{
	nsString attrValue;
	nsresult result = mContentNode->GetAttribute(kNameSpaceID_None, kSelectedAtom, attrValue);
    attrValue.ToLowerCase();
	return (result == NS_CONTENT_ATTR_NO_VALUE ||
	        (result == NS_CONTENT_ATTR_HAS_VALUE && attrValue=="true"));	
}

void nsHTItem::FinishSelectionChange()
{
	nsHTDataModel* pDataModel = NS_STATIC_CAST(nsHTDataModel*, mDataModel->GetImplData());
	nsVoidArray* pArray = pDataModel->GetSelectionArray();

	nsHierarchicalDataItem* self = GetDataItem();
	PRInt32 index = pArray->IndexOf((void*)self);
	
	if (IsSelectedDelegate() && index == -1)
	{
		// The item is not in the array and needs to be added.
		pArray->AppendElement(self);
	}
	else if (!IsSelectedDelegate() && index != -1)
	{
		pArray->RemoveElementAt(index);
	}
}

PRUint32 nsHTItem::GetIndentationLevelDelegate() const
{
	return mIndentationLevel;
}

void nsHTItem::SetIndentationLevelDelegate(PRUint32 n)
{
	mIndentationLevel = n;
}
