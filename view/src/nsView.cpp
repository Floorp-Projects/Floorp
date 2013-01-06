/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsView.h"

#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Likely.h"
#include "nsIWidget.h"
#include "nsWidgetsCID.h"
#include "nsViewManager.h"
#include "nsIFrame.h"
#include "nsGUIEvent.h"
#include "nsIComponentManager.h"
#include "nsGfxCIID.h"
#include "nsIInterfaceRequestor.h"
#include "nsXULPopupManager.h"
#include "nsIWidgetListener.h"

using namespace mozilla;

nsView::nsView(nsViewManager* aViewManager, nsViewVisibility aVisibility)
{
  MOZ_COUNT_CTOR(nsView);

  mVis = aVisibility;
  // Views should be transparent by default. Not being transparent is
  // a promise that the view will paint all its pixels opaquely. Views
  // should make this promise explicitly by calling
  // SetViewContentTransparency.
  mVFlags = 0;
  mViewManager = aViewManager;
  mDirtyRegion = nullptr;
  mWidgetIsTopLevel = false;
  mInAlternatePaint = false;
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

  // Destroy and release the widget
  DestroyWidget();

  delete mDirtyRegion;
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
      mWindow->Destroy();
    }

    NS_RELEASE(mWindow);
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
  delete this;
}

