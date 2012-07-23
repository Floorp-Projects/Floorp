/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsWindow_h__
#define __nsWindow_h__

#include <QKeyEvent>
#include <QGestureEvent>
#include <qgraphicswidget.h>
#include <QTime>

#include "nsAutoPtr.h"

#include "nsBaseWidget.h"
#include "nsGUIEvent.h"

#include "nsWeakReference.h"

#include "nsGkAtoms.h"
#include "nsIIdleServiceInternal.h"

#ifdef MOZ_LOGGING

// make sure that logging is enabled before including prlog.h
#define FORCE_PR_LOG

#include "prlog.h"
#include "nsTArray.h"

extern PRLogModuleInfo *gWidgetLog;
extern PRLogModuleInfo *gWidgetFocusLog;
extern PRLogModuleInfo *gWidgetIMLog;
extern PRLogModuleInfo *gWidgetDrawLog;

#define LOG(args) PR_LOG(gWidgetLog, 4, args)
#define LOGFOCUS(args) PR_LOG(gWidgetFocusLog, 4, args)
#define LOGIM(args) PR_LOG(gWidgetIMLog, 4, args)
#define LOGDRAW(args) PR_LOG(gWidgetDrawLog, 4, args)

#else

#ifdef DEBUG_WIDGETS

#define PR_LOG2(_args)         \
    PR_BEGIN_MACRO             \
      qDebug _args;            \
    PR_END_MACRO

#define LOG(args) PR_LOG2(args)
#define LOGFOCUS(args) PR_LOG2(args)
#define LOGIM(args) PR_LOG2(args)
#define LOGDRAW(args) PR_LOG2(args)

#else

#define LOG(args)
#define LOGFOCUS(args)
#define LOGIM(args)
#define LOGDRAW(args)

#endif

#endif /* MOZ_LOGGING */

class QEvent;
class QGraphicsView;

class MozQWidget;

class nsIdleService;

