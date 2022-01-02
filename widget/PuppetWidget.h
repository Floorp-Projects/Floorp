/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This "puppet widget" isn't really a platform widget.  It's intended
 * to be used in widgetless rendering contexts, such as sandboxed
 * content processes.  If any "real" widgetry is needed, the request
 * is forwarded to and/or data received from elsewhere.
 */

#ifndef mozilla_widget_PuppetWidget_h__
#define mozilla_widget_PuppetWidget_h__

#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "nsBaseWidget.h"
#include "nsCOMArray.h"
#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/ContentCache.h"
#include "mozilla/EventForwards.h"
#include "mozilla/TextEventDispatcherListener.h"
#include "mozilla/layers/MemoryPressureObserver.h"

namespace mozilla {

namespace dom {
class BrowserChild;
}  // namespace dom

namespace layers {
class WebRenderLayerManager;
}  // namespace layers

namespace widget {

struct AutoCacheNativeKeyCommands;

class PuppetWidget : public nsBaseWidget,
                     public TextEventDispatcherListener,
                     public layers::MemoryPressureListener {
  typedef mozilla::CSSRect CSSRect;
  typedef mozilla::dom::BrowserChild BrowserChild;
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::layers::WebRenderLayerManager WebRenderLayerManager;

  // Avoiding to make compiler confused between mozilla::widget and nsIWidget.
  typedef mozilla::widget::TextEventDispatcher TextEventDispatcher;
  typedef mozilla::widget::TextEventDispatcherListener
      TextEventDispatcherListener;

  typedef nsBaseWidget Base;

  // The width and height of the "widget" are clamped to this.
  static const size_t kMaxDimension;

 public:
  explicit PuppetWidget(BrowserChild* aBrowserChild);

 protected:
  virtual ~PuppetWidget();

 public:
  NS_DECL_ISUPPORTS_INHERITED

  // PuppetWidget creation is infallible, hence InfallibleCreate(), which
  // Create() calls.
  using nsBaseWidget::Create;  // for Create signature not overridden here
  virtual nsresult Create(nsIWidget* aParent, nsNativeWidget aNativeParent,
                          const LayoutDeviceIntRect& aRect,
                          nsWidgetInitData* aInitData = nullptr) override;
  void InfallibleCreate(nsIWidget* aParent, nsNativeWidget aNativeParent,
                        const LayoutDeviceIntRect& aRect,
                        nsWidgetInitData* aInitData = nullptr);

  void InitIMEState();

  virtual already_AddRefed<nsIWidget> CreateChild(
      const LayoutDeviceIntRect& aRect, nsWidgetInitData* aInitData = nullptr,
      bool aForceUseIWidgetParent = false) override;

  virtual void Destroy() override;

  virtual void Show(bool aState) override;

  virtual bool IsVisible() const override { return mVisible; }

  virtual void ConstrainPosition(bool /*ignored aAllowSlop*/, int32_t* aX,
                                 int32_t* aY) override {
    *aX = kMaxDimension;
    *aY = kMaxDimension;
  }

  // Widget position is controlled by the parent process via BrowserChild.
  virtual void Move(double aX, double aY) override {}

  virtual void Resize(double aWidth, double aHeight, bool aRepaint) override;
  virtual void Resize(double aX, double aY, double aWidth, double aHeight,
                      bool aRepaint) override {
    if (!mBounds.IsEqualXY(aX, aY)) {
      NotifyWindowMoved(aX, aY);
    }
    mBounds.MoveTo(aX, aY);
    return Resize(aWidth, aHeight, aRepaint);
  }

  // XXX/cjones: copying gtk behavior here; unclear what disabling a
  // widget is supposed to entail
  virtual void Enable(bool aState) override { mEnabled = aState; }
  virtual bool IsEnabled() const override { return mEnabled; }

  virtual void SetFocus(Raise, mozilla::dom::CallerType aCallerType) override;

  virtual void Invalidate(const LayoutDeviceIntRect& aRect) override;

  // PuppetWidgets don't have native data, as they're purely nonnative.
  virtual void* GetNativeData(uint32_t aDataType) override { return nullptr; }

  // PuppetWidgets don't have any concept of titles.
  virtual nsresult SetTitle(const nsAString& aTitle) override {
    return NS_ERROR_UNEXPECTED;
  }

  virtual mozilla::LayoutDeviceToLayoutDeviceMatrix4x4
  WidgetToTopLevelWidgetTransform() override;

  virtual LayoutDeviceIntPoint WidgetToScreenOffset() override;

  virtual LayoutDeviceIntPoint TopLevelWidgetToScreenOffset() override {
    return GetWindowPosition();
  }

  int32_t RoundsWidgetCoordinatesTo() override;

  void InitEvent(WidgetGUIEvent& aEvent,
                 LayoutDeviceIntPoint* aPoint = nullptr);

  virtual nsresult DispatchEvent(WidgetGUIEvent* aEvent,
                                 nsEventStatus& aStatus) override;
  ContentAndAPZEventStatus DispatchInputEvent(
      WidgetInputEvent* aEvent) override;
  void SetConfirmedTargetAPZC(
      uint64_t aInputBlockId,
      const nsTArray<ScrollableLayerGuid>& aTargets) const override;
  void UpdateZoomConstraints(
      const uint32_t& aPresShellId, const ScrollableLayerGuid::ViewID& aViewId,
      const mozilla::Maybe<ZoomConstraints>& aConstraints) override;
  bool AsyncPanZoomEnabled() const override;

  MOZ_CAN_RUN_SCRIPT virtual bool GetEditCommands(
      NativeKeyBindingsType aType, const mozilla::WidgetKeyboardEvent& aEvent,
      nsTArray<mozilla::CommandInt>& aCommands) override;

  friend struct AutoCacheNativeKeyCommands;

  //
  // nsBaseWidget methods we override
  //

  // Documents loaded in child processes are always subdocuments of
  // other docs in an ancestor process.  To ensure that the
  // backgrounds of those documents are painted like those of
  // same-process subdocuments, we force the widget here to be
  // transparent, which in turn will cause layout to use a transparent
  // backstop background color.
  virtual nsTransparencyMode GetTransparencyMode() override {
    return eTransparencyTransparent;
  }

  virtual WindowRenderer* GetWindowRenderer() override;

  // This is used for creating remote layer managers and for re-creating
  // them after a compositor reset. The lambda aInitializeFunc is used to
  // perform any caller-required initialization for the newly created layer
  // manager; in the event of a failure, return false and it will destroy the
  // new layer manager without changing the state of the widget.
  bool CreateRemoteLayerManager(
      const std::function<bool(WebRenderLayerManager*)>& aInitializeFunc);

  bool HasWindowRenderer() { return !!mWindowRenderer; }

  virtual void SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction) override;
  virtual InputContext GetInputContext() override;
  virtual NativeIMEContext GetNativeIMEContext() override;
  TextEventDispatcherListener* GetNativeTextEventDispatcherListener() override {
    return mNativeTextEventDispatcherListener
               ? mNativeTextEventDispatcherListener.get()
               : this;
  }
  void SetNativeTextEventDispatcherListener(
      TextEventDispatcherListener* aListener) {
    mNativeTextEventDispatcherListener = aListener;
  }

