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
 *   Mats Palmgren <mats.palmgren@bredband.net>
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

#include "prlink.h"

#include "nsWindow.h"
#include "mozqwidget.h"

#include "nsToolkit.h"
#include "nsIDeviceContext.h"
#include "nsIRenderingContext.h"
#include "nsIRegion.h"
#include "nsIRollupListener.h"
#include "nsIMenuRollup.h"
#include "nsWidgetsCID.h"
#include "nsQtKeyUtils.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIStringBundle.h"
#include "nsGfxCIID.h"

#include "imgIContainer.h"
#include "nsGfxCIID.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsAutoPtr.h"

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

#include <QtCore/QDebug>
#include <QtCore/QEvent>
#include <QtCore/QVariant>

#include "gfxQtPlatform.h"
#include "gfxXlibSurface.h"
#include "gfxQPainterSurface.h"
#include "gfxContext.h"
#include "gfxSharedImageSurface.h"

// Buffered Pixmap stuff
static QPixmap *gBufferPixmap = nsnull;
static int gBufferPixmapUsageCount = 0;

// Buffered shared image + pixmap
static gfxSharedImageSurface *gBufferImage = nsnull;
static gfxSharedImageSurface *gBufferImageTemp = nsnull;
PRBool gNeedColorConversion = PR_FALSE;
extern "C" {
#include "pixman.h"
}

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
    mPluginType          = PluginType_NONE;
    mQCursor             = Qt::ArrowCursor;
    mScene               = nsnull;
    
    if (!gGlobalsInitialized) {
        gGlobalsInitialized = PR_TRUE;

        // It's OK if either of these fail, but it may not be one day.
        initialize_prefs();
    }

    memset(mKeyDownFlags, 0, sizeof(mKeyDownFlags));

    mIsTransparent = PR_FALSE;

    mCursor = eCursor_standard;

    gBufferPixmapUsageCount++;
}

static inline gfxASurface::gfxImageFormat
_depth_to_gfximage_format(PRInt32 aDepth)
{
    switch (aDepth) {
    case 32:
        return gfxASurface::ImageFormatARGB32;
    case 24:
        return gfxASurface::ImageFormatRGB24;
    default:
        return gfxASurface::ImageFormatUnknown;
    }
}

static void
FreeOffScreenBuffers(void)
{
    delete gBufferImage;
    delete gBufferImageTemp;
    delete gBufferPixmap;
    gBufferImage = nsnull;
    gBufferImageTemp = nsnull;
    gBufferPixmap = nsnull;
}