void nsView::SetPosition(nscoord aX, nscoord aY)
{
  mDimBounds.x += aX - mPosX;
  mDimBounds.y += aY - mPosY;
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

nsIntRect nsView::CalcWidgetBounds(nsWindowType aType)
{
  int32_t p2a = mViewManager->AppUnitsPerDevPixel();

  nsRect viewBounds(mDimBounds);

  nsView* parent = GetParent();
  if (parent) {
    nsPoint offset;
    nsIWidget* parentWidget = parent->GetNearestWidget(&offset, p2a);
    // make viewBounds be relative to the parent widget, in appunits
    viewBounds += offset;

    if (parentWidget && aType == eWindowType_popup &&
        IsEffectivelyVisible()) {
      // put offset into screen coordinates. (based on client area origin)
      nsIntPoint screenPoint = parentWidget->WidgetToScreenOffset();
      viewBounds += nsPoint(NSIntPixelsToAppUnits(screenPoint.x, p2a),
                            NSIntPixelsToAppUnits(screenPoint.y, p2a));
    }
  }

  // Compute widget bounds in device pixels
  nsIntRect newBounds = viewBounds.ToNearestPixels(p2a);

  // Compute where the top-left of our widget ended up relative to the parent
  // widget, in appunits.
  nsPoint roundedOffset(NSIntPixelsToAppUnits(newBounds.x, p2a),
                        NSIntPixelsToAppUnits(newBounds.y, p2a));

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
  
  nsIntRect curBounds;
  mWindow->GetClientBounds(curBounds);

  nsWindowType type;
  mWindow->GetWindowType(type);

  if (type == eWindowType_popup &&
      ((curBounds.IsEmpty() && mDimBounds.IsEmpty()) ||
       mVis == nsViewVisibility_kHide)) {
    // Don't manipulate empty or hidden popup widgets. For example there's no
    // point moving hidden comboboxes around, or doing X server roundtrips
    // to compute their true screen position. This could mean that WidgetToScreen
    // operations on these widgets don't return up-to-date values, but popup
    // positions aren't reliable anyway because of correction to be on or off-screen.
    return;
  }

  NS_PRECONDITION(mWindow, "Why was this called??");

  nsIntRect newBounds = CalcWidgetBounds(type);

  bool changedPos = curBounds.TopLeft() != newBounds.TopLeft();
  bool changedSize = curBounds.Size() != newBounds.Size();

  // Child views are never attached to top level widgets, this is safe.

  // Coordinates are converted to display pixels for window Move/Resize APIs,
  // because of the potential for device-pixel coordinate spaces for mixed
  // hidpi/lodpi screens to overlap each other and result in bad placement
  // (bug 814434).
  nsRefPtr<nsDeviceContext> dx;
  mViewManager->GetDeviceContext(*getter_AddRefs(dx));
  double invScale = dx->UnscaledAppUnitsPerDevPixel() / 60.0;

  if (changedPos) {
    if (changedSize && !aMoveOnly) {
      mWindow->ResizeClient(newBounds.x * invScale,
                            newBounds.y * invScale,
                            newBounds.width * invScale,
                            newBounds.height * invScale,
                            aInvalidateChangedSize);
    } else {
      mWindow->MoveClient(newBounds.x * invScale,
                          newBounds.y * invScale);
    }
  } else {
    if (changedSize && !aMoveOnly) {
      mWindow->ResizeClient(newBounds.width * invScale,
                            newBounds.height * invScale,
                            aInvalidateChangedSize);
    } // else do nothing!
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

  if (nullptr != mWindow)
  {
    if (aEffectivelyVisible)
    {
      DoResetWidgetBounds(false, true);
      mWindow->Show(true);
    }
    else
      mWindow->Show(false);
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

void nsView::InvalidateHierarchy(nsViewManager *aViewManagerParent)
{
  if (mViewManager->GetRootView() == this)
    mViewManager->InvalidateHierarchy();

  for (nsView *child = mFirstChild; child; child = child->GetNextSibling())
    child->InvalidateHierarchy(aViewManagerParent);
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
      aChild->InvalidateHierarchy(nullptr); // don't care about releasing grabs
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
      child->InvalidateHierarchy(GetViewManager());
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
    int32_t curZ;
    widget->GetZIndex(&curZ);
    if (curZ != aZIndex) {
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
  NS_ABORT_IF_FALSE(!aWidgetInitData ||
                    aWidgetInitData->mWindowType != eWindowType_popup,
                    "Use CreateWidgetForPopup");

  DefaultWidgetInitData defaultInitData;
  bool initDataPassedIn = !!aWidgetInitData;
  aWidgetInitData = aWidgetInitData ? aWidgetInitData : &defaultInitData;
  defaultInitData.mListenForResizes =
    (!initDataPassedIn && GetParent() &&
     GetParent()->GetViewManager() != mViewManager);

  nsIntRect trect = CalcWidgetBounds(aWidgetInitData->mWindowType);

  nsRefPtr<nsDeviceContext> dx;
  mViewManager->GetDeviceContext(*getter_AddRefs(dx));

  nsIWidget* parentWidget =
    GetParent() ? GetParent()->GetNearestWidget(nullptr) : nullptr;
  if (!parentWidget) {
    NS_ERROR("nsView::CreateWidget without suitable parent widget??");
    return NS_ERROR_FAILURE;
  }

  // XXX: using aForceUseIWidgetParent=true to preserve previous
  // semantics.  It's not clear that it's actually needed.
  mWindow = parentWidget->CreateChild(trect, dx, aWidgetInitData,
                                      true).get();
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
  NS_ABORT_IF_FALSE(!aWidgetInitData ||
                    aWidgetInitData->mWindowType != eWindowType_popup,
                    "Use CreateWidgetForPopup");
  NS_ABORT_IF_FALSE(aParentWidget, "Parent widget required");

  DefaultWidgetInitData defaultInitData;
  aWidgetInitData = aWidgetInitData ? aWidgetInitData : &defaultInitData;

  nsIntRect trect = CalcWidgetBounds(aWidgetInitData->mWindowType);

  nsRefPtr<nsDeviceContext> dx;
  mViewManager->GetDeviceContext(*getter_AddRefs(dx));

  mWindow =
    aParentWidget->CreateChild(trect, dx, aWidgetInitData).get();
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
  NS_ABORT_IF_FALSE(aWidgetInitData, "Widget init data required");
  NS_ABORT_IF_FALSE(aWidgetInitData->mWindowType == eWindowType_popup,
                    "Use one of the other CreateWidget methods");

  nsIntRect trect = CalcWidgetBounds(aWidgetInitData->mWindowType);

  nsRefPtr<nsDeviceContext> dx;
  mViewManager->GetDeviceContext(*getter_AddRefs(dx));

  // XXX/cjones: having these two separate creation cases seems ... um
  // ... unnecessary, but it's the way the old code did it.  Please
  // unify them by first finding a suitable parent nsIWidget, then
  // getting rid of aForceUseIWidgetParent.
  if (aParentWidget) {
    // XXX: using aForceUseIWidgetParent=true to preserve previous
    // semantics.  It's not clear that it's actually needed.
    mWindow = aParentWidget->CreateChild(trect, dx, aWidgetInitData,
                                         true).get();
  }
  else {
    nsIWidget* nearestParent = GetParent() ? GetParent()->GetNearestWidget(nullptr)
                                           : nullptr;
    if (!nearestParent) {
      // Without a parent, we can't make a popup.  This can happen
      // when printing
      return NS_ERROR_FAILURE;
    }

    mWindow =
      nearestParent->CreateChild(trect, dx, aWidgetInitData).get();
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
  NS_ABORT_IF_FALSE(mWindow, "Must have a window to initialize");

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

  nsRefPtr<nsDeviceContext> dx;
  mViewManager->GetDeviceContext(*getter_AddRefs(dx));

  // Note, the previous device context will be released. Detaching
  // will not restore the old one.
  nsresult rv = aWidget->AttachViewToTopLevel(!nsIWidget::UsePuppetWidgets(), dx);
  if (NS_FAILED(rv))
    return rv;

  mWindow = aWidget;
  NS_ADDREF(mWindow);

  mWindow->SetAttachedWidgetListener(this);
  mWindow->EnableDragDrop(true);
  mWidgetIsTopLevel = true;

  // Refresh the view bounds
  nsWindowType type;
  mWindow->GetWindowType(type);
  CalcWidgetBounds(type);

  return NS_OK;
}

// Detach this view from an attached widget. 
nsresult nsView::DetachFromTopLevelWidget()
{
  NS_PRECONDITION(mWidgetIsTopLevel, "Not attached currently!");
  NS_PRECONDITION(mWindow, "null mWindow for DetachFromTopLevelWidget!");

  mWindow->SetAttachedWidgetListener(nullptr);
  NS_RELEASE(mWindow);

  mWidgetIsTopLevel = false;
  
  return NS_OK;
}

void nsView::SetZIndex(bool aAuto, int32_t aZIndex, bool aTopMost)
{
  bool oldIsAuto = GetZIndexIsAuto();
  mVFlags = (mVFlags & ~NS_VIEW_FLAG_AUTO_ZINDEX) | (aAuto ? NS_VIEW_FLAG_AUTO_ZINDEX : 0);
  mZIndex = aZIndex;
  SetTopMost(aTopMost);
  
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
    NS_RELEASE(mWindow);
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
    nsIntRect rect;
    mWindow->GetClientBounds(rect);
    nsRect windowBounds = rect.ToAppUnits(p2a);
    mWindow->GetBounds(rect);
    nsRect nonclientBounds = rect.ToAppUnits(p2a);
    nsrefcnt widgetRefCnt = mWindow->AddRef() - 1;
    mWindow->Release();
    int32_t Z;
    mWindow->GetZIndex(&Z);
    fprintf(out, "(widget=%p[%d] z=%d pos={%d,%d,%d,%d}) ",
            (void*)mWindow, widgetRefCnt, Z,
            nonclientBounds.x, nonclientBounds.y,
            windowBounds.width, windowBounds.height);
  }
  nsRect brect = GetBounds();
  fprintf(out, "{%d,%d,%d,%d}",
          brect.x, brect.y, brect.width, brect.height);
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
  NS_ABORT_IF_FALSE(GetParent() || !aOther || aOther->GetParent() ||
                    this == aOther, "caller of (outer) GetOffsetTo must not "
                    "pass unrelated views");
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
        offset += docOffset.ConvertAppUnits(currAPD, aAPD);
        docOffset.x = docOffset.y = 0;
        currAPD = newAPD;
      }
      currVM = newVM;
    }
    docOffset += v->GetPosition();
  }
  offset += docOffset.ConvertAppUnits(currAPD, aAPD);

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
  pt = pt.ConvertAppUnits(widgetAPD, ourAPD);
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
        pt += docPt.ConvertAppUnits(currAPD, aAPD);
        docPt.x = docPt.y = 0;
        currAPD = newAPD;
      }
      currVM = newVM;
    }
    docPt += v->GetPosition();
  }
  if (!v) {
    if (aOffset) {
      pt += docPt.ConvertAppUnits(currAPD, aAPD);
      *aOffset = pt;
    }
    return nullptr;
  }

  // pt is now the offset from v's origin to this view's origin.
  // We add the ViewToWidgetOffset to get the offset to the widget.
  if (aOffset) {
    docPt += v->ViewToWidgetOffset();
    pt += docPt.ConvertAppUnits(currAPD, aAPD);
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
  return mDimBounds.ConvertAppUnitsRoundOut(ourAPD, parentAPD);
}

