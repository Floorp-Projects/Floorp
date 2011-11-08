/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2001 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __nsWindow_h__
#define __nsWindow_h__

#include "mozilla/ipc/SharedMemorySysV.h"

#include "nsAutoPtr.h"

#include "mozcontainer.h"
#include "nsWeakReference.h"

#include "nsIDragService.h"
#include "nsITimer.h"
#include "nsGkAtoms.h"

#include "gfxASurface.h"

#include "nsBaseWidget.h"
#include "nsGUIEvent.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#ifdef MOZ_DFB
#include <gdk/gdkdirectfb.h>
#endif /* MOZ_DFB */

#ifdef MOZ_X11
#include <gdk/gdkx.h>
#endif /* MOZ_X11 */

#ifdef ACCESSIBILITY
#include "nsAccessible.h"
#endif

#include "nsGtkIMModule.h"

#ifdef MOZ_LOGGING

// make sure that logging is enabled before including prlog.h
#define FORCE_PR_LOG

#include "prlog.h"
#include "nsTArray.h"

extern PRLogModuleInfo *gWidgetLog;
extern PRLogModuleInfo *gWidgetFocusLog;
extern PRLogModuleInfo *gWidgetDragLog;
extern PRLogModuleInfo *gWidgetDrawLog;

#define LOG(args) PR_LOG(gWidgetLog, 4, args)
#define LOGFOCUS(args) PR_LOG(gWidgetFocusLog, 4, args)
#define LOGDRAG(args) PR_LOG(gWidgetDragLog, 4, args)
#define LOGDRAW(args) PR_LOG(gWidgetDrawLog, 4, args)

#else

#define LOG(args)
#define LOGFOCUS(args)
#define LOGDRAG(args)
#define LOGDRAW(args)

#endif /* MOZ_LOGGING */

#if defined(MOZ_X11) && defined(MOZ_HAVE_SHAREDMEMORYSYSV)
#  define MOZ_HAVE_SHMIMAGE

class nsShmImage;
#endif

class nsWindow : public nsBaseWidget, public nsSupportsWeakReference
{
public:
    nsWindow();
    virtual ~nsWindow();

    static void ReleaseGlobals();

    NS_DECL_ISUPPORTS_INHERITED
    
    void CommonCreate(nsIWidget *aParent, bool aListenForResizes);
    
    // event handling code
    void InitKeyEvent(nsKeyEvent &aEvent, GdkEventKey *aGdkEvent);

    void DispatchActivateEvent(void);
    void DispatchDeactivateEvent(void);
    void DispatchResizeEvent(nsIntRect &aRect, nsEventStatus &aStatus);

    virtual nsresult DispatchEvent(nsGUIEvent *aEvent, nsEventStatus &aStatus);
    
    // called when we are destroyed
    void OnDestroy(void);

    // called to check and see if a widget's dimensions are sane
    bool AreBoundsSane(void);

    // nsIWidget
    NS_IMETHOD         Create(nsIWidget        *aParent,
                              nsNativeWidget   aNativeParent,
                              const nsIntRect  &aRect,
                              EVENT_CALLBACK   aHandleEventFunction,
                              nsDeviceContext *aContext,
                              nsWidgetInitData *aInitData);
    NS_IMETHOD         Destroy(void);
    virtual nsIWidget *GetParent();
    virtual float      GetDPI();
    virtual nsresult   SetParent(nsIWidget* aNewParent);
    NS_IMETHOD         SetModal(bool aModal);
    NS_IMETHOD         IsVisible(bool & aState);
    NS_IMETHOD         ConstrainPosition(bool aAllowSlop,
                                         PRInt32 *aX,
                                         PRInt32 *aY);
    NS_IMETHOD         Move(PRInt32 aX,
                            PRInt32 aY);
    NS_IMETHOD         Show             (bool aState);
    NS_IMETHOD         Resize           (PRInt32 aWidth,
                                         PRInt32 aHeight,
                                         bool    aRepaint);
    NS_IMETHOD         Resize           (PRInt32 aX,
                                         PRInt32 aY,
                                         PRInt32 aWidth,
                                         PRInt32 aHeight,
                                         bool     aRepaint);
    NS_IMETHOD         IsEnabled        (bool *aState);


