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
#include "nsHTTreeItem.h"
#include "nsHTTreeDataModel.h"
#include "nsWidgetsCID.h"
#include "nsRepository.h"
#include "nsIImageObserver.h"
#include "nsIImageRequest.h"
#include "nsIImageGroup.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIImageObserverIID, NS_IIMAGEREQUESTOBSERVER_IID);

NS_IMPL_ADDREF(nsHTTreeItem)
NS_IMPL_RELEASE(nsHTTreeItem)

nsHTTreeItem::nsHTTreeItem() : nsTreeItem(), nsHTItem()
{
  NS_INIT_REFCNT();
  mClosedIconRequest = nsnull;
  mOpenIconRequest = nsnull;
  mClosedTriggerRequest = nsnull;
  mOpenTriggerRequest = nsnull;
  mBackgroundRequest = nsnull;
}

//--------------------------------------------------------------------
nsHTTreeItem::~nsHTTreeItem()
{
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
	
	if (styleInfo.showTrigger)
		styleInfo.pTriggerImage = GetTriggerImage();
	
	if (styleInfo.showIcon)
		styleInfo.pIconImage = GetIconImage();

	styleInfo.pBackgroundImage = GetBackgroundImage();
}

nsIImage* nsHTTreeItem::GetTriggerImage() const
{
	// TODO: Really read in these properties
	nsString openTriggerURL("file:///c|/Program%20Files/SL/CLIENT/IMAGES/overlay.gif");
	nsString closedTriggerURL("file:///c|/Program%20Files/SL/CLIENT/IMAGES/overlay.gif");
	
	if (IsExpanded())
	{
		if (mOpenTriggerRequest == nsnull)
		{
			// Request the image.
			mOpenTriggerRequest = RequestImage(openTriggerURL);
		}
		return mOpenTriggerRequest->GetImage();
	}
	else
	{
		if (mClosedTriggerRequest == nsnull)
			mClosedTriggerRequest = RequestImage(closedTriggerURL);
		return mClosedTriggerRequest->GetImage();
	}
}

nsIImage* nsHTTreeItem::GetIconImage() const
{
	nsString openIconURL("file:///c|/Program%20Files/SL/CLIENT/IMAGES/OpenRead.gif");
	nsString closedIconURL("file:///c|/Program%20Files/SL/CLIENT/IMAGES/ClosedRead.gif");
	
	if (IsExpanded())
	{
		if (mOpenIconRequest == nsnull)
			mOpenIconRequest = RequestImage(openIconURL);
		return mOpenIconRequest->GetImage();
	}
	else
	{
		if (mClosedIconRequest == nsnull)
			mClosedIconRequest = RequestImage(closedIconURL);
		return mClosedIconRequest->GetImage();
	}
}

nsIImage* nsHTTreeItem::GetBackgroundImage() const
{
	nsString bgURL("http://www.shadowland.org/images/chalk.jpg");
	if (mBackgroundRequest == nsnull)
		mBackgroundRequest = RequestImage(bgURL);
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
	delete url;

	return request;
}

void nsHTTreeItem::GetTextForColumn(nsTreeColumn* pColumn, nsString& nodeText) const
{
	nodeText = "Node Stuff";
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