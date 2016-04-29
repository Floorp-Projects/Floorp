/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_CompositorWidgetProxy_h__
#define mozilla_widget_CompositorWidgetProxy_h__

#include "nsISupports.h"
#include "mozilla/RefPtr.h"
#include "Units.h"

namespace mozilla {
namespace layers {
class Compositor;
class LayerManagerComposite;
class Compositor;
class Composer2D;
} // namespace layers
namespace gfx {
class DrawTarget;
} // namespace gfx
namespace widget {

/**
 * Access to a widget from the compositor is restricted to these methods.
 */
class CompositorWidgetProxy
{
public:
  /**
   * Called before rendering using OMTC. Returns false when the widget is
   * not ready to be rendered (for example while the window is closed).
   *
   * Always called from the compositing thread, which may be the main-thread if
   * OMTC is not enabled.
   */
  virtual bool PreRender(layers::LayerManagerComposite* aManager) {
    return true;
  }

  /**
   * Called after rendering using OMTC. Not called when rendering was
   * cancelled by a negative return value from PreRender.
   *
   * Always called from the compositing thread, which may be the main-thread if
   * OMTC is not enabled.
   */
  virtual void PostRender(layers::LayerManagerComposite* aManager)
  {}

  /**
   * Called before the LayerManager draws the layer tree.
   *
   * Always called from the compositing thread.
   */
  virtual void DrawWindowUnderlay(layers::LayerManagerComposite* aManager,
                                  LayoutDeviceIntRect aRect)
  {}

  /**
   * Called after the LayerManager draws the layer tree
   *
   * Always called from the compositing thread.
   */
  virtual void DrawWindowOverlay(layers::LayerManagerComposite* aManager,
                                 LayoutDeviceIntRect aRect)
  {}

  /**
   * Return a DrawTarget for the window which can be composited into.
   *
   * Called by BasicCompositor on the compositor thread for OMTC drawing
   * before each composition.
   *
   * The window may specify its buffer mode. If unspecified, it is assumed
   * to require double-buffering.
   */
  virtual already_AddRefed<gfx::DrawTarget> StartRemoteDrawing() = 0;
  virtual already_AddRefed<gfx::DrawTarget>
  StartRemoteDrawingInRegion(LayoutDeviceIntRegion& aInvalidRegion,
                             layers::BufferMode* aBufferMode)
  {
    return StartRemoteDrawing();
  }

  /**
   * Ensure that what was painted into the DrawTarget returned from
   * StartRemoteDrawing reaches the screen.
   *
   * Called by BasicCompositor on the compositor thread for OMTC drawing
   * after each composition.
   */
  virtual void EndRemoteDrawing()
  {}
  virtual void EndRemoteDrawingInRegion(gfx::DrawTarget* aDrawTarget,
                                        LayoutDeviceIntRegion& aInvalidRegion)
  {
    EndRemoteDrawing();
  }

  /**
   * Clean up any resources used by Start/EndRemoteDrawing.
   *
   * Called by BasicCompositor on the compositor thread for OMTC drawing
   * when the compositor is destroyed.
   */
  virtual void CleanupRemoteDrawing() = 0;

  /**
   * Called when shutting down the LayerManager to clean-up any cached resources.
   *
   * Always called from the compositing thread.
   */
  virtual void CleanupWindowEffects()
  {}

  /**
   * Create DrawTarget used as BackBuffer of the screen
   */
  virtual already_AddRefed<gfx::DrawTarget>
  CreateBackBufferDrawTarget(gfx::DrawTarget* aScreenTarget,
                             const LayoutDeviceIntRect& aRect,
                             const LayoutDeviceIntRect& aClearRect) = 0;

  /**
   * A hook for the widget to prepare a Compositor, during the latter's initialization.
   *
   * If this method returns true, it means that the widget will be able to
   * present frames from the compoositor.
   *
   * Returning false will cause the compositor's initialization to fail, and
   * a different compositor backend will be used (if any).
   */
  virtual bool InitCompositor(layers::Compositor* aCompositor) {
    return true;
  }

  /**
   * Return the size of the drawable area of the widget.
   */
  virtual LayoutDeviceIntSize GetClientSize() = 0;

  /**
   * Return the internal format of the default framebuffer for this
   * widget.
   */
  virtual uint32_t GetGLFrameBufferFormat() {
    return 0; /* GL_NONE */
  }

  /**
   * If this widget has a more efficient composer available for its
   * native framebuffer, return it.
   *
   * This can be called from a non-main thread, but that thread must
   * hold a strong reference to this.
   */
  virtual layers::Composer2D* GetComposer2D() {
    return nullptr;
  }

  /*
   * Access the underlying nsIWidget. This method will be removed when the compositor no longer
   * depends on nsIWidget on any platform.
   */
  virtual nsIWidget* RealWidget() = 0;
};


} // namespace widget
} // namespace mozilla

#endif
