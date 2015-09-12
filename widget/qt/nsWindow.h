/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsWindow_h__
#define __nsWindow_h__

#include <QPointF>

#include "nsAutoPtr.h"
#include "nsBaseWidget.h"
#include "mozilla/EventForwards.h"

#include "nsGkAtoms.h"
#include "nsIIdleServiceInternal.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"

#ifdef MOZ_LOGGING

#include "mozilla/Logging.h"
#include "nsTArray.h"

extern PRLogModuleInfo *gWidgetLog;
extern PRLogModuleInfo *gWidgetFocusLog;
extern PRLogModuleInfo *gWidgetIMLog;
extern PRLogModuleInfo *gWidgetDrawLog;

#define LOG(args) MOZ_LOG(gWidgetLog, mozilla::LogLevel::Debug, args)
#define LOGFOCUS(args) MOZ_LOG(gWidgetFocusLog, mozilla::LogLevel::Debug, args)
#define LOGIM(args) MOZ_LOG(gWidgetIMLog, mozilla::LogLevel::Debug, args)
#define LOGDRAW(args) MOZ_LOG(gWidgetDrawLog, mozilla::LogLevel::Debug, args)

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

class QCloseEvent;
class QFocusEvent;
class QHideEvent;
class QKeyEvent;
class QMouseEvent;
class QMoveEvent;
class QResizeEvent;
class QShowEvent;
class QTabletEvent;
class QTouchEvent;
class QWheelEvent;

namespace mozilla {
namespace widget {
class MozQWidget;
class nsWindow : public nsBaseWidget
{
public:
    nsWindow();

    NS_DECL_ISUPPORTS_INHERITED

    //
    // nsIWidget
    //
    NS_IMETHOD Create(nsIWidget        *aParent,
                      nsNativeWidget   aNativeParent,
                      const nsIntRect  &aRect,
                      nsWidgetInitData *aInitData);
    NS_IMETHOD Destroy(void);

    NS_IMETHOD Show(bool aState);
    virtual bool IsVisible() const;
    NS_IMETHOD ConstrainPosition(bool aAllowSlop,
                                 int32_t *aX,
                                 int32_t *aY);
    NS_IMETHOD Move(double aX,
                    double aY);
    NS_IMETHOD Resize(double aWidth,
                      double aHeight,
                      bool   aRepaint);
    NS_IMETHOD Resize(double aX,
                      double aY,
                      double aWidth,
                      double aHeight,
                      bool   aRepaint);
    NS_IMETHOD Enable(bool aState);
    // Some of the nsIWidget methods
    virtual bool IsEnabled() const;
    NS_IMETHOD SetFocus(bool aRaise = false);
    NS_IMETHOD ConfigureChildren(const nsTArray<nsIWidget::Configuration>&);
    NS_IMETHOD         Invalidate(const nsIntRect &aRect);
    virtual void*      GetNativeData(uint32_t aDataType);
    NS_IMETHOD         SetTitle(const nsAString& aTitle);
    NS_IMETHOD         SetCursor(nsCursor aCursor);
    NS_IMETHOD         SetCursor(imgIContainer* aCursor,
                                 uint32_t aHotspotX, uint32_t aHotspotY)
    {
        return NS_OK;
    }
    virtual mozilla::LayoutDeviceIntPoint WidgetToScreenOffset();
    NS_IMETHOD DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                             nsEventStatus& aStatus);
    NS_IMETHOD CaptureRollupEvents(nsIRollupListener *aListener,
                                   bool aDoCapture)
    {
        return NS_ERROR_NOT_IMPLEMENTED;
    }
    NS_IMETHOD ReparentNativeWidget(nsIWidget* aNewParent);

    NS_IMETHOD MakeFullScreen(bool aFullScreen, nsIScreen* aTargetScreen = nullptr);
    virtual mozilla::layers::LayerManager*
        GetLayerManager(PLayerTransactionChild* aShadowManager = nullptr,
                        LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
                        LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT,
                        bool* aAllowRetaining = nullptr);

