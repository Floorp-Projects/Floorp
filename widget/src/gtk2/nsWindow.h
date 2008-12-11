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

#include "nsAutoPtr.h"

#include "mozcontainer.h"
#include "mozdrawingarea.h"
#include "nsWeakReference.h"

#include "nsIDragService.h"
#include "nsITimer.h"
#include "nsWidgetAtoms.h"

#include "gfxASurface.h"

#include "nsBaseWidget.h"
#include "nsGUIEvent.h"
#include <gdk/gdkevents.h>
#include <gtk/gtk.h>

#ifdef MOZ_DFB
#include <gdk/gdkdirectfb.h>
#endif /* MOZ_DFB */

#ifdef MOZ_X11
#include <gdk/gdkx.h>
#endif /* MOZ_X11 */
#include <gtk/gtkwindow.h>

#ifdef ACCESSIBILITY
#include "nsIAccessNode.h"
#include "nsIAccessible.h"
#endif

#ifdef USE_XIM
#include <gtk/gtkimmulticontext.h>
#include "pldhash.h"
#endif

#ifdef MOZ_LOGGING

// make sure that logging is enabled before including prlog.h
#define FORCE_PR_LOG

#include "prlog.h"

extern PRLogModuleInfo *gWidgetLog;
extern PRLogModuleInfo *gWidgetFocusLog;
extern PRLogModuleInfo *gWidgetIMLog;
extern PRLogModuleInfo *gWidgetDrawLog;

#define LOG(args) PR_LOG(gWidgetLog, 4, args)
#define LOGFOCUS(args) PR_LOG(gWidgetFocusLog, 4, args)
#define LOGIM(args) PR_LOG(gWidgetIMLog, 4, args)
#define LOGDRAW(args) PR_LOG(gWidgetDrawLog, 4, args)

#else

#define LOG(args)
#define LOGFOCUS(args)
#define LOGIM(args)
#define LOGDRAW(args)

#endif /* MOZ_LOGGING */


class nsWindow : public nsBaseWidget, public nsSupportsWeakReference
{
public:
    nsWindow();
    virtual ~nsWindow();

    static void ReleaseGlobals();

    NS_DECL_ISUPPORTS_INHERITED
    
    void CommonCreate(nsIWidget *aParent, PRBool aListenForResizes);
    
    // event handling code
    void InitKeyEvent(nsKeyEvent &aEvent, GdkEventKey *aGdkEvent);

    void DispatchGotFocusEvent(void);
    void DispatchLostFocusEvent(void);
    void DispatchActivateEvent(void);
    void DispatchDeactivateEvent(void);
    void DispatchResizeEvent(nsRect &aRect, nsEventStatus &aStatus);

    virtual nsresult DispatchEvent(nsGUIEvent *aEvent, nsEventStatus &aStatus);
    
    // called when we are destroyed
    void OnDestroy(void);

    // called to check and see if a widget's dimensions are sane
    PRBool AreBoundsSane(void);

    // nsIWidget
    NS_IMETHOD         Create(nsIWidget        *aParent,
                              const nsRect     &aRect,
                              EVENT_CALLBACK   aHandleEventFunction,
                              nsIDeviceContext *aContext,
                              nsIAppShell      *aAppShell,
                              nsIToolkit       *aToolkit,
                              nsWidgetInitData *aInitData);
    NS_IMETHOD         Create(nsNativeWidget aParent,
                              const nsRect     &aRect,
                              EVENT_CALLBACK   aHandleEventFunction,
                              nsIDeviceContext *aContext,
                              nsIAppShell      *aAppShell,
                              nsIToolkit       *aToolkit,
                              nsWidgetInitData *aInitData);
    NS_IMETHOD         Destroy(void);
    virtual nsIWidget *GetParent();
    virtual nsresult   SetParent(nsIWidget* aNewParent);
    NS_IMETHOD         SetModal(PRBool aModal);
    NS_IMETHOD         IsVisible(PRBool & aState);
    NS_IMETHOD         ConstrainPosition(PRBool aAllowSlop,
                                         PRInt32 *aX,
                                         PRInt32 *aY);
    NS_IMETHOD         Move(PRInt32 aX,
                            PRInt32 aY);
    NS_IMETHOD         Show             (PRBool aState);
    NS_IMETHOD         Resize           (PRInt32 aWidth,
                                         PRInt32 aHeight,
                                         PRBool  aRepaint);
    NS_IMETHOD         Resize           (PRInt32 aX,
                                         PRInt32 aY,
                                         PRInt32 aWidth,
                                         PRInt32 aHeight,
                                         PRBool   aRepaint);
    NS_IMETHOD         GetPreferredSize (PRInt32 &aWidth,
                                         PRInt32 &aHeight);
    NS_IMETHOD         SetPreferredSize (PRInt32 aWidth,
                                         PRInt32 aHeight);
    NS_IMETHOD         IsEnabled        (PRBool *aState);


