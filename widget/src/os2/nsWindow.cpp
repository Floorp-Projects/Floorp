/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is
 * John Fairhurst, <john_fairhurst@iname.com>.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   IBM Corp.
 *   Rich Walsh <dragtext@e-vertise.com>
 *   Dan Rosen <dr@netscape.com>
 *   Dainis Jonitis <Dainis_Jonitis@swh-t.lv>
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

#include "nsWindow.h"
#include "nsIAppShell.h"
#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIRenderingContextOS2.h"
#include "nsIDeviceContext.h"
#include "nsIScreenManager.h"
#include "nsRect.h"
#include "nsTransform2D.h"
#include "nsGfxCIID.h"
#include "prtime.h"
// #include "nsTooltipManager.h"
#include "nsISupportsArray.h"
#include "nsIMenuBar.h"
//#include "nsIMenuItem.h"
#include "nsHashtable.h"
//#include "nsMenu.h"
#include "nsDragService.h"
#include "nsILocalFile.h"
#include "nsNetUtil.h"

#include "nsIRollupListener.h"
#include "nsIMenuRollup.h"
#include "nsIRegion.h"
#include "nsIPref.h"

//~~~ windowless plugin support
#include "nsplugindefs.h"

#include "nsITimer.h"
#include "nsIServiceManager.h"

// For SetIcon
#include "nsAppDirectoryServiceDefs.h"
#include "nsXPIDLString.h"
#include "nsIFile.h"

#include "nsISupportsPrimitives.h"

#include "nsOS2Uni.h"
#include "nsPaletteOS2.h"

#include <stdlib.h>
#include <ctype.h>

#include "nsdefs.h"
#include "wdgtos2rc.h"

#ifdef DEBUG_sobotka
static int WINDOWCOUNT = 0;
#endif

static const char *sScreenManagerContractID = "@mozilla.org/gfx/screenmanager;1";

// The subclass proc (fnwpNSWindow) calls ProcessMessage() in the object.
// Decisions are taken here about what to do - the purpose of the OnFoo()
// methods is to generate an NS event to the various people who are
// listening, or not.
//
// OS/2 things: remember supplied coords are in the XP space.  There are
// NS2PM methods for conversion of points & rectangles; position is a bit
// different in that it's the *parent* window whose height must be used.
//
// Deferred window positioning is emulated using WinSetMultWindowPos in
// the hopes that there was a good reason for adding it to nsIWidget.
//
// SetColorSpace() is not implemented on purpose.  So there.
//
// John Fairhurst 17-09-98 first version
//        Revised 01-12-98 to inherit from nsBaseWidget.
//        Revised 24-01-99 to use hashtable
//        Revised 15-03-99 for new menu classes
//        Revised 05-06-99 to use pres-params
//        Revised 19-06-99 drag'n'drop, etc.


nsWindow* nsWindow::gCurrentWindow = nsnull;
BOOL nsWindow::sIsRegistered       = FALSE;

////////////////////////////////////////////////////
// Rollup Listener - global variable defintions
////////////////////////////////////////////////////
nsIRollupListener * gRollupListener           = nsnull;
nsIWidget         * gRollupWidget             = nsnull;
PRBool              gRollupConsumeRollupEvent = PR_FALSE;
////////////////////////////////////////////////////

PRBool gJustGotActivate = PR_FALSE;
PRBool gJustGotDeactivate = PR_FALSE;

////////////////////////////////////////////////////
// Mouse Clicks - static variable defintions 
// for figuring out 1 - 3 Clicks
////////////////////////////////////////////////////
static POINTS gLastButton1Down = {0,0};

#define XFROMMP(m)    (SHORT(LOUSHORT(m)))
#define YFROMMP(m)    (SHORT(HIUSHORT(m)))
 ////////////////////////////////////////////////////

static PRBool gGlobalsInitialized = PR_FALSE;
static HPOINTER gPtrArray[IDC_COUNT];
static PRBool gIsTrackPoint = PR_FALSE;
static PRBool gIsDBCS = PR_FALSE;

// The last user input event time in milliseconds. If there are any pending
// native toolkit input events it returns the current time. The value is
// compatible with PR_IntervalToMicroseconds(PR_IntervalNow()).
static PRUint32 gLastInputEventTime = 0;

#ifdef DEBUG_FOCUS
static int currentWindowIdentifier = 0;
#endif

//-------------------------------------------------------------------------
// Drag and Drop flags and global data
// see the D&D section toward the end of this file for additional info

// actions that might cause problems during d&d
#define ACTION_PAINT    1
#define ACTION_DRAW     2
#define ACTION_SCROLL   3
#define ACTION_SHOW     4
#define ACTION_PTRPOS   5

// shorten these references a bit
#define DND_None                (nsIDragSessionOS2::DND_NONE)               
#define DND_NativeDrag          (nsIDragSessionOS2::DND_NATIVEDRAG)         
#define DND_MozDrag             (nsIDragSessionOS2::DND_MOZDRAG)            
#define DND_InDrop              (nsIDragSessionOS2::DND_INDROP)             
#define DND_DragStatus          (nsIDragSessionOS2::DND_DRAGSTATUS)         
#define DND_DispatchEnterEvent  (nsIDragSessionOS2::DND_DISPATCHENTEREVENT) 
#define DND_DispatchEvent       (nsIDragSessionOS2::DND_DISPATCHEVENT)      
#define DND_GetDragoverResult   (nsIDragSessionOS2::DND_GETDRAGOVERRESULT)  
#define DND_ExitSession         (nsIDragSessionOS2::DND_EXITSESSION)        

// set when any nsWindow is being dragged over
static PRUint32  gDragStatus = 0;


//
// App Command messages for IntelliMouse and Natural Keyboard Pro
//
#define WM_APPCOMMAND  0x0319

#define APPCOMMAND_BROWSER_BACKWARD       1
#define APPCOMMAND_BROWSER_FORWARD        2
#define APPCOMMAND_BROWSER_REFRESH        3
#define APPCOMMAND_BROWSER_STOP           4

#define FAPPCOMMAND_MASK  0xF000
#define GET_APPCOMMAND_LPARAM(lParam) ((USHORT)(HIUSHORT(lParam) & ~FAPPCOMMAND_MASK))

//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow() : nsBaseWidget()
{
    mWnd                = 0;
    mFrameWnd           = 0;
    mPrevWndProc        = NULL;
    mParent             = 0;
    mNextID             = 1;
    mSWPs               = 0;
    mlHave              = 0;
    mlUsed              = 0;
    mFrameIcon          = 0;
    mDeadKey            = 0;
    mHaveDeadKey        = FALSE;
    mIsDestroying       = PR_FALSE;
    mOnDestroyCalled    = PR_FALSE;

    mPreferredWidth     = 0;
    mPreferredHeight    = 0;
    mWindowState        = nsWindowState_ePrecreate;
    mWindowType         = eWindowType_child;
    mBorderStyle        = eBorderStyle_default;
    mFont               = nsnull;
    mOS2Toolkit         = nsnull;
    mIsScrollBar         = FALSE;
    mInSetFocus         = FALSE;
    mChromeHidden       = FALSE;
    mDragHps            = 0;
    mDragStatus         = 0;

    mIsTopWidgetWindow = PR_FALSE;

    if (!gGlobalsInitialized) {
      gGlobalsInitialized = PR_TRUE;
      HMODULE hModResources = NULLHANDLE;
      DosQueryModFromEIP(&hModResources, NULL, 0, NULL, NULL, (ULONG) &gGlobalsInitialized);
      for (int i = 0; i < IDC_COUNT; i++) {
        gPtrArray[i] = ::WinLoadPointer(HWND_DESKTOP, hModResources, IDC_BASE+i);
      }

      // Work out if the system is DBCS
      char buffer[CCHMAXPATH];
      COUNTRYCODE cc = { 0 };
      DosQueryDBCSEnv( CCHMAXPATH, &cc, buffer);
      gIsDBCS = buffer[0] || buffer[1];

      // This is ugly. The Thinkpad TrackPoint driver checks to see whether or not a window
      // actually has a scroll bar as a child before sending it scroll messages. Needless to
      // say, no Mozilla window has real scroll bars. So if you have the "os2.trackpoint"
      // preference set, we put an invisible scroll bar on every child window so we can
      // scroll. Woohoo!
      nsresult rv;
      nsCOMPtr<nsIPref> prefs(do_GetService(NS_PREF_CONTRACTID, &rv));
      if (NS_SUCCEEDED(rv) && prefs)
         prefs->GetBoolPref("os2.trackpoint", &gIsTrackPoint);
    }
}

//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
  // How destruction works: A call of Destroy() destroys the PM window.  This
  // triggers an OnDestroy(), which frees resources.  If not Destroy'd at
  // delete time, Destroy() gets called anyway.
  
  // NOTE: Calling virtual functions from destructors is bad; they always
  //       bind in the current object (ie. as if they weren't virtual).  It
  //       may even be illegal to call them from here.
  //
  mIsDestroying = PR_TRUE;
  if (gCurrentWindow == this) {
    gCurrentWindow = nsnull;
  }
  if (mFrameIcon) {
     WinFreeFileIcon(mFrameIcon);
     mFrameIcon = NULLHANDLE;
  }

  // If the widget was released without calling Destroy() then the native
  // window still exists, and we need to destroy it
  if( !(mWindowState & nsWindowState_eDead) )
  {
    mWindowState |= nsWindowState_eDoingDelete;
    mWindowState &= ~(nsWindowState_eLive|nsWindowState_ePrecreate|
                      nsWindowState_eInCreate);
//    if( mWnd)
      Destroy();
  }

}

/* static */ void
nsWindow::ReleaseGlobals()
{
  for (int i = 0; i < IDC_COUNT; i++) {
    WinDestroyPointer(gPtrArray[i]);
  }
}

NS_METHOD nsWindow::CaptureMouse(PRBool aCapture)
{
  if (PR_TRUE == aCapture) { 
    WinSetCapture( HWND_DESKTOP, mWnd);
  } else {
    WinSetCapture( HWND_DESKTOP, NULLHANDLE);
  }
//  mIsInMouseCapture = aCapture;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Deferred Window positioning
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::BeginResizingChildren(void)
{
   if( !mSWPs)
   {
      mlHave = 10;
      mlUsed = 0;
      mSWPs = (PSWP) malloc( 10 * sizeof( SWP));
   }
   return NS_OK;
}

void nsWindow::DeferPosition( HWND hwnd, HWND hwndInsertBehind,
                              long x, long y, long cx, long cy, ULONG flags)
{
   if( mSWPs)
   {
      if( mlHave == mlUsed) // need more swps
      {
         mlHave += 10;
         mSWPs = (PSWP) realloc( mSWPs, mlHave * sizeof( SWP));
      }
      mSWPs[ mlUsed].hwnd = hwnd;
      mSWPs[ mlUsed].hwndInsertBehind = hwndInsertBehind;
      mSWPs[ mlUsed].x = x;
      mSWPs[ mlUsed].y = y;
      mSWPs[ mlUsed].cx = cx;
      mSWPs[ mlUsed].cy = cy;
      mSWPs[ mlUsed].fl = flags;
      mSWPs[ mlUsed].ulReserved1 = 0;
      mSWPs[ mlUsed].ulReserved2 = 0;
      mlUsed++;
   }
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
   if( nsnull != mSWPs)
   {
      WinSetMultWindowPos( 0/*hab*/, mSWPs, mlUsed);
      free( mSWPs);
      mSWPs = nsnull;
      mlUsed = mlHave = 0;
   }
   return NS_OK;
}

NS_METHOD nsWindow::WidgetToScreen(const nsRect &aOldRect, nsRect &aNewRect)
{
  POINTL point = { aOldRect.x, aOldRect.y };
  NS2PM( point);

  WinMapWindowPoints( mWnd, HWND_DESKTOP, &point, 1);

  aNewRect.x = point.x;
  aNewRect.y = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN) - point.y - 1;
  aNewRect.width = aOldRect.width;
  aNewRect.height = aOldRect.height;
  return NS_OK;
}

NS_METHOD nsWindow::ScreenToWidget( const nsRect &aOldRect, nsRect &aNewRect)
{
  POINTL point = { aOldRect.x,
                   WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN) - aOldRect.y - 1 };
  WinMapWindowPoints( HWND_DESKTOP, mWnd, &point, 1);

  PM2NS( point);

  aNewRect.x = point.x;
  aNewRect.y = point.y;
  aNewRect.width = aOldRect.width;
  aNewRect.height = aOldRect.height;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Convert nsEventStatus value to a windows boolean
//
//-------------------------------------------------------------------------

