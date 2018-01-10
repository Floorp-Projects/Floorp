/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsView.h"

#include "mozilla/Attributes.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Likely.h"
#include "mozilla/Poison.h"
#include "nsIWidget.h"
#include "nsViewManager.h"
#include "nsIFrame.h"
#include "nsPresArena.h"
#include "nsXULPopupManager.h"
#include "nsIWidgetListener.h"
#include "nsContentUtils.h" // for nsAutoScriptBlocker
#include "mozilla/TimelineConsumers.h"
#include "mozilla/CompositeTimelineMarker.h"

using namespace mozilla;

static bool sShowPreviousPage = true;

nsView::nsView(nsViewManager* aViewManager, nsViewVisibility aVisibility)
  : mViewManager(aViewManager)
  , mParent(nullptr)
  , mNextSibling(nullptr)
  , mFirstChild(nullptr)
  , mFrame(nullptr)
  , mDirtyRegion(nullptr)
  , mZIndex(0)
  , mVis(aVisibility)
  , mPosX(0)
  , mPosY(0)
  , mVFlags(0)
  , mWidgetIsTopLevel(false)
  , mForcedRepaint(false)
  , mNeedsWindowPropertiesSync(false)
{
  MOZ_COUNT_CTOR(nsView);

  // Views should be transparent by default. Not being transparent is
  // a promise that the view will paint all its pixels opaquely. Views
  // should make this promise explicitly by calling
  // SetViewContentTransparency.

  static bool sShowPreviousPageInitialized = false;
  if (!sShowPreviousPageInitialized) {
    Preferences::AddBoolVarCache(&sShowPreviousPage, "layout.show_previous_page", true);
    sShowPreviousPageInitialized = true;
  }
}

void nsView::DropMouseGrabbing()
{
  nsIPresShell* presShell = mViewManager->GetPresShell();
  if (presShell)
    presShell->ClearMouseCaptureOnView(this);
}

nsView::~nsView()
{
  MOZ_COUNT_DTOR(nsView);

  while (GetFirstChild())
  {
    nsView* child = GetFirstChild();
    if (child->GetViewManager() == mViewManager) {
      child->Destroy();
    } else {
      // just unhook it. Someone else will want to destroy this.
      RemoveChild(child);
    }
  }

  if (mViewManager)
  {
    DropMouseGrabbing();

    nsView *rootView = mViewManager->GetRootView();

    if (rootView)
    {
      // Root views can have parents!
      if (mParent)
      {
        mViewManager->RemoveChild(this);
      }

      if (rootView == this)
      {
        // Inform the view manager that the root view has gone away...
        mViewManager->SetRootView(nullptr);
      }
    }
    else if (mParent)
    {
      mParent->RemoveChild(this);
    }

    mViewManager = nullptr;
  }
  else if (mParent)
  {
    mParent->RemoveChild(this);
  }

  if (mPreviousWindow) {
    mPreviousWindow->SetPreviouslyAttachedWidgetListener(nullptr);
  }

  // Destroy and release the widget
  DestroyWidget();

  delete mDirtyRegion;
}

class DestroyWidgetRunnable : public Runnable {
public:
  NS_DECL_NSIRUNNABLE

  explicit DestroyWidgetRunnable(nsIWidget* aWidget)
    : mozilla::Runnable("DestroyWidgetRunnable")
    , mWidget(aWidget)
  {
  }

private:
  nsCOMPtr<nsIWidget> mWidget;
};

NS_IMETHODIMP DestroyWidgetRunnable::Run()
{
  mWidget->Destroy();
  mWidget = nullptr;
  return NS_OK;
}


void nsView::DestroyWidget()
{
  if (mWindow)
  {
    // If we are not attached to a base window, we're going to tear down our
    // widget here. However, if we're attached to somebody elses widget, we
    // want to leave the widget alone: don't reset the client data or call
    // Destroy. Just clear our event view ptr and free our reference to it.
    if (mWidgetIsTopLevel) {
      mWindow->SetAttachedWidgetListener(nullptr);
    }
    else {
      mWindow->SetWidgetListener(nullptr);

      nsCOMPtr<nsIRunnable> widgetDestroyer =
        new DestroyWidgetRunnable(mWindow);

      // Don't leak if we happen to arrive here after the main thread
      // has disappeared.
      nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
      if (mainThread) {
        mainThread->Dispatch(widgetDestroyer.forget(), NS_DISPATCH_NORMAL);
      }
    }

    mWindow = nullptr;
  }
}

nsView* nsView::GetViewFor(nsIWidget* aWidget)
{
  NS_PRECONDITION(nullptr != aWidget, "null widget ptr");

  nsIWidgetListener* listener = aWidget->GetWidgetListener();
  if (listener) {
    nsView* view = listener->GetView();
    if (view)
      return view;
  }

  listener = aWidget->GetAttachedWidgetListener();
  return listener ? listener->GetView() : nullptr;
}

