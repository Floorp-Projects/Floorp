/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "gtklayout.h"

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

#include "nsGtkEventHandler.h"
#include "nsAppShell.h"

#include "stdio.h"

//#define DBG 0

static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);

extern GtkWidget *gAppContext;

void DoResize(GtkWidget *w, GtkAllocation *allocation, gpointer data);

//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow()
{
  NS_INIT_REFCNT();
  strcpy(gInstanceClassName, "nsWindow");
  mFontMetrics = nsnull;
  mVBox = nsnull;
  mIgnoreResize = PR_FALSE;
  mResized = PR_FALSE;
  mVisible = PR_FALSE;
  mDisplayed = PR_FALSE;
  mLowerLeft = PR_FALSE;
  mFont = nsnull;
}

//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
  OnDestroy();
  gtk_widget_destroy(mWidget);
  if (nsnull != mGC) {
    gdk_gc_destroy(mGC);
    mGC = nsnull;
//    ::XFreeGC((Display *)GetNativeData(NS_NATIVE_DISPLAY),mGC);
  }
}

//-------------------------------------------------------------------------
void nsWindow::ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY)
{

}

//-------------------------------------------------------------------------
//
// Setup initial tooltip rectangles
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[])
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Update all tooltip rectangles
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::UpdateTooltips(nsRect* aNewTips[])
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Remove all tooltip rectangles
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::RemoveTooltips()
{
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Create the native widget
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::CreateNative(GtkWidget *parentWidget)
{
  GtkWidget *mainWindow;

  mWidget = gtk_layout_new(FALSE, FALSE);
  gtk_widget_set_events (mWidget,
                         GDK_BUTTON_PRESS_MASK |
                         GDK_BUTTON_RELEASE_MASK |
                         GDK_POINTER_MOTION_MASK |
                         GDK_EXPOSURE_MASK |
                         GDK_ENTER_NOTIFY_MASK |
                         GDK_LEAVE_NOTIFY_MASK |
                         GDK_KEY_PRESS_MASK |
                         GDK_KEY_RELEASE_MASK);

  if (!parentWidget) {

    mainWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
// VBox for the menu, etc.
    mVBox = gtk_vbox_new(FALSE, 0);
    gtk_widget_show (mVBox);
    gtk_container_add(GTK_CONTAINER(mainWindow), mVBox);
    gtk_box_pack_start(GTK_BOX(mVBox), mWidget, TRUE, TRUE, 0);
  }
  gtk_widget_show(mWidget);
  // Force cursor to default setting
  gtk_widget_set_name(mWidget, "nsWindow");
  mCursor = eCursor_select;
  SetCursor(eCursor_standard);
}


//-------------------------------------------------------------------------
//
// Initialize all the Callbacks
//
//-------------------------------------------------------------------------
void nsWindow::InitCallbacks(char * aName)
{
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "button_press_event",
		     GTK_SIGNAL_FUNC(nsGtkWidget_ButtonPressMask_EventHandler),
		     this);

  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "button_release_event",
		     GTK_SIGNAL_FUNC(nsGtkWidget_ButtonReleaseMask_EventHandler),
		     this);

  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "motion_notify_event",
		     GTK_SIGNAL_FUNC(nsGtkWidget_ButtonMotionMask_EventHandler),
		     this);
/*
  XtAddEventHandler(mWidget,
		    PointerMotionMask,
		    PR_FALSE,
		    nsXtWidget_MotionMask_EventHandler,
		    this);

  XtAddEventHandler(mWidget,
		    EnterWindowMask,
		    PR_FALSE,
		    nsXtWidget_EnterMask_EventHandler,
		    this);
*/
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "enter_notify_event",
		     GTK_SIGNAL_FUNC(nsGtkWidget_EnterMask_EventHandler),
		     this);

  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "leave_notify_event",
		     GTK_SIGNAL_FUNC(nsGtkWidget_LeaveMask_EventHandler),
		     this);

  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "expose_event",
		     GTK_SIGNAL_FUNC(nsGtkWidget_ExposureMask_EventHandler),
		     this);

  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "key_press_event",
		     GTK_SIGNAL_FUNC(nsGtkWidget_KeyPressMask_EventHandler),
		     this);

  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "key_release_event",
		     GTK_SIGNAL_FUNC(nsGtkWidget_KeyReleaseMask_EventHandler),
		     this);

  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "size_allocate",
		     GTK_SIGNAL_FUNC(DoResize),
		     this);
}

