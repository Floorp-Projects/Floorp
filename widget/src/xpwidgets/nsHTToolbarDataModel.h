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

#ifndef nsHTToolbarDataModel_h___
#define nsHTToolbarDataModel_h___

#include "nsHTDataModel.h"
#include "nsToolbarDataModel.h"
#include "nsIImageObserver.h"


	
//------------------------------------------------------------
// An abstract API for communication with a hierarchical store of
// information. Iteration over children in the model is provided.
// The model also provides a flattened view of the toolbar (a list
// of visible nodes).
//------------------------------------------------------------

class nsHTToolbarDataModel : public nsHTDataModel, public nsToolbarDataModel, public nsIImageRequestObserver               
{
public:
    nsHTToolbarDataModel();
    virtual ~nsHTToolbarDataModel();

	// Isupports interface ------------------
	NS_DECL_ISUPPORTS

	// IImageRequestObserver Interface ----------------
	void Notify(nsIImageRequest *aImageRequest,
                      nsIImage *aImage,
                      nsImageNotification aNotificationType,
                      PRInt32 aParam1, PRInt32 aParam2,
                      void *aParam3);
	void NotifyError(nsIImageRequest *aImageRequest,
                           nsImageError aErrorType);

	// ---------------- End of interfaces

	// Functions inherited from abstract hierarchical data model should be delegated to our
	// concrete base class
	virtual void SetContentRoot(nsIContent* pContent); 
	virtual nsHierarchicalDataItem* GetRoot() const { return GetRootDelegate(); }
	virtual PRUint32 GetFirstVisibleItemIndex() const { return GetFirstVisibleItemIndexDelegate(); };
	virtual void SetFirstVisibleItemIndex(PRUint32 index) { SetFirstVisibleItemIndexDelegate(index); };
	virtual nsHierarchicalDataItem* GetNthItem(PRUint32 n) const { return GetNthItemDelegate(n) ;};
	virtual void SetDataModelListener(nsDataModelWidget* pListener) { SetDataModelListenerDelegate(pListener); };

	// ---------------- End of delegated functions
	
	// Style Retrievers
	virtual void GetToolbarStyle(nsIDeviceContext* pContext, 
								  nsBasicStyleInfo& styleInfo) const;
	
	// Inherited functions from HTDataModel go here.
	nsHierarchicalDataItem* CreateDataItemWithContentNode(nsIContent* pContent);

protected:

	nsIImageRequest* RequestImage(nsString& reqUrl) const; // Helper to kick off the image load.
	nsIImage* GetBGImage() const;

private:

	nsIImageRequest* mBGRequest;		// The toolbar background image. ***should be com_auto_ptr

};

#endif /* nsHTTreeDataModel_h___ */