class nsWindow : public nsBaseWidget,
                 public nsSupportsWeakReference
{
public:
    nsWindow();
    virtual ~nsWindow();

    nsEventStatus DoPaint( QPainter* aPainter, const QStyleOptionGraphicsItem * aOption, QWidget* aWidget);

    static void ReleaseGlobals();

    NS_DECL_ISUPPORTS_INHERITED

    //
    // nsIWidget
    //

    NS_IMETHOD         ConfigureChildren(const nsTArray<nsIWidget::Configuration>&);

    NS_IMETHOD         Create(nsIWidget        *aParent,
                              nsNativeWidget   aNativeParent,
                              const nsIntRect  &aRect,
                              EVENT_CALLBACK   aHandleEventFunction,
                              nsDeviceContext *aContext,
                              nsWidgetInitData *aInitData);

    virtual already_AddRefed<nsIWidget>
    CreateChild(const nsIntRect&  aRect,
                EVENT_CALLBACK    aHandleEventFunction,
                nsDeviceContext* aContext,
                nsWidgetInitData* aInitData = nsnull,
                bool              aForceUseIWidgetParent = true);

    NS_IMETHOD         Destroy(void);
    NS_IMETHOD         SetParent(nsIWidget* aNewParent);
    virtual nsIWidget *GetParent(void);
    virtual float      GetDPI();
    NS_IMETHOD         Show(bool aState);
    NS_IMETHOD         SetModal(bool aModal);
    virtual bool       IsVisible() const;
    NS_IMETHOD         ConstrainPosition(bool aAllowSlop,
                                         PRInt32 *aX,
                                         PRInt32 *aY);
    NS_IMETHOD         Move(PRInt32 aX,
                            PRInt32 aY);
    NS_IMETHOD         Resize(PRInt32 aWidth,
                              PRInt32 aHeight,
                              bool    aRepaint);
    NS_IMETHOD         Resize(PRInt32 aX,
                              PRInt32 aY,
                              PRInt32 aWidth,
                              PRInt32 aHeight,
                              bool     aRepaint);
    NS_IMETHOD         PlaceBehind(nsTopLevelWidgetZPlacement  aPlacement,
                                   nsIWidget                  *aWidget,
                                   bool                        aActivate);
    NS_IMETHOD         SetSizeMode(PRInt32 aMode);
    NS_IMETHOD         Enable(bool aState);
    NS_IMETHOD         SetFocus(bool aRaise = false);
    NS_IMETHOD         GetScreenBounds(nsIntRect &aRect);
    NS_IMETHOD         SetForegroundColor(const nscolor &aColor);
    NS_IMETHOD         SetBackgroundColor(const nscolor &aColor);
    NS_IMETHOD         SetCursor(nsCursor aCursor);
    NS_IMETHOD         SetCursor(imgIContainer* aCursor,
                                 PRUint32 aHotspotX, PRUint32 aHotspotY);
    NS_IMETHOD         SetHasTransparentBackground(bool aTransparent);
    NS_IMETHOD         GetHasTransparentBackground(bool& aTransparent);
    NS_IMETHOD         HideWindowChrome(bool aShouldHide);
    NS_IMETHOD         MakeFullScreen(bool aFullScreen);
    NS_IMETHOD         Invalidate(const nsIntRect &aRect);

    virtual void*      GetNativeData(PRUint32 aDataType);
    NS_IMETHOD         SetTitle(const nsAString& aTitle);
    NS_IMETHOD         SetIcon(const nsAString& aIconSpec);
    virtual nsIntPoint WidgetToScreenOffset();
    NS_IMETHOD         DispatchEvent(nsGUIEvent *aEvent, nsEventStatus &aStatus);

    NS_IMETHOD         EnableDragDrop(bool aEnable);
    NS_IMETHOD         CaptureMouse(bool aCapture);
    NS_IMETHOD         CaptureRollupEvents(nsIRollupListener *aListener,
                                           bool aDoCapture,
                                           bool aConsumeRollupEvent);

    NS_IMETHOD         SetWindowClass(const nsAString& xulWinType);

    NS_IMETHOD         GetAttention(PRInt32 aCycleCount);
    NS_IMETHOD         BeginResizeDrag   (nsGUIEvent* aEvent, PRInt32 aHorizontal, PRInt32 aVertical);

    NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                      const InputContextAction& aAction);
    NS_IMETHOD_(InputContext) GetInputContext();
    NS_IMETHOD_(bool)  HasGLContext();

    //
    // utility methods
    //
    void               QWidgetDestroyed();

    /***** from CommonWidget *****/

    // event handling code

    void DispatchActivateEvent(void);
    void DispatchDeactivateEvent(void);
    void DispatchActivateEventOnTopLevelWindow(void);
    void DispatchDeactivateEventOnTopLevelWindow(void);
    void DispatchResizeEvent(nsIntRect &aRect, nsEventStatus &aStatus);

    nsEventStatus DispatchEvent(nsGUIEvent *aEvent) {
        nsEventStatus status;
        DispatchEvent(aEvent, status);
        return status;
    }

    // Some of the nsIWidget methods
    virtual bool IsEnabled() const;

    // called when we are destroyed
    void OnDestroy(void);

    // called to check and see if a widget's dimensions are sane
    bool AreBoundsSane(void);

    NS_IMETHOD         ReparentNativeWidget(nsIWidget* aNewParent);

    QWidget* GetViewWidget();
    virtual PRUint32 GetGLFrameBufferFormat() MOZ_OVERRIDE;

