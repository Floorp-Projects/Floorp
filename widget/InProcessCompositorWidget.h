/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_InProcessCompositorWidget_h__
#define mozilla_widget_InProcessCompositorWidget_h__

#include "CompositorWidget.h"

namespace mozilla {
namespace widget {

// This version of CompositorWidget implements a wrapper around
// nsBaseWidget.
class InProcessCompositorWidget : public CompositorWidget
{
public:
  explicit InProcessCompositorWidget(nsBaseWidget* aWidget);

  virtual bool PreRender(WidgetRenderingContext* aManager) override;
  virtual void PostRender(WidgetRenderingContext* aManager) override;
  virtual void DrawWindowUnderlay(WidgetRenderingContext* aContext,
                                  LayoutDeviceIntRect aRect) override;
  virtual void DrawWindowOverlay(WidgetRenderingContext* aContext,
                                 LayoutDeviceIntRect aRect) override;
  virtual already_AddRefed<gfx::DrawTarget> StartRemoteDrawing() override;
  virtual already_AddRefed<gfx::DrawTarget>
  StartRemoteDrawingInRegion(LayoutDeviceIntRegion& aInvalidRegion,
                             layers::BufferMode* aBufferMode) override;
  virtual void EndRemoteDrawing() override;
  virtual void EndRemoteDrawingInRegion(gfx::DrawTarget* aDrawTarget,
                                        LayoutDeviceIntRegion& aInvalidRegion) override;
  virtual void CleanupRemoteDrawing() override;
  virtual void CleanupWindowEffects() override;
  virtual bool InitCompositor(layers::Compositor* aCompositor) override;
  virtual LayoutDeviceIntSize GetClientSize() override;
  virtual uint32_t GetGLFrameBufferFormat() override;
  virtual layers::Composer2D* GetComposer2D() override;
  virtual void ObserveVsync(VsyncObserver* aObserver) override;
  virtual uintptr_t GetWidgetKey() override;

  // If you can override this method, inherit from CompositorWidget instead.
  nsIWidget* RealWidget() override;

private:
  nsBaseWidget* mWidget;
};

} // namespace widget
} // namespace mozilla

#endif
