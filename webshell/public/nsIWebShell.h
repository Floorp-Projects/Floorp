/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsIWebShell_h___
#define nsIWebShell_h___

#include "nsweb.h"
#include "nsIWidget.h"
#include "nsIParser.h"
#include "nsIScrollableView.h"
#include "nsIContentViewerContainer.h"
#include "nsIChannel.h"
#include "nsIScrollableView.h"
#include "nsISessionHistory.h"
#include "nsIUrlDispatcher.h"

class nsIDOMElement;
class nsIDOMWindow;
class nsIFactory;
//class nsIPostData;
class nsIStreamObserver;
class nsIDocumentLoader;
class nsIDocumentLoaderObserver;
class nsIWebShell;
class nsIWebShellContainer;
class nsIPref;
class nsIContent;
class nsIURIContentListener;
class nsIDOMDocument;
class nsIChromeEventHandler;

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
                         nsresult aStatus) = 0;
 
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
   * Set the nsIWebShellContainer for the WebShell.
   */
  NS_IMETHOD SetContainer(nsIWebShellContainer* aContainer) = 0;

  /**
   * Return the current nsIWebShellContainer.
   */
  NS_IMETHOD GetContainer(nsIWebShellContainer*& aResult) = 0;

  /**
   * Returns the top level window (this would be the containing window
   * the same object that implements nsIWebShellContainer, nsIWebShellWindow).
   */
  NS_IMETHOD GetTopLevelWindow(nsIWebShellContainer** aTopLevelWindow) = 0;

  /**
   * Return the root WebShell instance.  Since WebShells can be nested 
   * (when frames are present for example) this instance represents the 
   * outermost WebShell.
   */
  NS_IMETHOD GetRootWebShell(nsIWebShell*& aResult) = 0;

  /**
   * Get the referrer of the link using this WebShell.
   */
  NS_IMETHOD GetReferrer(nsIURI **aReferrer) = 0;

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
                     nsIInputStream* aPostDataStream=nsnull,
                     PRBool aModifyHistory=PR_TRUE,
                     nsLoadFlags aType = nsIChannel::LOAD_NORMAL,
                     const PRUint32 aLocalIP=0,
                     nsISupports * aHistoryState=nsnull,
                     const PRUnichar* aReferrer=nsnull) = 0;

  /**
   * Load the document associated with the specified URL into the WebShell.
   */
  NS_IMETHOD LoadURL(const PRUnichar *aURLSpec,
                     const char* aCommand, 
                     nsIInputStream* aPostDataStream=nsnull,
                     PRBool aModifyHistory=PR_TRUE,
                     nsLoadFlags aType = nsIChannel::LOAD_NORMAL,
                     const PRUint32 aLocalIP=0,
                     nsISupports * aHistoryState=nsnull,
                     const PRUnichar* aReferrer=nsnull,
                     const char * aWindowTarget = nsnull) = 0;

  /**
   * Load the document with the specified URI into the WebShell. Assuming you have
   * an nsIURI instead of a PRUnichar string, this is the preferred way to load a url.
   */
   NS_IMETHOD LoadURI(nsIURI * aUri,
                      const char * aCommand,
                      nsIInputStream* aPostDataStream=nsnull,
                      PRBool aModifyHistory=PR_TRUE,
                      nsLoadFlags aType = nsIChannel::LOAD_NORMAL,
                      const PRUint32 aLocalIP=0,
                      nsISupports * aHistoryState=nsnull,
                      const PRUnichar* aReferrer=nsnull,
                      const char * aWindowTarget = nsnull) = 0;

  //
  // History api's
  //
  /**
   * Load the previous document in the history list.
   */
  NS_IMETHOD GoTo(PRInt32 aHistoryIndex) = 0;
  NS_IMETHOD GetHistoryLength(PRInt32& aResult) = 0;
  NS_IMETHOD GetHistoryIndex(PRInt32& aResult) = 0;
  NS_IMETHOD GetURL(PRInt32 aHistoryIndex, const PRUnichar **aURLResult) = 0;

  // SetToolBar
  // SetMenuBar
  // SetStatusBar

  /**
    * Finds text in content
   */
  NS_IMETHOD FindNext(const PRUnichar * aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound) = 0;

  /**
   * Set the URL of the current WebShell.
   */
  NS_IMETHOD SetURL(const PRUnichar* aURL) = 0;

  /**
   * Return the handle to SessionHistory
   */
  NS_IMETHOD GetSessionHistory(nsISessionHistory *& aResult) = 0;

  /**
   * Set a handle to SessionHistory
   */
  NS_IMETHOD SetSessionHistory(nsISessionHistory * aSHist) = 0;

  /* Set & Get Session History details */
  NS_IMETHOD SetIsInSHist(PRBool aIsFrame)       = 0;
  NS_IMETHOD GetIsInSHist(PRBool& aIsFrame)      = 0;

  /**
   * Get and set the history state for the document.
   */
  NS_IMETHOD CaptureHistoryState(nsISupports** aLayoutHistoryState) = 0;
  NS_IMETHOD GetHistoryState(nsISupports** aLayoutHistoryState) = 0;
  NS_IMETHOD SetHistoryState(nsISupports* aLayoutHistoryState) = 0;

  /**
   * Notify children to fire unload events before root data gone
   */
  NS_IMETHOD FireUnloadEvent(void) = 0;
};

extern "C" NS_WEB nsresult
NS_NewWebShellFactory(nsIFactory** aFactory);

#ifdef DETECT_WEBSHELL_LEAKS
extern "C" NS_WEB
unsigned long
NS_TotalWebShellsInExistence();
#endif

#endif /* nsIWebShell_h___ */