void nsView::Destroy()
{
  this->~nsView();
  mozWritePoison(this, sizeof(*this));
  nsView::operator delete(this);
}

void nsView::SetPosition(nscoord aX, nscoord aY)
{
  mDimBounds.MoveBy(aX - mPosX, aY - mPosY);
  mPosX = aX;
  mPosY = aY;

  NS_ASSERTION(GetParent() || (aX == 0 && aY == 0),
               "Don't try to move the root widget to something non-zero");

  ResetWidgetBounds(true, false);
}

void nsView::ResetWidgetBounds(bool aRecurse, bool aForceSync)
{
  if (mWindow) {
    if (!aForceSync) {
      // Don't change widget geometry synchronously, since that can
      // cause synchronous painting.
      mViewManager->PostPendingUpdate();
    } else {
      DoResetWidgetBounds(false, true);
    }
    return;
  }

  if (aRecurse) {
    // reposition any widgets under this view
    for (nsView* v = GetFirstChild(); v; v = v->GetNextSibling()) {
      v->ResetWidgetBounds(true, aForceSync);
    }
  }
}

bool nsView::IsEffectivelyVisible()
{
  for (nsView* v = this; v; v = v->mParent) {
    if (v->GetVisibility() == nsViewVisibility_kHide)
      return false;
  }
  return true;
}

LayoutDeviceIntRect nsView::CalcWidgetBounds(nsWindowType aType)
{
  int32_t p2a = mViewManager->AppUnitsPerDevPixel();

  nsRect viewBounds(mDimBounds);

  nsView* parent = GetParent();
  nsIWidget* parentWidget = nullptr;
  if (parent) {
    nsPoint offset;
    parentWidget = parent->GetNearestWidget(&offset, p2a);
    // make viewBounds be relative to the parent widget, in appunits
    viewBounds += offset;

    if (parentWidget && aType == eWindowType_popup &&
        IsEffectivelyVisible()) {
      // put offset into screen coordinates. (based on client area origin)
      LayoutDeviceIntPoint screenPoint = parentWidget->WidgetToScreenOffset();
      viewBounds += nsPoint(NSIntPixelsToAppUnits(screenPoint.x, p2a),
                            NSIntPixelsToAppUnits(screenPoint.y, p2a));
    }
  }

  // Compute widget bounds in device pixels
  LayoutDeviceIntRect newBounds =
    LayoutDeviceIntRect::FromUnknownRect(viewBounds.ToNearestPixels(p2a));

#if defined(XP_MACOSX) || defined(MOZ_WIDGET_GTK)
  // cocoa and GTK round widget coordinates to the nearest global "display
  // pixel" integer value. So we avoid fractional display pixel values by
  // rounding to the nearest value that won't yield a fractional display pixel.
  nsIWidget* widget = parentWidget ? parentWidget : mWindow.get();
  uint32_t round;
  if (aType == eWindowType_popup && widget &&
      ((round = widget->RoundsWidgetCoordinatesTo()) > 1)) {
    LayoutDeviceIntSize pixelRoundedSize = newBounds.Size();
    // round the top left and bottom right to the nearest round pixel
    newBounds.x = NSToIntRoundUp(NSAppUnitsToDoublePixels(viewBounds.x, p2a) / round) * round;
    newBounds.y = NSToIntRoundUp(NSAppUnitsToDoublePixels(viewBounds.y, p2a) / round) * round;
    newBounds.width =
      NSToIntRoundUp(NSAppUnitsToDoublePixels(viewBounds.XMost(), p2a) / round) * round - newBounds.x;
    newBounds.height =
      NSToIntRoundUp(NSAppUnitsToDoublePixels(viewBounds.YMost(), p2a) / round) * round - newBounds.y;
    // but if that makes the widget larger then our frame may not paint the
    // extra pixels, so reduce the size to the nearest round value
    if (newBounds.width > pixelRoundedSize.width) {
      newBounds.width -= round;
    }
    if (newBounds.height > pixelRoundedSize.height) {
      newBounds.height -= round;
    }
  }
#endif

  // Compute where the top-left of our widget ended up relative to the parent
  // widget, in appunits.
  nsPoint roundedOffset(NSIntPixelsToAppUnits(newBounds.X(), p2a),
                        NSIntPixelsToAppUnits(newBounds.Y(), p2a));

  // mViewToWidgetOffset is added to coordinates relative to the view origin
  // to get coordinates relative to the widget.
  // The view origin, relative to the parent widget, is at
  // (mPosX,mPosY) - mDimBounds.TopLeft() + viewBounds.TopLeft().
  // Our widget, relative to the parent widget, is roundedOffset.
  mViewToWidgetOffset = nsPoint(mPosX, mPosY)
    - mDimBounds.TopLeft() + viewBounds.TopLeft() - roundedOffset;

  return newBounds;
}

