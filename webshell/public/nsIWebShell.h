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
#include "nsILoadAttribs.h"

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

typedef enum {
  nsLoadURL,
  nsLoadHistory,
  nsLoadLink,
  nsLoadRefresh
} nsLoadType;

// Container for web shell's
class nsIWebShellContainer : public nsISupports {
public:
  // History control
  NS_IMETHOD WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsLoadType aReason) = 0;
  NS_IMETHOD BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL) = 0;

  // XXX not yet implemented; should we?
  NS_IMETHOD ProgressLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aProgress, PRInt32 aProgressMax) = 0;
  NS_IMETHOD EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aStatus) = 0;

  //instances
  NS_IMETHOD NewWebShell(nsIWebShell *&aNewWebShell) = 0;
  NS_IMETHOD FindWebShellWithName(const PRUnichar* aName,
                                  nsIWebShell*& aResult) = 0;

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
                  PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h,
                  nsScrollPreference aScrolling = nsScrollPreference_kAuto,
                  PRBool aAllowPlugins = PR_TRUE) = 0;

  NS_IMETHOD Destroy() = 0;

  NS_IMETHOD GetBounds(PRInt32 &x, PRInt32 &y, PRInt32 &w, PRInt32 &h) = 0;

  NS_IMETHOD SetBounds(PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h) = 0;

  NS_IMETHOD MoveTo(PRInt32 aX, PRInt32 aY) = 0;

  NS_IMETHOD Show() = 0;

  NS_IMETHOD Hide() = 0;

  NS_IMETHOD SetFocus() = 0;
  NS_IMETHOD RemoveFocus() = 0;

  NS_IMETHOD Repaint(PRBool aForce) = 0;

  // SetContextView moved to bottom to keep branch/tip interfaces
  // compatible
  NS_IMETHOD GetContentViewer(nsIContentViewer*& aResult) = 0;

  NS_IMETHOD SetContainer(nsIWebShellContainer* aContainer) = 0;
  NS_IMETHOD GetContainer(nsIWebShellContainer*& aResult) = 0;

  NS_IMETHOD SetObserver(nsIStreamObserver* anObserver) = 0;
  NS_IMETHOD GetObserver(nsIStreamObserver*& aResult) = 0;

  NS_IMETHOD SetPrefs(nsIPref* aPrefs) = 0;
  NS_IMETHOD GetPrefs(nsIPref*& aPrefs) = 0;

  NS_IMETHOD GetRootWebShell(nsIWebShell*& aResult) = 0;
  NS_IMETHOD SetParent(nsIWebShell* aParent) = 0;
  NS_IMETHOD GetParent(nsIWebShell*& aParent) = 0;
  NS_IMETHOD GetChildCount(PRInt32& aResult) = 0;
  NS_IMETHOD AddChild(nsIWebShell* aChild) = 0;
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIWebShell*& aResult) = 0;
  NS_IMETHOD GetName(PRUnichar** aName) = 0;
  NS_IMETHOD SetName(const PRUnichar* aName) = 0;
  NS_IMETHOD FindChildWithName(const PRUnichar* aName,
                               nsIWebShell*& aResult) = 0;

  // Document load api's
  NS_IMETHOD GetDocumentLoader(nsIDocumentLoader*& aResult) = 0;
  NS_IMETHOD LoadURL(const PRUnichar *aURLSpec,
                     nsIPostData* aPostData=nsnull,
                     PRBool aModifyHistory=PR_TRUE,
                     nsURLReloadType aType=nsURLReload,
                     const PRUint32 aLocalIP=0) = 0;
  NS_IMETHOD Stop(void) = 0;
  NS_IMETHOD Reload(nsURLReloadType aType) = 0;
  
  // History api's
  NS_IMETHOD Back() = 0;
  NS_IMETHOD CanBack() = 0;
  NS_IMETHOD Forward() = 0;
  NS_IMETHOD CanForward() = 0;
  NS_IMETHOD GoTo(PRInt32 aHistoryIndex) = 0;
  NS_IMETHOD GetHistoryIndex(PRInt32& aResult) = 0;
  NS_IMETHOD GetURL(PRInt32 aHistoryIndex, PRUnichar **aURLResult) = 0;

  // Chrome api's
  NS_IMETHOD SetTitle(const PRUnichar *aTitle) = 0;

  NS_IMETHOD GetTitle(PRUnichar **aResult) = 0;
  // SetToolBar
  // SetMenuBar
  // SetStatusBar

  NS_IMETHOD SetContentViewer(nsIContentViewer* aViewer) = 0;
  // XXX these are here until there a better way to pass along info to a sub doc
  NS_IMETHOD GetMarginWidth (PRInt32& aWidth)  = 0;
  NS_IMETHOD SetMarginWidth (PRInt32  aWidth)  = 0;
  NS_IMETHOD GetMarginHeight(PRInt32& aWidth)  = 0;
  NS_IMETHOD SetMarginHeight(PRInt32  aHeight) = 0;

  // Selection Related Methods
  /**
    * Returns the whether there is a selection or not
   */
  NS_IMETHOD IsSelection(PRBool & aIsSelection) = 0;

  /**
    * Returns the whether the selection can be cut (editor or a form control)
   */
  NS_IMETHOD IsSelectionCutable(PRBool & aIsSelection) = 0;

  /**
    * Returns whether the data can be pasted (editor or a form control)
   */
  NS_IMETHOD IsSelectionPastable(PRBool & aIsSelection) = 0;

  /**
    * Copies the Selection from the content or a form control
   */
  NS_IMETHOD GetSelection(PRUnichar *& aSelection) = 0;

  /**
    * Cuts the Selection from the content or a form control
   */
  NS_IMETHOD CutSelection(PRUnichar *& aSelection) = 0;

  /**
    * Pastes the Selection into the content or a form control
   */
  NS_IMETHOD PasteSelection(const PRUnichar * aSelection) = 0;

  /**
    * Selects all the Content
   */
  NS_IMETHOD SelectAll() = 0;

  /**
    * Finds text in content
   */
  NS_IMETHOD FindNext(const PRUnichar * aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound) = 0;

};

extern "C" NS_WEB nsresult
NS_NewWebShellFactory(nsIFactory** aFactory);

#endif /* nsIWebShell_h___ */
