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

#ifndef nsITabWidget_h__
#define nsITabWidget_h__

#include "nsIWidget.h"
#include "nsString.h"

#define NS_ITABWIDGET_IID      \
{ 0xf05ba6e0, 0xd4a7, 0x11d1, { 0x9e, 0xc0, 0x0, 0xaa, 0x0, 0x2f, 0xb8, 0x21 } };

/**
 * Tab widget.
 * Presents a lists of tabs to be clicked on.
 */
class nsITabWidget : public nsIWidget {

  public:
 
   /**
    * Setup the tabs
    *
    * @param aNumberOfTabs  The number of tabs in aTabLabels and aTabID
    * @param aTabLabels     title displayed in the tab
    */
  
    virtual void SetTabs(PRUint32 aNumberOfTabs, const nsString aTabLabels[]) = 0;

   /**
    * Get selected tab
    *
    * @return the index of the selected tab. Index ranges between 0 and (NumberOfTabs - 1)
    */
  
    virtual PRUint32 GetSelectedTab() = 0;
  
};

#endif
