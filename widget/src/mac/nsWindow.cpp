/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "prmem.h"

#include <Appearance.h>
#include <Timer.h>
#include <Icons.h>
#include <Errors.h>

#include "nsplugindefs.h"
#include "nsMacEventHandler.h"
#include "nsMacResources.h"
#include "nsRegionMac.h"
#include "nsIRollupListener.h"

#include "nsCarbonHelpers.h"
#include "nsGfxUtils.h"

#if PINK_PROFILING
#include "profilerutils.h"
#endif


////////////////////////////////////////////////////
nsIRollupListener * gRollupListener = nsnull;
nsIWidget         * gRollupWidget   = nsnull;

// Since we only want a single notification pending for the app we'll declare
// these static
static NMRec	gNMRec;
static Boolean	gNotificationInstalled = false;

// Routines for iterating over the rects of a region. Carbon and pre-Carbon
// do this differently so provide a way to do both.
#if TARGET_CARBON
static RegionToRectsUPP sUpdateRectProc = nsnull;
static RegionToRectsUPP sAddRectToArrayProc = nsnull;
static RegionToRectsUPP sCountRectProc = nsnull;
#else
void EachRegionRect (RgnHandle r, void (* proc)(Rect *, void *), void* data) ;
#endif

#pragma mark -

//#define PAINT_DEBUGGING         // flash areas as they are painted
//#define INVALIDATE_DEBUGGING    // flash areas as they are invalidated

#if defined(INVALIDATE_DEBUGGING) || defined(PAINT_DEBUGGING)
static void blinkRect(Rect* r);
static void blinkRgn(RgnHandle rgn);
#endif


//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow() : nsBaseWidget() , nsDeleteObserved(this), nsIKBStateControl()
{
	WIDGET_SET_CLASSNAME("nsWindow");

  mParent = nsnull;
  mBounds.SetRect(0,0,0,0);

  mResizingChildren = PR_FALSE;
  mVisible = PR_FALSE;
  mEnabled = PR_TRUE;
  SetPreferredSize(0,0);

  mFontMetrics = nsnull;
  mMenuBar = nsnull;
  mTempRenderingContext = nsnull;

  mWindowRegion = nsnull;
  mVisRegion = nsnull;
  mWindowPtr = nsnull;
  mDrawing = PR_FALSE;
  mDestroyCalled = PR_FALSE;
  mDestructorCalled = PR_FALSE;

  SetBackgroundColor(NS_RGB(255, 255, 255));
  SetForegroundColor(NS_RGB(0, 0, 0));

  mPluginPort = nsnull;

  AcceptFocusOnClick(PR_TRUE);
  
#if TARGET_CARBON
  if ( !sUpdateRectProc ) {
    sUpdateRectProc = NewRegionToRectsUPP ( PaintUpdateRectProc );
    sAddRectToArrayProc = NewRegionToRectsUPP ( AddRectToArrayProc );
    sCountRectProc = NewRegionToRectsUPP ( CountRectProc );
  }
#endif

}


//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
	// notify the children that we're gone
	nsCOMPtr<nsIEnumerator> children ( getter_AddRefs(GetChildren()) );
	if (children)
	{
		children->First();
		do
		{
			nsISupports* child;
			if (NS_SUCCEEDED(children->CurrentItem(&child)))
			{
				nsWindow* childWindow = static_cast<nsWindow*>(static_cast<nsIWidget*>(child));
				NS_RELEASE(child);

				childWindow->mParent = nsnull;
    	}
		} while (NS_SUCCEEDED(children->Next()));			
	}

	mDestructorCalled = PR_TRUE;

	//Destroy();

	if (mWindowRegion)
	{
		::DisposeRgn(mWindowRegion);
		mWindowRegion = nsnull;	
	}

	if (mVisRegion)
	{
		::DisposeRgn(mVisRegion);
		mVisRegion = nsnull;	
	}
			
	NS_IF_RELEASE(mTempRenderingContext);
	
	NS_IF_RELEASE(mFontMetrics);
	NS_IF_RELEASE(mMenuBar);
	NS_IF_RELEASE(mMenuListener);
	
	if (mPluginPort) {
		delete mPluginPort;
	}
}

