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

#ifndef nsHTTreeItem_h___
#define nsHTTreeItem_h___

#include "nsTreeItem.h"
#include "nsHTItem.h"
#include "nsIImageObserver.h"

class nsHTDataModel;
class nsIImageGroup;
class nsIContent;

//------------------------------------------------------------
// This class functions as the data source for column information (things like
// width, desired percentage, and sorting).

class nsHTTreeItem : public nsTreeItem, public nsHTItem, public nsIImageRequestObserver
                  
{
public:
    nsHTTreeItem(nsIContent* pContent, nsHierarchicalDataModel* pModel);
    virtual ~nsHTTreeItem();

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

	// All functions inherited from HierarchicalDataItem are delegated to
	// the concrete implementation.
	virtual PRBool IsExpanded() const { return IsExpandedDelegate(); };
	virtual void ToggleOpenState() { ToggleOpenStateDelegate(); };
	virtual PRUint32 GetIndentationLevel() const { return GetIndentationLevelDelegate(); };
	virtual void SetIndentationLevel(PRUint32 n) { SetIndentationLevelDelegate(n); };
	// End of delegated functions

	virtual void GetItemStyle(nsIDeviceContext* dc, 
							  nsTreeItemStyleInfo& styleInfo) const;

	virtual void GetTextForColumn(nsTreeColumn* pColumn, nsString& nodeText) const;

	virtual void GetTreeItemRectangle(nsRect& rect) const { rect = mRowRectangle; };
	virtual void SetTreeItemRectangle(const nsRect& rect) { mRowRectangle = rect; };
	virtual void GetTriggerRectangle(nsRect& rect) const { rect = mTriggerRectangle; };
	virtual void SetTriggerRectangle(const nsRect& rect) { mTriggerRectangle = rect; };

protected:
	nsIImageRequest* RequestImage(nsString& reqUrl) const; // Helper to kick off the image load.
	nsIImage* GetTriggerImage() const;
	nsIImage* GetIconImage() const;
	nsIImage* GetBackgroundImage() const;
	
protected:
	 nsIImageRequest* mClosedIconRequest;	// Closed image
	 nsIImageRequest* mOpenIconRequest;		// Open image
	 nsIImageRequest* mClosedTriggerRequest;	// Closed trigger image
	 nsIImageRequest* mOpenTriggerRequest;	// Open trigger image
	 nsIImageRequest* mBackgroundRequest;	// The background image

	 nsRect mRowRectangle;	// A cached copy of our position within the tree view.
	 nsRect mTriggerRectangle;	// A cached copy of our trigger rectangle
};

#endif /* nsHTTreeItem_h___ */
