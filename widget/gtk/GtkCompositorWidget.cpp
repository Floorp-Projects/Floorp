/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GtkCompositorWidget.h"

#include "mozilla/layers/CompositorThread.h"
#include "mozilla/widget/InProcessCompositorWidget.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#include "nsWindow.h"
#include "mozilla/X11Util.h"

#ifdef MOZ_WAYLAND
#  include "mozilla/layers/NativeLayerWayland.h"
#endif

namespace mozilla {
namespace widget {

GtkCompositorWidget::GtkCompositorWidget(
    const GtkCompositorWidgetInitData& aInitData,
    const layers::CompositorOptions& aOptions, nsWindow* aWindow)
    : CompositorWidget(aOptions),
      mWidget(aWindow),
      mClientSize("GtkCompositorWidget::mClientSize") {
#if defined(MOZ_WAYLAND)
  if (!aInitData.IsX11Display()) {
    if (!aWindow) {
      NS_WARNING("GtkCompositorWidget: We're missing nsWindow!");
    }
    mProvider.Initialize(aWindow);
    mNativeLayerRoot = nullptr;
  }
#endif
#if defined(MOZ_X11)
  if (aInitData.IsX11Display()) {
    mXWindow = (Window)aInitData.XWindow();

    // Grab the window's visual and depth
    XWindowAttributes windowAttrs;
    if (!XGetWindowAttributes(DefaultXDisplay(), mXWindow, &windowAttrs)) {
      NS_WARNING("GtkCompositorWidget(): XGetWindowAttributes() failed!");
    }

    Visual* visual = windowAttrs.visual;
    mDepth = windowAttrs.depth;

    // Initialize the window surface provider
    mProvider.Initialize(mXWindow, visual, mDepth, aInitData.Shaped());
  }
#endif
  auto size = mClientSize.Lock();
  *size = aInitData.InitialClientSize();
}

GtkCompositorWidget::~GtkCompositorWidget() { mProvider.CleanupResources(); }

already_AddRefed<gfx::DrawTarget> GtkCompositorWidget::StartRemoteDrawing() {
  return nullptr;
}
void GtkCompositorWidget::EndRemoteDrawing() {}

already_AddRefed<gfx::DrawTarget>
GtkCompositorWidget::StartRemoteDrawingInRegion(
    const LayoutDeviceIntRegion& aInvalidRegion,
    layers::BufferMode* aBufferMode) {
  return mProvider.StartRemoteDrawingInRegion(aInvalidRegion, aBufferMode);
}

void GtkCompositorWidget::EndRemoteDrawingInRegion(
    gfx::DrawTarget* aDrawTarget, const LayoutDeviceIntRegion& aInvalidRegion) {
  mProvider.EndRemoteDrawingInRegion(aDrawTarget, aInvalidRegion);
}

nsIWidget* GtkCompositorWidget::RealWidget() { return mWidget; }

void GtkCompositorWidget::NotifyClientSizeChanged(
    const LayoutDeviceIntSize& aClientSize) {
  auto size = mClientSize.Lock();
  *size = aClientSize;
}

LayoutDeviceIntSize GtkCompositorWidget::GetClientSize() {
  auto size = mClientSize.Lock();
  return *size;
}

uintptr_t GtkCompositorWidget::GetWidgetKey() {
  return reinterpret_cast<uintptr_t>(mWidget);
}

EGLNativeWindowType GtkCompositorWidget::GetEGLNativeWindow() {
  if (mWidget) {
    return (EGLNativeWindowType)mWidget->GetNativeData(NS_NATIVE_EGL_WINDOW);
  }
#if defined(MOZ_X11)
  if (mXWindow) {
    return (EGLNativeWindowType)mXWindow;
  }
#endif
  return nullptr;
}

int32_t GtkCompositorWidget::GetDepth() { return mDepth; }

#if defined(MOZ_WAYLAND)
void GtkCompositorWidget::SetEGLNativeWindowSize(
    const LayoutDeviceIntSize& aEGLWindowSize) {
  if (mWidget) {
    mWidget->SetEGLNativeWindowSize(aEGLWindowSize);
  }
}
#endif

LayoutDeviceIntRegion GtkCompositorWidget::GetTransparentRegion() {
  if (!mWidget) {
    return LayoutDeviceIntRect();
  }

  // We need to clear target buffer alpha values of popup windows as
  // SW-WR paints with alpha blending (see Bug 1674473).
  if (mWidget->IsPopup()) {
    return LayoutDeviceIntRect(LayoutDeviceIntPoint(0, 0), GetClientSize());
  }

  // Clear background of titlebar area to render titlebar
  // transparent corners correctly.
  return mWidget->GetTitlebarRect();
}

#ifdef MOZ_WAYLAND
RefPtr<mozilla::layers::NativeLayerRoot>
GtkCompositorWidget::GetNativeLayerRoot() {
  if (gfx::gfxVars::UseWebRenderCompositor()) {
    if (!mNativeLayerRoot) {
      MOZ_ASSERT(mWidget && mWidget->GetMozContainer());
      mNativeLayerRoot = NativeLayerRootWayland::CreateForMozContainer(
          mWidget->GetMozContainer());
    }
    return mNativeLayerRoot;
  }
  return nullptr;
}
#endif

}  // namespace widget
}  // namespace mozilla
