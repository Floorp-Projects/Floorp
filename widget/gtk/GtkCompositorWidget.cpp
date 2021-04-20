/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GtkCompositorWidget.h"

#include "mozilla/layers/CompositorThread.h"
#include "mozilla/widget/InProcessCompositorWidget.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#include "nsWindow.h"

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
  }
#endif
#if defined(MOZ_X11)
  if (aInitData.IsX11Display()) {
    // If we have a nsWindow, then grab the already existing display connection
    // If we don't, then use the init data to connect to the display
    if (aWindow) {
      mXDisplay = aWindow->XDisplay();
    } else {
      mXDisplay = XOpenDisplay(aInitData.XDisplayString().get());
    }
    mXWindow = (Window)aInitData.XWindow();

    // Grab the window's visual and depth
    XWindowAttributes windowAttrs;
    if (!XGetWindowAttributes(mXDisplay, mXWindow, &windowAttrs)) {
      NS_WARNING("GtkCompositorWidget(): XGetWindowAttributes() failed!");
    }

    Visual* visual = windowAttrs.visual;
    mDepth = windowAttrs.depth;

    // Initialize the window surface provider
    mProvider.Initialize(mXDisplay, mXWindow, visual, mDepth,
                         aInitData.Shaped());
  }
#endif
  auto size = mClientSize.Lock();
  *size = aInitData.InitialClientSize();
}

GtkCompositorWidget::~GtkCompositorWidget() {
  mProvider.CleanupResources();

#if defined(MOZ_X11)
  // If we created our own display connection, we need to destroy it
  if (!mWidget && mXDisplay) {
    XCloseDisplay(mXDisplay);
    mXDisplay = nullptr;
  }
#endif
}

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

}  // namespace widget
}  // namespace mozilla
