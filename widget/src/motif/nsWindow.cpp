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

#include "nsWindow.h"
#include "nsIFontMetrics.h"
#include "nsIFontCache.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsTransform2D.h"
#include "nsGfxCIID.h"

#include "nsXtEventHandler.h"

#include "X11/Xlib.h"
#include "Xm/Xm.h"
#include "Xm/Xm.h"
#include "Xm/MainW.h"
#include "Xm/Frame.h"
#include "Xm/XmStrDefs.h"
#include "Xm/DrawingA.h"

#include "X11/Intrinsic.h"

#include "stdio.h"

#define DBG 0

static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);

//NS_IMPL_QUERY_INTERFACE(nsWindow, kIWidgetIID)
//NS_IMPL_ADDREF(nsWindow)
//NS_IMPL_RELEASE(nsWindow)

/**
 *
 */
nsrefcnt nsWindow::AddRefObject(void) { 
  return ++mRefCnt; 
}

/**
 *
 */
nsrefcnt nsWindow::ReleaseObject(void) { 
  return (--mRefCnt) ? mRefCnt : (delete this, 0); 
}


/**
 *
 */
nsresult nsWindow::QueryObject(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }

    static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
    if (aIID.Equals(kIWidgetIID)) {
        *aInstancePtr = (void*) ((nsISupports*)this);
        AddRef();
        return NS_OK;
    }

    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports*)&mInner);
        AddRef();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}

/**
 * nsISupports methods
 */
nsresult nsWindow::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    return mOuter->QueryInterface(aIID, aInstancePtr);
}

/**
 *
 */
nsrefcnt nsWindow::AddRef(void)
{
    return mOuter->AddRef();
}

/**
 *
 */
nsrefcnt nsWindow::Release(void)
{
    return NS_RELEASE(mOuter);
}


void nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
}

void nsWindow::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
} 

//-------------------------------------------------------------------------
//
// Setup initial tooltip rectangles
//
//-------------------------------------------------------------------------
void nsWindow::SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[])
{
}

//-------------------------------------------------------------------------
//
// Update all tooltip rectangles
//
//-------------------------------------------------------------------------

void nsWindow::UpdateTooltips(nsRect* aNewTips[])
{
}

//-------------------------------------------------------------------------
//
// Remove all tooltip rectangles
//
//-------------------------------------------------------------------------

void nsWindow::RemoveTooltips()
{
}


//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow(nsISupports *aOuter):
  mEventListener(nsnull),
  mMouseListener(nsnull),
  mToolkit(nsnull),
  mFontMetrics(nsnull),
  mContext(nsnull),
  mWidget(nsnull),
  mEventCallback(nsnull),
  mIgnoreResize(PR_FALSE)
{
  strcpy(gInstanceClassName, "nsWindow");
  // XXX Til can deal with ColorMaps!
  SetForegroundColor(1);
  SetBackgroundColor(2);
  mBounds.x = 0;
  mBounds.y = 0;
  mBounds.width = 0;
  mBounds.height = 0;
  mResized = PR_FALSE;
  if (aOuter)
    mOuter = aOuter;
  else
    mOuter = &mInner;
}


//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
}


//-------------------------------------------------------------------------
//
// Create a window.
//
// Note: aNativeParent is always non-null if aWidgetParent is non-null.
// aNativeaParent is set regardless if the parent for the Create() was an 
// nsIWidget or a Native widget. 
// aNativeParent is equal to aWidgetParent->GetNativeData(NS_NATIVE_WIDGET)
//-------------------------------------------------------------------------

