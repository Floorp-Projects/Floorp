/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InProcessCompositorWidget.h"

#include "mozilla/VsyncDispatcher.h"
#include "nsBaseWidget.h"

#if defined(MOZ_WIDGET_ANDROID) && !defined(MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING)
#  include "mozilla/widget/AndroidCompositorWidget.h"
#endif

namespace mozilla {
namespace widget {

// Platforms with no OOP compositor process support use
// InProcessCompositorWidget by default.
#if !defined(MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING)
/* static */
RefPtr<CompositorWidget> CompositorWidget::CreateLocal(
    const CompositorWidgetInitData& aInitData,
    const layers::CompositorOptions& aOptions, nsIWidget* aWidget) {
  // We're getting crashes from storing a NULL mWidget, and this is the
  // only remaining explanation that doesn't involve memory corruption,
  // so placing a release assert here. For even more sanity-checking, we
  // do it after the static_cast.
  nsBaseWidget* widget = static_cast<nsBaseWidget*>(aWidget);
  MOZ_RELEASE_ASSERT(widget);
#  ifdef MOZ_WIDGET_ANDROID
  return new AndroidCompositorWidget(aOptions, widget);
#  else
  return new InProcessCompositorWidget(aOptions, widget);
#  endif
}
#endif

InProcessCompositorWidget::InProcessCompositorWidget(
    const layers::CompositorOptions& aOptions, nsBaseWidget* aWidget)
    : CompositorWidget(aOptions),
      mWidget(aWidget),
      mCanary(CANARY_VALUE),
      mWidgetSanity(aWidget) {
  // The only method of construction that is used outside of unit tests is
  // ::CreateLocal, above. That method of construction asserts that mWidget
  // is not assigned a NULL value. And yet mWidget is NULL in some crash
  // reports that involve other class methods. Adding a release assert here
  // will give us the earliest possible notification that we're headed for
  // a crash.
  MOZ_RELEASE_ASSERT(mWidget);
}

bool InProcessCompositorWidget::PreRender(WidgetRenderingContext* aContext) {
  CheckWidgetSanity();
  return mWidget->PreRender(aContext);
}

void InProcessCompositorWidget::PostRender(WidgetRenderingContext* aContext) {
  CheckWidgetSanity();
  mWidget->PostRender(aContext);
}

RefPtr<layers::NativeLayerRoot>
InProcessCompositorWidget::GetNativeLayerRoot() {
  CheckWidgetSanity();
  return mWidget->GetNativeLayerRoot();
}

already_AddRefed<gfx::DrawTarget>
InProcessCompositorWidget::StartRemoteDrawing() {
  CheckWidgetSanity();
  return mWidget->StartRemoteDrawing();
}

already_AddRefed<gfx::DrawTarget>
InProcessCompositorWidget::StartRemoteDrawingInRegion(
    const LayoutDeviceIntRegion& aInvalidRegion,
    layers::BufferMode* aBufferMode) {
  CheckWidgetSanity();
  return mWidget->StartRemoteDrawingInRegion(aInvalidRegion, aBufferMode);
}

void InProcessCompositorWidget::EndRemoteDrawing() {
  CheckWidgetSanity();
  mWidget->EndRemoteDrawing();
}

void InProcessCompositorWidget::EndRemoteDrawingInRegion(
    gfx::DrawTarget* aDrawTarget, const LayoutDeviceIntRegion& aInvalidRegion) {
  CheckWidgetSanity();
  mWidget->EndRemoteDrawingInRegion(aDrawTarget, aInvalidRegion);
}

void InProcessCompositorWidget::CleanupRemoteDrawing() {
  CheckWidgetSanity();
  mWidget->CleanupRemoteDrawing();
}

void InProcessCompositorWidget::CleanupWindowEffects() {
  CheckWidgetSanity();
  mWidget->CleanupWindowEffects();
}

bool InProcessCompositorWidget::InitCompositor(
    layers::Compositor* aCompositor) {
  CheckWidgetSanity();
  return mWidget->InitCompositor(aCompositor);
}

LayoutDeviceIntSize InProcessCompositorWidget::GetClientSize() {
  CheckWidgetSanity();
  return mWidget->GetClientSize();
}

uint32_t InProcessCompositorWidget::GetGLFrameBufferFormat() {
  CheckWidgetSanity();
  return mWidget->GetGLFrameBufferFormat();
}

uintptr_t InProcessCompositorWidget::GetWidgetKey() {
  CheckWidgetSanity();
  return reinterpret_cast<uintptr_t>(mWidget);
}

nsIWidget* InProcessCompositorWidget::RealWidget() { return mWidget; }

void InProcessCompositorWidget::ObserveVsync(VsyncObserver* aObserver) {
  CheckWidgetSanity();
  if (RefPtr<CompositorVsyncDispatcher> cvd =
          mWidget->GetCompositorVsyncDispatcher()) {
    cvd->SetCompositorVsyncObserver(aObserver);
  }
}

const char* InProcessCompositorWidget::CANARY_VALUE =
    reinterpret_cast<char*>(0x1a1a1a1a);

void InProcessCompositorWidget::CheckWidgetSanity() {
  MOZ_RELEASE_ASSERT(mWidgetSanity == mWidget);
  MOZ_RELEASE_ASSERT(mCanary == CANARY_VALUE);
}

}  // namespace widget
}  // namespace mozilla
