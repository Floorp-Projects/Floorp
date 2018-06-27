/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef HEADLESSWIDGET_H
#define HEADLESSWIDGET_H

#include "mozilla/widget/InProcessCompositorWidget.h"
#include "nsBaseWidget.h"
#include "CompositorWidget.h"
#include "mozilla/dom/WheelEventBinding.h"

// The various synthesized event values are hardcoded to avoid pulling
// in the platform specific widget code.
#if defined(MOZ_WIDGET_GTK)
#define MOZ_HEADLESS_MOUSE_MOVE 3 // GDK_MOTION_NOTIFY
#define MOZ_HEADLESS_MOUSE_DOWN 4 // GDK_BUTTON_PRESS
#define MOZ_HEADLESS_MOUSE_UP   7 // GDK_BUTTON_RELEASE
#define MOZ_HEADLESS_SCROLL_MULTIPLIER 3
#define MOZ_HEADLESS_SCROLL_DELTA_MODE mozilla::dom::WheelEvent_Binding::DOM_DELTA_LINE
#elif defined(XP_WIN)
#define MOZ_HEADLESS_MOUSE_MOVE 1 // MOUSEEVENTF_MOVE
#define MOZ_HEADLESS_MOUSE_DOWN 2 // MOUSEEVENTF_LEFTDOWN
#define MOZ_HEADLESS_MOUSE_UP   4 // MOUSEEVENTF_LEFTUP
#define MOZ_HEADLESS_SCROLL_MULTIPLIER .025 // default scroll lines (3) / WHEEL_DELTA (120)
#define MOZ_HEADLESS_SCROLL_DELTA_MODE mozilla::dom::WheelEvent_Binding::DOM_DELTA_LINE
#elif defined(XP_MACOSX)
#define MOZ_HEADLESS_MOUSE_MOVE 5 // NSMouseMoved
#define MOZ_HEADLESS_MOUSE_DOWN 1 // NSLeftMouseDown
#define MOZ_HEADLESS_MOUSE_UP   2 // NSLeftMouseUp
#define MOZ_HEADLESS_SCROLL_MULTIPLIER 1
#define MOZ_HEADLESS_SCROLL_DELTA_MODE mozilla::dom::WheelEvent_Binding::DOM_DELTA_PIXEL
#elif defined(ANDROID)
#define MOZ_HEADLESS_MOUSE_MOVE 7 // ACTION_HOVER_MOVE
#define MOZ_HEADLESS_MOUSE_DOWN 5 // ACTION_POINTER_DOWN
#define MOZ_HEADLESS_MOUSE_UP   6 // ACTION_POINTER_UP
#define MOZ_HEADLESS_SCROLL_MULTIPLIER 1
#define MOZ_HEADLESS_SCROLL_DELTA_MODE mozilla::dom::WheelEvent_Binding::DOM_DELTA_LINE
#else
#define MOZ_HEADLESS_MOUSE_MOVE -1
#define MOZ_HEADLESS_MOUSE_DOWN -1
#define MOZ_HEADLESS_MOUSE_UP   -1
#define MOZ_HEADLESS_SCROLL_MULTIPLIER -1
#define MOZ_HEADLESS_SCROLL_DELTA_MODE -1
#endif

namespace mozilla {
namespace widget {

class HeadlessWidget : public nsBaseWidget
{
public:
  HeadlessWidget();

  NS_INLINE_DECL_REFCOUNTING_INHERITED(HeadlessWidget, nsBaseWidget)

  void* GetNativeData(uint32_t aDataType) override
  {
    // Headless widgets have no native data.
    return nullptr;
  }

  virtual nsresult Create(nsIWidget* aParent,
                          nsNativeWidget aNativeParent,
                          const LayoutDeviceIntRect& aRect,
                          nsWidgetInitData* aInitData = nullptr) override;
  using nsBaseWidget::Create; // for Create signature not overridden here
  virtual already_AddRefed<nsIWidget> CreateChild(const LayoutDeviceIntRect& aRect,
                                                  nsWidgetInitData* aInitData = nullptr,
                                                  bool aForceUseIWidgetParent = false) override;

  virtual nsIWidget* GetTopLevelWidget() override;

  virtual void GetCompositorWidgetInitData(mozilla::widget::CompositorWidgetInitData* aInitData) override;

