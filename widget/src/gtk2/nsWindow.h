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
 * The Original Code mozilla.org code.
 *
 * The Initial Developer of the Original Code Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2001 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsCommonWidget.h"

#include "mozcontainer.h"
#include "mozdrawingarea.h"
#include "nsWeakReference.h"

#include "nsIDragService.h"
#include "nsITimer.h"

#include <gtk/gtk.h>

#include <gdk/gdkx.h>
#include <gtk/gtkwindow.h>

#ifdef ACCESSIBILITY
#include "nsIAccessNode.h"
#include "nsIAccessible.h"
#endif

#ifdef USE_XIM
#include <gtk/gtkimmulticontext.h>
#include "pldhash.h"
#endif

class nsWindow : public nsCommonWidget, public nsSupportsWeakReference {
public:
    nsWindow();
    virtual ~nsWindow();

    static void ReleaseGlobals();

    NS_DECL_ISUPPORTS_INHERITED

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
    NS_IMETHOD         SetModal(PRBool aModal);
    NS_IMETHOD         IsVisible(PRBool & aState);
    NS_IMETHOD         ConstrainPosition(PRBool aAllowSlop,
                                         PRInt32 *aX,
                                         PRInt32 *aY);
    NS_IMETHOD         Move(PRInt32 aX,
                            PRInt32 aY);
    NS_IMETHOD         PlaceBehind(nsTopLevelWidgetZPlacement  aPlacement,
                                   nsIWidget                  *aWidget,
                                   PRBool                      aActivate);
    NS_IMETHOD         SetSizeMode(PRInt32 aMode);
    NS_IMETHOD         Enable(PRBool aState);
    NS_IMETHOD         SetFocus(PRBool aRaise = PR_FALSE);
    NS_IMETHOD         GetScreenBounds(nsRect &aRect);
    NS_IMETHOD         SetForegroundColor(const nscolor &aColor);
    NS_IMETHOD         SetBackgroundColor(const nscolor &aColor);
    virtual            nsIFontMetrics* GetFont(void);
    NS_IMETHOD         SetFont(const nsFont &aFont);
    NS_IMETHOD         SetCursor(nsCursor aCursor);
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
    NS_IMETHOD         SetMenuBar(nsIMenuBar * aMenuBar);
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

    void               NativeResize(PRInt32 aWidth,
                                    PRInt32 aHeight,
                                    PRBool  aRepaint);

    void               NativeResize(PRInt32 aX,
                                    PRInt32 aY,
                                    PRInt32 aWidth,
                                    PRInt32 aHeight,
                                    PRBool  aRepaint);

    void               NativeShow  (PRBool  aAction);

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
    void               SetNonXEmbedPluginFocus(void);
    void               LoseNonXEmbedPluginFocus(void);

    Window             mOldFocusWindow;

    static guint32     mLastButtonPressTime;

#ifdef USE_XIM
    void               IMEDestroyContext (void);
    void               IMESetFocus       (void);
    void               IMELoseFocus      (void);
    void               IMEComposeStart   (void);
    void               IMEComposeText    (const PRUnichar *aText,
                                          const PRInt32 aLen,
                                          const gchar *aPreeditString,
                                          const PangoAttrList *aFeedback);
    void               IMEComposeEnd     (void);
    GtkIMContext*      IMEGetContext     (void);
    void               IMECreateContext  (void);
    PRBool             IMEFilterEvent    (GdkEventKey *aEvent);

    GtkIMContext       *mIMContext;
    PRBool             mComposingText;

#endif

   void                ResizeTransparencyBitmap(PRInt32 aNewWidth, PRInt32 aNewHeight);
   void                ApplyTransparencyBitmap();
#ifdef MOZ_XUL
   NS_IMETHOD          SetWindowTranslucency(PRBool aTransparent);
   NS_IMETHOD          GetWindowTranslucency(PRBool& aTransparent);
   NS_IMETHOD          UpdateTranslucentWindowAlpha(const nsRect& aRect, PRUint8* aAlphas);
#endif

private:
    void               GetToplevelWidget(GtkWidget **aWidget);
    void               GetContainerWindow(nsWindow  **aWindow);
    void              *SetupPluginPort(void);
    nsresult           SetWindowIconList(const nsCStringArray &aIconList);
    void               SetDefaultIcon(void);

    GtkWidget          *mShell;
    MozContainer       *mContainer;
    MozDrawingarea     *mDrawingarea;

    GtkWindowGroup     *mWindowGroup;

    PRUint32            mContainerGotFocus : 1,
                        mContainerLostFocus : 1,
                        mContainerBlockFocus : 1,
                        mInKeyRepeat : 1,
                        mIsVisible : 1,
                        mRetryPointerGrab : 1,
                        mActivatePending : 1,
                        mRetryKeyboardGrab : 1;
    GtkWindow          *mTransientParent;
    PRInt32             mSizeState;
    PluginType          mPluginType;

#ifdef ACCESSIBILITY
    nsCOMPtr<nsIAccessible> mRootAccessible;
    void                CreateRootAccessible();
    void                GetRootAccessible(nsIAccessible** aAccessible);
    void                DispatchActivateEvent(void);
    void                DispatchDeactivateEvent(void);
    NS_IMETHOD_(PRBool) DispatchAccessibleEvent(nsIAccessible** aAccessible);
#endif

    // The cursor cache
    static GdkCursor   *gsGtkCursorCache[eCursorCount];

    // Transparency
    PRBool       mIsTranslucent;
    // This bitmap tracks which pixels are transparent. We don't support
    // full translucency at this time; each pixel is either fully opaque
    // or fully transparent.
    gchar*       mTransparencyBitmap;
 
    // all of our DND stuff
    // this is the last window that had a drag event happen on it.
    static nsWindow    *mLastDragMotionWindow;
    void   InitDragEvent         (nsMouseEvent &aEvent);
    void   UpdateDragStatus      (nsMouseEvent &aEvent,
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

};

class nsChildWindow : public nsWindow {
public:
    nsChildWindow();
    ~nsChildWindow();
};

#endif /* __nsWindow_h__ */

