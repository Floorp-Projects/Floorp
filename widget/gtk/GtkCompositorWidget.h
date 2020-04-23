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

  Display* XDisplay() const { return mXDisplay; }
  Window XWindow() const { return mXWindow; }

  EGLNativeWindowType GetEGLNativeWindow();
#ifdef MOZ_WAYLAND
  void SetEGLNativeWindowSize(const LayoutDeviceIntSize& aEGLWindowSize);
#endif

  // PlatformCompositorWidgetDelegate Overrides

  void NotifyClientSizeChanged(const LayoutDeviceIntSize& aClientSize) override;

 protected:
  nsWindow* mWidget;

 private:
  LayoutDeviceIntSize mClientSize;

  Display* mXDisplay;
  Window mXWindow;
  WindowSurfaceProvider mProvider;
};

}  // namespace widget
}  // namespace mozilla

#endif  // widget_gtk_GtkCompositorWidget_h
