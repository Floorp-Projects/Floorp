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
#include "Xm/MainW.h"
#include "Xm/Frame.h"
#include "Xm/XmStrDefs.h"
#include "Xm/DrawingA.h"

#include "X11/Intrinsic.h"
#include "X11/cursorfont.h"

#include "stdio.h"

#include "Xm/DialogS.h"
#include "Xm/RowColumn.h"
#include "Xm/Form.h"

#define DBG 0

#define SET_VALUES 1

//#if defined(LINUX)
//#undef SET_VALUES
//#endif

Widget gFirstTopLevelWindow = 0;

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
    return mOuter->Release();
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

   // Setup for possible aggregation
  if (aOuter)
    mOuter = aOuter;
  else
    mOuter = &mInner;
  mRefCnt = 1; // FIXTHIS 

  mGC = nsnull ;
  mShown = PR_FALSE;
  mVisible = PR_FALSE;
  mDisplayed = PR_FALSE;
  mLowerLeft = PR_FALSE;
  mCursor = eCursor_standard;
  mClientData = nsnull;
}


//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
  XtDestroyWidget(mWidget);
  if (nsnull != mGC) {
    ::XFreeGC((Display *)GetNativeData(NS_NATIVE_DISPLAY),mGC);
    mGC = nsnull;
  }
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
void nsWindow::InitToolkit(nsIToolkit *aToolkit,
                           nsIWidget  *aWidgetParent) 
{
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

        // Create a shared GC for all widgets
        ((nsToolkit *)mToolkit)->SetSharedGC((GC)GetNativeData(NS_NATIVE_GRAPHIC));
      }
    }
  }

}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
void nsWindow::InitDeviceContext(nsIDeviceContext *aContext,
                                 Widget aParentWidget) 
{

  // keep a reference to the toolkit object
  if (aContext) {
    mContext = aContext;
    mContext->AddRef();
  }
  else {
    nsresult  res;

    static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
    static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

    //res = !NS_OK;
    res = NSRepository::CreateInstance(kDeviceContextCID,
                                       nsnull, 
                                       kDeviceContextIID, 
                                       (void **)&mContext);
    if (NS_OK == res) {
      mContext->Init(aParentWidget);
    }
  }

}


void nsWindow::CreateGC()
{
  // Create a Writeable GC for this Widget. Unfortunatley, 
  // the Window for the Widget is not created properly at this point and
  // we Need the GC prior to the Rendering Context getting created, so
  // we create a small dummy window of the default depth as our dummy Drawable
  // to create a compatible GC

  if (nsnull == mGC) {

    XGCValues values;
    Window w;
    Display * d = ::XtDisplay(mWidget);
    
    w = ::XCreateSimpleWindow(d,
			      RootWindow(d,DefaultScreen(d)),
			      0,0,1,1,0,
			      BlackPixel(d,DefaultScreen(d)),
			      WhitePixel(d,DefaultScreen(d)));
    mGC = ::XCreateGC(d, w, nsnull, &values);
    
    ::XDestroyWindow(d,w);
  }
}

void nsWindow::CreateMainWindow(nsNativeWidget aNativeParent, 
                      nsIWidget *aWidgetParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  Widget mainWindow = 0, frame = 0;
  mBounds = aRect;
  mAppShell = aAppShell;

  InitToolkit(aToolkit, aWidgetParent);
  
  // save the event callback function
  mEventCallback = aHandleEventFunction;

  InitDeviceContext(aContext, 
                    (Widget) aAppShell->GetNativeData(NS_NATIVE_SHELL));
  
  Widget frameParent = 0;

  if (gFirstTopLevelWindow == 0) {
    mainWindow = ::XtVaCreateManagedWidget("mainWindow",
        xmMainWindowWidgetClass,
        (Widget) aAppShell->GetNativeData(NS_NATIVE_SHELL), 
        nsnull);
    gFirstTopLevelWindow = mainWindow;
  }
  else {
    Widget shell = ::XtVaCreatePopupShell(" ",
        xmDialogShellWidgetClass,
        (Widget) aAppShell->GetNativeData(NS_NATIVE_SHELL), 0);
    XtVaSetValues(shell, 
        XmNwidth, aRect.width, XmNheight, aRect.height, nsnull);
    mainWindow = ::XtVaCreateManagedWidget("mainWindow",
				 xmMainWindowWidgetClass,
				 shell, 
         nsnull);
    XtVaSetValues(mainWindow, 
         XmNwidth, aRect.width, XmNheight, aRect.height, nsnull);
  }

  frame = ::XtVaCreateManagedWidget("frame",
				    xmDrawingAreaWidgetClass,
				    mainWindow,
				    XmNwidth, aRect.width,
				    XmNheight, aRect.height,
				    XmNmarginHeight, 0,
				    XmNmarginWidth, 0,
                                    XmNrecomputeSize, False,
				    nsnull);


  mWidget = frame ;

  if (mainWindow) {
    XmMainWindowSetAreas(mainWindow, nsnull, nsnull, nsnull, nsnull, frame);
  }
    
  if (aWidgetParent) {
    aWidgetParent->AddChild(this);
  }

  InitCallbacks();

  XtAddCallback(mWidget,
                XmNresizeCallback,
                nsXtWidget_Resize_Callback,
                this);


}


