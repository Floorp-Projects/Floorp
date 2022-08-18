/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_AndroidCompositorWidget_h
#define mozilla_widget_AndroidCompositorWidget_h

#include "CompositorWidget.h"
#include "AndroidNativeWindow.h"
#include "GLDefs.h"

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

class AndroidCompositorWidgetInitData;

class AndroidCompositorWidget : public CompositorWidget {
 public:
  AndroidCompositorWidget(const AndroidCompositorWidgetInitData& aInitData,
                          const layers::CompositorOptions& aOptions);
  ~AndroidCompositorWidget() override;

  EGLNativeWindowType GetEGLNativeWindow();

  // CompositorWidget overrides

  already_AddRefed<gfx::DrawTarget> StartRemoteDrawingInRegion(
      const LayoutDeviceIntRegion& aInvalidRegion,
      layers::BufferMode* aBufferMode) override;
  void EndRemoteDrawingInRegion(
      gfx::DrawTarget* aDrawTarget,
      const LayoutDeviceIntRegion& aInvalidRegion) override;

  bool OnResumeComposition() override;

  AndroidCompositorWidget* AsAndroid() override { return this; }

  LayoutDeviceIntSize GetClientSize() override;
  void NotifyClientSizeChanged(const LayoutDeviceIntSize& aClientSize);

 protected:
  int32_t mWidgetId;
  java::sdk::Surface::GlobalRef mSurface;
  ANativeWindow* mNativeWindow;
  ANativeWindow_Buffer mBuffer;
  int32_t mFormat;
  LayoutDeviceIntSize mClientSize;

 private:
  // Called whenever the compositor surface may have changed. The derived class
  // should update mSurface to the new compositor surface.
  virtual void OnCompositorSurfaceChanged() = 0;
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_AndroidCompositorWidget_h
