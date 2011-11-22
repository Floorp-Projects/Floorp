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

#ifndef nsIViewManager_h___
#define nsIViewManager_h___

#include "nscore.h"
#include "nsIView.h"
#include "nsEvent.h"

class nsIWidget;
struct nsRect;
class nsRegion;
class nsDeviceContext;

#define NS_IVIEWMANAGER_IID \
{ 0x1262a33f, 0xc19f, 0x4e5b, \
  { 0x85, 0x00, 0xab, 0xf3, 0x7d, 0xcf, 0x30, 0x1d } }

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
   * Called to force a redrawing of any dirty areas.
   */
  // XXXbz why is this exposed?  Shouldn't update view batches handle this?
  // It's not like Composite() does what's expected inside a view update batch
  // anyway, since dirty areas may not have been invalidated on the widget yet
  // and widget changes may not have been propagated yet.  Maybe this should
  // call FlushPendingInvalidates()?
  NS_IMETHOD  Composite(void) = 0;

  /**
   * Called to inform the view manager that the entire area of a view
   * is dirty and needs to be redrawn.
   * @param aView view to paint. should be root view
   * @param aUpdateFlags see bottom of nsIViewManager.h for description
   */
  NS_IMETHOD  UpdateView(nsIView *aView, PRUint32 aUpdateFlags) = 0;

  /**
   * Called to inform the view manager that some portion of a view is dirty and
   * needs to be redrawn. The rect passed in should be in the view's coordinate
   * space. Does not check for paint suppression.
   * @param aView view to paint. should be root view
   * @param rect rect to mark as damaged
   * @param aUpdateFlags see bottom of nsIViewManager.h for description
   */
  NS_IMETHOD  UpdateViewNoSuppression(nsIView *aView, const nsRect &aRect,
                                      PRUint32 aUpdateFlags) = 0;

  /**
   * Called to inform the view manager that it should redraw all views.
   * @param aView view to paint. should be root view
   * @param aUpdateFlags see bottom of nsIViewManager.h for description
   */
  NS_IMETHOD  UpdateAllViews(PRUint32 aUpdateFlags) = 0;

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

  class UpdateViewBatch {
  public:
    UpdateViewBatch() {}
  /**
   * prevents the view manager from refreshing. allows UpdateView()
   * to notify widgets of damaged regions that should be repainted
   * when the batch is ended. Call EndUpdateViewBatch on this object
   * before it is destroyed
   * @return error status
   */
    UpdateViewBatch(nsIViewManager* aVM) {
      if (aVM) {
        mRootVM = aVM->BeginUpdateViewBatch();
      }
    }
    ~UpdateViewBatch() {
      NS_ASSERTION(!mRootVM, "Someone forgot to call EndUpdateViewBatch!");
    }
    
    /**
     * See the constructor, this lets you "fill in" a blank UpdateViewBatch.
     */
    void BeginUpdateViewBatch(nsIViewManager* aVM) {
      NS_ASSERTION(!mRootVM, "already started a batch!");
      if (aVM) {
        mRootVM = aVM->BeginUpdateViewBatch();
      }
    }

  /**
   * allow the view manager to refresh any damaged areas accumulated
   * after the BeginUpdateViewBatch() call.  this may cause a
   * synchronous paint to occur inside the call if aUpdateFlags
   * NS_VMREFRESH_IMMEDIATE is set.
   *
   * If this is not the outermost view batch command, then this does
   * nothing except that the specified flags are remembered. When the
   * outermost batch finally ends, we merge together all the flags for the
   * inner batches in the following way:
   * -- If any batch specified NS_VMREFRESH_IMMEDIATE, then we use that flag
   * (i.e. there is a synchronous paint under the last EndUpdateViewBatch)
   * -- Otherwise if any batch specified NS_VMREFERSH_DEFERRED, then we use
   * that flag (i.e. invalidation is deferred until the processing of an
   * Invalidate PLEvent)
   * -- Otherwise all batches specified NS_VMREFRESH_NO_SYNC and we honor
   * that; all widgets are invalidated normally and will be painted the next
   * time the toolkit chooses to update them.
   *
   * @param aUpdateFlags see bottom of nsIViewManager.h for
   * description @return error status
   */
    void EndUpdateViewBatch(PRUint32 aUpdateFlags) {
      if (!mRootVM)
        return;
      mRootVM->EndUpdateViewBatch(aUpdateFlags);
      mRootVM = nsnull;
    }

  private:
    UpdateViewBatch(const UpdateViewBatch& aOther);
    const UpdateViewBatch& operator=(const UpdateViewBatch& aOther);

    nsCOMPtr<nsIViewManager> mRootVM;
  };
  
private:
  friend class UpdateViewBatch;

  virtual nsIViewManager* BeginUpdateViewBatch(void) = 0;
  NS_IMETHOD EndUpdateViewBatch(PRUint32 aUpdateFlags) = 0;

public:
  /**
   * Retrieve the widget at the root of the nearest enclosing
   * view manager whose root view has a widget.
   */
  NS_IMETHOD GetRootWidget(nsIWidget **aWidget) = 0;

  /**
   * Force update of view manager widget
   * Callers should use UpdateView(view, NS_VMREFRESH_IMMEDIATE) in most cases instead
   * @result error status
   */
  // XXXbz Callers seem to be confused about this one... and it doesn't play
  // right with view update batching at all (will miss updates).  Maybe this
  // should call FlushPendingInvalidates()?
  NS_IMETHOD ForceUpdate() = 0;

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
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIViewManager, NS_IVIEWMANAGER_IID)

// Paint timing mode flags

// intermediate: do no special timing processing; repaint when the
// toolkit issues an expose event (which will happen *before* PLEvent
// processing). This is essentially the default.
#define NS_VMREFRESH_NO_SYNC            0

// least immediate: we suppress invalidation, storing dirty areas in
// views, and post an Invalidate PLEvent. The Invalidate event gets
// processed after toolkit events such as window resize events!
// This is only usable with EndUpdateViewBatch and EnableRefresh.
#define NS_VMREFRESH_DEFERRED           0x0001

// most immediate: force a call to nsViewManager::Composite, which
// synchronously updates the window(s) right away before returning
#define NS_VMREFRESH_IMMEDIATE          0x0002

#endif  // nsIViewManager_h___
