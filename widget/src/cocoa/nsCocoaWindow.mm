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
 * Contributor(s): Josh Aas <josh@mozilla.com>
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

#include "nsCocoaWindow.h"

#include "nsIServiceManager.h"    // for drag and drop
#include "nsWidgetsCID.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsIDragSessionMac.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"
#include "nsGUIEvent.h"
#include "nsCarbonHelpers.h"
#include "nsGFXUtils.h"
#include "nsMacResources.h"
#include "nsIRollupListener.h"
#import  "nsChildView.h"

#include "nsIEventQueueService.h"

#include <CFString.h>

#include <Quickdraw.h>

// Define Class IDs -- i hate having to do this
static NS_DEFINE_CID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

// from MacHeaders.c
#ifndef topLeft
  #define topLeft(r) (((Point *) &(r))[0])
#endif
#ifndef botRight
  #define botRight(r) (((Point *) &(r))[1])
#endif

// externs defined in nsWindow.cpp
extern nsIRollupListener * gRollupListener;
extern nsIWidget         * gRollupWidget;

#define kWindowPositionSlop 20

NS_IMPL_ISUPPORTS_INHERITED0(nsCocoaWindow, Inherited)

//
// nsCocoaWindow constructor
//

nsCocoaWindow::nsCocoaWindow()
: mOffsetParent(nsnull)
, mIsDialog(PR_FALSE)
, mIsResizing(PR_FALSE)
, mWindowMadeHere(PR_FALSE)
, mWindow(nil)
{

}


//-------------------------------------------------------------------------
//
// nsCocoaWindow destructor
//
//-------------------------------------------------------------------------
nsCocoaWindow::~nsCocoaWindow()
{
  if (mWindow && mWindowMadeHere) {
    [mWindow autorelease];
    [mDelegate autorelease];
  }
}


//-------------------------------------------------------------------------
//
// Utility method for implementing both Create(nsIWidget ...) and
// Create(nsNativeWidget...)
//-------------------------------------------------------------------------

nsresult nsCocoaWindow::StandardCreate(nsIWidget *aParent,
                        const nsRect &aRect,
                        EVENT_CALLBACK aHandleEventFunction,
                        nsIDeviceContext *aContext,
                        nsIAppShell *aAppShell,
                        nsIToolkit *aToolkit,
                        nsWidgetInitData *aInitData,
                        nsNativeWidget aNativeParent)
{
  Inherited::BaseCreate ( aParent, aRect, aHandleEventFunction, aContext, aAppShell,
                            aToolkit, aInitData );
                            
  if (!aNativeParent || (aInitData && aInitData->mWindowType == eWindowType_popup)) {
    mOffsetParent = aParent;

    nsWindowType windowType = eWindowType_toplevel;
    if (aInitData) {
      mWindowType = aInitData->mWindowType;
      // if a toplevel window was requested without a titlebar, use a dialog windowproc
      if (aInitData->mWindowType == eWindowType_toplevel &&
        (aInitData->mBorderStyle == eBorderStyle_none ||
         aInitData->mBorderStyle != eBorderStyle_all && !(aInitData->mBorderStyle & eBorderStyle_title)))
        windowType = eWindowType_dialog;
    } 
    else {
      mWindowType = (mIsDialog ? eWindowType_dialog : eWindowType_toplevel);
    }
    
    // create the cocoa window
    NSRect rect;
    rect.origin.x = rect.origin.y = 1.0;
    rect.size.width = rect.size.height = 1.0;
    unsigned int features = NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask 
                              | NSResizableWindowMask;
    if (mWindowType == eWindowType_popup || mWindowType == eWindowType_invisible)
      features = 0;

    // XXXdwh Just don't make popup windows yet.  They mess up the world.
    if (mWindowType == eWindowType_popup)
      return NS_OK;

    mWindow = [[NSWindow alloc] initWithContentRect:rect styleMask:features 
                                backing:NSBackingStoreBuffered defer:NO];
    
    // Popups will receive a "close" message when an app terminates
    // that causes an extra release to occur.  Make sure popups
    // are set not to release when closed.
    if (features == 0)
      [mWindow setReleasedWhenClosed: NO];

    // create a quickdraw view as the toplevel content view of the window
    NSQuickDrawView* content = [[[NSQuickDrawView alloc] init] autorelease];
    [content setFrame:[[mWindow contentView] frame]];
    [mWindow setContentView:content];
    
    // register for mouse-moved events. The default is to ignore them for perf reasons.
    [mWindow setAcceptsMouseMovedEvents:YES];
    
    // setup our notification delegate. Note that setDelegate: does NOT retain.
    mDelegate = [[WindowDelegate alloc] initWithGeckoWindow:this];
    [mWindow setDelegate:mDelegate];
    
    mWindowMadeHere = PR_TRUE;    
  }
  
  return NS_OK;
}


//
// Create a nsCocoaWindow using a native window provided by the application
//
NS_IMETHODIMP nsCocoaWindow::Create(nsNativeWidget aNativeParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  return(StandardCreate(nsnull, aRect, aHandleEventFunction,
                          aContext, aAppShell, aToolkit, aInitData,
                            aNativeParent));
}


