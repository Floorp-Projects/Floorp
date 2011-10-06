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
 *   Patrick C. Beard <beard@netscape.com>
 *   Kevin McCluskey  <kmcclusk@netscape.com>
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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

#define PL_ARENA_CONST_ALIGN_MASK (sizeof(void*)-1)
#include "plarena.h"

#include "nsAutoPtr.h"
#include "nsViewManager.h"
#include "nsGfxCIID.h"
#include "nsView.h"
#include "nsISupportsArray.h"
#include "nsCOMPtr.h"
#include "nsGUIEvent.h"
#include "nsRegion.h"
#include "nsHashtable.h"
#include "nsCOMArray.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"
#include "nsIPluginWidget.h"
#include "nsXULPopupManager.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsEventStateManager.h"

PRTime gFirstPaintTimestamp = 0; // Timestamp of the first paint event
/**
   XXX TODO XXX

   DeCOMify newly private methods
   Optimize view storage
*/

/**
   A note about platform assumptions:

   We assume that a widget is z-ordered on top of its parent.
   
   We do NOT assume anything about the relative z-ordering of sibling widgets. Even though
   we ask for a specific z-order, we don't assume that widget z-ordering actually works.
*/

#define NSCOORD_NONE      PR_INT32_MIN

void
nsViewManager::PostInvalidateEvent()
{
  NS_ASSERTION(IsRootVM(), "Caller screwed up");

  if (!mInvalidateEvent.IsPending()) {
    nsRefPtr<nsInvalidateEvent> ev = new nsInvalidateEvent(this);
    if (NS_FAILED(NS_DispatchToCurrentThread(ev))) {
      NS_WARNING("failed to dispatch nsInvalidateEvent");
    } else {
      mInvalidateEvent = ev;
    }
  }
}

#undef DEBUG_MOUSE_LOCATION

PRInt32 nsViewManager::mVMCount = 0;

// Weakly held references to all of the view managers
nsVoidArray* nsViewManager::gViewManagers = nsnull;
PRUint32 nsViewManager::gLastUserEventTime = 0;

nsViewManager::nsViewManager()
  : mDelayedResize(NSCOORD_NONE, NSCOORD_NONE)
  , mRootViewManager(this)
{
  if (gViewManagers == nsnull) {
    NS_ASSERTION(mVMCount == 0, "View Manager count is incorrect");
    // Create an array to hold a list of view managers
    gViewManagers = new nsVoidArray;
  }
 
  gViewManagers->AppendElement(this);

  ++mVMCount;

  // NOTE:  we use a zeroing operator new, so all data members are
  // assumed to be cleared here.
  mHasPendingUpdates = PR_FALSE;
  mRecursiveRefreshPending = PR_FALSE;
  mUpdateBatchFlags = 0;
}

nsViewManager::~nsViewManager()
{
  if (mRootView) {
    // Destroy any remaining views
    mRootView->Destroy();
    mRootView = nsnull;
  }

  // Make sure to revoke pending events for all viewmanagers, since some events
  // are posted by a non-root viewmanager.
  mInvalidateEvent.Revoke();
  
  if (!IsRootVM()) {
    // We have a strong ref to mRootViewManager
    NS_RELEASE(mRootViewManager);
  }

  NS_ASSERTION((mVMCount > 0), "underflow of viewmanagers");
  --mVMCount;

#ifdef DEBUG
  bool removed =
#endif
    gViewManagers->RemoveElement(this);
  NS_ASSERTION(removed, "Viewmanager instance not was not in the global list of viewmanagers");

  if (0 == mVMCount) {
    // There aren't any more view managers so
    // release the global array of view managers
   
    NS_ASSERTION(gViewManagers != nsnull, "About to delete null gViewManagers");
    delete gViewManagers;
    gViewManagers = nsnull;
  }

  mObserver = nsnull;
}

NS_IMPL_ISUPPORTS1(nsViewManager, nsIViewManager)

// We don't hold a reference to the presentation context because it
// holds a reference to us.
NS_IMETHODIMP nsViewManager::Init(nsDeviceContext* aContext)
{
  NS_PRECONDITION(nsnull != aContext, "null ptr");

  if (nsnull == aContext) {
    return NS_ERROR_NULL_POINTER;
  }
  if (nsnull != mContext) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  mContext = aContext;

  return NS_OK;
}

NS_IMETHODIMP_(nsIView *)
nsViewManager::CreateView(const nsRect& aBounds,
                          const nsIView* aParent,
                          nsViewVisibility aVisibilityFlag)
{
  nsView *v = new nsView(this, aVisibilityFlag);
  if (v) {
    v->SetParent(static_cast<nsView*>(const_cast<nsIView*>(aParent)));
    v->SetPosition(aBounds.x, aBounds.y);
    nsRect dim(0, 0, aBounds.width, aBounds.height);
    v->SetDimensions(dim, PR_FALSE);
  }
  return v;
}

NS_IMETHODIMP_(nsIView*)
nsViewManager::GetRootView()
{
  return mRootView;
}

