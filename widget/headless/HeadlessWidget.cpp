/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "HeadlessWidget.h"
#include "Layers.h"
#include "BasicLayers.h"
#include "BasicEvents.h"

using namespace mozilla::layers;

/*static*/ already_AddRefed<nsIWidget>
nsIWidget::CreateHeadlessWidget()
{
  nsCOMPtr<nsIWidget> widget = new mozilla::widget::HeadlessWidget();
  return widget.forget();
}

namespace mozilla {
namespace widget {

NS_IMPL_ISUPPORTS_INHERITED0(HeadlessWidget, nsBaseWidget)

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
  mVisible = true;
  mEnabled = true;
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
  if (NS_FAILED(widget->Create(nullptr, nullptr, aRect, aInitData))) {
    return nullptr;
  }
  return widget.forget();
}

void
HeadlessWidget::Show(bool aState)
{
  mVisible = aState;
}

bool
HeadlessWidget::IsVisible() const
{
  return mVisible;
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
  if (!mLayerManager) {
    mLayerManager = new BasicLayerManager(this);
  }

  return mLayerManager;
}

void
HeadlessWidget::Resize(double aWidth,
                       double aHeight,
                       bool   aRepaint)
{
  mBounds.SizeTo(LayoutDeviceIntSize(NSToIntRound(aWidth),
                                     NSToIntRound(aHeight)));
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
  if (mSizeMode == nsSizeMode_Normal) {
    // Store the last normal size bounds so it can be restored when entering
    // normal mode again.
    mRestoreBounds = mBounds;
  }

  nsBaseWidget::SetSizeMode(aMode);

  // Normally in real widget backends a window event would be triggered that
  // would cause the window manager to handle resizing the window. In headless
  // the window must manually be resized.
  switch(aMode) {
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
