/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsWidget.h"
#include "nsIDeviceContext.h"
#include "nsIAppShell.h"
#include "nsGfxCIID.h"
#include "nsIComponentManager.h"
#include "nsIFontMetrics.h"
#include "nsQEventHandler.h"
#include <qwidget.h>

#include <qpainter.h>
#include <qpixmap.h>

static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);
static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

//#define DBG 1

PRLogModuleInfo * QtWidgetsLM   = PR_NewLogModule("QtWidgets");
PRLogModuleInfo * QtScrollingLM = PR_NewLogModule("QtScrolling");

//=============================================================================
//
// nsQBaseWidget class
//
//=============================================================================
nsQBaseWidget::nsQBaseWidget(nsWidget * widget)
    : mWidget(widget)
{
    NS_IF_ADDREF(mWidget);
}

nsQBaseWidget::~nsQBaseWidget()
{
    NS_IF_RELEASE(mWidget);
}

nsWidget::nsWidget()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWidget::nsWidget()\n"));
    // XXX Shouldn't this be done in nsBaseWidget?
    //NS_INIT_REFCNT();

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
    mShown           = PR_FALSE;
    mBounds.x        = 0;
    mBounds.y        = 0;
    mBounds.width    = 0;
    mBounds.height   = 0;
    mIsDestroying    = PR_FALSE;
    mOnDestroyCalled = PR_FALSE;
    mIsToplevel      = PR_FALSE;
    mEventHandler    = nsnull;
    mUpdateArea.SetRect(0, 0, 0, 0);
}

nsWidget::~nsWidget()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWidget::~nsWidget()\n"));
    mIsDestroying = PR_TRUE;
    if (nsnull != mWidget) 
    {
        Destroy();
    }

    if (mPainter)
    {
        delete mPainter;
    }

    if (mPixmap)
    {
        delete mPixmap;
    }
}

NS_METHOD nsWidget::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWidget::WidgetToScreen()\n"));
    // FIXME gdk_window_get_origin()   might do what we want.... ???
    NS_NOTYETIMPLEMENTED("nsWidget::WidgetToScreen");
    return NS_OK;
}

NS_METHOD nsWidget::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWidget::ScreenToWidget()\n"));
    NS_NOTYETIMPLEMENTED("nsWidget::ScreenToWidget");
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
    if (!mIsDestroying) 
    {
        nsBaseWidget::Destroy();
        NS_IF_RELEASE(mParent);
    }

    if (mWidget) 
    {
        delete mWidget;

        mWidget = nsnull;
        if (PR_FALSE == mOnDestroyCalled)
        {
            OnDestroy();
        }
    }

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
    if (!mIsDestroying) 
    {
        // dispatching of the event may cause the reference count to drop to 0
        // and result in this object being destroyed. To avoid that, add a reference
        // and then release it after dispatching the event
        AddRef();
        DispatchStandardEvent(NS_DESTROY);
        Release();
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
//  NS_NOTYETIMPLEMENTED("nsWidget::GetParent");
    if (mParent)
    {
        NS_ADDREF(mParent);
    }
    return mParent;
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
    if (!mWidget)
    {
        return NS_OK; // Will be null during printing
    }

    if (bState)
    {
        mWidget->show();
    }
    else
    {
        mWidget->hide();
    }

    mShown = bState;

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
        aState = PR_TRUE;
    }

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
#if 1

    if (mWidget->width() != aWidth ||
        mWidget->height() != aHeight)
#endif
    {
        mBounds.width  = aWidth;
        mBounds.height = aHeight;
     
        if (mWidget) 
        {
            mWidget->resize(aWidth, aHeight);
        }            

        if (aRepaint)
        {
            if (mWidget->isVisible())
            {
                mWidget->repaint();
            }
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
    Resize(aWidth,aHeight,aRepaint);
    Move(aX,aY);
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Send a resize message to the listener
//
//-------------------------------------------------------------------------
PRBool nsWidget::OnResize(nsRect &aRect)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::OnResize %s\n",
            mWidget ? mWidget->name() : "(null)"));
    // call the event callback
    if (mEventCallback) 
    {
        nsSizeEvent event;
        InitEvent(event, NS_SIZE);
        event.windowSize = &aRect;
        event.eventStructType = NS_SIZE_EVENT;
        if (mWidget) 
        {
            event.mWinWidth  = mWidget->width();
            event.mWinHeight = mWidget->height();
            event.point.x    = mWidget->x();
            event.point.y    = mWidget->y();
        } 
        else 
        {
            event.mWinWidth  = 0;
            event.mWinHeight = 0;
            event.point.x    = 0;
            event.point.y    = 0;
        }
        event.time = 0;
        PRBool result = DispatchWindowEvent(&event);
        // XXX why does this always crash?  maybe we need to add 
        // a ref in the dispatch code?  check the windows
        // code for a reference
        //NS_RELEASE(event.widget);
        return result;
    }
    return PR_FALSE;
}