//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Show(PRBool bState)
{
  if (bState) {
    if (mVBox) {                  // Toplevel
      gtk_widget_show (mVBox->parent);
    }
  }
  return nsWidget::Show(bState);
}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
  nsWidget::Resize(aWidth, aHeight,aRepaint);
#if 0
  NS_NOTYETIMPLEMENTED("nsWindow::Resize");
  if (DBG) printf("$$$$$$$$$ %s::Resize %d %d   Repaint: %s\n",
                  gInstanceClassName, aWidth, aHeight, (aRepaint?"true":"false"));
  mBounds.width  = aWidth;
  mBounds.height = aHeight;
  // TODO
  // gtk_layout_set_size(GTK_LAYOUT(layout), aWidth, aHeight);
  XtVaSetValues(mWidget, XmNx, mBounds.x, XmNy, mBounds.y, XmNwidth, aWidth, XmNheight, aHeight, nsnull);
#endif
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRUint32 aX, PRUint32 aY, PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
  nsWindow::Resize(aWidth, aHeight, aRepaint);
  nsWidget::Move(aX,aY);
#if 0
  NS_NOTYETIMPLEMENTED("nsWindow::Resize");
  mBounds.x      = aX;
  mBounds.y      = aY;
  mBounds.width  = aWidth;
  mBounds.height = aHeight;
  XtVaSetValues(mWidget, XmNx, aX, XmNy, GetYCoord(aY),
                        XmNwidth, aWidth, XmNheight, aHeight, nsnull);
#endif
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this component dimension
//
//-------------------------------------------------------------------------
void nsWindow::SetBounds(const nsRect &aRect)
{
  mBounds.x      = aRect.x;
  mBounds.y      = aRect.y;
  mBounds.width  = aRect.width;
  mBounds.height = aRect.height;
  //Resize(mBounds.x, mBounds.y, mBounds.width, mBounds.height, PR_TRUE);
}

