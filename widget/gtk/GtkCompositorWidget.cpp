/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GtkCompositorWidget.h"

#include "gfxPlatformGtk.h"
#include "mozilla/layers/CompositorThread.h"
#include "mozilla/widget/InProcessCompositorWidget.h"
#include "mozilla/widget/PlatformWidgetTypes.h"
#include "nsWindow.h"

namespace mozilla {
namespace widget {

GtkCompositorWidget::GtkCompositorWidget(
    const GtkCompositorWidgetInitData& aInitData,
    const layers::CompositorOptions& aOptions, nsWindow* aWindow)
    : CompositorWidget(aOptions), mWidget(aWindow) {
  // If we have a nsWindow, then grab the already existing display connection
  // If we don't, then use the init data to connect to the display
  if (aWindow) {
    mXDisplay = aWindow->XDisplay();
  } else {
    mXDisplay = XOpenDisplay(aInitData.XDisplayString().get());
  }

#ifdef MOZ_WAYLAND
  if (!mXDisplay) {
    MOZ_RELEASE_ASSERT(
        aWindow, "We're running on Wayland and but without valid nsWindow.");
    mProvider.Initialize(aWindow);
  } else
#endif
  {
    mXWindow = (Window)aInitData.XWindow();

    // Grab the window's visual and depth
    XWindowAttributes windowAttrs;
    if (!XGetWindowAttributes(mXDisplay, mXWindow, &windowAttrs)) {
      NS_WARNING("GtkCompositorWidget(): XGetWindowAttributes() failed!");
    }

    Visual* visual = windowAttrs.visual;
    int depth = windowAttrs.depth;

    // Initialize the window surface provider
    mProvider.Initialize(mXDisplay, mXWindow, visual, depth,
                         aInitData.Shaped());
  }
  mClientSize = aInitData.InitialClientSize();
}

GtkCompositorWidget::~GtkCompositorWidget() {
  mProvider.CleanupResources();

  // If we created our own display connection, we need to destroy it
  if (!mWidget && mXDisplay) {
    XCloseDisplay(mXDisplay);
    mXDisplay = nullptr;
  }
}

already_AddRefed<gfx::DrawTarget> GtkCompositorWidget::StartRemoteDrawing() {
  return nullptr;
}
void GtkCompositorWidget::EndRemoteDrawing() {}

already_AddRefed<gfx::DrawTarget>
GtkCompositorWidget::StartRemoteDrawingInRegion(
    LayoutDeviceIntRegion& aInvalidRegion, layers::BufferMode* aBufferMode) {
  return mProvider.StartRemoteDrawingInRegion(aInvalidRegion, aBufferMode);
}

void GtkCompositorWidget::EndRemoteDrawingInRegion(
    gfx::DrawTarget* aDrawTarget, LayoutDeviceIntRegion& aInvalidRegion) {
  mProvider.EndRemoteDrawingInRegion(aDrawTarget, aInvalidRegion);
}

nsIWidget* GtkCompositorWidget::RealWidget() { return mWidget; }

void GtkCompositorWidget::NotifyClientSizeChanged(
    const LayoutDeviceIntSize& aClientSize) {
  mClientSize = aClientSize;
}

#ifdef MOZ_WAYLAND
void GtkCompositorWidget::RequestsUpdatingEGLSurface() {
  mWaylandRequestsUpdatingEGLSurface = true;
}

bool GtkCompositorWidget::WaylandRequestsUpdatingEGLSurface() {
  bool ret = mWaylandRequestsUpdatingEGLSurface;
  mWaylandRequestsUpdatingEGLSurface = false;
  return ret;
}
#endif

LayoutDeviceIntSize GtkCompositorWidget::GetClientSize() { return mClientSize; }

uintptr_t GtkCompositorWidget::GetWidgetKey() {
  return reinterpret_cast<uintptr_t>(mWidget);
}

EGLNativeWindowType GtkCompositorWidget::GetEGLNativeWindow() {
  return (EGLNativeWindowType)mWidget->GetNativeData(NS_NATIVE_EGL_WINDOW);
}

}  // namespace widget
}  // namespace mozilla
