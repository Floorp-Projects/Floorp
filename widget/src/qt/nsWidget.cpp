/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsWidget.h"
#include "nsWindow.h"
#include "nsIDeviceContext.h"
#include "nsAppShell.h"
#include "nsGfxCIID.h"
#include "nsIComponentManager.h"
#include "nsIFontMetrics.h"
#include "nsQEventHandler.h"
#include "nsIRollupListener.h"
#include "nsIMenuRollup.h"

#include <qwidget.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qevent.h>
#include <qapplication.h>
#include <qobjectlist.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>

static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);
static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);

//JCG #define DBG 1

PRLogModuleInfo * QtWidgetsLM   = PR_NewLogModule("QtWidgets");

nsCOMPtr<nsIRollupListener> nsWidget::gRollupListener;
nsWeakPtr                   nsWidget::gRollupWidget;
PRBool                      nsWidget::gRollupConsumeRollupEvent = PR_FALSE;
PRUint32                    nsWidget::gWidgetCount = 0;

#ifdef DBG
PRUint32                    gQBaseWidgetCount = 0;
#endif

//=============================================================================
//
// nsQBaseWidget class
//
//=============================================================================
nsQBaseWidget::nsQBaseWidget(nsWidget * widget)
    : mWidget(widget)
{
  NS_IF_ADDREF(mWidget);
#ifdef DBG
  gQBaseWidgetCount++;
  printf("DBG: nsQBaseWidget CTOR. Count: %d\n",gQBaseWidgetCount);
#endif
}

nsQBaseWidget::~nsQBaseWidget()
{
  Destroy();
#ifdef DBG
  gQBaseWidgetCount--;
  printf("DBG: nsQBaseWidget DTOR. Count: %d\n",gQBaseWidgetCount);
#endif
}

void nsQBaseWidget::Destroy()
{
  if (mWidget) {
    if (!mWidget->IsInDTOR())
      NS_IF_RELEASE(mWidget);
    mWidget = nsnull;
  }
}

nsWidget::nsWidget()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWidget::nsWidget()\n"));

    // get the proper color from the look and feel code
    nsILookAndFeel * lookAndFeel;
    if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, 
                                                    nsnull, 
                                                    kILookAndFeelIID, 
                                                    (void**)&lookAndFeel)) 
    {
        lookAndFeel->GetColor(nsILookAndFeel::eColor_WindowBackground, 
                              mBackground);
    }
    NS_IF_RELEASE(lookAndFeel);
    mWidget          = nsnull;
    mPixmap          = nsnull;
    mPainter         = nsnull;
    mParent          = nsnull;
    mPreferredWidth  = 0;
    mPreferredHeight = 0;
    mInDTOR          = PR_FALSE;
    mShown           = PR_FALSE;
    mBounds.x        = 0;
    mBounds.y        = 0;
    mBounds.width    = 0;
    mBounds.height   = 0;
    mIsDestroying    = PR_FALSE;
    mOnDestroyCalled = PR_FALSE;
    mListenForResizes = PR_FALSE;
    mIsToplevel      = PR_FALSE;
    mEventHandler    = nsnull;
    mUpdateArea      = do_CreateInstance(kRegionCID);
    if (mUpdateArea)
    {
      mUpdateArea->Init();
      mUpdateArea->SetTo(0,0,0,0);
    }
    gWidgetCount++;
#ifdef DBG
  printf("DBG: nsWidget CTOR. Count: %d\n",gWidgetCount);
#endif
}

nsWidget::~nsWidget()
{
    mInDTOR = PR_TRUE;
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWidget::~nsWidget()\n"));

    NS_ASSERTION(mChildEventHandlers.isEmpty(),
                 "nsWidget DTOR: Child Event Handlers still active");
      
    Destroy();

    gWidgetCount--;

    if (mPainter) {
        if (mPainter->isActive())
           mPainter->end();
        delete mPainter;
        mPainter = nsnull;
    }

    if (mWidget) {
      if (mEventHandler) {
        mWidget->removeEventFilter(mEventHandler);
        delete mEventHandler;
        mEventHandler = nsnull;
      }
      if (!mIsToplevel) {
        delete mWidget;
      }
      mWidget = nsnull;
    }
    if (mPixmap) {
      delete mPixmap;
      mPixmap = nsnull;
    }
#ifdef DBG
  printf("DBG: nsWidget DTOR. Count: %d\n",gWidgetCount);
#endif
}

NS_IMPL_ISUPPORTS_INHERITED2(nsWidget, nsBaseWidget, nsIKBStateControl, nsISupportsWeakReference)