void nsWindow::CreateWindow(nsNativeWindow aNativeParent, 
                      nsIWidget *aWidgetParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  Widget mainWindow = 0, frame = 0;
  Widget parentWidget = 0;
  mBounds = aRect;

  if (nsnull == mToolkit) {
    if (nsnull != aToolkit) {
      mToolkit = (nsToolkit*)aToolkit;
      mToolkit->AddRef();
    }
    else {
      if (nsnull != aWidgetParent) {
	mToolkit = (nsToolkit*)(aWidgetParent->GetToolkit()); // the call AddRef's, we don't have to
      }
      // it's some top level window with no toolkit passed in.
      // Create a default toolkit with the current thread
      else {
	mToolkit = new nsToolkit();
	mToolkit->AddRef();
	mToolkit->Init(PR_GetCurrentThread());
      }
    }
    
  }
  
  // save the event callback function
  mEventCallback = aHandleEventFunction;
  
  // keep a reference to the toolkit object
  if (aContext) {
    mContext = aContext;
    mContext->AddRef();
  }
  else {
    nsresult  res;
    
    // XXX Move this! - For some reason Registering in another DLL (shell) isn't working
        
#define GFXWIN_DLL "libgfxunix.so"

    /*static NS_DEFINE_IID(kCRenderingContextIID, NS_RENDERING_CONTEXT_CID);
    static NS_DEFINE_IID(kCDeviceContextIID, NS_DEVICE_CONTEXT_CID);
    static NS_DEFINE_IID(kCFontMetricsIID, NS_FONT_METRICS_CID);
    static NS_DEFINE_IID(kCImageIID, NS_IMAGE_CID);
    
    NSRepository::RegisterFactory(kCRenderingContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCDeviceContextIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCFontMetricsIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
    NSRepository::RegisterFactory(kCImageIID, GFXWIN_DLL, PR_FALSE, PR_FALSE);
    
   */ 
    
    static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
    static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

    //res = !NS_OK;
    res = NSRepository::CreateInstance(kDeviceContextCID, 
				       nsnull, 
				       kDeviceContextIID, 
				       (void **)&mContext);
    if (NS_OK == res) {
      mContext->Init(nsnull);
    }
  }
  
  if (0==aNativeParent) {
    parentWidget = (Widget) aInitData ;
  }
  else
    parentWidget = (Widget)aNativeParent;
  
  Widget frameParent = 0;
  if (!aNativeParent) {
    mainWindow = ::XtVaCreateManagedWidget("mainWindow",
					   xmMainWindowWidgetClass,
					   parentWidget, 
  					   XmNwidth, aRect.width,
  					   XmNheight, aRect.height,
  					   nsnull);

    frameParent = mainWindow;

  }
  else
   frameParent = (Widget)aNativeParent;

  frame = ::XtVaCreateManagedWidget("frame",
				    xmDrawingAreaWidgetClass,
				    frameParent,
				    XmNwidth, aRect.width,
				    XmNheight, aRect.height,
				    XmNmarginHeight, 0,
				    XmNmarginWidth, 0,
				    nsnull);

  mWidget = frame ;


  if (mainWindow) {
    XmMainWindowSetAreas (mainWindow, nsnull, nsnull, nsnull, nsnull, frame);
  }
    
  if (aWidgetParent) {
    aWidgetParent->AddChild(this);
  }

  // Force cursor to default setting
  mCursor = eCursor_select;
  SetCursor(eCursor_standard);

  InitCallbacks();

  XtAddCallback(mWidget,
                XmNresizeCallback,
                nsXtWidget_Resize_Callback,
                this);

  /*XtAddCallback(mWidget,
                XmNexposeCallback,
                nsXtWidget_Expose_Callback,
                this);*/


}