NS_IMETHODIMP nsCocoaWindow::Create(nsIWidget* aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  return(StandardCreate(aParent, aRect, aHandleEventFunction,
                        aContext, aAppShell, aToolkit, aInitData, nsnull));
}


void*
nsCocoaWindow::GetNativeData(PRUint32 aDataType)
{
  void* retVal = nsnull;
  
  switch (aDataType) {
    // to emulate how windows works, we always have to return a NSView
    // for NS_NATIVE_WIDGET
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_DISPLAY:
      retVal = [mWindow contentView];
      break;
      
    case NS_NATIVE_WINDOW:  
      retVal = mWindow;
      break;
      
    case NS_NATIVE_GRAPHIC: // quickdraw port of top view (for now)
      retVal = [[mWindow contentView] qdPort];
      break;
  }

  return retVal;
}

NS_IMETHODIMP
nsCocoaWindow::IsVisible(PRBool & aState)
{
  aState = mVisible;
  return NS_OK;
}
   
//-------------------------------------------------------------------------
//
// Hide or show this window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsCocoaWindow::Show(PRBool bState)
{
  if (bState)
    [mWindow orderFront:NULL];
  else
    [mWindow orderOut:NULL];

  mVisible = bState;

  return NS_OK;
}


NS_IMETHODIMP nsCocoaWindow::Enable(PRBool aState)
{
  return NS_OK;
}


NS_IMETHODIMP nsCocoaWindow::IsEnabled(PRBool *aState)
{
  if (aState)
    *aState = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::ConstrainPosition(PRBool aAllowSlop,
                                               PRInt32 *aX, PRInt32 *aY)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Move this window
//
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
// Move
//-------------------------------------------------------------------------
NS_IMETHODIMP nsCocoaWindow::Move(PRInt32 aX, PRInt32 aY)
{  
  if (mWindow) {  
    // if we're a popup, we have to convert from our parent widget's coord
    // system to the global coord system first because the (x,y) we're given
    // is in its coordinate system.
    if ( mWindowType == eWindowType_popup ) {
      nsRect localRect, globalRect; 
      localRect.x = aX;
      localRect.y = aY;  
      if (mOffsetParent) {
        mOffsetParent->WidgetToScreen(localRect,globalRect);
        aX=globalRect.x;
        aY=globalRect.y;
     }
    }
    
    NSPoint coord = {aX, aY};
 
    // the point we have assumes that the screen origin is the top-left. Well,
    // it's not. Use the screen object to convert.
    //FIXME -- doesn't work on monitors other than primary
    NSRect screenRect = [[NSScreen mainScreen] frame];
    coord.y = (screenRect.origin.y + screenRect.size.height) - coord.y;
    //printf("final coords %f %f\n", coord.x, coord.y);
    //printf("- window coords before %f %f\n", [mWindow frame].origin.x, [mWindow frame].origin.y);
    [mWindow setFrameTopLeftPoint:coord];
    //printf("- window coords after %f %f\n", [mWindow frame].origin.x, [mWindow frame].origin.y);
  }
  
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Position the window behind the given window
//
//-------------------------------------------------------------------------
NS_METHOD nsCocoaWindow::PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                     nsIWidget *aWidget, PRBool aActivate)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// zoom/restore
//
//-------------------------------------------------------------------------
NS_METHOD nsCocoaWindow::SetSizeMode(PRInt32 aMode)
{
  return NS_OK;
}


void nsCocoaWindow::CalculateAndSetZoomedSize()
{
  
} // CalculateAndSetZoomedSize


//-------------------------------------------------------------------------
//
// Resize this window to a point given in global (screen) coordinates. This
// differs from simple Move(): that method makes JavaScript place windows
// like other browsers: it puts the top-left corner of the outer edge of the
// window border at the given coordinates, offset from the menubar.
// MoveToGlobalPoint expects the top-left corner of the portrect, which
// is inside the border, and is not offset by the menubar height.
//
//-------------------------------------------------------------------------
void nsCocoaWindow::MoveToGlobalPoint(PRInt32 aX, PRInt32 aY)
{

}


NS_IMETHODIMP nsCocoaWindow::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  Move(aX, aY);
  Resize(aWidth, aHeight, aRepaint);
  return NS_OK;
}


//
// Resize this window
//
NS_IMETHODIMP nsCocoaWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  if (mWindow) {
    NSRect newBounds = [mWindow frame];
    newBounds.size.width = aWidth;
    if (mWindowType == eWindowType_popup)
      newBounds.size.height = aHeight;
    else
      newBounds.size.height = aHeight + kTitleBarHeight;     // add height of title bar
    StartResizing();
    [mWindow setFrame:newBounds display:NO];
    StopResizing();
  }

  mBounds.width  = aWidth;
  mBounds.height = aHeight;
  
  // tell gecko to update all the child widgets
  ReportSizeEvent();
  
  // Inherited::Resize(aWidth, aHeight, aRepaint);
  return NS_OK;
}


