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


// ***IMPORTANT***
// On all platforms, we are assuming in places that the implementation of |nsIWidget|
// is really |nsWindow| and then calling methods specific to nsWindow to finish the job.
// This is by design and the assumption is safe because an nsIWidget can only be created through
// our Widget factory where all our widgets, including the XP widgets, inherit from nsWindow.
// Still, in the places (or most of them) where this assumption is done, a |static_cast| has been used.
// A similar warning is in nsWidgetFactory.cpp.


#include "nsWindow.h"
#include "nsIFontMetrics.h"
#include "nsIDeviceContext.h"
#include "nsCOMPtr.h"
#include "nsToolkit.h"
#include "nsIEnumerator.h"
#include <Appearance.h>

#include "nsplugindefs.h"
#include "nsMacEventHandler.h"

NS_IMPL_ADDREF(ChildWindow);
NS_IMPL_RELEASE(ChildWindow);

ChildWindow::ChildWindow() : nsWindow()
{
  NS_INIT_REFCNT();
  strcpy(gInstanceClassName, "ChildWindow");
}

#pragma mark -
//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow() : nsBaseWidget() , nsDeleteObserved(this)
{
  NS_INIT_REFCNT();
  strcpy(gInstanceClassName, "nsWindow");

  mParent = nsnull;
  mBounds.SetRect(0,0,0,0);
  mVisible = PR_FALSE;
  mEnabled = PR_TRUE;
	SetPreferredSize(0,0);

	mFontMetrics = nsnull;
	mMenuBar = nsnull;
	mTempRenderingContext = nsnull;

  mWindowRegion = nsnull;
  mWindowPtr = nsnull;
  mDrawing = PR_FALSE;
	mDestroyCalled = PR_FALSE;
	mDestructorCalled = PR_FALSE;

	SetBackgroundColor(NS_RGB(255, 255, 255));
	SetForegroundColor(NS_RGB(0, 0, 0));
	
	mPluginPort = nsnull;
}


//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
	mDestructorCalled = PR_TRUE;

	//Destroy();

	if (mWindowRegion != nsnull)
	{
		::DisposeRgn(mWindowRegion);
		mWindowRegion = nsnull;	
	}
			
	NS_IF_RELEASE(mTempRenderingContext);
	NS_IF_RELEASE(mMenuListener);
	
	if (mPluginPort != nsnull) {
		delete mPluginPort;
	}
}


//-------------------------------------------------------------------------
//
// Utility method for implementing both Create(nsIWidget ...) and
// Create(nsNativeWidget...)
//-------------------------------------------------------------------------

nsresult nsWindow::StandardCreate(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData,
                      nsNativeWidget aNativeParent)	// should always be nil here
{
	mParent = aParent;

	mBounds = aRect;
	mWindowRegion = ::NewRgn();
	if (mWindowRegion == nil)
		return NS_ERROR_OUT_OF_MEMORY;
	::SetRectRgn(mWindowRegion, 0, 0, aRect.width, aRect.height);		 

	BaseCreate(aParent, aRect, aHandleEventFunction, 
							aContext, aAppShell, aToolkit, aInitData);

	if (mParent)
	{
		SetBackgroundColor(mParent->GetBackgroundColor());
		SetForegroundColor(mParent->GetForegroundColor());
	}

	if (mWindowPtr == nsnull) {
		if (aParent)
			mWindowPtr = (WindowPtr)aParent->GetNativeData(NS_NATIVE_DISPLAY);
		else if (aAppShell)
			mWindowPtr = (WindowPtr)aAppShell->GetNativeData(NS_NATIVE_SHELL);
	}
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// create a nswindow
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
	return(StandardCreate(aParent, aRect, aHandleEventFunction,
													aContext, aAppShell, aToolkit, aInitData,
														nsnull));
}

//-------------------------------------------------------------------------
//
// Creates a main nsWindow using a native widget
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Create(nsNativeWidget aNativeParent,		// this is a nsWindow*
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
	// On Mac, a native widget is a nsWindow* because 
	// nsWindow::GetNativeData(NS_NATIVE_WIDGET) returns 'this'
	nsIWidget* aParent = (nsIWidget*)aNativeParent;
	
	return(Create(aParent, aRect, aHandleEventFunction,
									aContext, aAppShell, aToolkit, aInitData));
}

