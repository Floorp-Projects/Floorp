/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsBaseWidget_h__
#define nsBaseWidget_h__

#include "nsRect.h"
#include "nsIWidget.h"
#include "nsWidgetsCID.h"
#include "nsILocalFile.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsGUIEvent.h"
#include "nsAutoPtr.h"
#include "BasicLayers.h"
#include "nsIRollupListener.h"

class nsIContent;
class nsAutoRollup;
class gfxContext;

namespace mozilla {
namespace layers {
class CompositorChild;
class CompositorParent;
}
}

namespace base {
class Thread;
}

/**
 * Common widget implementation used as base class for native
 * or crossplatform implementations of Widgets. 
 * All cross-platform behavior that all widgets need to implement 
 * should be placed in this class. 
 * (Note: widget implementations are not required to use this
 * class, but it gives them a head start.)
 */

class nsBaseWidget : public nsIWidget
{
  friend class nsAutoRollup;

protected:
  typedef mozilla::layers::BasicLayerManager BasicLayerManager;
  typedef mozilla::layers::CompositorChild CompositorChild;
  typedef mozilla::layers::CompositorParent CompositorParent;
  typedef base::Thread Thread;

public:
  nsBaseWidget();
  virtual ~nsBaseWidget();
  
  NS_DECL_ISUPPORTS
  
  // nsIWidget interface
  NS_IMETHOD              CaptureMouse(bool aCapture);
  NS_IMETHOD              GetClientData(void*& aClientData);
  NS_IMETHOD              SetClientData(void* aClientData);
  NS_IMETHOD              Destroy();
  NS_IMETHOD              SetParent(nsIWidget* aNewParent);
  virtual nsIWidget*      GetParent(void);
  virtual nsIWidget*      GetTopLevelWidget();
  virtual nsIWidget*      GetSheetWindowParent(void);
  virtual float           GetDPI();
  virtual double          GetDefaultScale();
  virtual void            AddChild(nsIWidget* aChild);
  virtual void            RemoveChild(nsIWidget* aChild);