//------
// Move
//------
PRBool nsWidget::OnMove(PRInt32 aX, PRInt32 aY)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::OnMove %s\n",
            mWidget ? mWidget->name() : "(null)"));
    nsGUIEvent event;

    InitEvent(event, NS_MOVE);
    event.point.x = aX;
    event.point.y = aY;
    event.eventStructType = NS_GUI_EVENT;
    PRBool result = DispatchWindowEvent(&event);
    // NS_RELEASE(event.widget);
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
    if (mWidget)
    {
        mWidget->setEnabled(bState);
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

NS_METHOD nsWidget::GetBounds(nsRect &aRect)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::GetBounds %s: {%d,%d,%d,%d}\n",
            mWidget ? mWidget->name() : "(null)",
            mBounds.x,
            mBounds.y,
            mBounds.width,
            mBounds.height));
    aRect = mBounds;
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
    nsIFontMetrics* mFontMetrics;
    mContext->GetMetricsFor(aFont, mFontMetrics);

    if (mFontMetrics && mWidget)
    {
        nsFontHandle  fontHandle;
        mFontMetrics->GetFontHandle(fontHandle);

        QFont * font = (QFont *)fontHandle;
        
        mWidget->setFont(*font);
    }
    NS_RELEASE(mFontMetrics);
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

    if (mWidget)
    {
        // There are some "issues" with the conversion of rgb values
        QColor color(NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor));
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
           ("nsWidget::SetCursor %s\n",
            mWidget ? mWidget->name() : "(null)"));
    if (!mWidget)
    {
        return NS_ERROR_FAILURE;
    }

    // Only change cursor if it's changing
    if (aCursor != mCursor) 
    {
        QCursor newCursor;

        switch(aCursor) 
        {
        case eCursor_select:
            newCursor = QWidget::ibeamCursor;
            break;

        case eCursor_wait:
            newCursor = QWidget::waitCursor;
            break;

        case eCursor_hyperlink:
            newCursor = QWidget::pointingHandCursor;
            break;

        case eCursor_standard:
            newCursor = QWidget::arrowCursor;
            break;

        case eCursor_sizeWE:
            newCursor = QWidget::sizeBDiagCursor;
            break;

        case eCursor_sizeNS:
            newCursor = QWidget::sizeFDiagCursor;
            break;

        case eCursor_arrow_south:
        case eCursor_arrow_south_plus:
        case eCursor_arrow_north:
        case eCursor_arrow_north_plus:
            newCursor = QWidget::sizeVerCursor;
            break;

        case eCursor_arrow_east:
        case eCursor_arrow_east_plus:
        case eCursor_arrow_west:
        case eCursor_arrow_west_plus:
            newCursor = QWidget::sizeHorCursor;
            break;

        default:
            NS_ASSERTION(PR_FALSE, "Invalid cursor type");
            break;
        }
        
        mCursor = aCursor;
        mWidget->setCursor(newCursor);
    }

    return NS_OK;
}

NS_METHOD nsWidget::Invalidate(PRBool aIsSynchronous)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::Invalidate %s\n",
            mWidget ? mWidget->name() : "(null)"));
    if (mWidget == nsnull) 
    {
        return NS_OK; // mWidget will be null during printing. 
    }

    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::Invalidate: invalidating the whole widget\n"));

    if (aIsSynchronous) 
    {
        mWidget->repaint();
        mUpdateArea.SetRect(0, 0, 0, 0);
    } 
    else 
    {
        mWidget->update();
        mUpdateArea.SetRect(0, 0, mBounds.width, mBounds.height);
    }

    return NS_OK;
}

NS_METHOD nsWidget::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::Invalidate %s\n",
            mWidget ? mWidget->name() : "(null)"));
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

    if (aIsSynchronous) 
    {
        mWidget->repaint(aRect.x, aRect.y, aRect.width, aRect.height);
    } 
    else 
    {
        mUpdateArea.UnionRect(mUpdateArea, aRect);
        
        mWidget->update(aRect.x, aRect.y, aRect.width, aRect.height);
    }

    return NS_OK;
}

