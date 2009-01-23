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

#include <qevent.h> //XXX switch for forward-decl
#include <QtGui>
#include <qcursor.h>

#include "nsWindow.h"
#include "nsToolkit.h"
#include "nsIDeviceContext.h"
#include "nsIRenderingContext.h"
#include "nsIRegion.h"
#include "nsIRollupListener.h"
#include "nsIMenuRollup.h"
#include "nsIDOMNode.h"

#include "nsWidgetsCID.h"
#include "nsIDragService.h"

#include "nsQtKeyUtils.h"

#ifdef Q_WS_X11
#include <X11/XF86keysym.h>
#endif

#include "nsWidgetAtoms.h"

#ifdef MOZ_ENABLE_STARTUP_NOTIFICATION
#define SN_API_NOT_YET_FROZEN
#include <startup-notification-1.0/libsn/sn.h>
#endif

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"
#include "nsGfxCIID.h"

/* For SetIcon */
#include "nsAppDirectoryServiceDefs.h"
#include "nsXPIDLString.h"
#include "nsIFile.h"
#include "nsILocalFile.h"

/* SetCursor(imgIContainer*) */
#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsGfxCIID.h"
#include "nsIImage.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsAutoPtr.h"

#include "gfxQtPlatform.h"
#include "gfxXlibSurface.h"
#include "gfxQPainterSurface.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"

#include <qapplication.h>
#include <qdesktopwidget.h>
#include <qwidget.h>
#include <qcursor.h>
#include <qobject.h>
#include <execinfo.h>
#include <stdlib.h>

#ifdef Q_WS_X11
#include "qx11info_x11.h"
#endif

#include <execinfo.h>

#include "mozqwidget.h"

/* For PrepareNativeWidget */
static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);

// initialization static functions 
static nsresult    initialize_prefs        (void);

static NS_DEFINE_IID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

#define NS_WINDOW_TITLE_MAX_LENGTH 4095

#define kWindowPositionSlop 20

// QT
static const int WHEEL_DELTA = 120;
static PRBool gGlobalsInitialized = PR_FALSE;

static nsCOMPtr<nsIRollupListener> gRollupListener;
static nsWeakPtr                   gRollupWindow;
static PRBool                      gConsumeRollupEvent;

//static nsWindow * get_window_for_qt_widget(QWidget *widget);

static PRBool     check_for_rollup(double aMouseX, double aMouseY,
                                   PRBool aIsWheel);
static PRBool
is_mouse_in_window (QWidget* aWindow, double aMouseX, double aMouseY);

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
    aEvent.isShift   = aQEvent->modifiers() & Qt::ShiftModifier;
    aEvent.isControl = aQEvent->modifiers() & Qt::ControlModifier;
    aEvent.isAlt     = aQEvent->modifiers() & Qt::AltModifier;
    aEvent.isMeta    = aQEvent->modifiers() & Qt::MetaModifier;
    aEvent.time      = 0;

    // The transformations above and in gdk for the keyval are not invertible
    // so link to the GdkEvent (which will vanish soon after return from the
    // event callback) to give plugins access to hardware_keycode and state.
    // (An XEvent would be nice but the GdkEvent is good enough.)
    aEvent.nativeMsg = (void *)aQEvent;
}

static void
keyEventToContextMenuEvent(const nsKeyEvent* aKeyEvent,
                           nsMouseEvent* aCMEvent)
{
    memcpy(aCMEvent, aKeyEvent, sizeof(nsInputEvent));
//    aCMEvent->message = NS_CONTEXTMENU_KEY;
    aCMEvent->isShift = aCMEvent->isControl = PR_FALSE;
    aCMEvent->isControl = PR_FALSE;
    aCMEvent->isAlt = aCMEvent->isMeta = PR_FALSE;
    aCMEvent->isMeta = PR_FALSE;
    aCMEvent->clickCount = 0;
    aCMEvent->acceptActivation = PR_FALSE;
}

nsWindow::nsWindow()
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__, (void *)this));
    
    mIsTopLevel       = PR_FALSE;
    mIsDestroyed      = PR_FALSE;
    mIsShown          = PR_FALSE;
    mEnabled          = PR_TRUE;

    mPreferredWidth   = 0;
    mPreferredHeight  = 0;

    mDrawingArea         = nsnull;
    mIsVisible           = PR_FALSE;
    mActivatePending     = PR_FALSE;
    mWindowType          = eWindowType_child;
    mSizeState           = nsSizeMode_Normal;
    mPluginType          = PluginType_NONE;
    mQCursor             = Qt::ArrowCursor;

    if (!gGlobalsInitialized) {
        gGlobalsInitialized = PR_TRUE;

        // It's OK if either of these fail, but it may not be one day.
        initialize_prefs();
    }

    memset(mKeyDownFlags, 0, sizeof(mKeyDownFlags));

    mIsTransparent = PR_FALSE;

    mCursor = eCursor_standard;
}

nsWindow::~nsWindow()
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__, (void *)this));
    
    Destroy();
}

/* XXX - this gets called right after CreateQWidget, which also
 * sets mDrawingArea.  We probably want to always pass a MozQWidget
 * here; things won't really work at all with any generic widget.
 */
void
nsWindow::Initialize(QWidget *widget)
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__, (void *)this));

    Q_ASSERT(widget);

    mDrawingArea = widget;
    mDrawingArea->setMouseTracking(PR_TRUE);
    mDrawingArea->setFocusPolicy(Qt::WheelFocus);
}

/* static */ void
nsWindow::ReleaseGlobals()
{
}

NS_IMPL_ISUPPORTS_INHERITED1(nsWindow, nsBaseWidget, nsISupportsWeakReference)

NS_IMETHODIMP
nsWindow::Create(nsIWidget        *aParent,
                 const nsRect     &aRect,
                 EVENT_CALLBACK   aHandleEventFunction,
                 nsIDeviceContext *aContext,
                 nsIAppShell      *aAppShell,
                 nsIToolkit       *aToolkit,
                 nsWidgetInitData *aInitData)
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__, (void *)this));

    nsresult rv = NativeCreate(aParent, nsnull, aRect, aHandleEventFunction,
                               aContext, aAppShell, aToolkit, aInitData);
    return rv;
}

NS_IMETHODIMP
nsWindow::Create(nsNativeWidget aParent,
                 const nsRect     &aRect,
                 EVENT_CALLBACK   aHandleEventFunction,
                 nsIDeviceContext *aContext,
                 nsIAppShell      *aAppShell,
                 nsIToolkit       *aToolkit,
                 nsWidgetInitData *aInitData)
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__, (void *)this));

    nsresult rv = NativeCreate(nsnull, aParent, aRect, aHandleEventFunction,
                               aContext, aAppShell, aToolkit, aInitData);
    return rv;
}

