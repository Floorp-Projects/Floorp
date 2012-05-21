/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXULWindow_h__
#define nsXULWindow_h__

// Local Includes
#include "nsChromeTreeOwner.h"
#include "nsContentTreeOwner.h"

// Helper classes
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsWeakReference.h"
#include "nsCOMArray.h"

// Interfaces needed
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIXULWindow.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsGUIEvent.h"
#include "nsIXULBrowserWindow.h"
#include "nsIWeakReference.h"

// nsXULWindow

#define NS_XULWINDOW_IMPL_CID                         \
{ /* 8eaec2f3-ed02-4be2-8e0f-342798477298 */          \
     0x8eaec2f3,                                      \
     0xed02,                                          \
     0x4be2,                                          \
   { 0x8e, 0x0f, 0x34, 0x27, 0x98, 0x47, 0x72, 0x98 } \
}

class nsContentShellInfo;

class nsXULWindow : public nsIBaseWindow,
                    public nsIInterfaceRequestor,
                    public nsIXULWindow,
                    public nsSupportsWeakReference
{
friend class nsChromeTreeOwner;
friend class nsContentTreeOwner;

public:
   NS_DECL_ISUPPORTS

   NS_DECL_NSIINTERFACEREQUESTOR
   NS_DECL_NSIXULWINDOW
   NS_DECL_NSIBASEWINDOW

   NS_DECLARE_STATIC_IID_ACCESSOR(NS_XULWINDOW_IMPL_CID)

   void LockUntilChromeLoad() { mLockedUntilChromeLoad = true; }
   bool IsLocked() const { return mLockedUntilChromeLoad; }
   void IgnoreXULSizeMode(bool aEnable) { mIgnoreXULSizeMode = aEnable; }

protected:
   enum persistentAttributes {
     PAD_MISC =         0x1,
     PAD_POSITION =     0x2,
     PAD_SIZE =         0x4
   };

   nsXULWindow(PRUint32 aChromeFlags);
   virtual ~nsXULWindow();

   NS_IMETHOD EnsureChromeTreeOwner();
   NS_IMETHOD EnsureContentTreeOwner();
   NS_IMETHOD EnsurePrimaryContentTreeOwner();
   NS_IMETHOD EnsurePrompter();
   NS_IMETHOD EnsureAuthPrompter();
   
   void OnChromeLoaded();
   void StaggerPosition(PRInt32 &aRequestedX, PRInt32 &aRequestedY,
                        PRInt32 aSpecWidth, PRInt32 aSpecHeight);
   bool       LoadPositionFromXUL();
   bool       LoadSizeFromXUL();
   bool       LoadMiscPersistentAttributesFromXUL();
   void       SyncAttributesToWidget();
   NS_IMETHOD SavePersistentAttributes();

   NS_IMETHOD GetWindowDOMWindow(nsIDOMWindow** aDOMWindow);
   NS_IMETHOD GetWindowDOMElement(nsIDOMElement** aDOMElement);

   // See nsIDocShellTreeOwner for docs on next two methods
   NS_HIDDEN_(nsresult) ContentShellAdded(nsIDocShellTreeItem* aContentShell,
                                          bool aPrimary, bool aTargetable,
                                          const nsAString& aID);
   NS_HIDDEN_(nsresult) ContentShellRemoved(nsIDocShellTreeItem* aContentShell);
   NS_IMETHOD SizeShellTo(nsIDocShellTreeItem* aShellItem, PRInt32 aCX, 
      PRInt32 aCY);
   NS_IMETHOD ExitModalLoop(nsresult aStatus);
   NS_IMETHOD CreateNewChromeWindow(PRInt32 aChromeFlags, nsIXULWindow **_retval);
   NS_IMETHOD CreateNewContentWindow(PRInt32 aChromeFlags, nsIXULWindow **_retval);

   void       EnableParent(bool aEnable);
   bool       ConstrainToZLevel(bool aImmediate, nsWindowZ *aPlacement,
                                nsIWidget *aReqBelow, nsIWidget **aActualBelow);
   void       PlaceWindowLayersBehind(PRUint32 aLowLevel, PRUint32 aHighLevel,
                                      nsIXULWindow *aBehind);
   void       SetContentScrollbarVisibility(bool aVisible);
   bool       GetContentScrollbarVisibility();
   void       PersistentAttributesDirty(PRUint32 aDirtyFlags);
   PRUint32   AppUnitsPerDevPixel();

   nsChromeTreeOwner*      mChromeTreeOwner;
   nsContentTreeOwner*     mContentTreeOwner;
   nsContentTreeOwner*     mPrimaryContentTreeOwner;
   nsCOMPtr<nsIWidget>     mWindow;
   nsCOMPtr<nsIDocShell>   mDocShell;
   nsCOMPtr<nsIDOMWindow>  mDOMWindow;
   nsCOMPtr<nsIWeakReference> mParentWindow;
   nsCOMPtr<nsIPrompt>     mPrompter;
   nsCOMPtr<nsIAuthPrompt> mAuthPrompter;
   nsCOMPtr<nsIXULBrowserWindow> mXULBrowserWindow;
   nsCOMPtr<nsIDocShellTreeItem> mPrimaryContentShell;
   nsTArray<nsContentShellInfo*> mContentShells; // array of doc shells by id
   nsresult                mModalStatus;
   bool                    mContinueModalLoop;
   bool                    mDebuting;       // being made visible right now
   bool                    mChromeLoaded; // True when chrome has loaded
   bool                    mShowAfterLoad;
   bool                    mIntrinsicallySized; 
   bool                    mCenterAfterLoad;
   bool                    mIsHiddenWindow;
   bool                    mLockedUntilChromeLoad;
   bool                    mIgnoreXULSize;
   bool                    mIgnoreXULPosition;
   bool                    mChromeFlagsFrozen;
   bool                    mIgnoreXULSizeMode;
   PRUint32                mContextFlags;
   PRUint32                mPersistentAttributesDirty; // persistentAttributes
   PRUint32                mPersistentAttributesMask;
   PRUint32                mChromeFlags;
   PRUint32                mAppPerDev; // sometimes needed when we can't get
                                       // it from the widget
   nsString                mTitle;
   nsIntRect               mOpenerScreenRect; // the screen rect of the opener

   nsCOMArray<nsIWeakReference> mTargetableShells; // targetable shells only
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsXULWindow, NS_XULWINDOW_IMPL_CID)

// nsContentShellInfo
// Used to map shell IDs to nsIDocShellTreeItems.

class nsContentShellInfo
{
public:
   nsContentShellInfo(const nsAString& aID,
                      nsIWeakReference* aContentShell);
   ~nsContentShellInfo();

public:
   nsAutoString id; // The identifier of the content shell
   nsWeakPtr child; // content shell (weak reference to nsIDocShellTreeItem)
};

#endif /* nsXULWindow_h__ */