void nsView::DoResetWidgetBounds(bool aMoveOnly,
                                 bool aInvalidateChangedSize) {
  // The geometry of a root view's widget is controlled externally,
  // NOT by sizing or positioning the view
  if (mViewManager->GetRootView() == this) {
    return;
  }

  NS_PRECONDITION(mWindow, "Why was this called??");

  // Hold this ref to make sure it stays alive.
  nsCOMPtr<nsIWidget> widget = mWindow;

  // Stash a copy of these and use them so we can handle this being deleted (say
  // from sync painting/flushing from Show/Move/Resize on the widget).
  LayoutDeviceIntRect newBounds;
  RefPtr<nsDeviceContext> dx = mViewManager->GetDeviceContext();

  nsWindowType type = widget->WindowType();

  LayoutDeviceIntRect curBounds = widget->GetClientBounds();
  bool invisiblePopup = type == eWindowType_popup &&
                        ((curBounds.IsEmpty() && mDimBounds.IsEmpty()) ||
                         mVis == nsViewVisibility_kHide);

  if (invisiblePopup) {
    // We're going to hit the early exit below, avoid calling CalcWidgetBounds.
  } else {
    newBounds = CalcWidgetBounds(type);
  }

  bool curVisibility = widget->IsVisible();
  bool newVisibility = IsEffectivelyVisible();
  if (curVisibility && !newVisibility) {
    widget->Show(false);
  }

  if (invisiblePopup) {
    // Don't manipulate empty or hidden popup widgets. For example there's no
    // point moving hidden comboboxes around, or doing X server roundtrips
    // to compute their true screen position. This could mean that WidgetToScreen
    // operations on these widgets don't return up-to-date values, but popup
    // positions aren't reliable anyway because of correction to be on or off-screen.
    return;
  }

  bool changedPos = curBounds.TopLeft() != newBounds.TopLeft();
  bool changedSize = curBounds.Size() != newBounds.Size();

  // Child views are never attached to top level widgets, this is safe.

  // Coordinates are converted to desktop pixels for window Move/Resize APIs,
  // because of the potential for device-pixel coordinate spaces for mixed
  // hidpi/lodpi screens to overlap each other and result in bad placement
  // (bug 814434).
  DesktopToLayoutDeviceScale scale = dx->GetDesktopToDeviceScale();

  DesktopRect deskRect = newBounds / scale;
  if (changedPos) {
    if (changedSize && !aMoveOnly) {
      widget->ResizeClient(deskRect.X(), deskRect.Y(),
                           deskRect.Width(), deskRect.Height(),
                           aInvalidateChangedSize);
    } else {
      widget->MoveClient(deskRect.X(), deskRect.Y());
    }
  } else {
    if (changedSize && !aMoveOnly) {
      widget->ResizeClient(deskRect.Width(), deskRect.Height(),
                           aInvalidateChangedSize);
    } // else do nothing!
  }

  if (!curVisibility && newVisibility) {
    widget->Show(true);
  }
}

void nsView::SetDimensions(const nsRect& aRect, bool aPaint, bool aResizeWidget)
{
  nsRect dims = aRect;
  dims.MoveBy(mPosX, mPosY);

  // Don't use nsRect's operator== here, since it returns true when
  // both rects are empty even if they have different widths and we
  // have cases where that sort of thing matters to us.
  if (mDimBounds.TopLeft() == dims.TopLeft() &&
      mDimBounds.Size() == dims.Size()) {
    return;
  }

  mDimBounds = dims;

  if (aResizeWidget) {
    ResetWidgetBounds(false, false);
  }
}

void nsView::NotifyEffectiveVisibilityChanged(bool aEffectivelyVisible)
{
  if (!aEffectivelyVisible)
  {
    DropMouseGrabbing();
  }

  SetForcedRepaint(true);

  if (nullptr != mWindow)
  {
    ResetWidgetBounds(false, false);
  }

  for (nsView* child = mFirstChild; child; child = child->mNextSibling) {
    if (child->mVis == nsViewVisibility_kHide) {
      // It was effectively hidden and still is
      continue;
    }
    // Our child is visible if we are
    child->NotifyEffectiveVisibilityChanged(aEffectivelyVisible);
  }
}

void nsView::SetVisibility(nsViewVisibility aVisibility)
{
  mVis = aVisibility;
  NotifyEffectiveVisibilityChanged(IsEffectivelyVisible());
}

void nsView::SetFloating(bool aFloatingView)
{
	if (aFloatingView)
		mVFlags |= NS_VIEW_FLAG_FLOATING;
	else
		mVFlags &= ~NS_VIEW_FLAG_FLOATING;
}

