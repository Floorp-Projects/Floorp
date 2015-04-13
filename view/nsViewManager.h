/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsViewManager_h___
#define nsViewManager_h___

#include "nscore.h"
#include "nsView.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsTArray.h"
#include "nsDeviceContext.h"
#include "nsTArray.h"
#include "mozilla/EventForwards.h"

class nsIWidget;
struct nsRect;
class nsRegion;
class nsDeviceContext;
class nsIPresShell;

class nsViewManager final
{
  ~nsViewManager();
public:
  friend class nsView;

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_INLINE_DECL_REFCOUNTING(nsViewManager)

  nsViewManager();

  /**
   * Initialize the ViewManager
   * Note: this instance does not hold a reference to the presshell
   * because it holds a reference to this instance.
   * @result The result of the initialization, NS_OK if no errors
   */
  nsresult Init(nsDeviceContext* aContext);

  /**
   * Create an ordinary view
   * @param aBounds initial bounds for view
   *        XXX We should eliminate this parameter; you can set the bounds after CreateView
   * @param aParent intended parent for view. this is not actually set in the
   *        nsView through this method. it is only used by the initialization
   *        code to walk up the view tree, if necessary, to find resources.
   *        XXX We should eliminate this parameter!
   * @param aVisibilityFlag initial visibility state of view
   *        XXX We should eliminate this parameter; you can set it after CreateView
   * @result The new view.  Never null.
   */
  nsView* CreateView(const nsRect& aBounds,
                     nsView* aParent,
                     nsViewVisibility aVisibilityFlag = nsViewVisibility_kShow);

  /**
   * Get the root of the view tree.
   * @result the root view
   */
  nsView* GetRootView() { return mRootView; }

  /**
   * Set the root of the view tree. Does not destroy the current root view.
   * aView may have a parent view managed by a different view manager.
   * aView may have a widget (anything but printing) or may not (printing).
   * @param aView view to set as root
   */
  void SetRootView(nsView *aView);

  /**
   * Get the dimensions of the root window. The dimensions are in
   * twips
   * @param aWidth out parameter for width of window in twips
   * @param aHeight out parameter for height of window in twips
   */
  void GetWindowDimensions(nscoord *aWidth, nscoord *aHeight);

  /**
   * Set the dimensions of the root window.
   * Called if the root window is resized. The dimensions are in
   * twips
   * @param aWidth of window in twips
   * @param aHeight of window in twips
   */
  void SetWindowDimensions(nscoord aWidth, nscoord aHeight);

  /**
   * Do any resizes that are pending.
   */
  void FlushDelayedResize(bool aDoReflow);

  /**
   * Called to inform the view manager that the entire area of a view
   * is dirty and needs to be redrawn.
   * @param aView view to paint. should be root view
   */
  void InvalidateView(nsView *aView);

  /**
   * Called to inform the view manager that some portion of a view is dirty and
   * needs to be redrawn. The rect passed in should be in the view's coordinate
   * space. Does not check for paint suppression.
   * @param aView view to paint. should be root view
   * @param rect rect to mark as damaged
   */
  void InvalidateViewNoSuppression(nsView *aView, const nsRect &aRect);

  /**
   * Called to inform the view manager that it should invalidate all views.
   */
  void InvalidateAllViews();

  /**
   * Called to dispatch an event to the appropriate view. Often called
   * as a result of receiving a mouse or keyboard event from the widget
   * event system.
   * @param aEvent event to dispatch
   * @param aViewTarget dispatch the event to this view
   * @param aStatus event handling status
   */
  void DispatchEvent(mozilla::WidgetGUIEvent *aEvent,
                     nsView* aViewTarget,
                     nsEventStatus* aStatus);

  /**
   * Given a parent view, insert another view as its child.
   * aSibling and aAbove control the "document order" for the insertion.
   * If aSibling is null, the view is inserted at the end of the document order
   * if aAfter is true, otherwise it is inserted at the beginning.
   * If aSibling is non-null, then if aAfter is true, the view is inserted
   * after the sibling in document order (appearing above the sibling unless
   * overriden by z-order).
   * If it is false, the view is inserted before the sibling.
   * The view manager generates the appopriate dirty regions.
   * @param aParent parent view
   * @param aChild child view
   * @param aSibling sibling view
   * @param aAfter after or before in the document order
   */
  void InsertChild(nsView *aParent, nsView *aChild, nsView *aSibling,
                   bool aAfter);

