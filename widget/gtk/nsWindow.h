/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsWindow_h__
#define __nsWindow_h__

#include "mozilla/ipc/SharedMemorySysV.h"

#include "nsAutoPtr.h"

#include "mozcontainer.h"

#include "nsIDragService.h"
#include "nsITimer.h"
#include "nsGkAtoms.h"

#include "nsBaseWidget.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#ifdef MOZ_X11
#include <gdk/gdkx.h>
#endif /* MOZ_X11 */

#ifdef ACCESSIBILITY
#include "mozilla/a11y/Accessible.h"
#endif
#include "mozilla/EventForwards.h"

#include "IMContextWrapper.h"

#undef LOG
#ifdef MOZ_LOGGING

#include "mozilla/Logging.h"
#include "nsTArray.h"
#include "Units.h"

extern PRLogModuleInfo *gWidgetLog;
extern PRLogModuleInfo *gWidgetFocusLog;
extern PRLogModuleInfo *gWidgetDragLog;
extern PRLogModuleInfo *gWidgetDrawLog;

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

class gfxASurface;
class gfxPattern;
class nsPluginNativeWindowGtk;
#if defined(MOZ_X11) && defined(MOZ_HAVE_SHAREDMEMORYSYSV)
#  define MOZ_HAVE_SHMIMAGE
class nsShmImage;
#endif

namespace mozilla {
class TimeStamp;
class CurrentX11TimeGetter;
}

class nsWindow : public nsBaseWidget
{
public:
    nsWindow();

    static void ReleaseGlobals();

    NS_DECL_ISUPPORTS_INHERITED
    
    void CommonCreate(nsIWidget *aParent, bool aListenForResizes);
    
    virtual nsresult DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                                   nsEventStatus& aStatus) override;
    
    // called when we are destroyed
    virtual void OnDestroy(void) override;

    // called to check and see if a widget's dimensions are sane
    bool AreBoundsSane(void);

    // nsIWidget
    NS_IMETHOD         Create(nsIWidget        *aParent,
                              nsNativeWidget   aNativeParent,
                              const nsIntRect  &aRect,
                              nsWidgetInitData *aInitData) override;
    NS_IMETHOD         Destroy(void) override;
    virtual nsIWidget *GetParent() override;
    virtual float      GetDPI() override;
    virtual double     GetDefaultScaleInternal() override; 
    virtual nsresult   SetParent(nsIWidget* aNewParent) override;
    NS_IMETHOD         SetModal(bool aModal) override;
    virtual bool       IsVisible() const override;
    NS_IMETHOD         ConstrainPosition(bool aAllowSlop,
                                         int32_t *aX,
                                         int32_t *aY) override;
    virtual void       SetSizeConstraints(const SizeConstraints& aConstraints) override;
    NS_IMETHOD         Move(double aX,
                            double aY) override;
    NS_IMETHOD         Show             (bool aState) override;
    NS_IMETHOD         Resize           (double aWidth,
                                         double aHeight,
                                         bool   aRepaint) override;
    NS_IMETHOD         Resize           (double aX,
                                         double aY,
                                         double aWidth,
                                         double aHeight,
                                         bool   aRepaint) override;
    virtual bool       IsEnabled() const override;


    NS_IMETHOD         PlaceBehind(nsTopLevelWidgetZPlacement  aPlacement,
                                   nsIWidget                  *aWidget,
                                   bool                        aActivate) override;
    void               SetZIndex(int32_t aZIndex) override;
    NS_IMETHOD         SetSizeMode(nsSizeMode aMode) override;
    NS_IMETHOD         Enable(bool aState) override;
    NS_IMETHOD         SetFocus(bool aRaise = false) override;
    NS_IMETHOD         GetScreenBounds(nsIntRect &aRect) override;
    NS_IMETHOD         GetClientBounds(nsIntRect &aRect) override;
    virtual mozilla::gfx::IntSize GetClientSize() override;
    virtual nsIntPoint GetClientOffset() override;
    NS_IMETHOD         SetCursor(nsCursor aCursor) override;
    NS_IMETHOD         SetCursor(imgIContainer* aCursor,
                                 uint32_t aHotspotX, uint32_t aHotspotY) override;
    NS_IMETHOD         Invalidate(const nsIntRect &aRect) override;
    virtual void*      GetNativeData(uint32_t aDataType) override;
    void               SetNativeData(uint32_t aDataType, uintptr_t aVal) override;
    NS_IMETHOD         SetTitle(const nsAString& aTitle) override;
    NS_IMETHOD         SetIcon(const nsAString& aIconSpec) override;
    NS_IMETHOD         SetWindowClass(const nsAString& xulWinType) override;
    virtual mozilla::LayoutDeviceIntPoint WidgetToScreenOffset() override;
    NS_IMETHOD         EnableDragDrop(bool aEnable) override;
    NS_IMETHOD         CaptureMouse(bool aCapture) override;
    NS_IMETHOD         CaptureRollupEvents(nsIRollupListener *aListener,
                                           bool aDoCapture) override;
    NS_IMETHOD         GetAttention(int32_t aCycleCount) override;
    virtual nsresult   SetWindowClipRegion(const nsTArray<nsIntRect>& aRects,
                                           bool aIntersectWithExisting) override;
    virtual bool       HasPendingInputEvent() override;

    virtual bool PrepareForFullscreenTransition(nsISupports** aData) override;
    virtual void PerformFullscreenTransition(FullscreenTransitionStage aStage,
                                             uint16_t aDuration,
                                             nsISupports* aData,
                                             nsIRunnable* aCallback) override;
    NS_IMETHOD         MakeFullScreen(bool aFullScreen,
                                      nsIScreen* aTargetScreen = nullptr) override;
    NS_IMETHOD         HideWindowChrome(bool aShouldHide) override;

    /**
     * GetLastUserInputTime returns a timestamp for the most recent user input
     * event.  This is intended for pointer grab requests (including drags).
     */
    static guint32     GetLastUserInputTime();

    // utility method, -1 if no change should be made, otherwise returns a
    // value that can be passed to gdk_window_set_decorations
    gint               ConvertBorderStyles(nsBorderStyle aStyle);

    GdkRectangle DevicePixelsToGdkRectRoundOut(nsIntRect rect);

    // event callbacks
