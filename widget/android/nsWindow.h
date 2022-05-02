/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: set sw=2 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSWINDOW_H_
#define NSWINDOW_H_

#include "nsBaseWidget.h"
#include "gfxPoint.h"
#include "nsIUserIdleServiceInternal.h"
#include "nsTArray.h"
#include "EventDispatcher.h"
#include "mozilla/EventForwards.h"
#include "mozilla/java/GeckoSessionNatives.h"
#include "mozilla/java/WebResponseWrappers.h"
#include "mozilla/MozPromise.h"
#include "mozilla/Mutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TextRange.h"
#include "mozilla/UniquePtr.h"

struct ANPEvent;

namespace mozilla {
class WidgetTouchEvent;

namespace layers {
class CompositorBridgeChild;
class LayerManager;
class APZCTreeManager;
class UiCompositorControllerChild;
}  // namespace layers

namespace widget {
class AndroidView;
class GeckoEditableSupport;
class GeckoViewSupport;
class LayerViewSupport;
class NPZCSupport;
class PlatformCompositorWidgetDelegate;
}  // namespace widget

namespace ipc {
class Shmem;
}  // namespace ipc

namespace a11y {
class SessionAccessibility;
}  // namespace a11y
}  // namespace mozilla

class nsWindow final : public nsBaseWidget {
 private:
  virtual ~nsWindow();

 public:
  using nsBaseWidget::GetWindowRenderer;

  nsWindow();

  NS_INLINE_DECL_REFCOUNTING_INHERITED(nsWindow, nsBaseWidget)

  static void InitNatives();
  void OnGeckoViewReady();
  RefPtr<mozilla::MozPromise<bool, bool, false>> OnLoadRequest(
      nsIURI* aUri, int32_t aWindowType, int32_t aFlags,
      nsIPrincipal* aTriggeringPrincipal, bool aHasUserGesture,
      bool aIsTopLevel);

 private:
  // Unique ID given to each widget, used to map Surfaces to widgets
  // in the CompositorSurfaceManager.
  int32_t mWidgetId;

 private:
  RefPtr<mozilla::widget::AndroidView> mAndroidView;

  // Object that implements native LayerView calls.
  // Owned by the Java Compositor instance.
  mozilla::jni::NativeWeakPtr<mozilla::widget::LayerViewSupport>
      mLayerViewSupport;

  // Object that implements native NativePanZoomController calls.
  // Owned by the Java NativePanZoomController instance.
  mozilla::jni::NativeWeakPtr<mozilla::widget::NPZCSupport> mNPZCSupport;

  // Object that implements native GeckoEditable calls.
  // Strong referenced by the Java instance.
  mozilla::jni::NativeWeakPtr<mozilla::widget::GeckoEditableSupport>
      mEditableSupport;
  mozilla::jni::Object::GlobalRef mEditableParent;

  // Object that implements native SessionAccessibility calls.
  // Strong referenced by the Java instance.
  mozilla::jni::NativeWeakPtr<mozilla::a11y::SessionAccessibility>
      mSessionAccessibility;

  // Object that implements native GeckoView calls and associated states.
  // nullptr for nsWindows that were not opened from GeckoView.
  mozilla::jni::NativeWeakPtr<mozilla::widget::GeckoViewSupport>
      mGeckoViewSupport;

  mozilla::Atomic<bool, mozilla::ReleaseAcquire> mContentDocumentDisplayed;

 public:
  static already_AddRefed<nsWindow> From(nsPIDOMWindowOuter* aDOMWindow);
  static already_AddRefed<nsWindow> From(nsIWidget* aWidget);

  static nsWindow* TopWindow();

  static mozilla::Modifiers GetModifiers(int32_t aMetaState);
  static mozilla::TimeStamp GetEventTimeStamp(int64_t aEventTime);

  void InitEvent(mozilla::WidgetGUIEvent& event,
                 LayoutDeviceIntPoint* aPoint = 0);

  void UpdateOverscrollVelocity(const float aX, const float aY);
  void UpdateOverscrollOffset(const float aX, const float aY);

  mozilla::widget::EventDispatcher* GetEventDispatcher() const;

  void PassExternalResponse(mozilla::java::WebResponse::Param aResponse);

  void ShowDynamicToolbar();

  void DetachNatives();

  //
  // nsIWidget
  //

