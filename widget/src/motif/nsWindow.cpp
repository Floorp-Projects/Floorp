/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/cursorfont.h>
#include <Xm/Xm.h>
#include <Xm/MainW.h>
#include <Xm/Frame.h>
#include <Xm/XmStrDefs.h>
#include <Xm/DrawingA.h>
#include <Xm/DialogS.h>

#include "nsWindow.h"
#include "nsWidgetsCID.h"
#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsTransform2D.h"
#include "nsGfxCIID.h"
#include "nsXtEventHandler.h"
#include "nsXtManageWidget.h"
#include "nsAppShell.h"
#include "nsReadableUtils.h"

#include "stdio.h"

static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);

NS_IMPL_ADDREF(nsWindow)
NS_IMPL_RELEASE(nsWindow)

//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow():
  mWidget(nsnull),
  mEventCallback(nsnull),
  mContext(nsnull),
  mFontMetrics(nsnull),
  mToolkit(nsnull),
  mMouseListener(nsnull),
  mEventListener(nsnull)
{
  NS_INIT_REFCNT();
  strcpy(gInstanceClassName, "nsWindow");
  mBounds.x = 0;
  mBounds.y = 0;
  mBounds.width = 0;
  mBounds.height = 0;
  mResized = PR_FALSE;
  mGC = nsnull ;
  mShown = PR_FALSE;
  mVisible = PR_FALSE;
  mDisplayed = PR_FALSE;
  mLowerLeft = PR_FALSE;
  mCursor = eCursor_standard;
  mClientData = nsnull;
  mPreferredWidth  = 0;
  mPreferredHeight = 0;
  mFont = nsnull;
  mAppContext = nsnull;
}

//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
  OnDestroy();
  XtDestroyWidget(mWidget);
  if (nsnull != mGC) {
    ::XFreeGC((Display *)GetNativeData(NS_NATIVE_DISPLAY),mGC);
    mGC = nsnull;
  }
}

//-------------------------------------------------------------------------
void nsWindow::ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY)
{
}

//-------------------------------------------------------------------------
NS_METHOD nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsWindow::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
  return NS_OK;
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
    res = nsComponentManager::CreateInstance(kDeviceContextCID,
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
  mAppContext = nsAppShell::GetAppContext();

  // keep a reference to the device context
  if (aContext) {
    mContext = aContext;
    NS_ADDREF(mContext);
  }
  else {
    nsresult res;

    static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
    static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

    res = nsComponentManager::CreateInstance(kDeviceContextCID, nsnull, kDeviceContextIID, (void **)&mContext);

    if (NS_OK == res) {
      mContext->Init(nsnull);
    }
  }

  mBounds = aRect;
  mAppShell = aAppShell;

  InitToolkit(aToolkit, aWidgetParent);

  // save the event callback function
  mEventCallback = aHandleEventFunction;

  if (nsnull==aNativeParent) {
    /************************/
    /* Create a main window */
    /************************/

    InitDeviceContext(aContext,
                      (Widget) aAppShell->GetNativeData(NS_NATIVE_SHELL));

    Widget mainWindow = ::XtVaCreateManagedWidget("mainWindow",
                        xmMainWindowWidgetClass,
                        (Widget) aAppShell->GetNativeData(NS_NATIVE_SHELL), 
                        nsnull);

    // Initially used xmDrawingAreaWidgetClass instead of 
    // newManageClass. Drawing area will spontaneously resize 
    // to fit it's contents.  

    mWidget = ::XtVaCreateManagedWidget("drawingArea",
                                        newManageClass,
                                        mainWindow,
                                        XmNwidth, aRect.width,
                                        XmNheight, aRect.height,
                                        XmNmarginHeight, 0,
                                        XmNmarginWidth, 0,
                                        XmNrecomputeSize, False,
                                        XmNuserData, this,
                                        nsnull);

    if (mainWindow) {
      XmMainWindowSetAreas(mainWindow, nsnull, nsnull, nsnull, nsnull, mWidget);
    }

    if (aWidgetParent) {
      aWidgetParent->AddChild(this);
    }

    InitCallbacks();
    CreateGC();
  }
  else {
    /*************************/
    /* Create a child window */
    /*************************/

    InitDeviceContext(aContext, (Widget)aNativeParent);

    // Initially used xmDrawingAreaWidgetClass instead of 
    // newManageClass. Drawing area will spontaneously resize
    // to fit it's contents.

    mWidget = ::XtVaCreateManagedWidget("drawingArea",
                                        newManageClass,
                                        (Widget)aNativeParent,
                                        XmNwidth, aRect.width,
                                        XmNheight, aRect.height,
                                        XmNmarginHeight, 0,
                                        XmNmarginWidth, 0,
                                        XmNrecomputeSize, False,
                                        XmNuserData, this,
                                        nsnull);
    if (aWidgetParent) {
      aWidgetParent->AddChild(this);
    }

    // Force cursor to default setting

    // Note -- is this really necessary??  Ideally should find out--I suspect
    // it isn't.  --ZuperDee

    mCursor = eCursor_select;
    SetCursor(eCursor_standard);

    InitCallbacks();
    CreateGC();
  }
}

