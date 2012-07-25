/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include <QtOpenGL/QGLWidget>
#include <QtOpenGL/QGLContext>
#include <QApplication>
#include <QDesktopWidget>
#include <QtGui/QCursor>
#include <QIcon>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsSceneResizeEvent>
#include <QStyleOptionGraphicsItem>
#include <QPaintEngine>
#include <QMimeData>

#include <QtCore/QDebug>
#include <QtCore/QEvent>
#include <QtCore/QVariant>
#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0))
#include <QPinchGesture>
#include <QGestureRecognizer>
#include "mozSwipeGesture.h"
static Qt::GestureType gSwipeGestureId = Qt::CustomGesture;

// How many milliseconds mouseevents are blocked after receiving
// multitouch.
static const float GESTURES_BLOCK_MOUSE_FOR = 200;
#ifdef MOZ_ENABLE_QTMOBILITY
#include <QtSensors/QOrientationSensor>
using namespace QtMobility;
#endif // MOZ_ENABLE_QTMOBILITY
#endif // QT version check 4.6

#ifdef MOZ_X11
#include <X11/Xlib.h>
#endif //MOZ_X11

#include "nsXULAppAPI.h"

#include "prlink.h"

#include "nsWindow.h"
#include "mozqwidget.h"

#ifdef MOZ_ENABLE_QTMOBILITY
#include "mozqorientationsensorfilter.h"
#endif

#include "nsIdleService.h"
#include "nsRenderingContext.h"
#include "nsIRollupListener.h"
#include "nsWidgetsCID.h"
#include "nsQtKeyUtils.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"

#include "nsIStringBundle.h"
#include "nsGfxCIID.h"

#include "imgIContainer.h"
#include "nsGfxCIID.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsAutoPtr.h"

#include "gfxQtPlatform.h"
#ifdef MOZ_X11
#include "gfxXlibSurface.h"
#endif
#include "gfxQPainterSurface.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"

#include "nsIDOMSimpleGestureEvent.h" //Gesture support

#if MOZ_PLATFORM_MAEMO > 5
#include "nsIDOMWindow.h"
#include "nsIDOMElement.h"
#include "nsIFocusManager.h"
#endif

#ifdef MOZ_X11
#include "keysym2ucs.h"
#if MOZ_PLATFORM_MAEMO == 6
#include <X11/Xatom.h>
static Atom sPluginIMEAtom = nsnull;
#define PLUGIN_VKB_REQUEST_PROP "_NPAPI_PLUGIN_REQUEST_VKB"
#include <QThread>
#endif
#endif //MOZ_X11

#define GLdouble_defined 1
#include "Layers.h"
#include "LayerManagerOGL.h"
#include "nsFastStartupQt.h"

// If embedding clients want to create widget without real parent window
// then nsIBaseWindow->Init() should have parent argument equal to PARENTLESS_WIDGET
#define PARENTLESS_WIDGET (void*)0x13579

#include "nsShmImage.h"
extern "C" {
#define PIXMAN_DONT_DEFINE_STDINT
#include "pixman.h"
}

using namespace mozilla;
using namespace mozilla::widget;

// Cached offscreen surface
static nsRefPtr<gfxASurface> gBufferSurface;
#ifdef MOZ_HAVE_SHMIMAGE
// If we're using xshm rendering, mThebesSurface wraps gShmImage
nsRefPtr<nsShmImage> gShmImage;
#endif

static int gBufferPixmapUsageCount = 0;
static gfxIntSize gBufferMaxSize(0, 0);

// initialization static functions 
static nsresult    initialize_prefs        (void);

static NS_DEFINE_IID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

#define NS_WINDOW_TITLE_MAX_LENGTH 4095

#define kWindowPositionSlop 20

// Qt
static const int WHEEL_DELTA = 120;
static bool gGlobalsInitialized = false;

static nsIRollupListener*          gRollupListener;
static nsWeakPtr                   gRollupWindow;
static bool                        gConsumeRollupEvent;

static bool       check_for_rollup(double aMouseX, double aMouseY,
                                   bool aIsWheel);
static bool
is_mouse_in_window (MozQWidget* aWindow, double aMouseX, double aMouseY);

static bool sAltGrModifier = false;

#ifdef MOZ_ENABLE_QTMOBILITY
static QOrientationSensor *gOrientation = nsnull;
static MozQOrientationSensorFilter gOrientationFilter;
#endif

static bool
isContextMenuKeyEvent(const QKeyEvent *qe)
{
    PRUint32 kc = QtKeyCodeToDOMKeyCode(qe->key());
    if (qe->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        return false;

    bool isShift = qe->modifiers() & Qt::ShiftModifier;
    return (kc == NS_VK_F10 && isShift) ||
        (kc == NS_VK_CONTEXT_MENU && !isShift);
}

static void
InitKeyEvent(nsKeyEvent &aEvent, QKeyEvent *aQEvent)
{
    aEvent.InitBasicModifiers(aQEvent->modifiers() & Qt::ControlModifier,
                              aQEvent->modifiers() & Qt::AltModifier,
                              aQEvent->modifiers() & Qt::ShiftModifier,
                              aQEvent->modifiers() & Qt::MetaModifier);

    // TODO: Needs to set .location for desktop Qt build.
#ifdef MOZ_PLATFORM_MAEMO
    aEvent.location  = nsIDOMKeyEvent::DOM_KEY_LOCATION_MOBILE;
#endif
    aEvent.time      = 0;

    if (sAltGrModifier) {
        aEvent.modifiers |= (widget::MODIFIER_CONTROL | widget::MODIFIER_ALT);
    }

    // The transformations above and in qt for the keyval are not invertible
    // so link to the QKeyEvent (which will vanish soon after return from the
    // event callback) to give plugins access to hardware_keycode and state.
    // (An XEvent would be nice but the QKeyEvent is good enough.)
    aEvent.pluginEvent = (void *)aQEvent;
}

nsWindow::nsWindow()
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__, (void *)this));

    mIsTopLevel       = false;
    mIsDestroyed      = false;
    mIsShown          = false;
    mEnabled          = true;
    mWidget              = nsnull;
    mIsVisible           = false;
    mActivatePending     = false;
    mWindowType          = eWindowType_child;
    mSizeState           = nsSizeMode_Normal;
    mLastSizeMode        = nsSizeMode_Normal;
    mPluginType          = PluginType_NONE;
    mQCursor             = Qt::ArrowCursor;
    mNeedsResize         = false;
    mNeedsMove           = false;
    mListenForResizes    = false;
    mNeedsShow           = false;
    mGesturesCancelled   = false;
    mTimerStarted        = false;
    mPinchEvent.needDispatch = false;
    mMoveEvent.needDispatch = false;
    
    if (!gGlobalsInitialized) {
        gfxPlatform::GetPlatform();
        gGlobalsInitialized = true;

#if defined(MOZ_X11) && (MOZ_PLATFORM_MAEMO == 6)
        // This cannot be called on non-main thread
        if (QThread::currentThread() == qApp->thread()) {
            sPluginIMEAtom = XInternAtom(mozilla::DefaultXDisplay(), PLUGIN_VKB_REQUEST_PROP, False);
        }
#endif
        // It's OK if either of these fail, but it may not be one day.
        initialize_prefs();
    }

    memset(mKeyDownFlags, 0, sizeof(mKeyDownFlags));

    mIsTransparent = false;

    mCursor = eCursor_standard;

    gBufferPixmapUsageCount++;

#if (QT_VERSION > QT_VERSION_CHECK(4,6,0))
    if (gSwipeGestureId == Qt::CustomGesture) {
        // QGestureRecognizer takes ownership
        MozSwipeGestureRecognizer* swipeRecognizer = new MozSwipeGestureRecognizer;
        gSwipeGestureId = QGestureRecognizer::registerRecognizer(swipeRecognizer);
    }
#endif
}

static inline gfxASurface::gfxImageFormat
_depth_to_gfximage_format(PRInt32 aDepth)
{
    switch (aDepth) {
    case 32:
        return gfxASurface::ImageFormatARGB32;
    case 24:
        return gfxASurface::ImageFormatRGB24;
    case 16:
        return gfxASurface::ImageFormatRGB16_565;
    default:
        return gfxASurface::ImageFormatUnknown;
    }
}

static inline QImage::Format
_gfximage_to_qformat(gfxASurface::gfxImageFormat aFormat)
{
    switch (aFormat) {
    case gfxASurface::ImageFormatARGB32:
        return QImage::Format_ARGB32_Premultiplied;
    case gfxASurface::ImageFormatRGB24:
        return QImage::Format_ARGB32;
    case gfxASurface::ImageFormatRGB16_565:
        return QImage::Format_RGB16;
    default:
        return QImage::Format_Invalid;
    }
}

static bool
UpdateOffScreenBuffers(int aDepth, QSize aSize, QWidget* aWidget = nsnull)
{
    gfxIntSize size(aSize.width(), aSize.height());
    if (gBufferSurface) {
        if (gBufferMaxSize.width < size.width ||
            gBufferMaxSize.height < size.height) {
            gBufferSurface = nsnull;
        } else
            return true;
    }

    gBufferMaxSize.width = NS_MAX(gBufferMaxSize.width, size.width);
    gBufferMaxSize.height = NS_MAX(gBufferMaxSize.height, size.height);

    // Check if system depth has related gfxImage format
    gfxASurface::gfxImageFormat format =
        _depth_to_gfximage_format(aDepth);

    // Use fallback RGB24 format, Qt will do conversion for us
    if (format == gfxASurface::ImageFormatUnknown)
        format = gfxASurface::ImageFormatRGB24;

#ifdef MOZ_HAVE_SHMIMAGE
    if (aWidget) {
        if (gfxPlatform::GetPlatform()->ScreenReferenceSurface()->GetType() ==
            gfxASurface::SurfaceTypeImage) {
            Display* dpy = mozilla::DefaultXDisplay();
            gShmImage = nsShmImage::Create(gBufferMaxSize,
                                           DefaultVisualOfScreen(gfxQtPlatform::GetXScreen(aWidget)),
                                           aDepth);
            gBufferSurface = gShmImage->AsSurface();
            return true;
        }
    }
#endif

    gBufferSurface = gfxPlatform::GetPlatform()->
        CreateOffscreenSurface(gBufferMaxSize, gfxASurface::ContentFromFormat(format));

    return true;
}

nsWindow::~nsWindow()
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__, (void *)this));

    Destroy();
}

/* static */ void
nsWindow::ReleaseGlobals()
{
}

NS_IMPL_ISUPPORTS_INHERITED1(nsWindow, nsBaseWidget, nsISupportsWeakReference)

