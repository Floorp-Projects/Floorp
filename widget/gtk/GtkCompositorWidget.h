/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef widget_gtk_GtkCompositorWidget_h
#define widget_gtk_GtkCompositorWidget_h

#include "GLDefs.h"
#include "mozilla/DataMutex.h"
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
      const LayoutDeviceIntRegion& aInvalidRegion,
      layers::BufferMode* aBufferMode) override;
  void EndRemoteDrawingInRegion(
      gfx::DrawTarget* aDrawTarget,
      const LayoutDeviceIntRegion& aInvalidRegion) override;
  uintptr_t GetWidgetKey() override;

  LayoutDeviceIntSize GetClientSize() override;

  nsIWidget* RealWidget() override;
  GtkCompositorWidget* AsGTK() override { return this; }
  CompositorWidgetDelegate* AsDelegate() override { return this; }

  EGLNativeWindowType GetEGLNativeWindow();
  int32_t GetDepth();

  LayoutDeviceIntRegion GetTransparentRegion() override;

#if defined(MOZ_X11)
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
  // This field is written to on the main thread and read from on the compositor
  // or renderer thread. During window resizing, this is subject to a (largely
  // benign) read/write race, see bug 1665726. The DataMutex doesn't prevent the
  // read/write race, but it does make it Not Undefined Behaviour, and also
  // ensures we only ever use the old or new size, and not some weird synthesis
  // of the two.
  DataMutex<LayoutDeviceIntSize> mClientSize;

  WindowSurfaceProvider mProvider;

#if defined(MOZ_X11)
  Window mXWindow = {};
#endif
  int32_t mDepth = {};
};

}  // namespace widget
}  // namespace mozilla

#endif  // widget_gtk_GtkCompositorWidget_h
