/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsBaseWidget_h__
#define nsBaseWidget_h__

#include "mozilla/EventForwards.h"
#include "mozilla/WidgetUtils.h"
#include "nsRect.h"
#include "nsIWidget.h"
#include "nsWidgetsCID.h"
#include "nsIFile.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIRollupListener.h"
#include "nsIObserver.h"
#include "nsIWidgetListener.h"
#include "nsPIDOMWindow.h"
#include "nsWeakReference.h"
#include <algorithm>
class nsIContent;
class nsAutoRollup;
class gfxContext;

namespace mozilla {
#ifdef ACCESSIBILITY
namespace a11y {
class Accessible;
}
#endif

namespace layers {
class BasicLayerManager;
class CompositorChild;
class CompositorParent;
class APZCTreeManager;
class GeckoContentController;
class APZEventState;
struct ScrollableLayerGuid;
struct SetTargetAPZCCallback;
}

class CompositorVsyncDispatcher;
}

namespace base {
class Thread;
}

// Windows specific constant indicating the maximum number of touch points the
// inject api will allow. This also sets the maximum numerical value for touch
// ids we can use when injecting touch points on Windows.
#define TOUCH_INJECT_MAX_POINTS 256

class nsBaseWidget;

class WidgetShutdownObserver MOZ_FINAL : public nsIObserver
{
  ~WidgetShutdownObserver() {}

public:
  explicit WidgetShutdownObserver(nsBaseWidget* aWidget)
    : mWidget(aWidget)
  { }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsBaseWidget *mWidget;
};

/**
 * Common widget implementation used as base class for native
 * or crossplatform implementations of Widgets. 
 * All cross-platform behavior that all widgets need to implement 
 * should be placed in this class. 
 * (Note: widget implementations are not required to use this
 * class, but it gives them a head start.)
 */

class nsBaseWidget : public nsIWidget, public nsSupportsWeakReference
{
  friend class nsAutoRollup;

protected:
  typedef base::Thread Thread;
  typedef mozilla::layers::BasicLayerManager BasicLayerManager;
  typedef mozilla::layers::BufferMode BufferMode;
  typedef mozilla::layers::CompositorChild CompositorChild;
  typedef mozilla::layers::CompositorParent CompositorParent;
  typedef mozilla::layers::APZCTreeManager APZCTreeManager;
  typedef mozilla::layers::GeckoContentController GeckoContentController;
  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;
  typedef mozilla::layers::APZEventState APZEventState;
  typedef mozilla::layers::SetTargetAPZCCallback SetTargetAPZCCallback;
  typedef mozilla::ScreenRotation ScreenRotation;

  virtual ~nsBaseWidget();

public:
  nsBaseWidget();

  NS_DECL_ISUPPORTS

  // nsIWidget interface
  NS_IMETHOD              CaptureMouse(bool aCapture) MOZ_OVERRIDE;
  virtual nsIWidgetListener*  GetWidgetListener() MOZ_OVERRIDE;
  virtual void            SetWidgetListener(nsIWidgetListener* alistener) MOZ_OVERRIDE;
  NS_IMETHOD              Destroy() MOZ_OVERRIDE;
  NS_IMETHOD              SetParent(nsIWidget* aNewParent) MOZ_OVERRIDE;
  virtual nsIWidget*      GetParent(void) MOZ_OVERRIDE;
  virtual nsIWidget*      GetTopLevelWidget() MOZ_OVERRIDE;
  virtual nsIWidget*      GetSheetWindowParent(void) MOZ_OVERRIDE;
  virtual float           GetDPI() MOZ_OVERRIDE;
  virtual void            AddChild(nsIWidget* aChild) MOZ_OVERRIDE;
  virtual void            RemoveChild(nsIWidget* aChild) MOZ_OVERRIDE;

