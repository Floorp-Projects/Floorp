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
#include "stdio.h"

#define DBG 0


static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);

NS_IMPL_ADDREF(nsWindow)
NS_IMPL_RELEASE(nsWindow)




//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow() : nsIWidget(),
  mEventListener(nsnull),
  mMouseListener(nsnull),
  mToolkit(nsnull),
  mFontMetrics(nsnull),
  mContext(nsnull),
  mEventCallback(nsnull),
  mIgnoreResize(PR_FALSE),
  mParent(nsnull)
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
  NS_INIT_REFCNT();

  mShown = PR_FALSE;
  mVisible = PR_FALSE;
  mDisplayed = PR_FALSE;
  mLowerLeft = PR_FALSE;
  mCursor = eCursor_standard;
  mClientData = nsnull;
  mWindowRegion = nsnull;
  mChildren      = NULL;

}


//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{

	if(mWindowRegion!=nsnull)
		{
		DisposeRgn(mWindowRegion);
		mWindowRegion = nsnull;	
		}

	if (mParent == nsnull)
	{
		CloseWindow(mWindowPtr);
		delete[] mWindowRecord;

		mWindowPtr = nsnull;
		mWindowRecord = nsnull;
	}
  //XtDestroyWidget(mWidget);
  //if (nsnull != mGC) {
    //::XFreeGC((Display *)GetNativeData(NS_NATIVE_DISPLAY),mGC);
    //mGC = nsnull;
  //}
}


//-------------------------------------------------------------------------
//
// create a nswindow, if aParent is null, we will create the main parent
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
	 mParent = aParent;
	  
	// now create our stuff
	if (0==aParent)
    CreateMainWindow(0, 0, aRect, aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData);
  else
    CreateChildWindow(aParent->GetNativeData(NS_NATIVE_WIDGET), aParent, aRect,aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData);
}

//-------------------------------------------------------------------------
//
// Creates a main nsWindow using the native platforms window or widget
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
	if (0==aParent)
	{
		mParent = nsnull;
		CreateMainWindow(aParent,0 , aRect, aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData);
	}
	else
	{
		mParent = (nsWindow*)(((WindowPeek)aParent)->refCon);
		CreateChildWindow(aParent, mParent, aRect,aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData);
	}		
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
void 
nsWindow::InitToolkit(nsIToolkit *aToolkit,nsIWidget  *aWidgetParent) 
{
  if (nsnull == mToolkit) 
  	{ 
    if (nsnull != aToolkit) 
    	{
      mToolkit = (nsToolkit*)aToolkit;
      mToolkit->AddRef();
   		}
    else 
    	{
      if (nsnull != aWidgetParent) 
      	{
        mToolkit = (nsToolkit*)(aWidgetParent->GetToolkit()); // the call AddRef's, we don't have to
      	}
      else 
      	{				// it's some top level window with no toolkit passed in.
        mToolkit = new nsToolkit();
        mToolkit->AddRef();
        mToolkit->Init(PR_GetCurrentThread());
      	}
    	}
  	}

}


//-------------------------------------------------------------------------
//
// Create a new windowptr since we do not have a main window yet
//
//-------------------------------------------------------------------------
void 
nsWindow::CreateMainWindow(nsNativeWidget aNativeParent, 
                      nsIWidget *aWidgetParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
Rect		bounds;

  mBounds = aRect;
  mAppShell = aAppShell;
  NS_IF_ADDREF(mAppShell);
	
	InitToolkit(aToolkit, aWidgetParent);
	
	// save the event callback function
  mEventCallback = aHandleEventFunction;

	
	// build the main native window
	if(0==aNativeParent)
		{
		bounds.top = aRect.x;
		bounds.left = aRect.y;
		bounds.bottom = aRect.y+aRect.height;
		bounds.right = aRect.x+aRect.width;
		mWindowRecord = (WindowRecord*)new char[sizeof(WindowRecord)];   // allocate our own windowrecord space
		if (bounds.top <= 0)
			bounds.top = LMGetMBarHeight()+20;
		mWindowPtr = NewCWindow(mWindowRecord,&bounds,"\ptestwindow",TRUE,0,(GrafPort*)-1,TRUE,(long)this);
		

		mWindowMadeHere = PR_TRUE;
		mIsMainWindow = PR_TRUE;
		}
	else
		{
		mWindowRecord = (WindowRecord*)aNativeParent;
		mWindowPtr = (WindowPtr)aNativeParent;
		mWindowMadeHere = PR_FALSE;
		mIsMainWindow = PR_TRUE;		
		}
		
	mWindowRegion = NewRgn();
	SetRectRgn(mWindowRegion,0,0,mWindowPtr->portRect.right,mWindowPtr->portRect.bottom);
	
  InitDeviceContext(aContext, (nsNativeWidget)mWindowPtr);
}

