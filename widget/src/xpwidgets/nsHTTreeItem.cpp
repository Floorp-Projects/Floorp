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

#include "nsRect.h"
#include "nspr.h"
#include "nsString.h"
#include "nsHTColumn.h"
#include "nsHTTreeItem.h"
#include "nsHTTreeDataModel.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsIImageObserver.h"
#include "nsIImageRequest.h"
#include "nsIImageGroup.h"
#include "nsIContent.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIImageObserverIID, NS_IIMAGEREQUESTOBSERVER_IID);

NS_IMPL_ADDREF(nsHTTreeItem)
NS_IMPL_RELEASE(nsHTTreeItem)

nsHTTreeItem::nsHTTreeItem(nsIContent* pContent, nsHierarchicalDataModel* pModel) 
: nsTreeItem(), nsHTItem(pContent, pModel)
{
  NS_INIT_REFCNT();
  mClosedIconRequest = nsnull;
  mOpenIconRequest = nsnull;
  mClosedTriggerRequest = nsnull;
  mOpenTriggerRequest = nsnull;
  mBackgroundRequest = nsnull;

  SetImplData((void*)(nsHTItem*)this);
}

//--------------------------------------------------------------------
nsHTTreeItem::~nsHTTreeItem()
{
  NS_IF_RELEASE(mContentNode);

  // Empty out our rectangle array
  PRInt32 count = mColumnRectArray.Count();
  for (PRInt32 i = 0; i < count; i++)
  {
	  nsRect* rect = (nsRect*)mColumnRectArray[i];
	  delete rect;
  }

  mColumnRectArray.Clear();
}

// ISupports Implementation --------------------------------------------------------------------
nsresult nsHTTreeItem::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                          
    *aInstancePtr = (void*) (nsISupports *)(nsIImageRequestObserver*)this;                                        
    AddRef();                                                           
    return NS_OK;                                                        
  } 
  if (aIID.Equals(kIImageObserverIID)) {
    *aInstancePtr = (void*)(nsIImageRequestObserver*)this;
    AddRef();
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}

// TreeItem Implementation ---------------------

void nsHTTreeItem::GetItemStyle(nsIDeviceContext* dc, nsTreeItemStyleInfo& styleInfo) const
{
	styleInfo.foregroundColor = NS_RGB(0,0,0);
	styleInfo.backgroundColor = NS_RGB(240,240,240);
	styleInfo.showTrigger = PR_TRUE;
	styleInfo.showIcon = PR_TRUE;
	styleInfo.leftJustifyTrigger = PR_FALSE;
	
	styleInfo.rolloverFGColor = NS_RGB(0,0,255);

	styleInfo.selectionBGColor = NS_RGB(0,0,128);
	styleInfo.selectionFGColor = NS_RGB(255,255,255);

	styleInfo.showHorizontalDivider = PR_TRUE;
	styleInfo.showVerticalDivider = PR_TRUE;
	styleInfo.horizontalDividerColor = NS_RGB(255,255,255);
	styleInfo.verticalDividerColor = NS_RGB(255,255,255);

	if (styleInfo.showTrigger)
		styleInfo.pTriggerImage = GetTriggerImage();
	
	if (styleInfo.showIcon)
		styleInfo.pIconImage = GetIconImage();

	styleInfo.pBackgroundImage = nsnull; //GetBackgroundImage();
}

nsIImage* nsHTTreeItem::GetTriggerImage() const
{
	// cast away const because we can't use mutable
	nsHTTreeItem* self = NS_CONST_CAST(nsHTTreeItem*,this);

	// TODO: Really read in these properties
	nsString openTriggerURL("http://www.shadowland.org/client/images/overlay.gif");
	nsString closedTriggerURL("http://www.shadowland.org/client/images/overlay.gif");
	
	if (IsExpanded())
	{
		if (mOpenTriggerRequest == nsnull)
		{
			// Request the image.
			self->mOpenTriggerRequest = RequestImage(openTriggerURL);
		}
		return mOpenTriggerRequest->GetImage();
	}
	else
	{
		if (mClosedTriggerRequest == nsnull)
			self->mClosedTriggerRequest = RequestImage(closedTriggerURL);
		return mClosedTriggerRequest->GetImage();
	}
}