const char *nsWidget::GetName() 
{ 
  return ((mWidget != nsnull) ? mWidget->name() : ""); 
}

NS_METHOD nsWidget::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWidget::WidgetToScreen()\n"));

    if (mWidget) {
      QPoint offset(0,0);

      offset = mWidget->mapToGlobal(offset);

      aNewRect.x = aOldRect.x + offset.x();
      aNewRect.y = aOldRect.y + offset.y();
    }
    return NS_OK;
}

NS_METHOD nsWidget::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWidget::ScreenToWidget()\n"));

    QPoint offset(0,0);

    offset = mWidget->mapFromGlobal(offset);

    aNewRect.x = aOldRect.x + offset.x();
    aNewRect.y = aOldRect.y + offset.y();


    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Close this nsWidget
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWidget::Destroy(void)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWidget::Destroy()\n"));
    if (mIsDestroying)
      return NS_OK;

    mIsDestroying = PR_TRUE;
    nsBaseWidget::Destroy();
    DestroyChildren();

    if (mWidget) {
      if (mEventHandler)
        mEventHandler->Destroy();
      ((nsQWidget*)mWidget)->Destroy();
    }
    if (PR_FALSE == mOnDestroyCalled)
      OnDestroy();

    mEventCallback = nsnull;

    return NS_OK;
}

// make sure that we clean up here

void nsWidget::OnDestroy()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWidget::OnDestroy()\n"));
    mOnDestroyCalled = PR_TRUE;

    // release references to children, device context, toolkit + app shell
    nsBaseWidget::OnDestroy();

    // dispatch the event
    // dispatching of the event may cause the reference count to drop to 0
    // and result in this object being destroyed. To avoid that, add a reference
    // and then release it after dispatching the event
    nsCOMPtr<nsIWidget> kungFuDeathGrip = this; 
    DispatchStandardEvent(NS_DESTROY);
}

void nsWidget::DestroyChildren()
{
  QPtrDictIterator<nsQEventHandler> iter(mChildEventHandlers);

  while (iter.current()) {
    iter.current()->Destroy();
    ++iter;
  }
  const QObjectList *kids;
  QObject *kid = nsnull;
 
  if (mWidget) {
    kids = mWidget->children();
    if (kids) {                       // delete nsWidget children objects
          QObjectListIt it(*kids);
          while ((kid = it.current())) {
              mWidget->removeChild(kid);
              ++it;
          }
    }
  }
}

//-------------------------------------------------------------------------
//
// Get this nsWidget parent
//
//-------------------------------------------------------------------------

nsIWidget *nsWidget::GetParent(void)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWidget::GetParent()\n"));
    nsIWidget *ret;

    ret = mParent;
    NS_IF_ADDREF(ret);
    return ret;
}

//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------

NS_METHOD nsWidget::Show(PRBool bState)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::Show %s\n",
            mWidget ? mWidget->name() : "(null)"));
    if (!mWidget) {
        return NS_OK; // Will be null during printing
    }

    if (bState) {
        mWidget->show();
    }
    else {
        mWidget->hide();
    }

    mShown = bState;

    return NS_OK;
}

NS_IMETHODIMP nsWidget::CaptureRollupEvents(nsIRollupListener * aListener, 
                                            PRBool aDoCapture, 
                                            PRBool aConsumeRollupEvent)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::CaptureRollupEvents %s\n",
            mWidget ? mWidget->name() : "(null)"));
    return NS_OK;
}

NS_METHOD nsWidget::IsVisible(PRBool &aState)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::IsVisible %s\n",
            mWidget ? mWidget->name() : "(null)"));
    if (mWidget) 
    {
        aState = mWidget->isVisible();
    }
    else
    {
        aState = PR_FALSE;
    }

    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Constrain a potential move so that it remains onscreen
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::ConstrainPosition(PRInt32 *aX, PRInt32 *aY)
{
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------

NS_METHOD nsWidget::Move(PRInt32 aX, PRInt32 aY)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::Move %s by (%d,%d)\n",
            mWidget ? mWidget->name() : "(null)",
            aX,
            aY));
    mBounds.x = aX;
    mBounds.y = aY;
    if (mWidget)
    {
        mWidget->move(aX, aY);
    }

    return NS_OK;
}