    NS_IMETHOD_(void) SetInputContext(const InputContext& aContext,
                                      const InputContextAction& aAction);
    NS_IMETHOD_(InputContext) GetInputContext();

    virtual uint32_t GetGLFrameBufferFormat() override;

    already_AddRefed<mozilla::gfx::DrawTarget> StartRemoteDrawing() override;

    // Widget notifications
    virtual void OnPaint();
    virtual nsEventStatus focusInEvent(QFocusEvent* aEvent);
    virtual nsEventStatus focusOutEvent(QFocusEvent* aEvent);
    virtual nsEventStatus hideEvent(QHideEvent* aEvent);
    virtual nsEventStatus showEvent(QShowEvent* aEvent);
    virtual nsEventStatus keyPressEvent(QKeyEvent* aEvent);
    virtual nsEventStatus keyReleaseEvent(QKeyEvent* aEvent);
    virtual nsEventStatus mouseDoubleClickEvent(QMouseEvent* aEvent);
    virtual nsEventStatus mouseMoveEvent(QMouseEvent* aEvent);
    virtual nsEventStatus mousePressEvent(QMouseEvent* aEvent);
    virtual nsEventStatus mouseReleaseEvent(QMouseEvent* aEvent);
    virtual nsEventStatus moveEvent(QMoveEvent* aEvent);
    virtual nsEventStatus resizeEvent(QResizeEvent* aEvent);
    virtual nsEventStatus touchEvent(QTouchEvent* aEvent);
    virtual nsEventStatus wheelEvent(QWheelEvent* aEvent);
    virtual nsEventStatus tabletEvent(QTabletEvent* event);

protected:
    virtual ~nsWindow();

    nsWindow* mParent;
    bool  mVisible;
    InputContext mInputContext;
    nsCOMPtr<nsIIdleServiceInternal> mIdleService;
    MozQWidget* mWidget;

private:
    // event handling code
    nsEventStatus DispatchEvent(mozilla::WidgetGUIEvent* aEvent);
    void DispatchActivateEvent(void);
    void DispatchDeactivateEvent(void);
    void DispatchActivateEventOnTopLevelWindow(void);
    void DispatchDeactivateEventOnTopLevelWindow(void);
    void DispatchResizeEvent(nsIntRect &aRect, nsEventStatus &aStatus);

    // Remember the last sizemode so that we can restore it when
    // leaving fullscreen
    nsSizeMode mLastSizeMode;
    // is this widget enabled?
    bool mEnabled;

    // Call this function when the users activity is the direct cause of an
    // event (like a keypress or mouse click).
    void UserActivity();
    MozQWidget* createQWidget(MozQWidget* parent,
                              nsWidgetInitData* aInitData);

public:
    // Old QtWidget only
    NS_IMETHOD         SetParent(nsIWidget* aNewParent);
    virtual nsIWidget *GetParent(void);
    virtual float      GetDPI();
    NS_IMETHOD         SetModal(bool aModal);
    NS_IMETHOD         PlaceBehind(nsTopLevelWidgetZPlacement  aPlacement,
                                   nsIWidget                  *aWidget,
                                   bool                        aActivate);
    NS_IMETHOD         SetSizeMode(nsSizeMode aMode);
    NS_IMETHOD         GetScreenBounds(nsIntRect &aRect);
    NS_IMETHOD         SetHasTransparentBackground(bool aTransparent);
    NS_IMETHOD         GetHasTransparentBackground(bool& aTransparent);
    NS_IMETHOD         HideWindowChrome(bool aShouldHide);
    NS_IMETHOD         SetIcon(const nsAString& aIconSpec);
    NS_IMETHOD         CaptureMouse(bool aCapture);
    NS_IMETHOD         SetWindowClass(const nsAString& xulWinType);
    NS_IMETHOD         GetAttention(int32_t aCycleCount);
    NS_IMETHOD_(bool)  HasGLContext();

