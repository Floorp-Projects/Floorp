/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "HeadlessWidget.h"
#include "HeadlessCompositorWidget.h"
#include "Layers.h"
#include "BasicLayers.h"
#include "BasicEvents.h"
#include "mozilla/gfx/gfxVars.h"

using namespace mozilla::gfx;
using namespace mozilla::layers;

/*static*/ already_AddRefed<nsIWidget>
nsIWidget::CreateHeadlessWidget()
{
  nsCOMPtr<nsIWidget> widget = new mozilla::widget::HeadlessWidget();
  return widget.forget();
}

namespace mozilla {
namespace widget {

already_AddRefed<gfxContext>
CreateDefaultTarget(IntSize aSize)
{
  // Always use at least a 1x1 draw target to avoid gfx issues
  // with 0x0 targets.
  IntSize size = (aSize.width <= 0 || aSize.height <= 0) ? gfx::IntSize(1, 1) : aSize;
  RefPtr<DrawTarget> target = Factory::CreateDrawTarget(gfxVars::ContentBackend(), size, SurfaceFormat::B8G8R8A8);
  RefPtr<gfxContext> ctx = gfxContext::CreatePreservingTransformOrNull(target);
  return ctx.forget();
}

NS_IMPL_ISUPPORTS_INHERITED0(HeadlessWidget, nsBaseWidget)

HeadlessWidget* HeadlessWidget::sActiveWindow = nullptr;

HeadlessWidget::HeadlessWidget()
  : mEnabled(true)
  , mVisible(false)
  , mTopLevel(nullptr)
  , mCompositorWidget(nullptr)
  , mLastSizeMode(nsSizeMode_Normal)
  , mEffectiveSizeMode(nsSizeMode_Normal)
  , mRestoreBounds(0,0,0,0)
{
}

HeadlessWidget::~HeadlessWidget()
{
  if (sActiveWindow == this)
    sActiveWindow = nullptr;
}

nsresult
HeadlessWidget::Create(nsIWidget* aParent,
                       nsNativeWidget aNativeParent,
                       const LayoutDeviceIntRect& aRect,
                       nsWidgetInitData* aInitData)
{
  MOZ_ASSERT(!aNativeParent, "No native parents for headless widgets.");

  BaseCreate(nullptr, aInitData);

  mBounds = aRect;
  mRestoreBounds = aRect;

  if (aParent) {
    mTopLevel = aParent->GetTopLevelWidget();
  } else {
    mTopLevel = this;
  }

  return NS_OK;
}

already_AddRefed<nsIWidget>
HeadlessWidget::CreateChild(const LayoutDeviceIntRect& aRect,
                            nsWidgetInitData* aInitData,
                            bool aForceUseIWidgetParent)
{
  nsCOMPtr<nsIWidget> widget = nsIWidget::CreateHeadlessWidget();
  if (!widget) {
    return nullptr;
  }
  if (NS_FAILED(widget->Create(this, nullptr, aRect, aInitData))) {
    return nullptr;
  }
  return widget.forget();
}

void HeadlessWidget::GetCompositorWidgetInitData(mozilla::widget::CompositorWidgetInitData* aInitData)
{
  *aInitData = mozilla::widget::HeadlessCompositorWidgetInitData(GetClientSize());
}

nsIWidget*
HeadlessWidget::GetTopLevelWidget()
{
  return mTopLevel;
}

void
HeadlessWidget::RaiseWindow()
{
  MOZ_ASSERT(mTopLevel == this, "Raising a non-toplevel window.");

  if (sActiveWindow == this)
    return;

  // Raise the window to the top of the stack.
  nsWindowZ placement = nsWindowZTop;
  nsCOMPtr<nsIWidget> actualBelow;
  if (mWidgetListener)
    mWidgetListener->ZLevelChanged(true, &placement, nullptr, getter_AddRefs(actualBelow));

  // Deactivate the last active window.
  if (sActiveWindow && sActiveWindow->mWidgetListener)
    sActiveWindow->mWidgetListener->WindowDeactivated();

  // Activate this window.
  sActiveWindow = this;
  if (mWidgetListener)
    mWidgetListener->WindowActivated();
}

void
HeadlessWidget::Show(bool aState)
{
  mVisible = aState;

  // Top-level windows are activated/raised when shown.
  if (aState && mTopLevel == this)
    RaiseWindow();

  ApplySizeModeSideEffects();
}

bool
HeadlessWidget::IsVisible() const
{
  return mVisible;
}

nsresult
HeadlessWidget::SetFocus(bool aRaise)
{
  // aRaise == true means we request activation of our toplevel window.
  if (aRaise) {
    HeadlessWidget* topLevel = (HeadlessWidget*) GetTopLevelWidget();

    // The toplevel only becomes active if it's currently visible; otherwise, it
    // will be activated anyway when it's shown.
    if (topLevel->IsVisible())
      topLevel->RaiseWindow();
  }
  return NS_OK;
}

void
HeadlessWidget::Enable(bool aState)
{
  mEnabled = aState;
}

bool
HeadlessWidget::IsEnabled() const
{
  return mEnabled;
}

void
HeadlessWidget::Move(double aX, double aY)
{
  double scale = BoundsUseDesktopPixels() ? GetDesktopToDeviceScale().scale : 1.0;
  int32_t x = NSToIntRound(aX * scale);
  int32_t y = NSToIntRound(aY * scale);

  if (mWindowType == eWindowType_toplevel ||
      mWindowType == eWindowType_dialog) {
      SetSizeMode(nsSizeMode_Normal);
  }

  // Since a popup window's x/y coordinates are in relation to
  // the parent, the parent might have moved so we always move a
  // popup window.
  if (x == mBounds.x && y == mBounds.y &&
      mWindowType != eWindowType_popup) {
    return;
  }

  mBounds.x = x;
  mBounds.y = y;
  NotifyRollupGeometryChange();
}

LayoutDeviceIntPoint
HeadlessWidget::WidgetToScreenOffset()
{
  return LayoutDeviceIntPoint(mBounds.x, mBounds.y);
}

LayerManager*
HeadlessWidget::GetLayerManager(PLayerTransactionChild* aShadowManager,
                                LayersBackend aBackendHint,
                                LayerManagerPersistence aPersistence)
{
  return nsBaseWidget::GetLayerManager(aShadowManager, aBackendHint, aPersistence);
}

void
HeadlessWidget::SetCompositorWidgetDelegate(CompositorWidgetDelegate* delegate)
{
    if (delegate) {
        mCompositorWidget = delegate->AsHeadlessCompositorWidget();
        MOZ_ASSERT(mCompositorWidget,
                   "HeadlessWidget::SetCompositorWidgetDelegate called with a non-HeadlessCompositorWidget");
    } else {
        mCompositorWidget = nullptr;
    }
}

void
HeadlessWidget::Resize(double aWidth,
                       double aHeight,
                       bool   aRepaint)
{
  int32_t width = NSToIntRound(aWidth);
  int32_t height = NSToIntRound(aHeight);
  ConstrainSize(&width, &height);
  mBounds.SizeTo(LayoutDeviceIntSize(width, height));

  if (mCompositorWidget) {
    mCompositorWidget->NotifyClientSizeChanged(LayoutDeviceIntSize(mBounds.width, mBounds.height));
  }
  if (mWidgetListener) {
    mWidgetListener->WindowResized(this, mBounds.width, mBounds.height);
  }
  if (mAttachedWidgetListener) {
    mAttachedWidgetListener->WindowResized(this, mBounds.width, mBounds.height);
  }
}

void
HeadlessWidget::Resize(double aX,
                       double aY,
                       double aWidth,
                       double aHeight,
                       bool   aRepaint)
{
  if (mBounds.x != aX || mBounds.y != aY) {
    NotifyWindowMoved(aX, aY);
  }
  return Resize(aWidth, aHeight, aRepaint);
}

void
HeadlessWidget::SetSizeMode(nsSizeMode aMode)
{
  if (aMode == mSizeMode) {
    return;
  }

  nsBaseWidget::SetSizeMode(aMode);

  // Normally in real widget backends a window event would be triggered that
  // would cause the window manager to handle resizing the window. In headless
  // the window must manually be resized.
  ApplySizeModeSideEffects();
}

void
HeadlessWidget::ApplySizeModeSideEffects()
{
  if (!mVisible || mEffectiveSizeMode == mSizeMode) {
    return;
  }

  if (mEffectiveSizeMode == nsSizeMode_Normal) {
    // Store the last normal size bounds so it can be restored when entering
    // normal mode again.
    mRestoreBounds = mBounds;
  }

  switch(mSizeMode) {
  case nsSizeMode_Normal: {
    Resize(mRestoreBounds.x, mRestoreBounds.y, mRestoreBounds.width, mRestoreBounds.height, false);
    break;
  }
  case nsSizeMode_Minimized:
    break;
  case nsSizeMode_Maximized: {
    nsCOMPtr<nsIScreen> screen = GetWidgetScreen();
    if (screen) {
      int32_t left, top, width, height;
      if (NS_SUCCEEDED(screen->GetRectDisplayPix(&left, &top, &width, &height))) {
        Resize(0, 0, width, height, true);
      }
    }
    break;
  }
  case nsSizeMode_Fullscreen:
    // This will take care of resizing the window.
    nsBaseWidget::InfallibleMakeFullScreen(true);
    break;
  default:
    break;
  }

  mEffectiveSizeMode = mSizeMode;
}

nsresult
HeadlessWidget::MakeFullScreen(bool aFullScreen, nsIScreen* aTargetScreen)
{
  // Directly update the size mode here so a later call SetSizeMode does
  // nothing.
  if (aFullScreen) {
    if (mSizeMode != nsSizeMode_Fullscreen) {
      mLastSizeMode = mSizeMode;
    }
    mSizeMode = nsSizeMode_Fullscreen;
  } else {
    mSizeMode = mLastSizeMode;
  }

  nsBaseWidget::InfallibleMakeFullScreen(aFullScreen, aTargetScreen);

  if (mWidgetListener) {
    mWidgetListener->SizeModeChanged(mSizeMode);
    mWidgetListener->FullscreenChanged(aFullScreen);
  }

  return NS_OK;
}

nsresult
HeadlessWidget::DispatchEvent(WidgetGUIEvent* aEvent, nsEventStatus& aStatus)
{
#ifdef DEBUG
  debug_DumpEvent(stdout, aEvent->mWidget, aEvent, "HeadlessWidget", 0);
#endif

  aStatus = nsEventStatus_eIgnore;

  if (mAttachedWidgetListener) {
    aStatus = mAttachedWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);
  } else if (mWidgetListener) {
    aStatus = mWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);
  }

  return NS_OK;
}

} // namespace widget
} // namespace mozilla