NS_METHOD nsWidget::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::Resize %s (%p) to %dx%d\n",
            mWidget ? mWidget->name() : "(null)",
            this,
            aWidth,
            aHeight));

    if (mWidget && (mWidget->width() != aWidth ||
                    mWidget->height() != aHeight)) {
        mBounds.width  = aWidth;
        mBounds.height = aHeight;
     
        mWidget->resize(aWidth, aHeight);

        if (mPixmap) {
            mPixmap->resize(aWidth, aHeight);
        }
    }
    if (mListenForResizes) {
      nsSizeEvent sevent;
      sevent.message = NS_SIZE;
      sevent.widget = this;
      sevent.eventStructType = NS_SIZE_EVENT;
      sevent.windowSize = new nsRect(0, 0, aWidth, aHeight);
      sevent.point.x = 0;
      sevent.point.y = 0;
      sevent.mWinWidth = aWidth;
      sevent.mWinHeight = aHeight;
      sevent.time =  PR_IntervalNow();
      OnResize(sevent);
      delete sevent.windowSize;
    } 
    if (aRepaint) {
      if (mWidget && mWidget->isVisible()) {
        mWidget->repaint(false);
      }
    }
    return NS_OK;
}

NS_METHOD nsWidget::Resize(PRInt32 aX, 
                           PRInt32 aY, 
                           PRInt32 aWidth,
                           PRInt32 aHeight, 
                           PRBool aRepaint)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::Resize %s\n",
            mWidget ? mWidget->name() : "(null)"));
    Move(aX,aY);
    Resize(aWidth,aHeight,aRepaint);
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Send a resize message to the listener
//
//-------------------------------------------------------------------------
PRBool nsWidget::OnResize(nsSizeEvent event)
{

  mBounds.width = event.mWinWidth;
  mBounds.height = event.mWinHeight;

  return DispatchWindowEvent(&event);
}

PRBool nsWidget::OnResize(nsRect &aRect)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::OnResize %s\n",
            mWidget ? mWidget->name() : "(null)"));
    // call the event callback
  nsSizeEvent event;

  InitEvent(event, NS_SIZE);
  event.eventStructType = NS_SIZE_EVENT;

  nsRect *foo = new nsRect(0, 0, aRect.width, aRect.height);
  event.windowSize = foo;

  event.point.x = 0;
  event.point.y = 0;
  event.mWinWidth = aRect.width;
  event.mWinHeight = aRect.height;
 
  mBounds.width = aRect.width;
  mBounds.height = aRect.height;

  return DispatchWindowEvent(&event);
}

//------
// Move
//------
PRBool nsWidget::OnMove(PRInt32 aX, PRInt32 aY)
{
    nsGUIEvent event;

    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::OnMove %s\n",
            mWidget ? mWidget->name() : "(null)"));

    mBounds.x = aX;
    mBounds.y = aY; 
    InitEvent(event, NS_MOVE);
    event.point.x = aX;
    event.point.y = aY;
    event.eventStructType = NS_GUI_EVENT;
    PRBool result = DispatchWindowEvent(&event);
    return result;
}

//-------------------------------------------------------------------------
//
// Enable/disable this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::Enable(PRBool bState)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::Enable %s\n",
            mWidget ? mWidget->name() : "(null)"));

    if (mEventHandler)
    {
      mEventHandler->Enable(bState);
    }
    QPtrDictIterator<nsQEventHandler> iter(mChildEventHandlers);
    while (iter.current()) {
      iter.current()->Enable(bState);
      ++iter;
    }
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Give the focus to this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetFocus(void)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::SetFocus %s\n",
            mWidget ? mWidget->name() : "(null)"));
    if (mWidget)
    {
        mWidget->setFocus();
    }

    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this component font
//
//-------------------------------------------------------------------------
nsIFontMetrics *nsWidget::GetFont(void)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::GetFont %s\n",
            mWidget ? mWidget->name() : "(null)"));
    NS_NOTYETIMPLEMENTED("nsWidget::GetFont");
    return nsnull;
}