    //
    // utility methods
    //
    void               QWidgetDestroyed();
    // called when we are destroyed
    void OnDestroy(void);
    // called to check and see if a widget's dimensions are sane
    bool AreBoundsSane(void);
private:
    // Is this a toplevel window?
    bool                mIsTopLevel;
    // Has this widget been destroyed yet?
    bool                mIsDestroyed;
    // This flag tracks if we're hidden or shown.
    bool                mIsShown;
    // Has anyone set an x/y location for this widget yet? Toplevels
    // shouldn't be automatically set to 0,0 for first show.
    bool                mPlaced;
    /**
     * Event handlers (proxied from the actual qwidget).
     * They follow normal Qt widget semantics.
     */
    void Initialize(MozQWidget *widget);
    virtual nsEventStatus OnCloseEvent(QCloseEvent *);
    void               NativeResize(int32_t aWidth,
                                    int32_t aHeight,
                                    bool    aRepaint);
    void               NativeResize(int32_t aX,
                                    int32_t aY,
                                    int32_t aWidth,
                                    int32_t aHeight,
                                    bool    aRepaint);
    void               NativeShow  (bool    aAction);

private:
    typedef struct {
        QPointF pos;
        Qt::KeyboardModifiers modifiers;
        bool needDispatch;
    } MozCachedMoveEvent;

    bool               CheckForRollup(double aMouseX, double aMouseY, bool aIsWheel);
    void*              SetupPluginPort(void);
    nsresult           SetWindowIconList(const nsTArray<nsCString> &aIconList);
    void               SetDefaultIcon(void);

    nsEventStatus      DispatchCommandEvent(nsIAtom* aCommand);
    nsEventStatus      DispatchContentCommandEvent(mozilla::EventMessage aMsg);
    void               SetSoftwareKeyboardState(bool aOpen, const InputContextAction& aAction);
    void               ClearCachedResources();

    uint32_t           mActivatePending : 1;
    int32_t            mSizeState;

    bool         mIsTransparent;

    // all of our DND stuff
    // this is the last window that had a drag event happen on it.
    void   InitDragEvent(mozilla::WidgetMouseEvent& aEvent);

    // this is everything we need to be able to fire motion events
    // repeatedly
    uint32_t mKeyDownFlags[8];

    /* Helper methods for DOM Key Down event suppression. */
    uint32_t* GetFlagWord32(uint32_t aKeyCode, uint32_t* aMask) {
        /* Mozilla DOM Virtual Key Code is from 0 to 224. */
        NS_ASSERTION((aKeyCode <= 0xFF), "Invalid DOM Key Code");
        aKeyCode &= 0xFF;

        /* 32 = 2^5 = 0x20 */
        *aMask = uint32_t(1) << (aKeyCode & 0x1F);
        return &mKeyDownFlags[(aKeyCode >> 5)];
    }

    bool IsKeyDown(uint32_t aKeyCode) {
        uint32_t mask;
        uint32_t* flag = GetFlagWord32(aKeyCode, &mask);
        return ((*flag) & mask) != 0;
    }

    void SetKeyDownFlag(uint32_t aKeyCode) {
        uint32_t mask;
        uint32_t* flag = GetFlagWord32(aKeyCode, &mask);
        *flag |= mask;
    }

    void ClearKeyDownFlag(uint32_t aKeyCode) {
        uint32_t mask;
        uint32_t* flag = GetFlagWord32(aKeyCode, &mask);
        *flag &= ~mask;
    }
    int32_t mQCursor;


    void ProcessMotionEvent();

    void DispatchMotionToMainThread() {
        if (!mTimerStarted) {
            nsCOMPtr<nsIRunnable> event =
                NS_NewRunnableMethod(this, &nsWindow::ProcessMotionEvent);
            NS_DispatchToMainThread(event);
            mTimerStarted = true;
        }
    }

    bool mNeedsResize;
    bool mNeedsMove;
    bool mListenForResizes;
    bool mNeedsShow;
    MozCachedMoveEvent mMoveEvent;
    bool mTimerStarted;
};

}}

#endif /* __nsWindow_h__ */