#if (MOZ_WIDGET_GTK == 2)
    gboolean           OnExposeEvent(GdkEventExpose *aEvent);
#else
    gboolean           OnExposeEvent(cairo_t *cr);
#endif
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

    virtual already_AddRefed<mozilla::gfx::DrawTarget>
                       StartRemoteDrawingInRegion(nsIntRegion& aInvalidRegion) override;
    virtual void       EndRemoteDrawingInRegion(mozilla::gfx::DrawTarget* aDrawTarget,
                                                nsIntRegion& aInvalidRegion) override;

private:
    void               UpdateAlpha(gfxPattern* aPattern, nsIntRect aBoundsRect);

    void               NativeMove();
    void               NativeResize();
    void               NativeMoveResize();

    void               NativeShow  (bool    aAction);
    void               SetHasMappedToplevel(bool aState);
    mozilla::LayoutDeviceIntSize GetSafeWindowSize(mozilla::LayoutDeviceIntSize aSize);

    void               EnsureGrabs  (void);
    void               GrabPointer  (guint32 aTime);
    void               ReleaseGrabs (void);

public:
    enum PluginType {
        PluginType_NONE = 0,   /* do not have any plugin */
        PluginType_XEMBED,     /* the plugin support xembed */
        PluginType_NONXEMBED   /* the plugin does not support xembed */
    };

    void               SetPluginType(PluginType aPluginType);
#ifdef MOZ_X11
    void               SetNonXEmbedPluginFocus(void);
    void               LoseNonXEmbedPluginFocus(void);
#endif /* MOZ_X11 */

    void               ThemeChanged(void);

#ifdef MOZ_X11
    Window             mOldFocusWindow;