//-------------------------------------------------------------------------
//
// Set this component font
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetFont(const nsFont &aFont)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::SetFont %s\n",
            mWidget ? mWidget->name() : "(null)"));
    nsCOMPtr<nsIFontMetrics> fontMetrics;
    mContext->GetMetricsFor(aFont, *getter_AddRefs(fontMetrics));

    if (!fontMetrics)
       return NS_ERROR_FAILURE;

    nsFontHandle  fontHandle;
    fontMetrics->GetFontHandle(fontHandle);

    if (fontHandle) {
      QFont *font = (QFont*)fontHandle;

      if (mWidget)
        mWidget->setFont(*font);
    }
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set the background color
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetBackgroundColor(const nscolor &aColor)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::SetBackgroundColor %s\n",
            mWidget ? mWidget->name() : "(null)"));
    nsBaseWidget::SetBackgroundColor(aColor);

    QColor color(NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor));
    if (mWidget) {
        mWidget->setBackgroundColor(color);
    }
    return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set this component cursor
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetCursor(nsCursor aCursor)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::SetCursor %s(%p) to %d\n",
            mWidget ? mWidget->name() : "(null)", mWidget, aCursor));
    if (!mWidget)
    {
        return NS_ERROR_FAILURE;
    }

    // Only change cursor if it's changing
    if (aCursor != mCursor) 
    {
        PRBool  cursorSet = PR_FALSE;
        QCursor newCursor;

        switch(aCursor) 
        {
        case eCursor_select:
            newCursor = QWidget::ibeamCursor;
            cursorSet = PR_TRUE;
            break;

        case eCursor_wait:
            newCursor = QWidget::waitCursor;
            cursorSet = PR_TRUE;
            break;

        case eCursor_hyperlink:
            newCursor = QWidget::pointingHandCursor;
            cursorSet = PR_TRUE;
            break;

        case eCursor_standard:
            newCursor = QWidget::arrowCursor;
            cursorSet = PR_TRUE;
            break;

        case eCursor_sizeNS:
        case eCursor_arrow_south:
        case eCursor_arrow_south_plus:
        case eCursor_arrow_north:
        case eCursor_arrow_north_plus:
            newCursor = QWidget::sizeVerCursor;
            cursorSet = PR_TRUE;
            break;

        case eCursor_sizeWE:
        case eCursor_arrow_east:
        case eCursor_arrow_east_plus:
        case eCursor_arrow_west:
        case eCursor_arrow_west_plus:
            newCursor = QWidget::sizeHorCursor;
            cursorSet = PR_TRUE;
            break;

        case eCursor_sizeNW:
        case eCursor_sizeSE:
            newCursor = QWidget::sizeFDiagCursor;
            cursorSet = PR_TRUE;
            break;

        case eCursor_sizeNE:
        case eCursor_sizeSW:
            newCursor = QWidget::sizeBDiagCursor;
            cursorSet = PR_TRUE;
            break;

        case eCursor_crosshair:
            newCursor = QWidget::crossCursor;
            cursorSet = PR_TRUE;
            break;

        case eCursor_move:
            newCursor = QWidget::sizeAllCursor;
            cursorSet = PR_TRUE;
            break;

        case eCursor_help:
/* Question + Arrow */
        case eCursor_cell:
/* Plus */
        case eCursor_grab:
        case eCursor_grabbing:
/* Hand1 */
        case eCursor_spinning:
/* Exchange */

        case eCursor_copy: // CSS3
        case eCursor_alias:
        case eCursor_context_menu:
        // XXX: these CSS3 cursors need to be implemented
        // For CSS3 Cursor Definitions, See:
        // www.w3.org/TR/css3-userint

        case eCursor_count_up:
        case eCursor_count_down:
        case eCursor_count_up_down:
        // XXX: these CSS3 cursors need to be implemented

        default:
            NS_ASSERTION(PR_FALSE, "Invalid cursor type");
            break;
        }

        if (cursorSet) {
          mCursor = aCursor;

          // Since nsEventStateManager::UpdateCursor() doesn't use the same
          // nsWidget * that is given in DispatchEvent().
          qApp->restoreOverrideCursor();
          qApp->setOverrideCursor(newCursor);
        }
    }
    return NS_OK;
}

NS_METHOD nsWidget::Invalidate(PRBool aIsSynchronous)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::Invalidate %s\n",
            mWidget ? mWidget->name() : "(null)"));
    if (!mWidget) 
        return NS_OK; // mWidget will be null during printing. 

    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::Invalidate: invalidating the whole widget\n"));

    if (aIsSynchronous) 
    {
        mWidget->repaint(false);
        mUpdateArea->SetTo(0, 0, 0, 0);
    } 
    else 
    {
        mWidget->update();
        mUpdateArea->SetTo(0, 0, mBounds.width, mBounds.height);
    }

    return NS_OK;
}

NS_METHOD nsWidget::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::Invalidate %s (%p)\n",
            mWidget ? mWidget->name() : "(null)",
            mWidget));
    if (mWidget == nsnull) 
    {
        return NS_OK;  // mWidget is null during printing
    }
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::Invalidate: invalidating only (%d,%d) by (%d,%d)\n",
           aRect.x, 
           aRect.y,
           aRect.width,
           aRect.height));

    mUpdateArea->Union(aRect.x, aRect.y, aRect.width, aRect.height);
    if (aIsSynchronous) 
    {
        mWidget->repaint(aRect.x, aRect.y, aRect.width, aRect.height, false);
    } 
    else 
    {
        mWidget->update(aRect.x, aRect.y, aRect.width, aRect.height);
    }
    return NS_OK;
}

