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
nsWindow::nsWindow() : nsBaseWidget(),
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
nsRefData	*theRefData;

	if(mWindowRegion!=nsnull)
		{
		DisposeRgn(mWindowRegion);
		mWindowRegion = nsnull;	
		}

	if (mParent == nsnull)
	{
		theRefData = (nsRefData*)(((WindowPeek)mWindowPtr)->refCon);
		if(theRefData)
			delete theRefData;
		
		CloseWindow(mWindowPtr);
		delete[] mWindowRecord;

		mWindowPtr = nsnull;
		mWindowRecord = nsnull;
	}
}


//-------------------------------------------------------------------------
//
// create a nswindow, if aParent is null, we will create the main parent
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Create(nsIWidget *aParent,
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
		CreateChildWindow(aParent->GetNativeData(NS_NATIVE_WINDOW), aParent, aRect,aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData);
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Creates a main nsWindow using the native platforms window or widget
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Create(nsNativeWidget aParent,    						// this is a windowPtr, 
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
nsRefData				*theRefData;

	if (0==aParent)
	{
		mParent = nsnull;
		CreateMainWindow(aParent,0 , aRect, aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData);
	}
	else
	{
		theRefData = (nsRefData*)(((WindowPeek)aParent)->refCon);
		mParent = (nsWindow*)theRefData->GetCurWidget();
		CreateChildWindow(aParent,mParent, aRect,aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData);
	}		
	
	return NS_OK;
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
PRInt32			top;
Rect				bounds;
nsRefData		*theReferenceData;
	
  mAppShell = aAppShell;
  NS_IF_ADDREF(mAppShell);
	
	InitToolkit(aToolkit, aWidgetParent);
	
	// save the event callback function
  mEventCallback = aHandleEventFunction;

  strcpy(gInstanceClassName, "Parent nsWindow");
	
	// build the main native window
	if(0==aNativeParent)
		{
		top = aRect.x + LMGetMBarHeight() + 20; ;				// offset arect by the menubar and the dragbar
		
		
		bounds.top = top; 
		bounds.left = aRect.y;
		bounds.bottom = bounds.top+aRect.height;
		bounds.right = bounds.left+aRect.width;
		
		mWindowRecord = (WindowRecord*)new char[sizeof(WindowRecord)];   // allocate our own windowrecord space
			
		theReferenceData = new nsRefData();	
		theReferenceData->SetTopWidget(this);
		mWindowPtr = NewCWindow(mWindowRecord,&bounds,"\ptestwindow",TRUE,0,(GrafPort*)-1,TRUE,(long)theReferenceData);
		
		// the bounds of the widget is the mac content region in global coordinates
		::SetPort(mWindowPtr);
		bounds = mWindowPtr->portRect;
		LocalToGlobal(&topLeft(bounds));
		LocalToGlobal(&botRight(bounds));
		this->SetBounds(bounds);

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
	
	// the widow region is going to be the size of the content region, local coordinates	
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
  strcpy(gInstanceClassName, "Child nswindow");

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
  InitDeviceContext(aContext,(nsNativeWidget)mWindowPtr);
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
NS_IMETHODIMP nsWindow::Destroy()
{
nsRefData		*theRefData;

	if (mWindowMadeHere==PR_TRUE && mIsMainWindow==PR_TRUE)
		{
		theRefData = (nsRefData*)(((WindowPeek)mWindowPtr)->refCon);
		delete theRefData;
		
		CloseWindow(mWindowPtr);
		delete mWindowRecord;
		}
	
	if(mWindowRegion!=nsnull)
		{
		DisposeRgn(mWindowRegion);
		mWindowRegion = nsnull;	
		}

	nsBaseWidget::Destroy();

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
// Hide or show this component
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Show(PRBool bState)
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
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Move(PRUint32 aX, PRUint32 aY)
{
  mBounds.x = aX;
  mBounds.y = aY;
  
  // if its a main window, move the window,
  
  // else is a child, so change its relative position
  
  
  // update this change
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Resize(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
nsSizeEvent 	event;

  mBounds.width  = aWidth;
  mBounds.height = aHeight;
  
   if(nsnull!=mWindowRegion)
  	::DisposeRgn(mWindowRegion);
	mWindowRegion = NewRgn();
	SetRectRgn(mWindowRegion,mBounds.x,mBounds.y,mBounds.x+mBounds.width,mBounds.y+mBounds.height);		 
 
  if (aRepaint)
  	{
  	UpdateVisibilityFlag();
  	UpdateDisplay();
  	}
  
  event.message = NS_SIZE;
  event.point.x = 0;
  event.point.y = 0;
  event.windowSize = &mBounds;
  event.eventStructType = NS_SIZE_EVENT;
  event.widget = this;
 	return ( this->DispatchWindowEvent(&event) ? NS_OK : NS_ERROR_FAILURE );
}

    
//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Resize(PRUint32 aX, PRUint32 aY, PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
nsSizeEvent 	event;

  mBounds.x      = aX;
  mBounds.y      = aY;
  mBounds.width  = aWidth;
  mBounds.height = aHeight;
  if(nsnull!=mWindowRegion)
  	::DisposeRgn(mWindowRegion);
	mWindowRegion = NewRgn();
	SetRectRgn(mWindowRegion,mBounds.x,mBounds.y,mBounds.x+mBounds.width,mBounds.y+mBounds.height);

  if (aRepaint)
  	{
  	UpdateVisibilityFlag();
  	UpdateDisplay();
  	}
  
  event.message = NS_SIZE;
  event.point.x = 0;
  event.point.y = 0;
  event.windowSize = &mBounds;
  event.widget = this;
  event.eventStructType = NS_SIZE_EVENT;
 	return ( this->DispatchWindowEvent(&event) ? NS_OK : NS_ERROR_FAILURE );
}

    
//-------------------------------------------------------------------------
//
// Enable/disable this component
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Enable(PRBool bState)
{
	return NS_OK;
}

    
//-------------------------------------------------------------------------
/*  Set this widgets port, clipping, etc, it is in focus
 *  @update  dc 09/11/98
 *  @param   NONE
 *  @return  NONE
 */
NS_IMETHODIMP nsWindow::SetFocus(void)
{
nsRect							therect;
Rect								macrect;

	::SetPort(mWindowPtr);
	GetBounds(therect);
	nsRectToMacRect(therect,macrect);
	::ClipRect(&macrect);
	return NS_OK;
	
}

    
//-------------------------------------------------------------------------
//
// Get this component dimension
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::SetBounds(const nsRect &aRect)
{
 mBounds = aRect;
 return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this component dimension
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::GetBounds(nsRect &aRect)
{

  aRect = mBounds;
  return NS_OK;
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
NS_IMETHODIMP nsWindow::SetFont(const nsFont &aFont)
{
  if (mContext == nsnull) {
    return NS_OK;
  }
  nsIFontMetrics* metrics;
  mContext->GetMetricsFor(aFont, metrics);
  if (metrics != nsnull) {

    NS_RELEASE(metrics);
  } else {
    printf("****** Error: Metrics is NULL!\n");
  }
 
 	return NS_OK;
}
 
//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Invalidate(const nsRect &aRect, PRBool aIsSynchronous)
{
GrafPtr		curport;
RgnHandle	thergn;


	if(mWindowRegion)
		{
		::GetPort(&curport);
		::SetPort(mWindowPtr);
		thergn = ::NewRgn();
		::SetRectRgn(thergn,aRect.x,aRect.y,aRect.x+aRect.width,aRect.y+aRect.height);
		::SectRgn(thergn,mWindowRegion,thergn);
		::InvalRgn(thergn);
		::DisposeRgn(thergn);
		::SetPort(curport);
		}
	return NS_OK;
  
}
   
//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Invalidate(PRBool aIsSynchronous)
{
GrafPtr	curport;

	if(mWindowRegion)
		{
		::GetPort(&curport);
		::SetPort(mWindowPtr);
		::InvalRgn(mWindowRegion);
		::SetPort(curport);
		}
	return NS_OK;
  
}

//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void* nsWindow::GetNativeData(PRUint32 aDataType)
{
PRInt32		offx,offy;
nsRefData	*theRefData;

  switch(aDataType) 
  	{
		case NS_NATIVE_WIDGET:
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_GRAPHIC:
    case NS_NATIVE_DISPLAY:
			// set the refcon up with the current widget we are referencing
			theRefData = (nsRefData*)(((WindowPeek)mWindowPtr)->refCon);
			if(theRefData)
				{
				theRefData->SetCurWidget(this);
				}				
				
      return (void*)mWindowPtr;
    	break;
    case NS_NATIVE_REGION:
    	return (void*) mWindowRegion;
    	break;
    case NS_NATIVE_COLORMAP:
    	break;
    case NS_NATIVE_OFFSETX:
    	this->CalcOffset(offx,offy);
    	return (void*) offx;
    	break;
    case NS_NATIVE_OFFSETY:
    	this->CalcOffset(offx,offy);
    	return (void*) offy;
    	break;
    default:
      break;
  }

  return NULL;
}


//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::SetColorMap(nsColorMap *aColorMap)
{
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
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

//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus)
{
	NS_ADDREF(event->widget);
  aStatus = nsEventStatus_eIgnore;
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

//-------------------------------------------------------------------------
PRBool nsWindow::DispatchWindowEvent(nsGUIEvent* event)
{
  nsEventStatus status;
  DispatchEvent(event, status);
  return ConvertStatus(status);
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
    result = (DispatchWindowEvent(&aEvent));
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
nsresult				result;
nsRect 					rr;

  // call the event callback 
  if (mEventCallback) 
  	{
  	// currently we only update the entire widget instead of just the invalidated region
    GetBounds(rr);
    event.rect = &rr;

      {
      PRInt32					offx,offy;
			GrafPtr					theport;
			Rect						macrect;
			nsRect					therect;
			RgnHandle				thergn;
        
        CalcTotalOffset(offx,offy);
				GetPort(&theport);
				::SetPort(mWindowPtr);
				::SetOrigin(-offx,-offy);
				GetBounds(therect);
        
        if(mParent)
        	{
					nsRectToMacRect(therect,macrect);
					thergn = ::NewRgn();
					::GetClip(thergn);
					::ClipRect(&macrect);
					::PenNormal();
					//::RGBForeColor(&greencolor);
	        //::FrameRect(&macrect); 
					}
				else
					{
					macrect.top = 0;
					macrect.left = 0;
					macrect.bottom = therect.height;
					macrect.right = therect.width;
					
					thergn = ::NewRgn();
					::GetClip(thergn);
					::ClipRect(&macrect);
					}							

        result = (DispatchWindowEvent(&event));
        SetOrigin(0,0);
        SetPort(theport);
      }
  }
  return result;
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::BeginResizingChildren(void)
{
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::EndResizingChildren(void)
{
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnResize(nsSizeEvent &aEvent)
{
  nsRect* size = aEvent.windowSize;
  if (mEventCallback) {
    return(DispatchWindowEvent(&aEvent));
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
    return(DispatchWindowEvent(aEvent));
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
    return(DispatchWindowEvent(&aEvent));
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

#ifdef NOTNOW
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
#endif

/*
 *  Finds if a point in local coordinates is inside this object
 */
PRBool
nsWindow::PtInWindow(PRInt32 aX,PRInt32 aY)
{
PRBool	result = PR_FALSE;
nsPoint	hitPt(aX,aY);
nsRect	bounds,newbounds;
PRInt32	offx,offy;
	
	GetBounds(bounds);

	if(this->GetParent())
		{
		CalcOffset(offx,offy);
		bounds.x +=offx;
		bounds.y +=offy;
		}
	else
		{
		// no parent, the bounds in global, so make a local coorinate system at 0,0
		bounds.x = 0;
		bounds.y = 0;
		}
	
	if(bounds.Contains(hitPt))
		result = PR_TRUE;
	return(result);
}


/*
 *  @update  gpk 08/27/98
 *  @param   aX -- x offset in widget local coordinates
 *  @param   aY -- y offset in widget local coordinates
 *  @return  PR_TRUE if the pt is contained in the widget
 */
NS_IMETHODIMP nsWindow::SetBounds(const Rect& aMacRect)
{
	MacRectToNSRect(aMacRect,mBounds);
	return NS_OK;
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



//========================================================================
/*
 * Find the widget hit
 @param aThePoint -- a point in local coordinats to test for the hit. 
 */
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
nsWindow::DoPaintWidgets(RgnHandle	aTheRegion,nsIRenderingContext	*aRC)
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
					pevent.renderingContext = aRC;
					pevent.widget = child;
					pevent.eventStructType = NS_PAINT_EVENT;
					pevent.point.x = 0;
			    pevent.point.y = 0;
			    pevent.rect = &rect;
			    pevent.time = 0; 
			    child->OnPaint(pevent);
			    
			    // now go check out the childern
          child->DoPaintWidgets(aTheRegion,aRC);
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

NS_IMETHODIMP nsWindow::UpdateVisibilityFlag()
{
  //Widget parent = XtParent(mWidget);

  if (TRUE) {
    PRUint32 pWidth = 0;
    PRUint32 pHeight = 0; 
    //XtVaGetValues(parent, XmNwidth, &pWidth, XmNheight, &pHeight, nsnull);
    if ((mBounds.y + mBounds.height) > pHeight) {
      mVisible = PR_FALSE;
      return NS_OK;
    }
 
    if (mBounds.y < 0)
     mVisible = PR_FALSE;
  }
  
  mVisible = PR_TRUE;
  return NS_OK;
}

//-------------------------------------------------------------------------
/*  Calculate the x and y offsets for this particular widget
 *  @update  ps 09/22/98
 *  @param   aX -- x offset amount
 *  @param   aY -- y offset amount 
 *  @return  NOTHING
 */
NS_IMETHODIMP nsWindow::CalcOffset(PRInt32 &aX,PRInt32 &aY)
{
nsIWidget	*theparent,*child;
nsRect		therect;

	aX = 0;
	aY = 0;
	theparent = this->GetParent();
	while(theparent)
		{
		theparent->GetBounds(therect);
		child = theparent->GetParent();
		if(child)
			{
			aX += therect.x;
			aY += therect.y;
			}
		theparent = child;
		}

	return NS_OK;
}

//-------------------------------------------------------------------------
/*  Calculate the x and y offsets for this particular widget
 *  @update  ps 09/22/98
 *  @param   aX -- x offset amount
 *  @param   aY -- y offset amount 
 *  @return  NOTHING
 */
NS_IMETHODIMP nsWindow::CalcTotalOffset(PRInt32 &aX,PRInt32 &aY)
{
nsIWidget	*theparent,*child;
nsRect		therect;

	aX = mBounds.x;
	aY = mBounds.y;
	theparent = this->GetParent();
	while(theparent)
		{
		theparent->GetBounds(therect);
		child = theparent->GetParent();
		if(child)
			{
			aX += therect.x;
			aY += therect.y;
			}
		theparent = child;
		}

	return NS_OK;
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::UpdateDisplay()
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
  
  return NS_OK;
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
NS_IMETHODIMP nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// 
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
	return NS_OK;
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
PRInt32	offX,offY;
        
  this->CalcOffset(offX,offY);
	
	aX -=offX;
	aY -=offY;
	
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

//-------------------------------------------------------------------------
//
// Return PR_TRUE if the whether the component is visible, PR_FALSE otherwise
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::IsVisible(PRBool & bState)
{
  bState = mVisible;
  return NS_OK;
}

/**
 * Set the widget's MenuBar.
 * Must be called after Create.
 *
 * @param aTitle string displayed as the title of the widget
 */

NS_IMETHODIMP nsWindow::SetMenuBar(nsIMenuBar * aMenuBar)
{
  return NS_OK;
}


NS_METHOD nsWindow::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  aWidth  = mPreferredWidth;
  aHeight = mPreferredHeight;
  return NS_ERROR_FAILURE;
}

NS_METHOD nsWindow::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
  mPreferredWidth  = aWidth;
  mPreferredHeight = aHeight;
  return NS_OK;
}