  void                    SetZIndex(int32_t aZIndex) MOZ_OVERRIDE;
  NS_IMETHOD              PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                      nsIWidget *aWidget, bool aActivate) MOZ_OVERRIDE;

  NS_IMETHOD              SetSizeMode(int32_t aMode) MOZ_OVERRIDE;
  virtual int32_t         SizeMode() MOZ_OVERRIDE
  {
    return mSizeMode;
  }

  virtual nsCursor        GetCursor() MOZ_OVERRIDE;
  NS_IMETHOD              SetCursor(nsCursor aCursor) MOZ_OVERRIDE;
  NS_IMETHOD              SetCursor(imgIContainer* aCursor,
                                    uint32_t aHotspotX, uint32_t aHotspotY) MOZ_OVERRIDE;
  virtual void            ClearCachedCursor() MOZ_OVERRIDE { mUpdateCursor = true; }
  virtual void            SetTransparencyMode(nsTransparencyMode aMode) MOZ_OVERRIDE;
  virtual nsTransparencyMode GetTransparencyMode() MOZ_OVERRIDE;
  virtual void            GetWindowClipRegion(nsTArray<nsIntRect>* aRects) MOZ_OVERRIDE;
  NS_IMETHOD              SetWindowShadowStyle(int32_t aStyle) MOZ_OVERRIDE;
  virtual void            SetShowsToolbarButton(bool aShow) MOZ_OVERRIDE {}
  virtual void            SetShowsFullScreenButton(bool aShow) MOZ_OVERRIDE {}
  virtual void            SetWindowAnimationType(WindowAnimationType aType) MOZ_OVERRIDE {}
  NS_IMETHOD              HideWindowChrome(bool aShouldHide) MOZ_OVERRIDE;
  NS_IMETHOD              MakeFullScreen(bool aFullScreen, nsIScreen* aScreen = nullptr) MOZ_OVERRIDE;
  virtual LayerManager*   GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                                          LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                                          LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                                          bool* aAllowRetaining = nullptr) MOZ_OVERRIDE;

  CompositorVsyncDispatcher* GetCompositorVsyncDispatcher() MOZ_OVERRIDE;
  void            CreateCompositorVsyncDispatcher();
  virtual CompositorParent* NewCompositorParent(int aSurfaceWidth, int aSurfaceHeight);
  virtual void            CreateCompositor();
  virtual void            CreateCompositor(int aWidth, int aHeight);
  virtual void            PrepareWindowEffects() MOZ_OVERRIDE {}
  virtual void            CleanupWindowEffects() MOZ_OVERRIDE {}
  virtual bool            PreRender(LayerManagerComposite* aManager) MOZ_OVERRIDE { return true; }
  virtual void            PostRender(LayerManagerComposite* aManager) MOZ_OVERRIDE {}
  virtual void            DrawWindowUnderlay(LayerManagerComposite* aManager, nsIntRect aRect) MOZ_OVERRIDE {}
  virtual void            DrawWindowOverlay(LayerManagerComposite* aManager, nsIntRect aRect) MOZ_OVERRIDE {}
  virtual mozilla::TemporaryRef<mozilla::gfx::DrawTarget> StartRemoteDrawing() MOZ_OVERRIDE;
  virtual void            EndRemoteDrawing() MOZ_OVERRIDE { };
  virtual void            CleanupRemoteDrawing() MOZ_OVERRIDE { };
  virtual void            UpdateThemeGeometries(const nsTArray<ThemeGeometry>& aThemeGeometries) MOZ_OVERRIDE {}
  NS_IMETHOD              SetModal(bool aModal) MOZ_OVERRIDE;
  virtual uint32_t        GetMaxTouchPoints() const MOZ_OVERRIDE;
  NS_IMETHOD              SetWindowClass(const nsAString& xulWinType) MOZ_OVERRIDE;
  virtual nsresult        SetWindowClipRegion(const nsTArray<nsIntRect>& aRects, bool aIntersectWithExisting) MOZ_OVERRIDE;
  // Return whether this widget interprets parameters to Move and Resize APIs
  // as "global display pixels" rather than "device pixels", and therefore
  // applies its GetDefaultScale() value to them before using them as mBounds
  // etc (which are always stored in device pixels).
  // Note that APIs that -get- the widget's position/size/bounds, rather than
  // -setting- them (i.e. moving or resizing the widget) will always return
  // values in the widget's device pixels.
  bool                    BoundsUseDisplayPixels() const {
    return mWindowType <= eWindowType_popup;
  }
  NS_IMETHOD              MoveClient(double aX, double aY) MOZ_OVERRIDE;
  NS_IMETHOD              ResizeClient(double aWidth, double aHeight, bool aRepaint) MOZ_OVERRIDE;
  NS_IMETHOD              ResizeClient(double aX, double aY, double aWidth, double aHeight, bool aRepaint) MOZ_OVERRIDE;
  NS_IMETHOD              GetBounds(nsIntRect &aRect) MOZ_OVERRIDE;
  NS_IMETHOD              GetClientBounds(nsIntRect &aRect) MOZ_OVERRIDE;
  NS_IMETHOD              GetScreenBounds(nsIntRect &aRect) MOZ_OVERRIDE;
  NS_IMETHOD              GetRestoredBounds(nsIntRect &aRect) MOZ_OVERRIDE;
  NS_IMETHOD              GetNonClientMargins(nsIntMargin &margins) MOZ_OVERRIDE;
  NS_IMETHOD              SetNonClientMargins(nsIntMargin &margins) MOZ_OVERRIDE;
  virtual nsIntPoint      GetClientOffset() MOZ_OVERRIDE;
  NS_IMETHOD              EnableDragDrop(bool aEnable) MOZ_OVERRIDE;
  NS_IMETHOD              GetAttention(int32_t aCycleCount) MOZ_OVERRIDE;
  virtual bool            HasPendingInputEvent() MOZ_OVERRIDE;
  NS_IMETHOD              SetIcon(const nsAString &anIconSpec) MOZ_OVERRIDE;
  NS_IMETHOD              SetWindowTitlebarColor(nscolor aColor, bool aActive) MOZ_OVERRIDE;
  virtual void            SetDrawsInTitlebar(bool aState) MOZ_OVERRIDE {}
  virtual bool            ShowsResizeIndicator(nsIntRect* aResizerRect) MOZ_OVERRIDE;
  virtual void            FreeNativeData(void * data, uint32_t aDataType) MOZ_OVERRIDE {}
  NS_IMETHOD              BeginResizeDrag(mozilla::WidgetGUIEvent* aEvent,
                                          int32_t aHorizontal,
                                          int32_t aVertical) MOZ_OVERRIDE;
  NS_IMETHOD              BeginMoveDrag(mozilla::WidgetMouseEvent* aEvent) MOZ_OVERRIDE;
  virtual nsresult        ActivateNativeMenuItemAt(const nsAString& indexString) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult        ForceUpdateNativeMenuAt(const nsAString& indexString) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD              NotifyIME(const IMENotification& aIMENotification) MOZ_OVERRIDE MOZ_FINAL;
  NS_IMETHOD              StartPluginIME(const mozilla::WidgetKeyboardEvent& aKeyboardEvent,
                                         int32_t aPanelX, int32_t aPanelY,
                                         nsString& aCommitted) MOZ_OVERRIDE
                          { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD              SetPluginFocused(bool& aFocused) MOZ_OVERRIDE
                          { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD              AttachNativeKeyEvent(mozilla::WidgetKeyboardEvent& aEvent) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD_(bool)       ExecuteNativeKeyBinding(
                            NativeKeyBindingsType aType,
                            const mozilla::WidgetKeyboardEvent& aEvent,
                            DoCommandCallback aCallback,
                            void* aCallbackData) MOZ_OVERRIDE { return false; }
  NS_IMETHOD              SetLayersAcceleration(bool aEnabled) MOZ_OVERRIDE;
  virtual bool            ComputeShouldAccelerate(bool aDefault);
  NS_IMETHOD              GetToggledKeyState(uint32_t aKeyCode, bool* aLEDState) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsIMEUpdatePreference GetIMEUpdatePreference() MOZ_OVERRIDE { return nsIMEUpdatePreference(); }
  NS_IMETHOD              OnDefaultButtonLoaded(const nsIntRect &aButtonRect) MOZ_OVERRIDE { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD              OverrideSystemMouseScrollSpeed(double aOriginalDeltaX,
                                                         double aOriginalDeltaY,
                                                         double& aOverriddenDeltaX,
                                                         double& aOverriddenDeltaY) MOZ_OVERRIDE;
  virtual already_AddRefed<nsIWidget>
  CreateChild(const nsIntRect  &aRect,
              nsWidgetInitData *aInitData = nullptr,
              bool             aForceUseIWidgetParent = false) MOZ_OVERRIDE;
  NS_IMETHOD              AttachViewToTopLevel(bool aUseAttachedEvents) MOZ_OVERRIDE;
  virtual nsIWidgetListener* GetAttachedWidgetListener() MOZ_OVERRIDE;
  virtual void               SetAttachedWidgetListener(nsIWidgetListener* aListener) MOZ_OVERRIDE;
  NS_IMETHOD              RegisterTouchWindow() MOZ_OVERRIDE;
  NS_IMETHOD              UnregisterTouchWindow() MOZ_OVERRIDE;
  NS_IMETHOD_(TextEventDispatcher*) GetTextEventDispatcher() MOZ_OVERRIDE MOZ_FINAL;

  void NotifyWindowDestroyed();
  void NotifySizeMoveDone();
  void NotifyWindowMoved(int32_t aX, int32_t aY);

  // Register plugin windows for remote updates from the compositor
  virtual void RegisterPluginWindowForRemoteUpdates() MOZ_OVERRIDE;
  virtual void UnregisterPluginWindowForRemoteUpdates() MOZ_OVERRIDE;

  virtual void SetNativeData(uint32_t aDataType, uintptr_t aVal) MOZ_OVERRIDE {};

  // Should be called by derived implementations to notify on system color and
  // theme changes.
  void NotifySysColorChanged();
  void NotifyThemeChanged();
  void NotifyUIStateChanged(UIStateChangeType aShowAccelerators,
                            UIStateChangeType aShowFocusRings);

#ifdef ACCESSIBILITY
  // Get the accessible for the window.
  mozilla::a11y::Accessible* GetRootAccessible();
#endif

  nsPopupLevel PopupLevel() { return mPopupLevel; }

  virtual nsIntSize       ClientToWindowSize(const nsIntSize& aClientSize) MOZ_OVERRIDE
  {
    return aClientSize;
  }

  // return true if this is a popup widget with a native titlebar
  bool IsPopupWithTitleBar() const
  {
    return (mWindowType == eWindowType_popup && 
            mBorderStyle != eBorderStyle_default &&
            mBorderStyle & eBorderStyle_title);
  }

  NS_IMETHOD              ReparentNativeWidget(nsIWidget* aNewParent) MOZ_OVERRIDE = 0;

  virtual uint32_t GetGLFrameBufferFormat() MOZ_OVERRIDE;

  virtual const SizeConstraints& GetSizeConstraints() const MOZ_OVERRIDE;
  virtual void SetSizeConstraints(const SizeConstraints& aConstraints) MOZ_OVERRIDE;

  /**
   * Use this when GetLayerManager() returns a BasicLayerManager
   * (nsBaseWidget::GetLayerManager() does). This sets up the widget's
   * layer manager to temporarily render into aTarget.
   *
   * |aNaturalWidgetBounds| is the un-rotated bounds of |aWidget|.
   * |aRotation| is the "virtual rotation" to apply when rendering to
   * the target.  When |aRotation| is ROTATION_0,
   * |aNaturalWidgetBounds| is not used.
   */
  class AutoLayerManagerSetup {
  public:
    AutoLayerManagerSetup(nsBaseWidget* aWidget, gfxContext* aTarget,
                          BufferMode aDoubleBuffering,
                          ScreenRotation aRotation = mozilla::ROTATION_0);
    ~AutoLayerManagerSetup();
  private:
    nsBaseWidget* mWidget;
    nsRefPtr<BasicLayerManager> mLayerManager;
  };
  friend class AutoLayerManagerSetup;

  class AutoUseBasicLayerManager {
  public:
    explicit AutoUseBasicLayerManager(nsBaseWidget* aWidget);
    ~AutoUseBasicLayerManager();
  private:
    nsBaseWidget* mWidget;
    bool mPreviousTemporarilyUseBasicLayerManager;
  };
  friend class AutoUseBasicLayerManager;

  virtual bool            ShouldUseOffMainThreadCompositing();

  static nsIRollupListener* GetActiveRollupListener();

  void Shutdown();

protected:

  void            ResolveIconName(const nsAString &aIconName,
                                  const nsAString &aIconSuffix,
                                  nsIFile **aResult);
  virtual void    OnDestroy();
  void            BaseCreate(nsIWidget *aParent,
                             const nsIntRect &aRect,
                             nsWidgetInitData *aInitData);

  virtual void ConfigureAPZCTreeManager();
  virtual already_AddRefed<GeckoContentController> CreateRootContentController();

  // Dispatch an event that has been routed through APZ directly from the
  // widget.
  nsEventStatus DispatchEventForAPZ(mozilla::WidgetGUIEvent* aEvent,
                                    const ScrollableLayerGuid& aGuid,
                                    uint64_t aInputBlockId);

  const nsIntRegion RegionFromArray(const nsTArray<nsIntRect>& aRects);
  void ArrayFromRegion(const nsIntRegion& aRegion, nsTArray<nsIntRect>& aRects);

  virtual nsIContent* GetLastRollup() MOZ_OVERRIDE
  {
    return mLastRollup;
  }

  virtual nsresult SynthesizeNativeKeyEvent(int32_t aNativeKeyboardLayout,
                                            int32_t aNativeKeyCode,
                                            uint32_t aModifierFlags,
                                            const nsAString& aCharacters,
                                            const nsAString& aUnmodifiedCharacters) MOZ_OVERRIDE
  { return NS_ERROR_UNEXPECTED; }

  virtual nsresult SynthesizeNativeMouseEvent(mozilla::LayoutDeviceIntPoint aPoint,
                                              uint32_t aNativeMessage,
                                              uint32_t aModifierFlags) MOZ_OVERRIDE
  { return NS_ERROR_UNEXPECTED; }

  virtual nsresult SynthesizeNativeMouseMove(mozilla::LayoutDeviceIntPoint aPoint) MOZ_OVERRIDE
  { return NS_ERROR_UNEXPECTED; }

  virtual nsresult SynthesizeNativeMouseScrollEvent(mozilla::LayoutDeviceIntPoint aPoint,
                                                    uint32_t aNativeMessage,
                                                    double aDeltaX,
                                                    double aDeltaY,
                                                    double aDeltaZ,
                                                    uint32_t aModifierFlags,
                                                    uint32_t aAdditionalFlags) MOZ_OVERRIDE
  { return NS_ERROR_UNEXPECTED; }

  virtual nsresult SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                              TouchPointerState aPointerState,
                                              nsIntPoint aPointerScreenPoint,
                                              double aPointerPressure,
                                              uint32_t aPointerOrientation) MOZ_OVERRIDE
  { return NS_ERROR_UNEXPECTED; }

  virtual nsresult NotifyIMEInternal(const IMENotification& aIMENotification)
  { return NS_ERROR_NOT_IMPLEMENTED; }

protected:
  // Utility to check if an array of clip rects is equal to our
  // internally stored clip rect array mClipRects.
  bool IsWindowClipRegionEqual(const nsTArray<nsIntRect>& aRects);

  // Stores the clip rectangles in aRects into mClipRects.
  void StoreWindowClipRegion(const nsTArray<nsIntRect>& aRects);

  virtual already_AddRefed<nsIWidget>
  AllocateChildPopupWidget()
  {
    static NS_DEFINE_IID(kCPopUpCID, NS_CHILD_CID);
    nsCOMPtr<nsIWidget> widget = do_CreateInstance(kCPopUpCID);
    return widget.forget();
  }

  LayerManager* CreateBasicLayerManager();

  nsPopupType PopupType() const { return mPopupType; }

  void NotifyRollupGeometryChange()
  {
    // XULPopupManager isn't interested in this notification, so only
    // send it if gRollupListener is set.
    if (gRollupListener) {
      gRollupListener->NotifyGeometryChange();
    }
  }

  /**
   * Apply the current size constraints to the given size.
   *
   * @param aWidth width to constrain
   * @param aHeight height to constrain
   */
  void ConstrainSize(int32_t* aWidth, int32_t* aHeight) const
  {
    *aWidth = std::max(mSizeConstraints.mMinSize.width,
                     std::min(mSizeConstraints.mMaxSize.width, *aWidth));
    *aHeight = std::max(mSizeConstraints.mMinSize.height,
                      std::min(mSizeConstraints.mMaxSize.height, *aHeight));
  }

  virtual CompositorChild* GetRemoteRenderer() MOZ_OVERRIDE;

  virtual void GetPreferredCompositorBackends(nsTArray<mozilla::layers::LayersBackend>& aHints);

  /**
   * Notify the widget that this window is being used with OMTC.
   */
  virtual void WindowUsesOMTC() {}

  nsIDocument* GetDocument() const;

protected:
  /**
   * Starts the OMTC compositor destruction sequence.
   *
   * When this function returns, the compositor should not be 
   * able to access the opengl context anymore.
   * It is safe to call it several times if platform implementations
   * require the compositor to be destroyed before ~nsBaseWidget is
   * reached (This is the case with gtk2 for instance).
   */
  void DestroyCompositor();

  nsIWidgetListener* mWidgetListener;
  nsIWidgetListener* mAttachedWidgetListener;
  nsRefPtr<LayerManager> mLayerManager;
  nsRefPtr<LayerManager> mBasicLayerManager;
  nsRefPtr<CompositorChild> mCompositorChild;
  nsRefPtr<CompositorParent> mCompositorParent;
  nsRefPtr<mozilla::CompositorVsyncDispatcher> mCompositorVsyncDispatcher;
  nsRefPtr<APZCTreeManager> mAPZC;
  nsRefPtr<APZEventState> mAPZEventState;
  nsRefPtr<SetTargetAPZCCallback> mSetTargetAPZCCallback;
  nsRefPtr<WidgetShutdownObserver> mShutdownObserver;
  nsRefPtr<TextEventDispatcher> mTextEventDispatcher;
  nsCursor          mCursor;
  bool              mUpdateCursor;
  nsBorderStyle     mBorderStyle;
  bool              mUseLayersAcceleration;
  bool              mForceLayersAcceleration;
  bool              mTemporarilyUseBasicLayerManager;
  // Windows with out-of-process tabs always require OMTC. This flag designates
  // such windows.
  bool              mRequireOffMainThreadCompositing;
  bool              mUseAttachedEvents;
  nsIntRect         mBounds;
  nsIntRect*        mOriginalBounds;
  // When this pointer is null, the widget is not clipped
  nsAutoArrayPtr<nsIntRect> mClipRects;
  uint32_t          mClipRectCount;
  nsSizeMode        mSizeMode;
  nsPopupLevel      mPopupLevel;
  nsPopupType       mPopupType;
  SizeConstraints   mSizeConstraints;

  static nsIRollupListener* gRollupListener;

  // the last rolled up popup. Only set this when an nsAutoRollup is in scope,
  // so it can be cleared automatically.
  static nsIContent* mLastRollup;

#ifdef DEBUG
protected:
  static nsAutoString debug_GuiEventToString(mozilla::WidgetGUIEvent* aGuiEvent);
  static bool debug_WantPaintFlashing();

  static void debug_DumpInvalidate(FILE *                aFileOut,
                                   nsIWidget *           aWidget,
                                   const nsIntRect *     aRect,
                                   const nsAutoCString & aWidgetName,
                                   int32_t               aWindowID);

  static void debug_DumpEvent(FILE* aFileOut,
                              nsIWidget* aWidget,
                              mozilla::WidgetGUIEvent* aGuiEvent,
                              const nsAutoCString& aWidgetName,
                              int32_t aWindowID);

  static void debug_DumpPaintEvent(FILE *                aFileOut,
                                   nsIWidget *           aWidget,
                                   const nsIntRegion &   aPaintEvent,
                                   const nsAutoCString & aWidgetName,
                                   int32_t               aWindowID);

  static bool debug_GetCachedBoolPref(const char* aPrefName);
#endif
};

// A situation can occur when a mouse event occurs over a menu label while the
// menu popup is already open. The expected behaviour is to close the popup.
// This happens by calling nsIRollupListener::Rollup before the mouse event is
// processed. However, in cases where the mouse event is not consumed, this
// event will then get targeted at the menu label causing the menu to open
// again. To prevent this, we store in mLastRollup a reference to the popup
// that was closed during the Rollup call, and prevent this popup from
// reopening while processing the mouse event.
// mLastRollup should only be set while an nsAutoRollup is in scope;
// when it goes out of scope mLastRollup is cleared automatically.
// As mLastRollup is static, it can be retrieved by calling
// nsIWidget::GetLastRollup on any widget.
class nsAutoRollup
{
  bool wasClear;

  public:

  nsAutoRollup();
  ~nsAutoRollup();
};

#endif // nsBaseWidget_h__