NS_IMETHODIMP nsWidget::InvalidateRegion(const nsIRegion *aRegion,
                                         PRBool aIsSynchronous)
{
  nsRegionRectSet *regionRectSet = nsnull;
 
  if (!mWidget)
    return NS_OK;
 
  mUpdateArea->Union(*aRegion);
 
  if (NS_FAILED(mUpdateArea->GetRects(&regionRectSet)))
    return NS_ERROR_FAILURE;

  PRUint32 len;
  PRUint32 i;
 
  len = regionRectSet->mRectsLen;
 
  for (i = 0; i < len; ++i) {
    nsRegionRect *r = &(regionRectSet->mRects[i]);
 
    if (aIsSynchronous)
      mWidget->repaint(r->x, r->y, r->width, r->height, false);
    else
      mWidget->update(r->x, r->y, r->width, r->height);
  }
  // drop the const.. whats the right thing to do here?
  ((nsIRegion*)aRegion)->FreeRects(regionRectSet);
 
  return NS_OK;
}

NS_METHOD nsWidget::Update(void)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::Update %s(%p)\n",
            mWidget ? mWidget->name() : "(null)",
            mWidget));

    if (mIsDestroying)
      return NS_ERROR_FAILURE;

    return InvalidateRegion(mUpdateArea, PR_TRUE);
}

//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void *nsWidget::GetNativeData(PRUint32 aDataType)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::GetNativeData %s\n",
            mWidget ? mWidget->name() : "(null)"));

    switch(aDataType) 
    {
      case NS_NATIVE_WINDOW:
        if (mWidget)
          return (void*)((QPaintDevice*)mWidget);
	break;

      case NS_NATIVE_DISPLAY:
        if (mWidget)
          return (void*)(mWidget->x11Display());
        break;

      case NS_NATIVE_WIDGET:
      case NS_NATIVE_PLUGIN_PORT:
        if (mWidget)
          return (void*)mWidget;
	break;

      case NS_NATIVE_GRAPHIC:
        return (void*)mPainter;
	break;

      default:
        PR_LOG(QtWidgetsLM, 
               PR_LOG_DEBUG, 
               ("nsWidget::GetNativeData %s - weird value:%d\n",
                mWidget ? mWidget->name() : "(null)",
                aDataType));
        break;
    }
    return nsnull;
}

//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetColorMap(nsColorMap *aColorMap)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::SetColorMap %s\n",
            mWidget ? mWidget->name() : "(null)"));
    return NS_OK;
}

//Stub for nsWindow functionality
NS_METHOD nsWidget::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::Scroll %s\n",
            mWidget ? mWidget->name() : "(null)"));
    return NS_ERROR_FAILURE;
}

NS_METHOD nsWidget::BeginResizingChildren(void)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::BeginResizingChildren %s\n",
            mWidget ? mWidget->name() : "(null)"));
    return NS_OK;
}

NS_METHOD nsWidget::EndResizingChildren(void)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::EndResizingChildren %s\n",
            mWidget ? mWidget->name() : "(null)"));
    return NS_OK;
}

NS_METHOD nsWidget::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::GetPreferredSize %s\n",
            mWidget ? mWidget->name() : "(null)"));
    aWidth  = mPreferredWidth;
    aHeight = mPreferredHeight;
    return (mPreferredWidth != 0 && mPreferredHeight != 0) ? NS_OK
                                                           : NS_ERROR_FAILURE;
}

NS_METHOD nsWidget::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::SetPreferredSize %s\n",
            mWidget ? mWidget->name() : "(null)"));
    mPreferredWidth  = aWidth;
    mPreferredHeight = aHeight;
    return NS_OK;
}

NS_METHOD nsWidget::SetTitle(const nsString &aTitle)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::SetTitle %s\n",
            mWidget ? mWidget->name() : "(null)"));
    
    if (mWidget)
    {
        const char *title = aTitle.ToNewCString();

        mWidget->setCaption(QString::fromLocal8Bit(title));
        delete [] title;
    }
    return NS_OK;
}

