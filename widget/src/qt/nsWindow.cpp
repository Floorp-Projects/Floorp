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
#include <qapplication.h>
#include <qobjectlist.h>
#include "nslog.h"

NS_IMPL_LOG(nsWindowLog)
#define PRINTF NS_LOG_PRINTF(nsWindowLog)
#define FLUSH  NS_LOG_FLUSH(nsWindowLog)

static bool gAppTopWindowSet = PR_FALSE;

// are we grabbing?
PRBool      nsWindow::mIsGrabbing = PR_FALSE;
nsWindow   *nsWindow::mGrabWindow = NULL;

//=============================================================================
//
// nsQWidget class
//
//=============================================================================
nsQWidget::nsQWidget(nsWidget * widget, QWidget * parent, const char * name, WFlags f)
	     : QWidget(parent, name, f), nsQBaseWidget(widget)
{
}

nsQWidget::~nsQWidget()
{
}

void nsQWidget::Destroy()
{
  nsQBaseWidget::Destroy();
}

NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, nsWidget)

//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow() 
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::nsWindow()\n"));
    mResized = PR_FALSE;
    mVisible = PR_FALSE;
    mDisplayed = PR_FALSE;
    mLowerLeft = PR_FALSE;
    mIsDestroying = PR_FALSE;
    mIsDialog = PR_FALSE;
    mIsPopup = PR_FALSE;
    mOnDestroyCalled = PR_FALSE;
    mFont = nsnull;
    mWindowType = eWindowType_child;
    mBorderStyle = eBorderStyle_default;
    mBlockFocusEvents = PR_FALSE;
}

//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
  PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::~nsWindow()\n"));

  // make sure that we release the grab indicator here
  if (mGrabWindow == this) {
    mIsGrabbing = PR_FALSE;
    mGrabWindow = NULL;
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

NS_METHOD nsWindow::SetFocus()
{
    PR_LOG(QtWidgetsLM,
           PR_LOG_DEBUG,
           ("nsWindow::SetFocus %s\n",
            mWidget ? mWidget->name() : "(null)"));
    if (mWidget) {
        mWidget->setFocus();
    }
    // don't recurse  
    if (mBlockFocusEvents) {
      return NS_OK;
    }

    mBlockFocusEvents = PR_TRUE;

    nsGUIEvent event;

    event.message = NS_GOTFOCUS;
    event.widget  = this;

    event.eventStructType = NS_GUI_EVENT;

    event.time = 0;
    event.point.x = 0;
    event.point.y = 0;

    DispatchFocus(event);
 
    mBlockFocusEvents = PR_FALSE;

    return NS_OK;
}

NS_METHOD nsWindow::PreCreateWidget(nsWidgetInitData *aInitData)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::PreCreateWidget()\n"));
    if (nsnull != aInitData) {
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

    if (mBorderStyle == eBorderStyle_default) {
        w = 0;
    }
    else {
        if (mBorderStyle & eBorderStyle_all) {
            w |= 0;
        }
        if (mBorderStyle & eBorderStyle_border) {
            w |= Qt::WStyle_DialogBorder;
        }
        if (mBorderStyle & eBorderStyle_resizeh) {
            w |= Qt::WStyle_NormalBorder;
        }
        if (mBorderStyle & eBorderStyle_title) {
            w |= Qt::WStyle_Title;
        }
        if (mBorderStyle & eBorderStyle_menu) {
            w |= Qt::WStyle_SysMenu | Qt::WStyle_Title;
        }
        if (mBorderStyle & eBorderStyle_minimize) {
            w |= Qt::WStyle_Minimize;
        }
        if (mBorderStyle & eBorderStyle_maximize) {
            w |= Qt::WStyle_Maximize;
        }
        if (mBorderStyle & eBorderStyle_close) {
            PR_LOG(QtWidgetsLM, 
                   PR_LOG_DEBUG, 
                   ("eBorderStyle_close isn't handled yet... please fix me\n"));
        }
    }

    w |= (w) ? Qt::WStyle_Customize : 0;

    switch (mWindowType) {
      case eWindowType_toplevel:
        w |= Qt::WType_TopLevel | Qt::WDestructiveClose;
        break;

      case eWindowType_dialog:
    	mIsDialog = PR_TRUE;
        w |= Qt::WType_Modal;
        break;

      case eWindowType_popup:
    	mIsPopup = PR_TRUE;
        w |= Qt::WType_Popup;
        break;

      case eWindowType_child:
        break;
    }
    w |= Qt::WRepaintNoErase;
    w |= Qt::WResizeNoErase;

    mWidget = (QWidget*)new nsQWidget(this, parentWidget, QWidget::tr("nsWindow"), w);
    if (!mWidget)
      return NS_ERROR_OUT_OF_MEMORY;

    if (!parentWidget) {
        // This is a top-level window. I'm not sure what special actions need
        // to be taken here.
        mWidget->resize(mBounds.width, mBounds.height);
    	mIsToplevel = PR_TRUE;
        mListenForResizes = PR_TRUE;
        if (!gAppTopWindowSet) {
          qApp->setMainWidget(mWidget);
          gAppTopWindowSet = PR_TRUE;
        }
    }
    else {
      mIsToplevel = PR_FALSE;
    }
    // Force cursor to default setting
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
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::Scroll: (%d,%d)\n",
                                       aDx, aDy));
    if (mWidget) {
        mWidget->scroll(aDx, aDy);
    }

    return NS_OK;
}