  using nsBaseWidget::Create;  // for Create signature not overridden here
  [[nodiscard]] virtual nsresult Create(nsIWidget* aParent,
                                        nsNativeWidget aNativeParent,
                                        const LayoutDeviceIntRect& aRect,
                                        nsWidgetInitData* aInitData) override;
  virtual void Destroy() override;
  virtual void SetParent(nsIWidget* aNewParent) override;
  virtual nsIWidget* GetParent(void) override;
  virtual float GetDPI() override;
  virtual double GetDefaultScaleInternal() override;
  virtual void Show(bool aState) override;
  virtual bool IsVisible() const override;
  virtual void ConstrainPosition(bool aAllowSlop, int32_t* aX,
                                 int32_t* aY) override;
  virtual void Move(double aX, double aY) override;
  virtual void Resize(double aWidth, double aHeight, bool aRepaint) override;
  virtual void Resize(double aX, double aY, double aWidth, double aHeight,
                      bool aRepaint) override;
  void SetZIndex(int32_t aZIndex) override;
  virtual nsSizeMode SizeMode() override { return mSizeMode; }
  virtual void SetSizeMode(nsSizeMode aMode) override;
  virtual void Enable(bool aState) override;
  virtual bool IsEnabled() const override;
  virtual void Invalidate(const LayoutDeviceIntRect& aRect) override;
  virtual void SetFocus(Raise, mozilla::dom::CallerType aCallerType) override;
  virtual LayoutDeviceIntRect GetScreenBounds() override;
  virtual LayoutDeviceIntPoint WidgetToScreenOffset() override;
  virtual nsresult DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                                 nsEventStatus& aStatus) override;
  nsEventStatus DispatchEvent(mozilla::WidgetGUIEvent* aEvent);
  virtual nsresult MakeFullScreen(bool aFullScreen) override;
  void SetCursor(const Cursor& aDefaultCursor) override;
  void* GetNativeData(uint32_t aDataType) override;
  void SetNativeData(uint32_t aDataType, uintptr_t aVal) override;
  virtual nsresult SetTitle(const nsAString& aTitle) override { return NS_OK; }
  [[nodiscard]] virtual nsresult GetAttention(int32_t aCycleCount) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  TextEventDispatcherListener* GetNativeTextEventDispatcherListener() override;
  virtual void SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction) override;
  virtual InputContext GetInputContext() override;

  WindowRenderer* GetWindowRenderer() override;

  void NotifyCompositorSessionLost(
      mozilla::layers::CompositorSession* aSession) override;

  virtual bool NeedsPaint() override;

  virtual bool WidgetPaintsBackground() override;

  virtual uint32_t GetMaxTouchPoints() const override;

  void UpdateZoomConstraints(
      const uint32_t& aPresShellId, const ScrollableLayerGuid::ViewID& aViewId,
      const mozilla::Maybe<ZoomConstraints>& aConstraints) override;

  nsresult SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                      TouchPointerState aPointerState,
                                      LayoutDeviceIntPoint aPoint,
                                      double aPointerPressure,
                                      uint32_t aPointerOrientation,
                                      nsIObserver* aObserver) override;
  nsresult SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                      NativeMouseMessage aNativeMessage,
                                      mozilla::MouseButton aButton,
                                      nsIWidget::Modifiers aModifierFlags,
                                      nsIObserver* aObserver) override;
  nsresult SynthesizeNativeMouseMove(LayoutDeviceIntPoint aPoint,
                                     nsIObserver* aObserver) override;

  void SetCompositorWidgetDelegate(CompositorWidgetDelegate* delegate) override;

  virtual void GetCompositorWidgetInitData(
      mozilla::widget::CompositorWidgetInitData* aInitData) override;

  mozilla::layers::CompositorBridgeChild* GetCompositorBridgeChild() const;

  void SetContentDocumentDisplayed(bool aDisplayed);
  bool IsContentDocumentDisplayed();

  // Call this function when the users activity is the direct cause of an
  // event (like a keypress or mouse click).
  void UserActivity();

  mozilla::jni::Object::Ref& GetEditableParent() { return mEditableParent; }

  RefPtr<mozilla::a11y::SessionAccessibility> GetSessionAccessibility();

  void RecvToolbarAnimatorMessageFromCompositor(int32_t aMessage) override;
  void UpdateRootFrameMetrics(const ScreenPoint& aScrollOffset,
                              const CSSToScreenScale& aZoom) override;
  void RecvScreenPixels(mozilla::ipc::Shmem&& aMem, const ScreenIntSize& aSize,
                        bool aNeedsYFlip) override;
  void UpdateDynamicToolbarMaxHeight(mozilla::ScreenIntCoord aHeight) override;
  mozilla::ScreenIntCoord GetDynamicToolbarMaxHeight() const override {
    return mDynamicToolbarMaxHeight;
  }

  void UpdateDynamicToolbarOffset(mozilla::ScreenIntCoord aOffset);

  virtual mozilla::ScreenIntMargin GetSafeAreaInsets() const override;
  void UpdateSafeAreaInsets(const mozilla::ScreenIntMargin& aSafeAreaInsets);

 protected:
  void BringToFront();
  nsWindow* FindTopLevel();
  bool IsTopLevel();

  void ConfigureAPZControllerThread() override;
  void DispatchHitTest(const mozilla::WidgetTouchEvent& aEvent);

  already_AddRefed<GeckoContentController> CreateRootContentController()
      override;

  bool mIsVisible;
  nsTArray<nsWindow*> mChildren;
  nsWindow* mParent;

  nsCOMPtr<nsIUserIdleServiceInternal> mIdleService;
  mozilla::ScreenIntCoord mDynamicToolbarMaxHeight;
  mozilla::ScreenIntMargin mSafeAreaInsets;

  nsSizeMode mSizeMode;
  bool mIsFullScreen;

  bool UseExternalCompositingSurface() const override { return true; }

  static void DumpWindows();
  static void DumpWindows(const nsTArray<nsWindow*>& wins, int indent = 0);
  static void LogWindow(nsWindow* win, int index, int indent);

 private:
  void CreateLayerManager();
  void RedrawAll();

  void OnSizeChanged(const mozilla::gfx::IntSize& aSize);

  mozilla::layers::LayersId GetRootLayerId() const;
  RefPtr<mozilla::layers::UiCompositorControllerChild>
  GetUiCompositorControllerChild();

  mozilla::widget::PlatformCompositorWidgetDelegate* mCompositorWidgetDelegate;

  friend class mozilla::widget::GeckoViewSupport;
  friend class mozilla::widget::LayerViewSupport;
  friend class mozilla::widget::NPZCSupport;
};

#endif /* NSWINDOW_H_ */
