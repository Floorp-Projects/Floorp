/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GtkCompositorWidget.h"

#include "mozilla/layers/CompositorThread.h"
#include "mozilla/widget/InProcessCompositorWidget.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#include "nsWindow.h"

#ifdef MOZ_X11
#  include "mozilla/X11Util.h"
#endif

#ifdef MOZ_WAYLAND
#  include "mozilla/layers/NativeLayerWayland.h"
#endif

namespace mozilla {
namespace widget {

GtkCompositorWidget::GtkCompositorWidget(
    const GtkCompositorWidgetInitData& aInitData,
    const layers::CompositorOptions& aOptions, RefPtr<nsWindow> aWindow)
    : CompositorWidget(aOptions),
      mWidget(std::move(aWindow)),
      mClientSize("GtkCompositorWidget::mClientSize") {
#if defined(MOZ_WAYLAND)
  if (GdkIsWaylandDisplay()) {
    if (!mWidget) {
      NS_WARNING("GtkCompositorWidget: We're missing nsWindow!");
    }
    mProvider.Initialize(mWidget);
  }
#endif
#if defined(MOZ_X11)
  if (GdkIsX11Display()) {
    mXWindow = (Window)aInitData.XWindow();

    // Grab the window's visual and depth
    XWindowAttributes windowAttrs;
    if (!XGetWindowAttributes(DefaultXDisplay(), mXWindow, &windowAttrs)) {
      NS_WARNING("GtkCompositorWidget(): XGetWindowAttributes() failed!");
    }

    Visual* visual = windowAttrs.visual;
    int depth = windowAttrs.depth;

    // Initialize the window surface provider
    mProvider.Initialize(mXWindow, visual, depth, aInitData.Shaped());
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

void GtkCompositorWidget::RemoteLayoutSizeUpdated(
    const LayoutDeviceRect& aSize) {
  if (!mWidget || !mWidget->IsWaitingForCompositorResume()) {
    return;
  }

  // We're waiting for layout to match widget size.
  auto clientSize = mClientSize.Lock();
  if (clientSize->width != (int)aSize.width ||
      clientSize->height != (int)aSize.height) {
    return;
  }

  mWidget->ResumeCompositorFromCompositorThread();
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

#if defined(MOZ_WAYLAND)
void GtkCompositorWidget::SetEGLNativeWindowSize(
    const LayoutDeviceIntSize& aEGLWindowSize) {
  if (mWidget) {
    mWidget->SetEGLNativeWindowSize(aEGLWindowSize);
  }
}
#endif

LayoutDeviceIntRegion GtkCompositorWidget::GetTransparentRegion() {
  // We need to clear target buffer alpha values of popup windows as
  // SW-WR paints with alpha blending (see Bug 1674473).
  if (!mWidget || mWidget->IsPopup()) {
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