//-------------------------------------------------------------------------
//
// Get this component dimension
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::GetBounds(nsRect &aRect)
{
  aRect = mBounds;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Invalidate(PRBool aIsSynchronous)
{
#if 0
  if (mWidget == nsnull) {
    return NS_ERROR_FAILURE;
  }

  if (!XtIsRealized(mWidget)) {
    return NS_ERROR_FAILURE;
  }

  Window  win      = XtWindow(mWidget);
  Display *display = XtDisplay(mWidget);


  XEvent evt;
  evt.xgraphicsexpose.type       = GraphicsExpose;
  evt.xgraphicsexpose.send_event = False;
  evt.xgraphicsexpose.display    = display;
  evt.xgraphicsexpose.drawable   = win;
  evt.xgraphicsexpose.x          = 0;
  evt.xgraphicsexpose.y          = 0;
  evt.xgraphicsexpose.width      = mBounds.width;
  evt.xgraphicsexpose.height     = mBounds.height;
  evt.xgraphicsexpose.count      = 0;
  XSendEvent(display, win, False, ExposureMask, &evt);
  XFlush(display);
#endif
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
#if 0
  if (mWidget == nsnull) {
    return NS_ERROR_FAILURE;
  }

  if (!XtIsRealized(mWidget)) {
    return NS_ERROR_FAILURE;
  }

  Window  win      = XtWindow(mWidget);
  Display *display = XtDisplay(mWidget);


  XEvent evt;
  evt.xgraphicsexpose.type       = GraphicsExpose;
  evt.xgraphicsexpose.send_event = False;
  evt.xgraphicsexpose.display    = display;
  evt.xgraphicsexpose.drawable   = win;
  evt.xgraphicsexpose.x          = aRect.x;
  evt.xgraphicsexpose.y          = aRect.y;
  evt.xgraphicsexpose.width      = aRect.width;
  evt.xgraphicsexpose.height     = aRect.height;
  evt.xgraphicsexpose.count      = 0;
  XSendEvent(display, win, False, ExposureMask, &evt);
  XFlush(display);
#endif
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Force a synchronous repaint of the window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Update()
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Create a rendering context from this nsWindow
//
//-------------------------------------------------------------------------
nsIRenderingContext* nsWindow::GetRenderingContext()
{
  nsIRenderingContext * ctx = nsnull;

  if (GetNativeData(NS_NATIVE_WIDGET)) {

    nsresult  res;

    static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
    static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

    res = nsRepository::CreateInstance(kRenderingContextCID, nsnull, kRenderingContextIID, (void **)&ctx);

    if (NS_OK == res)
      ctx->Init(mContext, this);

    NS_ASSERTION(NULL != ctx, "Null rendering context");
  }

  return ctx;
}

//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void *nsWindow::GetNativeData(PRUint32 aDataType)
{
    switch(aDataType) {
      case NS_NATIVE_WINDOW:
	return (void *)GTK_LAYOUT(mWidget)->bin_window;
      case NS_NATIVE_DISPLAY:
	return (void *)GDK_DISPLAY();
      case NS_NATIVE_WIDGET:
	return (void *)mWidget;
      case NS_NATIVE_GRAPHIC:
        {
	      void *res;
	      if (mGC) {
		      res = mGC;
	      } else {
          NS_ASSERTION(mToolkit, "unable to return NS_NATIVE_GRAPHIC");
          res = (void *)((nsToolkit *)mToolkit)->GetSharedGC();
	      }
	      NS_ASSERTION(res, "unable to return NS_NATIVE_GRAPHIC");
	      return res;
	  }
      default:
	break;
    }
    return nsnull;
}

//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetColorMap(nsColorMap *aColorMap)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Return the used device context
//
//-------------------------------------------------------------------------
nsIDeviceContext* nsWindow::GetDeviceContext()
{
  NS_IF_ADDREF(mContext);
  return mContext;
}

//-------------------------------------------------------------------------
//
// Return the used app shell
//
//-------------------------------------------------------------------------
nsIAppShell* nsWindow::GetAppShell()
{
  NS_IF_ADDREF(mAppShell);
  return mAppShell;
}

//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  if (GTK_IS_LAYOUT(mWidget)) {
    GtkAdjustment* horiz = gtk_layout_get_hadjustment(GTK_LAYOUT(mWidget));
    GtkAdjustment* vert = gtk_layout_get_vadjustment(GTK_LAYOUT(mWidget));
    horiz->value -= aDx;
    vert->value -= aDy;
    gtk_signal_emit_by_name(GTK_OBJECT(horiz), "value_changed");
    gtk_signal_emit_by_name(GTK_OBJECT(vert), "value_changed");
  }

#if 0
  if (mWidget == nsnull) {
    return NS_ERROR_FAILURE;
  }

   // Scroll all of the child widgets
  Cardinal numChildren;
  XtVaGetValues(mWidget, XtNnumChildren, &numChildren, nsnull);
  if (numChildren > 0) {
    WidgetList children;
    XtVaGetValues(mWidget, XtNchildren, &children, nsnull);
    int i ;
    for(i = 0; i < numChildren; i++) {
      Position x;
      Position y;
      XtVaGetValues(children[i], XtNx, &x, XtNy, &y, nsnull);

      XtVaSetValues(children[i], XmNx, x + aDx, XmNy, y + aDy, nsnull);
    }
  }

  Window  win      = XtWindow(mWidget);
  Display *display = XtDisplay(mWidget);

  if (nsnull != aClipRect) {
    XCopyArea(display, win, win, XDefaultGC(display, 0),
            aClipRect->x, aClipRect->y,
            aClipRect->XMost(),  aClipRect->YMost(), aDx, aDy);
  }

   // Force a repaint
  XEvent evt;
  evt.xgraphicsexpose.type       = GraphicsExpose;
  evt.xgraphicsexpose.send_event = False;
  evt.xgraphicsexpose.display    = display;
  evt.xgraphicsexpose.drawable   = win;
  if (aDy < 0) {
    evt.xgraphicsexpose.x          = 0;
    evt.xgraphicsexpose.y          = mBounds.height+aDy;
    evt.xgraphicsexpose.width      = mBounds.width;
    evt.xgraphicsexpose.height     = -aDy;
  } else {
    evt.xgraphicsexpose.x          = 0;
    evt.xgraphicsexpose.y          = 0;
    evt.xgraphicsexpose.width      = mBounds.width;
    evt.xgraphicsexpose.height     = aDy;
  }
  evt.xgraphicsexpose.count      = 0;
  XSendEvent(display, win, False, ExposureMask, &evt);
  XFlush(display);
#endif
  return NS_OK;
}