  void InsertChild(nsView *aParent, nsView *aChild, int32_t aZIndex);

  /**
   * Remove a specific child view from its parent. This will NOT remove its placeholder
   * if there is one.
   * The view manager generates the appropriate dirty regions.
   * @param aParent parent view
   * @param aChild child view
   */
  void RemoveChild(nsView *aChild);

  /**
   * Move a view to the specified position, provided in parent coordinates.
   * The new position is the (0, 0) origin for the view's coordinate system.
   * The view's bounds may extend above or to the left of this point.
   * The view manager generates the appropriate dirty regions.
   * @param aView view to move
   * @param aX x value for new view position
   * @param aY y value for new view position
   */
  void MoveViewTo(nsView *aView, nscoord aX, nscoord aY);

  /**
   * Resize a view. In addition to setting the width and height, you can
   * set the x and y of its bounds relative to its position. Negative x and y
   * will let the view extend above and to the left of the (0,0) point in its
   * coordinate system.
   * The view manager generates the appropriate dirty regions.
   * @param aView view to move
   * @param the new bounds relative to the current position
   * @param RepaintExposedAreaOnly
   *     if true Repaint only the expanded or contracted region,
   *     if false Repaint the union of the old and new rectangles.
   */
  void ResizeView(nsView *aView, const nsRect &aRect,
                  bool aRepaintExposedAreaOnly = false);

  /**
   * Set the visibility of a view. Hidden views have the effect of hiding
   * their descendants as well. This does not affect painting, so layout
   * is responsible for ensuring that content in hidden views is not
   * painted nor handling events. It does affect the visibility of widgets;
   * if a view is hidden, descendant views with widgets have their widgets
   * hidden.
   * The view manager generates the appropriate dirty regions.
   * @param aView view to change visibility state of
   * @param visible new visibility state
   */
  void SetViewVisibility(nsView *aView, nsViewVisibility aVisible);

  /**
   * Set the z-index of a view. Positive z-indices mean that a view
   * is above its parent in z-order. Negative z-indices mean that a
   * view is below its parent.
   * The view manager generates the appropriate dirty regions.
   * @param aAutoZIndex indicate that the z-index of a view is "auto". An "auto" z-index
   * means that the view does not define a new stacking context,
   * which means that the z-indicies of the view's children are
   * relative to the view's siblings.
   * @param aView view to change z depth of
   * @param aZindex explicit z depth
   */
  void SetViewZIndex(nsView *aView, bool aAutoZIndex, int32_t aZindex);

  /**
   * Set whether the view "floats" above all other views,
   * which tells the compositor not to consider higher views in
   * the view hierarchy that would geometrically intersect with
   * this view. This is a hack, but it fixes some problems with
   * views that need to be drawn in front of all other views.
   */
  void SetViewFloating(nsView *aView, bool aFloatingView);

  /**
   * Set the presshell associated with this manager
   * @param aPresShell - new presshell
   */
  void SetPresShell(nsIPresShell *aPresShell) { mPresShell = aPresShell; }

  /**
   * Get the pres shell associated with this manager
   */
  nsIPresShell* GetPresShell() { return mPresShell; }

  /**
   * Get the device context associated with this manager
   */
  nsDeviceContext* GetDeviceContext() const
  {
    return mContext;
  }

  /**
   * A stack class for disallowing changes that would enter painting. For
   * example, popup widgets shouldn't be resized during reflow, since doing so
   * might cause synchronous painting inside reflow which is forbidden.
   * While refresh is disabled, widget geometry changes are deferred and will
   * be handled later, either from the refresh driver or from an NS_WILL_PAINT
   * event.
   * We don't want to defer widget geometry changes all the time. Resizing a
   * popup from script doesn't need to be deferred, for example, especially
   * since popup widget geometry is observable from script and expected to
   * update synchronously.
   */
  class MOZ_STACK_CLASS AutoDisableRefresh {
  public:
    explicit AutoDisableRefresh(nsViewManager* aVM) {
      if (aVM) {
        mRootVM = aVM->IncrementDisableRefreshCount();
      }
    }
    ~AutoDisableRefresh() {
      if (mRootVM) {
        mRootVM->DecrementDisableRefreshCount();
      }
    }
  private:
    AutoDisableRefresh(const AutoDisableRefresh& aOther);
    const AutoDisableRefresh& operator=(const AutoDisableRefresh& aOther);

