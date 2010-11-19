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
 *   Mats Palmgren <matspal@gmail.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Romashin Oleg <romaxa@gmail.com>
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Jeremias Bosch <jeremias.bosch@gmail.com>
 *   Steffen Imhof <steffen.imhof@gmail.com>
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

#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtGui/QCursor>
#include <QtGui/QIcon>
#include <QtGui/QX11Info>
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsView>
#include <QtGui/QGraphicsSceneContextMenuEvent>
#include <QtGui/QGraphicsSceneDragDropEvent>
#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtGui/QGraphicsSceneHoverEvent>
#include <QtGui/QGraphicsSceneWheelEvent>
#include <QtGui/QGraphicsSceneResizeEvent>
#include <QtGui/QStyleOptionGraphicsItem>
#include <QPaintEngine>

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
#endif // QT version check

#ifdef MOZ_X11
#include <QX11Info>
#include <X11/Xlib.h>
#endif //MOZ_X11

#include "nsXULAppAPI.h"

#include "prlink.h"

#include "nsWindow.h"
#include "mozqwidget.h"

#include "nsToolkit.h"
#include "nsIDeviceContext.h"
#include "nsIdleService.h"
#include "nsIRenderingContext.h"
#include "nsIRegion.h"
#include "nsIRollupListener.h"
#include "nsIMenuRollup.h"
#include "nsWidgetsCID.h"
#include "nsQtKeyUtils.h"
#include "mozilla/Services.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIStringBundle.h"
#include "nsGfxCIID.h"

#include "imgIContainer.h"
#include "nsGfxCIID.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsAutoPtr.h"

#include "gfxQtPlatform.h"
#include "gfxXlibSurface.h"
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
#endif //MOZ_X11

#include <QtOpenGL/QGLWidget>
#define GLdouble_defined 1
#include "Layers.h"
#include "LayerManagerOGL.h"

#include "nsShmImage.h"
extern "C" {
#include "pixman.h"
}

using namespace mozilla;

// imported in nsWidgetFactory.cpp
PRBool gDisableNativeTheme = PR_FALSE;

// Cached offscreen surface
static nsRefPtr<gfxASurface> gBufferSurface;
#ifdef MOZ_HAVE_SHMIMAGE
// If we're using xshm rendering, mThebesSurface wraps gShmImage
nsRefPtr<nsShmImage> gShmImage;
#endif

static int gBufferPixmapUsageCount = 0;
static gfxIntSize gBufferMaxSize(0, 0);

/* For PrepareNativeWidget */
static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);

// initialization static functions 
static nsresult    initialize_prefs        (void);

static NS_DEFINE_IID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

#define NS_WINDOW_TITLE_MAX_LENGTH 4095

#define kWindowPositionSlop 20

// Qt
static const int WHEEL_DELTA = 120;
static PRBool gGlobalsInitialized = PR_FALSE;

static nsIRollupListener*          gRollupListener;
static nsIMenuRollup*              gMenuRollup;
static nsWeakPtr                   gRollupWindow;
static PRBool                      gConsumeRollupEvent;

static PRBool     check_for_rollup(double aMouseX, double aMouseY,
                                   PRBool aIsWheel);
static bool
is_mouse_in_window (MozQWidget* aWindow, double aMouseX, double aMouseY);

static PRBool
isContextMenuKeyEvent(const QKeyEvent *qe)
{
    PRUint32 kc = QtKeyCodeToDOMKeyCode(qe->key());
    if (qe->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        return PR_FALSE;

    PRBool isShift = qe->modifiers() & Qt::ShiftModifier;
    return (kc == NS_VK_F10 && isShift) ||
        (kc == NS_VK_CONTEXT_MENU && !isShift);
}

static void
InitKeyEvent(nsKeyEvent &aEvent, QKeyEvent *aQEvent)
{
    aEvent.isShift   = (aQEvent->modifiers() & Qt::ShiftModifier) ? PR_TRUE : PR_FALSE;
    aEvent.isControl = (aQEvent->modifiers() & Qt::ControlModifier) ? PR_TRUE : PR_FALSE;
    aEvent.isAlt     = (aQEvent->modifiers() & Qt::AltModifier) ? PR_TRUE : PR_FALSE;
    aEvent.isMeta    = (aQEvent->modifiers() & Qt::MetaModifier) ? PR_TRUE : PR_FALSE;
    aEvent.time      = 0;

    // The transformations above and in qt for the keyval are not invertible
    // so link to the QKeyEvent (which will vanish soon after return from the
    // event callback) to give plugins access to hardware_keycode and state.
    // (An XEvent would be nice but the QKeyEvent is good enough.)
    aEvent.pluginEvent = (void *)aQEvent;
}

nsWindow::nsWindow()
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__, (void *)this));

    mIsTopLevel       = PR_FALSE;
    mIsDestroyed      = PR_FALSE;
    mIsShown          = PR_FALSE;
    mEnabled          = PR_TRUE;
    mWidget              = nsnull;
    mIsVisible           = PR_FALSE;
    mActivatePending     = PR_FALSE;
    mWindowType          = eWindowType_child;
    mSizeState           = nsSizeMode_Normal;
    mLastSizeMode        = nsSizeMode_Normal;
    mPluginType          = PluginType_NONE;
    mQCursor             = Qt::ArrowCursor;
    mNeedsResize         = PR_FALSE;
    mNeedsMove           = PR_FALSE;
    mListenForResizes    = PR_FALSE;
    mNeedsShow           = PR_FALSE;
    mGesturesCancelled   = PR_FALSE;
    
    if (!gGlobalsInitialized) {
        gGlobalsInitialized = PR_TRUE;

        // It's OK if either of these fail, but it may not be one day.
        initialize_prefs();
    }

    memset(mKeyDownFlags, 0, sizeof(mKeyDownFlags));

    mIsTransparent = PR_FALSE;

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

    gBufferMaxSize.width = PR_MAX(gBufferMaxSize.width, size.width);
    gBufferMaxSize.height = PR_MAX(gBufferMaxSize.height, size.height);

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
            gShmImage = nsShmImage::Create(gBufferMaxSize,
                                           (Visual*)aWidget->x11Info().visual(),
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
                      PR_TRUE);
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
    mIsDestroyed = PR_TRUE;

    if (gBufferPixmapUsageCount &&
        --gBufferPixmapUsageCount == 0) {

        gBufferSurface = nsnull;
#ifdef MOZ_HAVE_SHMIMAGE
        gShmImage = nsnull;
#endif
    }

    nsCOMPtr<nsIWidget> rollupWidget = do_QueryReferent(gRollupWindow);
    if (static_cast<nsIWidget *>(this) == rollupWidget.get()) {
        if (gRollupListener)
            gRollupListener->Rollup(nsnull, nsnull);
        gRollupWindow = nsnull;
        gRollupListener = nsnull;
        NS_IF_RELEASE(gMenuRollup);
    }

    if (mLayerManager) {
        mLayerManager->Destroy();
    }
    mLayerManager = nsnull;

    Show(PR_FALSE);

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
    if (aNewParent) {
        ReparentNativeWidget(aNewParent);
        aNewParent->AddChild(this);
        return NS_OK;
    }
    if (mWidget) {
        mWidget->setParentItem(0);
    }
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
nsWindow::SetModal(PRBool aModal)
{
    LOG(("nsWindow::SetModal [%p] %d, widget[%p]\n", (void *)this, aModal, mWidget));
    if (mWidget)
        mWidget->setModal(aModal);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::IsVisible(PRBool & aState)
{
    aState = mIsShown;
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ConstrainPosition(PRBool aAllowSlop, PRInt32 *aX, PRInt32 *aY)
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

        // the internal QGraphicsWidget is always in the top corner of
        // the view if it is a toplevel one
        aX = aY = 0;
    }

    if (aX == mBounds.x && aY == mBounds.y)
        return NS_OK;

    if (!mWidget)
        return NS_OK;

    // update the bounds
    QPointF pos( aX, aY );
    if (mWidget) {
        // the position of the widget is set relative to the parent
        // so we map the coordinates accordingly
        pos = mWidget->mapFromScene(pos);
        pos = mWidget->mapToParent(pos);
        mWidget->setPos(pos);
    }

    mBounds.x = pos.x();
    mBounds.y = pos.y();


    return NS_OK;
}

