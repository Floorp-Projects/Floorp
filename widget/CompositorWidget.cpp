/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorWidget.h"
#include "GLConsts.h"
#include "nsBaseWidget.h"
#include "VsyncDispatcher.h"

namespace mozilla {
namespace widget {

CompositorWidget::CompositorWidget(const layers::CompositorOptions& aOptions)
    : mOptions(aOptions) {}

CompositorWidget::~CompositorWidget() = default;

already_AddRefed<gfx::DrawTarget> CompositorWidget::StartRemoteDrawing() {
  return nullptr;
}

void CompositorWidget::CleanupRemoteDrawing() { mLastBackBuffer = nullptr; }

already_AddRefed<gfx::DrawTarget> CompositorWidget::GetBackBufferDrawTarget(
    gfx::DrawTarget* aScreenTarget, const gfx::IntRect& aRect,
    bool* aOutIsCleared) {
  MOZ_ASSERT(aScreenTarget);
  gfx::SurfaceFormat format =
      aScreenTarget->GetFormat() == gfx::SurfaceFormat::B8G8R8X8
          ? gfx::SurfaceFormat::B8G8R8X8
          : gfx::SurfaceFormat::B8G8R8A8;
  gfx::IntSize size = aRect.Size();
  gfx::IntSize clientSize = Max(size, GetClientSize().ToUnknownSize());

  *aOutIsCleared = false;
  // Re-use back buffer if possible
  if (!mLastBackBuffer ||
      mLastBackBuffer->GetBackendType() != aScreenTarget->GetBackendType() ||
      mLastBackBuffer->GetFormat() != format ||
      mLastBackBuffer->GetSize() != clientSize) {
    mLastBackBuffer =
        aScreenTarget->CreateSimilarDrawTarget(clientSize, format);
    *aOutIsCleared = true;
  }
  return do_AddRef(mLastBackBuffer);
}

already_AddRefed<gfx::SourceSurface> CompositorWidget::EndBackBufferDrawing() {
  RefPtr<gfx::SourceSurface> surface =
      mLastBackBuffer ? mLastBackBuffer->Snapshot() : nullptr;
  return surface.forget();
}

uint32_t CompositorWidget::GetGLFrameBufferFormat() { return LOCAL_GL_RGBA; }

RefPtr<VsyncObserver> CompositorWidget::GetVsyncObserver() const {
  // This should only used when the widget is in the GPU process, and should be
  // implemented by IPDL-enabled CompositorWidgets.
  // GPU process does not have a CompositorVsyncDispatcher.
  MOZ_ASSERT_UNREACHABLE("Must be implemented by derived class");
  return nullptr;
}

LayoutDeviceIntRegion CompositorWidget::GetTransparentRegion() {
  // By default, we check the transparency mode to determine if the widget is
  // transparent, and if so, designate the entire widget drawing area as
  // transparent. Widgets wanting more complex transparency region determination
  // should override this method.
  auto* widget = RealWidget();
  if (!widget || widget->GetTransparencyMode() != TransparencyMode::Opaque ||
      widget->WidgetPaintsBackground()) {
    return LayoutDeviceIntRect(LayoutDeviceIntPoint(0, 0), GetClientSize());
  }
  return LayoutDeviceIntRegion();
}

}  // namespace widget
}  // namespace mozilla
