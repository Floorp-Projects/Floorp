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

#include <X11/XF86keysym.h>

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

#include "gfxPlatformQt.h"
#include "gfxXlibSurface.h"
#include "gfxQPainterSurface.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"

#include <qapplication.h>
#include <qdesktopwidget.h>
#include <qwidget.h>
#include "qx11info_x11.h"
#include <qcursor.h>
#include <qobject.h>
#include <execinfo.h>
#include <stdlib.h>

#include <execinfo.h>

#include "mozqwidget.h"

/* For PrepareNativeWidget */
static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);

/* utility functions */
/*
static PRBool     is_mouse_in_window(QWidget* aWindow,
                                     double aMouseX, double aMouseY);
*/
// initialization static functions 
static nsresult    initialize_prefs        (void);

static NS_DEFINE_IID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

#define NS_WINDOW_TITLE_MAX_LENGTH 4095

#define kWindowPositionSlop 20

// QT
static const int WHEEL_DELTA = 120;
static PRBool gGlobalsInitialized = PR_FALSE;
//static nsWindow * get_window_for_qt_widget(QWidget *widget);
static bool ignoreEvent(nsEventStatus aStatus)
{
    return aStatus == nsEventStatus_eConsumeNoDefault;
}

static PRBool
isContextMenuKey(const nsKeyEvent &aKeyEvent)
{
    return ((aKeyEvent.keyCode == NS_VK_F10 && aKeyEvent.isShift &&
             !aKeyEvent.isControl && !aKeyEvent.isMeta && !aKeyEvent.isAlt) ||
            (aKeyEvent.keyCode == NS_VK_CONTEXT_MENU && !aKeyEvent.isShift &&
             !aKeyEvent.isControl && !aKeyEvent.isMeta && !aKeyEvent.isAlt));
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
    mDrawingarea         = nsnull;
    mIsVisible           = PR_FALSE;
    mRetryPointerGrab    = PR_FALSE;
    mRetryKeyboardGrab   = PR_FALSE;
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
    mTransparencyBitmap = nsnull;

    mTransparencyBitmapWidth  = 0;
    mTransparencyBitmapHeight = 0;
    mCursor = eCursor_standard;
}

nsWindow::~nsWindow()
{
    LOG(("nsWindow::~nsWindow() [%p]\n", (void *)this));

    delete[] mTransparencyBitmap;
    mTransparencyBitmap = nsnull;

    Destroy();
}

void
nsWindow::Initialize(QWidget *widget)
{
    Q_ASSERT(widget);

    mDrawingarea = widget;
    mDrawingarea->setMouseTracking(PR_TRUE);
    mDrawingarea->setFocusPolicy(Qt::WheelFocus);
}

/* static */ void
nsWindow::ReleaseGlobals()
{
}

NS_IMPL_ISUPPORTS_INHERITED1(nsWindow, nsCommonWidget,
                             nsISupportsWeakReference)

NS_IMETHODIMP
nsWindow::Create(nsIWidget        *aParent,
                 const nsRect     &aRect,
                 EVENT_CALLBACK   aHandleEventFunction,
                 nsIDeviceContext *aContext,
                 nsIAppShell      *aAppShell,
                 nsIToolkit       *aToolkit,
                 nsWidgetInitData *aInitData)
{
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
    nsresult rv = NativeCreate(nsnull, aParent, aRect, aHandleEventFunction,
                               aContext, aAppShell, aToolkit, aInitData);
    return rv;
}