static bool
UpdateOffScreenBuffers(QSize aSize, int aDepth)
{
    gfxIntSize size(aSize.width(), aSize.height());
    if (gBufferPixmap) {
        if (gBufferPixmap->size().width() < size.width ||
            gBufferPixmap->size().height() < size.height) {
            FreeOffScreenBuffers();
        } else
            return true;
    }

    gBufferPixmap = new QPixmap(size.width, size.height);
    if (!gBufferPixmap)
        return false;

    if (gfxQtPlatform::GetPlatform()->GetRenderMode() == gfxQtPlatform::RENDER_XLIB)
        return true;

    // Check if system depth has related gfxImage format
    gfxASurface::gfxImageFormat format =
        _depth_to_gfximage_format(gBufferPixmap->x11Info().depth());

    gNeedColorConversion = (format == gfxASurface::ImageFormatUnknown);

    gBufferImage = new gfxSharedImageSurface();
    if (!gBufferImage) {
        FreeOffScreenBuffers();
        return false;
    }

    if (!gBufferImage->Init(gfxIntSize(gBufferPixmap->size().width(),
                            gBufferPixmap->size().height()),
                            _depth_to_gfximage_format(gBufferPixmap->x11Info().depth()))) {
        FreeOffScreenBuffers();
        return false;
    }

    // gfxImageSurface does not support system color depth format
    // we have to paint it with temp surface and color conversion
    if (!gNeedColorConversion)
        return true;

    gBufferImageTemp = new gfxSharedImageSurface();
    if (!gBufferImageTemp) {
        FreeOffScreenBuffers();
        return false;
    }

    if (!gBufferImageTemp->Init(gfxIntSize(gBufferPixmap->size().width(),
                                gBufferPixmap->size().height()),
                                gfxASurface::ImageFormatRGB24)) {
        FreeOffScreenBuffers();
        return false;
    }
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

        FreeOffScreenBuffers();
    }

    nsCOMPtr<nsIWidget> rollupWidget = do_QueryReferent(gRollupWindow);
    if (static_cast<nsIWidget *>(this) == rollupWidget.get()) {
        if (gRollupListener)
            gRollupListener->Rollup(nsnull, nsnull);
        gRollupWindow = nsnull;
        gRollupListener = nsnull;
        NS_IF_RELEASE(gMenuRollup);
    }

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

    if (mWidget) {
        mWidget->dropReceiver();

        // Call deleteLater instead of delete; Qt still needs the object
        // to be valid even after sending it a Close event.  We could
        // also set WA_DeleteOnClose, but this gives us more control.
        mWidget->deleteLater();
    }
    mWidget = nsnull;

    OnDestroy();

    // tear down some infrastructure after all event handling is finished
    if (mScene) {
        QWidget* view = GetViewWidget();

        delete mScene;
        mScene = nsnull;

        delete view;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetParent(nsIWidget *aNewParent)
{
    NS_ENSURE_ARG_POINTER(aNewParent);
    if (aNewParent) {
        nsCOMPtr<nsIWidget> kungFuDeathGrip(this);

        nsIWidget* parent = GetParent();
        if (parent) {
            parent->RemoveChild(this);
        }

        MozQWidget* newParent = static_cast<MozQWidget*>(aNewParent->GetNativeData(NS_NATIVE_WINDOW));
        NS_ASSERTION(newParent, "Parent widget has a null native window handle");
        if (mWidget) {
            mWidget->setParentItem(newParent);
        }

        aNewParent->AddChild(this);

        return NS_OK;
    }

    nsCOMPtr<nsIWidget> kungFuDeathGrip(this);

    nsIWidget* parent = GetParent();

    if (parent)
        parent->RemoveChild(this);

    if (mWidget)
        mWidget->setParentItem(0);

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
    aState = mWidget ? mWidget->isVisible() : PR_FALSE;
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

    if (mWindowType == eWindowType_toplevel ||
        mWindowType == eWindowType_dialog) {
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

    switch (aMode) {
    case nsSizeMode_Maximized:
        GetViewWidget()->showMaximized();
        break;
    case nsSizeMode_Minimized:
        GetViewWidget()->showMinimized();
        break;
    case nsSizeMode_Fullscreen:
        GetViewWidget()->showFullScreen();
        break;

    default:
        // nsSizeMode_Normal, really.
        GetViewWidget()->showNormal();
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
    if (!aItem)
        return;

    if (!aVisibleItem && aItem->isVisible())
        aVisibleItem = aItem;
    else if (aVisibleItem && !aItem->isVisible())
        aVisibleItem = nsnull;

    // check further up the chain
    find_first_visible_parent(aItem->parentItem(), aVisibleItem);
}

NS_IMETHODIMP
nsWindow::SetFocus(PRBool aRaise)
{
    // Make sure that our owning widget has focus.  If it doesn't try to
    // grab it.  Note that we don't set our focus flag in this case.
    LOGFOCUS(("  SetFocus [%p]\n", (void *)this));

    if (!mWidget)
        return NS_ERROR_FAILURE;

    // Because QGraphicsItem cannot get the focus if they are 
    // invisible, we look up the chain, for the lowest visible
    // parent and focus that one
    QGraphicsItem* realFocusItem = nsnull;
    find_first_visible_parent(mWidget, realFocusItem);

    if (!realFocusItem || realFocusItem->hasFocus())
        return NS_OK;

    if (aRaise)
        realFocusItem->setFocus(Qt::ActiveWindowFocusReason);
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

    mWidget->update(aRect.x, aRect.y, aRect.width, aRect.height);

    // QGraphicsItems cannot trigger a repaint themselves, so we start it on the view
    if (aIsSynchronous && GetViewWidget())
        GetViewWidget()->repaint();

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Update()
{
    if (!mWidget)
        return NS_OK;

    mWidget->update(); // FIXME  This call cause update for whole window on each scroll event
    return NS_OK;
}

void
nsWindow::Scroll(const nsIntPoint& aDelta,
                 const nsTArray<nsIntRect>& aDestRects,
                 const nsTArray<nsIWidget::Configuration>& aConfigurations)
{
    if (!mWidget) {
        NS_ERROR("No widget to scroll.");
        return;
    }

    nsAutoTArray<nsWindow*,1> windowsToShow;
    // Hide any widgets that are becoming invisible or that are moving.
    // Moving widgets are hidden for the duration of the scroll so that
    // the XCopyArea treats their drawn pixels as part of the window
    // that should be scrolled. This works well when the widgets are
    // moving because they're being scrolled, which is normally true.
    for (PRUint32 i = 0; i < aConfigurations.Length(); ++i) {
        const Configuration& configuration = aConfigurations[i];
        nsWindow* w = static_cast<nsWindow*>(configuration.mChild);
        NS_ASSERTION(w->GetParent() == this,
                     "Configuration widget is not a child");
        if (w->mIsShown &&
            (configuration.mClipRegion.IsEmpty() ||
             configuration.mBounds != w->mBounds)) {
            w->NativeShow(PR_FALSE);
            windowsToShow.AppendElement(w);
        }
    }

    for (BlitRectIter iter(aDelta, aDestRects); !iter.IsDone(); ++iter) {
        const nsIntRect & r = iter.Rect();
        QRect rect(r.x - aDelta.x, r.y - aDelta.y, r.width, r.height);
        mWidget->scroll(aDelta.x, aDelta.y, rect);
    }
    ConfigureChildren(aConfigurations);

    // Show windows again...
    for (PRUint32 i = 0; i < windowsToShow.Length(); ++i) {
        windowsToShow[i]->NativeShow(PR_TRUE);
    }
}

QWidget* nsWindow::GetViewWidget()
{
    QWidget* viewWidget = nsnull;

    if (!mScene) {
        if (mParent)
            return static_cast<nsWindow*>(mParent.get())->GetViewWidget();

        return nsnull;
    }

    NS_ASSERTION(mScene->views().size() == 1, "Not exactly one view for our scene!");
    viewWidget = mScene->views()[0];

    return viewWidget;
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

#ifdef Q_WS_X11
    case NS_NATIVE_DISPLAY:
        return GetViewWidget()->x11Info().display();
        break;
#endif

    case NS_NATIVE_GRAPHIC: {
        NS_ASSERTION(nsnull != mToolkit, "NULL toolkit, unable to get a GC");
        return (void *)static_cast<nsToolkit *>(mToolkit)->GetSharedGC();
        break;
    }

    case NS_NATIVE_SHELLWIDGET:
        return (void *) GetViewWidget();

    default:
        NS_WARNING("nsWindow::GetNativeData called with bad value");
        return nsnull;
    }
}

NS_IMETHODIMP
nsWindow::SetTitle(const nsAString& aTitle)
{
    QString qStr(QString::fromUtf16(aTitle.BeginReading(), aTitle.Length()));
    if (mIsTopLevel)
        GetViewWidget()->setWindowTitle(qStr);
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
    if (aCapture)
        GetViewWidget()->grabMouse();
    else
        GetViewWidget()->releaseMouse();

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
GetSurfaceForQWidget(QPixmap* aDrawable)
{
    gfxASurface* result =
        new gfxXlibSurface(aDrawable->x11Info().display(),
                           aDrawable->handle(),
                           (Visual*)aDrawable->x11Info().visual(),
                           gfxIntSize(aDrawable->size().width(), aDrawable->size().height()));
    NS_IF_ADDREF(result);
    return result;
}
#endif

nsEventStatus
nsWindow::DoPaint(QPainter* aPainter, const QStyleOptionGraphicsItem* aOption)
{
    if (mIsDestroyed) {
        LOG(("Expose event on destroyed window [%p] window %p\n",
             (void *)this, mWidget));
        return nsEventStatus_eIgnore;
    }

    if (!mWidget)
        return nsEventStatus_eIgnore;

    static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);

    nsCOMPtr<nsIRegion> updateRegion = do_CreateInstance(kRegionCID);
    if (!updateRegion)
        return nsEventStatus_eIgnore;

    QRectF r;

    if (aOption)
        r = aOption->exposedRect;
    else
        r = mWidget->boundingRect();

    updateRegion->SetTo( (int)r.x(), (int)r.y(), (int)r.width(), (int)r.height() );

    gfxQtPlatform::RenderMode renderMode = gfxQtPlatform::GetPlatform()->GetRenderMode();
    // Prepare offscreen buffers if RenderMode Xlib or Image
    if (renderMode != gfxQtPlatform::RENDER_QPAINTER)
        if (!UpdateOffScreenBuffers(r.size().toSize(), QX11Info().depth()))
            return nsEventStatus_eIgnore;

    nsRefPtr<gfxASurface> targetSurface = nsnull;
    if (renderMode == gfxQtPlatform::RENDER_XLIB) {
        targetSurface = GetSurfaceForQWidget(gBufferPixmap);
    } else if (renderMode == gfxQtPlatform::RENDER_SHARED_IMAGE) {
        targetSurface = gNeedColorConversion ? gBufferImageTemp->getASurface()
                                             : gBufferImage->getASurface();
    } else if (renderMode == gfxQtPlatform::RENDER_QPAINTER) {
        targetSurface = new gfxQPainterSurface(aPainter);
    }

    if (NS_UNLIKELY(!targetSurface))
        return nsEventStatus_eIgnore;

    nsRefPtr<gfxContext> ctx = new gfxContext(targetSurface);

    nsCOMPtr<nsIRenderingContext> rc;
    GetDeviceContext()->CreateRenderingContextInstance(*getter_AddRefs(rc));
    if (NS_UNLIKELY(!rc))
        return nsEventStatus_eIgnore;

    rc->Init(GetDeviceContext(), ctx);

    nsPaintEvent event(PR_TRUE, NS_PAINT, this);

    nsIntRect rect(r.x(), r.y(), r.width(), r.height());
    event.refPoint.x = r.x();
    event.refPoint.y = r.y();
    event.rect = nsnull;
    event.region = updateRegion;
    event.renderingContext = rc;

    nsEventStatus status = DispatchEvent(&event);

    // DispatchEvent can Destroy us (bug 378273), avoid doing any paint
    // operations below if that happened - it will lead to XError and exit().
    if (NS_UNLIKELY(mIsDestroyed))
        return status;

    if (status == nsEventStatus_eIgnore)
        return status;

    LOGDRAW(("[%p] draw done\n", this));

    if (renderMode == gfxQtPlatform::RENDER_SHARED_IMAGE) {
        if (gNeedColorConversion) {
            pixman_image_t *src_image = NULL;
            pixman_image_t *dst_image = NULL;
            src_image = pixman_image_create_bits(PIXMAN_x8r8g8b8,
                                                 gBufferImageTemp->GetSize().width,
                                                 gBufferImageTemp->GetSize().height,
                                                 (uint32_t*)gBufferImageTemp->Data(),
                                                 gBufferImageTemp->Stride());
            dst_image = pixman_image_create_bits(PIXMAN_r5g6b5,
                                                 gBufferImage->GetSize().width,
                                                 gBufferImage->GetSize().height,
                                                 (uint32_t*)gBufferImage->Data(),
                                                 gBufferImage->Stride());
            pixman_image_composite(PIXMAN_OP_SRC,
                                   src_image,
                                   NULL,
                                   dst_image,
                                   0, 0,
                                   0, 0,
                                   0, 0,
                                   gBufferImageTemp->GetSize().width,
                                   gBufferImageTemp->GetSize().height);
            pixman_image_unref(src_image);
            pixman_image_unref(dst_image);
        }

        Display *disp = gBufferPixmap->x11Info().display();
        XGCValues gcv;
        gcv.graphics_exposures = False;
        GC gc = XCreateGC (disp, gBufferPixmap->handle(), GCGraphicsExposures, &gcv);
        XShmPutImage(disp, gBufferPixmap->handle(), gc, gBufferImage->image(),
                     0, 0, 0, 0, gBufferImage->GetSize().width, gBufferImage->GetSize().height,
                     False);
        XFreeGC (disp, gc);
    }

    if (renderMode != gfxQtPlatform::RENDER_QPAINTER)
        aPainter->drawPixmap(QPoint(0, 0), *gBufferPixmap);

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

nsEventStatus
nsWindow::OnMotionNotifyEvent(QGraphicsSceneMouseEvent *aEvent)
{
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
nsWindow::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *aEvent)
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

    DispatchActivateEvent();

    LOGFOCUS(("Events sent from focus in event [%p]\n", (void *)this));
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::OnFocusOutEvent(QEvent *aEvent)
{
    LOGFOCUS(("OnFocusOutEvent [%p]\n", (void *)this));

    if (mWidget)
        DispatchDeactivateEvent();

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

    PRBool setNoDefault = PR_FALSE;

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

        downEvent.charCode = domCharCode;
        downEvent.keyCode = domCharCode ? 0 : domKeyCode;

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

nsEventStatus
nsWindow::OnKeyReleaseEvent(QKeyEvent *aEvent)
{
    LOGFOCUS(("OnKeyReleaseEvent [%p]\n", (void *)this));

    if (isContextMenuKeyEvent(aEvent)) {
        // er, what do we do here? DoDefault or NoDefault?
        return nsEventStatus_eConsumeDoDefault;
    }

    PRUint32 domCharCode = 0;
    PRUint32 domKeyCode = QtKeyCodeToDOMKeyCode(aEvent->key());

    if (aEvent->text().length() && aEvent->text()[0].isPrint())
        domCharCode = (PRInt32) aEvent->text()[0].unicode();

    // send the key event as a key up event
    nsKeyEvent event(PR_TRUE, NS_KEY_UP, this);
    InitKeyEvent(event, aEvent);

    event.charCode = domCharCode;
    event.keyCode = domCharCode ? 0 : domKeyCode;

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
        do_GetService(NS_STRINGBUNDLE_CONTRACTID);

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

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetWindowClass(const nsAString &xulWinType)
{
    if (!mWidget)
      return NS_ERROR_FAILURE;

    nsXPIDLString brandName;
    GetBrandName(brandName);

#ifdef Q_WS_X11
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

    XSetClassHint(GetViewWidget()->x11Info().display(),
                  GetViewWidget()->handle(),
                  class_hint);

    nsMemory::Free(class_hint->res_class);
    nsMemory::Free(class_hint->res_name);
    XFree(class_hint);
#endif

    return NS_OK;
}

void
nsWindow::NativeResize(PRInt32 aWidth, PRInt32 aHeight, PRBool  aRepaint)
{
    LOG(("nsWindow::NativeResize [%p] %d %d\n", (void *)this,
         aWidth, aHeight));

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

    mWidget->setGeometry(aX, aY, aWidth, aHeight);

    if (aRepaint)
        mWidget->update();
}

void
nsWindow::NativeShow(PRBool aAction)
{
    if (aAction == PR_TRUE)
        mWidget->show();
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

void
nsWindow::GetToplevelWidget(MozQWidget **aWidget)
{
    *aWidget = mWidget;
}

void *
nsWindow::SetupPluginPort(void)
{
    if (!mWidget)
        return nsnull;

    qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);

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
 
    GetViewWidget()->setWindowIcon(icon);
 
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
    if (aFullScreen) {
        if (mSizeMode != nsSizeMode_Fullscreen)
            mLastSizeMode = mSizeMode;

        mSizeMode = nsSizeMode_Fullscreen;
        GetViewWidget()->showFullScreen();
    }
    else {
        mSizeMode = mLastSizeMode;

        switch (mSizeMode) {
        case nsSizeMode_Maximized:
            GetViewWidget()->showMaximized();
            break;
        case nsSizeMode_Minimized:
            GetViewWidget()->showMinimized();
            break;
        case nsSizeMode_Normal:
            GetViewWidget()->showNormal();
            break;
        }
    }
    
    NS_ASSERTION(mLastSizeMode != nsSizeMode_Fullscreen,
                 "mLastSizeMode should never be fullscreen");
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::HideWindowChrome(PRBool aShouldHide)
{
    if (!mWidget) {
        // Pass the request to the toplevel window
        MozQWidget *topWidget = nsnull;
        GetToplevelWidget(&topWidget);
//        return topWindow->HideWindowChrome(aShouldHide);
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
#ifdef Q_WS_X11
    XSync(GetViewWidget()->x11Info().display(), False);
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
    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (!prefs)
        return NS_OK;

    PRBool val = PR_TRUE;
    nsresult rv;
    rv = prefs->GetBoolPref("mozilla.widget.raise-on-setfocus", &val);

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
    qDebug("===================== popup!");
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

    // create a QGraphicsView if this is a new toplevel window
    MozQGraphicsView* newView = 0;

    if (eWindowType_dialog == mWindowType ||
        eWindowType_toplevel == mWindowType)
    {
        mScene = new QGraphicsScene();
        if (!mScene) {
            delete widget;
            return nsnull;
        }

        newView = new MozQGraphicsView(mScene);
        if (!newView) {
            delete mScene;
            delete widget;
            return nsnull;
        }

        newView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        newView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        newView->show();
        newView->raise();
    }

    if (mScene && newView) {
        mScene->addItem(widget);
        newView->setTopLevel(widget);
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

    gfxQtPlatform::RenderMode renderMode = gfxQtPlatform::GetPlatform()->GetRenderMode();
    if (renderMode == gfxQtPlatform::RENDER_QPAINTER) {
        mThebesSurface = new gfxQPainterSurface(gfxIntSize(1, 1), gfxASurface::CONTENT_COLOR);
    } else if (renderMode == gfxQtPlatform::RENDER_XLIB) {
        mThebesSurface = new gfxXlibSurface(QX11Info().display(),
                                            (Visual*)QX11Info().visual(),
                                            gfxIntSize(1, 1), QX11Info().depth());
    }
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
nsWindow::imStartEvent(QEvent *)
{
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::imComposeEvent(QEvent *)
{
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::imEndEvent(QEvent * )
{
    return nsEventStatus_eIgnore;
}

nsIWidget *
nsWindow::GetParent(void)
{
    return mParent;
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

    if (!mWidget)
        return NS_OK;

    mWidget->setVisible(aState);
    if (mWindowType == eWindowType_popup && aState)
        Resize(mBounds.x, mBounds.y, mBounds.width, mBounds.height, PR_FALSE);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
    mBounds.width = aWidth;
    mBounds.height = aHeight;

    if (!mWidget)
        return NS_OK;

    mWidget->resize(aWidth, aHeight);

    if (mIsTopLevel) {
        GetViewWidget()->resize(aWidth,aHeight);
    }

    if (aRepaint)
        mWidget->update();

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

    mWidget->setGeometry(aX, aY, aWidth, aHeight);

    if (mIsTopLevel) {
        GetViewWidget()->resize(aWidth,aHeight);
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