  NS_IMETHOD              SetZIndex(PRInt32 aZIndex);
  NS_IMETHOD              GetZIndex(PRInt32* aZIndex);
  NS_IMETHOD              PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                      nsIWidget *aWidget, bool aActivate);

  NS_IMETHOD              SetSizeMode(PRInt32 aMode);
  NS_IMETHOD              GetSizeMode(PRInt32* aMode);

  virtual nscolor         GetForegroundColor(void);
  NS_IMETHOD              SetForegroundColor(const nscolor &aColor);
  virtual nscolor         GetBackgroundColor(void);
  NS_IMETHOD              SetBackgroundColor(const nscolor &aColor);
  virtual nsCursor        GetCursor();
  NS_IMETHOD              SetCursor(nsCursor aCursor);
  NS_IMETHOD              SetCursor(imgIContainer* aCursor,
                                    PRUint32 aHotspotX, PRUint32 aHotspotY);
  NS_IMETHOD              GetWindowType(nsWindowType& aWindowType);
  virtual void            SetTransparencyMode(nsTransparencyMode aMode);
  virtual nsTransparencyMode GetTransparencyMode();
  virtual void            GetWindowClipRegion(nsTArray<nsIntRect>* aRects);
  NS_IMETHOD              SetWindowShadowStyle(PRInt32 aStyle);
  virtual void            SetShowsToolbarButton(bool aShow) {}
  virtual void            SetShowsFullScreenButton(bool aShow) {}
  virtual void            SetWindowAnimationType(WindowAnimationType aType) {}
  NS_IMETHOD              HideWindowChrome(bool aShouldHide);
  NS_IMETHOD              MakeFullScreen(bool aFullScreen);
  virtual nsDeviceContext* GetDeviceContext();
  virtual LayerManager*   GetLayerManager(PLayersChild* aShadowManager = nsnull,
                                          LayersBackend aBackendHint = LayerManager::LAYERS_NONE,
                                          LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                                          bool* aAllowRetaining = nsnull);

  virtual void            CreateCompositor();
  virtual void            DrawWindowUnderlay(LayerManager* aManager, nsIntRect aRect) {}
  virtual void            DrawWindowOverlay(LayerManager* aManager, nsIntRect aRect) {}
  virtual void            UpdateThemeGeometries(const nsTArray<ThemeGeometry>& aThemeGeometries) {}
  virtual gfxASurface*    GetThebesSurface();
  NS_IMETHOD              SetModal(bool aModal); 
  NS_IMETHOD              SetWindowClass(const nsAString& xulWinType);
  NS_IMETHOD              MoveClient(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD              ResizeClient(PRInt32 aWidth, PRInt32 aHeight, bool aRepaint);
  NS_IMETHOD              ResizeClient(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, bool aRepaint);
  NS_IMETHOD              SetBounds(const nsIntRect &aRect);
  NS_IMETHOD              GetBounds(nsIntRect &aRect);
  NS_IMETHOD              GetClientBounds(nsIntRect &aRect);
  NS_IMETHOD              GetScreenBounds(nsIntRect &aRect);
  NS_IMETHOD              GetNonClientMargins(nsIntMargin &margins);
  NS_IMETHOD              SetNonClientMargins(nsIntMargin &margins);
  virtual nsIntPoint      GetClientOffset();
  NS_IMETHOD              EnableDragDrop(bool aEnable);
  NS_IMETHOD              GetAttention(PRInt32 aCycleCount);
  virtual bool            HasPendingInputEvent();
  NS_IMETHOD              SetIcon(const nsAString &anIconSpec);
  NS_IMETHOD              BeginSecureKeyboardInput();
  NS_IMETHOD              EndSecureKeyboardInput();
  NS_IMETHOD              SetWindowTitlebarColor(nscolor aColor, bool aActive);
  virtual void            SetDrawsInTitlebar(bool aState) {}
  virtual bool            ShowsResizeIndicator(nsIntRect* aResizerRect);
  virtual void            FreeNativeData(void * data, PRUint32 aDataType) {}
  NS_IMETHOD              BeginResizeDrag(nsGUIEvent* aEvent, PRInt32 aHorizontal, PRInt32 aVertical);
  NS_IMETHOD              BeginMoveDrag(nsMouseEvent* aEvent);
  virtual nsresult        ActivateNativeMenuItemAt(const nsAString& indexString) { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsresult        ForceUpdateNativeMenuAt(const nsAString& indexString) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD              ResetInputState() { return NS_OK; }
  NS_IMETHOD              CancelIMEComposition() { return NS_OK; }
  NS_IMETHOD              SetAcceleratedRendering(bool aEnabled);
  virtual bool            GetAcceleratedRendering();
  virtual bool            GetShouldAccelerate();
  NS_IMETHOD              GetToggledKeyState(PRUint32 aKeyCode, bool* aLEDState) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD              OnIMEFocusChange(bool aFocus) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD              OnIMETextChange(PRUint32 aStart, PRUint32 aOldEnd, PRUint32 aNewEnd) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD              OnIMESelectionChange(void) { return NS_ERROR_NOT_IMPLEMENTED; }
  virtual nsIMEUpdatePreference GetIMEUpdatePreference() { return nsIMEUpdatePreference(false, false); }
  NS_IMETHOD              OnDefaultButtonLoaded(const nsIntRect &aButtonRect) { return NS_ERROR_NOT_IMPLEMENTED; }
  NS_IMETHOD              OverrideSystemMouseScrollSpeed(PRInt32 aOriginalDelta, bool aIsHorizontal, PRInt32 &aOverriddenDelta);
  virtual already_AddRefed<nsIWidget>
  CreateChild(const nsIntRect  &aRect,
              EVENT_CALLBACK   aHandleEventFunction,
              nsDeviceContext *aContext,
              nsWidgetInitData *aInitData = nsnull,
              bool             aForceUseIWidgetParent = false);
  NS_IMETHOD              SetEventCallback(EVENT_CALLBACK aEventFunction, nsDeviceContext *aContext);
  NS_IMETHOD              AttachViewToTopLevel(EVENT_CALLBACK aViewEventFunction, nsDeviceContext *aContext);
  virtual ViewWrapper*    GetAttachedViewPtr();
  NS_IMETHOD              SetAttachedViewPtr(ViewWrapper* aViewWrapper);
  NS_IMETHOD              RegisterTouchWindow();
  NS_IMETHOD              UnregisterTouchWindow();

  nsPopupLevel PopupLevel() { return mPopupLevel; }

  virtual nsIntSize       ClientToWindowSize(const nsIntSize& aClientSize)
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

  NS_IMETHOD              ReparentNativeWidget(nsIWidget* aNewParent) = 0;

  virtual PRUint32 GetGLFrameBufferFormat() MOZ_OVERRIDE;

  /**
   * Use this when GetLayerManager() returns a BasicLayerManager
   * (nsBaseWidget::GetLayerManager() does). This sets up the widget's
   * layer manager to temporarily render into aTarget.
   */
  class AutoLayerManagerSetup {
  public:
    AutoLayerManagerSetup(nsBaseWidget* aWidget, gfxContext* aTarget,
                          BasicLayerManager::BufferMode aDoubleBuffering);
    ~AutoLayerManagerSetup();
  private:
    nsBaseWidget* mWidget;
  };
  friend class AutoLayerManagerSetup;

  class AutoUseBasicLayerManager {
  public:
    AutoUseBasicLayerManager(nsBaseWidget* aWidget);
    ~AutoUseBasicLayerManager();
  private:
    nsBaseWidget* mWidget;
  };
  friend class AutoUseBasicLayerManager;

  bool HasDestroyStarted() const 
  {
    return mOnDestroyCalled;
  }

  bool                    Destroyed() { return mOnDestroyCalled; }

  nsWindowType            GetWindowType() { return mWindowType; }

  static bool             UseOffMainThreadCompositing();
protected:

  virtual void            ResolveIconName(const nsAString &aIconName,
                                          const nsAString &aIconSuffix,
                                          nsILocalFile **aResult);
  virtual void            OnDestroy();
  virtual void            BaseCreate(nsIWidget *aParent,
                                     const nsIntRect &aRect,
                                     EVENT_CALLBACK aHandleEventFunction,
                                     nsDeviceContext *aContext,
                                     nsWidgetInitData *aInitData);

  virtual nsIContent* GetLastRollup()
  {
    return mLastRollup;
  }

  virtual nsresult SynthesizeNativeKeyEvent(PRInt32 aNativeKeyboardLayout,
                                            PRInt32 aNativeKeyCode,
                                            PRUint32 aModifierFlags,
                                            const nsAString& aCharacters,
                                            const nsAString& aUnmodifiedCharacters)
  { return NS_ERROR_UNEXPECTED; }

  virtual nsresult SynthesizeNativeMouseEvent(nsIntPoint aPoint,
                                              PRUint32 aNativeMessage,
                                              PRUint32 aModifierFlags)
  { return NS_ERROR_UNEXPECTED; }

  virtual nsresult SynthesizeNativeMouseMove(nsIntPoint aPoint)
  { return NS_ERROR_UNEXPECTED; }

  virtual nsresult SynthesizeNativeMouseScrollEvent(nsIntPoint aPoint,
                                                    PRUint32 aNativeMessage,
                                                    double aDeltaX,
                                                    double aDeltaY,
                                                    double aDeltaZ,
                                                    PRUint32 aModifierFlags,
                                                    PRUint32 aAdditionalFlags)
  { return NS_ERROR_UNEXPECTED; }

  // Stores the clip rectangles in aRects into mClipRects. Returns true
  // if the new rectangles are different from the old rectangles.
  bool StoreWindowClipRegion(const nsTArray<nsIntRect>& aRects);

  virtual already_AddRefed<nsIWidget>
  AllocateChildPopupWidget()
  {
    static NS_DEFINE_IID(kCPopUpCID, NS_CHILD_CID);
    nsCOMPtr<nsIWidget> widget = do_CreateInstance(kCPopUpCID);
    return widget.forget();
  }

  BasicLayerManager* CreateBasicLayerManager();

  void NotifyRollupGeometryChange(nsIRollupListener* aRollupListener)
  {
    if (aRollupListener) {
      aRollupListener->NotifyGeometryChange();
    }
  }

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

  void*             mClientData;
  ViewWrapper*      mViewWrapperPtr;
  EVENT_CALLBACK    mEventCallback;
  EVENT_CALLBACK    mViewCallback;
  nsDeviceContext* mContext;
  nsRefPtr<LayerManager> mLayerManager;
  nsRefPtr<LayerManager> mBasicLayerManager;
  nsRefPtr<CompositorChild> mCompositorChild;
  nsRefPtr<CompositorParent> mCompositorParent;
  Thread*           mCompositorThread;
  nscolor           mBackground;
  nscolor           mForeground;
  nsCursor          mCursor;
  nsWindowType      mWindowType;
  nsBorderStyle     mBorderStyle;
  bool              mOnDestroyCalled;
  bool              mUseAcceleratedRendering;
  bool              mForceLayersAcceleration;
  bool              mTemporarilyUseBasicLayerManager;
  nsIntRect         mBounds;
  nsIntRect*        mOriginalBounds;
  // When this pointer is null, the widget is not clipped
  nsAutoArrayPtr<nsIntRect> mClipRects;
  PRUint32          mClipRectCount;
  PRInt32           mZIndex;
  nsSizeMode        mSizeMode;
  nsPopupLevel      mPopupLevel;

  // the last rolled up popup. Only set this when an nsAutoRollup is in scope,
  // so it can be cleared automatically.
  static nsIContent* mLastRollup;

#ifdef DEBUG
protected:
  static nsAutoString debug_GuiEventToString(nsGUIEvent * aGuiEvent);
  static bool debug_WantPaintFlashing();

  static void debug_DumpInvalidate(FILE *                aFileOut,
                                   nsIWidget *           aWidget,
                                   const nsIntRect *     aRect,
                                   const nsCAutoString & aWidgetName,
                                   PRInt32               aWindowID);

  static void debug_DumpEvent(FILE *                aFileOut,
                              nsIWidget *           aWidget,
                              nsGUIEvent *          aGuiEvent,
                              const nsCAutoString & aWidgetName,
                              PRInt32               aWindowID);

  static void debug_DumpPaintEvent(FILE *                aFileOut,
                                   nsIWidget *           aWidget,
                                   nsPaintEvent *        aPaintEvent,
                                   const nsCAutoString & aWidgetName,
                                   PRInt32               aWindowID);

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
