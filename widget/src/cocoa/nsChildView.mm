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

#include <unistd.h>

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

  mView = nil;
  
  mParentView = nil;
  
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
    NSWindow* win = [mView window];
    NSResponder* responder = [win firstResponder];

    // We're being unhooked from the view hierarchy, don't leave our view
    // or a child view as the window first responder.

    if (responder && [responder isKindOfClass:[NSView class]] &&
        [(NSView*)responder isDescendantOf:mView])
      [win makeFirstResponder: [mView superview]];

    [mView removeFromSuperviewWithoutNeedingDisplay];
    [mView release];
  }
  
  NS_IF_RELEASE(mTempRenderingContext); 
  NS_IF_RELEASE(mFontMetrics);
  
  delete mPluginPort;
}

NS_IMPL_ISUPPORTS_INHERITED2(nsChildView, nsBaseWidget, nsIKBStateControl, nsIEventSink);

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
                      nsNativeWidget aNativeParent)
{
  mBounds = aRect;

//  CalcWindowRegions();

  BaseCreate(aParent, aRect, aHandleEventFunction, 
              aContext, aAppShell, aToolkit, aInitData);

  // inherit things from the parent view and create our parallel 
  // NSView in the Cocoa display system
  mParentView = nil;
  if ( aParent ) {
    SetBackgroundColor(aParent->GetBackgroundColor());
    SetForegroundColor(aParent->GetForegroundColor());

    // inherit the top-level window. NS_NATIVE_WIDGET is always a NSView
    // regardless of if we're asking a window or a view (for compatibility
    // with windows).
    mParentView = (NSView*)aParent->GetNativeData(NS_NATIVE_WIDGET);     
  }
  else
    mParentView = NS_REINTERPRET_CAST(NSView*,aNativeParent);
   
  NS_ASSERTION(mParentView, "no parent view at all :(");
  
  // create our parallel NSView and hook it up to our parent. Recall
  // that NS_NATIVE_WIDGET is the NSView.
  NSRect r;
  ConvertGeckoToCocoaRect(mBounds, r);
  mView = [CreateCocoaView() retain];
  [mView setFrame:r];
  
  NS_ASSERTION(mParentView && mView, "couldn't hook up new NSView in hierarchy");
  if (mParentView && mView ) {
    if (![mParentView isKindOfClass: [ChildView class]]) {
      [mParentView addSubview:mView];
      mVisible = PR_TRUE;
      NSWindow* window = [mParentView window];
      if (!window) {
        // The enclosing view that embeds Gecko is actually hidden
        // right now!  This can happen when Gecko is embedded in the
        // tab of a Cocoa tab view.  See if the parent view responds
        // to our special getNativeWindow selector, and if it does,
        // use that to get the window instead.
        //if ([mParentView respondsToSelector: @selector(getNativeWindow:)])
          [mView setNativeWindow: [mParentView getNativeWindow]];
      }
      else
        [mView setNativeWindow: window];
    }
    else
      [mView setNativeWindow: [mParentView getNativeWindow]];
  }
  
  return NS_OK;
}


