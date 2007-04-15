/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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
#include "nsIRegion.h"
#include "nsIRollupListener.h"
#include "nsPIWidgetMac.h"

#include "nsCarbonHelpers.h"
#include "nsGfxUtils.h"
#include "nsRegionPool.h"

#include <Gestalt.h>

#ifdef MAC_OS_X_VERSION_10_3
const short PANTHER_RESIZE_UP_CURSOR     = 19;
const short PANTHER_RESIZE_DOWN_CURSOR   = 20;
const short PANTHER_RESIZE_UPDOWN_CURSOR = 21;
#else
//19, 20, 21 from Appearance.h in 10.3
const short PANTHER_RESIZE_UP_CURSOR     = 19; // kThemeResizeUpCursor
const short PANTHER_RESIZE_DOWN_CURSOR   = 20; // kThemeResizeDownCursor
const short PANTHER_RESIZE_UPDOWN_CURSOR = 21; // kThemeResizeUpDownCursor
#endif

const short JAGUAR_RESIZE_UP_CURSOR      = 135;
const short JAGUAR_RESIZE_DOWN_CURSOR    = 136;
const short JAGUAR_RESIZE_UPDOWN_CURSOR  = 141;

////////////////////////////////////////////////////
nsIRollupListener * gRollupListener = nsnull;
nsIWidget         * gRollupWidget   = nsnull;

// Since we only want a single notification pending for the app we'll declare
// these static
static NMRec	gNMRec;
static Boolean	gNotificationInstalled = false;

//used to animate the Arrow + Beachball cursor
static CursorSpinner *gCursorSpinner = nsnull;
static const int kSpinCursorFirstFrame = 200;

// Iterates over the rects of a region.
static RegionToRectsUPP sAddRectToArrayProc = nsnull;

#pragma mark -


//#define PAINT_DEBUGGING         // flash areas as they are painted
//#define INVALIDATE_DEBUGGING    // flash areas as they are invalidated

#if defined(INVALIDATE_DEBUGGING) || defined(PAINT_DEBUGGING)
static void blinkRect(const Rect* r, PRBool isPaint);
static void blinkRgn(RgnHandle rgn, PRBool isPaint);
#endif

#if defined(INVALIDATE_DEBUGGING) || defined(PAINT_DEBUGGING) || defined (PINK_PROFILING)
static Boolean KeyDown(const UInt8 theKey)
{
	KeyMap map;
	GetKeys(map);
	return ((*((UInt8 *)map + (theKey >> 3)) >> (theKey & 7)) & 1) != 0;
}
#endif

#if defined(INVALIDATE_DEBUGGING) || defined(PAINT_DEBUGGING)

static Boolean caps_lock()
{
  return KeyDown(0x39);
}


static void FlushCurPortBuffer()
{
    CGrafPtr    curPort;
    ::GetPort((GrafPtr*)&curPort);
    ::QDFlushPortBuffer(curPort, nil);      // OK to call on 9/carbon (does nothing)
}

static void blinkRect(const Rect* r, PRBool isPaint)
{
	StRegionFromPool oldClip;
	UInt32 endTicks;

    if (oldClip != NULL)
    ::GetClip(oldClip);

    ::ClipRect(r);

    if (isPaint)
    {
        Pattern grayPattern;

        ::GetQDGlobalsGray(&grayPattern);

        ::ForeColor(blackColor);
        ::BackColor(whiteColor);

        ::PenMode(patXor);
        ::PenPat(&grayPattern);
        ::PaintRect(r);
        FlushCurPortBuffer();
        ::Delay(5, &endTicks);
        ::PaintRect(r);
        FlushCurPortBuffer();
        ::PenNormal();
    }
    else
    {
        ::InvertRect(r);
        FlushCurPortBuffer();
        ::Delay(5, &endTicks);
        ::InvertRect(r);
        FlushCurPortBuffer();
    }

	if (oldClip != NULL)
		::SetClip(oldClip);
}

static void blinkRgn(RgnHandle rgn, PRBool isPaint)
{
    StRegionFromPool oldClip;
    UInt32 endTicks;

    if (oldClip != NULL)
        ::GetClip(oldClip);

    ::SetClip(rgn);

    if (isPaint)
    {
        Pattern grayPattern;

        ::GetQDGlobalsGray(&grayPattern);

        ::ForeColor(blackColor);
        ::BackColor(whiteColor);

        ::PenMode(patXor);
        ::PenPat(&grayPattern);
        ::PaintRgn(rgn);
        FlushCurPortBuffer();

        ::Delay(5, &endTicks);
        ::PaintRgn(rgn);
        FlushCurPortBuffer();
        ::PenNormal();
    }
    else
    {
        ::InvertRgn(rgn);
        FlushCurPortBuffer();
        ::Delay(5, &endTicks);
        ::InvertRgn(rgn);
        FlushCurPortBuffer();
    }

    if (oldClip != NULL)
        ::SetClip(oldClip);
}