nsPoint
nsView::ConvertFromParentCoords(nsPoint aPt) const
{
  const nsView* parent = GetParent();
  if (parent) {
    aPt = aPt.ConvertAppUnits(parent->GetViewManager()->AppUnitsPerDevPixel(),
                              GetViewManager()->AppUnitsPerDevPixel());
  }
  aPt -= GetPosition();
  return aPt;
}

static bool
IsPopupWidget(nsIWidget* aWidget)
{
  nsWindowType type;
  aWidget->GetWindowType(type);
  return (type == eWindowType_popup);
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
    nsRefPtr<nsDeviceContext> devContext;
    mViewManager->GetDeviceContext(*getter_AddRefs(devContext));
    int32_t p2a = devContext->AppUnitsPerDevPixel();
    mViewManager->SetWindowDimensions(NSIntPixelsToAppUnits(aWidth, p2a),
                                      NSIntPixelsToAppUnits(aHeight, p2a));
    return true;
  }
  else if (IsPopupWidget(aWidget)) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm) {
      pm->PopupResized(mFrame, nsIntSize(aWidth, aHeight));
      return true;
    }
  }

  return false;
}

bool
nsView::RequestWindowClose(nsIWidget* aWidget)
{
  if (mFrame && IsPopupWidget(aWidget) &&
      mFrame->GetType() == nsGkAtoms::menuPopupFrame) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm) {
      pm->HidePopup(mFrame->GetContent(), false, true, false);
      return true;
    }
  }

  return false;
}