NS_IMPL_ISUPPORTS_INHERITED1(nsWindow, nsBaseWidget, nsIKBStateControl);

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
	CalcWindowRegions();

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
/* this is always null
		else if (aAppShell)
			mWindowPtr = (WindowPtr)aAppShell->GetNativeData(NS_NATIVE_SHELL);
*/
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
	mParent = 0;

  // just to be safe. If we're going away and for some reason we're still
  // the rollup widget, rollup and turn off capture.
  if ( this == gRollupWidget ) {
    if ( gRollupListener )
      gRollupListener->Rollup();
    CaptureRollupEvents(nsnull, PR_FALSE, PR_TRUE);
  }

	NS_IF_RELEASE(mMenuBar);
	SetMenuBar(nsnull);

	ReportDestroyEvent();	// beard: this seems to cause the window to be deleted. moved all release code to destructor.

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
    // pinkerton
    // Windows and GrafPorts are VERY different under Carbon, and we can no
    // longer pass them interchagably. When we ask for a GrafPort, we cannot
    // return a window or vice versa.
      retVal = (void*)::GetWindowPort(mWindowPtr);
      break;
      
    case NS_NATIVE_DISPLAY:
      retVal = (void*)mWindowPtr;
    	break;

    case NS_NATIVE_REGION:
		retVal = (void*)mVisRegion;
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
		mPluginPort->port = ::GetWindowPort(mWindowPtr);
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

    
NS_IMETHODIMP nsWindow::ModalEventFilter(PRBool aRealEvent, void *aEvent,
                                         PRBool *aForWindow)
{
	*aForWindow = PR_FALSE;
	EventRecord *theEvent = (EventRecord *) aEvent;

	if (aRealEvent && theEvent->what != nullEvent ) {

		WindowPtr window = (WindowPtr) GetNativeData(NS_NATIVE_DISPLAY);
		WindowPtr rollupWindow = gRollupWidget ? (WindowPtr) gRollupWidget->GetNativeData(NS_NATIVE_DISPLAY) : nsnull;
		WindowPtr eventWindow = nsnull;
		
		PRInt16 where = ::FindWindow ( theEvent->where, &eventWindow );
		PRBool inWindow = eventWindow && (eventWindow == window || eventWindow == rollupWindow);

		switch ( theEvent->what ) {
			// is it a mouse event?
			case mouseUp:
					*aForWindow = PR_TRUE;
				break;
			case mouseDown:
				// is it in the given window?
				// (note we also let some events questionable for modal dialogs pass through.
				// but it makes sense that the draggability et.al. of a modal window should
				// be controlled by whether the window has a drag bar).
				if (inWindow) {
				     if ( where == inContent || where == inDrag   || where == inGrow ||
				          where == inGoAway  || where == inZoomIn || where == inZoomOut )
					*aForWindow = PR_TRUE;
				}
				else      // we're in another window.
				{
				  // let's handle dragging of background windows here
				  if (eventWindow && (where == inDrag) && (theEvent->modifiers & cmdKey))
				  {
    				Rect screenRect;
    				::GetRegionBounds(::GetGrayRgn(), &screenRect);
				    ::DragWindow(eventWindow, theEvent->where, &screenRect);
				  }
				  
  				*aForWindow = PR_FALSE;
				}
				break;
			case keyDown:
			case keyUp:
			case autoKey:
				*aForWindow = PR_TRUE;
				break;

			case diskEvt:
			    // I think dialogs might want to support floppy insertion, and it doesn't
			    // interfere with modality...
			case updateEvt:
				// always let update events through, because if we don't handle them, we're
				// doomed!
			case activateEvt:
				// activate events aren't so much a request as simply informative. might
				// as well acknowledge them.
				*aForWindow = PR_TRUE;
				break;

			case osEvt:
				// check for mouseMoved or suspend/resume events. We especially need to
				// let suspend/resume events through in order to make sure the clipboard is
				// converted correctly.
				unsigned char eventType = (theEvent->message >> 24) & 0x00ff;
				if (eventType == mouseMovedMessage) {
					// I'm guessing we don't want to let these through unless the mouse is
					// in the modal dialog so we don't see rollover feedback in windows behind
					// the dialog.
					if ( where == inContent && inWindow )
						*aForWindow = PR_TRUE;
				}
				if ( eventType == suspendResumeMessage ) {
					*aForWindow = PR_TRUE;
					if (theEvent->message & resumeFlag) {
						// divert it to our window if it isn't naturally
						if (!inWindow) {
							StPortSetter portSetter(window);
							theEvent->where.v = 0;
							theEvent->where.h = 0;
							::LocalToGlobal(&theEvent->where);
						}
				  	}
				}

				break;
		} // case of which event type
	} else
		*aForWindow = PR_TRUE;

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

    
static Boolean we_are_front_process()
{
	ProcessSerialNumber	thisPSN;
	ProcessSerialNumber	frontPSN;
	(void)::GetCurrentProcess(&thisPSN);
	if (::GetFrontProcess(&frontPSN) == noErr)
	{
		if ((frontPSN.highLongOfPSN == thisPSN.highLongOfPSN) &&
			(frontPSN.lowLongOfPSN == thisPSN.lowLongOfPSN))
			return true;
	}
	return false;
}

//-------------------------------------------------------------------------
//
// Set the focus on this component
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::SetFocus(PRBool aRaise)
{
	gEventDispatchHandler.SetFocus(this);
	
	// Here's where we see if there's a notification we need to remove
	if (gNotificationInstalled && we_are_front_process())
	{
		(void)::NMRemove(&gNMRec);
		gNotificationInstalled = false;
	}
	
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
  if (mMenuBar)
    mMenuBar->SetParent(nsnull);
  NS_IF_RELEASE(mMenuBar);
  NS_IF_ADDREF(aMenuBar);
  mMenuBar = aMenuBar;
  return NS_OK;
}

NS_IMETHODIMP nsWindow::ShowMenuBar(PRBool aShow)
{
  // this may never be implemented on the Mac
  return NS_ERROR_FAILURE;
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
	
	// allow the cursor to be set internally if we're in the bg, but
	// don't actually set it.
  if ( !nsToolkit::IsAppInForeground() )
    return NS_OK;

  // mac specific cursor manipulation
	if (nsToolkit::HasAppearanceManager())
	{
		short cursor = -1;
	  switch (aCursor)
	  {
	    case eCursor_standard:				cursor = kThemeArrowCursor;						break;
	    case eCursor_wait:						cursor = kThemeWatchCursor;						break;
	    case eCursor_select:					cursor = kThemeIBeamCursor;						break;
	    case eCursor_hyperlink:				cursor = kThemePointingHandCursor;		break;
			case eCursor_sizeWE:					cursor = kThemeResizeLeftRightCursor;	break;
			case eCursor_sizeNS:					cursor = 129; 	break;
			case eCursor_sizeNW:					cursor = 130; 	break;
			case eCursor_sizeSE:					cursor = 131; 	break;
			case eCursor_sizeNE:					cursor = 132; 	break;
			case eCursor_sizeSW:					cursor = 133; 	break;
			case eCursor_arrow_north:			cursor = 134; 	break;
			case eCursor_arrow_north_plus:cursor = 135; 	break;
			case eCursor_arrow_south:			cursor = 136; 	break;
			case eCursor_arrow_south_plus:cursor = 137; 	break;
			case eCursor_arrow_west:			cursor = 138; 	break;
			case eCursor_arrow_west_plus:	cursor = 139; 	break;
			case eCursor_arrow_east:			cursor = 140; 	break;
			case eCursor_arrow_east_plus:	cursor = 141; 	break;
			case eCursor_crosshair:				cursor = kThemeCrossCursor; 					break;
			case eCursor_move:						cursor = kThemeOpenHandCursor; 				break;
			case eCursor_help:						cursor = 143; 	break;
			case eCursor_copy:						cursor = 144; 	break; // CSS3
			case eCursor_alias:						cursor = 145; 	break;
			case eCursor_context_menu:		cursor = 146; 	break;
			case eCursor_cell:						cursor = kThemePlusCursor; 			 			break;
			case eCursor_grab:						cursor = kThemeOpenHandCursor; 				break;
			case eCursor_grabbing:				cursor = kThemeClosedHandCursor; 			break;
			case eCursor_spinning:				cursor = 149; break;  // better than kThemeSpinningCursor
			case eCursor_count_up:				cursor = kThemeCountingUpHandCursor; 					break;
			case eCursor_count_down:			cursor = kThemeCountingDownHandCursor;				break;
			case eCursor_count_up_down:		cursor = kThemeCountingUpAndDownHandCursor; 	break;

	  }
	  if (cursor >= 0)
	  {
	  	if (cursor >= 128)
	  	{
        nsMacResources::OpenLocalResourceFile();
        CursHandle cursHandle = ::GetCursor(cursor);
        NS_ASSERTION ( cursHandle, "Can't load cursor, is the resource file installed correctly?" );
        if ( cursHandle )
          ::SetCursor(*cursHandle);
        nsMacResources::CloseLocalResourceFile();
			}
			else
	  		::SetThemeCursor(cursor);
	  }
  }
  else
  {
		short cursor = -1;
	  switch (aCursor)
	  {
	    case eCursor_standard:				::InitCursor();					break;
	    case eCursor_wait:						cursor = watchCursor;		break;
	    case eCursor_select:					cursor = iBeamCursor;		break;
	    case eCursor_hyperlink:				cursor = plusCursor;		break;
			case eCursor_sizeWE:					cursor = 128; 					break;
			case eCursor_sizeNS:					cursor = 129; 	break;
			case eCursor_sizeNW:					cursor = 130; 	break;
			case eCursor_sizeSE:					cursor = 131; 	break;
			case eCursor_sizeNE:					cursor = 132; 	break;
			case eCursor_sizeSW:					cursor = 133; 	break;
			case eCursor_arrow_north:			cursor = 134; 	break;
			case eCursor_arrow_north_plus:cursor = 135; 	break;
			case eCursor_arrow_south:			cursor = 136; 	break;
			case eCursor_arrow_south_plus:cursor = 137; 	break;
			case eCursor_arrow_west:			cursor = 138; 	break;
			case eCursor_arrow_west_plus:	cursor = 139; 	break;
			case eCursor_arrow_east:			cursor = 140; 	break;
			case eCursor_arrow_east_plus:	cursor = 141; 	break;
			case eCursor_crosshair:				cursor = crossCursor; 	break;
			case eCursor_move:						cursor = 142; 	break;
			case eCursor_help:						cursor = 143; 	break;
			case eCursor_copy:						cursor = 144; 	break; // CSS3
			case eCursor_alias:						cursor = 145; 	break;
			case eCursor_context_menu:		cursor = 146; 	break;
			case eCursor_cell:						cursor = plusCursor; 	 break;
			case eCursor_grab:						cursor = 147; 	break;
			case eCursor_grabbing:				cursor = 148; 	break;
			case eCursor_spinning:				cursor = 149; 	break;
			case eCursor_count_up:				cursor = watchCursor; 	break;
			case eCursor_count_down:			cursor = watchCursor; 	break;
			case eCursor_count_up_down:		cursor = watchCursor; 	break;
	  }
	  if (cursor > 0)
	  {
      if (cursor >= 128) {
        nsMacResources::OpenLocalResourceFile();
        CursHandle cursHandle = ::GetCursor(cursor);
        NS_ASSERTION ( cursHandle, "Can't load cursor, is the resource file installed correctly?" );
        if ( cursHandle )
          ::SetCursor(*cursHandle);
        nsMacResources::CloseLocalResourceFile();
      }
      else {
        CursHandle cursHandle = ::GetCursor(cursor);
        NS_ASSERTION ( cursHandle, "Can't load cursor, is the resource file installed correctly?" );
        if ( cursHandle )
          ::SetCursor(*cursHandle);
      }
	  }
  }
 
  return NS_OK;
  
} // nsWindow::SetCursor

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


NS_METHOD nsWindow::SetBounds(const nsRect &aRect)
{
  nsresult rv = Inherited::SetBounds(aRect);
  if ( NS_SUCCEEDED(rv) )
    CalcWindowRegions();

  return rv;
}


NS_IMETHODIMP nsWindow::ConstrainPosition(PRInt32 *aX, PRInt32 *aY)
{
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Move this component
// aX and aY are in the parent widget coordinate system
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
	if ((mBounds.x != aX) || (mBounds.y != aY))
	{
		// Invalidate the current location (unless it's the top-level window)
		if (mParent != nsnull)
			Invalidate(PR_FALSE);
	  	
		// Set the bounds
		mBounds.x = aX;
		mBounds.y = aY;

		// Recalculate the regions
		CalcWindowRegions();

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
NS_IMETHODIMP nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  if ((mBounds.width != aWidth) || (mBounds.height != aHeight))
  {
    // Set the bounds
    mBounds.width  = aWidth;
    mBounds.height = aHeight;

	// Recalculate the regions
	CalcWindowRegions();
	
    // Invalidate the new location
    if (aRepaint)
      Invalidate(PR_FALSE);

    // Report the event
    ReportSizeEvent();
  }
  else {
    // Recalculate the regions. We always need to do this, our parents may have
    // changed, hence changing our notion of visibility. We then also should make
    // sure that we invalidate ourselves correctly. Fixes bug 18240 (pinkerton).
    CalcWindowRegions();
    if (aRepaint)
      Invalidate(PR_FALSE);
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
	Move(aX, aY);
	Resize(aWidth, aHeight, aRepaint);
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
	mResizingChildren = PR_TRUE;
	mSaveVisible = mVisible;
	mVisible = PR_FALSE;

	return NS_OK;
}

//-------------------------------------------------------------------------
// 
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::EndResizingChildren(void)
{
	mResizingChildren = PR_FALSE;
	mVisible = mSaveVisible;

	CalcWindowRegions();
	return NS_OK;
}

#pragma mark -

static Boolean KeyDown(const UInt8 theKey)
{
	KeyMap map;
	GetKeys(map);
	return ((*((UInt8 *)map + (theKey >> 3)) >> (theKey & 7)) & 1) != 0;
}

#if defined(INVALIDATE_DEBUGGING) || defined(PAINT_DEBUGGING)

static Boolean caps_lock()
{
  return KeyDown(0x39);
}

static void blinkRect(Rect* r)
{
	StRegionFromPool oldClip;
	if (oldClip != NULL)
		::GetClip(oldClip);

	::ClipRect(r);
	::InvertRect(r);
	UInt32 end = ::TickCount() + 5;
	while (::TickCount() < end) ;
	::InvertRect(r);

	if (oldClip != NULL)
		::SetClip(oldClip);
}

static void blinkRgn(RgnHandle rgn)
{
	StRegionFromPool oldClip;
	if (oldClip != NULL)
		::GetClip(oldClip);

	::SetClip(rgn);
	::InvertRgn(rgn);
	UInt32 end = ::TickCount() + 5;
	while (::TickCount() < end) ;
	::InvertRgn(rgn);

	if (oldClip != NULL)
		::SetClip(oldClip);
}

#endif

//-------------------------------------------------------------------------
//
// Invalidate this component's visible area
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Invalidate(PRBool aIsSynchronous)
{
	nsRect area = mBounds;
	area.x = area.y = 0;
	Invalidate(area, aIsSynchronous);
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Invalidate this component's visible area
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Invalidate(const nsRect &aRect, PRBool aIsSynchronous)
{
	if (!mWindowPtr)
		return NS_OK;

	nsRect wRect = aRect;
	wRect.MoveBy(mBounds.x, mBounds.y);				// beard:  this is required, see GetNativeData(NS_NATIVE_OFFSETX).
	LocalToWindowCoordinate(wRect);
	Rect macRect;
	nsRectToMacRect(wRect, macRect);

	StPortSetter portSetter(mWindowPtr);
	Rect savePortRect;
	::GetWindowPortBounds(mWindowPtr, &savePortRect);
	::SetOrigin(0, 0);

#ifdef INVALIDATE_DEBUGGING
	if (caps_lock())
		::blinkRect(&macRect);
#endif

	::InvalWindowRect(mWindowPtr, &macRect);
	if ( aIsSynchronous )
	  Update();
	::SetOrigin(savePortRect.left, savePortRect.top);

	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Invalidate this component's visible area
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWindow::InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous)
{
	if (!mWindowPtr)
		return NS_OK;

	// copy invalid region into a working region.
	void* nativeRgn;
	aRegion->GetNativeRegion(nativeRgn);
	StRegionFromPool windowRgn;
	::CopyRgn(RgnHandle(nativeRgn), windowRgn);

	// translate this region into window coordinates.
	PRInt32	offX, offY;
	this->CalcOffset(offX, offY);
	::OffsetRgn(windowRgn, mBounds.x + offX, mBounds.y + offY);
	
	StPortSetter portSetter(mWindowPtr);
	Rect savePortRect;
	::GetWindowPortBounds(mWindowPtr, &savePortRect);
	::SetOrigin(0, 0);

#ifdef INVALIDATE_DEBUGGING
	if (caps_lock())
		::blinkRgn(windowRgn);
#endif

	::InvalWindowRgn(mWindowPtr, windowRgn);
	if ( aIsSynchronous )
	  Update();
	::SetOrigin(savePortRect.left, savePortRect.top);

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

	CalcWindowRegions();	//¥REVISIT

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
void
nsWindow::Flash(nsPaintEvent	&aEvent)
{
#ifdef NS_DEBUG
	Rect flashRect;
	if (debug_WantPaintFlashing() && aEvent.rect ) {
		::SetRect ( &flashRect, aEvent.rect->x, aEvent.rect->y, aEvent.rect->x + aEvent.rect->width,
	          aEvent.rect->y + aEvent.rect->height );
		StPortSetter portSetter(mWindowPtr);
		unsigned long endTicks;
		::InvertRect ( &flashRect );
		::Delay(10, &endTicks);
		::InvertRect ( &flashRect );
	}
#endif
}


//
// OnPaint
//
// Dummy impl, meant to be overridden
//
PRBool
nsWindow::OnPaint(nsPaintEvent &event)
{
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
	if (! mVisible || !mWindowPtr)
		return NS_OK;

	static PRBool  reentrant = PR_FALSE;

	if (reentrant)
		HandleUpdateEvent(nil);
	else
	{
		reentrant = PR_TRUE;

		StRegionFromPool regionToValidate;
		if (!regionToValidate)
			return NS_ERROR_OUT_OF_MEMORY;

		// make a copy of the window update rgn
		StRegionFromPool saveUpdateRgn;
		if (!saveUpdateRgn)
			return NS_ERROR_OUT_OF_MEMORY;
		if(mWindowPtr)
			::GetWindowUpdateRegion ( mWindowPtr, saveUpdateRgn );

		// draw the widget
		StPortSetter portSetter(mWindowPtr);

		::BeginUpdate(mWindowPtr);
		HandleUpdateEvent(regionToValidate);
		::EndUpdate(mWindowPtr);

		// restore the window update rgn
#if TARGET_CARBON
		// saveUpdateRgn is in global coords, so we need to shift it to local coords
		Point origin = {0, 0};
		::GlobalToLocal(&origin);
		::OffsetRgn(saveUpdateRgn, origin.h, origin.v);
		::InvalWindowRgn(mWindowPtr, saveUpdateRgn);
#else
	  ::CopyRgn(saveUpdateRgn, ((WindowRecord*)mWindowPtr)->updateRgn);
#endif

    ::ValidWindowRgn(mWindowPtr, regionToValidate);
    
		reentrant = PR_FALSE;
	}

	return NS_OK;
}


#pragma mark -


static Boolean control_key_down()
{
	EventRecord event;
	::EventAvail(0, &event);
	return (event.modifiers & controlKey) != 0;
}

static long long microseconds()
{
	unsigned long long micros;
	Microseconds((UnsignedWide*)&micros);
	return micros;
}


#if TARGET_CARBON


//
// UpdateRect
//
// Called once for every rect in the update region. Send a paint event to Gecko to handle
// this. |refCon| contains the |nsWindow| being updated. 
//
OSStatus
nsWindow::PaintUpdateRectProc (UInt16 message, RgnHandle rgn, const Rect *inDirtyRect, void *refCon)
{
  if (message == kQDRegionToRectsMsgParse) {
    nsWindow* self = NS_REINTERPRET_CAST(nsWindow*, refCon);
    Rect dirtyRect = *inDirtyRect;
  
  	nsCOMPtr<nsIRenderingContext> renderingContext ( dont_AddRef(self->GetRenderingContext()) );
  	if (renderingContext)
  	{
    	nsRect bounds = self->mBounds;
    	self->LocalToWindowCoordinate(bounds);
    	
  		// determine the rect to draw
  		::OffsetRect(&dirtyRect, -bounds.x, -bounds.y);
  		nsRect rect ( dirtyRect.left, dirtyRect.top, dirtyRect.right - dirtyRect.left,
  		                dirtyRect.bottom - dirtyRect.top );

  		// update the widget
  		self->UpdateWidget(rect, renderingContext);
    }
  }
  
  return noErr;
  
} // PaintUpdateRectProc


//
// AddRectToArrayProc
//
// Add each rect to an array so we can post-process them. |inArray| is a
// pointer to the TRectArray we're adding to.
//
OSStatus
nsWindow::AddRectToArrayProc (UInt16 message, RgnHandle rgn, const Rect *inDirtyRect, void *inArray)
{
  if (message == kQDRegionToRectsMsgParse) {
    NS_ASSERTION ( inArray, "You better pass an array!" );
    TRectArray* rectArray = NS_REINTERPRET_CAST(TRectArray*, inArray);
  
    rectArray->mRectList[rectArray->Count()] = *inDirtyRect;
    ++rectArray->mNumRects;
  }
  
  return noErr;
}


//
// CountUpdateRectProc
// 
// Used to count the number of rects in a region. |refCon| is a pointer to a
// counter. We just increment it every time we're called and by the end we'll have
// the number of rects.
//
OSStatus
nsWindow::CountRectProc (UInt16 message, RgnHandle rgn, const Rect *inDirtyRect, void *refCon)
{
  if (message == kQDRegionToRectsMsgParse) {
    NS_ASSERTION ( refCon, "You better pass a counter!" );
    (*NS_REINTERPRET_CAST(long*, refCon))++;                  // increment
  }
  return noErr;
}


#endif

//
// UpdateRect
//
// Called once for every rect in the update region. Send a paint event to Gecko to handle
// this. |inData| contains the |nsWindow| being updated. 
//
void 
nsWindow::PaintUpdateRect (Rect *inDirtyRect, void* inData)
{
  nsWindow* self = NS_REINTERPRET_CAST(nsWindow*, inData);
  Rect dirtyRect = *inDirtyRect;
   
	nsCOMPtr<nsIRenderingContext> renderingContext ( dont_AddRef(self->GetRenderingContext()) );
	if (renderingContext)
	{
  	nsRect bounds = self->mBounds;
  	self->LocalToWindowCoordinate(bounds);
  	
		// determine the rect to draw
		::OffsetRect(&dirtyRect, -bounds.x, -bounds.y);
		nsRect rect ( dirtyRect.left, dirtyRect.top, dirtyRect.right - dirtyRect.left,
		                dirtyRect.bottom - dirtyRect.top );

		// update the widget
		self->UpdateWidget(rect, renderingContext);
  }

} // PaintUpdateRect


//
// CountRect
//
// Used to count the number of rects in a region. |refCon| is a pointer to a
// counter. We just increment it every time we're called and by the end we'll have
// the number of rects.
//
void
nsWindow::CountRect ( Rect *inDirtyRect, void *refCon )
{
  NS_ASSERTION ( refCon, "You better pass a counter!" );
  (*NS_REINTERPRET_CAST(long*, refCon))++;                  // increment
}


//
// AddRectToArray
//
// Add each rect to an array so we can post-process them. |inArray| is a
// pointer to the TRectArray we're adding to.
//
void
nsWindow::AddRectToArray ( Rect* inDirtyRect, void* inArray )
{
  NS_ASSERTION ( inArray, "You better pass an array!" );
  TRectArray* rectArray = NS_REINTERPRET_CAST(TRectArray*, inArray);
  
  rectArray->mRectList[rectArray->Count()] = *inDirtyRect;
  ++rectArray->mNumRects;
  
} // AddRectToArray


//-------------------------------------------------------------------------
//	HandleUpdateEvent
//
//		Called by the event handler to redraw the top-level widget.
//		Must be called between BeginUpdate/EndUpdate: the window visRgn
//		is expected to be set to whatever needs to be drawn.
//-------------------------------------------------------------------------
nsresult nsWindow::HandleUpdateEvent(RgnHandle regionToValidate)
{
#if PINK_PROFILING
if (KeyDown(0x39))	// press [caps lock] to start the profile
	ProfileStart();
else
	ProfileStop(); 
#endif

	if (! mVisible)
		return NS_OK;

	// make sure the port is set and origin is (0, 0).
	StPortSetter portSetter(mWindowPtr);
	::SetOrigin(0, 0);
	
	// get the damaged region from the OS
	StRegionFromPool damagedRgn;
	if (!damagedRgn)
		return NS_ERROR_OUT_OF_MEMORY;
	::GetPortVisibleRegion(GrafPtr(GetWindowPort(mWindowPtr)), damagedRgn);

#ifdef PAINT_DEBUGGING	
	blinkRgn(damagedRgn);
#endif

	// calculate the update region relatively to the window port rect
	// (at this point, the grafPort origin should always be 0,0
	// so mWindowRegion has to be converted to window coordinates)
	StRegionFromPool updateRgn;
	if (!updateRgn)
		return NS_ERROR_OUT_OF_MEMORY;
	::CopyRgn(mWindowRegion, updateRgn);

	nsRect bounds = mBounds;
	LocalToWindowCoordinate(bounds);
	::OffsetRgn(updateRgn, bounds.x, bounds.y);

#ifdef PAINT_DEBUGGING
	blinkRgn(updateRgn);
#endif
	
	// check if the update region is visible
	::SectRgn(damagedRgn, updateRgn, updateRgn);
	if (!::EmptyRgn(updateRgn))
	{
#if DEBUG
		// measure the time it takes to refresh the window, if the control key is down.
    unsigned long long start, finish;
    Boolean measure_duration = control_key_down();
    if (measure_duration)
      start = microseconds();
#endif

	  // Iterate over each rect in the region, sending a paint event for each. Carbon
	  // has a routine for this, pre-carbon doesn't so we roll our own. If the region
	  // is very complicated (more than 15 pieces), just use a bounding box.
    const int kMaxUpdateRects = 15;           // the most rects we'll try to deal with
    const int kRectsBeforeBoundingBox = 10;   // if we have more than this, just do bounding box

    int numRects = 0;

#if TARGET_CARBON
    QDRegionToRects ( updateRgn, kQDParseRegionFromTopLeft, sCountRectProc, &numRects );
    if ( numRects <= kMaxUpdateRects ) {
      Rect rectList[kMaxUpdateRects];
      TRectArray rectWrapper ( rectList );
       
      // compile a list of rectangles 
      QDRegionToRects ( updateRgn, kQDParseRegionFromTopLeft, sAddRectToArrayProc, &rectWrapper );
    
      // amalgamate adjoining rects into a single rect. This 
      // may over-draw a little, but will prevent us from going down into
      // the layout engine for lots of little 1-pixel wide rects.
      if ( numRects > 1 )
        CombineRects ( rectWrapper );
    
      // now paint 'em! (|numRects| may be invalid now, so check count again). If
      // we're above a certain threshold, just bail and do bounding box
      if ( rectWrapper.Count() < kRectsBeforeBoundingBox ) {
        for ( int i = 0; i < rectWrapper.Count(); ++i )
          PaintUpdateRectProc (kQDRegionToRectsMsgParse, updateRgn, &rectList[i], this );
      }
      else {
        Rect boundingBox;
        ::GetRegionBounds(updateRgn, &boundingBox);
        PaintUpdateRectProc ( kQDRegionToRectsMsgParse, updateRgn, &boundingBox, this );
      }
    }
    else {
      Rect boundingBox;
      ::GetRegionBounds(updateRgn, &boundingBox);
      PaintUpdateRect ( &boundingBox, this );
    }
#else
    EachRegionRect ( updateRgn, CountRect, &numRects );
    if ( numRects <= kMaxUpdateRects ) {
      Rect rectList[kMaxUpdateRects];
      TRectArray rectWrapper ( rectList );
       
      // compile a list of rectangles 
      EachRegionRect ( updateRgn, AddRectToArray, &rectWrapper );
    
      // amalgamate adjoining rects into a single rect. This 
      // may over-draw a little, but will prevent us from going down into
      // the layout engine for lots of little 1-pixel wide rects.
      if ( numRects > 1 )
        CombineRects ( rectWrapper );
    
      // now paint 'em! (|numRects| may be invalid now, so check count again). If
      // we're above a certain threshold, just bail and do bounding box
      if ( rectWrapper.Count() < kRectsBeforeBoundingBox ) {
        for ( int i = 0; i < rectWrapper.Count(); ++i )
          PaintUpdateRect ( &rectList[i], this );
      }
      else {
        Rect boundingBox;
        ::GetRegionBounds(updateRgn, &boundingBox);
        PaintUpdateRect ( &boundingBox, this );
      }
    }
    else {
      Rect boundingBox;
      ::GetRegionBounds(updateRgn, &boundingBox);
      PaintUpdateRect ( &boundingBox, this );
    }
#endif

#if DEBUG
    if (measure_duration) {
      finish = microseconds();
      printf("update took %g microseconds.\n", double(finish - start));
    }
#endif

    // Copy updateRgn to regionToValidate
    if (regionToValidate)
      ::CopyRgn(updateRgn, regionToValidate);
	}

#if PINK_PROFILING
	ProfileSuspend();
	ProfileStop();
#endif

	return NS_OK;
}


//
// SortRectsLeftToRight
//
// |inRectArray| is an array of mac |Rect|s, sort them by increasing |left|
// values (so they are sorted left to right). I feel so dirty for using a
// bubble sort, but darn if it isn't simple and we're only going to have 
// about 10 of these rects at maximum (that's a fairly compilicated update
// region).
//
void
nsWindow::SortRectsLeftToRight ( TRectArray & inRectArray )
{
  PRInt32 numRects = inRectArray.Count();
  
  for ( int i = 0; i < numRects - 1; ++i ) {
    for ( int j = i+1; j < numRects; ++j ) {
      if ( inRectArray.mRectList[j].left < inRectArray.mRectList[i].left ) {
        Rect temp = inRectArray.mRectList[i];
        inRectArray.mRectList[i] = inRectArray.mRectList[j];
        inRectArray.mRectList[j] = temp;
      }
    }
  }

} // SortRectsLeftToRight


//
// CombineRects
//
// When breaking up our update region into rects, invariably we end up with lots
// of tall, thin rectangles that are right next to each other (the drop
// shadow of windows are an extreme case). Combine adjacent rectangles if the 
// wasted area (the difference between area of the two rects and the bounding
// box of the two joined rects) is small enough.
//
// As a side effect, the rects will be sorted left->right.
//
void
nsWindow::CombineRects ( TRectArray & rectArray )
{
  const float kCombineThresholdRatio = 0.50;      // 50%
  
  // We assume the rects are sorted left to right below, so sort 'em.
  SortRectsLeftToRight ( rectArray );
  
  // Here's basically what we're doing:
  //
  // compute the area of X and X+1
  // compute area of the bounding rect (topLeft of X and bottomRight of X+1)
  // if ( ratio of combined areas to bounding box is > 50% )
  //   make bottomRight of X be bottomRight of X+1
  //   delete X+1 from array and don't advance X, 
  //    otherwise move along to X+1
  
  int i = 0;
  while ( i < rectArray.Count()-1) {
    Rect* curr = &rectArray.mRectList[i];
    Rect* next = &rectArray.mRectList[i+1];
  
    // compute areas of current and next rects
    int currArea = (curr->right - curr->left) * (curr->bottom - curr->top);
    int nextArea = (next->right - next->left) * (next->bottom - next->top);

    // compute the bounding box and its area
    Rect boundingBox;
    ::UnionRect ( curr, next, &boundingBox );
    int boundingRectArea = (boundingBox.right - boundingBox.left) * 
                              (boundingBox.bottom - boundingBox.top);
    
    // determine if we should combine the rects, based on if the ratio of the
    // combined areas to the bounding rect's area is above some threshold.
    if ( (currArea + nextArea) / (float)boundingRectArea > kCombineThresholdRatio ) {
      // we're combining, so combine the rects, delete the next rect (i+1), remove it from
      // the array, and _don't_ advance to the next rect.
      
      // make the current rectangle the bounding box and shift everything from 
      // i+2 over.
      *curr = boundingBox;
      for ( int j = i+1; j < rectArray.Count()-1; ++j )
        rectArray.mRectList[j] = rectArray.mRectList[j+1];
      --rectArray.mNumRects;
      
    } // if we should combine
    else
      ++i;
  } // foreach rect
  
} // CombineRects


#pragma mark -


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void nsWindow::UpdateWidget(nsRect& aRect, nsIRenderingContext* aContext)
{
	if (! mVisible)
		return;

	// initialize the paint event
	nsPaintEvent paintEvent;
	paintEvent.eventStructType			= NS_PAINT_EVENT;		// nsEvent
	paintEvent.message					= NS_PAINT;
	paintEvent.widget					= this;					// nsGUIEvent
	paintEvent.nativeMsg				= NULL;
	paintEvent.renderingContext			= aContext;				// nsPaintEvent
	paintEvent.rect						= &aRect;

	// draw the widget
	StartDraw(aContext);
	if ( OnPaint(paintEvent) ) {
		nsEventStatus	eventStatus;
		DispatchWindowEvent(paintEvent,eventStatus);
		if(eventStatus != nsEventStatus_eIgnore)
			Flash(paintEvent);
	}
	EndDraw();

	// beard:  Since we clip so aggressively, drawing from front to back should work,
	// and it does for the most part. However; certain cases, such as overlapping
	// areas that are handled by different view managers, don't properly clip siblings.
#ifdef FRONT_TO_BACK
#	define FIRST_CHILD(children) (children->Last())
#	define NEXT_CHILD(children) (children->Prev())
#else
#	define FIRST_CHILD(children) (children->First())
#	define NEXT_CHILD(children) (children->Next())
#endif

	// recursively draw the children
	nsCOMPtr<nsIBidirectionalEnumerator> children(getter_AddRefs((nsIBidirectionalEnumerator*)GetChildren()));
	if (children) {
		FIRST_CHILD(children);
		do {
			nsISupports* child;
			if (NS_SUCCEEDED(children->CurrentItem(&child))) {
				nsWindow* childWindow = static_cast<nsWindow*>(static_cast<nsIWidget*>(child));

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
		} while (NS_SUCCEEDED(NEXT_CHILD(children)));
	}

#undef FIRST_CHILD
#undef NEXT_CHILD
}



//
// ScrollBits
//
// ::ScrollRect() unfortunately paints the invalidated area with the 
// background pattern. This causes lots of ugly flashing and makes us look 
// pretty bad. Instead, we roll our own ::ScrollRect() by using ::CopyBits() 
// to scroll the image in the view and then set the update
// rgn appropriately so that the compositor can blit it to the screen.
//
// This will also work with system floating windows over the area that is
// scrolling.
//
// Under Carbon, this whole routine is basically moot as Apple has answered
// our prayers with ::ScrollWindowRect().
//
void
nsWindow::ScrollBits ( Rect & inRectToScroll, PRInt32 inLeftDelta, PRInt32 inTopDelta )
{                        
#if TARGET_CARBON
  ::ScrollWindowRect ( mWindowPtr, &inRectToScroll, inLeftDelta, inTopDelta, 
                        kScrollWindowInvalidate, NULL );
#else
	// Get Frame in local coords from clip rect (there might be a border around view)
	StRegionFromPool clipRgn;
	if ( !clipRgn ) return;
	::GetClip(clipRgn);
	::SectRgn(clipRgn, mVisRegion, clipRgn);

	Rect frame;
	::GetRegionBounds(clipRgn, &frame);

	StRegionFromPool totalVisRgn;
	if ( !totalVisRgn ) return;
	::RectRgn(totalVisRgn, &frame);
	
		// compute the source and destination of copybits
	Rect source = inRectToScroll;
	SectRect(&source, &frame, &source);

	Rect dest = source;
	::OffsetRect(&dest, inLeftDelta, inTopDelta);

	// compute the area that is to be updated by subtracting the dest from the visible area
	StRegionFromPool destRgn;
	if ( !destRgn ) return;
	::RectRgn(destRgn, &dest);		

	StRegionFromPool updateRgn;
	if ( !updateRgn ) return;
	::RectRgn(updateRgn, &frame);
	::DiffRgn (updateRgn, destRgn, updateRgn);

	if(::EmptyRgn(mWindowPtr->visRgn))		
	{
		::CopyBits ( 
			&mWindowPtr->portBits, 
			&mWindowPtr->portBits, 
			&source, 
			&dest, 
			srcCopy, 
			nil);
	}
	else
	{
		// compute the non-visible region
		StRegionFromPool nonVisibleRgn;
		if ( !nonVisibleRgn ) return;

    ::DiffRgn ( totalVisRgn, mWindowPtr->visRgn, nonVisibleRgn );
		
		// compute the extra area that may need to be updated
		// scoll the non-visible region to determine what needs updating
		::OffsetRgn ( nonVisibleRgn, inLeftDelta, inTopDelta );
		
		// calculate a mask region to not copy the non-visble portions of the window from the port
		StRegionFromPool copyMaskRgn;
		if ( !copyMaskRgn ) return;
		::DiffRgn(totalVisRgn, nonVisibleRgn, copyMaskRgn);
		
		// use copybits to simulate a ScrollRect()
		RGBColor black = { 0, 0, 0 };
		RGBColor white = { 0xFFFF, 0xFFFF, 0xFFFF } ;
		::RGBForeColor(&black);
		::RGBBackColor(&white);
		::PenNormal();	
		::CopyBits ( 
			&mWindowPtr->portBits, 
			&mWindowPtr->portBits, 
			&source, 
			&dest, 
			srcCopy, 
			copyMaskRgn);

		// union the update regions together and invalidate them
		::UnionRgn(nonVisibleRgn, updateRgn, updateRgn);
	}
	
  // If the region to be scrolled contains regions which are currently dirty,
  // we must scroll those too, and union them with the updateRgn.
  // get a copy of the dirty region
  StRegionFromPool  winUpdateRgn;
  if (!winUpdateRgn) return;
  ::BeginUpdate(mWindowPtr);
  ::CopyRgn(mWindowPtr->visRgn, winUpdateRgn);
  ::EndUpdate(mWindowPtr);
  
  StRegionFromPool  dirtyRgn;
  if (!dirtyRgn) return;
  // get only the part of the dirtyRgn that intersects the frame
  ::SectRgn(winUpdateRgn, totalVisRgn, dirtyRgn);
  // offset by the amount scrolled
  ::OffsetRgn(dirtyRgn, inLeftDelta, inTopDelta);
  // now intersect with the frame again
  ::SectRgn(dirtyRgn, totalVisRgn, dirtyRgn);
  // and add it to the dirty region
  ::UnionRgn(updateRgn, dirtyRgn, updateRgn);
  
  // we also need to re-dirty the dirty rgn outside out frame,
  // since BeginUpdate/EndUpdate cleared it.
  ::DiffRgn(winUpdateRgn, totalVisRgn, winUpdateRgn);
  // and add it to the dirty region
  ::UnionRgn(updateRgn, winUpdateRgn, updateRgn);
  
	::InvalWindowRgn(mWindowPtr, updateRgn);
	
#endif // !TARGET_CARBON
}


//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
	if (! mVisible)
		return NS_OK;

  nsRect scrollRect;	

	// If the clipping region is non-rectangular, just force a full update, sorry.
  // XXX ?
	if (!IsRegionRectangular(mWindowRegion)) {
		Invalidate(PR_TRUE);
		goto scrollChildren;
	}

	//--------
	// Scroll this widget
	if (aClipRect)
		scrollRect = *aClipRect;
	else
	{
		scrollRect = mBounds;
		scrollRect.x = scrollRect.y = 0;
	}

	Rect macRect;
	nsRectToMacRect(scrollRect, macRect);


	StartDraw();

		// Clip to the windowRegion instead of the visRegion (note: the visRegion
		// is equal to the windowRegion minus the children). The result is that
		// ScrollRect() scrolls the visible bits of this widget as well as its children.
		::SetClip(mWindowRegion);

		// Scroll the bits now. We've rolled our own because ::ScrollRect looks ugly
		ScrollBits(macRect,aDx,aDy);

	EndDraw();

scrollChildren:
	//--------
	// Scroll the children
	nsCOMPtr<nsIEnumerator> children ( getter_AddRefs(GetChildren()) );
	if (children)
	{
		children->First();
		do
		{
			nsISupports* child;
			if (NS_SUCCEEDED(children->CurrentItem(&child)))
			{
				nsWindow* childWindow = static_cast<nsWindow*>(static_cast<nsIWidget*>(child));
				NS_RELEASE(child);

				nsRect bounds;
				childWindow->GetBounds(bounds);
				bounds.x += aDx;
				bounds.y += aDy;
				childWindow->SetBounds(bounds);
  		}
		} while (NS_SUCCEEDED(children->Next()));			
	}

	// recalculate the window regions
	CalcWindowRegions();

	return NS_OK;
}

//-------------------------------------------------------------------------
//
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
PRBool nsWindow::DispatchWindowEvent(nsGUIEvent &event,nsEventStatus &aStatus)
{
  DispatchEvent(&event, aStatus);
  return ConvertStatus(aStatus);
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
            // printf("Mouse enter");
            //mCurrentWindow = this;
          	//}
        	} 
        else 
        	{
          // printf("Mouse exit");
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
//
//
//-------------------------------------------------------------------------
void nsWindow::CalcWindowRegions()
{
	//------
	// calculate the window region
	if (mWindowRegion == nsnull)
	{
		mWindowRegion = ::NewRgn();
		if (mWindowRegion == nsnull)
			return;
	}
 	::SetRectRgn(mWindowRegion, 0, 0, mBounds.width, mBounds.height);

	// intersect with all the parents
	nsWindow* parent = (nsWindow*)mParent;
	nsPoint origin(-mBounds.x, -mBounds.y);
	while (parent)
	{
		if (parent->mWindowRegion)
		{
			::OffsetRgn(parent->mWindowRegion, origin.x, origin.y);
			::SectRgn(mWindowRegion, parent->mWindowRegion, mWindowRegion);
			::OffsetRgn(parent->mWindowRegion, -origin.x, -origin.y);
		}
		origin.x -= parent->mBounds.x;
		origin.y -= parent->mBounds.y;
		parent = (nsWindow*)parent->mParent;
	}

	//------
	// calculate the visible region
	if (mVisRegion == nsnull)
	{
		mVisRegion = ::NewRgn();
		if (mVisRegion == nsnull)
			return;
	}
	::CopyRgn(mWindowRegion, mVisRegion);

	// clip the children out of the visRegion
	nsCOMPtr<nsIEnumerator> children ( getter_AddRefs(GetChildren()) );
	if (children)
	{
		StRegionFromPool childRgn;
		if (childRgn != nsnull) {
			children->First();
			do
			{
				nsISupports* child;
				if (NS_SUCCEEDED(children->CurrentItem(&child)))
				{
					nsWindow* childWindow = static_cast<nsWindow*>(static_cast<nsIWidget*>(child));
					NS_RELEASE(child);
					
					PRBool visible;
					childWindow->IsVisible(visible);
					if (visible) {
						nsRect childRect;
						childWindow->GetBounds(childRect);

						Rect macRect;
						::SetRect(&macRect, childRect.x, childRect.y, childRect.XMost(), childRect.YMost());
						::RectRgn(childRgn, &macRect);
						::DiffRgn(mVisRegion, childRgn, mVisRegion);
					}
				}
			} while (NS_SUCCEEDED(children->Next()));
		}
	}
}

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

	nsCOMPtr<nsIEnumerator> normalEnum ( getter_AddRefs(GetChildren()) );
	nsCOMPtr<nsIBidirectionalEnumerator> children ( do_QueryInterface(normalEnum) );
	if (children)
	{
		// traverse through all the nsWindows to find out who got hit, lowest level of course
		children->Last();
		do
		{
			nsISupports* child;
			if (NS_SUCCEEDED(children->CurrentItem(&child)))
      {
      	nsWindow* childWindow = static_cast<nsWindow*>(static_cast<nsIWidget*>(child));
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
	}

	return widgetHit;
}

#pragma mark -


//-------------------------------------------------------------------------
// WidgetToScreen
//		Walk up the parent tree, converting the given rect to global coordinates.
//      This is similiar to CalcOffset() but we can't use GetBounds() because it
//      only knows how to give us local coordinates.
//		@param aLocalRect  -- rect in local coordinates of this widget
//		@param aGlobalRect -- |aLocalRect| in global coordinates
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::WidgetToScreen(const nsRect& aLocalRect, nsRect& aGlobalRect)
{	
	aGlobalRect = aLocalRect;
	nsIWidget* theParent = this->GetParent();
	if ( theParent ) {
		// Recursive case
		//
		// Convert the local rect to global, except for this level.
		theParent->WidgetToScreen(aLocalRect, aGlobalRect);
	  NS_RELEASE(theParent);

		// the offset from our parent is in the x/y of our bounding rect
		nsRect myBounds;
		GetBounds(myBounds);
		aGlobalRect.MoveBy(myBounds.x, myBounds.y);
	}
	else {
		// Base case of recursion
		//
		// When there is no parent, we're at the top level window. Use
		// the origin (shifted into global coordinates) to find the offset.
		StPortSetter	portSetter(mWindowPtr);
		::SetOrigin(0,0);
		
		// convert origin into global coords and shift output rect by that ammount
		Point origin = {0, 0};
		::LocalToGlobal ( &origin );
		aGlobalRect.MoveBy ( origin.h, origin.v );
	}
	
	return NS_OK;
}



//-------------------------------------------------------------------------
// ScreenToWidget
//		Walk up the parent tree, converting the given rect to local coordinates.
//		@param aGlobalRect  -- rect in screen coordinates 
//		@param aLocalRect -- |aGlobalRect| in coordinates of this widget
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::ScreenToWidget(const nsRect& aGlobalRect, nsRect& aLocalRect)
{
	aLocalRect = aGlobalRect;
	nsIWidget* theParent = this->GetParent();
	if ( theParent ) {
		// Recursive case
		//
		// Convert the local rect to global, except for this level.
		theParent->WidgetToScreen(aGlobalRect, aLocalRect);
	  NS_RELEASE(theParent);
	  
		// the offset from our parent is in the x/y of our bounding rect
		nsRect myBounds;
		GetBounds(myBounds);
		aLocalRect.MoveBy(myBounds.x, myBounds.y);
	}
	else {
		// Base case of recursion
		//
		// When there is no parent, we're at the top level window. Use
		// the origin (shifted into local coordinates) to find the offset.
		StPortSetter	portSetter(mWindowPtr);
		::SetOrigin(0,0);
		
		// convert origin into local coords and shift output rect by that ammount
		Point origin = {0, 0};
		::GlobalToLocal ( &origin );
		aLocalRect.MoveBy ( origin.h, origin.v );
	}
	
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

NS_IMETHODIMP nsWindow::CaptureRollupEvents(nsIRollupListener * aListener, 
                                            PRBool aDoCapture, 
                                            PRBool aConsumeRollupEvent)
{
  if (aDoCapture) {
    NS_IF_RELEASE(gRollupListener);
    NS_IF_RELEASE(gRollupWidget);
    gRollupListener = aListener;
    NS_ADDREF(aListener);
    gRollupWidget = this;
    NS_ADDREF(this);
  } else {
    NS_IF_RELEASE(gRollupListener);
    //gRollupListener = nsnull;
    NS_IF_RELEASE(gRollupWidget);
  }

  return NS_OK;
}

NS_IMETHODIMP nsWindow::SetTitle(const nsString& title)
{
  NS_ASSERTION(0, "Would some Mac person please implement me? Thanks.");
  return NS_OK;
}

NS_IMETHODIMP nsWindow::GetAttention()
{
	// Since the Mac doesn't consider each window a seperate process this call functions
	// slightly different than on other platforms.  We first check to see if we're the
	// foreground process and, if so, ignore the call.  We also check to see if a notification
	// is already pending and, if so, remove it since we only have one notification per process.
	// After all that checking we install a notification manager request to mark the app's icon
	// in the process menu and play the default alert sound
  
  OSErr err;
    
	if (we_are_front_process())
		return NS_OK;
  
	if (gNotificationInstalled)
	{
		(void)::NMRemove(&gNMRec);
		gNotificationInstalled = false;
	}
	
	err = GetIconSuite( &gNMRec.nmIcon, 128, svAllSmallData );
	if ( err != noErr )
		gNMRec.nmIcon = NULL;
		
	// Setup and install the notification manager rec
	gNMRec.qType		= nmType;
	gNMRec.nmMark		= 1;			// Flag the icon in the process menu
	gNMRec.nmSound		= (Handle)-1L;	// Use the default alert sound
	gNMRec.nmStr		= NULL;			// No alert/window so no text
	gNMRec.nmResp		= NULL;			// No response proc, use the default behavior
	gNMRec.nmRefCon	= NULL;
	if (::NMInstall(&gNMRec) == noErr)
		gNotificationInstalled = true;

	return NS_OK;
}

#pragma mark -


NS_IMETHODIMP nsWindow::ResetInputState()
{
	// currently, the nsMacEventHandler is owned by nsMacWindow, which is the top level window
	// we delegate this call to its parent
  nsCOMPtr<nsIWidget> parent = getter_AddRefs(GetParent());
  NS_ASSERTION(parent, "cannot get parent");
  if(parent)
  {
  	nsCOMPtr<nsIKBStateControl> kb = do_QueryInterface(parent);
 	  NS_ASSERTION(kb, "cannot get parent");
  	if(kb) {
  		return kb->ResetInputState();
  	}
  }
	return NS_ERROR_ABORT;
}


#if !TARGET_CARBON

void EachRegionRect (RgnHandle r, void (* proc)(Rect *, void *), void* data) ;

//
// Written by Hugh Fisher, March 1993.
// Used w/out asking his permission.
//
// This can go away when we get an api on nsIRegion to create one from
// a RgnHandle. Then we can use nsIRegion::GetRects to do the work for us.
//
void 
EachRegionRect (RgnHandle r, void (* proc)(Rect *, void *), void* inData)
{
#define EndMark 	32767
#define MaxY		32767
#define StackMax	1024

	typedef struct {
		short	size;
		Rect	bbox;
		short	data[];
		} ** Internal;
	
	Internal region;
	short	 width, xAdjust, y, index, x1, x2, x;
	Rect	 box;
	short	 stackStorage[1024];
	short *	 buffer;
	
	region = (Internal)r;
	
	/* Check for plain rectangle */
	if ((**region).size == 10) {
		proc(&(**region).bbox, inData);
		return;
	}
	/* Got to scale x coordinates into range 0..something */
	xAdjust = (**region).bbox.left;
	width = (**region).bbox.right - xAdjust;
	/* Most regions will be less than 1024 pixels wide */
	if (width < StackMax)
		buffer = stackStorage;
	else {
		buffer = (short *)PR_Malloc(width * 2);
		if (buffer == NULL)
			/* Truly humungous region or very low on memory.
			   Quietly doing nothing seems to be the
			   traditional Quickdraw response. */
			return;
	}
	/* Initialise scan line list to bottom edges */
	for (x = (**region).bbox.left; x < (**region).bbox.right; x++)
		buffer[x - xAdjust] = MaxY;
	index = 0;
	/* Loop until we hit an empty scan line */
	while ((**region).data[index] != EndMark) {
		y = (**region).data[index];
		index ++;
		/* Loop through horizontal runs on this line */
		while ((**region).data[index] != EndMark) {
			x1 = (**region).data[index];
			index ++;
			x2 = (**region).data[index];
			index ++;
			x = x1;
			while (x < x2) {
				if (buffer[x - xAdjust] < y) {
					/* We have a bottom edge - how long for? */
					box.left = x;
					box.top  = buffer[x - xAdjust];
					while (x < x2 && buffer[x - xAdjust] == box.top) {
						buffer[x - xAdjust] = MaxY;
						x ++;
					}
					/* Pass to client proc */
					box.right  = x;
					box.bottom = y;
					proc(&box, inData);
				} else {
					/* This becomes a top edge */
					buffer[x - xAdjust] = y;
					x ++;
				}
			}
		}
		index ++;
	}
	/* Clean up after ourselves */
	if (width >= StackMax)
		PR_Free((void *)buffer);
#undef EndMark
#undef MaxY
#undef StackMax
}

#endif