protected:
    nsCOMPtr<nsIWidget> mParent;
    // Is this a toplevel window?
    bool                mIsTopLevel;
    // Has this widget been destroyed yet?
    bool                mIsDestroyed;

    // This flag tracks if we're hidden or shown.
    bool                mIsShown;
    // is this widget enabled?
    bool                mEnabled;
    // Has anyone set an x/y location for this widget yet? Toplevels
    // shouldn't be automatically set to 0,0 for first show.
    bool                mPlaced;

    // Remember the last sizemode so that we can restore it when
    // leaving fullscreen
    nsSizeMode         mLastSizeMode;

    InputContext mInputContext;

    /**
     * Event handlers (proxied from the actual qwidget).
     * They follow normal Qt widget semantics.
     */
    void Initialize(MozQWidget *widget);
    friend class nsQtEventDispatcher;
    friend class InterceptContainer;
    friend class MozQWidget;

    virtual nsEventStatus OnMoveEvent(QGraphicsSceneHoverEvent *);
    virtual nsEventStatus OnResizeEvent(QGraphicsSceneResizeEvent *);
    virtual nsEventStatus OnCloseEvent(QCloseEvent *);
    virtual nsEventStatus OnEnterNotifyEvent(QGraphicsSceneHoverEvent *);
    virtual nsEventStatus OnLeaveNotifyEvent(QGraphicsSceneHoverEvent *);
    virtual nsEventStatus OnMotionNotifyEvent(QPointF, Qt::KeyboardModifiers);
    virtual nsEventStatus OnButtonPressEvent(QGraphicsSceneMouseEvent *);
    virtual nsEventStatus OnButtonReleaseEvent(QGraphicsSceneMouseEvent *);
    virtual nsEventStatus OnMouseDoubleClickEvent(QGraphicsSceneMouseEvent *);
    virtual nsEventStatus OnFocusInEvent(QEvent *);
    virtual nsEventStatus OnFocusOutEvent(QEvent *);
    virtual nsEventStatus OnKeyPressEvent(QKeyEvent *);
    virtual nsEventStatus OnKeyReleaseEvent(QKeyEvent *);
    virtual nsEventStatus OnScrollEvent(QGraphicsSceneWheelEvent *);

    virtual nsEventStatus contextMenuEvent(QGraphicsSceneContextMenuEvent *);
    virtual nsEventStatus imComposeEvent(QInputMethodEvent *, bool &handled);
    virtual nsEventStatus OnDragEnter (QGraphicsSceneDragDropEvent *);
    virtual nsEventStatus OnDragMotionEvent(QGraphicsSceneDragDropEvent *);
    virtual nsEventStatus OnDragLeaveEvent(QGraphicsSceneDragDropEvent *);
    virtual nsEventStatus OnDragDropEvent (QGraphicsSceneDragDropEvent *);
    virtual nsEventStatus showEvent(QShowEvent *);
    virtual nsEventStatus hideEvent(QHideEvent *);

//Gestures are only supported in qt > 4.6
#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0))
    virtual nsEventStatus OnTouchEvent(QTouchEvent *event, bool &handled);

    virtual nsEventStatus OnGestureEvent(QGestureEvent *event, bool &handled);
    nsEventStatus DispatchGestureEvent(PRUint32 aMsg, PRUint32 aDirection,
                                       double aDelta, const nsIntPoint& aRefPoint);

    double DistanceBetweenPoints(const QPointF &aFirstPoint, const QPointF &aSecondPoint);
#endif

    void               NativeResize(PRInt32 aWidth,
                                    PRInt32 aHeight,
                                    bool    aRepaint);

    void               NativeResize(PRInt32 aX,
                                    PRInt32 aY,
                                    PRInt32 aWidth,
                                    PRInt32 aHeight,
                                    bool    aRepaint);

    void               NativeShow  (bool    aAction);

    enum PluginType {
        PluginType_NONE = 0,   /* do not have any plugin */
        PluginType_XEMBED,     /* the plugin support xembed */
        PluginType_NONXEMBED   /* the plugin does not support xembed */
    };

    void               SetPluginType(PluginType aPluginType);

    void               ThemeChanged(void);

    gfxASurface*       GetThebesSurface();