  virtual void Destroy() override;
  virtual void Show(bool aState) override;
  virtual bool IsVisible() const override;
  virtual void Move(double aX, double aY) override;
  virtual void Resize(double aWidth,
                      double aHeight,
                      bool   aRepaint) override;
  virtual void Resize(double aX,
                      double aY,
                      double aWidth,
                      double aHeight,
                      bool   aRepaint) override;
  virtual void SetSizeMode(nsSizeMode aMode) override;
  virtual nsresult MakeFullScreen(bool aFullScreen,
                                  nsIScreen* aTargetScreen = nullptr) override;
  virtual void Enable(bool aState) override;
  virtual bool IsEnabled() const override;
  virtual nsresult SetFocus(bool aRaise) override;
  virtual nsresult ConfigureChildren(const nsTArray<Configuration>& aConfigurations) override
  {
    MOZ_ASSERT_UNREACHABLE("Headless widgets do not support configuring children.");
    return NS_ERROR_FAILURE;
  }
  virtual void Invalidate(const LayoutDeviceIntRect& aRect) override
  {
    // TODO: see if we need to do anything here.
  }
  virtual nsresult SetTitle(const nsAString& title) override {
    // Headless widgets have no title, so just ignore it.
    return NS_OK;
  }
  virtual nsresult SetNonClientMargins(LayoutDeviceIntMargin &margins) override {
    // Headless widgets have no chrome margins, so just ignore the call.
    return NS_OK;
  }
  virtual LayoutDeviceIntPoint WidgetToScreenOffset() override;
  virtual void SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction) override
  {
    mInputContext = aContext;
  }
  virtual InputContext GetInputContext() override
  {
    return mInputContext;
  }

  virtual LayerManager*
  GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                  LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                  LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT) override;

  void SetCompositorWidgetDelegate(CompositorWidgetDelegate* delegate) override;

  virtual MOZ_MUST_USE nsresult AttachNativeKeyEvent(WidgetKeyboardEvent& aEvent) override;
  virtual void GetEditCommands(
                 NativeKeyBindingsType aType,
                 const WidgetKeyboardEvent& aEvent,
                 nsTArray<CommandInt>& aCommands) override;

  virtual nsresult DispatchEvent(WidgetGUIEvent* aEvent,
                                 nsEventStatus& aStatus) override;

  virtual nsresult SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                              uint32_t aNativeMessage,
                                              uint32_t aModifierFlags,
                                              nsIObserver* aObserver) override;
  virtual nsresult SynthesizeNativeMouseMove(LayoutDeviceIntPoint aPoint,
                                             nsIObserver* aObserver) override
                   { return SynthesizeNativeMouseEvent(aPoint, MOZ_HEADLESS_MOUSE_MOVE, 0, aObserver); };

  virtual nsresult SynthesizeNativeMouseScrollEvent(LayoutDeviceIntPoint aPoint,
                                                    uint32_t aNativeMessage,
                                                    double aDeltaX,
                                                    double aDeltaY,
                                                    double aDeltaZ,
                                                    uint32_t aModifierFlags,
                                                    uint32_t aAdditionalFlags,
                                                    nsIObserver* aObserver) override;

  virtual nsresult SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                              TouchPointerState aPointerState,
                                              LayoutDeviceIntPoint aPoint,
                                              double aPointerPressure,
                                              uint32_t aPointerOrientation,
                                              nsIObserver* aObserver) override;

private:
  ~HeadlessWidget();
  bool mEnabled;
  bool mVisible;
  bool mDestroyed;
  nsIWidget* mTopLevel;
  HeadlessCompositorWidget* mCompositorWidget;
  // The size mode before entering fullscreen mode.
  nsSizeMode mLastSizeMode;
  // The last size mode set while the window was visible.
  nsSizeMode mEffectiveSizeMode;
  InputContext mInputContext;
  mozilla::UniquePtr<mozilla::MultiTouchInput> mSynthesizedTouchInput;
  // In headless there is no window manager to track window bounds
  // across size mode changes, so we must track it to emulate.
  LayoutDeviceIntRect mRestoreBounds;
  void ApplySizeModeSideEffects();
  // Similarly, we must track the active window ourselves in order
  // to dispatch (de)activation events properly.
  void RaiseWindow();
  // The top level widgets are tracked for window ordering. They are
  // stored in order of activation where the last element is always the
  // currently active widget.
  static StaticAutoPtr<nsTArray<HeadlessWidget*>> sActiveWindows;
  // Get the most recently activated widget or null if there are none.
  static already_AddRefed<HeadlessWidget>GetActiveWindow();
};

} // namespace widget
} // namespace mozilla

#endif