NS_IMETHODIMP
nsWindow::ConfigureChildren(const nsTArray<nsIWidget::Configuration>& aConfigurations)
{
    for (PRUint32 i = 0; i < aConfigurations.Length(); ++i) {
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
nsWindow::Destroy(void)
{
    if (mIsDestroyed || !mWidget)
        return NS_OK;

    LOG(("nsWindow::Destroy [%p]\n", (void *)this));
    mIsDestroyed = true;

    if (gBufferPixmapUsageCount &&
        --gBufferPixmapUsageCount == 0) {

        gBufferSurface = nsnull;
#ifdef MOZ_HAVE_SHMIMAGE
        gShmImage = nsnull;
#endif
#ifdef MOZ_ENABLE_QTMOBILITY
        if (gOrientation) {
            gOrientation->removeFilter(&gOrientationFilter);
            gOrientation->stop();
            delete gOrientation;
            gOrientation = nsnull;
        }
#endif
    }

    nsCOMPtr<nsIWidget> rollupWidget = do_QueryReferent(gRollupWindow);
    if (static_cast<nsIWidget *>(this) == rollupWidget.get()) {
        if (gRollupListener)
            gRollupListener->Rollup(0);
        gRollupWindow = nsnull;
        gRollupListener = nsnull;
    }

    if (mLayerManager) {
        mLayerManager->Destroy();
    }
    mLayerManager = nsnull;

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
    mThebesSurface = nsnull;

    QWidget *view = nsnull;
    QGraphicsScene *scene = nsnull;
    if (mWidget) {
        if (mIsTopLevel) {
            view = GetViewWidget();
            scene = mWidget->scene();
        }

        mWidget->dropReceiver();

        // Call deleteLater instead of delete; Qt still needs the object
        // to be valid even after sending it a Close event.  We could
        // also set WA_DeleteOnClose, but this gives us more control.
        mWidget->deleteLater();
    }
    mWidget = nsnull;

    OnDestroy();

    // tear down some infrastructure after all event handling is finished
    delete scene;
    delete view;

    return NS_OK;
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
nsWindow::ReparentNativeWidget(nsIWidget *aNewParent)
{
    NS_PRECONDITION(aNewParent, "");

    MozQWidget* newParent = static_cast<MozQWidget*>(aNewParent->GetNativeData(NS_NATIVE_WINDOW));
    NS_ASSERTION(newParent, "Parent widget has a null native window handle");
    if (mWidget) {
        mWidget->setParentItem(newParent);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetModal(bool aModal)
{
    LOG(("nsWindow::SetModal [%p] %d, widget[%p]\n", (void *)this, aModal, mWidget));
    if (mWidget)
        mWidget->setModal(aModal);

    return NS_OK;
}

bool
nsWindow::IsVisible() const
{
    return mIsShown;
}

NS_IMETHODIMP
nsWindow::ConstrainPosition(bool aAllowSlop, PRInt32 *aX, PRInt32 *aY)
{
    if (mWidget) {
        PRInt32 screenWidth  = QApplication::desktop()->width();
        PRInt32 screenHeight = QApplication::desktop()->height();

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
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
    LOG(("nsWindow::Move [%p] %d %d\n", (void *)this,
         aX, aY));

    if (mIsTopLevel) {
        SetSizeMode(nsSizeMode_Normal);
    }

    if (aX == mBounds.x && aY == mBounds.y)
        return NS_OK;

    mNeedsMove = false;

    // update the bounds
    QPointF pos( aX, aY );
    if (mIsTopLevel) {
        QWidget *widget = GetViewWidget();
        NS_ENSURE_TRUE(widget, NS_OK);
        widget->move(aX, aY);
    }
    else if (mWidget) {
        // the position of the widget is set relative to the parent
        // so we map the coordinates accordingly
        pos = mWidget->mapFromScene(pos);
        pos = mWidget->mapToParent(pos);
        mWidget->setPos(pos);
    }

    mBounds.x = pos.x();
    mBounds.y = pos.y();

    NotifyRollupGeometryChange(gRollupListener);
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
nsWindow::SetSizeMode(PRInt32 aMode)
{
    nsresult rv;

    LOG(("nsWindow::SetSizeMode [%p] %d\n", (void *)this, aMode));
    if (aMode != nsSizeMode_Minimized) {
        GetViewWidget()->activateWindow();
    }

    // Save the requested state.
    rv = nsBaseWidget::SetSizeMode(aMode);

    // return if there's no shell or our current state is the same as
    // the mode we were just set to.
    if (!mWidget || mSizeState == mSizeMode) {
        return rv;
    }

    QWidget *widget = GetViewWidget();
    NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);

    switch (aMode) {
    case nsSizeMode_Maximized:
        widget->showMaximized();
        break;
    case nsSizeMode_Minimized:
        widget->showMinimized();
        break;
    case nsSizeMode_Fullscreen:
        widget->showFullScreen();
        break;

    default:
        // nsSizeMode_Normal, really.
        widget->showNormal();
        break;
    }

    mSizeState = mSizeMode;

    return rv;
}

// Helper function to recursively find the first parent item that
// is still visible (QGraphicsItem can be hidden even if they are
// set to visible if one of their ancestors is invisible)
static void find_first_visible_parent(QGraphicsItem* aItem, QGraphicsItem*& aVisibleItem)
{
    NS_ENSURE_TRUE(aItem, );

    aVisibleItem = nsnull;
    QGraphicsItem* parItem = nsnull;
    while (!aVisibleItem) {
        if (aItem->isVisible())
            aVisibleItem = aItem;
        else {
            parItem = aItem->parentItem();
            if (parItem)
                aItem = parItem;
            else {
                aItem->setVisible(true);
                aVisibleItem = aItem;
            }
        }
    }
}

NS_IMETHODIMP
nsWindow::SetFocus(bool aRaise)
{
    // Make sure that our owning widget has focus.  If it doesn't try to
    // grab it.  Note that we don't set our focus flag in this case.
    LOGFOCUS(("  SetFocus [%p]\n", (void *)this));

    if (!mWidget)
        return NS_ERROR_FAILURE;

    if (mWidget->hasFocus())
        return NS_OK;

    // Because QGraphicsItem cannot get the focus if they are
    // invisible, we look up the chain, for the lowest visible
    // parent and focus that one
    QGraphicsItem* realFocusItem = nsnull;
    find_first_visible_parent(mWidget, realFocusItem);

    if (!realFocusItem || realFocusItem->hasFocus())
        return NS_OK;

    if (aRaise) {
        // the raising has to happen on the view widget
        QWidget *widget = GetViewWidget();
        if (widget)
            widget->raise();
        realFocusItem->setFocus(Qt::ActiveWindowFocusReason);
    }
    else
        realFocusItem->setFocus(Qt::OtherFocusReason);

    DispatchActivateEvent();

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetScreenBounds(nsIntRect &aRect)
{
    aRect = nsIntRect(nsIntPoint(0, 0), mBounds.Size());
    if (mIsTopLevel) {
        QWidget *widget = GetViewWidget();
        NS_ENSURE_TRUE(widget, NS_OK);
        QPoint pos = widget->pos();
        aRect.MoveTo(pos.x(), pos.y());
    }
    else {
        aRect.MoveTo(WidgetToScreenOffset());
    }
    LOG(("GetScreenBounds %d %d | %d %d | %d %d\n",
         aRect.x, aRect.y,
         mBounds.width, mBounds.height,
         aRect.width, aRect.height));
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetForegroundColor(const nscolor &aColor)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::SetBackgroundColor(const nscolor &aColor)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::SetCursor(nsCursor aCursor)
{
    if (mCursor == aCursor)
        return NS_OK;

    mCursor = aCursor;
    if (mWidget)
        mWidget->SetCursor(mCursor);
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetCursor(imgIContainer* aCursor,
                    PRUint32 aHotspotX, PRUint32 aHotspotY)
{
    return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsWindow::Invalidate(const nsIntRect &aRect)
{
    LOGDRAW(("Invalidate (rect) [%p,%p]: %d %d %d %d\n", (void *)this,
             (void*)mWidget,aRect.x, aRect.y, aRect.width, aRect.height));

    if (!mWidget)
        return NS_OK;

    mDirtyScrollArea = mDirtyScrollArea.united(QRect(aRect.x, aRect.y, aRect.width, aRect.height));

    mWidget->update(aRect.x, aRect.y, aRect.width, aRect.height);

    return NS_OK;
}

// Returns the graphics view widget for this nsWindow by iterating
// the chain of parents until a toplevel window with a view/scene is found.
// (This function always returns something or asserts if the precondition
// is not met)
QWidget* nsWindow::GetViewWidget()
{
    NS_ASSERTION(mWidget, "Calling GetViewWidget without mWidget created");
    if (!mWidget || !mWidget->scene() || !mWidget->scene()->views().size())
        return nsnull;

    NS_ASSERTION(mWidget->scene()->views().size() == 1, "Not exactly one view for our scene!");
    return mWidget->scene()->views()[0];
}

void*
nsWindow::GetNativeData(PRUint32 aDataType)
{
    switch (aDataType) {
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_WIDGET: {
        if (!mWidget)
            return nsnull;

        return mWidget;
        break;
    }

    case NS_NATIVE_PLUGIN_PORT:
        return SetupPluginPort();
        break;

    case NS_NATIVE_DISPLAY:
        {
#ifdef MOZ_X11
            return gfxQtPlatform::GetXDisplay(GetViewWidget());
#else
            return nsnull;
#endif
        }
        break;

    case NS_NATIVE_GRAPHIC: {
        return nsnull;
        break;
    }

    case NS_NATIVE_SHELLWIDGET: {
        QWidget* widget = nsnull;
        if (mWidget && mWidget->scene())
            widget = mWidget->scene()->views()[0]->viewport();
        return (void *) widget;
    }

    case NS_NATIVE_SHAREABLE_WINDOW: {
        QWidget *widget = GetViewWidget();
        return widget ? (void*)widget->winId() : nsnull;
    }

    default:
        NS_WARNING("nsWindow::GetNativeData called with bad value");
        return nsnull;
    }
}

NS_IMETHODIMP
nsWindow::SetTitle(const nsAString& aTitle)
{
    QString qStr(QString::fromUtf16(aTitle.BeginReading(), aTitle.Length()));
    if (mIsTopLevel) {
        QWidget *widget = GetViewWidget();
        if (widget)
            widget->setWindowTitle(qStr);
    }
    else if (mWidget)
        mWidget->setWindowTitle(qStr);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetIcon(const nsAString& aIconSpec)
{
    if (!mWidget)
        return NS_OK;

    nsCOMPtr<nsIFile> iconFile;
    nsCAutoString path;
    nsTArray<nsCString> iconList;

    // Look for icons with the following suffixes appended to the base name.
    // The last two entries (for the old XPM format) will be ignored unless
    // no icons are found using the other suffixes. XPM icons are depricated.

    const char extensions[6][7] = { ".png", "16.png", "32.png", "48.png",
                                    ".xpm", "16.xpm" };

    for (PRUint32 i = 0; i < ArrayLength(extensions); i++) {
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

nsIntPoint
nsWindow::WidgetToScreenOffset()
{
    NS_ENSURE_TRUE(mWidget, nsIntPoint(0,0));

    QPointF origin(0, 0);
    origin = mWidget->mapToScene(origin);

    return nsIntPoint(origin.x(), origin.y());
}

NS_IMETHODIMP
nsWindow::EnableDragDrop(bool aEnable)
{
    mWidget->setAcceptDrops(aEnable);
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::CaptureMouse(bool aCapture)
{
    LOG(("CaptureMouse %p\n", (void *)this));

    if (!mWidget)
        return NS_OK;

    QWidget *widget = GetViewWidget();
    NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);

    if (aCapture)
        widget->grabMouse();
    else
        widget->releaseMouse();

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::CaptureRollupEvents(nsIRollupListener *aListener,
                              bool               aDoCapture,
                              bool               aConsumeRollupEvent)
{
    if (!mWidget)
        return NS_OK;

    LOG(("CaptureRollupEvents %p\n", (void *)this));

    if (aDoCapture) {
        gConsumeRollupEvent = aConsumeRollupEvent;
        gRollupListener = aListener;
        gRollupWindow = do_GetWeakReference(static_cast<nsIWidget*>(this));
    }
    else {
        gRollupListener = nsnull;
        gRollupWindow = nsnull;
    }

    return NS_OK;
}

bool
check_for_rollup(double aMouseX, double aMouseY,
                 bool aIsWheel)
{
    bool retVal = false;
    nsCOMPtr<nsIWidget> rollupWidget = do_QueryReferent(gRollupWindow);

    if (rollupWidget && gRollupListener) {
        MozQWidget *currentPopup =
            (MozQWidget *)rollupWidget->GetNativeData(NS_NATIVE_WINDOW);

        if (!is_mouse_in_window(currentPopup, aMouseX, aMouseY)) {
            bool rollup = true;
            if (aIsWheel) {
                rollup = gRollupListener->ShouldRollupOnMouseWheelEvent();
                retVal = true;
            }
            // if we're dealing with menus, we probably have submenus and
            // we don't want to rollup if the clickis in a parent menu of
            // the current submenu
            PRUint32 popupsToRollup = PR_UINT32_MAX;
            if (gRollupListener) {
                nsAutoTArray<nsIWidget*, 5> widgetChain;
                PRUint32 sameTypeCount = gRollupListener->GetSubmenuWidgetChain(&widgetChain);
                for (PRUint32 i=0; i<widgetChain.Length(); ++i) {
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
                gRollupListener->Rollup(popupsToRollup);
                retVal = true;
            }
        }
    } else {
        gRollupWindow = nsnull;
        gRollupListener = nsnull;
    }

    return retVal;
}

/* static */
bool
is_mouse_in_window (MozQWidget* aWindow, double aMouseX, double aMouseY)
{
    return aWindow->geometry().contains( aMouseX, aMouseY );
}

NS_IMETHODIMP
nsWindow::GetAttention(PRInt32 aCycleCount)
{
    LOG(("nsWindow::GetAttention [%p]\n", (void *)this));
    return NS_ERROR_NOT_IMPLEMENTED;
}

#ifdef MOZ_X11
static already_AddRefed<gfxASurface>
GetSurfaceForQWidget(QWidget* aDrawable)
{
    gfxASurface* result =
        new gfxXlibSurface(gfxQtPlatform::GetXDisplay(aDrawable),
                           aDrawable->winId(),
                           DefaultVisualOfScreen(gfxQtPlatform::GetXScreen(aDrawable)),
                           gfxIntSize(aDrawable->size().width(),
                           aDrawable->size().height()));
    NS_IF_ADDREF(result);
    return result;
}
#endif

static void
DispatchDidPaint(nsIWidget* aWidget)
{
    nsEventStatus status;
    nsPaintEvent didPaintEvent(true, NS_DID_PAINT, aWidget);
    aWidget->DispatchEvent(&didPaintEvent, status);
}

nsEventStatus
nsWindow::DoPaint(QPainter* aPainter, const QStyleOptionGraphicsItem* aOption, QWidget* aWidget)
{
    if (mIsDestroyed) {
        LOG(("Expose event on destroyed window [%p] window %p\n",
             (void *)this, mWidget));
        return nsEventStatus_eIgnore;
    }

    // Dispatch WILL_PAINT to allow scripts etc. to run before we
    // dispatch PAINT
    {
        nsEventStatus status;
        nsPaintEvent willPaintEvent(true, NS_WILL_PAINT, this);
        willPaintEvent.willSendDidPaint = true;
        DispatchEvent(&willPaintEvent, status);
    }

    if (!mWidget)
        return nsEventStatus_eIgnore;

    QRectF r;
    if (aOption)
        r = aOption->exposedRect;
    else
        r = mWidget->boundingRect();

    if (r.isEmpty())
        return nsEventStatus_eIgnore;

    if (!mDirtyScrollArea.isEmpty())
        mDirtyScrollArea = QRegion();

    nsEventStatus status;
    nsIntRect rect(r.x(), r.y(), r.width(), r.height());

    nsFastStartup* startup = nsFastStartup::GetSingleton();
    if (startup) {
        startup->RemoveFakeLayout();
    }

    if (GetLayerManager(nsnull)->GetBackendType() == mozilla::layers::LAYERS_OPENGL) {
        aPainter->beginNativePainting();
        nsPaintEvent event(true, NS_PAINT, this);
        event.willSendDidPaint = true;
        event.refPoint.x = r.x();
        event.refPoint.y = r.y();
        event.region = nsIntRegion(rect);
        static_cast<mozilla::layers::LayerManagerOGL*>(GetLayerManager(nsnull))->
            SetClippingRegion(event.region);

        gfxMatrix matr;
        matr.Translate(gfxPoint(aPainter->transform().dx(), aPainter->transform().dy()));
#ifdef MOZ_ENABLE_QTMOBILITY
        // This is needed for rotate transformation on MeeGo
        // This will work very slow if pixman does not handle rotation very well
        matr.Rotate((M_PI/180) * gOrientationFilter.GetWindowRotationAngle());
        static_cast<mozilla::layers::LayerManagerOGL*>(GetLayerManager(nsnull))->
            SetWorldTransform(matr);
#endif //MOZ_ENABLE_QTMOBILITY

        status = DispatchEvent(&event);
        aPainter->endNativePainting();
        DispatchDidPaint(this);
        return status;
    }

    gfxQtPlatform::RenderMode renderMode = gfxQtPlatform::GetPlatform()->GetRenderMode();
    int depth = aPainter->device()->depth();

    nsRefPtr<gfxASurface> targetSurface = nsnull;
    if (renderMode == gfxQtPlatform::RENDER_BUFFERED) {
        // Prepare offscreen buffers iamge or xlib, depends from paintEngineType
        if (!UpdateOffScreenBuffers(depth, QSize(r.width(), r.height())))
            return nsEventStatus_eIgnore;

        targetSurface = gBufferSurface;

#ifdef CAIRO_HAS_QT_SURFACE
    } else if (renderMode == gfxQtPlatform::RENDER_QPAINTER) {
        targetSurface = new gfxQPainterSurface(aPainter);
#endif
    } else if (renderMode == gfxQtPlatform::RENDER_DIRECT) {
        if (!UpdateOffScreenBuffers(depth, aWidget->size(), aWidget)) {
            return nsEventStatus_eIgnore;
        }
        targetSurface = gBufferSurface;
    }

    if (NS_UNLIKELY(!targetSurface))
        return nsEventStatus_eIgnore;

    nsRefPtr<gfxContext> ctx = new gfxContext(targetSurface);

    // We will paint to 0, 0 position in offscrenn buffer
    if (renderMode == gfxQtPlatform::RENDER_BUFFERED) {
        ctx->Translate(gfxPoint(-r.x(), -r.y()));
    }
    else if (renderMode == gfxQtPlatform::RENDER_DIRECT) {
      gfxMatrix matr;
      matr.Translate(gfxPoint(aPainter->transform().dx(), aPainter->transform().dy()));
#ifdef MOZ_ENABLE_QTMOBILITY
         // This is needed for rotate transformation on MeeGo
         // This will work very slow if pixman does not handle rotation very well
         matr.Rotate((M_PI/180) * gOrientationFilter.GetWindowRotationAngle());
         NS_ASSERTION(PIXMAN_VERSION > PIXMAN_VERSION_ENCODE(0, 21, 2) ||
                      !gOrientationFilter.GetWindowRotationAngle(),
                      "Old pixman and rotate transform, it is going to be slow");
#endif //MOZ_ENABLE_QTMOBILITY

      ctx->SetMatrix(matr);
    }

    nsPaintEvent event(true, NS_PAINT, this);
    event.willSendDidPaint = true;
    event.refPoint.x = rect.x;
    event.refPoint.y = rect.y;
    event.region = nsIntRegion(rect);
    {
        AutoLayerManagerSetup
            setupLayerManager(this, ctx, BasicLayerManager::BUFFER_NONE);
        status = DispatchEvent(&event);
    }

    // DispatchEvent can Destroy us (bug 378273), avoid doing any paint
    // operations below if that happened - it will lead to XError and exit().
    if (NS_UNLIKELY(mIsDestroyed))
        return status;

    if (status == nsEventStatus_eIgnore)
        return status;

    LOGDRAW(("[%p] draw done\n", this));

    // Handle buffered painting mode
    if (renderMode == gfxQtPlatform::RENDER_BUFFERED) {
#if defined(MOZ_X11) && defined(Q_WS_X11)
        if (gBufferSurface->GetType() == gfxASurface::SurfaceTypeXlib) {
            // Paint offscreen pixmap to QPainter
            static QPixmap gBufferPixmap;
            Drawable draw = static_cast<gfxXlibSurface*>(gBufferSurface.get())->XDrawable();
            if (gBufferPixmap.handle() != draw)
                gBufferPixmap = QPixmap::fromX11Pixmap(draw, QPixmap::ExplicitlyShared);
            XSync(static_cast<gfxXlibSurface*>(gBufferSurface.get())->XDisplay(), False);
            aPainter->drawPixmap(QPoint(rect.x, rect.y), gBufferPixmap,
                                 QRect(0, 0, rect.width, rect.height));

        } else
#endif
        if (gBufferSurface->GetType() == gfxASurface::SurfaceTypeImage) {
            // in raster mode we can just wrap gBufferImage as QImage and paint directly
            gfxImageSurface *imgs = static_cast<gfxImageSurface*>(gBufferSurface.get());
            QImage img(imgs->Data(),
                       imgs->Width(),
                       imgs->Height(),
                       imgs->Stride(),
                       _gfximage_to_qformat(imgs->Format()));
            aPainter->drawImage(QPoint(rect.x, rect.y), img,
                                QRect(0, 0, rect.width, rect.height));
        }
    } else if (renderMode == gfxQtPlatform::RENDER_DIRECT) {
        QRect trans = aPainter->transform().mapRect(r).toRect();
#ifdef MOZ_X11
        if (gBufferSurface->GetType() == gfxASurface::SurfaceTypeXlib) {
            nsRefPtr<gfxASurface> widgetSurface = GetSurfaceForQWidget(aWidget);
            nsRefPtr<gfxContext> ctx = new gfxContext(widgetSurface);
            ctx->SetSource(gBufferSurface);
            ctx->Rectangle(gfxRect(trans.x(), trans.y(), trans.width(), trans.height()), true);
            ctx->Clip();
            ctx->Fill();
        } else
#endif
        if (gBufferSurface->GetType() == gfxASurface::SurfaceTypeImage) {
#ifdef MOZ_HAVE_SHMIMAGE
            if (gShmImage) {
                gShmImage->Put(aWidget, trans);
            } else
#endif
            {
                // Qt should take care about optimized rendering on QImage into painter device (gl/fb/image et.c.)
                gfxImageSurface *imgs = static_cast<gfxImageSurface*>(gBufferSurface.get());
                QImage img(imgs->Data(),
                           imgs->Width(),
                           imgs->Height(),
                           imgs->Stride(),
                          _gfximage_to_qformat(imgs->Format()));
                aPainter->drawImage(trans, img, trans);
            }
        }
    }

    ctx = nsnull;
    targetSurface = nsnull;
    DispatchDidPaint(this);

    // check the return value!
    return status;
}

nsEventStatus
nsWindow::OnMoveEvent(QGraphicsSceneHoverEvent *aEvent)
{
    LOG(("configure event [%p] %d %d\n", (void *)this,
        aEvent->pos().x(),  aEvent->pos().y()));

    // can we shortcut?
    if (!mWidget)
        return nsEventStatus_eIgnore;

    if ((mBounds.x == aEvent->pos().x() &&
         mBounds.y == aEvent->pos().y()))
    {
        return nsEventStatus_eIgnore;
    }

    nsGUIEvent event(true, NS_MOVE, this);

    event.refPoint.x = aEvent->pos().x();
    event.refPoint.y = aEvent->pos().y();

    // XXX mozilla will invalidate the entire window after this move
    // complete.  wtf?
    return DispatchEvent(&event);
}

nsEventStatus
nsWindow::OnResizeEvent(QGraphicsSceneResizeEvent *aEvent)
{
    nsIntRect rect;

    // Generate XPFE resize event
    GetBounds(rect);

    rect.width = aEvent->newSize().width();
    rect.height = aEvent->newSize().height();

    mBounds.width = rect.width;
    mBounds.height = rect.height;

    nsEventStatus status;
    DispatchResizeEvent(rect, status);
    return status;
}

nsEventStatus
nsWindow::OnCloseEvent(QCloseEvent *aEvent)
{
    nsGUIEvent event(true, NS_XUL_CLOSE, this);

    event.refPoint.x = 0;
    event.refPoint.y = 0;

    return DispatchEvent(&event);
}

nsEventStatus
nsWindow::OnEnterNotifyEvent(QGraphicsSceneHoverEvent *aEvent)
{
    nsMouseEvent event(true, NS_MOUSE_ENTER, this, nsMouseEvent::eReal);

    event.refPoint.x = nscoord(aEvent->pos().x());
    event.refPoint.y = nscoord(aEvent->pos().y());

    LOG(("OnEnterNotify: %p\n", (void *)this));

    return DispatchEvent(&event);
}

nsEventStatus
nsWindow::OnLeaveNotifyEvent(QGraphicsSceneHoverEvent *aEvent)
{
    nsMouseEvent event(true, NS_MOUSE_EXIT, this, nsMouseEvent::eReal);

    event.refPoint.x = nscoord(aEvent->pos().x());
    event.refPoint.y = nscoord(aEvent->pos().y());

    LOG(("OnLeaveNotify: %p\n", (void *)this));

    return DispatchEvent(&event);
}

// Block the mouse events if user was recently executing gestures;
// otherwise there will be also some panning during/after gesture
#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0))
#define CHECK_MOUSE_BLOCKED { \
if (mLastMultiTouchTime.isValid()) { \
    if (mLastMultiTouchTime.elapsed() < GESTURES_BLOCK_MOUSE_FOR) \
        return nsEventStatus_eIgnore; \
    else \
        mLastMultiTouchTime = QTime(); \
   } \
}
#else
define CHECK_MOUSE_BLOCKED {}
#endif

nsEventStatus
nsWindow::OnMotionNotifyEvent(QPointF aPos,  Qt::KeyboardModifiers aModifiers)
{
    UserActivity();

    CHECK_MOUSE_BLOCKED

    mMoveEvent.pos = aPos;
    mMoveEvent.modifiers = aModifiers;
    mMoveEvent.needDispatch = true;
    DispatchMotionToMainThread();

    return nsEventStatus_eIgnore;
}

void
nsWindow::InitButtonEvent(nsMouseEvent &aMoveEvent,
                          QGraphicsSceneMouseEvent *aEvent, int aClickCount)
{
    aMoveEvent.refPoint.x = nscoord(aEvent->pos().x());
    aMoveEvent.refPoint.y = nscoord(aEvent->pos().y());

    aMoveEvent.InitBasicModifiers(aEvent->modifiers() & Qt::ControlModifier,
                                  aEvent->modifiers() & Qt::AltModifier,
                                  aEvent->modifiers() & Qt::ShiftModifier,
                                  aEvent->modifiers() & Qt::MetaModifier);
    aMoveEvent.clickCount      = aClickCount;
}

nsEventStatus
nsWindow::OnButtonPressEvent(QGraphicsSceneMouseEvent *aEvent)
{
    // The user has done something.
    UserActivity();

    CHECK_MOUSE_BLOCKED

    QPointF pos = aEvent->pos();

    // we check against the widgets geometry, so use parent coordinates
    // for the check
    if (mWidget)
        pos = mWidget->mapToParent(pos);

    bool rolledUp = check_for_rollup( pos.x(), pos.y(), false);
    if (gConsumeRollupEvent && rolledUp)
        return nsEventStatus_eIgnore;

    PRUint16      domButton;
    switch (aEvent->button()) {
    case Qt::MidButton:
        domButton = nsMouseEvent::eMiddleButton;
        break;
    case Qt::RightButton:
        domButton = nsMouseEvent::eRightButton;
        break;
    default:
        domButton = nsMouseEvent::eLeftButton;
        break;
    }

    nsMouseEvent event(true, NS_MOUSE_BUTTON_DOWN, this, nsMouseEvent::eReal);
    event.button = domButton;
    InitButtonEvent(event, aEvent, 1);

    LOG(("%s [%p] button: %d\n", __PRETTY_FUNCTION__, (void*)this, domButton));

    nsEventStatus status = DispatchEvent(&event);

    // right menu click on linux should also pop up a context menu
    if (domButton == nsMouseEvent::eRightButton &&
        NS_LIKELY(!mIsDestroyed)) {
        nsMouseEvent contextMenuEvent(true, NS_CONTEXTMENU, this,
                                      nsMouseEvent::eReal);
        InitButtonEvent(contextMenuEvent, aEvent, 1);
        DispatchEvent(&contextMenuEvent, status);
    }

    return status;
}

nsEventStatus
nsWindow::OnButtonReleaseEvent(QGraphicsSceneMouseEvent *aEvent)
{
    UserActivity();
    CHECK_MOUSE_BLOCKED

    // The user has done something.
    UserActivity();

    PRUint16 domButton;

    switch (aEvent->button()) {
    case Qt::MidButton:
        domButton = nsMouseEvent::eMiddleButton;
        break;
    case Qt::RightButton:
        domButton = nsMouseEvent::eRightButton;
        break;
    default:
        domButton = nsMouseEvent::eLeftButton;
        break;
    }

    LOG(("%s [%p] button: %d\n", __PRETTY_FUNCTION__, (void*)this, domButton));

    nsMouseEvent event(true, NS_MOUSE_BUTTON_UP, this, nsMouseEvent::eReal);
    event.button = domButton;
    InitButtonEvent(event, aEvent, 1);

    nsEventStatus status = DispatchEvent(&event);

    return status;
}

nsEventStatus
nsWindow::OnMouseDoubleClickEvent(QGraphicsSceneMouseEvent *aEvent)
{
    PRUint32 eventType;

    switch (aEvent->button()) {
    case Qt::MidButton:
        eventType = nsMouseEvent::eMiddleButton;
        break;
    case Qt::RightButton:
        eventType = nsMouseEvent::eRightButton;
        break;
    default:
        eventType = nsMouseEvent::eLeftButton;
        break;
    }

    nsMouseEvent event(true, NS_MOUSE_DOUBLECLICK, this, nsMouseEvent::eReal);
    event.button = eventType;

    InitButtonEvent(event, aEvent, 2);
    //pressed
    return DispatchEvent(&event);
}

nsEventStatus
nsWindow::OnFocusInEvent(QEvent *aEvent)
{
    LOGFOCUS(("OnFocusInEvent [%p]\n", (void *)this));

    if (!mWidget)
        return nsEventStatus_eIgnore;

    DispatchActivateEventOnTopLevelWindow();

    LOGFOCUS(("Events sent from focus in event [%p]\n", (void *)this));
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::OnFocusOutEvent(QEvent *aEvent)
{
    LOGFOCUS(("OnFocusOutEvent [%p]\n", (void *)this));

    if (!mWidget)
        return nsEventStatus_eIgnore;

#if MOZ_PLATFORM_MAEMO > 5
    if (((QFocusEvent*)aEvent)->reason() == Qt::OtherFocusReason
         && mWidget->isVKBOpen()) {
        // We assume that the VKB was open in this case, because of the focus
        // reason and clear the focus in the active window.
        nsCOMPtr<nsIFocusManager> fm = do_GetService("@mozilla.org/focus-manager;1");
        if (fm) {
            nsCOMPtr<nsIDOMWindow> domWindow;
            fm->GetActiveWindow(getter_AddRefs(domWindow));
            fm->ClearFocus(domWindow);
        }

        return nsEventStatus_eIgnore;
    }
#endif

    DispatchDeactivateEventOnTopLevelWindow();

    LOGFOCUS(("Done with container focus out [%p]\n", (void *)this));
    return nsEventStatus_eIgnore;
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
    nsCommandEvent event(true, nsGkAtoms::onAppCommand, aCommand, this);

    nsEventStatus status;
    DispatchEvent(&event, status);

    return status;
}

nsEventStatus
nsWindow::DispatchContentCommandEvent(PRInt32 aMsg)
{
    nsContentCommandEvent event(true, aMsg, this);

    nsEventStatus status;
    DispatchEvent(&event, status);

    return status;
}

nsEventStatus
nsWindow::OnKeyPressEvent(QKeyEvent *aEvent)
{
    LOGFOCUS(("OnKeyPressEvent [%p]\n", (void *)this));

    // The user has done something.
    UserActivity();

    bool setNoDefault = false;

    if (aEvent->key() == Qt::Key_AltGr) {
        sAltGrModifier = true;
    }

#ifdef MOZ_X11
    // before we dispatch a key, check if it's the context menu key.
    // If so, send a context menu key event instead.
    if (isContextMenuKeyEvent(aEvent)) {
        nsMouseEvent contextMenuEvent(true, NS_CONTEXTMENU, this,
                                      nsMouseEvent::eReal,
                                      nsMouseEvent::eContextMenuKey);
        //keyEventToContextMenuEvent(&event, &contextMenuEvent);
        return DispatchEvent(&contextMenuEvent);
    }

    PRUint32 domCharCode = 0;
    PRUint32 domKeyCode = QtKeyCodeToDOMKeyCode(aEvent->key());

    // get keymap and modifier map from the Xserver
    Display *display = mozilla::DefaultXDisplay();
    int x_min_keycode = 0, x_max_keycode = 0, xkeysyms_per_keycode;
    XDisplayKeycodes(display, &x_min_keycode, &x_max_keycode);
    XModifierKeymap *xmodmap = XGetModifierMapping(display);
    if (!xmodmap)
        return nsEventStatus_eIgnore;

    KeySym *xkeymap = XGetKeyboardMapping(display, x_min_keycode, x_max_keycode - x_min_keycode,
                                          &xkeysyms_per_keycode);
    if (!xkeymap) {
        XFreeModifiermap(xmodmap);
        return nsEventStatus_eIgnore;
    }

    // create modifier masks
    qint32 shift_mask = 0, shift_lock_mask = 0, caps_lock_mask = 0, num_lock_mask = 0;

    for (int i = 0; i < 8 * xmodmap->max_keypermod; ++i) {
        qint32 maskbit = 1 << (i / xmodmap->max_keypermod);
        KeyCode modkeycode = xmodmap->modifiermap[i];
        if (modkeycode == NoSymbol) {
            continue;
        }

        quint32 mapindex = (modkeycode - x_min_keycode) * xkeysyms_per_keycode;
        for (int j = 0; j < xkeysyms_per_keycode; ++j) {
            KeySym modkeysym = xkeymap[mapindex + j];
            switch (modkeysym) {
                case XK_Num_Lock:
                    num_lock_mask |= maskbit;
                    break;
                case XK_Caps_Lock:
                    caps_lock_mask |= maskbit;
                    break;
                case XK_Shift_Lock:
                    shift_lock_mask |= maskbit;
                    break;
                case XK_Shift_L:
                case XK_Shift_R:
                    shift_mask |= maskbit;
                    break;
            }
        }
    }
    // indicate whether is down or not
    bool shift_state = ((shift_mask & aEvent->nativeModifiers()) != 0) ^
                          (bool)(shift_lock_mask & aEvent->nativeModifiers());
    bool capslock_state = (bool)(caps_lock_mask & aEvent->nativeModifiers());

    // try to find a keysym that we can translate to a DOMKeyCode
    // this is needed because some of Qt's keycodes cannot be translated
    // TODO: use US keyboard keymap instead of localised keymap
    if (!domKeyCode &&
        aEvent->nativeScanCode() >= (quint32)x_min_keycode &&
        aEvent->nativeScanCode() <= (quint32)x_max_keycode) {
        int index = (aEvent->nativeScanCode() - x_min_keycode) * xkeysyms_per_keycode;
        for(int i = 0; (i < xkeysyms_per_keycode) && (domKeyCode == (quint32)NoSymbol); ++i) {
            domKeyCode = QtKeyCodeToDOMKeyCode(xkeymap[index + i]);
        }
    }

    // store character in domCharCode
    if (aEvent->text().length() && aEvent->text()[0].isPrint())
        domCharCode = (PRInt32) aEvent->text()[0].unicode();

    // If the key isn't autorepeat, we need to send the initial down event
    if (!aEvent->isAutoRepeat() && !IsKeyDown(domKeyCode)) {
        // send the key down event

        SetKeyDownFlag(domKeyCode);

        nsKeyEvent downEvent(true, NS_KEY_DOWN, this);
        InitKeyEvent(downEvent, aEvent);

        downEvent.keyCode = domKeyCode;

        nsEventStatus status = DispatchEvent(&downEvent);

        // DispatchEvent can Destroy us (bug 378273)
        if (NS_UNLIKELY(mIsDestroyed)) {
            qWarning() << "Returning[" << __LINE__ << "]: " << "Window destroyed";
            return status;
        }

        // If prevent default on keydown, do same for keypress
        if (status == nsEventStatus_eConsumeNoDefault)
            setNoDefault = true;
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

        return setNoDefault ?
            nsEventStatus_eConsumeNoDefault :
            nsEventStatus_eIgnore;
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

    nsKeyEvent event(true, NS_KEY_PRESS, this);
    InitKeyEvent(event, aEvent);

    // If prevent default on keydown, do same for keypress
    if (setNoDefault) {
        event.flags |= NS_EVENT_FLAG_NO_DEFAULT;
    }

    // If there is no charcode attainable from the text, try to
    // generate it from the keycode. Check shift state for case
    // Also replace the charcode if ControlModifier is the only
    // pressed Modifier
    if ((!domCharCode) &&
        (QApplication::keyboardModifiers() &
        (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {

        // get a character from X11 key map
        KeySym keysym = aEvent->nativeVirtualKey();
        if (keysym) {
            domCharCode = (PRUint32) keysym2ucs(keysym);
            if (domCharCode == -1 || ! QChar((quint32)domCharCode).isPrint()) {
                domCharCode = 0;
            }
        }

        // if Ctrl is pressed and domCharCode is not a ASCII character
        if (domCharCode > 0xFF && (QApplication::keyboardModifiers() & Qt::ControlModifier)) {
            // replace Unicode character
            int index = (aEvent->nativeScanCode() - x_min_keycode) * xkeysyms_per_keycode;
            for (int i = 0; i < xkeysyms_per_keycode; ++i) {
                if (xkeymap[index + i] <= 0xFF && !shift_state) {
                    domCharCode = (PRUint32) QChar::toLower((uint) xkeymap[index + i]);
                    break;
                }
            }
        }

    } else { // The key event should cause a character input.
             // At that time, we need to reset the modifiers
             // because nsEditor will not accept a key event
             // for text input if one or more modifiers are set.
        event.modifiers &= ~(widget::MODIFIER_CONTROL |
                             widget::MODIFIER_ALT |
                             widget::MODIFIER_META);
    }

    KeySym keysym = NoSymbol;
    int index = (aEvent->nativeScanCode() - x_min_keycode) * xkeysyms_per_keycode;
    for (int i = 0; i < xkeysyms_per_keycode; ++i) {
        if (xkeymap[index + i] == aEvent->nativeVirtualKey()) {
            if ((i % 2) == 0) { // shifted char
                keysym = xkeymap[index + i + 1];
                break;
            } else { // unshifted char
                keysym = xkeymap[index + i - 1];
                break;
            }
        }
        if (xkeysyms_per_keycode - 1 == i) {
            qWarning() << "Symbol '" << aEvent->nativeVirtualKey() << "' not found";
        }
    }
    QChar unshiftedChar(domCharCode);
    long ucs = keysym2ucs(keysym);
    ucs = ucs == -1 ? 0 : ucs;
    QChar shiftedChar((uint)ucs);

    // append alternativeCharCodes if modifier is pressed
    // append an additional alternativeCharCodes if domCharCode is not a Latin character
    // and if one of these modifiers is pressed (i.e. Ctrl, Alt, Meta)
    if (domCharCode &&
        (QApplication::keyboardModifiers() &
        (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {

        event.charCode = domCharCode;
        event.keyCode = 0;
        nsAlternativeCharCode altCharCode(0, 0);
        // if character has a lower and upper representation
        if ((unshiftedChar.isUpper() || unshiftedChar.isLower()) &&
            unshiftedChar.toLower() == shiftedChar.toLower()) {
            if (shift_state ^ capslock_state) {
                altCharCode.mUnshiftedCharCode = (PRUint32) QChar::toUpper((uint)domCharCode);
                altCharCode.mShiftedCharCode = (PRUint32) QChar::toLower((uint)domCharCode);
            } else {
                altCharCode.mUnshiftedCharCode = (PRUint32) QChar::toLower((uint)domCharCode);
                altCharCode.mShiftedCharCode = (PRUint32) QChar::toUpper((uint)domCharCode);
            }
        } else {
            altCharCode.mUnshiftedCharCode = (PRUint32) unshiftedChar.unicode();
            altCharCode.mShiftedCharCode = (PRUint32) shiftedChar.unicode();
        }

        // append alternative char code to event
        if ((altCharCode.mUnshiftedCharCode && altCharCode.mUnshiftedCharCode != domCharCode) ||
            (altCharCode.mShiftedCharCode && altCharCode.mShiftedCharCode != domCharCode)) {
            event.alternativeCharCodes.AppendElement(altCharCode);
        }

        // check if the alternative char codes are latin-1
        if (altCharCode.mUnshiftedCharCode > 0xFF || altCharCode.mShiftedCharCode > 0xFF) {
            altCharCode.mUnshiftedCharCode = altCharCode.mShiftedCharCode = 0;

            // find latin char for keycode
            KeySym keysym = NoSymbol;
            int index = (aEvent->nativeScanCode() - x_min_keycode) * xkeysyms_per_keycode;
            // find first shifted and unshifted Latin-Char in XKeyMap
            for (int i = 0; i < xkeysyms_per_keycode; ++i) {
                keysym = xkeymap[index + i];
                if (keysym && keysym <= 0xFF) {
                    if ((shift_state && (i % 2 == 1)) ||
                        (!shift_state && (i % 2 == 0))) {
                        altCharCode.mUnshiftedCharCode = altCharCode.mUnshiftedCharCode ?
                            altCharCode.mUnshiftedCharCode :
                            keysym;
                    } else {
                        altCharCode.mShiftedCharCode = altCharCode.mShiftedCharCode ?
                            altCharCode.mShiftedCharCode :
                            keysym;
                    }
                    if (altCharCode.mUnshiftedCharCode && altCharCode.mShiftedCharCode) {
                        break;
                    }
                }
            }

            if (altCharCode.mUnshiftedCharCode || altCharCode.mShiftedCharCode) {
                event.alternativeCharCodes.AppendElement(altCharCode);
            }
        }
    } else {
        event.charCode = domCharCode;
    }

    if (xmodmap) {
        XFreeModifiermap(xmodmap);
    }
    if (xkeymap) {
        XFree(xkeymap);
    }

    event.keyCode = domCharCode ? 0 : domKeyCode;
    // send the key press event
    return DispatchEvent(&event);
#else

    //:TODO: fix shortcuts hebrew for non X11,
    //see Bug 562195##51

    // before we dispatch a key, check if it's the context menu key.
    // If so, send a context menu key event instead.
    if (isContextMenuKeyEvent(aEvent)) {
        nsMouseEvent contextMenuEvent(true, NS_CONTEXTMENU, this,
                                      nsMouseEvent::eReal,
                                      nsMouseEvent::eContextMenuKey);
        //keyEventToContextMenuEvent(&event, &contextMenuEvent);
        return DispatchEvent(&contextMenuEvent);
    }

    PRUint32 domCharCode = 0;
    PRUint32 domKeyCode = QtKeyCodeToDOMKeyCode(aEvent->key());

    if (aEvent->text().length() && aEvent->text()[0].isPrint())
        domCharCode = (PRInt32) aEvent->text()[0].unicode();

    // If the key isn't autorepeat, we need to send the initial down event
    if (!aEvent->isAutoRepeat() && !IsKeyDown(domKeyCode)) {
        // send the key down event

        SetKeyDownFlag(domKeyCode);

        nsKeyEvent downEvent(true, NS_KEY_DOWN, this);
        InitKeyEvent(downEvent, aEvent);

        downEvent.keyCode = domKeyCode;

        nsEventStatus status = DispatchEvent(&downEvent);

        // If prevent default on keydown, do same for keypress
        if (status == nsEventStatus_eConsumeNoDefault)
            setNoDefault = true;
    }

    nsKeyEvent event(true, NS_KEY_PRESS, this);
    InitKeyEvent(event, aEvent);

    event.charCode = domCharCode;

    event.keyCode = domCharCode ? 0 : domKeyCode;

    if (setNoDefault)
        event.flags |= NS_EVENT_FLAG_NO_DEFAULT;

    // send the key press event
    return DispatchEvent(&event);
#endif
}

nsEventStatus
nsWindow::OnKeyReleaseEvent(QKeyEvent *aEvent)
{
    LOGFOCUS(("OnKeyReleaseEvent [%p]\n", (void *)this));

    // The user has done something.
    UserActivity();

    if (isContextMenuKeyEvent(aEvent)) {
        // er, what do we do here? DoDefault or NoDefault?
        return nsEventStatus_eConsumeDoDefault;
    }

    PRUint32 domKeyCode = QtKeyCodeToDOMKeyCode(aEvent->key());

#ifdef MOZ_X11
    if (!domKeyCode) {
        // get keymap from the Xserver
        Display *display = mozilla::DefaultXDisplay();
        int x_min_keycode = 0, x_max_keycode = 0, xkeysyms_per_keycode;
        XDisplayKeycodes(display, &x_min_keycode, &x_max_keycode);
        KeySym *xkeymap = XGetKeyboardMapping(display, x_min_keycode, x_max_keycode - x_min_keycode,
                                              &xkeysyms_per_keycode);

        if (aEvent->nativeScanCode() >= (quint32)x_min_keycode &&
            aEvent->nativeScanCode() <= (quint32)x_max_keycode) {
            int index = (aEvent->nativeScanCode() - x_min_keycode) * xkeysyms_per_keycode;
            for(int i = 0; (i < xkeysyms_per_keycode) && (domKeyCode == (quint32)NoSymbol); ++i) {
                domKeyCode = QtKeyCodeToDOMKeyCode(xkeymap[index + i]);
            }
        }

        if (xkeymap) {
            XFree(xkeymap);
        }
    }
#endif // MOZ_X11

    // send the key event as a key up event
    nsKeyEvent event(true, NS_KEY_UP, this);
    InitKeyEvent(event, aEvent);

    if (aEvent->key() == Qt::Key_AltGr) {
        sAltGrModifier = false;
    }

    event.keyCode = domKeyCode;

    // unset the key down flag
    ClearKeyDownFlag(event.keyCode);

    return DispatchEvent(&event);
}

nsEventStatus
nsWindow::OnScrollEvent(QGraphicsSceneWheelEvent *aEvent)
{
    // check to see if we should rollup
    nsMouseScrollEvent event(true, NS_MOUSE_SCROLL, this);

    switch (aEvent->orientation()) {
    case Qt::Vertical:
        event.scrollFlags = nsMouseScrollEvent::kIsVertical;
        break;
    case Qt::Horizontal:
        event.scrollFlags = nsMouseScrollEvent::kIsHorizontal;
        break;
    default:
        Q_ASSERT(0);
        break;
    }

    // negative values for aEvent->delta indicate downward scrolling;
    // this is opposite Gecko usage.

    event.delta = (int)(aEvent->delta() / WHEEL_DELTA) * -3;

    event.refPoint.x = nscoord(aEvent->scenePos().x());
    event.refPoint.y = nscoord(aEvent->scenePos().y());

    event.InitBasicModifiers(aEvent->modifiers() & Qt::ControlModifier,
                             aEvent->modifiers() & Qt::AltModifier,
                             aEvent->modifiers() & Qt::ShiftModifier,
                             aEvent->modifiers() & Qt::MetaModifier);
    event.time            = 0;

    return DispatchEvent(&event);
}


nsEventStatus
nsWindow::showEvent(QShowEvent *)
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__,(void *)this));
    mIsVisible = true;
    return nsEventStatus_eConsumeDoDefault;
}

nsEventStatus
nsWindow::hideEvent(QHideEvent *)
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__,(void *)this));
    mIsVisible = false;
    return nsEventStatus_eConsumeDoDefault;
}

//Gestures are only supported in 4.6.0 >
#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0))
nsEventStatus nsWindow::OnTouchEvent(QTouchEvent *event, bool &handled)
{
    handled = false;
    const QList<QTouchEvent::TouchPoint> &touchPoints = event->touchPoints();

    if (event->type() == QEvent::TouchBegin) {
        handled = true;
        for (int i = touchPoints.count() -1; i >= 0; i--) {
            QPointF fpos = touchPoints[i].pos();
            nsGestureNotifyEvent gestureNotifyEvent(true, NS_GESTURENOTIFY_EVENT_START, this);
            gestureNotifyEvent.refPoint = nsIntPoint(fpos.x(), fpos.y());
            DispatchEvent(&gestureNotifyEvent);
        }
    }
    else if (event->type() == QEvent::TouchEnd) {
        mGesturesCancelled = false;
        mPinchEvent.needDispatch = false;
    }

    if (touchPoints.count() > 0) {
        // Remember start touch point in order to use it for
        // distance calculation in NS_SIMPLE_GESTURE_MAGNIFY_UPDATE
        mPinchEvent.touchPoint = touchPoints.at(0).pos();
    }

    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::OnGestureEvent(QGestureEvent* event, bool &handled) {

    handled = false;
    if (mGesturesCancelled) {
        return nsEventStatus_eIgnore;
    }

    nsEventStatus result = nsEventStatus_eIgnore;

    QGesture* gesture = event->gesture(Qt::PinchGesture);

    if (gesture) {
        QPinchGesture* pinch = static_cast<QPinchGesture*>(gesture);
        handled = true;

        mPinchEvent.centerPoint =
            mWidget->mapFromScene(event->mapToGraphicsScene(pinch->centerPoint()));
        nsIntPoint centerPoint(mPinchEvent.centerPoint.x(),
                               mPinchEvent.centerPoint.y());

        if (pinch->state() == Qt::GestureStarted) {
            event->accept();
            mPinchEvent.startDistance = DistanceBetweenPoints(mPinchEvent.centerPoint, mPinchEvent.touchPoint) * 2;
            mPinchEvent.prevDistance = mPinchEvent.startDistance;
            result = DispatchGestureEvent(NS_SIMPLE_GESTURE_MAGNIFY_START,
                                          0, 0, centerPoint);
        }
        else if (pinch->state() == Qt::GestureUpdated) {
            mPinchEvent.needDispatch = true;
            mPinchEvent.delta = 0;
            DispatchMotionToMainThread();
        }
        else if (pinch->state() == Qt::GestureFinished) {
            double distance = DistanceBetweenPoints(mPinchEvent.centerPoint, mPinchEvent.touchPoint) * 2;
            double delta = distance - mPinchEvent.startDistance;
            result = DispatchGestureEvent(NS_SIMPLE_GESTURE_MAGNIFY,
                                          0, delta, centerPoint);
            mPinchEvent.needDispatch = false;
        }
        else {
            handled = false;
        }

        //Disable mouse events when gestures are used, because they cause problems with
        //Fennec
        mLastMultiTouchTime.start();
    }

    gesture = event->gesture(gSwipeGestureId);
    if (gesture) {
        if (gesture->state() == Qt::GestureStarted) {
            event->accept();
        }
        if (gesture->state() == Qt::GestureFinished) {
            event->accept();
            handled = true;

            MozSwipeGesture* swipe = static_cast<MozSwipeGesture*>(gesture);
            nsIntPoint hotspot;
            hotspot.x = swipe->hotSpot().x();
            hotspot.y = swipe->hotSpot().y();

            // Cancel pinch gesture
            mGesturesCancelled = true;
            mPinchEvent.needDispatch = false;

            double distance = DistanceBetweenPoints(swipe->hotSpot(), mPinchEvent.touchPoint) * 2;
            PRFloat64 delta = distance - mPinchEvent.startDistance;

            DispatchGestureEvent(NS_SIMPLE_GESTURE_MAGNIFY, 0, delta / 2, hotspot);

            result = DispatchGestureEvent(NS_SIMPLE_GESTURE_SWIPE,
                                          swipe->Direction(), 0, hotspot);
        }
        mLastMultiTouchTime.start();
    }

    return result;
}

nsEventStatus
nsWindow::DispatchGestureEvent(PRUint32 aMsg, PRUint32 aDirection,
                               double aDelta, const nsIntPoint& aRefPoint)
{
    nsSimpleGestureEvent mozGesture(true, aMsg, this, 0, 0.0);
    mozGesture.direction = aDirection;
    mozGesture.delta = aDelta;
    mozGesture.refPoint = aRefPoint;

    Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();

    mozGesture.InitBasicModifiers(modifiers & Qt::ControlModifier,
                                  modifiers & Qt::AltModifier,
                                  modifiers & Qt::ShiftModifier,
                                  false);
    mozGesture.button    = 0;
    mozGesture.time      = 0;

    return DispatchEvent(&mozGesture);
}


double
nsWindow::DistanceBetweenPoints(const QPointF &aFirstPoint, const QPointF &aSecondPoint)
{
    double result = 0;
    double deltaX = abs(aFirstPoint.x() - aSecondPoint.x());
    double deltaY = abs(aFirstPoint.y() - aSecondPoint.y());
    result = sqrt(deltaX*deltaX + deltaY*deltaY);
    return result;
}

#endif //qt version check

void
nsWindow::ThemeChanged()
{
    nsGUIEvent event(true, NS_THEMECHANGED, this);

    DispatchEvent(&event);

    // do nothing
    return;
}

nsEventStatus
nsWindow::OnDragMotionEvent(QGraphicsSceneDragDropEvent *aEvent)
{
    LOG(("nsWindow::OnDragMotionSignal\n"));

    nsMouseEvent event(true, NS_DRAGDROP_OVER, 0,
                       nsMouseEvent::eReal);
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::OnDragLeaveEvent(QGraphicsSceneDragDropEvent *aEvent)
{
    // XXX Do we want to pass this on only if the event's subwindow is null?
    LOG(("nsWindow::OnDragLeaveSignal(%p)\n", this));
    nsMouseEvent event(true, NS_DRAGDROP_EXIT, this, nsMouseEvent::eReal);

    return DispatchEvent(&event);
}

nsEventStatus
nsWindow::OnDragDropEvent(QGraphicsSceneDragDropEvent *aDropEvent)
{
    if (aDropEvent->proposedAction() == Qt::CopyAction)
    {
        printf("text version of the data: %s\n", aDropEvent->mimeData()->text().toAscii().data());
        aDropEvent->acceptProposedAction();
    }

    LOG(("nsWindow::OnDragDropSignal\n"));
    nsMouseEvent event(true, NS_DRAGDROP_OVER, 0,
                       nsMouseEvent::eReal);
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::OnDragEnter(QGraphicsSceneDragDropEvent *aDragEvent)
{
    // Is it some format we think we can support?
    if ( aDragEvent->mimeData()->hasFormat(kURLMime)
      || aDragEvent->mimeData()->hasFormat(kURLDataMime)
      || aDragEvent->mimeData()->hasFormat(kURLDescriptionMime)
      || aDragEvent->mimeData()->hasFormat(kHTMLMime)
      || aDragEvent->mimeData()->hasFormat(kUnicodeMime)
      || aDragEvent->mimeData()->hasFormat(kTextMime)
       )
    {
        aDragEvent->acceptProposedAction();
    }

    // XXX Do we want to pass this on only if the event's subwindow is null?

    LOG(("nsWindow::OnDragEnter(%p)\n", this));

    nsMouseEvent event(true, NS_DRAGDROP_ENTER, this, nsMouseEvent::eReal);
    return DispatchEvent(&event);
}

static void
GetBrandName(nsXPIDLString& brandName)
{
    nsCOMPtr<nsIStringBundleService> bundleService =
        mozilla::services::GetStringBundleService();

    nsCOMPtr<nsIStringBundle> bundle;
    if (bundleService)
        bundleService->CreateBundle(
            "chrome://branding/locale/brand.properties",
            getter_AddRefs(bundle));

    if (bundle)
        bundle->GetStringFromName(
            NS_LITERAL_STRING("brandShortName").get(),
            getter_Copies(brandName));

    if (brandName.IsEmpty())
        brandName.Assign(NS_LITERAL_STRING("Mozilla"));
}


nsresult
nsWindow::Create(nsIWidget        *aParent,
                 nsNativeWidget    aNativeParent,
                 const nsIntRect  &aRect,
                 EVENT_CALLBACK    aHandleEventFunction,
                 nsDeviceContext *aContext,
                 nsWidgetInitData *aInitData)
{
    // only set the base parent if we're not going to be a dialog or a
    // toplevel
    nsIWidget *baseParent = aParent;

    if (aInitData &&
        (aInitData->mWindowType == eWindowType_dialog ||
         aInitData->mWindowType == eWindowType_toplevel ||
         aInitData->mWindowType == eWindowType_invisible)) {

        baseParent = nsnull;
        // also drop native parent for toplevel windows
        aNativeParent = nsnull;
    }

    // initialize all the common bits of this class
    BaseCreate(baseParent, aRect, aHandleEventFunction, aContext, aInitData);

    // and do our common creation
    mParent = aParent;

    // save our bounds
    mBounds = aRect;

    // find native parent
    MozQWidget *parent = nsnull;

    if (aParent != nsnull)
        parent = static_cast<MozQWidget*>(aParent->GetNativeData(NS_NATIVE_WIDGET));

    // ok, create our QGraphicsWidget
    mWidget = createQWidget(parent, aNativeParent, aInitData);

    if (!mWidget)
        return NS_ERROR_OUT_OF_MEMORY;

    LOG(("Create: nsWindow [%p] [%p]\n", (void *)this, (void *)mWidget));

    // resize so that everything is set to the right dimensions
    Resize(mBounds.x, mBounds.y, mBounds.width, mBounds.height, false);

    // check if we should listen for resizes
    mListenForResizes = (aNativeParent ||
                         (aInitData && aInitData->mListenForResizes));

    return NS_OK;
}

already_AddRefed<nsIWidget>
nsWindow::CreateChild(const nsIntRect&  aRect,
                      EVENT_CALLBACK    aHandleEventFunction,
                      nsDeviceContext* aContext,
                      nsWidgetInitData* aInitData,
                      bool              /*aForceUseIWidgetParent*/)
{
    //We need to force parent widget, otherwise GetTopLevelWindow doesn't work
    return nsBaseWidget::CreateChild(aRect,
                                     aHandleEventFunction,
                                     aContext,
                                     aInitData,
                                     true); // Force parent
}


NS_IMETHODIMP
nsWindow::SetWindowClass(const nsAString &xulWinType)
{
    if (!mWidget)
      return NS_ERROR_FAILURE;

    nsXPIDLString brandName;
    GetBrandName(brandName);

#ifdef MOZ_X11
    XClassHint *class_hint = XAllocClassHint();
    if (!class_hint)
      return NS_ERROR_OUT_OF_MEMORY;
    const char *role = NULL;
    class_hint->res_name = ToNewCString(xulWinType);
    if (!class_hint->res_name) {
      XFree(class_hint);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    class_hint->res_class = ToNewCString(brandName);
    if (!class_hint->res_class) {
      nsMemory::Free(class_hint->res_name);
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

    QWidget *widget = GetViewWidget();
    // If widget not show, handle might be null
    if (widget && widget->winId())
        XSetClassHint(gfxQtPlatform::GetXDisplay(widget),
                      widget->winId(),
                      class_hint);

    nsMemory::Free(class_hint->res_class);
    nsMemory::Free(class_hint->res_name);
    XFree(class_hint);
#endif

    return NS_OK;
}

void
nsWindow::NativeResize(PRInt32 aWidth, PRInt32 aHeight, bool    aRepaint)
{
    LOG(("nsWindow::NativeResize [%p] %d %d\n", (void *)this,
         aWidth, aHeight));

    mNeedsResize = false;

    if (mIsTopLevel) {
        QGraphicsView *widget = qobject_cast<QGraphicsView*>(GetViewWidget());
        NS_ENSURE_TRUE(widget,);
        // map from in-scene widget to scene, from scene to view.
        QRect r = widget->mapFromScene(mWidget->mapToScene(QRect(0, 0, aWidth, aHeight))).boundingRect();
        // going from QPolygon to QRect includes the points, adding one to width and height
        r.adjust(0, 0, -1, -1);
        widget->resize(r.width(), r.height());
    }
    else {
        mWidget->resize(aWidth, aHeight);
    }

    if (aRepaint)
        mWidget->update();
}

void
nsWindow::NativeResize(PRInt32 aX, PRInt32 aY,
                       PRInt32 aWidth, PRInt32 aHeight,
                       bool    aRepaint)
{
    LOG(("nsWindow::NativeResize [%p] %d %d %d %d\n", (void *)this,
         aX, aY, aWidth, aHeight));

    mNeedsResize = false;
    mNeedsMove = false;

    if (mIsTopLevel) {
        QGraphicsView *widget = qobject_cast<QGraphicsView*>(GetViewWidget());
        NS_ENSURE_TRUE(widget,);
        // map from in-scene widget to scene, from scene to view.
        QRect r = widget->mapFromScene(mWidget->mapToScene(QRect(aX, aY, aWidth, aHeight))).boundingRect();
        // going from QPolygon to QRect includes the points, adding one to width and height
        r.adjust(0, 0, -1, -1);
        widget->setGeometry(r.x(), r.y(), r.width(), r.height());
    }
    else {
        mWidget->setGeometry(aX, aY, aWidth, aHeight);
    }

    if (aRepaint)
        mWidget->update();
}

void
nsWindow::NativeShow(bool aAction)
{
    if (aAction) {
        QWidget *widget = GetViewWidget();
        // On e10s, we never want the child process or plugin process
        // to go fullscreen because if we do the window because visible
        // do to disabled Qt-Xembed
        if (widget &&
            !widget->isVisible())
            MakeFullScreen(mSizeMode == nsSizeMode_Fullscreen);
        mWidget->show();

        // unset our flag now that our window has been shown
        mNeedsShow = false;
    }
    else
        mWidget->hide();
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
    return nsnull;
}

nsresult
nsWindow::SetWindowIconList(const nsTArray<nsCString> &aIconList)
{
    QIcon icon;

    for (PRUint32 i = 0; i < aIconList.Length(); ++i) {
        const char *path = aIconList[i].get();
        LOG(("window [%p] Loading icon from %s\n", (void *)this, path));
        icon.addFile(path);
    }

    QWidget *widget = GetViewWidget();
    NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);
    widget->setWindowIcon(icon);

    return NS_OK;
}

void
nsWindow::SetDefaultIcon(void)
{
    SetIcon(NS_LITERAL_STRING("default"));
}

void nsWindow::QWidgetDestroyed()
{
    mWidget = nsnull;
}

NS_IMETHODIMP
nsWindow::MakeFullScreen(bool aFullScreen)
{
    QWidget *widget = GetViewWidget();
    NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);
    if (aFullScreen) {
        if (mSizeMode != nsSizeMode_Fullscreen)
            mLastSizeMode = mSizeMode;

        mSizeMode = nsSizeMode_Fullscreen;
        widget->showFullScreen();
    }
    else {
        mSizeMode = mLastSizeMode;

        switch (mSizeMode) {
        case nsSizeMode_Maximized:
            widget->showMaximized();
            break;
        case nsSizeMode_Minimized:
            widget->showMinimized();
            break;
        case nsSizeMode_Normal:
            widget->showNormal();
            break;
        default:
            widget->showNormal();
            break;
        }
    }

    NS_ASSERTION(mLastSizeMode != nsSizeMode_Fullscreen,
                 "mLastSizeMode should never be fullscreen");
    return nsBaseWidget::MakeFullScreen(aFullScreen);
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

    // For some window managers, adding or removing window decorations
    // requires unmapping and remapping our toplevel window.  Go ahead
    // and flush the queue here so that we don't end up with a BadWindow
    // error later when this happens (when the persistence timer fires
    // and GetWindowPos is called)
    QWidget *widget = GetViewWidget();
    NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);
#ifdef MOZ_X11
    XSync(gfxQtPlatform::GetXDisplay(widget), False);
#endif

    return NS_OK;
}

//////////////////////////////////////////////////////////////////////
// These are all of our drag and drop operations

void
nsWindow::InitDragEvent(nsMouseEvent &aEvent)
{
    // set the keyboard modifiers
}

// This will update the drag action based on the information in the
// drag context.

/* static */
nsresult
initialize_prefs(void)
{
    // check to see if we should set our raise pref
    return NS_OK;
}

inline bool
is_context_menu_key(const nsKeyEvent& aKeyEvent)
{
    return ((aKeyEvent.keyCode == NS_VK_F10 && aKeyEvent.IsShift() &&
             !aKeyEvent.IsControl() && !aKeyEvent.IsMeta() &&
             !aKeyEvent.IsAlt()) ||
            (aKeyEvent.keyCode == NS_VK_CONTEXT_MENU && !aKeyEvent.IsShift() &&
             !aKeyEvent.IsControl() && !aKeyEvent.IsMeta() &&
             !aKeyEvent.IsAlt()));
}

void
key_event_to_context_menu_event(nsMouseEvent &aEvent,
                                QKeyEvent *aGdkEvent)
{
    aEvent.refPoint = nsIntPoint(0, 0);
    aEvent.modifiers = 0;
    aEvent.time = 0;
    aEvent.clickCount = 1;
}

// nsChildWindow class

nsChildWindow::nsChildWindow()
{
}

nsChildWindow::~nsChildWindow()
{
}

nsPopupWindow::nsPopupWindow()
{
#ifdef DEBUG_WIDGETS
    qDebug("===================== popup!");
#endif
}

nsPopupWindow::~nsPopupWindow()
{
}

NS_IMETHODIMP_(bool)
nsWindow::HasGLContext()
{
    QGraphicsView *view = qobject_cast<QGraphicsView*>(GetViewWidget());
    return view && qobject_cast<QGLWidget*>(view->viewport());
}

MozQWidget*
nsWindow::createQWidget(MozQWidget *parent,
                        nsNativeWidget nativeParent,
                        nsWidgetInitData *aInitData)
{
    const char *windowName = NULL;
    Qt::WindowFlags flags = Qt::Widget;
    QWidget *parentWidget = (parent && parent->getReceiver()) ?
            parent->getReceiver()->GetViewWidget() : nsnull;

#ifdef DEBUG_WIDGETS
    qDebug("NEW WIDGET\n\tparent is %p (%s)", (void*)parent,
           parent ? qPrintable(parent->objectName()) : "null");
#endif

    // ok, create our windows
    switch (mWindowType) {
    case eWindowType_dialog:
        windowName = "topLevelDialog";
        mIsTopLevel = true;
        flags |= Qt::Dialog;
        break;
    case eWindowType_popup:
        windowName = "topLevelPopup";
        break;
    case eWindowType_toplevel:
        windowName = "topLevelWindow";
        mIsTopLevel = true;
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

    MozQWidget* parentQWidget = nsnull;
    if (parent) {
        parentQWidget = parent;
    } else if (nativeParent && nativeParent != PARENTLESS_WIDGET) {
        parentQWidget = static_cast<MozQWidget*>(nativeParent);
    }
    MozQWidget * widget = new MozQWidget(this, parentQWidget);
    if (!widget)
        return nsnull;

    // make only child and plugin windows focusable
    if (eWindowType_child == mWindowType || eWindowType_plugin == mWindowType) {
        widget->setFlag(QGraphicsItem::ItemIsFocusable);
        widget->setFocusPolicy(Qt::WheelFocus);
    }

    // create a QGraphicsView if this is a new toplevel window

    if (mIsTopLevel) {
        QGraphicsView* newView =
            nsFastStartup::GetStartupGraphicsView(parentWidget, widget);

        if (mWindowType == eWindowType_dialog) {
            newView->setWindowModality(Qt::WindowModal);
        }

#if defined(MOZ_PLATFORM_MAEMO) || defined(MOZ_GL_PROVIDER)
        if (GetShouldAccelerate()) {
            // Only create new OGL widget if it is not yet installed
            if (!HasGLContext()) {
                MozQGraphicsView *qview = qobject_cast<MozQGraphicsView*>(newView);
                if (qview) {
                    qview->setGLWidgetEnabled(true);
                }
            }
        }
#endif

        if (gfxQtPlatform::GetPlatform()->GetRenderMode() == gfxQtPlatform::RENDER_DIRECT) {
            // Disable double buffer and system background rendering
#if defined(MOZ_X11) && (QT_VERSION < QT_VERSION_CHECK(5,0,0))
            newView->viewport()->setAttribute(Qt::WA_PaintOnScreen, true);
#endif
            newView->viewport()->setAttribute(Qt::WA_NoSystemBackground, true);
        }
        // Enable gestures:
#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0))
#if defined MOZ_ENABLE_MEEGOTOUCH
        // Disable default Gesture filters (speedup filtering)
        newView->viewport()->ungrabGesture(Qt::PanGesture);
        newView->viewport()->ungrabGesture(Qt::TapGesture);
        newView->viewport()->ungrabGesture(Qt::TapAndHoldGesture);
        newView->viewport()->ungrabGesture(Qt::SwipeGesture);
#endif

        // Enable required filters
        newView->viewport()->grabGesture(Qt::PinchGesture);
        newView->viewport()->grabGesture(gSwipeGestureId);
#endif
        newView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        newView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0))
        // Top level widget is just container, and should not be painted
        widget->setFlag(QGraphicsItem::ItemHasNoContents);
#endif

#ifdef MOZ_X11
        if (newView->effectiveWinId()) {
            XSetWindowBackgroundPixmap(mozilla::DefaultXDisplay(),
                                       newView->effectiveWinId(), None);
        }
#endif
    }

    if (mWindowType == eWindowType_popup) {
        widget->setZValue(100);

        // XXX is this needed for Qt?
        // gdk does not automatically set the cursor for "temporary"
        // windows, which are what gtk uses for popups.
        SetCursor(eCursor_standard);
    } else if (mIsTopLevel) {
        SetDefaultIcon();
    }
#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0))
#if defined MOZ_ENABLE_MEEGOTOUCH
    // Disable default Gesture filters (speedup filtering)
    widget->ungrabGesture(Qt::PanGesture);
    widget->ungrabGesture(Qt::TapGesture);
    widget->ungrabGesture(Qt::TapAndHoldGesture);
    widget->ungrabGesture(Qt::SwipeGesture);
#endif
    widget->grabGesture(Qt::PinchGesture);
    widget->grabGesture(gSwipeGestureId);
#endif

    return widget;
}

// return the gfxASurface for rendering to this widget
gfxASurface*
nsWindow::GetThebesSurface()
{
    /* This is really a dummy surface; this is only used when doing reflow, because
     * we need a RenderingContext to measure text against.
     */
    if (mThebesSurface)
        return mThebesSurface;

#ifdef CAIRO_HAS_QT_SURFACE
    gfxQtPlatform::RenderMode renderMode = gfxQtPlatform::GetPlatform()->GetRenderMode();
    if (renderMode == gfxQtPlatform::RENDER_QPAINTER) {
        mThebesSurface = new gfxQPainterSurface(gfxIntSize(1, 1), gfxASurface::CONTENT_COLOR);
    }
#endif
    if (!mThebesSurface) {
        gfxASurface::gfxImageFormat imageFormat = gfxASurface::ImageFormatRGB24;
        mThebesSurface = new gfxImageSurface(gfxIntSize(1, 1), imageFormat);
    }

    return mThebesSurface;
}

NS_IMETHODIMP
nsWindow::BeginResizeDrag(nsGUIEvent* aEvent, PRInt32 aHorizontal, PRInt32 aVertical)
{
    NS_ENSURE_ARG_POINTER(aEvent);

    if (aEvent->eventStructType != NS_MOUSE_EVENT) {
        // you can only begin a resize drag with a mouse event
        return NS_ERROR_INVALID_ARG;
    }

    nsMouseEvent* mouse_event = static_cast<nsMouseEvent*>(aEvent);

    if (mouse_event->button != nsMouseEvent::eLeftButton) {
        // you can only begin a resize drag with the left mouse button
        return NS_ERROR_INVALID_ARG;
    }

    return NS_OK;
}

nsEventStatus
nsWindow::contextMenuEvent(QGraphicsSceneContextMenuEvent *)
{
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::imComposeEvent(QInputMethodEvent *event, bool &handled)
{
    // XXX Needs to check whether this widget has been destroyed or not after
    //     each DispatchEvent().

    nsCompositionEvent start(true, NS_COMPOSITION_START, this);
    DispatchEvent(&start);

    nsAutoString compositionStr(event->commitString().utf16());

    if (!compositionStr.IsEmpty()) {
      nsCompositionEvent update(true, NS_COMPOSITION_UPDATE, this);
      update.data = compositionStr;
      DispatchEvent(&update);
    }

    nsTextEvent text(true, NS_TEXT_TEXT, this);
    text.theText = compositionStr;
    DispatchEvent(&text);

    nsCompositionEvent end(true, NS_COMPOSITION_END, this);
    end.data = compositionStr;
    DispatchEvent(&end);

    return nsEventStatus_eIgnore;
}

nsIWidget *
nsWindow::GetParent(void)
{
    return mParent;
}

float
nsWindow::GetDPI()
{
    QDesktopWidget* rootWindow = QApplication::desktop();
    double heightInches = rootWindow->heightMM()/25.4;
    if (heightInches < 0.25) {
        // Something's broken, but we'd better not crash.
        return 96.0f;
    }

    return float(rootWindow->height()/heightInches);
}

void
nsWindow::DispatchActivateEvent(void)
{
    nsGUIEvent event(true, NS_ACTIVATE, this);
    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsWindow::DispatchDeactivateEvent(void)
{
    nsGUIEvent event(true, NS_DEACTIVATE, this);
    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsWindow::DispatchActivateEventOnTopLevelWindow(void)
{
    nsWindow * topLevelWindow = static_cast<nsWindow*>(GetTopLevelWidget());
    if (topLevelWindow != nsnull)
         topLevelWindow->DispatchActivateEvent();
}

void
nsWindow::DispatchDeactivateEventOnTopLevelWindow(void)
{
    nsWindow * topLevelWindow = static_cast<nsWindow*>(GetTopLevelWidget());
    if (topLevelWindow != nsnull)
         topLevelWindow->DispatchDeactivateEvent();
}

void
nsWindow::DispatchResizeEvent(nsIntRect &aRect, nsEventStatus &aStatus)
{
    nsSizeEvent event(true, NS_SIZE, this);

    event.windowSize = &aRect;
    event.refPoint.x = aRect.x;
    event.refPoint.y = aRect.y;
    event.mWinWidth = aRect.width;
    event.mWinHeight = aRect.height;

    nsEventStatus status;
    DispatchEvent(&event, status); 
}

NS_IMETHODIMP
nsWindow::DispatchEvent(nsGUIEvent *aEvent,
                              nsEventStatus &aStatus)
{
#ifdef DEBUG
    debug_DumpEvent(stdout, aEvent->widget, aEvent,
                    nsCAutoString("something"), 0);
#endif

    aStatus = nsEventStatus_eIgnore;

    // send it to the standard callback
    if (mEventCallback)
        aStatus = (* mEventCallback)(aEvent);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Show(bool aState)
{
    LOG(("nsWindow::Show [%p] state %d\n", (void *)this, aState));

    mIsShown = aState;

#ifdef MOZ_ENABLE_QTMOBILITY
    if (mWidget &&
        (mWindowType == eWindowType_toplevel ||
         mWindowType == eWindowType_dialog ||
         mWindowType == eWindowType_popup))
    {
        if (!gOrientation) {
            gOrientation = new QOrientationSensor();
            gOrientation->addFilter(&gOrientationFilter);
            gOrientation->start();
            if (!gOrientation->isActive()) {
                qWarning("Orientationsensor didn't start!");
            }
            gOrientationFilter.filter(gOrientation->reading());

            QObject::connect((QObject*) &gOrientationFilter, SIGNAL(orientationChanged()),
                             mWidget, SLOT(orientationChanged()));
        }
    }
#endif

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
    else
        // If someone is hiding this widget, clear any needing show flag.
        mNeedsShow = false;

    NativeShow(aState);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, bool aRepaint)
{
    mBounds.width = aWidth;
    mBounds.height = aHeight;

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
    else if (AreBoundsSane() && mListenForResizes) {
        // For widgets that we listen for resizes for (widgets created
        // with native parents) we apparently _always_ have to resize.  I
        // dunno why, but apparently we're lame like that.
        NativeResize(aWidth, aHeight, aRepaint);
    }
    else {
        mNeedsResize = true;
    }

    // synthesize a resize event if this isn't a toplevel
    if (mIsTopLevel || mListenForResizes) {
        nsIntRect rect(mBounds.x, mBounds.y, aWidth, aHeight);
        nsEventStatus status;
        DispatchResizeEvent(rect, status);
    }

    NotifyRollupGeometryChange(gRollupListener);
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight,
                 bool aRepaint)
{
    mBounds.x = aX;
    mBounds.y = aY;
    mBounds.width = aWidth;
    mBounds.height = aHeight;

    mPlaced = true;

    if (!mWidget)
        return NS_OK;

    // Has this widget been set to visible?
    if (mIsShown) {
        // Are the bounds sane?
        if (AreBoundsSane()) {
            // Yep?  Resize the window
            NativeResize(aX, aY, aWidth, aHeight, aRepaint);
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
        NativeResize(aX, aY, aWidth, aHeight, aRepaint);
    }
    else {
        mNeedsResize = true;
        mNeedsMove = true;
    }

    if (mIsTopLevel || mListenForResizes) {
        // synthesize a resize event
        nsIntRect rect(aX, aY, aWidth, aHeight);
        nsEventStatus status;
        DispatchResizeEvent(rect, status);
    }

    if (aRepaint)
        mWidget->update();

    NotifyRollupGeometryChange(gRollupListener);
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

void
nsWindow::OnDestroy(void)
{
    if (mOnDestroyCalled)
        return;

    mOnDestroyCalled = true;

    // release references to children and device context
    nsBaseWidget::OnDestroy();

    // let go of our parent
    mParent = nsnull;

    nsCOMPtr<nsIWidget> kungFuDeathGrip = this;

    nsGUIEvent event(true, NS_DESTROY, this);
    nsEventStatus status;
    DispatchEvent(&event, status);
}

bool
nsWindow::AreBoundsSane(void)
{
    if (mBounds.width > 0 && mBounds.height > 0)
        return true;

    return false;
}

#if defined(MOZ_X11) && (MOZ_PLATFORM_MAEMO == 6)
typedef enum {
    VKBUndefined,
    VKBOpen,
    VKBClose
} PluginVKBState;

static QCoreApplication::EventFilter previousEventFilter = NULL;

static PluginVKBState
GetPluginVKBState(Window aWinId)
{
    // Set default value as unexpected error
    PluginVKBState imeState = VKBUndefined;
    Display *display = mozilla::DefaultXDisplay();

    Atom actualType;
    int actualFormat;
    unsigned long nitems;
    unsigned long bytes;
    union {
        unsigned char* asUChar;
        unsigned long* asLong;
    } data = {0};
    int status = XGetWindowProperty(display, aWinId, sPluginIMEAtom,
                                    0, 1, False, AnyPropertyType,
                                    &actualType, &actualFormat, &nitems,
                                    &bytes, &data.asUChar);

    if (status == Success && actualType == XA_CARDINAL && actualFormat == 32 && nitems == 1) {
        // Assume that plugin set value false - close VKB, true - open VKB
        imeState = data.asLong[0] ? VKBOpen : VKBClose;
    }

    if (status == Success) {
        XFree(data.asUChar);
    }

    return imeState;
}

static void
SetVKBState(Window aWinId, PluginVKBState aState)
{
    Display *display = mozilla::DefaultXDisplay();
    if (aState != VKBUndefined) {
        unsigned long isOpen = aState == VKBOpen ? 1 : 0;
        XChangeProperty(display, aWinId, sPluginIMEAtom, XA_CARDINAL, 32,
                        PropModeReplace, (unsigned char *) &isOpen, 1);
    } else {
        XDeleteProperty(display, aWinId, sPluginIMEAtom);
    }
    XSync(display, False);
}

static bool
x11EventFilter(void* message, long* result)
{
    XEvent* event = static_cast<XEvent*>(message);
    if (event->type == PropertyNotify) {
        if (event->xproperty.atom == sPluginIMEAtom) {
            PluginVKBState state = GetPluginVKBState(event->xproperty.window);
            if (state == VKBOpen) {
                MozQWidget::requestVKB();
            } else if (state == VKBClose) {
                MozQWidget::hideVKB();
            }
            return true;
        }
    }
    if (previousEventFilter) {
        return previousEventFilter(message, result);
    }

    return false;
}
#endif

NS_IMETHODIMP_(void)
nsWindow::SetInputContext(const InputContext& aContext,
                          const InputContextAction& aAction)
{
    NS_ENSURE_TRUE(mWidget, );

    // SetSoftwareKeyboardState uses mInputContext,
    // so, before calling that, record aContext in mInputContext.
    mInputContext = aContext;

#if defined(MOZ_X11) && (MOZ_PLATFORM_MAEMO == 6)
    if (sPluginIMEAtom) {
        static QCoreApplication::EventFilter currentEventFilter = NULL;
        if (mInputContext.mIMEState.mEnabled == IMEState::PLUGIN &&
            currentEventFilter != x11EventFilter) {
            // Install event filter for listening Plugin IME state changes
            previousEventFilter = QCoreApplication::instance()->setEventFilter(x11EventFilter);
            currentEventFilter = x11EventFilter;
        } else if (mInputContext.mIMEState.mEnabled != IMEState::PLUGIN &&
                   currentEventFilter == x11EventFilter) {
            // Remove event filter
            QCoreApplication::instance()->setEventFilter(previousEventFilter);
            currentEventFilter = previousEventFilter;
            previousEventFilter = NULL;
            QWidget* view = GetViewWidget();
            if (view) {
                SetVKBState(view->winId(), VKBUndefined);
            }
        }
    }
#endif

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
    return mInputContext;
}

void
nsWindow::SetSoftwareKeyboardState(bool aOpen,
                                   const InputContextAction& aAction)
{
    if (aOpen) {
        NS_ENSURE_TRUE(mInputContext.mIMEState.mEnabled != IMEState::DISABLED,);

        // Ensure that opening the virtual keyboard is allowed for this specific
        // InputContext depending on the content.ime.strict.policy pref
        if (mInputContext.mIMEState.mEnabled != IMEState::PLUGIN &&
            Preferences::GetBool("content.ime.strict_policy", false) &&
            !aAction.ContentGotFocusByTrustedCause() &&
            !aAction.UserMightRequestOpenVKB()) {
            return;
        }
#if defined(MOZ_X11) && (MOZ_PLATFORM_MAEMO == 6)
        // doen't open VKB if plugin did set closed state
        else if (sPluginIMEAtom) {
            QWidget* view = GetViewWidget();
            if (view && GetPluginVKBState(view->winId()) == VKBClose) {
                return;
            }
        }
#endif
    }

    if (aOpen) {
        // VKB open need to be delayed in order to give
        // to plugins chance prevent VKB from opening
        PRInt32 openDelay =
            Preferences::GetInt("ui.vkb.open.delay", 200);
        MozQWidget::requestVKB(openDelay, mWidget);
    } else {
        MozQWidget::hideVKB();
    }
    return;
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

PRUint32
nsWindow::GetGLFrameBufferFormat()
{
    if (mLayerManager &&
        mLayerManager->GetBackendType() == mozilla::layers::LAYERS_OPENGL) {
        // On maemo the hardware fb has RGB format.
#ifdef MOZ_PLATFORM_MAEMO
        return LOCAL_GL_RGB;
#else
        return LOCAL_GL_RGBA;
#endif
    }
    return LOCAL_GL_NONE;
}