PRBool nsWindow::ConvertStatus(nsEventStatus aStatus)
{
  switch(aStatus) {
  case nsEventStatus_eIgnore:
    return PR_FALSE;
  case nsEventStatus_eConsumeNoDefault:
    return PR_TRUE;
  case nsEventStatus_eConsumeDoDefault:
    return PR_FALSE;
  default:
    NS_ASSERTION(0, "Illegal nsEventStatus enumeration value");
    break;
  }
  return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// Initialize an event to dispatch
//
//-------------------------------------------------------------------------
void nsWindow::InitEvent(nsGUIEvent& event, nsPoint* aPoint)
{
    NS_ADDREF(event.widget);

    if (nsnull == aPoint) {     // use the point from the event
      // for most events, get the message position;  for drag events,
      // msg position may be incorrect, so get the current position instead
      POINTL ptl;
      if (CheckDragStatus(ACTION_PTRPOS, 0))
        WinQueryPointerPos( HWND_DESKTOP, &ptl);
      else
        WinQueryMsgPos( 0/*hab*/, &ptl);
      WinMapWindowPoints( HWND_DESKTOP, mWnd, &ptl, 1);

#if 0
      printf("++++++++++nsWindow::InitEvent (!pt) mapped point = %ld, %ld\n", ptl.x, ptl.y);
#endif

      PM2NS( ptl);

      event.point.x = ptl.x;
      event.point.y = ptl.y;

#if 0
      printf("++++++++++nsWindow::InitEvent (!pt) converted point = %ld, %ld\n", ptl.x, ptl.y);
#endif
   }
   else
   {                     // use the point override if provided
      event.point.x = aPoint->x;
      event.point.y = aPoint->y;

#if 0
      printf("++++++++++nsWindow::InitEvent point = %ld, %ld\n", aPoint->x, aPoint->y);
#endif
   }

   event.time = WinQueryMsgTime( 0/*hab*/);

   /* OS2TODO
   mLastPoint.x = event.point.x;
   mLastPoint.y = event.point.y;
   */
}

//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWindow::DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus)
{
#if defined(TRACE_EVENTS) && defined(DEBUG_sobotka)
  DebugPrintEvent(*event, mWnd);
#endif

  aStatus = nsEventStatus_eIgnore;

  // Filters: if state is eInCreate, only send out NS_CREATE
  //          if state is eDoingDelete, don't send out anything because,
  //                                    well, the object's being deleted...
  if( ((mWindowState == nsWindowState_eInCreate) && event->message == NS_CREATE)
      || (mWindowState & nsWindowState_eLive) )
  {
    if (nsnull != mEventCallback) {
      aStatus = (*mEventCallback)( event);
    }
   
    // Dispatch to event listener if event was not consumed
    if ((aStatus != nsEventStatus_eIgnore) && (nsnull != mEventListener)) {
      aStatus = mEventListener->ProcessEvent(*event);
    }
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
PRBool nsWindow::DispatchWindowEvent(nsGUIEvent* event)
{
  nsEventStatus status;
  DispatchEvent(event, status);
  return ConvertStatus(status);
}

PRBool nsWindow::DispatchWindowEvent(nsGUIEvent*event, nsEventStatus &aStatus) {
  DispatchEvent(event, aStatus);
  return ConvertStatus(aStatus);
}

//-------------------------------------------------------------------------
//
// Dispatch standard event
//
//-------------------------------------------------------------------------

PRBool nsWindow::DispatchStandardEvent(PRUint32 aMsg)
{
  nsGUIEvent event(aMsg, this);
  InitEvent(event);

  PRBool result = DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);
  return result;
}

//-------------------------------------------------------------------------
//
// Dispatch app command event
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchAppCommandEvent(PRUint32 aEventCommand)
{
  nsAppCommandEvent event(NS_APPCOMMAND_START, this);

  InitEvent(event);
  event.appCommand = NS_APPCOMMAND_START + aEventCommand;

  DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::CaptureRollupEvents(nsIRollupListener * aListener, 
                                            PRBool aDoCapture, 
                                            PRBool aConsumeRollupEvent)
{
  if (aDoCapture) {
    /* we haven't bothered carrying a weak reference to gRollupWidget because
       we believe lifespan is properly scoped. this next assertion helps
       assure that remains true. */
    NS_ASSERTION(!gRollupWidget, "rollup widget reassigned before release");
    gRollupConsumeRollupEvent = aConsumeRollupEvent;
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

PRBool 
nsWindow::EventIsInsideWindow(nsWindow* aWindow) 
{
  RECTL r;
  POINTL mp;
  if (WinQueryMsgPos( 0/*hab*/, &mp)) {
    WinMapWindowPoints( HWND_DESKTOP, aWindow->mWnd, &mp, 1);
    WinQueryWindowRect( aWindow->mWnd, &r);
    // now make sure that it wasn't one of our children
    if (mp.x < r.xLeft || mp.x > r.xRight ||
       mp.y > r.yTop || mp.y < r.yBottom) {
      return PR_FALSE;
    }
  } 

  return PR_TRUE;
}


static PCSZ GetNSWindowPropName() {
  static ATOM atom = 0;

  // this is threadsafe, even without locking;
  // even if there's a race, GlobalAddAtom("nsWindowPtr")
  // will just return the same value
  if (!atom) {
    atom = WinAddAtom(WinQuerySystemAtomTable(), "nsWindowPtr");
  }
  return (PCSZ)atom;
}

nsWindow * nsWindow::GetNSWindowPtr(HWND aWnd) {
  return (nsWindow *) ::WinQueryProperty(aWnd, GetNSWindowPropName());
}

BOOL nsWindow::SetNSWindowPtr(HWND aWnd, nsWindow * ptr) {
  if (ptr == NULL) {
    ::WinRemoveProperty(aWnd, GetNSWindowPropName());
    return TRUE;
  } else {
    return ::WinSetProperty(aWnd, GetNSWindowPropName(), (PVOID)ptr, 0);
  }
}

//
// DealWithPopups
//
// Handle events that may cause a popup (combobox, XPMenu, etc) to need to rollup.
//
BOOL
nsWindow :: DealWithPopups ( ULONG inMsg, MRESULT* outResult )
{
  if ( gRollupListener && gRollupWidget) {
    if(inMsg == WM_BUTTON1DOWN ||
        inMsg == WM_BUTTON2DOWN || inMsg == WM_BUTTON3DOWN) {
      // Rollup if the event is outside the popup.
      PRBool rollup = !nsWindow::EventIsInsideWindow((nsWindow*)gRollupWidget);
      
      // If we're dealing with menus, we probably have submenus and we don't
      // want to rollup if the click is in a parent menu of the current submenu.
      if (rollup) {
        nsCOMPtr<nsIMenuRollup> menuRollup ( do_QueryInterface(gRollupListener) );
        if ( menuRollup ) {
          nsCOMPtr<nsISupportsArray> widgetChain;
          menuRollup->GetSubmenuWidgetChain ( getter_AddRefs(widgetChain) );
          if ( widgetChain ) {
            PRUint32 count = 0;
            widgetChain->Count(&count);
            for ( PRUint32 i = 0; i < count; ++i ) {
              nsCOMPtr<nsISupports> genericWidget;
              widgetChain->GetElementAt ( i, getter_AddRefs(genericWidget) );
              nsCOMPtr<nsIWidget> widget ( do_QueryInterface(genericWidget) );
              if ( widget ) {
                nsIWidget* temp = widget.get();
                if ( nsWindow::EventIsInsideWindow((nsWindow*)temp) ) {
                  rollup = PR_FALSE;
                  break;
                }
              }
            } // foreach parent menu widget
          }
        } // if rollup listener knows about menus
      }

      // if we've determined that we should still rollup everything, do it.
      if ( rollup ) {
        gRollupListener->Rollup();

        // return TRUE tells Windows that the event is consumed, 
        // false allows the event to be dispatched
        //
        // So if we are NOT supposed to be consuming events, let it go through
        if (gRollupConsumeRollupEvent) {
          *outResult = (MRESULT)TRUE;
          return TRUE;
        } 
      }
    } // if event that might trigger a popup to rollup    
  } // if rollup listeners registered

  return FALSE;

} // DealWithPopups



// Are both windows from this app?
BOOL bothFromSameWindow( HWND hwnd1, HWND hwnd2 )
{
  HWND hwnd1Chain = WinQueryWindow( hwnd1, QW_OWNER );
  if (!hwnd1Chain)
    hwnd1Chain = WinQueryWindow( hwnd1, QW_PARENT );
  HWND hwnd1GChain = WinQueryWindow( hwnd1Chain, QW_OWNER );
  if (!hwnd1GChain)
    hwnd1GChain = WinQueryWindow( hwnd1Chain, QW_PARENT );
  HWND hwnd2Chain = WinQueryWindow( hwnd2, QW_OWNER );
  if (!hwnd2Chain)
    hwnd2Chain = WinQueryWindow( hwnd2, QW_PARENT );
  HWND hwnd2GChain = WinQueryWindow( hwnd2Chain, QW_OWNER );
  if (!hwnd2GChain)
    hwnd2GChain = WinQueryWindow( hwnd2Chain, QW_PARENT );
  while( hwnd1GChain) {
    hwnd1 = hwnd1Chain;
    hwnd1Chain = hwnd1GChain;
    hwnd1GChain = WinQueryWindow( hwnd1Chain, QW_OWNER );
    if (!hwnd1GChain)
      hwnd1GChain = WinQueryWindow( hwnd1Chain, QW_PARENT );
  }
  while( hwnd2GChain) {
    hwnd2 = hwnd2Chain;
    hwnd2Chain = hwnd2GChain;
    hwnd2GChain = WinQueryWindow( hwnd2Chain, QW_OWNER );
    if (!hwnd2GChain)
      hwnd2GChain = WinQueryWindow( hwnd2Chain, QW_PARENT );
  }
  return (hwnd1 == hwnd2);
}

//-------------------------------------------------------------------------
//
// the nsWindow procedure for all nsWindows in this toolkit
//
//-------------------------------------------------------------------------
MRESULT EXPENTRY fnwpNSWindow( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   MRESULT popupHandlingResult;
   if( nsWindow::DealWithPopups(msg, &popupHandlingResult) )
      return popupHandlingResult;

   // Get the nsWindow for this hwnd
   nsWindow *wnd = nsWindow::GetNSWindowPtr(hwnd);

   // check to see if we have a rollup listener registered
   if( nsnull != gRollupListener && nsnull != gRollupWidget) {
      if( msg == WM_ACTIVATE || msg == WM_BUTTON1DOWN ||
          msg == WM_BUTTON2DOWN || msg == WM_BUTTON3DOWN) {
      // Rollup if the event is outside the popup.
      PRBool rollup = !nsWindow::EventIsInsideWindow((nsWindow*)gRollupWidget);
      
      // If we're dealing with menus, we probably have submenus and we don't
      // want to rollup if the click is in a parent menu of the current submenu.
      if (rollup) {
        nsCOMPtr<nsIMenuRollup> menuRollup ( do_QueryInterface(gRollupListener) );
        if ( menuRollup ) {
          nsCOMPtr<nsISupportsArray> widgetChain;
          menuRollup->GetSubmenuWidgetChain ( getter_AddRefs(widgetChain) );
          if ( widgetChain ) {
            PRUint32 count = 0;
            widgetChain->Count(&count);
            for ( PRUint32 i = 0; i < count; ++i ) {
              nsCOMPtr<nsISupports> genericWidget;
              widgetChain->GetElementAt ( i, getter_AddRefs(genericWidget) );
              nsCOMPtr<nsIWidget> widget ( do_QueryInterface(genericWidget) );
              if ( widget ) {
                nsIWidget* temp = widget.get();
                if ( nsWindow::EventIsInsideWindow((nsWindow*)temp) ) {
                  rollup = PR_FALSE;
                  break;
                }
              }
            } // foreach parent menu widget
          }
        } // if rollup listener knows about menus
      }
      }
      else if( msg == WM_SETFOCUS) {
         if( !mp2 && 
             !bothFromSameWindow( ((nsWindow*)gRollupWidget)->GetMainWindow(), 
                                  (HWND)mp1) ) {
            gRollupListener->Rollup();
         }
      }
   }

   // Messages which get re-routed if their source was an nsWindow
   // (it's very bad to reroute messages whose source isn't an nsWindow,
   // listboxes with scrollbars for example would break)
   switch( msg)
   {
      case WM_CONTROL:
      case WM_HSCROLL:
      case WM_VSCROLL: // !! potential problems here if canvas children
      {
         // assume parent == owner, true for our creations
         HWND hwndChild = WinWindowFromID( hwnd, SHORT1FROMMP( mp1));
         if( hwndChild)
         {
            nsWindow *w = nsWindow::GetNSWindowPtr(hwndChild);
            if( w)
               wnd = w;
         }
         break;
      }
   }

    // hold on to the window for the life of this method, in case it gets
    // deleted during processing. yes, it's a double hack, since someWindow
    // is not really an interface.
    nsCOMPtr<nsISupports> kungFuDeathGrip;
    if (!wnd->mIsDestroying) // not if we're in the destructor!
      kungFuDeathGrip = do_QueryInterface((nsBaseWidget*)wnd);

   MRESULT mresult = 0;

   if (wnd)
   {
      if( PR_FALSE == wnd->ProcessMessage( msg, mp1, mp2, mresult) &&
          WinIsWindow( (HAB)0, hwnd) && wnd->GetPrevWP())
      {
         mresult = (wnd->GetPrevWP())( hwnd, msg, mp1, mp2);

      }
   }
   else
      /* erm */ mresult = WinDefWindowProc( hwnd, msg, mp1, mp2);

   return mresult;
}

//-------------------------------------------------------------------------
//
// Utility method for implementing both Create(nsIWidget ...) and
// Create(nsNativeWidget...)
//-------------------------------------------------------------------------
void nsWindow::DoCreate( HWND hwndP, nsWindow *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
   mWindowState = nsWindowState_eInCreate;

  if( aInitData && (aInitData->mWindowType == eWindowType_dialog ||
                    aInitData->mWindowType == eWindowType_toplevel ||
                    aInitData->mWindowType == eWindowType_invisible))
    mIsTopWidgetWindow = PR_TRUE;
  else
    mIsTopWidgetWindow = PR_FALSE;

   if( aInitData != nsnull) {
     SetWindowType(aInitData->mWindowType);
     SetBorderStyle(aInitData->mBorderStyle);
   }

   // Must ensure toolkit before attempting to thread-switch!
   if( !mToolkit)
   {
      if( aToolkit)
      {
         mToolkit = aToolkit;
         NS_ADDREF(mToolkit);
      }
      else if( aParent)
         mToolkit = aParent->GetToolkit();
      else
      {
         // it's some top level window with no toolkit passed in.
         // Create a default toolkit with the current thread
         mToolkit = new nsToolkit;
         NS_ADDREF(mToolkit);
         mToolkit->Init( PR_GetCurrentThread());
      }
      mOS2Toolkit = (nsToolkit*) mToolkit;
   }

   // Switch to the PM thread if necessary...
   if(mOS2Toolkit && !mOS2Toolkit->IsGuiThread())
   {
      ULONG args[7] = { hwndP, (ULONG) aParent, (ULONG) &aRect,
                        (ULONG) aHandleEventFunction,
                        (ULONG) aContext, (ULONG) aAppShell,
                        (ULONG) aInitData };
      MethodInfo info( this, nsWindow::CREATE, 7, args);
      mOS2Toolkit->CallMethod( &info);
   }
   else
      // This is potentially virtual; overridden in nsFrameWindow
      RealDoCreate( hwndP, aParent, aRect, aHandleEventFunction,
                    aContext, aAppShell, aInitData);

   mWindowState = nsWindowState_eLive;
}

void nsWindow::RealDoCreate( HWND              hwndP,
                             nsWindow         *aParent,
                             const nsRect     &aRect,
                             EVENT_CALLBACK    aHandleEventFunction,
                             nsIDeviceContext *aContext,
                             nsIAppShell      *aAppShell,
                             nsWidgetInitData *aInitData,
                             HWND              hwndOwner)
{

  // XXXX TEST HACK to get rid of window bunnies
  //  if (!(aRect.height > 1) && !(aRect.width > 1))
  //    return;

   // Set up parent data - don't addref to avoid circularity
   mParent = aParent;

   ULONG style = WindowStyle();
   if( aInitData)
   {
      if( aInitData->clipChildren)
         style |= WS_CLIPCHILDREN;
#if 0
      //
      // Windows has a slightly different idea of what the implications are
      // of a window having or not having the CLIPSIBLINGS style.
      // All 'canvas' components we create must have clipsiblings, or
      // strange things happen & performance actually degrades.
      //
      else
        style &= ~WS_CLIPCHILDREN;
#endif

      if( aInitData->clipSiblings)
         style |= WS_CLIPSIBLINGS;
      else
         style &= ~WS_CLIPSIBLINGS;
   }

   mIsScrollBar = (!(strcmp( WindowClass(), WC_SCROLLBAR_STRING )));

   if( hwndP != HWND_DESKTOP)
   {
      // For pop-up menus, the parent is the desktop, but use the "parent" as owner
      if( aInitData && aInitData->mWindowType == eWindowType_popup)
      {
         if( !hwndOwner)
         {
            hwndOwner = hwndP;
         }
         hwndP = HWND_DESKTOP;
      }
      // For scrollbars, the parent is the owner, for notification purposes
      else if(!hwndOwner )
      {
         if( mIsScrollBar )
         {
            hwndOwner = hwndP;
         }
      }
   }

#ifdef DEBUG_FOCUS
   mWindowIdentifier = currentWindowIdentifier;
   currentWindowIdentifier++;
   if (aInitData && (aInitData->mWindowType == eWindowType_toplevel)) {
      printf("[%x] Create Frame Window (%d)\n", this, mWindowIdentifier);
   } else {
     printf("[%x] Create Window  (%d)\n", this, mWindowIdentifier);
   }
#endif

   // Create a window: create hidden & then size to avoid swp_noadjust problems
   // owner == parent except for 'borderless top-level' -- see nsCanvas.cpp
   mWnd = WinCreateWindow( hwndP,
                           WindowClass(),
                           0,          // text
                           style,
                           0, 0, 0, 0, // pos/size
                           hwndOwner,
                           HWND_TOP,
                           mParent ? mParent->GetNextID() : 0,
                           0, 0);      // ctldata, presparams

   NS_ASSERTION( mWnd, "Couldn't create window");

   if (gIsTrackPoint && mWindowType == eWindowType_child && !mIsScrollBar) {
     WinCreateWindow(mWnd, WC_SCROLLBAR, 0, SBS_VERT,
                     0, 0, 0, 0, mWnd, HWND_TOP,
                     FID_VERTSCROLL, NULL, NULL);
   }

#if DEBUG_sobotka
   printf("\n+++++++++++In nsWindow::RealDoCreate created 0x%lx, %d x %d\n",
	  mWnd, aRect.width, aRect.height);
   printf("+++++++++++Location =  %d x %d\n", aRect.x, aRect.y);
   printf("+++++++++++Parent = 0x%lx\n", GetParentHWND());
   printf("+++++++++++WINDOWCOUNT+ = %d\n", ++WINDOWCOUNT);
#endif

   // Make sure we have a device context from somewhere
   if( aContext)
   {
      mContext = aContext;
      NS_ADDREF(mContext);
   }
   else
   {
      nsresult rc = NS_OK;
      static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);

      rc = nsComponentManager::CreateInstance( kDeviceContextCID, nsnull,
                                               NS_GET_IID(nsIDeviceContext),
                                               (void **)&mContext);
      if( NS_SUCCEEDED(rc))
         mContext->Init( (nsNativeWidget) mWnd);
#ifdef DEBUG
      else
         printf( "Couldn't find DC instance for nsWindow\n");
#endif
   }

   Resize( aRect.x, aRect.y, aRect.width, aRect.height, PR_FALSE);

   // Record bounds.  This is XP, the rect of the entire main window in
   // parent space.  Returned by GetBounds().
   // NB: We haven't subclassed yet, so callbacks to change mBounds won't
   //     have happened!
   mBounds = aRect;
   mBounds.height = GetHeight( aRect.height);

   // Record passed in things
   mAppShell = aAppShell;

//   NS_IF_ADDREF( mAppShell);
   GetAppShell();  // Let the base widget class update the refcount for us....
   mEventCallback = aHandleEventFunction;

   if( mParent)
      mParent->AddChild( this);

   mContentType = aInitData? aInitData->mContentType : eContentTypeInherit;
   if (mContentType == eContentTypeInherit && aParent) {
      mContentType = aParent->mContentType;
   }

   // call the event callback to notify about creation

   DispatchStandardEvent( NS_CREATE );
   SubclassWindow(TRUE);
   PostCreateWidget();
}

//-------------------------------------------------------------------------
//
// Create the proper widget
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
   HWND hwndP = aParent ? (HWND)aParent->GetNativeData( NS_NATIVE_WINDOW)
                        : HWND_DESKTOP;

   DoCreate( hwndP, (nsWindow*) aParent, aRect, aHandleEventFunction,
             aContext, aAppShell, aToolkit, aInitData);

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
   // We need to find the nsWindow that goes with the native window, or controls
   // all get the ID of 0, and a zillion toolkits get created.
   //
   nsWindow *pParent = nsnull;
   HWND      hwndP = (HWND) aParent;

   if( hwndP && hwndP != HWND_DESKTOP)
      pParent = GetNSWindowPtr(hwndP);

   // XXX WC_MOZILLA will probably need a change here
   //
   if( !hwndP)
     hwndP = HWND_DESKTOP;

   DoCreate( hwndP, pParent, aRect, aHandleEventFunction, aContext,
             aAppShell, aToolkit, aInitData);

   return NS_OK;
}

//-------------------------------------------------------------------------
//
// Close this nsWindow
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Destroy()
{
  // Switch to the "main gui thread" if necessary... This method must
  // be executed on the "gui thread"...
   // Switch to the PM thread if necessary...
   if( mToolkit && !mOS2Toolkit->IsGuiThread())
   {
      MethodInfo info( this, nsWindow::DESTROY);
      mOS2Toolkit->CallMethod( &info);
   }
   else
   {
      // avoid calling into other objects if we're being deleted, 'cos
      // they must have no references to us.
      if( (mWindowState & nsWindowState_eLive) && mParent )
         nsBaseWidget::Destroy();

      // just to be safe. If we're going away and for some reason we're still
      // the rollup widget, rollup and turn off capture.
      if (this == gRollupWidget) {
         if (gRollupListener)
            gRollupListener->Rollup();
         CaptureRollupEvents(nsnull, PR_FALSE, PR_TRUE);
      }

      if( mWnd)
      {
         HWND hwndBeingDestroyed = mFrameWnd ? mFrameWnd : mWnd;
#ifdef DEBUG_FOCUS
         printf("[%x] Destroy (%d)\n", this, mWindowIdentifier);
#endif
         if (hwndBeingDestroyed == WinQueryFocus(HWND_DESKTOP)) {
           WinSetFocus(HWND_DESKTOP, WinQueryWindow(hwndBeingDestroyed, QW_PARENT));
         }
         WinDestroyWindow(hwndBeingDestroyed);
      }
   }
   return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this nsWindow parent
//
//-------------------------------------------------------------------------
nsIWidget* nsWindow::GetParent(void)
{
    if (mIsTopWidgetWindow) {
       // Must use a flag instead of mWindowType to tell if the window is the 
       // owned by the topmost widget, because a child window can be embedded inside
       // a HWND which is not associated with a nsIWidget.
      return nsnull;
    }
    /* If this widget has already been destroyed, pretend we have no parent.
       This corresponds to code in Destroy which removes the destroyed
       widget from its parent's child list. */
    if (mIsDestroying || mOnDestroyCalled)
      return nsnull;

   nsWindow *widget = nsnull;
   if ((nsnull != mParent) && (!mParent->mIsDestroying))
   {
      NS_ADDREF(mParent);
      widget = mParent;
   }

   return widget;
}

// Now, OS/2 methods
HWND nsWindow::GetParentHWND() const
{
   HWND hwnd = 0;
   if( nsnull != mParent)
      hwnd = mParent->mWnd;
   else
      hwnd = WinQueryWindow( GetMainWindow(), QW_PARENT);
   return hwnd;
}

// ptl is in parent's space
void nsWindow::NS2PM_PARENT( POINTL &ptl)
{
   if( mParent)
      mParent->NS2PM( ptl);
   else
   {
      HWND hwndp = WinQueryWindow( GetMainWindow(), QW_PARENT);
      SWP  swp = { 0 };
      WinQueryWindowPos( hwndp, &swp);
      ptl.y = swp.cy - ptl.y - 1;
   }
}

// ptl is in this window's space
void nsWindow::NS2PM( POINTL &ptl)
{
   ptl.y = GetClientHeight() - ptl.y - 1;
#if 0
   printf("+++++++++In NS2PM client height = %ld\n", GetClientHeight());
#endif
}

// rcl is in this window's space
void nsWindow::NS2PM( RECTL &rcl)
{
   LONG height = rcl.yTop - rcl.yBottom;
   rcl.yTop = GetClientHeight() - rcl.yBottom;
   rcl.yBottom = rcl.yTop - height;
}

//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Show(PRBool bState)
{
   // doesn't seem to require a message queue.
   if( mWnd)
   {
      HWND hwnd = GetMainWindow();
      if( bState == PR_TRUE)
      {
        // don't try to show new windows (e.g. the Bookmark menu)
        // during a native dragover because they'll remain invisible;
      if (CheckDragStatus(ACTION_SHOW, 0))
          WinShowWindow( hwnd, TRUE);
      }
      else
         WinShowWindow( hwnd, FALSE);
   }

   return NS_OK;
}

//-------------------------------------------------------------------------
//
// Return PR_TRUE if the whether the component is visible, PR_FALSE otherwise
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::IsVisible(PRBool & bState)
{
   // I guess this means visible & not showing...
   bState = WinIsWindowVisible( mWnd) ? PR_TRUE : PR_FALSE;
   return NS_OK;
}

//-------------------------------------------------------------------------
//
// Position the window behind the given window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                nsIWidget *aWidget, PRBool aActivate)
{
  HWND behind = aWidget ? (HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW) : HWND_TOP;
  UINT flags = SWP_ZORDER;
  if (aActivate)
    flags |= SWP_ACTIVATE;

  WinSetWindowPos(mWnd, behind, 0, 0, 0, 0, flags);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Maximize, minimize or restore the window.
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWindow::SetSizeMode(PRInt32 aMode) {

  nsresult rv;
  PRBool visible;

  // save the requested state
  rv = nsBaseWidget::SetSizeMode(aMode);

  IsVisible( visible);
  if (NS_SUCCEEDED(rv) && visible) {
    ULONG mode;
    switch (aMode) {
      case nsSizeMode_Maximized :
        mode = SWP_MAXIMIZE;
        break;
      case nsSizeMode_Minimized :
        mode = SWP_MINIMIZE;
        break;
      default :
        mode = SWP_RESTORE;
    }
    ::WinSetWindowPos(GetMainWindow(), NULLHANDLE, 0L, 0L, 0L, 0L, mode);
  }
  return rv;
}

//-------------------------------------------------------------------------
// Return PR_TRUE in aForWindow if the given event should be processed
// assuming this is a modal window.
//-------------------------------------------------------------------------
NS_METHOD nsWindow::ModalEventFilter(PRBool aRealEvent, void *aEvent,
                                     PRBool *aForWindow)
{
  if( PR_FALSE == aRealEvent) {
    *aForWindow = PR_FALSE;
    return NS_OK;
  }
#if 0
  // Set aForWindow if either:
  //   * the message is for a descendent of the given window
  //   * the message is for another window, but is a message which
  //     should be allowed for a disabled window.
 
  PRBool isMouseEvent = PR_FALSE;
  PRBool isInWindow = PR_FALSE;
 
  // Examine the target window & find the frame
  // XXX should GetNativeData() use GetMainWindow() ?
  HWND hwnd = (HWND)GetNativeData(NS_NATIVE_WINDOW);
  hwnd = WinQueryWindow(hwnd, QW_PARENT);
 
  if( hwnd == mQmsg.hwnd || WinIsChild( mQmsg.hwnd, hwnd))
     isInWindow = PR_TRUE;
  else if (!isInWindow && gRollupWidget &&
           EventIsInsideWindow((nsWindow*)gRollupWidget))
     // include for consideration any popup menu which may be active at the moment
     isInWindow = PR_TRUE;
 
  // XXX really ought to do something about focus here
 
  if( !isInWindow)
  {
     // Block mouse messages for non-modal windows
     if( mQmsg.msg >= WM_MOUSEFIRST && mQmsg.msg <= WM_MOUSELAST)
        isMouseEvent = PR_TRUE;
     else if( mQmsg.msg >= WM_MOUSETRANSLATEFIRST &&
              mQmsg.msg <= WM_MOUSETRANSLATELAST)
        isMouseEvent = PR_TRUE;
     else if( mQmsg.msg == WM_MOUSEENTER || mQmsg.msg == WM_MOUSELEAVE)
        isMouseEvent = PR_TRUE;
  }
 
  // set dispatch indicator
  *aForWindow = isInWindow || !isMouseEvent;
#else
  *aForWindow = PR_TRUE;
#endif

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Constrain a potential move to fit onscreen
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::ConstrainPosition(PRBool aAllowSlop,
                                      PRInt32 *aX, PRInt32 *aY)
{
  if (!mIsTopWidgetWindow) // only a problem for top-level windows
    return NS_OK;

  PRBool doConstrain = PR_FALSE; // whether we have enough info to do anything

  /* get our playing field. use the current screen, or failing that
    for any reason, use device caps for the default screen. */
  RECTL screenRect;

  nsCOMPtr<nsIScreenManager> screenmgr = do_GetService(sScreenManagerContractID);
  if (screenmgr) {
    nsCOMPtr<nsIScreen> screen;
    PRInt32 left, top, width, height;

    // zero size rects confuse the screen manager
    width = mBounds.width > 0 ? mBounds.width : 1;
    height = mBounds.height > 0 ? mBounds.height : 1;
    screenmgr->ScreenForRect(*aX, *aY, width, height,
                            getter_AddRefs(screen));
    if (screen) {
      screen->GetAvailRect(&left, &top, &width, &height);
      screenRect.xLeft = left;
      screenRect.xRight = left+width;
      screenRect.yTop = top;
      screenRect.yBottom = top+height;
      doConstrain = PR_TRUE;
    }
  }

#define kWindowPositionSlop 100

  if (doConstrain) {
    if (aAllowSlop) {
      if (*aX < screenRect.xLeft - mBounds.width + kWindowPositionSlop)
        *aX = screenRect.xLeft - mBounds.width + kWindowPositionSlop;
      else if (*aX >= screenRect.xRight - kWindowPositionSlop)
        *aX = screenRect.xRight - kWindowPositionSlop;
  
      if (*aY < screenRect.yTop)
        *aY = screenRect.yTop;
      else if (*aY >= screenRect.yBottom - kWindowPositionSlop)
        *aY = screenRect.yBottom - kWindowPositionSlop;
  
    } else {
  
      if (*aX < screenRect.xLeft)
        *aX = screenRect.xLeft;
      else if (*aX >= screenRect.xRight - mBounds.width)
        *aX = screenRect.xRight - mBounds.width;
  
      if (*aY < screenRect.yTop)
        *aY = screenRect.yTop;
      else if (*aY >= screenRect.yBottom - mBounds.height)
        *aY = screenRect.yBottom - mBounds.height;
    }
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
   Resize( aX, aY, mBounds.width, mBounds.height, PR_FALSE);
   return NS_OK;
}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
   Resize( mBounds.x, mBounds.y, aWidth, aHeight, aRepaint);
   return NS_OK;
}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRInt32 aX,
                      PRInt32 aY,
                      PRInt32 w,
                      PRInt32 h,
                      PRBool   aRepaint)
{
  //   NS_ASSERTION((w >=0 ), "Negative width passed to nsWindow::Resize");
  //   NS_ASSERTION((h >=0 ), "Negative height passed to nsWindow::Resize");

   // WinSetWindowPos() appears not to require a msgq
   if( mWnd)
   {
      // need to keep top-left corner in the same place
      // work out real coords of top left
      POINTL ptl= { aX, aY };
      NS2PM_PARENT( ptl);
      // work out real coords of bottom left
      ptl.y -= GetHeight( h) - 1;
      if( mParent)
      {
         WinMapWindowPoints( mParent->mWnd, WinQueryWindow(mWnd, QW_PARENT), &ptl, 1);
      }

      if( !SetWindowPos( 0, ptl.x, ptl.y, w, GetHeight(h), SWP_MOVE | SWP_SIZE))
         if( aRepaint)
            Invalidate(PR_FALSE);

#if DEBUG_sobotka
   printf("+++++++++++Resized 0x%lx at %ld, %ld to %ld x %ld\n\n", mWnd, ptl.x, ptl.y, w, GetHeight(h));
#endif

   }
   else
   {
      mBounds.x = aX;
      mBounds.y = aY;
      mBounds.width = w;
      mBounds.height = h;
   }
   return NS_OK;
}

//-------------------------------------------------------------------------
//
// Enable/disable this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Enable(PRBool bState)
{
   if (mWnd) {
      WinEnableWindow( GetMainWindow(), !!bState);
   }
   return NS_OK;
}


NS_METHOD nsWindow::IsEnabled(PRBool *aState)
{
   NS_ENSURE_ARG_POINTER(aState);
   *aState = !mWnd || ::WinIsWindowEnabled(mWnd);
   return NS_OK;
}



//-------------------------------------------------------------------------
//
// Give the focus to this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetFocus(PRBool aRaise)
{
    //
    // Switch to the "main gui thread" if necessary... This method must
    // be executed on the "gui thread"...
    //
    // Switch to the PM thread if necessary...
    if( !mOS2Toolkit->IsGuiThread())
    {
        MethodInfo info(this, nsWindow::SET_FOCUS);
        mOS2Toolkit->CallMethod(&info);
    }
    else
    if (mWnd) {
        if (!mInSetFocus) {
#ifdef DEBUG_FOCUS
           printf("[%x] SetFocus (%d)\n", this, mWindowIdentifier);
#endif
           mInSetFocus = TRUE;
           WinSetFocus( HWND_DESKTOP, mWnd);
           mInSetFocus = FALSE;
        }

    }
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this component dimension
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::GetBounds(nsRect &aRect)
{
  if (mFrameWnd) {
    SWP swp;
    WinQueryWindowPos(mFrameWnd, &swp);
    aRect.width = swp.cx;
    aRect.height = swp.cy;
    aRect.x = swp.x;
    aRect.y = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN) - (swp.y+swp.cy);
  } else {
    aRect = mBounds;
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this component dimension
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::GetClientBounds(nsRect &aRect)
{

   // nsFrameWindow overrides this...
   aRect.x = 0;
   aRect.y = 0;
   aRect.width = mBounds.width;
   aRect.height = mBounds.height;
   return NS_OK;
}

//get the bounds, but don't take into account the client size

void nsWindow::GetNonClientBounds(nsRect &aRect)
{
  if (mWnd) {
      RECTL r;
      WinQueryWindowRect(mWnd, &r);

      // assign size
      aRect.width = r.xRight - r.xLeft;
      aRect.height = r.yBottom - r.yTop;

      // convert coordinates if parent exists
      HWND parent = WinQueryWindow(mWnd, QW_PARENT);
      if (parent) {
        RECTL pr;
        WinQueryWindowRect(parent, &pr);
        r.xLeft -= pr.xLeft;
        r.yTop -= pr.yTop;
      }
      aRect.x = r.xLeft;
      aRect.y = r.yTop;
  } else {
      aRect.SetRect(0,0,0,0);
  }
}

//-------------------------------------------------------------------------
//
// Get this component font
//
//-------------------------------------------------------------------------
nsIFontMetrics *nsWindow::GetFont(void)
{
   nsIFontMetrics *metrics = nsnull;

   if( mToolkit)
   {
      char buf[2][128];
      int  ptSize;
   
      WinQueryPresParam( mWnd, PP_FONTNAMESIZE, 0, 0, 128, buf[0], 0);
   
      if( 2 == sscanf( buf[0], "%d.%s", &ptSize, buf[1])) // mmm, scanf()...
      {
         float twip2dev, twip2app;
         twip2dev = mContext->TwipsToDevUnits();
         twip2app = mContext->DevUnitsToAppUnits();
         twip2app *= twip2dev;
   
         nscoord appSize = (nscoord) (twip2app * ptSize * 20);
   
         nsFont font( buf[1], NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
                      NS_FONT_WEIGHT_NORMAL, 0 /*decoration*/, appSize);
   
         mContext->GetMetricsFor( font, metrics);
      }
   }

   return metrics;
}

//-------------------------------------------------------------------------
//
// Set this component font
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetFont(const nsFont &aFont)
{
   if( mToolkit) // called from print-routine (XXX check)
   {
      // jump through hoops to convert the size in the font (in app units)
      // into points. 
      float dev2twip, app2twip;
      dev2twip = mContext->DevUnitsToTwips();
      app2twip = mContext->AppUnitsToDevUnits();
      app2twip *= dev2twip;
      int points = NSTwipsToFloorIntPoints( nscoord( aFont.size * app2twip));

      nsAutoCharBuffer fontname;
      PRInt32 fontnameLength;
      WideCharToMultiByte(0, aFont.name.get(), aFont.name.Length(),
                          fontname, fontnameLength);

      char *buffer = new char[fontnameLength + 6];
      if (buffer) {
        sprintf(buffer, "%d.%s", points, fontname.get());
        ::WinSetPresParam(mWnd, PP_FONTNAMESIZE,
                          strlen(buffer) + 1, buffer);
        delete [] buffer;
      }
   }

   if( !mFont)
      mFont = new nsFont( aFont);
   else
      *mFont = aFont;

   return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this component cursor
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::SetCursor(nsCursor aCursor)
{
  HPOINTER newPointer = NULLHANDLE;

  switch(aCursor) {
  case eCursor_select:
    newPointer = ::WinQuerySysPointer(HWND_DESKTOP, SPTR_TEXT, FALSE);
    break;
    
  case eCursor_wait:
    newPointer = ::WinQuerySysPointer(HWND_DESKTOP, SPTR_WAIT, FALSE);
    break;

  case eCursor_hyperlink:
    newPointer = gPtrArray[IDC_SELECTANCHOR-IDC_BASE];
    break;

  case eCursor_standard:
    newPointer = ::WinQuerySysPointer(HWND_DESKTOP, SPTR_ARROW, FALSE);
    break;

  case eCursor_sizeWE:
    newPointer = ::WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZEWE, FALSE);
    break;

  case eCursor_sizeNS:
    newPointer = ::WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZENS, FALSE);
    break;

  case eCursor_sizeNW:
  case eCursor_sizeSE:
    newPointer = ::WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZENWSE, FALSE);
    break;

  case eCursor_sizeNE:
  case eCursor_sizeSW:
    newPointer = ::WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZENESW, FALSE);
    break;

  case eCursor_arrow_north:
    newPointer = gPtrArray[IDC_ARROWNORTH-IDC_BASE];
    break;

  case eCursor_arrow_north_plus:
    newPointer = gPtrArray[IDC_ARROWNORTHPLUS-IDC_BASE];
    break;

  case eCursor_arrow_south:
    newPointer = gPtrArray[IDC_ARROWSOUTH-IDC_BASE];
    break;

  case eCursor_arrow_south_plus:
    newPointer = gPtrArray[IDC_ARROWSOUTHPLUS-IDC_BASE];
    break;

  case eCursor_arrow_east:
    newPointer = gPtrArray[IDC_ARROWEAST-IDC_BASE];
    break;

  case eCursor_arrow_east_plus:
    newPointer = gPtrArray[IDC_ARROWEASTPLUS-IDC_BASE];
    break;

  case eCursor_arrow_west:
    newPointer = gPtrArray[IDC_ARROWWEST-IDC_BASE];
    break;

  case eCursor_arrow_west_plus:
    newPointer = gPtrArray[IDC_ARROWWESTPLUS-IDC_BASE];
    break;

  case eCursor_crosshair:
    newPointer = gPtrArray[IDC_CROSS-IDC_BASE];
    break;
             
  case eCursor_move:
    newPointer = ::WinQuerySysPointer(HWND_DESKTOP, SPTR_MOVE, FALSE);
    break;

  case eCursor_help:
    newPointer = gPtrArray[IDC_HELP-IDC_BASE];
    break;

  case eCursor_copy: // CSS3
    newPointer = gPtrArray[IDC_COPY-IDC_BASE];
    break;

  case eCursor_alias:
    newPointer = gPtrArray[IDC_ALIAS-IDC_BASE];
    break;

  case eCursor_cell:
    newPointer = gPtrArray[IDC_CELL-IDC_BASE];
    break;

  case eCursor_grab:
    newPointer = gPtrArray[IDC_GRAB-IDC_BASE];
    break;

  case eCursor_grabbing:
    newPointer = gPtrArray[IDC_GRABBING-IDC_BASE];
    break;

  case eCursor_spinning:
    newPointer = gPtrArray[IDC_ARROWWAIT-IDC_BASE];
    break;

  case eCursor_context_menu:
  case eCursor_count_up:
  case eCursor_count_down:
  case eCursor_count_up_down:
    break;

  case eCursor_zoom_in:
    newPointer = gPtrArray[IDC_ZOOMIN-IDC_BASE];
    break;

  case eCursor_zoom_out:
    newPointer = gPtrArray[IDC_ZOOMOUT-IDC_BASE];
    break;

  default:
    NS_ASSERTION(0, "Invalid cursor type");
    break;
  }

  if (newPointer) {
    WinSetPointer(HWND_DESKTOP, newPointer);
  }

  return NS_OK;
}

NS_IMETHODIMP nsWindow::HideWindowChrome(PRBool aShouldHide) 
{
  HWND hwndFrame = NULLHANDLE;
  HWND hwndTitleBar = NULLHANDLE;
  HWND hwndSysMenu = NULLHANDLE;
  HWND hwndMinMax = NULLHANDLE;
  HWND hwndParent;
  ULONG ulStyle;
  char className[19];

  HWND hwnd = (HWND)GetNativeData(NS_NATIVE_WINDOW);
  for ( ; hwnd != NULLHANDLE; hwnd = WinQueryWindow(hwnd, QW_PARENT)) {
    ::WinQueryClassName(hwnd, 19, className);
    if (strcmp(className, WC_FRAME_STRING) == 0)
    {
      hwndFrame = hwnd;
      break;
    }
  }


  if (aShouldHide) {
    hwndParent = HWND_OBJECT;
    mChromeHidden = TRUE;
  } else {
    hwndParent = hwndFrame;
    mChromeHidden = FALSE;
  }
  hwndTitleBar = (HWND)WinQueryProperty(hwndFrame, "hwndTitleBar");
  if (hwndTitleBar)
    WinSetParent(hwndTitleBar, hwndParent, TRUE);
  hwndSysMenu = (HWND)WinQueryProperty(hwndFrame, "hwndSysMenu");
  if (hwndSysMenu)
    WinSetParent(hwndSysMenu, hwndParent, TRUE);
  hwndMinMax = (HWND)WinQueryProperty(hwndFrame, "hwndMinMax");
  if (hwndMinMax)
    WinSetParent(hwndMinMax, hwndParent, TRUE);
  if (aShouldHide) {
    ulStyle = (ULONG)WinQueryWindowULong(hwndFrame, QWL_STYLE);
    WinSetWindowULong(hwndFrame, QWL_STYLE, ulStyle & ~FS_SIZEBORDER);
    WinSetProperty(hwndFrame, "ulStyle", (PVOID)ulStyle, 0);
    WinSendMsg(hwndFrame, WM_UPDATEFRAME, 0, 0);
  } else {
    ulStyle = (ULONG)WinQueryProperty(hwndFrame, "ulStyle");
    WinSetWindowULong(hwndFrame, QWL_STYLE, ulStyle);
    WinSendMsg(hwndFrame, WM_UPDATEFRAME, MPFROMLONG(FCF_TITLEBAR | FCF_SYSMENU | FCF_MINMAX), 0);
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
    if (mWnd)
    {
      WinInvalidateRect( mWnd, 0, FALSE);
#if 0
      if( PR_TRUE == aIsSynchronous) {
         Update();
      }
#endif
    }

    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Invalidate(const nsRect &aRect, PRBool aIsSynchronous)
{
  if (mWnd)
  {
    RECTL rcl = { aRect.x, aRect.y, aRect.x + aRect.width, aRect.y + aRect.height };
    NS2PM( rcl);

    WinInvalidateRect( mWnd, &rcl, FALSE);
#if 0
    if( PR_TRUE == aIsSynchronous) {
      Update();
    }
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsWindow::InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous)

{
  nsresult rv = NS_OK;
  if (mWnd) {
    PRInt32 aX, aY, aWidth, aHeight;
    ((nsIRegion*)aRegion)->GetBoundingBox (&aX, &aY, &aWidth, &aHeight);

    RECTL rcl = { aX, aY, aX + aWidth, aY + aHeight };
    NS2PM (rcl);
    WinInvalidateRect (mWnd, &rcl, FALSE);

#if 0
        if( PR_TRUE == aIsSynchronous) {
          Update();
        }
#endif

  }
  return rv;  
}

//-------------------------------------------------------------------------
//
// Force a synchronous repaint of the window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Update()
{
  // Switch to the PM thread if necessary...
  if( !mOS2Toolkit->IsGuiThread())
  {
    MethodInfo info(this, nsWindow::UPDATE_WINDOW);
    mOS2Toolkit->CallMethod(&info);
  }
  else 
  if (mWnd)
    WinUpdateWindow( mWnd);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void* nsWindow::GetNativeData(PRUint32 aDataType)
{
    switch(aDataType) {
        case NS_NATIVE_WIDGET:
        case NS_NATIVE_WINDOW:
        case NS_NATIVE_PLUGIN_PORT:
            return (void*)mWnd;

        case NS_NATIVE_GRAPHIC: {
        // during a native drag over the current window or any drag
        // originating in Moz, return a drag HPS to avoid screen corruption;
            HPS hps = 0;
            CheckDragStatus(ACTION_DRAW, &hps);
            if (!hps)
              hps = WinGetPS(mWnd);
            nsPaletteOS2::SelectGlobalPalette(hps, mWnd);
            return (void*)hps;
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
  switch(aDataType)
  {
    case NS_NATIVE_GRAPHIC:
      if (data) {
        if (!ReleaseIfDragHPS((HPS)data))
          WinReleasePS((HPS)data);
      }
      break;
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_PLUGIN_PORT:
    case NS_NATIVE_COLORMAP:
      break;
    default: 
      break;
  }
}

//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetColorMap(nsColorMap *aColorMap)
{
   // SetColorMap - not implemented.
   // Any palette lives in the device context & should be altered there.
   // And shouldn't be altered at all, once libimg has decided what should
   // be in it.  So my opinion is this method shouldn't be called.
   return NS_ERROR_NOT_IMPLEMENTED;
}

//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
//XXX Scroll is obsolete and should go away soon
NS_METHOD nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  RECTL rcl;

  if (nsnull != aClipRect)
  {
    rcl.xLeft = aClipRect->x;
    rcl.yBottom = aClipRect->y + aClipRect->height;
    rcl.xRight = rcl.xLeft + aClipRect->width;
    rcl.yTop = rcl.yBottom + aClipRect->height;
    NS2PM( rcl);
    // this rect is inex
  }

    // this prevents screen corruption while scrolling during a
    // Moz-originated drag - during a native drag, the screen
    // isn't updated until the drag ends. so there's no corruption
  HPS hps = 0;
  CheckDragStatus(ACTION_SCROLL, &hps);

  WinScrollWindow( mWnd, aDx, -aDy, aClipRect ? &rcl : 0, 0, 0,
                   0, SW_SCROLLCHILDREN | SW_INVALIDATERGN);
  Update();

  if (hps)
    ReleaseIfDragHPS(hps);

  return NS_OK;
}

NS_IMETHODIMP nsWindow::ScrollWidgets(PRInt32 aDx, PRInt32 aDy)
{
    // this prevents screen corruption while scrolling during a
    // Moz-originated drag - during a native drag, the screen
    // isn't updated until the drag ends. so there's no corruption
  HPS hps = 0;
  CheckDragStatus(ACTION_SCROLL, &hps);

    // Scroll the entire contents of the window + change the offset of any child windows
  WinScrollWindow( mWnd, aDx, -aDy, 0, 0, 0, 0,
                   SW_INVALIDATERGN | SW_SCROLLCHILDREN);
  Update(); // Force synchronous generation of NS_PAINT

  if (hps)
    ReleaseIfDragHPS(hps);

  return NS_OK;
}

NS_IMETHODIMP nsWindow::ScrollRect(nsRect &aRect, PRInt32 aDx, PRInt32 aDy)
{
  RECTL rcl;

  rcl.xLeft = aRect.x;
  rcl.yBottom = aRect.y + aRect.height;
  rcl.xRight = rcl.xLeft + aRect.width;
  rcl.yTop = rcl.yBottom + aRect.height;
  NS2PM( rcl);

    // this prevents screen corruption while scrolling during a
    // Moz-originated drag - during a native drag, the screen
    // isn't updated until the drag ends. so there's no corruption
  HPS hps = 0;
  CheckDragStatus(ACTION_SCROLL, &hps);

    // Scroll the bits in the window defined by trect. 
    // Child windows are not scrolled.
  WinScrollWindow(mWnd, aDx, -aDy, &rcl, 0, 0, 0, SW_INVALIDATERGN);
  Update(); // Force synchronous generation of NS_PAINT

  if (hps)
    ReleaseIfDragHPS(hps);

  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Every function that needs a thread switch goes through this function
// by calling SendMessage (..WM_CALLMETHOD..) in nsToolkit::CallMethod.
//
//-------------------------------------------------------------------------
BOOL nsWindow::CallMethod(MethodInfo *info)
{
    BOOL bRet = TRUE;

    switch (info->methodId) {
        case nsWindow::CREATE:
            NS_ASSERTION(info->nArgs == 7, "Wrong number of arguments to CallMethod Create");
            DoCreate( (HWND)               info->args[0],
                      (nsWindow*)          info->args[1],
                      (const nsRect&)*(nsRect*) (info->args[2]),
                      (EVENT_CALLBACK)    (info->args[3]), 
                      (nsIDeviceContext*) (info->args[4]),
                      (nsIAppShell*)      (info->args[5]),
                      nsnull, /* toolkit */
                      (nsWidgetInitData*) (info->args[6]));
            break;

        case nsWindow::DESTROY:
            NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod Destroy");
            Destroy();
            break;

        case nsWindow::SET_FOCUS:
            NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod SetFocus");
            SetFocus(PR_FALSE);
            break;

        case nsWindow::UPDATE_WINDOW:
            NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod UpdateWindow");
            Update();
            break;

        case nsWindow::SET_TITLE:
            NS_ASSERTION(info->nArgs == 1, "Wrong number of arguments to CallMethod SetTitle");
            SetTitle( (const nsAString &) info->args[0]);
            break;

        case nsWindow::GET_TITLE:
            NS_ASSERTION(info->nArgs == 2, "Wrong number of arguments to CallMethod GetTitle");
            GetWindowText( *((nsString*) info->args[0]),
                           (PRUint32*)info->args[1]);
            break;

        default:
            bRet = FALSE;
            break;
    }

    return bRet;
}

//-------------------------------------------------------------------------
//
// OnKey
//
//-------------------------------------------------------------------------
// Key handler.  Specs for the various text messages are really confused;
// see other platforms for best results of how things are supposed to work.
//
// Perhaps more importantly, the main man listening to these events (besides
// random bits of javascript) is ender -- see 
// mozilla/editor/base/nsEditorEventListeners.cpp.
//
PRBool nsWindow::OnKey( MPARAM mp1, MPARAM mp2)
{
   nsKeyEvent pressEvent;
   USHORT     fsFlags = SHORT1FROMMP(mp1);
   USHORT     usVKey = SHORT2FROMMP(mp2);
   USHORT     usChar = SHORT1FROMMP(mp2);
//   UCHAR      uchScan = CHAR4FROMMP(mp1);

   // It appears we're not supposed to transmit shift, control & alt events
   // to gecko.  Shrug.
   //
   // XXX this may be wrong, but is what gtk is doing...
   if( fsFlags & KC_VIRTUALKEY &&
      (usVKey == VK_SHIFT || usVKey == VK_CTRL ||
       usVKey == VK_ALTGRAF)) return PR_FALSE;

   // My gosh this is ugly
   // Workaround bug where using Alt+Esc let an Alt key creep through
   // Only handle alt by itself if the LONEKEY bit is set
   if ((fsFlags & KC_VIRTUALKEY) &&
       (usVKey == VK_ALT) &&
       !usChar && 
       ((fsFlags & KC_LONEKEY) == 0) &&
       (fsFlags & KC_KEYUP)) {
         return PR_FALSE;
       }

   // Now check if it's a dead-key
   if( fsFlags & KC_DEADKEY)
   {
      // XXX CUA says we're supposed to give some kind of feedback `display the
      //     dead key glyph'.  I'm not sure if we can use the COMPOSE messages
      //     to do this -- it should really be done by someone who can test it
      //     & has some idea what `ought' to happen...

      return PR_TRUE;
   }

   // Now dispatch a keyup/keydown event.  This one is *not* meant to
   // have the unicode charcode in.

   nsPoint point(0,0);
   nsKeyEvent event((fsFlags & KC_KEYUP) ? NS_KEY_UP : NS_KEY_DOWN, this);
   InitEvent( event, &point);
   event.keyCode   = WMChar2KeyCode( mp1, mp2);
   event.isShift   = (fsFlags & KC_SHIFT) ? PR_TRUE : PR_FALSE;
   event.isControl = (fsFlags & KC_CTRL) ? PR_TRUE : PR_FALSE;
   event.isAlt     = (fsFlags & KC_ALT) ? PR_TRUE : PR_FALSE;
   event.isMeta    = PR_FALSE;
   event.charCode = 0;

   /* Checking for a scroll mouse event vs. a keyboard event */
   /* The way we know this is that the repeat count is 0 and */
   /* the key is not physically down */
   /* unfortunately, there is an exception here - if alt or ctrl are */
   /* held down, repeat count is set so we have to add special checks for them */
   if (((event.keyCode == NS_VK_UP) || (event.keyCode == NS_VK_DOWN)) &&
       (!(fsFlags & KC_KEYUP))
       && ((CHAR3FROMMP(mp1) == 0) || fsFlags & KC_CTRL || fsFlags & KC_ALT)
       ) {
      if (!(WinGetPhysKeyState(HWND_DESKTOP, CHAR4FROMMP(mp1)) & 0x8000)) {
         MPARAM mp2;
         if (event.keyCode == NS_VK_UP)
            mp2 = MPFROM2SHORT(0, SB_LINEUP);
         else
            mp2 = MPFROM2SHORT(0, SB_LINEDOWN);
         WinSendMsg(mWnd, WM_VSCROLL, 0, mp2);
         return FALSE;
      }
   }

   pressEvent = event;

   PRBool rc = DispatchWindowEvent( &event);

   // Break off now if this was a key-up.
   if( fsFlags & KC_KEYUP)
   {
      NS_RELEASE( event.widget);
      return rc;
   }

   // Break off if we've got an "invalid composition" -- that is, the user
   // typed a deadkey last time, but has now typed something that doesn't
   // make sense in that context.
   if( fsFlags & KC_INVALIDCOMP)
   {
      mHaveDeadKey = FALSE;
      // XXX actually, not sure whether we're supposed to abort the keypress
      //     or process it as though the dead key has been pressed.
      NS_RELEASE( event.widget);
      return rc;
   }

   // Now we need to dispatch a keypress event which has the unicode char.

   pressEvent.message = NS_KEY_PRESS;

   if( usChar)
   {
      USHORT inbuf[2];
      inbuf[0] = usChar;
      inbuf[1] = '\0';

      nsAutoChar16Buffer outbuf;
      PRInt32 bufLength;
      MultiByteToWideChar(0, (const char*)inbuf, 2, outbuf, bufLength);

      pressEvent.charCode = outbuf.get()[0];

      if (pressEvent.isControl && !(fsFlags & (KC_VIRTUALKEY | KC_DEADKEY))) {
        if (!pressEvent.isShift && (pressEvent.charCode >= 'A' && pressEvent.charCode <= 'Z'))
        {
          pressEvent.charCode = tolower(pressEvent.charCode);
        }
        if (pressEvent.isShift && (pressEvent.charCode >= 'a' && pressEvent.charCode <= 'z'))
        {
          pressEvent.charCode = toupper(pressEvent.charCode);
        }
        pressEvent.keyCode = 0;
      } else if( !pressEvent.isControl && !pressEvent.isAlt && pressEvent.charCode != 0)
      {
         if ( !(fsFlags & KC_VIRTUALKEY) || 
              ((fsFlags & KC_CHAR) && (pressEvent.keyCode == 0)) )
         {
//            pressEvent.isShift = PR_FALSE;
            pressEvent.keyCode = 0;
         }
         else if (usVKey == VK_SPACE)
         {
//            pressEvent.isShift = PR_FALSE;
         }
         else  // Real virtual key 
         {
            pressEvent.charCode = 0;
         }
      }
      rc = DispatchWindowEvent( &pressEvent);
   }

   NS_RELEASE( pressEvent.widget);
   return rc;
}

void nsWindow::ConstrainZLevel(HWND *aAfter) {

  nsZLevelEvent  event(NS_SETZLEVEL, this);
  nsWindow      *aboveWindow = 0;

  InitEvent(event);

  if (*aAfter == HWND_BOTTOM)
    event.mPlacement = nsWindowZBottom;
  else if (*aAfter == HWND_TOP)
    event.mPlacement = nsWindowZTop;
  else {
    event.mPlacement = nsWindowZRelative;
    aboveWindow = GetNSWindowPtr(*aAfter);
  }
  event.mReqBelow = aboveWindow;
  event.mActualBelow = nsnull;

  event.mImmediate = PR_FALSE;
  event.mAdjusted = PR_FALSE;
  DispatchWindowEvent(&event);

  if (event.mAdjusted) {
    if (event.mPlacement == nsWindowZBottom)
      *aAfter = HWND_BOTTOM;
    else if (event.mPlacement == nsWindowZTop)
      *aAfter = HWND_TOP;
    else {
      *aAfter = (HWND)event.mActualBelow->GetNativeData(NS_NATIVE_WINDOW);
      /* If we have a client window, use the frame */
      if (WinQueryWindowUShort(*aAfter, QWS_ID) == FID_CLIENT) {
        *aAfter = WinQueryWindow(*aAfter, QW_PARENT);
      }
    }
  }
  NS_IF_RELEASE(event.mActualBelow);
  NS_RELEASE(event.widget);
}


// 'Window procedure'
PRBool nsWindow::ProcessMessage( ULONG msg, MPARAM mp1, MPARAM mp2, MRESULT &rc)
{
    PRBool result = PR_FALSE; // call the default window procedure
    PRBool isMozWindowTakingFocus = PR_TRUE;

    switch (msg) {
//#if 0
        case WM_COMMAND: // fire off menu selections
        {
           nsMenuEvent event(NS_MENU_SELECTED, this);
           event.mCommand = SHORT1FROMMP(mp1);
           InitEvent(event);
           result = DispatchWindowEvent(&event);
           NS_RELEASE(event.widget);
        }
#if 0
          USHORT usSrc = SHORT1FROMMP( mp2);
          if( usSrc == CMDSRC_MENU || usSrc == CMDSRC_ACCELERATOR)
            result = OnMenuClick( SHORT1FROMMP(mp1));
          break;
        }

        case WM_INITMENU:
          result = OnActivateMenu( HWNDFROMMP(mp2), TRUE);
          break;

        case WM_MENUEND:
          result = OnActivateMenu( HWNDFROMMP(mp2), FALSE);
          break;
#endif

#if 0  // Tooltips appear to be gone
        case WMU_SHOW_TOOLTIP:
        {
          nsTooltipEvent event(NS_SHOW_TOOLTIP, this);
          InitEvent( event );
          event.tipIndex = LONGFROMMP(mp1);
          result = DispatchWindowEvent(&event);
          NS_RELEASE(event.widget);
          break;
        }

        case WMU_HIDE_TOOLTIP:
          result = DispatchStandardEvent( NS_HIDE_TOOLTIP );
          break;
#endif
        case WM_CONTROL: // remember this is resent to the orginator...
          result = OnControl( mp1, mp2);
          break;

        case WM_CLOSE:  // close request
          mWindowState |= nsWindowState_eClosing;
          DispatchStandardEvent( NS_XUL_CLOSE );
          result = PR_TRUE; // abort window closure
          break;

        case WM_DESTROY:
            // clean up.
            OnDestroy();
            result = PR_TRUE;
            break;

        case WM_PAINT:
            result = OnPaint();
            break;

        case WM_TRANSLATEACCEL:
            {
              PQMSG pQmsg = (PQMSG)mp1;
              if (pQmsg->msg == WM_CHAR) 
              {
                LONG mp1 = (LONG)pQmsg->mp1;
                LONG mp2 = (LONG)pQmsg->mp2;

                // If we have a shift+enter combination, return false
                // immediately.  OS/2 considers the shift+enter
                // combination an accelerator and will translate it into
                // a WM_OPEN message.  When this happens we do not get a
                // WM_CHAR for the down transition of the enter key when
                // shift is also pressed and OnKeyDown will not be called.
                if (((SHORT1FROMMP(mp1) & (KC_VIRTUALKEY | KC_SHIFT)) &&
                     (SHORT2FROMMP(mp2) == VK_ENTER)) ||
                // Let Mozilla handle standalone F10, not the OS
                    ((SHORT1FROMMP(mp1) & KC_VIRTUALKEY) &&    
                     ((SHORT1FROMMP(mp1) & (KC_SHIFT | KC_ALT | KC_CTRL)) == 0) &&
                     (SHORT2FROMMP(mp2) == VK_F10)) ||
                // Let Mozilla handle standalone F1, not the OS
                    ((SHORT1FROMMP(mp1) & KC_VIRTUALKEY) &&    
                     ((SHORT1FROMMP(mp1) & (KC_SHIFT | KC_ALT | KC_CTRL)) == 0) &&
                     (SHORT2FROMMP(mp2) == VK_F1)) ||
                // Let Mozilla handle standalone Alt, not the OS
                    ((SHORT1FROMMP(mp1) & KC_KEYUP) && (SHORT1FROMMP(mp1) & KC_LONEKEY) &&
                     (SHORT2FROMMP(mp2) == VK_ALT)) ||
                // Let Mozilla handle Alt+Enter, not the OS
                    ((SHORT1FROMMP(mp1) & (KC_VIRTUALKEY | KC_ALT)) &&
                     (SHORT2FROMMP(mp2) == VK_NEWLINE)) 
                   )
                {
                  return(PR_TRUE);
                }
              }
            }
            break;

        case WM_CHAR:
            result = OnKey( mp1, mp2);
            break;

        case WM_QUERYCONVERTPOS:
          {
            PRECTL pCursorRect = (PRECTL)mp1;
            nsCompositionEvent event(NS_COMPOSITION_QUERY, this);
            nsPoint point;
            point.x = 0;
            point.y = 0;
            InitEvent(event,&point);
            DispatchWindowEvent(&event);
            if ((event.theReply.mCursorPosition.x) || 
                (event.theReply.mCursorPosition.y)) 
            {
              pCursorRect->xLeft = event.theReply.mCursorPosition.x + 1;
              pCursorRect->xRight = pCursorRect->xLeft + event.theReply.mCursorPosition.width - 1;
              pCursorRect->yTop = GetClientHeight() - event.theReply.mCursorPosition.y;
              pCursorRect->yBottom = pCursorRect->yTop - event.theReply.mCursorPosition.height;

              point.x = 0;
              point.y = 0;

              rc = (MRESULT)QCP_CONVERT;
            }
            else
              rc = (MRESULT)QCP_NOCONVERT;

            result = PR_TRUE;
            break;
          }

        // Mouseclicks: we don't dispatch CLICK events because they just cause
        // trouble: gecko seems to expect EITHER buttondown/up OR click events
        // and so that's what we give it.
    
        case WM_BUTTON1DOWN:
          if (!mIsScrollBar)
            WinSetCapture( HWND_DESKTOP, mWnd);
          result = DispatchMouseEvent( NS_MOUSE_LEFT_BUTTON_DOWN, mp1, mp2);
            // there's no need to clear this on button-up
          gLastButton1Down.x = XFROMMP(mp1);
          gLastButton1Down.y = YFROMMP(mp1);
          WinSetActiveWindow(HWND_DESKTOP, mWnd);
          result = PR_TRUE;
          break;
        case WM_BUTTON1UP:
          if (!mIsScrollBar)
            WinSetCapture( HWND_DESKTOP, 0); // release
          result = DispatchMouseEvent( NS_MOUSE_LEFT_BUTTON_UP, mp1, mp2);
          break;
        case WM_BUTTON1DBLCLK:
          result = DispatchMouseEvent( NS_MOUSE_LEFT_DOUBLECLICK, mp1, mp2);
          break;
    
        case WM_BUTTON2DOWN:
          if (!mIsScrollBar)
            WinSetCapture( HWND_DESKTOP, mWnd);
          result = DispatchMouseEvent( NS_MOUSE_RIGHT_BUTTON_DOWN, mp1, mp2);
          break;
        case WM_BUTTON2UP:
          if (!mIsScrollBar)
            WinSetCapture( HWND_DESKTOP, 0); // release
          result = DispatchMouseEvent( NS_MOUSE_RIGHT_BUTTON_UP, mp1, mp2);
          break;
        case WM_BUTTON2DBLCLK:
          result = DispatchMouseEvent( NS_MOUSE_RIGHT_DOUBLECLICK, mp1, mp2);
          break;
        case WM_CONTEXTMENU:
          if (SHORT2FROMMP(mp2) == TRUE) {
            HWND hwndCurrFocus = WinQueryFocus(HWND_DESKTOP);
            if (hwndCurrFocus != mWnd) {
              WinSendMsg(hwndCurrFocus, msg, mp1, mp2);
            } else {
              result = DispatchMouseEvent( NS_CONTEXTMENU_KEY, mp1, mp2);
            }
          } else {
            result = DispatchMouseEvent( NS_CONTEXTMENU, mp1, mp2);
          }
          break;

          // if MB1 & MB2 are both pressed, perform a copy or paste;
          // see how far the mouse has moved since MB1-down to determine
          // the operation (this really ought to look for selected content)
        case WM_CHORD:
          if (WinGetKeyState(HWND_DESKTOP, VK_BUTTON1) & 
              WinGetKeyState(HWND_DESKTOP, VK_BUTTON2) &
              0x8000) {
            PRBool isCopy = FALSE;
            if (abs(XFROMMP(mp1) - gLastButton1Down.x) >
                  (WinQuerySysValue(HWND_DESKTOP, SV_CXMOTIONSTART) / 2) ||
                abs(YFROMMP(mp1) - gLastButton1Down.y) >
                  (WinQuerySysValue(HWND_DESKTOP, SV_CYMOTIONSTART) / 2))
              isCopy = TRUE;

            nsKeyEvent event(NS_KEY_PRESS, this);
            nsPoint point(0,0);
            InitEvent( event, &point);

            event.keyCode   = NS_VK_INSERT;
            if (isCopy) {
              event.isShift   = PR_FALSE;
              event.isControl = PR_TRUE;
            } else {
              event.isShift   = PR_TRUE;
              event.isControl = PR_FALSE;
            }
            event.isAlt     = PR_FALSE;
            event.isMeta    = PR_FALSE;
            event.eventStructType = NS_KEY_EVENT;
            event.charCode = 0;
            result = DispatchWindowEvent( &event);
          }
          break;

        case WM_BUTTON3DOWN:
          result = DispatchMouseEvent( NS_MOUSE_MIDDLE_BUTTON_DOWN, mp1, mp2);
          break;
        case WM_BUTTON3UP:
          result = DispatchMouseEvent( NS_MOUSE_MIDDLE_BUTTON_UP, mp1, mp2);
          break;
        case WM_BUTTON3DBLCLK:
          result = DispatchMouseEvent( NS_MOUSE_MIDDLE_DOUBLECLICK, mp1, mp2);
          break;
    
        case WM_MOUSEMOVE:
          {
            static POINTL ptlLastPos = { -1, -1 };
            // See if mouse has actually moved.
            if ( ptlLastPos.x == (SHORT)SHORT1FROMMP(mp1) &&
                 ptlLastPos.y == (SHORT)SHORT2FROMMP(mp1)) {
                return PR_TRUE;
            } else {
                // Yes, remember new position.
                ptlLastPos.x = (SHORT)SHORT1FROMMP(mp1);
                ptlLastPos.y = (SHORT)SHORT2FROMMP(mp1);
            }
          }
          result = DispatchMouseEvent( NS_MOUSE_MOVE, mp1, mp2);
          // don't propogate mouse move or the OS will change the pointer
          if (!mIsScrollBar)
            result = PR_TRUE;
          break;
        case WM_MOUSEENTER:
          result = DispatchMouseEvent( NS_MOUSE_ENTER, mp1, mp2);
          break;
        case WM_MOUSELEAVE:
          result = DispatchMouseEvent( NS_MOUSE_EXIT, mp1, mp2);
          break;

        case WM_APPCOMMAND:
        {
          PRUint32 appCommand = GET_APPCOMMAND_LPARAM(mp2);

          switch (appCommand)
          {
            case APPCOMMAND_BROWSER_BACKWARD:
            case APPCOMMAND_BROWSER_FORWARD:
            case APPCOMMAND_BROWSER_REFRESH:
            case APPCOMMAND_BROWSER_STOP:
              DispatchAppCommandEvent(appCommand);
              // tell the driver that we handled the event
              rc = (MRESULT)1;
              result = PR_TRUE;
              break;

            default:
              rc = (MRESULT)0;
              result = PR_FALSE;
              break;
          }
          break;
        }
    
        case WM_HSCROLL:
        case WM_VSCROLL:
          result = OnScroll( msg, mp1, mp2);
          break;

       case WM_ACTIVATE:
#ifdef DEBUG_FOCUS
          printf("[%x] WM_ACTIVATE (%d)\n", this, mWindowIdentifier);
#endif
          if (mp1) {
            /* The window is being activated */
            gJustGotActivate = PR_TRUE;
          } else {
            /* The window is being deactivated */
            gJustGotDeactivate = PR_TRUE;
          }
          break;

        case WM_FOCUSCHANGED:
#ifdef DEBUG_FOCUS
          printf("[%x] WM_FOCUSCHANGED (%d)\n", this, mWindowIdentifier);
#endif
          if (SHORT1FROMMP(mp2)) {
            /* We are receiving focus */
#ifdef DEBUG_FOCUS
            printf("[%x] NS_GOTFOCUS (%d)\n", this, mWindowIdentifier);
#endif
            result = DispatchFocus(NS_GOTFOCUS, isMozWindowTakingFocus);
            /* If mp1 is 0, this is the special WM_FOCUSCHANGED we got */
            /* from the frame activate, so act like we just got activated */
            if (gJustGotActivate || mp1 == 0) {
              gJustGotActivate = PR_FALSE;
              gJustGotDeactivate = PR_FALSE;
#ifdef DEBUG_FOCUS
              printf("[%x] NS_ACTIVATE (%d)\n", this, mWindowIdentifier);
#endif
              result = DispatchFocus(NS_ACTIVATE, isMozWindowTakingFocus);
            }
            if ( WinIsChild( mWnd, HWNDFROMMP(mp1)) && mNextID == 1) {
#ifdef DEBUG_FOCUS
              printf("[%x] NS_PLUGIN_ACTIVATE (%d)\n", this, mWindowIdentifier);
#endif
              result = DispatchFocus(NS_PLUGIN_ACTIVATE, isMozWindowTakingFocus);
              WinSetFocus(HWND_DESKTOP, mWnd);
            }

          } else {
            /* We are losing focus */
            char className[19];
            ::WinQueryClassName((HWND)mp1, 19, className);
            if (strcmp(className, WindowClass()) != 0 && 
                strcmp(className, WC_SCROLLBAR_STRING) != 0) {
               isMozWindowTakingFocus = PR_FALSE;
            }
            if (gJustGotDeactivate) {
               gJustGotDeactivate = PR_FALSE;
#ifdef DEBUG_FOCUS
               printf("[%x] NS_DEACTIVATE (%d)\n", this, mWindowIdentifier);
#endif
               result = DispatchFocus(NS_DEACTIVATE, isMozWindowTakingFocus);
            }
#ifdef DEBUG_FOCUS
            printf("[%x] NS_LOSTFOCUS (%d)\n", this, mWindowIdentifier);
#endif
            result = DispatchFocus(NS_LOSTFOCUS, isMozWindowTakingFocus);
          }
          break;

        case WM_WINDOWPOSCHANGED: 
          result = OnReposition( (PSWP) mp1);
          break;
    
    
        case WM_REALIZEPALETTE:          // hopefully only nsCanvas & nsFrame
          result = OnRealizePalette();   // will need this
          break;
    
        case WM_PRESPARAMCHANGED:
          // This is really for font-change notifies.  Do that first.
          rc = GetPrevWP()( mWnd, msg, mp1, mp2);
          OnPresParamChanged( mp1, mp2);
          result = PR_TRUE;
          break;

          // all msgs that occur when this window is the target of a drag
        case DM_DRAGOVER:
        case DM_DRAGLEAVE:
        case DM_DROP:
        case DM_RENDERCOMPLETE:
        case DM_DROPHELP:
          OnDragDropMsg(msg, mp1, mp2, rc);
          result = PR_TRUE;
          break;
    }
    
    return result;
}


// -----------------------------------------------------------------------
//
// Subclass (or remove the subclass from) this component's nsWindow
//
// -----------------------------------------------------------------------
void nsWindow::SubclassWindow(BOOL bState)
{
  if (NULL != mWnd) {
    NS_PRECONDITION(::WinIsWindow(0, mWnd), "Invalid window handle");
    
    if (bState) {
        // change the nsWindow proc
        mPrevWndProc = WinSubclassWindow(mWnd, fnwpNSWindow);
        NS_ASSERTION(mPrevWndProc, "Null standard window procedure");
        // connect the this pointer to the nsWindow handle
        SetNSWindowPtr(mWnd, this);
    } 
    else {
        WinSubclassWindow(mWnd, mPrevWndProc);
        SetNSWindowPtr(mWnd, NULL);
        mPrevWndProc = NULL;
    }
  }
}

// Overridable OnMessage() methods ------------------------------------------

PRBool nsWindow::OnReposition( PSWP pSwp)
{
   PRBool result = PR_FALSE;
 
   if( pSwp->fl & SWP_MOVE && !(pSwp->fl & SWP_MINIMIZE))
   {
      // need screen coords.
      POINTL ptl = { pSwp->x, pSwp->y + pSwp->cy - 1 };
      WinMapWindowPoints( WinQueryWindow( mWnd, QW_PARENT), GetParentHWND(), &ptl, 1);
      PM2NS_PARENT( ptl);
      mBounds.x = ptl.x;
      mBounds.y = ptl.y;
 
      WinMapWindowPoints( GetParentHWND(), HWND_DESKTOP, &ptl, 1);
      result = OnMove( ptl.x, ptl.y);
   }
   if( pSwp->fl & SWP_SIZE && !(pSwp->fl & SWP_MINIMIZE))
      result = OnResize( pSwp->cx, pSwp->cy);

   return result;
}

PRBool nsWindow::OnRealizePalette()
{
  if (WinQueryWindowUShort(mWnd, QWS_ID) == FID_CLIENT) {
    HWND hwndFocus = WinQueryFocus(HWND_DESKTOP);
    if (WinIsChild(hwndFocus, mWnd)) {
      /* We are getting the focus */
      HPS hps = WinGetPS(hwndFocus);
      nsPaletteOS2::SelectGlobalPalette(hps, hwndFocus);
      WinReleasePS(hps);
      WinInvalidateRect( mWnd, 0, TRUE);
    } else {
      /* We are losing the focus */
      WinInvalidateRect( mWnd, 0, TRUE);
    }
  }

  // Always call the default window procedure
  return PR_TRUE;
}

PRBool nsWindow::OnPresParamChanged( MPARAM mp1, MPARAM mp2)
{
   return PR_FALSE;
}

// This is necessary because notification codes are defined from 1 for each
// control: thus we cannot tell the difference between an EN_SELECT and
// a TABN_SELECT, etc.
// So delegate to those classes who actually want to handle the thing.
PRBool nsWindow::OnControl( MPARAM mp1, MPARAM mp2)
{
   return PR_TRUE; // default to speed things up a bit...
}


//-------------------------------------------------------------------------
//
// WM_DESTROY has been called
//
//-------------------------------------------------------------------------
void nsWindow::OnDestroy()
{
   mOnDestroyCalled = PR_TRUE;

   SubclassWindow( PR_FALSE);
   mWnd = 0;

   // if we were in the middle of deferred window positioning then free up
   if( mSWPs) free( mSWPs);
   mSWPs = 0;
   mlHave = mlUsed = 0;

   // release references to context, toolkit, appshell, children
   nsBaseWidget::OnDestroy();

   // kill font
   delete mFont;

   // dispatching of the event may cause the reference count to drop to 0
   // and result in this object being deleted. To avoid that, add a
   // reference and then release it after dispatching the event.
   //
   // It's important *not* to do this if we're being called from the
   // destructor -- this would result in our destructor being called *again*
   // from the Release() below.  This is very bad...
   if( !(nsWindowState_eDoingDelete & mWindowState) )
   {
      AddRef();
      DispatchStandardEvent( NS_DESTROY );
      Release();
   }

   // dead widget
   mWindowState |= nsWindowState_eDead;
   mWindowState &= ~(nsWindowState_eLive|nsWindowState_ePrecreate|
                     nsWindowState_eInCreate);
}

//-------------------------------------------------------------------------
//
// Move
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnMove(PRInt32 aX, PRInt32 aY)
{            
  // Params here are in XP-space for the desktop
  nsGUIEvent event(NS_MOVE, this);
  InitEvent( event);
  event.point.x = aX;
  event.point.y = aY;

  PRBool result = DispatchWindowEvent( &event);
  NS_RELEASE(event.widget);
  return result;
}

//-------------------------------------------------------------------------
//
// Paint
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnPaint()
{
   PRBool rc = PR_FALSE;

   if( mContext && (mEventCallback || mEventListener))
   {
      // Get rect to redraw and validate window
      RECTL rcl = { 0 };

      // get the current drag status;  if we're currently in a Moz-originated
      // drag, get the special drag HPS then pass it to WinBeginPaint();
      // if there is no hpsDrag, WinBeginPaint() will return a normal HPS
      HPS hpsDrag = 0;
      CheckDragStatus(ACTION_PAINT, &hpsDrag);
      HPS hPS = WinBeginPaint(mWnd, hpsDrag, &rcl);
      nsPaletteOS2::SelectGlobalPalette(hPS, mWnd);

      // if the update rect is empty, suppress the paint event
      if (!WinIsRectEmpty(0, &rcl))
      {
          // call the event callback 
          if (mEventCallback) 
          {
              nsPaintEvent event(NS_PAINT, this);
              InitEvent(event);
     
              // build XP rect from in-ex window rect
              nsRect rect;
              rect.x = rcl.xLeft;
              rect.y = GetClientHeight() - rcl.yTop;
              rect.width = rcl.xRight - rcl.xLeft;
              rect.height = rcl.yTop - rcl.yBottom;
              event.rect = &rect;
              event.region = nsnull;
     
#ifdef NS_DEBUG
          debug_DumpPaintEvent(stdout,
                               this,
                               &event,
                               nsCAutoString("noname"),
                               (PRInt32) mWnd);
#endif // NS_DEBUG

              //nsresult  res;
             
              static NS_DEFINE_CID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
             
              if( NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID, nsnull,
                                                       NS_GET_IID(nsIRenderingContext),
                                                       (void **)&event.renderingContext) )
              {
                 nsIRenderingContextOS2 *winrc;

                 if (NS_OK == event.renderingContext->QueryInterface(NS_GET_IID(nsIRenderingContextOS2), (void **)&winrc))
                 {
                    nsIDrawingSurface* surf;
                   
                    //i know all of this seems a little backwards. i'll fix it, i swear. MMP
                   
                    if (NS_OK == winrc->CreateDrawingSurface(hPS, surf, event.widget))
                    {
                      event.renderingContext->Init(mContext, surf);
                      rc = DispatchWindowEvent(&event);
                      event.renderingContext->DestroyDrawingSurface(surf);
                    }

                    NS_RELEASE(winrc);
                 }
              }
     
              NS_RELEASE(event.renderingContext);
              NS_RELEASE(event.widget);
          }
      }
     
      WinEndPaint(hPS);
      if (hpsDrag)
        ReleaseIfDragHPS(hpsDrag);
   }

   return rc;
}


//-------------------------------------------------------------------------
//
// Send a resize message to the listener
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnResize(PRInt32 aX, PRInt32 aY)
{
   mBounds.width = aX;
   mBounds.height = aY;
   return DispatchResizeEvent( aX, aY);
}

PRBool nsWindow::DispatchResizeEvent( PRInt32 aX, PRInt32 aY)
{
   PRBool result;
   // call the event callback 
   nsSizeEvent event(NS_SIZE, this);
   nsRect      rect( 0, 0, aX, aY);

   InitEvent( event);
   event.windowSize = &rect;             // this is the *client* rectangle
   event.mWinWidth = mBounds.width;
   event.mWinHeight = mBounds.height;

   result = DispatchWindowEvent( &event);
   NS_RELEASE(event.widget);
   return result;
}                                           

//-------------------------------------------------------------------------
//
// Deal with all sort of mouse event
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchMouseEvent( PRUint32 aEventType, MPARAM mp1, MPARAM mp2)
{
  PRBool result = PR_FALSE;

  if (nsnull == mEventCallback && nsnull == mMouseListener) {
    return result;
  }

  nsMouseEvent event(aEventType, this);

  // Mouse leave & enter messages don't seem to have position built in.
  if( aEventType && aEventType != NS_MOUSE_ENTER && aEventType != NS_MOUSE_EXIT)
  {
    POINTL ptl;
    if (aEventType == NS_CONTEXTMENU_KEY) {
      WinQueryPointerPos(HWND_DESKTOP, &ptl);
      WinMapWindowPoints( HWND_DESKTOP, mWnd, &ptl, 1 );
    } else {
      ptl.x = (SHORT)SHORT1FROMMP(mp1);
      ptl.y = (SHORT)SHORT2FROMMP(mp1);
    }
    PM2NS(ptl);
    nsPoint pt( ptl.x, ptl.y);
    InitEvent( event, &pt);

    USHORT usFlags = SHORT2FROMMP( mp2);
    event.isShift = (usFlags & KC_SHIFT) ? PR_TRUE : PR_FALSE;
    event.isControl = (usFlags & KC_CTRL) ? PR_TRUE : PR_FALSE;
    event.isAlt = (usFlags & KC_ALT) ? PR_TRUE : PR_FALSE;
  }
  else
  {
    InitEvent( event, nsnull);
    event.isShift = WinIsKeyDown( VK_SHIFT);
    event.isControl = WinIsKeyDown( VK_CTRL);
    event.isAlt = WinIsKeyDown( VK_ALT) || WinIsKeyDown( VK_ALTGRAF);
  }

  event.isMeta    = PR_FALSE;

  //Dblclicks are used to set the click count, then changed to mousedowns
  if (aEventType == NS_MOUSE_LEFT_DOUBLECLICK ||
      aEventType == NS_MOUSE_RIGHT_DOUBLECLICK) {
    event.message = (aEventType == NS_MOUSE_LEFT_DOUBLECLICK) ? 
                    NS_MOUSE_LEFT_BUTTON_DOWN : NS_MOUSE_RIGHT_BUTTON_DOWN;
    event.clickCount = 2;
  }
  else {
    event.clickCount = 1;
  }

  nsPluginEvent pluginEvent;

  switch (aEventType)//~~~
  {
    case NS_MOUSE_LEFT_BUTTON_DOWN:
      pluginEvent.event = WM_BUTTON1DOWN;
      break;
    case NS_MOUSE_LEFT_BUTTON_UP:
      pluginEvent.event = WM_BUTTON1UP;
      break;
    case NS_MOUSE_LEFT_DOUBLECLICK:
      pluginEvent.event = WM_BUTTON1DBLCLK;
      break;
    case NS_MOUSE_RIGHT_BUTTON_DOWN:
      pluginEvent.event = WM_BUTTON2DOWN;
      break;
    case NS_MOUSE_RIGHT_BUTTON_UP:
      pluginEvent.event = WM_BUTTON2UP;
      break;
    case NS_MOUSE_RIGHT_DOUBLECLICK:
      pluginEvent.event = WM_BUTTON2DBLCLK;
      break;
    case NS_MOUSE_MIDDLE_BUTTON_DOWN:
      pluginEvent.event = WM_BUTTON3DOWN;
      break;
    case NS_MOUSE_MIDDLE_BUTTON_UP:
      pluginEvent.event = WM_BUTTON3UP;
      break;
    case NS_MOUSE_MIDDLE_DOUBLECLICK:
      pluginEvent.event = WM_BUTTON3DBLCLK;
      break;
    case NS_MOUSE_MOVE:
      pluginEvent.event = WM_MOUSEMOVE;
      break;
    default:
      break;
  }

  pluginEvent.wParam = 0;
/* OS2TODO
  pluginEvent.wParam |= (event.isShift) ? MK_SHIFT : 0;
  pluginEvent.wParam |= (event.isControl) ? MK_CONTROL : 0;
*/
  pluginEvent.lParam = MAKELONG(event.point.x, event.point.y);

  event.nativeMsg = (void *)&pluginEvent;

  // call the event callback 
  if (nsnull != mEventCallback) {

    result = DispatchWindowEvent(&event);

#if 0  // OS2TODO
    if (aEventType == NS_MOUSE_MOVE) {

      // if we are not in mouse capture mode (mouse down and hold)
      // then use "this" window
      // if we are in mouse capture, then all events are being directed
      // back to the nsWindow doing the capture. So therefore, the detection
      // of whether we are in a new nsWindow is wrong. Meaning this MOUSE_MOVE
      // event hold the captured windows pointer not the one the mouse is over.
      //
      // So we use "WindowFromPoint" to find what window we are over and 
      // set that window into the mouse trailer timer.
      if (!mIsInMouseCapture) {
        MouseTrailer * mouseTrailer = MouseTrailer::GetMouseTrailer(0);
        MouseTrailer::SetMouseTrailerWindow(this);
        mouseTrailer->CreateTimer();
      } else {
        POINT mp;
        DWORD pos = ::GetMessagePos();
        mp.x      = LOWORD(pos);
        mp.y      = HIWORD(pos);

        // OK, now find out if we are still inside
        // the captured native window
        POINT cpos;
        cpos.x = LOWORD(pos);
        cpos.y = HIWORD(pos);

        nsWindow * someWindow = NULL;
        HWND hWnd = ::WindowFromPoint(mp);
        if (hWnd != NULL) {
          ::ScreenToClient(hWnd, &cpos);
          RECT r;
          VERIFY(::GetWindowRect(hWnd, &r));
          if (cpos.x >= r.left && cpos.x <= r.right &&
              cpos.y >= r.top && cpos.y <= r.bottom) {
            // yes we are so we should be able to get a valid window
            // although, strangley enough when we are on the frame part of the
            // window we get right here when in capture mode
            // but this window won't match the capture mode window so
            // we are ok
            someWindow = (nsWindow*)::GetWindowLong(hWnd, GWL_USERDATA);
          } 
        }
        // only set the window into the mouse trailer if we have a good window
        if (nsnull != someWindow)  {
          MouseTrailer * mouseTrailer = MouseTrailer::GetMouseTrailer(0);
          MouseTrailer::SetMouseTrailerWindow(someWindow);
          mouseTrailer->CreateTimer();
        }
      }

      nsRect rect;
      GetBounds(rect);
      rect.x = 0;
      rect.y = 0;

      if (rect.Contains(event.point.x, event.point.y)) {
        if (gCurrentWindow == NULL || gCurrentWindow != this) {
          if ((nsnull != gCurrentWindow) && (!gCurrentWindow->mIsDestroying)) {
            MouseTrailer::IgnoreNextCycle();
            gCurrentWindow->DispatchMouseEvent(NS_MOUSE_EXIT, gCurrentWindow->GetLastPoint());
          }
          gCurrentWindow = this;
          if (!mIsDestroying) {
            gCurrentWindow->DispatchMouseEvent(NS_MOUSE_ENTER);
          }
        }
      } 
    } else if (aEventType == NS_MOUSE_EXIT) {
      if (gCurrentWindow == this) {
        gCurrentWindow = nsnull;
      }
    }
#endif //OS2TODO
    NS_RELEASE(event.widget);
    return result;
  }

  if (nsnull != mMouseListener) {
    switch (aEventType) {
      case NS_MOUSE_MOVE: {
        result = ConvertStatus(mMouseListener->MouseMoved(event));
        nsRect rect;
        GetBounds(rect);
        if (rect.Contains(event.point.x, event.point.y)) {
          if (gCurrentWindow == NULL || gCurrentWindow != this) {
            gCurrentWindow = this;
          }
        } else {
          //printf("Mouse exit");
        }

      } break;

      case NS_MOUSE_LEFT_BUTTON_DOWN:
      case NS_MOUSE_MIDDLE_BUTTON_DOWN:
      case NS_MOUSE_RIGHT_BUTTON_DOWN:
        result = ConvertStatus(mMouseListener->MousePressed(event));
        break;

      case NS_MOUSE_LEFT_BUTTON_UP:
      case NS_MOUSE_MIDDLE_BUTTON_UP:
      case NS_MOUSE_RIGHT_BUTTON_UP:
        result = ConvertStatus(mMouseListener->MouseReleased(event));
//        result = ConvertStatus(mMouseListener->MouseClicked(event));
        break;

      case NS_MOUSE_LEFT_CLICK:
      case NS_MOUSE_MIDDLE_CLICK:
      case NS_MOUSE_RIGHT_CLICK:
        result = ConvertStatus(mMouseListener->MouseClicked(event));
        break;
    } // switch
  } 

  NS_RELEASE(event.widget);
  return result;
}


//-------------------------------------------------------------------------
//
// Deal with focus messages
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchFocus(PRUint32 aEventType, PRBool isMozWindowTakingFocus)
{
  // call the event callback 
  if (mEventCallback) {
    nsFocusEvent event(aEventType, this);
    InitEvent(event);

    //focus and blur event should go to their base widget loc, not current mouse pos
    event.point.x = 0;
    event.point.y = 0;

    event.isMozWindowTakingFocus = isMozWindowTakingFocus;

    nsPluginEvent pluginEvent;

    switch (aEventType)//~~~
    {
      case NS_GOTFOCUS:
        pluginEvent.event = WM_SETFOCUS;
        break;
      case NS_LOSTFOCUS:
        pluginEvent.event = WM_FOCUSCHANGED;
        break;
      case NS_PLUGIN_ACTIVATE:
        pluginEvent.event = WM_FOCUSCHANGED;
        break;
      default:
        break;
    }

    event.nativeMsg = (void *)&pluginEvent;

    PRBool result = DispatchWindowEvent(&event);
    NS_RELEASE(event.widget);
    return result;
  }
  return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// Deal with scrollbar messages
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnScroll( ULONG msgid, MPARAM mp1, MPARAM mp2)
{
   return( (msgid == WM_HSCROLL) ? OnHScroll(mp1, mp2) : OnVScroll(mp1, mp2) );
}

PRBool nsWindow::OnVScroll( MPARAM mp1, MPARAM mp2)
{
    if (nsnull != mEventCallback) {
        nsMouseScrollEvent scrollEvent(NS_MOUSE_SCROLL, this);
        InitEvent(scrollEvent);
        scrollEvent.isShift = WinIsKeyDown( VK_SHIFT);
        scrollEvent.isControl = WinIsKeyDown( VK_CTRL);
        scrollEvent.isAlt = WinIsKeyDown( VK_ALT) || WinIsKeyDown( VK_ALTGRAF);
        scrollEvent.isMeta = PR_FALSE;
        scrollEvent.scrollFlags = nsMouseScrollEvent::kIsVertical;
        switch (SHORT2FROMMP(mp2)) {
          case SB_LINEUP:
            scrollEvent.delta = -1;
            break;
          case SB_LINEDOWN:
            scrollEvent.delta = 1;
            break;
          case SB_PAGEUP:
            scrollEvent.scrollFlags |= nsMouseScrollEvent::kIsFullPage;
            scrollEvent.delta = -1;
            break;
          case SB_PAGEDOWN:
            scrollEvent.scrollFlags |= nsMouseScrollEvent::kIsFullPage;
            scrollEvent.delta = 1;
            break;
          default:
            scrollEvent.delta = 0;
            break;
        }
        DispatchWindowEvent(&scrollEvent);
        NS_RELEASE(scrollEvent.widget);
    }
    return PR_FALSE;
}

PRBool nsWindow::OnHScroll( MPARAM mp1, MPARAM mp2)
{
    if (nsnull != mEventCallback) {
        nsMouseScrollEvent scrollEvent(NS_MOUSE_SCROLL, this);
        InitEvent(scrollEvent);
        scrollEvent.isShift = WinIsKeyDown( VK_SHIFT);
        scrollEvent.isControl = WinIsKeyDown( VK_CTRL);
        scrollEvent.isAlt = WinIsKeyDown( VK_ALT) || WinIsKeyDown( VK_ALTGRAF);
        scrollEvent.isMeta = PR_FALSE;
        scrollEvent.scrollFlags = nsMouseScrollEvent::kIsHorizontal;
        switch (SHORT2FROMMP(mp2)) {
          case SB_LINELEFT:
            scrollEvent.delta = -1;
            break;
          case SB_LINERIGHT:
            scrollEvent.delta = 1;
            break;
          case SB_PAGELEFT:
            scrollEvent.scrollFlags |= nsMouseScrollEvent::kIsFullPage;
            scrollEvent.delta = -1;
            break;
          case SB_PAGERIGHT:
            scrollEvent.scrollFlags |= nsMouseScrollEvent::kIsFullPage;
            scrollEvent.delta = 1;
            break;
          default:
            scrollEvent.delta = 0;
            break;
        }
        DispatchWindowEvent(&scrollEvent);
        NS_RELEASE(scrollEvent.widget);
    }
    return PR_FALSE;
}

/* On OS/2, if you pass a titlebar > 512 characters, it doesn't display at all. */
/* We are going to limit our titlebars to 256 just to be on the safe side */
#define MAX_TITLEBAR_LENGTH 256

NS_METHOD nsWindow::SetTitle(const nsAString& aTitle) 
{
   // Switch to the PM thread if necessary...
   if( mOS2Toolkit && !mOS2Toolkit->IsGuiThread())
   {
      ULONG ulong = (ULONG) &aTitle;
      MethodInfo info( this, nsWindow::SET_TITLE, 1, &ulong);
      mOS2Toolkit->CallMethod( &info);
   }
   else if (mWnd)
   {
      PRUnichar* uchtemp = ToNewUnicode(aTitle);
      for (PRUint32 i=0;i<aTitle.Length();i++) {
       switch (uchtemp[i]) {
         case 0x2018:
         case 0x2019:
           uchtemp[i] = 0x0027;
           break;
         case 0x201C:
         case 0x201D:
           uchtemp[i] = 0x0022;
           break;
         case 0x2014:
           uchtemp[i] = 0x002D;
           break;
       }
      }

      nsAutoCharBuffer title;
      PRInt32 titleLength;
      WideCharToMultiByte(0, uchtemp, aTitle.Length(), title, titleLength);
      ::WinSetWindowText(GetMainWindow(), title.get());
      if (mChromeHidden) {
         /* If the chrome is hidden, set the text of the titlebar directly */
         if (mFrameWnd) {
            HWND hwndTitleBar = (HWND)::WinQueryProperty(mFrameWnd,
                                                         "hwndTitleBar");
            ::WinSetWindowText(hwndTitleBar, title.get());
         }
      }
      nsMemory::Free(uchtemp);
   }
   return NS_OK;
} 

NS_METHOD nsWindow::SetIcon(const nsAString& anIconSpec) 
{
  // Assume the given string is a local identifier for an icon file.

  nsCOMPtr<nsILocalFile> iconFile;
  ResolveIconName(aIconSpec, NS_LITERAL_STRING(".ico"),
                  getter_AddRefs(iconFile));
  if (!iconFile)
    return NS_OK; // not an error if icon is not found

  // Now try the char* path.
  nsCAutoString path;
  iconFile->GetNativePath( path );

  if (mFrameIcon) {
    WinFreeFileIcon(mFrameIcon);
    mFrameIcon = NULLHANDLE;
  }
  mFrameIcon = WinLoadFileIcon(path.get(), FALSE);
  if (mFrameIcon)
    WinSendMsg(mFrameWnd, WM_SETICON, (MPARAM)mFrameIcon, (MPARAM)0);

  return NS_OK;
}

NS_METHOD nsWindow::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  aWidth = mPreferredWidth;
  aHeight = mPreferredHeight;
  return NS_OK;
}

NS_METHOD nsWindow::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
  mPreferredWidth = aWidth;
  mPreferredHeight = aHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetLastInputEventTime(PRUint32& aTime)
{
   ULONG ulStatus = WinQueryQueueStatus(HWND_DESKTOP);

   // If there is pending input then return the current time.
   if (ulStatus && (QS_KEY | QS_MOUSE | QS_MOUSEBUTTON | QS_MOUSEMOVE)) {
     gLastInputEventTime = PR_IntervalToMicroseconds(PR_IntervalNow());
   } 

   aTime = gLastInputEventTime;

   return NS_OK;
}

// --------------------------------------------------------------------------
// OS2-specific routines to emulate Windows behaviors
// --------------------------------------------------------------------------

BOOL nsWindow::SetWindowPos( HWND ib, long x, long y, long cx, long cy, ULONG flags)
{
   BOOL bDeferred = FALSE;

   if( mParent && mParent->mSWPs) // XXX bit implicit...
   {
      mParent->DeferPosition( GetMainWindow(), ib, x, y, cx, cy, flags);
      bDeferred = TRUE;
   }
   else // WinSetWindowPos appears not to need msgq (hmm)
      WinSetWindowPos( GetMainWindow(), ib, x, y, cx, cy, GetSWPFlags(flags));

   // When the window is actually sized, mBounds will be updated in the fnwp.

   return bDeferred;
}

nsresult nsWindow::GetWindowText( nsString &aStr, PRUint32 *rc)
{
   // Switch to the PM thread if necessary...
   if( !mOS2Toolkit->IsGuiThread())
   {
      ULONG args[] = { (ULONG) &aStr, (ULONG) rc };
      MethodInfo info( this, nsWindow::GET_TITLE, 2, args);
      mOS2Toolkit->CallMethod( &info);
   }
   else if( mWnd)
   {
      // XXX there must be some way to query the text straight into the string!
      int length = WinQueryWindowTextLength( mWnd);
      char *tmp = new char [ length + 1 ];
      WinQueryWindowText( mWnd, length + 1, tmp);
      aStr.AssignWithConversion( tmp);
      delete [] tmp;
   }
   return NS_OK;
}

// --------------------------------------------------------------------------
// Misc helpers -------------------------------------------------------------

// May need to thread-switch these...
void nsWindow::AddToStyle( ULONG style)
{
   if( mWnd)
      WinSetWindowBits( mWnd, QWL_STYLE, style, style);
}

void nsWindow::RemoveFromStyle( ULONG style)
{
   if( mWnd)
   {
      ULONG oldStyle = WinQueryWindowULong( mWnd, QWL_STYLE);
      oldStyle &= ~style;
      WinSetWindowULong( mWnd, QWL_STYLE, oldStyle);
   }
}

// --------------------------------------------------------------------------
// Drag & Drop - Target methods
// --------------------------------------------------------------------------
//
// nsWindow knows almost nothing about d&d except that it can cause
// video corruption if the screen is updated during a drag. It relies
// on nsIDragSessionOS2 to handle native d&d messages and to return
// the status flags it uses to control screen updates.
//
// OnDragDropMsg() handles all of the DM_* messages messages nsWindow
// should ever receive.  CheckDragStatus() determines if a screen update
// is safe and may return a drag HPS if doing so will avoid corruption.
// As far as its author (R.Walsh) can tell, every use is required.
//
// For Moz drags, all while-you-drag features should be fully enabled &
// corruption free;  for native drags, popups & scrolling are suppressed
// but some niceties, e.g. moving the cursor in text fields, are enabled.
//
// --------------------------------------------------------------------------

// This method was designed to be totally ignorant of drag and drop.
// It gives nsIDragSessionOS2 (near) complete control over handling.

PRBool nsWindow::OnDragDropMsg(ULONG msg, MPARAM mp1, MPARAM mp2, MRESULT &mr)
{
  nsresult rv;
  PRUint32 eventType = 0;
  PRUint32 dragFlags = 0;

  mr = 0;
  nsCOMPtr<nsIDragService> dragService =
                    do_GetService("@mozilla.org/widget/dragservice;1", &rv);
  if (dragService) {
    nsCOMPtr<nsIDragSessionOS2> dragSession(
                        do_QueryInterface(dragService, &rv));
    if (dragSession) {

        // handle all possible input without regard to outcome
      switch (msg) {

        case DM_DRAGOVER:
          rv = dragSession->DragOverMsg((PDRAGINFO)mp1, mr, &dragFlags);
          eventType = NS_DRAGDROP_OVER;
          break;

        case DM_DRAGLEAVE:
          rv = dragSession->DragLeaveMsg((PDRAGINFO)mp1, &dragFlags);
          eventType = NS_DRAGDROP_EXIT;
          break;

        case DM_DROP:
          rv = dragSession->DropMsg((PDRAGINFO)mp1, mWnd, &dragFlags);
          eventType = NS_DRAGDROP_DROP;
          break;

        case DM_DROPHELP:
          rv = dragSession->DropHelpMsg((PDRAGINFO)mp1, &dragFlags);
          eventType = NS_DRAGDROP_EXIT;
          break;

        case DM_RENDERCOMPLETE:
          rv = dragSession->RenderCompleteMsg((PDRAGTRANSFER)mp1,
                                              SHORT1FROMMP(mp2), &dragFlags);
          eventType = NS_DRAGDROP_DROP;
          break;

        default:
          rv = NS_ERROR_FAILURE;
      }

        // handle all possible outcomes without regard to their source
      if NS_SUCCEEDED(rv) {
        mDragStatus = gDragStatus = (dragFlags & DND_DragStatus);

        if (dragFlags & DND_DispatchEnterEvent)
          DispatchStandardEvent(NS_DRAGDROP_ENTER);

        if (dragFlags & DND_DispatchEvent)
          DispatchStandardEvent(eventType);

        if (dragFlags & DND_GetDragoverResult)
          dragSession->GetDragoverResult(mr);

        if (dragFlags & DND_ExitSession)
          dragSession->ExitSession(&dragFlags);
      }
    }
  }
    // save final drag status
  gDragStatus = mDragStatus = (dragFlags & DND_DragStatus);

  return PR_TRUE;
}

// --------------------------------------------------------------------------

// CheckDragStatus() concentrates all the hacks needed to avoid video
// corruption during d&d into one place.  The caller specifies an action
// that might be a problem;  the method tells it whether to proceed and
// provides a Drg HPS if the situation calls for one.

PRBool nsWindow::CheckDragStatus(PRUint32 aAction, HPS * oHps)
{
  PRBool rtn    = PR_TRUE;
  PRBool getHps = PR_FALSE;

  switch (aAction) {

      // OnPaint() & Scroll..() - only Moz drags get a Drg hps
    case ACTION_PAINT:
    case ACTION_SCROLL:
      if (gDragStatus & DND_MozDrag)
        getHps = PR_TRUE;
      break;

      // GetNativeData() - Moz drags + native drags over this nsWindow
    case ACTION_DRAW:
      if ((gDragStatus & DND_MozDrag) ||
          (mDragStatus & DND_NativeDrag))
        getHps = PR_TRUE;
      break;

      // Show() - don't show popups during a native dragover
    case ACTION_SHOW:
      if ((gDragStatus & (DND_NativeDrag | DND_InDrop)) == DND_NativeDrag)
        rtn = PR_FALSE;
      break;

      // InitEvent() - use PtrPos while in drag, MsgPos otherwise
    case ACTION_PTRPOS:
      if (!gDragStatus)
        rtn = PR_FALSE;
      break;

    default:
      rtn = PR_FALSE;
  }

    // if the caller wants an HPS, and the current drag status
    // calls for one, *and* a drag hps hasn't already been requested
    // for this window, get the hps;  otherwise, return zero;
    // (if we provide a 2nd hps for a window, the cursor in text
    // fields won't be erased when it's moved to another position)
  if (oHps)
    if (getHps && !mDragHps) {
      mDragHps = DrgGetPS(mWnd);
      *oHps = mDragHps;
    }
    else
      *oHps = 0;

  return rtn;
}

//-------------------------------------------------------------------------

// if there's an outstanding drag hps & it matches the one
// passed in, release it

PRBool nsWindow::ReleaseIfDragHPS(HPS aHps)
{

  if (mDragHps && aHps == mDragHps) {
    DrgReleasePS(mDragHps);
    mDragHps = 0;
    return PR_TRUE;
  }

  return PR_FALSE;
}

// --------------------------------------------------------------------------
// Raptor object access
// --------------------------------------------------------------------------

// 'Native data'
// function to translate from a WM_CHAR to an NS VK_ constant ---------------
PRUint32 WMChar2KeyCode( MPARAM mp1, MPARAM mp2)
{
   PRUint32 rc = 0;
   USHORT   flags = SHORT1FROMMP( mp1);

   // First check for characters.
   // This is complicated by keystrokes such as Ctrl+K not having the KC_CHAR
   // bit set, but thankfully they do have the character actually there.
   //
   // So go on the assumption that `if not vkey or deadkey then char'
   //
   if( !(flags & (KC_VIRTUALKEY | KC_DEADKEY)))
   {
      rc = SHORT1FROMMP(mp2);

      if( rc < 0xFF)
      {
         switch( rc)
         {
            case ';':  rc = NS_VK_SEMICOLON;     break;
            case '=':  rc = NS_VK_EQUALS;        break;
            case '*':  rc = NS_VK_MULTIPLY;      break;
            case '+':  rc = NS_VK_ADD;           break;
            case '-':  rc = NS_VK_SUBTRACT;      break;
            case '.':  rc = NS_VK_PERIOD;        break; // NS_VK_DECIMAL ?
            case '|':  rc = NS_VK_SEPARATOR;     break;
            case ',':  rc = NS_VK_COMMA;         break;
            case '/':  rc = NS_VK_SLASH;         break; // NS_VK_DIVIDE ?
            case '`':  rc = NS_VK_BACK_QUOTE;    break;
            case '(':  rc = NS_VK_OPEN_BRACKET;  break;
            case '\\': rc = NS_VK_BACK_SLASH;    break;
            case ')':  rc = NS_VK_CLOSE_BRACKET; break;
            case '\'': rc = NS_VK_QUOTE;         break;
         }
      }
   }
   else if( flags & KC_VIRTUALKEY)
   {
      USHORT vk = SHORT2FROMMP( mp2);
      if( isNumPadScanCode(CHAR4FROMMP(mp1)) && 
          ( ((flags & KC_ALT) && (CHAR4FROMMP(mp1) != PMSCAN_PADPERIOD)) || 
            ((flags & (KC_CHAR | KC_SHIFT)) == KC_CHAR) ) )
      {
         // No virtual key for Alt+NumPad or NumLock+NumPad
         rc = 0;
      }
      else if( !(flags & KC_CHAR) || isNumPadScanCode(CHAR4FROMMP(mp1)) ||
          (vk == VK_BACKSPACE) || (vk == VK_TAB) || (vk == VK_BACKTAB) ||
          (vk == VK_ENTER) || (vk == VK_NEWLINE) || (vk == VK_SPACE) )
      {
         if( vk >= VK_F1 && vk <= VK_F24)
            rc = NS_VK_F1 + (vk - VK_F1);
         else switch( vk)
         {
            case VK_NUMLOCK:   rc = NS_VK_NUM_LOCK; break;
            case VK_SCRLLOCK:  rc = NS_VK_SCROLL_LOCK; break;
            case VK_ESC:       rc = NS_VK_ESCAPE; break; // NS_VK_CANCEL
            case VK_BACKSPACE: rc = NS_VK_BACK; break;
            case VK_TAB:       rc = NS_VK_TAB; break;
            case VK_BACKTAB:   rc = NS_VK_TAB; break; // layout tests for isShift
            case VK_CLEAR:     rc = NS_VK_CLEAR; break;
            case VK_NEWLINE:   rc = NS_VK_RETURN; break;
            case VK_ENTER:     rc = NS_VK_RETURN; break;
            case VK_SHIFT:     rc = NS_VK_SHIFT; break;
            case VK_CTRL:      rc = NS_VK_CONTROL; break;
            case VK_ALT:       rc = NS_VK_ALT; break;
            case VK_PAUSE:     rc = NS_VK_PAUSE; break;
            case VK_CAPSLOCK:  rc = NS_VK_CAPS_LOCK; break;
            case VK_SPACE:     rc = NS_VK_SPACE; break;
            case VK_PAGEUP:    rc = NS_VK_PAGE_UP; break;
            case VK_PAGEDOWN:  rc = NS_VK_PAGE_DOWN; break;
            case VK_END:       rc = NS_VK_END; break;
            case VK_HOME:      rc = NS_VK_HOME; break;
            case VK_LEFT:      rc = NS_VK_LEFT; break;
            case VK_UP:        rc = NS_VK_UP; break;
            case VK_RIGHT:     rc = NS_VK_RIGHT; break;
            case VK_DOWN:      rc = NS_VK_DOWN; break;
            case VK_PRINTSCRN: rc = NS_VK_PRINTSCREEN; break;
            case VK_INSERT:    rc = NS_VK_INSERT; break;
            case VK_DELETE:    rc = NS_VK_DELETE; break;
         }
      }
   }

   return rc;
}

PCSZ nsWindow::WindowClass()
{
    const PCSZ className = "MozillaWindowClass";

    if (!nsWindow::sIsRegistered)
    {
        nsWindow::sIsRegistered = WinRegisterClass((HAB)0, className,
                                                   WinDefWindowProc, 0, 4);
    }

    return className;
}

ULONG nsWindow::WindowStyle()
{
   return BASE_CONTROL_STYLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
}

ULONG nsWindow::GetFCFlags()
{
  ULONG style = FCF_TITLEBAR | FCF_SYSMENU | FCF_TASKLIST |
                FCF_CLOSEBUTTON | FCF_NOBYTEALIGN |
                (gIsDBCS ? FCF_DBE_APPSTAT : 0);

  if (mWindowType == eWindowType_dialog) {
    style |= FCF_DIALOGBOX;
    if (mBorderStyle == eBorderStyle_default) {
      style |= FCF_DLGBORDER;
    } else {
      style |= FCF_SIZEBORDER | FCF_MINMAX;
    }
  }
  else {
    style |= FCF_SIZEBORDER | FCF_MINMAX;
  }

  if (mWindowType == eWindowType_invisible) {
    style &= ~FCF_TASKLIST;
  }

  if (mBorderStyle != eBorderStyle_default && mBorderStyle != eBorderStyle_all) {
    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_resizeh)) {
      style &= ~FCF_SIZEBORDER;
      style |= FCF_DLGBORDER;
    }
    
    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_border))
      style &= ~(FCF_DLGBORDER | FCF_SIZEBORDER);
    
    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_title)) {
      style &= ~(FCF_TITLEBAR | FCF_TASKLIST);
    }

    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_close))
      style &= ~FCF_CLOSEBUTTON;

    if (mBorderStyle == eBorderStyle_none ||
      !(mBorderStyle & (eBorderStyle_menu | eBorderStyle_close)))
      style &= ~FCF_SYSMENU;
    // Looks like getting rid of the system menu also does away with the
    // close box. So, we only get rid of the system menu if you want neither it
    // nor the close box. How does the Windows "Dialog" window class get just
    // closebox and no sysmenu? Who knows.
    
    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_minimize))
      style &= ~FCF_MINBUTTON;
    
    if (mBorderStyle == eBorderStyle_none || !(mBorderStyle & eBorderStyle_maximize))
      style &= ~FCF_MAXBUTTON;
  }

  return style;
}
