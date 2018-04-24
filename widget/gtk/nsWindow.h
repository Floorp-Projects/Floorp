/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsWindow_h__
#define __nsWindow_h__

#include "mozcontainer.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsIDragService.h"
#include "nsITimer.h"
#include "nsGkAtoms.h"
#include "nsRefPtrHashtable.h"

#include "nsBaseWidget.h"
#include "CompositorWidget.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#ifdef MOZ_X11
#include <gdk/gdkx.h>
#include "X11UndefineNone.h"
#endif /* MOZ_X11 */
#ifdef MOZ_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#include "mozilla/widget/WindowSurface.h"
#include "mozilla/widget/WindowSurfaceProvider.h"

#ifdef ACCESSIBILITY
#include "mozilla/a11y/Accessible.h"
#endif
#include "mozilla/EventForwards.h"
#include "mozilla/TouchEvents.h"

#include "IMContextWrapper.h"

#undef LOG
#ifdef MOZ_LOGGING

#include "mozilla/Logging.h"
#include "nsTArray.h"
#include "Units.h"

extern mozilla::LazyLogModule gWidgetLog;
extern mozilla::LazyLogModule gWidgetFocusLog;
extern mozilla::LazyLogModule gWidgetDragLog;
extern mozilla::LazyLogModule gWidgetDrawLog;

#define LOG(args) MOZ_LOG(gWidgetLog, mozilla::LogLevel::Debug, args)
#define LOGFOCUS(args) MOZ_LOG(gWidgetFocusLog, mozilla::LogLevel::Debug, args)
#define LOGDRAG(args) MOZ_LOG(gWidgetDragLog, mozilla::LogLevel::Debug, args)
#define LOGDRAW(args) MOZ_LOG(gWidgetDrawLog, mozilla::LogLevel::Debug, args)

#else

#define LOG(args)
#define LOGFOCUS(args)
#define LOGDRAG(args)
#define LOGDRAW(args)

#endif /* MOZ_LOGGING */

class gfxPattern;

namespace mozilla {
class TimeStamp;
class CurrentX11TimeGetter;
}

class nsWindow final : public nsBaseWidget
{
public:
    typedef mozilla::gfx::DrawTarget DrawTarget;
    typedef mozilla::WidgetEventTime WidgetEventTime;
    typedef mozilla::WidgetKeyboardEvent WidgetKeyboardEvent;
    typedef mozilla::widget::PlatformCompositorWidgetDelegate PlatformCompositorWidgetDelegate;

    nsWindow();

    static void ReleaseGlobals();

    NS_INLINE_DECL_REFCOUNTING_INHERITED(nsWindow, nsBaseWidget)

    void CommonCreate(nsIWidget *aParent, bool aListenForResizes);