nsIImage* nsHTTreeItem::GetIconImage() const
{
	// cast away const because we can't use mutable
	nsHTTreeItem* self = NS_CONST_CAST(nsHTTreeItem*,this);

	nsString openIconURL("http://www.shadowland.org/CLIENT/IMAGES/OpenRead.gif");
	nsString closedIconURL("http://www.shadowland.org/CLIENT/IMAGES/ClosedRead.gif");
	
	if (IsExpanded())
	{
		if (mOpenIconRequest == nsnull)
			self->mOpenIconRequest = RequestImage(openIconURL);
		return mOpenIconRequest->GetImage();
	}
	else
	{
		if (mClosedIconRequest == nsnull)
			self->mClosedIconRequest = RequestImage(closedIconURL);
		return mClosedIconRequest->GetImage();
	}
}

nsIImage* nsHTTreeItem::GetBackgroundImage() const
{
	// cast away const because we can't use mutable
	nsHTTreeItem* self = NS_CONST_CAST(nsHTTreeItem*,this);

	nsString bgURL("http://www.shadowland.org/images/chalk.jpg");
	if (mBackgroundRequest == nsnull)
		self->mBackgroundRequest = RequestImage(bgURL);
	return mBackgroundRequest->GetImage();
}

nsIImageRequest* nsHTTreeItem::RequestImage(nsString& reqUrl) const
{
	nsHTTreeDataModel* pDataModel = (nsHTTreeDataModel*)(mDataModel);
	nsIImageGroup* pGroup = pDataModel->GetImageGroup();

	char * url = reqUrl.ToNewCString();

	nsIImageRequest * request;
	request = pGroup->GetImage(url,
								 (nsIImageRequestObserver*)this,
								 NULL, 0, 0,  
								 0);
	delete[] url;

	return request;
}

void nsHTTreeItem::GetTextForColumn(nsTreeColumn* pColumn, nsString& nodeText) const
{
	nsString columnName;
	pColumn->GetColumnName(columnName);

	// Look for a child of the content node that has this name as its tag.
	nsIContent* pColumnNode = nsHTDataModel::FindChildWithName(mContentNode, "columns");
	if (pColumnNode) 
	{
		nsIContent* pChildNode = nsHTDataModel::FindChildWithName(pColumnNode, columnName);
		if (pChildNode)
			nsHTDataModel::GetChildTextForNode(pChildNode, nodeText);
	}
}

// image request observer implementation
void nsHTTreeItem::Notify(nsIImageRequest *aImageRequest,
                          nsIImage *aImage,
                          nsImageNotification aNotificationType,
                          PRInt32 aParam1, PRInt32 aParam2,
                          void *aParam3)
{
  if (aNotificationType == nsImageNotification_kImageComplete)
  {
	  // Notify the data source that we loaded.  It can then inform the data source listener
	  // regarding what happened.
	  nsHTTreeDataModel* pDataModel = (nsHTTreeDataModel*)(mDataModel);	
	  if (pDataModel)
		  pDataModel->ImageLoaded(this);
  }
}

void nsHTTreeItem::NotifyError(nsIImageRequest *aImageRequest,
                               nsImageError aErrorType)
{
}


PRUint32 nsHTTreeItem::GetChildCount() const
{
	nsIContent* pChildrenNode = nsHTDataModel::FindChildWithName(mContentNode, "children");
	PRInt32 childCount = 0;
	if (pChildrenNode)
		pChildrenNode->ChildCount(childCount);

	return childCount;
}

nsIContent* nsHTTreeItem::GetNthChildContentNode(PRUint32 n) const
{
	nsIContent* pGrandchild = nsnull;
	nsIContent* pChildrenNode = nsHTDataModel::FindChildWithName(mContentNode, "children");
	if (pChildrenNode)
	{
		pChildrenNode->ChildAt(n, pGrandchild);
	}
	return pGrandchild;
}

void nsHTTreeItem::SetContentRectangle(const nsRect& rect, PRUint32 n)
{
	PRInt32 count = mColumnRectArray.Count();
	if (count == (PRInt32)n)
	  mColumnRectArray.AppendElement(new nsRect(rect));
	else
	{
		nsRect* existingRect = (nsRect*)(mColumnRectArray[n]);
		*existingRect = rect;
	}
}
