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

LayerManager*
HeadlessWidget::GetLayerManager(PLayerTransactionChild* aShadowManager,
                                LayersBackend aBackendHint,
                                LayerManagerPersistence aPersistence)
{
  if (!mLayerManager) {
    mLayerManager = new BasicLayerManager(BasicLayerManager::BLM_OFFSCREEN);
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
