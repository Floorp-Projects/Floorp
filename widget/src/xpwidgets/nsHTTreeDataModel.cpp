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
#include "nsHTColumn.h"
#include "nsHTControlStripItem.h"
#include "nsIDeviceContext.h"
#include "nsIImageObserver.h"
#include "nsIImageRequest.h"
#include "nsIImageGroup.h"
#include "nsHTTreeItem.h"
#include "nsIContent.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIImageObserverIID, NS_IIMAGEREQUESTOBSERVER_IID);

NS_IMPL_ADDREF(nsHTTreeDataModel)
NS_IMPL_RELEASE(nsHTTreeDataModel)

nsHTTreeDataModel::nsHTTreeDataModel() : nsTreeDataModel(), nsHTDataModel()
{
	NS_INIT_REFCNT();
	SetImplData((void*)(nsHTDataModel*)this);

	// Image Request Inits
	mTitleBGRequest = nsnull;
	mControlStripBGRequest = nsnull;
	mColumnHeaderBGRequest = nsnull;

	mSelectedColumnIndex = -1;

	// Hard-coded values.
	mVisibleColumnCount = 3;

    mSingleControlStripItem = new nsHTControlStripItem();
}

//--------------------------------------------------------------------
nsHTTreeDataModel::~nsHTTreeDataModel()
{
	// Delete hard-coded value
	delete mSingleControlStripItem;
}

// ISupports Implementation --------------------------------------------------------------------
nsresult nsHTTreeDataModel::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
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

// Hierarchical Tree Data Model Implementation ---------------------

void nsHTTreeDataModel::SetContentRoot(nsIContent* pContent)
{
	// Construct our visible items array
	SetContentRootDelegate(pContent);

	// Destroy our old columns list. TODO
	
	// Create our new columns list.
	// Column headers come from the root node.
	if (mRootNode)
	{
		nsHTItem* pItem = (nsHTItem*)(mRootNode->GetImplData());
		nsIContent* pColumnNode = nsHTDataModel::FindChildWithName(pItem->GetContentNode(), "columns");
		if (pColumnNode)
		{
			PRInt32 numChildren;
			pColumnNode->ChildCount(numChildren);
			for (PRInt32 i = 0; i < numChildren; i++)
			{
				nsIContent* child = nsnull;
				pColumnNode->ChildAt(i, child);
				if (child)
				{
					// We have a column to add to our array.
					nsHTColumn* pColumn = new nsHTColumn(child);
					mColumnArray.AppendElement(pColumn);
				}

				NS_IF_RELEASE(child);
			}

			// For now make only the name column visible.
			mVisibleColumnCount = 1;
			mTotalColumnCount = mColumnArray.Count();
		}
	}
}


PRUint32 nsHTTreeDataModel::GetVisibleColumnCount() const
{
	return mVisibleColumnCount;
}

PRUint32 nsHTTreeDataModel::GetColumnCount() const
{
	return mTotalColumnCount;
}

nsTreeColumn* nsHTTreeDataModel::GetNthColumn(PRUint32 n) const
{
	if (n < mTotalColumnCount)
		return (nsTreeColumn*)mColumnArray[n];
	else return nsnull;
}	

void nsHTTreeDataModel::SetVisibleColumnCount(PRUint32 columnCount) 
{
	mVisibleColumnCount = columnCount;
}

PRUint32 nsHTTreeDataModel::GetControlStripItemCount() const
{
	return 2;
}

nsTreeControlStripItem* nsHTTreeDataModel::GetNthControlStripItem(PRUint32 n) const
{
	return mSingleControlStripItem;
}

void nsHTTreeDataModel::GetControlStripCloseText(nsString& closeText) const
{
	closeText = "Close";
}

PRBool nsHTTreeDataModel::ShowTitleBar() const
{
	return PR_TRUE;
}

PRBool nsHTTreeDataModel::ShowTitleBarText() const
{
	return PR_TRUE;
}

PRBool nsHTTreeDataModel::ShowControlStrip() const
{
	return PR_TRUE;
}

PRBool nsHTTreeDataModel::ShowColumnHeaders() const
{
	return PR_TRUE;
}

void nsHTTreeDataModel::GetTitleBarStyle(nsIDeviceContext* dc, nsBasicStyleInfo& styleInfo) const
{
	// Initialize the font.
	nsString fontFamily("Haettenschweiler");
	int fontSize = 24;
	int fontWeight = 400;
	int fontStyle = NS_FONT_STYLE_NORMAL;
	int fontDecoration = NS_FONT_DECORATION_NONE;

	float t2d;
	dc->GetTwipsToDevUnits(t2d);
	nsFont theFont(fontFamily, fontStyle, NS_FONT_VARIANT_NORMAL,
						fontWeight, fontDecoration,
						nscoord(t2d * NSIntPointsToTwips(fontSize)));

	styleInfo.font = theFont;
	
	// Init the colors
	styleInfo.foregroundColor = NS_RGB(255,255,255);
	styleInfo.backgroundColor = NS_RGB(0,0,0);

	styleInfo.pBackgroundImage = nsnull; //GetTitleBGImage();
}