//-------------------------------------------------------------------------
//
// Initialize all the Callbacks
//
//-------------------------------------------------------------------------
void nsWindow::InitCallbacks(char * aName)
{
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

}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsWindow::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }
    nsresult result = NS_NOINTERFACE;
    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports*)this);
        AddRef();
        return NS_OK;
    }

    if (aIID.Equals(kIWidgetIID)) {
        *aInstancePtr = (void*) ((nsIWidget*)this);
        AddRef();
        result = NS_OK;
    }

    return result;
}

//-------------------------------------------------------------------------
//
// create with nsIWidget parent
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::Create(nsIWidget *aParent,
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
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// create with a native parent
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Create(nsNativeWidget aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
    CreateWindow(aParent, 0, aRect, aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData);
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Close this nsWindow
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Destroy()
{
  return NS_OK;
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
  // XXX: Implement this
  return nsnull;
}


//-------------------------------------------------------------------------
//
// Get this nsWindow's list of children
//
//-------------------------------------------------------------------------
nsIEnumerator* nsWindow::GetChildren()
{
  //XXX: Implement this
  return nsnull;
}


//-------------------------------------------------------------------------
//
// Add a child to the list of children
//
//-------------------------------------------------------------------------
void nsWindow::AddChild(nsIWidget* aChild)
{
  // XXX:Implement this
}


//-------------------------------------------------------------------------
//
// Remove a child from the list of children
//
//-------------------------------------------------------------------------
void nsWindow::RemoveChild(nsIWidget* aChild)
{
  // XXX:Implement this
}


//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Show(PRBool aState)
{
  mShown = aState;
  if (aState) {
    XtManageChild(mWidget);
  }
  else
    XtUnmanageChild(mWidget);

  return NS_OK;
}

NS_METHOD nsWindow::SetModal(PRBool aModal)
{
  //XXX:Implement this
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsWindow::IsVisible(PRBool & aState)
{
  aState = mShown;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Check a potential new window position to be sure it fits on screen
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::ConstrainPosition(PRInt32 *aX, PRInt32 *aY)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
  mBounds.x = aX;
  mBounds.y = aY;

  XtVaSetValues(mWidget, XmNx, aX, XmNy, GetYCoord(aY), nsnull);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
//#if 0
  printf("$$$$$$$$$ %s::Resize %d %d   Repaint: %s\n", 
         gInstanceClassName, aWidth, aHeight, (aRepaint?"true":"false"));
//#endif

  // ignore resizes smaller than or equal to 1x1
  if (aWidth <=1 || aHeight <= 1)
    return NS_OK;

  mBounds.width  = aWidth;
  mBounds.height = aHeight;
  XtVaSetValues(mWidget,
                XmNx, mBounds.x,
                XmNy, mBounds.y,
                XmNwidth, aWidth,
                XmNheight, aHeight,
                nsnull);
  if (aRepaint)
    Invalidate(PR_FALSE);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth,
                           PRInt32 aHeight, PRBool aRepaint)
{
  Resize(aWidth,aHeight,aRepaint);
  Move(aX,aY);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Enable/disable this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Enable(PRBool bState)
{
  XtVaSetValues(mWidget, XmNsensitive, bState, nsnull);
  return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Give the focus to this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetFocus(PRBool aRaise)
{
  // Go get the parent of all widget's to determine which widget 
  // tree to use to set the focus. 
  Widget w = mWidget;
  while (NULL != XtParent(w)) {
    w = XtParent(w);
  }

  XtSetKeyboardFocus(w, mWidget);
  return NS_OK;
}

/*
 * Set this component dimension
 */
NS_METHOD nsWindow::SetBounds(const nsRect &aRect)
{
  mBounds = aRect;
  return NS_OK;
}

/*
 * Get this component dimension
 */
NS_METHOD nsWindow::GetBounds(nsRect &aRect)
{
  aRect = mBounds;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the foreground color
//
//-------------------------------------------------------------------------
nscolor nsWindow::GetForegroundColor(void)
{
  return (mForeground);
  return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Set the foreground color
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetForegroundColor(const nscolor &aColor)
{
  PRUint32 pixel;
  mForeground = aColor;
  mContext->ConvertPixel(aColor, pixel);
  XtVaSetValues(mWidget, XtNforeground, pixel, nsnull);
  return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Get the background color
//
//-------------------------------------------------------------------------
nscolor nsWindow::GetBackgroundColor(void)
{
  return (mBackground);
  return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Set the background color
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetBackgroundColor(const nscolor &aColor)
{
//   mBackground = aColor ;
//   PRUint32 pixel;
//   mContext->ConvertPixel(aColor, pixel);
//   XtVaSetValues(mWidget, XtNbackground, pixel, nsnull);
  return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Get this component font
//
//-------------------------------------------------------------------------
nsIFontMetrics* nsWindow::GetFont(void)
{
  //XXX: Implement this
  NS_NOTYETIMPLEMENTED("GetFont not yet implemented"); // to be implemented
  return nsnull;
}

    
//-------------------------------------------------------------------------
//
// Set this component font
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetFont(const nsFont &aFont)
{
    if (mContext == nsnull) {
      return NS_ERROR_FAILURE;
    }
    nsIFontMetrics* metrics;
    mContext->GetMetricsFor(aFont, metrics);
    if (metrics != nsnull) {

      XmFontList      fontList = NULL;
      XmFontListEntry entry    = NULL;
      nsFontHandle    fontHandle;
      metrics->GetFontHandle(fontHandle);
      XFontStruct * fontStruct = XQueryFont(XtDisplay(mWidget), (XID)fontHandle);

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
      return NS_ERROR_FAILURE;
    }

     // XXX Temporary, should not be caching the font
    if (mFont == nsnull) {
      mFont = new nsFont(aFont);
    } else {
      *mFont  = aFont;
    }

  return NS_OK;
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

NS_METHOD nsWindow::SetCursor(nsCursor aCursor)
{
  Window window = ::XtWindow(mWidget);
  if (nsnull==window)
    return NS_ERROR_FAILURE;

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
        //XXX Commenting invalid assertion, there are other cursor types.
        //NS_ASSERTION(PR_FALSE, "Invalid cursor type");
      break;
    }

    if (nsnull != newCursor) {
      mCursor = aCursor;
      XDefineCursor(display, window, newCursor);
    }
 }
  return NS_OK;
}
    
//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Invalidate(PRBool aIsSynchronous)
{
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
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
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
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void *nsWindow::GetNativeData(PRUint32 aDataType)
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
        if (nsnull == mGC) {
           NS_ASSERTION(mToolkit, "Unable to return GC, toolkit is null");
          res = (void *)((nsToolkit *)mToolkit)->GetSharedGC(); 
        }
        else {
          res = (void *)mGC;
        }
        NS_ASSERTION(res, "Unable to return GC");
        return res;
      }
    case NS_NATIVE_COLORMAP:
    default:
      break;
  }

  return NULL;
}

//~~~
void nsWindow::FreeNativeData(void * data, PRUint32 aDataType)
{
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
    
    res = nsComponentManager::CreateInstance(kRenderingContextCID, nsnull, kRenderingContextIID, (void **)&ctx);
    
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
  if (mWidget == nsnull) {
    return NS_ERROR_FAILURE;
  }

   // Scroll all of the child widgets
  Cardinal numChildren;
  XtVaGetValues(mWidget, XtNnumChildren, &numChildren, nsnull);
  if (numChildren > 0) {
    WidgetList children;
    XtVaGetValues(mWidget, XtNchildren, &children, nsnull);
    unsigned int i;
    for(i = 0; i < numChildren; i++) {
      Position x;
      Position y;
      XtVaGetValues(children[i], XtNx, &x, XtNy, &y, nsnull);

      XtVaSetValues(children[i], XmNx, x + aDx, XmNy, y + aDy, nsnull);
    } 
  }

  if (aClipRect)
    Invalidate(*aClipRect, PR_TRUE);
  else
    Invalidate(PR_TRUE);
  return NS_OK;
}

NS_METHOD nsWindow::SetBorderStyle(nsBorderStyle aBorderStyle) 
{
  return NS_OK;
} 

NS_METHOD nsWindow::SetTitle(const nsString& aTitle) 
{
/*
  if(!mBaseWindow)
    return NS_ERROR_FAILURE;
    
  const char *text = ToNewCString(aTitle);
  XStoreName(gDisplay, mBaseWindow, text);
  delete [] text;
*/
  return NS_OK;
}

NS_METHOD nsWindow::CaptureRollupEvents(nsIRollupListener *aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent)
{
  return NS_OK;
}

/**
 * Processes a mouse pressed event
 **/
NS_METHOD nsWindow::AddMouseListener(nsIMouseListener * aListener)
{
  return NS_OK;
}

/**
 * Processes a mouse pressed event
 **/
NS_METHOD nsWindow::AddEventListener(nsIEventListener * aListener)
{
  return NS_OK;
}

NS_METHOD nsWindow::AddMenuListener(nsIMenuListener * aListener)
{
  //XXX:Implement this.
  return NS_OK;
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

//////////////////////////////////////////////////////////////////
//
// Turning TRACE_EVENTS on will cause printfs for all
// mouse events that are dispatched.
//
// These are extra noisy, and thus have their own switch:
//
// NS_MOUSE_MOVE
// NS_PAINT
// NS_MOUSE_ENTER, NS_MOUSE_EXIT
//
//////////////////////////////////////////////////////////////////

#define TRACE_EVENTS
#undef TRACE_EVENTS_MOTION
#define TRACE_EVENTS_PAINT
#undef TRACE_EVENTS_CROSSING

#ifdef DEBUG
void
nsWindow::DebugPrintEvent(nsGUIEvent &   aEvent,
                          Widget         aWidget)
{
#ifndef TRACE_EVENTS_MOTION
  if (aEvent.message == NS_MOUSE_MOVE)
  {
    return;
  }
#endif

#ifndef TRACE_EVENTS_PAINT
  if (aEvent.message == NS_PAINT)
  {
    return;
  }
#endif

#ifndef TRACE_EVENTS_CROSSING
  if (aEvent.message == NS_MOUSE_ENTER || aEvent.message == NS_MOUSE_EXIT)
  {
    return;
  }
#endif

  static int sPrintCount=0;

  printf("%4d %-26s(this=%-8p , widget=%-8p",
         sPrintCount++,
         NS_LossyConvertUCS2toASCII(debug_GuiEventToString(&aEvent)).get(),
         this,
         (void *) aWidget);
  
  printf(" , x=%-3d, y=%d)",aEvent.point.x,aEvent.point.y);

  printf("\n");
}
#endif // DEBUG
//////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWindow::DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus)
{
#ifdef TRACE_EVENTS
  DebugPrintEvent(*event,mWidget);
#endif

  NS_ADDREF(event->widget);

  aStatus = nsEventStatus_eIgnore;
  
  //if (nsnull != mMenuListener)
  //	aStatus = mMenuListener->MenuSelected(*event);
  if (nsnull != mEventCallback) {
    aStatus = (*mEventCallback)(event);
  } 

  // Dispatch to event listener if event was not consumed
  if ((aStatus != nsEventStatus_eIgnore) && (nsnull != mEventListener)) {
    aStatus = mEventListener->ProcessEvent(*event);
  }
  NS_RELEASE(event->widget);

  return NS_OK;

}

PRBool nsWindow::DispatchWindowEvent(nsGUIEvent* event)
{
  nsEventStatus status;
  DispatchEvent(event, status);
  return ConvertStatus(status);
}


//-------------------------------------------------------------------------
//
// Deals with all sorts of mouse events
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchMouseEvent(nsMouseEvent& aEvent)
{
  PRBool result = PR_FALSE;
  if (nsnull == mEventCallback && nsnull == mMouseListener) {
    return result;
  }

  // call the event callback 
  if (nsnull != mEventCallback) {
    result = DispatchWindowEvent(&aEvent);
    return result;
  }

  if (nsnull != mMouseListener) {
    switch (aEvent.message) {
      case NS_MOUSE_MOVE:
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
    } // switch
  } 
  return result;
}

/**
 * Processes an Expose Event
 **/
PRBool nsWindow::OnPaint(nsPaintEvent &event)
{
  nsresult result;

  // call the event callback
  if (mEventCallback) {
    event.renderingContext = nsnull;
#if 0
    if (event.rect) {
    printf("nsWindow::OnPaint(this=%p, {%i,%i,%i,%i})\n", this,
            event.rect->x, event.rect->y,
            event.rect->width, event.rect->height);
    }
    else {
      printf("nsWindow::OnPaint(this=%p, NO RECT)\n", this);
    }
#endif
    static NS_DEFINE_CID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
    if (NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID,
                                              nsnull,
                                              NS_GET_IID(nsIRenderingContext),
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

// FIXME: Needs to be implemented??  Looks like a stub to me...
NS_METHOD nsWindow::BeginResizingChildren(void)
{
  return NS_OK;
}

// FIXME: Needs to be implemented??  Looks like a stub to me...
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
  if (mEventCallback) {
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

void nsWindow::SetResizeRect(nsRect& aRect) 
{
  mResizeRect = aRect;
  printf("SetResizeRect: %i %i\n",mResizeRect.width,mResizeRect.height);
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
    return(aNewY - mBounds.height);
  }
  return(aNewY);
}

//-----------------------------------------------------
// Resize handler code for child and main windows.
//-----------------------------------------------------

void nsWindow_ResetResize_Callback(XtPointer call_data)
{
  nsWindow* widgetWindow = (nsWindow*)call_data;
  widgetWindow->SetResized(PR_FALSE);
}

void nsWindow_Refresh_Callback(XtPointer call_data)
{
  nsWindow* widgetWindow = (nsWindow*)call_data;
  nsRect bounds;
  widgetWindow->GetResizeRect(&bounds);

  nsSizeEvent event;
  event.message = NS_SIZE;
  event.widget  = widgetWindow;
  event.time    = 0; //TBD
  event.windowSize = &bounds;

  widgetWindow->SetBounds(bounds);
  widgetWindow->OnResize(event);
  nsPaintEvent pevent;
  pevent.message = NS_PAINT;
  pevent.widget = widgetWindow;
  pevent.time = 0;
  pevent.rect = (nsRect *)&bounds;
  widgetWindow->OnPaint(pevent);

  XtAppAddTimeOut(widgetWindow->mAppContext, 50, (XtTimerCallbackProc)nsWindow_ResetResize_Callback, widgetWindow);
}

// Resize a child window widget. All nsManageWidget's use
// this to resize. The nsManageWidget passes all resize
// request's directly to this function.

extern "C" void nsWindow_ResizeWidget(Widget w)
{
  int width = 0;
  int height = 0;
  nsWindow *win = 0;

  // Get the new size for the window
  printf("Zuperdee says %-8p\n",(Widget&) w);
  XtVaGetValues(w, XmNuserData, &win, XmNwidth, &width, XmNheight, &height, nsnull);
  printf("%i %i\n",width,height);

  // Setup the resize rectangle for the window.
  nsRect bounds;
  bounds.width = width;
  bounds.height = height;
  bounds.x = 0;
  bounds.y = 0;
  win->SetResizeRect(bounds);

  if (! win->GetResized()) {
    win->SetResized(PR_TRUE);
    if (win->IsChild()) {
       // Call refresh directly. Don't filter resize events.
      nsWindow_Refresh_Callback(win);
    }
    else {
       // XXX: KLUDGE: Do actual resize later. This lets most
       // of the resize events come through before actually
       // resizing. This is only needed for main (shell)
       // windows. This should be replaced with code that actually
       // Compresses the event queue.
      XtAppAddTimeOut(win->mAppContext, 250, (XtTimerCallbackProc)nsWindow_Refresh_Callback, win);
    }
  }
}

NS_METHOD nsWindow::SetMenuBar(nsIMenuBar * aMenuBar)
{
  return NS_ERROR_FAILURE;
}

NS_METHOD nsWindow::ShowMenuBar(PRBool aShow)
{
  return NS_ERROR_FAILURE;
}

NS_METHOD nsWindow::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  aWidth  = mPreferredWidth;
  aHeight = mPreferredHeight;
  return (mPreferredWidth != 0 && mPreferredHeight != 0)?NS_OK:NS_ERROR_FAILURE;
}

NS_METHOD nsWindow::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
  mPreferredWidth  = aWidth;
  mPreferredHeight = aHeight;
  return NS_OK;
}

NS_METHOD nsWindow::EnableFileDrop(PRBool aEnable)
{
  //XXX:Implement this.
  return NS_OK;
}

NS_METHOD nsWindow::CaptureMouse(PRBool aCapture)
{
  //XXX:Implement this.
  printf("nsWindow::CaptureMouse called\n");
  return NS_OK;
}