    NS_IMETHOD         PlaceBehind(nsTopLevelWidgetZPlacement  aPlacement,
                                   nsIWidget                  *aWidget,
                                   bool                        aActivate);
    NS_IMETHOD         SetZIndex(PRInt32 aZIndex);
    NS_IMETHOD         SetSizeMode(PRInt32 aMode);
    NS_IMETHOD         Enable(bool aState);
    NS_IMETHOD         SetFocus(bool aRaise = false);
    NS_IMETHOD         GetScreenBounds(nsIntRect &aRect);
    NS_IMETHOD         SetForegroundColor(const nscolor &aColor);
    NS_IMETHOD         SetBackgroundColor(const nscolor &aColor);
    NS_IMETHOD         SetCursor(nsCursor aCursor);
    NS_IMETHOD         SetCursor(imgIContainer* aCursor,
                                 PRUint32 aHotspotX, PRUint32 aHotspotY);
    NS_IMETHOD         Invalidate(const nsIntRect &aRect,
                                  bool             aIsSynchronous);
    NS_IMETHOD         Update();
    virtual void*      GetNativeData(PRUint32 aDataType);
    NS_IMETHOD         SetTitle(const nsAString& aTitle);
    NS_IMETHOD         SetIcon(const nsAString& aIconSpec);
    NS_IMETHOD         SetWindowClass(const nsAString& xulWinType);
    virtual nsIntPoint WidgetToScreenOffset();
    NS_IMETHOD         EnableDragDrop(bool aEnable);
    NS_IMETHOD         CaptureMouse(bool aCapture);
    NS_IMETHOD         CaptureRollupEvents(nsIRollupListener *aListener,
                                           bool aDoCapture,
                                           bool aConsumeRollupEvent);
    NS_IMETHOD         GetAttention(PRInt32 aCycleCount);

    virtual bool       HasPendingInputEvent();

    NS_IMETHOD         MakeFullScreen(bool aFullScreen);
    NS_IMETHOD         HideWindowChrome(bool aShouldHide);

    // utility method, -1 if no change should be made, otherwise returns a
    // value that can be passed to gdk_window_set_decorations
    gint               ConvertBorderStyles(nsBorderStyle aStyle);

    // event callbacks
#if defined(MOZ_WIDGET_GTK2)
    gboolean           OnExposeEvent(GdkEventExpose *aEvent);
#else
    gboolean           OnExposeEvent(cairo_t *cr);
#endif
    gboolean           OnConfigureEvent(GtkWidget *aWidget,
                                        GdkEventConfigure *aEvent);
    void               OnContainerUnrealize(GtkWidget *aWidget);
    void               OnSizeAllocate(GtkWidget *aWidget,
                                      GtkAllocation *aAllocation);
    void               OnDeleteEvent(GtkWidget *aWidget,
                                     GdkEventAny *aEvent);
    void               OnEnterNotifyEvent(GtkWidget *aWidget,
                                          GdkEventCrossing *aEvent);
    void               OnLeaveNotifyEvent(GtkWidget *aWidget,
                                          GdkEventCrossing *aEvent);
    void               OnMotionNotifyEvent(GtkWidget *aWidget,
                                           GdkEventMotion *aEvent);
    void               OnButtonPressEvent(GtkWidget *aWidget,
                                          GdkEventButton *aEvent);
    void               OnButtonReleaseEvent(GtkWidget *aWidget,
                                            GdkEventButton *aEvent);
    void               OnContainerFocusInEvent(GtkWidget *aWidget,
                                               GdkEventFocus *aEvent);
    void               OnContainerFocusOutEvent(GtkWidget *aWidget,
                                                GdkEventFocus *aEvent);
    gboolean           OnKeyPressEvent(GtkWidget *aWidget,
                                       GdkEventKey *aEvent);
    gboolean           OnKeyReleaseEvent(GtkWidget *aWidget,
                                         GdkEventKey *aEvent);
    void               OnScrollEvent(GtkWidget *aWidget,
                                     GdkEventScroll *aEvent);
    void               OnVisibilityNotifyEvent(GtkWidget *aWidget,
                                               GdkEventVisibility *aEvent);
    void               OnWindowStateEvent(GtkWidget *aWidget,
                                          GdkEventWindowState *aEvent);
    gboolean           OnDragMotionEvent(GtkWidget       *aWidget,
                                         GdkDragContext  *aDragContext,
                                         gint             aX,
                                         gint             aY,
                                         guint            aTime,
                                         gpointer         aData);
    void               OnDragLeaveEvent(GtkWidget *      aWidget,
                                        GdkDragContext   *aDragContext,
                                        guint            aTime,
                                        gpointer         aData);
    gboolean           OnDragDropEvent(GtkWidget        *aWidget,
                                       GdkDragContext   *aDragContext,
                                       gint             aX,
                                       gint             aY,
                                       guint            aTime,
                                       gpointer         aData);
    void               OnDragDataReceivedEvent(GtkWidget       *aWidget,
                                               GdkDragContext  *aDragContext,
                                               gint             aX,
                                               gint             aY,
                                               GtkSelectionData*aSelectionData,
                                               guint            aInfo,
                                               guint            aTime,
                                               gpointer         aData);
    void               OnDragLeave(void);
    void               OnDragEnter(nscoord aX, nscoord aY);

