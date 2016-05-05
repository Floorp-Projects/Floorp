/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorWidgetProxy.h"
#include "GLConsts.h"
#include "nsBaseWidget.h"
#include "VsyncDispatcher.h"

namespace mozilla {
namespace widget {

CompositorWidgetProxy::~CompositorWidgetProxy()
{
}

already_AddRefed<gfx::DrawTarget>
CompositorWidgetProxy::StartRemoteDrawing()
{
  return nullptr;
}

void
CompositorWidgetProxy::CleanupRemoteDrawing()
{
  mLastBackBuffer = nullptr;
}

already_AddRefed<gfx::DrawTarget>
CompositorWidgetProxy::CreateBackBufferDrawTarget(gfx::DrawTarget* aScreenTarget,
                                                  const LayoutDeviceIntRect& aRect,
                                                  const LayoutDeviceIntRect& aClearRect)
{
  MOZ_ASSERT(aScreenTarget);
  gfx::SurfaceFormat format = gfx::SurfaceFormat::B8G8R8A8;
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

uint32_t
CompositorWidgetProxy::GetGLFrameBufferFormat()
{
  return LOCAL_GL_RGBA;
}

CompositorWidgetProxyWrapper::CompositorWidgetProxyWrapper(nsBaseWidget* aWidget)
 : mWidget(aWidget)
{
}

bool
CompositorWidgetProxyWrapper::PreRender(layers::LayerManagerComposite* aManager)
{
  return mWidget->PreRender(aManager);
}

void
CompositorWidgetProxyWrapper::PostRender(layers::LayerManagerComposite* aManager)
{
  mWidget->PostRender(aManager);
}

void
CompositorWidgetProxyWrapper::DrawWindowUnderlay(layers::LayerManagerComposite* aManager,
                                                 LayoutDeviceIntRect aRect)
{
  mWidget->DrawWindowUnderlay(aManager, aRect);
}

void
CompositorWidgetProxyWrapper::DrawWindowOverlay(layers::LayerManagerComposite* aManager,
                                                LayoutDeviceIntRect aRect)
{
  mWidget->DrawWindowOverlay(aManager, aRect);
}

already_AddRefed<gfx::DrawTarget>
CompositorWidgetProxyWrapper::StartRemoteDrawing()
{
  return mWidget->StartRemoteDrawing();
}

already_AddRefed<gfx::DrawTarget>
CompositorWidgetProxyWrapper::StartRemoteDrawingInRegion(LayoutDeviceIntRegion& aInvalidRegion,
                                                         layers::BufferMode* aBufferMode)
{
  return mWidget->StartRemoteDrawingInRegion(aInvalidRegion, aBufferMode);
}

void
CompositorWidgetProxyWrapper::EndRemoteDrawing()
{
  mWidget->EndRemoteDrawing();
}

void
CompositorWidgetProxyWrapper::EndRemoteDrawingInRegion(gfx::DrawTarget* aDrawTarget,
                                                       LayoutDeviceIntRegion& aInvalidRegion)
{
  mWidget->EndRemoteDrawingInRegion(aDrawTarget, aInvalidRegion);
}

void
CompositorWidgetProxyWrapper::CleanupRemoteDrawing()
{
  mWidget->CleanupRemoteDrawing();
}

void
CompositorWidgetProxyWrapper::CleanupWindowEffects()
{
  mWidget->CleanupWindowEffects();
}

bool
CompositorWidgetProxyWrapper::InitCompositor(layers::Compositor* aCompositor)
{
  return mWidget->InitCompositor(aCompositor);
}

LayoutDeviceIntSize
CompositorWidgetProxyWrapper::GetClientSize()
{
  return mWidget->GetClientSize();
}

uint32_t
CompositorWidgetProxyWrapper::GetGLFrameBufferFormat()
{
  return mWidget->GetGLFrameBufferFormat();
}

layers::Composer2D*
CompositorWidgetProxyWrapper::GetComposer2D()
{
  return mWidget->GetComposer2D();
}

uintptr_t
CompositorWidgetProxyWrapper::GetWidgetKey()
{
  return reinterpret_cast<uintptr_t>(mWidget);
}

nsIWidget*
CompositorWidgetProxyWrapper::RealWidget()
{
  return mWidget;
}

already_AddRefed<CompositorVsyncDispatcher>
CompositorWidgetProxyWrapper::GetCompositorVsyncDispatcher()
{
  RefPtr<CompositorVsyncDispatcher> cvd = mWidget->GetCompositorVsyncDispatcher();
  return cvd.forget();
}

} // namespace widget
} // namespace mozilla
