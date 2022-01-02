/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_CompositorWidget_h__
#define mozilla_widget_CompositorWidget_h__

#include "nsISupports.h"
#include "mozilla/RefPtr.h"
#include "Units.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/layers/LayersTypes.h"

#ifdef MOZ_IS_GCC
#  include "mozilla/layers/NativeLayer.h"
#endif

class nsIWidget;
class nsBaseWidget;

namespace mozilla {
class VsyncObserver;
namespace gl {
class GLContext;
}  // namespace gl
namespace layers {
class Compositor;
class LayerManager;
class NativeLayerRoot;
}  // namespace layers
namespace gfx {
class DrawTarget;
class SourceSurface;
}  // namespace gfx
namespace widget {

class WinCompositorWidget;
class GtkCompositorWidget;
class AndroidCompositorWidget;
class CompositorWidgetInitData;

// Gecko widgets usually need to communicate with the CompositorWidget with
// platform-specific messages (for example to update the window size or
// transparency). This functionality is controlled through a "host". Since
// this functionality is platform-dependent, it is only forward declared
// here.
class PlatformCompositorWidgetDelegate;

// Headless mode uses its own, singular CompositorWidget implementation.
class HeadlessCompositorWidget;

class CompositorWidgetDelegate {
 public:
  virtual PlatformCompositorWidgetDelegate* AsPlatformSpecificDelegate() {
    return nullptr;
  }

  virtual HeadlessCompositorWidget* AsHeadlessCompositorWidget() {
    return nullptr;
  }
};

// Platforms that support out-of-process widgets.
#if defined(XP_WIN) || defined(MOZ_X11) || defined(MOZ_WIDGET_ANDROID)
// CompositorWidgetParent should implement CompositorWidget and
// PCompositorWidgetParent.
class CompositorWidgetParent;

// CompositorWidgetChild should implement CompositorWidgetDelegate and
// PCompositorWidgetChild.
class CompositorWidgetChild;

#  define MOZ_WIDGET_SUPPORTS_OOP_COMPOSITING
#endif

class WidgetRenderingContext {
 public:
#if defined(XP_MACOSX)
  gl::GLContext* mGL = nullptr;
#endif
};

/**
 * Access to a widget from the compositor is restricted to these methods.
 */
class CompositorWidget {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(mozilla::widget::CompositorWidget)

  /**
   * Create an in-process compositor widget. aWidget may be ignored if the
   * platform does not require it.
   */
  static RefPtr<CompositorWidget> CreateLocal(
      const CompositorWidgetInitData& aInitData,
      const layers::CompositorOptions& aOptions, nsIWidget* aWidget);

  /**
   * Called before rendering using OMTC. Returns false when the widget is
   * not ready to be rendered (for example while the window is closed).
   *
   * Always called from the compositing thread, which may be the main-thread if
   * OMTC is not enabled.
   */
  virtual bool PreRender(WidgetRenderingContext* aContext) { return true; }

  /**
   * Called after rendering using OMTC. Not called when rendering was
   * cancelled by a negative return value from PreRender.
   *
   * Always called from the compositing thread, which may be the main-thread if
   * OMTC is not enabled.
   */
  virtual void PostRender(WidgetRenderingContext* aContext) {}

  /**
   * Called before the first composite. If the result is non-null, one or more
   * native layers will be placed on the window and used for compositing.
   * When native layers are used, StartRemoteDrawing(InRegion) and
   * EndRemoteDrawing(InRegion) will not be called.
   */
  virtual RefPtr<layers::NativeLayerRoot> GetNativeLayerRoot() {
    return nullptr;
  }

  /**
   * Return a DrawTarget for the window which can be composited into.
   *
   * Only called if GetNativeLayerRoot() returns nullptr.
   * Called by BasicCompositor on the compositor thread for OMTC drawing
   * before each composition (unless there's a native layer root).
   *
   * The window may specify its buffer mode. If unspecified, it is assumed
   * to require double-buffering.
   */
  virtual already_AddRefed<gfx::DrawTarget> StartRemoteDrawing();
  virtual already_AddRefed<gfx::DrawTarget> StartRemoteDrawingInRegion(
      const LayoutDeviceIntRegion& aInvalidRegion,
      layers::BufferMode* aBufferMode) {
    return StartRemoteDrawing();
  }

