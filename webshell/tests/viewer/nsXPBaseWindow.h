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
#ifndef nsXPBaseWindow_h___
#define nsXPBaseWindow_h___

#include "nsIXPBaseWindow.h"
#include "nsIStreamListener.h"
#ifdef NECKO
#else
#include "nsINetSupport.h"
#endif
#include "nsIWebShell.h"
#include "nsIScriptContextOwner.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsCRT.h"

#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsIDOMMouseListener.h"

class nsViewerApp;
class nsIPresShell;
class nsIPref;

/**
 * 
 */
class nsXPBaseWindow : public nsIXPBaseWindow,
                       public nsIStreamObserver,
#ifdef NECKO
#else
                       public nsINetSupport,
#endif
                       public nsIWebShellContainer,
                       public nsIDOMMouseListener
{
public:
  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  nsXPBaseWindow();
  virtual ~nsXPBaseWindow();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIBrowserWindow
  NS_IMETHOD Init(nsXPBaseWindowType aType,
                  nsIAppShell*       aAppShell,
                  nsIPref*           aPrefs,
                  const nsString&    aDialogURL,
                  const nsString&    aTitle,
                  const nsRect&      aBounds,
                  PRUint32           aChromeMask,
                  PRBool             aAllowPlugins = PR_TRUE);


  NS_IMETHOD SetLocation(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD SetDimensions(PRInt32 aWidth, PRInt32 aHeight);

  NS_IMETHOD GetWindowBounds(nsRect& aBounds);
  NS_IMETHOD GetBounds(nsRect& aBounds);
  NS_IMETHOD SetVisible(PRBool aIsVisible);
  NS_IMETHOD Close();
  NS_IMETHOD SetTitle(const PRUnichar* aTitle);
  NS_IMETHOD GetTitle(const PRUnichar** aResult);
  NS_IMETHOD GetWebShell(nsIWebShell*& aResult);
  NS_IMETHOD GetPresShell(nsIPresShell*& aPresShell);

  //NS_IMETHOD HandleEvent(nsGUIEvent * anEvent);

  NS_IMETHOD LoadURL(const nsString &aURL);

#ifdef NECKO
  // nsIStreamObserver
	NS_IMETHOD OnStartRequest(nsIChannel* channel, nsISupports *ctxt);
	NS_IMETHOD OnStopRequest(nsIChannel* channel, nsISupports *ctxt, 
                           nsresult status, const PRUnichar *errorMsg);
#else
  // nsIStreamObserver
  NS_IMETHOD OnStartRequest(nsIURI* aURL, const char *aContentType);
  NS_IMETHOD OnProgress(nsIURI* aURL, PRUint32 aProgress, PRUint32 aProgressMax);
  NS_IMETHOD OnStatus(nsIURI* aURL, const PRUnichar* aMsg);
  NS_IMETHOD OnStopRequest(nsIURI* aURL, nsresult status, const PRUnichar* aMsg);
#endif

  // nsIWebShellContainer
  NS_IMETHOD WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsLoadType aReason);
  NS_IMETHOD BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL);
  NS_IMETHOD ProgressLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aProgress, PRInt32 aProgressMax);
  NS_IMETHOD EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsresult aStatus);
  NS_IMETHOD NewWebShell(PRUint32 aChromeMask,
                         PRBool aVisible,
                         nsIWebShell *&aNewWebShell);
  NS_IMETHOD ContentShellAdded(nsIWebShell* aChildShell, nsIContent* frameNode);
  NS_IMETHOD CreatePopup(nsIDOMElement* aElement, nsIDOMElement* aPopupContent, 
                         PRInt32 aXPos, PRInt32 aYPos, 
                         const nsString& aPopupType, const nsString& anAnchorAlignment,
                         const nsString& aPopupAlignment,
                         nsIDOMWindow* aWindow, nsIDOMWindow** outPopup);
  NS_IMETHOD FindWebShellWithName(const PRUnichar* aName, nsIWebShell*& aResult);
  NS_IMETHOD FocusAvailable(nsIWebShell* aFocusedWebShell, PRBool& aFocusTaken);

#ifndef NECKO
  // nsINetSupport
  NS_IMETHOD_(void) Alert(const nsString &aText);
  NS_IMETHOD_(PRBool) Confirm(const nsString &aText);
  NS_IMETHOD_(PRBool) Prompt(const nsString &aText,
                             const nsString &aDefault,
                             nsString &aResult);
  NS_IMETHOD_(PRBool) PromptUserAndPassword(const nsString &aText,
                                            nsString &aUser,
                                            nsString &aPassword);
  NS_IMETHOD_(PRBool) PromptPassword(const nsString &aText,
                                     nsString &aPassword);
#endif

  void Layout(PRInt32 aWidth, PRInt32 aHeight);

  void ForceRefresh();

  //nsEventStatus ProcessDialogEvent(nsGUIEvent *aEvent);


  void SetApp(nsViewerApp* aApp) {
    mApp = aApp;
  }

  // DOM Element & Node Interfaces
  NS_IMETHOD GetDocument(nsIDOMHTMLDocument *& aDocument);
  NS_IMETHOD AddEventListener(nsIDOMNode * aNode);
  NS_IMETHOD RemoveEventListener(nsIDOMNode * aNode);
  NS_IMETHOD AddWindowListener(nsIWindowListener * aWindowListener);

  // nsIDOMEventListener
  virtual nsresult HandleEvent(nsIDOMEvent* aEvent);

  // nsIDOMMouseListener (is derived from nsIDOMEventListener)
  virtual nsresult MouseDown(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseUp(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseOver(nsIDOMEvent* aMouseEvent);
  virtual nsresult MouseOut(nsIDOMEvent* aMouseEvent);

protected:
  void         GetContentRoot(); //Gets the Root Content node after Doc is loaded
  nsIContent * mContentRoot; // Points at the Root Content Node


protected:
  nsViewerApp* mApp;

  nsString     mTitle;
  nsString     mDialogURL;

  nsIWidget*   mWindow;
  nsIWebShell* mWebShell;

  nsIWindowListener * mWindowListener; // XXX Someday this will be a list
  PRBool       mDocIsLoaded;

  //for creating more instances
  nsIAppShell* mAppShell;       //not addref'ed!
  nsIPref*     mPrefs;          //not addref'ed!
  PRBool       mAllowPlugins;

  nsXPBaseWindowType mWindowType;

};

// XXX This is bad; because we can't hang a closure off of the event
// callback we have no way to store our This pointer; therefore we
// have to hunt to find the browswer that events belong too!!!

// aWhich for FindBrowserFor
#define FIND_WINDOW   0
#define FIND_BACK     1
#define FIND_FORWARD  2
#define FIND_LOCATION 3



#endif /* nsXPBaseWindow_h___ */
