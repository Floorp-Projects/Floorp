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
#ifndef nsIWebViewerContainer_h___
#define nsIWebViewerContainer_h___

#include "nsIContentViewer.h"
#include "nsIParser.h"
#include "nsIMenuManager.h"
#include "nsIXPFCMenuBar.h"
#include "nsIWebShell.h"
#include "nsIXPFCToolbarManager.h"
#include "nsIXPFCToolbar.h"
#include "nsIXPFCDialog.h"
#include "nsIApplicationShell.h"
#include "nsIXPFCCommand.h"
#include "nsIXPFCCanvas.h"

//06245670-306a-11d2-9247-00805f8a7ab6
#define NS_IWEB_VIEWER_CONTAINER_IID   \
{ 0x06245670, 0x306a, 0x11d2,    \
{ 0x92, 0x47, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

class nsIWebViewerContainer : public nsIContentViewerContainer
{

public:

  NS_IMETHOD SetMenuManager(nsIMenuManager * aMenuManager) = 0;
  NS_IMETHOD_(nsIMenuManager *) GetMenuManager() = 0;
  NS_IMETHOD SetMenuBar(nsIXPFCMenuBar * aMenuBar) = 0;
  NS_IMETHOD UpdateMenuBar() = 0;

  NS_IMETHOD SetToolbarManager(nsIXPFCToolbarManager * aToolbarManager) = 0;
  NS_IMETHOD_(nsIXPFCToolbarManager *) GetToolbarManager() = 0;
  NS_IMETHOD AddToolbar(nsIXPFCToolbar * aToolbar) = 0;
  NS_IMETHOD RemoveToolbar(nsIXPFCToolbar * aToolbar) = 0;
  NS_IMETHOD UpdateToolbars() = 0;

  NS_IMETHOD ShowDialog(nsIXPFCDialog * aDialog) = 0;

  NS_IMETHOD SetApplicationShell(nsIApplicationShell* aShell) = 0;
  NS_IMETHOD GetApplicationShell(nsIApplicationShell*& aResult) = 0;

  NS_IMETHOD SetContentViewer(nsIContentViewer* aViewer) = 0;
  NS_IMETHOD GetContentViewer(nsIContentViewer*& aResult) = 0;

  NS_IMETHOD_(nsEventStatus) ProcessCommand(nsIXPFCCommand * aCommand) = 0;

  NS_IMETHOD LoadURL(const nsString& aURLSpec,
                     nsIStreamObserver* aListener,
                     nsIXPFCCanvas * aParentCanvas = 0,
                     nsIPostData* aPostData = 0) = 0;

};


#endif /* nsWebViewerContainer_h___ */