//
// CreateCocoaView
//
// Creates the appropriate child view. Override to create something other than
// our |ChildView| object. Autoreleases, so caller must retain.
//
NSView*
nsChildView::CreateCocoaView ( )
{
  return [[[ChildView alloc] initWithGeckoChild:this eventSink:nsnull] autorelease];
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
// Creates a main nsChildView using a native widget (an NSView)
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
  // what we're passed in |aNativeParent| is an NSView. 
  return(StandardCreate(nsnull, aRect, aHandleEventFunction,
                  aContext, aAppShell, aToolkit, aInitData, aNativeParent));
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

static pascal OSStatus OnContentClick(EventHandlerCallRef handler, EventRef event, void* userData)
{
    WindowRef window;
    GetEventParameter(event, kEventParamDirectObject, typeWindowRef, NULL,
                      sizeof(window), NULL, &window);

    EventRecord macEvent;
    ConvertEventRefToEventRecord(event, &macEvent);
    GrafPtr port = GetWindowPort(window);
    StPortSetter setter(port);
    Point localWhere = macEvent.where;
    GlobalToLocal(&localWhere);
    
    nsChildView* childView = (nsChildView*) userData;
    nsMouseEvent geckoEvent;
    geckoEvent.eventStructType = NS_MOUSE_EVENT;
    geckoEvent.message = NS_MOUSE_LEFT_BUTTON_DOWN;
    geckoEvent.nativeMsg = &macEvent;
    geckoEvent.widget = childView;
    geckoEvent.time = PR_IntervalNow();
    geckoEvent.flags = 0;
    geckoEvent.clickCount = 1;

    geckoEvent.refPoint.x = geckoEvent.point.x = localWhere.h;
    geckoEvent.refPoint.y = geckoEvent.point.y = localWhere.v;

    geckoEvent.isShift = ((macEvent.modifiers & (shiftKey|rightShiftKey)) != 0);
    geckoEvent.isControl = ((macEvent.modifiers & controlKey) != 0);
    geckoEvent.isAlt = ((macEvent.modifiers & optionKey) != 0);
    geckoEvent.isMeta = ((macEvent.modifiers & cmdKey) != 0);
    
    // send event into Gecko by going directly to the
    // the widget.
    childView->DispatchMouseEvent(geckoEvent);

    return noErr;
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
    case NS_NATIVE_WIDGET:            // the NSView
    case NS_NATIVE_DISPLAY:
      retVal = (void*)mView;
      break;

    case NS_NATIVE_WINDOW:
      retVal = [mView getNativeWindow];
      break;
      
    case NS_NATIVE_GRAPHIC:           // quickdraw port (for now)
      retVal = GetQuickDrawPort();
      break;
      
    case NS_NATIVE_REGION:
    {
#if 1
      //FIXME - this will leak.
      RgnHandle visRgn = ::NewRgn();
      GrafPtr port = GetQuickDrawPort();
      //printf("asked for visrgn, port is %d\n", port);
      //QDDebugPrintPortInfo(port);
      if (port)
        ::GetPortVisibleRegion(GetQuickDrawPort(), visRgn);
      retVal = (void*)visRgn;
      //retVal = (void*)mVisRegion;
#endif
      break;
    }
      
    case NS_NATIVE_OFFSETX:
      retVal = 0;
      break;

    case NS_NATIVE_OFFSETY:
      retVal = 0;
      break;
    
#if 0
    case NS_NATIVE_COLORMAP:
      //¥TODO
      break;
#endif

    case NS_NATIVE_PLUGIN_PORT:
      // this needs to be a combination of the port and the offsets.
      if (mPluginPort == nsnull)
        mPluginPort = new nsPluginPort;
        
        [mView setIsPluginView: YES];
        
        NSWindow* window = [mView window];
        if (window) {
            WindowRef topLevelWindow = (WindowRef) [window _windowRef];  // PRIVATE APPLE SPI FOO.
            if (topLevelWindow) {
                mPluginPort->port = GetWindowPort(topLevelWindow);

                NSPoint viewOrigin = [mView convertPoint:NSZeroPoint toView:nil];
                NSRect frame = [window frame];
                viewOrigin.y = frame.size.height - viewOrigin.y;
                
                // need to convert view's origin to window coordinates.
                // then, encode as "SetOrigin" ready values.
                mPluginPort->portx = -viewOrigin.x;
                mPluginPort->porty = -viewOrigin.y;
                
                // set up the clipping region for plugins.
                RgnHandle clipRgn = ::NewRgn();
                if (clipRgn != NULL) {
                    NSRect visibleBounds = [mView visibleRect];
                    NSPoint clipOrigin = [mView convertPoint:visibleBounds.origin toView:nil];
                    clipOrigin.y = frame.size.height - clipOrigin.y;
                    SetRectRgn(clipRgn, clipOrigin.x, clipOrigin.y,
                               clipOrigin.x + visibleBounds.size.width,
                               clipOrigin.y + visibleBounds.size.height);
                    SetPortClipRegion(mPluginPort->port, clipRgn);
                    DisposeRgn(clipRgn);
                }
            }
        }
    
      retVal = (void*)mPluginPort;
        break;
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
  if (bState != mVisible) {
    if (bState) 
      [mParentView addSubview: mView];
    else
      [mView removeFromSuperview];
  }
  mVisible = bState;
  return NS_OK;
}

nsIWidget*
nsChildView::GetParent(void)
{
  return nsnull;
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


//
// Enable
//
// Enable/disable this view
//
NS_IMETHODIMP nsChildView::Enable(PRBool aState)
{
  // unimplemented;
  return NS_OK;
}


NS_IMETHODIMP nsChildView::IsEnabled(PRBool *aState)
{
  // unimplemented
  if (aState)
   *aState = PR_TRUE;
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
  NSWindow* window = [mView window];
  if (window)
    [window makeFirstResponder: mView];
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


NS_IMETHODIMP nsChildView::ConstrainPosition(PRBool aAllowSlop,
                                             PRInt32 *aX, PRInt32 *aY)
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
  return MoveWithRepaintOption(aX, aY, PR_TRUE);
}

NS_IMETHODIMP nsChildView::MoveWithRepaintOption(PRInt32 aX, PRInt32 aY, PRBool aRepaint)
{
  if ((mBounds.x != aX) || (mBounds.y != aY))
  {
    // Invalidate the current location
    if (mVisible && aRepaint)
      [[mView superview] setNeedsDisplayInRect: [mView frame]];    //XXX needed?
    
    // Set the bounds
    mBounds.x = aX;
    mBounds.y = aY;
   
    NSRect r;
    ConvertGeckoToCocoaRect(mBounds, r);
    [mView setFrame:r];

    if (mVisible && aRepaint)
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

    if (mVisible && aRepaint)
      [[mView superview] setNeedsDisplayInRect: [mView frame]];    //XXX needed?
    
  // Recalculate the regions
  //CalcWindowRegions();
    NSRect r;
    ConvertGeckoToCocoaRect(mBounds, r);
    [mView setFrame:r];

    if (mVisible && aRepaint)
      [mView setNeedsDisplay:YES];

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
NS_IMETHODIMP nsChildView::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  MoveWithRepaintOption(aX, aY, aRepaint);
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
  if (!mView || !mVisible)
    return NS_OK;

  if (aIsSynchronous)
    [mView display];
  else
    [mView setNeedsDisplay:YES];
  
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Invalidate this component's visible area
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsChildView::Invalidate(const nsRect &aRect, PRBool aIsSynchronous)
{
  if ( !mView || !mVisible)
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
  if ( !mView || !mVisible)
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
#if DEBUG
  Rect flashRect;
  if (debug_WantPaintFlashing() && aEvent.rect ) {
    ::SetRect ( &flashRect, aEvent.rect->x, aEvent.rect->y, aEvent.rect->x + aEvent.rect->width,
            aEvent.rect->y + aEvent.rect->height );
    StPortSetter portSetter(GetQuickDrawPort());
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
  // Update means "Flush any pending changes right now."  It does *not* mean
  // repaint the world. :) -- dwh
  [mView displayIfNeeded];
  return NS_OK;
}


#pragma mark -


//
// UpdateWidget
//
// Dispatches the Paint event into Gecko. Usually called from our 
// NSView in response to the display system noticing that something
// needs repainting. We don't have to worry about painting our child views
// because the display system will take care of that for us.
//
void 
nsChildView::UpdateWidget(nsRect& aRect, nsIRenderingContext* aContext)
{
  if (! mVisible)
    return;
  
  StPortSetter port(GetQuickDrawPort());
  if (mPluginPort) ::SetOrigin(mPluginPort->portx, mPluginPort->porty);
        
  // initialize the paint event
  nsPaintEvent paintEvent;
  paintEvent.eventStructType      = NS_PAINT_EVENT;   // nsEvent
  paintEvent.nativeMsg = nsnull;
  paintEvent.message          = NS_PAINT;
  paintEvent.widget         = this;         // nsGUIEvent
  paintEvent.nativeMsg        = NULL;
  paintEvent.renderingContext     = aContext;       // nsPaintEvent
  paintEvent.rect           = &aRect;

  // offscreen drawing is pointless.
  if (paintEvent.rect->x < 0)
    paintEvent.rect->x = 0;
  if (paintEvent.rect->y < 0)
    paintEvent.rect->y = 0;
    
  // draw the widget
  StartDraw(aContext);
  if ( OnPaint(paintEvent) ) {
    nsEventStatus eventStatus;
    DispatchWindowEvent(paintEvent,eventStatus);
    if(eventStatus != nsEventStatus_eIgnore)
      Flash(paintEvent);
  }
  EndDraw();
  
#if 0
  // draw where a plugin will be.
  if (mPluginPort) {
      ::SetOrigin(mPluginPort->portx, mPluginPort->porty);
      Rect bounds = { 0, 0, mBounds.height, mBounds.width };
      ::FrameRect(&bounds);
      ::MoveTo(bounds.left, bounds.top);
      ::LineTo(bounds.right, bounds.bottom);
      ::MoveTo(bounds.right, bounds.top);
      ::LineTo(bounds.left, bounds.bottom);
      ::SetOrigin(0, 0);
  }
#endif
}


//
// Scroll
//
// Scroll the bits of a view and its children
//
// FIXME: I'm sure the invalidating can be optimized, just no time now.
//
NS_IMETHODIMP nsChildView::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
    if ( !mVisible)
        return NS_OK;

    NSSize scrollVector = {aDx,aDy};
    [mView scrollRect: [mView bounds] by:scrollVector];

    // Scroll the children
    nsCOMPtr<nsIEnumerator> children ( getter_AddRefs(GetChildren()) );
    if ( children ) {
        children->First();
        do {
        nsCOMPtr<nsISupports> child;
        if (NS_SUCCEEDED(children->CurrentItem(getter_AddRefs(child)))) {
            nsCOMPtr<nsIWidget> widget = do_QueryInterface(child);

            // We use resize rather than move since it gives us control
            // over repainting.  In the case of blitting, Quickdraw views
            // draw their child widgets on the blit, so we can scroll
            // like a bat out of hell by not wasting time invalidating
            // the widgets, since it's completely unnecessary to do so.
            nsRect bounds;
            widget->GetBounds(bounds);
            widget->Resize(bounds.x + aDx, bounds.y + aDy, bounds.width, bounds.height, PR_FALSE);
        }
        } while (NS_SUCCEEDED(children->Next()));     
    }

    NSRect frame = [mView bounds];
    NSRect horizInvalid = frame;
    NSRect vertInvalid = frame;

    horizInvalid.size.width = abs(aDx);
    vertInvalid.size.height = abs(aDy);
    if (aDy < 0)
      vertInvalid.origin.y = frame.origin.y + frame.size.height + aDy;
    if (aDx < 0)
      horizInvalid.origin.x = frame.origin.x + frame.size.width + aDx;

    if (aDx != 0)
      [mView setNeedsDisplayInRect: horizInvalid];

    if (aDy != 0)
      [mView setNeedsDisplayInRect: vertInvalid];
    
    return NS_OK;
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
      if (NS_MENU_EVENT == event->eventStructType)
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
  moveEvent.nativeMsg = nsnull;
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
  moveEvent.nativeMsg = nsnull;
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
  sizeEvent.nativeMsg = nsnull;
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
  NSRect bounds = {{0, 0}, {0, 0}};
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
  temp.origin = [[mView getNativeWindow] convertBaseToScreen:temp.origin];   // convert to screen coords
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
  temp.origin = [[mView getNativeWindow] convertScreenToBase:temp.origin];   // convert to screen coords
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
#if 0
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
#endif
  return NS_ERROR_ABORT;
}


//
// GetQuickDrawPort
//
// Find a quickdraw port in which to draw (needed by GFX until it
// is converted to Cocoa). This must be overridden if CreateCocoaView()
// does not create something that inherits from NSQuickDrawView!
//
GrafPtr
nsChildView::GetQuickDrawPort()
{
    if (mPluginPort)
        return mPluginPort->port;
    else
  return [mView qdPort];
}

#pragma mark -


//
// DispatchEvent
//
// Handle an event coming into us and send it to gecko.
//
NS_IMETHODIMP
nsChildView::DispatchEvent ( void* anEvent, PRBool *_retval )
{
  return NS_OK;
}


//
// DragEvent
//
// The drag manager has let us know that something related to a drag has
// occurred in this window. It could be any number of things, ranging from 
// a drop, to a drag enter/leave, or a drag over event. The actual event
// is passed in |aMessage| and is passed along to our event hanlder so Gecko
// knows about it.
//
NS_IMETHODIMP
nsChildView::DragEvent(PRUint32 aMessage, PRInt16 aMouseGlobalX, PRInt16 aMouseGlobalY,
                         PRUint16 aKeyModifiers, PRBool *_retval)
{
  // ensure that this is going to a ChildView (not something else like a
  // scrollbar). I think it's safe to just bail at this point if it's not
  // what we expect it to be
  if ( ![mView isKindOfClass:[ChildView class]] ) {
    *_retval = PR_FALSE;
    return NS_OK;
  }
  
  nsMouseEvent geckoEvent;
	geckoEvent.eventStructType = NS_DRAGDROP_EVENT;
  
  // we're given the point in global coordinates. We need to convert it to
  // window coordinates for convert:message:toGeckoEvent
  NSPoint pt; pt.x = aMouseGlobalX; pt.y = aMouseGlobalY;
  [[mView window] convertScreenToBase:pt];
	[mView convert:pt message:aMessage modifiers:0 toGeckoEvent:&geckoEvent];

// XXXPINK
// hack, because we're currently getting the point in Carbon global coordinates,
// but obviously the cocoa views don't know how to convert those (because they
// use an entirely different coordinate system).
  geckoEvent.point.x = 50; geckoEvent.point.y = 50;
//printf("mouse location is %d %d\n", geckoEvent.point.x, geckoEvent.point.y);
  DispatchWindowEvent(geckoEvent);
  
  // we handled the event
  *_retval = PR_TRUE;
  
  return NS_OK;
}


//
// Scroll
//
// Someone wants us to scroll in the current window, probably as the result
// of a scrollWheel event or external scrollbars. Pass along to the 
// eventhandler.
//
NS_IMETHODIMP
nsChildView::Scroll ( PRBool aVertical, PRInt16 aNumLines, PRInt16 aMouseLocalX, 
                        PRInt16 aMouseLocalY, PRBool *_retval )
{
#if 0
  *_retval = PR_FALSE;
  Point localPoint = {aMouseLocalY, aMouseLocalX};
  if ( mMacEventHandler.get() )
    *_retval = mMacEventHandler->Scroll(aVertical ? kEventMouseWheelAxisY : kEventMouseWheelAxisX,
                                          aNumLines, localPoint);
#endif
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsChildView::Idle()
{
  // do some idle stuff?
  return NS_ERROR_NOT_IMPLEMENTED;
}


#pragma mark -


@implementation ChildView

-(NSMenu*)menuForEvent:(NSEvent*)theEvent
{
  nsMouseEvent geckoEvent;

  int button = [theEvent buttonNumber];
  if (button == 1) {
    // The right mouse went down.  Fire off a right mouse down and
    // then send the context menu event.
    geckoEvent.eventStructType = NS_MOUSE_EVENT;
    geckoEvent.nativeMsg = nsnull;
    [self convert: theEvent message: NS_MOUSE_RIGHT_BUTTON_DOWN toGeckoEvent:&geckoEvent];
    geckoEvent.clickCount = 1;
    mGeckoChild->DispatchMouseEvent(geckoEvent);
  }

  // Fire the context menu event into Gecko.
  geckoEvent.eventStructType = NS_MOUSE_EVENT;
  geckoEvent.nativeMsg = nsnull;
  [self convert:theEvent message:NS_CONTEXTMENU toGeckoEvent:&geckoEvent];
  geckoEvent.clickCount = 0;
  
  // send event into Gecko by going directly to the
  // the widget.
  mGeckoChild->DispatchMouseEvent(geckoEvent);
  
  // Go up our view chain to fetch the correct menu to return.
  return [self getContextMenu];
}

-(NSMenu*)getContextMenu
{
  return [[self superview] getContextMenu];
}

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
  mIsPluginView = NO;
  return self;
}

- (NSWindow*) getNativeWindow
{
  NSWindow* currWin = [self window];
  if (currWin)
     return currWin;
  else
     return mWindow;
}

- (void) setNativeWindow: (NSWindow*)aWindow
{
  mWindow = aWindow;
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

// -isOpaque
//
// XXXdwh.  Quickdraw views are transparent by default.  Since Gecko does its own blending if/when
// opacity is specified, we would like to optimize here by turning off the transparency of the view. 
// But we can't. :(
- (BOOL)isOpaque
{
  return mIsPluginView;
}

-(void)setIsPluginView:(BOOL)aIsPlugin
{
  mIsPluginView = aIsPlugin;
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
  PRBool isVisible;
  mGeckoChild->IsVisible(isVisible);
  if (!isVisible)
    return;
    
   // tell gecko to paint.
  nsRect r;
  ConvertCocoaToGeckoRect(aRect, r);
  nsCOMPtr<nsIRenderingContext> rendContext = getter_AddRefs(mGeckoChild->GetRenderingContext());
  mGeckoChild->UpdateWidget(r, rendContext);
}

- (void)mouseDown:(NSEvent *)theEvent
{
  nsMouseEvent geckoEvent;
  geckoEvent.eventStructType = NS_MOUSE_EVENT;
  geckoEvent.nativeMsg = nsnull;
  [self convert:theEvent message:NS_MOUSE_LEFT_BUTTON_DOWN toGeckoEvent:&geckoEvent];
  geckoEvent.clickCount = [theEvent clickCount];
  
  NSPoint mouseLoc = [theEvent locationInWindow];
  NSPoint screenLoc = [[self window] convertBaseToScreen: mouseLoc];

  EventRecord macEvent;
  macEvent.what = mouseDown;
  macEvent.message = 0;
  macEvent.when = ::TickCount();
  // macEvent.where.h = screenLoc.x, macEvent.where.v = screenLoc.y; XXX fix this, they are flipped!
  GetGlobalMouse(&macEvent.where);
  macEvent.modifiers = GetCurrentKeyModifiers();
  geckoEvent.nativeMsg = &macEvent;

  // send event into Gecko by going directly to the
  // the widget.
  mGeckoChild->DispatchMouseEvent(geckoEvent);
  
} // mouseDown


- (void)mouseUp:(NSEvent *)theEvent
{
  nsMouseEvent geckoEvent;
  geckoEvent.eventStructType = NS_MOUSE_EVENT;
  geckoEvent.nativeMsg = nsnull;
  [self convert:theEvent message:NS_MOUSE_LEFT_BUTTON_UP toGeckoEvent:&geckoEvent];
  
  NSPoint mouseLoc = [theEvent locationInWindow];
  NSPoint screenLoc = [[self window] convertBaseToScreen: mouseLoc];

  EventRecord macEvent;
  macEvent.what = mouseUp;
  macEvent.message = 0;
  macEvent.when = ::TickCount();
  // macEvent.where.h = screenLoc.x, macEvent.where.v = screenLoc.y; XXX fix this, they are flipped!
  GetGlobalMouse(&macEvent.where);
  macEvent.modifiers = GetCurrentKeyModifiers();
  geckoEvent.nativeMsg = &macEvent;

  // send event into Gecko by going directly to the
  // the widget.
  mGeckoChild->DispatchMouseEvent(geckoEvent);
  
} // mouseUp

- (void)mouseMoved:(NSEvent*)theEvent
{
  NSView* view = [[[self window] contentView] hitTest: [theEvent locationInWindow]];
  if (view != (NSView*)self) {
    // We shouldn't handle this.  Send it to the right view.
    [view mouseMoved: theEvent];
    return;
  }
  
  nsMouseEvent geckoEvent;
  geckoEvent.eventStructType = NS_MOUSE_EVENT;
  geckoEvent.nativeMsg = nsnull;
  [self convert:theEvent message:NS_MOUSE_MOVE toGeckoEvent:&geckoEvent];

  NSPoint mouseLoc = [theEvent locationInWindow];
  NSPoint screenLoc = [[self window] convertBaseToScreen: mouseLoc];

  EventRecord macEvent;
  macEvent.what = nullEvent;
  macEvent.message = 0;
  macEvent.when = ::TickCount();
  // macEvent.where.h = screenLoc.x, macEvent.where.v = screenLoc.y; XXX fix this, they are flipped!
  GetGlobalMouse(&macEvent.where);
  macEvent.modifiers = GetCurrentKeyModifiers();
  geckoEvent.nativeMsg = &macEvent;

  // send event into Gecko by going directly to the
  // the widget.
  mGeckoChild->DispatchMouseEvent(geckoEvent);
}

- (void)mouseDragged:(NSEvent*)theEvent
{
    nsMouseEvent geckoEvent;
    geckoEvent.eventStructType = NS_MOUSE_EVENT;
    geckoEvent.nativeMsg = nsnull;
    [self convert:theEvent message:NS_MOUSE_MOVE toGeckoEvent:&geckoEvent];
    
    // send event into Gecko by going directly to the
    // the widget.
    mGeckoChild->DispatchMouseEvent(geckoEvent);    
}

- (void)mouseEntered:(NSEvent*)theEvent
{
  printf("got mouse ENTERED view\n");
}

- (void)mouseExited:(NSEvent*)theEvent
{
  printf("got mouse EXIT view\n");
}

- (void)otherMouseDown:(NSEvent *)theEvent
{
  nsMouseEvent geckoEvent;
  geckoEvent.eventStructType = NS_MOUSE_EVENT;
  geckoEvent.nativeMsg = nsnull;
  [self convert:theEvent message:NS_MOUSE_MIDDLE_BUTTON_DOWN toGeckoEvent:&geckoEvent];
  geckoEvent.clickCount = [theEvent clickCount];
  
  // send event into Gecko by going directly to the
  // the widget.
  mGeckoChild->DispatchMouseEvent(geckoEvent);
  
} // otherMouseDown


- (void)otherMouseUp:(NSEvent *)theEvent
{
  nsMouseEvent geckoEvent;
  geckoEvent.eventStructType = NS_MOUSE_EVENT;
  geckoEvent.nativeMsg = nsnull;
  [self convert:theEvent message:NS_MOUSE_MIDDLE_BUTTON_UP toGeckoEvent:&geckoEvent];
  
  // send event into Gecko by going directly to the
  // the widget.
  mGeckoChild->DispatchMouseEvent(geckoEvent);
  
} // mouseUp

const PRInt32 kNumLines = 4;

-(void)scrollWheel:(NSEvent*)theEvent
{
  // XXXdwh. We basically always get 1 or -1 as the delta.  This is treated by 
  // Gecko as the number of lines to scroll.  We go ahead and use a 
  // default kNumLines of 8 for now (until I learn how we can get settings from
  // the OS). --dwh
  nsMouseScrollEvent geckoEvent;
  geckoEvent.eventStructType = NS_MOUSE_SCROLL_EVENT;
  geckoEvent.nativeMsg = nsnull;
  [self convert:theEvent message:NS_MOUSE_SCROLL toGeckoEvent:&geckoEvent];
  geckoEvent.delta = PRInt32([theEvent deltaY])*(-kNumLines);
  geckoEvent.scrollFlags |= nsMouseScrollEvent::kIsVertical;
 
  // send event into Gecko by going directly to the
  // the widget.
  mGeckoChild->DispatchWindowEvent(geckoEvent);
}

//
// -convert:message:toGeckoEvent:
//
// convert from one event system to the other for event dispatching
//
- (void) convert:(NSEvent*)inEvent message:(PRInt32)inMsg toGeckoEvent:(nsInputEvent*)outGeckoEvent
{
  outGeckoEvent->nativeMsg = inEvent;
  [self convert:[inEvent locationInWindow] message:inMsg modifiers:[inEvent modifierFlags]
          toGeckoEvent:outGeckoEvent];
}

- (void) convert:(NSPoint)inPoint message:(PRInt32)inMsg modifiers:(unsigned int)inMods toGeckoEvent:(nsInputEvent*)outGeckoEvent
{
  outGeckoEvent->message = inMsg;
  outGeckoEvent->widget = [self widget];
  outGeckoEvent->nativeMsg = nsnull;
  outGeckoEvent->time = PR_IntervalNow();
  
  if (outGeckoEvent->eventStructType != NS_KEY_EVENT) {
    NSPoint mouseLoc = inPoint;
    
    // convert point to view coordinate system
    NSPoint localPoint = [self convertPoint:mouseLoc fromView:nil];
    outGeckoEvent->refPoint.x = outGeckoEvent->point.x = NS_STATIC_CAST(nscoord, localPoint.x);
    outGeckoEvent->refPoint.y = outGeckoEvent->point.y = NS_STATIC_CAST(nscoord, localPoint.y);

    if (outGeckoEvent->message == NS_MOUSE_SCROLL) {
      outGeckoEvent->refPoint.x = outGeckoEvent->refPoint.y = 0;
      outGeckoEvent->point.x = outGeckoEvent->point.y = 0;
    }
  }
  
  // set up modifier keys
  outGeckoEvent->isShift = ((inMods & NSShiftKeyMask) != 0);
  outGeckoEvent->isControl = ((inMods & NSControlKeyMask) != 0);
  outGeckoEvent->isAlt = ((inMods & NSAlternateKeyMask) != 0);
  outGeckoEvent->isMeta = ((inMods & NSCommandKeyMask) != 0);
}

 
//
// -widget
//
// return our gecko child view widget. Note this does not AddRef.
//
- (nsIWidget*) widget
{
  return NS_STATIC_CAST(nsIWidget*, mGeckoChild);
}

static void convertCocoaEventToMacEvent(NSEvent* cocoaEvent, EventRecord& macEvent)
{
    // XXX Revisit this fast and dirty conversion!
    macEvent.what = ([cocoaEvent type] == NSKeyDown ? keyDown : keyUp);
    UInt32 charCode = [[cocoaEvent characters] characterAtIndex: 0];
    if (charCode >= 0x0080) {
        switch (charCode) {
        case NSUpArrowFunctionKey:
            charCode = 0x0000001E;
            break;
        case NSDownArrowFunctionKey:
            charCode = 0x0000001F;
            break;
        case NSLeftArrowFunctionKey:
            charCode = 0x0000001C;
            break;
        case NSRightArrowFunctionKey:
            charCode = 0x0000001D;
            break;
        default:
            printf("### FIX ME - Convert NSString to C string with current encoding... ###\n");
            break;
        }
    }
    macEvent.message = (charCode & 0x00FF) | ([cocoaEvent keyCode] << 8);
    macEvent.when = ::TickCount();
    GetGlobalMouse(&macEvent.where);
    macEvent.modifiers = GetCurrentKeyModifiers();
}

- (void)keyDown:(NSEvent*)theEvent;
{
  PRBool isChar = PR_FALSE;
  BOOL isARepeat = [theEvent isARepeat];
  if (!isARepeat) {
    // Fire a key down.
    nsKeyEvent geckoEvent;
    geckoEvent.point.x = geckoEvent.point.y = 0;
    [self convert: theEvent message: NS_KEY_DOWN
          isChar: &isChar
          toGeckoEvent: &geckoEvent];
    geckoEvent.isChar = isChar;

    // As an optimisation, only do this when there is a plugin present.
    EventRecord macEvent;
    convertCocoaEventToMacEvent(theEvent, macEvent);
    geckoEvent.nativeMsg = &macEvent;

    // fprintf(stdout, "keyDown/autoKey received: message = 0x%08X\n", macEvent.message);

    mGeckoChild->DispatchWindowEvent(geckoEvent);
  }
  
  // Check to see if we are still the first responder.
  // The key down event may have shifted the focus, in which
  // case we should not fire the key press.
  NSResponder* resp = [[self window] firstResponder];
  if (resp != (NSResponder*)self) {
    printf("No soup for you!\n");
    return;
  }
  
  // Fire a key press.
  nsKeyEvent geckoEvent;
  geckoEvent.point.x = geckoEvent.point.y = 0;
  isChar = PR_FALSE;
  [self convert: theEvent message: NS_KEY_PRESS
        isChar: &isChar
        toGeckoEvent: &geckoEvent];
  geckoEvent.isChar = isChar;
  if (isChar) {
    // Get the chars of the event and loop over our list
    // of chars, setting each one.
    NSString* text = [theEvent characters];
    PRUint32 length = [text length];
    for (PRUint32 i = 0; i < length; i++) {
      unichar c = [text characterAtIndex: i];
      geckoEvent.charCode = c;
      mGeckoChild->DispatchWindowEvent(geckoEvent);
    }
  }
  else
    mGeckoChild->DispatchWindowEvent(geckoEvent);
}

- (void)keyUp:(NSEvent*)theEvent;
{
  // Fire a key up.
  nsKeyEvent geckoEvent;
  geckoEvent.point.x = geckoEvent.point.y = 0;
  PRBool isChar;
  [self convert: theEvent message: NS_KEY_UP
        isChar: &isChar
        toGeckoEvent: &geckoEvent];

  // As an optimisation, only do this when there is a plugin present.
  EventRecord macEvent;
  convertCocoaEventToMacEvent(theEvent, macEvent);
  geckoEvent.nativeMsg = &macEvent;
  
  mGeckoChild->DispatchWindowEvent(geckoEvent);
}

// This method is called when we are about to be focused.
- (BOOL)becomeFirstResponder
{
  nsFocusEvent event;
  event.eventStructType = NS_FOCUS_EVENT;
  event.nativeMsg = nsnull;
  event.message = NS_GOTFOCUS;
  event.widget = mGeckoChild;

  //focus and blur event should go to their base widget loc
  event.point.x = 0;
  event.point.y = 0;

  mGeckoChild->DispatchWindowEvent(event);
  return [super becomeFirstResponder];
}

// This method is called when are are about to lose focus.
- (BOOL)resignFirstResponder
{
  nsFocusEvent event;
  event.eventStructType = NS_FOCUS_EVENT;
  event.nativeMsg = nsnull;
  event.message = NS_LOSTFOCUS;
  event.widget = mGeckoChild;

  //focus and blur event should go to their base widget loc
  event.point.x = 0;
  event.point.y = 0;

  mGeckoChild->DispatchWindowEvent(event);

  return [super resignFirstResponder];
}

//-------------------------------------------------------------------------
//
// ConvertMacToRaptorKeyCode
//
//-------------------------------------------------------------------------


// Key code constants
enum
{
	kEscapeKeyCode			= 0x35,
	kCommandKeyCode     = 0x37,
	kShiftKeyCode				= 0x38,
	kCapsLockKeyCode		= 0x39,
	kControlKeyCode			= 0x3B,
	kOptionkeyCode			= 0x3A,		// left and right option keys
	kClearKeyCode				= 0x47,
	
	// function keys
	kF1KeyCode					= 0x7A,
	kF2KeyCode					= 0x78,
	kF3KeyCode					= 0x63,
	kF4KeyCode					= 0x76,
	kF5KeyCode					= 0x60,
	kF6KeyCode					= 0x61,
	kF7KeyCode					= 0x62,
	kF8KeyCode					= 0x64,
	kF9KeyCode					= 0x65,
	kF10KeyCode					= 0x6D,
	kF11KeyCode					= 0x67,
	kF12KeyCode					= 0x6F,
	kF13KeyCode					= 0x69,
	kF14KeyCode					= 0x6B,
	kF15KeyCode					= 0x71,
	
	kPrintScreenKeyCode	= kF13KeyCode,
	kScrollLockKeyCode	= kF14KeyCode,
	kPauseKeyCode				= kF15KeyCode,
	
	// keypad
	kKeypad0KeyCode			= 0x52,
	kKeypad1KeyCode			= 0x53,
	kKeypad2KeyCode			= 0x54,
	kKeypad3KeyCode			= 0x55,
	kKeypad4KeyCode			= 0x56,
	kKeypad5KeyCode			= 0x57,
	kKeypad6KeyCode			= 0x58,
	kKeypad7KeyCode			= 0x59,
	kKeypad8KeyCode			= 0x5B,
	kKeypad9KeyCode			= 0x5C,
	
	kKeypadMultiplyKeyCode	= 0x43,
	kKeypadAddKeyCode				= 0x45,
	kKeypadSubtractKeyCode	= 0x4E,
	kKeypadDecimalKeyCode		= 0x41,
	kKeypadDivideKeyCode		= 0x4B,
	kKeypadEqualsKeyCode		= 0x51,			// no correpsonding raptor key code
	kEnterKeyCode           = 0x4C,
	kReturnKeyCode          = 0x24,
	kPowerbookEnterKeyCode  = 0x34,     // Enter on Powerbook's keyboard is different
	
	kInsertKeyCode					= 0x72,				// also help key
	kDeleteKeyCode					= 0x75,				// also forward delete key
	kTabKeyCode							= 0x30,
	kBackspaceKeyCode       = 0x33,
	kHomeKeyCode						= 0x73,	
	kEndKeyCode							= 0x77,
	kPageUpKeyCode					= 0x74,
	kPageDownKeyCode				= 0x79,
	kLeftArrowKeyCode				= 0x7B,
	kRightArrowKeyCode			= 0x7C,
	kUpArrowKeyCode					= 0x7E,
	kDownArrowKeyCode				= 0x7D
	
};

static PRUint32 ConvertMacToRaptorKeyCode(UInt32 keyCode, nsKeyEvent* aKeyEvent, NSString* characters)
{
	PRUint32	raptorKeyCode = 0;
	PRUint8 charCode = [characters characterAtIndex: 0];
    
	switch (keyCode)
	{
//	case ??							:				raptorKeyCode = NS_VK_CANCEL;		break;			// don't know what this key means. Nor does joki

// modifiers. We don't get separate events for these
		case kEscapeKeyCode:				raptorKeyCode = NS_VK_ESCAPE;					break;
		case kShiftKeyCode:					raptorKeyCode = NS_VK_SHIFT;					break;
//		case kCommandKeyCode:       raptorKeyCode = NS_VK_META;           break;
		case kCapsLockKeyCode:			raptorKeyCode = NS_VK_CAPS_LOCK;			break;
		case kControlKeyCode:				raptorKeyCode = NS_VK_CONTROL;				break;
		case kOptionkeyCode:				raptorKeyCode = NS_VK_ALT;						break;
		case kClearKeyCode:					raptorKeyCode = NS_VK_CLEAR;					break;

// function keys
		case kF1KeyCode:						raptorKeyCode = NS_VK_F1;							break;
		case kF2KeyCode:						raptorKeyCode = NS_VK_F2;							break;
		case kF3KeyCode:						raptorKeyCode = NS_VK_F3;							break;
		case kF4KeyCode:						raptorKeyCode = NS_VK_F4;							break;
		case kF5KeyCode:						raptorKeyCode = NS_VK_F5;							break;
		case kF6KeyCode:						raptorKeyCode = NS_VK_F6;							break;
		case kF7KeyCode:						raptorKeyCode = NS_VK_F7;							break;
		case kF8KeyCode:						raptorKeyCode = NS_VK_F8;							break;
		case kF9KeyCode:						raptorKeyCode = NS_VK_F9;							break;
		case kF10KeyCode:						raptorKeyCode = NS_VK_F10;						break;
		case kF11KeyCode:						raptorKeyCode = NS_VK_F11;						break;
		case kF12KeyCode:						raptorKeyCode = NS_VK_F12;						break;
//	case kF13KeyCode:						raptorKeyCode = NS_VK_F13;						break;		// clash with the 3 below
//	case kF14KeyCode:						raptorKeyCode = NS_VK_F14;						break;
//	case kF15KeyCode:						raptorKeyCode = NS_VK_F15;						break;
		case kPauseKeyCode:					raptorKeyCode = NS_VK_PAUSE;					break;
		case kScrollLockKeyCode:		raptorKeyCode = NS_VK_SCROLL_LOCK;		break;
		case kPrintScreenKeyCode:		raptorKeyCode = NS_VK_PRINTSCREEN;		break;
	
// keypad
		case kKeypad0KeyCode:				raptorKeyCode = NS_VK_NUMPAD0;				break;
		case kKeypad1KeyCode:				raptorKeyCode = NS_VK_NUMPAD1;				break;
		case kKeypad2KeyCode:				raptorKeyCode = NS_VK_NUMPAD2;				break;
		case kKeypad3KeyCode:				raptorKeyCode = NS_VK_NUMPAD3;				break;
		case kKeypad4KeyCode:				raptorKeyCode = NS_VK_NUMPAD4;				break;
		case kKeypad5KeyCode:				raptorKeyCode = NS_VK_NUMPAD5;				break;
		case kKeypad6KeyCode:				raptorKeyCode = NS_VK_NUMPAD6;				break;
		case kKeypad7KeyCode:				raptorKeyCode = NS_VK_NUMPAD7;				break;
		case kKeypad8KeyCode:				raptorKeyCode = NS_VK_NUMPAD8;				break;
		case kKeypad9KeyCode:				raptorKeyCode = NS_VK_NUMPAD9;				break;

		case kKeypadMultiplyKeyCode:	raptorKeyCode = NS_VK_MULTIPLY;			break;
		case kKeypadAddKeyCode:				raptorKeyCode = NS_VK_ADD;					break;
		case kKeypadSubtractKeyCode:	raptorKeyCode = NS_VK_SUBTRACT;			break;
		case kKeypadDecimalKeyCode:		raptorKeyCode = NS_VK_DECIMAL;			break;
		case kKeypadDivideKeyCode:		raptorKeyCode = NS_VK_DIVIDE;				break;
//	case ??								:				raptorKeyCode = NS_VK_SEPARATOR;		break;


// these may clash with forward delete and help
    case kInsertKeyCode:				raptorKeyCode = NS_VK_INSERT;					break;
    case kDeleteKeyCode:				raptorKeyCode = NS_VK_DELETE;					break;

    case kBackspaceKeyCode:     raptorKeyCode = NS_VK_BACK;           break;
    case kTabKeyCode:           raptorKeyCode = NS_VK_TAB;            break;
    case kHomeKeyCode:          raptorKeyCode = NS_VK_HOME;           break;
    case kEndKeyCode:           raptorKeyCode = NS_VK_END;            break;
		case kPageUpKeyCode:				raptorKeyCode = NS_VK_PAGE_UP;        break;
		case kPageDownKeyCode:			raptorKeyCode = NS_VK_PAGE_DOWN;      break;
		case kLeftArrowKeyCode:			raptorKeyCode = NS_VK_LEFT;           break;
		case kRightArrowKeyCode:		raptorKeyCode = NS_VK_RIGHT;          break;
		case kUpArrowKeyCode:				raptorKeyCode = NS_VK_UP;             break;
		case kDownArrowKeyCode:			raptorKeyCode = NS_VK_DOWN;           break;

		default:
				if (aKeyEvent->isControl)
				  charCode += 64;
	  	
				// if we haven't gotten the key code already, look at the char code
				switch (charCode)
				{
					case kReturnCharCode:				raptorKeyCode = NS_VK_RETURN;				break;
					case kEnterCharCode:				raptorKeyCode = NS_VK_RETURN;				break;			// fix me!
					case ' ':										raptorKeyCode = NS_VK_SPACE;				break;
					case ';':										raptorKeyCode = NS_VK_SEMICOLON;		break;
					case '=':										raptorKeyCode = NS_VK_EQUALS;				break;
					case ',':										raptorKeyCode = NS_VK_COMMA;				break;
					case '.':										raptorKeyCode = NS_VK_PERIOD;				break;
					case '/':										raptorKeyCode = NS_VK_SLASH;				break;
					case '`':										raptorKeyCode = NS_VK_BACK_QUOTE;		break;
					case '{':
					case '[':										raptorKeyCode = NS_VK_OPEN_BRACKET;	break;
					case '\\':									raptorKeyCode = NS_VK_BACK_SLASH;		break;
					case '}':
					case ']':										raptorKeyCode = NS_VK_CLOSE_BRACKET;	break;
					case '\'':
					case '"':										raptorKeyCode = NS_VK_QUOTE;				break;
					
					default:
						
						if (charCode >= '0' && charCode <= '9')		// numerals
						{
							raptorKeyCode = charCode;
						}
						else if (charCode >= 'a' && charCode <= 'z')		// lowercase
						{
							raptorKeyCode = toupper(charCode);
						}
						else if (charCode >= 'A' && charCode <= 'Z')		// uppercase
						{
							raptorKeyCode = charCode;
						}

						break;
				}
	}

	return raptorKeyCode;
}

static PRBool IsSpecialRaptorKey(UInt32 macKeyCode)
{
	PRBool	isSpecial;

	// 
	// this table is used to determine which keys are special and should not generate a charCode
	//	
	switch (macKeyCode)
	{
// modifiers. We don't get separate events for these
// yet
		case kEscapeKeyCode:				isSpecial = PR_TRUE; break;
		case kShiftKeyCode:					isSpecial = PR_TRUE; break;
		case kCommandKeyCode:       isSpecial = PR_TRUE; break;
		case kCapsLockKeyCode:			isSpecial = PR_TRUE; break;
		case kControlKeyCode:				isSpecial = PR_TRUE; break;
		case kOptionkeyCode:				isSpecial = PR_TRUE; break;
		case kClearKeyCode:					isSpecial = PR_TRUE; break;

// function keys
		case kF1KeyCode:					isSpecial = PR_TRUE; break;
		case kF2KeyCode:					isSpecial = PR_TRUE; break;
		case kF3KeyCode:					isSpecial = PR_TRUE; break;
		case kF4KeyCode:					isSpecial = PR_TRUE; break;
		case kF5KeyCode:					isSpecial = PR_TRUE; break;
		case kF6KeyCode:					isSpecial = PR_TRUE; break;
		case kF7KeyCode:					isSpecial = PR_TRUE; break;
		case kF8KeyCode:					isSpecial = PR_TRUE; break;
		case kF9KeyCode:					isSpecial = PR_TRUE; break;
		case kF10KeyCode:					isSpecial = PR_TRUE; break;
		case kF11KeyCode:					isSpecial = PR_TRUE; break;
		case kF12KeyCode:					isSpecial = PR_TRUE; break;
		case kPauseKeyCode:				isSpecial = PR_TRUE; break;
		case kScrollLockKeyCode:	isSpecial = PR_TRUE; break;
		case kPrintScreenKeyCode:	isSpecial = PR_TRUE; break;

		case kInsertKeyCode:        isSpecial = PR_TRUE; break;
		case kDeleteKeyCode:				isSpecial = PR_TRUE; break;
		case kTabKeyCode:						isSpecial = PR_TRUE; break;
		case kBackspaceKeyCode:     isSpecial = PR_TRUE; break;

		case kHomeKeyCode:					isSpecial = PR_TRUE; break;	
		case kEndKeyCode:						isSpecial = PR_TRUE; break;
		case kPageUpKeyCode:				isSpecial = PR_TRUE; break;
		case kPageDownKeyCode:			isSpecial = PR_TRUE; break;
		case kLeftArrowKeyCode:			isSpecial = PR_TRUE; break;
		case kRightArrowKeyCode:		isSpecial = PR_TRUE; break;
		case kUpArrowKeyCode:				isSpecial = PR_TRUE; break;
		case kDownArrowKeyCode:			isSpecial = PR_TRUE; break;
		case kReturnKeyCode:        isSpecial = PR_TRUE; break;
		case kEnterKeyCode:         isSpecial = PR_TRUE; break;
		case kPowerbookEnterKeyCode: isSpecial = PR_TRUE; break;

		default:							isSpecial = PR_FALSE; break;
	}
	return isSpecial;
}

- (void) convert:(NSEvent*)aKeyEvent message:(PRUint32)aMessage 
           isChar:(PRBool*)aIsChar
           toGeckoEvent:(nsKeyEvent*)outGeckoEvent
{
  outGeckoEvent->eventStructType = NS_KEY_EVENT;
  [self convert:aKeyEvent message:aMessage toGeckoEvent:outGeckoEvent];

  // Initialize the out boolean for whether or not we are using
  // charCodes to false.
  if (aIsChar)
    *aIsChar = PR_FALSE;
    
  // Check to see if the message is a key press that does not involve
  // one of our special key codes.
  if (aMessage == NS_KEY_PRESS && !IsSpecialRaptorKey([aKeyEvent keyCode])) 
  {
    if (!outGeckoEvent->isControl && !outGeckoEvent->isMeta)
      outGeckoEvent->isShift = outGeckoEvent->isControl = outGeckoEvent->isAlt = outGeckoEvent->isMeta = 0;
    
    // We're not a special key.
    outGeckoEvent->keyCode	= 0;
    if (aIsChar)
      *aIsChar =  PR_TRUE; 
  }
  else
  {
    outGeckoEvent->keyCode = ConvertMacToRaptorKeyCode([aKeyEvent keyCode], outGeckoEvent, [aKeyEvent characters]);
    outGeckoEvent->charCode = 0;
  } 
  
  if (aMessage == NS_KEY_PRESS && !outGeckoEvent->isMeta && outGeckoEvent->keyCode != NS_VK_PAGE_UP && 
      outGeckoEvent->keyCode != NS_VK_PAGE_DOWN)
    ::ObscureCursor();
}

@end