void nsView::InvalidateHierarchy()
{
  if (mViewManager->GetRootView() == this)
    mViewManager->InvalidateHierarchy();

  for (nsView *child = mFirstChild; child; child = child->GetNextSibling())
    child->InvalidateHierarchy();
}

void nsView::InsertChild(nsView *aChild, nsView *aSibling)
{
  NS_PRECONDITION(nullptr != aChild, "null ptr");

  if (nullptr != aChild)
  {
    if (nullptr != aSibling)
    {
#ifdef DEBUG
      NS_ASSERTION(aSibling->GetParent() == this, "tried to insert view with invalid sibling");
#endif
      //insert after sibling
      aChild->SetNextSibling(aSibling->GetNextSibling());
      aSibling->SetNextSibling(aChild);
    }
    else
    {
      aChild->SetNextSibling(mFirstChild);
      mFirstChild = aChild;
    }
    aChild->SetParent(this);

    // If we just inserted a root view, then update the RootViewManager
    // on all view managers in the new subtree.

    nsViewManager *vm = aChild->GetViewManager();
    if (vm->GetRootView() == aChild)
    {
      aChild->InvalidateHierarchy();
    }
  }
}

void nsView::RemoveChild(nsView *child)
{
  NS_PRECONDITION(nullptr != child, "null ptr");

  if (nullptr != child)
  {
    nsView* prevKid = nullptr;
    nsView* kid = mFirstChild;
    DebugOnly<bool> found = false;
    while (nullptr != kid) {
      if (kid == child) {
        if (nullptr != prevKid) {
          prevKid->SetNextSibling(kid->GetNextSibling());
        } else {
          mFirstChild = kid->GetNextSibling();
        }
        child->SetParent(nullptr);
        found = true;
        break;
      }
      prevKid = kid;
	    kid = kid->GetNextSibling();
    }
    NS_ASSERTION(found, "tried to remove non child");

    // If we just removed a root view, then update the RootViewManager
    // on all view managers in the removed subtree.

    nsViewManager *vm = child->GetViewManager();
    if (vm->GetRootView() == child)
    {
      child->InvalidateHierarchy();
    }
  }
}

// Native widgets ultimately just can't deal with the awesome power of
// CSS2 z-index. However, we set the z-index on the widget anyway
// because in many simple common cases the widgets do end up in the
// right order. We set each widget's z-index to the z-index of the
// nearest ancestor that has non-auto z-index.
static void UpdateNativeWidgetZIndexes(nsView* aView, int32_t aZIndex)
{
  if (aView->HasWidget()) {
    nsIWidget* widget = aView->GetWidget();
    if (widget->GetZIndex() != aZIndex) {
      widget->SetZIndex(aZIndex);
    }
  } else {
    for (nsView* v = aView->GetFirstChild(); v; v = v->GetNextSibling()) {
      if (v->GetZIndexIsAuto()) {
        UpdateNativeWidgetZIndexes(v, aZIndex);
      }
    }
  }
}

static int32_t FindNonAutoZIndex(nsView* aView)
{
  while (aView) {
    if (!aView->GetZIndexIsAuto()) {
      return aView->GetZIndex();
    }
    aView = aView->GetParent();
  }
  return 0;
}

struct DefaultWidgetInitData : public nsWidgetInitData {
  DefaultWidgetInitData() : nsWidgetInitData()
  {
    mWindowType = eWindowType_child;
    clipChildren = true;
    clipSiblings = true;
  }
};

nsresult nsView::CreateWidget(nsWidgetInitData *aWidgetInitData,
                               bool aEnableDragDrop,
                               bool aResetVisibility)
{
  AssertNoWindow();
  MOZ_ASSERT(!aWidgetInitData ||
             aWidgetInitData->mWindowType != eWindowType_popup,
             "Use CreateWidgetForPopup");

  DefaultWidgetInitData defaultInitData;
  bool initDataPassedIn = !!aWidgetInitData;
  aWidgetInitData = aWidgetInitData ? aWidgetInitData : &defaultInitData;
  defaultInitData.mListenForResizes =
    (!initDataPassedIn && GetParent() &&
     GetParent()->GetViewManager() != mViewManager);

  LayoutDeviceIntRect trect = CalcWidgetBounds(aWidgetInitData->mWindowType);

  nsIWidget* parentWidget =
    GetParent() ? GetParent()->GetNearestWidget(nullptr) : nullptr;
  if (!parentWidget) {
    NS_ERROR("nsView::CreateWidget without suitable parent widget??");
    return NS_ERROR_FAILURE;
  }

  // XXX: using aForceUseIWidgetParent=true to preserve previous
  // semantics.  It's not clear that it's actually needed.
  mWindow = parentWidget->CreateChild(trect, aWidgetInitData, true);
  if (!mWindow) {
    return NS_ERROR_FAILURE;
  }

  InitializeWindow(aEnableDragDrop, aResetVisibility);

  return NS_OK;
}

