/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIViewManager_h___
#define nsIViewManager_h___

#include "nscore.h"
#include "nsIView.h"
#include "nsEvent.h"

class nsIWidget;
struct nsRect;
class nsRegion;
class nsDeviceContext;
class nsIPresShell;

#define NS_IVIEWMANAGER_IID \
{ 0x540610a6, 0x4fdd, 0x4ae3, \
  { 0x9b, 0xdb, 0xa6, 0x4d, 0x8b, 0xca, 0x02, 0x0f } }

class nsIViewManager : public nsISupports
{
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IVIEWMANAGER_IID)
  /**
   * Initialize the ViewManager
   * Note: this instance does not hold a reference to the presshell
   * because it holds a reference to this instance.
   * @result The result of the initialization, NS_OK if no errors
   */
  NS_IMETHOD  Init(nsDeviceContext* aContext) = 0;

  /**
   * Create an ordinary view
   * @param aBounds initial bounds for view
   *        XXX We should eliminate this parameter; you can set the bounds after CreateView
   * @param aParent intended parent for view. this is not actually set in the
   *        nsIView through this method. it is only used by the initialization
   *        code to walk up the view tree, if necessary, to find resources.
   *        XXX We should eliminate this parameter!
   * @param aVisibilityFlag initial visibility state of view
   *        XXX We should eliminate this parameter; you can set it after CreateView
   * @result The new view
   */
  NS_IMETHOD_(nsIView*) CreateView(const nsRect& aBounds,
                                   const nsIView* aParent,
                                   nsViewVisibility aVisibilityFlag = nsViewVisibility_kShow) = 0;

  /**
   * Get the root of the view tree.
   * @result the root view
   */
  NS_IMETHOD_(nsIView*) GetRootView() = 0;

  /**
   * Set the root of the view tree. Does not destroy the current root view.
   * aView may have a parent view managed by a different view manager.
   * aView may have a widget (anything but printing) or may not (printing).
   * @param aView view to set as root
   */
  NS_IMETHOD  SetRootView(nsIView *aView) = 0;

  /**
   * Get the dimensions of the root window. The dimensions are in
   * twips
   * @param aWidth out parameter for width of window in twips
   * @param aHeight out parameter for height of window in twips
   */
  NS_IMETHOD  GetWindowDimensions(nscoord *aWidth, nscoord *aHeight) = 0;

  /**
   * Set the dimensions of the root window.
   * Called if the root window is resized. The dimensions are in
   * twips
   * @param aWidth of window in twips
   * @param aHeight of window in twips
   */
  NS_IMETHOD  SetWindowDimensions(nscoord aWidth, nscoord aHeight) = 0;

  /**
   * Do any resizes that are pending.
   */
  NS_IMETHOD  FlushDelayedResize(bool aDoReflow) = 0;

  /**
   * Called to inform the view manager that the entire area of a view
   * is dirty and needs to be redrawn.
   * @param aView view to paint. should be root view
   */
  NS_IMETHOD  InvalidateView(nsIView *aView) = 0;

  /**
   * Called to inform the view manager that some portion of a view is dirty and
   * needs to be redrawn. The rect passed in should be in the view's coordinate
   * space. Does not check for paint suppression.
   * @param aView view to paint. should be root view
   * @param rect rect to mark as damaged
   */
  NS_IMETHOD  InvalidateViewNoSuppression(nsIView *aView, const nsRect &aRect) = 0;

  /**
   * Called to inform the view manager that it should invalidate all views.
   */
  NS_IMETHOD  InvalidateAllViews() = 0;

  /**
   * Called to dispatch an event to the appropriate view. Often called
   * as a result of receiving a mouse or keyboard event from the widget
   * event system.
   * @param aEvent event to dispatch
   * @param aViewTarget dispatch the event to this view
   * @param aStatus event handling status
   */
  NS_IMETHOD  DispatchEvent(nsGUIEvent *aEvent,
      nsIView* aViewTarget, nsEventStatus* aStatus) = 0;

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
  NS_IMETHOD  InsertChild(nsIView *aParent, nsIView *aChild, nsIView *aSibling,
                          bool aAfter) = 0;

  /**
   * Remove a specific child view from its parent. This will NOT remove its placeholder
   * if there is one.
   * The view manager generates the appropriate dirty regions.
   * @param aParent parent view
   * @param aChild child view
   */
  NS_IMETHOD  RemoveChild(nsIView *aChild) = 0;

  /**
   * Move a view to the specified position, provided in parent coordinates.
   * The new position is the (0, 0) origin for the view's coordinate system.
   * The view's bounds may extend above or to the left of this point.
   * The view manager generates the appropriate dirty regions.
   * @param aView view to move
   * @param aX x value for new view position
   * @param aY y value for new view position
   */
  NS_IMETHOD  MoveViewTo(nsIView *aView, nscoord aX, nscoord aY) = 0;

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
  NS_IMETHOD  ResizeView(nsIView *aView, const nsRect &aRect,
                         bool aRepaintExposedAreaOnly = false) = 0;

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
  NS_IMETHOD  SetViewVisibility(nsIView *aView, nsViewVisibility aVisible) = 0;

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
   * @param aTopMost used when this view is z-index:auto to compare against 
   *        other z-index:auto views.
   *        true if the view should be topmost when compared with 
   *        other z-index:auto views.
   */
  NS_IMETHOD  SetViewZIndex(nsIView *aView, bool aAutoZIndex, PRInt32 aZindex, bool aTopMost = false) = 0;

  /**
   * Set whether the view "floats" above all other views,
   * which tells the compositor not to consider higher views in
   * the view hierarchy that would geometrically intersect with
   * this view. This is a hack, but it fixes some problems with
   * views that need to be drawn in front of all other views.
   */
  NS_IMETHOD  SetViewFloating(nsIView *aView, bool aFloatingView) = 0;

  /**
   * Set the presshell associated with this manager
   * @param aPresShell - new presshell
   */
  virtual void SetPresShell(nsIPresShell *aPresShell) = 0;

  /**
   * Get the pres shell associated with this manager
   */
  virtual nsIPresShell* GetPresShell() = 0;

  /**
   * Get the device context associated with this manager
   * @result device context
   */
  NS_IMETHOD  GetDeviceContext(nsDeviceContext *&aContext) = 0;

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
  class NS_STACK_CLASS AutoDisableRefresh {
  public:
    AutoDisableRefresh(nsIViewManager* aVM) {
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

    nsCOMPtr<nsIViewManager> mRootVM;
  };

private:
  friend class AutoDisableRefresh;

  virtual nsIViewManager* IncrementDisableRefreshCount() = 0;
  virtual void DecrementDisableRefreshCount() = 0;

public:
  /**
   * Retrieve the widget at the root of the nearest enclosing
   * view manager whose root view has a widget.
   */
  NS_IMETHOD GetRootWidget(nsIWidget **aWidget) = 0;

  /**
   * Indicate whether the viewmanager is currently painting
   *
   * @param aPainting true if the viewmanager is painting
   *                  false otherwise
   */
  NS_IMETHOD IsPainting(bool& aIsPainting)=0;

  /**
   * Retrieve the time of the last user event. User events
   * include mouse and keyboard events. The viewmanager
   * saves the time of the last user event.
   *
   * @param aTime Last user event time in microseconds
   */
  NS_IMETHOD GetLastUserEventTime(PRUint32& aTime)=0;

  /**
   * Find the nearest display root view for the view aView. This is the view for
   * the nearest enclosing popup or the root view for the root document.
   */
  static nsIView* GetDisplayRootFor(nsIView* aView);

  /**
   * Flush the accumulated dirty region to the widget and update widget
   * geometry.
   */
  virtual void ProcessPendingUpdates()=0;

  /**
   * Just update widget geometry without flushing the dirty region
   */
  virtual void UpdateWidgetGeometry() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIViewManager, NS_IVIEWMANAGER_IID)

#endif  // nsIViewManager_h___