//-------------------------------------------------------------------------
//
// Close this nsWindow
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Destroy()
{
	if (mDestroyCalled)
		return NS_OK;
	mDestroyCalled = PR_TRUE;

	nsBaseWidget::OnDestroy();
	nsBaseWidget::Destroy();
	SetMenuBar(nsnull);

	ReportDestroyEvent();	// beard: this seems to cause the window to be deleted. moved all release code to destructor.

	/* following is a terrible hack. what the heck is the relationship between the webshell window
	   and this nsWindow?  To make a new window, you make one of the former, which makes one
	   of the latter. To delete a window, you come to this method.  What the!!???  Here we are
	   doing something really awful because it allows you to actually close (or appear to close)
	   a window on the Mac, whose OS doesn't kill the window for you.
	*/
	/* Unfortunately this hack has the nasty side effect of causing a crash when trying
	   to load a new URL in either appRunner or viewer.  Feeling that it is better to have
	   windows that you can't close rather than to crash trying to browse I'm commenting
	   out the Release() for now.
	*/
	//Release();	// the ref added when nsWebShellWindow made us

	return NS_OK;
}

#pragma mark -
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
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void* nsWindow::GetNativeData(PRUint32 aDataType)
{
	nsPoint		point;
	void*		retVal = nsnull;

  switch (aDataType) 
	{
	case NS_NATIVE_WIDGET:
    case NS_NATIVE_WINDOW:
    	retVal = (void*)this;
    	break;

    case NS_NATIVE_GRAPHIC:
    case NS_NATIVE_DISPLAY:
      retVal = (void*)mWindowPtr;
    	break;

    case NS_NATIVE_REGION:
    	retVal = (void*)mWindowRegion;
    	break;

    case NS_NATIVE_COLORMAP:
    	//¥TODO
    	break;

    case NS_NATIVE_OFFSETX:
    	point.MoveTo(mBounds.x, mBounds.y);
    	LocalToWindowCoordinate(point);
    	retVal = (void*)point.x;
     	break;

    case NS_NATIVE_OFFSETY:
    	point.MoveTo(mBounds.x, mBounds.y);
    	LocalToWindowCoordinate(point);
    	retVal = (void*)point.y;
    	break;
    
    case NS_NATIVE_PLUGIN_PORT:
    	// this needs to be a combination of the port and the offsets.
    	if (mPluginPort == nsnull)
    		mPluginPort = new nsPluginPort;
    		
		point.MoveTo(mBounds.x, mBounds.y);
		LocalToWindowCoordinate(point);

		// for compatibility with 4.X, this origin is what you'd pass
		// to SetOrigin.
		mPluginPort->port = CGrafPtr(mWindowPtr);
		mPluginPort->portx = -point.x;
		mPluginPort->porty = -point.y;
		
    	retVal = (void*)mPluginPort;
	}

  return retVal;
}

#pragma mark -
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