NS_METHOD nsWindow::SetTitle(const nsString& aTitle)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::SetTitle()\n"));
    if (!mWidget) {
        return NS_ERROR_FAILURE;
    }
    const char * titleStr = aTitle.ToNewCString();
    mWidget->setCaption(QString::fromLocal8Bit(titleStr));
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
    if (mEventCallback) {

        event.renderingContext = nsnull;
        if (event.rect) {
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
        else {
            PR_LOG(QtWidgetsLM, 
                   PR_LOG_DEBUG, 
                   ("nsWindow::OnPaint: this=%p, NO RECT\n",
                   this));
            x = 0;
            y = 0;
            if (mWidget) {
              width = mWidget->width();
              height = mWidget->height();
            }
            else {
              width = 0;
              height = 0;
            }
        }
        static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
        static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
        if (NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID,
                                                        nsnull,
                                                        kRenderingContextIID,
                                                        (void **)&event.renderingContext)) {
            if (mBounds.width && mBounds.height) {
                event.renderingContext->Init(mContext, this);
                result = DispatchWindowEvent(&event);
                NS_RELEASE(event.renderingContext);
                if (mWidget && mPixmap) {
                    PR_LOG(QtWidgetsLM, 
                           PR_LOG_DEBUG, 
                           ("nsWindow::OnPaint: bitBlt: {%d,%d,%d,%d}\n",
                            x, y, width, height));
                    bitBlt(mWidget, x, y, mPixmap, x, y, width, height, Qt::CopyROP);
                    mPixmap->fill(mWidget, 0, 0);
                }
            }
        }
        else {
            result = PR_FALSE;
        }
    }
    return result;
}


NS_METHOD nsWindow::BeginResizingChildren(void)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::BeginResizingChildren()\n"));
    return NS_OK;
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::EndResizingChildren()\n"));
    return NS_OK;
}

PRBool nsWindow::OnKey(nsKeyEvent &aEvent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::OnKey()\n"));
    if (mEventCallback) {
        return DispatchWindowEvent(&aEvent);
    }
    return PR_FALSE;
}


PRBool nsWindow::DispatchFocus(nsGUIEvent &aEvent)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::DispatchFocus()\n"));
    if (mEventCallback) {
        return DispatchWindowEvent(&aEvent);
    }
    return PR_FALSE;
}

PRBool nsWindow::OnScroll(nsScrollbarEvent &aEvent, PRUint32 cPos)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsWindow::OnScroll()\n"));
    return PR_FALSE;
}

NS_METHOD nsWindow::ConstrainPosition(PRInt32 *aX, PRInt32 *aY)
{
    return NS_OK;
}

NS_METHOD nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
    PR_LOG(QtWidgetsLM, 
           PR_LOG_DEBUG, 
           ("nsWindow::Move %s by (%d,%d)\n",
           mWidget ? mWidget->name() : "(null)",
           aX, aY));

  if (mWidget && mParent && mWindowType == eWindowType_popup) {
    nsRect oldrect, newrect;

    mBounds.x = aX;
    mBounds.y = aY;

    oldrect.x = aX;
    oldrect.y = aY;
    mParent->WidgetToScreen(oldrect, newrect);
    mWidget->move(newrect.x,newrect.y);

    return NS_OK;
  }
  else
    return(nsWidget::Move(aX,aY));
}

NS_IMETHODIMP nsWindow::CaptureRollupEvents(nsIRollupListener * aListener, 
                                            PRBool aDoCapture,
                                            PRBool aConsumeRollupEvent)
{ 
  if (aDoCapture)
  {
     /* Create a pointer region */
     mIsGrabbing = PR_TRUE;
     mGrabWindow = this;

     gRollupConsumeRollupEvent = PR_TRUE;
     gRollupListener = aListener;
     gRollupWidget = getter_AddRefs(NS_GetWeakReference(NS_STATIC_CAST(nsIWidget*,this)));
  }
  else
  {
     // make sure that the grab window is marked as released
     if (mGrabWindow == this) {
       mGrabWindow = NULL;
     }
     mIsGrabbing = PR_FALSE;

     gRollupListener = nsnull;
     gRollupWidget = nsnull;
  }

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
  PRINTF("ChildWindow::~ChildWindow:%p\n", this);
#endif
  if (mEventHandler && mParent) {
      ((nsWidget*)(mParent.get()))->RemoveChildEventHandler(mEventHandler);
  }
}

PRBool ChildWindow::IsChild() const
{
  return PR_TRUE;
}

NS_METHOD ChildWindow::CreateNative(QWidget *parentWidget)
{
  nsresult rv;

  rv = nsWindow::CreateNative(parentWidget);
  if (rv == NS_OK && mEventHandler && mParent)
    ((nsWidget*)(mParent.get()))->AddChildEventHandler(mEventHandler);
  return rv;
}
