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
#include "nsIScrollableView.h"

class nsIFactory;
class nsIPostData;
class nsIStreamObserver;
class nsIDocumentLoader;
class nsIDocumentLoaderObserver;
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
  static const nsIID& GetIID() { static nsIID iid = NS_IWEB_SHELL_CONTAINER_IID; return iid; }

  // History control
   NS_IMETHOD WillLoadURL(nsIWebShell* aShell,
                          const PRUnichar* aURL,
                          nsLoadType aReason) = 0;
 
   NS_IMETHOD BeginLoadURL(nsIWebShell* aShell,
                           const PRUnichar* aURL) = 0;
 

   NS_IMETHOD ProgressLoadURL(nsIWebShell* aShell,
                              const PRUnichar* aURL,
                              PRInt32 aProgress,
                              PRInt32 aProgressMax) = 0;
 
   NS_IMETHOD EndLoadURL(nsIWebShell* aShell,
                         const PRUnichar* aURL,
                         PRInt32 aStatus) = 0;
 
 //instances

  // XXX kipp sez: I don't think that this method should be a part of
  // this interface.
  NS_IMETHOD NewWebShell(PRUint32 aChromeMask,
                         PRBool aVisible,
                         nsIWebShell *&aNewWebShell) = 0;

  NS_IMETHOD FindWebShellWithName(const PRUnichar* aName,
                                  nsIWebShell*& aResult) = 0;

  
  /**
   * Notify the WebShellContainer that a contained webshell is
   * offering focus (for example if it finshed tabbing through its
   * contents).  The container can choose to set focus to themselves
   * or ignore the message and let the contained keep focus
   */
  NS_IMETHOD FocusAvailable(nsIWebShell* aFocusedWebShell, PRBool& aFocusTaken) = 0;

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
  static const nsIID& GetIID() { static nsIID iid = NS_IWEB_SHELL_IID; return iid; }

  /**
   * Initialization function for a WebShell instance.  This method provides 
   * information needed by the WebShell to embed itself inside of a native 
   * window provided by the caller. It is assumed that this function will be 
   * called only once.
   */
  NS_IMETHOD Init(nsNativeWidget aNativeParent,
                  PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h,
                  nsScrollPreference aScrolling = nsScrollPreference_kAuto,
                  PRBool aAllowPlugins = PR_TRUE,
                  PRBool aIsSunkenBorder = PR_FALSE) = 0;

  /**
   * Notify the WebShell that its parent's window is being destroyed.  After 
   * being destroyed, a WebShell is no longer visible and can no longer display 
   * documents.
   */
  NS_IMETHOD Destroy() = 0;

  /**
   * Return the current dimensions of the WebShell.
   */
  NS_IMETHOD GetBounds(PRInt32 &x, PRInt32 &y, PRInt32 &w, PRInt32 &h) = 0;

  /**
   * Resize the WebShell to the given dimensions.
   */
  NS_IMETHOD SetBounds(PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h) = 0;

  NS_IMETHOD MoveTo(PRInt32 aX, PRInt32 aY) = 0;

  /**
   * Make the WebShell visible.
   */
  NS_IMETHOD Show() = 0;

  /**
   * Make the WebShell invisible.
   */
  NS_IMETHOD Hide() = 0;

  /**
   * Give the WebShell window focus.
   */
  NS_IMETHOD SetFocus() = 0;
  NS_IMETHOD RemoveFocus() = 0;

  /**
   * Force the WebShell to repaint its window.
   */
  NS_IMETHOD Repaint(PRBool aForce) = 0;

  /**
   * Set the nsIWebShellContainer for the WebShell.
   */
  NS_IMETHOD SetContainer(nsIWebShellContainer* aContainer) = 0;

  /**
   * Return the current nsIWebShellContainer.
   */
  NS_IMETHOD GetContainer(nsIWebShellContainer*& aResult) = 0;

  /**
   * Set the nsIStreamObserver which receives all notifications from URLs 
   * in the old fashion.
   */
  NS_IMETHOD SetObserver(nsIStreamObserver* anObserver) = 0;

  /**
   * Return the current nsIStreamObserver.
   */
  NS_IMETHOD GetObserver(nsIStreamObserver*& aResult) = 0;

  /**
   * Set the DocLoaderObserver which receives all notifications from URLs 
   * loaded by the document.
   */
  NS_IMETHOD SetDocLoaderObserver(nsIDocumentLoaderObserver* anObserver) = 0;

  /**
   * Return the current nsIDocLoadeObserver
   */
  NS_IMETHOD GetDocLoaderObserver(nsIDocumentLoaderObserver*& aResult) = 0;

  /**
   * Set the nsIPref used to get/set preference values...
   */
  NS_IMETHOD SetPrefs(nsIPref* aPrefs) = 0;

  /**
   * Return the current nsIPref interface.
   */
  NS_IMETHOD GetPrefs(nsIPref*& aPrefs) = 0;

  /**
   * Return the root WebShell instance.  Since WebShells can be nested 
   * (when frames are present for example) this instance represents the 
   * outermost WebShell.
   */
  NS_IMETHOD GetRootWebShell(nsIWebShell*& aResult) = 0;

  /**
   * Set the parent WebShell.
   */
  NS_IMETHOD SetParent(nsIWebShell* aParent) = 0;

  /**
   * Return the parent WebShell.
   */
  NS_IMETHOD GetParent(nsIWebShell*& aParent) = 0;

  /**
   * Return the current number of WebShells which are immediate children 
   * of the current WebShell.
   */
  NS_IMETHOD GetChildCount(PRInt32& aResult) = 0;

  /**
   * Add a new child WebShell.
   */
  NS_IMETHOD AddChild(nsIWebShell* aChild) = 0;

  /**
   * Return the child WebShell at the requested index.
   */
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIWebShell*& aResult) = 0;

  /**
   * Return the name of the current WebShell.
   */
  NS_IMETHOD GetName(const PRUnichar** aName) = 0;

  /**
   * Set the name of the current WebShell.
   */
  NS_IMETHOD SetName(const PRUnichar* aName) = 0;

  /**
   * Return the child WebShell with the specified name.
   */
  NS_IMETHOD FindChildWithName(const PRUnichar* aName,
                               nsIWebShell*& aResult) = 0;
  //
  // Document load api's
  //
  /**
   * Return the nsIDocumentLoader associated with the WebShell.
   */
  NS_IMETHOD GetDocumentLoader(nsIDocumentLoader*& aResult) = 0;

  /**
   * Load the document associated with the specified URL into the WebShell.
   */
  NS_IMETHOD LoadURL(const PRUnichar *aURLSpec,
                     nsIPostData* aPostData=nsnull,
                     PRBool aModifyHistory=PR_TRUE,
                     nsURLReloadType aType=nsURLReload,
                     const PRUint32 aLocalIP=0) = 0;

  /**
   * Load the document associated with the specified URL into the WebShell.
   */
  NS_IMETHOD LoadURL(const PRUnichar *aURLSpec,
                     const char* aCommand, 
                     nsIPostData* aPostData=nsnull,
                     PRBool aModifyHistory=PR_TRUE,
                     nsURLReloadType aType=nsURLReload,
                     const PRUint32 aLocalIP=0) = 0;


  /**
   * Stop loading the current document.
   */
  NS_IMETHOD Stop(void) = 0;

  /**
   * Reload the current document.
   */
  NS_IMETHOD Reload(nsURLReloadType aType) = 0;

  //
  // History api's
  //
  /**
   * Load the previous document in the history list.
   */
  NS_IMETHOD Back() = 0;
  NS_IMETHOD CanBack() = 0;
  NS_IMETHOD Forward() = 0;
  NS_IMETHOD CanForward() = 0;
  NS_IMETHOD GoTo(PRInt32 aHistoryIndex) = 0;
  NS_IMETHOD GetHistoryLength(PRInt32& aResult) = 0;
  NS_IMETHOD GetHistoryIndex(PRInt32& aResult) = 0;
  NS_IMETHOD GetURL(PRInt32 aHistoryIndex, const PRUnichar **aURLResult) = 0;

  // Chrome api's
  NS_IMETHOD SetTitle(const PRUnichar *aTitle) = 0;

  NS_IMETHOD GetTitle(const PRUnichar **aResult) = 0;
  // SetToolBar
  // SetMenuBar
  // SetStatusBar

  NS_IMETHOD SetContentViewer(nsIContentViewer* aViewer) = 0;
  // XXX these are here until there a better way to pass along info to a sub doc
  NS_IMETHOD GetMarginWidth (PRInt32& aWidth)  = 0;
  NS_IMETHOD SetMarginWidth (PRInt32  aWidth)  = 0;
  NS_IMETHOD GetMarginHeight(PRInt32& aWidth)  = 0;
  NS_IMETHOD SetMarginHeight(PRInt32  aHeight) = 0;
  NS_IMETHOD SetScrolling(PRInt32 aScrolling, PRBool aSetCurrentAndInitial = PR_TRUE)  = 0;
  NS_IMETHOD GetScrolling(PRInt32& aScrolling) = 0;
  NS_IMETHOD SetIsFrame(PRBool aIsFrame)       = 0;
  NS_IMETHOD GetIsFrame(PRBool& aIsFrame)      = 0;

  NS_IMETHOD GetDefaultCharacterSet (const PRUnichar** aDefaultCharacterSet) = 0;
  NS_IMETHOD SetDefaultCharacterSet (const PRUnichar*  aDefaultCharacterSet)  = 0;
  /**
   * Set/Get the document scale factor
   */

  NS_IMETHOD SetZoom(float aZoom) = 0;
  NS_IMETHOD GetZoom(float *aZoom) = 0;

  /**
    * Finds text in content
   */
  NS_IMETHOD FindNext(const PRUnichar * aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound) = 0;
};

extern "C" NS_WEB nsresult
NS_NewWebShellFactory(nsIFactory** aFactory);

#endif /* nsIWebShell_h___ */