nsresult nsView::CreateWidgetForParent(nsIWidget* aParentWidget,
                                        nsWidgetInitData *aWidgetInitData,
                                        bool aEnableDragDrop,
                                        bool aResetVisibility)
{
  AssertNoWindow();
  MOZ_ASSERT(!aWidgetInitData ||
             aWidgetInitData->mWindowType != eWindowType_popup,
             "Use CreateWidgetForPopup");
  MOZ_ASSERT(aParentWidget, "Parent widget required");

  DefaultWidgetInitData defaultInitData;
  aWidgetInitData = aWidgetInitData ? aWidgetInitData : &defaultInitData;

  LayoutDeviceIntRect trect = CalcWidgetBounds(aWidgetInitData->mWindowType);

  mWindow = aParentWidget->CreateChild(trect, aWidgetInitData);
  if (!mWindow) {
    return NS_ERROR_FAILURE;
  }

  InitializeWindow(aEnableDragDrop, aResetVisibility);

  return NS_OK;
}

nsresult nsView::CreateWidgetForPopup(nsWidgetInitData *aWidgetInitData,
                                       nsIWidget* aParentWidget,
                                       bool aEnableDragDrop,
                                       bool aResetVisibility)
{
  AssertNoWindow();
  MOZ_ASSERT(aWidgetInitData, "Widget init data required");
  MOZ_ASSERT(aWidgetInitData->mWindowType == eWindowType_popup,
             "Use one of the other CreateWidget methods");

  LayoutDeviceIntRect trect = CalcWidgetBounds(aWidgetInitData->mWindowType);

  // XXX/cjones: having these two separate creation cases seems ... um
  // ... unnecessary, but it's the way the old code did it.  Please
  // unify them by first finding a suitable parent nsIWidget, then
  // getting rid of aForceUseIWidgetParent.
  if (aParentWidget) {
    // XXX: using aForceUseIWidgetParent=true to preserve previous
    // semantics.  It's not clear that it's actually needed.
    mWindow = aParentWidget->CreateChild(trect, aWidgetInitData, true);
  }
  else {
    nsIWidget* nearestParent = GetParent() ? GetParent()->GetNearestWidget(nullptr)
                                           : nullptr;
    if (!nearestParent) {
      // Without a parent, we can't make a popup.  This can happen
      // when printing
      return NS_ERROR_FAILURE;
    }

    mWindow = nearestParent->CreateChild(trect, aWidgetInitData);
  }
  if (!mWindow) {
    return NS_ERROR_FAILURE;
  }

  InitializeWindow(aEnableDragDrop, aResetVisibility);

  return NS_OK;
}

void
nsView::InitializeWindow(bool aEnableDragDrop, bool aResetVisibility)
{
  MOZ_ASSERT(mWindow, "Must have a window to initialize");

  mWindow->SetWidgetListener(this);

  if (aEnableDragDrop) {
    mWindow->EnableDragDrop(true);
  }

  // propagate the z-index to the widget.
  UpdateNativeWidgetZIndexes(this, FindNonAutoZIndex(this));

  //make sure visibility state is accurate

  if (aResetVisibility) {
    SetVisibility(GetVisibility());
  }
}

void
nsView::SetNeedsWindowPropertiesSync()
{
  mNeedsWindowPropertiesSync = true;
  if (mViewManager) {
    mViewManager->PostPendingUpdate();
  }
}


