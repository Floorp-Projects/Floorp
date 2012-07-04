/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "nsIPresShell.h"
#include "nsDeviceContext.h"


/**
   Invalidation model:

   1) Callers call into the view manager and ask it to invalidate a view.
   
   2) The view manager finds the "right" widget for the view, henceforth called
      the root widget.

   3) The view manager traverses descendants of the root widget and for each
      one that needs invalidation stores the rect to invalidate on the widget's
      view (batching).

   4) The dirty region is flushed to the right widget when
      ProcessPendingUpdates is called from the RefreshDriver.

   It's important to note that widgets associated to views outside this view
   manager can end up being invalidated during step 3.  Therefore, the end of a
   view update batch really needs to traverse the entire view tree, to ensure
   that those invalidates happen.

   To cope with this, invalidation processing and should only happen on the
   root viewmanager.
*/

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
  NS_IMETHOD  FlushDelayedResize(bool aDoReflow);

  NS_IMETHOD  InvalidateView(nsIView *aView);
  NS_IMETHOD  InvalidateViewNoSuppression(nsIView *aView, const nsRect &aRect);
  NS_IMETHOD  InvalidateAllViews();

  NS_IMETHOD  DispatchEvent(nsGUIEvent *aEvent,
      nsIView* aTargetView, nsEventStatus* aStatus);

  NS_IMETHOD  InsertChild(nsIView *parent, nsIView *child, nsIView *sibling,
                          bool above);

  NS_IMETHOD  InsertChild(nsIView *parent, nsIView *child,
                          PRInt32 zindex);

  NS_IMETHOD  RemoveChild(nsIView *parent);

  NS_IMETHOD  MoveViewTo(nsIView *aView, nscoord aX, nscoord aY);

  NS_IMETHOD  ResizeView(nsIView *aView, const nsRect &aRect, bool aRepaintExposedAreaOnly = false);

  NS_IMETHOD  SetViewFloating(nsIView *aView, bool aFloating);

  NS_IMETHOD  SetViewVisibility(nsIView *aView, nsViewVisibility aVisible);

  NS_IMETHOD  SetViewZIndex(nsIView *aView, bool aAuto, PRInt32 aZIndex, bool aTopMost=false);

  virtual void SetPresShell(nsIPresShell *aPresShell) { mPresShell = aPresShell; }
  virtual nsIPresShell* GetPresShell() { return mPresShell; }

  NS_IMETHOD  GetDeviceContext(nsDeviceContext *&aContext);

  virtual nsIViewManager* IncrementDisableRefreshCount();
  virtual void DecrementDisableRefreshCount();

  NS_IMETHOD GetRootWidget(nsIWidget **aWidget);
 
  NS_IMETHOD IsPainting(bool& aIsPainting);
  NS_IMETHOD GetLastUserEventTime(PRUint32& aTime);
  static PRUint32 gLastUserEventTime;

  /* Update the cached RootViewManager pointer on this view manager. */
  void InvalidateHierarchy();

  virtual void ProcessPendingUpdates();
  virtual void UpdateWidgetGeometry();

protected:
  virtual ~nsViewManager();

private:

  void FlushPendingInvalidates();
  void ProcessPendingUpdatesForView(nsView *aView,
                                    bool aFlushDirtyRegion = true);
  void FlushDirtyRegionToWidget(nsView* aView);
  /**
   * Call WillPaint() on all view observers under this vm root.
   */
  void CallWillPaintOnObservers(bool aWillSendDidPaint);
  void CallDidPaintOnObserver();
  void ReparentChildWidgets(nsIView* aView, nsIWidget *aNewWidget);
  void ReparentWidgets(nsIView* aView, nsIView *aParent);
  void InvalidateWidgetArea(nsView *aWidgetView, const nsRegion &aDamagedRegion);

  void InvalidateViews(nsView *aView);

  // aView is the view for aWidget and aRegion is relative to aWidget.
  void Refresh(nsView *aView, nsIWidget *aWidget, const nsIntRegion& aRegion,
               bool aWillSendDidPaint);

  void InvalidateRectDifference(nsView *aView, const nsRect& aRect, const nsRect& aCutOut);
  void InvalidateHorizontalBandDifference(nsView *aView, const nsRect& aRect, const nsRect& aCutOut,
                                          nscoord aY1, nscoord aY2, bool aInCutOut);

  // Utilities

  bool IsViewInserted(nsView *aView);

  /**
   * Intersects aRect with aView's bounds and then transforms it from aView's
   * coordinate system to the coordinate system of the widget attached to
   * aView.
   */
  nsIntRect ViewToWidget(nsView *aView, const nsRect &aRect) const;

  void DoSetWindowDimensions(nscoord aWidth, nscoord aHeight);

  bool IsPainting() const {
    return RootViewManager()->mPainting;
  }

  void SetPainting(bool aPainting) {
    RootViewManager()->mPainting = aPainting;
  }

  nsresult InvalidateView(nsIView *aView, const nsRect &aRect);

public: // NOT in nsIViewManager, so private to the view module
  nsView* GetRootViewImpl() const { return mRootView; }
  nsViewManager* RootViewManager() const { return mRootViewManager; }
  bool IsRootVM() const { return this == RootViewManager(); }

  // Whether synchronous painting is allowed at the moment. For example,
  // widget geometry changes can cause synchronous painting, so they need to
  // be deferred while refresh is disabled.
  bool IsPaintingAllowed() { return RootViewManager()->mRefreshDisableCount == 0; }

  // Call this when you need to let the viewmanager know that it now has
  // pending updates.
  void PostPendingUpdate();

  PRUint32 AppUnitsPerDevPixel() const
  {
    return mContext->AppUnitsPerDevPixel();
  }

private:
  nsRefPtr<nsDeviceContext> mContext;
  nsIPresShell   *mPresShell;

  // The size for a resize that we delayed until the root view becomes
  // visible again.
  nsSize            mDelayedResize;

  nsView            *mRootView;
  // mRootViewManager is a strong ref unless it equals |this|.  It's
  // never null (if we have no ancestors, it will be |this|).
  nsViewManager     *mRootViewManager;

  // The following members should not be accessed directly except by
  // the root view manager.  Some have accessor functions to enforce
  // this, as noted.
  
  PRInt32           mRefreshDisableCount;
  // Use IsPainting() and SetPainting() to access mPainting.
  bool              mPainting;
  bool              mRecursiveRefreshPending;
  bool              mHasPendingUpdates;
  bool              mHasPendingWidgetGeometryChanges;
  bool              mInScroll;

  //from here to public should be static and locked... MMP
  static PRInt32           mVMCount;        //number of viewmanagers

  //list of view managers
  static nsVoidArray       *gViewManagers;
};

#endif /* nsViewManager_h___ */
