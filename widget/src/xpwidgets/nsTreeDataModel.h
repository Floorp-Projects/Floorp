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

#ifndef nsTreeDataModel_h___
#define nsTreeDataModel_h___

#include "nsHierarchicalDataModel.h"

class nsTreeItem;
class nsTreeColumn;
class nsTreeControlStripItem;

struct nsColumnHeaderStyleInfo: public nsBasicStyleInfo
{
	nscolor sortFGColor;
	nscolor sortBGColor;
	nsIImage* pSortBackgroundImage;
	PRBool transparentSort;
	nsFont sortFont;

	nsColumnHeaderStyleInfo(const nsFont& aFont)
		:nsBasicStyleInfo(aFont), sortFont(aFont)
	{ pSortBackgroundImage = nsnull; };
};

struct nsTreeItemStyleInfo: public nsBasicStyleInfo
{
	PRBool selectWholeLine;
	nscolor selectionFGColor;
	nscolor selectionBGColor;
	nsIImage* pSelectionBackgroundImage;
	PRBool transparentSelection;
	nsFont selectionFont;

	PRBool rolloverWholeLine;
	nscolor rolloverFGColor;
	nscolor rolloverBGColor;
	nsIImage* pRolloverBackgroundImage;
	PRBool transparentRollover;
	nsFont rolloverFont;

	nscolor sortFGColor;
	nscolor sortBGColor;
	nsIImage* pSortBackgroundImage;
	PRBool transparentSort;
	nsFont sortFont;

	PRBool showTrigger;
	PRBool leftJustifyTrigger;
	PRBool showIcon;

	nsIImage* pTriggerImage;
	nsIImage* pIconImage;

	nscolor showHorizontalDivider;
	nscolor horizontalDividerColor;

	nscolor showVerticalDivider;
	nscolor verticalDividerColor;

	nsTreeItemStyleInfo(const nsFont& aFont)
		:nsBasicStyleInfo(aFont), sortFont(aFont), selectionFont(aFont), rolloverFont(aFont)
	{
		pSelectionBackgroundImage = pSortBackgroundImage = 
			pTriggerImage = pIconImage = nsnull; 
	};
};

//------------------------------------------------------------
// An abstract API for communication with a hierarchical store of
// information that is designed to go into a tree widget (a widget
// with tree lines and multiple columns).
//------------------------------------------------------------

class nsTreeDataModel : public nsHierarchicalDataModel
                  
{
protected:
    nsTreeDataModel() {}; // Disallow instantiation of abstract class.

public:
    virtual ~nsTreeDataModel() {};

	// Column APIs
	virtual PRUint32 GetVisibleColumnCount() const = 0;
	virtual PRUint32 GetColumnCount() const = 0;
	virtual nsTreeColumn* GetNthColumn(PRUint32 n) const = 0;
	virtual void SetVisibleColumnCount(PRUint32 n) = 0;

	// Control Strip Iteration
	virtual PRUint32 GetControlStripItemCount() const = 0;
	virtual nsTreeControlStripItem* GetNthControlStripItem(PRUint32 n) const = 0;
	virtual void GetControlStripCloseText(nsString& closeText) const = 0;

	// Visibility Queries
	virtual PRBool ShowTitleBar() const = 0;
	virtual PRBool ShowTitleBarText() const = 0;
	virtual PRBool ShowColumnHeaders() const = 0;
	virtual PRBool ShowControlStrip() const = 0;

	// Style Retrievers
	virtual void GetTitleBarStyle(nsIDeviceContext* pContext,
								  nsBasicStyleInfo& styleInfo) const = 0;
	virtual void GetColumnHeaderStyle(nsIDeviceContext* pContext, 
									  nsColumnHeaderStyleInfo& styleInfo) const = 0;
	virtual void GetControlStripStyle(nsIDeviceContext* pContext, 
									  nsBasicStyleInfo& styleInfo) const = 0;

	// Text for the title bar
	virtual void GetTitleBarText(nsString& text) const = 0;
};

#endif /* nsTreeDataModel_h___ */