    virtual nsresult DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                                   nsEventStatus& aStatus) override;

    // called when we are destroyed
    virtual void OnDestroy(void) override;

    // called to check and see if a widget's dimensions are sane
    bool AreBoundsSane(void);

    // nsIWidget
    using nsBaseWidget::Create; // for Create signature not overridden here
    virtual MOZ_MUST_USE nsresult Create(nsIWidget* aParent,
                                         nsNativeWidget aNativeParent,
                                         const LayoutDeviceIntRect& aRect,
                                         nsWidgetInitData* aInitData) override;
    virtual void       Destroy() override;
    virtual nsIWidget *GetParent() override;
    virtual float      GetDPI() override;
    virtual double     GetDefaultScaleInternal() override;
    mozilla::DesktopToLayoutDeviceScale GetDesktopToDeviceScale() override;
    virtual void       SetParent(nsIWidget* aNewParent) override;
    virtual void       SetModal(bool aModal) override;
    virtual bool       IsVisible() const override;
    virtual void       ConstrainPosition(bool aAllowSlop,
                                         int32_t *aX,
                                         int32_t *aY) override;
    virtual void       SetSizeConstraints(const SizeConstraints& aConstraints) override;
    virtual void       Move(double aX,
                            double aY) override;
    virtual void       Show             (bool aState) override;
    virtual void       Resize           (double aWidth,
                                         double aHeight,
                                         bool   aRepaint) override;
    virtual void       Resize           (double aX,
                                         double aY,
                                         double aWidth,
                                         double aHeight,
                                         bool   aRepaint) override;
    virtual bool       IsEnabled() const override;

    void               SetZIndex(int32_t aZIndex) override;
    virtual void       SetSizeMode(nsSizeMode aMode) override;
    virtual void       Enable(bool aState) override;
    virtual nsresult   SetFocus(bool aRaise = false) override;
    virtual LayoutDeviceIntRect GetScreenBounds() override;
    virtual LayoutDeviceIntRect GetClientBounds() override;
    virtual LayoutDeviceIntSize GetClientSize() override;
    virtual LayoutDeviceIntPoint GetClientOffset() override;
    virtual void       SetCursor(nsCursor aCursor) override;
    virtual nsresult   SetCursor(imgIContainer* aCursor,
                                 uint32_t aHotspotX, uint32_t aHotspotY) override;
    virtual void       Invalidate(const LayoutDeviceIntRect& aRect) override;
    virtual void*      GetNativeData(uint32_t aDataType) override;
    virtual nsresult   SetTitle(const nsAString& aTitle) override;
    virtual void       SetIcon(const nsAString& aIconSpec) override;
    virtual void       SetWindowClass(const nsAString& xulWinType) override;
    virtual LayoutDeviceIntPoint WidgetToScreenOffset() override;
    virtual void       CaptureMouse(bool aCapture) override;
    virtual void       CaptureRollupEvents(nsIRollupListener *aListener,
                                           bool aDoCapture) override;
    virtual MOZ_MUST_USE nsresult GetAttention(int32_t aCycleCount) override;
    virtual nsresult   SetWindowClipRegion(const nsTArray<LayoutDeviceIntRect>& aRects,
                                           bool aIntersectWithExisting) override;
    virtual bool       HasPendingInputEvent() override;

    virtual bool PrepareForFullscreenTransition(nsISupports** aData) override;
    virtual void PerformFullscreenTransition(FullscreenTransitionStage aStage,
                                             uint16_t aDuration,
                                             nsISupports* aData,
                                             nsIRunnable* aCallback) override;
    virtual already_AddRefed<nsIScreen> GetWidgetScreen() override;
    virtual nsresult   MakeFullScreen(bool aFullScreen,
                                      nsIScreen* aTargetScreen = nullptr) override;
    virtual void       HideWindowChrome(bool aShouldHide) override;

    /**
     * GetLastUserInputTime returns a timestamp for the most recent user input
     * event.  This is intended for pointer grab requests (including drags).
     */
    static guint32     GetLastUserInputTime();

    // utility method, -1 if no change should be made, otherwise returns a
    // value that can be passed to gdk_window_set_decorations
    gint               ConvertBorderStyles(nsBorderStyle aStyle);

    GdkRectangle DevicePixelsToGdkRectRoundOut(LayoutDeviceIntRect aRect);

    // event callbacks
    gboolean           OnExposeEvent(cairo_t *cr);
    gboolean           OnConfigureEvent(GtkWidget *aWidget,
                                        GdkEventConfigure *aEvent);
    void               OnContainerUnrealize();
    void               OnSizeAllocate(GtkAllocation *aAllocation);
    void               OnDeleteEvent();
    void               OnEnterNotifyEvent(GdkEventCrossing *aEvent);
    void               OnLeaveNotifyEvent(GdkEventCrossing *aEvent);
    void               OnMotionNotifyEvent(GdkEventMotion *aEvent);
    void               OnButtonPressEvent(GdkEventButton *aEvent);
    void               OnButtonReleaseEvent(GdkEventButton *aEvent);
    void               OnContainerFocusInEvent(GdkEventFocus *aEvent);
    void               OnContainerFocusOutEvent(GdkEventFocus *aEvent);
    gboolean           OnKeyPressEvent(GdkEventKey *aEvent);
    gboolean           OnKeyReleaseEvent(GdkEventKey *aEvent);

    /**
     * MaybeDispatchContextMenuEvent() may dispatch eContextMenu event if
     * the given key combination should cause opening context menu.
     *
     * @param aEvent            The native key event.
     * @return                  true if this method dispatched eContextMenu
     *                          event.  Otherwise, false.
     *                          Be aware, when this returns true, the
     *                          widget may have been destroyed.
     */
    bool               MaybeDispatchContextMenuEvent(const GdkEventKey* aEvent);

    void               OnScrollEvent(GdkEventScroll *aEvent);
    void               OnVisibilityNotifyEvent(GdkEventVisibility *aEvent);
    void               OnWindowStateEvent(GtkWidget *aWidget,
                                          GdkEventWindowState *aEvent);
    void               OnDragDataReceivedEvent(GtkWidget       *aWidget,
                                               GdkDragContext  *aDragContext,
                                               gint             aX,
                                               gint             aY,
                                               GtkSelectionData*aSelectionData,
                                               guint            aInfo,
                                               guint            aTime,
                                               gpointer         aData);
    gboolean           OnPropertyNotifyEvent(GtkWidget *aWidget,
                                             GdkEventProperty *aEvent);
