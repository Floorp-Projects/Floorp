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

#ifndef nsToolbarDataModel_h___
#define nsToolbarDataModel_h___

#include "nsHierarchicalDataModel.h"


//*** placeholder for style info to come later
struct nsToolbarItemStyleInfo : public nsBasicStyleInfo
{
	PRBool selectWholeLine;
/*
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
*/
	nsToolbarItemStyleInfo(const nsFont& aFont)
		: nsBasicStyleInfo(aFont)
	{
	};

};


//
// class nsToolbarDataModel
//
// An abstract API for communication with a hierarchical store of
// information that is designed to go into a toolbar widget
//
// ***right now this is just a placeholder layer so that we can insert
// ***things later. Don't fret that it doesn't contain much of anything
//

class nsToolbarDataModel : public nsHierarchicalDataModel                 
{
protected:
    nsToolbarDataModel() {}; // Disallow instantiation of abstract class.

public:
    virtual ~nsToolbarDataModel() {};

	// Style Retrievers
	virtual void GetToolbarStyle(nsIDeviceContext* pContext,
								  nsBasicStyleInfo& styleInfo) const = 0;

};

#endif /* nsToolbarDataModel_h___ */