NS_IMETHODIMP
nsWindow::Destroy(void)
{
    if (mIsDestroyed || !mDrawingArea)
        return NS_OK;

    LOG(("nsWindow::Destroy [%p]\n", (void *)this));
    mIsDestroyed = PR_TRUE;

    nsCOMPtr<nsIWidget> rollupWidget = do_QueryReferent(gRollupWindow);
    if (static_cast<nsIWidget *>(this) == rollupWidget.get()) {
        if (gRollupListener)
            gRollupListener->Rollup(nsnull);
        gRollupWindow = nsnull;
        gRollupListener = nsnull;
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

    if (mMozQWidget) {
        mMozQWidget->dropReceiver();

        // Call deleteLater instead of delete; Qt still needs the object
        // to be valid even after sending it a Close event.  We could
        // also set WA_DeleteOnClose, but this gives us more control.
        mMozQWidget->deleteLater();
    }

    mDrawingArea = nsnull;

    OnDestroy();

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetParent(nsIWidget *aNewParent)
{
    NS_ENSURE_ARG_POINTER(aNewParent);

    QWidget* newParentWindow =
        static_cast<QWidget*>(aNewParent->GetNativeData(NS_NATIVE_WINDOW));
    NS_ASSERTION(newParentWindow, "Parent widget has a null native window handle");

    if (mDrawingArea) {
        qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
        // moz_drawingarea_reparent(mDrawingArea, newParentWindow);
    } else {
        NS_NOTREACHED("nsWindow::SetParent - reparenting a non-child window");
    }
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetModal(PRBool aModal)
{
    LOG(("nsWindow::SetModal [%p] %d, widget[%p]\n", (void *)this, aModal, mDrawingArea));

    MozQWidget *mozWidget = static_cast<MozQWidget*>(mDrawingArea);
    if (mozWidget)
        mozWidget->setModal(aModal);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::IsVisible(PRBool & aState)
{
    aState = mDrawingArea?mDrawingArea->isVisible():PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ConstrainPosition(PRBool aAllowSlop, PRInt32 *aX, PRInt32 *aY)
{
    if (mDrawingArea) {
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

    // Since a popup window's x/y coordinates are in relation to to
    // the parent, the parent might have moved so we always move a
    // popup window.
    //bool popup = mDrawingArea ? mDrawingArea->windowType() == Qt::Popup : false;
    if (aX == mBounds.x && aY == mBounds.y &&
        mWindowType != eWindowType_popup)
        return NS_OK;

    // XXX Should we do some AreBoundsSane check here?


    if (!mDrawingArea)
        return NS_OK;

    QPoint pos(aX, aY);
    if (mDrawingArea) {
        if (mParent && mDrawingArea->windowType() == Qt::Popup) {
            nsRect oldrect, newrect;
            oldrect.x = aX;
            oldrect.y = aY;

            mParent->WidgetToScreen(oldrect, newrect);

            pos = QPoint(newrect.x, newrect.y);
#ifdef DEBUG_WIDGETS
            qDebug("pos is [%d,%d]", pos.x(), pos.y());
#endif
        } else {
            qDebug("Widget within another? (%p)", (void*)mDrawingArea);
        }
    }

    mBounds.x = pos.x();
    mBounds.y = pos.y();

    mDrawingArea->move(pos);

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
nsWindow::SetZIndex(PRInt32 aZIndex)
{
    nsIWidget* oldPrev = GetPrevSibling();

    nsBaseWidget::SetZIndex(aZIndex);

    if (GetPrevSibling() == oldPrev) {
        return NS_OK;
    }

    NS_ASSERTION(!mDrawingArea, "Expected Mozilla child widget");

    // We skip the nsWindows that don't have mDrawingAreas.
    // These are probably in the process of being destroyed.

    if (!GetNextSibling()) {
        // We're to be on top.
        if (mDrawingArea) {
            qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
            // gdk_window_raise(mDrawingArea->clip_window);
        }
    } else {
        // All the siblings before us need to be below our widget. 
        for (nsWindow* w = this; w;
             w = static_cast<nsWindow*>(w->GetPrevSibling())) {
            if (w->mDrawingArea) {
                qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
                // gdk_window_lower(w->mDrawingArea->clip_window);
            }
        }
    }
    return NS_OK;
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
    if (!mDrawingArea || mSizeState == mSizeMode) {
        return rv;
    }

    switch (aMode) {
    case nsSizeMode_Maximized:
        mDrawingArea->showMaximized();
        break;
    case nsSizeMode_Minimized:
        mDrawingArea->showMinimized();
        break;
    default:
        // nsSizeMode_Normal, really.
        mDrawingArea->showNormal ();
        // KILLME
        //if (mSizeState == nsSizeMode_Minimized)
        //    gtk_window_deiconify(GTK_WINDOW(mDrawingArea));
        //else if (mSizeState == nsSizeMode_Maximized)
        //    gtk_window_unmaximize(GTK_WINDOW(mDrawingArea));
        break;
    }

    mSizeState = mSizeMode;

    return rv;
}

typedef void (* SetUserTimeFunc)(QWidget* aWindow, quint32 aTimestamp);

// This will become obsolete when new GTK APIs are widely supported,
// as described here: http://bugzilla.gnome.org/show_bug.cgi?id=347375
/*
static void
SetUserTimeAndStartupIDForActivatedWindow(QWidget* aWindow)
{
    nsCOMPtr<nsIToolkit> toolkit;
    NS_GetCurrentToolkit(getter_AddRefs(toolkit));
    if (!toolkit)
        return;

    nsToolkit* QTToolkit = static_cast<nsToolkit*>
                                          (static_cast<nsIToolkit*>(toolkit));
    nsCAutoString desktopStartupID;
    QTToolkit->GetDesktopStartupID(&desktopStartupID);
    if (desktopStartupID.IsEmpty()) {
        // We don't have the data we need. Fall back to an
        // approximation ... using the timestamp of the remote command
        // being received as a guess for the timestamp of the user event
        // that triggered it.
        PRUint32 timestamp = QTToolkit->GetFocusTimestamp();
        if (timestamp) {
            aWindow->focusWidget ();
            // gdk_window_focus(aWindow->window, timestamp);
            QTToolkit->SetFocusTimestamp(0);
        }
        return;
    }

    QTToolkit->SetDesktopStartupID(EmptyCString());
}
*/

NS_IMETHODIMP
nsWindow::SetFocus(PRBool aRaise)
{
    // Make sure that our owning widget has focus.  If it doesn't try to
    // grab it.  Note that we don't set our focus flag in this case.

    LOGFOCUS(("  SetFocus [%p]\n", (void *)this));

    if (!mDrawingArea)
        return NS_ERROR_FAILURE;

    if (aRaise)
        mDrawingArea->raise();
    mDrawingArea->setFocus();

    // If there is already a focused child window, dispatch a LOSTFOCUS
    // event from that widget and unset its got focus flag.

    LOGFOCUS(("  widget now has focus - dispatching events [%p]\n",
              (void *)this));

    DispatchGotFocusEvent();

    LOGFOCUS(("  done dispatching events in SetFocus() [%p]\n",
              (void *)this));

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetScreenBounds(nsRect &aRect)
{
    nsRect origin(0, 0, mBounds.width, mBounds.height);
    WidgetToScreen(origin, aRect);
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
    if (mMozQWidget)
        mMozQWidget->SetCursor(mCursor);
    return NS_OK;
}

/*
static
PRUint8* Data32BitTo1Bit(PRUint8* aImageData,
                         PRUint32 aImageBytesPerRow,
                         PRUint32 aWidth, PRUint32 aHeight)
{
  PRUint32 outBpr = (aWidth + 7) / 8;

  PRUint8* outData = new PRUint8[outBpr * aHeight];
  if (!outData)
      return NULL;

  PRUint8 *outRow = outData,
          *imageRow = aImageData;

  for (PRUint32 curRow = 0; curRow < aHeight; curRow++) {
      PRUint8 *irow = imageRow;
      PRUint8 *orow = outRow;
      PRUint8 imagePixels = 0;
      PRUint8 offset = 0;

      for (PRUint32 curCol = 0; curCol < aWidth; curCol++) {
          PRUint8 r = *imageRow++,
                  g = *imageRow++,
                  b = *imageRow++;
                  imageRow++;

          if ((r + b + g) < 3 * 128)
              imagePixels |= (1 << offset);

          if (offset == 7) {
              *outRow++ = imagePixels;
              offset = 0;
              imagePixels = 0;
          } else {
              offset++;
          }
      }
      if (offset != 0)
          *outRow++ = imagePixels;

      imageRow = irow + aImageBytesPerRow;
      outRow = orow + outBpr;
  }

  return outData;
}
*/


NS_IMETHODIMP
nsWindow::SetCursor(imgIContainer* aCursor,
                    PRUint32 aHotspotX, PRUint32 aHotspotY)
{
    nsresult rv = NS_ERROR_OUT_OF_MEMORY;
    qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
    return rv;
}


NS_IMETHODIMP
nsWindow::Validate()
{
    // Get the update for this window and, well, just drop it on the
    // floor.
    if (!mDrawingArea)
        return NS_OK;

    qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Invalidate(PRBool aIsSynchronous)
{
    LOGDRAW(("Invalidate (all) [%p]: \n", (void *)this));

    if (!mDrawingArea)
        return NS_OK;

    if (aIsSynchronous && !mDrawingArea->paintingActive())
        mDrawingArea->repaint();
    else
        mDrawingArea->update();

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Invalidate(const nsRect &aRect,
                     PRBool        aIsSynchronous)
{
    LOGDRAW(("Invalidate (rect) [%p,%p]: %d %d %d %d (sync: %d)\n", (void *)this,
             (void*)mDrawingArea,aRect.x, aRect.y, aRect.width, aRect.height, aIsSynchronous));

    if (!mDrawingArea)
        return NS_OK;

    if (aIsSynchronous)
        mDrawingArea->repaint(aRect.x, aRect.y, aRect.width, aRect.height);
    else {
        mDrawingArea->update(aRect.x, aRect.y, aRect.width, aRect.height);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::InvalidateRegion(const nsIRegion* aRegion,
                           PRBool           aIsSynchronous)
{

    QRegion *region = nsnull;
    aRegion->GetNativeRegion((void *&)region);

    if (region && mDrawingArea) {
        QRect rect = region->boundingRect();

//        LOGDRAW(("Invalidate (region) [%p]: %d %d %d %d (sync: %d)\n",
//                 (void *)this,
//                 rect.x, rect.y, rect.width, rect.height, aIsSynchronous));

        if (aIsSynchronous && !mDrawingArea->paintingActive())
            mDrawingArea->repaint(*region);
        else
            mDrawingArea->update(*region);
    }
    else {
        qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
        LOGDRAW(("Invalidate (region) [%p] with empty region\n",
                 (void *)this));
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Update()
{
    if (!mDrawingArea)
        return NS_OK;

    // mDrawingArea->update(); // FIXME  This call cause update for whole window on each scroll event
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetColorMap(nsColorMap *aColorMap)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::Scroll(PRInt32  aDx,
                 PRInt32  aDy,
                 nsRect  *aClipRect)
{
    if (!mDrawingArea)
        return NS_OK;

    mDrawingArea->scroll(aDx, aDy);

    // Update bounds on our child windows
    for (nsIWidget* kid = mFirstChild; kid; kid = kid->GetNextSibling()) {
        nsRect bounds;
        kid->GetBounds(bounds);
        bounds.x += aDx;
        bounds.y += aDy;
        static_cast<nsBaseWidget*>(kid)->SetBounds(bounds);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ScrollWidgets(PRInt32 aDx,
                        PRInt32 aDy)
{
    if (!mDrawingArea)
        return NS_OK;

    mDrawingArea->scroll(aDx, aDy);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ScrollRect(nsRect  &aSrcRect,
                     PRInt32  aDx,
                     PRInt32  aDy)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

void*
nsWindow::GetNativeData(PRUint32 aDataType)
{
    switch (aDataType) {
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_WIDGET: {
        if (!mDrawingArea)
            return nsnull;

        return mDrawingArea;
        break;
    }

    case NS_NATIVE_PLUGIN_PORT:
        return SetupPluginPort();
        break;

#ifdef Q_WS_X11
    case NS_NATIVE_DISPLAY:
        return mDrawingArea->x11Info().display();
        break;
#endif

    case NS_NATIVE_GRAPHIC: {
        NS_ASSERTION(nsnull != mToolkit, "NULL toolkit, unable to get a GC");
        return (void *)static_cast<nsToolkit *>(mToolkit)->GetSharedGC();
        break;
    }

    case NS_NATIVE_SHELLWIDGET:
        return (void *) mDrawingArea;

    default:
        NS_WARNING("nsWindow::GetNativeData called with bad value");
        return nsnull;
    }
}

NS_IMETHODIMP
nsWindow::SetBorderStyle(nsBorderStyle aBorderStyle)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::SetTitle(const nsAString& aTitle)
{
    if (mDrawingArea) {
        QString qStr(QString::fromUtf16(aTitle.BeginReading(), aTitle.Length()));
        mDrawingArea->setWindowTitle(qStr);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetIcon(const nsAString& aIconSpec)
{
    if (!mDrawingArea)
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

NS_IMETHODIMP
nsWindow::ShowMenuBar(PRBool aShow)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
    NS_ENSURE_TRUE(mDrawingArea, NS_OK);

    QPoint origin(aOldRect.x, aOldRect.y);
    origin = mDrawingArea->mapToGlobal(origin);

    aNewRect.x = origin.x();
    aNewRect.y = origin.y();
    aNewRect.width = aOldRect.width;
    aNewRect.height = aOldRect.height;

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
    NS_ENSURE_TRUE(mDrawingArea, NS_OK);

    QPoint origin(aOldRect.x, aOldRect.y);
    origin = mDrawingArea->mapFromGlobal(origin);

    aNewRect.x = origin.x();
    aNewRect.y = origin.y();
    aNewRect.width = aOldRect.width;
    aNewRect.height = aOldRect.height;

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::BeginResizingChildren(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::EndResizingChildren(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::EnableDragDrop(PRBool aEnable)
{
    mDrawingArea->setAcceptDrops(aEnable);
    return NS_OK;
}

void
nsWindow::ConvertToDeviceCoordinates(nscoord &aX,
                                     nscoord &aY)
{
}

NS_IMETHODIMP
nsWindow::PreCreateWidget(nsWidgetInitData *aWidgetInitData)
{
    if (nsnull != aWidgetInitData) {
        mWindowType = aWidgetInitData->mWindowType;
        mBorderStyle = aWidgetInitData->mBorderStyle;
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindow::CaptureMouse(PRBool aCapture)
{
    LOG(("CaptureMouse %p\n", (void *)this));

    if (!mDrawingArea)
        return NS_OK;

    if (aCapture)
        mDrawingArea->grabMouse();
    else
        mDrawingArea->releaseMouse();

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::CaptureRollupEvents(nsIRollupListener *aListener,
                              PRBool             aDoCapture,
                              PRBool             aConsumeRollupEvent)
{
    if (!mDrawingArea)
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

PRBool
check_for_rollup(double aMouseX, double aMouseY,
                 PRBool aIsWheel)
{
    PRBool retVal = PR_FALSE;
    nsCOMPtr<nsIWidget> rollupWidget = do_QueryReferent(gRollupWindow);

    if (rollupWidget && gRollupListener) {
        QWidget *currentPopup =
            (QWidget *)rollupWidget->GetNativeData(NS_NATIVE_WINDOW);

        if (!is_mouse_in_window(currentPopup, aMouseX, aMouseY)) {
            PRBool rollup = PR_TRUE;
            if (aIsWheel) {
                gRollupListener->ShouldRollupOnMouseWheelEvent(&rollup);
                retVal = PR_TRUE;
            }
            // if we're dealing with menus, we probably have submenus and
            // we don't want to rollup if the clickis in a parent menu of
            // the current submenu
            nsCOMPtr<nsIMenuRollup> menuRollup;
            menuRollup = (do_QueryInterface(gRollupListener));
            if (menuRollup) {
                nsAutoTArray<nsIWidget*, 5> widgetChain;
                menuRollup->GetSubmenuWidgetChain(&widgetChain);
                for (PRUint32 i=0; i<widgetChain.Length(); ++i) {
                    nsIWidget* widget =  widgetChain[i];
                    QWidget* currWindow =
                        (QWidget*) widget->GetNativeData(NS_NATIVE_WINDOW);
                    if (is_mouse_in_window(currWindow, aMouseX, aMouseY)) {
                       rollup = PR_FALSE;
                       break;
                    }
                } // foreach parent menu widget
            } // if rollup listener knows about menus

            // if we've determined that we should still rollup, do it.
            if (rollup) {
                gRollupListener->Rollup(nsnull);
                retVal = PR_TRUE;
            }
        }
    } else {
        gRollupWindow = nsnull;
        gRollupListener = nsnull;
    }

    return retVal;
}

/* static */
PRBool
is_mouse_in_window (QWidget* aWindow, double aMouseX, double aMouseY)
{
    int x = 0;
    int y = 0;
    int w, h;

    x = aWindow->pos().x();
    y = aWindow->pos().y();
    w = aWindow->size().width();
    h = aWindow->size().height();

    if (aMouseX > x && aMouseX < x + w &&
        aMouseY > y && aMouseY < y + h)
        return PR_TRUE;

    return PR_FALSE;
}

NS_IMETHODIMP
nsWindow::GetAttention(PRInt32 aCycleCount)
{
    LOG(("nsWindow::GetAttention [%p]\n", (void *)this));

    SetUrgencyHint(mDrawingArea, PR_TRUE);

    return NS_OK;
}

void
nsWindow::LoseFocus(void)
{
    // make sure that we reset our key down counter so the next keypress
    // for this widget will get the down event
    memset(mKeyDownFlags, 0, sizeof(mKeyDownFlags));

    // Dispatch a lostfocus event
    DispatchLostFocusEvent();

    LOGFOCUS(("  widget lost focus [%p]\n", (void *)this));
}

static int gDoubleBuffering = -1;

nsEventStatus
nsWindow::OnPaintEvent(QPaintEvent *aEvent)
{
    //fprintf (stderr, "===== Expose start\n");

    if (mIsDestroyed) {
        LOG(("Expose event on destroyed window [%p] window %p\n",
             (void *)this, mDrawingArea));
        return nsEventStatus_eIgnore;
    }

    if (!mDrawingArea)
        return nsEventStatus_eIgnore;

    static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);

    nsCOMPtr<nsIRegion> updateRegion = do_CreateInstance(kRegionCID);
    if (!updateRegion)
        return nsEventStatus_eIgnore;

    updateRegion->Init();

    QVector<QRect>  rects = aEvent->region().rects();

    LOGDRAW(("[%p] sending expose event %p 0x%lx (rects follow):\n",
             (void *)this, (void *)aEvent, 0));

    for (int i = 0; i < rects.size(); ++i) {
       QRect r = rects.at(i);
       updateRegion->Union(r.x(), r.y(), r.width(), r.height());
       LOGDRAW(("\t%d %d %d %d\n", r.x(), r.y(), r.width(), r.height()));
    }

    QPainter painter;

    if (!painter.begin(mDrawingArea)) {
        fprintf (stderr, "*********** Failed to begin painting!\n");
        return nsEventStatus_eConsumeNoDefault;
    }

    nsRefPtr<gfxQPainterSurface> targetSurface = new gfxQPainterSurface(&painter);
    nsRefPtr<gfxContext> ctx = new gfxContext(targetSurface);

    nsCOMPtr<nsIRenderingContext> rc;
    GetDeviceContext()->CreateRenderingContextInstance(*getter_AddRefs(rc));
    if (NS_UNLIKELY(!rc))
        return nsEventStatus_eIgnore;

    rc->Init(GetDeviceContext(), ctx);

    nsIntRect boundsRect;

    updateRegion->GetBoundingBox(&boundsRect.x, &boundsRect.y,
                                 &boundsRect.width, &boundsRect.height);

    nsPaintEvent event(PR_TRUE, NS_PAINT, this);
    QRect r = aEvent->rect();
    if (!r.isValid())
        r = mDrawingArea->rect();
    nsRect rect(r.x(), r.y(), r.width(), r.height());
    event.refPoint.x = aEvent->rect().x();
    event.refPoint.y = aEvent->rect().y();
    event.rect = &rect; // was null FIXME
    event.region = updateRegion;
    event.renderingContext = rc;

    nsEventStatus status = DispatchEvent(&event);
    //nsEventStatus status = nsEventStatus_eConsumeNoDefault;

    // DispatchEvent can Destroy us (bug 378273), avoid doing any paint
    // operations below if that happened - it will lead to XError and exit().
    if (NS_UNLIKELY(mIsDestroyed))
        return status;

    if (status == nsEventStatus_eIgnore)
        return status;

    LOGDRAW(("[%p] draw done\n", this));

    ctx = nsnull;
    targetSurface = nsnull;

    //fprintf (stderr, "===== Expose end\n");

    // check the return value!
    return status;
}

nsEventStatus
nsWindow::OnMoveEvent(QMoveEvent *aEvent)
{
    LOG(("configure event [%p] %d %d\n", (void *)this,
        aEvent->pos().x(),  aEvent->pos().y()));

    // can we shortcut?
    if (!mDrawingArea)
        return nsEventStatus_eIgnore;

    if ((mBounds.x == aEvent->pos().x() &&
         mBounds.y == aEvent->pos().y()))
    {
        return nsEventStatus_eIgnore;
    }

    // Toplevel windows need to have their bounds set so that we can
    // keep track of our location.  It's not often that the x,y is set
    // by the layout engine.  Width and height are set elsewhere.
    QPoint pos = aEvent->pos();
    if (mIsTopLevel) {
        // Need to translate this into the right coordinates
        nsRect oldrect, newrect;
        WidgetToScreen(oldrect, newrect);
        mBounds.x = newrect.x;
        mBounds.y = newrect.y;
    }

    nsGUIEvent event(PR_TRUE, NS_MOVE, this);

    event.refPoint.x = pos.x();
    event.refPoint.y = pos.y();

    // XXX mozilla will invalidate the entire window after this move
    // complete.  wtf?
    return DispatchEvent(&event);
}

nsEventStatus
nsWindow::OnResizeEvent(QResizeEvent *e)
{
    nsRect rect;

    // Generate XPFE resize event
    GetBounds(rect);

    rect.width = e->size().width();
    rect.height = e->size().height();

    LOG(("size_allocate [%p] %d %d\n",
         (void *)this, rect.width, rect.height));

    mBounds.width = rect.width;
    mBounds.height = rect.height;

#ifdef DEBUG_WIDGETS
    qDebug("resizeEvent: mDrawingArea=%p, aWidth=%d, aHeight=%d, aX = %d, aY = %d", (void*)mDrawingArea,
           rect.width, rect.height, rect.x, rect.y);
#endif

    if (mDrawingArea)
        mDrawingArea->resize(rect.width, rect.height);

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
nsWindow::OnEnterNotifyEvent(QEvent *aEvent)
{
    nsMouseEvent event(PR_TRUE, NS_MOUSE_ENTER, this, nsMouseEvent::eReal);

    QPoint pt = QCursor::pos();

    event.refPoint.x = nscoord(pt.x());
    event.refPoint.y = nscoord(pt.y());

    LOG(("OnEnterNotify: %p\n", (void *)this));

    return DispatchEvent(&event);
}

nsEventStatus
nsWindow::OnLeaveNotifyEvent(QEvent *aEvent)
{
    nsMouseEvent event(PR_TRUE, NS_MOUSE_EXIT, this, nsMouseEvent::eReal);

    QPoint pt = QCursor::pos();

    event.refPoint.x = nscoord(pt.x());
    event.refPoint.y = nscoord(pt.y());

    LOG(("OnLeaveNotify: %p\n", (void *)this));

    return DispatchEvent(&event);
}

nsEventStatus
nsWindow::OnMotionNotifyEvent(QMouseEvent *aEvent)
{
    // when we receive this, it must be that the gtk dragging is over,
    // it is dropped either in or out of mozilla, clear the flag
    //mDrawingArea->setCursor(mQCursor);

    nsMouseEvent event(PR_TRUE, NS_MOUSE_MOVE, this, nsMouseEvent::eReal);


    event.refPoint.x = nscoord(aEvent->x());
    event.refPoint.y = nscoord(aEvent->y());

    event.isShift         = aEvent->modifiers() & Qt::ShiftModifier;
    event.isControl       = aEvent->modifiers() & Qt::ControlModifier;
    event.isAlt           = aEvent->modifiers() & Qt::AltModifier;
    event.isMeta          = aEvent->modifiers() & Qt::MetaModifier;
    event.clickCount      = 0;

    nsEventStatus status = DispatchEvent(&event);

    //fprintf (stderr, "[%p] %p MotionNotify -> %d\n", this, mDrawingArea, status);

    return status;
}

void
nsWindow::InitButtonEvent(nsMouseEvent &event,
                          QMouseEvent *aEvent, int aClickCount)
{
    event.refPoint.x = nscoord(aEvent->x());
    event.refPoint.y = nscoord(aEvent->y());

    event.isShift         = aEvent->modifiers() & Qt::ShiftModifier;
    event.isControl       = aEvent->modifiers() & Qt::ControlModifier;
    event.isAlt           = aEvent->modifiers() & Qt::AltModifier;
    event.isMeta          = aEvent->modifiers() & Qt::MetaModifier;
    event.clickCount      = aClickCount;
}

nsEventStatus
nsWindow::OnButtonPressEvent(QMouseEvent *aEvent)
{
    PRBool rolledUp = check_for_rollup(aEvent->globalX(),
                                       aEvent->globalY(), PR_FALSE);
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

    //fprintf (stderr, "[%p] %p ButtonPress -> %d\n", this, mDrawingArea, status);

    return status;
}

nsEventStatus
nsWindow::OnButtonReleaseEvent(QMouseEvent *aEvent)
{
    PRUint16 domButton;
//    mLastButtonReleaseTime = aEvent->time;

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

    //fprintf (stderr, "[%p] %p ButtonRelease -> %d\n", this, mDrawingArea, status);

    return status;
}

nsEventStatus
nsWindow::mouseDoubleClickEvent(QMouseEvent *e)
{
    PRUint32      eventType;

    switch (e->button()) {
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

    InitButtonEvent(event, e, 2);
    //pressed
    return DispatchEvent(&event);
}

nsEventStatus
nsWindow::OnFocusInEvent(QFocusEvent *aEvent)
{
    LOGFOCUS(("OnFocusInEvent [%p]\n", (void *)this));
    // Return if someone has blocked events for this widget.  This will
    // happen if someone has called gtk_widget_grab_focus() from
    // nsWindow::SetFocus() and will prevent recursion.

    if (!mDrawingArea)
        return nsEventStatus_eIgnore;

    // Unset the urgency hint, if possible
//    SetUrgencyHint(top_window, PR_FALSE);

    // dispatch a got focus event
    DispatchGotFocusEvent();

    // send the activate event if it wasn't already sent via any
    // SetFocus() calls that were the result of the GOTFOCUS event
    // above.
    DispatchActivateEvent();

    LOGFOCUS(("Events sent from focus in event [%p]\n", (void *)this));
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::OnFocusOutEvent(QFocusEvent *aEvent)
{
    LOGFOCUS(("OnFocusOutEvent [%p]\n", (void *)this));

    DispatchLostFocusEvent();
    if (mDrawingArea)
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

    return TRUE;
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
nsWindow::OnScrollEvent(QWheelEvent *aEvent)
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

    event.refPoint.x = nscoord(aEvent->x());
    event.refPoint.y = nscoord(aEvent->y());

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
    // qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
/*
    QRect r = mDrawingArea->rect();
    nsRect rect(r.x(), r.y(), r.width(), r.height());

    nsCOMPtr<nsIRenderingContext> rc = getter_AddRefs(GetRenderingContext());
       // Generate XPFE paint event
    nsPaintEvent event(PR_TRUE, NS_PAINT, this);
    event.refPoint.x = 0;
    event.refPoint.y = 0;
    event.rect = &rect;
    // XXX fix this!
    event.region = nsnull;
    // XXX fix this!
    event.renderingContext = rc;

    return DispatchEvent(&event);
*/
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

nsEventStatus
nsWindow::OnWindowStateEvent(QEvent *aEvent)
{
    qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
    nsSizeModeEvent event(PR_TRUE, NS_SIZEMODE, this);
    return DispatchEvent(&event);
}

void
nsWindow::ThemeChanged()
{
    nsGUIEvent event(PR_TRUE, NS_THEMECHANGED, this);

    DispatchEvent(&event);

    if (!mDrawingArea || NS_UNLIKELY(mIsDestroyed))
        return;
    qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
    return;
}

nsEventStatus
nsWindow::OnDragMotionEvent(QDragMoveEvent *e)
{
    LOG(("nsWindow::OnDragMotionSignal\n"));

    nsMouseEvent event(PR_TRUE, NS_DRAGDROP_OVER, 0,
                       nsMouseEvent::eReal);
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::OnDragLeaveEvent(QDragLeaveEvent *e)
{
    // XXX Do we want to pass this on only if the event's subwindow is null?
    LOG(("nsWindow::OnDragLeaveSignal(%p)\n", this));
    nsMouseEvent event(PR_TRUE, NS_DRAGDROP_EXIT, this, nsMouseEvent::eReal);

    return DispatchEvent(&event);
}

nsEventStatus
nsWindow::OnDragDropEvent(QDropEvent *aDropEvent)
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
nsWindow::OnDragEnter(QDragEnterEvent *aDragEvent)
{
#if 0
    // TODO: Remove debugging prints
    QStringList strings = aDragEvent->mimeData()->formats();
    for (int i=0; i<strings.size(); ++i)
    {
        printf("%i: %s\n", i, strings.at(i).toLocal8Bit().constData());
    }
#endif

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
nsWindow::NativeCreate(nsIWidget        *aParent,
                       nsNativeWidget    aNativeParent,
                       const nsRect     &aRect,
                       EVENT_CALLBACK    aHandleEventFunction,
                       nsIDeviceContext *aContext,
                       nsIAppShell      *aAppShell,
                       nsIToolkit       *aToolkit,
                       nsWidgetInitData *aInitData)
{
    // only set the base parent if we're going to be a dialog or a
    // toplevel
    nsIWidget *baseParent = aInitData &&
        (aInitData->mWindowType == eWindowType_dialog ||
         aInitData->mWindowType == eWindowType_toplevel ||
         aInitData->mWindowType == eWindowType_invisible) ?
        nsnull : aParent;

    // initialize all the common bits of this class
    BaseCreate(baseParent, aRect, aHandleEventFunction, aContext,
               aAppShell, aToolkit, aInitData);

    // and do our common creation
    mParent = aParent;

    // save our bounds
    mBounds = aRect;

    // figure out our parent window
    QWidget      *parent = nsnull;
    if (aParent != nsnull)
        parent = (QWidget*)aParent->GetNativeData(NS_NATIVE_WIDGET);
    else
        parent = (QWidget*)aNativeParent;

    // ok, create our windows
    mDrawingArea = createQWidget(parent, aInitData);

    Initialize(mDrawingArea);

    LOG(("Create: nsWindow [%p] [%p]\n", (void *)this, (void *)mDrawingArea));

    // resize so that everything is set to the right dimensions
    Resize(mBounds.x, mBounds.y, mBounds.width, mBounds.height, PR_FALSE);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetWindowClass(const nsAString &xulWinType)
{
  if (!mDrawingArea)
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

  // gdk_window_set_role(GTK_WIDGET(mDrawingArea)->window, role);
  qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
  // Can't use gtk_window_set_wmclass() for this; it prints
  // a warning & refuses to make the change.
  XSetClassHint(mDrawingArea->x11Info().display(),
                mDrawingArea->handle(),
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

    mDrawingArea->resize( aWidth, aHeight);

    if (aRepaint)
        mDrawingArea->update();
}

void
nsWindow::NativeResize(PRInt32 aX, PRInt32 aY,
                       PRInt32 aWidth, PRInt32 aHeight,
                       PRBool  aRepaint)
{
    LOG(("nsWindow::NativeResize [%p] %d %d %d %d\n", (void *)this,
         aX, aY, aWidth, aHeight));

    QPoint pos(aX, aY);
    if (mDrawingArea)
    {
        if (mParent && mDrawingArea->windowType() == Qt::Popup) {
            nsRect oldrect, newrect;
            oldrect.x = aX;
            oldrect.y = aY;

            mParent->WidgetToScreen(oldrect, newrect);

            pos = QPoint(newrect.x, newrect.y);
#ifdef DEBUG_WIDGETS
            qDebug("pos is [%d,%d]", pos.x(), pos.y());
#endif
        } else {
#ifdef DEBUG_WIDGETS
            qDebug("Widget with original position? (%p)", mDrawingArea);
#endif
        }
    }

    mDrawingArea->setGeometry(pos.x(), pos.y(), aWidth, aHeight);

    if (aRepaint)
        mDrawingArea->update();
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
nsWindow::GetToplevelWidget(QWidget **aWidget)
{
    *aWidget = nsnull;

    if (mDrawingArea) {
        *aWidget = mDrawingArea;
        return;
    }
}

void
nsWindow::SetUrgencyHint(QWidget *top_window, PRBool state)
{
    if (!top_window)
        return;
    qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
/*
    // Try to get a pointer to gdk_window_set_urgency_hint
    PRLibrary* lib;
    _gdk_window_set_urgency_hint_fn _gdk_window_set_urgency_hint = nsnull;
    _gdk_window_set_urgency_hint = (_gdk_window_set_urgency_hint_fn)
           PR_FindFunctionSymbolAndLibrary("gdk_window_set_urgency_hint", &lib);

    if (_gdk_window_set_urgency_hint) {
        _gdk_window_set_urgency_hint(top_window->window, state);
        PR_UnloadLibrary(lib);
    }
    else if (state) {
        gdk_window_show_unraised(top_window->window);
    }
*/
}

void *
nsWindow::SetupPluginPort(void)
{
    if (!mDrawingArea)
        return nsnull;

    qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);

/*
    // we have to flush the X queue here so that any plugins that
    // might be running on separate X connections will be able to use
    // this window in case it was just created
    XWindowAttributes xattrs;
    XGetWindowAttributes(Qt::Key_DISPLAY (),
                         Qt::Key_WINDOW_XWINDOW(mDrawingArea->inner_window),
                         &xattrs);
    XSelectInput (Qt::Key_DISPLAY (),
                  Qt::Key_WINDOW_XWINDOW(mDrawingArea->inner_window),
                  xattrs.your_event_mask |
                  SubstructureNotifyMask);

    gdk_window_add_filter(mDrawingArea->inner_window,
                          plugin_window_filter_func,
                          this);

    XSync(Qt::Key_DISPLAY(), False);

    return (void *)Qt::Key_WINDOW_XWINDOW(mDrawingArea->inner_window);
*/
    return nsnull;
}

nsresult
nsWindow::SetWindowIconList(const nsTArray<nsCString> &aIconList)
{
    qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
    return NS_OK;
}

void
nsWindow::SetDefaultIcon(void)
{
    SetIcon(NS_LITERAL_STRING("default"));
}

void
nsWindow::SetPluginType(PluginType aPluginType)
{
    mPluginType = aPluginType;
}

void
nsWindow::SetNonXEmbedPluginFocus()
{
    qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
}

void
nsWindow::LoseNonXEmbedPluginFocus()
{
    qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
    LOGFOCUS(("nsWindow::LoseNonXEmbedPluginFocus\n"));
    LOGFOCUS(("nsWindow::LoseNonXEmbedPluginFocus end\n"));
}


qint32
nsWindow::ConvertBorderStyles(nsBorderStyle aStyle)
{
    qint32 w = 0;

    if (aStyle == eBorderStyle_default)
        return -1;

    qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
/*
    if (aStyle & eBorderStyle_all)
        w |= Qt::Key_DECOR_ALL;
    if (aStyle & eBorderStyle_border)
        w |= Qt::Key_DECOR_BORDER;
    if (aStyle & eBorderStyle_resizeh)
        w |= Qt::Key_DECOR_RESIZEH;
    if (aStyle & eBorderStyle_title)
        w |= Qt::Key_DECOR_TITLE;
    if (aStyle & eBorderStyle_menu)
        w |= Qt::Key_DECOR_MENU;
    if (aStyle & eBorderStyle_minimize)
        w |= Qt::Key_DECOR_MINIMIZE;
    if (aStyle & eBorderStyle_maximize)
        w |= Qt::Key_DECOR_MAXIMIZE;
    if (aStyle & eBorderStyle_close) {
#ifdef DEBUG
        printf("we don't handle eBorderStyle_close yet... please fix me\n");
#endif
    }
*/
    return w;
}

NS_IMETHODIMP
nsWindow::MakeFullScreen(PRBool aFullScreen)
{
/*
#if GTK_CHECK_VERSION(2,2,0)
    if (aFullScreen)
        gdk_window_fullscreen (mDrawingArea->window);
    else
        gdk_window_unfullscreen (mDrawingArea->window);
    return NS_OK;
#else
*/
    return nsBaseWidget::MakeFullScreen(aFullScreen);
//#endif
}

NS_IMETHODIMP
nsWindow::HideWindowChrome(PRBool aShouldHide)
{
    if (!mDrawingArea) {
        // Pass the request to the toplevel window
        QWidget *topWidget = nsnull;
        GetToplevelWidget(&topWidget);
//        return topWindow->HideWindowChrome(aShouldHide);
        return NS_ERROR_FAILURE;
    }

    // Sawfish, metacity, and presumably other window managers get
    // confused if we change the window decorations while the window
    // is visible.
    PRBool wasVisible = PR_FALSE;
    if (mDrawingArea->isVisible()) {
        mDrawingArea->hide();
        wasVisible = PR_TRUE;
    }

    qint32 wmd;
    if (aShouldHide)
        wmd = 0;
    else
        wmd = ConvertBorderStyles(mBorderStyle);

//    gdk_window_set_decorations(mDrawingArea->window, (GdkWMDecoration) wmd);

    if (wasVisible) {
        mDrawingArea->show();
    }

    // For some window managers, adding or removing window decorations
    // requires unmapping and remapping our toplevel window.  Go ahead
    // and flush the queue here so that we don't end up with a BadWindow
    // error later when this happens (when the persistence timer fires
    // and GetWindowPos is called)
#ifdef Q_WS_X11
    XSync(mDrawingArea->x11Info().display(), False);
#endif

    return NS_OK;
}

/* static */
/*
nsWindow *
get_window_for_qt_widget(QWidget *widget)
{
    MozQWidget *mozWidget = static_cast<MozQWidget*>(widget);
    return mozWidget->getReceiver();
}
*/

//////////////////////////////////////////////////////////////////////
// These are all of our drag and drop operations

void
nsWindow::InitDragEvent(nsMouseEvent &aEvent)
{
    // set the keyboard modifiers
/*
    qint32 x, y;

    GdkModifierType state = (GdkModifierType)0;
    gdk_window_get_pointer(NULL, &x, &y, &state);
    aEvent.isShift = (state & Qt::Key_SHIFT_MASK) ? PR_TRUE : PR_FALSE;
    aEvent.isControl = (state & Qt::Key_CONTROL_MASK) ? PR_TRUE : PR_FALSE;
    aEvent.isAlt = (state & Qt::Key_MOD1_MASK) ? PR_TRUE : PR_FALSE;
    aEvent.isMeta = PR_FALSE; // GTK+ doesn't support the meta key
*/
}

// This will update the drag action based on the information in the
// drag context.  Gtk gets this from a combination of the key settings
// and what the source is offering.

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
    aEvent.refPoint = nsPoint(0, 0);
    aEvent.isShift = PR_FALSE;
    aEvent.isControl = PR_FALSE;
    aEvent.isAlt = PR_FALSE;
    aEvent.isMeta = PR_FALSE;
    aEvent.time = 0;
    aEvent.clickCount = 1;
}

/*
static PRBool
gdk_keyboard_get_modmap_masks(Display*  aDisplay,
                              PRUint32* aCapsLockMask,
                              PRUint32* aNumLockMask,
                              PRUint32* aScrollLockMask)
{
    *aCapsLockMask = 0;
    *aNumLockMask = 0;
    *aScrollLockMask = 0;

    int min_keycode = 0;
    int max_keycode = 0;
    XDisplayKeycodes(aDisplay, &min_keycode, &max_keycode);

    int keysyms_per_keycode = 0;
    KeySym* xkeymap = XGetKeyboardMapping(aDisplay, min_keycode,
                                          max_keycode - min_keycode + 1,
                                          &keysyms_per_keycode);
    if (!xkeymap) {
        return PR_FALSE;
    }

    XModifierKeymap* xmodmap = XGetModifierMapping(aDisplay);
    if (!xmodmap) {
        XFree(xkeymap);
        return PR_FALSE;
    }

//      The modifiermap member of the XModifierKeymap structure contains 8 sets
//      of max_keypermod KeyCodes, one for each modifier in the order Shift,
//      Lock, Control, Mod1, Mod2, Mod3, Mod4, and Mod5.
//      Only nonzero KeyCodes have meaning in each set, and zero KeyCodes are ignored.
    const unsigned int map_size = 8 * xmodmap->max_keypermod;
    for (unsigned int i = 0; i < map_size; i++) {
        KeyCode keycode = xmodmap->modifiermap[i];
        if (!keycode || keycode < min_keycode || keycode > max_keycode)
            continue;

        const KeySym* syms = xkeymap + (keycode - min_keycode) * keysyms_per_keycode;
        const unsigned int mask = 1 << (i / xmodmap->max_keypermod);
        for (int j = 0; j < keysyms_per_keycode; j++) {
            switch (syms[j]) {
                case Qt::Key_CapsLock:   *aCapsLockMask |= mask;   break;
                case Qt::Key_NumLock:    *aNumLockMask |= mask;    break;
                case Qt::Key_ScrollLock: *aScrollLockMask |= mask; break;
            }
        }
    }

    XFreeModifiermap(xmodmap);
    XFree(xkeymap);
    return PR_TRUE;
}
*/

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

QWidget*
nsWindow::createQWidget(QWidget *parent, nsWidgetInitData *aInitData)
{
    Qt::WFlags flags = Qt::Widget;
    const char *windowName = NULL;

    if (gDoubleBuffering == -1) {
        if (getenv("MOZ_NO_DOUBLEBUFFER"))
            gDoubleBuffering = 0;
        else
            gDoubleBuffering = 1;
    }

#ifdef DEBUG_WIDGETS
    qDebug("NEW WIDGET\n\tparent is %p (%s)", (void*)parent,
           parent ? qPrintable(parent->objectName()) : "null");
#endif
    // ok, create our windows
    switch (mWindowType) {
    case eWindowType_dialog:
        flags |= Qt::Dialog;
        windowName = "topLevelDialog";
        break;
    case eWindowType_popup:
        flags |= Qt::ToolTip;
        windowName = "topLevelPopup";
        break;
    case eWindowType_toplevel:
        flags |= Qt::Window;
        windowName = "topLevelWindow";
        break;
    case eWindowType_invisible:
        flags |= Qt::Window;
        windowName = "topLevelInvisible";
        break;
    case eWindowType_child:
    default: // plugin, java, sheet
        windowName = "paintArea";
        break;
    }

    mMozQWidget = new MozQWidget(this, parent, windowName, flags);
    mDrawingArea = mMozQWidget;
 
    if (mWindowType == eWindowType_popup) {
        mMozQWidget->setFocusPolicy(Qt::WheelFocus);
 
        // XXX is this needed for Qt?
        // gdk does not automatically set the cursor for "temporary"
        // windows, which are what gtk uses for popups.
        SetCursor(eCursor_standard);
    } else if (mIsTopLevel) {
        SetDefaultIcon();
    }
 
    mMozQWidget->setAttribute(Qt::WA_StaticContents);
    mMozQWidget->setAttribute(Qt::WA_OpaquePaintEvent); // Transparent Widget Background
    mMozQWidget->setAttribute(Qt::WA_NoSystemBackground);
  
    if (!gDoubleBuffering)
        mMozQWidget->setAttribute(Qt::WA_PaintOnScreen);

    return mDrawingArea;
}

// return the gfxASurface for rendering to this widget
gfxASurface*
nsWindow::GetThebesSurface()
{
    /* This is really a dummy surface; this is only used when doing reflow, because
     * we need a RenderingContext to measure text against.
     */
    if (!mThebesSurface)
        mThebesSurface = new gfxQPainterSurface(gfxIntSize(5,5), gfxASurface::CONTENT_COLOR);

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

    qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);

    return NS_OK;
}

nsEventStatus
nsWindow::contextMenuEvent(QContextMenuEvent *)
{
    //qDebug("context menu");
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::imStartEvent(QEvent *)
{
    qWarning("XXX imStartEvent");
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::imComposeEvent(QEvent *)
{
    qWarning("XXX imComposeEvent");
    return nsEventStatus_eIgnore;
}

nsEventStatus
nsWindow::imEndEvent(QEvent * )
{
    qWarning("XXX imComposeEvent");
    return nsEventStatus_eIgnore;
}

nsIWidget *
nsWindow::GetParent(void)
{
    return mParent;
}

void
nsWindow::DispatchGotFocusEvent(void)
{
    nsGUIEvent event(PR_TRUE, NS_GOTFOCUS, this);
    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsWindow::DispatchLostFocusEvent(void)
{
    nsGUIEvent event(PR_TRUE, NS_LOSTFOCUS, this);
    nsEventStatus status;
    DispatchEvent(&event, status);
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
nsWindow::DispatchResizeEvent(nsRect &aRect, nsEventStatus &aStatus)
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

    // dispatch to event listener if event was not consumed
    if ((aStatus != nsEventStatus_eIgnore) && mEventListener)
        aStatus = mEventListener->ProcessEvent(*aEvent);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Show(PRBool aState)
{
    LOG(("nsWindow::Show [%p] state %d\n", (void *)this, aState));

    mIsShown = aState;

    if (!mDrawingArea)
        return NS_OK;

    mDrawingArea->setVisible(aState);
    if (mWindowType == eWindowType_popup && aState)
        Resize(mBounds.x, mBounds.y, mBounds.width, mBounds.height, PR_FALSE);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
    mBounds.width = aWidth;
    mBounds.height = aHeight;

    if (!mDrawingArea)
        return NS_OK;

    mDrawingArea->resize(aWidth, aHeight);

    if (aRepaint)
        mDrawingArea->update();

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

    if (!mDrawingArea)
        return NS_OK;

    QPoint pos(aX, aY);

    // XXXvlad what?
#if 0
    if (mParent && mDrawingArea->windowType() == Qt::Popup) {
        nsRect oldrect, newrect;
        oldrect.x = aX;
        oldrect.y = aY;

        mParent->WidgetToScreen(oldrect, newrect);

        pos = QPoint(newrect.x, newrect.y);
#ifdef DEBUG_WIDGETS
        qDebug("pos is [%d,%d]", pos.x(), pos.y());
#endif
    } else {
#ifdef DEBUG_WIDGETS
        qDebug("Widget with original position? (%p)", mDrawingArea);
#endif
    }
#endif

    mDrawingArea->setGeometry(pos.x(), pos.y(), aWidth, aHeight);

    if (aRepaint)
        mDrawingArea->update();

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetPreferredSize(PRInt32 &aWidth,
                                 PRInt32 &aHeight)
{
    aWidth  = mPreferredWidth;
    aHeight = mPreferredHeight;
    return (mPreferredWidth != 0 && mPreferredHeight != 0) ? 
        NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWindow::SetPreferredSize(PRInt32 aWidth,
                                 PRInt32 aHeight)
{
    mPreferredWidth  = aWidth;
    mPreferredHeight = aHeight;
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