void nsHTTreeDataModel::GetColumnHeaderStyle(nsIDeviceContext* dc, nsColumnHeaderStyleInfo& styleInfo) const
{
	// Initialize the font.
	nsString fontFamily("Lucida Handwriting");
	int fontSize = 12;
	int fontWeight = 400;
	int fontStyle = NS_FONT_STYLE_NORMAL;
	int fontDecoration = NS_FONT_DECORATION_NONE;

	float t2d;
	dc->GetTwipsToDevUnits(t2d);
	nsFont theFont(fontFamily, fontStyle, NS_FONT_VARIANT_NORMAL,
						fontWeight, fontDecoration,
						nscoord(t2d * NSIntPointsToTwips(fontSize)));

	styleInfo.font = theFont;
	
	// Init the colors
	styleInfo.foregroundColor = NS_RGB(0,0,0);
	styleInfo.backgroundColor = NS_RGB(192,192,192);
	styleInfo.sortFGColor = NS_RGB(0,0,0);
	styleInfo.sortBGColor = NS_RGB(64,64,64);
	styleInfo.disabledColor = NS_RGB(128,128,128);

	styleInfo.pBackgroundImage = nsnull; //GetColumnHeaderBGImage();
}

void nsHTTreeDataModel::GetControlStripStyle(nsIDeviceContext* dc, nsBasicStyleInfo& styleInfo) const
{
	// Initialize the font.
	nsString fontFamily("Arial Narrow");
	int fontSize = 12;
	int fontWeight = 400;
	int fontStyle = NS_FONT_STYLE_NORMAL;
	int fontDecoration = NS_FONT_DECORATION_NONE;

	float t2d;
	dc->GetTwipsToDevUnits(t2d);
	nsFont theFont(fontFamily, fontStyle, NS_FONT_VARIANT_NORMAL,
						fontWeight, fontDecoration,
						nscoord(t2d * NSIntPointsToTwips(fontSize)));

	styleInfo.font = theFont;
	
	// Init the colors
	styleInfo.foregroundColor = NS_RGB(255,255,255);
	styleInfo.backgroundColor = NS_RGB(0,0,0);

	styleInfo.pBackgroundImage = nsnull; //GetControlStripBGImage();
}

void nsHTTreeDataModel::GetTitleBarText(nsString& text) const
{
	text = "Bookmarks";
}

// Protected Helpers
nsIImage* nsHTTreeDataModel::GetTitleBGImage() const
{
	// cast away const because we can't use mutable
	nsHTTreeDataModel* self = NS_CONST_CAST(nsHTTreeDataModel*,this);

	nsString url("http://www.shadowland.org/images/ancient_glyphs.jpg");
	if (mTitleBGRequest == nsnull)
		self->mTitleBGRequest = RequestImage(url);
	return mTitleBGRequest->GetImage();
}

nsIImage* nsHTTreeDataModel::GetControlStripBGImage() const
{
	// cast away const because we can't use mutable
	nsHTTreeDataModel* self = NS_CONST_CAST(nsHTTreeDataModel*,this);

	nsString url("http://www.shadowland.org/images/minute_bumps.jpg");
	if (mControlStripBGRequest == nsnull)
		self->mControlStripBGRequest = RequestImage(url);
	return mControlStripBGRequest->GetImage();
}

nsIImage* nsHTTreeDataModel::GetColumnHeaderBGImage() const
{
	// cast away const because we can't use mutable
	nsHTTreeDataModel* self = NS_CONST_CAST(nsHTTreeDataModel*,this);

	nsString url("http://www.shadowland.org/images/tapestry.jpg");
	if (mColumnHeaderBGRequest == nsnull)
		self->mColumnHeaderBGRequest = RequestImage(url);
	return mColumnHeaderBGRequest->GetImage();
}

nsIImageRequest* nsHTTreeDataModel::RequestImage(nsString& reqUrl) const
{
	nsIImageGroup* pGroup = GetImageGroup();

	char * url = reqUrl.ToNewCString();

	nsIImageRequest * request;
	request = pGroup->GetImage(url,
								 (nsIImageRequestObserver*)this,
								 NULL, 0, 0,  
								 0);
	delete[] url;

	return request;
}

// image request observer implementation
void nsHTTreeDataModel::Notify(nsIImageRequest *aImageRequest,
                          nsIImage *aImage,
                          nsImageNotification aNotificationType,
                          PRInt32 aParam1, PRInt32 aParam2,
                          void *aParam3)
{
  if (aNotificationType == nsImageNotification_kImageComplete)
  {
	  // Notify the data source that we loaded.  It can then inform the data source listener
	  // regarding what happened.
	  ImageLoaded(nsnull);
  }
}

void nsHTTreeDataModel::NotifyError(nsIImageRequest *aImageRequest,
                               nsImageError aErrorType)
{
}

// Inherited functions from nsHTDataModel
nsHierarchicalDataItem* nsHTTreeDataModel::CreateDataItemWithContentNode(nsIContent* pContent)
{
	nsHTTreeItem* pItem = new nsHTTreeItem(pContent, this);

	// Register our item as an event listener on the DOM node.
	return pItem;
}