nsresult nsWidget::CreateWidget(nsIWidget *aParent,
                                const nsRect &aRect,
                                EVENT_CALLBACK aHandleEventFunction,
                                nsIDeviceContext *aContext,
                                nsIAppShell *aAppShell,
                                nsIToolkit *aToolkit,
                                nsWidgetInitData *aInitData,
                                nsNativeWidget aNativeParent)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::CreateWidget()\n"));
    QWidget *parentWidget = nsnull;
    nsIWidget *baseParent;
    int width = 0;
    int height = 0;

    if (aParent)
    {
        PR_LOG(QtWidgetsLM, 
               PR_LOG_DEBUG, 
               ("nsWidget::CreateWidget (%p) nsIWidget parent (%p)\n",
                this,
                aParent));
    }
    else if (aNativeParent)
    {
        PR_LOG(QtWidgetsLM, 
               PR_LOG_DEBUG, 
               ("nsWidget::CreateWidget (%p) native parent (%p)\n",
                this,
                aNativeParent));
    }
    else if (aAppShell)
    {
        PR_LOG(QtWidgetsLM, 
               PR_LOG_DEBUG, 
               ("nsWidget::CreateWidget (%p) nsAppShell parent (%p)\n",
                this,
                aAppShell));
    }
    baseParent = aInitData
                 && (aInitData->mWindowType == eWindowType_dialog
                     || aInitData->mWindowType == eWindowType_toplevel)
                 ? nsnull : aParent;

    BaseCreate(baseParent, aRect, aHandleEventFunction, aContext,
               aAppShell, aToolkit, aInitData);

    mParent = aParent;
    if (aNativeParent) 
    {
        parentWidget = (QWidget*)aNativeParent;
        mListenForResizes = PR_TRUE;
    } 
    else if (aParent) 
    {
        parentWidget = (QWidget*)aParent->GetNativeData(NS_NATIVE_WIDGET);
    } 
    if (parentWidget)
    {
        PR_LOG(QtWidgetsLM, 
               PR_LOG_DEBUG, 
               ("nsWidget::CreateWidget created under %s (%p)\n",
                parentWidget->name(), 
                parentWidget));
    }
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::CreateWidget: x=%d,y=%d,width=%d,height=%d\n",
            aRect.x, 
            aRect.y, 
            aRect.width, 
            aRect.height));

    mBounds = aRect;

    CreateNative(parentWidget);

    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::CreateWidget created native widget (%p)\n",
            mWidget));

    // XXXXX
    //
    // For some reason if I correct the width and height of the widget, 
    // nothing shows up in the main browser window.
    //
    // XXXXX
    if (aRect.width <= 0) 
    {
        PR_LOG(QtWidgetsLM, 
               PR_LOG_DEBUG, 
               ("nsWidget::CreateWidget: Fixing width...\n"));
        width = 1;
    }
    else 
    {
        width = aRect.width;
    }
    if (aRect.height <= 0) 
    {
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::CreateWidget: Fixing height...\n"));
        height = 1;
    }
    else 
    {
        height = aRect.height;
    }
    Resize(width, height, PR_FALSE);

    DispatchStandardEvent(NS_CREATE);
    InitCallbacks();

    return NS_OK;
}

//-------------------------------------------------------------------------
//
// create with nsIWidget parent
//
//-------------------------------------------------------------------------

NS_METHOD nsWidget::Create(nsIWidget *aParent,
                           const nsRect &aRect,
                           EVENT_CALLBACK aHandleEventFunction,
                           nsIDeviceContext *aContext,
                           nsIAppShell *aAppShell,
                           nsIToolkit *aToolkit,
                           nsWidgetInitData *aInitData)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::Create()\n"));
    return(CreateWidget(aParent, aRect, aHandleEventFunction,
                        aContext, aAppShell, aToolkit, aInitData,
                        nsnull));
}

//-------------------------------------------------------------------------
//
// create with a native parent
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::Create(nsNativeWidget aParent,
                           const nsRect &aRect,
                           EVENT_CALLBACK aHandleEventFunction,
                           nsIDeviceContext *aContext,
                           nsIAppShell *aAppShell,
                           nsIToolkit *aToolkit,
                           nsWidgetInitData *aInitData)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::Create()\n"));
    return(CreateWidget(nsnull, aRect, aHandleEventFunction,
                        aContext, aAppShell, aToolkit, aInitData,
                        aParent));
}

//-------------------------------------------------------------------------
//
// Initialize all the Callbacks
//
//-------------------------------------------------------------------------
void nsWidget::InitCallbacks(char *aName)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::InitCallbacks %s\n",
            mWidget ? mWidget->name() : "(null)"));
// Original Comment:
// I think I need some way to notify the XPFE system when a widget has been
// shown. The only way that I can see to do this is to use the QEvent-style
// classes.
}

void nsWidget::ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::ConvertToDeviceCoordinates %s\n",
            mWidget ? mWidget->name() : "(null)"));
}

