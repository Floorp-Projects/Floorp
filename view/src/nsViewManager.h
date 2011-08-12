/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsViewManager_h___
#define nsViewManager_h___
#include "nsCOMPtr.h"
#include "nsIViewManager.h"
#include "nsCRT.h"
#include "nsITimer.h"
#include "prtime.h"
#include "prinrval.h"
#include "nsVoidArray.h"
#include "nsThreadUtils.h"
#include "nsView.h"
#include "nsIViewObserver.h"
#include "nsDeviceContext.h"


/**
   Invalidation model:

   1) Callers call into the view manager and ask it to update a view.
   
   2) The view manager finds the "right" widget for the view, henceforth called
      the root widget.

   3) The view manager traverses descendants of the root widget and for each
      one that needs invalidation either

      a)  Calls Invalidate() on the widget (no batching)
    or
      b)  Stores the rect to invalidate on the widget's view (batching)

   // XXXbz we want to change this a bit.  See bug 243726

   4) When batching, the call to end the batch either processes the pending
      Invalidate() calls on the widgets or posts an event to do so.

   It's important to note that widgets associated to views outside this view
   manager can end up being invalidated during step 3.  Therefore, the end of a
   view update batch really needs to traverse the entire view tree, to ensure
   that those invalidates happen.

   To cope with this, invalidate event processing and view update batch
   handling should only happen on the root viewmanager.  This means the root
   view manager is the only thing keeping track of mUpdateCnt.  As a result,
   Composite() calls should also be forwarded to the root view manager.
*/

class nsInvalidateEvent;

class nsViewManager : public nsIViewManager {
public:
  nsViewManager();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS

  NS_IMETHOD  Init(nsDeviceContext* aContext);

  NS_IMETHOD_(nsIView*) CreateView(const nsRect& aBounds,
                                   const nsIView* aParent,
                                   nsViewVisibility aVisibilityFlag = nsViewVisibility_kShow);

  NS_IMETHOD_(nsIView*) GetRootView();
  NS_IMETHOD  SetRootView(nsIView *aView);

  NS_IMETHOD  GetWindowDimensions(nscoord *width, nscoord *height);
  NS_IMETHOD  SetWindowDimensions(nscoord width, nscoord height);
  NS_IMETHOD  FlushDelayedResize(PRBool aDoReflow);

  NS_IMETHOD  Composite(void);

  NS_IMETHOD  UpdateView(nsIView *aView, PRUint32 aUpdateFlags);
  NS_IMETHOD  UpdateViewNoSuppression(nsIView *aView, const nsRect &aRect,
                                      PRUint32 aUpdateFlags);
  NS_IMETHOD  UpdateAllViews(PRUint32 aUpdateFlags);

  NS_IMETHOD  DispatchEvent(nsGUIEvent *aEvent,
      nsIView* aTargetView, nsEventStatus* aStatus);

  NS_IMETHOD  InsertChild(nsIView *parent, nsIView *child, nsIView *sibling,
                          PRBool above);

  NS_IMETHOD  InsertChild(nsIView *parent, nsIView *child,
                          PRInt32 zindex);

  NS_IMETHOD  RemoveChild(nsIView *parent);

  NS_IMETHOD  MoveViewTo(nsIView *aView, nscoord aX, nscoord aY);

  NS_IMETHOD  ResizeView(nsIView *aView, const nsRect &aRect, PRBool aRepaintExposedAreaOnly = PR_FALSE);

  NS_IMETHOD  SetViewFloating(nsIView *aView, PRBool aFloating);

  NS_IMETHOD  SetViewVisibility(nsIView *aView, nsViewVisibility aVisible);

  NS_IMETHOD  SetViewZIndex(nsIView *aView, PRBool aAuto, PRInt32 aZIndex, PRBool aTopMost=PR_FALSE);

  virtual void SetViewObserver(nsIViewObserver *aObserver) { mObserver = aObserver; }
  virtual nsIViewObserver* GetViewObserver() { return mObserver; }

  NS_IMETHOD  GetDeviceContext(nsDeviceContext *&aContext);

  virtual nsIViewManager* BeginUpdateViewBatch(void);
  NS_IMETHOD  EndUpdateViewBatch(PRUint32 aUpdateFlags);

  NS_IMETHOD GetRootWidget(nsIWidget **aWidget);
  NS_IMETHOD ForceUpdate();
 
  NS_IMETHOD IsPainting(PRBool& aIsPainting);
  NS_IMETHOD GetLastUserEventTime(PRUint32& aTime);
  void ProcessInvalidateEvent();
  static PRUint32 gLastUserEventTime;

  /* Update the cached RootViewManager pointer on this view manager. */
  void InvalidateHierarchy();

protected:
  virtual ~nsViewManager();

private:

  void FlushPendingInvalidates();
  void ProcessPendingUpdates(nsView *aView, PRBool aDoInvalidate);
  /**
   * Call WillPaint() on all view observers under this vm root.
   */
  void CallWillPaintOnObservers(PRBool aWillSendDidPaint);
  void CallDidPaintOnObservers();
  void ReparentChildWidgets(nsIView* aView, nsIWidget *aNewWidget);
  void ReparentWidgets(nsIView* aView, nsIView *aParent);
  void UpdateWidgetArea(nsView *aWidgetView, nsIWidget* aWidget,
                        const nsRegion &aDamagedRegion,
                        nsView* aIgnoreWidgetView);

  void UpdateViews(nsView *aView, PRUint32 aUpdateFlags);

