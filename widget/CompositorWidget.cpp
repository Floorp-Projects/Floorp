/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorWidget.h"
#include "GLConsts.h"
#include "nsBaseWidget.h"
#include "VsyncDispatcher.h"

namespace mozilla {
namespace widget {

CompositorWidget::~CompositorWidget()
{
}

already_AddRefed<gfx::DrawTarget>
CompositorWidget::StartRemoteDrawing()
{
  return nullptr;
}

void
CompositorWidget::CleanupRemoteDrawing()
{
  mLastBackBuffer = nullptr;
}

already_AddRefed<gfx::DrawTarget>
CompositorWidget::GetBackBufferDrawTarget(gfx::DrawTarget* aScreenTarget,
                                          const LayoutDeviceIntRect& aRect,
                                          const LayoutDeviceIntRect& aClearRect)
{
  MOZ_ASSERT(aScreenTarget);
  gfx::SurfaceFormat format =
    aScreenTarget->GetFormat() == gfx::SurfaceFormat::B8G8R8X8 ? gfx::SurfaceFormat::B8G8R8X8 : gfx::SurfaceFormat::B8G8R8A8;
  gfx::IntSize size = aRect.ToUnknownRect().Size();
  gfx::IntSize clientSize(GetClientSize().ToUnknownSize());

  RefPtr<gfx::DrawTarget> target;
  // Re-use back buffer if possible
  if (mLastBackBuffer &&
      mLastBackBuffer->GetBackendType() == aScreenTarget->GetBackendType() &&
      mLastBackBuffer->GetFormat() == format &&
      size <= mLastBackBuffer->GetSize() &&
      mLastBackBuffer->GetSize() <= clientSize) {
    target = mLastBackBuffer;
    target->SetTransform(gfx::Matrix());
    if (!aClearRect.IsEmpty()) {
      gfx::IntRect clearRect = aClearRect.ToUnknownRect() - aRect.ToUnknownRect().TopLeft();
      target->ClearRect(gfx::Rect(clearRect.x, clearRect.y, clearRect.width, clearRect.height));
    }
  } else {
    target = aScreenTarget->CreateSimilarDrawTarget(size, format);
    mLastBackBuffer = target;
  }
  return target.forget();
}

already_AddRefed<gfx::SourceSurface>
CompositorWidget::EndBackBufferDrawing()
{
  RefPtr<gfx::SourceSurface> surface = mLastBackBuffer ? mLastBackBuffer->Snapshot() : nullptr;
  return surface.forget();
}

uint32_t
CompositorWidget::GetGLFrameBufferFormat()
{
  return LOCAL_GL_RGBA;
}

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

already_AddRefed<CompositorVsyncDispatcher>
InProcessCompositorWidget::GetCompositorVsyncDispatcher()
{
  RefPtr<CompositorVsyncDispatcher> cvd = mWidget->GetCompositorVsyncDispatcher();
  return cvd.forget();
}

} // namespace widget
} // namespace mozilla