private:
    typedef struct {
        QPointF centerPoint;
        QPointF touchPoint;
        double delta;
        bool needDispatch;
        double startDistance;
        double prevDistance;
    } MozCachedTouchEvent;

    typedef struct {
        QPointF pos;
        Qt::KeyboardModifiers modifiers;
        bool needDispatch;
    } MozCachedMoveEvent;

    void*              SetupPluginPort(void);
    nsresult           SetWindowIconList(const nsTArray<nsCString> &aIconList);
    void               SetDefaultIcon(void);
    void               InitButtonEvent(nsMouseEvent &event, QGraphicsSceneMouseEvent *aEvent, int aClickCount = 1);
    nsEventStatus      DispatchCommandEvent(nsIAtom* aCommand);
    nsEventStatus      DispatchContentCommandEvent(PRInt32 aMsg);
    MozQWidget*        createQWidget(MozQWidget* parent,
                                     nsNativeWidget nativeParent,
                                     nsWidgetInitData* aInitData);
    void               SetSoftwareKeyboardState(bool aOpen, const InputContextAction& aAction);

    MozQWidget*        mWidget;

    PRUint32           mIsVisible : 1,
                       mActivatePending : 1;
    PRInt32            mSizeState;
    PluginType         mPluginType;

    nsRefPtr<gfxASurface> mThebesSurface;
    nsCOMPtr<nsIIdleServiceInternal> mIdleService;

    bool         mIsTransparent;
 
    // all of our DND stuff
    // this is the last window that had a drag event happen on it.
    void   InitDragEvent         (nsMouseEvent &aEvent);

    // this is everything we need to be able to fire motion events
    // repeatedly
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

    bool IsKeyDown(PRUint32 aKeyCode) {
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
    PRInt32 mQCursor;

    // Call this function when the users activity is the direct cause of an
    // event (like a keypress or mouse click).
    void UserActivity();

    inline void ProcessMotionEvent() {
        if (mPinchEvent.needDispatch) {
            double distance = DistanceBetweenPoints(mPinchEvent.centerPoint, mPinchEvent.touchPoint);
            distance *= 2;
            mPinchEvent.delta = distance - mPinchEvent.prevDistance;
            nsIntPoint centerPoint(mPinchEvent.centerPoint.x(), mPinchEvent.centerPoint.y());
            DispatchGestureEvent(NS_SIMPLE_GESTURE_MAGNIFY_UPDATE,
                                 0, mPinchEvent.delta, centerPoint);
            mPinchEvent.prevDistance = distance;
        }
        if (mMoveEvent.needDispatch) {
            nsMouseEvent event(true, NS_MOUSE_MOVE, this, nsMouseEvent::eReal);

            event.refPoint.x = nscoord(mMoveEvent.pos.x());
            event.refPoint.y = nscoord(mMoveEvent.pos.y());

            event.InitBasicModifiers(mMoveEvent.modifiers & Qt::ControlModifier,
                                     mMoveEvent.modifiers & Qt::AltModifier,
                                     mMoveEvent.modifiers & Qt::ShiftModifier,
                                     mMoveEvent.modifiers & Qt::MetaModifier);
            event.clickCount      = 0;

            DispatchEvent(&event);
            mMoveEvent.needDispatch = false;
        }

        mTimerStarted = false;
    }

    void DispatchMotionToMainThread() {
        if (!mTimerStarted) {
            nsCOMPtr<nsIRunnable> event =
                NS_NewRunnableMethod(this, &nsWindow::ProcessMotionEvent);
            NS_DispatchToMainThread(event);
            mTimerStarted = true;
        }
    }

    // Remember dirty area caused by ::Scroll
    QRegion mDirtyScrollArea;

#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0))
    QTime mLastMultiTouchTime;
#endif

    bool mNeedsResize;
    bool mNeedsMove;
    bool mListenForResizes;
    bool mNeedsShow;
    bool mGesturesCancelled;
    MozCachedTouchEvent mPinchEvent;
    MozCachedMoveEvent mMoveEvent;
    bool mTimerStarted;
};

class nsChildWindow : public nsWindow
{
public:
    nsChildWindow();
    ~nsChildWindow();

    PRInt32 mChildID;
};

class nsPopupWindow : public nsWindow
{
public:
    nsPopupWindow ();
    ~nsPopupWindow ();

    PRInt32 mChildID;
};
#endif /* __nsWindow_h__ */