  virtual void SetCursor(const Cursor&) override;

  // Gets the DPI of the screen corresponding to this widget.
  // Contacts the parent process which gets the DPI from the
  // proper widget there. TODO: Handle DPI changes that happen
  // later on.
  virtual float GetDPI() override;
  virtual double GetDefaultScaleInternal() override;

  virtual bool NeedsPaint() override;

  // Paint the widget immediately if any paints are queued up.
  void PaintNowIfNeeded();

  virtual BrowserChild* GetOwningBrowserChild() override {
    return mBrowserChild;
  }

  void UpdateBackingScaleCache(float aDpi, int32_t aRounding, double aScale) {
    mDPI = aDpi;
    mRounding = aRounding;
    mDefaultScale = aScale;
  }

  // safe area insets support
  virtual ScreenIntMargin GetSafeAreaInsets() const override;
  void UpdateSafeAreaInsets(const ScreenIntMargin& aSafeAreaInsets);

  // Get the offset to the chrome of the window that this tab belongs to.
  //
  // NOTE: In OOP iframes this value is zero. You should use
  // WidgetToTopLevelWidgetTransform instead which is already including the
  // chrome offset.
  LayoutDeviceIntPoint GetChromeOffset();

  // Get the screen position of the application window.
  LayoutDeviceIntPoint GetWindowPosition();