NS_METHOD nsWidget::Update(void)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::Update %s\n",
            mWidget ? mWidget->name() : "(null)"));
    if (!mWidget)
    {
        return NS_OK;
    }
    
    //mWidget->repaint();

    if (mUpdateArea.width && mUpdateArea.height) 
    {
        if (!mIsDestroying) 
        {
            Invalidate(mUpdateArea, PR_TRUE);
            mUpdateArea.SetRect(0, 0, 0, 0);
            return NS_OK;
        }
        else 
        {
            return NS_ERROR_FAILURE;
        }
    }
    else 
    {
        PR_LOG(QtWidgetsLM, 
               PR_LOG_DEBUG, 
               ("nsWidget::Update: avoided empty update\n"));
    }
    return NS_OK;
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
#if 0
        // Return Drawable.
        if (!mPixmap && mBounds.width && mBounds.height)
        {
            PR_LOG(QtWidgetsLM, 
                   PR_LOG_DEBUG, 
                   ("nsWidget::GetNativeData %s creating pixmap %dx%d\n",
                    mWidget ? mWidget->name() : "(null)",
                    mBounds.width, 
                    mBounds.height));
            mPixmap = new QPixmap(mBounds.width, mBounds.height);
        }
        if (mPixmap)
        {
            return mPixmap->handle();
        }
        else
        {
            NS_ASSERTION(0, "Couldn't allocated QPixmap");
            return nsnull;
        }
#else
        if (!mPixmap && mBounds.width && mBounds.height)
        {
            PR_LOG(QtWidgetsLM, 
                   PR_LOG_DEBUG, 
                   ("nsWidget::GetNativeData %s creating pixmap %dx%d\n",
                    mWidget ? mWidget->name() : "(null)",
                    mBounds.width, 
                    mBounds.height));
            //
            // BAD !!!!!!
            //
            mPixmap  = new QPixmap(mBounds.width, mBounds.height);
        }
        return (void *)mPixmap;
        //case NS_NATIVE_DISPLAY:
        //return (void *)GDK_DISPLAY();
#endif
    case NS_NATIVE_WIDGET:
        return (void *)mWidget;
    case NS_NATIVE_GRAPHIC:
#if 0
        // Return GC.
        return qt_xget_temp_gc();
#else
        return mPainter;
#endif
        //return (void *)((nsToolkit *)mToolkit)->GetSharedGC();
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

NS_METHOD nsWidget::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::Scroll %s\n",
            mWidget ? mWidget->name() : "(null)"));
    NS_NOTYETIMPLEMENTED("nsWidget::Scroll");
    return NS_OK;
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
    return (mPreferredWidth != 0 && mPreferredHeight != 0)?NS_OK:NS_ERROR_FAILURE;
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

NS_METHOD nsWidget::SetMenuBar(nsIMenuBar * aMenuBar)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::SetMenuBar %s\n",
            mWidget ? mWidget->name() : "(null)"));
    NS_NOTYETIMPLEMENTED("nsWidget::SetMenuBar");
    return NS_OK;
}

NS_METHOD nsWidget::ShowMenuBar(PRBool aShow)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::ShowMenuBar %s\n",
            mWidget ? mWidget->name() : "(null)"));
    NS_NOTYETIMPLEMENTED("nsWidget::ShowMenuBar");
    return NS_OK;
}

