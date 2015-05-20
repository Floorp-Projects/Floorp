/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"

#include <QGuiApplication>
#include <QtGui/QCursor>
#include <QIcon>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QPaintEngine>
#include <QMimeData>
#include <QScreen>

#include <QtCore/QDebug>
#include <QtCore/QEvent>
#include <QtCore/QVariant>
#include <algorithm>

#ifdef MOZ_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "gfxXlibSurface.h"
#endif //MOZ_X11

#include "nsXULAppAPI.h"

#include "prlink.h"

#include "nsWindow.h"
#include "mozqwidget.h"

#include "nsIdleService.h"
#include "nsIRollupListener.h"
#include "nsWidgetsCID.h"
#include "nsQtKeyUtils.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "mozilla/Likely.h"
#include "mozilla/layers/LayersTypes.h"
#include "nsIWidgetListener.h"
#include "ClientLayerManager.h"
#include "BasicLayers.h"

#include "nsIStringBundle.h"
#include "nsGfxCIID.h"

#include "imgIContainer.h"
#include "nsGfxCIID.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsAutoPtr.h"

#include "gfxQtPlatform.h"

#include "nsIDOMWheelEvent.h"

#include "GLContext.h"

#ifdef MOZ_X11
#include "keysym2ucs.h"
#endif

#include "Layers.h"
#include "GLContextProvider.h"

using namespace mozilla;
using namespace mozilla::gl;
using namespace mozilla::widget;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using mozilla::gl::GLContext;

#define kWindowPositionSlop 20

// Qt
static const int WHEEL_DELTA = 120;
static bool gGlobalsInitialized = false;
static bool sAltGrModifier = false;

static void find_first_visible_parent(QWindow* aItem, QWindow*& aVisibleItem);
static bool is_mouse_in_window (MozQWidget* aWindow, double aMouseX, double aMouseY);

NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, nsBaseWidget)

nsWindow::nsWindow()
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__, (void *)this));

    mIsTopLevel       = false;
    mIsDestroyed      = false;
    mIsShown          = false;
    mEnabled          = true;
    mWidget              = nullptr;
    mVisible           = false;
    mActivatePending     = false;
    mWindowType          = eWindowType_child;
    mSizeState           = nsSizeMode_Normal;
    mLastSizeMode        = nsSizeMode_Normal;
    mQCursor             = Qt::ArrowCursor;
    mNeedsResize         = false;
    mNeedsMove           = false;
    mListenForResizes    = false;
    mNeedsShow           = false;
    mTimerStarted        = false;
    mMoveEvent.needDispatch = false;

    if (!gGlobalsInitialized) {
        gfxPlatform::GetPlatform();
        gGlobalsInitialized = true;
    }

    memset(mKeyDownFlags, 0, sizeof(mKeyDownFlags));

    mIsTransparent = false;

    mCursor = eCursor_standard;
}

nsWindow::~nsWindow()
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__, (void *)this));

    Destroy();
}

nsresult
nsWindow::Create(nsIWidget        *aParent,
                 nsNativeWidget    aNativeParent,
                 const nsIntRect  &aRect,
                 nsWidgetInitData *aInitData)
{
    // only set the base parent if we're not going to be a dialog or a
    // toplevel
    nsIWidget *baseParent = aParent;

    // initialize all the common bits of this class
    BaseCreate(baseParent, aRect, aInitData);

    mVisible = true;

    // and do our common creation
    mParent = (nsWindow *)aParent;

    // save our bounds
    mBounds = aRect;

    // find native parent
    MozQWidget *parent = nullptr;

    if (aParent != nullptr) {
        parent = static_cast<MozQWidget*>(aParent->GetNativeData(NS_NATIVE_WIDGET));
    } else if (aNativeParent != nullptr) {
        parent = static_cast<MozQWidget*>(aNativeParent);
        if (parent && mParent == nullptr) {
            mParent = parent->getReceiver();
        }
    }

    LOG(("Create: nsWindow [%p] mWidget:[%p] parent:[%p], natPar:[%p] mParent:%p\n", (void *)this, (void*)mWidget, parent, aNativeParent, mParent));

    // ok, create our QGraphicsWidget
    mWidget = createQWidget(parent, aInitData);

    if (!mWidget) {
        return NS_ERROR_OUT_OF_MEMORY;
    }


    // resize so that everything is set to the right dimensions
    Resize(mBounds.x, mBounds.y, mBounds.width, mBounds.height, false);

    // check if we should listen for resizes
    mListenForResizes = (aNativeParent ||
                         (aInitData && aInitData->mListenForResizes));

    return NS_OK;
}

MozQWidget*
nsWindow::createQWidget(MozQWidget* parent,
                        nsWidgetInitData* aInitData)
{
    const char *windowName = nullptr;
    Qt::WindowFlags flags = Qt::Widget;

    // ok, create our windows
    switch (mWindowType) {
    case eWindowType_dialog:
        windowName = "topLevelDialog";
        flags = Qt::Dialog;
        break;
    case eWindowType_popup:
        windowName = "topLevelPopup";
        flags = Qt::Popup;
        break;
    case eWindowType_toplevel:
        windowName = "topLevelWindow";
        flags = Qt::Window;
        break;
    case eWindowType_invisible:
        windowName = "topLevelInvisible";
        break;
    case eWindowType_child:
    case eWindowType_plugin:
    default: // sheet
        windowName = "paintArea";
        break;
    }

    MozQWidget* widget = new MozQWidget(this, parent);
    if (!widget) {
        return nullptr;
    }

    widget->setObjectName(QString(windowName));
    if (mWindowType == eWindowType_invisible) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 1, 0))
        widget->setVisibility(QWindow::Hidden);
#else
        widget->hide();
#endif
    }
    if (mWindowType == eWindowType_dialog) {
        widget->setModality(Qt::WindowModal);
    }

    widget->create();

    // create a QGraphicsView if this is a new toplevel window
    LOG(("nsWindow::%s [%p] Created Window: %s, widget:%p, par:%p\n", __FUNCTION__, (void *)this, windowName, widget, parent));

    return widget;
}

