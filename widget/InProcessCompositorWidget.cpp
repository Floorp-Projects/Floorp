/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InProcessCompositorWidget.h"
#include "nsBaseWidget.h"

namespace mozilla {
namespace widget {

// Platforms with no OOP compositor process support use
// InProcessCompositorWidget by default.
#if !defined(MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING)
/* static */ RefPtr<CompositorWidget>
CompositorWidget::CreateLocal(const CompositorWidgetInitData& aInitData, nsIWidget* aWidget)
{
  MOZ_ASSERT(aWidget);
  return new InProcessCompositorWidget(static_cast<nsBaseWidget*>(aWidget));
}
#endif

InProcessCompositorWidget::InProcessCompositorWidget(nsBaseWidget* aWidget)
 : mWidget(aWidget)
{
}

bool
InProcessCompositorWidget::PreRender(layers::LayerManagerComposite* aManager)
{
  return mWidget->PreRender(aManager);
}

void
InProcessCompositorWidget::PostRender(layers::LayerManagerComposite* aManager)
{
  mWidget->PostRender(aManager);
}

void
InProcessCompositorWidget::DrawWindowUnderlay(layers::LayerManagerComposite* aManager,
                                          LayoutDeviceIntRect aRect)
{
  mWidget->DrawWindowUnderlay(aManager, aRect);
}

void
InProcessCompositorWidget::DrawWindowOverlay(layers::LayerManagerComposite* aManager,
                                         LayoutDeviceIntRect aRect)
{
  mWidget->DrawWindowOverlay(aManager, aRect);
}

already_AddRefed<gfx::DrawTarget>
InProcessCompositorWidget::StartRemoteDrawing()
{
  return mWidget->StartRemoteDrawing();
}

already_AddRefed<gfx::DrawTarget>
InProcessCompositorWidget::StartRemoteDrawingInRegion(LayoutDeviceIntRegion& aInvalidRegion,
                                                  layers::BufferMode* aBufferMode)
{
  return mWidget->StartRemoteDrawingInRegion(aInvalidRegion, aBufferMode);
}

void
InProcessCompositorWidget::EndRemoteDrawing()
{
  mWidget->EndRemoteDrawing();
}

void
InProcessCompositorWidget::EndRemoteDrawingInRegion(gfx::DrawTarget* aDrawTarget,
                                                LayoutDeviceIntRegion& aInvalidRegion)
{
  mWidget->EndRemoteDrawingInRegion(aDrawTarget, aInvalidRegion);
}

void
InProcessCompositorWidget::CleanupRemoteDrawing()
{
  mWidget->CleanupRemoteDrawing();
}

void
InProcessCompositorWidget::CleanupWindowEffects()
{
  mWidget->CleanupWindowEffects();
}

bool
InProcessCompositorWidget::InitCompositor(layers::Compositor* aCompositor)
{
  return mWidget->InitCompositor(aCompositor);
}

LayoutDeviceIntSize
InProcessCompositorWidget::GetClientSize()
{
  return mWidget->GetClientSize();
}

uint32_t
InProcessCompositorWidget::GetGLFrameBufferFormat()
{
  return mWidget->GetGLFrameBufferFormat();
}

layers::Composer2D*
InProcessCompositorWidget::GetComposer2D()
{
  return mWidget->GetComposer2D();
}

uintptr_t
InProcessCompositorWidget::GetWidgetKey()
{
  return reinterpret_cast<uintptr_t>(mWidget);
}

nsIWidget*
InProcessCompositorWidget::RealWidget()
{
  return mWidget;
}

void
InProcessCompositorWidget::ObserveVsync(VsyncObserver* aObserver)
{
  RefPtr<CompositorVsyncDispatcher> cvd = mWidget->GetCompositorVsyncDispatcher();
  cvd->SetCompositorVsyncObserver(aObserver);
}

} // namespace widget
} // namespace mozilla

