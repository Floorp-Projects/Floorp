/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsIWebShell_h___
#define nsIWebShell_h___

#include "nsweb.h"
#include "nsIWidget.h"
#include "nsIScrollableView.h"
#include "nsIContentViewerContainer.h"

class nsIFactory;
class nsIPostData;
class nsIStreamObserver;
class nsIDocumentLoader;
class nsIWebShell;
class nsIWebShellContainer;
class nsIPref;

// Interface ID for nsIWebShell
#define NS_IWEB_SHELL_IID \
 { 0xa6cf9058, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

// Interface ID for nsIWebShellContainer
#define NS_IWEB_SHELL_CONTAINER_IID \
 { 0xa6cf905a, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

// Class ID for an implementation of nsIWebShell
#define NS_WEB_SHELL_CID \
 { 0xa6cf9059, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

//----------------------------------------------------------------------

// Container for web shell's
class nsIWebShellContainer : public nsISupports {
public:
  NS_IMETHOD SetTitle(const nsString& aTitle) = 0;

  NS_IMETHOD GetTitle(nsString& aResult) = 0;

  // History control
  NS_IMETHOD WillLoadURL(nsIWebShell* aShell, const nsString& aURL) = 0;
  NS_IMETHOD BeginLoadURL(nsIWebShell* aShell, const nsString& aURL) = 0;

  // XXX not yet implemented; should we?
  NS_IMETHOD EndLoadURL(nsIWebShell* aShell, const nsString& aURL) = 0;

  // Chrome control
// NS_IMETHOD SetHistoryIndex(PRInt32 aIndex, PRInt32 aMaxIndex) = 0;

  // Link traversing control
};

// Return value from WillLoadURL
#define NS_WEB_SHELL_CANCEL_URL_LOAD      0xC0E70000

//----------------------------------------------------------------------

/**
 * The web shell is a container for implementations of nsIContentViewer.
 * It is a content-viewer-container and also knows how to delegate certain
 * behavior to an nsIWebShellContainer.
 *
 * Web shells can be arranged in a tree.
 *
 * Web shells are also nsIWebShellContainer's because they can contain
 * other web shells.
 */
class nsIWebShell : public nsIContentViewerContainer {
public:
  NS_IMETHOD Init(nsNativeWidget aNativeParent,
                  const nsRect& aBounds,
                  nsScrollPreference aScrolling = nsScrollPreference_kAuto,
                  PRBool aAllowPlugins = PR_TRUE) = 0;

  NS_IMETHOD Destroy() = 0;

  NS_IMETHOD GetBounds(nsRect& aResult) = 0;

  NS_IMETHOD SetBounds(const nsRect& aBounds) = 0;

  NS_IMETHOD MoveTo(PRInt32 aX, PRInt32 aY) = 0;

  NS_IMETHOD Show() = 0;

  NS_IMETHOD Hide() = 0;

  NS_IMETHOD GetContentViewer(nsIContentViewer*& aResult) = 0;
  // XXX SetContentViewer?

  NS_IMETHOD SetContainer(nsIWebShellContainer* aContainer) = 0;
  NS_IMETHOD GetContainer(nsIWebShellContainer*& aResult) = 0;

  NS_IMETHOD SetObserver(nsIStreamObserver* anObserver) = 0;
  NS_IMETHOD GetObserver(nsIStreamObserver*& aResult) = 0;

  NS_IMETHOD GetDocumentLoader(nsIDocumentLoader*& aResult) = 0;

  NS_IMETHOD SetPrefs(nsIPref* aPrefs) = 0;
  NS_IMETHOD GetPrefs(nsIPref*& aPrefs) = 0;

  NS_IMETHOD GetRootWebShell(nsIWebShell*& aResult) = 0;
  NS_IMETHOD SetParent(nsIWebShell* aParent) = 0;
  NS_IMETHOD GetParent(nsIWebShell*& aParent) = 0;
  NS_IMETHOD GetChildCount(PRInt32& aResult) = 0;
  NS_IMETHOD AddChild(nsIWebShell* aChild) = 0;
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIWebShell*& aResult) = 0;
  NS_IMETHOD GetName(nsString& aName) = 0;
  NS_IMETHOD SetName(const nsString& aName) = 0;
  NS_IMETHOD FindChildWithName(const nsString& aName,
                               nsIWebShell*& aResult) = 0;

  // History api's
  NS_IMETHOD Back() = 0;
  NS_IMETHOD Forward() = 0;
  NS_IMETHOD LoadURL(const nsString& aURLSpec,
                     nsIPostData* aPostData=nsnull) = 0;
  NS_IMETHOD GoTo(PRInt32 aHistoryIndex) = 0;
  NS_IMETHOD GetHistoryIndex(PRInt32& aResult) = 0;
  NS_IMETHOD GetURL(PRInt32 aHistoryIndex, nsString& aURLResult) = 0;

  // Chrome api's
  NS_IMETHOD SetTitle(const nsString& aTitle) = 0;

  NS_IMETHOD GetTitle(nsString& aResult) = 0;
  // SetToolBar
  // SetMenuBar
  // SetStatusBar
};

extern "C" NS_WEB nsresult
NS_NewWebShellFactory(nsIFactory** aFactory);

#endif /* nsIWebShell_h___ */