    nsRefPtr<nsViewManager> mRootVM;
  };

private:
  friend class AutoDisableRefresh;

  nsViewManager* IncrementDisableRefreshCount();
  void DecrementDisableRefreshCount();

public:
  /**
   * Retrieve the widget at the root of the nearest enclosing
   * view manager whose root view has a widget.
   */
  void GetRootWidget(nsIWidget **aWidget);

  /**
   * Indicate whether the viewmanager is currently painting
   *
   * @param aPainting true if the viewmanager is painting
   *                  false otherwise
   */
  void IsPainting(bool& aIsPainting);

  /**
   * Retrieve the time of the last user event. User events
   * include mouse and keyboard events. The viewmanager
   * saves the time of the last user event.
   *
   * @param aTime Last user event time in microseconds
   */
  void GetLastUserEventTime(uint32_t& aTime);

  /**
   * Find the nearest display root view for the view aView. This is the view for
   * the nearest enclosing popup or the root view for the root document.
   */
  static nsView* GetDisplayRootFor(nsView* aView);

  /**
   * Flush the accumulated dirty region to the widget and update widget
   * geometry.
   */
  void ProcessPendingUpdates();

  /**
   * Just update widget geometry without flushing the dirty region
   */
  void UpdateWidgetGeometry();

  int32_t AppUnitsPerDevPixel() const
  {
    return mContext->AppUnitsPerDevPixel();
  }

private:
  static uint32_t gLastUserEventTime;

  /* Update the cached RootViewManager pointer on this view manager. */
  void InvalidateHierarchy();
  void FlushPendingInvalidates();

  void ProcessPendingUpdatesForView(nsView *aView,
                                    bool aFlushDirtyRegion = true);
  void ProcessPendingUpdatesRecurse(nsView* aView,
                                    nsTArray<nsCOMPtr<nsIWidget> >& aWidgets);
  void ProcessPendingUpdatesPaint(nsIWidget* aWidget);

  void FlushDirtyRegionToWidget(nsView* aView);
  /**
   * Call WillPaint() on all view observers under this vm root.
   */
  void CallWillPaintOnObservers();
  void ReparentChildWidgets(nsView* aView, nsIWidget *aNewWidget);
  void ReparentWidgets(nsView* aView, nsView *aParent);
  void InvalidateWidgetArea(nsView *aWidgetView, const nsRegion &aDamagedRegion);

  void InvalidateViews(nsView *aView);

  // aView is the view for aWidget and aRegion is relative to aWidget.
  void Refresh(nsView *aView, const nsIntRegion& aRegion);

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

  void InvalidateView(nsView *aView, const nsRect &aRect);

  nsViewManager* RootViewManager() const { return mRootViewManager; }
  bool IsRootVM() const { return this == RootViewManager(); }

  // Whether synchronous painting is allowed at the moment. For example,
  // widget geometry changes can cause synchronous painting, so they need to
  // be deferred while refresh is disabled.
  bool IsPaintingAllowed() { return RootViewManager()->mRefreshDisableCount == 0; }

  void WillPaintWindow(nsIWidget* aWidget);
  bool PaintWindow(nsIWidget* aWidget, nsIntRegion aRegion);
  void DidPaintWindow();

  // Call this when you need to let the viewmanager know that it now has
  // pending updates.
  void PostPendingUpdate();

  nsRefPtr<nsDeviceContext> mContext;
  nsIPresShell   *mPresShell;

  // The size for a resize that we delayed until the root view becomes
  // visible again.
  nsSize            mDelayedResize;

  nsView           *mRootView;
  // mRootViewManager is a strong ref unless it equals |this|.  It's
  // never null (if we have no ancestors, it will be |this|).
  nsViewManager   *mRootViewManager;

  // The following members should not be accessed directly except by
  // the root view manager.  Some have accessor functions to enforce
  // this, as noted.

  int32_t           mRefreshDisableCount;
  // Use IsPainting() and SetPainting() to access mPainting.
  bool              mPainting;
  bool              mRecursiveRefreshPending;
  bool              mHasPendingWidgetGeometryChanges;

  //from here to public should be static and locked... MMP

  //list of view managers
  static nsTArray<nsViewManager*> *gViewManagers;
};

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

#endif  // nsViewManager_h___