#if GTK_CHECK_VERSION(3,4,0)
    gboolean           OnTouchEvent(GdkEventTouch* aEvent);
#endif

    virtual already_AddRefed<mozilla::gfx::DrawTarget>
                       StartRemoteDrawingInRegion(LayoutDeviceIntRegion& aInvalidRegion,
                                                  mozilla::layers::BufferMode* aBufferMode) override;
    virtual void       EndRemoteDrawingInRegion(mozilla::gfx::DrawTarget* aDrawTarget,
                                                LayoutDeviceIntRegion& aInvalidRegion) override;

    void               SetProgress(unsigned long progressPercent);

private:
    void               UpdateAlpha(mozilla::gfx::SourceSurface* aSourceSurface, nsIntRect aBoundsRect);

    void               NativeMove();
    void               NativeResize();
    void               NativeMoveResize();

    void               NativeShow  (bool    aAction);
    void               SetHasMappedToplevel(bool aState);
    LayoutDeviceIntSize GetSafeWindowSize(LayoutDeviceIntSize aSize);

    void               EnsureGrabs  (void);
    void               GrabPointer  (guint32 aTime);
    void               ReleaseGrabs (void);

    void               UpdateClientOffset();

    void               DispatchContextMenuEventFromMouseEvent(uint16_t domButton,
                                                              GdkEventButton *aEvent);
public:
    void               ThemeChanged(void);
    void               OnDPIChanged(void);
    void               OnCheckResize(void);
    void               OnCompositedChanged(void);

#ifdef MOZ_X11
    Window             mOldFocusWindow;
#endif /* MOZ_X11 */

    static guint32     sLastButtonPressTime;

    virtual MOZ_MUST_USE nsresult
                       BeginResizeDrag(mozilla::WidgetGUIEvent* aEvent,
                                       int32_t aHorizontal,
                                       int32_t aVertical) override;
    virtual MOZ_MUST_USE nsresult
                       BeginMoveDrag(mozilla::WidgetMouseEvent* aEvent) override;

    MozContainer*      GetMozContainer() { return mContainer; }
    // GetMozContainerWidget returns the MozContainer even for undestroyed
    // descendant windows
    GtkWidget*         GetMozContainerWidget();
    GdkWindow*         GetGdkWindow() { return mGdkWindow; }
    bool               IsDestroyed() { return mIsDestroyed; }

    void               DispatchDragEvent(mozilla::EventMessage aMsg,
                                         const LayoutDeviceIntPoint& aRefPoint,
                                         guint aTime);
    static void        UpdateDragStatus (GdkDragContext *aDragContext,
                                         nsIDragService *aDragService);
    /**
     * DispatchKeyDownOrKeyUpEvent() dispatches eKeyDown or eKeyUp event.
     *
     * @param aEvent            A native GDK_KEY_PRESS or GDK_KEY_RELEASE
     *                          event.
     * @param aProcessedByIME   true if the event is handled by IME.
     * @param aIsCancelled      [Out] true if the default is prevented.
     * @return                  true if eKeyDown event is actually dispatched.
     *                          Otherwise, false.
     */
    bool DispatchKeyDownOrKeyUpEvent(GdkEventKey* aEvent,
                                     bool aProcessedByIME,
                                     bool* aIsCancelled);

    /**
     * DispatchKeyDownOrKeyUpEvent() dispatches eKeyDown or eKeyUp event.
     *
     * @param aEvent            An eKeyDown or eKeyUp event.  This will be
     *                          dispatched as is.
     * @param aIsCancelled      [Out] true if the default is prevented.
     * @return                  true if eKeyDown event is actually dispatched.
     *                          Otherwise, false.
     */
    bool DispatchKeyDownOrKeyUpEvent(WidgetKeyboardEvent& aEvent,
                                     bool* aIsCancelled);

    WidgetEventTime    GetWidgetEventTime(guint32 aEventTime);
    mozilla::TimeStamp GetEventTimeStamp(guint32 aEventTime);
    mozilla::CurrentX11TimeGetter* GetCurrentTimeGetter();

    virtual void SetInputContext(const InputContext& aContext,
                                 const InputContextAction& aAction) override;
    virtual InputContext GetInputContext() override;
    virtual TextEventDispatcherListener*
        GetNativeTextEventDispatcherListener() override;
    void GetEditCommandsRemapped(NativeKeyBindingsType aType,
                                 const mozilla::WidgetKeyboardEvent& aEvent,
                                 nsTArray<mozilla::CommandInt>& aCommands,
                                 uint32_t aGeckoKeyCode,
                                 uint32_t aNativeKeyCode);
    virtual void GetEditCommands(
                     NativeKeyBindingsType aType,
                     const mozilla::WidgetKeyboardEvent& aEvent,
                     nsTArray<mozilla::CommandInt>& aCommands) override;

    // These methods are for toplevel windows only.
    void               ResizeTransparencyBitmap();
    void               ApplyTransparencyBitmap();
    void               ClearTransparencyBitmap();

   virtual void        SetTransparencyMode(nsTransparencyMode aMode) override;
   virtual nsTransparencyMode GetTransparencyMode() override;