void
nsView::WillPaintWindow(nsIWidget* aWidget, bool aWillSendDidPaint)
{
  nsRefPtr<nsViewManager> vm = mViewManager;
  vm->WillPaintWindow(aWidget, aWillSendDidPaint);
}

bool
nsView::PaintWindow(nsIWidget* aWidget, nsIntRegion aRegion, uint32_t aFlags)
{
  NS_ASSERTION(this == nsView::GetViewFor(aWidget), "wrong view for widget?");

  mInAlternatePaint = aFlags & PAINT_IS_ALTERNATE;
  nsRefPtr<nsViewManager> vm = mViewManager;
  bool result = vm->PaintWindow(aWidget, aRegion, aFlags);
  // PaintWindow can destroy this via WillPaintWindow notification, so we have
  // to re-get the view from the widget.
  nsView* view = nsView::GetViewFor(aWidget);
  if (view) {
    view->mInAlternatePaint = false;
  }
  return result;
}

void
nsView::DidPaintWindow()
{
  nsRefPtr<nsViewManager> vm = mViewManager;
  vm->DidPaintWindow();
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
nsView::HandleEvent(nsGUIEvent* aEvent, bool aUseAttachedEvents)
{
  NS_PRECONDITION(nullptr != aEvent->widget, "null widget ptr");

  nsEventStatus result = nsEventStatus_eIgnore;
  nsView* view;
  if (aUseAttachedEvents) {
    nsIWidgetListener* listener = aEvent->widget->GetAttachedWidgetListener();
    view = listener ? listener->GetView() : nullptr;
  }
  else {
    view = GetViewFor(aEvent->widget);
  }

  if (view) {
    nsRefPtr<nsViewManager> vm = view->GetViewManager();
    vm->DispatchEvent(aEvent, view, &result);
  }

  return result;
}