//-------------------------------------------------------------------------
//
// Create a nsWindow, a WindowPtr will not be created here
//
//-------------------------------------------------------------------------
void nsWindow::CreateChildWindow(nsNativeWidget  aNativeParent, 
                      nsIWidget *aWidgetParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{

	// bounds of this child
  mBounds = aRect;
  mAppShell = aAppShell;
  NS_IF_ADDREF(mAppShell);
  mIsMainWindow = PR_FALSE;
  mWindowMadeHere = PR_TRUE;

  InitToolkit(aToolkit, aWidgetParent);
  
	// save the event callback function
  mEventCallback = aHandleEventFunction;
  
  // add this new nsWindow to the parents list
	if (aWidgetParent) 
		{
		aWidgetParent->AddChild(this);
		mWindowRecord = (WindowRecord*)aNativeParent;
		mWindowPtr = (WindowPtr)aNativeParent;
		}
 
	mWindowRegion = NewRgn();
	SetRectRgn(mWindowRegion,aRect.x,aRect.y,aRect.x+aRect.width,aRect.y+aRect.height);		 
  InitDeviceContext(aContext,aNativeParent);

  // Force cursor to default setting
  mCursor = eCursor_select;
  SetCursor(eCursor_standard);

}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
void nsWindow::InitDeviceContext(nsIDeviceContext *aContext,nsNativeWidget aParentWidget) 
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

    res = nsRepository::CreateInstance(kDeviceContextCID,
                                       nsnull, 
                                       kDeviceContextIID, 
                                       (void **)&mContext);
    if (NS_OK == res) 
    	{
      mContext->Init(aParentWidget);
    	}
  }

}

//-------------------------------------------------------------------------
//
// Close this nsWindow
//
//-------------------------------------------------------------------------
void nsWindow::Destroy()
{

	if (mWindowMadeHere==PR_TRUE && mIsMainWindow==PR_TRUE)
		{
		CloseWindow(mWindowPtr);
		delete mWindowRecord;
		}
	
	if(mWindowRegion!=nsnull)
		{
		DisposeRgn(mWindowRegion);
		mWindowRegion = nsnull;	
		}
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
  NS_IF_ADDREF(mParent);
  return  mParent;
}


//-------------------------------------------------------------------------
//
// Get this nsWindow's list of children
//
//-------------------------------------------------------------------------
nsIEnumerator* nsWindow::GetChildren()
{
  if (mChildren) {
    // Reset the current position to 0
    mChildren->Reset();

    // XXX Does this copy of our enumerator work? It looks like only
    // the first widget in the list is added...
    Enumerator * children = new Enumerator;
    nsISupports   * next = mChildren->Next();
    if (next) {
      nsIWidget *widget;
      if (NS_OK == next->QueryInterface(kIWidgetIID, (void**)&widget)) {
        children->Append(widget);
      }
    }

    return (nsIEnumerator*)children;
  }
	return NULL;
}


//-------------------------------------------------------------------------
//
// Add a child to the list of children
//
//-------------------------------------------------------------------------
void nsWindow::AddChild(nsIWidget* aChild)
{
	if (!mChildren)
		mChildren = new Enumerator();

	mChildren->Append(aChild);
}

//-------------------------------------------------------------------------
//
// Remove a child from the list of children
//
//-------------------------------------------------------------------------
void nsWindow::RemoveChild(nsIWidget* aChild)
{
	if (mChildren)
    mChildren->Remove(aChild);
}

