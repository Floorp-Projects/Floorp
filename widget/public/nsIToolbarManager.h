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

#ifndef nsIToolbarManager_h___
#define nsIToolbarManager_h___

#include "nsISupports.h"

class nsIToolbar;
class nsIToolbarManagerListener;
class nsIImageButton;

// {19F205F1-5193-11d2-8DC1-00609703C14E}
#define NS_ITOOLBARMANAGER_IID      \
{ 0x19f205f1, 0x5193, 0x11d2, \
  { 0x8d, 0xc1, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

class nsIToolbarManager : public nsISupports
{

public:

 /**
  * Adds a toolbar to the toolbar manager
  *
  */
  NS_IMETHOD AddToolbar(nsIToolbar* aToolbar) = 0;

 /**
  * Adds a toolbar to the toolbar manager
  *
  */
  NS_IMETHOD InsertToolbarAt(nsIToolbar* aToolbar, PRInt32 anIndex) = 0;

 /**
  * Get a toolbar to the toolbar manager
  *
  */
  NS_IMETHOD GetNumToolbars(PRInt32 & aNumToolbars) = 0;

 /**
  * Get a toolbar to the toolbar manager
  *
  */
  NS_IMETHOD GetToolbarAt(nsIToolbar*& aToolbar, PRInt32 anIndex) = 0;

 /**
  * Forces the toolbar manager to layout
  *
  */
  NS_IMETHOD DoLayout() = 0;

 /**
  * Adds a Listener to the toolbar manager
  *
  */
  NS_IMETHOD AddToolbarListener(nsIToolbarManagerListener * aListener) = 0;

 /**
  * Registers the URLS for the Toolbar Tab Images that enable
  * the Toolbar to collapse
  *
  */
  NS_IMETHOD SetCollapseTabURLs(const nsString& aUpURL,
                                const nsString& aPressedURL,
                                const nsString& aDisabledURL,
                                const nsString& aRollOverURL) = 0;

 /**
  * Get the URLS for the Toolbar Tab Images that enable
  * the Toolbar to collapse
  *
  */
  NS_IMETHOD GetCollapseTabURLs(nsString& aUpURL,
                                nsString& aPressedURL,
                                nsString& aDisabledURL,
                                nsString& aRollOverURL) = 0;

 /**
  * Registers the URLS for the Tab Images for the manager for
  * making toolbars expand
  *
  */
  NS_IMETHOD SetExpandTabURLs(const nsString& aUpURL,
                              const nsString& aPressedURL,
                              const nsString& aDisabledURL,
                              const nsString& aRollOverURL) = 0;

};

#endif /* nsIToolbarManager_h___ */