NS_IMETHODIMP
nsWindow::Destroy(void)
{
    if (mIsDestroyed || !mWidget) {
        return NS_OK;
    }

    LOG(("nsWindow::Destroy [%p]\n", (void *)this));
    mIsDestroyed = true;

    /** Need to clean our LayerManager up while still alive */
    if (mLayerManager) {
        mLayerManager->Destroy();
    }
    mLayerManager = nullptr;

    // It is safe to call DestroyeCompositor several times (here and 
    // in the parent class) since it will take effect only once.
    // The reason we call it here is because on gtk platforms we need 
    // to destroy the compositor before we destroy the gdk window (which
    // destroys the the gl context attached to it).
    DestroyCompositor();

    ClearCachedResources();

    nsIRollupListener* rollupListener = nsBaseWidget::GetActiveRollupListener();
    if (rollupListener) {
        nsCOMPtr<nsIWidget> rollupWidget = rollupListener->GetRollupWidget();
        if (static_cast<nsIWidget *>(this) == rollupWidget) {
            rollupListener->Rollup(0, false, nullptr, nullptr);
        }
    }

    Show(false);

    // walk the list of children and call destroy on them.  Have to be
    // careful, though -- calling destroy on a kid may actually remove
    // it from our child list, losing its sibling links.
    for (nsIWidget* kid = mFirstChild; kid; ) {
        nsIWidget* next = kid->GetNextSibling();
        kid->Destroy();
        kid = next;
    }

    // Destroy thebes surface now. Badness can happen if we destroy
    // the surface after its X Window.
    if (mWidget) {
        mWidget->dropReceiver();

        // Call deleteLater instead of delete; Qt still needs the object
        // to be valid even after sending it a Close event.  We could
        // also set WA_DeleteOnClose, but this gives us more control.
        mWidget->deleteLater();
    }
    mWidget = nullptr;

    OnDestroy();

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Show(bool aState)
{
    LOG(("nsWindow::Show [%p] state %d\n", (void *)this, aState));
    if (aState == mIsShown) {
        return NS_OK;
    }

    // Clear our cached resources when the window is hidden.
    if (mIsShown && !aState) {
        ClearCachedResources();
    }

    mIsShown = aState;

    if ((aState && !AreBoundsSane()) || !mWidget) {
        LOG(("\tbounds are insane or window hasn't been created yet\n"));
        mNeedsShow = true;
        return NS_OK;
    }

    if (aState) {
        if (mNeedsMove) {
            NativeResize(mBounds.x, mBounds.y, mBounds.width, mBounds.height,
                         false);
        } else if (mNeedsResize) {
            NativeResize(mBounds.width, mBounds.height, false);
        }
    }
    else {
        // If someone is hiding this widget, clear any needing show flag.
        mNeedsShow = false;
    }

    NativeShow(aState);

    return NS_OK;
}

bool
nsWindow::IsVisible() const
{
    return mIsShown;
}

NS_IMETHODIMP
nsWindow::ConstrainPosition(bool aAllowSlop, int32_t *aX, int32_t *aY)
{
    if (!mWidget) {
        return NS_ERROR_FAILURE;
    }

    int32_t screenWidth  = qApp->primaryScreen()->size().width();
    int32_t screenHeight = qApp->primaryScreen()->size().height();

    if (aAllowSlop) {
        if (*aX < (kWindowPositionSlop - mBounds.width))
            *aX = kWindowPositionSlop - mBounds.width;
        if (*aX > (screenWidth - kWindowPositionSlop))
            *aX = screenWidth - kWindowPositionSlop;
        if (*aY < (kWindowPositionSlop - mBounds.height))
            *aY = kWindowPositionSlop - mBounds.height;
        if (*aY > (screenHeight - kWindowPositionSlop))
            *aY = screenHeight - kWindowPositionSlop;
    } else {
        if (*aX < 0)
            *aX = 0;
        if (*aX > (screenWidth - mBounds.width))
            *aX = screenWidth - mBounds.width;
        if (*aY < 0)
            *aY = 0;
        if (*aY > (screenHeight - mBounds.height))
            *aY = screenHeight - mBounds.height;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Move(double aX, double aY)
{
    LOG(("nsWindow::Move [%p] %f %f\n", (void *)this,
         aX, aY));

    int32_t x = NSToIntRound(aX);
    int32_t y = NSToIntRound(aY);

    if (mIsTopLevel) {
        SetSizeMode(nsSizeMode_Normal);
    }

    if (x == mBounds.x && y == mBounds.y) {
        return NS_OK;
    }

    mNeedsMove = false;

    // update the bounds
    QPoint pos(x, y);
    if (mIsTopLevel) {
        mWidget->setPosition(x, y);
    }
    else if (mWidget) {
        // the position of the widget is set relative to the parent
        // so we map the coordinates accordingly
        pos = mWidget->mapToGlobal(pos);
        mWidget->setPosition(pos);
    }

    mBounds.x = pos.x();
    mBounds.y = pos.y();

    NotifyRollupGeometryChange();
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Resize(double aWidth, double aHeight, bool aRepaint)
{
    mBounds.width = NSToIntRound(aWidth);
    mBounds.height = NSToIntRound(aHeight);

    if (!mWidget)
        return NS_OK;

    if (mIsShown) {
        if (AreBoundsSane()) {
            if (mIsTopLevel || mNeedsShow)
                NativeResize(mBounds.x, mBounds.y,
                             mBounds.width, mBounds.height, aRepaint);
            else
                NativeResize(mBounds.width, mBounds.height, aRepaint);

            // Does it need to be shown because it was previously insane?
            if (mNeedsShow) {
                NativeShow(true);
            }
        }
        else {
            // If someone has set this so that the needs show flag is false
            // and it needs to be hidden, update the flag and hide the
            // window.  This flag will be cleared the next time someone
            // hides the window or shows it.  It also prevents us from
            // calling NativeShow(false) excessively on the window which
            // causes unneeded X traffic.
            if (!mNeedsShow) {
                mNeedsShow = true;
                NativeShow(false);
            }
        }
    }
    else if (AreBoundsSane() && mListenForResizes) {
        // For widgets that we listen for resizes for (widgets created
        // with native parents) we apparently _always_ have to resize.  I
        // dunno why, but apparently we're lame like that.
        NativeResize(mBounds.width, mBounds.height, aRepaint);
    }
    else {
        mNeedsResize = true;
    }

    // synthesize a resize event if this isn't a toplevel
    if (mIsTopLevel || mListenForResizes) {
        nsEventStatus status;
        DispatchResizeEvent(mBounds, status);
    }

    NotifyRollupGeometryChange();
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Resize(double aX, double aY, double aWidth, double aHeight,
                 bool aRepaint)
{
    mBounds.x = NSToIntRound(aX);
    mBounds.y = NSToIntRound(aY);
    mBounds.width = NSToIntRound(aWidth);
    mBounds.height = NSToIntRound(aHeight);

    mPlaced = true;

    if (!mWidget) {
        return NS_OK;
    }

    // Has this widget been set to visible?
    if (mIsShown) {
        // Are the bounds sane?
        if (AreBoundsSane()) {
            // Yep?  Resize the window
            NativeResize(mBounds.x, mBounds.y, mBounds.width, mBounds.height,
                         aRepaint);
            // Does it need to be shown because it was previously insane?
            if (mNeedsShow)
                NativeShow(true);
        }
        else {
            // If someone has set this so that the needs show flag is false
            // and it needs to be hidden, update the flag and hide the
            // window.  This flag will be cleared the next time someone
            // hides the window or shows it.  It also prevents us from
            // calling NativeShow(false) excessively on the window which
            // causes unneeded X traffic.
            if (!mNeedsShow) {
                mNeedsShow = true;
                NativeShow(false);
            }
        }
    }
    // If the widget hasn't been shown, mark the widget as needing to be
    // resized before it is shown
    else if (AreBoundsSane() && mListenForResizes) {
        // For widgets that we listen for resizes for (widgets created
        // with native parents) we apparently _always_ have to resize.  I
        // dunno why, but apparently we're lame like that.
        NativeResize(mBounds.x, mBounds.y, mBounds.width, mBounds.height,
                     aRepaint);
    }
    else {
        mNeedsResize = true;
        mNeedsMove = true;
    }

    if (mIsTopLevel || mListenForResizes) {
        // synthesize a resize event
        nsEventStatus status;
        DispatchResizeEvent(mBounds, status);
    }

    if (aRepaint) {
        mWidget->renderLater();
    }

    NotifyRollupGeometryChange();
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Enable(bool aState)
{
    mEnabled = aState;

    return NS_OK;
}

bool
nsWindow::IsEnabled() const
{
    return mEnabled;
}

NS_IMETHODIMP
nsWindow::SetFocus(bool aRaise)
{
    // Make sure that our owning widget has focus.  If it doesn't try to
    // grab it.  Note that we don't set our focus flag in this case.
    LOGFOCUS(("  SetFocus [%p]\n", (void *)this));

    if (!mWidget) {
        return NS_ERROR_FAILURE;
    }

    if (mWidget->focusObject()) {
        return NS_OK;
    }

    // Because QGraphicsItem cannot get the focus if they are
    // invisible, we look up the chain, for the lowest visible
    // parent and focus that one
    QWindow* realFocusItem = nullptr;
    find_first_visible_parent(mWidget, realFocusItem);

    if (!realFocusItem || realFocusItem->focusObject()) {
        return NS_OK;
    }

    if (aRaise && mWidget) {
        // the raising has to happen on the view widget
        mWidget->raise();
    }

    // XXXndeakin why is this here? It should dispatch only when the OS
    // notifies us.
    DispatchActivateEvent();

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ConfigureChildren(const nsTArray<nsIWidget::Configuration>& aConfigurations)
{
    for (uint32_t i = 0; i < aConfigurations.Length(); ++i) {
        const Configuration& configuration = aConfigurations[i];

        nsWindow* w = static_cast<nsWindow*>(configuration.mChild);
        NS_ASSERTION(w->GetParent() == this,
                     "Configured widget is not a child");

        if (w->mBounds.Size() != configuration.mBounds.Size()) {
            w->Resize(configuration.mBounds.x, configuration.mBounds.y,
                      configuration.mBounds.width, configuration.mBounds.height,
                      true);
        } else if (w->mBounds.TopLeft() != configuration.mBounds.TopLeft()) {
            w->Move(configuration.mBounds.x, configuration.mBounds.y);
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Invalidate(const nsIntRect &aRect)
{
    LOGDRAW(("Invalidate (rect) [%p,%p]: %d %d %d %d\n", (void *)this,
             (void*)mWidget,aRect.x, aRect.y, aRect.width, aRect.height));

    if (!mWidget) {
        return NS_OK;
    }

    mWidget->renderLater();

    return NS_OK;
}

LayoutDeviceIntPoint
nsWindow::WidgetToScreenOffset()
{
    NS_ENSURE_TRUE(mWidget, LayoutDeviceIntPoint(0,0));

    QPoint origin(0, 0);
    origin = mWidget->mapToGlobal(origin);

    return LayoutDeviceIntPoint(origin.x(), origin.y());
}

void*
nsWindow::GetNativeData(uint32_t aDataType)
{
    switch (aDataType) {
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_WIDGET: {
        return mWidget;
    }
    case NS_NATIVE_SHAREABLE_WINDOW: {
        return mWidget ? (void*)mWidget->winId() : nullptr;
    }
    case NS_NATIVE_DISPLAY: {
#ifdef MOZ_X11
        return gfxQtPlatform::GetXDisplay(mWidget);
#endif
        break;
    }
    case NS_NATIVE_PLUGIN_PORT:
    case NS_NATIVE_GRAPHIC:
    case NS_NATIVE_SHELLWIDGET: {
        break;
    }
    default:
        NS_WARNING("nsWindow::GetNativeData called with bad value");
        return nullptr;
    }
    LOG(("nsWindow::%s [%p] aDataType:%i\n", __FUNCTION__, (void *)this, aDataType));
    return nullptr;
}

NS_IMETHODIMP
nsWindow::DispatchEvent(WidgetGUIEvent* aEvent, nsEventStatus& aStatus)
{
#ifdef DEBUG
    debug_DumpEvent(stdout, aEvent->widget, aEvent,
                    nsAutoCString("something"), 0);
#endif

    aStatus = nsEventStatus_eIgnore;

    // send it to the standard callback
    if (mWidgetListener) {
        aStatus = mWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);
    }

    return NS_OK;
}

NS_IMETHODIMP_(void)
nsWindow::SetInputContext(const InputContext& aContext,
                          const InputContextAction& aAction)
{
    NS_ENSURE_TRUE_VOID(mWidget);

    // SetSoftwareKeyboardState uses mInputContext,
    // so, before calling that, record aContext in mInputContext.
    mInputContext = aContext;

    switch (mInputContext.mIMEState.mEnabled) {
        case IMEState::ENABLED:
        case IMEState::PASSWORD:
        case IMEState::PLUGIN:
            SetSoftwareKeyboardState(true, aAction);
            break;
        default:
            SetSoftwareKeyboardState(false, aAction);
            break;
    }
}

NS_IMETHODIMP_(InputContext)
nsWindow::GetInputContext()
{
    mInputContext.mIMEState.mOpen = IMEState::OPEN_STATE_NOT_SUPPORTED;
    // Our qt widget looks like using only one context per process.
    // However, it's better to set the context's pointer.
    mInputContext.mNativeIMEContext = qApp->inputMethod();

    return mInputContext;
}

NS_IMETHODIMP
nsWindow::ReparentNativeWidget(nsIWidget *aNewParent)
{
    NS_PRECONDITION(aNewParent, "");

    MozQWidget* newParent = static_cast<MozQWidget*>(aNewParent->GetNativeData(NS_NATIVE_WINDOW));
    NS_ASSERTION(newParent, "Parent widget has a null native window handle");
    if (mWidget) {
        mWidget->setParent(newParent);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::MakeFullScreen(bool aFullScreen, nsIScreen*)
{
    NS_ENSURE_TRUE(mWidget, NS_ERROR_FAILURE);

    if (aFullScreen) {
        if (mSizeMode != nsSizeMode_Fullscreen) {
            mLastSizeMode = mSizeMode;
        }

        mSizeMode = nsSizeMode_Fullscreen;
        mWidget->showFullScreen();
    }
    else {
        mSizeMode = mLastSizeMode;

        switch (mSizeMode) {
        case nsSizeMode_Maximized:
            mWidget->showMaximized();
            break;
        case nsSizeMode_Minimized:
            mWidget->showMinimized();
            break;
        case nsSizeMode_Normal:
            mWidget->showNormal();
            break;
        default:
            mWidget->showNormal();
            break;
        }
    }

    NS_ASSERTION(mLastSizeMode != nsSizeMode_Fullscreen,
                 "mLastSizeMode should never be fullscreen");
    return nsBaseWidget::MakeFullScreen(aFullScreen);
}

LayerManager*
nsWindow::GetLayerManager(PLayerTransactionChild* aShadowManager,
                          LayersBackend aBackendHint,
                          LayerManagerPersistence aPersistence,
                          bool* aAllowRetaining)
{
    if (!mLayerManager && eTransparencyTransparent == GetTransparencyMode()) {
        mLayerManager = CreateBasicLayerManager();
    }

    return nsBaseWidget::GetLayerManager(aShadowManager, aBackendHint,
                                         aPersistence, aAllowRetaining);
}

void
nsWindow::UserActivity()
{
    if (!mIdleService) {
        mIdleService = do_GetService("@mozilla.org/widget/idleservice;1");
    }

    if (mIdleService) {
        mIdleService->ResetIdleTimeOut(0);
    }
}

uint32_t
nsWindow::GetGLFrameBufferFormat()
{
    if (mLayerManager &&
        mLayerManager->GetBackendType() == mozilla::layers::LayersBackend::LAYERS_OPENGL) {
        return LOCAL_GL_RGB;
    }
    return LOCAL_GL_NONE;
}

TemporaryRef<DrawTarget>
nsWindow::StartRemoteDrawing()
{
    if (!mWidget) {
        return nullptr;
    }

#ifdef MOZ_X11
    Display* dpy = gfxQtPlatform::GetXDisplay(mWidget);
    Screen* screen = DefaultScreenOfDisplay(dpy);
    Visual* defaultVisual = DefaultVisualOfScreen(screen);
    gfxASurface* surf = new gfxXlibSurface(dpy, mWidget->winId(), defaultVisual,
                                           gfxIntSize(mWidget->width(),
                                                      mWidget->height()));

    IntSize size(surf->GetSize().width, surf->GetSize().height);
    if (size.width <= 0 || size.height <= 0) {
        return nullptr;
    }

    return gfxPlatform::GetPlatform()->CreateDrawTargetForSurface(surf, size);
#else
    return nullptr;
#endif
}

NS_IMETHODIMP
nsWindow::SetCursor(nsCursor aCursor)
{
    if (mCursor == aCursor && !mUpdateCursor) {
        return NS_OK;
    }
    mUpdateCursor = false;
    mCursor = aCursor;
    if (mWidget) {
        mWidget->SetCursor(mCursor);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetTitle(const nsAString& aTitle)
{
    QString qStr(QString::fromUtf16((const ushort*)aTitle.BeginReading(), aTitle.Length()));

    mWidget->setTitle(qStr);

    return NS_OK;
}

// EVENTS

void
nsWindow::OnPaint()
{
    LOGDRAW(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    nsIWidgetListener* listener =
        mAttachedWidgetListener ? mAttachedWidgetListener : mWidgetListener;
    if (!listener) {
        return;
    }

    listener->WillPaintWindow(this);

    switch (GetLayerManager()->GetBackendType()) {
        case mozilla::layers::LayersBackend::LAYERS_CLIENT: {
            nsIntRegion region(nsIntRect(0, 0, mWidget->width(), mWidget->height()));
            listener->PaintWindow(this, region);
            break;
        }
        default:
            NS_ERROR("Invalid layer manager");
    }

    listener->DidPaintWindow();
}

nsEventStatus
nsWindow::moveEvent(QMoveEvent* aEvent)
{
    LOG(("configure event [%p] %d %d\n", (void *)this,
        aEvent->pos().x(),  aEvent->pos().y()));

    // can we shortcut?
    if (!mWidget || !mWidgetListener)
        return nsEventStatus_eIgnore;

    if ((mBounds.x == aEvent->pos().x() &&
         mBounds.y == aEvent->pos().y()))
    {
        return nsEventStatus_eIgnore;
    }

    NotifyWindowMoved(aEvent->pos().x(), aEvent->pos().y());
    return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus
nsWindow::resizeEvent(QResizeEvent* aEvent)
{
    nsIntRect rect;

    // Generate XPFE resize event
    GetBounds(rect);

    rect.width = aEvent->size().width();
    rect.height = aEvent->size().height();

    mBounds.width = rect.width;
    mBounds.height = rect.height;

    nsEventStatus status;
    DispatchResizeEvent(rect, status);
    return status;
}

nsEventStatus
nsWindow::mouseMoveEvent(QMouseEvent* aEvent)
{
    UserActivity();

    mMoveEvent.pos = aEvent->pos();
    mMoveEvent.modifiers = aEvent->modifiers();
    mMoveEvent.needDispatch = true;
    DispatchMotionToMainThread();

    return nsEventStatus_eIgnore;
}

static void
InitMouseEvent(WidgetMouseEvent& aMouseEvent, QMouseEvent* aEvent,
               int aClickCount)
{
    aMouseEvent.refPoint.x = nscoord(aEvent->pos().x());
    aMouseEvent.refPoint.y = nscoord(aEvent->pos().y());

    aMouseEvent.InitBasicModifiers(aEvent->modifiers() & Qt::ControlModifier,
                                   aEvent->modifiers() & Qt::AltModifier,
                                   aEvent->modifiers() & Qt::ShiftModifier,
                                   aEvent->modifiers() & Qt::MetaModifier);
    aMouseEvent.clickCount = aClickCount;

    switch (aEvent->button()) {
    case Qt::LeftButton:
        aMouseEvent.button = WidgetMouseEvent::eLeftButton;
        break;
    case Qt::RightButton:
        aMouseEvent.button = WidgetMouseEvent::eRightButton;
        break;
    case Qt::MiddleButton:
        aMouseEvent.button = WidgetMouseEvent::eMiddleButton;
        break;
    default:
        break;
    }
}

static bool
IsAcceptedButton(Qt::MouseButton button)
{
    switch (button) {
    case Qt::LeftButton:
    case Qt::RightButton:
    case Qt::MiddleButton:
        return true;
    default:
        return false;
    }
}

nsEventStatus
nsWindow::mousePressEvent(QMouseEvent* aEvent)
{
    // The user has done something.
    UserActivity();

    QPoint pos = aEvent->pos();

    // we check against the widgets geometry, so use parent coordinates
    // for the check
    if (mWidget)
        pos = mWidget->mapToGlobal(pos);

    if (CheckForRollup(pos.x(), pos.y(), false))
        return nsEventStatus_eIgnore;

    if (!IsAcceptedButton(aEvent->button())) {
        if (aEvent->button() == Qt::BackButton)
            return DispatchCommandEvent(nsGkAtoms::Back);
        if (aEvent->button() == Qt::ForwardButton)
            return DispatchCommandEvent(nsGkAtoms::Forward);
        return nsEventStatus_eIgnore;
    }

    WidgetMouseEvent event(true, NS_MOUSE_BUTTON_DOWN, this,
                           WidgetMouseEvent::eReal);
    InitMouseEvent(event, aEvent, 1);
    nsEventStatus status = DispatchEvent(&event);

    // Right click on linux should also pop up a context menu.
    if (event.button == WidgetMouseEvent::eRightButton &&
        MOZ_LIKELY(!mIsDestroyed)) {
        WidgetMouseEvent contextMenuEvent(true, NS_CONTEXTMENU, this,
                                          WidgetMouseEvent::eReal);
        InitMouseEvent(contextMenuEvent, aEvent, 1);
        DispatchEvent(&contextMenuEvent, status);
    }

    return status;
}

nsEventStatus
nsWindow::mouseReleaseEvent(QMouseEvent* aEvent)
{
    // The user has done something.
    UserActivity();

    if (!IsAcceptedButton(aEvent->button()))
        return nsEventStatus_eIgnore;

    WidgetMouseEvent event(true, NS_MOUSE_BUTTON_UP, this,
                           WidgetMouseEvent::eReal);
    InitMouseEvent(event, aEvent, 1);
    return DispatchEvent(&event);
}

nsEventStatus
nsWindow::mouseDoubleClickEvent(QMouseEvent* aEvent)
{
    // The user has done something.
    UserActivity();

    if (!IsAcceptedButton(aEvent->button()))
        return nsEventStatus_eIgnore;

    WidgetMouseEvent event(true, NS_MOUSE_DOUBLECLICK, this,
                           WidgetMouseEvent::eReal);
    InitMouseEvent(event, aEvent, 2);
    return DispatchEvent(&event);
}

nsEventStatus
nsWindow::focusInEvent(QFocusEvent* aEvent)
{
    LOGFOCUS(("OnFocusInEvent [%p]\n", (void *)this));

    if (!mWidget) {
        return nsEventStatus_eIgnore;
    }

    DispatchActivateEventOnTopLevelWindow();

    LOGFOCUS(("Events sent from focus in event [%p]\n", (void *)this));
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::focusOutEvent(QFocusEvent* aEvent)
{
    LOGFOCUS(("OnFocusOutEvent [%p]\n", (void *)this));

    if (!mWidget) {
        return nsEventStatus_eIgnore;
    }

    DispatchDeactivateEventOnTopLevelWindow();

    LOGFOCUS(("Done with container focus out [%p]\n", (void *)this));
    return nsEventStatus_eIgnore;
}

static bool
IsContextMenuKeyEvent(const QKeyEvent* aQEvent)
{
    if (aQEvent->modifiers() & (Qt::ControlModifier |
                                Qt::AltModifier |
                                Qt::MetaModifier)) {
        return false;
    }

    bool isShift = aQEvent->modifiers() & Qt::ShiftModifier;
    uint32_t keyCode = QtKeyCodeToDOMKeyCode(aQEvent->key());
    return (keyCode == NS_VK_F10 && isShift) ||
           (keyCode == NS_VK_CONTEXT_MENU && !isShift);
}

static void
InitKeyEvent(WidgetKeyboardEvent& aEvent, QKeyEvent* aQEvent)
{
    aEvent.InitBasicModifiers(aQEvent->modifiers() & Qt::ControlModifier,
                              aQEvent->modifiers() & Qt::AltModifier,
                              aQEvent->modifiers() & Qt::ShiftModifier,
                              aQEvent->modifiers() & Qt::MetaModifier);

    aEvent.mIsRepeat =
        (aEvent.message == NS_KEY_DOWN || aEvent.message == NS_KEY_PRESS) &&
        aQEvent->isAutoRepeat();
    aEvent.time = 0;

    if (sAltGrModifier) {
        aEvent.modifiers |= (MODIFIER_CONTROL | MODIFIER_ALT);
    }

    if (aQEvent->text().length() && aQEvent->text()[0].isPrint()) {
        aEvent.charCode = (int32_t) aQEvent->text()[0].unicode();
        aEvent.keyCode = 0;
        aEvent.mKeyNameIndex = KEY_NAME_INDEX_PrintableKey;
    } else {
        aEvent.charCode = 0;
        aEvent.keyCode = QtKeyCodeToDOMKeyCode(aQEvent->key());
        aEvent.mKeyNameIndex = QtKeyCodeToDOMKeyNameIndex(aQEvent->key());
    }

    aEvent.mCodeNameIndex = ScanCodeToDOMCodeNameIndex(aQEvent->nativeScanCode());

    // The transformations above and in qt for the keyval are not invertible
    // so link to the QKeyEvent (which will vanish soon after return from the
    // event callback) to give plugins access to hardware_keycode and state.
    // (An XEvent would be nice but the QKeyEvent is good enough.)
    aEvent.mPluginEvent.Copy(*aQEvent);
}

nsEventStatus
nsWindow::keyPressEvent(QKeyEvent* aEvent)
{
    LOGFOCUS(("OnKeyPressEvent [%p]\n", (void *)this));

    // The user has done something.
    UserActivity();

    if (aEvent->key() == Qt::Key_AltGr) {
        sAltGrModifier = true;
    }

    // Before we dispatch a key, check if it's the context menu key.
    // If so, send a context menu key event instead.
    if (IsContextMenuKeyEvent(aEvent)) {
        WidgetMouseEvent contextMenuEvent(true, NS_CONTEXTMENU, this,
                                          WidgetMouseEvent::eReal,
                                          WidgetMouseEvent::eContextMenuKey);
        return DispatchEvent(&contextMenuEvent);
    }

    //:TODO: fix shortcuts hebrew for non X11,
    //see Bug 562195##51

    uint32_t domKeyCode = QtKeyCodeToDOMKeyCode(aEvent->key());

    if (!aEvent->isAutoRepeat() && !IsKeyDown(domKeyCode)) {
        SetKeyDownFlag(domKeyCode);

        WidgetKeyboardEvent downEvent(true, NS_KEY_DOWN, this);
        InitKeyEvent(downEvent, aEvent);

        nsEventStatus status = DispatchEvent(&downEvent);

        // DispatchEvent can Destroy us (bug 378273)
        if (MOZ_UNLIKELY(mIsDestroyed)) {
            qWarning() << "Returning[" << __LINE__ << "]: " << "Window destroyed";
            return status;
        }

        // If prevent default on keydown, don't dispatch keypress event
        if (status == nsEventStatus_eConsumeNoDefault) {
            return nsEventStatus_eConsumeNoDefault;
        }
    }

    // Don't pass modifiers as NS_KEY_PRESS events.
    // Instead of selectively excluding some keys from NS_KEY_PRESS events,
    // we instead selectively include (as per MSDN spec
    // ( http://msdn.microsoft.com/en-us/library/system.windows.forms.control.keypress%28VS.71%29.aspx );
    // no official spec covers KeyPress events).
    if (aEvent->key() == Qt::Key_Shift   ||
        aEvent->key() == Qt::Key_Control ||
        aEvent->key() == Qt::Key_Meta    ||
        aEvent->key() == Qt::Key_Alt     ||
        aEvent->key() == Qt::Key_AltGr) {
        return nsEventStatus_eIgnore;
    }

    // Look for specialized app-command keys
    switch (aEvent->key()) {
        case Qt::Key_Back:
            return DispatchCommandEvent(nsGkAtoms::Back);
        case Qt::Key_Forward:
            return DispatchCommandEvent(nsGkAtoms::Forward);
        case Qt::Key_Refresh:
            return DispatchCommandEvent(nsGkAtoms::Reload);
        case Qt::Key_Stop:
            return DispatchCommandEvent(nsGkAtoms::Stop);
        case Qt::Key_Search:
            return DispatchCommandEvent(nsGkAtoms::Search);
        case Qt::Key_Favorites:
            return DispatchCommandEvent(nsGkAtoms::Bookmarks);
        case Qt::Key_HomePage:
            return DispatchCommandEvent(nsGkAtoms::Home);
        case Qt::Key_Copy:
        case Qt::Key_F16: // F16, F20, F18, F14 are old keysyms for Copy Cut Paste Undo
            return DispatchContentCommandEvent(NS_CONTENT_COMMAND_COPY);
        case Qt::Key_Cut:
        case Qt::Key_F20:
            return DispatchContentCommandEvent(NS_CONTENT_COMMAND_CUT);
        case Qt::Key_Paste:
        case Qt::Key_F18:
        case Qt::Key_F9:
            return DispatchContentCommandEvent(NS_CONTENT_COMMAND_PASTE);
        case Qt::Key_F14:
            return DispatchContentCommandEvent(NS_CONTENT_COMMAND_UNDO);
    }

    // Qt::Key_Redo and Qt::Key_Undo are not available yet.
    if (aEvent->nativeVirtualKey() == 0xff66) {
        return DispatchContentCommandEvent(NS_CONTENT_COMMAND_REDO);
    }
    if (aEvent->nativeVirtualKey() == 0xff65) {
        return DispatchContentCommandEvent(NS_CONTENT_COMMAND_UNDO);
    }

    WidgetKeyboardEvent event(true, NS_KEY_PRESS, this);
    InitKeyEvent(event, aEvent);
    // Seend the key press event
    return DispatchEvent(&event);
}

nsEventStatus
nsWindow::keyReleaseEvent(QKeyEvent* aEvent)
{
    LOGFOCUS(("OnKeyReleaseEvent [%p]\n", (void *)this));

    // The user has done something.
    UserActivity();

    if (IsContextMenuKeyEvent(aEvent)) {
        // er, what do we do here? DoDefault or NoDefault?
        return nsEventStatus_eConsumeDoDefault;
    }

    // send the key event as a key up event
    WidgetKeyboardEvent event(true, NS_KEY_UP, this);
    InitKeyEvent(event, aEvent);

    if (aEvent->key() == Qt::Key_AltGr) {
        sAltGrModifier = false;
    }

    // unset the key down flag
    ClearKeyDownFlag(event.keyCode);

    return DispatchEvent(&event);
}

nsEventStatus
nsWindow::wheelEvent(QWheelEvent* aEvent)
{
    // check to see if we should rollup
    WidgetWheelEvent wheelEvent(true, NS_WHEEL_WHEEL, this);
    wheelEvent.deltaMode = nsIDOMWheelEvent::DOM_DELTA_LINE;

    // negative values for aEvent->delta indicate downward scrolling;
    // this is opposite Gecko usage.
    // TODO: Store the unused delta values due to fraction round and add it
    //       to next event.  The stored values should be reset by other
    //       direction scroll event.
    int32_t delta = (int)(aEvent->delta() / WHEEL_DELTA) * -3;

    switch (aEvent->orientation()) {
    case Qt::Vertical:
        wheelEvent.deltaY = wheelEvent.lineOrPageDeltaY = delta;
        break;
    case Qt::Horizontal:
        wheelEvent.deltaX = wheelEvent.lineOrPageDeltaX = delta;
        break;
    default:
        Q_ASSERT(0);
        break;
    }

    wheelEvent.refPoint.x = nscoord(aEvent->pos().x());
    wheelEvent.refPoint.y = nscoord(aEvent->pos().y());

    wheelEvent.InitBasicModifiers(aEvent->modifiers() & Qt::ControlModifier,
                                  aEvent->modifiers() & Qt::AltModifier,
                                  aEvent->modifiers() & Qt::ShiftModifier,
                                  aEvent->modifiers() & Qt::MetaModifier);
    wheelEvent.time = 0;

    return DispatchEvent(&wheelEvent);
}

nsEventStatus
nsWindow::showEvent(QShowEvent *)
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__,(void *)this));
    mVisible = true;
    return nsEventStatus_eConsumeDoDefault;
}

nsEventStatus
nsWindow::hideEvent(QHideEvent *)
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__,(void *)this));
    mVisible = false;
    return nsEventStatus_eConsumeDoDefault;
}

nsEventStatus nsWindow::touchEvent(QTouchEvent* aEvent)
{
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::tabletEvent(QTabletEvent* aEvent)
{
    LOGFOCUS(("nsWindow::%s [%p]\n", __FUNCTION__, (void *)this));
    return nsEventStatus_eIgnore;
}

//  Helpers

nsEventStatus
nsWindow::DispatchEvent(WidgetGUIEvent* aEvent)
{
    nsEventStatus status;
    DispatchEvent(aEvent, status);
    return status;
}

void
nsWindow::DispatchActivateEvent(void)
{
    if (mWidgetListener) {
        mWidgetListener->WindowActivated();
    }
}

void
nsWindow::DispatchDeactivateEvent(void)
{
    if (mWidgetListener) {
        mWidgetListener->WindowDeactivated();
    }
}

void
nsWindow::DispatchActivateEventOnTopLevelWindow(void)
{
    nsWindow* topLevelWindow = static_cast<nsWindow*>(GetTopLevelWidget());
    if (topLevelWindow != nullptr) {
        topLevelWindow->DispatchActivateEvent();
    }
}

void
nsWindow::DispatchDeactivateEventOnTopLevelWindow(void)
{
    nsWindow* topLevelWindow = static_cast<nsWindow*>(GetTopLevelWidget());
    if (topLevelWindow != nullptr) {
        topLevelWindow->DispatchDeactivateEvent();
    }
}

void
nsWindow::DispatchResizeEvent(nsIntRect &aRect, nsEventStatus &aStatus)
{
    aStatus = nsEventStatus_eIgnore;
    if (mWidgetListener &&
        mWidgetListener->WindowResized(this, aRect.width, aRect.height)) {
        aStatus = nsEventStatus_eConsumeNoDefault;
    }
}

///////////////////////////////////// OLD GECKO ECENTS need to Sort ///////////////////

void
nsWindow::ClearCachedResources()
{
    if (mLayerManager &&
        mLayerManager->GetBackendType() == mozilla::layers::LayersBackend::LAYERS_BASIC) {
        mLayerManager->ClearCachedResources();
    }
    for (nsIWidget* kid = mFirstChild; kid; ) {
        nsIWidget* next = kid->GetNextSibling();
        static_cast<nsWindow*>(kid)->ClearCachedResources();
        kid = next;
    }
}

NS_IMETHODIMP
nsWindow::SetParent(nsIWidget *aNewParent)
{
    NS_ENSURE_ARG_POINTER(aNewParent);

    nsCOMPtr<nsIWidget> kungFuDeathGrip(this);
    nsIWidget* parent = GetParent();
    if (parent) {
        parent->RemoveChild(this);
    }
    ReparentNativeWidget(aNewParent);
    aNewParent->AddChild(this);
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetModal(bool aModal)
{
    LOG(("nsWindow::SetModal [%p] %d, widget[%p]\n", (void *)this, aModal, mWidget));
    if (mWidget) {
        mWidget->setModality(aModal ? Qt::WindowModal : Qt::NonModal);
    }

    return NS_OK;
}


NS_IMETHODIMP
nsWindow::PlaceBehind(nsTopLevelWidgetZPlacement  aPlacement,
                      nsIWidget                  *aWidget,
                      bool                        aActivate)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::SetSizeMode(int32_t aMode)
{
    nsresult rv;

    LOG(("nsWindow::SetSizeMode [%p] %d\n", (void *)this, aMode));
    if (aMode != nsSizeMode_Minimized) {
        mWidget->requestActivate();
    }

    // Save the requested state.
    rv = nsBaseWidget::SetSizeMode(aMode);

    // return if there's no shell or our current state is the same as
    // the mode we were just set to.
    if (!mWidget || mSizeState == mSizeMode) {
        return rv;
    }

    switch (aMode) {
    case nsSizeMode_Maximized:
        mWidget->showMaximized();
        break;
    case nsSizeMode_Minimized:
        mWidget->showMinimized();
        break;
    case nsSizeMode_Fullscreen:
        mWidget->showFullScreen();
        break;

    default:
        // nsSizeMode_Normal, really.
        mWidget->show();
        break;
    }

    mSizeState = mSizeMode;

    return rv;
}

// Helper function to recursively find the first parent item that
// is still visible (QGraphicsItem can be hidden even if they are
// set to visible if one of their ancestors is invisible)
/* static */
void find_first_visible_parent(QWindow* aItem, QWindow*& aVisibleItem)
{
    NS_ENSURE_TRUE_VOID(aItem);

    aVisibleItem = nullptr;
    QWindow* parItem = nullptr;
    while (!aVisibleItem) {
        if (aItem->isVisible()) {
            aVisibleItem = aItem;
        }
        else {
            parItem = aItem->parent();
            if (parItem) {
                aItem = parItem;
            }
            else {
                aItem->setVisible(true);
                aVisibleItem = aItem;
            }
        }
    }
}

NS_IMETHODIMP
nsWindow::GetScreenBounds(nsIntRect &aRect)
{
    aRect = gfx::IntRect(gfx::IntPoint(0, 0), mBounds.Size());
    if (mIsTopLevel) {
        QPoint pos = mWidget->position();
        aRect.MoveTo(pos.x(), pos.y());
    }
    else {
        aRect.MoveTo(WidgetToScreenOffsetUntyped());
    }
    LOG(("GetScreenBounds %d %d | %d %d | %d %d\n",
         aRect.x, aRect.y,
         mBounds.width, mBounds.height,
         aRect.width, aRect.height));
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetIcon(const nsAString& aIconSpec)
{
    if (!mWidget)
        return NS_OK;

    nsCOMPtr<nsIFile> iconFile;
    nsAutoCString path;
    nsTArray<nsCString> iconList;

    // Look for icons with the following suffixes appended to the base name.
    // The last two entries (for the old XPM format) will be ignored unless
    // no icons are found using the other suffixes. XPM icons are depricated.

    const char extensions[6][7] = { ".png", "16.png", "32.png", "48.png",
                                    ".xpm", "16.xpm" };

    for (uint32_t i = 0; i < ArrayLength(extensions); i++) {
        // Don't bother looking for XPM versions if we found a PNG.
        if (i == ArrayLength(extensions) - 2 && iconList.Length())
            break;

        nsAutoString extension;
        extension.AppendASCII(extensions[i]);

        ResolveIconName(aIconSpec, extension, getter_AddRefs(iconFile));
        if (iconFile) {
            iconFile->GetNativePath(path);
            iconList.AppendElement(path);
        }
    }

    // leave the default icon intact if no matching icons were found
    if (iconList.Length() == 0)
        return NS_OK;

    return SetWindowIconList(iconList);
}

NS_IMETHODIMP
nsWindow::CaptureMouse(bool aCapture)
{
    LOG(("CaptureMouse %p\n", (void *)this));

    if (!mWidget)
        return NS_OK;

    mWidget->setMouseGrabEnabled(aCapture);

    return NS_OK;
}

bool
nsWindow::CheckForRollup(double aMouseX, double aMouseY,
                         bool aIsWheel)
{
    nsIRollupListener* rollupListener = GetActiveRollupListener();
    nsCOMPtr<nsIWidget> rollupWidget;
    if (rollupListener) {
        rollupWidget = rollupListener->GetRollupWidget();
    }
    if (!rollupWidget) {
        nsBaseWidget::gRollupListener = nullptr;
        return false;
    }

    bool retVal = false;
    MozQWidget *currentPopup =
        (MozQWidget *)rollupWidget->GetNativeData(NS_NATIVE_WINDOW);
    if (!is_mouse_in_window(currentPopup, aMouseX, aMouseY)) {
        bool rollup = true;
        if (aIsWheel) {
            rollup = rollupListener->ShouldRollupOnMouseWheelEvent();
            retVal = true;
        }
        // if we're dealing with menus, we probably have submenus and
        // we don't want to rollup if the clickis in a parent menu of
        // the current submenu
        uint32_t popupsToRollup = UINT32_MAX;
        if (rollupListener) {
            nsAutoTArray<nsIWidget*, 5> widgetChain;
            uint32_t sameTypeCount = rollupListener->GetSubmenuWidgetChain(&widgetChain);
            for (uint32_t i=0; i<widgetChain.Length(); ++i) {
                nsIWidget* widget =  widgetChain[i];
                MozQWidget* currWindow =
                    (MozQWidget*) widget->GetNativeData(NS_NATIVE_WINDOW);
                if (is_mouse_in_window(currWindow, aMouseX, aMouseY)) {
                  if (i < sameTypeCount) {
                    rollup = false;
                  }
                  else {
                    popupsToRollup = sameTypeCount;
                  }
                  break;
                }
            } // foreach parent menu widget
        } // if rollup listener knows about menus

        // if we've determined that we should still rollup, do it.
        if (rollup) {
            nsIntPoint pos(aMouseX, aMouseY);
            retVal = rollupListener->Rollup(popupsToRollup, true, &pos, nullptr);
        }
    }

    return retVal;
}

/* static */
bool
is_mouse_in_window (MozQWidget* aWindow, double aMouseX, double aMouseY)
{
    return aWindow->geometry().contains(aMouseX, aMouseY);
}

NS_IMETHODIMP
nsWindow::GetAttention(int32_t aCycleCount)
{
    LOG(("nsWindow::GetAttention [%p]\n", (void *)this));
    return NS_ERROR_NOT_IMPLEMENTED;
}



nsEventStatus
nsWindow::OnCloseEvent(QCloseEvent *aEvent)
{
    if (!mWidgetListener)
        return nsEventStatus_eIgnore;
    mWidgetListener->RequestWindowClose(this);
    return nsEventStatus_eConsumeNoDefault;
}


inline bool
is_latin_shortcut_key(quint32 aKeyval)
{
    return ((Qt::Key_0 <= aKeyval && aKeyval <= Qt::Key_9) ||
            (Qt::Key_A <= aKeyval && aKeyval <= Qt::Key_Z));
}

nsEventStatus
nsWindow::DispatchCommandEvent(nsIAtom* aCommand)
{
    WidgetCommandEvent event(true, nsGkAtoms::onAppCommand, aCommand, this);

    nsEventStatus status;
    DispatchEvent(&event, status);

    return status;
}

nsEventStatus
nsWindow::DispatchContentCommandEvent(int32_t aMsg)
{
    WidgetContentCommandEvent event(true, aMsg, this);

    nsEventStatus status;
    DispatchEvent(&event, status);

    return status;
}


static void
GetBrandName(nsXPIDLString& brandName)
{
    nsCOMPtr<nsIStringBundleService> bundleService =
        mozilla::services::GetStringBundleService();

    nsCOMPtr<nsIStringBundle> bundle;
    if (bundleService) {
        bundleService->CreateBundle(
            "chrome://branding/locale/brand.properties",
            getter_AddRefs(bundle));
    }

    if (bundle) {
        bundle->GetStringFromName(
            MOZ_UTF16("brandShortName"),
            getter_Copies(brandName));
    }

    if (brandName.IsEmpty()) {
        brandName.AssignLiteral(MOZ_UTF16("Mozilla"));
    }
}

NS_IMETHODIMP
nsWindow::SetWindowClass(const nsAString &xulWinType)
{
    if (!mWidget) {
        return NS_ERROR_FAILURE;
    }

    nsXPIDLString brandName;
    GetBrandName(brandName);

#ifdef MOZ_X11
    XClassHint *class_hint = XAllocClassHint();
    if (!class_hint) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    const char *role = nullptr;
    class_hint->res_name = ToNewCString(xulWinType);
    if (!class_hint->res_name) {
        XFree(class_hint);
        return NS_ERROR_OUT_OF_MEMORY;
    }
    class_hint->res_class = ToNewCString(brandName);
    if (!class_hint->res_class) {
        free(class_hint->res_name);
        XFree(class_hint);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    // Parse res_name into a name and role. Characters other than
    // [A-Za-z0-9_-] are converted to '_'. Anything after the first
    // colon is assigned to role; if there's no colon, assign the
    // whole thing to both role and res_name.
    for (char *c = class_hint->res_name; *c; c++) {
        if (':' == *c) {
            *c = 0;
            role = c + 1;
        }
        else if (!isascii(*c) || (!isalnum(*c) && ('_' != *c) && ('-' != *c)))
            *c = '_';
    }
    class_hint->res_name[0] = toupper(class_hint->res_name[0]);
    if (!role) role = class_hint->res_name;

    QWindow *widget = mWidget;
    // If widget not show, handle might be null
    if (widget && widget->winId()) {
        XSetClassHint(gfxQtPlatform::GetXDisplay(widget),
                      widget->winId(),
                      class_hint);
    }

    free(class_hint->res_class);
    free(class_hint->res_name);
    XFree(class_hint);
#endif

    return NS_OK;
}

void
nsWindow::NativeResize(int32_t aWidth, int32_t aHeight, bool    aRepaint)
{
    LOG(("nsWindow::NativeResize [%p] %d %d\n", (void *)this,
         aWidth, aHeight));

    mNeedsResize = false;

    mWidget->resize(aWidth, aHeight);

    if (aRepaint) {
        mWidget->renderLater();
    }
}

void
nsWindow::NativeResize(int32_t aX, int32_t aY,
                       int32_t aWidth, int32_t aHeight,
                       bool    aRepaint)
{
    LOG(("nsWindow::NativeResize [%p] %d %d %d %d\n", (void *)this,
         aX, aY, aWidth, aHeight));

    mNeedsResize = false;
    mNeedsMove = false;

    mWidget->setGeometry(aX, aY, aWidth, aHeight);

    if (aRepaint) {
        mWidget->renderLater();
    }
}

void
nsWindow::NativeShow(bool aAction)
{
    if (aAction) {
        // On e10s, we never want the child process or plugin process
        // to go fullscreen because if we do the window because visible
        // do to disabled Qt-Xembed
        mWidget->show();
        // unset our flag now that our window has been shown
        mNeedsShow = false;
    }
    else {
        mWidget->hide();
    }
}

NS_IMETHODIMP
nsWindow::SetHasTransparentBackground(bool aTransparent)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::GetHasTransparentBackground(bool& aTransparent)
{
    aTransparent = mIsTransparent;
    return NS_OK;
}

void *
nsWindow::SetupPluginPort(void)
{
    NS_WARNING("Not implemented");
    return nullptr;
}

nsresult
nsWindow::SetWindowIconList(const nsTArray<nsCString> &aIconList)
{
    QIcon icon;

    for (uint32_t i = 0; i < aIconList.Length(); ++i) {
        const char *path = aIconList[i].get();
        LOG(("window [%p] Loading icon from %s\n", (void *)this, path));
        icon.addFile(path);
    }

    mWidget->setIcon(icon);

    return NS_OK;
}

void
nsWindow::SetDefaultIcon(void)
{
    SetIcon(NS_LITERAL_STRING("default"));
}

void nsWindow::QWidgetDestroyed()
{
    mWidget = nullptr;
}


NS_IMETHODIMP
nsWindow::HideWindowChrome(bool aShouldHide)
{
    if (!mWidget) {
        // Nothing to hide
        return NS_ERROR_FAILURE;
    }

    // Sawfish, metacity, and presumably other window managers get
    // confused if we change the window decorations while the window
    // is visible.
    bool wasVisible = false;
    if (mWidget->isVisible()) {
        NativeShow(false);
        wasVisible = true;
    }

    if (wasVisible) {
        NativeShow(true);
    }

    return NS_OK;
}

//////////////////////////////////////////////////////////////////////

NS_IMETHODIMP_(bool)
nsWindow::HasGLContext()
{
    return false;
}


nsIWidget*
nsWindow::GetParent(void)
{
    return mParent;
}

float
nsWindow::GetDPI()
{
    return qApp->primaryScreen()->logicalDotsPerInch();
}

void
nsWindow::OnDestroy(void)
{
    if (mOnDestroyCalled) {
        return;
    }

    mOnDestroyCalled = true;

    // release references to children and device context
    nsBaseWidget::OnDestroy();

    // let go of our parent
    mParent = nullptr;

    nsCOMPtr<nsIWidget> kungFuDeathGrip = this;
    NotifyWindowDestroyed();
}

bool
nsWindow::AreBoundsSane(void)
{
    if (mBounds.width > 0 && mBounds.height > 0) {
        return true;
    }

    return false;
}

void
nsWindow::SetSoftwareKeyboardState(bool aOpen,
                                   const InputContextAction& aAction)
{
    if (aOpen) {
        NS_ENSURE_TRUE_VOID(mInputContext.mIMEState.mEnabled !=
                            IMEState::DISABLED);

        // Ensure that opening the virtual keyboard is allowed for this specific
        // InputContext depending on the content.ime.strict.policy pref
        if (mInputContext.mIMEState.mEnabled != IMEState::PLUGIN &&
            Preferences::GetBool("content.ime.strict_policy", false) &&
            !aAction.ContentGotFocusByTrustedCause() &&
            !aAction.UserMightRequestOpenVKB()) {
            return;
        }
    }

    if (aOpen) {
        qApp->inputMethod()->show();
    } else {
        qApp->inputMethod()->hide();
    }

    return;
}


void
nsWindow::ProcessMotionEvent()
{
    if (mMoveEvent.needDispatch) {
        WidgetMouseEvent event(true, NS_MOUSE_MOVE, this,
                               WidgetMouseEvent::eReal);

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