  void TriggerRefresh(PRUint32 aUpdateFlags);

  // aView is the view for aWidget and aRegion is relative to aWidget.
  void Refresh(nsView *aView, nsIWidget *aWidget,
               const nsIntRegion& aRegion, PRUint32 aUpdateFlags);
  // aRootView is the view for aWidget, aRegion is relative to aRootView, and
  // aIntRegion is relative to aWidget.
  void RenderViews(nsView *aRootView, nsIWidget *aWidget,
                   const nsRegion& aRegion, const nsIntRegion& aIntRegion,
                   PRBool aPaintDefaultBackground, PRBool aWillSendDidPaint);

  void InvalidateRectDifference(nsView *aView, const nsRect& aRect, const nsRect& aCutOut, PRUint32 aUpdateFlags);
  void InvalidateHorizontalBandDifference(nsView *aView, const nsRect& aRect, const nsRect& aCutOut,
                                          PRUint32 aUpdateFlags, nscoord aY1, nscoord aY2, PRBool aInCutOut);

  // Utilities

  PRBool IsViewInserted(nsView *aView);

  /**
   * Function to recursively call Update() on all widgets belonging to
   * a view or its kids.
   */
  void UpdateWidgetsForView(nsView* aView);

  /**
   * Intersects aRect with aView's bounds and then transforms it from aView's
   * coordinate system to the coordinate system of the widget attached to
   * aView.
   */
  nsIntRect ViewToWidget(nsView *aView, const nsRect &aRect) const;

  void DoSetWindowDimensions(nscoord aWidth, nscoord aHeight);

  // Safety helpers
  void IncrementUpdateCount() {
    NS_ASSERTION(IsRootVM(),
                 "IncrementUpdateCount called on non-root viewmanager");
    ++mUpdateCnt;
  }

  void DecrementUpdateCount() {
    NS_ASSERTION(IsRootVM(),
                 "DecrementUpdateCount called on non-root viewmanager");
    --mUpdateCnt;
  }

  PRInt32 UpdateCount() const {
    NS_ASSERTION(IsRootVM(),
                 "DecrementUpdateCount called on non-root viewmanager");
    return mUpdateCnt;
  }

  void ClearUpdateCount() {
    NS_ASSERTION(IsRootVM(),
                 "DecrementUpdateCount called on non-root viewmanager");
    mUpdateCnt = 0;
  }

  PRBool IsPainting() const {
    return RootViewManager()->mPainting;
  }

  void SetPainting(PRBool aPainting) {
    RootViewManager()->mPainting = aPainting;
  }

  nsresult UpdateView(nsIView *aView, const nsRect &aRect, PRUint32 aUpdateFlags);

public: // NOT in nsIViewManager, so private to the view module
  nsView* GetRootViewImpl() const { return mRootView; }
  nsViewManager* RootViewManager() const { return mRootViewManager; }
  PRBool IsRootVM() const { return this == RootViewManager(); }

  nsEventStatus HandleEvent(nsView* aView, nsGUIEvent* aEvent);

  PRBool IsRefreshEnabled() { return RootViewManager()->mUpdateBatchCnt == 0; }

  // Call this when you need to let the viewmanager know that it now has
  // pending updates.
  void PostPendingUpdate() { RootViewManager()->mHasPendingUpdates = PR_TRUE; }

  PRUint32 AppUnitsPerDevPixel() const
  {
    return mContext->AppUnitsPerDevPixel();
  }

private:
  nsRefPtr<nsDeviceContext> mContext;
  nsIViewObserver   *mObserver;

  // The size for a resize that we delayed until the root view becomes
  // visible again.
  nsSize            mDelayedResize;

  nsView            *mRootView;
  // mRootViewManager is a strong ref unless it equals |this|.  It's
  // never null (if we have no ancestors, it will be |this|).
  nsViewManager     *mRootViewManager;

  nsRevocableEventPtr<nsInvalidateEvent> mInvalidateEvent;

  // The following members should not be accessed directly except by
  // the root view manager.  Some have accessor functions to enforce
  // this, as noted.
  
  // Use IncrementUpdateCount(), DecrementUpdateCount(), UpdateCount(),
  // ClearUpdateCount() on the root viewmanager to access mUpdateCnt.
  PRInt32           mUpdateCnt;
  PRInt32           mUpdateBatchCnt;
  PRUint32          mUpdateBatchFlags;
  // Use IsPainting() and SetPainting() to access mPainting.
  PRPackedBool      mPainting;
  PRPackedBool      mRecursiveRefreshPending;
  PRPackedBool      mHasPendingUpdates;
  PRPackedBool      mInScroll;

  //from here to public should be static and locked... MMP
  static PRInt32           mVMCount;        //number of viewmanagers

  //list of view managers
  static nsVoidArray       *gViewManagers;

  void PostInvalidateEvent();
};

//when the refresh happens, should it be double buffered?
#define NS_VMREFRESH_DOUBLE_BUFFER      0x0001

class nsInvalidateEvent : public nsRunnable {
public:
  nsInvalidateEvent(class nsViewManager *vm) : mViewManager(vm) {
    NS_ASSERTION(mViewManager, "null parameter");
  }
  void Revoke() { mViewManager = nsnull; }

  NS_IMETHOD Run() {
    if (mViewManager)
      mViewManager->ProcessInvalidateEvent();
    return NS_OK;
  }
protected:
  class nsViewManager *mViewManager;
};

#endif /* nsViewManager_h___ */