NS_IMETHODIMP nsCocoaWindow::GetScreenBounds(nsRect &aRect)
{
  return NS_OK;
}


PRBool nsCocoaWindow::OnPaint(nsPaintEvent &event)
{
  return PR_TRUE; // don't dispatch the update event
}

//
// Set this window's title
//
NS_IMETHODIMP nsCocoaWindow::SetTitle(const nsAString& aTitle)
{
  const nsString& strTitle = PromiseFlatString(aTitle);
  NSString* title = [NSString stringWithCharacters:strTitle.get() length:strTitle.Length()];
  [mWindow setTitle:title];

  return NS_OK;
}


//-------------------------------------------------------------------------
// Pass notification of some drag event to Gecko
//
// The drag manager has let us know that something related to a drag has
// occurred in this window. It could be any number of things, ranging from 
// a drop, to a drag enter/leave, or a drag over event. The actual event
// is passed in |aMessage| and is passed along to our event hanlder so Gecko
// knows about it.
//-------------------------------------------------------------------------
PRBool nsCocoaWindow::DragEvent ( unsigned int aMessage, Point aMouseGlobal, UInt16 aKeyModifiers )
{
  return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// Like ::BringToFront, but constrains the window to its z-level
//
//-------------------------------------------------------------------------
void nsCocoaWindow::ComeToFront()
{

}


NS_IMETHODIMP nsCocoaWindow::ResetInputState()
{
// return mMacEventHandler->ResetInputState();
  return NS_OK;
}


void nsCocoaWindow::SetIsActive(PRBool aActive)
{
// mIsActive = aActive;
}


void nsCocoaWindow::IsActive(PRBool* aActive)
{
// *aActive = mIsActive;
}


//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------
NS_IMETHODIMP 
nsCocoaWindow::DispatchEvent(nsGUIEvent* event, nsEventStatus& aStatus)
{
  aStatus = nsEventStatus_eIgnore;

  nsIWidget* aWidget = event->widget;
  NS_IF_ADDREF(aWidget);
  
  if (nsnull != mMenuListener){
    if(NS_MENU_EVENT == event->eventStructType)
      aStatus = mMenuListener->MenuSelected(static_cast<nsMenuEvent&>(*event));
  }
  if (mEventCallback)
    aStatus = (*mEventCallback)(event);

  // Dispatch to event listener if event was not consumed
  if ((aStatus != nsEventStatus_eConsumeNoDefault) && (mEventListener != nsnull))
    aStatus = mEventListener->ProcessEvent(*event);

  NS_IF_RELEASE(aWidget);

  return NS_OK;
}


void
nsCocoaWindow::ReportSizeEvent()
{
  // nsEvent
  nsSizeEvent sizeEvent(PR_TRUE, NS_SIZE, this);
  sizeEvent.time = PR_IntervalNow();

  // nsSizeEvent
  sizeEvent.windowSize = &mBounds;
  sizeEvent.mWinWidth  = mBounds.width;
  sizeEvent.mWinHeight = mBounds.height;
  
  // dispatch event
  nsEventStatus status = nsEventStatus_eIgnore;
  DispatchEvent(&sizeEvent, status);
}


NS_IMETHODIMP nsCocoaWindow::SetMenuBar(nsIMenuBar *aMenuBar)
{
  if (mMenuBar)
    mMenuBar->SetParent(nsnull);
  mMenuBar = aMenuBar;
  return NS_OK;
}


NS_IMETHODIMP nsCocoaWindow::ShowMenuBar(PRBool aShow)
{
  return NS_ERROR_FAILURE;
}


nsIMenuBar* nsCocoaWindow::GetMenuBar()
{
  return mMenuBar;
}


@implementation WindowDelegate


- (id)initWithGeckoWindow:(nsCocoaWindow*)geckoWind
{
  [super init];
  mGeckoWindow = geckoWind;
  return self;
}

- (void)windowDidResize:(NSNotification *)aNotification
{
  if (!mGeckoWindow->IsResizing()) {
    // must remember to give Gecko top-left, not straight cocoa origin
    // and that Gecko already compensates for the title bar, so we have to
    // strip it out here.
    NSRect frameRect = [[aNotification object] frame];
    mGeckoWindow->Resize (NS_STATIC_CAST(PRInt32,frameRect.size.width),
                          NS_STATIC_CAST(PRInt32,frameRect.size.height - nsCocoaWindow::kTitleBarHeight), PR_TRUE);
  }
}


- (void)windowDidBecomeMain:(NSNotification *)aNotification
{
  nsIMenuBar* myMenuBar = mGeckoWindow->GetMenuBar();
  if (myMenuBar)
    myMenuBar->Paint();
}


- (void)windowDidResignMain:(NSNotification *)aNotification
{
  //printf(@"got deactivate");
}


- (void)windowDidBecomeKey:(NSNotification *)aNotification
{
  //printf("we're key window\n");
}


- (void)windowDidResignKey:(NSNotification *)aNotification
{
  //printf("we're not the key window\n");
}


- (void)windowDidMove:(NSNotification *)aNotification
{
}


@end
