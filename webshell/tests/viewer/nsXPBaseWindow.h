/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsXPBaseWindow_h___
#define nsXPBaseWindow_h___

#include "nsIXPBaseWindow.h"
#include "nsIStreamListener.h"
#include "nsIWebShell.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsCRT.h"

#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMMouseListener.h"
#include "nsIWidget.h"

class nsViewerApp;
class nsIPresShell;
class nsIPref;

/**
 * 
 */
class nsXPBaseWindow : public nsIXPBaseWindow,
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

  // nsIWebShellContainer
  NS_IMETHOD WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsLoadType aReason);
  NS_IMETHOD BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL);
  NS_IMETHOD ProgressLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aProgress, PRInt32 aProgressMax);
  NS_IMETHOD EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsresult aStatus);
  NS_IMETHOD NewWebShell(PRUint32 aChromeMask,
                         PRBool aVisible,
                         nsIWebShell *&aNewWebShell);
  NS_IMETHOD ContentShellAdded(nsIWebShell* aChildShell, nsIContent* frameNode);
  NS_IMETHOD FindWebShellWithName(const PRUnichar* aName, nsIWebShell*& aResult);
  NS_IMETHOD FocusAvailable(nsIWebShell* aFocusedWebShell, PRBool& aFocusTaken);

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
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

  // nsIDOMMouseListener (is derived from nsIDOMEventListener)
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent);

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
