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
 
#include "nsHTToolbarDataModel.h"

#include "nspr.h"
#include "nsString.h"
#include "nsFont.h"
#include "nsWidgetsCID.h"
#include "nsDataModelWidget.h"
#include "nsIDeviceContext.h"
#include "nsIImageObserver.h"
#include "nsIImageRequest.h"
#include "nsIImageGroup.h"
#include "nsIContent.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIImageObserverIID, NS_IIMAGEREQUESTOBSERVER_IID);

NS_IMPL_ADDREF(nsHTToolbarDataModel)
NS_IMPL_RELEASE(nsHTToolbarDataModel)

nsHTToolbarDataModel::nsHTToolbarDataModel() : nsToolbarDataModel(), nsHTDataModel(),
	mBGRequest(nsnull)
{
	NS_INIT_REFCNT();

}

//--------------------------------------------------------------------
nsHTToolbarDataModel::~nsHTToolbarDataModel()
{
	NS_IF_RELEASE(mBGRequest);
}


// ISupports Implementation --------------------------------------------------------------------
nsresult nsHTToolbarDataModel::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
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

void 
nsHTToolbarDataModel::SetContentRoot(nsIContent* pContent)
{
	// Construct our visible items array
	SetContentRootDelegate(pContent);
}


//
// GetToolbarStyle
//
// Loads up all the style information from the DOM/CSS.
// NOTE: right now this is just hard-coded stuff. THis may all go away when the toolbar
//        is integrated with the frame system.
//
void
nsHTToolbarDataModel::GetToolbarStyle ( nsIDeviceContext* dc, nsBasicStyleInfo& styleInfo ) const
{
	// Initialize the font.
	nsString fontFamily("Helvetica");
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

	styleInfo.pBackgroundImage = GetBGImage();
}


//
// GetBGImage
//
// Fetch the bg image of this toolbar
//
nsIImage* 
nsHTToolbarDataModel :: GetBGImage() const
{
	nsString url("http://www.shadowland.org/images/ancient_glyphs.jpg");

	if (mBGRequest == nsnull) {
		// cast away const because we can't use mutable
		nsHTToolbarDataModel* self = NS_CONST_CAST(nsHTToolbarDataModel*, this);
		self->mBGRequest = RequestImage(url);
	}
	return mBGRequest->GetImage();
	
} // GetBGImage


//
// RequestImage
//
// Actually kick off the image request.
//
nsIImageRequest* 
nsHTToolbarDataModel :: RequestImage(nsString& reqUrl) const
{
	nsIImageGroup* pGroup = GetImageGroup();

	char * url = reqUrl.ToNewCString();  //*** candidate for auto_ptr
	nsIImageRequest *request = pGroup->GetImage(url,
								 (nsIImageRequestObserver*)this,
								 NULL, 0, 0,  
								 0);
	delete[] url;

	return request;
} // RequestImage


//
// Notify
//
// image request observer implementation
//
void 
nsHTToolbarDataModel::Notify(nsIImageRequest *aImageRequest,
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
} // Notify


//
// NotifyError
//
// Called when the image observer gets an error
//
void 
nsHTToolbarDataModel::NotifyError(nsIImageRequest *aImageRequest,
                               nsImageError aErrorType)
{
	//*** what should we do here?
}


//
// CreateDataItemWithContentNode
//
// Build one of our special data items hooked up to the given content node.
//
nsHierarchicalDataItem* 
nsHTToolbarDataModel :: CreateDataItemWithContentNode(nsIContent* aContent)
{
	//nsHTTreeItem* pItem = new nsHTTreeItem(pContent, this);
	return nsnull;
}