  virtual LayoutDeviceIntRect GetScreenBounds() override;

  virtual nsresult SynthesizeNativeKeyEvent(
      int32_t aNativeKeyboardLayout, int32_t aNativeKeyCode,
      uint32_t aModifierFlags, const nsAString& aCharacters,
      const nsAString& aUnmodifiedCharacters, nsIObserver* aObserver) override;
  virtual nsresult SynthesizeNativeMouseEvent(
      LayoutDeviceIntPoint aPoint, NativeMouseMessage aNativeMessage,
      MouseButton aButton, nsIWidget::Modifiers aModifierFlags,
      nsIObserver* aObserver) override;
  virtual nsresult SynthesizeNativeMouseMove(LayoutDeviceIntPoint aPoint,
                                             nsIObserver* aObserver) override;
  virtual nsresult SynthesizeNativeMouseScrollEvent(
      LayoutDeviceIntPoint aPoint, uint32_t aNativeMessage, double aDeltaX,
      double aDeltaY, double aDeltaZ, uint32_t aModifierFlags,
      uint32_t aAdditionalFlags, nsIObserver* aObserver) override;
  virtual nsresult SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                              TouchPointerState aPointerState,
                                              LayoutDeviceIntPoint aPoint,
                                              double aPointerPressure,
                                              uint32_t aPointerOrientation,
                                              nsIObserver* aObserver) override;
  virtual nsresult SynthesizeNativeTouchPadPinch(
      TouchpadGesturePhase aEventPhase, float aScale,
      LayoutDeviceIntPoint aPoint, int32_t aModifierFlags) override;
  virtual nsresult SynthesizeNativeTouchTap(LayoutDeviceIntPoint aPoint,
                                            bool aLongTap,
                                            nsIObserver* aObserver) override;
  virtual nsresult ClearNativeTouchSequence(nsIObserver* aObserver) override;
  virtual uint32_t GetMaxTouchPoints() const override;
  virtual nsresult SynthesizeNativePenInput(uint32_t aPointerId,
                                            TouchPointerState aPointerState,
                                            LayoutDeviceIntPoint aPoint,
                                            double aPressure,
                                            uint32_t aRotation, int32_t aTiltX,
                                            int32_t aTiltY, int32_t aButton,
                                            nsIObserver* aObserver) override;

  virtual nsresult SynthesizeNativeTouchpadDoubleTap(
      LayoutDeviceIntPoint aPoint, uint32_t aModifierFlags) override;

  virtual nsresult SynthesizeNativeTouchpadPan(TouchpadGesturePhase aEventPhase,
                                               LayoutDeviceIntPoint aPoint,
                                               double aDeltaX, double aDeltaY,
                                               int32_t aModifierFlags) override;

  virtual void LockNativePointer() override;
  virtual void UnlockNativePointer() override;

  virtual void StartAsyncScrollbarDrag(
      const AsyncDragMetrics& aDragMetrics) override;

  virtual void ZoomToRect(const uint32_t& aPresShellId,
                          const ScrollableLayerGuid::ViewID& aViewId,
                          const CSSRect& aRect,
                          const uint32_t& aFlags) override;

  virtual bool HasPendingInputEvent() override;

  virtual void LookUpDictionary(
      const nsAString& aText,
      const nsTArray<mozilla::FontRange>& aFontRangeArray,
      const bool aIsVertical, const LayoutDeviceIntPoint& aPoint) override;

  nsresult SetSystemFont(const nsCString& aFontName) override;
  nsresult GetSystemFont(nsCString& aFontName) override;

  // TextEventDispatcherListener
  using nsBaseWidget::NotifyIME;
  NS_IMETHOD NotifyIME(TextEventDispatcher* aTextEventDispatcher,
                       const IMENotification& aNotification) override;
  NS_IMETHOD_(IMENotificationRequests) GetIMENotificationRequests() override;
  NS_IMETHOD_(void)
  OnRemovedFrom(TextEventDispatcher* aTextEventDispatcher) override;
  NS_IMETHOD_(void)
  WillDispatchKeyboardEvent(TextEventDispatcher* aTextEventDispatcher,
                            WidgetKeyboardEvent& aKeyboardEvent,
                            uint32_t aIndexOfKeypress, void* aData) override;