#if (MOZ_WIDGET_GTK >= 3)
   virtual void        UpdateOpaqueRegion(const LayoutDeviceIntRegion& aOpaqueRegion) override;
#endif
   virtual nsresult    ConfigureChildren(const nsTArray<Configuration>& aConfigurations) override;
   nsresult            UpdateTranslucentWindowAlphaInternal(const nsIntRect& aRect,
                                                            uint8_t* aAlphas, int32_t aStride);

    virtual void       ReparentNativeWidget(nsIWidget* aNewParent) override;

    virtual nsresult SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                                uint32_t aNativeMessage,
                                                uint32_t aModifierFlags,
                                                nsIObserver* aObserver) override;

    virtual nsresult SynthesizeNativeMouseMove(LayoutDeviceIntPoint aPoint,
                                               nsIObserver* aObserver) override
    { return SynthesizeNativeMouseEvent(aPoint, GDK_MOTION_NOTIFY, 0, aObserver); }

    virtual nsresult SynthesizeNativeMouseScrollEvent(LayoutDeviceIntPoint aPoint,
                                                      uint32_t aNativeMessage,
                                                      double aDeltaX,
                                                      double aDeltaY,
                                                      double aDeltaZ,
                                                      uint32_t aModifierFlags,
                                                      uint32_t aAdditionalFlags,
                                                      nsIObserver* aObserver) override;

#if GTK_CHECK_VERSION(3,4,0)
    virtual nsresult SynthesizeNativeTouchPoint(uint32_t aPointerId,
                                                TouchPointerState aPointerState,
                                                LayoutDeviceIntPoint aPoint,
                                                double aPointerPressure,
                                                uint32_t aPointerOrientation,
                                                nsIObserver* aObserver) override;
#endif


#ifdef MOZ_X11
    Display* XDisplay() { return mXDisplay; }
#endif
#ifdef MOZ_WAYLAND
    wl_display* GetWaylandDisplay();
    wl_surface* GetWaylandSurface();
