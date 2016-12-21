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
#include "nsBaseScreen.h"
#include "nsBaseWidget.h"
#include "nsCOMArray.h"
#include "nsIKeyEventInPluginCallback.h"
#include "nsIScreenManager.h"
#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/ContentCache.h"
#include "mozilla/EventForwards.h"

namespace mozilla {

namespace dom {
class TabChild;
} // namespace dom

namespace widget {

struct AutoCacheNativeKeyCommands;

class PuppetWidget : public nsBaseWidget
{
  typedef mozilla::dom::TabChild TabChild;
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef nsBaseWidget Base;
  typedef mozilla::CSSRect CSSRect;

  // The width and height of the "widget" are clamped to this.
  static const size_t kMaxDimension;

public:
  explicit PuppetWidget(TabChild* aTabChild);

protected:
  virtual ~PuppetWidget();

public:
  NS_DECL_ISUPPORTS_INHERITED

  // PuppetWidget creation is infallible, hence InfallibleCreate(), which
  // Create() calls.
  using nsBaseWidget::Create; // for Create signature not overridden here
  virtual nsresult Create(nsIWidget* aParent,
                          nsNativeWidget aNativeParent,
                          const LayoutDeviceIntRect& aRect,
                          nsWidgetInitData* aInitData = nullptr)
                          override;
  void InfallibleCreate(nsIWidget* aParent,
                        nsNativeWidget aNativeParent,
                        const LayoutDeviceIntRect& aRect,
                        nsWidgetInitData* aInitData = nullptr);

  void InitIMEState();

  virtual already_AddRefed<nsIWidget>
  CreateChild(const LayoutDeviceIntRect& aRect,
              nsWidgetInitData* aInitData = nullptr,
              bool aForceUseIWidgetParent = false) override;

  virtual void Destroy() override;

  virtual void Show(bool aState) override;

  virtual bool IsVisible() const override
  { return mVisible; }

  virtual void ConstrainPosition(bool     /*ignored aAllowSlop*/,
                                 int32_t* aX,
                                 int32_t* aY) override
  { *aX = kMaxDimension; *aY = kMaxDimension; }

  // Widget position is controlled by the parent process via TabChild.
  virtual void Move(double aX, double aY) override {}

  virtual void Resize(double aWidth,
                      double aHeight,
                      bool   aRepaint) override;
  virtual void Resize(double aX,
                      double aY,
                      double aWidth,
                      double aHeight,
                      bool   aRepaint) override
  {
    if (mBounds.x != aX || mBounds.y != aY) {
      NotifyWindowMoved(aX, aY);
    }
    mBounds.x = aX;
    mBounds.y = aY;
    return Resize(aWidth, aHeight, aRepaint);
  }

  // XXX/cjones: copying gtk behavior here; unclear what disabling a
  // widget is supposed to entail
  virtual void Enable(bool aState) override
  { mEnabled = aState; }
  virtual bool IsEnabled() const override
  { return mEnabled; }

  virtual nsresult SetFocus(bool aRaise = false) override;

  virtual nsresult ConfigureChildren(const nsTArray<Configuration>& aConfigurations) override;

  virtual void Invalidate(const LayoutDeviceIntRect& aRect) override;

  // PuppetWidgets don't have native data, as they're purely nonnative.
  virtual void* GetNativeData(uint32_t aDataType) override;
#if defined(XP_WIN)
  void SetNativeData(uint32_t aDataType, uintptr_t aVal) override;
#endif

  // PuppetWidgets don't have any concept of titles.
  virtual nsresult SetTitle(const nsAString& aTitle) override
  { return NS_ERROR_UNEXPECTED; }

  virtual LayoutDeviceIntPoint WidgetToScreenOffset() override
  { return LayoutDeviceIntPoint::FromUnknownPoint(GetWindowPosition() + GetChromeDimensions()); }

  int32_t RoundsWidgetCoordinatesTo() override;

  void InitEvent(WidgetGUIEvent& aEvent,
                 LayoutDeviceIntPoint* aPoint = nullptr);

  virtual nsresult DispatchEvent(WidgetGUIEvent* aEvent,
                                 nsEventStatus& aStatus) override;
  nsEventStatus DispatchInputEvent(WidgetInputEvent* aEvent) override;
  void SetConfirmedTargetAPZC(uint64_t aInputBlockId,
                              const nsTArray<ScrollableLayerGuid>& aTargets) const override;
  void UpdateZoomConstraints(const uint32_t& aPresShellId,
                             const FrameMetrics::ViewID& aViewId,
                             const mozilla::Maybe<ZoomConstraints>& aConstraints) override;
  bool AsyncPanZoomEnabled() const override;

  virtual bool
  ExecuteNativeKeyBinding(NativeKeyBindingsType aType,
                          const mozilla::WidgetKeyboardEvent& aEvent,
                          DoCommandCallback aCallback,
                          void* aCallbackData) override;

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
  virtual nsTransparencyMode GetTransparencyMode() override
  { return eTransparencyTransparent; }