NS_IMETHODIMP
nsWindow::PlaceBehind(nsTopLevelWidgetZPlacement  aPlacement,
                      nsIWidget                  *aWidget,
                      PRBool                      aActivate)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::SetSizeMode(PRInt32 aMode)
{
    nsresult rv;

    LOG(("nsWindow::SetSizeMode [%p] %d\n", (void *)this, aMode));

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
nsWindow::SetFocus(PRBool aRaise)
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
    aRect = nsIntRect(WidgetToScreenOffset(), mBounds.Size());
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
nsWindow::Invalidate(const nsIntRect &aRect,
                     PRBool        aIsSynchronous)
{
    LOGDRAW(("Invalidate (rect) [%p,%p]: %d %d %d %d (sync: %d)\n", (void *)this,
             (void*)mWidget,aRect.x, aRect.y, aRect.width, aRect.height, aIsSynchronous));

    if (!mWidget)
        return NS_OK;

    mDirtyScrollArea = mDirtyScrollArea.united(QRect(aRect.x, aRect.y, aRect.width, aRect.height));

    mWidget->update(aRect.x, aRect.y, aRect.width, aRect.height);

    // QGraphicsItems cannot trigger a repaint themselves, so we start it on the view
    if (aIsSynchronous) {
        QWidget *widget = GetViewWidget();
        if (widget)
            widget->repaint();
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Update()
{
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
            QWidget *widget = GetViewWidget();
            return widget ? widget->x11Info().display() : nsnull;
        }
        break;

    case NS_NATIVE_GRAPHIC: {
        NS_ASSERTION(nsnull != mToolkit, "NULL toolkit, unable to get a GC");
        return (void *)static_cast<nsToolkit *>(mToolkit)->GetSharedGC();
        break;
    }

    case NS_NATIVE_SHELLWIDGET: {
        QWidget* widget = nsnull;
        if (mWidget && mWidget->scene())
            widget = mWidget->scene()->views()[0]->viewport();
        return (void *) widget;
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

    nsCOMPtr<nsILocalFile> iconFile;
    nsCAutoString path;
    nsTArray<nsCString> iconList;

    // Look for icons with the following suffixes appended to the base name.
    // The last two entries (for the old XPM format) will be ignored unless
    // no icons are found using the other suffixes. XPM icons are depricated.

    const char extensions[6][7] = { ".png", "16.png", "32.png", "48.png",
                                    ".xpm", "16.xpm" };

    for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(extensions); i++) {
        // Don't bother looking for XPM versions if we found a PNG.
        if (i == NS_ARRAY_LENGTH(extensions) - 2 && iconList.Length())
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
nsWindow::EnableDragDrop(PRBool aEnable)
{
    mWidget->setAcceptDrops(aEnable);
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::CaptureMouse(PRBool aCapture)
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
                              nsIMenuRollup     *aMenuRollup,
                              PRBool             aDoCapture,
                              PRBool             aConsumeRollupEvent)
{
    if (!mWidget)
        return NS_OK;

    LOG(("CaptureRollupEvents %p\n", (void *)this));

    if (aDoCapture) {
        gConsumeRollupEvent = aConsumeRollupEvent;
        gRollupListener = aListener;
        NS_IF_RELEASE(gMenuRollup);
        gMenuRollup = aMenuRollup;
        NS_IF_ADDREF(aMenuRollup);
        gRollupWindow = do_GetWeakReference(static_cast<nsIWidget*>(this));
    }
    else {
        gRollupListener = nsnull;
        NS_IF_RELEASE(gMenuRollup);
        gRollupWindow = nsnull;
    }

    return NS_OK;
}

PRBool
check_for_rollup(double aMouseX, double aMouseY,
                 PRBool aIsWheel)
{
    PRBool retVal = PR_FALSE;
    nsCOMPtr<nsIWidget> rollupWidget = do_QueryReferent(gRollupWindow);

    if (rollupWidget && gRollupListener) {
        MozQWidget *currentPopup =
            (MozQWidget *)rollupWidget->GetNativeData(NS_NATIVE_WINDOW);

        if (!is_mouse_in_window(currentPopup, aMouseX, aMouseY)) {
            PRBool rollup = PR_TRUE;
            if (aIsWheel) {
                gRollupListener->ShouldRollupOnMouseWheelEvent(&rollup);
                retVal = PR_TRUE;
            }
            // if we're dealing with menus, we probably have submenus and
            // we don't want to rollup if the clickis in a parent menu of
            // the current submenu
            PRUint32 popupsToRollup = PR_UINT32_MAX;
            if (gMenuRollup) {
                nsAutoTArray<nsIWidget*, 5> widgetChain;
                PRUint32 sameTypeCount = gMenuRollup->GetSubmenuWidgetChain(&widgetChain);
                for (PRUint32 i=0; i<widgetChain.Length(); ++i) {
                    nsIWidget* widget =  widgetChain[i];
                    MozQWidget* currWindow =
                        (MozQWidget*) widget->GetNativeData(NS_NATIVE_WINDOW);
                    if (is_mouse_in_window(currWindow, aMouseX, aMouseY)) {
                      if (i < sameTypeCount) {
                        rollup = PR_FALSE;
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
                gRollupListener->Rollup(popupsToRollup, nsnull);
                retVal = PR_TRUE;
            }
        }
    } else {
        gRollupWindow = nsnull;
        gRollupListener = nsnull;
        NS_IF_RELEASE(gMenuRollup);
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
        new gfxXlibSurface(aDrawable->x11Info().display(),
                           aDrawable->handle(),
                           (Visual*)aDrawable->x11Info().visual(),
                           gfxIntSize(aDrawable->size().width(),
                           aDrawable->size().height()));
    NS_IF_ADDREF(result);
    return result;
}
#endif

nsEventStatus
nsWindow::DoPaint(QPainter* aPainter, const QStyleOptionGraphicsItem* aOption, QWidget* aWidget)
{
    if (mIsDestroyed) {
        LOG(("Expose event on destroyed window [%p] window %p\n",
             (void *)this, mWidget));
        return nsEventStatus_eIgnore;
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

    if (GetLayerManager()->GetBackendType() == LayerManager::LAYERS_OPENGL) {
        nsPaintEvent event(PR_TRUE, NS_PAINT, this);
        event.refPoint.x = r.x();
        event.refPoint.y = r.y();
        event.region = nsIntRegion(rect);
        static_cast<mozilla::layers::LayerManagerOGL*>(GetLayerManager())->
            SetClippingRegion(event.region);
        return DispatchEvent(&event);
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
#ifdef MOZ_ENABLE_MEEGOTOUCH
    else if (renderMode == gfxQtPlatform::RENDER_DIRECT) {
        // This is needed for rotate transformation on Meego
        // This will work very slow if pixman does not handle rotation very well
        gfxMatrix matr;
        M::OrientationAngle angle = MApplication::activeWindow()->orientationAngle();
        matr.Translate(gfxPoint(aPainter->transform().dx(), aPainter->transform().dy()));
        matr.Rotate((M_PI/180)*angle);
        ctx->SetMatrix(matr);
        NS_ASSERTION(PIXMAN_VERSION < PIXMAN_VERSION_ENCODE(0, 21, 2) && angle, "Old pixman and rotate transform, it is going to be slow");
    }
#endif

    nsPaintEvent event(PR_TRUE, NS_PAINT, this);
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
        if (gBufferSurface->GetType() == gfxASurface::SurfaceTypeXlib) {
            // Paint offscreen pixmap to QPainter
            static QPixmap gBufferPixmap;
            Drawable draw = static_cast<gfxXlibSurface*>(gBufferSurface.get())->XDrawable();
            if (gBufferPixmap.handle() != draw)
                gBufferPixmap = QPixmap::fromX11Pixmap(draw, QPixmap::ExplicitlyShared);
            XSync(static_cast<gfxXlibSurface*>(gBufferSurface.get())->XDisplay(), False);
            aPainter->drawPixmap(QPoint(rect.x, rect.y), gBufferPixmap,
                                 QRect(0, 0, rect.width, rect.height));

        } else if (gBufferSurface->GetType() == gfxASurface::SurfaceTypeImage) {
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
        if (gBufferSurface->GetType() == gfxASurface::SurfaceTypeXlib) {
            nsRefPtr<gfxASurface> widgetSurface = GetSurfaceForQWidget(aWidget);
            nsRefPtr<gfxContext> ctx = new gfxContext(widgetSurface);
            ctx->SetSource(gBufferSurface);
            ctx->Rectangle(gfxRect(trans.x(), trans.y(), trans.width(), trans.height()), PR_TRUE);
            ctx->Clip();
            ctx->Fill();
        } else if (gBufferSurface->GetType() == gfxASurface::SurfaceTypeImage) {
#ifdef MOZ_HAVE_SHMIMAGE
            if (gShmImage) {
                gShmImage->Put(aWidget, trans);
            } else
#endif
            if (gBufferSurface) {
                nsRefPtr<gfxASurface> widgetSurface = GetSurfaceForQWidget(aWidget);
                nsRefPtr<gfxContext> ctx = new gfxContext(widgetSurface);
                ctx->SetSource(gBufferSurface);
                ctx->Rectangle(gfxRect(trans.x(), trans.y(), trans.width(), trans.height()), PR_TRUE);
                ctx->Clip();
                ctx->Fill();
            }
        }
    }

    ctx = nsnull;
    targetSurface = nsnull;

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

    nsGUIEvent event(PR_TRUE, NS_MOVE, this);

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
    nsGUIEvent event(PR_TRUE, NS_XUL_CLOSE, this);

    event.refPoint.x = 0;
    event.refPoint.y = 0;

    return DispatchEvent(&event);
}

nsEventStatus
nsWindow::OnEnterNotifyEvent(QGraphicsSceneHoverEvent *aEvent)
{
    nsMouseEvent event(PR_TRUE, NS_MOUSE_ENTER, this, nsMouseEvent::eReal);

    event.refPoint.x = nscoord(aEvent->pos().x());
    event.refPoint.y = nscoord(aEvent->pos().y());

    LOG(("OnEnterNotify: %p\n", (void *)this));

    return DispatchEvent(&event);
}

nsEventStatus
nsWindow::OnLeaveNotifyEvent(QGraphicsSceneHoverEvent *aEvent)
{
    nsMouseEvent event(PR_TRUE, NS_MOUSE_EXIT, this, nsMouseEvent::eReal);

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
nsWindow::OnMotionNotifyEvent(QGraphicsSceneMouseEvent *aEvent)
{
    UserActivity();

    CHECK_MOUSE_BLOCKED

    nsMouseEvent event(PR_TRUE, NS_MOUSE_MOVE, this, nsMouseEvent::eReal);

    event.refPoint.x = nscoord(aEvent->pos().x());
    event.refPoint.y = nscoord(aEvent->pos().y());

    event.isShift         = ((aEvent->modifiers() & Qt::ShiftModifier) != 0);
    event.isControl       = ((aEvent->modifiers() & Qt::ControlModifier) != 0);
    event.isAlt           = ((aEvent->modifiers() & Qt::AltModifier) != 0);
    event.isMeta          = ((aEvent->modifiers() & Qt::MetaModifier) != 0);
    event.clickCount      = 0;

    nsEventStatus status = DispatchEvent(&event);

    return status;
}

void
nsWindow::InitButtonEvent(nsMouseEvent &aMoveEvent,
                          QGraphicsSceneMouseEvent *aEvent, int aClickCount)
{
    aMoveEvent.refPoint.x = nscoord(aEvent->pos().x());
    aMoveEvent.refPoint.y = nscoord(aEvent->pos().y());

    aMoveEvent.isShift         = ((aEvent->modifiers() & Qt::ShiftModifier) != 0);
    aMoveEvent.isControl       = ((aEvent->modifiers() & Qt::ControlModifier) != 0);
    aMoveEvent.isAlt           = ((aEvent->modifiers() & Qt::AltModifier) != 0);
    aMoveEvent.isMeta          = ((aEvent->modifiers() & Qt::MetaModifier) != 0);
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

    PRBool rolledUp = check_for_rollup( pos.x(), pos.y(), PR_FALSE);
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

    nsMouseEvent event(PR_TRUE, NS_MOUSE_BUTTON_DOWN, this, nsMouseEvent::eReal);
    event.button = domButton;
    InitButtonEvent(event, aEvent, 1);

    LOG(("%s [%p] button: %d\n", __PRETTY_FUNCTION__, (void*)this, domButton));

    nsEventStatus status = DispatchEvent(&event);

    // right menu click on linux should also pop up a context menu
    if (domButton == nsMouseEvent::eRightButton &&
        NS_LIKELY(!mIsDestroyed)) {
        nsMouseEvent contextMenuEvent(PR_TRUE, NS_CONTEXTMENU, this,
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

    nsMouseEvent event(PR_TRUE, NS_MOUSE_BUTTON_UP, this, nsMouseEvent::eReal);
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

    nsMouseEvent event(PR_TRUE, NS_MOUSE_DOUBLECLICK, this, nsMouseEvent::eReal);
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

inline PRBool
is_latin_shortcut_key(quint32 aKeyval)
{
    return ((Qt::Key_0 <= aKeyval && aKeyval <= Qt::Key_9) ||
            (Qt::Key_A <= aKeyval && aKeyval <= Qt::Key_Z));
}

PRBool
nsWindow::DispatchCommandEvent(nsIAtom* aCommand)
{
    nsCommandEvent event(PR_TRUE, nsWidgetAtoms::onAppCommand, aCommand, this);

    DispatchEvent(&event);

    return PR_TRUE;
}

nsEventStatus
nsWindow::OnKeyPressEvent(QKeyEvent *aEvent)
{
    LOGFOCUS(("OnKeyPressEvent [%p]\n", (void *)this));

    // The user has done something.
    UserActivity();

    PRBool setNoDefault = PR_FALSE;

#ifdef MOZ_X11
    // before we dispatch a key, check if it's the context menu key.
    // If so, send a context menu key event instead.
    if (isContextMenuKeyEvent(aEvent)) {
        nsMouseEvent contextMenuEvent(PR_TRUE, NS_CONTEXTMENU, this,
                                      nsMouseEvent::eReal,
                                      nsMouseEvent::eContextMenuKey);
        //keyEventToContextMenuEvent(&event, &contextMenuEvent);
        return DispatchEvent(&contextMenuEvent);
    }

    PRUint32 domCharCode = 0;
    PRUint32 domKeyCode = QtKeyCodeToDOMKeyCode(aEvent->key());

    // get keymap and modifier map from the Xserver
    Display *display = QX11Info::display();
    int x_min_keycode = 0, x_max_keycode = 0, xkeysyms_per_keycode;
    XDisplayKeycodes(display, &x_min_keycode, &x_max_keycode);
    XModifierKeymap *xmodmap = XGetModifierMapping(display);
    KeySym *xkeymap = XGetKeyboardMapping(display, x_min_keycode, x_max_keycode - x_min_keycode,
                                          &xkeysyms_per_keycode);

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
    PRBool shift_state = ((shift_mask & aEvent->nativeModifiers()) != 0) ^
                          (bool)(shift_lock_mask & aEvent->nativeModifiers());
    PRBool capslock_state = (bool)(caps_lock_mask & aEvent->nativeModifiers());

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

        nsKeyEvent downEvent(PR_TRUE, NS_KEY_DOWN, this);
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
            setNoDefault = PR_TRUE;
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

    nsKeyEvent event(PR_TRUE, NS_KEY_PRESS, this);
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
        event.isControl = PR_FALSE;
        event.isAlt = PR_FALSE;
        event.isMeta = PR_FALSE;
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
        nsMouseEvent contextMenuEvent(PR_TRUE, NS_CONTEXTMENU, this,
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

        nsKeyEvent downEvent(PR_TRUE, NS_KEY_DOWN, this);
        InitKeyEvent(downEvent, aEvent);

        downEvent.keyCode = domKeyCode;

        nsEventStatus status = DispatchEvent(&downEvent);

        // If prevent default on keydown, do same for keypress
        if (status == nsEventStatus_eConsumeNoDefault)
            setNoDefault = PR_TRUE;
    }

    nsKeyEvent event(PR_TRUE, NS_KEY_PRESS, this);
    InitKeyEvent(event, aEvent);

    event.charCode = domCharCode;

    event.keyCode = domCharCode ? 0 : domKeyCode;

    if (setNoDefault)
        event.flags |= NS_EVENT_FLAG_NO_DEFAULT;

    // send the key press event
    return DispatchEvent(&event);
 }
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
        Display *display = QX11Info::display();
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
    nsKeyEvent event(PR_TRUE, NS_KEY_UP, this);
    InitKeyEvent(event, aEvent);

    event.keyCode = domKeyCode;

    // unset the key down flag
    ClearKeyDownFlag(event.keyCode);

    return DispatchEvent(&event);
}

nsEventStatus
nsWindow::OnScrollEvent(QGraphicsSceneWheelEvent *aEvent)
{
    // check to see if we should rollup
    nsMouseScrollEvent event(PR_TRUE, NS_MOUSE_SCROLL, this);

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

    event.isShift         = aEvent->modifiers() & Qt::ShiftModifier;
    event.isControl       = aEvent->modifiers() & Qt::ControlModifier;
    event.isAlt           = aEvent->modifiers() & Qt::AltModifier;
    event.isMeta          = aEvent->modifiers() & Qt::MetaModifier;
    event.time            = 0;

    return DispatchEvent(&event);
}


nsEventStatus
nsWindow::showEvent(QShowEvent *)
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__,(void *)this));
    mIsVisible = PR_TRUE;
    return nsEventStatus_eConsumeDoDefault;
}

nsEventStatus
nsWindow::hideEvent(QHideEvent *)
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__,(void *)this));
    mIsVisible = PR_FALSE;
    return nsEventStatus_eConsumeDoDefault;
}

//Gestures are only supported in 4.6.0 >
#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0))
nsEventStatus nsWindow::OnTouchEvent(QTouchEvent *event, PRBool &handled)
{
    handled = PR_FALSE;
    const QList<QTouchEvent::TouchPoint> &touchPoints = event->touchPoints();

    if (event->type() == QEvent::TouchBegin) {
        handled = PR_TRUE;
        for (int i = touchPoints.count() -1; i >= 0; i--) {
            QPointF fpos = touchPoints[i].pos();
            nsGestureNotifyEvent gestureNotifyEvent(PR_TRUE, NS_GESTURENOTIFY_EVENT_START, this);
            gestureNotifyEvent.refPoint = nsIntPoint(fpos.x(), fpos.y());
            DispatchEvent(&gestureNotifyEvent);
        }
    }
    else if (event->type() == QEvent::TouchEnd) {
        mGesturesCancelled = PR_FALSE;
    }

    if (touchPoints.count() == 2) {
        mTouchPointDistance = DistanceBetweenPoints(touchPoints.at(0).scenePos(),
                                                    touchPoints.at(1).scenePos());
        if (event->type() == QEvent::TouchBegin) {
            mLastPinchDistance = mTouchPointDistance;
        }
    }

    //Disable mouse events when gestures are used, because they cause problems with
    //Fennec
    if (touchPoints.count() > 1) {
        mLastMultiTouchTime.start();
    }

    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::OnGestureEvent(QGestureEvent* event, PRBool &handled) {

    handled = PR_FALSE;
    if (mGesturesCancelled) {
        return nsEventStatus_eIgnore;
    }

    nsEventStatus result = nsEventStatus_eIgnore;

    QGesture* gesture = event->gesture(Qt::PinchGesture);

    if (gesture) {
        QPinchGesture* pinch = static_cast<QPinchGesture*>(gesture);
        handled = PR_TRUE;

        QPointF mappedCenterPoint =
            mWidget->mapFromScene(event->mapToGraphicsScene(pinch->centerPoint()));
        nsIntPoint centerPoint(mappedCenterPoint.x(), mappedCenterPoint.y());

        if (pinch->state() == Qt::GestureStarted) {
            event->accept();
            mPinchStartDistance = mTouchPointDistance;
            result = DispatchGestureEvent(NS_SIMPLE_GESTURE_MAGNIFY_START,
                                          0, 0, centerPoint);
        }
        else if (pinch->state() == Qt::GestureUpdated) {
            double delta = mTouchPointDistance - mLastPinchDistance;

            result = DispatchGestureEvent(NS_SIMPLE_GESTURE_MAGNIFY_UPDATE,
                                          0, delta, centerPoint);
        }
        else if (pinch->state() == Qt::GestureFinished) {
            double delta =mTouchPointDistance - mPinchStartDistance;
            result = DispatchGestureEvent(NS_SIMPLE_GESTURE_MAGNIFY,
                                          0, delta, centerPoint);
        }
        else {
            handled = false;
        }

        mLastPinchDistance = mTouchPointDistance;
    }

    gesture = event->gesture(gSwipeGestureId);
    if (gesture) {
        if (gesture->state() == Qt::GestureStarted) {
            event->accept();
        }
        if (gesture->state() == Qt::GestureFinished) {
            event->accept();
            handled = PR_TRUE;

            MozSwipeGesture* swipe = static_cast<MozSwipeGesture*>(gesture);
            nsIntPoint hotspot;
            hotspot.x = swipe->hotSpot().x();
            hotspot.y = swipe->hotSpot().y();

            // Cancel pinch gesture
            mGesturesCancelled = PR_TRUE;
            PRFloat64 delta = mTouchPointDistance - mPinchStartDistance;
            DispatchGestureEvent(NS_SIMPLE_GESTURE_MAGNIFY, 0, delta/2, hotspot);

            result = DispatchGestureEvent(NS_SIMPLE_GESTURE_SWIPE,
                                          swipe->Direction(), 0, hotspot);
        }
    }

    return result;
}

nsEventStatus
nsWindow::DispatchGestureEvent(PRUint32 aMsg, PRUint32 aDirection,
                               double aDelta, const nsIntPoint& aRefPoint)
{
    nsSimpleGestureEvent mozGesture(PR_TRUE, aMsg, this, 0, 0.0);
    mozGesture.direction = aDirection;
    mozGesture.delta = aDelta;
    mozGesture.refPoint = aRefPoint;

    Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();

    mozGesture.isShift   = (modifiers & Qt::ShiftModifier) ? PR_TRUE : PR_FALSE;
    mozGesture.isControl = (modifiers & Qt::ControlModifier) ? PR_TRUE : PR_FALSE;
    mozGesture.isMeta    = PR_FALSE;
    mozGesture.isAlt     = (modifiers & Qt::AltModifier) ? PR_TRUE : PR_FALSE;
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
    nsGUIEvent event(PR_TRUE, NS_THEMECHANGED, this);

    DispatchEvent(&event);

    // do nothing
    return;
}

nsEventStatus
nsWindow::OnDragMotionEvent(QGraphicsSceneDragDropEvent *aEvent)
{
    LOG(("nsWindow::OnDragMotionSignal\n"));

    nsMouseEvent event(PR_TRUE, NS_DRAGDROP_OVER, 0,
                       nsMouseEvent::eReal);
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::OnDragLeaveEvent(QGraphicsSceneDragDropEvent *aEvent)
{
    // XXX Do we want to pass this on only if the event's subwindow is null?
    LOG(("nsWindow::OnDragLeaveSignal(%p)\n", this));
    nsMouseEvent event(PR_TRUE, NS_DRAGDROP_EXIT, this, nsMouseEvent::eReal);

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
    nsMouseEvent event(PR_TRUE, NS_DRAGDROP_OVER, 0,
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

    nsMouseEvent event(PR_TRUE, NS_DRAGDROP_ENTER, this, nsMouseEvent::eReal);
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
                 nsIDeviceContext *aContext,
                 nsIAppShell      *aAppShell,
                 nsIToolkit       *aToolkit,
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
    BaseCreate(baseParent, aRect, aHandleEventFunction, aContext,
               aAppShell, aToolkit, aInitData);

    // and do our common creation
    mParent = aParent;

    // save our bounds
    mBounds = aRect;

    // find native parent
    MozQWidget *parent = nsnull;

    if (aParent != nsnull)
        parent = static_cast<MozQWidget*>(aParent->GetNativeData(NS_NATIVE_WIDGET));
    else
        parent = static_cast<MozQWidget*>(aNativeParent);

    // ok, create our QGraphicsWidget
    mWidget = createQWidget(parent, aInitData);

    if (!mWidget)
        return NS_ERROR_OUT_OF_MEMORY;

    LOG(("Create: nsWindow [%p] [%p]\n", (void *)this, (void *)mWidget));

    // resize so that everything is set to the right dimensions
    Resize(mBounds.x, mBounds.y, mBounds.width, mBounds.height, PR_FALSE);

    // check if we should listen for resizes
    mListenForResizes = (aNativeParent ||
                         (aInitData && aInitData->mListenForResizes));

    return NS_OK;
}

already_AddRefed<nsIWidget>
nsWindow::CreateChild(const nsIntRect&  aRect,
                      EVENT_CALLBACK    aHandleEventFunction,
                      nsIDeviceContext* aContext,
                      nsIAppShell*      aAppShell,
                      nsIToolkit*       aToolkit,
                      nsWidgetInitData* aInitData,
                      PRBool            /*aForceUseIWidgetParent*/)
{
    //We need to force parent widget, otherwise GetTopLevelWindow doesn't work
    return nsBaseWidget::CreateChild(aRect,
                                     aHandleEventFunction,
                                     aContext,
                                     aAppShell,
                                     aToolkit,
                                     aInitData,
                                     PR_TRUE); // Force parent
}


NS_IMETHODIMP
nsWindow::SetWindowClass(const nsAString &xulWinType)
{
    if (!mWidget)
      return NS_ERROR_FAILURE;

    nsXPIDLString brandName;
    GetBrandName(brandName);

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
    if (widget && widget->handle())
        XSetClassHint(widget->x11Info().display(),
                      widget->handle(),
                      class_hint);

    nsMemory::Free(class_hint->res_class);
    nsMemory::Free(class_hint->res_name);
    XFree(class_hint);

    return NS_OK;
}

void
nsWindow::NativeResize(PRInt32 aWidth, PRInt32 aHeight, PRBool  aRepaint)
{
    LOG(("nsWindow::NativeResize [%p] %d %d\n", (void *)this,
         aWidth, aHeight));

    mNeedsResize = PR_FALSE;

#ifndef MOZ_ENABLE_MEEGOTOUCH
    if (mIsTopLevel && XRE_GetProcessType() == GeckoProcessType_Default) {
        QWidget *widget = GetViewWidget();
        NS_ENSURE_TRUE(widget,);
        widget->resize(aWidth, aHeight);
    }
#endif

    mWidget->resize( aWidth, aHeight);

    if (aRepaint)
        mWidget->update();
}

void
nsWindow::NativeResize(PRInt32 aX, PRInt32 aY,
                       PRInt32 aWidth, PRInt32 aHeight,
                       PRBool  aRepaint)
{
    LOG(("nsWindow::NativeResize [%p] %d %d %d %d\n", (void *)this,
         aX, aY, aWidth, aHeight));

    mNeedsResize = PR_FALSE;
    mNeedsMove = PR_FALSE;

#ifndef MOZ_ENABLE_MEEGOTOUCH
    if (mIsTopLevel) {
        if (XRE_GetProcessType() == GeckoProcessType_Default) {
            QWidget *widget = GetViewWidget();
            NS_ENSURE_TRUE(widget,);
            widget->setGeometry(aX, aY, aWidth, aHeight);
        }
    }
#endif

    mWidget->setGeometry(aX, aY, aWidth, aHeight);

    if (aRepaint)
        mWidget->update();
}

void
nsWindow::NativeShow(PRBool aAction)
{
    if (aAction) {
        QWidget *widget = GetViewWidget();
        // On e10s, we never want the child process or plugin process
        // to go fullscreen because if we do the window because visible
        // do to disabled Qt-Xembed
        if ((XRE_GetProcessType() == GeckoProcessType_Default) &&
            widget && !widget->isVisible())
            MakeFullScreen(mSizeMode == nsSizeMode_Fullscreen);
        mWidget->show();

        // unset our flag now that our window has been shown
        mNeedsShow = PR_FALSE;
    }
    else
        mWidget->hide();
}

NS_IMETHODIMP
nsWindow::SetHasTransparentBackground(PRBool aTransparent)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::GetHasTransparentBackground(PRBool& aTransparent)
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
nsWindow::MakeFullScreen(PRBool aFullScreen)
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
nsWindow::HideWindowChrome(PRBool aShouldHide)
{
    if (!mWidget) {
        // Nothing to hide
        return NS_ERROR_FAILURE;
    }

    // Sawfish, metacity, and presumably other window managers get
    // confused if we change the window decorations while the window
    // is visible.
    PRBool wasVisible = PR_FALSE;
    if (mWidget->isVisible()) {
        NativeShow(PR_FALSE);
        wasVisible = PR_TRUE;
    }

    if (wasVisible) {
        NativeShow(PR_TRUE);
    }

    // For some window managers, adding or removing window decorations
    // requires unmapping and remapping our toplevel window.  Go ahead
    // and flush the queue here so that we don't end up with a BadWindow
    // error later when this happens (when the persistence timer fires
    // and GetWindowPos is called)
    QWidget *widget = GetViewWidget();
    NS_ENSURE_TRUE(widget, NS_ERROR_FAILURE);
    XSync(widget->x11Info().display(), False);

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
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefs)
        return NS_OK;

    PRBool val = PR_TRUE;
    nsresult rv;
    rv = prefs->GetBoolPref("mozilla.widget.disable-native-theme", &val);
    if (NS_SUCCEEDED(rv))
        gDisableNativeTheme = val;

    return NS_OK;
}

inline PRBool
is_context_menu_key(const nsKeyEvent& aKeyEvent)
{
    return ((aKeyEvent.keyCode == NS_VK_F10 && aKeyEvent.isShift &&
             !aKeyEvent.isControl && !aKeyEvent.isMeta && !aKeyEvent.isAlt) ||
            (aKeyEvent.keyCode == NS_VK_CONTEXT_MENU && !aKeyEvent.isShift &&
             !aKeyEvent.isControl && !aKeyEvent.isMeta && !aKeyEvent.isAlt));
}

void
key_event_to_context_menu_event(nsMouseEvent &aEvent,
                                QKeyEvent *aGdkEvent)
{
    aEvent.refPoint = nsIntPoint(0, 0);
    aEvent.isShift = PR_FALSE;
    aEvent.isControl = PR_FALSE;
    aEvent.isAlt = PR_FALSE;
    aEvent.isMeta = PR_FALSE;
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

MozQWidget*
nsWindow::createQWidget(MozQWidget *parent, nsWidgetInitData *aInitData)
{
    const char *windowName = NULL;

#ifdef DEBUG_WIDGETS
    qDebug("NEW WIDGET\n\tparent is %p (%s)", (void*)parent,
           parent ? qPrintable(parent->objectName()) : "null");
#endif

    // ok, create our windows
    switch (mWindowType) {
    case eWindowType_dialog:
        windowName = "topLevelDialog";
        if (!parent)
            mIsTopLevel = PR_TRUE;
        break;
    case eWindowType_popup:
        windowName = "topLevelPopup";
        break;
    case eWindowType_toplevel:
        windowName = "topLevelWindow";
        mIsTopLevel = PR_TRUE;
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

    MozQWidget * widget = new MozQWidget(this, parent);
    if (!widget)
        return nsnull;

    // make only child and plugin windows focusable
    if (eWindowType_child == mWindowType || eWindowType_plugin == mWindowType) {
        widget->setFlag(QGraphicsItem::ItemIsFocusable);
        widget->setFocusPolicy(Qt::WheelFocus);
    }

    // create a QGraphicsView if this is a new toplevel window

    if (mIsTopLevel) {
        QGraphicsView* newView = nsnull;
#ifdef MOZ_ENABLE_MEEGOTOUCH
        if (XRE_GetProcessType() == GeckoProcessType_Default) {
            newView = new MozMGraphicsView(widget);
        } else
#else
        newView = new MozQGraphicsView(widget);
#endif
        if (!newView) {
            delete widget;
            return nsnull;
        }
        if (!IsAcceleratedQView(newView) && GetShouldAccelerate()) {
            newView->setViewport(new QGLWidget());
        }

        if (gfxQtPlatform::GetPlatform()->GetRenderMode() == gfxQtPlatform::RENDER_DIRECT) {
            // Disable double buffer and system background rendering
            newView->viewport()->setAttribute(Qt::WA_PaintOnScreen, true);
            newView->viewport()->setAttribute(Qt::WA_NoSystemBackground, true);
        }
        // Enable gestures:
#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0))
        newView->viewport()->grabGesture(Qt::PinchGesture);
        newView->viewport()->grabGesture(gSwipeGestureId);
#endif
        newView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        newView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

#if (QT_VERSION >= QT_VERSION_CHECK(4, 6, 0))
        // Top level widget is just container, and should not be painted
        widget->setFlag(QGraphicsItem::ItemHasNoContents);
#endif
    } else if (eWindowType_dialog == mWindowType && parent)
        parent->scene()->addItem(widget);

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
    widget->grabGesture(Qt::PinchGesture);
    widget->grabGesture(gSwipeGestureId);
#endif

    return widget;
}

PRBool
nsWindow::IsAcceleratedQView(QGraphicsView *view)
{
    if (view && view->viewport()) {
        QPaintEngine::Type type = view->viewport()->paintEngine()->type();
        return (type == QPaintEngine::OpenGL || type == QPaintEngine::OpenGL2);
    }
    return PR_FALSE;
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
nsWindow::imComposeEvent(QInputMethodEvent *event, PRBool &handled)
{
    nsCompositionEvent start(PR_TRUE, NS_COMPOSITION_START, this);
    DispatchEvent(&start);

    nsTextEvent text(PR_TRUE, NS_TEXT_TEXT, this);
    text.theText.Assign(event->commitString().utf16());
    DispatchEvent(&text);

    nsCompositionEvent end(PR_TRUE, NS_COMPOSITION_END, this);
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
    nsGUIEvent event(PR_TRUE, NS_ACTIVATE, this);
    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsWindow::DispatchDeactivateEvent(void)
{
    nsGUIEvent event(PR_TRUE, NS_DEACTIVATE, this);
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
    nsSizeEvent event(PR_TRUE, NS_SIZE, this);

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
nsWindow::Show(PRBool aState)
{
    LOG(("nsWindow::Show [%p] state %d\n", (void *)this, aState));

    mIsShown = aState;

    if ((aState && !AreBoundsSane()) || !mWidget) {
        LOG(("\tbounds are insane or window hasn't been created yet\n"));
        mNeedsShow = PR_TRUE;
        return NS_OK;
    }

    if (aState) {
        if (mNeedsMove) {
            NativeResize(mBounds.x, mBounds.y, mBounds.width, mBounds.height,
                         PR_FALSE);
        } else if (mNeedsResize) {
            NativeResize(mBounds.width, mBounds.height, PR_FALSE);
        }
    }
    else
        // If someone is hiding this widget, clear any needing show flag.
        mNeedsShow = PR_FALSE;

    NativeShow(aState);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
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
                NativeShow(PR_TRUE);
        }
        else {
            // If someone has set this so that the needs show flag is false
            // and it needs to be hidden, update the flag and hide the
            // window.  This flag will be cleared the next time someone
            // hides the window or shows it.  It also prevents us from
            // calling NativeShow(PR_FALSE) excessively on the window which
            // causes unneeded X traffic.
            if (!mNeedsShow) {
                mNeedsShow = PR_TRUE;
                NativeShow(PR_FALSE);
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
        mNeedsResize = PR_TRUE;
    }

    // synthesize a resize event if this isn't a toplevel
    if (mIsTopLevel || mListenForResizes) {
        nsIntRect rect(mBounds.x, mBounds.y, aWidth, aHeight);
        nsEventStatus status;
        DispatchResizeEvent(rect, status);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight,
                 PRBool aRepaint)
{
    mBounds.x = aX;
    mBounds.y = aY;
    mBounds.width = aWidth;
    mBounds.height = aHeight;

    mPlaced = PR_TRUE;

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
                NativeShow(PR_TRUE);
        }
        else {
            // If someone has set this so that the needs show flag is false
            // and it needs to be hidden, update the flag and hide the
            // window.  This flag will be cleared the next time someone
            // hides the window or shows it.  It also prevents us from
            // calling NativeShow(PR_FALSE) excessively on the window which
            // causes unneeded X traffic.
            if (!mNeedsShow) {
                mNeedsShow = PR_TRUE;
                NativeShow(PR_FALSE);
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
        mNeedsResize = PR_TRUE;
        mNeedsMove = PR_TRUE;
    }

    if (mIsTopLevel || mListenForResizes) {
        // synthesize a resize event
        nsIntRect rect(aX, aY, aWidth, aHeight);
        nsEventStatus status;
        DispatchResizeEvent(rect, status);
    }

    if (aRepaint)
        mWidget->update();

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Enable(PRBool aState)
{
    mEnabled = aState;

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::IsEnabled(PRBool *aState)
{
    *aState = mEnabled;

    return NS_OK;
}

void
nsWindow::OnDestroy(void)
{
    if (mOnDestroyCalled)
        return;

    mOnDestroyCalled = PR_TRUE;

    // release references to children, device context, toolkit + app shell
    nsBaseWidget::OnDestroy();

    // let go of our parent
    mParent = nsnull;

    nsCOMPtr<nsIWidget> kungFuDeathGrip = this;

    nsGUIEvent event(PR_TRUE, NS_DESTROY, this);
    nsEventStatus status;
    DispatchEvent(&event, status);
}

PRBool
nsWindow::AreBoundsSane(void)
{
    if (mBounds.width > 0 && mBounds.height > 0)
        return PR_TRUE;

    return PR_FALSE;
}

NS_IMETHODIMP
nsWindow::SetIMEEnabled(PRUint32 aState)
{
    NS_ENSURE_TRUE(mWidget, NS_ERROR_FAILURE);

    switch (aState) {
        case nsIWidget::IME_STATUS_ENABLED:
        case nsIWidget::IME_STATUS_PASSWORD:
            {
                PRInt32 openDelay = 200;
                nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
                if (prefs)
                  prefs->GetIntPref("ui.vkb.open.delay", &openDelay);

                mWidget->requestVKB(openDelay);
            }
            break;
        default:
            mWidget->hideVKB();
            break;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetIMEEnabled(PRUint32* aState)
{
    NS_ENSURE_ARG_POINTER(aState);
    NS_ENSURE_TRUE(mWidget, NS_ERROR_FAILURE);

    *aState = mWidget->isVKBOpen() ? IME_STATUS_ENABLED : IME_STATUS_DISABLED;
    return NS_OK;
}

void
nsWindow::UserActivity()
{
  if (!mIdleService) {
    mIdleService = do_GetService("@mozilla.org/widget/idleservice;1");
  }

  if (mIdleService) {
    mIdleService->ResetIdleTimeOut();
  }
}