#endif
    virtual void GetCompositorWidgetInitData(mozilla::widget::CompositorWidgetInitData* aInitData) override;

    virtual nsresult SetNonClientMargins(LayoutDeviceIntMargin& aMargins) override;
    void SetDrawsInTitlebar(bool aState) override;
    virtual void UpdateWindowDraggingRegion(const LayoutDeviceIntRegion& aRegion) override;

    // HiDPI scale conversion
    gint GdkScaleFactor();

    // To GDK
    gint DevicePixelsToGdkCoordRoundUp(int pixels);
    gint DevicePixelsToGdkCoordRoundDown(int pixels);
    GdkPoint DevicePixelsToGdkPointRoundDown(LayoutDeviceIntPoint point);
    GdkRectangle DevicePixelsToGdkSizeRoundUp(LayoutDeviceIntSize pixelSize);

    // From GDK
    int GdkCoordToDevicePixels(gint coord);
    LayoutDeviceIntPoint GdkPointToDevicePixels(GdkPoint point);
    LayoutDeviceIntPoint GdkEventCoordsToDevicePixels(gdouble x, gdouble y);
    LayoutDeviceIntRect GdkRectToDevicePixels(GdkRectangle rect);

    virtual bool WidgetTypeSupportsAcceleration() override;

    typedef enum { CSD_SUPPORT_SYSTEM,    // CSD including shadows
                   CSD_SUPPORT_CLIENT,    // CSD without shadows
                   CSD_SUPPORT_NONE,      // WM does not support CSD at all
                   CSD_SUPPORT_UNKNOWN
    } CSDSupportLevel;
    /**
     * Get the support of Client Side Decoration by checking
     * the XDG_CURRENT_DESKTOP environment variable.
     */
    static CSDSupportLevel GetSystemCSDSupportLevel();

protected:
    virtual ~nsWindow();

    // event handling code
    void DispatchActivateEvent(void);
    void DispatchDeactivateEvent(void);
    void DispatchResized();
    void MaybeDispatchResized();

    // Helper for SetParent and ReparentNativeWidget.
    void ReparentNativeWidgetInternal(nsIWidget* aNewParent,
                                      GtkWidget* aNewContainer,
                                      GdkWindow* aNewParentWindow,
                                      GtkWidget* aOldContainer);

    virtual void RegisterTouchWindow() override;

    nsCOMPtr<nsIWidget> mParent;
    // Is this a toplevel window?
    bool                mIsTopLevel;
    // Has this widget been destroyed yet?
    bool                mIsDestroyed;

    // Should we send resize events on all resizes?
    bool                mListenForResizes;
    // Does WindowResized need to be called on listeners?
    bool                mNeedsDispatchResized;
    // This flag tracks if we're hidden or shown.
    bool                mIsShown;
    bool                mNeedsShow;
    // is this widget enabled?
    bool                mEnabled;
    // has the native window for this been created yet?
    bool                mCreated;
#if GTK_CHECK_VERSION(3,4,0)
    // whether we handle touch event
    bool                mHandleTouchEvent;
#endif
    // true if this is a drag and drop feedback popup
    bool               mIsDragPopup;
    // Can we access X?
    bool               mIsX11Display;

private:
    void               DestroyChildWindows();
    GtkWidget         *GetToplevelWidget();
    nsWindow          *GetContainerWindow();
    void               SetUrgencyHint(GtkWidget *top_window, bool state);
    void               SetDefaultIcon(void);
    void               SetWindowDecoration(nsBorderStyle aStyle);
    void               InitButtonEvent(mozilla::WidgetMouseEvent& aEvent,
                                       GdkEventButton* aGdkEvent);
    bool               DispatchCommandEvent(nsAtom* aCommand);
    bool               DispatchContentCommandEvent(mozilla::EventMessage aMsg);
    bool               CheckForRollup(gdouble aMouseX, gdouble aMouseY,
                                      bool aIsWheel, bool aAlwaysRollup);
    void               CheckForRollupDuringGrab()
    {
      CheckForRollup(0, 0, false, true);
    }

    bool               GetDragInfo(mozilla::WidgetMouseEvent* aMouseEvent,
                                   GdkWindow** aWindow, gint* aButton,
                                   gint* aRootX, gint* aRootY);
    void               ClearCachedResources();
    nsIWidgetListener* GetListener();
    bool               IsComposited() const;

    void               UpdateClientOffsetForCSDWindow();

    GtkWidget          *mShell;
    MozContainer       *mContainer;
    GdkWindow          *mGdkWindow;
    PlatformCompositorWidgetDelegate* mCompositorWidgetDelegate;


    uint32_t            mHasMappedToplevel : 1,
                        mIsFullyObscured : 1,
                        mRetryPointerGrab : 1;
    nsSizeMode          mSizeState;

    int32_t             mTransparencyBitmapWidth;
    int32_t             mTransparencyBitmapHeight;

    nsIntPoint          mClientOffset;

