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


#include "nsChildView.h"
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
#include "nsMacResources.h"
#include "nsRegionMac.h"
#include "nsIRollupListener.h"
#include "nsIEventSink.h"

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
static NMRec  gNMRec;
static Boolean  gNotificationInstalled = false;


#pragma mark -

//#define PAINT_DEBUGGING         // flash areas as they are painted
//#define INVALIDATE_DEBUGGING    // flash areas as they are invalidated

#if defined(INVALIDATE_DEBUGGING) || defined(PAINT_DEBUGGING)
static void blinkRect(Rect* r);
static void blinkRgn(RgnHandle rgn);
#endif


#pragma mark -


//
// Convenience routines to go from a gecko rect to cocoa NSRects and back
//

static void ConvertGeckoToCocoaRect ( const nsRect & inGeckoRect, NSRect & outCocoaRect );
static void
ConvertGeckoToCocoaRect ( const nsRect & inGeckoRect, NSRect & outCocoaRect )
{
  outCocoaRect.origin.x = inGeckoRect.x;
  outCocoaRect.origin.y = inGeckoRect.y;
  outCocoaRect.size.width = inGeckoRect.width;
  outCocoaRect.size.height = inGeckoRect.height;
}

static void ConvertCocoaToGeckoRect ( const NSRect & inCocoaRect, nsRect & outGeckoRect ) ;
static void
ConvertCocoaToGeckoRect ( const NSRect & inCocoaRect, nsRect & outGeckoRect )
{
  outGeckoRect.x = NS_STATIC_CAST(nscoord, inCocoaRect.origin.x);
  outGeckoRect.y = NS_STATIC_CAST(nscoord, inCocoaRect.origin.y);
  outGeckoRect.width = NS_STATIC_CAST(nscoord, inCocoaRect.size.width);
  outGeckoRect.height = NS_STATIC_CAST(nscoord, inCocoaRect.size.height);
}


static void  ConvertGeckoRectToMacRect(const nsRect& aRect, Rect& outMacRect) ;
static void 
ConvertGeckoRectToMacRect(const nsRect& aRect, Rect& outMacRect)
{
  outMacRect.left = aRect.x;
  outMacRect.top = aRect.y;
  outMacRect.right = aRect.x + aRect.width;
  outMacRect.bottom = aRect.y + aRect.height;
}


#pragma mark -


//-------------------------------------------------------------------------
//
// nsChildView constructor
//
//-------------------------------------------------------------------------
nsChildView::nsChildView() : nsBaseWidget() , nsDeleteObserved(this)
{
  WIDGET_SET_CLASSNAME("nsChildView");

  mParent = nsnull;
  mWindow = nil;
  mView = nil;

  mDestroyCalled = PR_FALSE;
  mDestructorCalled = PR_FALSE;
  mVisible = PR_FALSE;
  mDrawing = PR_FALSE;
  mFontMetrics = nsnull;
  mTempRenderingContext = nsnull;

  SetBackgroundColor(NS_RGB(255, 255, 255));
  SetForegroundColor(NS_RGB(0, 0, 0));

  mPluginPort = nsnull;

  AcceptFocusOnClick(PR_TRUE);
}


//-------------------------------------------------------------------------
//
// nsChildView destructor
//
//-------------------------------------------------------------------------
nsChildView::~nsChildView()
{
  if ( mView ) {
    [mView removeFromSuperview];
    [mView release];
  }
  
  // notify the children that we're gone
//FIXME do we still need this?
  nsCOMPtr<nsIEnumerator> children ( getter_AddRefs(GetChildren()) );
  if (children)
  {
    children->First();
    do
    {
      nsISupports* child;
      if (NS_SUCCEEDED(children->CurrentItem(&child)))
      {
        nsChildView* childWindow = static_cast<nsChildView*>(static_cast<nsIWidget*>(child));
        NS_RELEASE(child);

        childWindow->mParent = nsnull;
      }
    } while (NS_SUCCEEDED(children->Next()));     
  }
      
  NS_IF_RELEASE(mTempRenderingContext); 
  NS_IF_RELEASE(mFontMetrics);
  
  delete mPluginPort;
  
}

NS_IMPL_ISUPPORTS_INHERITED1(nsChildView, nsBaseWidget, nsIKBStateControl);

//-------------------------------------------------------------------------
//
// Utility method for implementing both Create(nsIWidget ...) and
// Create(nsNativeWidget...)
//-------------------------------------------------------------------------

