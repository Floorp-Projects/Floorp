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

#include "nsWindow.h"
#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsTransform2D.h"
#include "nsGfxCIID.h"
#include "nsMenuBar.h"

#include "nsAppShell.h"
#include "nsClipboard.h"

#include "stdio.h"

//#define DBG 0

//=============================================================================
//
// nsQWidget class
//
//=============================================================================
nsQWidget::nsQWidget(nsWidget * widget,
                     QWidget * parent, 
                     const char * name, 
                     WFlags f)
	: QWidget(parent, name, f), nsQBaseWidget(widget)
{
}

nsQWidget::~nsQWidget()
{
}

/**
 * Implement the standard QueryInterface for NS_IWIDGET_IID and NS_ISUPPORTS_IID
 * @modify gpk 8/4/98
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 *
*/
nsresult nsWindow::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::QueryInterface()\n"));
    if (NULL == aInstancePtr) 
    {
        return NS_ERROR_NULL_POINTER;
    }

    static NS_DEFINE_IID(kCWindow, NS_WINDOW_CID);
    if (aIID.Equals(kCWindow)) 
    {
        *aInstancePtr = (void*) ((nsWindow*)this);
        AddRef();
        return NS_OK;
    }
    return nsWidget::QueryInterface(aIID,aInstancePtr);
}



//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow() 
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::nsWindow()\n"));
    NS_INIT_REFCNT();
    //strcpy(gInstanceClassName, "nsWindow");
    mFontMetrics = nsnull;
    mShell = nsnull;
    mHBox = nsnull;
    mResized = PR_FALSE;
    mVisible = PR_FALSE;
    mDisplayed = PR_FALSE;
    mLowerLeft = PR_FALSE;
    mIsDestroying = PR_FALSE;
    mOnDestroyCalled = PR_FALSE;
    mFont = nsnull;
}

//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::~nsWindow()\n"));
    mIsDestroying = PR_TRUE;
    if (mShell)
    {
        delete mShell;
        mShell = nsnull;
    }

    if (mHBox)
    {
        delete mHBox;
        mHBox = nsnull;
    }
}

//-------------------------------------------------------------------------
PRBool nsWindow::IsChild() const
{
    return PR_FALSE;
}

//-------------------------------------------------------------------------
void nsWindow::ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWindow::ConvertToDeviceCoordinates()\n"));
}

//-------------------------------------------------------------------------
//
// Setup initial tooltip rectangles
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[])
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::SetTooltips()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Update all tooltip rectangles
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::UpdateTooltips(nsRect* aNewTips[])
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::UpdateTooltips()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Remove all tooltip rectangles
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::RemoveTooltips()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::RemoveTooltips()\n"));
    return NS_OK;
}

NS_METHOD nsWindow::Destroy()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::Destroy()\n"));
    // disconnect from the parent

    if (mIsDestroying == PR_TRUE) 
    {
        nsBaseWidget::Destroy();
        if (PR_FALSE == mOnDestroyCalled)
        {
            OnDestroy();
        }
        if (mShell) 
        {
            delete mShell;
            mShell = nsnull;
        }
    }

    return NS_OK;
}

void nsWindow::OnDestroy()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::OnDestroy()\n"));
    mOnDestroyCalled = PR_TRUE;

    // release references to children, device context, toolkit, and app shell
    nsBaseWidget::OnDestroy();

    // dispatch the event
    if (mIsDestroying == PR_TRUE) 
    {
        // dispatching of the event may cause the reference count to drop to 0
        // and result in this object being destroyed. To avoid that, add a 
        // reference and then release it after dispatching the event
        AddRef();
        DispatchStandardEvent(NS_DESTROY);
        Release();
    }
}


NS_METHOD nsWindow::PreCreateWidget(nsWidgetInitData *aInitData)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::PreCreateWidget()\n"));
    if (nsnull != aInitData) 
    {
        SetWindowType(aInitData->mWindowType);
        SetBorderStyle(aInitData->mBorderStyle);

        return NS_OK;
    }

    return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