    NS_IMETHOD         PlaceBehind(nsTopLevelWidgetZPlacement  aPlacement,
                                   nsIWidget                  *aWidget,
                                   PRBool                      aActivate);
    NS_IMETHOD         SetZIndex(PRInt32 aZIndex);
    NS_IMETHOD         SetSizeMode(PRInt32 aMode);
    NS_IMETHOD         Enable(PRBool aState);
    NS_IMETHOD         SetFocus(PRBool aRaise = PR_FALSE);
    NS_IMETHOD         GetScreenBounds(nsRect &aRect);
    NS_IMETHOD         SetForegroundColor(const nscolor &aColor);
    NS_IMETHOD         SetBackgroundColor(const nscolor &aColor);
    NS_IMETHOD         SetCursor(nsCursor aCursor);
    NS_IMETHOD         SetCursor(imgIContainer* aCursor,
                                 PRUint32 aHotspotX, PRUint32 aHotspotY);
    NS_IMETHOD         Validate();
    NS_IMETHOD         Invalidate(PRBool aIsSynchronous);
    NS_IMETHOD         Invalidate(const nsRect &aRect,
                                  PRBool        aIsSynchronous);
    NS_IMETHOD         InvalidateRegion(const nsIRegion *aRegion,
                                        PRBool           aIsSynchronous);
    NS_IMETHOD         Update();
    NS_IMETHOD         SetColorMap(nsColorMap *aColorMap);
    NS_IMETHOD         Scroll(PRInt32  aDx,
                              PRInt32  aDy,
                              nsRect  *aClipRect);
    NS_IMETHOD         ScrollWidgets(PRInt32 aDx,
                                     PRInt32 aDy);
    NS_IMETHOD         ScrollRect(nsRect  &aSrcRect,
                                  PRInt32  aDx,
                                  PRInt32  aDy);
    virtual void*      GetNativeData(PRUint32 aDataType);
    NS_IMETHOD         SetBorderStyle(nsBorderStyle aBorderStyle);
    NS_IMETHOD         SetTitle(const nsAString& aTitle);
    NS_IMETHOD         SetIcon(const nsAString& aIconSpec);
    NS_IMETHOD         SetWindowClass(const nsAString& xulWinType);
    NS_IMETHOD         SetMenuBar(void * aMenuBar);
    NS_IMETHOD         ShowMenuBar(PRBool aShow);
    NS_IMETHOD         WidgetToScreen(const nsRect& aOldRect,
                                      nsRect& aNewRect);
    NS_IMETHOD         ScreenToWidget(const nsRect& aOldRect,
                                      nsRect& aNewRect);
    NS_IMETHOD         BeginResizingChildren(void);
    NS_IMETHOD         EndResizingChildren(void);
    NS_IMETHOD         EnableDragDrop(PRBool aEnable);
    virtual void       ConvertToDeviceCoordinates(nscoord &aX,
                                                  nscoord &aY);
    NS_IMETHOD         PreCreateWidget(nsWidgetInitData *aWidgetInitData);
    NS_IMETHOD         CaptureMouse(PRBool aCapture);
    NS_IMETHOD         CaptureRollupEvents(nsIRollupListener *aListener,
                                           PRBool aDoCapture,
                                           PRBool aConsumeRollupEvent);
    NS_IMETHOD         GetAttention(PRInt32 aCycleCount);
    NS_IMETHOD         MakeFullScreen(PRBool aFullScreen);
    NS_IMETHOD         HideWindowChrome(PRBool aShouldHide);

    // utility methods
    void               LoseFocus();
    gint               ConvertBorderStyles(nsBorderStyle aStyle);