    virtual void       NativeResize(PRInt32 aWidth,
                                    PRInt32 aHeight,
                                    bool    aRepaint);

    virtual void       NativeResize(PRInt32 aX,
                                    PRInt32 aY,
                                    PRInt32 aWidth,
                                    PRInt32 aHeight,
                                    bool    aRepaint);

    virtual void       NativeShow  (bool    aAction);
    void               SetHasMappedToplevel(bool aState);
    nsIntSize          GetSafeWindowSize(nsIntSize aSize);

    void               EnsureGrabs  (void);
    void               GrabPointer  (void);
    void               ReleaseGrabs (void);

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

    void CheckNeedDragLeaveEnter(nsWindow* aInnerMostWidget,
                                 nsIDragService* aDragService,
                                 GdkDragContext *aDragContext,
                                 nscoord aX, nscoord aY);

#ifdef MOZ_X11
    Window             mOldFocusWindow;
#endif /* MOZ_X11 */

    static guint32     sLastButtonPressTime;
    static guint32     sLastButtonReleaseTime;

    NS_IMETHOD         BeginResizeDrag(nsGUIEvent* aEvent, PRInt32 aHorizontal, PRInt32 aVertical);
    NS_IMETHOD         BeginMoveDrag(nsMouseEvent* aEvent);

    MozContainer*      GetMozContainer() { return mContainer; }
    GdkWindow*         GetGdkWindow() { return mGdkWindow; }
    bool               IsDestroyed() { return mIsDestroyed; }

    // If this dispatched the keydown event actually, this returns TRUE,
    // otherwise, FALSE.
    bool               DispatchKeyDownEvent(GdkEventKey *aEvent,
                                            bool *aIsCancelled);

    NS_IMETHOD ResetInputState();
    NS_IMETHOD SetInputMode(const IMEContext& aContext);
    NS_IMETHOD GetInputMode(IMEContext& aContext);
    NS_IMETHOD CancelIMEComposition();
    NS_IMETHOD OnIMEFocusChange(bool aFocus);
    NS_IMETHOD GetToggledKeyState(PRUint32 aKeyCode, bool* aLEDState);

   void                ResizeTransparencyBitmap(PRInt32 aNewWidth, PRInt32 aNewHeight);
   void                ApplyTransparencyBitmap();
   virtual void        SetTransparencyMode(nsTransparencyMode aMode);
   virtual nsTransparencyMode GetTransparencyMode();
   virtual nsresult    ConfigureChildren(const nsTArray<Configuration>& aConfigurations);
   nsresult            UpdateTranslucentWindowAlphaInternal(const nsIntRect& aRect,
                                                            PRUint8* aAlphas, PRInt32 aStride);

#if defined(MOZ_WIDGET_GTK2)
    gfxASurface       *GetThebesSurface();

    static already_AddRefed<gfxASurface> GetSurfaceForGdkDrawable(GdkDrawable* aDrawable,
                                                                  const nsIntSize& aSize);
#else
    gfxASurface       *GetThebesSurface(cairo_t *cr);
#endif
    NS_IMETHOD         ReparentNativeWidget(nsIWidget* aNewParent);

#ifdef ACCESSIBILITY
    static bool        sAccessibilityEnabled;
#endif
protected:
    // Helper for SetParent and ReparentNativeWidget.
    void ReparentNativeWidgetInternal(nsIWidget* aNewParent,
                                      GtkWidget* aNewContainer,
                                      GdkWindow* aNewParentWindow,
                                      GtkWidget* aOldContainer);
    nsCOMPtr<nsIWidget> mParent;
    // Is this a toplevel window?
    bool                mIsTopLevel;
    // Has this widget been destroyed yet?
    bool                mIsDestroyed;