  virtual LayerManager*
  GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                  LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                  LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT) override;

  // This is used after a compositor reset.
  LayerManager* RecreateLayerManager(PLayerTransactionChild* aShadowManager);

  virtual void SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction) override;
  virtual InputContext GetInputContext() override;
  virtual NativeIMEContext GetNativeIMEContext() override;
  virtual nsIMEUpdatePreference GetIMEUpdatePreference() override;

  virtual void SetCursor(nsCursor aCursor) override;
  virtual nsresult SetCursor(imgIContainer* aCursor,
                             uint32_t aHotspotX, uint32_t aHotspotY) override;

  virtual void ClearCachedCursor() override;

  // Gets the DPI of the screen corresponding to this widget.
  // Contacts the parent process which gets the DPI from the
  // proper widget there. TODO: Handle DPI changes that happen
  // later on.
  virtual float GetDPI() override;
  virtual double GetDefaultScaleInternal() override;

  virtual bool NeedsPaint() override;

  // Paint the widget immediately if any paints are queued up.
  void PaintNowIfNeeded();

  virtual TabChild* GetOwningTabChild() override { return mTabChild; }

  void UpdateBackingScaleCache(float aDpi, int32_t aRounding, double aScale)
  {
    mDPI = aDpi;
    mRounding = aRounding;
    mDefaultScale = aScale;
  }

  nsIntSize GetScreenDimensions();

  // Get the size of the chrome of the window that this tab belongs to.
  nsIntPoint GetChromeDimensions();

  // Get the screen position of the application window.
  nsIntPoint GetWindowPosition();

  virtual LayoutDeviceIntRect GetScreenBounds() override;

  virtual MOZ_MUST_USE nsresult
  StartPluginIME(const mozilla::WidgetKeyboardEvent& aKeyboardEvent,
                 int32_t aPanelX, int32_t aPanelY,
                 nsString& aCommitted) override;

  virtual void SetPluginFocused(bool& aFocused) override;
  virtual void DefaultProcOfPluginEvent(
                 const mozilla::WidgetPluginEvent& aEvent) override;

  virtual nsresult SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                            int32_t aNativeKeyCode,
                                            uint32_t aModifierFlags,
                                            const nsAString& aCharacters,
                                            const nsAString& aUnmodifiedCharacters,
                                            nsIObserver* aObserver) override;
  virtual nsresult SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                              uint32_t aNativeMessage,
                                              uint32_t aModifierFlags,
                                              nsIObserver* aObserver) override;
  virtual nsresult SynthesizeNativeMouseMove(LayoutDeviceIntPoint aPoint,
                                             nsIObserver* aObserver) override;
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
  virtual nsresult SynthesizeNativeTouchTap(LayoutDeviceIntPoint aPoint,
                                            bool aLongTap,
                                            nsIObserver* aObserver) override;
  virtual nsresult ClearNativeTouchSequence(nsIObserver* aObserver) override;
  virtual uint32_t GetMaxTouchPoints() const override;

  virtual void StartAsyncScrollbarDrag(const AsyncDragMetrics& aDragMetrics) override;

  virtual void SetCandidateWindowForPlugin(
                 const CandidateWindowPosition& aPosition) override;

  virtual void ZoomToRect(const uint32_t& aPresShellId,
                          const FrameMetrics::ViewID& aViewId,
                          const CSSRect& aRect,
                          const uint32_t& aFlags) override;

  virtual bool HasPendingInputEvent() override;

  void HandledWindowedPluginKeyEvent(const NativeEventData& aKeyEventData,
                                     bool aIsConsumed);
  virtual nsresult OnWindowedPluginKeyEvent(
                     const NativeEventData& aKeyEventData,
                     nsIKeyEventInPluginCallback* aCallback) override;

  virtual void LookUpDictionary(
                 const nsAString& aText,
                 const nsTArray<mozilla::FontRange>& aFontRangeArray,
                 const bool aIsVertical,
                 const LayoutDeviceIntPoint& aPoint) override;

protected:
  virtual nsresult NotifyIMEInternal(
                     const IMENotification& aIMENotification) override;

