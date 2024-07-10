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
#include "mozilla/UniquePtr.h"
#include "MozContainerSurfaceLock.h"

class nsIWidget;
class nsWindow;

namespace mozilla {

namespace layers {
class NativeLayerRootWayland;
}  // namespace layers

namespace widget {

class PlatformCompositorWidgetDelegate : public CompositorWidgetDelegate {
 public:
  virtual void NotifyClientSizeChanged(
      const LayoutDeviceIntSize& aClientSize) = 0;
  virtual GtkCompositorWidget* AsGtkCompositorWidget() { return nullptr; };

  virtual void CleanupResources() = 0;
  virtual void SetRenderingSurface(const uintptr_t aXWindow,
                                   const bool aShaped) = 0;

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
                      RefPtr<nsWindow> aWindow /* = nullptr*/);
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

  LayoutDeviceIntSize GetClientSize() override;
  void RemoteLayoutSizeUpdated(const LayoutDeviceRect& aSize);

  nsIWidget* RealWidget() override;
  GtkCompositorWidget* AsGTK() override { return this; }
  CompositorWidgetDelegate* AsDelegate() override { return this; }

  EGLNativeWindowType GetEGLNativeWindow();

  LayoutDeviceIntRegion GetTransparentRegion() override;

  // Suspend rendering of this remote widget and clear all resources.
  // Can be used when underlying window is hidden/unmapped.
  void CleanupResources() override;

  // Resume rendering with to given aXWindow (X11) or nsWindow (Wayland).
  void SetRenderingSurface(const uintptr_t aXWindow,
                           const bool aShaped) override;

  // If we fail to set window size (due to different screen scale or so)
  // we can't paint the frame by compositor.
  bool SetEGLNativeWindowSize(const LayoutDeviceIntSize& aEGLWindowSize);

#if defined(MOZ_X11)
  Window XWindow() const { return mProvider.GetXWindow(); }
#endif
#if defined(MOZ_WAYLAND)
  RefPtr<mozilla::layers::NativeLayerRoot> GetNativeLayerRoot() override;
#endif

  // PlatformCompositorWidgetDelegate Overrides

  void NotifyClientSizeChanged(const LayoutDeviceIntSize& aClientSize) override;
  GtkCompositorWidget* AsGtkCompositorWidget() override { return this; }

  UniquePtr<MozContainerSurfaceLock> LockSurface();

 private:
#if defined(MOZ_WAYLAND)
  void ConfigureWaylandBackend();
#endif
#if defined(MOZ_X11)
  void ConfigureX11Backend(Window aXWindow, bool aShaped);
#endif
#ifdef MOZ_LOGGING
  bool IsPopup();
#endif

 protected:
  RefPtr<nsWindow> mWidget;

 private:
  // This field is written to on the main thread and read from on the compositor
  // or renderer thread. During window resizing, this is subject to a (largely
  // benign) read/write race, see bug 1665726. The DataMutex doesn't prevent the
  // read/write race, but it does make it Not Undefined Behaviour, and also
  // ensures we only ever use the old or new size, and not some weird synthesis
  // of the two.
  DataMutex<LayoutDeviceIntSize> mClientSize;

  // Holds rendering resources
  WindowSurfaceProvider mProvider;

#ifdef MOZ_WAYLAND
  RefPtr<mozilla::layers::NativeLayerRootWayland> mNativeLayerRoot;
#endif
};

}  // namespace widget
}  // namespace mozilla

#endif  // widget_gtk_GtkCompositorWidget_h