    // This is a flag that tracks if we need to resize a widget or
    // window when we show it.
    bool                mNeedsResize;
    // This is a flag that tracks if we need to move a widget or
    // window when we show it.
    bool                mNeedsMove;
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
    void               GetToplevelWidget(GtkWidget **aWidget);
    GtkWidget         *GetMozContainerWidget();
    nsWindow          *GetContainerWindow();
    void               SetUrgencyHint(GtkWidget *top_window, bool state);
    void              *SetupPluginPort(void);
    nsresult           SetWindowIconList(const nsTArray<nsCString> &aIconList);
    void               SetDefaultIcon(void);
    void               InitButtonEvent(nsMouseEvent &aEvent, GdkEventButton *aGdkEvent);
    bool               DispatchCommandEvent(nsIAtom* aCommand);
    bool               DispatchContentCommandEvent(PRInt32 aMsg);
    void               SetWindowClipRegion(const nsTArray<nsIntRect>& aRects,
                                           bool aIntersectWithExisting);
    bool               GetDragInfo(nsMouseEvent* aMouseEvent,
                                   GdkWindow** aWindow, gint* aButton,
                                   gint* aRootX, gint* aRootY);
    void               ClearCachedResources();

    GtkWidget          *mShell;
    MozContainer       *mContainer;
    GdkWindow          *mGdkWindow;

    GtkWindowGroup     *mWindowGroup;

    PRUint32            mHasMappedToplevel : 1,
                        mIsFullyObscured : 1,
                        mRetryPointerGrab : 1;
    GtkWindow          *mTransientParent;
    PRInt32             mSizeState;
    PluginType          mPluginType;

    PRInt32             mTransparencyBitmapWidth;
    PRInt32             mTransparencyBitmapHeight;

#ifdef MOZ_HAVE_SHMIMAGE
    // If we're using xshm rendering, mThebesSurface wraps mShmImage
    nsRefPtr<nsShmImage>  mShmImage;
#endif
    nsRefPtr<gfxASurface> mThebesSurface;

#ifdef MOZ_DFB
    int                    mDFBCursorX;
    int                    mDFBCursorY;
    PRUint32               mDFBCursorCount;
    IDirectFB             *mDFB;
    IDirectFBDisplayLayer *mDFBLayer;
#endif

#ifdef ACCESSIBILITY
    nsRefPtr<nsAccessible> mRootAccessible;

    /**
     * Request to create the accessible for this window if it is top level.
     */
    void                CreateRootAccessible();

    /**
     * Generate the NS_GETACCESSIBLE event to get accessible for this window
     * and return it.
     */
    nsAccessible       *DispatchAccessibleEvent();

    /**
     * Dispatch accessible event for the top level window accessible.
     *
     * @param  aEventType  [in] the accessible event type to dispatch
     */
    void                DispatchEventToRootAccessible(PRUint32 aEventType);

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
    // this is the last window that had a drag event happen on it.
    static nsWindow    *sLastDragMotionWindow;
    void   InitDragEvent         (nsDragEvent &aEvent);
    void   UpdateDragStatus      (GdkDragContext *aDragContext,
                                  nsIDragService *aDragService);

    nsCOMPtr<nsITimer> mDragLeaveTimer;
    float              mLastMotionPressure;

    // Remember the last sizemode so that we can restore it when
    // leaving fullscreen
    nsSizeMode         mLastSizeMode;

    static bool        sIsDraggingOutOf;
    // drag in progress
    static bool DragInProgress(void);

    void         FireDragLeaveTimer       (void);
    static void  DragLeaveTimerCallback  (nsITimer *aTimer, void *aClosure);

    void DispatchMissedButtonReleases(GdkEventCrossing *aGdkEvent);

    /**
     * |mIMModule| takes all IME related stuff.
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
    nsRefPtr<nsGtkIMModule> mIMModule;
};

class nsChildWindow : public nsWindow {
public:
    nsChildWindow();
    ~nsChildWindow();
};

#endif /* __nsWindow_h__ */