NS_IMETHODIMP
nsWindow::Destroy(void)
{
    if (mIsDestroyed || !mCreated)
        return NS_OK;

    LOG(("nsWindow::Destroy [%p]\n", (void *)this));
    mIsDestroyed = PR_TRUE;
    mCreated = PR_FALSE;

    NativeShow(PR_FALSE);

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

    if (mDrawingarea) {
        delete mDrawingarea;
        mDrawingarea = nsnull;
    }

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

    if (mDrawingarea) {
        qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
        // moz_drawingarea_reparent(mDrawingarea, newParentWindow);
    } else {
        NS_NOTREACHED("nsWindow::SetParent - reparenting a non-child window");
    }
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetModal(PRBool aModal)
{
    LOG(("nsWindow::SetModal [%p] %d, widget[%p]\n", (void *)this, aModal, mDrawingarea));

    MozQWidget *mozWidget = static_cast<MozQWidget*>(mDrawingarea);
    if (mozWidget)
        mozWidget->setModal(aModal);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::IsVisible(PRBool & aState)
{
    aState = mDrawingarea?mDrawingarea->isVisible():PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ConstrainPosition(PRBool aAllowSlop, PRInt32 *aX, PRInt32 *aY)
{
    if (mDrawingarea) {
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

    mPlaced = PR_TRUE;

    // Since a popup window's x/y coordinates are in relation to to
    // the parent, the parent might have moved so we always move a
    // popup window.
    //bool popup = mDrawingarea ? mDrawingarea->windowType() == Qt::Popup : false;
    if (aX == mBounds.x && aY == mBounds.y &&
        mWindowType != eWindowType_popup)
        return NS_OK;

    // XXX Should we do some AreBoundsSane check here?


    if (!mDrawingarea)
        return NS_OK;

    QPoint pos(aX, aY);
    if (mDrawingarea) {
        if (mParent && mDrawingarea->windowType() == Qt::Popup) {
            nsRect oldrect, newrect;
            oldrect.x = aX;
            oldrect.y = aY;

            mParent->WidgetToScreen(oldrect, newrect);

            pos = QPoint(newrect.x, newrect.y);
#ifdef DEBUG_WIDGETS
            qDebug("pos is [%d,%d]", pos.x(), pos.y());
#endif
        } else {
            qDebug("Widget within another? (%p)", (void*)mDrawingarea);
        }
    }

    mBounds.x = pos.x();
    mBounds.y = pos.y();

    if (!mCreated)
        return NS_OK;

    if (mIsTopLevel) {
        mDrawingarea->move(pos);
    }
    else if (mDrawingarea) {
        mDrawingarea->move(pos);
    }

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

    NS_ASSERTION(!mDrawingarea, "Expected Mozilla child widget");

    // We skip the nsWindows that don't have mDrawingareas.
    // These are probably in the process of being destroyed.

    if (!GetNextSibling()) {
        // We're to be on top.
        if (mDrawingarea) {
            qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
            // gdk_window_raise(mDrawingarea->clip_window);
        }
    } else {
        // All the siblings before us need to be below our widget. 
        for (nsWindow* w = this; w;
             w = static_cast<nsWindow*>(w->GetPrevSibling())) {
            if (w->mDrawingarea) {
                qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
                // gdk_window_lower(w->mDrawingarea->clip_window);
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
    if (!mDrawingarea || mSizeState == mSizeMode) {
        return rv;
    }

    switch (aMode) {
    case nsSizeMode_Maximized:
        mDrawingarea->showMaximized();
        break;
    case nsSizeMode_Minimized:
        mDrawingarea->showMinimized();
        break;
    default:
        // nsSizeMode_Normal, really.
        mDrawingarea->showNormal ();
        // KILLME
        //if (mSizeState == nsSizeMode_Minimized)
        //    gtk_window_deiconify(GTK_WINDOW(mDrawingarea));
        //else if (mSizeState == nsSizeMode_Maximized)
        //    gtk_window_unmaximize(GTK_WINDOW(mDrawingarea));
        break;
    }

    mSizeState = mSizeMode;

    return rv;
}

NS_IMETHODIMP
nsWindow::Enable(PRBool aState)
{
    return NS_ERROR_NOT_IMPLEMENTED;
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

    if (!mDrawingarea)
        return NS_ERROR_FAILURE;

    if (aRaise)
        mDrawingarea->raise();
    mDrawingarea->setFocus();

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
    MozQWidget *mozWidget = static_cast<MozQWidget*>(mDrawingarea);
    mozWidget->SetCursor(mCursor);
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
    if (!mDrawingarea)
        return NS_OK;

    qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Invalidate(PRBool aIsSynchronous)
{
    LOGDRAW(("Invalidate (all) [%p]: \n", (void *)this));

    if (!mDrawingarea)
        return NS_OK;

    if (aIsSynchronous)
        mDrawingarea->repaint();
    else
        mDrawingarea->update();

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Invalidate(const nsRect &aRect,
                     PRBool        aIsSynchronous)
{
    LOGDRAW(("Invalidate (rect) [%p]: %d %d %d %d (sync: %d)\n", (void *)this,
             aRect.x, aRect.y, aRect.width, aRect.height, aIsSynchronous));

    if (!mDrawingarea)
        return NS_OK;

    if (aIsSynchronous)
        mDrawingarea->repaint(aRect.x, aRect.y, aRect.width, aRect.height);
    else
        mDrawingarea->update(aRect.x, aRect.y, aRect.width, aRect.height);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::InvalidateRegion(const nsIRegion* aRegion,
                           PRBool           aIsSynchronous)
{

    QRegion *region = nsnull;
    aRegion->GetNativeRegion((void *&)region);

    if (region && mDrawingarea) {
        QRect rect = region->boundingRect();

//        LOGDRAW(("Invalidate (region) [%p]: %d %d %d %d (sync: %d)\n",
//                 (void *)this,
//                 rect.x, rect.y, rect.width, rect.height, aIsSynchronous));

        if (aIsSynchronous)
            mDrawingarea->repaint(*region);
        else
            mDrawingarea->update(*region);
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
    if (!mDrawingarea)
        return NS_OK;

    mDrawingarea->update();
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
    if (!mDrawingarea)
        return NS_OK;

    mDrawingarea->scroll(aDx, aDy);

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
    if (!mDrawingarea)
        return NS_OK;

    mDrawingarea->scroll(aDx, aDy);

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
        if (!mDrawingarea)
            return nsnull;

        return mDrawingarea;
        break;
    }

    case NS_NATIVE_PLUGIN_PORT:
        return SetupPluginPort();
        break;

    case NS_NATIVE_DISPLAY:
        return mDrawingarea->x11Info().display();
        break;

    case NS_NATIVE_GRAPHIC: {
        NS_ASSERTION(nsnull != mToolkit, "NULL toolkit, unable to get a GC");
        return (void *)static_cast<nsToolkit *>(mToolkit)->GetSharedGC();
        break;
    }

    case NS_NATIVE_SHELLWIDGET:
        return (void *) mDrawingarea;

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
    if (!mDrawingarea)
        return NS_OK;

    nsAString::const_iterator it;
    QString qStr((QChar*)aTitle.BeginReading(it).get(), -1);

    if (mDrawingarea)
        mDrawingarea->setWindowTitle(qStr);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetIcon(const nsAString& aIconSpec)
{
    if (!mDrawingarea)
        return NS_OK;

    nsCOMPtr<nsILocalFile> iconFile;
    nsCAutoString path;
    nsCStringArray iconList;

    // Look for icons with the following suffixes appended to the base name.
    // The last two entries (for the old XPM format) will be ignored unless
    // no icons are found using the other suffixes. XPM icons are depricated.

    const char extensions[6][7] = { ".png", "16.png", "32.png", "48.png",
                                    ".xpm", "16.xpm" };

    for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(extensions); i++) {
        // Don't bother looking for XPM versions if we found a PNG.
        if (i == NS_ARRAY_LENGTH(extensions) - 2 && iconList.Count())
            break;

        nsAutoString extension;
        extension.AppendASCII(extensions[i]);

        ResolveIconName(aIconSpec, extension, getter_AddRefs(iconFile));
        if (iconFile) {
            iconFile->GetNativePath(path);
            iconList.AppendCString(path);
        }
    }

    // leave the default icon intact if no matching icons were found
    if (iconList.Count() == 0)
        return NS_OK;

    return SetWindowIconList(iconList);
}

NS_IMETHODIMP
nsWindow::SetMenuBar(nsIMenuBar * aMenuBar)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::ShowMenuBar(PRBool aShow)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
    NS_ENSURE_TRUE(mDrawingarea, NS_OK);

    PRInt32 X,Y;

    QPoint offset(0,0);
    offset = mDrawingarea->mapFromGlobal(offset);
    X = offset.x();
    Y = offset.y();
    LOG(("WidgetToScreen (container) %d %d\n", X, Y));

    aNewRect.x = aOldRect.x + X;
    aNewRect.y = aOldRect.y + Y;
    aNewRect.width = aOldRect.width;
    aNewRect.height = aOldRect.height;

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
    NS_ENSURE_TRUE(mDrawingarea, NS_OK);

    PRInt32 X,Y;

    QPoint offset(0,0);
    offset = mDrawingarea->mapFromGlobal(offset);
    X = offset.x();
    Y = offset.y();
    LOG(("WidgetToScreen (container) %d %d\n", X, Y));

    aNewRect.x = aOldRect.x - X;
    aNewRect.y = aOldRect.y - Y;
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

    if (!mDrawingarea)
        return NS_OK;

/*
    if (aCapture) {
        GrabPointer();
    }
    else {
        ReleaseGrabs();
    }
*/

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::CaptureRollupEvents(nsIRollupListener *aListener,
                              PRBool             aDoCapture,
                              PRBool             aConsumeRollupEvent)
{
    if (!mDrawingarea)
        return NS_OK;

    LOG(("CaptureRollupEvents %p\n", (void *)this));
/*
    if (aDoCapture) {
        GrabPointer();
        GrabKeyboard();
    }
    else {
        ReleaseGrabs();
    }
*/

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetAttention(PRInt32 aCycleCount)
{
    LOG(("nsWindow::GetAttention [%p]\n", (void *)this));

    SetUrgencyHint(mDrawingarea, PR_TRUE);

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

bool
nsWindow::OnExposeEvent(QPaintEvent *aEvent)
{
    if (gDoubleBuffering == -1) {
        if (getenv("MOZ_NO_DOUBLEBUFFER"))
            gDoubleBuffering = 0;
        else
            gDoubleBuffering = 1;
    }

    if (mIsDestroyed) {
        LOG(("Expose event on destroyed window [%p] window %p\n",
             (void *)this, mDrawingarea));
        return FALSE;
    }

    if (!mDrawingarea)
        return FALSE;

    static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);

    nsCOMPtr<nsIRegion> updateRegion = do_CreateInstance(kRegionCID);
    if (!updateRegion)
        return FALSE;

    updateRegion->Init();

    QVector<QRect>  rects = aEvent->region().rects();

    LOGDRAW(("sending expose event [%p] %p 0x%lx (rects follow):\n",
             (void *)this, (void *)aEvent, 0));

    for (int i = 0; i < rects.size(); ++i) {
       QRect r = rects.at(i);
       updateRegion->Union(r.x(), r.y(), r.width(), r.height());
       LOGDRAW(("\t%d %d %d %d\n", r.x(), r.y(), r.width(), r.height()));
    }

    QPainter painter(mDrawingarea);

    nsRefPtr<gfxQPainterSurface> targetSurface = new gfxQPainterSurface(&painter);
    nsRefPtr<gfxContext> ctx = new gfxContext(targetSurface);

    nsCOMPtr<nsIRenderingContext> rc;
    GetDeviceContext()->CreateRenderingContextInstance(*getter_AddRefs(rc));
    if (NS_UNLIKELY(!rc))
        return FALSE;

    rc->Init(GetDeviceContext(), ctx);

    PRBool translucent;
    GetHasTransparentBackground(translucent);
    nsIntRect boundsRect;

    updateRegion->GetBoundingBox(&boundsRect.x, &boundsRect.y,
                                 &boundsRect.width, &boundsRect.height);

    // do double-buffering and clipping here
    ctx->Save();
    ctx->NewPath();
    if (translucent) {
        // Collapse update area to the bounding box. This is so we only have to
        // call UpdateTranslucentWindowAlpha once. After we have dropped
        // support for non-Thebes graphics, UpdateTranslucentWindowAlpha will be
        // our private interface so we can rework things to avoid this.
        ctx->Rectangle(gfxRect(boundsRect.x, boundsRect.y,
                               boundsRect.width, boundsRect.height));
    } else {
        for (int i = 0; i < rects.size(); ++i) {
           QRect r = rects.at(i);
           ctx->Rectangle(gfxRect(r.x(), r.y(), r.width(), r.height()));
        }
    }
    ctx->Clip();

    // double buffer
    if (translucent) {
        ctx->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
    } else if (gDoubleBuffering) {
        ctx->PushGroup(gfxASurface::CONTENT_COLOR);
    }

#if 0
    // NOTE: Paint flashing region would be wrong for cairo, since
    // cairo inflates the update region, etc.  So don't paint flash
    // for cairo.
#ifdef DEBUG
    if (WANT_PAINT_FLASHING && aEvent->window)
        gdk_window_flash(aEvent->window, 1, 100, aEvent->region);
#endif
#endif

    nsPaintEvent event(PR_TRUE, NS_PAINT, this);
    QRect r = aEvent->rect();
    if (!r.isValid())
        r = mDrawingarea->rect();
    nsRect rect(r.x(), r.y(), r.width(), r.height());
    event.refPoint.x = aEvent->rect().x();
    event.refPoint.y = aEvent->rect().y();
    event.rect = &rect; // was null FIXME
    event.region = updateRegion;
    event.renderingContext = rc;

    nsEventStatus status;
    DispatchEvent(&event, status);

    // DispatchEvent can Destroy us (bug 378273), avoid doing any paint
    // operations below if that happened - it will lead to XError and exit().
    if (NS_UNLIKELY(mIsDestroyed))
        return ignoreEvent(status);

    if (status == nsEventStatus_eIgnore) {
        ctx->Restore();
        return ignoreEvent(status);
    }

    if (translucent) {
        nsRefPtr<gfxPattern> pattern = ctx->PopGroup();
        ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
        ctx->SetPattern(pattern);
        ctx->Paint();

        nsRefPtr<gfxImageSurface> img =
            new gfxImageSurface(gfxIntSize(boundsRect.width, boundsRect.height),
                                gfxImageSurface::ImageFormatA8);
        if (img && !img->CairoStatus()) {
            img->SetDeviceOffset(gfxPoint(-boundsRect.x, -boundsRect.y));

            nsRefPtr<gfxContext> imgCtx = new gfxContext(img);
            if (imgCtx) {
                imgCtx->SetPattern(pattern);
                imgCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
                imgCtx->Paint();
            }

            UpdateTranslucentWindowAlphaInternal(nsRect(boundsRect.x, boundsRect.y,
                                                        boundsRect.width, boundsRect.height),
                                                 img->Data(), img->Stride());
        }
    } else if (gDoubleBuffering) {
        ctx->PopGroupToSource();
        ctx->Paint();
    }

    ctx->Restore();

    // check the return value!
    return ignoreEvent(status);
}

bool
nsWindow::OnConfigureEvent(QMoveEvent *aEvent)
{
    LOG(("configure event [%p] %d %d\n", (void *)this,
        aEvent->pos().x(),  aEvent->pos().y()));

    // can we shortcut?
    if (!mDrawingarea
        || (mBounds.x == aEvent->pos().x()
        && mBounds.y == aEvent->pos().y()))
    return FALSE;

    // Toplevel windows need to have their bounds set so that we can
    // keep track of our location.  It's not often that the x,y is set
    // by the layout engine.  Width and height are set elsewhere.
    QPoint pos = aEvent->pos();
    if (mIsTopLevel) {
        mPlaced = PR_TRUE;
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
    nsEventStatus status;
    DispatchEvent(&event, status);

    return ignoreEvent(status);
}

bool
nsWindow::OnSizeAllocate(QResizeEvent *e)
{
    nsRect rect;

    // Generate XPFE resize event
    GetBounds(rect);

    rect.width = e->size().width();
    rect.height = e->size().height();

    LOG(("size_allocate [%p] %d %d\n",
         (void *)this, rect.width, rect.height));

    ResizeTransparencyBitmap(rect.width, rect.height);

    mBounds.width = rect.width;
    mBounds.height = rect.height;

#ifdef DEBUG_WIDGETS
    qDebug("resizeEvent: mDrawingarea=%p, aWidth=%d, aHeight=%d, aX = %d, aY = %d", (void*)mDrawingarea,
           rect.width, rect.height, rect.x, rect.y);
#endif

    if (mTransparencyBitmap) {
      ApplyTransparencyBitmap();
    }

    if (mDrawingarea)
        mDrawingarea->resize(rect.width, rect.height);

    nsEventStatus status;
    DispatchResizeEvent(rect, status);
    return ignoreEvent(status);
}

bool
nsWindow::OnDeleteEvent(QCloseEvent *aEvent)
{
    nsGUIEvent event(PR_TRUE, NS_XUL_CLOSE, this);

    event.refPoint.x = 0;
    event.refPoint.y = 0;

    nsEventStatus status;
    DispatchEvent(&event, status);
    return ignoreEvent(status);
}

bool
nsWindow::OnEnterNotifyEvent(QEvent *aEvent)
{
    nsMouseEvent event(PR_TRUE, NS_MOUSE_ENTER, this, nsMouseEvent::eReal);

    QPoint pt = QCursor::pos();

    event.refPoint.x = nscoord(pt.x());
    event.refPoint.y = nscoord(pt.y());

    LOG(("OnEnterNotify: %p\n", (void *)this));

    nsEventStatus status;
    DispatchEvent(&event, status);
    return FALSE;
}

bool
nsWindow::OnLeaveNotifyEvent(QEvent *aEvent)
{
    nsMouseEvent event(PR_TRUE, NS_MOUSE_EXIT, this, nsMouseEvent::eReal);

    QPoint pt = QCursor::pos();

    event.refPoint.x = nscoord(pt.x());
    event.refPoint.y = nscoord(pt.y());

    LOG(("OnLeaveNotify: %p\n", (void *)this));

    nsEventStatus status;
    DispatchEvent(&event, status);
    return FALSE;
}

bool
nsWindow::OnMotionNotifyEvent(QMouseEvent *aEvent)
{
    // when we receive this, it must be that the gtk dragging is over,
    // it is dropped either in or out of mozilla, clear the flag
    //mDrawingarea->setCursor(mQCursor);

    nsMouseEvent event(PR_TRUE, NS_MOUSE_MOVE, this, nsMouseEvent::eReal);


    event.refPoint.x = nscoord(aEvent->x());
    event.refPoint.y = nscoord(aEvent->y());

    event.isShift         = aEvent->modifiers() & Qt::ShiftModifier;
    event.isControl       = aEvent->modifiers() & Qt::ControlModifier;
    event.isAlt           = aEvent->modifiers() & Qt::AltModifier;
    event.isMeta          = aEvent->modifiers() & Qt::MetaModifier;
    event.clickCount      = 0;

    nsEventStatus status;
    DispatchEvent(&event, status);
    return ignoreEvent(status);
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

bool
nsWindow::OnButtonPressEvent(QMouseEvent *aEvent)
{
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

    nsEventStatus status;
    DispatchEvent(&event, status);

    // right menu click on linux should also pop up a context menu
    if (domButton == nsMouseEvent::eRightButton &&
        NS_LIKELY(!mIsDestroyed)) {
        nsMouseEvent contextMenuEvent(PR_TRUE, NS_CONTEXTMENU, this,
                                      nsMouseEvent::eReal);
        InitButtonEvent(contextMenuEvent, aEvent, 1);
        DispatchEvent(&contextMenuEvent, status);
    }

    return ignoreEvent(status);
}

bool
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

    nsMouseEvent event(PR_TRUE, NS_MOUSE_BUTTON_UP, this, nsMouseEvent::eReal);
    event.button = domButton;
    InitButtonEvent(event, aEvent, 1);

    nsEventStatus status;
    DispatchEvent(&event, status);
    return ignoreEvent(status);
}

bool
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
    nsEventStatus status;
    DispatchEvent(&event, status);
    return ignoreEvent(status);
}

bool
nsWindow::OnContainerFocusInEvent(QFocusEvent *aEvent)
{
    LOGFOCUS(("OnContainerFocusInEvent [%p]\n", (void *)this));
    // Return if someone has blocked events for this widget.  This will
    // happen if someone has called gtk_widget_grab_focus() from
    // nsWindow::SetFocus() and will prevent recursion.

    if (!mDrawingarea)
        return FALSE;

    // Unset the urgency hint, if possible
//    SetUrgencyHint(top_window, PR_FALSE);

    // dispatch a got focus event
    DispatchGotFocusEvent();

    // send the activate event if it wasn't already sent via any
    // SetFocus() calls that were the result of the GOTFOCUS event
    // above.
    DispatchActivateEvent();

    LOGFOCUS(("Events sent from focus in event [%p]\n", (void *)this));
    return FALSE;
}

bool
nsWindow::OnContainerFocusOutEvent(QFocusEvent *aEvent)
{
    LOGFOCUS(("OnContainerFocusOutEvent [%p]\n", (void *)this));

    DispatchLostFocusEvent();
    if (mDrawingarea)
        DispatchDeactivateEvent();

    LOGFOCUS(("Done with container focus out [%p]\n", (void *)this));
    return FALSE;
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
    nsEventStatus status;
    nsCommandEvent event(PR_TRUE, nsWidgetAtoms::onAppCommand, aCommand, this);
    DispatchEvent(&event, status);
    return TRUE;
}

bool
nsWindow::OnKeyPressEvent(QKeyEvent *aEvent)
{
    LOGFOCUS(("OnKeyPressEvent [%p]\n", (void *)this));

    nsEventStatus status;

    nsKeyEvent event(PR_TRUE, NS_KEY_PRESS, this);
    InitKeyEvent(event, aEvent);
    event.charCode = (PRInt32)aEvent->text()[0].unicode();
    // qDebug("FIXME:>>>>>>Func:%s::%d, %i\n", __PRETTY_FUNCTION__, __LINE__, event.charCode);

    if (!aEvent->isAutoRepeat()) {
        // send the key down event
        nsKeyEvent downEvent(PR_TRUE, NS_KEY_DOWN, this);
        InitKeyEvent(downEvent, aEvent);
        DispatchEvent(&downEvent, status);
        if (ignoreEvent(status)) { // If prevent default on keydown, do same for keypress
            event.flags |= NS_EVENT_FLAG_NO_DEFAULT;
        }
    }

    // before we dispatch a key, check if it's the context menu key.
    // If so, send a context menu key event instead.
    if (isContextMenuKey(event)) {
        nsMouseEvent contextMenuEvent(PR_TRUE, NS_CONTEXTMENU, this,
                                      nsMouseEvent::eReal,
                                      nsMouseEvent::eContextMenuKey);
        keyEventToContextMenuEvent(&event, &contextMenuEvent);
        DispatchEvent(&contextMenuEvent, status);
    }
    else {
        // send the key press event
        DispatchEvent(&event, status);
    }


    // If the event was consumed, return.
    LOGIM(("status %d\n", status));
    if (status == nsEventStatus_eConsumeNoDefault) {
        LOGIM(("key press consumed\n"));
        return TRUE;
    }

    return FALSE;
}

bool
nsWindow::OnKeyReleaseEvent(QKeyEvent *aEvent)
{
    LOGFOCUS(("OnKeyReleaseEvent [%p]\n", (void *)this));

    // send the key event as a key up event
    nsKeyEvent event(PR_TRUE, NS_KEY_UP, this);
    InitKeyEvent(event, aEvent);

    // unset the key down flag
    ClearKeyDownFlag(event.keyCode);

    nsEventStatus status;
    DispatchEvent(&event, status);

    // If the event was consumed, return.
    if (status == nsEventStatus_eConsumeNoDefault) {
        LOGIM(("key release consumed\n"));
        return TRUE;
    }

    return FALSE;
}

bool
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
    event.delta = (int)((aEvent->delta() / WHEEL_DELTA) * -3);

    event.refPoint.x = nscoord(aEvent->x());
    event.refPoint.y = nscoord(aEvent->y());

    event.isShift         = aEvent->modifiers() & Qt::ShiftModifier;
    event.isControl       = aEvent->modifiers() & Qt::ControlModifier;
    event.isAlt           = aEvent->modifiers() & Qt::AltModifier;
    event.isMeta          = aEvent->modifiers() & Qt::MetaModifier;
    event.time            = 0;

    nsEventStatus status;
    DispatchEvent(&event, status);
    return ignoreEvent(status);
}


bool
nsWindow::showEvent(QShowEvent *)
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__,(void *)this));
    // qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
/*
    QRect r = mDrawingarea->rect();
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

    nsEventStatus status;
    DispatchEvent(&event, status);
*/
    mIsVisible = PR_TRUE;
    return false;
}

bool
nsWindow::hideEvent(QHideEvent *)
{
    LOG(("%s [%p]\n", __PRETTY_FUNCTION__,(void *)this));
    mIsVisible = PR_FALSE;
    return false;
}

bool
nsWindow::OnWindowStateEvent(QEvent *aEvent)
{
    qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
    nsSizeModeEvent event(PR_TRUE, NS_SIZEMODE, this);
    nsEventStatus status;
    DispatchEvent(&event, status);
    return ignoreEvent(status);
}

void
nsWindow::ThemeChanged()
{
    nsGUIEvent event(PR_TRUE, NS_THEMECHANGED, this);
    nsEventStatus status = nsEventStatus_eIgnore;
    DispatchEvent(&event, status);

    if (!mDrawingarea || NS_UNLIKELY(mIsDestroyed))
        return;
    qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
    return;
}

bool
nsWindow::OnDragMotionEvent(QDragMoveEvent *e)
{
    LOG(("nsWindow::OnDragMotionSignal\n"));

    nsMouseEvent event(PR_TRUE, NS_DRAGDROP_OVER, 0,
                       nsMouseEvent::eReal);
    return TRUE;
}

bool
nsWindow::OnDragLeaveEvent(QDragLeaveEvent *e)
{
    // XXX Do we want to pass this on only if the event's subwindow is null?
    LOG(("nsWindow::OnDragLeaveSignal(%p)\n", this));
    nsMouseEvent event(PR_TRUE, NS_DRAGDROP_EXIT, this, nsMouseEvent::eReal);

    nsEventStatus status;
    DispatchEvent(&event, status);
    return ignoreEvent(status);
}

bool
nsWindow::OnDragDropEvent(QDropEvent *e)
{
    LOG(("nsWindow::OnDragDropSignal\n"));
    nsMouseEvent event(PR_TRUE, NS_DRAGDROP_OVER, 0,
                       nsMouseEvent::eReal);
    return TRUE;
}

bool
nsWindow::OnDragEnter(QDragEnterEvent *)
{
    // XXX Do we want to pass this on only if the event's subwindow is null?

    LOG(("nsWindow::OnDragEnter(%p)\n", this));

    nsMouseEvent event(PR_TRUE, NS_DRAGDROP_ENTER, this, nsMouseEvent::eReal);
    nsEventStatus status;
    DispatchEvent(&event, status);
    return ignoreEvent(status);
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

    // Do we need to listen for resizes?
    PRBool listenForResizes = PR_FALSE;;
    if (aNativeParent || (aInitData && aInitData->mListenForResizes))
        listenForResizes = PR_TRUE;

    // and do our common creation
    CommonCreate(aParent, listenForResizes);

    // save our bounds
    mBounds = aRect;
    if (mWindowType != eWindowType_child) {
        // The window manager might place us. Indicate that if we're
        // shown, we want to go through
        // nsWindow::NativeResize(x,y,w,h) to maybe set our own
        // position.
        mNeedsMove = PR_TRUE;
    }

    // figure out our parent window
    QWidget      *parent = nsnull;
    if (aParent != nsnull)
        parent = (QWidget*)aParent->GetNativeData(NS_NATIVE_WIDGET);
    else
        parent = (QWidget*)aNativeParent;

    // ok, create our windows
    mDrawingarea = createQWidget(parent, aInitData);

    Initialize(mDrawingarea);

    LOG(("nsWindow [%p]\n", (void *)this));
    if (mDrawingarea) {
        LOG(("\tmDrawingarea %p %p %p %lx %lx\n", (void *)mDrawingarea));
    }

    // resize so that everything is set to the right dimensions
    if (!mIsTopLevel)
        Resize(mBounds.x, mBounds.y, mBounds.width, mBounds.height, PR_FALSE);

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetWindowClass(const nsAString &xulWinType)
{
  if (!mDrawingarea)
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

  // gdk_window_set_role(GTK_WIDGET(mDrawingarea)->window, role);
  qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
  // Can't use gtk_window_set_wmclass() for this; it prints
  // a warning & refuses to make the change.
  XSetClassHint(mDrawingarea->x11Info().display(),
                mDrawingarea->handle(),
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

    ResizeTransparencyBitmap(aWidth, aHeight);

    // clear our resize flag
    mNeedsResize = PR_FALSE;

    mDrawingarea->resize( aWidth, aHeight);

    if (aRepaint) {
        if (mDrawingarea->isVisible())
            mDrawingarea->repaint();
    }
}

void
nsWindow::NativeResize(PRInt32 aX, PRInt32 aY,
                       PRInt32 aWidth, PRInt32 aHeight,
                       PRBool  aRepaint)
{
    mNeedsResize = PR_FALSE;
    mNeedsMove = PR_FALSE;

    LOG(("nsWindow::NativeResize [%p] %d %d %d %d\n", (void *)this,
         aX, aY, aWidth, aHeight));

    ResizeTransparencyBitmap(aWidth, aHeight);

    QPoint pos(aX, aY);
    if (mDrawingarea)
    {
        if (mParent && mDrawingarea->windowType() == Qt::Popup) {
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
            qDebug("Widget with original position? (%p)", mDrawingarea);
#endif
        }
    }

    mDrawingarea->setGeometry(pos.x(), pos.y(), aWidth, aHeight);

    if (aRepaint) {
        if (mDrawingarea->isVisible())
            mDrawingarea->repaint();
    }
}

void
nsWindow::NativeShow (PRBool  aAction)
{
    if (aAction) {
        // GTK wants us to set the window mask before we show the window
        // for the first time, or setting the mask later won't work.
        // GTK also wants us to NOT set the window mask if we're not really
        // going to need it, because GTK won't let us unset the mask properly
        // later.
        // So, we delay setting the mask until the last moment: when the window
        // is shown.
        // XXX that may or may not be true for GTK+ 2.x
        if (mTransparencyBitmap) {
            ApplyTransparencyBitmap();
        }

        // unset our flag now that our window has been shown
        mNeedsShow = PR_FALSE;
    }
    if (!mDrawingarea) {
        //XXX: apperently can be null during the printing, check whether
        //     that's true
        qDebug("nsCommon::Show : widget empty");
        return;
    }
    mDrawingarea->setShown(aAction);
}

void
nsWindow::EnsureGrabs(void)
{
    if (mRetryPointerGrab)
        GrabPointer();
    if (mRetryKeyboardGrab)
        GrabKeyboard();
}

NS_IMETHODIMP
nsWindow::SetHasTransparentBackground(PRBool aTransparent)
{
//    if (!mDrawingarea) {
        // Pass the request to the toplevel window
//        return topWindow->SetHasTransparentBackground(aTransparent);
//    }

    if (mIsTransparent == aTransparent)
        return NS_OK;

    if (!aTransparent) {
        if (mTransparencyBitmap) {
            delete[] mTransparencyBitmap;
            mTransparencyBitmap = nsnull;
            mTransparencyBitmapWidth = 0;
            mTransparencyBitmapHeight = 0;
            // gtk_widget_reset_shapes(mDrawingarea);
        }
    } // else the new default alpha values are "all 1", so we don't
    // need to change anything yet

    mIsTransparent = aTransparent;
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetHasTransparentBackground(PRBool& aTransparent)
{
    if (!mDrawingarea) {
        // Pass the request to the toplevel window
//        QWidget *topWidget = nsnull;
//        GetToplevelWidget(&topWidget);
//        if (!topWidget) {
//            aTransparent = PR_FALSE;
//            return NS_ERROR_FAILURE;
//        }

//        if (!topWindow) {
//            aTransparent = PR_FALSE;
//            return NS_ERROR_FAILURE;
//        }

//        return topWindow->GetHasTransparentBackground(aTransparent);
    }

    aTransparent = mIsTransparent;
    return NS_OK;
}

void
nsWindow::ResizeTransparencyBitmap(PRInt32 aNewWidth, PRInt32 aNewHeight)
{
    if (!mTransparencyBitmap)
        return;

    if (aNewWidth == mTransparencyBitmapWidth &&
        aNewHeight == mTransparencyBitmapHeight)
        return;

    PRInt32 newSize = ((aNewWidth+7)/8)*aNewHeight;
    char* newBits = new char[newSize];
    if (!newBits) {
        delete[] mTransparencyBitmap;
        mTransparencyBitmap = nsnull;
        mTransparencyBitmapWidth = 0;
        mTransparencyBitmapHeight = 0;
        return;
    }
    // fill new mask with "opaque", first
    memset(newBits, 255, newSize);

    // Now copy the intersection of the old and new areas into the new mask
    PRInt32 copyWidth = PR_MIN(aNewWidth, mTransparencyBitmapWidth);
    PRInt32 copyHeight = PR_MIN(aNewHeight, mTransparencyBitmapHeight);
    PRInt32 oldRowBytes = (mTransparencyBitmapWidth+7)/8;
    PRInt32 newRowBytes = (aNewWidth+7)/8;
    PRInt32 copyBytes = (copyWidth+7)/8;

    PRInt32 i;
    char* fromPtr = mTransparencyBitmap;
    char* toPtr = newBits;
    for (i = 0; i < copyHeight; i++) {
        memcpy(toPtr, fromPtr, copyBytes);
        fromPtr += oldRowBytes;
        toPtr += newRowBytes;
    }

    delete[] mTransparencyBitmap;
    mTransparencyBitmap = newBits;
    mTransparencyBitmapWidth = aNewWidth;
    mTransparencyBitmapHeight = aNewHeight;
}

static PRBool
ChangedMaskBits(char* aMaskBits, PRInt32 aMaskWidth, PRInt32 aMaskHeight,
        const nsRect& aRect, PRUint8* aAlphas, PRInt32 aStride)
{
    PRInt32 x, y, xMax = aRect.XMost(), yMax = aRect.YMost();
    PRInt32 maskBytesPerRow = (aMaskWidth + 7)/8;
    for (y = aRect.y; y < yMax; y++) {
        char* maskBytes = aMaskBits + y*maskBytesPerRow;
        PRUint8* alphas = aAlphas;
        for (x = aRect.x; x < xMax; x++) {
            PRBool newBit = *alphas > 0;
            alphas++;

            char maskByte = maskBytes[x >> 3];
            PRBool maskBit = (maskByte & (1 << (x & 7))) != 0;

            if (maskBit != newBit) {
                return PR_TRUE;
            }
        }
        aAlphas += aStride;
    }

    return PR_FALSE;
}

static
void UpdateMaskBits(char* aMaskBits, PRInt32 aMaskWidth, PRInt32 aMaskHeight,
        const nsRect& aRect, PRUint8* aAlphas, PRInt32 aStride)
{
    PRInt32 x, y, xMax = aRect.XMost(), yMax = aRect.YMost();
    PRInt32 maskBytesPerRow = (aMaskWidth + 7)/8;
    for (y = aRect.y; y < yMax; y++) {
        char* maskBytes = aMaskBits + y*maskBytesPerRow;
        PRUint8* alphas = aAlphas;
        for (x = aRect.x; x < xMax; x++) {
            PRBool newBit = *alphas > 0;
            alphas++;

            char mask = 1 << (x & 7);
            char maskByte = maskBytes[x >> 3];
            // Note: '-newBit' turns 0 into 00...00 and 1 into 11...11
            maskBytes[x >> 3] = (maskByte & ~mask) | (-newBit & mask);
        }
        aAlphas += aStride;
    }
}

void
nsWindow::ApplyTransparencyBitmap()
{
    qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);
/*
    gtk_widget_reset_shapes(mDrawingarea);
    GdkBitmap* maskBitmap = gdk_bitmap_create_from_data(mDrawingarea->window,
            mTransparencyBitmap,
            mTransparencyBitmapWidth, mTransparencyBitmapHeight);
    if (!maskBitmap)
        return;

    gtk_widget_shape_combine_mask(mDrawingarea, maskBitmap, 0, 0);
    gdk_bitmap_unref(maskBitmap);
*/
}

nsresult
nsWindow::UpdateTranslucentWindowAlphaInternal(const nsRect& aRect,
                                               PRUint8* aAlphas, PRInt32 aStride)
{
    if (!mDrawingarea) {
        // Pass the request to the toplevel window
//        return topWindow->UpdateTranslucentWindowAlphaInternal(aRect, aAlphas, aStride);
        return NS_ERROR_FAILURE;
    }

    NS_ASSERTION(mIsTransparent, "Window is not transparent");

    if (mTransparencyBitmap == nsnull) {
        PRInt32 size = ((mBounds.width+7)/8)*mBounds.height;
        mTransparencyBitmap = new char[size];
        if (mTransparencyBitmap == nsnull)
            return NS_ERROR_FAILURE;
        memset(mTransparencyBitmap, 255, size);
        mTransparencyBitmapWidth = mBounds.width;
        mTransparencyBitmapHeight = mBounds.height;
    }

    NS_ASSERTION(aRect.x >= 0 && aRect.y >= 0
            && aRect.XMost() <= mBounds.width && aRect.YMost() <= mBounds.height,
            "Rect is out of window bounds");

    if (!ChangedMaskBits(mTransparencyBitmap, mBounds.width, mBounds.height,
                         aRect, aAlphas, aStride))
        // skip the expensive stuff if the mask bits haven't changed; hopefully
        // this is the common case
        return NS_OK;

    UpdateMaskBits(mTransparencyBitmap, mBounds.width, mBounds.height,
                   aRect, aAlphas, aStride);

    if (!mNeedsShow) {
        ApplyTransparencyBitmap();
    }
    return NS_OK;
}

void
nsWindow::GrabPointer(void)
{
    LOG(("GrabPointer %d\n", mRetryPointerGrab));

    mRetryPointerGrab = PR_FALSE;

    // If the window isn't visible, just set the flag to retry the
    // grab.  When this window becomes visible, the grab will be
    // retried.
    PRBool visibility = PR_TRUE;
    IsVisible(visibility);
    if (!visibility) {
        LOG(("GrabPointer: window not visible\n"));
        mRetryPointerGrab = PR_TRUE;
        return;
    }

    if (!mDrawingarea)
        return;

    mDrawingarea->grabMouse();
}

void
nsWindow::GrabKeyboard(void)
{
    LOG(("GrabKeyboard %d\n", mRetryKeyboardGrab));

    mRetryKeyboardGrab = PR_FALSE;

    // If the window isn't visible, just set the flag to retry the
    // grab.  When this window becomes visible, the grab will be
    // retried.
    PRBool visibility = PR_TRUE;
    IsVisible(visibility);
    if (!visibility) {
        LOG(("GrabKeyboard: window not visible\n"));
        mRetryKeyboardGrab = PR_TRUE;
        return;
    }

    if (!mDrawingarea)
        return;

    mDrawingarea->grabKeyboard();
}

void
nsWindow::ReleaseGrabs(void)
{
    LOG(("ReleaseGrabs\n"));

    mRetryPointerGrab = PR_FALSE;
    mRetryKeyboardGrab = PR_FALSE;

//    gdk_pointer_ungrab(Qt::Key_CURRENT_TIME);
//    gdk_keyboard_ungrab(Qt::Key_CURRENT_TIME);
}

void
nsWindow::GetToplevelWidget(QWidget **aWidget)
{
    *aWidget = nsnull;

    if (mDrawingarea) {
        *aWidget = mDrawingarea;
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
    if (!mDrawingarea)
        return nsnull;

    qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);

/*
    // we have to flush the X queue here so that any plugins that
    // might be running on separate X connections will be able to use
    // this window in case it was just created
    XWindowAttributes xattrs;
    XGetWindowAttributes(Qt::Key_DISPLAY (),
                         Qt::Key_WINDOW_XWINDOW(mDrawingarea->inner_window),
                         &xattrs);
    XSelectInput (Qt::Key_DISPLAY (),
                  Qt::Key_WINDOW_XWINDOW(mDrawingarea->inner_window),
                  xattrs.your_event_mask |
                  SubstructureNotifyMask);

    gdk_window_add_filter(mDrawingarea->inner_window,
                          plugin_window_filter_func,
                          this);

    XSync(Qt::Key_DISPLAY(), False);

    return (void *)Qt::Key_WINDOW_XWINDOW(mDrawingarea->inner_window);
*/
    return nsnull;
}

nsresult
nsWindow::SetWindowIconList(const nsCStringArray &aIconList)
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
        gdk_window_fullscreen (mDrawingarea->window);
    else
        gdk_window_unfullscreen (mDrawingarea->window);
    return NS_OK;
#else
*/
    return nsBaseWidget::MakeFullScreen(aFullScreen);
//#endif
}

NS_IMETHODIMP
nsWindow::HideWindowChrome(PRBool aShouldHide)
{
    if (!mDrawingarea) {
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
    if (mDrawingarea->isVisible()) {
        mDrawingarea->hide();
        wasVisible = PR_TRUE;
    }

    qint32 wmd;
    if (aShouldHide)
        wmd = 0;
    else
        wmd = ConvertBorderStyles(mBorderStyle);

//    gdk_window_set_decorations(mDrawingarea->window, (GdkWMDecoration) wmd);

    if (wasVisible) {
        mDrawingarea->show();
    }

    // For some window managers, adding or removing window decorations
    // requires unmapping and remapping our toplevel window.  Go ahead
    // and flush the queue here so that we don't end up with a BadWindow
    // error later when this happens (when the persistence timer fires
    // and GetWindowPos is called)
    XSync(mDrawingarea->x11Info().display(), False);

    return NS_OK;
}

/* static */
/*
PRBool
is_mouse_in_window (QWidget* aWindow, double aMouseX, double aMouseY)
{
    qint32 x = 0;
    qint32 y = 0;
    qint32 w, h;

    qint32 offsetX = 0;
    qint32 offsetY = 0;

    QWidget *window;

    window = aWindow;

    while (window) {
        qint32 tmpX = window->pos().x();
        qint32 tmpY = window->pos().y();

        // if this is a window, compute x and y given its origin and our
        // offset
        x = tmpX + offsetX;
        y = tmpY + offsetY;
        break;

        offsetX += tmpX;
        offsetY += tmpY;
    }

    w = window->size().width();
    h = window->size().height();

    if (aMouseX > x && aMouseX < x + w &&
        aMouseY > y && aMouseY < y + h)
        return PR_TRUE;

    return PR_FALSE;
}
*/

/* static */
/*
nsWindow *
get_window_for_qt_widget(QWidget *widget)
{
    MozQWidget *mozWidget = static_cast<MozQWidget*>(widget);
    return mozWidget->getReciever();
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
#ifdef DEBUG_WIDGETS
    qDebug("NEW WIDGET\n\tparent is %p (%s)", (void*)parent,
           parent ? qPrintable(parent->objectName()) : "null");
#endif
    // ok, create our windows
    switch (mWindowType) {
    case eWindowType_dialog:
    case eWindowType_popup:
    case eWindowType_toplevel:
    case eWindowType_invisible: {
        mIsTopLevel = PR_TRUE;

        nsXPIDLString brandName;
        GetBrandName(brandName);
        NS_ConvertUTF16toUTF8 cBrand(brandName);
        if (mWindowType == eWindowType_dialog) {
            //gtk_window_set_wmclass(GTK_WINDOW(mDrawingarea), "Dialog", cBrand.get());
            //gtk_window_set_type_hint(GTK_WINDOW(mDrawingarea),
            //                         Qt::Key_WINDOW_TYPE_HINT_DIALOG);
            flags |= Qt::Dialog;
            mDrawingarea = new MozQWidget(this, parent, "topLevelDialog", flags);
            qDebug("\t\t#### dialog (%p)", (void*)mDrawingarea);
            //SetDefaultIcon();
        }
        else if (mWindowType == eWindowType_popup) {
            flags |= Qt::Popup;
            // gtk_window_set_wmclass(GTK_WINDOW(mDrawingarea), "Toplevel", cBrand.get());
            //gtk_window_set_decorated(GTK_WINDOW(mDrawingarea), FALSE);
            mDrawingarea = new MozQWidget(this, parent, "topLevelPopup", flags);
            qDebug("\t\t#### popup (%p)", (void*)mDrawingarea);
            mDrawingarea->setFocusPolicy(Qt::WheelFocus);
        }
        else { // must be eWindowType_toplevel
            flags |= Qt::Window;
            mDrawingarea = new MozQWidget(this, parent, "topLevelWindow", flags);
            qDebug("\t\t#### toplevel (%p)", (void*)mDrawingarea);
            //SetDefaultIcon();
        }
        if (mWindowType == eWindowType_popup) {
            // gdk does not automatically set the cursor for "temporary"
            // windows, which are what gtk uses for popups.
            mCursor = eCursor_wait; // force SetCursor to actually set the
                                    // cursor, even though our internal state
                                    // indicates that we already have the
                                    // standard cursor.
            SetCursor(eCursor_standard);
        }
    }
        break;
    case eWindowType_child: {
        mDrawingarea = new MozQWidget(this, parent, "paintArea", 0);
        qDebug("\t\t#### child (%p)", (void*)mDrawingarea);
    }
        break;
    default:
        break;
    }

    mDrawingarea->setAttribute(Qt::WA_StaticContents);
    mDrawingarea->setAttribute(Qt::WA_OpaquePaintEvent); // Transparent Widget Background

    // Disable the double buffer because it will make the caret crazy
    // For bug#153805 (Gtk2 double buffer makes carets misbehave)
    mDrawingarea->setAttribute(Qt::WA_NoSystemBackground);
    mDrawingarea->setAttribute(Qt::WA_PaintOnScreen);

    return mDrawingarea;
}

// return the gfxASurface for rendering to this widget
gfxASurface*
nsWindow::GetThebesSurface()
{
    // XXXvlad always create a new thebes surface for now,
    // because the old clip doesn't get cleared otherwise.
    // we should fix this at some point, and just reset
    // the clip.
    mThebesSurface = nsnull;

    if (!mThebesSurface) {
#if 0
        qint32 x_offset = 0, y_offset = 0;
        qint32 width = mDrawingarea->width(), height = mDrawingarea->height();

        // Owen Taylor says this is the right thing to do!
        width = PR_MIN(32767, width);
        height = PR_MIN(32767, height);

        mThebesSurface = new gfxXlibSurface
            (mDrawingarea->x11Info().display(),
             (Drawable)mDrawingarea->handle(),
             static_cast<Visual*>(mDrawingarea->x11Info().visual()),
             gfxIntSize(width, height));
        // if the surface creation is reporting an error, then
        // we don't have a surface to give back
        if (mThebesSurface && mThebesSurface->CairoStatus() != 0)
            mThebesSurface = nsnull;

        if (mThebesSurface) {
            mThebesSurface->SetDeviceOffset(gfxPoint(-x_offset, -y_offset));
        }
#else
        mThebesSurface = new gfxQPainterSurface(gfxIntSize(5,5));
#endif
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

    qDebug("FIXME:>>>>>>Func:%s::%d\n", __PRETTY_FUNCTION__, __LINE__);

    return NS_OK;
}

bool
nsWindow::contextMenuEvent(QContextMenuEvent *)
{
    //qDebug("context menu");
    return false;
}

bool
nsWindow::imStartEvent(QEvent *)
{
    qWarning("XXX imStartEvent");
    return false;
}

bool
nsWindow::imComposeEvent(QEvent *)
{
    qWarning("XXX imComposeEvent");
    return false;
}

bool
nsWindow::imEndEvent(QEvent * )
{
    qWarning("XXX imComposeEvent");
    return false;
}