#endif

struct TRectArray
{
  TRectArray(Rect* aRectList, PRUint32 aCapacity)
  : mRectList(aRectList)
  , mNumRects(0)
  , mCapacity(aCapacity) { }

  Rect*    mRectList;
  PRUint32 mNumRects;
  PRUint32 mCapacity;
};

#pragma mark -

//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow() : nsBaseWidget() , nsDeleteObserved(this), nsIKBStateControl()
{
	WIDGET_SET_CLASSNAME("nsWindow");

  mParent = nsnull;
  mIsTopWidgetWindow = PR_FALSE;
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
  mInUpdate = PR_FALSE;
  mDestructorCalled = PR_FALSE;

  SetBackgroundColor(NS_RGB(255, 255, 255));
  SetForegroundColor(NS_RGB(0, 0, 0));

  mPluginPort = nsnull;

  AcceptFocusOnClick(PR_TRUE);
  
  if (!sAddRectToArrayProc)
    sAddRectToArrayProc = ::NewRegionToRectsUPP(AddRectToArrayProc);
}


//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
	// notify the children that we're gone
	for (nsIWidget* kid = mFirstChild; kid; kid = kid->GetNextSibling()) {
		nsWindow* childWindow = NS_STATIC_CAST(nsWindow*, kid);
		childWindow->mParent = nsnull;
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

NS_IMPL_ISUPPORTS_INHERITED2(nsWindow, nsBaseWidget, nsIKBStateControl, nsIPluginWidget)

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
	if (mOnDestroyCalled)
		return NS_OK;
	mOnDestroyCalled = PR_TRUE;

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
  if (mIsTopWidgetWindow) return nsnull;
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
    	// TODO
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
  *aForWindow = PR_TRUE;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Enable/disable this component
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Enable(PRBool aState)
{
	mEnabled = aState;
	return NS_OK;
}

    
NS_IMETHODIMP nsWindow::IsEnabled(PRBool *aState)
{
	NS_ENSURE_ARG_POINTER(aState);
	*aState = mEnabled;
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
  nsCOMPtr<nsIWidget> top;
  nsToolkit::GetTopWidget(mWindowPtr, getter_AddRefs(top));

  if (top) {
    nsCOMPtr<nsPIWidgetMac> topMac = do_QueryInterface(top);

    if (topMac) {
      nsMacEventDispatchHandler* eventDispatchHandler = nsnull;
      topMac->GetEventDispatchHandler(&eventDispatchHandler);

      if (eventDispatchHandler)
        eventDispatchHandler->SetFocus(this);
    }
  }
	
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
	// TODO
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


PRBool OnPantherOrLater() // Return true if we are on Mac OS X 10.3 or later
{
    static PRBool gInitVer1030 = PR_FALSE;
    static PRBool gOnPantherOrLater = PR_FALSE;
    if(!gInitVer1030)
    {
        gOnPantherOrLater =
            (nsToolkit::OSXVersion() >= MAC_OS_X_VERSION_10_3_HEX);
        gInitVer1030 = PR_TRUE;
    }
    return gOnPantherOrLater;
}

//
// SetCursor
//
// Override to set the cursor on the mac
//
NS_METHOD nsWindow::SetCursor(nsCursor aCursor)
{
  nsBaseWidget::SetCursor(aCursor);
	
  if ( gCursorSpinner == nsnull )
  {
      gCursorSpinner = new CursorSpinner();
  }
  
  // mac specific cursor manipulation
  short cursor = -1;
  switch (aCursor)
  {
    case eCursor_standard:            cursor = kThemeArrowCursor; break;
    case eCursor_wait:                cursor = kThemeWatchCursor; break;
    case eCursor_select:              cursor = kThemeIBeamCursor; break;
    case eCursor_hyperlink:           cursor = kThemePointingHandCursor; break;
    case eCursor_crosshair:           cursor = kThemeCrossCursor; break;
    case eCursor_move:                cursor = kThemeOpenHandCursor; break;
    case eCursor_help:                cursor = 128; break;
    case eCursor_copy:                cursor = kThemeCopyArrowCursor; break;
    case eCursor_alias:               cursor = kThemeAliasArrowCursor; break;
    case eCursor_context_menu:        cursor = kThemeContextualMenuArrowCursor; break;
    case eCursor_cell:                cursor = kThemePlusCursor; break;
    case eCursor_grab:                cursor = kThemeOpenHandCursor; break;
    case eCursor_grabbing:            cursor = kThemeClosedHandCursor; break;
    case eCursor_spinning:            cursor = kSpinCursorFirstFrame; break; // better than kThemeSpinningCursor
    case eCursor_zoom_in:             cursor = 129; break;
    case eCursor_zoom_out:            cursor = 130; break;
    case eCursor_not_allowed:
    case eCursor_no_drop:             cursor = kThemeNotAllowedCursor; break;  
    case eCursor_col_resize:          cursor = 132; break; 
    case eCursor_row_resize:          cursor = 133; break;
    case eCursor_vertical_text:       cursor = 134; break;   
    case eCursor_all_scroll:          cursor = kThemeOpenHandCursor; break;
    case eCursor_n_resize:            cursor = OnPantherOrLater() ? PANTHER_RESIZE_UP_CURSOR : JAGUAR_RESIZE_UP_CURSOR; break;
    case eCursor_s_resize:            cursor = OnPantherOrLater() ? PANTHER_RESIZE_DOWN_CURSOR : JAGUAR_RESIZE_DOWN_CURSOR; break;
    case eCursor_w_resize:            cursor = kThemeResizeLeftCursor; break; 
    case eCursor_e_resize:            cursor = kThemeResizeRightCursor; break;
    case eCursor_nw_resize:           cursor = 137; break;
    case eCursor_se_resize:           cursor = 138; break;
    case eCursor_ne_resize:           cursor = 139; break;
    case eCursor_sw_resize:           cursor = 140; break;
    case eCursor_ew_resize:           cursor = kThemeResizeLeftRightCursor; break;
    case eCursor_ns_resize:           cursor = OnPantherOrLater() ? PANTHER_RESIZE_UPDOWN_CURSOR : JAGUAR_RESIZE_UPDOWN_CURSOR; break;
    case eCursor_nesw_resize:         cursor = 142; break;
    case eCursor_nwse_resize:         cursor = 143; break;        
    default:                          
      cursor = kThemeArrowCursor; 
      break;
  }


  if (aCursor == eCursor_spinning)
  {
    gCursorSpinner->StartSpinCursor();
  }
  else
  {
    gCursorSpinner->StopSpinCursor();
    nsWindow::SetCursorResource(cursor);
  }

  return NS_OK;
  
} // nsWindow::SetCursor

void nsWindow::SetCursorResource(short aCursorResourceNum)
{
    if (aCursorResourceNum >= 0)
    {
        if (aCursorResourceNum >= 128)
        {
            nsMacResources::OpenLocalResourceFile();
            CursHandle cursHandle = ::GetCursor(aCursorResourceNum);
            NS_ASSERTION (cursHandle, "Can't load cursor, is the resource file installed correctly?");
            if (cursHandle)
            {
                ::SetCursor(*cursHandle);
            }
            nsMacResources::CloseLocalResourceFile();            
        }
        else
        {
            ::SetThemeCursor(aCursorResourceNum);
        }
    }    
}

CursorSpinner::CursorSpinner() :
    mSpinCursorFrame(0), mTimerUPP(nsnull), mTimerRef(nsnull)
{
   mTimerUPP = NewEventLoopTimerUPP(SpinCursor);
}

CursorSpinner::~CursorSpinner()
{
    if (mTimerRef) ::RemoveEventLoopTimer(mTimerRef);
    if (mTimerUPP) ::DisposeEventLoopTimerUPP(mTimerUPP);
}

short CursorSpinner::GetNextCursorFrame()
{
    int result = kSpinCursorFirstFrame + mSpinCursorFrame;
    mSpinCursorFrame = (mSpinCursorFrame + 1) % 4;
    return (short) result;
}

void CursorSpinner::StartSpinCursor()
{
    OSStatus result = noErr;
    if (mTimerRef == nsnull)
    {
        result = ::InstallEventLoopTimer(::GetMainEventLoop(), 0, 0.25 * kEventDurationSecond,
                                         mTimerUPP, this, &mTimerRef);
        if (result != noErr)
        {
            mTimerRef = nsnull;
            nsWindow::SetCursorResource(kSpinCursorFirstFrame);
        }
    }
}

void CursorSpinner::StopSpinCursor()
{
    if (mTimerRef)
    {
        ::RemoveEventLoopTimer(mTimerRef);
        mTimerRef = nsnull;
    }
}

pascal void CursorSpinner::SpinCursor(EventLoopTimerRef inTimer, void *inUserData)
{
    CursorSpinner* cs = reinterpret_cast<CursorSpinner*>(inUserData);
    nsWindow::SetCursorResource(cs->GetNextCursorFrame());
}

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


NS_IMETHODIMP nsWindow::ConstrainPosition(PRBool aAllowSlop,
                                          PRInt32 *aX, PRInt32 *aY)
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
		if ((mParent != nsnull) && (!mIsTopWidgetWindow))
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


//-------------------------------------------------------------------------
//
// Validate this component's visible area
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Validate()
{
	if (!mWindowPtr || !mVisible || !ContainerHierarchyIsVisible())
		return NS_OK;

	nsRect wRect = mBounds;
	LocalToWindowCoordinate(wRect);
	Rect macRect;
	nsRectToMacRect(wRect, macRect);

	StPortSetter    portSetter(mWindowPtr);
	StOriginSetter  originSetter(mWindowPtr);

	::ValidWindowRect(mWindowPtr, &macRect);

	return NS_OK;
}

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

  if (!mVisible || !ContainerHierarchyIsVisible())
    return NS_OK;

	nsRect wRect = aRect;
	wRect.MoveBy(mBounds.x, mBounds.y);				// beard:  this is required, see GetNativeData(NS_NATIVE_OFFSETX).
	LocalToWindowCoordinate(wRect);
	Rect macRect;
	nsRectToMacRect(wRect, macRect);

	StPortSetter portSetter(mWindowPtr);
	StOriginSetter  originSetter(mWindowPtr);

#ifdef INVALIDATE_DEBUGGING
	if (caps_lock())
		::blinkRect(&macRect, PR_FALSE);
#endif

	::InvalWindowRect(mWindowPtr, &macRect);
	if ( aIsSynchronous )
	  Update();

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

  if (!mVisible || !ContainerHierarchyIsVisible())
    return NS_OK;
    
	// copy invalid region into a working region.
	void* nativeRgn;
	aRegion->GetNativeRegion(nativeRgn);
	StRegionFromPool windowRgn;
	::CopyRgn(RgnHandle(nativeRgn), windowRgn);

	// translate this region into window coordinates.
	PRInt32	offX, offY;
	CalcOffset(offX, offY);
	::OffsetRgn(windowRgn, mBounds.x + offX, mBounds.y + offY);
	
	StPortSetter    portSetter(mWindowPtr);
    StOriginSetter  originSetter(mWindowPtr);

#ifdef INVALIDATE_DEBUGGING
	if (caps_lock())
		::blinkRgn(windowRgn, PR_FALSE);
#endif

	::InvalWindowRgn(mWindowPtr, windowRgn);
	if ( aIsSynchronous )
	  Update();

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
	if (mDrawing || mOnDestroyCalled)
		return;
	mDrawing = PR_TRUE;

	CalcWindowRegions();	// REVISIT

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
	mTempRenderingContext->PushState();           // push the state so we can pop it later
}


//-------------------------------------------------------------------------
//	EndDraw
//
//-------------------------------------------------------------------------
void nsWindow::EndDraw()
{
	if (! mDrawing || mOnDestroyCalled)
		return;
	mDrawing = PR_FALSE;

	mTempRenderingContext->PopState();

	NS_RELEASE(mTempRenderingContext);
	
	// note that we may leave the window origin in an unspecified state here
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
  if (! mVisible || !mWindowPtr || !ContainerHierarchyIsVisible())
    return NS_OK;

  static PRBool  reentrant = PR_FALSE;

  if (reentrant)
    HandleUpdateEvent(nil);
  else
  {
    reentrant = PR_TRUE;
    
    StRegionFromPool redrawnRegion;
    if (!redrawnRegion)
      return NS_ERROR_OUT_OF_MEMORY;

    // make a copy of the window update rgn (which is in global coords)
    StRegionFromPool saveUpdateRgn;
    if (!saveUpdateRgn)
      return NS_ERROR_OUT_OF_MEMORY;
    ::GetWindowRegion(mWindowPtr, kWindowUpdateRgn, saveUpdateRgn);

    // Sometimes, the window update region will be larger than the window
    // itself.  Because we obviously don't redraw anything outside of the
    // window, redrawnRegion won't include that larger area, and the
    // larger area will be re-invalidated.  That triggers an endless
    // sequence of kEventWindowUpdate events.  Avoid that condition by
    // restricting the update region to the content region.
    StRegionFromPool windowContentRgn;
    if (!windowContentRgn)
      return NS_ERROR_OUT_OF_MEMORY;

    ::GetWindowRegion(mWindowPtr, kWindowContentRgn, windowContentRgn);

    ::SectRgn(saveUpdateRgn, windowContentRgn, saveUpdateRgn);

    // draw the widget
    StPortSetter portSetter(mWindowPtr);
    StOriginSetter originSetter(mWindowPtr);

    // BeginUpate replaces the visRgn with the intersection of the
    // visRgn and the updateRgn.
    ::BeginUpdate(mWindowPtr);
    mInUpdate = PR_TRUE;

    HandleUpdateEvent(redrawnRegion);

    // EndUpdate replaces the normal visRgn
    ::EndUpdate(mWindowPtr);
    mInUpdate = PR_FALSE;
    
    // Restore the parts of the window update rgn that we didn't draw,
    // taking care to maintain any bits that got invalidated during the
    // paint (yes, this can happen). We do this because gecko might not own
    // the entire window (think: embedding).

    Point origin = {0, 0};
    ::GlobalToLocal(&origin);
    // saveUpdateRgn is in global coords, so we need to shift it to local coords
    ::OffsetRgn(saveUpdateRgn, origin.h, origin.v);
    
    // figure out the difference between the old update region
    // and what we redrew
    ::DiffRgn(saveUpdateRgn, redrawnRegion, saveUpdateRgn);

    // and invalidate it
    ::InvalWindowRgn(mWindowPtr, saveUpdateRgn);

    reentrant = PR_FALSE;
  }

  return NS_OK;
}


#pragma mark -




//
// AddRectToArrayProc
//
// Add each rect to an array so we can post-process them. |inArray| is a
// pointer to the TRectArray we're adding to.
//
OSStatus
nsWindow::AddRectToArrayProc(UInt16 message, RgnHandle rgn,
                             const Rect* inDirtyRect, void* inArray)
{
  if (message == kQDRegionToRectsMsgParse) {
    NS_ASSERTION(inArray, "You better pass an array!");
    TRectArray* rectArray = NS_REINTERPRET_CAST(TRectArray*, inArray);

    if (rectArray->mNumRects == rectArray->mCapacity) {
      // This should not happen - returning memFullErr below should have
      // prevented it.
      return memFullErr;
    }

    rectArray->mRectList[rectArray->mNumRects++] = *inDirtyRect;

    if (rectArray->mNumRects == rectArray->mCapacity) {
      // Signal that the array is full and QDRegionToRects should stop.
      // After returning memFullErr, this proc should not be called with
      // the same |message| again.
      return memFullErr;
    }
  }

  return noErr;
}


//
// PaintUpdateRect
//
// Called once for every rect in the update region. Send a paint event to Gecko to handle
// this. |inData| contains the |nsWindow| being updated. 
//
void 
nsWindow::PaintUpdateRect(Rect *inDirtyRect, void* inData)
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


//-------------------------------------------------------------------------
//	HandleUpdateEvent
//
//		Called by the event handler to redraw the top-level widget.
//		Must be called between BeginUpdate/EndUpdate: the window visRgn
//		is expected to be set to whatever needs to be drawn.
//-------------------------------------------------------------------------
nsresult nsWindow::HandleUpdateEvent(RgnHandle regionToValidate)
{
  if (! mVisible || !ContainerHierarchyIsVisible())
    return NS_OK;

  // make sure the port is set and origin is (0, 0).
  StPortSetter    portSetter(mWindowPtr);
  // zero the origin, and set it back after drawing widgets
  StOriginSetter  originSetter(mWindowPtr);
  
  // get the damaged region from the OS
  StRegionFromPool damagedRgn;
  if (!damagedRgn)
    return NS_ERROR_OUT_OF_MEMORY;
  ::GetPortVisibleRegion(::GetWindowPort(mWindowPtr), damagedRgn);

/*
#ifdef PAINT_DEBUGGING  
  if (caps_lock())
    blinkRgn(damagedRgn, PR_TRUE);
#endif
*/
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

  // check if the update region is visible
  ::SectRgn(damagedRgn, updateRgn, updateRgn);

#ifdef PAINT_DEBUGGING
  if (caps_lock())
    blinkRgn(updateRgn, PR_TRUE);
#endif

  if (!::EmptyRgn(updateRgn)) {
    // Iterate over each rect in the region, sending a paint event for each.
    // If the region is very complicated (more than 15 pieces), just use a
    // bounding box.

    // The most rects we'll try to deal with:
    const PRUint32 kMaxUpdateRects = 15;
    
    // If, after combining rects, there are still more than this, just do
    // a bounding box:
    const PRUint32 kRectsBeforeBoundingBox = 10;

    // Leave one extra slot in rectList to give AddRectToArrayProc a way to
    // signal that kMaxUpdateRects has been exceeded.
    const PRUint32 kRectCapacity = kMaxUpdateRects + 1;

    Rect rectList[kRectCapacity];
    TRectArray rectWrapper(rectList, kRectCapacity);
    ::QDRegionToRects(updateRgn, kQDParseRegionFromTopLeft,
                      sAddRectToArrayProc, &rectWrapper);
    PRUint32 numRects = rectWrapper.mNumRects;

    PRBool painted = PR_FALSE;

    if (numRects <= kMaxUpdateRects) {
      if (numRects > 1) {
        // amalgamate adjoining rects into a single rect. This 
        // may over-draw a little, but will prevent us from going down into
        // the layout engine for lots of little 1-pixel wide rects.
        CombineRects(rectWrapper);

        // |numRects| may be invalid now, so check count again.
        numRects = rectWrapper.mNumRects;
      }

      // Now paint 'em!
      // If we're above a certain threshold, just bail and do bounding box.
      if (numRects < kRectsBeforeBoundingBox) {
        painted = PR_TRUE;
        for (PRUint32 i = 0 ; i < numRects ; i++)
          PaintUpdateRect(&rectList[i], this);
      }
    }

    if (!painted) {
      // There were too many rects.  Do a bounding box.
      Rect boundingBox;
      ::GetRegionBounds(updateRgn, &boundingBox);
      PaintUpdateRect(&boundingBox, this);
    }

    // Copy updateRgn to regionToValidate
    if (regionToValidate)
      ::CopyRgn(updateRgn, regionToValidate);
  }

  NS_ASSERTION(ValidateDrawingState(), "Bad drawing state");

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
  PRInt32 numRects = inRectArray.mNumRects;
  
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
  
  PRUint32 i = 0;
  while (i < rectArray.mNumRects - 1) {
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
      for (PRUint32 j = i + 1 ; j < rectArray.mNumRects - 1 ; ++j)
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
	if (! mVisible || !ContainerHierarchyIsVisible())
		return;

	// initialize the paint event
	nsPaintEvent paintEvent(PR_TRUE, NS_PAINT, this);
	paintEvent.renderingContext = aContext;         // nsPaintEvent
	paintEvent.rect             = &aRect;

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
#	define FIRST_CHILD() (mLastChild)
#	define NEXT_CHILD(child) ((child)->GetPrevSibling())
#else
#	define FIRST_CHILD() (mFirstChild)
#	define NEXT_CHILD(child) ((child)->GetNextSibling())
#endif

	// recursively draw the children
	for (nsIWidget* kid = FIRST_CHILD(); kid; kid = NEXT_CHILD(kid)) {
		nsWindow* childWindow = NS_STATIC_CAST(nsWindow*, kid);

		nsRect childBounds;
		childWindow->GetBounds(childBounds);

		// redraw only the intersection of the child rect and the update rect
		nsRect intersection;
		if (intersection.IntersectRect(aRect, childBounds))
		{
			intersection.MoveBy(-childBounds.x, -childBounds.y);
			childWindow->UpdateWidget(intersection, aContext);
		}
	}

#undef FIRST_CHILD
#undef NEXT_CHILD

	NS_ASSERTION(ValidateDrawingState(), "Bad drawing state");
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
  ::ScrollWindowRect ( mWindowPtr, &inRectToScroll, inLeftDelta, inTopDelta, 
                        kScrollWindowInvalidate, NULL );
}


//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  if (mVisible && ContainerHierarchyIsVisible())
  {
    // If the clipping region is non-rectangular, just force a full update.
    // Happens when scrolling pages with absolutely-positioned divs
    // (see bug 289353 for examples).
    if (!IsRegionRectangular(mWindowRegion))
    {
      Invalidate(PR_FALSE);
      goto scrollChildren;
    }

    //--------
    // Scroll this widget
    nsRect scrollRect;  
    if (aClipRect)
      scrollRect = *aClipRect;
    else
    {
      scrollRect = mBounds;
      scrollRect.x = scrollRect.y = 0;
    }

    // If we're scrolling by an amount that is larger than the height or
    // width, just invalidate the entire area.
    if (aDx >= scrollRect.width || aDy >= scrollRect.height)
    {
      Invalidate(scrollRect, PR_FALSE);
    }
    else
    {
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
    }
  }

scrollChildren:
  //--------
  // Scroll the children
  for (nsIWidget* kid = mFirstChild; kid; kid = kid->GetNextSibling()) {
    nsWindow* childWindow = NS_STATIC_CAST(nsWindow*, kid);

    nsRect bounds;
    childWindow->GetBounds(bounds);
    bounds.x += aDx;
    bounds.y += aDy;
    childWindow->SetBounds(bounds);
  }

  // recalculate the window regions
  CalcWindowRegions();

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::DispatchEvent(nsGUIEvent* event, nsEventStatus& aStatus)
{
  aStatus = nsEventStatus_eIgnore;
	if (mEnabled && !mDestructorCalled)
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
  if (mEventCallback && (mEnabled || aEvent.message == NS_MOUSE_EXIT)) {
    result = (DispatchWindowEvent(aEvent));
    return result;
  }

  if (nsnull != mMouseListener) {
    switch (aEvent.message) {
      case NS_MOUSE_MOVE: {
        result = ConvertStatus(mMouseListener->MouseMoved(aEvent));
        nsRect rect;
        GetBounds(rect);
        if (rect.Contains(aEvent.refPoint.x, aEvent.refPoint.y)) 
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

      case NS_MOUSE_BUTTON_DOWN:
        result = ConvertStatus(mMouseListener->MousePressed(aEvent));
        break;

      case NS_MOUSE_BUTTON_UP:
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
	nsGUIEvent moveEvent(PR_TRUE, NS_DESTROY, this);
	moveEvent.message			= NS_DESTROY;
	moveEvent.time				= PR_IntervalNow();

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
	nsGUIEvent moveEvent(PR_TRUE, NS_MOVE, this);
	moveEvent.refPoint.x	= mBounds.x;
	moveEvent.refPoint.y	= mBounds.y;
	moveEvent.time				= PR_IntervalNow();

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
	nsSizeEvent sizeEvent(PR_TRUE, NS_SIZE, this);
	sizeEvent.time				= PR_IntervalNow();

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

	// must stop enumerating when hitting a native window boundary
	if (!mIsTopWidgetWindow)
	{
		while (parent && (!parent->mIsTopWidgetWindow))
	{
    if (parent->mWindowRegion)
    {
      // Under 10.2, if we offset a region beyond the coordinate space,
      // OffsetRgn() will silently fail and restoring it will then cause the
      // widget to be out of place (visible as 'shearing' when scrolling).
      // To prevent that, we copy the original region and work on that. (bug 162885)
      StRegionFromPool shiftedParentWindowRgn;
      if ( !shiftedParentWindowRgn )
        return;
      ::CopyRgn(parent->mWindowRegion, shiftedParentWindowRgn); 
      ::OffsetRgn(shiftedParentWindowRgn, origin.x, origin.y);
      ::SectRgn(mWindowRegion, shiftedParentWindowRgn, mWindowRegion);
    }
		origin.x -= parent->mBounds.x;
		origin.y -= parent->mBounds.y;
		parent = (nsWindow*)parent->mParent;
		}
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
	if (mFirstChild)
	{
		StRegionFromPool childRgn;
		if (childRgn != nsnull) {
			nsIWidget* child = mFirstChild;
			do
			{
				nsWindow* childWindow = NS_STATIC_CAST(nsWindow*, child);
					
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
				
				child = child->GetNextSibling();
			} while (child);
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
 
void
nsWindow::CalcOffset(PRInt32 &aX, PRInt32 &aY)
{
  aX = aY = 0;

  nsIWidget* theParent = GetParent();
  while (theParent)
  {
    nsRect theRect;
    theParent->GetBounds(theRect);
    aX += theRect.x;
    aY += theRect.y;

    theParent = theParent->GetParent();
  }
}


PRBool
nsWindow::ContainerHierarchyIsVisible()
{
  nsIWidget* theParent = GetParent();
  
  while (theParent)
  {
    PRBool  visible;
    theParent->IsVisible(visible);
    if (!visible)
      return PR_FALSE;
    
    theParent = theParent->GetParent();
  }
  
  return PR_TRUE;
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
	if (!mVisible || !ContainerHierarchyIsVisible() || !PointInWidget(aThePoint))
		return nsnull;

	nsWindow* widgetHit = this;

	// traverse through all the nsWindows to find out who got hit, lowest level of course
	for (nsIWidget* kid = mLastChild; kid; kid = kid->GetPrevSibling()) {
		nsWindow* childWindow = NS_STATIC_CAST(nsWindow*, kid);
		
		nsWindow* deeperHit = childWindow->FindWidgetHit(aThePoint);
		if (deeperHit)
		{
			widgetHit = deeperHit;
			break;
		}
	}

	return widgetHit;
}

#pragma mark -


//-------------------------------------------------------------------------
// WidgetToScreen
//		Walk up the parent tree, converting the given rect to global coordinates.
//      This is similar to CalcOffset() but we can't use GetBounds() because it
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
		StOriginSetter	originSetter(mWindowPtr);
		
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
	nsIWidget* theParent = GetParent();
	if ( theParent ) {
		// Recursive case
		//
		// Convert the local rect to global, except for this level.
		theParent->WidgetToScreen(aGlobalRect, aLocalRect);
	  
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
		StOriginSetter	originSetter(mWindowPtr);
		
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
void nsWindow::nsRectToMacRect(const nsRect& aRect, Rect& aMacRect)
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
void
nsWindow::ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY)
{
	PRInt32	offX, offY;
	CalcOffset(offX,offY);

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

NS_IMETHODIMP nsWindow::SetTitle(const nsAString& title)
{
  NS_ERROR("Would some Mac person please implement me? Thanks.");
  return NS_OK;
}

NS_IMETHODIMP nsWindow::GetAttention(PRInt32 aCycleCount)
{
        // Since the Mac doesn't consider each window a separate process this call functions
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
	gNMRec.qType    = nmType;
	gNMRec.nmMark   = 1;      // Make the dock icon bounce
	gNMRec.nmSound  = NULL;   // No alert sound, see bug 307323
	gNMRec.nmStr    = NULL;   // No alert/window so no text
	gNMRec.nmResp   = NULL;   // No response proc, use the default behavior
	gNMRec.nmRefCon = 0;
	if (::NMInstall(&gNMRec) == noErr)
		gNotificationInstalled = true;

	return NS_OK;
}

#pragma mark -


NS_IMETHODIMP nsWindow::GetPluginClipRect(nsRect& outClipRect, nsPoint& outOrigin, PRBool& outWidgetVisible)
{
  PRBool isVisible = mVisible;

  nsRect widgetClipRect = mBounds;
  // absX and absY are top-level window-relative coordinates
  nscoord absX = widgetClipRect.x;
  nscoord absY = widgetClipRect.y;

  nscoord ancestorX = -widgetClipRect.x;
  nscoord ancestorY = -widgetClipRect.y;

  // Calculate clipping relative to this widget
  widgetClipRect.x = 0;
  widgetClipRect.y = 0;

  // Gather up the absolute position of the widget, clip window, and visibilty
  nsIWidget* widget = GetParent();
  while (widget)
  {
    if (isVisible)
      widget->IsVisible(isVisible);

    nsRect widgetRect;
    widget->GetClientBounds(widgetRect);
    nscoord wx = widgetRect.x;
    nscoord wy = widgetRect.y;

    widgetRect.x = ancestorX;
    widgetRect.y = ancestorY;

    widgetClipRect.IntersectRect(widgetClipRect, widgetRect);
    absX += wx;
    absY += wy;
    widget = widget->GetParent();
    if (!widget)
    {
      // Don't include the top-level windows offset
      // printf("Top level window offset %d %d\n", wx, wy);
      absX -= wx;
      absY -= wy;
    }
    ancestorX -= wx;
    ancestorY -= wy;
  }

  widgetClipRect.x += absX;
  widgetClipRect.y += absY;

  outClipRect = widgetClipRect;
  outOrigin.x = absX;
  outOrigin.y = absY;
  
  outWidgetVisible = isVisible;
  return NS_OK;
}


NS_IMETHODIMP nsWindow::StartDrawPlugin(void)
{
  // setup the port background color for the plugin
  GrafPtr savePort;
  ::GetPort(&savePort);  // save our current port
  ::SetPortWindowPort(mWindowPtr);
  
  RGBColor macColor;
  macColor.red   = COLOR8TOCOLOR16(NS_GET_R(mBackground));  // convert to Mac color
  macColor.green = COLOR8TOCOLOR16(NS_GET_G(mBackground));
  macColor.blue  = COLOR8TOCOLOR16(NS_GET_B(mBackground));
  ::RGBBackColor(&macColor);

  ::SetPort(savePort);  // restore port

  return NS_OK;
}

NS_IMETHODIMP nsWindow::EndDrawPlugin(void)
{
  GrafPtr savePort;
  ::GetPort(&savePort);  // save our current port
  ::SetPortWindowPort(mWindowPtr);

  RGBColor rgbWhite = { 0xFFFF, 0xFFFF, 0xFFFF };
  ::RGBBackColor(&rgbWhite);

  ::SetPort(savePort);  // restore port
  return NS_OK;
}


#pragma mark -


NS_IMETHODIMP nsWindow::ResetInputState()
{
	// currently, the nsMacEventHandler is owned by nsMacWindow, which is the top level window
	// we delegate this call to its parent
  nsIWidget* parent = GetParent();
  NS_ASSERTION(parent, "cannot get parent");
  if (parent)
  {
    nsCOMPtr<nsIKBStateControl> kb = do_QueryInterface(parent);
    NS_ASSERTION(kb, "cannot get parent");
  	if (kb) {
  		return kb->ResetInputState();
  	}
  }
	return NS_ERROR_ABORT;
}

NS_IMETHODIMP nsWindow::SetIMEOpenState(PRBool aState) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsWindow::GetIMEOpenState(PRBool* aState) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsWindow::SetIMEEnabled(PRUint32 aState) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsWindow::GetIMEEnabled(PRUint32* aState) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsWindow::CancelIMEComposition() {
  return NS_ERROR_NOT_IMPLEMENTED;
}