//
// Create the native widget
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::CreateNative(QWidget *parentWidget)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::CreateNative()\n"));

    QWidget::WFlags w = 0;

    if (mBorderStyle == eBorderStyle_default)
    {
        w = 0;
    }
    else
    {
        if (mBorderStyle & eBorderStyle_all)
        {
            w |= 0;
        }
        if (mBorderStyle & eBorderStyle_border)
        {
            w |= Qt::WStyle_DialogBorder;
        }
        if (mBorderStyle & eBorderStyle_resizeh)
        {
            w |= Qt::WStyle_NormalBorder;
        }
        if (mBorderStyle & eBorderStyle_title)
        {
            w |= Qt::WStyle_Title;
        }
        if (mBorderStyle & eBorderStyle_menu)
        {
            w |= Qt::WStyle_SysMenu | Qt::WStyle_Title;
        }
        if (mBorderStyle & eBorderStyle_minimize)
        {
            w |= Qt::WStyle_Minimize;
        }
        if (mBorderStyle & eBorderStyle_maximize)
        {
            w |= Qt::WStyle_Maximize;
        }
        if (mBorderStyle & eBorderStyle_close)
        {
            PR_LOG(QtWidgetsLM, 
                   PR_LOG_DEBUG, 
                   ("eBorderStyle_close isn't handled yet... please fix me\n"));
        }
    }

#if 0
    switch (mWindowType)
    {
    case eWindowType_toplevel:
        w |= Qt::WType_TopLevel;
        break;

    case eWindowType_dialog:
        w |= Qt::WType_Modal;
        break;

    case eWindowType_popup:
        w |= Qt::WType_Popup;
        break;

    case eWindowType_child:
        w |= 0;
        break;
    }
#endif

    w |= (w) ? Qt::WStyle_Customize | w : 0;

    mWidget = new nsQWidget(this, 
                            parentWidget, 
                            QWidget::tr("nsWindow"),
                            w);

    if (!parentWidget) 
    {
        // This is a top-level window. I'm not sure what special actions need
        // to be taken here.

        //mWidget->resize(mBounds.width, mBounds.height);
        //mWidget->show();

        //qApp->setMainWidget(mWidget);
    }

    // Force cursor to default setting
    mIsToplevel = PR_TRUE;
    mCursor = eCursor_select;
    SetCursor(eCursor_standard);

    return nsWidget::CreateNative(parentWidget);
}


//-------------------------------------------------------------------------
//
// Initialize all the Callbacks
//
//-------------------------------------------------------------------------
void nsWindow::InitCallbacks(char * aName)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::InitCallbacks()\n"));

#if 0
    QObject::connect(mWidget, 
                     SIGNAL(pressed()), 
                     (QObject *)mEventHandler,
                     SLOT(HandleButtonPress(mWidget)));
#endif
}

//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetColorMap(nsColorMap *aColorMap)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::SetColorMap()\n"));
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::Scroll()\n"));
    if (mWidget)
    {
        mWidget->scroll(aDx, aDy);
    }

    return NS_OK;
}

NS_METHOD nsWindow::SetTitle(const nsString& aTitle)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::SetTitle()\n"));
    if (!mWidget)
    {
        return NS_ERROR_FAILURE;
    }
    char * titleStr = aTitle.ToNewCString();
    mWidget->setCaption(titleStr);
    delete[] titleStr;
    return NS_OK;
}


/**
 * Processes an Expose Event
 *
 **/
PRBool nsWindow::OnPaint(nsPaintEvent &event)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::OnPaint()\n"));
    nsresult result ;
    PRInt32 x;
    PRInt32 y;
    PRInt32 width;
    PRInt32 height;
    

    // call the event callback
    if (mEventCallback) 
    {

        event.renderingContext = nsnull;
#if 1
        if (event.rect) 
        {
            PR_LOG(QtWidgetsLM, 
                   PR_LOG_DEBUG, 
                   ("nsWindow::OnPaint: this=%p, {%d,%d,%d,%d}\n",
                   this,
                   event.rect->x, 
                   event.rect->y,
                   event.rect->width, 
                   event.rect->height));
            x = event.rect->x;
            y = event.rect->y;
            width = event.rect->width;
            height = event.rect->height;
        }
        else 
        {
            PR_LOG(QtWidgetsLM, 
                   PR_LOG_DEBUG, 
                   ("nsWindow::OnPaint: this=%p, NO RECT\n",
                   this));
            x = 0;
            y = 0;
            width = mWidget->width();
            height = mWidget->height();
        }
#endif
        static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
        static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
        if (NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID,
                                                        nsnull,
                                                        kRenderingContextIID,
                                                        (void **)&event.renderingContext))
        {
            if (mBounds.width && mBounds.height)
            {
                event.renderingContext->Init(mContext, this);
                result = DispatchWindowEvent(&event);
                NS_RELEASE(event.renderingContext);
                PR_LOG(QtWidgetsLM, 
                       PR_LOG_DEBUG, 
                       ("nsWindow::OnPaint: bitBlt: {%d,%d,%d,%d}\n",
                       //mWidget->width(),
                       //mWidget->height());
                       x,
                       y,
                       width,
                       height));
#if 1
                bitBlt(mWidget, 
                       x, 
                       y, 
                       mPixmap, 
                       x, 
                       y, 
                       //mPixmap->width(), 
                       //mPixmap->height(), 
                       //mWidget->width(),
                       //mWidget->height(),
                       width,
                       height,
                       Qt::CopyROP);
#endif
                //mPixmap->fill(mWidget, 0, 0);
            }
        }
        else
        {
            result = PR_FALSE;
        }
    
        //NS_RELEASE(event.widget);
    }
    return result;
}