//-------------------------------------------------------------------------
//
// Initialize all the Callbacks
//
//-------------------------------------------------------------------------
void nsWindow::InitCallbacks(char * aName)
{
  if (DBG) fprintf(stderr, "Setting Up Callbacks 0x%x [%s]\n", this, (aName != nsnull?aName:"<unknown>"));
  // setup the event Handlers
  XtAddEventHandler(mWidget, 
		    ButtonPressMask, 
		    PR_FALSE, 
		    nsXtWidget_ButtonPressMask_EventHandler,
		    this);

  XtAddEventHandler(mWidget, 
		    ButtonReleaseMask, 
		    PR_FALSE, 
		    nsXtWidget_ButtonReleaseMask_EventHandler,
		    this);

  XtAddEventHandler(mWidget, 
		    ButtonMotionMask, 
		    PR_FALSE, 
		    nsXtWidget_ButtonMotionMask_EventHandler,
		    this);

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

  XtAddEventHandler(mWidget, 
		    LeaveWindowMask, 
		    PR_FALSE, 
		    nsXtWidget_LeaveMask_EventHandler,
		    this);

  XtAddEventHandler(mWidget, 
		    ExposureMask, 
		    PR_TRUE, 
		    nsXtWidget_ExposureMask_EventHandler,
		    this);

  /*XtAddEventHandler(mWidget, 
                    ResizeRedirectMask,
                    PR_FALSE, 
                    nsXtWidget_ResizeRedirectMask_EventHandler,
                    this);*/

  /*XtAddCallback(mWidget, 
                XmNresizeCallback, 
                nsXtWidget_Resize_Callback, 
                NULL);*/


}


//-------------------------------------------------------------------------
//
// create with nsIWidget parent
//
//-------------------------------------------------------------------------

void nsWindow::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
    CreateWindow((nsNativeWindow)((aParent) ? aParent->GetNativeData(NS_NATIVE_WIDGET) : 0), aParent, aRect, aHandleEventFunction, aContext, aToolkit, aInitData);
}

//-------------------------------------------------------------------------
//
// create with a native parent
//
//-------------------------------------------------------------------------
void nsWindow::Create(nsNativeWindow aParent,
                         const nsRect &aRect,
                         EVENT_CALLBACK aHandleEventFunction,
                         nsIDeviceContext *aContext,
                         nsIToolkit *aToolkit,
                         nsWidgetInitData *aInitData)
{
    CreateWindow(aParent, 0, aRect, aHandleEventFunction, aContext, aToolkit, aInitData);
}


//-------------------------------------------------------------------------
//
// Close this nsWindow
//
//-------------------------------------------------------------------------
void nsWindow::Destroy()
{
}


//-------------------------------------------------------------------------
//
// Get this nsWindow parent
//
//-------------------------------------------------------------------------
nsIWidget* nsWindow::GetParent(void)
{
  return nsnull;
}


//-------------------------------------------------------------------------
//
// Get this nsWindow's list of children
//
//-------------------------------------------------------------------------
nsIEnumerator* nsWindow::GetChildren()
{
    return nsnull;
}


//-------------------------------------------------------------------------
//
// Add a child to the list of children
//
//-------------------------------------------------------------------------
void nsWindow::AddChild(nsIWidget* aChild)
{
}


//-------------------------------------------------------------------------
//
// Remove a child from the list of children
//
//-------------------------------------------------------------------------
void nsWindow::RemoveChild(nsIWidget* aChild)
{
}


//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------
void nsWindow::Show(PRBool bState)
{
  if (bState)
    XtManageChild(mWidget);
  else
    XtUnmanageChild(mWidget);
}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
void nsWindow::Move(PRUint32 aX, PRUint32 aY)
{
  XtVaSetValues(mWidget, XmNx, aX, XmNy, aY, nsnull);

}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
void nsWindow::Resize(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
  if (DBG) printf("$$$$$$$$$ %s::Resize %d %d   Repaint: %s\n", 
                  gInstanceClassName, aWidth, aHeight, (aRepaint?"true":"false"));
  mBounds.width  = aWidth;
  mBounds.height = aHeight;
  XtVaSetValues(mWidget, XmNwidth, aWidth, XmNheight, aHeight, nsnull);
}

    
//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
void nsWindow::Resize(PRUint32 aX, PRUint32 aY, PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
  mBounds.x      = aX;
  mBounds.y      = aY;
  mBounds.width  = aWidth;
  mBounds.height = aHeight;
  XtVaSetValues(mWidget, XmNx, aX, XmNy, aY, 
                         XmNwidth, aWidth, XmNheight, aHeight, nsnull);
}

    
//-------------------------------------------------------------------------
//
// Enable/disable this component
//
//-------------------------------------------------------------------------
void nsWindow::Enable(PRBool bState)
{
}

    
//-------------------------------------------------------------------------
//
// Give the focus to this component
//
//-------------------------------------------------------------------------
void nsWindow::SetFocus(void)
{
}

    
//-------------------------------------------------------------------------
//
// Get this component dimension
//
//-------------------------------------------------------------------------
void nsWindow::SetBounds(const nsRect &aRect)
{
 mBounds = aRect;
}