  virtual void OnMemoryPressure(layers::MemoryPressureReason aWhy) override;

 private:
  void Paint();

  void SetChild(PuppetWidget* aChild);

  nsresult RequestIMEToCommitComposition(bool aCancel);
  nsresult NotifyIMEOfFocusChange(const IMENotification& aIMENotification);
  nsresult NotifyIMEOfSelectionChange(const IMENotification& aIMENotification);
  nsresult NotifyIMEOfCompositionUpdate(
      const IMENotification& aIMENotification);
  nsresult NotifyIMEOfTextChange(const IMENotification& aIMENotification);
  nsresult NotifyIMEOfMouseButtonEvent(const IMENotification& aIMENotification);
  nsresult NotifyIMEOfPositionChange(const IMENotification& aIMENotification);

  bool CacheEditorRect();
  bool CacheCompositionRects(uint32_t& aStartOffset,
                             nsTArray<LayoutDeviceIntRect>& aRectArray,
                             uint32_t& aTargetCauseOffset);
  bool GetCaretRect(LayoutDeviceIntRect& aCaretRect, uint32_t aCaretOffset);
  uint32_t GetCaretOffset();

  nsIWidgetListener* GetCurrentWidgetListener();

  // When this widget caches input context and currently managed by
  // IMEStateManager, the cache is valid.
  bool HaveValidInputContextCache() const;

  class WidgetPaintTask : public Runnable {
   public:
    NS_DECL_NSIRUNNABLE
    explicit WidgetPaintTask(PuppetWidget* widget)
        : Runnable("PuppetWidget::WidgetPaintTask"), mWidget(widget) {}
    void Revoke() { mWidget = nullptr; }

   private:
    PuppetWidget* mWidget;
  };

  nsRefreshDriver* GetTopLevelRefreshDriver() const;

  // BrowserChild normally holds a strong reference to this PuppetWidget
  // or its root ancestor, but each PuppetWidget also needs a
  // reference back to BrowserChild (e.g. to delegate nsIWidget IME calls
  // to chrome) So we hold a weak reference to BrowserChild here.  Since
  // it's possible for BrowserChild to outlive the PuppetWidget, we clear
  // this weak reference in Destroy()
  BrowserChild* mBrowserChild;
  // The "widget" to which we delegate events if we don't have an
  // event handler.
  RefPtr<PuppetWidget> mChild;
  nsRevocableEventPtr<WidgetPaintTask> mWidgetPaintTask;
  RefPtr<layers::MemoryPressureObserver> mMemoryPressureObserver;
  // XXX/cjones: keeping this around until we teach LayerManager to do
  // retained-content-only transactions
  RefPtr<DrawTarget> mDrawTarget;
  // IME
  IMENotificationRequests mIMENotificationRequestsOfParent;
  InputContext mInputContext;
  // mNativeIMEContext is initialized when this dispatches every composition
  // event both from parent process's widget and TextEventDispatcher in same
  // process.  If it hasn't been started composition yet, this isn't necessary
  // for XP code since there is no TextComposition instance which is caused by
  // the PuppetWidget instance.
  NativeIMEContext mNativeIMEContext;
  ContentCacheInChild mContentCache;

  // The DPI of the screen corresponding to this widget
  float mDPI;
  int32_t mRounding;
  double mDefaultScale;

  ScreenIntMargin mSafeAreaInsets;

  RefPtr<TextEventDispatcherListener> mNativeTextEventDispatcherListener;

 protected:
  bool mEnabled;
  bool mVisible;

 private:
  bool mNeedIMEStateInit;
  // When remote process requests to commit/cancel a composition, the
  // composition may have already been committed in the main process.  In such
  // case, this will receive remaining composition events for the old
  // composition even after requesting to commit/cancel the old composition
  // but the TextComposition for the old composition has already been
  // destroyed. So, until this meets new eCompositionStart, following
  // composition events should be ignored if this is set to true.
  bool mIgnoreCompositionEvents;
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_PuppetWidget_h__
