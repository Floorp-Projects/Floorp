/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define PL_ARENA_CONST_ALIGN_MASK (sizeof(void*)-1)
#include "plarena.h"

#include "nsAutoPtr.h"
#include "nsViewManager.h"
#include "nsGfxCIID.h"
#include "nsView.h"
#include "nsCOMPtr.h"
#include "nsGUIEvent.h"
#include "nsRegion.h"
#include "nsCOMArray.h"
#include "nsIPluginWidget.h"
#include "nsXULPopupManager.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsEventStateManager.h"
#include "mozilla/StartupTimeline.h"
#include "GeckoProfiler.h"
#include "nsRefreshDriver.h"
#include "mozilla/Preferences.h"
#include "nsContentUtils.h"

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

#define NSCOORD_NONE      INT32_MIN

#undef DEBUG_MOUSE_LOCATION

int32_t nsViewManager::mVMCount = 0;

// Weakly held references to all of the view managers
nsVoidArray* nsViewManager::gViewManagers = nullptr;
uint32_t nsViewManager::gLastUserEventTime = 0;

nsViewManager::nsViewManager()
  : mDelayedResize(NSCOORD_NONE, NSCOORD_NONE)
{
  mRootViewManager = this;
  if (gViewManagers == nullptr) {
    NS_ASSERTION(mVMCount == 0, "View Manager count is incorrect");
    // Create an array to hold a list of view managers
    gViewManagers = new nsVoidArray;
  }
 
  gViewManagers->AppendElement(this);

  ++mVMCount;

  // NOTE:  we use a zeroing operator new, so all data members are
  // assumed to be cleared here.
  mHasPendingWidgetGeometryChanges = false;
  mRecursiveRefreshPending = false;
}

nsViewManager::~nsViewManager()
{
  if (mRootView) {
    // Destroy any remaining views
    mRootView->Destroy();
    mRootView = nullptr;
  }

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
   
    NS_ASSERTION(gViewManagers != nullptr, "About to delete null gViewManagers");
    delete gViewManagers;
    gViewManagers = nullptr;
  }

  mPresShell = nullptr;
}

// We don't hold a reference to the presentation context because it
// holds a reference to us.
nsresult
nsViewManager::Init(nsDeviceContext* aContext)
{
  NS_PRECONDITION(nullptr != aContext, "null ptr");

  if (nullptr == aContext) {
    return NS_ERROR_NULL_POINTER;
  }
  if (nullptr != mContext) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  mContext = aContext;

  return NS_OK;
}

nsView*
nsViewManager::CreateView(const nsRect& aBounds,
                          nsView* aParent,
                          nsViewVisibility aVisibilityFlag)
{
  nsView *v = new nsView(this, aVisibilityFlag);
  v->SetParent(aParent);
  v->SetPosition(aBounds.x, aBounds.y);
  nsRect dim(0, 0, aBounds.width, aBounds.height);
  v->SetDimensions(dim, false);
  return v;
}

void
nsViewManager::SetRootView(nsView *aView)
{
  NS_PRECONDITION(!aView || aView->GetViewManager() == this,
                  "Unexpected viewmanager on root view");
  
  // Do NOT destroy the current root view. It's the caller's responsibility
  // to destroy it
  mRootView = aView;

  if (mRootView) {
    nsView* parent = mRootView->GetParent();
    if (parent) {
      // Calling InsertChild on |parent| will InvalidateHierarchy() on us, so
      // no need to set mRootViewManager ourselves here.
      parent->InsertChild(mRootView, nullptr);
    } else {
      InvalidateHierarchy();
    }

    mRootView->SetZIndex(false, 0, false);
  }
  // Else don't touch mRootViewManager
}