    // event callbacks
    gboolean           OnExposeEvent(GtkWidget *aWidget,
                                     GdkEventExpose *aEvent);
    gboolean           OnConfigureEvent(GtkWidget *aWidget,
                                        GdkEventConfigure *aEvent);
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
                                         void            *aData);
    void               OnDragLeaveEvent(GtkWidget *      aWidget,
                                        GdkDragContext   *aDragContext,
                                        guint            aTime,
                                        gpointer         aData);
    gboolean           OnDragDropEvent(GtkWidget        *aWidget,
                                       GdkDragContext   *aDragContext,
                                       gint             aX,
                                       gint             aY,
                                       guint            aTime,
                                       gpointer         *aData);
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


    nsresult           NativeCreate(nsIWidget        *aParent,
                                    nsNativeWidget    aNativeParent,
                                    const nsRect     &aRect,
                                    EVENT_CALLBACK    aHandleEventFunction,
                                    nsIDeviceContext *aContext,
                                    nsIAppShell      *aAppShell,
                                    nsIToolkit       *aToolkit,
                                    nsWidgetInitData *aInitData);

    virtual void       NativeResize(PRInt32 aWidth,
                                    PRInt32 aHeight,
                                    PRBool  aRepaint);

    virtual void       NativeResize(PRInt32 aX,
                                    PRInt32 aY,
                                    PRInt32 aWidth,
                                    PRInt32 aHeight,
                                    PRBool  aRepaint);

    virtual void       NativeShow  (PRBool  aAction);
    virtual nsSize     GetSafeWindowSize(nsSize aSize);

    void               EnsureGrabs  (void);
    void               GrabPointer  (void);
    void               GrabKeyboard (void);
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

#ifdef MOZ_X11
    Window             mOldFocusWindow;
#endif /* MOZ_X11 */

    static guint32     mLastButtonPressTime;
    static guint32     mLastButtonReleaseTime;

    NS_IMETHOD         BeginResizeDrag   (nsGUIEvent* aEvent, PRInt32 aHorizontal, PRInt32 aVertical);

#ifdef USE_XIM
    void               IMEInitData       (void);
    void               IMEReleaseData    (void);
    void               IMEDestroyContext (void);
    void               IMESetFocus       (void);
    void               IMELoseFocus      (void);
    void               IMEComposeStart   (void);
    void               IMEComposeText    (const PRUnichar *aText,
                                          const PRInt32 aLen,
                                          const gchar *aPreeditString,
                                          const gint aCursorPos,
                                          const PangoAttrList *aFeedback);
    void               IMEComposeEnd     (void);
    GtkIMContext*      IMEGetContext     (void);
    nsWindow*          IMEGetOwningWindow(void);
    // "Enabled" means the users can use all IMEs.
    // I.e., the focus is in the normal editors.
    PRBool             IMEIsEnabledState (void);
    // "Editable" means the users can input characters. They may be not able to
    // use IMEs but they can use dead keys.
    // I.e., the forcus is in the normal editors or the password editors or
    // the |ime-mode: disabled;| editors.
    PRBool             IMEIsEditableState(void);
    nsWindow*          IMEComposingWindow(void);
    void               IMECreateContext  (void);
    PRBool             IMEFilterEvent    (GdkEventKey *aEvent);
    void               IMESetCursorPosition(const nsTextEventReply& aReply);

    /*
     *  |mIMEData| has all IME data for the window and its children widgets.
     *  Only stand-alone windows and child windows embedded in non-Mozilla GTK
     *  containers own IME contexts.
     *  But this is referred from all children after the widget gets focus.
     *  The children refers to its owning window's object.
     */
    struct nsIMEData {
        // Actual context. This is used for handling the user's input.
        GtkIMContext       *mContext;
        // mSimpleContext is used for the password field and
        // the |ime-mode: disabled;| editors. These editors disable IME.
        // But dead keys should work. Fortunately, the simple IM context of
        // GTK2 support only them.
        GtkIMContext       *mSimpleContext;
        // mDummyContext is a dummy context and will be used in IMESetFocus()
        // when mEnabled is false. This mDummyContext IM state is always
        // "off", so it works to switch conversion mode to OFF on IM status
        // window.
        GtkIMContext       *mDummyContext;
        // This mComposingWindow is set in IMEComposeStart(), when user starts
        // composition, then unset in IMEComposeEnd() when user ends the
        // composition. We will keep the widget where the actual composition is
        // started. During the composition, we may get some events like
        // ResetInputStateInternal() and CancelIMECompositionInternal() by
        // changing input focus, we will use the original widget of
        // mComposingWindow to commit or reset the composition.
        nsWindow           *mComposingWindow;
        // Owner of this struct.
        // The owner window must release the contexts at destroying.
        nsWindow           *mOwner;
        // The reference counter. When this will be zero by the decrement,
        // the decrementer must free the instance.
        PRUint32           mRefCount;
        // IME enabled state in this window.
        PRUint32           mEnabled;
        nsIMEData(nsWindow* aOwner) {
            mContext         = nsnull;
            mSimpleContext   = nsnull;
            mDummyContext    = nsnull;
            mComposingWindow = nsnull;
            mOwner           = aOwner;
            mRefCount        = 1;
            mEnabled         = nsIWidget::IME_STATUS_ENABLED;
        }
    };
    nsIMEData          *mIMEData;

    NS_IMETHOD ResetInputState();
    NS_IMETHOD SetIMEOpenState(PRBool aState);
    NS_IMETHOD GetIMEOpenState(PRBool* aState);
    NS_IMETHOD SetIMEEnabled(PRUint32 aState);
    NS_IMETHOD GetIMEEnabled(PRUint32* aState);
    NS_IMETHOD CancelIMEComposition();
    NS_IMETHOD GetToggledKeyState(PRUint32 aKeyCode, PRBool* aLEDState);

