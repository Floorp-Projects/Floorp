/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 */

#ifndef nsXULWindow_h__
#define nsXULWindow_h__

// Local Includes
#include "nsChromeTreeOwner.h"
#include "nsContentTreeOwner.h"

// Helper classes
#include "nsCOMPtr.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsWeakReference.h"

// Interfaces needed
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMWindowInternal.h"
#include "nsIEventQueueService.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIWidget.h"
#include "nsIXULWindow.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsGUIEvent.h"

// nsXULWindow

class nsXULWindow : public nsIXULWindow, public nsIBaseWindow, 
   public nsIInterfaceRequestor,
   public nsSupportsWeakReference
{
friend class nsChromeTreeOwner;
friend class nsContentTreeOwner;

public:
   NS_DECL_ISUPPORTS

   NS_DECL_NSIINTERFACEREQUESTOR
   NS_DECL_NSIXULWINDOW
   NS_DECL_NSIBASEWINDOW

protected:
   nsXULWindow();
   virtual ~nsXULWindow();

   NS_IMETHOD EnsureChromeTreeOwner();
   NS_IMETHOD EnsureContentTreeOwner();
   NS_IMETHOD EnsurePrimaryContentTreeOwner();
   NS_IMETHOD EnsurePrompter();
   NS_IMETHOD EnsureAuthPrompter();
   
   void OnChromeLoaded();
   void StaggerPosition(PRInt32 &aRequestedX, PRInt32 &aRequestedY,
                        PRInt32 aSpecWidth, PRInt32 aSpecHeight);
   NS_IMETHOD LoadPositionAndSizeFromXUL(PRBool aPosition, PRBool aSize);
   NS_IMETHOD LoadTitleFromXUL();
   NS_IMETHOD LoadIconFromXUL();
   NS_IMETHOD PersistPositionAndSize(PRBool aPosition, PRBool aSize, PRBool aSizeMode);

   NS_IMETHOD GetWindowDOMWindow(nsIDOMWindowInternal** aDOMWindow);
   NS_IMETHOD GetWindowDOMElement(nsIDOMElement** aDOMElement);
   NS_IMETHOD GetDOMElementById(char* aID, nsIDOMElement** aDOMElement);
   NS_IMETHOD ContentShellAdded(nsIDocShellTreeItem* aContentShell,
      PRBool aPrimary, const PRUnichar* aID);
   NS_IMETHOD SizeShellTo(nsIDocShellTreeItem* aShellItem, PRInt32 aCX, 
      PRInt32 aCY);
   NS_IMETHOD ExitModalLoop(nsresult aStatus);
   NS_IMETHOD GetNewWindow(PRInt32 aChromeFlags, 
      nsIDocShellTreeItem** aDocShellTreeItem);
   NS_IMETHOD CreateNewChromeWindow(PRInt32 aChromeFlags,
      nsIDocShellTreeItem** aDocShellTreeItem);
   NS_IMETHOD CreateNewContentWindow(PRInt32 aChromeFlags,
      nsIDocShellTreeItem** aDocShellTreeItem);
   NS_IMETHOD NotifyObservers(const PRUnichar* aTopic, const PRUnichar* aData);

   void EnableParent(PRBool aEnable);
   void ActivateParent();
   PRBool ConstrainToZLevel(PRBool aImmediate, nsWindowZ *aPlacement,
            nsIWidget *aReqBelow, nsIWidget **aActualBelow);
   void                    SetContentScrollbarVisibility(PRBool aVisible);

   nsChromeTreeOwner*      mChromeTreeOwner;
   nsContentTreeOwner*     mContentTreeOwner;
   nsContentTreeOwner*     mPrimaryContentTreeOwner;
   nsCOMPtr<nsIWidget>     mWindow;
   nsCOMPtr<nsIDocShell>   mDocShell;
   nsCOMPtr<nsIDOMWindowInternal>  mDOMWindow;
   nsCOMPtr<nsIWeakReference> mParentWindow;
   nsCOMPtr<nsIPrompt>     mPrompter;
   nsCOMPtr<nsIAuthPrompt> mAuthPrompter;
   nsVoidArray             mContentShells;
   PRBool                  mContinueModalLoop;
   nsresult                mModalStatus;
   PRBool                  mDebuting;       // being made visible right now
   PRBool                  mChromeLoaded; // True when chrome has loaded
   PRBool                  mShowAfterLoad;
   PRBool                  mIntrinsicallySized; 
   PRBool                  mCenterAfterLoad;
   PRBool                  mIsHiddenWindow;
   PRBool                  mHadChildWindow;
   unsigned long           mZlevel;
};

// nsContentShellInfo

class nsContentShellInfo
{
public:
   nsContentShellInfo(const nsAString& aID,
                      PRBool aPrimary,
                      nsIDocShellTreeItem* aContentShell);
   ~nsContentShellInfo();

public:
   nsAutoString                  id;   // The identifier of the content shell
   PRBool                        primary; // Signals the fact that the shell is primary
   nsCOMPtr<nsIDocShellTreeItem> child; // content shell
};

// nsEventQueueStack
// a little utility object to push an event queue and pop it when it
// goes out of scope. should probably be in a file of utility functions.
class nsEventQueueStack
{
public:
   nsEventQueueStack();
   ~nsEventQueueStack();

   nsresult Success();

protected:
   nsCOMPtr<nsIEventQueueService>   mService;
   nsCOMPtr<nsIEventQueue>          mQueue;
};

#endif /* nsXULWindow_h__ */