//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------
void nsWindow::Show(PRBool bState)
{
	// set the state
  mShown = bState;
  
  // if its a main window, do the thing
  if (bState) 
  	{		// visible
  	if(mIsMainWindow)			// mac WindowPtr
  		{
  		}
  	}
  else
  	{		// hidden
  	if(mIsMainWindow)			// mac WindowPtr
  		{
  		}  	
  	}
    	
  // update the change
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
  
  // if its a main window, move the window,
  
  // else is a child, so change its relative position
  
  
  // update this change
  
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
  if (aRepaint)
  {
  	UpdateVisibilityFlag();
  	UpdateDisplay();
  }
  //XtVaSetValues(mWidget, XmNx, mBounds.x, XmNy, mBounds.y, XmNwidth, aWidth, XmNheight, aHeight, nsnull);

//    XtResizeWidget(mWidget, aWidth, aHeight, 0);
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
  if (aRepaint)
  {
  	UpdateVisibilityFlag();
  	UpdateDisplay();
  }
 // XtVaSetValues(mWidget, XmNx, aX, XmNy, GetYCoord(aY),XmNwidth, aWidth, XmNheight, aHeight, nsnull);
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
/*  Set this widgets port, clipping, etc, it is in focus
 *  @update  dc 09/11/98
 *  @param   NONE
 *  @return  NONE
 */
void nsWindow::SetFocus(void)
{
nsRect							therect;
Rect								macrect;

	::SetPort(mWindowPtr);
	GetBounds(therect);
	nsRectToMacRect(therect,macrect);
	::ClipRect(&macrect);
	
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
    if (mContext == nsnull) {
      return;
    }
    nsIFontCache* fontCache = nsnull;
    mContext->GetFontCache(fontCache);
    if (fontCache != nsnull) {
      nsIFontMetrics* metrics;
      fontCache->GetMetricsFor(aFont, metrics);
      if (metrics != nsnull) {

        //XmFontList      fontList = NULL;
        //XmFontListEntry entry    = NULL;
        //XFontStruct * fontStruct = XQueryFont(XtDisplay(mWidget),(XID)metrics->GetFontHandle());
        //if (fontStruct != NULL) {
          //entry = XmFontListEntryCreate(XmFONTLIST_DEFAULT_TAG,XmFONT_IS_FONT, fontStruct);
          //fontList = XmFontListAppendEntry(NULL, entry);

         // XtVaSetValues(mWidget, XmNfontList, fontList, NULL);

          //XmFontListEntryFree(&entry);
          //XmFontListFree(fontList);
        //}

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
}
    
//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
void nsWindow::Invalidate(PRBool aIsSynchronous)
{
GrafPtr	curport;

	if(mWindowRegion)
		{
		::GetPort(&curport);
		::SetPort(mWindowPtr);
		::InvalRgn(mWindowRegion);
		::SetPort(curport);
		}

  
}

//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void* nsWindow::GetNativeData(PRUint32 aDataType)
{
  switch(aDataType) 
  	{
		case NS_NATIVE_WIDGET:
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_GRAPHIC:
    case NS_NATIVE_DISPLAY:
      return (void*)mWindowPtr;
    	break;
    case NS_NATIVE_REGION:
    	return (void*) mWindowRegion;
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

  if (GetNativeData(NS_NATIVE_WIDGET)) 
  	{
    nsresult  res = NS_OK;
    
    static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
    static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
    
    res = nsRepository::CreateInstance(kRenderingContextCID, nsnull, kRenderingContextIID, (void **)&ctx);
    
    if (NS_OK == res)
    	{
      ctx->Init(mContext, this);
      
      //ctx->SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine)
      }
    
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
  NS_IF_ADDREF(mToolkit);
  return mToolkit;
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
void nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
}

//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
void nsWindow::SetBorderStyle(nsBorderStyle aBorderStyle) 
{
} 

//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
void nsWindow::SetTitle(const nsString& aTitle) 
{
} 

//-------------------------------------------------------------------------
//
// Processes a mouse pressed event
//
//-------------------------------------------------------------------------
void nsWindow::AddMouseListener(nsIMouseListener * aListener)
{
  NS_PRECONDITION(mMouseListener == nsnull, "Null mouse listener");
  mMouseListener = aListener;
}

//-------------------------------------------------------------------------
//
// Processes a mouse pressed event
//
//-------------------------------------------------------------------------
void nsWindow::AddEventListener(nsIEventListener * aListener)
{
  NS_PRECONDITION(mEventListener == nsnull, "Null event listener");
  mEventListener = aListener;
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
  event->widgetSupports = this;

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
PRBool nsWindow::DispatchMouseEvent(nsMouseEvent &aEvent)
{

  PRBool result = PR_FALSE;
  if (nsnull == mEventCallback && nsnull == mMouseListener) {
    return result;
  }

  // call the event callback 
  if (nsnull != mEventCallback) 
  	{
    result = DispatchEvent(&aEvent);
    return result;
  	}

  if (nsnull != mMouseListener) {
    switch (aEvent.message) {
      case NS_MOUSE_MOVE: {
        result = ConvertStatus(mMouseListener->MouseMoved(aEvent));
        nsRect rect;
        GetBounds(rect);
        if (rect.Contains(aEvent.point.x, aEvent.point.y)) 
        	{
          //if (mWindowPtr == NULL || mWindowPtr != this) 
          	//{
            printf("Mouse enter");
            //mCurrentWindow = this;
          	//}
        	} 
        else 
        	{
          printf("Mouse exit");
        	}

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
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------
/**
 * Processes an Expose Event
 *
 **/
PRBool nsWindow::OnPaint(nsPaintEvent &event)
{
nsresult	result;
nsRect 		rr;

  // call the event callback 
  if (mEventCallback) 
  	{
  	// currently we only update the entire widget instead of just the invalidated region
    GetBounds(rr);
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

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
void nsWindow::BeginResizingChildren(void)
{
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
void nsWindow::EndResizingChildren(void)
{
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
void nsWindow::OnDestroy()
{
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
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

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnKey(PRUint32 aEventType, PRUint32 aKeyCode, nsKeyEvent* aEvent)
{
  if (mEventCallback) {
    return(DispatchEvent(aEvent));
  }
  else
   return FALSE;
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchFocus(nsGUIEvent &aEvent)
{
  if (mEventCallback) {
    return(DispatchEvent(&aEvent));
  }

 return FALSE;
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnScroll(nsScrollbarEvent & aEvent, PRUint32 cPos)
{
 return FALSE;
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
void nsWindow::SetIgnoreResize(PRBool aIgnore)
{
  mIgnoreResize = aIgnore;
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------

PRBool nsWindow::IgnoreResize()
{
  return mIgnoreResize;
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
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

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
void 
nsWindow::SetResized(PRBool aResized)
{
  mResized = aResized;
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
PRBool 
nsWindow::GetResized()
{
  return(mResized);
}

/*
 *  @update  gpk 08/27/98
 *  @param   aX -- x offset in widget local coordinates
 *  @param   aY -- y offset in widget local coordinates
 *  @return  PR_TRUE if the pt is contained in the widget
 */
PRBool
nsWindow::PtInWindow(PRInt32 aX,PRInt32 aY)
{
PRBool	result = PR_FALSE;
Point		hitpt;

	hitpt.h = aX;
	hitpt.v = aY;
	
	if( PtInRgn( hitpt,mWindowRegion) )
		result = TRUE;
	return(result);
}


/*
 *  @update  gpk 08/27/98
 *  @param   aX -- x offset in widget local coordinates
 *  @param   aY -- y offset in widget local coordinates
 *  @return  PR_TRUE if the pt is contained in the widget
 */
void nsWindow::SetBounds(const Rect& aMacRect)
{
	MacRectToNSRect(aMacRect,mBounds);
}


/*
 *  Set a Mac Rect to the value of an nsRect 
 *  The source rect is assumed to be in pixels not TWIPS
 *  @update  gpk 08/27/98
 *  @param   aRect -- The nsRect that is the source
 *  @param   aMacRect -- The Mac Rect destination
 */
void nsWindow::nsRectToMacRect(const nsRect& aRect, Rect& aMacRect) const
{
		aMacRect.left = aRect.x;
		aMacRect.top = aRect.y;
		aMacRect.right = aRect.x + aRect.width;
		aMacRect.bottom = aRect.y + aRect.height;
}

/*
 *  Set an nsRect to the value of a Mac Rect 
 *  The source rect is assumed to be in pixels not TWIPS
 *  @update  gpk 08/27/98
 *  @param   aMacRect -- The Mac Rect that is the source
 *  @param   aRect -- The nsRect coming in
 */
void nsWindow::MacRectToNSRect(const Rect& aMacRect, nsRect& aRect) const
{
	aRect.x = aMacRect.left;
	aRect.y = aMacRect.top;
	aRect.width = aMacRect.right-aMacRect.left;
	aRect.height = aMacRect.bottom-aMacRect.top;
}


//-------------------------------------------------------------------------
//
// Locate the widget that contains the point
//
//-------------------------------------------------------------------------
nsWindow* 
nsWindow::FindWidgetHit(Point aThePoint)
{
nsWindow	*child = this;
nsWindow	*deeperWindow;
nsRect		rect;

	if (this->PtInWindow(aThePoint.h,aThePoint.v))
		{
		// traverse through all the nsWindows to find out who got hit, lowest level of course
		if (mChildren) 
			{
	    mChildren->ResetToLast();
	    child = (nsWindow*)mChildren->Previous();
    	nsRect bounds;
    	GetBounds(bounds);
    	aThePoint.h -= bounds.x;
    	aThePoint.v -= bounds.y;
	    while(child)
        {
        if (child->PtInWindow(aThePoint.h,aThePoint.v) ) 
          {
          // go down this windows list
          deeperWindow = child->FindWidgetHit(aThePoint);
          if (deeperWindow)
            return(deeperWindow);
          else
            return(child);
          }
        child = (nsWindow*)mChildren->Previous();	
        }
			}
			return this;
		}
	return nsnull;
}

//-------------------------------------------------------------------------
/*  Go thru this widget and its childern and find out who intersects the region, and generate paint event.
 *  @update  dc 08/28/98
 *  @param   aTheRegion -- The region to paint
 *  @return  nothing is returned
 */
void 
nsWindow::DoPaintWidgets(RgnHandle	aTheRegion)
{
nsWindow			*child = this;
nsRect				rect;
RgnHandle			thergn;
Rect					bounds;
nsPaintEvent 	pevent;


	thergn = NewRgn();
	::SectRgn(aTheRegion,this->mWindowRegion,thergn);
	
	if (!::EmptyRgn(thergn))
		{
		// traverse through all the nsWindows to find who needs to be painted
		if (mChildren) 
			{
	    mChildren->Reset();
	    child = (nsWindow*)mChildren->Next();
	    while(child)
        { 
        if (child->RgnIntersects(aTheRegion,thergn) ) 
          {
          // first paint or update this widget
          bounds = (**thergn).rgnBBox;
          rect.x = bounds.left;
          rect.y = bounds.top;
          rect.width = bounds.left + (bounds.right-bounds.left);
          rect.height = bounds.top + (bounds.bottom-bounds.top);
          
					// generate a paint event
					pevent.message = NS_PAINT;
					pevent.widget = child;
					pevent.eventStructType = NS_PAINT_EVENT;
					pevent.point.x = 0;
			    pevent.point.y = 0;
			    pevent.rect = &rect;
			    pevent.time = 0; 
			    child->OnPaint(pevent);
			    
			    // now go check out the childern
          child->DoPaintWidgets(aTheRegion);
          }
        child = (nsWindow*)mChildren->Next();	
        }
			}
		}
	DisposeRgn(thergn);
}

//-------------------------------------------------------------------------
/*  Go thru this widget and its children and generate resize event.
 *  @update  ps 09/17/98
 *  @param   aEvent -- The resize event
 *  @return  nothing is returned
 */
void 
nsWindow::DoResizeWidgets(nsSizeEvent &aEvent)
{
		nsWindow	*child = this;

	// traverse through all the nsWindows to resize them
	if (mChildren) 
	{
	    mChildren->Reset();
	    child = (nsWindow*)mChildren->Next();
	    while (child)
	    { 
			aEvent.widget = child;
			child->OnResize(aEvent);
				    
			// now go check out the childern
			child->DoResizeWidgets(aEvent);
		    child = (nsWindow*)mChildren->Next();	
		}
	}
}

//-------------------------------------------------------------------------
/*
 *  @update  dc 08/28/98
 *  @param   aTheRegion -- The region to intersect with for this widget
 *  @return  PR_TRUE if the these regions intersect
 */

PRBool
nsWindow::RgnIntersects(RgnHandle aTheRegion,RgnHandle	aIntersectRgn)
{
PRBool			result = PR_FALSE;


	::SectRgn(aTheRegion,this->mWindowRegion,aIntersectRgn);
	if (!::EmptyRgn(aIntersectRgn))
		result = TRUE;
	return(result);
}


//-------------------------------------------------------------------------

void nsWindow::UpdateVisibilityFlag()
{
  //Widget parent = XtParent(mWidget);

  if (TRUE) {
    PRUint32 pWidth = 0;
    PRUint32 pHeight = 0; 
    //XtVaGetValues(parent, XmNwidth, &pWidth, XmNheight, &pHeight, nsnull);
    if ((mBounds.y + mBounds.height) > pHeight) {
      mVisible = PR_FALSE;
      return;
    }
 
    if (mBounds.y < 0)
     mVisible = PR_FALSE;
  }
  
  mVisible = PR_TRUE;
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
void nsWindow::UpdateDisplay()
{
    // If not displayed and needs to be displayed
  if ((PR_FALSE==mDisplayed) &&
     (PR_TRUE==mShown) && 
     (PR_TRUE==mVisible)) {
    //XtManageChild(mWidget);
    mDisplayed = PR_TRUE;
  }

    // Displayed and needs to be removed
  if (PR_TRUE==mDisplayed) { 
    if ((PR_FALSE==mShown) || (PR_FALSE==mVisible)) {
      //XtUnmanageChild(mWidget);
      mDisplayed = PR_FALSE;
    }
  }
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
PRUint32 nsWindow::GetYCoord(PRUint32 aNewY)
{
  if (PR_TRUE==mLowerLeft) {
    return(aNewY - 12 /*KLUDGE fix this later mBounds.height */);
  }
  return(aNewY);
}



/**
 * Implement the standard QueryInterface for NS_IWIDGET_IID and NS_ISUPPORTS_IID
 * @param aIID -- the name of the 
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
*/ 
nsresult nsWindow::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }

    static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
    if (aIID.Equals(kIWidgetIID)) {
        *aInstancePtr = (void*) ((nsIWidget*)(nsISupports*)this);
        AddRef();
        return NS_OK;
    }

    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports*)this);
        AddRef();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}


//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
void nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
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

//=================================================================
/*  Convert the coordinates to some device coordinates so GFX can draw.
 *  @update  dc 09/16/98
 *  @param   nscoord -- X coordinate to convert
 *  @param   nscoord -- Y coordinate to convert
 *  @return  NONE
 */
void  nsWindow::ConvertToDeviceCoordinates(nscoord	&aX,nscoord	&aY)
{
nsRect	rect;
        
	this->GetBounds(rect);
  aX +=rect.x;
  aY +=rect.y;

}

//-------------------------------------------------------------------------
//
// Constructor
//
//-------------------------------------------------------------------------
#define INITIAL_SIZE        2

nsWindow::Enumerator::Enumerator()
{
    mArraySize = INITIAL_SIZE;
    mNumChildren = 0;
    mChildrens = (nsIWidget**)new PRInt32[mArraySize];
    memset(mChildrens, 0, sizeof(PRInt32) * mArraySize);
    mCurrentPosition = 0;
}


//-------------------------------------------------------------------------
//
// Destructor
//
//-------------------------------------------------------------------------
nsWindow::Enumerator::~Enumerator()
{   
	if (mChildrens) 
		{
		delete[] mChildrens;
		mChildrens = nsnull;
		}
}

//-------------------------------------------------------------------------
//
// Get enumeration next element. Return null at the end
//
//-------------------------------------------------------------------------
nsIWidget* nsWindow::Enumerator::Next()
{
	NS_ASSERTION(mChildrens != nsnull,"it is not valid to call this method on an empty list");
	if (mChildrens != nsnull)
		{
		if (mCurrentPosition < mArraySize && mChildrens[mCurrentPosition]) 
			return mChildrens[mCurrentPosition++];
		}
  return nsnull;
}

//-------------------------------------------------------------------------
//
// Get enumeration previous element. Return null at the beginning
//
//-------------------------------------------------------------------------
nsIWidget* nsWindow::Enumerator::Previous()
{
	NS_ASSERTION(mChildrens != nsnull,"it is not valid to call this method on an empty list");
	if (mChildrens != nsnull)
		{
		if ((mCurrentPosition >=0) && (mCurrentPosition < mArraySize) && (mChildrens[mCurrentPosition])) 
			return mChildrens[mCurrentPosition--];
		}
  return NULL;
}

//-------------------------------------------------------------------------
//
// Reset enumerator internal pointer to the beginning
//
//-------------------------------------------------------------------------
void nsWindow::Enumerator::Reset()
{
    mCurrentPosition = 0;
}

//-------------------------------------------------------------------------
//
// Reset enumerator internal pointer to the end
//
//-------------------------------------------------------------------------
void nsWindow::Enumerator::ResetToLast()
{
    mCurrentPosition = mNumChildren-1;
}

//-------------------------------------------------------------------------
//
// Append an element 
//
//-------------------------------------------------------------------------
void nsWindow::Enumerator::Append(nsIWidget* aWinWidget)
{

  if (aWinWidget) 
  	{
    if (mNumChildren == mArraySize) 
        GrowArray();
    mChildrens[mNumChildren] = aWinWidget;
    mNumChildren++;
    NS_ADDREF(aWinWidget);
  	}
}


//-------------------------------------------------------------------------
//
// Remove an element 
//
//-------------------------------------------------------------------------
void nsWindow::Enumerator::Remove(nsIWidget* aWinWidget)
{
int	pos;

		if(mNumChildren)
			{
	    for(pos = 0; mChildrens[pos] && (mChildrens[pos] != aWinWidget); pos++){};
	    if (mChildrens[pos] == aWinWidget) 
	    	{
	      memcpy(mChildrens + pos, mChildrens + pos + 1, mArraySize - pos - 1);
	    	}
			mNumChildren--;
			}
}

//-------------------------------------------------------------------------
//
// Grow the size of the children array
//
//-------------------------------------------------------------------------
void nsWindow::Enumerator::GrowArray()
{
    mArraySize <<= 1;
    nsIWidget **newArray = (nsIWidget**)new PRInt32[mArraySize];
    memset(newArray, 0, sizeof(PRInt32) * mArraySize);
    memcpy(newArray, mChildrens, (mArraySize>>1) * sizeof(PRInt32));
    mChildrens = newArray;
}

/*
 * Convert an nsPoint into mac local coordinated.
 * The tree hierarchy is navigated upwards, changing
 * the x,y offset by the parent's coordinates
 *
 */
void nsWindow::LocalToWindowCoordinate(nsPoint& aPoint)
{
	nsIWidget* 	parent = GetParent();
  nsRect 			bounds;
  
	while (parent)
	{
		parent->GetBounds(bounds);
		aPoint.x += bounds.x;
		aPoint.y += bounds.y;	
		parent = parent->GetParent();
	}
}

/* 
 * Convert an nsRect's local coordinates to global coordinates
 */
void nsWindow::LocalToWindowCoordinate(nsRect& aRect)
{
	nsIWidget* 	parent = GetParent();
  nsRect 			bounds;
  
	while (parent)
	{
		parent->GetBounds(bounds);
		aRect.x += bounds.x;
		aRect.y += bounds.y;	
		parent = parent->GetParent();
	}
}

/* 
 * Convert a nsString to a PascalStr255
 */
void nsWindow::StringToStr255(const nsString& aText, Str255& aStr255)
{
  char buffer[256];
	
	aText.ToCString(buffer,255);
		
	PRInt32 len = strlen(buffer);
	memcpy(&aStr255[1],buffer,len);
	aStr255[0] = len;
	
		
}


/* 
 * Convert a nsString to a PascalStr255
 */
void nsWindow::Str255ToString(const Str255& aStr255, nsString& aText)
{
  char 		buffer[256];
	PRInt32 len = aStr255[0];
  
	memcpy(buffer,&aStr255[1],len);
	buffer[len] = 0;

	aText = buffer;		
}