#endif

   void                ResizeTransparencyBitmap(PRInt32 aNewWidth, PRInt32 aNewHeight);
   void                ApplyTransparencyBitmap();
   virtual void        SetTransparencyMode(nsTransparencyMode aMode);
   virtual nsTransparencyMode GetTransparencyMode();
   nsresult            UpdateTranslucentWindowAlphaInternal(const nsRect& aRect,
                                                            PRUint8* aAlphas, PRInt32 aStride);

    gfxASurface       *GetThebesSurface();

    static already_AddRefed<gfxASurface> GetSurfaceForGdkDrawable(GdkDrawable* aDrawable,
                                                                  const nsSize& aSize);

#ifdef ACCESSIBILITY
    static PRBool      sAccessibilityEnabled;
#endif
protected:
    nsCOMPtr<nsIWidget> mParent;
    // Is this a toplevel window?
    PRPackedBool        mIsTopLevel;
    // Has this widget been destroyed yet?
    PRPackedBool        mIsDestroyed;

    // This is a flag that tracks if we need to resize a widget or
    // window when we show it.
    PRPackedBool        mNeedsResize;
    // This is a flag that tracks if we need to move a widget or
    // window when we show it.
    PRPackedBool        mNeedsMove;
    // Should we send resize events on all resizes?
    PRPackedBool        mListenForResizes;
    // This flag tracks if we're hidden or shown.
    PRPackedBool        mIsShown;
    PRPackedBool        mNeedsShow;
    // is this widget enabled?
    PRPackedBool        mEnabled;
    // has the native window for this been created yet?
    PRPackedBool        mCreated;
    // Has anyone set an x/y location for this widget yet? Toplevels
    // shouldn't be automatically set to 0,0 for first show.
    PRPackedBool        mPlaced;

    // Preferred sizes
    PRUint32            mPreferredWidth;
    PRUint32            mPreferredHeight;

private:
    void               GetToplevelWidget(GtkWidget **aWidget);
    GtkWidget         *GetMozContainerWidget();
    void               GetContainerWindow(nsWindow  **aWindow);
    void               SetUrgencyHint(GtkWidget *top_window, PRBool state);
    void              *SetupPluginPort(void);
    nsresult           SetWindowIconList(const nsCStringArray &aIconList);
    void               SetDefaultIcon(void);
    void               InitButtonEvent(nsMouseEvent &aEvent, GdkEventButton *aGdkEvent);
    PRBool             DispatchCommandEvent(nsIAtom* aCommand);

    GtkWidget          *mShell;
    MozContainer       *mContainer;
    MozDrawingarea     *mDrawingarea;

    GtkWindowGroup     *mWindowGroup;

    PRUint32            mContainerGotFocus : 1,
                        mContainerLostFocus : 1,
                        mContainerBlockFocus : 1,
                        mIsVisible : 1,
                        mRetryPointerGrab : 1,
                        mActivatePending : 1,
                        mRetryKeyboardGrab : 1;
    GtkWindow          *mTransientParent;
    PRInt32             mSizeState;
    PluginType          mPluginType;

    PRInt32             mTransparencyBitmapWidth;
    PRInt32             mTransparencyBitmapHeight;

    nsRefPtr<gfxASurface> mThebesSurface;