// Attach to a top level widget and start receiving mirrored events.
nsresult nsView::AttachToTopLevelWidget(nsIWidget* aWidget)
{
  NS_PRECONDITION(nullptr != aWidget, "null widget ptr");
  /// XXXjimm This is a temporary workaround to an issue w/document
  // viewer (bug 513162).
  nsIWidgetListener* listener = aWidget->GetAttachedWidgetListener();
  if (listener) {
    nsView *oldView = listener->GetView();
    if (oldView) {
      oldView->DetachFromTopLevelWidget();
    }
  }

  // Note, the previous device context will be released. Detaching
  // will not restore the old one.
  aWidget->AttachViewToTopLevel(!nsIWidget::UsePuppetWidgets());

  mWindow = aWidget;

  mWindow->SetAttachedWidgetListener(this);
  if (mWindow->WindowType() != eWindowType_invisible) {
    nsresult rv = mWindow->AsyncEnableDragDrop(true);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mWidgetIsTopLevel = true;

  // Refresh the view bounds
  CalcWidgetBounds(mWindow->WindowType());

  return NS_OK;
}

// Detach this view from an attached widget.
nsresult nsView::DetachFromTopLevelWidget()
{
  NS_PRECONDITION(mWidgetIsTopLevel, "Not attached currently!");
  NS_PRECONDITION(mWindow, "null mWindow for DetachFromTopLevelWidget!");

  mWindow->SetAttachedWidgetListener(nullptr);
  nsIWidgetListener* listener = mWindow->GetPreviouslyAttachedWidgetListener();

  if (listener && listener->GetView()) {
    // Ensure the listener doesn't think it's being used anymore
    listener->GetView()->SetPreviousWidget(nullptr);
  }

  // If the new view's frame is paint suppressed then the window
  // will want to use us instead until that's done
  mWindow->SetPreviouslyAttachedWidgetListener(this);

  mPreviousWindow = mWindow;
  mWindow = nullptr;

  mWidgetIsTopLevel = false;

  return NS_OK;
}

void nsView::SetZIndex(bool aAuto, int32_t aZIndex)
{
  bool oldIsAuto = GetZIndexIsAuto();
  mVFlags = (mVFlags & ~NS_VIEW_FLAG_AUTO_ZINDEX) | (aAuto ? NS_VIEW_FLAG_AUTO_ZINDEX : 0);
  mZIndex = aZIndex;

  if (HasWidget() || !oldIsAuto || !aAuto) {
    UpdateNativeWidgetZIndexes(this, FindNonAutoZIndex(this));
  }
}

void nsView::AssertNoWindow()
{
  // XXX: it would be nice to make this a strong assert
  if (MOZ_UNLIKELY(mWindow)) {
    NS_ERROR("We already have a window for this view? BAD");
    mWindow->SetWidgetListener(nullptr);
    mWindow->Destroy();
    mWindow = nullptr;
  }
}

//
// internal window creation functions
//
void nsView::AttachWidgetEventHandler(nsIWidget* aWidget)
{
#ifdef DEBUG
  NS_ASSERTION(!aWidget->GetWidgetListener(), "Already have a widget listener");
#endif

  aWidget->SetWidgetListener(this);
}

void nsView::DetachWidgetEventHandler(nsIWidget* aWidget)
{
  NS_ASSERTION(!aWidget->GetWidgetListener() ||
               aWidget->GetWidgetListener()->GetView() == this, "Wrong view");
  aWidget->SetWidgetListener(nullptr);
}

#ifdef DEBUG
void nsView::List(FILE* out, int32_t aIndent) const
{
  int32_t i;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fprintf(out, "%p ", (void*)this);
  if (nullptr != mWindow) {
    nscoord p2a = mViewManager->AppUnitsPerDevPixel();
    LayoutDeviceIntRect rect = mWindow->GetClientBounds();
    nsRect windowBounds = LayoutDeviceIntRect::ToAppUnits(rect, p2a);
    rect = mWindow->GetBounds();
    nsRect nonclientBounds = LayoutDeviceIntRect::ToAppUnits(rect, p2a);
    nsrefcnt widgetRefCnt = mWindow.get()->AddRef() - 1;
    mWindow.get()->Release();
    int32_t Z = mWindow->GetZIndex();
    fprintf(out, "(widget=%p[%" PRIuPTR "] z=%d pos={%d,%d,%d,%d}) ",
            (void*)mWindow, widgetRefCnt, Z,
            nonclientBounds.X(), nonclientBounds.Y(),
            windowBounds.Width(), windowBounds.Height());
  }
  nsRect brect = GetBounds();
  fprintf(out, "{%d,%d,%d,%d}",
          brect.X(), brect.Y(), brect.Width(), brect.Height());
  fprintf(out, " z=%d vis=%d frame=%p <\n",
          mZIndex, mVis, static_cast<void*>(mFrame));
  for (nsView* kid = mFirstChild; kid; kid = kid->GetNextSibling()) {
    NS_ASSERTION(kid->GetParent() == this, "incorrect parent");
    kid->List(out, aIndent + 1);
  }
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fputs(">\n", out);
}
#endif // DEBUG

nsPoint nsView::GetOffsetTo(const nsView* aOther) const
{
  return GetOffsetTo(aOther, GetViewManager()->AppUnitsPerDevPixel());
}

nsPoint nsView::GetOffsetTo(const nsView* aOther, const int32_t aAPD) const
{
  MOZ_ASSERT(GetParent() || !aOther || aOther->GetParent() || this == aOther,
             "caller of (outer) GetOffsetTo must not pass unrelated views");
  // We accumulate the final result in offset
  nsPoint offset(0, 0);
  // The offset currently accumulated at the current APD
  nsPoint docOffset(0, 0);
  const nsView* v = this;
  nsViewManager* currVM = v->GetViewManager();
  int32_t currAPD = currVM->AppUnitsPerDevPixel();
  const nsView* root = nullptr;
  for ( ; v != aOther && v; root = v, v = v->GetParent()) {
    nsViewManager* newVM = v->GetViewManager();
    if (newVM != currVM) {
      int32_t newAPD = newVM->AppUnitsPerDevPixel();
      if (newAPD != currAPD) {
        offset += docOffset.ScaleToOtherAppUnits(currAPD, aAPD);
        docOffset.x = docOffset.y = 0;
        currAPD = newAPD;
      }
      currVM = newVM;
    }
    docOffset += v->GetPosition();
  }
  offset += docOffset.ScaleToOtherAppUnits(currAPD, aAPD);

  if (v != aOther) {
    // Looks like aOther wasn't an ancestor of |this|.  So now we have
    // the root-VM-relative position of |this| in |offset|.  Get the
    // root-VM-relative position of aOther and subtract it.
    nsPoint negOffset = aOther->GetOffsetTo(root, aAPD);
    offset -= negOffset;
  }

  return offset;
}

nsPoint nsView::GetOffsetToWidget(nsIWidget* aWidget) const
{
  nsPoint pt;
  // Get the view for widget
  nsView* widgetView = GetViewFor(aWidget);
  if (!widgetView) {
    return pt;
  }

  // Get the offset to the widget view in the widget view's APD
  // We get the offset in the widget view's APD first and then convert to our
  // APD afterwards so that we can include the widget view's ViewToWidgetOffset
  // in the sum in its native APD, and then convert the whole thing to our APD
  // so that we don't have to convert the APD of the relatively small
  // ViewToWidgetOffset by itself with a potentially large relative rounding
  // error.
  pt = -widgetView->GetOffsetTo(this);
  // Add in the offset to the widget.
  pt += widgetView->ViewToWidgetOffset();

  // Convert to our appunits.
  int32_t widgetAPD = widgetView->GetViewManager()->AppUnitsPerDevPixel();
  int32_t ourAPD = GetViewManager()->AppUnitsPerDevPixel();
  pt = pt.ScaleToOtherAppUnits(widgetAPD, ourAPD);
  return pt;
}

nsIWidget* nsView::GetNearestWidget(nsPoint* aOffset) const
{
  return GetNearestWidget(aOffset, GetViewManager()->AppUnitsPerDevPixel());
}

nsIWidget* nsView::GetNearestWidget(nsPoint* aOffset, const int32_t aAPD) const
{
  // aOffset is based on the view's position, which ignores any chrome on
  // attached parent widgets.

  // We accumulate the final result in pt
  nsPoint pt(0, 0);
  // The offset currently accumulated at the current APD
  nsPoint docPt(0,0);
  const nsView* v = this;
  nsViewManager* currVM = v->GetViewManager();
  int32_t currAPD = currVM->AppUnitsPerDevPixel();
  for ( ; v && !v->HasWidget(); v = v->GetParent()) {
    nsViewManager* newVM = v->GetViewManager();
    if (newVM != currVM) {
      int32_t newAPD = newVM->AppUnitsPerDevPixel();
      if (newAPD != currAPD) {
        pt += docPt.ScaleToOtherAppUnits(currAPD, aAPD);
        docPt.x = docPt.y = 0;
        currAPD = newAPD;
      }
      currVM = newVM;
    }
    docPt += v->GetPosition();
  }
  if (!v) {
    if (aOffset) {
      pt += docPt.ScaleToOtherAppUnits(currAPD, aAPD);
      *aOffset = pt;
    }
    return nullptr;
  }

  // pt is now the offset from v's origin to this view's origin.
  // We add the ViewToWidgetOffset to get the offset to the widget.
  if (aOffset) {
    docPt += v->ViewToWidgetOffset();
    pt += docPt.ScaleToOtherAppUnits(currAPD, aAPD);
    *aOffset = pt;
  }
  return v->GetWidget();
}

bool nsView::IsRoot() const
{
  NS_ASSERTION(mViewManager != nullptr," View manager is null in nsView::IsRoot()");
  return mViewManager->GetRootView() == this;
}

nsRect
nsView::GetBoundsInParentUnits() const
{
  nsView* parent = GetParent();
  nsViewManager* VM = GetViewManager();
  if (this != VM->GetRootView() || !parent) {
    return mDimBounds;
  }
  int32_t ourAPD = VM->AppUnitsPerDevPixel();
  int32_t parentAPD = parent->GetViewManager()->AppUnitsPerDevPixel();
  return mDimBounds.ScaleToOtherAppUnitsRoundOut(ourAPD, parentAPD);
}

nsPoint
nsView::ConvertFromParentCoords(nsPoint aPt) const
{
  const nsView* parent = GetParent();
  if (parent) {
    aPt = aPt.ScaleToOtherAppUnits(
      parent->GetViewManager()->AppUnitsPerDevPixel(),
      GetViewManager()->AppUnitsPerDevPixel());
  }
  aPt -= GetPosition();
  return aPt;
}

static bool
IsPopupWidget(nsIWidget* aWidget)
{
  return (aWidget->WindowType() == eWindowType_popup);
}

nsIPresShell*
nsView::GetPresShell()
{
  return GetViewManager()->GetPresShell();
}

bool
nsView::WindowMoved(nsIWidget* aWidget, int32_t x, int32_t y)
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm && IsPopupWidget(aWidget)) {
    pm->PopupMoved(mFrame, nsIntPoint(x, y));
    return true;
  }

  return false;
}