NS_METHOD nsWindow::SetBorderStyle(nsBorderStyle aBorderStyle)
{
  return NS_OK;
}

NS_METHOD nsWindow::SetTitle(const nsString& aTitle)
{
  if (mVBox) // Top level widget (has correct parent)
  {
    char * titleStr = aTitle.ToNewCString();
    gtk_window_set_title(GTK_WINDOW(mVBox->parent), titleStr);
    delete[] titleStr;
  }
  return NS_OK;
}


/**
 * Processes an Expose Event
 *
 **/
PRBool nsWindow::OnPaint(nsPaintEvent &event)
{
  nsresult result ;

  // call the event callback
  if (mEventCallback) {

    nsRect rr ;

    /*
     * Maybe  ... some day ... somone will pull the invalid rect
     * out of the paint message rather than drawing the whole thing...
     */
    GetBounds(rr);

    rr.x = 0;
    rr.y = 0;

    event.rect = &rr;

    event.renderingContext = nsnull;
    static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
    static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
    if (NS_OK == nsRepository::CreateInstance(kRenderingContextCID,
					      nsnull,
					      kRenderingContextIID,
					      (void **)&event.renderingContext))
      {
        event.renderingContext->Init(mContext, this);
        result = DispatchWindowEvent(&event);
        NS_RELEASE(event.renderingContext);
      }
    else
      {
        result = PR_FALSE;
      }
  }
  return result;
}


NS_METHOD nsWindow::BeginResizingChildren(void)
{
  return NS_OK;
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
  return NS_OK;
}


void nsWindow::OnDestroy()
{
    // release references to children, device context, toolkit, and app shell
  //XXX: Why is there a problem releasing these
  // NS_IF_RELEASE(mChildren);
  //  NS_IF_RELEASE(mContext);
  //   NS_IF_RELEASE(mToolkit);
  //   NS_IF_RELEASE(mAppShell);
}

PRBool nsWindow::OnResize(nsSizeEvent &aEvent)
{
  nsRect* size = aEvent.windowSize;

  if (mEventCallback && !mIgnoreResize) {
    return DispatchWindowEvent(&aEvent);
  }
  return FALSE;
}

PRBool nsWindow::OnKey(PRUint32 aEventType, PRUint32 aKeyCode, nsKeyEvent* aEvent)
{
  if (mEventCallback) {
    return DispatchWindowEvent(aEvent);
  }
  else
   return FALSE;
}


PRBool nsWindow::DispatchFocus(nsGUIEvent &aEvent)
{
  if (mEventCallback) {
    return DispatchWindowEvent(&aEvent);
  }
 return FALSE;
}

PRBool nsWindow::OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos)
{
  return FALSE;
}

void nsWindow::SetIgnoreResize(PRBool aIgnore)
{
  mIgnoreResize = aIgnore;
}

PRBool nsWindow::IgnoreResize()
{
  return mIgnoreResize;
}

void nsWindow::SetResizeRect(nsRect& aRect)
{
  mResizeRect = aRect;
}

void nsWindow::GetResizeRect(nsRect* aRect)
{
  aRect->x = mResizeRect.x;
  aRect->y = mResizeRect.y;
  aRect->width = mResizeRect.width;
  aRect->height = mResizeRect.height;
}

void nsWindow::SetResized(PRBool aResized)
{
  mResized = aResized;
}

PRBool nsWindow::GetResized()
{
  return(mResized);
}

void nsWindow::UpdateVisibilityFlag()
{
  GtkWidget *parent = mWidget->parent;

  if (parent) {
    mVisible = GTK_WIDGET_VISIBLE(parent);
  }
    /*
    PRUint32 pWidth = 0;
    PRUint32 pHeight = 0;
    XtVaGetValues(parent, XmNwidth, &pWidth, XmNheight, &pHeight, nsnull);
    if ((mBounds.y + mBounds.height) > pHeight) {
      mVisible = PR_FALSE;
      return;
    }

    if (mBounds.y < 0)
     mVisible = PR_FALSE;
  }

  mVisible = PR_TRUE;
  */
}