void nsWidget::InitEvent(nsGUIEvent& event, 
                         PRUint32 aEventType, 
                         nsPoint* aPoint)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::InitEvent %s\n",
            mWidget ? mWidget->name() : "(null)"));

    event.widget = this;

    if (aPoint == nsnull) 
    {     
      event.point.x = 0;
      event.point.y = 0;
    }    
    else 
    {
      // use the point override if provided
      event.point.x = aPoint->x;
      event.point.y = aPoint->y;
    }
    event.time =  PR_IntervalNow();
    event.message = aEventType;
}

PRBool nsWidget::ConvertStatus(nsEventStatus aStatus)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::ConvertStatus %s\n",
            mWidget ? mWidget->name() : "(null)"));

    switch(aStatus) 
    {
    case nsEventStatus_eIgnore:
        return(PR_FALSE);
    case nsEventStatus_eConsumeNoDefault:
        return(PR_TRUE);
    case nsEventStatus_eConsumeDoDefault:
        return(PR_FALSE);
    default:
        NS_ASSERTION(0, "Illegal nsEventStatus enumeration value");
        break;
    }
    return(PR_FALSE);
}

PRBool nsWidget::DispatchWindowEvent(nsGUIEvent* event)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::DispatchWindowEvent %s\n",
            mWidget ? mWidget->name() : "(null)"));

    nsEventStatus status;
    DispatchEvent(event, status);
    return ConvertStatus(status);
}

//-------------------------------------------------------------------------
//
// Dispatch standard event
//
//-------------------------------------------------------------------------

PRBool nsWidget::DispatchStandardEvent(PRUint32 aMsg)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::DispatchStandardEvent %s\n",
            mWidget ? mWidget->name() : "(null)"));

    nsGUIEvent event;
    event.eventStructType = NS_GUI_EVENT;
    InitEvent(event, aMsg);

    PRBool result = DispatchWindowEvent(&event);
    return result;
}


//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWidget::DispatchEvent(nsGUIEvent *event,
                                      nsEventStatus &aStatus)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::DispatchEvent: %s(%p): event listener=%p\n",
            mWidget ? mWidget->name() : "(null)", mWidget, mEventListener));

    NS_ADDREF(event->widget);

    if (nsnull != mMenuListener) {
      if (NS_MENU_EVENT == event->eventStructType)
        aStatus = mMenuListener->MenuSelected(NS_STATIC_CAST(nsMenuEvent&,
                                                             *event));
    }
    aStatus = nsEventStatus_eIgnore;
    if (nsnull != mEventCallback) {
        PR_LOG(QtWidgetsLM, 
               PR_LOG_DEBUG, 
               ("nsWidget::DispatchEvent %s: calling callback function\n",
                mWidget ? mWidget->name() : "(null)"));   

        aStatus = (*mEventCallback)(event);
    }
    // Dispatch to event listener if event was not consumed
    if (aStatus != nsEventStatus_eIgnore && nsnull != mEventListener) {
        PR_LOG(QtWidgetsLM, 
               PR_LOG_DEBUG, 
               ("nsWidget::DispatchEvent %s: calling event listener\n",
                mWidget ? mWidget->name() : "(null)", aStatus));   

        aStatus = mEventListener->ProcessEvent(*event);
    }
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::DispatchEvent %s: status=%d\n",
            mWidget ? mWidget->name() : "(null)", aStatus));   

    NS_RELEASE(event->widget);
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Deal with all sort of mouse event
//
//-------------------------------------------------------------------------
PRBool nsWidget::DispatchMouseEvent(nsMouseEvent& aEvent)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::DispatchMouseEvent %s\n",
            mWidget ? mWidget->name() : "(null)"));

    PRBool result = PR_FALSE;

    if (aEvent.message == NS_MOUSE_LEFT_BUTTON_DOWN
        || aEvent.message == NS_MOUSE_RIGHT_BUTTON_DOWN
         || aEvent.message == NS_MOUSE_MIDDLE_BUTTON_DOWN) {
      if (HandlePopup(((QMouseEvent*)(aEvent.nativeMsg))->globalX(),
                      ((QMouseEvent*)(aEvent.nativeMsg))->globalY()))
        return result;
    }
    if (nsnull == mEventCallback && nsnull == mMouseListener) 
    {
        return result;
    }
    // call the event callback
    if (nsnull != mEventCallback) 
    {
        result = DispatchWindowEvent(&aEvent);

        return result;
    }
    if (nsnull != mMouseListener) 
    {
        switch (aEvent.message) 
        {
          case NS_MOUSE_MOVE: 
          {
//              result = ConvertStatus(mMouseListener->MouseMoved(event));
//              nsRect rect;
//              GetBounds(rect);
//              if (rect.Contains(event.point.x, event.point.y)) {
//              if (mCurrentWindow == NULL || mCurrentWindow != this) {
//              printf("Mouse enter");
//              mCurrentWindow = this;
//              }
//              } else {
//              printf("Mouse exit");
//              }
          }
          break;

          case NS_MOUSE_LEFT_BUTTON_DOWN:
          case NS_MOUSE_MIDDLE_BUTTON_DOWN:
          case NS_MOUSE_RIGHT_BUTTON_DOWN:
            result = ConvertStatus(mMouseListener->MousePressed(aEvent));
          break;

          case NS_MOUSE_LEFT_BUTTON_UP:
          case NS_MOUSE_MIDDLE_BUTTON_UP:
          case NS_MOUSE_RIGHT_BUTTON_UP:
            result = ConvertStatus(mMouseListener->MouseReleased(aEvent));
            result = ConvertStatus(mMouseListener->MouseClicked(aEvent));
          break;

          default:
          break;
        } // switch
    }
    return result;
}