#endif /* MOZ_X11 */

    static guint32     sLastButtonPressTime;

    NS_IMETHOD         BeginResizeDrag(mozilla::WidgetGUIEvent* aEvent,
                                       int32_t aHorizontal,
                                       int32_t aVertical) override;
    NS_IMETHOD         BeginMoveDrag(mozilla::WidgetMouseEvent* aEvent) override;

    MozContainer*      GetMozContainer() { return mContainer; }
    // GetMozContainerWidget returns the MozContainer even for undestroyed
    // descendant windows
    GtkWidget*         GetMozContainerWidget();
    GdkWindow*         GetGdkWindow() { return mGdkWindow; }
    bool               IsDestroyed() { return mIsDestroyed; }

    void               DispatchDragEvent(mozilla::EventMessage aMsg,
                                         const nsIntPoint& aRefPoint,
                                         guint aTime);
    static void        UpdateDragStatus (GdkDragContext *aDragContext,
                                         nsIDragService *aDragService);
    // If this dispatched the keydown event actually, this returns TRUE,
    // otherwise, FALSE.
    bool               DispatchKeyDownEvent(GdkEventKey *aEvent,
                                            bool *aIsCancelled);
    mozilla::TimeStamp GetEventTimeStamp(guint32 aEventTime);
    mozilla::CurrentX11TimeGetter* GetCurrentTimeGetter();

    NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                      const InputContextAction& aAction) override;
    NS_IMETHOD_(InputContext) GetInputContext() override;
    virtual nsIMEUpdatePreference GetIMEUpdatePreference() override;
    bool ExecuteNativeKeyBindingRemapped(
                        NativeKeyBindingsType aType,
                        const mozilla::WidgetKeyboardEvent& aEvent,
                        DoCommandCallback aCallback,
                        void* aCallbackData,
                        uint32_t aGeckoKeyCode,
                        uint32_t aNativeKeyCode);
    NS_IMETHOD_(bool) ExecuteNativeKeyBinding(
                        NativeKeyBindingsType aType,
                        const mozilla::WidgetKeyboardEvent& aEvent,
                        DoCommandCallback aCallback,
                        void* aCallbackData) override;
    NS_IMETHOD GetToggledKeyState(uint32_t aKeyCode,
                                  bool* aLEDState) override;

    // These methods are for toplevel windows only.
    void               ResizeTransparencyBitmap();
    void               ApplyTransparencyBitmap();
    void               ClearTransparencyBitmap();

   virtual void        SetTransparencyMode(nsTransparencyMode aMode) override;
   virtual nsTransparencyMode GetTransparencyMode() override;
   virtual nsresult    ConfigureChildren(const nsTArray<Configuration>& aConfigurations) override;
   nsresult            UpdateTranslucentWindowAlphaInternal(const nsIntRect& aRect,
                                                            uint8_t* aAlphas, int32_t aStride);
    virtual already_AddRefed<mozilla::gfx::DrawTarget> GetDrawTarget(const nsIntRegion& aRegion);

#if (MOZ_WIDGET_GTK == 2)
    static already_AddRefed<gfxASurface> GetSurfaceForGdkDrawable(GdkDrawable* aDrawable,
                                                                  const nsIntSize& aSize);
#endif
    NS_IMETHOD         ReparentNativeWidget(nsIWidget* aNewParent) override;

    virtual nsresult SynthesizeNativeMouseEvent(mozilla::LayoutDeviceIntPoint aPoint,
                                                uint32_t aNativeMessage,
                                                uint32_t aModifierFlags,
                                                nsIObserver* aObserver) override;

    virtual nsresult SynthesizeNativeMouseMove(mozilla::LayoutDeviceIntPoint aPoint,
                                               nsIObserver* aObserver) override
    { return SynthesizeNativeMouseEvent(aPoint, GDK_MOTION_NOTIFY, 0, aObserver); }

    virtual nsresult SynthesizeNativeMouseScrollEvent(mozilla::LayoutDeviceIntPoint aPoint,
                                                      uint32_t aNativeMessage,
                                                      double aDeltaX,
                                                      double aDeltaY,
                                                      double aDeltaZ,
                                                      uint32_t aModifierFlags,
                                                      uint32_t aAdditionalFlags,
                                                      nsIObserver* aObserver) override;

