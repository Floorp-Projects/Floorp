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

#ifndef nsHTTreeDataModel_h___
#define nsHTTreeDataModel_h___

#include "nsHTDataModel.h"
#include "nsTreeDataModel.h"
#include "nsIImageObserver.h"

class nsTreeColumn;
class nsTreeControlStripItem;
	
//------------------------------------------------------------
// An abstract API for communication with a hierarchical store of
// information. Iteration over children in the model is provided.
// The model also provides a flattened view of the tree (a list
// of visible nodes).
//------------------------------------------------------------

class nsHTTreeDataModel : public nsHTDataModel, public nsTreeDataModel, public nsIImageRequestObserver
                  
{
public:
    nsHTTreeDataModel();
    virtual ~nsHTTreeDataModel();

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
	// End of interfaces

	// Functions inherited from abstract hierarchical data model should be delegated to our
	// concrete base class
	// Setting the Content Root for the tree
	virtual void SetContentRoot(nsIContent* pContent) { SetContentRootDelegate(pContent); }
	virtual nsHierarchicalDataItem* GetRoot() const { return GetRootDelegate(); }
	virtual PRUint32 GetFirstVisibleItemIndex() const { return GetFirstVisibleItemIndexDelegate(); };
	virtual void SetFirstVisibleItemIndex(PRUint32 index) { SetFirstVisibleItemIndexDelegate(index); };
	virtual nsHierarchicalDataItem* GetNthItem(PRUint32 n) const { return GetNthItemDelegate(n) ;};
	virtual void SetDataModelListener(nsDataModelWidget* pListener) { SetDataModelListenerDelegate(pListener); };
	// End of delegated functions
	
	// Column Iteration
	virtual PRUint32 GetVisibleColumnCount() const;
	virtual PRUint32 GetColumnCount() const;
	virtual nsTreeColumn* GetNthColumn(PRUint32 n) const;
	virtual void SetVisibleColumnCount(PRUint32 n);

	// Control Strip Iteration
	virtual PRUint32 GetControlStripItemCount() const;
	virtual nsTreeControlStripItem* GetNthControlStripItem(PRUint32 n) const;
	virtual void GetControlStripCloseText(nsString& closeText) const;

	// Visibility Queries
	virtual PRBool ShowTitleBar() const;
	virtual PRBool ShowTitleBarText() const;
	virtual PRBool ShowColumnHeaders() const;
	virtual PRBool ShowControlStrip() const;

	// Style Retrievers
	virtual void GetTitleBarStyle(nsIDeviceContext* pContext, 
								  nsBasicStyleInfo& styleInfo) const;
	virtual void GetColumnHeaderStyle(nsIDeviceContext* pContext, 
									  nsColumnHeaderStyleInfo& styleInfo) const;
	virtual void GetControlStripStyle(nsIDeviceContext* pContext,  
									  nsBasicStyleInfo& styleInfo) const;

	// Text for the title bar, control strip and column headers
	virtual void GetTitleBarText(nsString& text) const;

public:
	// Inherited functions from HTDataModel go here.
	nsHierarchicalDataItem* CreateDataItemWithContentNode(nsIContent* pContent);

protected:
	nsIImageRequest* RequestImage(nsString& reqUrl) const; // Helper to kick off the image load.
	nsIImage* GetTitleBGImage() const;
	nsIImage* GetControlStripBGImage() const;
	nsIImage* GetColumnHeaderBGImage() const;

private:
	nsTreeColumn *mSingleColumn, *mSecondColumn, *mThirdColumn;
	nsTreeControlStripItem *mSingleControlStripItem;

	PRUint32 mVisibleColumnCount;
	PRUint32 mTotalColumnCount;
	nsIImageRequest* mTitleBGRequest;		// The title bar background image
	nsIImageRequest* mControlStripBGRequest;// The control strip bg image
	nsIImageRequest* mColumnHeaderBGRequest;// The column header background image

};

#endif /* nsHTTreeDataModel_h___ */