NS_METHOD nsWindow::BeginResizingChildren(void)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::BeginResizingChildren()\n"));
//    gtk_layout_freeze(GTK_LAYOUT(mWidget));
    return NS_OK;
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::EndResizingChildren()\n"));
//    gtk_layout_thaw(GTK_LAYOUT(mWidget));
    return NS_OK;
}

#if 0
PRBool nsWindow::OnResize(nsSizeEvent &aEvent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::OnResize()\n"));
    if (mEventCallback) 
    {
        return DispatchWindowEvent(&aEvent);
    }
    return PR_FALSE;
}
#endif

PRBool nsWindow::OnKey(nsKeyEvent &aEvent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::OnKey()\n"));
    if (mEventCallback) 
    {
        return DispatchWindowEvent(&aEvent);
    }
    return PR_FALSE;
}


PRBool nsWindow::DispatchFocus(nsGUIEvent &aEvent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::DispatchFocus()\n"));
    if (mEventCallback) 
    {
        return DispatchWindowEvent(&aEvent);
    }
    return PR_FALSE;
}

PRBool nsWindow::OnScroll(nsScrollbarEvent &aEvent, PRUint32 cPos)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::OnScroll()\n"));
    return PR_FALSE;
}

NS_METHOD nsWindow::SetMenuBar(nsIMenuBar * aMenuBar)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::SetMenuBar()\n"));
    QWidget *menubar;
    void *voidData;
    aMenuBar->GetNativeData(voidData);
    menubar = (QWidget *) voidData;

    return NS_OK;
}

NS_METHOD nsWindow::ShowMenuBar(PRBool aShow)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::ShowMenuBar()\n"));
    return NS_ERROR_FAILURE; // DRaM
}

NS_METHOD nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWindow::Move %s by (%d,%d)\n",
           mWidget ? mWidget->name() : "(null)",
           aX,
           aY));

#if 1
    return nsWidget::Move(aX, aY);
#else
    if (mIsTopLevel && mShell)
    {
        if (!mParent)
        {
        }
        else
        {
        }
    }
    else if (mWidget)
    {
        mWidget->move(aX, aY);
    }

    return NS_OK;
#endif
}

NS_METHOD nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWindow::Resize %s (%p) to %dx%d\n",
           mWidget ? mWidget->name() : "(null)",
           this,
           aWidth, 
           aHeight));

#if 1
    return nsWidget::Resize(aWidth, aHeight, aRepaint);
#else
    // ignore resizes smaller than or equal to 1x1
    if (aWidth <= 1 || aHeight <= 1)
    {
        return NS_OK;
    }

    mBounds.width  = aWidth;
    mBounds.height = aHeight;

    if (mWidget) 
    {
        mWidget->resize((int)aWidth, (int)aHeight);
        
        // toplevel window?  if so, we should resize it as well.
        if (mIsToplevel && mShell)
        {
        }
        
        if (aRepaint)
        {
            Invalidate(PR_FALSE);
        }
    }
#endif
}

NS_METHOD nsWindow::Resize(PRInt32 aX, 
                           PRInt32 aY, 
                           PRInt32 aWidth,
                           PRInt32 aHeight, 
                           PRBool aRepaint)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::Resize()\n"));
    Resize(aWidth,aHeight,aRepaint);
    Move(aX,aY);
    return NS_OK;
}

//////////////////////////////////////////////////////////////////////

ChildWindow::ChildWindow()
{
}

ChildWindow::~ChildWindow()
{
#ifdef NOISY_DESTROY
  IndentByDepth(stdout);
  printf("ChildWindow::~ChildWindow:%p\n", this);
#endif
}

PRBool ChildWindow::IsChild() const
{
  return PR_TRUE;
}

NS_METHOD ChildWindow::Destroy()
{
#ifdef NOISY_DESTROY
  IndentByDepth(stdout);
  printf("ChildWindow::Destroy:%p  \n", this);
#endif
  // Skip over baseclass Destroy method which doesn't do what we want;
  // instead make sure widget destroy method gets invoked.
  return nsWidget::Destroy();
}
