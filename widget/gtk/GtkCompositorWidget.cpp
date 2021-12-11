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

#ifdef MOZ_LOGGING
#  undef LOG
#  define LOG(...)                                    \
    MOZ_LOG(IsPopup() ? gWidgetPopupLog : gWidgetLog, \
            mozilla::LogLevel::Debug, (__VA_ARGS__))
#endif /* MOZ_LOGGING */

namespace mozilla {
namespace widget {

GtkCompositorWidget::GtkCompositorWidget(
    const GtkCompositorWidgetInitData& aInitData,
    const layers::CompositorOptions& aOptions, RefPtr<nsWindow> aWindow)
    : CompositorWidget(aOptions),
      mWidget(std::move(aWindow)),
      mClientSize("GtkCompositorWidget::mClientSize"),
      mIsRenderingSuspended(true) {
#if defined(MOZ_WAYLAND)
  if (GdkIsWaylandDisplay()) {
    ConfigureWaylandBackend(mWidget);
  }
#endif
#if defined(MOZ_X11)
  if (GdkIsX11Display()) {
    mXWindow = (Window)aInitData.XWindow();
    ConfigureX11Backend(mXWindow, aInitData.Shaped());
  }
#endif
  auto size = mClientSize.Lock();
  *size = aInitData.InitialClientSize();

  LOG("GtkCompositorWidget::GtkCompositorWidget() [%p] mXWindow %p "
      "mIsRenderingSuspended %d\n",
      (void*)mWidget.get(), (void*)mXWindow, !!mIsRenderingSuspended);
}

GtkCompositorWidget::~GtkCompositorWidget() {
  LOG("GtkCompositorWidget::~GtkCompositorWidget [%p]\n", (void*)mWidget.get());
  DisableRendering();
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

void GtkCompositorWidget::DisableRendering() {
  LOG("GtkCompositorWidget::DisableRendering [%p]\n", (void*)mWidget.get());
  mIsRenderingSuspended = true;
  mProvider.CleanupResources();
#if defined(MOZ_X11)
  mXWindow = {};
#endif
}

#if defined(MOZ_WAYLAND)
bool GtkCompositorWidget::ConfigureWaylandBackend(RefPtr<nsWindow> aWindow) {
  mProvider.Initialize(aWindow);
  return true;
}
#endif

#if defined(MOZ_X11)
bool GtkCompositorWidget::ConfigureX11Backend(Window aXWindow, bool aShaped) {
  mXWindow = aXWindow;

  // We don't have X window yet.
  if (!mXWindow) {
    mIsRenderingSuspended = true;
    return false;
  }

  // Grab the window's visual and depth
  XWindowAttributes windowAttrs;
  if (!XGetWindowAttributes(DefaultXDisplay(), mXWindow, &windowAttrs)) {
    NS_WARNING("GtkCompositorWidget(): XGetWindowAttributes() failed!");
    return false;
  }

  Visual* visual = windowAttrs.visual;
  int depth = windowAttrs.depth;

  // Initialize the window surface provider
  mProvider.Initialize(mXWindow, visual, depth, aShaped);
  return true;
}
#endif

void GtkCompositorWidget::EnableRendering(const uintptr_t aXWindow,
                                          const bool aShaped) {
  LOG("GtkCompositorWidget::EnableRendering() [%p]\n", (void*)mWidget.get());

  if (!mIsRenderingSuspended) {
    LOG("  quit, mIsRenderingSuspended = false\n");
    return;
  }
#if defined(MOZ_WAYLAND)
  if (GdkIsWaylandDisplay()) {
    LOG("  configure widget %p\n", mWidget.get());
    if (!ConfigureWaylandBackend(mWidget)) {
      return;
    }
  }
#endif
#if defined(MOZ_X11)
  if (GdkIsX11Display()) {
    LOG("  configure XWindow %p shaped %d\n", (void*)aXWindow, aShaped);
    if (!ConfigureX11Backend((Window)aXWindow, aShaped)) {
      return;
    }
  }
#endif
  mIsRenderingSuspended = false;
}
#ifdef MOZ_LOGGING
bool GtkCompositorWidget::IsPopup() {
  return mWidget ? mWidget->IsPopup() : false;
}
#endif

}  // namespace widget
}  // namespace mozilla