void nsWindow::CreateChildWindow(nsNativeWidget aNativeParent, 
                      nsIWidget *aWidgetParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  mBounds = aRect;
  mAppShell = aAppShell;

  InitToolkit(aToolkit, aWidgetParent);
  
  // save the event callback function
  mEventCallback = aHandleEventFunction;
  
  InitDeviceContext(aContext, (Widget)aNativeParent);

  mWidget = ::XtVaCreateManagedWidget("frame",
				    xmDrawingAreaWidgetClass,
				    (Widget)aNativeParent,
				    XmNwidth, aRect.width,
				    XmNheight, aRect.height,
				    XmNmarginHeight, 0,
				    XmNmarginWidth, 0,
                                    XmNrecomputeSize, False,
				    nsnull);


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


  CreateGC();
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

void nsWindow::CreateWindow(nsNativeWidget aNativeParent, 
                      nsIWidget *aWidgetParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  if (0==aNativeParent)
    CreateMainWindow(aNativeParent, aWidgetParent, aRect, 
        aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData);
  else
    CreateChildWindow(aNativeParent, aWidgetParent, aRect, 
        aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData);
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

  XtAddEventHandler(mWidget, 
                    KeyPressMask,
                    PR_FALSE, 
                    nsXtWidget_KeyPressMask_EventHandler,
                    this);

  XtAddEventHandler(mWidget, 
                    KeyReleaseMask,
                    PR_FALSE, 
                    nsXtWidget_KeyReleaseMask_EventHandler,
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
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
    if (aParent)
      aParent->AddChild(this);
    CreateWindow((nsNativeWidget)((aParent) ? aParent->GetNativeData(NS_NATIVE_WIDGET) : 0), 
        aParent, aRect, aHandleEventFunction, aContext, aAppShell, aToolkit,
        aInitData);
}

//-------------------------------------------------------------------------
//
// create with a native parent
//
//-------------------------------------------------------------------------
void nsWindow::Create(nsNativeWidget aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
    CreateWindow(aParent, 0, aRect, aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData);
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
// Accessor functions to get/set the client data
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWindow::GetClientData(void*& aClientData)
{
  aClientData = mClientData;
  return NS_OK;
}

NS_IMETHODIMP nsWindow::SetClientData(void* aClientData)
{
  mClientData = aClientData;
  return NS_OK;
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
  mShown = bState;
  if (bState) {
    XtManageChild(mWidget);
  }
  else
    XtUnmanageChild(mWidget);

//  UpdateVisibilityFlag();
//  UpdateDisplay();
}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
void nsWindow::Move(PRUint32 aX, PRUint32 aY)
{
  mBounds.x = aX;
  mBounds.y = aY;
//  UpdateVisibilityFlag();
//  UpdateDisplay();

#ifdef SET_VALUES
  XtVaSetValues(mWidget, XmNx, aX, XmNy, GetYCoord(aY), nsnull);
#else
  XtMoveWidget(mWidget, (Position)aX, (Position)GetYCoord(aY));
#endif
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
//  UpdateVisibilityFlag();
//  UpdateDisplay();
//#ifdef SET_VALUES

  XtVaSetValues(mWidget, XmNx, mBounds.x, XmNy, mBounds.y, XmNwidth, aWidth, XmNheight, aHeight, nsnull);

//#else
//  XtConfigureWidget(mWidget, mBounds.x, mBounds.y, aWidth, aHeight, 0);
//  XtResizeWidget(mWidget, aWidth, aHeight, 0);
//#endif
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
//  UpdateVisibilityFlag();
//  UpdateDisplay();
#ifdef SET_VALUES
  XtVaSetValues(mWidget, XmNx, aX, XmNy, GetYCoord(aY),
                        XmNwidth, aWidth, XmNheight, aHeight, nsnull);
#else
  XtConfigureWidget(mWidget, aX, GetYCoord(aY), aWidth, aHeight, 0);
#endif
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
  //XtVaSetValues(mWidget, 
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
    if (mContext == nsnull) {
      return;
    }
    nsIFontCache* fontCache = mContext->GetFontCache();
    if (fontCache != nsnull) {
      nsIFontMetrics* metrics = fontCache->GetMetricsFor(aFont);
      if (metrics != nsnull) {

        XmFontList      fontList = NULL;
        XmFontListEntry entry    = NULL;
        XFontStruct * fontStruct = XQueryFont(XtDisplay(mWidget), 
                                              (XID)metrics->GetFontHandle());
        if (fontStruct != NULL) {
          entry = XmFontListEntryCreate(XmFONTLIST_DEFAULT_TAG, 
                                        XmFONT_IS_FONT, fontStruct);
          fontList = XmFontListAppendEntry(NULL, entry);

          XtVaSetValues(mWidget, XmNfontList, fontList, NULL);

          XmFontListEntryFree(&entry);
          XmFontListFree(fontList);
        }

        NS_RELEASE(metrics);
      } else {
        printf("****** Error: Metrics is NULL!\n");
      }
      NS_RELEASE(fontCache);
    } else {
      printf("****** Error: FontCache is NULL!\n");
    }
}

    
//-------------------------------------------------------------------------
//
// Get this component cursor
//
//-------------------------------------------------------------------------
nsCursor nsWindow::GetCursor()
{
  return eCursor_standard;
}

    
//-------------------------------------------------------------------------
//
// Set this component cursor
//
//-------------------------------------------------------------------------

void nsWindow::SetCursor(nsCursor aCursor)
{
  Window window = ::XtWindow(mWidget);
  if (0==window)
    return;

  // Only change cursor if it's changing
  if (aCursor != mCursor) {
    Cursor newCursor = 0;
    Display *display = ::XtDisplay(mWidget);

    switch(aCursor) {
      case eCursor_select:
        newCursor = XCreateFontCursor(display, XC_xterm);
      break;
   
      case eCursor_wait: 
        newCursor = XCreateFontCursor(display, XC_watch);
      break;

      case eCursor_hyperlink:
        newCursor = XCreateFontCursor(display, XC_hand2);
      break;

      case eCursor_standard:
        newCursor = XCreateFontCursor(display, XC_left_ptr);
      break;

      case eCursor_arrow_south:
      case eCursor_arrow_south_plus:
        newCursor = XCreateFontCursor(display, XC_bottom_side);
      break;

      case eCursor_arrow_north:
      case eCursor_arrow_north_plus:
        newCursor = XCreateFontCursor(display, XC_top_side);
      break;

      case eCursor_arrow_east:
      case eCursor_arrow_east_plus:
        newCursor = XCreateFontCursor(display, XC_right_side);
      break;

      case eCursor_arrow_west:
      case eCursor_arrow_west_plus:
        newCursor = XCreateFontCursor(display, XC_left_side);
      break;

      default:
        NS_ASSERTION(0, "Invalid cursor type");
      break;
    }

    if (0 != newCursor) {
      mCursor = aCursor;
      ::XDefineCursor(display, window, newCursor);
    }
 }
}
    
//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
void nsWindow::Invalidate(PRBool aIsSynchronous)
{
  if (mWidget == nsnull) {
    return;
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
      return (void*)XtWindow(mWidget);
    case NS_NATIVE_DISPLAY:
      return (void*)XtDisplay(mWidget);
    case NS_NATIVE_WIDGET:
      return (void*)(mWidget);
    case NS_NATIVE_GRAPHIC:
      {
        void *res = NULL;
        // We Cache a Read-Only Shared GC in the Toolkit.  If we don't
        // have one ourselves (because it needs to be writeable) grab the
        // the shared GC
        if (nsnull == mGC)
          res = (void *)((nsToolkit *)mToolkit)->GetSharedGC(); 
        else
          res = (void *)mGC;
        NS_ASSERTION(res, "Unable to return GC");
        return res;
      }
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
  return nsnull;
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
// Return the used app shell
//
//-------------------------------------------------------------------------
nsIAppShell* nsWindow::GetAppShell()
{ 
  return mAppShell;
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

#ifdef SET_VALUES
      XtVaSetValues(children[i], XmNx, x + aDx, XmNy, y + aDy, nsnull);
#else
      XtMoveWidget(children[i], x + aDx,  y + aDy);
#endif
    } 
  }
  
  Window  win      = XtWindow(mWidget);
  Display *display = XtDisplay(mWidget);
  
  if (nsnull != aClipRect) {
    XCopyArea(display, win, win, XDefaultGC(display, 0), 
            aClipRect->x, aClipRect->y, 
            aClipRect->XMost(),  aClipRect->YMost(), aDx, aDy);
  }

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

//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------

PRBool nsWindow::DispatchEvent(nsGUIEvent* event)
{
  PRBool result = PR_FALSE;
  event->widgetSupports = mOuter;

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

    return result;
  }

  if (nsnull != mMouseListener) {
    switch (aEvent.message) {
      case NS_MOUSE_MOVE: {
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
    return(DispatchEvent(&aEvent));
  }
  return FALSE;
}

PRBool nsWindow::OnKey(PRUint32 aEventType, PRUint32 aKeyCode, nsKeyEvent* aEvent)
{
  if (mEventCallback) {
    return(DispatchEvent(aEvent));
  }
  else
   return FALSE;
}


PRBool nsWindow::DispatchFocus(nsGUIEvent &aEvent)
{
  if (mEventCallback) {
    return(DispatchEvent(&aEvent));
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
  Widget parent = XtParent(mWidget);

  if (parent) {
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
}

void nsWindow::UpdateDisplay()
{
    // If not displayed and needs to be displayed
  if ((PR_FALSE==mDisplayed) &&
     (PR_TRUE==mShown) && 
     (PR_TRUE==mVisible)) {
    XtManageChild(mWidget);
    mDisplayed = PR_TRUE;
  }

    // Displayed and needs to be removed
  if (PR_TRUE==mDisplayed) { 
    if ((PR_FALSE==mShown) || (PR_FALSE==mVisible)) {
      XtUnmanageChild(mWidget);
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