void nsWindow::UpdateDisplay()
{
    // If not displayed and needs to be displayed
  if ((PR_FALSE==mDisplayed) &&
     (PR_TRUE==mShown) &&
     (PR_TRUE==mVisible)) {
    gtk_widget_show(mWidget);
    mDisplayed = PR_TRUE;
  }

    // Displayed and needs to be removed
  if (PR_TRUE==mDisplayed) {
    if ((PR_FALSE==mShown) || (PR_FALSE==mVisible)) {
      gtk_widget_hide(mWidget);
      //XtUnmanageChild(mWidget);
      mDisplayed = PR_FALSE;
    }
  }
}

PRUint32 nsWindow::GetYCoord(PRUint32 aNewY)
{
  if (PR_TRUE==mLowerLeft) {
    return(aNewY - 12 /*KLUDGE fix this later mBounds.height */);
  }
  return(aNewY);
}


//
//-----------------------------------------------------
// Resize handler code for child and main windows.
//-----------------------------------------------------
//

gint ResetResize(gpointer call_data)
{
    nsWindow* widgetWindow = (nsWindow*)call_data;
    widgetWindow->SetResized(PR_FALSE);
    return FALSE;
}

gint DoRefresh(gpointer call_data)
{
    nsWindow *win = (nsWindow*)call_data;
    
    nsRect bounds;
    win->GetResizeRect(&bounds);

    nsSizeEvent sizeEvent;
    sizeEvent.eventStructType = NS_SIZE_EVENT;
    sizeEvent.message         = NS_SIZE;
    sizeEvent.point.x         = 0;
    sizeEvent.point.y         = 0;
    sizeEvent.time            = PR_IntervalNow();

    // nsGUIEvent
    sizeEvent.widget          = win;
    sizeEvent.nativeMsg       = nsnull;

    // nsSizeEvent
    sizeEvent.windowSize     = &bounds;
    sizeEvent.mWinWidth      = bounds.width;
    sizeEvent.mWinHeight     = bounds.height;
 
    win->SetBounds(bounds);
    win->OnResize(sizeEvent);

    nsPaintEvent pevent;
    pevent.message = NS_PAINT;
    pevent.widget = win;
    pevent.time = PR_IntervalNow();
    pevent.rect = (nsRect *)&bounds;
    win->OnPaint(pevent);

    gtk_timeout_add(10, (GtkFunction)ResetResize, win);
    return FALSE;
}


void DoResize(GtkWidget *w, GtkAllocation *allocation, gpointer data)
{
//  g_print("DoResized called\n");
  nsWindow *win = (nsWindow*)data;

  gtk_layout_freeze(GTK_LAYOUT(w));

  nsRect bounds;
  bounds.width = allocation->width;
  bounds.height = allocation->height;
  bounds.x = 0;
  bounds.y = 0;
  win->SetResizeRect(bounds);

  if (!win->GetResized()) {
    if (win->IsChild()) {
      DoRefresh(win);
    }
    else {
      gtk_timeout_add(250, (GtkFunction)DoRefresh, win);
    }
  }

  win->SetResized(PR_TRUE);
  gtk_layout_thaw(GTK_LAYOUT(w));
}

NS_METHOD nsWindow::SetMenuBar(nsIMenuBar * aMenuBar)
{
  GtkWidget *menubar;
  void *voidData;

  aMenuBar->GetNativeData(voidData);
  menubar = GTK_WIDGET(voidData);

//  gtk_menu_bar_set_shadow_type (GTK_MENU_BAR(menubar), GTK_SHADOW_NONE);
  
  gtk_box_pack_start(GTK_BOX(mVBox), menubar, FALSE, FALSE, 0);
  gtk_box_reorder_child(GTK_BOX(mVBox), menubar, 0);
  printf("adding menu bar (%p) to vbox (%p)\n", menubar, mVBox);
  return NS_OK;
}

/**
 *
 *
 **/
NS_METHOD nsWindow::GetClientBounds(nsRect &aRect)
{
  return GetBounds(aRect);
}


/**
 * Calculates the border width and height
 *
 **/
NS_METHOD nsWindow::GetBorderSize(PRInt32 &aWidth, PRInt32 &aHeight)
{
  nsRect rectWin;
  nsRect rectClient;
  GetBounds(rectWin);
  GetClientBounds(rectClient);

  aWidth  = rectWin.width - rectClient.width;
  aHeight = rectWin.height - rectClient.height;

  return NS_OK;
}