//-------------------------------------------------------------------------
//
// Get this component dimension
//
//-------------------------------------------------------------------------
void nsWindow::GetBounds(nsRect &aRect)
{

  /*XWindowAttributes attrs ;
  Window w = nsnull;

  if (mWidget)
    w = ::XtWindow(mWidget);
  
  if (mWidget && w) {
    
    XWindowAttributes attrs ;
    
    Display * d = ::XtDisplay(mWidget);
    
    XGetWindowAttributes(d, w, &attrs);
    
    aRect.x = attrs.x ;
    aRect.y = attrs.y ;
    aRect.width = attrs.width ;
    aRect.height = attrs.height;
    
 } else {
   //printf("Bad bounds computed for nsIWidget\n");

   // XXX If this code gets hit, one should question why and how
   // and fix it there.
    aRect = mBounds;
  
  }
  */
  aRect = mBounds;
}

    
//-------------------------------------------------------------------------
//
// Get the foreground color
//
//-------------------------------------------------------------------------
nscolor nsWindow::GetForegroundColor(void)
{
  return (mForeground);
}

    
//-------------------------------------------------------------------------
//
// Set the foreground color
//
//-------------------------------------------------------------------------
void nsWindow::SetForegroundColor(const nscolor &aColor)
{
  mForeground = aColor;
}

    
//-------------------------------------------------------------------------
//
// Get the background color
//
//-------------------------------------------------------------------------
nscolor nsWindow::GetBackgroundColor(void)
{
  return (mBackground);
}

    
//-------------------------------------------------------------------------
//
// Set the background color
//
//-------------------------------------------------------------------------
void nsWindow::SetBackgroundColor(const nscolor &aColor)
{
  mBackground = aColor ;
}

    
//-------------------------------------------------------------------------
//
// Get this component font
//
//-------------------------------------------------------------------------
nsIFontMetrics* nsWindow::GetFont(void)
{
    NS_NOTYETIMPLEMENTED("GetFont not yet implemented"); // to be implemented
    return nsnull;
}

    
//-------------------------------------------------------------------------
//
// Set this component font
//
//-------------------------------------------------------------------------
void nsWindow::SetFont(const nsFont &aFont)
{
}

    
//-------------------------------------------------------------------------
//
// Get this component cursor
//
//-------------------------------------------------------------------------
nsCursor nsWindow::GetCursor()
{
}

    
//-------------------------------------------------------------------------
//
// Set this component cursor
//
//-------------------------------------------------------------------------

void nsWindow::SetCursor(nsCursor aCursor)
{
}
    
//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
void nsWindow::Invalidate(PRBool aIsSynchronous)
{
}