bool
nsView::WindowResized(nsIWidget* aWidget, int32_t aWidth, int32_t aHeight)
{
  // The root view may not be set if this is the resize associated with
  // window creation
  SetForcedRepaint(true);
  if (this == mViewManager->GetRootView()) {
    RefPtr<nsDeviceContext> devContext = mViewManager->GetDeviceContext();
    // ensure DPI is up-to-date, in case of window being opened and sized
    // on a non-default-dpi display (bug 829963)
    devContext->CheckDPIChange();
    int32_t p2a = devContext->AppUnitsPerDevPixel();
    mViewManager->SetWindowDimensions(NSIntPixelsToAppUnits(aWidth, p2a),
                                      NSIntPixelsToAppUnits(aHeight, p2a));

    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm) {
      nsIPresShell* presShell = mViewManager->GetPresShell();
      if (presShell && presShell->GetDocument()) {
        pm->AdjustPopupsOnWindowChange(presShell);
      }
    }

    return true;
  }
  if (IsPopupWidget(aWidget)) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm) {
      pm->PopupResized(mFrame, LayoutDeviceIntSize(aWidth, aHeight));
      return true;
    }
  }

  return false;
}

bool
nsView::RequestWindowClose(nsIWidget* aWidget)
{
  if (mFrame && IsPopupWidget(aWidget) && mFrame->IsMenuPopupFrame()) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm) {
      pm->HidePopup(mFrame->GetContent(), false, true, false, false);
      return true;
    }
  }

  return false;
}