#ifdef MOZ_DFB
    int                    mDFBCursorX;
    int                    mDFBCursorY;
    PRUint32               mDFBCursorCount;
    IDirectFB             *mDFB;
    IDirectFBDisplayLayer *mDFBLayer;
#endif

#ifdef ACCESSIBILITY
    nsCOMPtr<nsIAccessible> mRootAccessible;
    void                CreateRootAccessible();
    void                GetRootAccessible(nsIAccessible** aAccessible);
    void                DispatchActivateEventAccessible();
    void                DispatchDeactivateEventAccessible();
    NS_IMETHOD_(PRBool) DispatchAccessibleEvent(nsIAccessible** aAccessible);
#endif

    // The cursor cache
    static GdkCursor   *gsGtkCursorCache[eCursorCount];

    // Transparency
    PRBool       mIsTransparent;
    // This bitmap tracks which pixels are transparent. We don't support
    // full translucency at this time; each pixel is either fully opaque
    // or fully transparent.
    gchar*       mTransparencyBitmap;
 
    // all of our DND stuff
    // this is the last window that had a drag event happen on it.
    static nsWindow    *mLastDragMotionWindow;
    void   InitDragEvent         (nsDragEvent &aEvent);
    void   UpdateDragStatus      (nsDragEvent &aEvent,
                                  GdkDragContext *aDragContext,
                                  nsIDragService *aDragService);

    // this is everything we need to be able to fire motion events
    // repeatedly
    GtkWidget         *mDragMotionWidget;
    GdkDragContext    *mDragMotionContext;
    gint               mDragMotionX;
    gint               mDragMotionY;
    guint              mDragMotionTime;
    guint              mDragMotionTimerID;
    nsCOMPtr<nsITimer> mDragLeaveTimer;
    float              mLastMotionPressure;

    static PRBool      sIsDraggingOutOf;
    // drag in progress
    static PRBool DragInProgress(void);

    void         ResetDragMotionTimer     (GtkWidget      *aWidget,
                                           GdkDragContext *aDragContext,
                                           gint           aX,
                                           gint           aY,
                                           guint          aTime);
    void         FireDragMotionTimer      (void);
    void         FireDragLeaveTimer       (void);
    static guint DragMotionTimerCallback (gpointer aClosure);
    static void  DragLeaveTimerCallback  (nsITimer *aTimer, void *aClosure);

    /* Key Down event is DOM Virtual Key driven, needs 256 bits. */
    PRUint32 mKeyDownFlags[8];

    /* Helper methods for DOM Key Down event suppression. */
    PRUint32* GetFlagWord32(PRUint32 aKeyCode, PRUint32* aMask) {
        /* Mozilla DOM Virtual Key Code is from 0 to 224. */
        NS_ASSERTION((aKeyCode <= 0xFF), "Invalid DOM Key Code");
        aKeyCode &= 0xFF;

        /* 32 = 2^5 = 0x20 */
        *aMask = PRUint32(1) << (aKeyCode & 0x1F);
        return &mKeyDownFlags[(aKeyCode >> 5)];
    }

    PRBool IsKeyDown(PRUint32 aKeyCode) {
        PRUint32 mask;
        PRUint32* flag = GetFlagWord32(aKeyCode, &mask);
        return ((*flag) & mask) != 0;
    }

    void SetKeyDownFlag(PRUint32 aKeyCode) {
        PRUint32 mask;
        PRUint32* flag = GetFlagWord32(aKeyCode, &mask);
        *flag |= mask;
    }

    void ClearKeyDownFlag(PRUint32 aKeyCode) {
        PRUint32 mask;
        PRUint32* flag = GetFlagWord32(aKeyCode, &mask);
        *flag &= ~mask;
    }

};

class nsChildWindow : public nsWindow {
public:
    nsChildWindow();
    ~nsChildWindow();
};

#endif /* __nsWindow_h__ */