nsresult nsChildView::StandardCreate(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData,
                      nsNativeWidget aNativeParent) // should always be nil here
{
  mParent = aParent;
  mBounds = aRect;

//  CalcWindowRegions();

  BaseCreate(aParent, aRect, aHandleEventFunction, 
              aContext, aAppShell, aToolkit, aInitData);

  // inherit things from the parent view and create our parallel 
  // NSView in the Cocoa display system
  if (mParent) {
    SetBackgroundColor(mParent->GetBackgroundColor());
    SetForegroundColor(mParent->GetForegroundColor());

    // inherit the top-level window. NS_NATIVE_DISPLAY is the NSWindow that rules us all
    mWindow = (NSWindow*)aParent->GetNativeData(NS_NATIVE_DISPLAY);
  
    // get the event sink for our view. Walk up the parent chain to the
    // toplevel window, it's the sink.
    nsCOMPtr<nsIWidget> curr = aParent;
    nsCOMPtr<nsIWidget> topLevel = nsnull;
    while ( curr ) {
      topLevel = curr;
      nsCOMPtr<nsIWidget> temp = curr;
      curr = dont_AddRef(temp->GetParent());
    }
    nsCOMPtr<nsIEventSink> sink ( do_QueryInterface(topLevel) );
    NS_ASSERTION(sink, "no event sink, event dispatching will not work");

    // create our parallel NSView and hook it up to our parent. Recall
    // that NS_NATIVE_WINDOW is the NSView.
    NSRect r;
    ConvertGeckoToCocoaRect(mBounds, r);
    mView = [[ChildView alloc] initWithGeckoChild:this eventSink:sink];
    [mView setFrame:r];
    
    NSView* superView = (NSView*)aParent->GetNativeData(NS_NATIVE_WINDOW);
    NS_ASSERTION(superView && mView, "couldn't hook up new NSView in hierarchy");
    if ( superView && mView )
      [superView addSubview:mView];
    
  }
  
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// create a nsChildView
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsChildView::Create(nsIWidget *aParent,
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
// Creates a main nsChildView using a native widget
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsChildView::Create(nsNativeWidget aNativeParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  // we know that whatever is passed in for |aNativeParent| really is
  // a nsIWidget since all our impls of NS_NATIVE_WIDGET return things
  // that are nsIWidgets. Such terrible apis.
  return(Create((nsIWidget*)aNativeParent, aRect, aHandleEventFunction,
                  aContext, aAppShell, aToolkit, aInitData));
}

//-------------------------------------------------------------------------
//
// Close this nsChildView
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsChildView::Destroy()
{
  if (mDestroyCalled)
    return NS_OK;
  mDestroyCalled = PR_TRUE;

  nsBaseWidget::OnDestroy();
  nsBaseWidget::Destroy();
  mParent = nsnull;

  // just to be safe. If we're going away and for some reason we're still
  // the rollup widget, rollup and turn off capture.
  if ( this == gRollupWidget ) {
    if ( gRollupListener )
      gRollupListener->Rollup();
    CaptureRollupEvents(nsnull, PR_FALSE, PR_TRUE);
  }

  ReportDestroyEvent(); // beard: this seems to cause the window to be deleted. moved all release code to destructor.

  return NS_OK;
}

#pragma mark -
//-------------------------------------------------------------------------
//
// Get this nsChildView parent
//
//-------------------------------------------------------------------------
nsIWidget* nsChildView::GetParent(void)
{
  NS_IF_ADDREF(mParent);
  return  mParent;
}

//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void* nsChildView::GetNativeData(PRUint32 aDataType)
{
  void* retVal = nsnull;

  switch (aDataType) 
  {
	  case NS_NATIVE_WIDGET:            // the widget implementation
    	retVal = (void*)this;
    	break;

    case NS_NATIVE_WINDOW:            // the native child area
      retVal = (void*)mView;
      break;
      
    case NS_NATIVE_DISPLAY:           // the native window
      retVal = (void*)mWindow;
    	break;

    case NS_NATIVE_GRAPHIC:           // quickdraw port (for now)
      retVal = [mView qdPort];
      break;
      
    case NS_NATIVE_REGION:
    {
#if 0
      //FIXME - this will leak.
      RgnHandle visRgn = ::NewRgn();
      ::GetPortVisibleRegion([mView qdPort], visRgn);
      retVal = (void*)visRgn;
      //retVal = (void*)mVisRegion;
#endif
      break;
    }
      
    case NS_NATIVE_OFFSETX:
    {
      nsPoint point(0,0);
      point.MoveTo(mBounds.x, mBounds.y);
      LocalToWindowCoordinate(point);
      retVal = (void*)point.x;
      break;
    }

    case NS_NATIVE_OFFSETY:
    {
      nsPoint point(0,0);
      point.MoveTo(mBounds.x, mBounds.y);
      LocalToWindowCoordinate(point);
      retVal = (void*)point.y;
      break;
    }
    
#if 0
    case NS_NATIVE_COLORMAP:
      //¥TODO
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
#endif
  }

  return retVal;
}

#pragma mark -


//-------------------------------------------------------------------------
//
// Return PR_TRUE if the whether the component is visible, PR_FALSE otherwise
//
//-------------------------------------------------------------------------
NS_METHOD nsChildView::IsVisible(PRBool & bState)
{
  bState = mVisible;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsChildView::Show(PRBool bState)
{
  mVisible = bState;
  return NS_OK;
}

    
NS_IMETHODIMP nsChildView::ModalEventFilter(PRBool aRealEvent, void *aEvent,
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
NS_IMETHODIMP nsChildView::Enable(PRBool bState)
{
  // unimplemented.
  return NS_OK;
}

    
static Boolean we_are_front_process()
{
  ProcessSerialNumber thisPSN;
  ProcessSerialNumber frontPSN;
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
NS_IMETHODIMP nsChildView::SetFocus(PRBool aRaise)
{
#if 0
  gEventDispatchHandler.SetFocus(this);
  
  // Here's where we see if there's a notification we need to remove
  if (gNotificationInstalled && we_are_front_process())
  {
    (void)::NMRemove(&gNMRec);
    gNotificationInstalled = false;
  }
#endif
  
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this component font
//
//-------------------------------------------------------------------------
nsIFontMetrics* nsChildView::GetFont(void)
{
  return mFontMetrics;
}

    
//-------------------------------------------------------------------------
//
// Set this component font
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsChildView::SetFont(const nsFont &aFont)
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
NS_IMETHODIMP nsChildView::SetColorMap(nsColorMap *aColorMap)
{
  return NS_OK;
}


//
// SetMenuBar
// ShowMenuBar
// GetMenuBar
//
// Meaningless in this context. A subview doesn't have a menubar.
//

NS_IMETHODIMP nsChildView::SetMenuBar(nsIMenuBar * aMenuBar)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsChildView::ShowMenuBar(PRBool aShow)
{
  return NS_ERROR_FAILURE;
}

nsIMenuBar* nsChildView::GetMenuBar()
{
  return nsnull;
}


//
// SetCursor
//
// Override to set the cursor on the mac
//
NS_METHOD nsChildView::SetCursor(nsCursor aCursor)
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
      case eCursor_standard:        cursor = kThemeArrowCursor;           break;
      case eCursor_wait:            cursor = kThemeWatchCursor;           break;
      case eCursor_select:          cursor = kThemeIBeamCursor;           break;
      case eCursor_hyperlink:       cursor = kThemePointingHandCursor;    break;
      case eCursor_sizeWE:          cursor = kThemeResizeLeftRightCursor; break;
      case eCursor_sizeNS:          cursor = 129;   break;
      case eCursor_sizeNW:          cursor = 130;   break;
      case eCursor_sizeSE:          cursor = 131;   break;
      case eCursor_sizeNE:          cursor = 132;   break;
      case eCursor_sizeSW:          cursor = 133;   break;
      case eCursor_arrow_north:     cursor = 134;   break;
      case eCursor_arrow_north_plus:cursor = 135;   break;
      case eCursor_arrow_south:     cursor = 136;   break;
      case eCursor_arrow_south_plus:cursor = 137;   break;
      case eCursor_arrow_west:      cursor = 138;   break;
      case eCursor_arrow_west_plus: cursor = 139;   break;
      case eCursor_arrow_east:      cursor = 140;   break;
      case eCursor_arrow_east_plus: cursor = 141;   break;
      case eCursor_crosshair:       cursor = kThemeCrossCursor;           break;
      case eCursor_move:            cursor = kThemeOpenHandCursor;        break;
      case eCursor_help:            cursor = 143;   break;
      case eCursor_copy:            cursor = 144;   break; // CSS3
      case eCursor_alias:           cursor = 145;   break;
      case eCursor_context_menu:    cursor = 146;   break;
      case eCursor_cell:            cursor = kThemePlusCursor;            break;
      case eCursor_grab:            cursor = kThemeOpenHandCursor;        break;
      case eCursor_grabbing:        cursor = kThemeClosedHandCursor;      break;
      case eCursor_spinning:        cursor = 149; break;  // better than kThemeSpinningCursor
      case eCursor_count_up:        cursor = kThemeCountingUpHandCursor;          break;
      case eCursor_count_down:      cursor = kThemeCountingDownHandCursor;        break;
      case eCursor_count_up_down:   cursor = kThemeCountingUpAndDownHandCursor;   break;

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
      case eCursor_standard:        ::InitCursor();         break;
      case eCursor_wait:            cursor = watchCursor;   break;
      case eCursor_select:          cursor = iBeamCursor;   break;
      case eCursor_hyperlink:       cursor = plusCursor;    break;
      case eCursor_sizeWE:          cursor = 128;           break;
      case eCursor_sizeNS:          cursor = 129;   break;
      case eCursor_sizeNW:          cursor = 130;   break;
      case eCursor_sizeSE:          cursor = 131;   break;
      case eCursor_sizeNE:          cursor = 132;   break;
      case eCursor_sizeSW:          cursor = 133;   break;
      case eCursor_arrow_north:     cursor = 134;   break;
      case eCursor_arrow_north_plus:cursor = 135;   break;
      case eCursor_arrow_south:     cursor = 136;   break;
      case eCursor_arrow_south_plus:cursor = 137;   break;
      case eCursor_arrow_west:      cursor = 138;   break;
      case eCursor_arrow_west_plus: cursor = 139;   break;
      case eCursor_arrow_east:      cursor = 140;   break;
      case eCursor_arrow_east_plus: cursor = 141;   break;
      case eCursor_crosshair:       cursor = crossCursor;   break;
      case eCursor_move:            cursor = 142;   break;
      case eCursor_help:            cursor = 143;   break;
      case eCursor_copy:            cursor = 144;   break; // CSS3
      case eCursor_alias:           cursor = 145;   break;
      case eCursor_context_menu:    cursor = 146;   break;
      case eCursor_cell:            cursor = plusCursor;   break;
      case eCursor_grab:            cursor = 147;   break;
      case eCursor_grabbing:        cursor = 148;   break;
      case eCursor_spinning:        cursor = 149;   break;
      case eCursor_count_up:        cursor = watchCursor;   break;
      case eCursor_count_down:      cursor = watchCursor;   break;
      case eCursor_count_up_down:   cursor = watchCursor;   break;
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
  
} // nsChildView::SetCursor

#pragma mark -
//-------------------------------------------------------------------------
//
// Get this component dimension
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsChildView::GetBounds(nsRect &aRect)
{
  aRect = mBounds;
  return NS_OK;
}


NS_METHOD nsChildView::SetBounds(const nsRect &aRect)
{
  nsresult rv = Inherited::SetBounds(aRect);
  if ( NS_SUCCEEDED(rv) ) {
    //CalcWindowRegions();
    NSRect r;
    ConvertGeckoToCocoaRect(aRect, r);
    [mView setFrame:r];
  }

  return rv;
}


NS_IMETHODIMP nsChildView::ConstrainPosition(PRInt32 *aX, PRInt32 *aY)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Move this component
// aX and aY are in the parent widget coordinate system
//-------------------------------------------------------------------------
NS_IMETHODIMP nsChildView::Move(PRInt32 aX, PRInt32 aY)
{
  if ((mBounds.x != aX) || (mBounds.y != aY))
  {
    // Invalidate the current location (unless it's the top-level window)
    if (mParent != nsnull)
      Invalidate(PR_FALSE);
    [[mView superview] setNeedsDisplayInRect:[mView frame]];    //XXX needed?
    
    // Set the bounds
    mBounds.x = aX;
    mBounds.y = aY;

    // Recalculate the regions
    //CalcWindowRegions();
    NSRect r;
    ConvertGeckoToCocoaRect(mBounds, r);
    [mView setFrame:r];
    [mView setNeedsDisplay:YES];
    
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
NS_IMETHODIMP nsChildView::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  if ((mBounds.width != aWidth) || (mBounds.height != aHeight))
  {
    // Set the bounds
    mBounds.width  = aWidth;
    mBounds.height = aHeight;

    [[mView superview] setNeedsDisplayInRect:[mView frame]];    //XXX needed?
    
  // Recalculate the regions
  //CalcWindowRegions();
    NSRect r;
    ConvertGeckoToCocoaRect(mBounds, r);
    [mView setFrame:r];
    [mView setNeedsDisplay:YES];

    // Report the event
    ReportSizeEvent();
  }
  else {
    // Recalculate the regions. We always need to do this, our parents may have
    // changed, hence changing our notion of visibility. We then also should make
    // sure that we invalidate ourselves correctly. Fixes bug 18240 (pinkerton).
    CalcWindowRegions();
    [mView setNeedsDisplay:YES];
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsChildView::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  Move(aX, aY);
  Resize(aWidth, aHeight, aRepaint);
  return NS_OK;
}


//
// GetPreferredSize
// SetPreferredSize
//
// Nobody even calls these aywhere in the code
//
NS_METHOD nsChildView::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  return NS_ERROR_FAILURE;
}

NS_METHOD nsChildView::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
  return NS_ERROR_FAILURE;
}


//-------------------------------------------------------------------------
// 
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsChildView::BeginResizingChildren(void)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
// 
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsChildView::EndResizingChildren(void)
{
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
NS_IMETHODIMP nsChildView::Invalidate(PRBool aIsSynchronous)
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
NS_IMETHODIMP nsChildView::Invalidate(const nsRect &aRect, PRBool aIsSynchronous)
{
  if ( !mView )
    return NS_OK;
    
  NSRect r;
  ConvertGeckoToCocoaRect ( aRect, r );
  
  if ( aIsSynchronous )
    [mView displayRect:r];
  else
    [mView setNeedsDisplayInRect:r];
  
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Invalidate this component's visible area
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsChildView::InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous)
{
  if ( !mView )
    return NS_OK;
    
//FIXME rewrite to use a Cocoa region when nsIRegion isn't a QD Region
  NSRect r;
  nsRect bounds;
  nsIRegion* region = NS_CONST_CAST(nsIRegion*, aRegion);     // ugh. this method should be const
  region->GetBoundingBox ( &bounds.x, &bounds.y, &bounds.width, &bounds.height );
  ConvertGeckoToCocoaRect(bounds, r);
  
  if ( aIsSynchronous )
    [mView displayRect:r];
  else
    [mView setNeedsDisplayInRect:r];

  return NS_OK;
}

inline PRUint16 COLOR8TOCOLOR16(PRUint8 color8)
{
  // return (color8 == 0xFF ? 0xFFFF : (color8 << 8));
  return (color8 << 8) | color8;  /* (color8 * 257) == (color8 * 0x0101) */
}

//-------------------------------------------------------------------------
//  StartDraw
//
//-------------------------------------------------------------------------
void nsChildView::StartDraw(nsIRenderingContext* aRenderingContext)
{
  if (mDrawing)
    return;
  mDrawing = PR_TRUE;

  CalcWindowRegions();  //¥REVISIT

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

  mTempRenderingContext->SetColor(color);       // just in case, set the rendering context color too
}


//-------------------------------------------------------------------------
//  EndDraw
//
//-------------------------------------------------------------------------
void nsChildView::EndDraw()
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
nsChildView::Flash(nsPaintEvent &aEvent)
{
  Rect flashRect;
  if (debug_WantPaintFlashing() && aEvent.rect ) {
    ::SetRect ( &flashRect, aEvent.rect->x, aEvent.rect->y, aEvent.rect->x + aEvent.rect->width,
            aEvent.rect->y + aEvent.rect->height );
    StPortSetter portSetter(NS_REINTERPRET_CAST(GrafPtr, [mView qdPort]));
    unsigned long endTicks;
    ::InvertRect ( &flashRect );
    ::Delay(10, &endTicks);
    ::InvertRect ( &flashRect );
  }
}


//
// OnPaint
//
// Dummy impl, meant to be overridden
//
PRBool
nsChildView::OnPaint(nsPaintEvent &event)
{
  return PR_TRUE;
}


//
// Update
//
// this is handled for us by UpdateWidget
// 
NS_IMETHODIMP nsChildView::Update()
{
  UpdateWidget(mBounds, GetRenderingContext());
  return NS_OK;
}


#pragma mark -


//
// UpdateWidget
//
// Dispatches the Paint event into Gecko. Usually called from our 
// NSView in response to the display system noticing that something'
// needs repainting. We don't have to worry about painting our child views
// because the display system will take care of that for us.
//
void 
nsChildView::UpdateWidget(nsRect& aRect, nsIRenderingContext* aContext)
{
  if (! mVisible)
    return;

  ::SetPort([mView qdPort]);

  // initialize the paint event
  nsPaintEvent paintEvent;
  paintEvent.eventStructType      = NS_PAINT_EVENT;   // nsEvent
  paintEvent.message          = NS_PAINT;
  paintEvent.widget         = this;         // nsGUIEvent
  paintEvent.nativeMsg        = NULL;
  paintEvent.renderingContext     = aContext;       // nsPaintEvent
  paintEvent.rect           = &aRect;

  // draw the widget
  StartDraw(aContext);
  if ( OnPaint(paintEvent) ) {
    nsEventStatus eventStatus;
    DispatchWindowEvent(paintEvent,eventStatus);
    if(eventStatus != nsEventStatus_eIgnore)
      Flash(paintEvent);
  }
  EndDraw();
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
nsChildView::ScrollBits ( Rect & inRectToScroll, PRInt32 inLeftDelta, PRInt32 inTopDelta )
{
#if 0
  ::ScrollWindowRect ( mWindowPtr, &inRectToScroll, inLeftDelta, inTopDelta, 
                        kScrollWindowInvalidate, NULL );
#endif
}


//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsChildView::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  NS_WARNING("Scroll not yet implemented for Cocoa");
  return NS_OK;
#if 0
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
  ConvertGeckoRectToMacRect(scrollRect, macRect);


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
        nsChildView* childWindow = static_cast<nsChildView*>(static_cast<nsIWidget*>(child));
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
#endif
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------

PRBool nsChildView::ConvertStatus(nsEventStatus aStatus)
{
  switch (aStatus)
  {
    case nsEventStatus_eIgnore:             return(PR_FALSE);
    case nsEventStatus_eConsumeNoDefault:   return(PR_TRUE);  // don't do default processing
    case nsEventStatus_eConsumeDoDefault:   return(PR_FALSE);
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
NS_IMETHODIMP nsChildView::DispatchEvent(nsGUIEvent* event, nsEventStatus& aStatus)
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
PRBool nsChildView::DispatchWindowEvent(nsGUIEvent &event)
{
  nsEventStatus status;
  DispatchEvent(&event, status);
  return ConvertStatus(status);
}

//-------------------------------------------------------------------------
PRBool nsChildView::DispatchWindowEvent(nsGUIEvent &event,nsEventStatus &aStatus)
{
  DispatchEvent(&event, aStatus);
  return ConvertStatus(aStatus);
}

//-------------------------------------------------------------------------
//
// Deal with all sort of mouse event
//
//-------------------------------------------------------------------------
PRBool nsChildView::DispatchMouseEvent(nsMouseEvent &aEvent)
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
PRBool nsChildView::ReportDestroyEvent()
{
  // nsEvent
  nsGUIEvent moveEvent;
  moveEvent.eventStructType = NS_GUI_EVENT;
  moveEvent.message     = NS_DESTROY;
  moveEvent.point.x     = 0;
  moveEvent.point.y     = 0;
  moveEvent.time        = PR_IntervalNow();

  // nsGUIEvent
  moveEvent.widget      = this;
  moveEvent.nativeMsg   = nsnull;

  // dispatch event
  return (DispatchWindowEvent(moveEvent));
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool nsChildView::ReportMoveEvent()
{
  // nsEvent
  nsGUIEvent moveEvent;
  moveEvent.eventStructType = NS_GUI_EVENT;
  moveEvent.message     = NS_MOVE;
  moveEvent.point.x     = mBounds.x;
  moveEvent.point.y     = mBounds.y;
  moveEvent.time        = PR_IntervalNow();

  // nsGUIEvent
  moveEvent.widget      = this;
  moveEvent.nativeMsg   = nsnull;

  // dispatch event
  return (DispatchWindowEvent(moveEvent));
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool nsChildView::ReportSizeEvent()
{
  // nsEvent
  nsSizeEvent sizeEvent;
  sizeEvent.eventStructType = NS_SIZE_EVENT;
  sizeEvent.message     = NS_SIZE;
  sizeEvent.point.x     = 0;
  sizeEvent.point.y     = 0;
  sizeEvent.time        = PR_IntervalNow();

  // nsGUIEvent
  sizeEvent.widget      = this;
  sizeEvent.nativeMsg   = nsnull;

  // nsSizeEvent
  sizeEvent.windowSize  = &mBounds;
  sizeEvent.mWinWidth   = mBounds.width;
  sizeEvent.mWinHeight  = mBounds.height;
  
  // dispatch event
  return(DispatchWindowEvent(sizeEvent));
}



#pragma mark -


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void nsChildView::CalcWindowRegions()
{
  // i don't think this is necessary anymore...
}



//-------------------------------------------------------------------------
/*  Calculate the x and y offsets for this particular widget
 *  @update  ps 09/22/98
 *  @param   aX -- x offset amount
 *  @param   aY -- y offset amount 
 *  @return  NOTHING
 */
 
NS_IMETHODIMP nsChildView::CalcOffset(PRInt32 &aX,PRInt32 &aY)
{
  aX = aY = 0;
  NSRect bounds;
  bounds = [mView convertRect:bounds toView:nil];
  aX += NS_STATIC_CAST(PRInt32, bounds.origin.x);
  aY += NS_STATIC_CAST(PRInt32, bounds.origin.y);

  return NS_OK;
}


//-------------------------------------------------------------------------
// PointInWidget
//    Find if a point in local coordinates is inside this object
//-------------------------------------------------------------------------
PRBool nsChildView::PointInWidget(Point aThePoint)
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
//    Recursively look for the widget hit
//    @param aParent   -- parent widget. 
//    @param aThePoint -- a point in local coordinates to test for the hit. 
//-------------------------------------------------------------------------
nsChildView*  nsChildView::FindWidgetHit(Point aThePoint)
{
  if (!mVisible || !PointInWidget(aThePoint))
    return nsnull;

  nsChildView* widgetHit = this;

  nsCOMPtr<nsIEnumerator> normalEnum ( getter_AddRefs(GetChildren()) );
  nsCOMPtr<nsIBidirectionalEnumerator> children ( do_QueryInterface(normalEnum) );
  if (children)
  {
    // traverse through all the nsChildViews to find out who got hit, lowest level of course
    children->Last();
    do
    {
      nsISupports* child;
      if (NS_SUCCEEDED(children->CurrentItem(&child)))
      {
        nsChildView* childWindow = static_cast<nsChildView*>(static_cast<nsIWidget*>(child));
        NS_RELEASE(child);

        nsChildView* deeperHit = childWindow->FindWidgetHit(aThePoint);
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
//    Convert the given rect to global coordinates.
//    @param aLocalRect  -- rect in local coordinates of this widget
//    @param aGlobalRect -- |aLocalRect| in global coordinates
//-------------------------------------------------------------------------
NS_IMETHODIMP nsChildView::WidgetToScreen(const nsRect& aLocalRect, nsRect& aGlobalRect)
{
  NSRect temp;
  ConvertGeckoToCocoaRect(aLocalRect, temp);
  temp = [mView convertRect:temp toView:nil];                       // convert to window coords
  temp.origin = [mWindow convertBaseToScreen:temp.origin];   // convert to screen coords
  ConvertCocoaToGeckoRect(temp, aGlobalRect);
    
  return NS_OK;
}



//-------------------------------------------------------------------------
// ScreenToWidget
//    Convert the given rect to local coordinates.
//    @param aGlobalRect  -- rect in screen coordinates 
//    @param aLocalRect -- |aGlobalRect| in coordinates of this widget
//-------------------------------------------------------------------------
NS_IMETHODIMP nsChildView::ScreenToWidget(const nsRect& aGlobalRect, nsRect& aLocalRect)
{
  NSRect temp;
  ConvertGeckoToCocoaRect(aGlobalRect, temp);
  temp.origin = [mWindow convertScreenToBase:temp.origin];   // convert to screen coords
  temp = [mView convertRect:temp fromView:nil];                     // convert to window coords
  ConvertCocoaToGeckoRect(temp, aLocalRect);
  
  return NS_OK;
} 


//=================================================================
/*  Convert the coordinates to some device coordinates so GFX can draw.
 *  @update  dc 09/16/98
 *  @param   nscoord -- X coordinate to convert
 *  @param   nscoord -- Y coordinate to convert
 *  @return  NONE
 */
void  nsChildView::ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY)
{
  PRInt32 offX = 0, offY = 0;
  this->CalcOffset(offX,offY);

  aX += offX;
  aY += offY;
}


NS_IMETHODIMP nsChildView::CaptureRollupEvents(nsIRollupListener * aListener, 
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


NS_IMETHODIMP nsChildView::SetTitle(const nsString& title)
{
  // nothing to do here
  return NS_OK;
}


NS_IMETHODIMP nsChildView::GetAttention()
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
  gNMRec.qType    = nmType;
  gNMRec.nmMark   = 1;      // Flag the icon in the process menu
  gNMRec.nmSound    = (Handle)-1L;  // Use the default alert sound
  gNMRec.nmStr    = NULL;     // No alert/window so no text
  gNMRec.nmResp   = NULL;     // No response proc, use the default behavior
  gNMRec.nmRefCon = NULL;
  if (::NMInstall(&gNMRec) == noErr)
    gNotificationInstalled = true;

  return NS_OK;
}

#pragma mark -


NS_IMETHODIMP nsChildView::ResetInputState()
{
  // currently, the nsMacEventHandler is owned by nsCocoaWindow, which is the top level window
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




#pragma mark -


@implementation ChildView


//
// initWithGeckoChild:eventSink:
//
// do init stuff
//
- (id)initWithGeckoChild:(nsChildView*)inChild eventSink:(nsIEventSink*)inSink
{
  [super init];
  
//  mMouseEnterExitTag = [self addTrackingRect:[self frame] owner:self userData:nsnull assumeInside:NO];  
//printf("__ctor created tag %ld (%d)\n", mMouseEnterExitTag, self);

  mGeckoChild = inChild;
  mEventSink = inSink;
//  mMouseEnterExitTag = nsnull;
  return self;
}


- (void) dealloc
{
  // make sure we remove our tracking rect or this view will keep getting 
  // notifications long after it's gone.
//  if ( mMouseEnterExitTag )
//printf("(((((dtor removing tag %ld, (%d)))))\n", mMouseEnterExitTag, self);
    //[self removeTrackingRect:mMouseEnterExitTag];
  
  [super dealloc];
}


//
// -setFrame
//
// Override in order to keep our mouse enter/exit tracking rect in sync with
// the frame of the view
//
- (void)setFrame:(NSRect)frameRect
{  
  [super setFrame:frameRect];

  // re-establish our tracking rect
  //if ( mMouseEnterExitTag )
//printf("(((((setFrame removing tag %ld, (%d)))))\n", mMouseEnterExitTag, self);
//    [self removeTrackingRect:mMouseEnterExitTag];
//  mMouseEnterExitTag = [self addTrackingRect:frameRect owner:self userData:nsnull assumeInside:NO];  
//printf("__setFrame created tag %ld\n", mMouseEnterExitTag);
}


// 
// -isFlipped
//
// Make the origin of this view the topLeft corner (gecko origin) rather
// than the bottomLeft corner (standard cocoa origin).
//
- (BOOL)isFlipped
{
  return YES;
}


//
// -acceptsFirstResponder
//
// We accept key and mouse events, so don't keep passing them up the chain. Allow
// this to be a 'focussed' widget for event dispatch
//
- (BOOL)acceptsFirstResponder
{
  return YES;
}


//
// -drawRect:
//
// The display system has told us that a portion of our view is dirty. Tell
// gecko to paint it
//
- (void)drawRect:(NSRect)aRect
{
//  printf("drawing (%d) %f %f w/h %f %f\n", self, aRect.origin.x, aRect.origin.y, aRect.size.width, aRect.size.height);
  
  // tell gecko to paint.
  nsRect r;
  ConvertCocoaToGeckoRect(aRect, r);
  mGeckoChild->UpdateWidget(r, mGeckoChild->GetRenderingContext());
}


- (void)mouseDown:(NSEvent *)theEvent
{
  nsMouseEvent geckoEvent;
  geckoEvent.eventStructType = NS_MOUSE_EVENT;
  [self convert:theEvent message:NS_MOUSE_LEFT_BUTTON_DOWN toGeckoEvent:&geckoEvent];
  geckoEvent.clickCount = [theEvent clickCount];
  
  // send event into Gecko by going directly to the
  // the widget.
  mGeckoChild->DispatchMouseEvent(geckoEvent);
  
} // mouseDown


- (void)mouseUp:(NSEvent *)theEvent
{
  nsMouseEvent geckoEvent;
  geckoEvent.eventStructType = NS_MOUSE_EVENT;
  [self convert:theEvent message:NS_MOUSE_LEFT_BUTTON_UP toGeckoEvent:&geckoEvent];
  
  // send event into Gecko by going directly to the
  // the widget.
  mGeckoChild->DispatchMouseEvent(geckoEvent);
  
} // mouseUp


- (void)mouseMoved:(NSEvent*)theEvent
{
  nsMouseEvent geckoEvent;
  geckoEvent.eventStructType = NS_MOUSE_EVENT;
  [self convert:theEvent message:NS_MOUSE_MOVE toGeckoEvent:&geckoEvent];

  // send event into Gecko by going directly to the
  // the widget.
  mGeckoChild->DispatchMouseEvent(geckoEvent);
  
}

- (void)mouseDragged:(NSEvent*)theEvent
{
  [self mouseMoved:theEvent];
}

- (void)mouseEntered:(NSEvent*)theEvent
{
  printf("got mouse ENTERED view\n");
}

- (void)mouseExited:(NSEvent*)theEvent
{
  printf("got mouse EXIT view\n");
}




- (void)keyDown:(NSEvent*)theEvent;
{
  //nsKeyEvent geckoEvent;
  printf("got keydown in view\n");


}


- (void)keyUp:(NSEvent*)theEvent;
{
  printf("got keyup in view\n");


}


//
// -convert:message:toGeckoEvent:
//
// convert from one event system to the other for even dispatching
//
- (void) convert:(NSEvent*)inEvent message:(PRInt32)inMsg toGeckoEvent:(nsInputEvent*)outGeckoEvent
{
  outGeckoEvent->message = inMsg;
  outGeckoEvent->widget = [self widget];
  outGeckoEvent->nativeMsg = inEvent;
  outGeckoEvent->time = PR_IntervalNow();
  NSPoint mouseLoc = [inEvent locationInWindow];
  outGeckoEvent->refPoint.x = NS_STATIC_CAST(nscoord, mouseLoc.x);
  outGeckoEvent->refPoint.y = NS_STATIC_CAST(nscoord, mouseLoc.y);

  // setup modifier keys
  unsigned int modifiers = [inEvent modifierFlags];
	outGeckoEvent->isShift = ((modifiers & NSShiftKeyMask) != 0);
	outGeckoEvent->isControl = ((modifiers & NSControlKeyMask) != 0);
	outGeckoEvent->isAlt = ((modifiers & NSAlternateKeyMask) != 0);
	outGeckoEvent->isMeta = ((modifiers & NSCommandKeyMask) != 0);

  // convert point to view coordinate system
  NSPoint localPoint = [self convertPoint:mouseLoc fromView:nil];
  outGeckoEvent->point.x = NS_STATIC_CAST(nscoord, localPoint.x);
  outGeckoEvent->point.y = NS_STATIC_CAST(nscoord, localPoint.y);

} // convert:toGeckoEvent:

 
//
// -widget
//
// return our gecko child view widget. Note this does not AddRef.
//
- (nsIWidget*) widget
{
  return NS_STATIC_CAST(nsIWidget*, mGeckoChild);
}

@end