void
nsView::WillPaintWindow(nsIWidget* aWidget)
{
  RefPtr<nsViewManager> vm = mViewManager;
  vm->WillPaintWindow(aWidget);
}

bool
nsView::PaintWindow(nsIWidget* aWidget, LayoutDeviceIntRegion aRegion)
{
  NS_ASSERTION(this == nsView::GetViewFor(aWidget), "wrong view for widget?");

  RefPtr<nsViewManager> vm = mViewManager;
  bool result = vm->PaintWindow(aWidget, aRegion);
  return result;
}

void
nsView::DidPaintWindow()
{
  RefPtr<nsViewManager> vm = mViewManager;
  vm->DidPaintWindow();
}

void
nsView::DidCompositeWindow(uint64_t aTransactionId,
                           const TimeStamp& aCompositeStart,
                           const TimeStamp& aCompositeEnd)
{
  nsIPresShell* presShell = mViewManager->GetPresShell();
  if (presShell) {
    nsAutoScriptBlocker scriptBlocker;

    nsPresContext* context = presShell->GetPresContext();
    nsRootPresContext* rootContext = context->GetRootPresContext();
    if (rootContext) {
      rootContext->NotifyDidPaintForSubtree(aTransactionId, aCompositeEnd);
    }

    // If the two timestamps are identical, this was likely a fake composite
    // event which wouldn't be terribly useful to display.
    if (aCompositeStart == aCompositeEnd) {
      return;
    }

    nsIDocShell* docShell = context->GetDocShell();
    RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();

    if (timelines && timelines->HasConsumer(docShell)) {
      timelines->AddMarkerForDocShell(docShell,
        MakeUnique<CompositeTimelineMarker>(aCompositeStart, MarkerTracingType::START));
      timelines->AddMarkerForDocShell(docShell,
        MakeUnique<CompositeTimelineMarker>(aCompositeEnd, MarkerTracingType::END));
    }
  }
}

void
nsView::RequestRepaint()
{
  nsIPresShell* presShell = mViewManager->GetPresShell();
  if (presShell) {
    presShell->ScheduleViewManagerFlush();
  }
}

nsEventStatus
nsView::HandleEvent(WidgetGUIEvent* aEvent,
                    bool aUseAttachedEvents)
{
  NS_PRECONDITION(nullptr != aEvent->mWidget, "null widget ptr");

  nsEventStatus result = nsEventStatus_eIgnore;
  nsView* view;
  if (aUseAttachedEvents) {
    nsIWidgetListener* listener = aEvent->mWidget->GetAttachedWidgetListener();
    view = listener ? listener->GetView() : nullptr;
  }
  else {
    view = GetViewFor(aEvent->mWidget);
  }

  if (view) {
    RefPtr<nsViewManager> vm = view->GetViewManager();
    vm->DispatchEvent(aEvent, view, &result);
  }

  return result;
}

bool
nsView::IsPrimaryFramePaintSuppressed()
{
  return sShowPreviousPage && mFrame && mFrame->PresShell()->IsPaintingSuppressed();
}