NS_METHOD nsWidget::IsMenuBarVisible(PRBool *aVisible)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWidget::IsMenuBarVisible %s\n",
            mWidget ? mWidget->name() : "(null)"));
    NS_NOTYETIMPLEMENTED("nsWidget::IsMenuBarvisible");
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

    BaseCreate(aParent, aRect, aHandleEventFunction, aContext,
               aAppShell, aToolkit, aInitData);

    mParent = aParent;
    if (aNativeParent) 
    {
        parentWidget = (QWidget *) aNativeParent;
    } 
    else if (aParent) 
    {
        parentWidget = (QWidget *) aParent->GetNativeData(NS_NATIVE_WIDGET);
    } 
    else if (aAppShell) 
    {
        nsNativeWidget shellWidget = aAppShell->GetNativeData(NS_NATIVE_SHELL);
        if (shellWidget)
        {
            parentWidget = (QWidget *) shellWidget;
        }
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

    mPainter = new QPainter;

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
    
    Resize(aRect.x, aRect.y, aRect.width, aRect.height, PR_TRUE);
    //Resize(aRect.width, aRect.height, PR_TRUE);

    if (aRect.width && aRect.height)
    {
        DispatchStandardEvent(NS_CREATE);
    }

    InitCallbacks();

    //mPixmap  = new QPixmap(aRect.width, aRect.height);
    //mPainter = new QPainter(mPixmap);

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

#if 0
    // I think I need some way to notify the XPFE system when a widget has been
    // shown. The only way that I can see to do this is to use the QEvent-style
    // classes.
    QObject::connect(mWidget, 
                     SIGNAL(), 
                     mEventHandler, 
                     SLOT());
#endif

/* basically we are keeping the parent from getting the childs signals by
 * doing this. */
#if 0
    gtk_signal_connect_after(GTK_OBJECT(mWidget),
                             "button_press_event",
                             GTK_SIGNAL_FUNC(gtk_true),
                             NULL);
    gtk_signal_connect(GTK_OBJECT(mWidget),
                       "button_release_event",
                       GTK_SIGNAL_FUNC(gtk_true),
                       NULL);
    gtk_signal_connect(GTK_OBJECT(mWidget),
                       "motion_notify_event",
                       GTK_SIGNAL_FUNC(gtk_true),
                       NULL);
#endif
    /*
      gtk_signal_connect(GTK_OBJECT(mWidget),
      "enter_notify_event",
      GTK_SIGNAL_FUNC(gtk_true),
      NULL);
      gtk_signal_connect(GTK_OBJECT(mWidget),
      "leave_notify_event",
      GTK_SIGNAL_FUNC(gtk_true),
      NULL);
    
      gtk_signal_connect(GTK_OBJECT(mWidget),
      "draw",
      GTK_SIGNAL_FUNC(gtk_false),
      NULL);
      gtk_signal_connect(GTK_OBJECT(mWidget),
      "expose_event",
      GTK_SIGNAL_FUNC(gtk_true),
      NULL);
      gtk_signal_connect(GTK_OBJECT(mWidget),
      "key_press_event",
      GTK_SIGNAL_FUNC(gtk_true),
      NULL);
      gtk_signal_connect(GTK_OBJECT(mWidget),
      "key_release_event",
      GTK_SIGNAL_FUNC(gtk_true),
      NULL);
      gtk_signal_connect(GTK_OBJECT(mWidget),
      "focus_in_event",
      GTK_SIGNAL_FUNC(gtk_true),
      NULL);
      gtk_signal_connect(GTK_OBJECT(mWidget),
      "focus_out_event",
      GTK_SIGNAL_FUNC(gtk_true),
      NULL);
    */
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
    NS_IF_ADDREF(event.widget);

#if 0
    if (aPoint == nsnull) 
    {     
        // use the point from the event
        // get the message position in client coordinates and in twips

        if (ge != nsnull) 
        {
            //       ::ScreenToClient(mWnd, &cpos);
            event.point.x = PRInt32(ge->x);
            event.point.y = PRInt32(ge->y);
        }
        else 
        { 
            event.point.x = 0;
            event.point.y = 0;
        }  
    }    
    else 
    {
        // use the point override if provided
        event.point.x = aPoint->x;
        event.point.y = aPoint->y;
    }
#endif

    event.time = 0;
    event.message = aEventType;

//    mLastPoint.x = event.point.x;
//    mLastPoint.y = event.point.y;
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
    NS_IF_RELEASE(event.widget);
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
           ("nsWidget::DispatchEvent %s\n",
            mWidget ? mWidget->name() : "(null)"));
    NS_ADDREF(event->widget);

    if (nsnull != mMenuListener) 
    {
        if (NS_MENU_EVENT == event->eventStructType)
        {
            aStatus = mMenuListener->MenuSelected(NS_STATIC_CAST(nsMenuEvent&,
                                                                 *event));
        }
    }

    aStatus = nsEventStatus_eIgnore;
    if (nsnull != mEventCallback) 
    {
        aStatus = (*mEventCallback)(event);
    }

    // Dispatch to event listener if event was not consumed
    if ((aStatus != nsEventStatus_eIgnore) && (nsnull != mEventListener)) 
    {
        aStatus = mEventListener->ProcessEvent(*event);
    }
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
            /*result = ConvertStatus(mMouseListener->MouseMoved(event));
              nsRect rect;
              GetBounds(rect);
              if (rect.Contains(event.point.x, event.point.y)) {
              if (mCurrentWindow == NULL || mCurrentWindow != this) {
              //printf("Mouse enter");
              mCurrentWindow = this;
              }
              } else {
              //printf("Mouse exit");
              }*/

        } break;

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
        } // switch
    }
    return result;
}

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

    mEventHandler = nsQEventHandler::Instance(mWidget, this);

    if (mEventHandler && mWidget)
    {
        mWidget->installEventFilter(mEventHandler);
    }

    return NS_OK;
}