PRBool
nsWidget::IsMouseInWindow(QWidget* inWindow,PRInt32 inMouseX,PRInt32 inMouseY)
{
  QPoint origin = inWindow->mapToGlobal(QPoint(0,0));

  if (inMouseX > origin.x() && inMouseX < (origin.x() + inWindow->width())
      && inMouseY > origin.y() && inMouseY < (origin.y() + inWindow->height()))
    return PR_TRUE;

  return PR_FALSE;
}

//
// HandlePopup
//
// Deal with rollup of popups (xpmenus, etc)
//
PRBool
nsWidget::HandlePopup(PRInt32 inMouseX,PRInt32 inMouseY)
{
  PRBool retVal = PR_FALSE;
  nsCOMPtr<nsIWidget> rollupWidget = do_QueryReferent(gRollupWidget);
  if (rollupWidget && gRollupListener)
  {
    QWidget *currentPopup = (QWidget*)rollupWidget->GetNativeData(NS_NATIVE_WIDGET);
    if ( currentPopup && !IsMouseInWindow(currentPopup, inMouseX, inMouseY) ) {
      PRBool rollup = PR_TRUE;
      // if we're dealing with menus, we probably have submenus and we don't
      // want to rollup if the clickis in a parent menu of the current submenu
      nsCOMPtr<nsIMenuRollup> menuRollup(do_QueryInterface(gRollupListener));
      if (menuRollup) {
        nsCOMPtr<nsISupportsArray> widgetChain;
        menuRollup->GetSubmenuWidgetChain(getter_AddRefs(widgetChain));
        if (widgetChain) {
          PRUint32 count = 0;
          widgetChain->Count(&count);
          for (PRUint32 i = 0; i < count; ++i) {
            nsCOMPtr<nsISupports> genericWidget;
            widgetChain->GetElementAt(i,getter_AddRefs(genericWidget));
            nsCOMPtr<nsIWidget> widget(do_QueryInterface(genericWidget));
            if (widget) {
              QWidget *currWindow = (QWidget*)widget->GetNativeData(NS_NATIVE_WIDGET);
              if ( currWindow && IsMouseInWindow(currWindow, inMouseX, inMouseY) ) {
                rollup = PR_FALSE;
                break;
              }
            }
          } // foreach parent menu widget
        }
      } // if rollup listener knows about menus

      // if we've determined that we should still rollup, do it.
      if ( rollup ) {
        gRollupListener->Rollup();
        retVal = PR_TRUE;
      }
    }
  } else {
    gRollupWidget = nsnull;
    gRollupListener = nsnull;
  }
  return retVal;
} // HandlePopup


//-------------------------------------------------------------------------
//
// Base implementation of CreateNative.
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::CreateNative(QWidget *parentWindow)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::CreateNative()\n"));

    if (mWidget)
    {
      mWidget->setMouseTracking(true);
    }
    mEventHandler = new nsQEventHandler(this);
    if (mEventHandler && mWidget)
      mWidget->installEventFilter(mEventHandler);

    return NS_OK;
}

NS_IMETHODIMP nsWidget::ResetInputState()
{
  return NS_OK;  
}

NS_IMETHODIMP nsWidget::PasswordFieldInit()
{
  return NS_OK;  
}

void nsWidget::AddChildEventHandler(nsQEventHandler *aEHandler)
{
  mChildEventHandlers.insert((void*)aEHandler,aEHandler);
  if (mParent)
    ((nsWidget*)(mParent.get()))->AddChildEventHandler(aEHandler);
}

void nsWidget::RemoveChildEventHandler(nsQEventHandler *aEHandler)
{
  mChildEventHandlers.take((void*)aEHandler);
  if (mParent)
    ((nsWidget*)(mParent.get()))->RemoveChildEventHandler(aEHandler);
}

