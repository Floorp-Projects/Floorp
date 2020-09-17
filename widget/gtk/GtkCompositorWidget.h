/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_gtk_GtkCompositorWidget_h
#define widget_gtk_GtkCompositorWidget_h

#include "GLDefs.h"
#include "mozilla/widget/CompositorWidget.h"
#include "WindowSurfaceProvider.h"

class nsIWidget;
class nsWindow;

namespace mozilla {
namespace widget {

class PlatformCompositorWidgetDelegate : public CompositorWidgetDelegate {
 public:
  virtual void NotifyClientSizeChanged(
      const LayoutDeviceIntSize& aClientSize) = 0;

  // CompositorWidgetDelegate Overrides

  PlatformCompositorWidgetDelegate* AsPlatformSpecificDelegate() override {
    return this;
  }
};

class GtkCompositorWidgetInitData;

class GtkCompositorWidget : public CompositorWidget,
                            public PlatformCompositorWidgetDelegate {
 public:
  GtkCompositorWidget(const GtkCompositorWidgetInitData& aInitData,
                      const layers::CompositorOptions& aOptions,
                      nsWindow* aWindow /* = nullptr*/);
  ~GtkCompositorWidget();

  // CompositorWidget Overrides

  already_AddRefed<gfx::DrawTarget> StartRemoteDrawing() override;
  void EndRemoteDrawing() override;

  already_AddRefed<gfx::DrawTarget> StartRemoteDrawingInRegion(
      LayoutDeviceIntRegion& aInvalidRegion,
      layers::BufferMode* aBufferMode) override;
  void EndRemoteDrawingInRegion(
      gfx::DrawTarget* aDrawTarget,
      const LayoutDeviceIntRegion& aInvalidRegion) override;
  uintptr_t GetWidgetKey() override;

  LayoutDeviceIntSize GetClientSize() override;

  nsIWidget* RealWidget() override;
  GtkCompositorWidget* AsX11() override { return this; }
  CompositorWidgetDelegate* AsDelegate() override { return this; }

  EGLNativeWindowType GetEGLNativeWindow();
  int32_t GetDepth();

#if defined(MOZ_X11)
  Display* XDisplay() const { return mXDisplay; }
  Window XWindow() const { return mXWindow; }
#endif
#if defined(MOZ_WAYLAND)
  void SetEGLNativeWindowSize(const LayoutDeviceIntSize& aEGLWindowSize);
#endif

  // PlatformCompositorWidgetDelegate Overrides

  void NotifyClientSizeChanged(const LayoutDeviceIntSize& aClientSize) override;

 protected:
  nsWindow* mWidget;

 private:
  // Webrender can try to poll this while we're handling a window resize event.
  // This read/write race is largely benign because it's fine if webrender and
  // the window desync for a frame (leading to the page displaying
  // larger/smaller than the window for a split second) -- nobody expects
  // perfect rendering while resizing a window. This atomic doesn't change the
  // fact that the window and content can display at different sizes, but it
  // does make it Not Undefined Behaviour, and also ensures webrender only
  // ever uses the old or new size, and not some weird synthesis of the two.
  Atomic<LayoutDeviceIntSize> mClientSize;

  WindowSurfaceProvider mProvider;

#if defined(MOZ_X11)
  Display* mXDisplay = {};
  Window mXWindow = {};
#endif
  int32_t mDepth = {};
};

}  // namespace widget
}  // namespace mozilla

#endif  // widget_gtk_GtkCompositorWidget_h