private:
  nsresult Paint();

  void SetChild(PuppetWidget* aChild);

  nsresult RequestIMEToCommitComposition(bool aCancel);
  nsresult NotifyIMEOfFocusChange(const IMENotification& aIMENotification);
  nsresult NotifyIMEOfSelectionChange(const IMENotification& aIMENotification);
  nsresult NotifyIMEOfCompositionUpdate(const IMENotification& aIMENotification);
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

  class PaintTask : public Runnable {
  public:
    NS_DECL_NSIRUNNABLE
    explicit PaintTask(PuppetWidget* widget) : mWidget(widget) {}
    void Revoke() { mWidget = nullptr; }
  private:
    PuppetWidget* mWidget;
  };

  class MemoryPressureObserver : public nsIObserver {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
    explicit MemoryPressureObserver(PuppetWidget* aWidget) : mWidget(aWidget) {}
    void Remove();
  private:
    virtual ~MemoryPressureObserver() {}
    PuppetWidget* mWidget;
  };
  friend class MemoryPressureObserver;

  // TabChild normally holds a strong reference to this PuppetWidget
  // or its root ancestor, but each PuppetWidget also needs a
  // reference back to TabChild (e.g. to delegate nsIWidget IME calls
  // to chrome) So we hold a weak reference to TabChild here.  Since
  // it's possible for TabChild to outlive the PuppetWidget, we clear
  // this weak reference in Destroy()
  TabChild* mTabChild;
  // The "widget" to which we delegate events if we don't have an
  // event handler.
  RefPtr<PuppetWidget> mChild;
  LayoutDeviceIntRegion mDirtyRegion;
  nsRevocableEventPtr<PaintTask> mPaintTask;
  RefPtr<MemoryPressureObserver> mMemoryPressureObserver;
  // XXX/cjones: keeping this around until we teach LayerManager to do
  // retained-content-only transactions
  RefPtr<DrawTarget> mDrawTarget;
  // IME
  nsIMEUpdatePreference mIMEPreferenceOfParent;
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

  // Precomputed answers for ExecuteNativeKeyBinding
  InfallibleTArray<mozilla::CommandInt> mSingleLineCommands;
  InfallibleTArray<mozilla::CommandInt> mMultiLineCommands;
  InfallibleTArray<mozilla::CommandInt> mRichTextCommands;

  nsCOMPtr<imgIContainer> mCustomCursor;
  uint32_t mCursorHotspotX, mCursorHotspotY;

  nsCOMArray<nsIKeyEventInPluginCallback> mKeyEventInPluginCallbacks;

protected:
  bool mEnabled;
  bool mVisible;

private:
  bool mNeedIMEStateInit;
  bool mNativeKeyCommandsValid;
};

struct AutoCacheNativeKeyCommands
{
  explicit AutoCacheNativeKeyCommands(PuppetWidget* aWidget)
    : mWidget(aWidget)
  {
    mSavedValid = mWidget->mNativeKeyCommandsValid;
    mSavedSingleLine = mWidget->mSingleLineCommands;
    mSavedMultiLine = mWidget->mMultiLineCommands;
    mSavedRichText = mWidget->mRichTextCommands;
  }

  void Cache(const InfallibleTArray<mozilla::CommandInt>& aSingleLineCommands,
             const InfallibleTArray<mozilla::CommandInt>& aMultiLineCommands,
             const InfallibleTArray<mozilla::CommandInt>& aRichTextCommands)
  {
    mWidget->mNativeKeyCommandsValid = true;
    mWidget->mSingleLineCommands = aSingleLineCommands;
    mWidget->mMultiLineCommands = aMultiLineCommands;
    mWidget->mRichTextCommands = aRichTextCommands;
  }

  void CacheNoCommands()
  {
    mWidget->mNativeKeyCommandsValid = true;
    mWidget->mSingleLineCommands.Clear();
    mWidget->mMultiLineCommands.Clear();
    mWidget->mRichTextCommands.Clear();
  }

  ~AutoCacheNativeKeyCommands()
  {
    mWidget->mNativeKeyCommandsValid = mSavedValid;
    mWidget->mSingleLineCommands = mSavedSingleLine;
    mWidget->mMultiLineCommands = mSavedMultiLine;
    mWidget->mRichTextCommands = mSavedRichText;
  }

private:
  PuppetWidget* mWidget;
  bool mSavedValid;
  InfallibleTArray<mozilla::CommandInt> mSavedSingleLine;
  InfallibleTArray<mozilla::CommandInt> mSavedMultiLine;
  InfallibleTArray<mozilla::CommandInt> mSavedRichText;
};

class PuppetScreen : public nsBaseScreen
{
public:
    explicit PuppetScreen(void* nativeScreen);
    ~PuppetScreen();

    NS_IMETHOD GetId(uint32_t* aId) override;
    NS_IMETHOD GetRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight) override;
    NS_IMETHOD GetAvailRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight) override;
    NS_IMETHOD GetPixelDepth(int32_t* aPixelDepth) override;
    NS_IMETHOD GetColorDepth(int32_t* aColorDepth) override;
    NS_IMETHOD GetRotation(uint32_t* aRotation) override;
    NS_IMETHOD SetRotation(uint32_t  aRotation) override;
};

class PuppetScreenManager final : public nsIScreenManager
{
    ~PuppetScreenManager();

public:
    PuppetScreenManager();

    NS_DECL_ISUPPORTS
    NS_DECL_NSISCREENMANAGER

protected:
    nsCOMPtr<nsIScreen> mOneScreen;
};

} // namespace widget
} // namespace mozilla

#endif  // mozilla_widget_PuppetWidget_h__