  /**
   * Ensure that what was painted into the DrawTarget returned from
   * StartRemoteDrawing reaches the screen.
   *
   * Called by BasicCompositor on the compositor thread for OMTC drawing
   * after each composition for which StartRemoteDrawing(InRegion) was called.
   */
  virtual void EndRemoteDrawing() {}
  virtual void EndRemoteDrawingInRegion(
      gfx::DrawTarget* aDrawTarget,
      const LayoutDeviceIntRegion& aInvalidRegion) {
    EndRemoteDrawing();
  }

  /**
   * Return true when it is better to defer EndRemoteDrawing().
   *
   * Called by BasicCompositor on the compositor thread for OMTC drawing
   * after each composition.
   */
  virtual bool NeedsToDeferEndRemoteDrawing() { return false; }

  /**
   * Some widgets (namely Gtk) may need clean up underlying surface
   * before painting to draw transparent objects correctly. Return
   * the transparent region where this clearing is required.
   */
  virtual LayoutDeviceIntRegion GetTransparentRegion();

  /**
   * Called when shutting down the LayerManager to clean-up any cached
   * resources.
   *
   * Always called from the compositing thread.
   */
  virtual void CleanupWindowEffects() {}

  /**
   * A hook for the widget to prepare a Compositor, during the latter's
   * initialization.
   *
   * If this method returns true, it means that the widget will be able to
   * present frames from the compoositor.
   *
   * Returning false will cause the compositor's initialization to fail, and
   * a different compositor backend will be used (if any).
   */
  virtual bool InitCompositor(layers::Compositor* aCompositor) { return true; }

  /**
   * A hook that is ran whenever composition is resumed.
   *
   * This is called from CompositorBridgeParent::ResumeComposition,
   * immediately prior to webrender being resumed.
   */
  virtual void OnResumeComposition() {}

  /**
   * Return the size of the drawable area of the widget.
   */
  virtual LayoutDeviceIntSize GetClientSize() = 0;

  /**
   * Return the internal format of the default framebuffer for this
   * widget.
   */
  virtual uint32_t GetGLFrameBufferFormat();

  /*
   * Access the underlying nsIWidget. This method will be removed when the
   * compositor no longer depends on nsIWidget on any platform.
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
  virtual uintptr_t GetWidgetKey() { return 0; }

  /**
   * Create a backbuffer for the software compositor.
   */
  virtual already_AddRefed<gfx::DrawTarget> GetBackBufferDrawTarget(
      gfx::DrawTarget* aScreenTarget, const gfx::IntRect& aRect,
      bool* aOutIsCleared);

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
   * Get the compositor options for the compositor associated with this
   * CompositorWidget.
   */
  const layers::CompositorOptions& GetCompositorOptions() { return mOptions; }

  /**
   * Return true if the window is hidden and should not be composited.
   */
  virtual bool IsHidden() const { return false; }

  /**
   * This is only used by out-of-process compositors.
   */
  virtual RefPtr<VsyncObserver> GetVsyncObserver() const;

  virtual WinCompositorWidget* AsWindows() { return nullptr; }
  virtual GtkCompositorWidget* AsGTK() { return nullptr; }
  virtual AndroidCompositorWidget* AsAndroid() { return nullptr; }

  /**
   * Return the platform-specific delegate for the widget, if any.
   */
  virtual CompositorWidgetDelegate* AsDelegate() { return nullptr; }

 protected:
  explicit CompositorWidget(const layers::CompositorOptions& aOptions);
  virtual ~CompositorWidget();

  // Back buffer of BasicCompositor
  RefPtr<gfx::DrawTarget> mLastBackBuffer;

  layers::CompositorOptions mOptions;
};

}  // namespace widget
}  // namespace mozilla

#endif