//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void* nsWindow::GetNativeData(PRUint32 aDataType)
{
    switch(aDataType) {

        case NS_NATIVE_WINDOW:
	  {
              return (void*)XtWindow(mWidget);
	  }
	break;
        case NS_NATIVE_DISPLAY:
	  {
            return (void*)XtDisplay(mWidget);
	  }
	break;
        case NS_NATIVE_WIDGET:
	  {
            return (void*)(mWidget);
	  }
	break;
        case NS_NATIVE_GRAPHIC:
	  {
	    XGCValues values;
	    GC aGC;
  
	    aGC= ::XtGetGC(mWidget, 
			   nsnull,
			   &values);

            return (void*)aGC;
	  }
	break;
        case NS_NATIVE_COLORMAP:
        default:
            break;
    }

    return NULL;

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
    
    res = NSRepository::CreateInstance(kRenderingContextCID, nsnull, kRenderingContextIID, (void **)&ctx);
    
    if (NS_OK == res)
      ctx->Init(mContext, this);
    
    NS_ASSERTION(NULL != ctx, "Null rendering context");
  }
  
  return ctx;

}

//-------------------------------------------------------------------------
//
// Return the toolkit this widget was created on
//
//-------------------------------------------------------------------------
nsIToolkit* nsWindow::GetToolkit()
{
}


//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
void nsWindow::SetColorMap(nsColorMap *aColorMap)
{
}

//-------------------------------------------------------------------------
//
// Return the used device context
//
//-------------------------------------------------------------------------
nsIDeviceContext* nsWindow::GetDeviceContext() 
{ 
  return mContext;
}


//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
void nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  if (mWidget == nsnull) {
    return;
  }

  Window  win      = XtWindow(mWidget);
  Display *display = XtDisplay(mWidget);
  
  XCopyArea(display, win, win, XDefaultGC(display, 0), 
            aClipRect->x, aClipRect->y, 
            aClipRect->XMost(),  aClipRect->YMost(), aDx, aDy);

#if 1
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
  if (DBG) printf("Forcing repaint %d %d %d %d\n", evt.xgraphicsexpose.x, 
                                          evt.xgraphicsexpose.y, 
                                          evt.xgraphicsexpose.width, 
                                          evt.xgraphicsexpose.height);

  XSendEvent(display, win, False, ExposureMask, &evt);
  XFlush(display);
#endif
}





void nsWindow::SetBorderStyle(nsBorderStyle aBorderStyle) 
{
} 

void nsWindow::SetTitle(const nsString& aTitle) 
{
} 


/**
 * Processes a mouse pressed event
 *
 **/
void nsWindow::AddMouseListener(nsIMouseListener * aListener)
{
}

/**
 * Processes a mouse pressed event
 *
 **/
void nsWindow::AddEventListener(nsIEventListener * aListener)
{
}

PRBool nsWindow::ConvertStatus(nsEventStatus aStatus)
{
  switch(aStatus) {
    case nsEventStatus_eIgnore:
      return(PR_FALSE);
    break;
    case nsEventStatus_eConsumeNoDefault:
      return(PR_TRUE);
    break;
    case nsEventStatus_eConsumeDoDefault:
      return(PR_FALSE);
    break;
    default:
      NS_ASSERTION(0, "Illegal nsEventStatus enumeration value");
      return(PR_FALSE);
    break;
  }
}

//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------

PRBool nsWindow::DispatchEvent(nsGUIEvent* event)
{
  PRBool result = PR_FALSE;
 
  if (nsnull != mEventCallback) {
    result = ConvertStatus((*mEventCallback)(event));
  }
    // Dispatch to event listener if event was not consumed
  if ((result != PR_TRUE) && (nsnull != mEventListener)) {
    return ConvertStatus(mEventListener->ProcessEvent(*event));
  }
  else {
    return(result); 
  }
}