protected:
    virtual ~nsWindow();

    // event handling code
    void DispatchActivateEvent(void);
    void DispatchDeactivateEvent(void);
    void DispatchResized(int32_t aWidth, int32_t aHeight);

    // Helper for SetParent and ReparentNativeWidget.
    void ReparentNativeWidgetInternal(nsIWidget* aNewParent,
                                      GtkWidget* aNewContainer,
                                      GdkWindow* aNewParentWindow,
                                      GtkWidget* aOldContainer);

    virtual nsresult NotifyIMEInternal(
                         const IMENotification& aIMENotification) override;

    nsCOMPtr<nsIWidget> mParent;
    // Is this a toplevel window?
    bool                mIsTopLevel;
    // Has this widget been destroyed yet?
    bool                mIsDestroyed;

    // Should we send resize events on all resizes?
    bool                mListenForResizes;
    // This flag tracks if we're hidden or shown.
    bool                mIsShown;
    bool                mNeedsShow;
    // is this widget enabled?
    bool                mEnabled;
    // has the native window for this been created yet?
    bool                mCreated;

private:
    void               DestroyChildWindows();
    GtkWidget         *GetToplevelWidget();
    nsWindow          *GetContainerWindow();
    void               SetUrgencyHint(GtkWidget *top_window, bool state);
    void              *SetupPluginPort(void);
    void               SetDefaultIcon(void);
    void               InitButtonEvent(mozilla::WidgetMouseEvent& aEvent,
                                       GdkEventButton* aGdkEvent);
    bool               DispatchCommandEvent(nsIAtom* aCommand);
    bool               DispatchContentCommandEvent(mozilla::EventMessage aMsg);
    bool               CheckForRollup(gdouble aMouseX, gdouble aMouseY,
                                      bool aIsWheel, bool aAlwaysRollup);
    bool               GetDragInfo(mozilla::WidgetMouseEvent* aMouseEvent,
                                   GdkWindow** aWindow, gint* aButton,
                                   gint* aRootX, gint* aRootY);
    void               ClearCachedResources();

    GtkWidget          *mShell;
    MozContainer       *mContainer;
    GdkWindow          *mGdkWindow;

    uint32_t            mHasMappedToplevel : 1,
                        mIsFullyObscured : 1,
                        mRetryPointerGrab : 1;
    nsSizeMode          mSizeState;
    PluginType          mPluginType;

    int32_t             mTransparencyBitmapWidth;
    int32_t             mTransparencyBitmapHeight;

#if GTK_CHECK_VERSION(3,4,0)
    // This field omits duplicate scroll events caused by GNOME bug 726878.
    guint32             mLastScrollEventTime;
#endif

#ifdef MOZ_X11
    Display*            mXDisplay;
    Drawable            mXWindow;
    Visual*             mXVisual;
    int                 mXDepth;
#endif

#ifdef MOZ_HAVE_SHMIMAGE
    // If we're using xshm rendering
    nsRefPtr<nsShmImage>  mShmImage;
#endif

#ifdef ACCESSIBILITY
    nsRefPtr<mozilla::a11y::Accessible> mRootAccessible;

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

    // Updates the bounds of the socket widget we manage for remote plugins.
    void ResizePluginSocketWidget();

    // e10s specific - for managing the socket widget this window hosts.
    nsPluginNativeWindowGtk* mPluginNativeWindow;

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
                                          LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                                          bool* aAllowRetaining = nullptr) override;

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
    nsRefPtr<mozilla::widget::IMContextWrapper> mIMContext;

    nsAutoPtr<mozilla::CurrentX11TimeGetter> mCurrentTimeGetter;

    // HiDPI scale conversion
    gint GdkScaleFactor();

    // To GDK
    gint DevicePixelsToGdkCoordRoundUp(int pixels);
    gint DevicePixelsToGdkCoordRoundDown(int pixels);
    GdkPoint DevicePixelsToGdkPointRoundDown(nsIntPoint point);
    GdkRectangle DevicePixelsToGdkSizeRoundUp(nsIntSize pixelSize);

    // From GDK
    int GdkCoordToDevicePixels(gint coord);
    mozilla::LayoutDeviceIntPoint GdkPointToDevicePixels(GdkPoint point);
    nsIntRect GdkRectToDevicePixels(GdkRectangle rect);
};

class nsChildWindow : public nsWindow {
public:
    nsChildWindow();
    ~nsChildWindow();
};

#endif /* __nsWindow_h__ */
