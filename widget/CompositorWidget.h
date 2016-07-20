/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_CompositorWidget_h__
#define mozilla_widget_CompositorWidget_h__

#include "nsISupports.h"
#include "mozilla/RefPtr.h"
#include "Units.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/layers/LayersTypes.h"

class nsIWidget;
class nsBaseWidget;

namespace mozilla {
class VsyncObserver;
namespace layers {
class Compositor;
class LayerManagerComposite;
class Compositor;
class Composer2D;
} // namespace layers
namespace gfx {
class DrawTarget;
class SourceSurface;
} // namespace gfx
namespace widget {

class WinCompositorWidget;
class CompositorWidgetInitData;

// Gecko widgets usually need to communicate with the CompositorWidget with
// platform-specific messages (for example to update the window size or
// transparency). This functionality is controlled through a "host". Since
// this functionality is platform-dependent, it is only forward declared
// here.
class CompositorWidgetDelegate;

// Platforms that support out-of-process widgets.
#if defined(XP_WIN)
// CompositorWidgetParent should implement CompositorWidget and
// PCompositorWidgetParent.
class CompositorWidgetParent;

// CompositorWidgetChild should implement CompositorWidgetDelegate and
// PCompositorWidgetChild.
class CompositorWidgetChild;

# define MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING
#endif

/**
 * Access to a widget from the compositor is restricted to these methods.
 */
class CompositorWidget
{
public:
  NS_INLINE_DECL_REFCOUNTING(mozilla::widget::CompositorWidget)

  /**
   * Create an in-process compositor widget. aWidget may be ignored if the
   * platform does not require it.
   */
  static RefPtr<CompositorWidget> CreateLocal(const CompositorWidgetInitData& aInitData, nsIWidget* aWidget);

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
  virtual already_AddRefed<gfx::DrawTarget> StartRemoteDrawing();
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
   * Called when shutting down the LayerManager to clean-up any cached resources.
   *
   * Always called from the compositing thread.
   */
  virtual void CleanupWindowEffects()
  {}

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
  virtual uint32_t GetGLFrameBufferFormat();

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

  /**
   * Clean up any resources used by Start/EndRemoteDrawing.
   *
   * Called by BasicCompositor on the compositor thread for OMTC drawing
   * when the compositor is destroyed.
   */
  virtual void CleanupRemoteDrawing();

  /**
   * Return a key that can represent the widget object round-trip across the
   * CompositorBridge channel. This only needs to be implemented on GTK and
   * Windows.
   *
   * The key must be the nsIWidget pointer cast to a uintptr_t. See
   * CompositorBridgeChild::RecvHideAllPlugins and
   * CompositorBridgeParent::SendHideAllPlugins.
   */
  virtual uintptr_t GetWidgetKey() {
    return 0;
  }

  /**
   * Create a backbuffer for the software compositor.
   */
  virtual already_AddRefed<gfx::DrawTarget>
  GetBackBufferDrawTarget(gfx::DrawTarget* aScreenTarget,
                          const LayoutDeviceIntRect& aRect,
                          const LayoutDeviceIntRect& aClearRect);

  /**
   * Ensure end of composition to back buffer.
   *
   * Called by BasicCompositor on the compositor thread for OMTC drawing
   * after each composition to back buffer.
   */
  virtual already_AddRefed<gfx::SourceSurface> EndBackBufferDrawing();

  /**
   * Observe or unobserve vsync.
   */
  virtual void ObserveVsync(VsyncObserver* aObserver) = 0;

  /**
   * This is only used by out-of-process compositors.
   */
  virtual RefPtr<VsyncObserver> GetVsyncObserver() const;

  virtual WinCompositorWidget* AsWindows() {
    return nullptr;
  }

  /**
   * Return the platform-specific delegate for the widget, if any.
   */
  virtual CompositorWidgetDelegate* AsDelegate() {
    return nullptr;
  }

protected:
  virtual ~CompositorWidget();

  // Back buffer of BasicCompositor
  RefPtr<gfx::DrawTarget> mLastBackBuffer;
};

} // namespace widget
} // namespace mozilla

#endif