//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Show(PRBool bState)
{
  mVisible = bState;
  return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Enable/disable this component
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Enable(PRBool bState)
{
	mEnabled = bState;
	return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Set the focus on this component
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::SetFocus(void)
{
	if (mToolkit)
		((nsToolkit*)mToolkit)->SetFocus(this);
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this component font
//
//-------------------------------------------------------------------------
nsIFontMetrics* nsWindow::GetFont(void)
{
	return mFontMetrics;
}

    
//-------------------------------------------------------------------------
//
// Set this component font
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::SetFont(const nsFont &aFont)
{
	NS_IF_RELEASE(mFontMetrics);
	if (mContext)
		mContext->GetMetricsFor(aFont, mFontMetrics);
 	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::SetColorMap(nsColorMap *aColorMap)
{
	//¥TODO
	// We may need to move this to nsMacWindow:
	// I'm not sure all the individual widgets
	// can have each their own colorMap on Mac.
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set the widget's MenuBar.
// Must be called after Create.
// Releases a previously set nsIMenuBar
// AddRefs the passed in nsIMenuBar
// @param aMenuBar a pointer to an nsIMenuBar interface on an object
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::SetMenuBar(nsIMenuBar * aMenuBar)
{
  if (mMenuBar != nsnull)
    mMenuBar->SetParent(nsnull);
  NS_IF_RELEASE(mMenuBar);
  NS_IF_ADDREF(aMenuBar);
  mMenuBar = aMenuBar;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the widget's MenuBar.
//
//-------------------------------------------------------------------------
nsIMenuBar* nsWindow::GetMenuBar()
{
  return mMenuBar;
}


//
// SetCursor
//
// Override to set the cursor on the mac
//
NS_METHOD nsWindow::SetCursor(nsCursor aCursor)
{
  nsBaseWidget::SetCursor(aCursor);
	
  // mac specific cursor manipulation
	//¥TODO: We need a way to get non-os cursors here.
	if (nsToolkit::HasAppearanceManager())
	{
		short cursor = -1;
	  switch (aCursor)
	  {
	    case eCursor_standard:	cursor = kThemeArrowCursor;		break;
	    case eCursor_wait:			cursor = kThemeWatchCursor;		break;
	    case eCursor_select:		cursor = kThemeIBeamCursor;		break;
	    case eCursor_hyperlink:	cursor = kThemePointingHandCursor;		break;
			case eCursor_sizeWE:		cursor = kThemeResizeLeftRightCursor;	break;
			case eCursor_sizeNS:		cursor = kThemeResizeLeftRightCursor;	break;	//¥TODO: bad id
	  }
	  if (cursor >= 0)
	  	::SetThemeCursor(cursor);
  }
  else
  {
		short cursor = -1;
	  switch (aCursor)
	  {
	    case eCursor_standard:	::InitCursor();					break;
	    case eCursor_wait:			cursor = watchCursor;		break;
	    case eCursor_select:		cursor = iBeamCursor;		break;
	    case eCursor_hyperlink:	cursor = plusCursor;		break;
	  }
	  if (cursor > 0)
	  	::SetCursor(*(::GetCursor(cursor)));
  }
 
  return NS_OK;
  
} // nsWindow :: SetCursor
    

#pragma mark -
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
// Move this component
// aX and aY are in the parent widget coordinate system
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Move(PRUint32 aX, PRUint32 aY)
{
	if ((mBounds.x != aX) || (mBounds.y != aY))
	{
		// Invalidate the current location (unless it's the top-level window)
		if (mParent != nsnull)
			Invalidate(PR_FALSE);
	  	
		// Set the bounds
		mBounds.x = aX;
		mBounds.y = aY;

		// Report the event
		ReportMoveEvent();
	}
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Resize(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
	if ((mBounds.width != aWidth) || (mBounds.height != aHeight))
	{
	  // Set the bounds
	  mBounds.width  = aWidth;
	  mBounds.height = aHeight;

		// Update the region
		::SetRectRgn(mWindowRegion, 0, 0, aWidth, aHeight);		 
	 
		// Invalidate the new location
		if (aRepaint)
			Invalidate(PR_FALSE);

		// Report the event
		ReportSizeEvent();
	}
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Resize(PRUint32 aX, PRUint32 aY, PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
	nsWindow::Move(aX, aY);
	nsWindow::Resize(aWidth, aHeight, aRepaint);
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

//-------------------------------------------------------------------------
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
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::EndResizingChildren(void)
{
	return NS_OK;
}

#pragma mark -
 
//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Invalidate(const nsRect &aRect, PRBool aIsSynchronous)
{
	if (!mWindowPtr)
		return NS_OK;

#if 0	// We don't want to draw synchronously on Mac: it makes the xpfe apps much slower
	static PRBool	reentrant = PR_FALSE;
	if (aIsSynchronous && !reentrant)
	{
		reentrant = PR_TRUE;	// no reentrance please
		Update();
		reentrant = PR_FALSE;
	}
	else
#endif
	{
		nsRect wRect = aRect;
		Rect macRect;
		LocalToWindowCoordinate(wRect);
		nsRectToMacRect(wRect, macRect);

		GrafPtr savePort;
		::GetPort(&savePort);
		::SetPort(mWindowPtr);
		::SetOrigin(0, 0);
		::InvalRect(&macRect);
		::SetPort(savePort);
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
	nsWindow::Invalidate(mBounds, aIsSynchronous);
	return NS_OK;
}

inline PRUint16 COLOR8TOCOLOR16(PRUint8 color8)
{
	// return (color8 == 0xFF ? 0xFFFF : (color8 << 8));
	return (color8 << 8) | color8;	/* (color8 * 257) == (color8 * 0x0101) */
}

//-------------------------------------------------------------------------
//	StartDraw
//
//-------------------------------------------------------------------------
void nsWindow::StartDraw(nsIRenderingContext* aRenderingContext)
{
	if (mDrawing)
		return;
	mDrawing = PR_TRUE;

	if (aRenderingContext == nsnull)
	{
		// make sure we have a rendering context
		mTempRenderingContext = GetRenderingContext();
		mTempRenderingContextMadeHere = PR_TRUE;
	}
	else
	{
		// if we already have a rendering context, save its state
		NS_IF_ADDREF(aRenderingContext);
		mTempRenderingContext = aRenderingContext;
		mTempRenderingContextMadeHere = PR_FALSE;
		mTempRenderingContext->PushState();

		// set the environment to the current widget
		mTempRenderingContext->Init(mContext, this);
	}

	// set the widget font. nsMacControl implements SetFont, which is where
	// the font should get set.
	if (mFontMetrics)
	{
		mTempRenderingContext->SetFont(mFontMetrics);
	}

	// set the widget background and foreground colors
	nscolor color = GetBackgroundColor();
	RGBColor macColor;
	macColor.red   = COLOR8TOCOLOR16(NS_GET_R(color));
	macColor.green = COLOR8TOCOLOR16(NS_GET_G(color));
	macColor.blue  = COLOR8TOCOLOR16(NS_GET_B(color));
	::RGBBackColor(&macColor);

	color = GetForegroundColor();
	macColor.red   = COLOR8TOCOLOR16(NS_GET_R(color));
	macColor.green = COLOR8TOCOLOR16(NS_GET_G(color));
	macColor.blue  = COLOR8TOCOLOR16(NS_GET_B(color));
	::RGBForeColor(&macColor);

	mTempRenderingContext->SetColor(color);				// just in case, set the rendering context color too
}


//-------------------------------------------------------------------------
//	EndDraw
//
//-------------------------------------------------------------------------
void nsWindow::EndDraw()
{
	if (! mDrawing)
		return;
	mDrawing = PR_FALSE;

	if (mTempRenderingContextMadeHere)
	{
		PRBool clipEmpty;
		mTempRenderingContext->PopState(clipEmpty);
	}
	NS_RELEASE(mTempRenderingContext);
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnPaint(nsPaintEvent &event)
{
	// override this
  return PR_TRUE;
}


//-------------------------------------------------------------------------
//	Update
//
//		Redraw this widget.
//
//		We draw the widget between BeginUpdate and EndUpdate because some
//		operations go much faster when the visRgn contains what needs to be
//		painted. Then we restore the original updateRgn and validate this
//		widget's rectangle.
//-------------------------------------------------------------------------
NS_IMETHODIMP	nsWindow::Update()
{
	if (! mVisible)
		return NS_OK;

	static PRBool  reentrant = PR_FALSE;

	if (reentrant)
		HandleUpdateEvent();
	else
	{
		reentrant = PR_TRUE;
		GrafPtr savePort;
		::GetPort(&savePort);
		::SetPort(mWindowPtr);

		// make a copy of the window update rgn
		RgnHandle saveUpdateRgn = ::NewRgn();
		::CopyRgn(((WindowRecord*)mWindowPtr)->updateRgn, saveUpdateRgn);

		// draw the widget
		::BeginUpdate(mWindowPtr);
		HandleUpdateEvent();
		::EndUpdate(mWindowPtr);

		// restore the window update rgn
		::CopyRgn(saveUpdateRgn, ((WindowRecord*)mWindowPtr)->updateRgn);

		// validate the rect of the widget we have just drawn
		nsRect bounds = mBounds;
		LocalToWindowCoordinate(bounds);
		Rect macRect;
		nsRectToMacRect(bounds, macRect);
		::ValidRect(&macRect);
		::DisposeRgn(saveUpdateRgn);

		::SetPort(savePort);
		reentrant = PR_FALSE;
	}

	return NS_OK;
}


//-------------------------------------------------------------------------
//	HandleUpdateEvent
//
//		Called by the event handler to redraw the top-level widget.
//		Must be called between BeginUpdate/EndUpdate: the window visRgn
//		is expected to be set to whatever needs to be drawn.
//-------------------------------------------------------------------------
nsresult nsWindow::HandleUpdateEvent()
{
	if (! mVisible)
		return NS_OK;

	// get the damaged region from the OS
	RgnHandle damagedRgn = mWindowPtr->visRgn;

	// calculate the update region relatively to the window port rect
	// (at this point, the grafPort origin should always be 0,0
	// so mWindowRegion has to be converted to window coordinates)
	RgnHandle updateRgn = ::NewRgn();
	if (updateRgn == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;
	::CopyRgn(mWindowRegion, updateRgn);

	nsRect bounds = mBounds;
	LocalToWindowCoordinate(bounds);
	::OffsetRgn(updateRgn, bounds.x, bounds.y);

	// check if the update region is visible
	::SectRgn(damagedRgn, updateRgn, updateRgn);
	if (!::EmptyRgn(updateRgn))
	{
		nsIRenderingContext* renderingContext = GetRenderingContext();
		if (renderingContext)
		{
			// determine the rect to draw
			nsRect rect;
			Rect macRect = (*updateRgn)->rgnBBox;
			::OffsetRect(&macRect, -bounds.x, -bounds.y);
			rect.SetRect(macRect.left, macRect.top, macRect.right - macRect.left, macRect.bottom - macRect.top);

			// update the widget
			UpdateWidget(rect, renderingContext);

			NS_RELEASE(renderingContext);
		}
	}
	::DisposeRgn(updateRgn);
	return NS_OK;
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void nsWindow::UpdateWidget(nsRect& aRect, nsIRenderingContext* aContext)
{
	// initialize the paint event
	nsPaintEvent paintEvent;
	paintEvent.eventStructType	= NS_PAINT_EVENT;		// nsEvent
	paintEvent.message			= NS_PAINT;
	paintEvent.widget			= this;					// nsGUIEvent
	paintEvent.nativeMsg		= NULL;
	paintEvent.renderingContext	= aContext;				// nsPaintEvent
	paintEvent.rect				= &aRect;

	// draw the widget
	StartDraw(aContext);
	if (OnPaint(paintEvent))
		DispatchWindowEvent(paintEvent);
	EndDraw();

	// recursively draw the children
	nsIEnumerator* children = GetChildren();
	if (children)
	{
		children->First();
		do
		{
			nsISupports* child;
			if (NS_SUCCEEDED(children->CurrentItem(&child)))
			{
				nsWindow* childWindow = static_cast<nsWindow*>(child);
				nsRect childBounds;
				childWindow->GetBounds(childBounds);

				// redraw only the intersection of the child rect and the update rect
				nsRect intersection;
				if (intersection.IntersectRect(aRect, childBounds))
				{
					intersection.MoveBy(-childBounds.x, -childBounds.y);
					childWindow->UpdateWidget(intersection, aContext);
				}
				NS_RELEASE(child);
    	}
		}
		while (NS_SUCCEEDED(children->Next()));			
		NS_RELEASE(children);
	}
}


//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
	// scroll the rect
	StartDraw();
		nsRect scrollRect;
		if (aClipRect)
			scrollRect = *aClipRect;
		else
		{
			scrollRect = mBounds;
			scrollRect.x = scrollRect.y = 0;
		}

		Rect macRect;
		nsRectToMacRect(scrollRect, macRect);

		RgnHandle updateRgn = ::NewRgn();
		if (updateRgn == nil)
			return NS_ERROR_OUT_OF_MEMORY;
		// ::ClipRect(&macRect);
		::ScrollRect(&macRect, aDx, aDy, updateRgn);
		::InvalRgn(updateRgn);
		::DisposeRgn(updateRgn);
	EndDraw();

	// scroll the children
	nsIEnumerator* children = GetChildren();
	if (children)
	{
		children->First();
		do
		{
			nsISupports* child;
			if (NS_SUCCEEDED(children->CurrentItem(&child)))
			{
				nsWindow* childWindow = static_cast<nsWindow*>(child);

				nsRect bounds;
				childWindow->GetBounds(bounds);
				bounds.x += aDx;
				bounds.y += aDy;
				childWindow->SetBounds(bounds);
				childWindow->Scroll(aDx, aDy, &bounds);

				NS_RELEASE(child);
  		}
		}
		while (NS_SUCCEEDED(children->Next()));			
		NS_RELEASE(children);
	}
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------

PRBool nsWindow::ConvertStatus(nsEventStatus aStatus)
{
  switch (aStatus)
  {
    case nsEventStatus_eIgnore:							return(PR_FALSE);
    case nsEventStatus_eConsumeNoDefault:		return(PR_TRUE);	// don't do default processing
    case nsEventStatus_eConsumeDoDefault:		return(PR_FALSE);
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
NS_IMETHODIMP nsWindow::DispatchEvent(nsGUIEvent* event, nsEventStatus& aStatus)
{
  aStatus = nsEventStatus_eIgnore;
	if (! mDestructorCalled)
	{
		nsIWidget* aWidget = event->widget;
		NS_IF_ADDREF(aWidget);
	  
	  if (nsnull != mMenuListener){
	    if(NS_MENU_EVENT == event->eventStructType)
	  	  aStatus = mMenuListener->MenuSelected( static_cast<nsMenuEvent&>(*event) );
	  }
	  if (mEventCallback)
	    aStatus = (*mEventCallback)(event);

		// Dispatch to event listener if event was not consumed
	  if ((aStatus != nsEventStatus_eConsumeNoDefault) && (mEventListener != nsnull))
	    aStatus = mEventListener->ProcessEvent(*event);

		NS_IF_RELEASE(aWidget);
	}
  return NS_OK;
}

//-------------------------------------------------------------------------
PRBool nsWindow::DispatchWindowEvent(nsGUIEvent &event)
{
  nsEventStatus status;
  DispatchEvent(&event, status);
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
    result = (DispatchWindowEvent(aEvent));
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

#pragma mark -

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool nsWindow::ReportDestroyEvent()
{
	// nsEvent
	nsGUIEvent moveEvent;
	moveEvent.eventStructType = NS_GUI_EVENT;
	moveEvent.message			= NS_DESTROY;
	moveEvent.point.x			= 0;
	moveEvent.point.y			= 0;
	moveEvent.time				= PR_IntervalNow();

	// nsGUIEvent
	moveEvent.widget			= this;
	moveEvent.nativeMsg		= nsnull;

	// dispatch event
	return (DispatchWindowEvent(moveEvent));
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool nsWindow::ReportMoveEvent()
{
	// nsEvent
	nsGUIEvent moveEvent;
	moveEvent.eventStructType = NS_GUI_EVENT;
	moveEvent.message			= NS_MOVE;
	moveEvent.point.x			= mBounds.x;
	moveEvent.point.y			= mBounds.y;
	moveEvent.time				= PR_IntervalNow();

	// nsGUIEvent
	moveEvent.widget			= this;
	moveEvent.nativeMsg		= nsnull;

	// dispatch event
	return (DispatchWindowEvent(moveEvent));
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool nsWindow::ReportSizeEvent()
{
	// nsEvent
	nsSizeEvent sizeEvent;
	sizeEvent.eventStructType = NS_SIZE_EVENT;
	sizeEvent.message			= NS_SIZE;
	sizeEvent.point.x			= 0;
	sizeEvent.point.y			= 0;
	sizeEvent.time				= PR_IntervalNow();

	// nsGUIEvent
	sizeEvent.widget			= this;
	sizeEvent.nativeMsg		= nsnull;

	// nsSizeEvent
	sizeEvent.windowSize	= &mBounds;
	sizeEvent.mWinWidth		= mBounds.width;
	sizeEvent.mWinHeight	= mBounds.height;
  
	// dispatch event
	return(DispatchWindowEvent(sizeEvent));
}



#pragma mark -
//-------------------------------------------------------------------------
/*
 *  @update  dc 08/28/98
 *  @param   aTheRegion -- The region to intersect with for this widget
 *  @return  PR_TRUE if the these regions intersect
 */

PRBool nsWindow::RgnIntersects(RgnHandle aTheRegion, RgnHandle aIntersectRgn)
{
	::SectRgn(aTheRegion, this->mWindowRegion, aIntersectRgn);
	return (::EmptyRgn(aIntersectRgn) != false);
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
	aX = aY = 0;
	nsIWidget* grandParent;
	nsIWidget* theParent = this->GetParent();
	while (theParent)
	{
		nsRect theRect;
		theParent->GetBounds(theRect);
		aX += theRect.x;
		aY += theRect.y;

		grandParent = theParent->GetParent();
		NS_IF_RELEASE(theParent);
		theParent = grandParent;
	}
	return NS_OK;
}


//-------------------------------------------------------------------------
// PointInWidget
//		Find if a point in local coordinates is inside this object
//-------------------------------------------------------------------------
PRBool nsWindow::PointInWidget(Point aThePoint)
{
	// get the origin in local coordinates
	nsPoint widgetOrigin(0, 0);
	LocalToWindowCoordinate(widgetOrigin);

	// get rectangle relatively to the parent
	nsRect widgetRect;
	GetBounds(widgetRect);

	// convert the topLeft corner to local coordinates
	widgetRect.MoveBy(widgetOrigin.x, widgetOrigin.y);

	// finally tell whether it's a hit
	return(widgetRect.Contains(aThePoint.h, aThePoint.v));
}


//-------------------------------------------------------------------------
// FindWidgetHit
//		Recursively look for the widget hit
//		@param aParent   -- parent widget. 
//		@param aThePoint -- a point in local coordinates to test for the hit. 
//-------------------------------------------------------------------------
nsWindow*  nsWindow::FindWidgetHit(Point aThePoint)
{
	if (!mVisible || !PointInWidget(aThePoint))
		return nsnull;

	nsWindow* widgetHit = this;

	nsIBidirectionalEnumerator* children = static_cast<nsIBidirectionalEnumerator*>(GetChildren());
	if (children)
	{
		// traverse through all the nsWindows to find out who got hit, lowest level of course
		children->Last();
		do
		{
			nsISupports* child;
      if (NS_SUCCEEDED(children->CurrentItem(&child)))
      {
      	nsWindow* childWindow = static_cast<nsWindow*>(child);
			  NS_RELEASE(child);
			  nsWindow* deeperHit = childWindow->FindWidgetHit(aThePoint);
			  if (deeperHit)
			  {
				  widgetHit = deeperHit;
				  break;
			  }
      }
		}
    while (NS_SUCCEEDED(children->Prev()));
		NS_RELEASE(children);
	}

	return widgetHit;
}

#pragma mark -
#pragma mark - will be sorted -
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


//=================================================================
/*  Convert the coordinates to some device coordinates so GFX can draw.
 *  @update  dc 09/16/98
 *  @param   nscoord -- X coordinate to convert
 *  @param   nscoord -- Y coordinate to convert
 *  @return  NONE
 */
void  nsWindow::ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY)
{
	PRInt32	offX, offY;
  this->CalcOffset(offX,offY);

	aX += offX;
	aY += offY;
}

/*
 * Convert an nsPoint into mac local coordinates.
 * The tree hierarchy is navigated upwards, changing
 * the x,y offset by the parent's coordinates
 *
 */
void nsWindow::LocalToWindowCoordinate(nsPoint& aPoint)
{
	ConvertToDeviceCoordinates(aPoint.x, aPoint.y);
}

/* 
 * Convert widget local coordinates into mac local coordinates
 */
void nsWindow::LocalToWindowCoordinate(nscoord& aX, nscoord& aY)
{
	ConvertToDeviceCoordinates(aX, aY);
}

/* 
 * Convert an nsRect into mac local coordinates
 */
void nsWindow::LocalToWindowCoordinate(nsRect& aRect)
{
	ConvertToDeviceCoordinates(aRect.x, aRect.y);
}

//=================================================================
/*  Convert the device coordinates to local coordinates.
 *  @update  dc 09/16/98
 *  @param   nscoord -- X coordinate to convert
 *  @param   nscoord -- Y coordinate to convert
 *  @return  NONE
 */
void  nsWindow::ConvertToLocalCoordinates(nscoord &aX, nscoord &aY)
{
	PRInt32	offX, offY;
	this->CalcOffset(offX,offY);

	aX -= offX;
	aY -= offY;
}

/*
 * Convert an nsPoint into mac local coordinates.
 * The tree hierarchy is navigated upwards, changing
 * the x,y offset by the parent's coordinates
 *
 */
void nsWindow::WindowToLocalCoordinate(nsPoint& aPoint)
{
	ConvertToLocalCoordinates(aPoint.x, aPoint.y);
}

/* 
 * Convert widget local coordinates into mac local coordinates
 */
void nsWindow::WindowToLocalCoordinate(nscoord& aX, nscoord& aY)
{
	ConvertToLocalCoordinates(aX, aY);
}

/* 
 * Convert an nsRect into mac local coordinates
 */
void nsWindow::WindowToLocalCoordinate(nsRect& aRect)
{
	ConvertToLocalCoordinates(aRect.x, aRect.y);
}

#pragma mark -
#pragma mark - will be gone -
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