//-------------------------------------------------------------------------
//
// Deal with all sort of mouse event
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchMouseEvent(nsMouseEvent aEvent)
{
  PRBool result = PR_FALSE;
  if (nsnull == mEventCallback && nsnull == mMouseListener) {
    return result;
  }


  // call the event callback 
  if (nsnull != mEventCallback) {

    result = DispatchEvent(&aEvent);

    //printf("**result=%d%\n",result);
    /*if (aEventType == NS_MOUSE_MOVE) {

      //MouseTrailer * mouseTrailer = MouseTrailer::GetMouseTrailer(0);
      //MouseTrailer::SetMouseTrailerWindow(this);
      //mouseTrailer->CreateTimer();

      nsRect rect;
      GetBounds(rect);
      rect.x = 0;
      rect.y = 0;
      //printf("Rect[%d, %d, %d, %d]  Point[%d,%d]\n", rect.x, rect.y, rect.width, rect.height, event.position.x, event.position.y);
      //printf("mCurrentWindow 0x%X\n", mCurrentWindow);

      if (rect.Contains(event.point.x, event.point.y)) {
        if (mCurrentWindow == NULL || mCurrentWindow != this) {
          if ((nsnull != mCurrentWindow) && (!mCurrentWindow->mIsDestroying)) {
            mCurrentWindow->DispatchMouseEvent(NS_MOUSE_EXIT);
          }
          mCurrentWindow = this;
          mCurrentWindow->DispatchMouseEvent(NS_MOUSE_ENTER);
        }
      } 
    } else if (aEventType == NS_MOUSE_EXIT) {
      if (mCurrentWindow == this) {
        mCurrentWindow = nsnull;
      }
    }*/

    return result;
  }

  /*if (nsnull != mMouseListener) {
    switch (aEventType) {
      case NS_MOUSE_MOVE: {
        result = ConvertStatus(mMouseListener->MouseMoved(event));
        nsRect rect;
        GetBounds(rect);
        if (rect.Contains(event.point.x, event.point.y)) {
          if (mCurrentWindow == NULL || mCurrentWindow != this) {
            //printf("Mouse enter");
            mCurrentWindow = this;
          }
        } else {
          //printf("Mouse exit");
        }

      } break;

      case NS_MOUSE_LEFT_BUTTON_DOWN:
      case NS_MOUSE_MIDDLE_BUTTON_DOWN:
      case NS_MOUSE_RIGHT_BUTTON_DOWN:
        result = ConvertStatus(mMouseListener->MousePressed(event));
        break;

      case NS_MOUSE_LEFT_BUTTON_UP:
      case NS_MOUSE_MIDDLE_BUTTON_UP:
      case NS_MOUSE_RIGHT_BUTTON_UP:
        result = ConvertStatus(mMouseListener->MouseReleased(event));
        result = ConvertStatus(mMouseListener->MouseClicked(event));
        break;
    } // switch
  } */
  return result;
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

    GetBounds(rr);
    
    event.rect = &rr;

    event.renderingContext = nsnull;
    static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
    static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
    
    if (NS_OK == NSRepository::CreateInstance(kRenderingContextCID, 
					      nsnull, 
					      kRenderingContextIID, 
					      (void **)&event.renderingContext))
      {
	event.renderingContext->Init(mContext, this);
	result = DispatchEvent(&event);
	NS_RELEASE(event.renderingContext);
      }
    else 
      {
        result = PR_FALSE;
      }
  }
  return result;
}


void nsWindow::BeginResizingChildren(void)
{
}

void nsWindow::EndResizingChildren(void)
{
}


void nsWindow::OnDestroy()
{
}

PRBool nsWindow::OnResize(nsSizeEvent &aEvent)
{
    nsRect* size = aEvent.windowSize;
  /*if (mWidget) {
    Arg arg[3];
    printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ Setting to 600,800\n");
    XtSetArg(arg[0], XtNwidth, 600);
    XtSetArg(arg[1], XtNheight,800);

    XtSetValues(mWidget, arg, 2);

  }*/

  if (mEventCallback && !mIgnoreResize) {
printf("IN ON Resize %d %d %d %d\n",size->x, size->y, size->width, size->height);
    return(DispatchEvent(&aEvent));
  }
  return FALSE;
}

PRBool nsWindow::OnKey(PRUint32 aEventType, PRUint32 aKeyCode)
{
 return FALSE;
}


PRBool nsWindow::DispatchFocus(PRUint32 aEventType)
{
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