void
nsViewManager::GetWindowDimensions(nscoord *aWidth, nscoord *aHeight)
{
  if (nullptr != mRootView) {
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
}

void nsViewManager::DoSetWindowDimensions(nscoord aWidth, nscoord aHeight)
{
  nsRect oldDim = mRootView->GetDimensions();
  nsRect newDim(0, 0, aWidth, aHeight);
  // We care about resizes even when one dimension is already zero.
  if (!oldDim.IsEqualEdges(newDim)) {
    // Don't resize the widget. It is already being set elsewhere.
    mRootView->SetDimensions(newDim, true, false);
    if (mPresShell)
      mPresShell->ResizeReflow(aWidth, aHeight);
  }
}

void
nsViewManager::SetWindowDimensions(nscoord aWidth, nscoord aHeight)
{
  if (mRootView) {
    if (mRootView->IsEffectivelyVisible() && mPresShell && mPresShell->IsVisible()) {
      if (mDelayedResize != nsSize(NSCOORD_NONE, NSCOORD_NONE) &&
          mDelayedResize != nsSize(aWidth, aHeight)) {
        // We have a delayed resize; that now obsolete size may already have
        // been flushed to the PresContext so we need to update the PresContext
        // with the new size because if the new size is exactly the same as the
        // root view's current size then DoSetWindowDimensions will not
        // request a resize reflow (which would correct it). See bug 617076.
        mDelayedResize = nsSize(aWidth, aHeight);
        FlushDelayedResize(false);
      }
      mDelayedResize.SizeTo(NSCOORD_NONE, NSCOORD_NONE);
      DoSetWindowDimensions(aWidth, aHeight);
    } else {
      mDelayedResize.SizeTo(aWidth, aHeight);
      if (mPresShell && mPresShell->GetDocument()) {
        mPresShell->GetDocument()->SetNeedStyleFlush();
      }
    }
  }
}

void
nsViewManager::FlushDelayedResize(bool aDoReflow)
{
  if (mDelayedResize != nsSize(NSCOORD_NONE, NSCOORD_NONE)) {
    if (aDoReflow) {
      DoSetWindowDimensions(mDelayedResize.width, mDelayedResize.height);
      mDelayedResize.SizeTo(NSCOORD_NONE, NSCOORD_NONE);
    } else if (mPresShell) {
      nsPresContext* presContext = mPresShell->GetPresContext();
      if (presContext) {
        presContext->SetVisibleArea(nsRect(nsPoint(0, 0), mDelayedResize));
      }
    }
  }
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

nsView* nsViewManager::GetDisplayRootFor(nsView* aView)
{
  nsView *displayRoot = aView;
  for (;;) {
    nsView *displayParent = displayRoot->GetParent();
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
void nsViewManager::Refresh(nsView *aView, const nsIntRegion& aRegion)
{
  NS_ASSERTION(aView->GetViewManager() == this, "wrong view manager");

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
  
  nsIWidget *widget = aView->GetWidget();
  if (!widget) {
    return;
  }

  NS_ASSERTION(!IsPainting(), "recursive painting not permitted");
  if (IsPainting()) {
    RootViewManager()->mRecursiveRefreshPending = true;
    return;
  }  

  {
    nsAutoScriptBlocker scriptBlocker;
    SetPainting(true);

    NS_ASSERTION(GetDisplayRootFor(aView) == aView,
                 "Widgets that we paint must all be display roots");

    if (mPresShell) {
#ifdef DEBUG_INVALIDATIONS
      printf("--COMPOSITE-- %p\n", mPresShell);
#endif
      mPresShell->Paint(aView, damageRegion,
                        nsIPresShell::PAINT_COMPOSITE);
#ifdef DEBUG_INVALIDATIONS
      printf("--ENDCOMPOSITE--\n");
#endif
      mozilla::StartupTimeline::RecordOnce(mozilla::StartupTimeline::FIRST_PAINT);
    }

    SetPainting(false);
  }

  if (RootViewManager()->mRecursiveRefreshPending) {
    RootViewManager()->mRecursiveRefreshPending = false;
    InvalidateAllViews();
  }
}

void nsViewManager::ProcessPendingUpdatesForView(nsView* aView,
                                                 bool aFlushDirtyRegion)
{
  NS_ASSERTION(IsRootVM(), "Updates will be missed");

  // Protect against a null-view.
  if (!aView) {
    return;
  }

  if (aView->HasWidget()) {
    aView->ResetWidgetBounds(false, true);
  }

  // process pending updates in child view.
  for (nsView* childView = aView->GetFirstChild(); childView;
       childView = childView->GetNextSibling()) {
    ProcessPendingUpdatesForView(childView, aFlushDirtyRegion);
  }

  // Push out updates after we've processed the children; ensures that
  // damage is applied based on the final widget geometry
  if (aFlushDirtyRegion) {
    nsIWidget *widget = aView->GetWidget();
    if (widget && widget->NeedsPaint()) {
      // If an ancestor widget was hidden and then shown, we could
      // have a delayed resize to handle.
      for (nsViewManager *vm = this; vm;
           vm = vm->mRootView->GetParent()
                  ? vm->mRootView->GetParent()->GetViewManager()
                  : nullptr) {
        if (vm->mDelayedResize != nsSize(NSCOORD_NONE, NSCOORD_NONE) &&
            vm->mRootView->IsEffectivelyVisible() &&
            mPresShell && mPresShell->IsVisible()) {
          vm->FlushDelayedResize(true);
          vm->InvalidateView(vm->mRootView);
        }
      }

      NS_ASSERTION(aView->HasWidget(), "Must have a widget!");

#ifdef DEBUG_INVALIDATIONS
      printf("---- PAINT START ----PresShell(%p), nsView(%p), nsIWidget(%p)\n", mPresShell, aView, widget);
#endif
      nsAutoScriptBlocker scriptBlocker;
      NS_ASSERTION(aView->HasWidget(), "Must have a widget!");
      aView->GetWidget()->WillPaint();
      SetPainting(true);
      mPresShell->Paint(aView, nsRegion(),
                        nsIPresShell::PAINT_LAYERS);
#ifdef DEBUG_INVALIDATIONS
      printf("---- PAINT END ----\n");
#endif
      aView->SetForcedRepaint(false);
      SetPainting(false);
    }
    FlushDirtyRegionToWidget(aView);
  }
}

void nsViewManager::FlushDirtyRegionToWidget(nsView* aView)
{
  if (!aView->HasNonEmptyDirtyRegion())
    return;

  nsRegion* dirtyRegion = aView->GetDirtyRegion();
  nsView* nearestViewWithWidget = aView;
  while (!nearestViewWithWidget->HasWidget() &&
         nearestViewWithWidget->GetParent()) {
    nearestViewWithWidget = nearestViewWithWidget->GetParent();
  }
  nsRegion r =
    ConvertRegionBetweenViews(*dirtyRegion, aView, nearestViewWithWidget);
  nsViewManager* widgetVM = nearestViewWithWidget->GetViewManager();
  widgetVM->InvalidateWidgetArea(nearestViewWithWidget, r);
  dirtyRegion->SetEmpty();
}

void
nsViewManager::InvalidateView(nsView *aView)
{
  // Mark the entire view as damaged
  InvalidateView(aView, aView->GetDimensions());
}

static void
AddDirtyRegion(nsView *aView, const nsRegion &aDamagedRegion)
{
  nsRegion* dirtyRegion = aView->GetDirtyRegion();
  if (!dirtyRegion)
    return;

  dirtyRegion->Or(*dirtyRegion, aDamagedRegion);
  dirtyRegion->SimplifyOutward(8);
}

void
nsViewManager::PostPendingUpdate()
{
  nsViewManager* rootVM = RootViewManager();
  rootVM->mHasPendingWidgetGeometryChanges = true;
  if (rootVM->mPresShell) {
    rootVM->mPresShell->ScheduleViewManagerFlush();
  }
}

/**
 * @param aDamagedRegion this region, relative to aWidgetView, is invalidated in
 * every widget child of aWidgetView, plus aWidgetView's own widget
 */
void
nsViewManager::InvalidateWidgetArea(nsView *aWidgetView,
                                    const nsRegion &aDamagedRegion)
{
  NS_ASSERTION(aWidgetView->GetViewManager() == this,
               "InvalidateWidgetArea called on view we don't own");
  nsIWidget* widget = aWidgetView->GetWidget();

#if 0
  nsRect dbgBounds = aDamagedRegion.GetBounds();
  printf("InvalidateWidgetArea view:%X (%d) widget:%X region: %d, %d, %d, %d\n",
    aWidgetView, aWidgetView->IsAttachedToTopLevel(),
    widget, dbgBounds.x, dbgBounds.y, dbgBounds.width, dbgBounds.height);
#endif

  // If the widget is hidden, it don't cover nothing
  if (widget && !widget->IsVisible()) {
    return;
  }

  if (!widget) {
    // The root view or a scrolling view might not have a widget
    // (for example, during printing). We get here when we scroll
    // during printing to show selected options in a listbox, for example.
    return;
  }

  // Update all child widgets with the damage. In the process,
  // accumulate the union of all the child widget areas, or at least
  // some subset of that.
  nsRegion children;
  if (widget->GetTransparencyMode() != eTransparencyTransparent) {
    for (nsIWidget* childWidget = widget->GetFirstChild();
         childWidget;
         childWidget = childWidget->GetNextSibling()) {
      nsView* view = nsView::GetViewFor(childWidget);
      NS_ASSERTION(view != aWidgetView, "will recur infinitely");
      nsWindowType type;
      childWidget->GetWindowType(type);
      if (view && childWidget->IsVisible() && type != eWindowType_popup) {
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
        for (uint32_t i = 0; i < clipRects.Length(); ++i) {
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
  leftOver.Sub(aDamagedRegion, children);

  if (!leftOver.IsEmpty()) {
    const nsRect* r;
    for (nsRegionRectIterator iter(leftOver); (r = iter.Next());) {
      nsIntRect bounds = ViewToWidget(aWidgetView, *r);
      widget->Invalidate(bounds);
    }
  }
}

static bool
ShouldIgnoreInvalidation(nsViewManager* aVM)
{
  while (aVM) {
    nsIPresShell* shell = aVM->GetPresShell();
    if (!shell || shell->ShouldIgnoreInvalidation()) {
      return true;
    }
    nsView* view = aVM->GetRootView()->GetParent();
    aVM = view ? view->GetViewManager() : nullptr;
  }
  return false;
}

void
nsViewManager::InvalidateView(nsView *aView, const nsRect &aRect)
{
  // If painting is suppressed in the presshell or an ancestor drop all
  // invalidates, it will invalidate everything when it unsuppresses.
  if (ShouldIgnoreInvalidation(this)) {
    return;
  }

  InvalidateViewNoSuppression(aView, aRect);
}

void
nsViewManager::InvalidateViewNoSuppression(nsView *aView,
                                           const nsRect &aRect)
{
  NS_PRECONDITION(nullptr != aView, "null view");

  NS_ASSERTION(aView->GetViewManager() == this,
               "InvalidateViewNoSuppression called on view we don't own");

  nsRect damagedRect(aRect);
  if (damagedRect.IsEmpty()) {
    return;
  }

  nsView* displayRoot = GetDisplayRootFor(aView);
  nsViewManager* displayRootVM = displayRoot->GetViewManager();
  // Propagate the update to the displayRoot, since iframes, for example,
  // can overlap each other and be translucent.  So we have to possibly
  // invalidate our rect in each of the widgets we have lying about.
  damagedRect.MoveBy(aView->GetOffsetTo(displayRoot));
  int32_t rootAPD = displayRootVM->AppUnitsPerDevPixel();
  int32_t APD = AppUnitsPerDevPixel();
  damagedRect = damagedRect.ConvertAppUnitsRoundOut(APD, rootAPD);

  // accumulate this rectangle in the view's dirty region, so we can
  // process it later.
  AddDirtyRegion(displayRoot, nsRegion(damagedRect));
}

void
nsViewManager::InvalidateAllViews()
{
  if (RootViewManager() != this) {
    return RootViewManager()->InvalidateAllViews();
  }
  
  InvalidateViews(mRootView);
}

void nsViewManager::InvalidateViews(nsView *aView)
{
  // Invalidate this view.
  InvalidateView(aView);

  // Invalidate all children as well.
  nsView* childView = aView->GetFirstChild();
  while (nullptr != childView)  {
    childView->GetViewManager()->InvalidateViews(childView);
    childView = childView->GetNextSibling();
  }
}

void nsViewManager::WillPaintWindow(nsIWidget* aWidget)
{
  if (aWidget) {
    nsView* view = nsView::GetViewFor(aWidget);
    if (view && view->ForcedRepaint()) {
      ProcessPendingUpdates();
      // Re-get the view pointer here since the ProcessPendingUpdates might have
      // destroyed it during CallWillPaintOnObservers.
      view = nsView::GetViewFor(aWidget);
      if (view) {
        view->SetForcedRepaint(false);
      }
    }
  }

  nsCOMPtr<nsIPresShell> shell = mPresShell;
  if (shell) {
    shell->WillPaintWindow();
  }
}

bool nsViewManager::PaintWindow(nsIWidget* aWidget, nsIntRegion aRegion,
                                uint32_t aFlags)
{
  if (!aWidget || !mContext)
    return false;

  NS_ASSERTION(IsPaintingAllowed(),
               "shouldn't be receiving paint events while painting is disallowed!");

  // Get the view pointer here since NS_WILL_PAINT might have
  // destroyed it during CallWillPaintOnObservers (bug 378273).
  nsView* view = nsView::GetViewFor(aWidget);
  if (view && !aRegion.IsEmpty()) {
    Refresh(view, aRegion);
  }

  return true;
}

void nsViewManager::DidPaintWindow()
{
  nsCOMPtr<nsIPresShell> shell = mPresShell;
  if (shell) {
    shell->DidPaintWindow();
  }
}

void
nsViewManager::DispatchEvent(nsGUIEvent *aEvent, nsView* aView, nsEventStatus* aStatus)
{
  PROFILER_LABEL("event", "nsViewManager::DispatchEvent");

  if ((NS_IS_MOUSE_EVENT(aEvent) &&
       // Ignore mouse events that we synthesize.
       static_cast<nsMouseEvent*>(aEvent)->reason == nsMouseEvent::eReal &&
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

  // Find the view whose coordinates system we're in.
  nsView* view = aView;
  bool dispatchUsingCoordinates = NS_IsEventUsingCoordinates(aEvent);
  if (dispatchUsingCoordinates) {
    // Will dispatch using coordinates. Pretty bogus but it's consistent
    // with what presshell does.
    view = GetDisplayRootFor(view);
  }

  // If the view has no frame, look for a view that does.
  nsIFrame* frame = view->GetFrame();
  if (!frame &&
      (dispatchUsingCoordinates || NS_IS_KEY_EVENT(aEvent) ||
       NS_IS_IME_RELATED_EVENT(aEvent) ||
       NS_IS_NON_RETARGETED_PLUGIN_EVENT(aEvent) ||
       aEvent->message == NS_PLUGIN_ACTIVATE ||
       aEvent->message == NS_PLUGIN_FOCUS ||
       aEvent->message == NS_PLUGIN_RESOLUTION_CHANGED)) {
    while (view && !view->GetFrame()) {
      view = view->GetParent();
    }

    if (view) {
      frame = view->GetFrame();
    }
  }

  if (nullptr != frame) {
    // Hold a refcount to the presshell. The continued existence of the
    // presshell will delay deletion of this view hierarchy should the event
    // want to cause its destruction in, say, some JavaScript event handler.
    nsCOMPtr<nsIPresShell> shell = view->GetViewManager()->GetPresShell();
    if (shell) {
      shell->HandleEvent(frame, aEvent, false, aStatus);
	  return;
    }
  }

  *aStatus = nsEventStatus_eIgnore;
}

// Recursively reparent widgets if necessary 

void nsViewManager::ReparentChildWidgets(nsView* aView, nsIWidget *aNewWidget)
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

  for (nsView *kid = aView->GetFirstChild(); kid; kid = kid->GetNextSibling()) {
    ReparentChildWidgets(kid, aNewWidget);
  }
}

// Reparent a view and its descendant views widgets if necessary

void nsViewManager::ReparentWidgets(nsView* aView, nsView *aParent)
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
  if (aView->HasWidget() || aView->GetFirstChild()) {
    nsIWidget* parentWidget = aParent->GetNearestWidget(nullptr);
    if (parentWidget) {
      ReparentChildWidgets(aView, parentWidget);
      return;
    }
    NS_WARNING("Can not find a widget for the parent view");
  }
}

void
nsViewManager::InsertChild(nsView *aParent, nsView *aChild, nsView *aSibling,
                           bool aAfter)
{
  NS_PRECONDITION(nullptr != aParent, "null ptr");
  NS_PRECONDITION(nullptr != aChild, "null ptr");
  NS_ASSERTION(aSibling == nullptr || aSibling->GetParent() == aParent,
               "tried to insert view with invalid sibling");
  NS_ASSERTION(!IsViewInserted(aChild), "tried to insert an already-inserted view");

  if ((nullptr != aParent) && (nullptr != aChild))
    {
      // if aAfter is set, we will insert the child after 'prev' (i.e. after 'kid' in document
      // order, otherwise after 'kid' (i.e. before 'kid' in document order).

      if (nullptr == aSibling) {
        if (aAfter) {
          // insert at end of document order, i.e., before first view
          // this is the common case, by far
          aParent->InsertChild(aChild, nullptr);
          ReparentWidgets(aChild, aParent);
        } else {
          // insert at beginning of document order, i.e., after last view
          nsView *kid = aParent->GetFirstChild();
          nsView *prev = nullptr;
          while (kid) {
            prev = kid;
            kid = kid->GetNextSibling();
          }
          // prev is last view or null if there are no children
          aParent->InsertChild(aChild, prev);
          ReparentWidgets(aChild, aParent);
        }
      } else {
        nsView *kid = aParent->GetFirstChild();
        nsView *prev = nullptr;
        while (kid && aSibling != kid) {
          //get the next sibling view
          prev = kid;
          kid = kid->GetNextSibling();
        }
        NS_ASSERTION(kid != nullptr,
                     "couldn't find sibling in child list");
        if (aAfter) {
          // insert after 'kid' in document order, i.e. before in view order
          aParent->InsertChild(aChild, prev);
          ReparentWidgets(aChild, aParent);
        } else {
          // insert before 'kid' in document order, i.e. after in view order
          aParent->InsertChild(aChild, kid);
          ReparentWidgets(aChild, aParent);
        }
      }

      // if the parent view is marked as "floating", make the newly added view float as well.
      if (aParent->GetFloating())
        aChild->SetFloating(true);

      //and mark this area as dirty if the view is visible...

      if (nsViewVisibility_kHide != aChild->GetVisibility())
        aChild->GetViewManager()->InvalidateView(aChild);
    }
}

void
nsViewManager::InsertChild(nsView *aParent, nsView *aChild, int32_t aZIndex)
{
  // no-one really calls this with anything other than aZIndex == 0 on a fresh view
  // XXX this method should simply be eliminated and its callers redirected to the real method
  SetViewZIndex(aChild, false, aZIndex, false);
  InsertChild(aParent, aChild, nullptr, true);
}

void
nsViewManager::RemoveChild(nsView *aChild)
{
  NS_ASSERTION(aChild, "aChild must not be null");

  nsView* parent = aChild->GetParent();

  if (nullptr != parent) {
    NS_ASSERTION(aChild->GetViewManager() == this ||
                 parent->GetViewManager() == this, "wrong view manager");
    aChild->GetViewManager()->InvalidateView(aChild);
    parent->RemoveChild(aChild);
  }
}

void
nsViewManager::MoveViewTo(nsView *aView, nscoord aX, nscoord aY)
{
  NS_ASSERTION(aView->GetViewManager() == this, "wrong view manager");
  nsPoint oldPt = aView->GetPosition();
  nsRect oldBounds = aView->GetBoundsInParentUnits();
  aView->SetPosition(aX, aY);

  // only do damage control if the view is visible

  if ((aX != oldPt.x) || (aY != oldPt.y)) {
    if (aView->GetVisibility() != nsViewVisibility_kHide) {
      nsView* parentView = aView->GetParent();
      if (parentView) {
        nsViewManager* parentVM = parentView->GetViewManager();
        parentVM->InvalidateView(parentView, oldBounds);
        parentVM->InvalidateView(parentView, aView->GetBoundsInParentUnits());
      }
    }
  }
}

void nsViewManager::InvalidateHorizontalBandDifference(nsView *aView, const nsRect& aRect, const nsRect& aCutOut,
  nscoord aY1, nscoord aY2, bool aInCutOut) {
  nscoord height = aY2 - aY1;
  if (aRect.x < aCutOut.x) {
    nsRect r(aRect.x, aY1, aCutOut.x - aRect.x, height);
    InvalidateView(aView, r);
  }
  if (!aInCutOut && aCutOut.x < aCutOut.XMost()) {
    nsRect r(aCutOut.x, aY1, aCutOut.width, height);
    InvalidateView(aView, r);
  }
  if (aCutOut.XMost() < aRect.XMost()) {
    nsRect r(aCutOut.XMost(), aY1, aRect.XMost() - aCutOut.XMost(), height);
    InvalidateView(aView, r);
  }
}

void nsViewManager::InvalidateRectDifference(nsView *aView, const nsRect& aRect, const nsRect& aCutOut) {
  NS_ASSERTION(aView->GetViewManager() == this,
               "InvalidateRectDifference called on view we don't own");
  if (aRect.y < aCutOut.y) {
    InvalidateHorizontalBandDifference(aView, aRect, aCutOut, aRect.y, aCutOut.y, false);
  }
  if (aCutOut.y < aCutOut.YMost()) {
    InvalidateHorizontalBandDifference(aView, aRect, aCutOut, aCutOut.y, aCutOut.YMost(), true);
  }
  if (aCutOut.YMost() < aRect.YMost()) {
    InvalidateHorizontalBandDifference(aView, aRect, aCutOut, aCutOut.YMost(), aRect.YMost(), false);
  }
}

void
nsViewManager::ResizeView(nsView *aView, const nsRect &aRect, bool aRepaintExposedAreaOnly)
{
  NS_ASSERTION(aView->GetViewManager() == this, "wrong view manager");

  nsRect oldDimensions = aView->GetDimensions();
  if (!oldDimensions.IsEqualEdges(aRect)) {
    // resize the view.
    // Prevent Invalidation of hidden views 
    if (aView->GetVisibility() == nsViewVisibility_kHide) {
      aView->SetDimensions(aRect, false);
    } else {
      nsView* parentView = aView->GetParent();
      if (!parentView) {
        parentView = aView;
      }
      nsRect oldBounds = aView->GetBoundsInParentUnits();
      aView->SetDimensions(aRect, true);
      nsViewManager* parentVM = parentView->GetViewManager();
      if (!aRepaintExposedAreaOnly) {
        // Invalidate the union of the old and new size
        InvalidateView(aView, aRect);
        parentVM->InvalidateView(parentView, oldBounds);
      } else {
        InvalidateRectDifference(aView, aRect, oldDimensions);
        nsRect newBounds = aView->GetBoundsInParentUnits();
        parentVM->InvalidateRectDifference(parentView, oldBounds, newBounds);
      } 
    }
  }

  // Note that if layout resizes the view and the view has a custom clip
  // region set, then we expect layout to update the clip region too. Thus
  // in the case where mClipRect has been optimized away to just be a null
  // pointer, and this resize is implicitly changing the clip rect, it's OK
  // because layout will change it back again if necessary.
}

void
nsViewManager::SetViewFloating(nsView *aView, bool aFloating)
{
  NS_ASSERTION(!(nullptr == aView), "no view");

  aView->SetFloating(aFloating);
}

void
nsViewManager::SetViewVisibility(nsView *aView, nsViewVisibility aVisible)
{
  NS_ASSERTION(aView->GetViewManager() == this, "wrong view manager");

  if (aVisible != aView->GetVisibility()) {
    aView->SetVisibility(aVisible);

    if (IsViewInserted(aView)) {
      if (!aView->HasWidget()) {
        if (nsViewVisibility_kHide == aVisible) {
          nsView* parentView = aView->GetParent();
          if (parentView) {
            parentView->GetViewManager()->
              InvalidateView(parentView, aView->GetBoundsInParentUnits());
          }
        }
        else {
          InvalidateView(aView);
        }
      }
    }
  }
}

bool nsViewManager::IsViewInserted(nsView *aView)
{
  if (mRootView == aView) {
    return true;
  } else if (aView->GetParent() == nullptr) {
    return false;
  } else {
    nsView* view = aView->GetParent()->GetFirstChild();
    while (view != nullptr) {
      if (view == aView) {
        return true;
      }
      view = view->GetNextSibling();
    }
    return false;
  }
}

void
nsViewManager::SetViewZIndex(nsView *aView, bool aAutoZIndex, int32_t aZIndex, bool aTopMost)
{
  NS_ASSERTION((aView != nullptr), "no view");

  // don't allow the root view's z-index to be changed. It should always be zero.
  // This could be removed and replaced with a style rule, or just removed altogether, with interesting consequences
  if (aView == mRootView) {
    return;
  }

  bool oldTopMost = aView->IsTopMost();
  bool oldIsAuto = aView->GetZIndexIsAuto();

  if (aAutoZIndex) {
    aZIndex = 0;
  }

  int32_t oldidx = aView->GetZIndex();
  aView->SetZIndex(aAutoZIndex, aZIndex, aTopMost);

  if (oldidx != aZIndex || oldTopMost != aTopMost ||
      oldIsAuto != aAutoZIndex) {
    InvalidateView(aView);
  }
}

nsViewManager*
nsViewManager::IncrementDisableRefreshCount()
{
  if (!IsRootVM()) {
    return RootViewManager()->IncrementDisableRefreshCount();
  }

  ++mRefreshDisableCount;

  return this;
}

void
nsViewManager::DecrementDisableRefreshCount()
{
  NS_ASSERTION(IsRootVM(), "Should only be called on root");
  --mRefreshDisableCount;
  NS_ASSERTION(mRefreshDisableCount >= 0, "Invalid refresh disable count!");
}

void
nsViewManager::GetRootWidget(nsIWidget **aWidget)
{
  if (!mRootView) {
    *aWidget = nullptr;
    return;
  }
  if (mRootView->HasWidget()) {
    *aWidget = mRootView->GetWidget();
    NS_ADDREF(*aWidget);
    return;
  }
  if (mRootView->GetParent()) {
    mRootView->GetParent()->GetViewManager()->GetRootWidget(aWidget);
    return;
  }
  *aWidget = nullptr;
}

nsIntRect nsViewManager::ViewToWidget(nsView *aView, const nsRect &aRect) const
{
  NS_ASSERTION(aView->GetViewManager() == this, "wrong view manager");

  // account for the view's origin not lining up with the widget's
  nsRect rect = aRect + aView->ViewToWidgetOffset();

  // finally, convert to device coordinates.
  return rect.ToOutsidePixels(AppUnitsPerDevPixel());
}

void
nsViewManager::IsPainting(bool& aIsPainting)
{
  aIsPainting = IsPainting();
}

void
nsViewManager::ProcessPendingUpdates()
{
  if (!IsRootVM()) {
    RootViewManager()->ProcessPendingUpdates();
    return;
  }

  mPresShell->GetPresContext()->RefreshDriver()->RevokeViewManagerFlush();

  // Flush things like reflows by calling WillPaint on observer presShells.
  if (mPresShell) {
    CallWillPaintOnObservers();
  }
  ProcessPendingUpdatesForView(mRootView, true);
}

void
nsViewManager::UpdateWidgetGeometry()
{
  if (!IsRootVM()) {
    RootViewManager()->UpdateWidgetGeometry();
    return;
  }

  if (mHasPendingWidgetGeometryChanges) {
    mHasPendingWidgetGeometryChanges = false;
    ProcessPendingUpdatesForView(mRootView, false);
  }
}

void
nsViewManager::CallWillPaintOnObservers()
{
  NS_PRECONDITION(IsRootVM(), "Must be root VM for this to be called!");

  int32_t index;
  for (index = 0; index < mVMCount; index++) {
    nsViewManager* vm = (nsViewManager*)gViewManagers->ElementAt(index);
    if (vm->RootViewManager() == this) {
      // One of our kids.
      if (vm->mRootView && vm->mRootView->IsEffectivelyVisible()) {
        nsCOMPtr<nsIPresShell> shell = vm->GetPresShell();
        if (shell) {
          shell->WillPaint();
        }
      }
    }
  }
}

void
nsViewManager::GetLastUserEventTime(uint32_t& aTime)
{
  aTime = gLastUserEventTime;
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