NS_IMETHODIMP nsViewManager::SetRootView(nsIView *aView)
{
  nsView* view = static_cast<nsView*>(aView);

  NS_PRECONDITION(!view || view->GetViewManager() == this,
                  "Unexpected viewmanager on root view");
  
  // Do NOT destroy the current root view. It's the caller's responsibility
  // to destroy it
  mRootView = view;

  if (mRootView) {
    nsView* parent = mRootView->GetParent();
    if (parent) {
      // Calling InsertChild on |parent| will InvalidateHierarchy() on us, so
      // no need to set mRootViewManager ourselves here.
      parent->InsertChild(mRootView, nsnull);
    } else {
      InvalidateHierarchy();
    }

    mRootView->SetZIndex(PR_FALSE, 0, PR_FALSE);
  }
  // Else don't touch mRootViewManager

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::GetWindowDimensions(nscoord *aWidth, nscoord *aHeight)
{
  if (nsnull != mRootView) {
    if (mDelayedResize == nsSize(NSCOORD_NONE, NSCOORD_NONE)) {
      nsRect dim = mRootView->GetDimensions();
      *aWidth = dim.width;
      *aHeight = dim.height;
    } else {
      *aWidth = mDelayedResize.width;
      *aHeight = mDelayedResize.height;
    }
  }
  else
    {
      *aWidth = 0;
      *aHeight = 0;
    }
  return NS_OK;
}

void nsViewManager::DoSetWindowDimensions(nscoord aWidth, nscoord aHeight)
{
  nsRect oldDim = mRootView->GetDimensions();
  nsRect newDim(0, 0, aWidth, aHeight);
  // We care about resizes even when one dimension is already zero.
  if (!oldDim.IsEqualEdges(newDim)) {
    // Don't resize the widget. It is already being set elsewhere.
    mRootView->SetDimensions(newDim, PR_TRUE, PR_FALSE);
    if (mObserver)
      mObserver->ResizeReflow(mRootView, aWidth, aHeight);
  }
}

NS_IMETHODIMP nsViewManager::SetWindowDimensions(nscoord aWidth, nscoord aHeight)
{
  if (mRootView) {
    if (mRootView->IsEffectivelyVisible()) {
      if (mDelayedResize != nsSize(NSCOORD_NONE, NSCOORD_NONE) &&
          mDelayedResize != nsSize(aWidth, aHeight)) {
        // We have a delayed resize; that now obsolete size may already have
        // been flushed to the PresContext so we need to update the PresContext
        // with the new size because if the new size is exactly the same as the
        // root view's current size then DoSetWindowDimensions will not
        // request a resize reflow (which would correct it). See bug 617076.
        mDelayedResize = nsSize(aWidth, aHeight);
        FlushDelayedResize(PR_FALSE);
      }
      mDelayedResize.SizeTo(NSCOORD_NONE, NSCOORD_NONE);
      DoSetWindowDimensions(aWidth, aHeight);
    } else {
      mDelayedResize.SizeTo(aWidth, aHeight);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::FlushDelayedResize(bool aDoReflow)
{
  if (mDelayedResize != nsSize(NSCOORD_NONE, NSCOORD_NONE)) {
    if (aDoReflow) {
      DoSetWindowDimensions(mDelayedResize.width, mDelayedResize.height);
      mDelayedResize.SizeTo(NSCOORD_NONE, NSCOORD_NONE);
    } else if (mObserver) {
      nsCOMPtr<nsIPresShell> shell = do_QueryInterface(mObserver);
      nsPresContext* presContext = shell->GetPresContext();
      if (presContext) {
        presContext->SetVisibleArea(nsRect(nsPoint(0, 0), mDelayedResize));
      }
    }
  }
  return NS_OK;
}

// Convert aIn from being relative to and in appunits of aFromView, to being
// relative to and in appunits of aToView.
static nsRegion ConvertRegionBetweenViews(const nsRegion& aIn,
                                          nsView* aFromView,
                                          nsView* aToView)
{
  nsRegion out = aIn;
  out.MoveBy(aFromView->GetOffsetTo(aToView));
  out = out.ConvertAppUnitsRoundOut(
    aFromView->GetViewManager()->AppUnitsPerDevPixel(),
    aToView->GetViewManager()->AppUnitsPerDevPixel());
  return out;
}

nsIView* nsIViewManager::GetDisplayRootFor(nsIView* aView)
{
  nsIView *displayRoot = aView;
  for (;;) {
    nsIView *displayParent = displayRoot->GetParent();
    if (!displayParent)
      return displayRoot;

    if (displayRoot->GetFloating() && !displayParent->GetFloating())
      return displayRoot;

    // If we have a combobox dropdown popup within a panel popup, both the view
    // for the dropdown popup and its parent will be floating, so we need to
    // distinguish this situation. We do this by looking for a widget. Any view
    // with a widget is a display root, except for plugins.
    nsIWidget* widget = displayRoot->GetWidget();
    if (widget) {
      nsWindowType type;
      widget->GetWindowType(type);
      if (type == eWindowType_popup) {
        NS_ASSERTION(displayRoot->GetFloating() && displayParent->GetFloating(),
          "this should only happen with floating views that have floating parents");
        return displayRoot;
      }
    }

    displayRoot = displayParent;
  }
}

/**
   aRegion is given in device coordinates!!
   aContext may be null, in which case layers should be used for
   rendering.
*/
void nsViewManager::Refresh(nsView *aView, nsIWidget *aWidget,
                            const nsIntRegion& aRegion,
                            PRUint32 aUpdateFlags)
{
  NS_ASSERTION(aView == nsView::GetViewFor(aWidget), "view widget mismatch");
  NS_ASSERTION(aView->GetViewManager() == this, "wrong view manager");

  if (! IsRefreshEnabled())
    return;

  // damageRegion is the damaged area, in twips, relative to the view origin
  nsRegion damageRegion = aRegion.ToAppUnits(AppUnitsPerDevPixel());
  // move region from widget coordinates into view coordinates
  damageRegion.MoveBy(-aView->ViewToWidgetOffset());

  if (damageRegion.IsEmpty()) {
#ifdef DEBUG_roc
    nsRect viewRect = aView->GetDimensions();
    nsRect damageRect = damageRegion.GetBounds();
    printf("XXX Damage rectangle (%d,%d,%d,%d) does not intersect the widget's view (%d,%d,%d,%d)!\n",
           damageRect.x, damageRect.y, damageRect.width, damageRect.height,
           viewRect.x, viewRect.y, viewRect.width, viewRect.height);
#endif
    return;
  }

  NS_ASSERTION(!IsPainting(), "recursive painting not permitted");
  if (IsPainting()) {
    RootViewManager()->mRecursiveRefreshPending = PR_TRUE;
    return;
  }  

  {
    nsAutoScriptBlocker scriptBlocker;
    SetPainting(PR_TRUE);

    RenderViews(aView, aWidget, damageRegion, aRegion, PR_FALSE, PR_FALSE);

    SetPainting(PR_FALSE);
  }

  if (RootViewManager()->mRecursiveRefreshPending) {
    // Unset this flag first, since if aUpdateFlags includes NS_VMREFRESH_IMMEDIATE
    // we'll reenter this code from the UpdateAllViews call.
    RootViewManager()->mRecursiveRefreshPending = PR_FALSE;
    UpdateAllViews(aUpdateFlags);
  }
}

// aRC and aRegion are in view coordinates
void nsViewManager::RenderViews(nsView *aView, nsIWidget *aWidget,
                                const nsRegion& aRegion,
                                const nsIntRegion& aIntRegion,
                                bool aPaintDefaultBackground,
                                bool aWillSendDidPaint)
{
  NS_ASSERTION(GetDisplayRootFor(aView) == aView,
               "Widgets that we paint must all be display roots");

  if (mObserver) {
    mObserver->Paint(aView, aWidget, aRegion, aIntRegion,
                     aPaintDefaultBackground, aWillSendDidPaint);
    if (!gFirstPaintTimestamp)
      gFirstPaintTimestamp = PR_Now();
  }
}

void nsViewManager::ProcessPendingUpdates(nsView* aView, bool aDoInvalidate)
{
  NS_ASSERTION(IsRootVM(), "Updates will be missed");

  // Protect against a null-view.
  if (!aView) {
    return;
  }

  if (aView->HasWidget()) {
    aView->ResetWidgetBounds(PR_FALSE, PR_FALSE, PR_TRUE);
  }

  // process pending updates in child view.
  for (nsView* childView = aView->GetFirstChild(); childView;
       childView = childView->GetNextSibling()) {
    ProcessPendingUpdates(childView, aDoInvalidate);
  }

  if (aDoInvalidate && aView->HasNonEmptyDirtyRegion()) {
    // Push out updates after we've processed the children; ensures that
    // damage is applied based on the final widget geometry
    NS_ASSERTION(IsRefreshEnabled(), "Cannot process pending updates with refresh disabled");
    nsRegion* dirtyRegion = aView->GetDirtyRegion();
    if (dirtyRegion) {
      nsView* nearestViewWithWidget = aView;
      while (!nearestViewWithWidget->HasWidget() &&
             nearestViewWithWidget->GetParent()) {
        nearestViewWithWidget = nearestViewWithWidget->GetParent();
      }
      nsRegion r =
        ConvertRegionBetweenViews(*dirtyRegion, aView, nearestViewWithWidget);
      nsViewManager* widgetVM = nearestViewWithWidget->GetViewManager();
      widgetVM->
        UpdateWidgetArea(nearestViewWithWidget,
                         nearestViewWithWidget->GetWidget(), r, nsnull);
      dirtyRegion->SetEmpty();
    }
  }
}

NS_IMETHODIMP nsViewManager::Composite()
{
  if (!IsRootVM()) {
    return RootViewManager()->Composite();
  }
  ForceUpdate();
  ClearUpdateCount();

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::UpdateView(nsIView *aView, PRUint32 aUpdateFlags)
{
  // Mark the entire view as damaged
  return UpdateView(aView, aView->GetDimensions(), aUpdateFlags);
}

/**
 * @param aWidget the widget for aWidgetView; in some cases the widget
 * is being managed directly by the frame system, so aWidgetView->GetWidget()
 * will return null but nsView::GetViewFor(aWidget) returns aWidgetview
 * @param aDamagedRegion this region, relative to aWidgetView, is invalidated in
 * every widget child of aWidgetView, plus aWidgetView's own widget
 * @param aIgnoreWidgetView if non-null, the aIgnoreWidgetView's widget and its
 * children are not updated.
 */
void
nsViewManager::UpdateWidgetArea(nsView *aWidgetView, nsIWidget* aWidget,
                                const nsRegion &aDamagedRegion,
                                nsView* aIgnoreWidgetView)
{
  NS_ASSERTION(aWidgetView->GetViewManager() == this,
               "UpdateWidgetArea called on view we don't own");

#if 0
  nsRect dbgBounds = aDamagedRegion.GetBounds();
  printf("UpdateWidgetArea view:%X (%d) widget:%X region: %d, %d, %d, %d\n",
    aWidgetView, aWidgetView->IsAttachedToTopLevel(),
    aWidget, dbgBounds.x, dbgBounds.y, dbgBounds.width, dbgBounds.height);
#endif

  if (!IsRefreshEnabled()) {
    // accumulate this rectangle in the view's dirty region, so we can
    // process it later.
    nsRegion* dirtyRegion = aWidgetView->GetDirtyRegion();
    if (!dirtyRegion) return;

    dirtyRegion->Or(*dirtyRegion, aDamagedRegion);
    // Don't let dirtyRegion grow beyond 8 rects
    dirtyRegion->SimplifyOutward(8);
    nsViewManager* rootVM = RootViewManager();
    rootVM->mHasPendingUpdates = PR_TRUE;
    rootVM->IncrementUpdateCount();
    return;
    // this should only happen at the top level, and this result
    // should not be consumed by top-level callers, so it doesn't
    // really matter what we return
  }

  // If the bounds don't overlap at all, there's nothing to do
  nsRegion intersection;
  intersection.And(aWidgetView->GetInvalidationDimensions(), aDamagedRegion);
  if (intersection.IsEmpty()) {
    return;
  }

  // If the widget is hidden, it don't cover nothing
  if (aWidget) {
    bool visible;
    aWidget->IsVisible(visible);
    if (!visible)
      return;
  }

  if (aWidgetView == aIgnoreWidgetView) {
    // the widget for aIgnoreWidgetView (and its children) should be treated as already updated.
    return;
  }

  if (!aWidget) {
    // The root view or a scrolling view might not have a widget
    // (for example, during printing). We get here when we scroll
    // during printing to show selected options in a listbox, for example.
    return;
  }

  // Update all child widgets with the damage. In the process,
  // accumulate the union of all the child widget areas, or at least
  // some subset of that.
  nsRegion children;
  if (aWidget->GetTransparencyMode() != eTransparencyTransparent) {
    for (nsIWidget* childWidget = aWidget->GetFirstChild();
         childWidget;
         childWidget = childWidget->GetNextSibling()) {
      nsView* view = nsView::GetViewFor(childWidget);
      NS_ASSERTION(view != aWidgetView, "will recur infinitely");
      bool visible;
      childWidget->IsVisible(visible);
      nsWindowType type;
      childWidget->GetWindowType(type);
      if (view && visible && type != eWindowType_popup) {
        NS_ASSERTION(type == eWindowType_plugin,
                     "Only plugin or popup widgets can be children!");

        // We do not need to invalidate in plugin widgets, but we should
        // exclude them from the invalidation region IF we're not on
        // Mac. On Mac we need to draw under plugin widgets, because
        // plugin widgets are basically invisible
#ifndef XP_MACOSX
        // GetBounds should compensate for chrome on a toplevel widget
        nsIntRect bounds;
        childWidget->GetBounds(bounds);

        nsTArray<nsIntRect> clipRects;
        childWidget->GetWindowClipRegion(&clipRects);
        for (PRUint32 i = 0; i < clipRects.Length(); ++i) {
          nsRect rr = (clipRects[i] + bounds.TopLeft()).
            ToAppUnits(AppUnitsPerDevPixel());
          children.Or(children, rr - aWidgetView->ViewToWidgetOffset()); 
          children.SimplifyInward(20);
        }
#endif
      }
    }
  }

  nsRegion leftOver;
  leftOver.Sub(intersection, children);

  if (!leftOver.IsEmpty()) {
    NS_ASSERTION(IsRefreshEnabled(), "Can only get here with refresh enabled, I hope");

    const nsRect* r;
    for (nsRegionRectIterator iter(leftOver); (r = iter.Next());) {
      nsIntRect bounds = ViewToWidget(aWidgetView, *r);
      aWidget->Invalidate(bounds, PR_FALSE);
    }
  }
}

static bool
ShouldIgnoreInvalidation(nsViewManager* aVM)
{
  while (aVM) {
    nsIViewObserver* vo = aVM->GetViewObserver();
    if (!vo || vo->ShouldIgnoreInvalidation()) {
      return PR_TRUE;
    }
    nsView* view = aVM->GetRootViewImpl()->GetParent();
    aVM = view ? view->GetViewManager() : nsnull;
  }
  return PR_FALSE;
}

nsresult nsViewManager::UpdateView(nsIView *aView, const nsRect &aRect,
                                   PRUint32 aUpdateFlags)
{
  // If painting is suppressed in the presshell or an ancestor drop all
  // invalidates, it will invalidate everything when it unsuppresses.
  if (ShouldIgnoreInvalidation(this)) {
    return NS_OK;
  }

  return UpdateViewNoSuppression(aView, aRect, aUpdateFlags);
}

NS_IMETHODIMP nsViewManager::UpdateViewNoSuppression(nsIView *aView,
                                                     const nsRect &aRect,
                                                     PRUint32 aUpdateFlags)
{
  NS_PRECONDITION(nsnull != aView, "null view");

  nsView* view = static_cast<nsView*>(aView);

  NS_ASSERTION(view->GetViewManager() == this,
               "UpdateView called on view we don't own");

  nsRect damagedRect(aRect);
  if (damagedRect.IsEmpty()) {
    return NS_OK;
  }

  nsView* displayRoot = static_cast<nsView*>(GetDisplayRootFor(view));
  nsViewManager* displayRootVM = displayRoot->GetViewManager();
  // Propagate the update to the displayRoot, since iframes, for example,
  // can overlap each other and be translucent.  So we have to possibly
  // invalidate our rect in each of the widgets we have lying about.
  damagedRect.MoveBy(view->GetOffsetTo(displayRoot));
  PRInt32 rootAPD = displayRootVM->AppUnitsPerDevPixel();
  PRInt32 APD = AppUnitsPerDevPixel();
  damagedRect = damagedRect.ConvertAppUnitsRoundOut(APD, rootAPD);
  displayRootVM->UpdateWidgetArea(displayRoot, displayRoot->GetWidget(),
                                  nsRegion(damagedRect), nsnull);

  RootViewManager()->IncrementUpdateCount();

  if (!IsRefreshEnabled()) {
    return NS_OK;
  }

  // See if we should do an immediate refresh or wait
  if (aUpdateFlags & NS_VMREFRESH_IMMEDIATE) {
    Composite();
  } 

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::UpdateAllViews(PRUint32 aUpdateFlags)
{
  if (RootViewManager() != this) {
    return RootViewManager()->UpdateAllViews(aUpdateFlags);
  }
  
  UpdateViews(mRootView, aUpdateFlags);
  return NS_OK;
}

void nsViewManager::UpdateViews(nsView *aView, PRUint32 aUpdateFlags)
{
  // update this view.
  UpdateView(aView, aUpdateFlags);

  // update all children as well.
  nsView* childView = aView->GetFirstChild();
  while (nsnull != childView)  {
    childView->GetViewManager()->UpdateViews(childView, aUpdateFlags);
    childView = childView->GetNextSibling();
  }
}

static bool
IsViewForPopup(nsIView* aView)
{
  nsIWidget* widget = aView->GetWidget();
  if (widget) {
    nsWindowType type;
    widget->GetWindowType(type);
    return (type == eWindowType_popup);
  }

  return PR_FALSE;
}

NS_IMETHODIMP nsViewManager::DispatchEvent(nsGUIEvent *aEvent,
                                           nsIView* aView, nsEventStatus *aStatus)
{
  NS_ASSERTION(!aView || static_cast<nsView*>(aView)->GetViewManager() == this,
               "wrong view manager");

  *aStatus = nsEventStatus_eIgnore;

  switch(aEvent->message)
    {
    case NS_SIZE:
      {
        if (aView)
          {
            // client area dimensions are set on the view
            nscoord width = ((nsSizeEvent*)aEvent)->windowSize->width;
            nscoord height = ((nsSizeEvent*)aEvent)->windowSize->height;

            // The root view may not be set if this is the resize associated with
            // window creation

            if (aView == mRootView)
              {
                PRInt32 p2a = AppUnitsPerDevPixel();
                SetWindowDimensions(NSIntPixelsToAppUnits(width, p2a),
                                    NSIntPixelsToAppUnits(height, p2a));
                *aStatus = nsEventStatus_eConsumeNoDefault;
              }
            else if (IsViewForPopup(aView))
              {
                nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
                if (pm)
                  {
                    pm->PopupResized(aView, nsIntSize(width, height));
                    *aStatus = nsEventStatus_eConsumeNoDefault;
                  }
              }
          }
        }

        break;

    case NS_MOVE:
      {
        // A popup's parent view is the root view for the parent window, so when
        // a popup moves, the popup's frame and view position must be updated
        // to match.
        if (aView && IsViewForPopup(aView))
          {
            nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
            if (pm)
              {
                pm->PopupMoved(aView, aEvent->refPoint);
                *aStatus = nsEventStatus_eConsumeNoDefault;
              }
          }
        break;
      }

    case NS_DONESIZEMOVE:
      {
        nsCOMPtr<nsIPresShell> shell = do_QueryInterface(mObserver);
        if (shell) {
          nsPresContext* presContext = shell->GetPresContext();
          if (presContext) {
            nsEventStateManager::ClearGlobalActiveContent(nsnull);
          }
    
          mObserver->ClearMouseCapture(aView);
        }
      }
      break;
  
    case NS_XUL_CLOSE:
      {
        // if this is a popup, make a request to hide it. Note that a popuphidden
        // event listener may cancel the event and the popup will not be hidden.
        nsIWidget* widget = aView->GetWidget();
        if (widget) {
          nsWindowType type;
          widget->GetWindowType(type);
          if (type == eWindowType_popup) {
            nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
            if (pm) {
              pm->HidePopup(aView);
              *aStatus = nsEventStatus_eConsumeNoDefault;
            }
          }
        }
      }
      break;

    case NS_WILL_PAINT:
    case NS_PAINT:
      {
        nsPaintEvent *event = static_cast<nsPaintEvent*>(aEvent);

        if (!aView || !mContext)
          break;

        *aStatus = nsEventStatus_eConsumeNoDefault;

        if (aEvent->message == NS_PAINT && event->region.IsEmpty())
          break;

        NS_ASSERTION(static_cast<nsView*>(aView) ==
                       nsView::GetViewFor(event->widget),
                     "view/widget mismatch");

        // The region is in device units, and it's in the coordinate space of
        // its associated widget.

        // Refresh the view
        if (IsRefreshEnabled()) {
          nsRefPtr<nsViewManager> rootVM = RootViewManager();

          // If an ancestor widget was hidden and then shown, we could
          // have a delayed resize to handle.
          bool didResize = false;
          for (nsViewManager *vm = this; vm;
               vm = vm->mRootView->GetParent()
                      ? vm->mRootView->GetParent()->GetViewManager()
                      : nsnull) {
            if (vm->mDelayedResize != nsSize(NSCOORD_NONE, NSCOORD_NONE) &&
                vm->mRootView->IsEffectivelyVisible()) {
              vm->FlushDelayedResize(PR_TRUE);

              // Paint later.
              vm->UpdateView(vm->mRootView, NS_VMREFRESH_NO_SYNC);
              didResize = PR_TRUE;

              // not sure if it's valid for us to claim that we
              // ignored this, but we're going to do so anyway, since
              // we didn't actually paint anything
              *aStatus = nsEventStatus_eIgnore;
            }
          }

          if (!didResize) {
            //NS_ASSERTION(view->IsEffectivelyVisible(), "painting an invisible view");

            // Notify view observers that we're about to paint.
            // Make sure to not send WillPaint notifications while scrolling.

            nsCOMPtr<nsIWidget> widget;
            rootVM->GetRootWidget(getter_AddRefs(widget));
            bool transparentWindow = false;
            if (widget)
                transparentWindow = widget->GetTransparencyMode() == eTransparencyTransparent;

            nsView* view = static_cast<nsView*>(aView);
            if (!transparentWindow) {
              nsIViewObserver* observer = GetViewObserver();
              if (observer) {
                // Do an update view batch.  Make sure not to do it DEFERRED,
                // since that would effectively delay any invalidates that are
                // triggered by the WillPaint notification (they'd happen when
                // the invalid event fires, which is later than the reflow
                // event would fire and could end up being after some timer
                // events, leading to frame dropping in DHTML).  Note that the
                // observer may try to reenter this code from inside
                // WillPaint() by trying to do a synchronous paint, but since
                // refresh will be disabled it won't be able to do the paint.
                // We should really sort out the rules on our synch painting
                // api....
                UpdateViewBatch batch(this);
                rootVM->CallWillPaintOnObservers(event->willSendDidPaint);
                batch.EndUpdateViewBatch(NS_VMREFRESH_NO_SYNC);

                // Get the view pointer again since the code above might have
                // destroyed it (bug 378273).
                view = nsView::GetViewFor(aEvent->widget);
              }
            }
            // Make sure to sync up any widget geometry changes we
            // have pending before we paint.
            if (rootVM->mHasPendingUpdates) {
              rootVM->ProcessPendingUpdates(mRootView, PR_FALSE);
            }
            
            if (view && aEvent->message == NS_PAINT) {
              Refresh(view, event->widget,
                      event->region, NS_VMREFRESH_DOUBLE_BUFFER);
            }
          }
        } else if (aEvent->message == NS_PAINT) {
          // since we got an NS_PAINT event, we need to
          // draw something so we don't get blank areas,
          // unless there's no widget or it's transparent.
          nsRegion rgn = event->region.ToAppUnits(AppUnitsPerDevPixel());
          rgn.MoveBy(-aView->ViewToWidgetOffset());
          RenderViews(static_cast<nsView*>(aView), event->widget, rgn,
                      event->region, PR_TRUE, event->willSendDidPaint);
          // Clients like the editor can trigger multiple
          // reflows during what the user perceives as a single
          // edit operation, so it disables view manager
          // refreshing until the edit operation is complete
          // so that users don't see the intermediate steps.
          // 
          // Unfortunately some of these reflows can trigger
          // nsScrollPortView and nsScrollingView Scroll() calls
          // which in most cases force an immediate BitBlt and
          // synchronous paint to happen even if the view manager's
          // refresh is disabled. (Bug 97674)
          //
          // Calling UpdateView() here, is necessary to add
          // the exposed region specified in the synchronous paint
          // event to  the view's damaged region so that it gets
          // painted properly when refresh is enabled.
          //
          // Note that calling UpdateView() here was deemed
          // to have the least impact on performance, since the
          // other alternative was to make Scroll() post an
          // async paint event for the *entire* ScrollPort or
          // ScrollingView's viewable area. (See bug 97674 for this
          // alternate patch.)

          UpdateView(aView, rgn.GetBounds(), NS_VMREFRESH_NO_SYNC);
        }

        break;
      }

    case NS_DID_PAINT: {
      nsRefPtr<nsViewManager> rootVM = RootViewManager();
      rootVM->CallDidPaintOnObservers();
      break;
    }

    case NS_CREATE:
    case NS_DESTROY:
    case NS_SETZLEVEL:
      /* Don't pass these events through. Passing them through
         causes performance problems on pages with lots of views/frames 
         @see bug 112861 */
      *aStatus = nsEventStatus_eConsumeNoDefault;
      break;

    case NS_DISPLAYCHANGED:

      //Destroy the cached backbuffer to force a new backbuffer
      //be constructed with the appropriate display depth.
      //@see bugzilla bug 6061
      *aStatus = nsEventStatus_eConsumeDoDefault;
      break;

    case NS_SYSCOLORCHANGED:
      {
        // Hold a refcount to the observer. The continued existence of the observer will
        // delay deletion of this view hierarchy should the event want to cause its
        // destruction in, say, some JavaScript event handler.
        nsCOMPtr<nsIViewObserver> obs = GetViewObserver();
        if (obs) {
          obs->HandleEvent(aView, aEvent, PR_FALSE, aStatus);
        }
      }
      break; 

    default:
      {
        if ((NS_IS_MOUSE_EVENT(aEvent) &&
             // Ignore mouse events that we synthesize.
             static_cast<nsMouseEvent*>(aEvent)->reason ==
               nsMouseEvent::eReal &&
             // Ignore mouse exit and enter (we'll get moves if the user
             // is really moving the mouse) since we get them when we
             // create and destroy widgets.
             aEvent->message != NS_MOUSE_EXIT &&
             aEvent->message != NS_MOUSE_ENTER) ||
            NS_IS_KEY_EVENT(aEvent) ||
            NS_IS_IME_EVENT(aEvent) ||
            aEvent->message == NS_PLUGIN_INPUT_EVENT) {
          gLastUserEventTime = PR_IntervalToMicroseconds(PR_IntervalNow());
        }

        if (aEvent->message == NS_DEACTIVATE) {
          // if a window is deactivated, clear the mouse capture regardless
          // of what is capturing
          nsIViewObserver* viewObserver = GetViewObserver();
          if (viewObserver) {
            viewObserver->ClearMouseCapture(nsnull);
          }
        }

        //Find the view whose coordinates system we're in.
        nsView* baseView = static_cast<nsView*>(aView);
        nsView* view = baseView;

        if (NS_IsEventUsingCoordinates(aEvent)) {
          // will dispatch using coordinates. Pretty bogus but it's consistent
          // with what presshell does.
          view = static_cast<nsView*>(GetDisplayRootFor(baseView));
        }

        if (nsnull != view) {
          *aStatus = HandleEvent(view, aEvent);
        }
    
        break;
      }
    }

  return NS_OK;
}

nsEventStatus nsViewManager::HandleEvent(nsView* aView, nsGUIEvent* aEvent)
{
#if 0
  printf(" %d %d %d %d (%d,%d) \n", this, event->widget, event->widgetSupports, 
         event->message, event->point.x, event->point.y);
#endif
  // Hold a refcount to the observer. The continued existence of the observer will
  // delay deletion of this view hierarchy should the event want to cause its
  // destruction in, say, some JavaScript event handler.
  nsCOMPtr<nsIViewObserver> obs = aView->GetViewManager()->GetViewObserver();
  nsEventStatus status = nsEventStatus_eIgnore;
  if (obs) {
     obs->HandleEvent(aView, aEvent, PR_FALSE, &status);
  }

  return status;
}

// Recursively reparent widgets if necessary 

void nsViewManager::ReparentChildWidgets(nsIView* aView, nsIWidget *aNewWidget)
{
  NS_PRECONDITION(aNewWidget, "");

  if (aView->HasWidget()) {
    // Check to see if the parent widget is the
    // same as the new parent. If not then reparent
    // the widget, otherwise there is nothing more
    // to do for the view and its descendants
    nsIWidget* widget = aView->GetWidget();
    nsIWidget* parentWidget = widget->GetParent();
    if (parentWidget) {
      // Child widget
      if (parentWidget != aNewWidget) {
#ifdef DEBUG
        nsresult rv =
#endif
          widget->SetParent(aNewWidget);
        NS_ASSERTION(NS_SUCCEEDED(rv), "SetParent failed!");
      }
    } else {
      // Toplevel widget (popup, dialog, etc)
      widget->ReparentNativeWidget(aNewWidget);
    }
    return;
  }

  // Need to check each of the views children to see
  // if they have a widget and reparent it.

  nsView* view = static_cast<nsView*>(aView);
  for (nsView *kid = view->GetFirstChild(); kid; kid = kid->GetNextSibling()) {
    ReparentChildWidgets(kid, aNewWidget);
  }
}

// Reparent a view and its descendant views widgets if necessary

void nsViewManager::ReparentWidgets(nsIView* aView, nsIView *aParent)
{
  NS_PRECONDITION(aParent, "Must have a parent");
  NS_PRECONDITION(aView, "Must have a view");
  
  // Quickly determine whether the view has pre-existing children or a
  // widget. In most cases the view will not have any pre-existing 
  // children when this is called.  Only in the case
  // where a view has been reparented by removing it from
  // a reinserting it into a new location in the view hierarchy do we
  // have to consider reparenting the existing widgets for the view and
  // it's descendants.
  nsView* view = static_cast<nsView*>(aView);
  if (view->HasWidget() || view->GetFirstChild()) {
    nsIWidget* parentWidget = aParent->GetNearestWidget(nsnull);
    if (parentWidget) {
      ReparentChildWidgets(aView, parentWidget);
      return;
    }
    NS_WARNING("Can not find a widget for the parent view");
  }
}

NS_IMETHODIMP nsViewManager::InsertChild(nsIView *aParent, nsIView *aChild, nsIView *aSibling,
                                         bool aAfter)
{
  nsView* parent = static_cast<nsView*>(aParent);
  nsView* child = static_cast<nsView*>(aChild);
  nsView* sibling = static_cast<nsView*>(aSibling);
  
  NS_PRECONDITION(nsnull != parent, "null ptr");
  NS_PRECONDITION(nsnull != child, "null ptr");
  NS_ASSERTION(sibling == nsnull || sibling->GetParent() == parent,
               "tried to insert view with invalid sibling");
  NS_ASSERTION(!IsViewInserted(child), "tried to insert an already-inserted view");

  if ((nsnull != parent) && (nsnull != child))
    {
      // if aAfter is set, we will insert the child after 'prev' (i.e. after 'kid' in document
      // order, otherwise after 'kid' (i.e. before 'kid' in document order).

#if 1
      if (nsnull == aSibling) {
        if (aAfter) {
          // insert at end of document order, i.e., before first view
          // this is the common case, by far
          parent->InsertChild(child, nsnull);
          ReparentWidgets(child, parent);
        } else {
          // insert at beginning of document order, i.e., after last view
          nsView *kid = parent->GetFirstChild();
          nsView *prev = nsnull;
          while (kid) {
            prev = kid;
            kid = kid->GetNextSibling();
          }
          // prev is last view or null if there are no children
          parent->InsertChild(child, prev);
          ReparentWidgets(child, parent);
        }
      } else {
        nsView *kid = parent->GetFirstChild();
        nsView *prev = nsnull;
        while (kid && sibling != kid) {
          //get the next sibling view
          prev = kid;
          kid = kid->GetNextSibling();
        }
        NS_ASSERTION(kid != nsnull,
                     "couldn't find sibling in child list");
        if (aAfter) {
          // insert after 'kid' in document order, i.e. before in view order
          parent->InsertChild(child, prev);
          ReparentWidgets(child, parent);
        } else {
          // insert before 'kid' in document order, i.e. after in view order
          parent->InsertChild(child, kid);
          ReparentWidgets(child, parent);
        }
      }
#else // don't keep consistent document order, but order things by z-index instead
      // essentially we're emulating the old InsertChild(parent, child, zindex)
      PRInt32 zIndex = child->GetZIndex();
      while (nsnull != kid)
        {
          PRInt32 idx = kid->GetZIndex();

          if (CompareZIndex(zIndex, child->IsTopMost(), child->GetZIndexIsAuto(),
                            idx, kid->IsTopMost(), kid->GetZIndexIsAuto()) >= 0)
            break;

          prev = kid;
          kid = kid->GetNextSibling();
        }

      parent->InsertChild(child, prev);
      ReparentWidgets(child, parent);
#endif

      // if the parent view is marked as "floating", make the newly added view float as well.
      if (parent->GetFloating())
        child->SetFloating(PR_TRUE);

      //and mark this area as dirty if the view is visible...

      if (nsViewVisibility_kHide != child->GetVisibility())
        child->GetViewManager()->UpdateView(child, NS_VMREFRESH_NO_SYNC);
    }
  return NS_OK;
}

NS_IMETHODIMP nsViewManager::InsertChild(nsIView *aParent, nsIView *aChild, PRInt32 aZIndex)
{
  // no-one really calls this with anything other than aZIndex == 0 on a fresh view
  // XXX this method should simply be eliminated and its callers redirected to the real method
  SetViewZIndex(aChild, PR_FALSE, aZIndex, PR_FALSE);
  return InsertChild(aParent, aChild, nsnull, PR_TRUE);
}

NS_IMETHODIMP nsViewManager::RemoveChild(nsIView *aChild)
{
  nsView* child = static_cast<nsView*>(aChild);
  NS_ENSURE_ARG_POINTER(child);

  nsView* parent = child->GetParent();

  if (nsnull != parent) {
    NS_ASSERTION(child->GetViewManager() == this ||
                 parent->GetViewManager() == this, "wrong view manager");
    child->GetViewManager()->UpdateView(child, NS_VMREFRESH_NO_SYNC);
    parent->RemoveChild(child);
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::MoveViewTo(nsIView *aView, nscoord aX, nscoord aY)
{
  nsView* view = static_cast<nsView*>(aView);
  NS_ASSERTION(view->GetViewManager() == this, "wrong view manager");
  nsPoint oldPt = view->GetPosition();
  nsRect oldBounds = view->GetBoundsInParentUnits();
  view->SetPosition(aX, aY);

  // only do damage control if the view is visible

  if ((aX != oldPt.x) || (aY != oldPt.y)) {
    if (view->GetVisibility() != nsViewVisibility_kHide) {
      nsView* parentView = view->GetParent();
      if (parentView) {
        nsViewManager* parentVM = parentView->GetViewManager();
        parentVM->UpdateView(parentView, oldBounds, NS_VMREFRESH_NO_SYNC);
        parentVM->UpdateView(parentView, view->GetBoundsInParentUnits(),
                             NS_VMREFRESH_NO_SYNC);
      }
    }
  }
  return NS_OK;
}

void nsViewManager::InvalidateHorizontalBandDifference(nsView *aView, const nsRect& aRect, const nsRect& aCutOut,
  PRUint32 aUpdateFlags, nscoord aY1, nscoord aY2, bool aInCutOut) {
  nscoord height = aY2 - aY1;
  if (aRect.x < aCutOut.x) {
    nsRect r(aRect.x, aY1, aCutOut.x - aRect.x, height);
    UpdateView(aView, r, aUpdateFlags);
  }
  if (!aInCutOut && aCutOut.x < aCutOut.XMost()) {
    nsRect r(aCutOut.x, aY1, aCutOut.width, height);
    UpdateView(aView, r, aUpdateFlags);
  }
  if (aCutOut.XMost() < aRect.XMost()) {
    nsRect r(aCutOut.XMost(), aY1, aRect.XMost() - aCutOut.XMost(), height);
    UpdateView(aView, r, aUpdateFlags);
  }
}

void nsViewManager::InvalidateRectDifference(nsView *aView, const nsRect& aRect, const nsRect& aCutOut,
  PRUint32 aUpdateFlags) {
  NS_ASSERTION(aView->GetViewManager() == this,
               "InvalidateRectDifference called on view we don't own");
  if (aRect.y < aCutOut.y) {
    InvalidateHorizontalBandDifference(aView, aRect, aCutOut, aUpdateFlags, aRect.y, aCutOut.y, PR_FALSE);
  }
  if (aCutOut.y < aCutOut.YMost()) {
    InvalidateHorizontalBandDifference(aView, aRect, aCutOut, aUpdateFlags, aCutOut.y, aCutOut.YMost(), PR_TRUE);
  }
  if (aCutOut.YMost() < aRect.YMost()) {
    InvalidateHorizontalBandDifference(aView, aRect, aCutOut, aUpdateFlags, aCutOut.YMost(), aRect.YMost(), PR_FALSE);
  }
}

NS_IMETHODIMP nsViewManager::ResizeView(nsIView *aView, const nsRect &aRect, bool aRepaintExposedAreaOnly)
{
  nsView* view = static_cast<nsView*>(aView);
  NS_ASSERTION(view->GetViewManager() == this, "wrong view manager");

  nsRect oldDimensions = view->GetDimensions();
  if (!oldDimensions.IsEqualEdges(aRect)) {
    // resize the view.
    // Prevent Invalidation of hidden views 
    if (view->GetVisibility() == nsViewVisibility_kHide) {  
      view->SetDimensions(aRect, PR_FALSE);
    } else {
      nsView* parentView = view->GetParent();
      if (!parentView) {
        parentView = view;
      }
      nsRect oldBounds = view->GetBoundsInParentUnits();
      view->SetDimensions(aRect, PR_TRUE);
      nsViewManager* parentVM = parentView->GetViewManager();
      if (!aRepaintExposedAreaOnly) {
        //Invalidate the union of the old and new size
        UpdateView(view, aRect, NS_VMREFRESH_NO_SYNC);
        parentVM->UpdateView(parentView, oldBounds, NS_VMREFRESH_NO_SYNC);
      } else {
        InvalidateRectDifference(view, aRect, oldDimensions, NS_VMREFRESH_NO_SYNC);
        nsRect newBounds = view->GetBoundsInParentUnits();
        parentVM->InvalidateRectDifference(parentView, oldBounds, newBounds,
                                           NS_VMREFRESH_NO_SYNC);
      } 
    }
  }

  // Note that if layout resizes the view and the view has a custom clip
  // region set, then we expect layout to update the clip region too. Thus
  // in the case where mClipRect has been optimized away to just be a null
  // pointer, and this resize is implicitly changing the clip rect, it's OK
  // because layout will change it back again if necessary.

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::SetViewFloating(nsIView *aView, bool aFloating)
{
  nsView* view = static_cast<nsView*>(aView);

  NS_ASSERTION(!(nsnull == view), "no view");

  view->SetFloating(aFloating);

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::SetViewVisibility(nsIView *aView, nsViewVisibility aVisible)
{
  nsView* view = static_cast<nsView*>(aView);
  NS_ASSERTION(view->GetViewManager() == this, "wrong view manager");

  if (aVisible != view->GetVisibility()) {
    view->SetVisibility(aVisible);

    if (IsViewInserted(view)) {
      if (!view->HasWidget()) {
        if (nsViewVisibility_kHide == aVisible) {
          nsView* parentView = view->GetParent();
          if (parentView) {
            parentView->GetViewManager()->
              UpdateView(parentView, view->GetBoundsInParentUnits(),
                         NS_VMREFRESH_NO_SYNC);
          }
        }
        else {
          UpdateView(view, NS_VMREFRESH_NO_SYNC);
        }
      }
    }
  }
  return NS_OK;
}

void nsViewManager::UpdateWidgetsForView(nsView* aView)
{
  NS_PRECONDITION(aView, "Must have view!");

  // No point forcing an update if invalidations have been suppressed.
  if (!IsRefreshEnabled())
    return;  

  nsWeakView parentWeakView = aView;
  if (aView->HasWidget()) {
    aView->GetWidget()->Update();  // Flushes Layout!
    if (!parentWeakView.IsAlive()) {
      return;
    }
  }

  nsView* childView = aView->GetFirstChild();
  while (childView) {
    nsWeakView childWeakView = childView;
    UpdateWidgetsForView(childView);
    if (NS_LIKELY(childWeakView.IsAlive())) {
      childView = childView->GetNextSibling();
    }
    else {
      // The current view was destroyed - restart at the first child if the
      // parent is still alive.
      childView = parentWeakView.IsAlive() ? aView->GetFirstChild() : nsnull;
    }
  }
}

bool nsViewManager::IsViewInserted(nsView *aView)
{
  if (mRootView == aView) {
    return PR_TRUE;
  } else if (aView->GetParent() == nsnull) {
    return PR_FALSE;
  } else {
    nsView* view = aView->GetParent()->GetFirstChild();
    while (view != nsnull) {
      if (view == aView) {
        return PR_TRUE;
      }        
      view = view->GetNextSibling();
    }
    return PR_FALSE;
  }
}

NS_IMETHODIMP nsViewManager::SetViewZIndex(nsIView *aView, bool aAutoZIndex, PRInt32 aZIndex, bool aTopMost)
{
  nsView* view = static_cast<nsView*>(aView);
  nsresult  rv = NS_OK;

  NS_ASSERTION((view != nsnull), "no view");

  // don't allow the root view's z-index to be changed. It should always be zero.
  // This could be removed and replaced with a style rule, or just removed altogether, with interesting consequences
  if (aView == mRootView) {
    return rv;
  }

  bool oldTopMost = view->IsTopMost();
  bool oldIsAuto = view->GetZIndexIsAuto();

  if (aAutoZIndex) {
    aZIndex = 0;
  }

  PRInt32 oldidx = view->GetZIndex();
  view->SetZIndex(aAutoZIndex, aZIndex, aTopMost);

  if (oldidx != aZIndex || oldTopMost != aTopMost ||
      oldIsAuto != aAutoZIndex) {
    UpdateView(view, NS_VMREFRESH_NO_SYNC);
  }

  return rv;
}

NS_IMETHODIMP nsViewManager::GetDeviceContext(nsDeviceContext *&aContext)
{
  aContext = mContext;
  NS_IF_ADDREF(aContext);
  return NS_OK;
}

void nsViewManager::TriggerRefresh(PRUint32 aUpdateFlags)
{
  if (!IsRootVM()) {
    RootViewManager()->TriggerRefresh(aUpdateFlags);
    return;
  }
  
  if (mUpdateBatchCnt > 0)
    return;

  // nested batching can combine IMMEDIATE with DEFERRED. Favour
  // IMMEDIATE over DEFERRED and DEFERRED over NO_SYNC.  We need to
  // check for IMMEDIATE before checking mHasPendingUpdates, because
  // the latter might be false as far as gecko is concerned but the OS
  // might still have queued up expose events that it hasn't sent yet.
  if (aUpdateFlags & NS_VMREFRESH_IMMEDIATE) {
    FlushPendingInvalidates();
    Composite();
  } else if (!mHasPendingUpdates) {
    // Nothing to do
  } else if (aUpdateFlags & NS_VMREFRESH_DEFERRED) {
    PostInvalidateEvent();
  } else { // NO_SYNC
    FlushPendingInvalidates();
  }
}

nsIViewManager* nsViewManager::BeginUpdateViewBatch(void)
{
  if (!IsRootVM()) {
    return RootViewManager()->BeginUpdateViewBatch();
  }
  
  if (mUpdateBatchCnt == 0) {
    mUpdateBatchFlags = 0;
  }

  ++mUpdateBatchCnt;

  return this;
}

NS_IMETHODIMP nsViewManager::EndUpdateViewBatch(PRUint32 aUpdateFlags)
{
  NS_ASSERTION(IsRootVM(), "Should only be called on root");
  
  --mUpdateBatchCnt;

  NS_ASSERTION(mUpdateBatchCnt >= 0, "Invalid batch count!");

  if (mUpdateBatchCnt < 0)
    {
      mUpdateBatchCnt = 0;
      return NS_ERROR_FAILURE;
    }

  mUpdateBatchFlags |= aUpdateFlags;
  if (mUpdateBatchCnt == 0) {
    TriggerRefresh(mUpdateBatchFlags);
  }

  return NS_OK;
}

NS_IMETHODIMP nsViewManager::GetRootWidget(nsIWidget **aWidget)
{
  if (!mRootView) {
    *aWidget = nsnull;
    return NS_OK;
  }
  if (mRootView->HasWidget()) {
    *aWidget = mRootView->GetWidget();
    NS_ADDREF(*aWidget);
    return NS_OK;
  }
  if (mRootView->GetParent())
    return mRootView->GetParent()->GetViewManager()->GetRootWidget(aWidget);
  *aWidget = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsViewManager::ForceUpdate()
{
  if (!IsRootVM()) {
    return RootViewManager()->ForceUpdate();
  }

  // Walk the view tree looking for widgets, and call Update() on each one
  if (mRootView) {
    UpdateWidgetsForView(mRootView);
  }
  
  return NS_OK;
}

nsIntRect nsViewManager::ViewToWidget(nsView *aView, const nsRect &aRect) const
{
  NS_ASSERTION(aView->GetViewManager() == this, "wrong view manager");

  // intersect aRect with bounds of aView, to prevent generating any illegal rectangles.
  nsRect bounds = aView->GetInvalidationDimensions();
  nsRect rect;
  rect.IntersectRect(aRect, bounds);

  // account for the view's origin not lining up with the widget's
  rect += aView->ViewToWidgetOffset();

  // finally, convert to device coordinates.
  return rect.ToOutsidePixels(AppUnitsPerDevPixel());
}

NS_IMETHODIMP
nsViewManager::IsPainting(bool& aIsPainting)
{
  aIsPainting = IsPainting();
  return NS_OK;
}

void
nsViewManager::FlushPendingInvalidates()
{
  NS_ASSERTION(IsRootVM(), "Must be root VM for this to be called!");
  NS_ASSERTION(mUpdateBatchCnt == 0, "Must not be in an update batch!");

  if (mHasPendingUpdates) {
    ProcessPendingUpdates(mRootView, PR_TRUE);
    mHasPendingUpdates = PR_FALSE;
  }
}

void
nsViewManager::CallWillPaintOnObservers(bool aWillSendDidPaint)
{
  NS_PRECONDITION(IsRootVM(), "Must be root VM for this to be called!");
  NS_PRECONDITION(mUpdateBatchCnt > 0, "Must be in an update batch!");

#ifdef DEBUG
  PRInt32 savedUpdateBatchCnt = mUpdateBatchCnt;
#endif
  PRInt32 index;
  for (index = 0; index < mVMCount; index++) {
    nsViewManager* vm = (nsViewManager*)gViewManagers->ElementAt(index);
    if (vm->RootViewManager() == this) {
      // One of our kids.
      if (vm->mRootView && vm->mRootView->IsEffectivelyVisible()) {
        nsCOMPtr<nsIViewObserver> obs = vm->GetViewObserver();
        if (obs) {
          obs->WillPaint(aWillSendDidPaint);
          NS_ASSERTION(mUpdateBatchCnt == savedUpdateBatchCnt,
                       "Observer did not end view batch?");
        }
      }
    }
  }
}

void
nsViewManager::CallDidPaintOnObservers()
{
  NS_PRECONDITION(IsRootVM(), "Must be root VM for this to be called!");

  PRInt32 index;
  for (index = 0; index < mVMCount; index++) {
    nsViewManager* vm = (nsViewManager*)gViewManagers->ElementAt(index);
    if (vm->RootViewManager() == this) {
      // One of our kids.
      if (vm->mRootView && vm->mRootView->IsEffectivelyVisible()) {
        nsCOMPtr<nsIViewObserver> obs = vm->GetViewObserver();
        if (obs) {
          obs->DidPaint();
        }
      }
    }
  }
}

void
nsViewManager::ProcessInvalidateEvent()
{
  NS_ASSERTION(IsRootVM(),
               "Incorrectly targeted invalidate event");
  // If we're in the middle of an update batch, just repost the event,
  // to be processed when the batch ends.
  bool processEvent = (mUpdateBatchCnt == 0);
  if (processEvent) {
    FlushPendingInvalidates();
  }
  mInvalidateEvent.Forget();
  if (!processEvent) {
    // We didn't actually process this event... post a new one
    PostInvalidateEvent();
  }
}

NS_IMETHODIMP
nsViewManager::GetLastUserEventTime(PRUint32& aTime)
{
  aTime = gLastUserEventTime;
  return NS_OK;
}

void
nsViewManager::InvalidateHierarchy()
{
  if (mRootView) {
    if (!IsRootVM()) {
      NS_RELEASE(mRootViewManager);
    }
    nsView *parent = mRootView->GetParent();
    if (parent) {
      mRootViewManager = parent->GetViewManager()->RootViewManager();
      NS_ADDREF(mRootViewManager);
      NS_ASSERTION(mRootViewManager != this,
                   "Root view had a parent, but it has the same view manager");
    } else {
      mRootViewManager = this;
    }
  }
}