#if GTK_CHECK_VERSION(3,4,0)
    // This field omits duplicate scroll events caused by GNOME bug 726878.
    guint32             mLastScrollEventTime;

    // for touch event handling
    nsRefPtrHashtable<nsPtrHashKey<GdkEventSequence>, mozilla::dom::Touch> mTouches;
#endif

#ifdef MOZ_X11
    Display*            mXDisplay;
    Window              mXWindow;
    Visual*             mXVisual;
    int                 mXDepth;
    mozilla::widget::WindowSurfaceProvider mSurfaceProvider;
#endif

    // Upper bound on pending ConfigureNotify events to be dispatched to the
    // window. See bug 1225044.
    unsigned int mPendingConfigures;

    // Window titlebar rendering mode, CSD_SUPPORT_NONE if it's disabled
    // for this window.
    CSDSupportLevel    mCSDSupportLevel;
    // If true, draw our own window titlebar.
    bool               mDrawInTitlebar;
    // Draggable titlebar region maintained by UpdateWindowDraggingRegion
    LayoutDeviceIntRegion mDraggableRegion;

#ifdef ACCESSIBILITY
    RefPtr<mozilla::a11y::Accessible> mRootAccessible;

    /**
     * Request to create the accessible for this window if it is top level.
     */
    void                CreateRootAccessible();

    /**
     * Dispatch accessible event for the top level window accessible.
     *
     * @param  aEventType  [in] the accessible event type to dispatch
     */
    void                DispatchEventToRootAccessible(uint32_t aEventType);

    /**
     * Dispatch accessible window activate event for the top level window
     * accessible.
     */
    void                DispatchActivateEventAccessible();

    /**
     * Dispatch accessible window deactivate event for the top level window
     * accessible.
     */
    void                DispatchDeactivateEventAccessible();

    /**
     * Dispatch accessible window maximize event for the top level window
     * accessible.
     */
    void                DispatchMaximizeEventAccessible();

    /**
     * Dispatch accessible window minize event for the top level window
     * accessible.
     */
    void                DispatchMinimizeEventAccessible();

    /**
     * Dispatch accessible window restore event for the top level window
     * accessible.
     */
    void                DispatchRestoreEventAccessible();
#endif

    // The cursor cache
    static GdkCursor   *gsGtkCursorCache[eCursorCount];

    // Transparency
    bool         mIsTransparent;
    // This bitmap tracks which pixels are transparent. We don't support
    // full translucency at this time; each pixel is either fully opaque
    // or fully transparent.
    gchar*       mTransparencyBitmap;

    // all of our DND stuff
    void   InitDragEvent(mozilla::WidgetDragEvent& aEvent);

    float              mLastMotionPressure;

    // Remember the last sizemode so that we can restore it when
    // leaving fullscreen
    nsSizeMode         mLastSizeMode;

    static bool DragInProgress(void);

    void DispatchMissedButtonReleases(GdkEventCrossing *aGdkEvent);

    // nsBaseWidget
    virtual LayerManager* GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                                          LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                                          LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT) override;

    void SetCompositorWidgetDelegate(CompositorWidgetDelegate* delegate) override;

    void CleanLayerManagerRecursive();

    virtual int32_t RoundsWidgetCoordinatesTo() override;

    /**
     * |mIMContext| takes all IME related stuff.
     *
     * This is owned by the top-level nsWindow or the topmost child
     * nsWindow embedded in a non-Gecko widget.
     *
     * The instance is created when the top level widget is created.  And when
     * the widget is destroyed, it's released.  All child windows refer its
     * ancestor widget's instance.  So, one set of IM contexts is created for
     * all windows in a hierarchy.  If the children are released after the top
     * level window is released, the children still have a valid pointer,
     * however, IME doesn't work at that time.
     */
    RefPtr<mozilla::widget::IMContextWrapper> mIMContext;

    mozilla::UniquePtr<mozilla::CurrentX11TimeGetter> mCurrentTimeGetter;
    static CSDSupportLevel sCSDSupportLevel;
};

#endif /* __nsWindow_h__ */
