/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 et cin: */
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
 *   Peter Weilbacher <mozilla@Weilbacher.org>
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
#ifdef MOZ_CAIRO_GFX
#include "gfxOS2Surface.h"
#include "gfxContext.h"
#else
#include "nsIRenderingContextOS2.h"
#endif
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
#include "nsWidgetAtoms.h"

#include "nsIRollupListener.h"
#include "nsIMenuRollup.h"
#include "nsIRegion.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"

//~~~ windowless plugin support
#include "nsplugindefs.h"

#include "nsITimer.h"
#include "nsIServiceManager.h"

// For SetIcon
#include "nsAppDirectoryServiceDefs.h"
#include "nsXPIDLString.h"
#include "nsIFile.h"

#include "nsOS2Uni.h"
#include "nsPaletteOS2.h"

#include "imgIContainer.h"
#include "gfxIImageFrame.h"

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
  int currentWindowIdentifier = 0;
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
    mCssCursorHPtr      = 0;

    mIsTopWidgetWindow = PR_FALSE;

#ifdef MOZ_CAIRO_GFX
    mThebesSurface = nsnull;
#endif

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
      nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
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

  if (mCssCursorHPtr) {
    WinDestroyPointer(mCssCursorHPtr);
    mCssCursorHPtr = 0;
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
// Initialize an event to dispatch
//
//-------------------------------------------------------------------------
void nsWindow::InitEvent(nsGUIEvent& event, nsPoint* aPoint)
{
  NS_ADDREF(event.widget);

  // if no point was supplied, calculate it
  if (nsnull == aPoint) {
    // for most events, get the message position;  for drag events,
    // msg position may be incorrect, so get the current position instead
    POINTL ptl;
    if (CheckDragStatus(ACTION_PTRPOS, 0))
      WinQueryPointerPos( HWND_DESKTOP, &ptl);
    else
      WinQueryMsgPos( 0/*hab*/, &ptl);

    WinMapWindowPoints( HWND_DESKTOP, mWnd, &ptl, 1);
    PM2NS( ptl);
    event.refPoint.x = ptl.x;
    event.refPoint.y = ptl.y;
  }
  else
  // use the point override if provided
  {
    event.refPoint.x = aPoint->x;
    event.refPoint.y = aPoint->y;
  }

  event.time = WinQueryMsgTime( 0/*hab*/);
  return;
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
  nsGUIEvent event(PR_TRUE, aMsg, this);
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
PRBool nsWindow::DispatchCommandEvent(PRUint32 aEventCommand)
{
  nsCOMPtr<nsIAtom> command;
  switch (aEventCommand) {
    case APPCOMMAND_BROWSER_BACKWARD:
      command = nsWidgetAtoms::Back;
      break;
    case APPCOMMAND_BROWSER_FORWARD:
      command = nsWidgetAtoms::Forward;
      break;
    case APPCOMMAND_BROWSER_REFRESH:
      command = nsWidgetAtoms::Reload;
      break;
    case APPCOMMAND_BROWSER_STOP:
      command = nsWidgetAtoms::Stop;
      break;
    default:
      return PR_FALSE;
  }
  nsCommandEvent event(PR_TRUE, nsWidgetAtoms::onAppCommand, command, this);

  InitEvent(event);
  PRBool result = DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);

  return result;
}

//-------------------------------------------------------------------------
//
// Dispatch DragDrop (target) event
//
//-------------------------------------------------------------------------

PRBool nsWindow::DispatchDragDropEvent(PRUint32 aMsg)
{
  nsMouseEvent event(PR_TRUE, aMsg, this, nsMouseEvent::eReal);
  InitEvent(event);

  event.isShift   = WinIsKeyDown(VK_SHIFT);
  event.isControl = WinIsKeyDown(VK_CTRL);
  event.isAlt     = WinIsKeyDown(VK_ALT) || WinIsKeyDown(VK_ALTGRAF);
  event.isMeta    = PR_FALSE;

  PRBool result = DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);

  return result;
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
   if (aInitData && (aInitData->mWindowType == eWindowType_toplevel))
     DEBUGFOCUS(Create Frame Window);
   else
     DEBUGFOCUS(Create Window);
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
      nsresult rc;
      static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);

      rc = CallCreateInstance(kDeviceContextCID, &mContext);
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
   mBounds.height = aRect.height;

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

#ifdef MOZ_CAIRO_GFX
//-------------------------------------------------------------------------
//
// Create a Thebes surface using the current window handle
//
//-------------------------------------------------------------------------
gfxASurface* nsWindow::GetThebesSurface()
{
  if (mWnd && !mThebesSurface) {
    mThebesSurface = new gfxOS2Surface(mWnd);
  }
  return mThebesSurface;
}
#endif

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
  if (mToolkit && !mOS2Toolkit->IsGuiThread()) {
    MethodInfo info(this, nsWindow::DESTROY);
    mOS2Toolkit->CallMethod(&info);
  } else {
    // avoid calling into other objects if we're being deleted, 'cos
    // they must have no references to us.
    if ((mWindowState & nsWindowState_eLive) && mParent) {
      nsBaseWidget::Destroy();
    }

    // just to be safe. If we're going away and for some reason we're still
    // the rollup widget, rollup and turn off capture.
    if (this == gRollupWidget) {
      if (gRollupListener) {
        gRollupListener->Rollup();
      }
      CaptureRollupEvents(nsnull, PR_FALSE, PR_TRUE);
    }

#ifdef MOZ_CAIRO_GFX
    // Destroy thebes surface now, XXX do we need this at all??
    mThebesSurface = nsnull;
#endif

    if (mWnd) {
      HWND hwndBeingDestroyed = mFrameWnd ? mFrameWnd : mWnd;
      DEBUGFOCUS(Destroy);
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
   printf("+++++++++In NS2PM client height = %d\n", GetClientHeight());
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

// when the frame has its controls, this method is advisory because
// the min/max/restore has already occurred;  only when the frame is
// in kiosk/fullscreen mode does it perform the minimize or restore

NS_IMETHODIMP nsWindow::SetSizeMode(PRInt32 aMode)
{
  nsresult rv;

  // save the requested state
  rv = nsBaseWidget::SetSizeMode(aMode);

  // this is part of a kludge to keep minimized windows from getting
  // restored when they get the focus - we defer the activation event
  // until the window has actually been restored;  see WM_FOCUSCHANGED
  if (gJustGotActivate) {
    DEBUGFOCUS(deferred NS_ACTIVATE);
    gJustGotActivate = PR_FALSE;
    gJustGotDeactivate = PR_FALSE;
    DispatchFocus(NS_ACTIVATE, TRUE);
  }

  // nothing to do in these cases
  if (!NS_SUCCEEDED(rv) || !mChromeHidden || aMode == nsSizeMode_Maximized)
    return rv;

  HWND  hFrame = GetMainWindow();
  ULONG ulStyle = WinQueryWindowULong( hFrame, QWL_STYLE);

  // act on the request if the frame isn't already in the requested state
  if (aMode == nsSizeMode_Minimized) {
    if (!(ulStyle & WS_MINIMIZED))
      WinSetWindowPos(hFrame, 0, 0, 0, 0, 0, SWP_MINIMIZE | SWP_DEACTIVATE);
  }
  else
    if (ulStyle & (WS_MAXIMIZED | WS_MINIMIZED))
      WinSetWindowPos(hFrame, 0, 0, 0, 0, 0, SWP_RESTORE);

  return NS_OK;
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

   // Set cached value for lightweight and printing
   mBounds.x      = aX;
   mBounds.y      = aY;
   mBounds.width  = w;
   mBounds.height = h;

   // WinSetWindowPos() appears not to require a msgq
   if( mWnd)
   {
      // need to keep top-left corner in the same place
      // work out real coords of top left
      POINTL ptl = { aX, aY };
      NS2PM_PARENT( ptl);
      // work out real coords of bottom left
      ptl.y -= h - 1;
      if( mParent && mWindowType != eWindowType_popup)
      {
         WinMapWindowPoints( mParent->mWnd, WinQueryWindow(mWnd, QW_PARENT), &ptl, 1);
      }
      else if (mWindowType == eWindowType_popup ) {
         // aX already gives the right position, just transform aY by hand:
         ptl.y = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN) - h - 1 - aY;
      }

      if( !SetWindowPos( 0, ptl.x, ptl.y, w, h, SWP_MOVE | SWP_SIZE))
         if( aRepaint)
            Invalidate(PR_FALSE);

#if DEBUG_sobotka
   printf("+++++++++++Resized 0x%lx at %ld, %ld to %d x %d (%d,%d)\n", mWnd, ptl.x, ptl.y, w, h, aX, aY);
#endif

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
           DEBUGFOCUS(SetFocus);
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
      char fontNameSize[FACESIZE+5]; // FACESIZE + decimal + up to three digits for font size + null terminator
      char fontName[FACESIZE];
      int  ptSize;
   
      WinQueryPresParam( mWnd, PP_FONTNAMESIZE, 0, 0, FACESIZE+5, fontNameSize, 0);
   
      // FACESIZE = 32, hence the 31 here
      if( 2 == sscanf( fontNameSize, "%d.%31s", &ptSize, fontName))
      {
         nscoord appSize = NSToCoordRound(float(ptSize) * mContext->AppUnitsPerInch() / 72);
   
         nsFont font( fontName, NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
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
      int points = aFont.size * 72 / mContext->AppUnitsPerInch();

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

  switch (aCursor) {
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
  
    case eCursor_n_resize:
    case eCursor_s_resize:
      newPointer = ::WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZENS, FALSE);
      break;
  
    case eCursor_w_resize:
    case eCursor_e_resize:
      newPointer = ::WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZEWE, FALSE);
      break;
  
    case eCursor_nw_resize:
    case eCursor_se_resize:
      newPointer = ::WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZENWSE, FALSE);
      break;
  
    case eCursor_ne_resize:
    case eCursor_sw_resize:
      newPointer = ::WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZENESW, FALSE);
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
      // XXX this CSS3 cursor needs to be implemented
      break;
  
    case eCursor_zoom_in:
      newPointer = gPtrArray[IDC_ZOOMIN-IDC_BASE];
      break;
  
    case eCursor_zoom_out:
      newPointer = gPtrArray[IDC_ZOOMOUT-IDC_BASE];
      break;
  
    case eCursor_not_allowed:
    case eCursor_no_drop:
      newPointer = ::WinQuerySysPointer(HWND_DESKTOP, SPTR_ILLEGAL, FALSE);
      break;
  
    case eCursor_col_resize:
      newPointer = gPtrArray[IDC_COLRESIZE-IDC_BASE];
      break;
  
    case eCursor_row_resize:
      newPointer = gPtrArray[IDC_ROWRESIZE-IDC_BASE];
      break;
  
    case eCursor_vertical_text:
      newPointer = gPtrArray[IDC_VERTICALTEXT-IDC_BASE];
      break;
  
    case eCursor_all_scroll:
      // XXX not 100% appropriate perhaps
      newPointer = ::WinQuerySysPointer(HWND_DESKTOP, SPTR_MOVE, FALSE);
      break;
  
    case eCursor_nesw_resize:
      newPointer = ::WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZENESW, FALSE);
      break;
  
    case eCursor_nwse_resize:
      newPointer = ::WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZENWSE, FALSE);
      break;
  
    case eCursor_ns_resize:
      newPointer = ::WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZENS, FALSE);
      break;
  
    case eCursor_ew_resize:
      newPointer = ::WinQuerySysPointer(HWND_DESKTOP, SPTR_SIZEWE, FALSE);
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

//-------------------------------------------------------------------------

// create a mouse pointer on the fly to support the CSS 'cursor' style;
// this code is based on the Win version by C. Biesinger but has been
// substantially modified to accommodate platform differences and to
// improve efficiency

NS_IMETHODIMP nsWindow::SetCursor(imgIContainer* aCursor,
                                  PRUint32 aHotspotX, PRUint32 aHotspotY)
{

  // if this is the same image as last time, reuse the saved hptr;
  // it will be destroyed when we create a new one or when the
  // current window is destroyed
  if (mCssCursorImg == aCursor && mCssCursorHPtr) {
    WinSetPointer(HWND_DESKTOP, mCssCursorHPtr);
    return NS_OK;
  }

  nsCOMPtr<gfxIImageFrame> frame;
  aCursor->GetFrameAt(0, getter_AddRefs(frame));
  if (!frame)
    return NS_ERROR_NOT_AVAILABLE;

  // if the image is ridiculously large, exit because
  // it will be unrecognizable when shrunk to 32x32
  PRInt32 width, height;
  frame->GetWidth(&width);
  frame->GetHeight(&height);
  if (width > 128 || height > 128)
    return NS_ERROR_FAILURE;

  gfx_format format;
  nsresult rv = frame->GetFormat(&format);
  if (NS_FAILED(rv))
    return rv;

  // only 24-bit images with 0, 1, or 8-bit alpha data are supported
  if (format != gfxIFormats::BGR_A1 && format != gfxIFormats::BGR_A8 &&
      format != gfxIFormats::BGR)
    return NS_ERROR_UNEXPECTED;

  frame->LockImageData();
  PRUint32 dataLen;
  PRUint8* data;
  rv = frame->GetImageData(&data, &dataLen);
  if (NS_FAILED(rv)) {
    frame->UnlockImageData();
    return rv;
  }

  // create the color bitmap
  HBITMAP hBmp = 0;
  hBmp = DataToBitmap(data, width, height, 24);
  frame->UnlockImageData();
  if (!hBmp)
    return NS_ERROR_FAILURE;

  // create a transparency mask from the alpha data;
  HBITMAP hAlpha = 0;

  // image has no alpha data - make the pointer opaque
  if (format == gfxIFormats::BGR) {
    hAlpha = CreateTransparencyMask(format, 0, width, height);
    if (!hAlpha) {
      GpiDeleteBitmap(hBmp);
      return NS_ERROR_FAILURE;
    }
  }
  // image has either 1 or 8 bits of alpha data
  else {
    PRUint8* adata;
    frame->LockAlphaData();
    rv = frame->GetAlphaData(&adata, &dataLen);
    if (NS_FAILED(rv)) {
      GpiDeleteBitmap(hBmp);
      frame->UnlockAlphaData();
      return rv;
    }

    // create the bitmap
    hAlpha = CreateTransparencyMask(format, adata, width, height);
    frame->UnlockAlphaData();
    if (!hAlpha) {
      GpiDeleteBitmap(hBmp);
      return NS_ERROR_FAILURE;
    }
  }

  POINTERINFO info = {0};
  info.fPointer = TRUE;
  info.xHotspot = aHotspotX;
  info.yHotspot = height - aHotspotY - 1;
  info.hbmPointer = hAlpha;
  info.hbmColor = hBmp;

  // create the pointer
  HPOINTER cursor = WinCreatePointerIndirect(HWND_DESKTOP, &info);
  GpiDeleteBitmap(hBmp);
  GpiDeleteBitmap(hAlpha);
  if (cursor == NULL)
    return NS_ERROR_FAILURE;

  // use it
  WinSetPointer(HWND_DESKTOP, cursor);

  // destroy the previous hptr;  this has to be done after the
  // new pointer is set or else WinDestroyPointer() will fail
  if (mCssCursorHPtr)
    WinDestroyPointer(mCssCursorHPtr);

  // save the hptr and a reference to the image for next time
  mCssCursorHPtr = cursor;
  mCssCursorImg = aCursor;

  return NS_OK;
}

//-------------------------------------------------------------------------

// render image or modified alpha data as a native bitmap

// aligned bytes per row, rounded up to next dword bounday
#define ALIGNEDBPR(cx,bits) ( ( ( ((cx)*(bits)) + 31) / 32) * 4)

HBITMAP nsWindow::DataToBitmap(PRUint8* aImageData, PRUint32 aWidth,
                               PRUint32 aHeight, PRUint32 aDepth)
{
  // get a presentation space for this window
  HPS hps = (HPS)GetNativeData(NS_NATIVE_GRAPHIC);
  if (!hps)
    return 0;

  // a handy structure that does double duty
  // as both BITMAPINFOHEADER2 & BITMAPINFO2
  struct {
    BITMAPINFOHEADER2 head;
    RGB2 black;
    RGB2 white;
  } bi;

  memset( &bi, 0, sizeof(bi));
  bi.white.bBlue = 255;
  bi.white.bGreen = 255;
  bi.white.bRed = 255;

  // fill in the particulars
  bi.head.cbFix = sizeof(bi.head);
  bi.head.cx = aWidth;
  bi.head.cy = aHeight;
  bi.head.cPlanes = 1;
  bi.head.cBitCount = aDepth;
  bi.head.ulCompression = BCA_UNCOMP;
  bi.head.cbImage = ALIGNEDBPR(aWidth, aDepth) * aHeight;
  bi.head.cclrUsed = (aDepth == 1 ? 2 : 0);

  // create a bitmap from the image data
  HBITMAP hBmp = GpiCreateBitmap(hps, &bi.head, CBM_INIT,
                 NS_REINTERPRET_CAST(BYTE*, aImageData),
                 (BITMAPINFO2*)&bi);

  // free the hps, then return the bitmap
  FreeNativeData((void*)hps, NS_NATIVE_GRAPHIC);
  return hBmp;
}

//-------------------------------------------------------------------------

// create a monochrome AND/XOR bitmap from 0, 1, or 8-bit alpha data

HBITMAP nsWindow::CreateTransparencyMask(gfx_format format,
                                         PRUint8* aImageData,
                                         PRUint32 aWidth,
                                         PRUint32 aHeight)
{
  // calc width in bytes, rounding up to a dword boundary
  PRUint32 abpr = ALIGNEDBPR(aWidth, 1);
  PRUint32 cbData = abpr * aHeight;

  // alloc space to hold both the AND & XOR bitmaps
  PRUint8* mono = (PRUint8*)malloc(cbData * 2);
  if (!mono)
    return NULL;

  // init the XOR bitmap to produce either black or transparent pixels
  memset(mono, 0x00, cbData);

  switch (format) {

    // make the AND mask opaque
    case gfxIFormats::BGR:
      memset(&mono[cbData], 0x00, cbData);
      break;

    // make the AND mask the inverse of the 1-bit alpha data
    case gfxIFormats::BGR_A1: {
      PRUint32* pSrc = (PRUint32*)aImageData;
      PRUint32* pAnd = (PRUint32*)&mono[cbData];

      for (PRUint32 dataNdx = 0; dataNdx < cbData; dataNdx += 4)
        *pAnd++ = ~(*pSrc++);

      break;
    }

    // make the AND mask the inverse of the 8-bit alpha data
    case gfxIFormats::BGR_A8: {
      PRUint8*  pSrc = aImageData;
      PRUint32* pAnd = (PRUint32*)&mono[cbData];

      // if aWidth isn't a multiple of 4, the input contains byte padding;
      // if aWidth isn't a multiple of 32, the output contains bit padding
      for (PRUint32 dataNdx = 0; dataNdx < cbData; dataNdx += 4) {
        PRUint32 dst = 0;
        PRUint32 colNdx = 0;

        // construct an output dword, then save
        for (PRUint32 byteNdx = 0; byteNdx < 4; byteNdx++) {
          PRUint32 mask = 0x80 << (byteNdx * 8);

          // construct an output byte from 8 input bytes
          for (PRUint32 bitNdx = 0; bitNdx < 8; bitNdx++) {
            if (*pSrc++ < 128)
              dst |= mask;
            mask >>= 1;

            // at the end of a row, skip over any padding in the
            // input, then break out of the byte & dword loops
            if (++colNdx >= aWidth) {
              pSrc += (4 - (aWidth & 3)) & 3;
              break;
            }
          }
          if (colNdx >= aWidth)
            break;
        }
        *pAnd++ = dst;
      }

      break;
    }
  }

  // create the bitmap
  HBITMAP hAlpha = DataToBitmap(mono, aWidth, aHeight * 2, 1);

  // free the buffer, then return the bitmap
  free(mono);
  return hAlpha;
}

//-------------------------------------------------------------------------

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
// OnKey
//-------------------------------------------------------------------------
// Key handler.  Specs for the various text messages are really confused;
// see other platforms for best results of how things are supposed to work.
//
// Perhaps more importantly, the main man listening to these events (besides
// random bits of javascript) is ender -- see 
// mozilla/editor/base/nsEditorEventListeners.cpp.
PRBool nsWindow::OnKey(MPARAM mp1, MPARAM mp2)
{
  nsKeyEvent pressEvent(PR_TRUE, 0, nsnull);
  USHORT fsFlags = SHORT1FROMMP(mp1);
  USHORT usVKey = SHORT2FROMMP(mp2);
  USHORT usChar = SHORT1FROMMP(mp2);
  UCHAR uchScan = CHAR4FROMMP(mp1);

  // It appears we're not supposed to transmit shift, control & alt events
  // to gecko.  Shrug.
  //
  // this is what gtk is doing...
  if (fsFlags & KC_VIRTUALKEY && !(fsFlags & KC_KEYUP) &&
      (usVKey == VK_SHIFT || usVKey == VK_CTRL || usVKey == VK_ALTGRAF) ) {
    return PR_FALSE;
  }

  // My gosh this is ugly
  // Workaround bug where using Alt+Esc let an Alt key creep through
  // Only handle alt by itself if the LONEKEY bit is set
  if ((fsFlags & KC_VIRTUALKEY) && (usVKey == VK_ALT) && !usChar && 
      ((fsFlags & KC_LONEKEY) == 0) && (fsFlags & KC_KEYUP)) {
    return PR_FALSE;
  }

   // Now check if it's a dead-key
  if (fsFlags & KC_DEADKEY) {
    // CUA says we're supposed to give some kind of feedback `display
    // the dead key glyph'.  I'm not sure if we can use the COMPOSE
    // messagesto do this -- it should really be done by someone who can
    // test it & has some idea what `ought' to happen...
    return PR_TRUE;
  }

  // Now dispatch a keyup/keydown event.  This one is *not* meant to
  // have the unicode charcode in.
  nsPoint point(0,0);
  nsKeyEvent event(PR_TRUE, (fsFlags & KC_KEYUP) ? NS_KEY_UP : NS_KEY_DOWN,
                   this);
  InitEvent(event, &point);
  event.keyCode   = WMChar2KeyCode(mp1, mp2);
  event.isShift   = (fsFlags & KC_SHIFT) ? PR_TRUE : PR_FALSE;
  event.isControl = (fsFlags & KC_CTRL) ? PR_TRUE : PR_FALSE;
  event.isAlt     = (fsFlags & KC_ALT) ? PR_TRUE : PR_FALSE;
  event.isMeta    = PR_FALSE;
  event.charCode = 0;

  // Checking for a scroll mouse event vs. a keyboard event
  // The way we know this is that the repeat count is 0 and
  // the key is not physically down
  // unfortunately, there is an exception here - if alt or ctrl are
  // held down, repeat count is set so we have to add special checks for them
  if (((event.keyCode == NS_VK_UP) || (event.keyCode == NS_VK_DOWN)) &&
      !(fsFlags & KC_KEYUP) &&
      ((CHAR3FROMMP(mp1) == 0) || fsFlags & KC_CTRL || fsFlags & KC_ALT) ) {
    if (!(WinGetPhysKeyState(HWND_DESKTOP, uchScan) & 0x8000)) {
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
  PRBool rc = DispatchWindowEvent(&event);

  // Break off now if this was a key-up.
  if (fsFlags & KC_KEYUP) {
    NS_RELEASE(event.widget);
    return rc;
  }

  // Break off if we've got an "invalid composition" -- that is, the user
  // typed a deadkey last time, but has now typed something that doesn't
  // make sense in that context.
  if (fsFlags & KC_INVALIDCOMP) {
    mHaveDeadKey = FALSE;
    // actually, not sure whether we're supposed to abort the keypress
    //     or process it as though the dead key has been pressed.
    NS_RELEASE(event.widget);
    return rc;
  }

  // Now we need to dispatch a keypress event which has the unicode char.
  pressEvent.message = NS_KEY_PRESS;
  if (rc) { // If keydown default was prevented, do same for keypress
    pressEvent.flags |= NS_EVENT_FLAG_NO_DEFAULT;
  }

  if (usChar) {
    USHORT inbuf[2];
    inbuf[0] = usChar;
    inbuf[1] = '\0';

    nsAutoChar16Buffer outbuf;
    PRInt32 bufLength;
    MultiByteToWideChar(0, (const char*)inbuf, 2, outbuf, bufLength);

    pressEvent.charCode = outbuf.get()[0];

    if (pressEvent.isControl && !(fsFlags & (KC_VIRTUALKEY | KC_DEADKEY))) {
      if (!pressEvent.isShift && (pressEvent.charCode >= 'A' && pressEvent.charCode <= 'Z')) {
        pressEvent.charCode = tolower(pressEvent.charCode);
      }
      if (pressEvent.isShift && (pressEvent.charCode >= 'a' && pressEvent.charCode <= 'z')) {
        pressEvent.charCode = toupper(pressEvent.charCode);
      }
      pressEvent.keyCode = 0;
    } else if (!pressEvent.isControl && !pressEvent.isAlt && pressEvent.charCode != 0) {
      if (!(fsFlags & KC_VIRTUALKEY) || // not virtual key
          ((fsFlags & KC_CHAR) && (pressEvent.keyCode == 0)) ) {
        pressEvent.keyCode = 0;
      } else if (usVKey == VK_SPACE) {
        // space key, do nothing here
      } else if ((fsFlags & KC_VIRTUALKEY) &&
                 isNumPadScanCode(uchScan) && pressEvent.keyCode != 0 && isNumlockOn) {
        // this is NumLock+Numpad (no Alt), handle this like a normal number
        pressEvent.keyCode = 0;
      } else { // Real virtual key 
        pressEvent.charCode = 0;
      }
    }
    rc = DispatchWindowEvent(&pressEvent);
  }

  NS_RELEASE(pressEvent.widget);
  return rc;
}

void nsWindow::ConstrainZLevel(HWND *aAfter) {

  nsZLevelEvent  event(PR_TRUE, NS_SETZLEVEL, this);
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

    switch (msg) {
        case WM_COMMAND: // fire off menu selections
        {
           nsMenuEvent event(PR_TRUE, NS_MENU_SELECTED, this);
           event.mCommand = SHORT1FROMMP(mp1);
           InitEvent(event);
           result = DispatchWindowEvent(&event);
           NS_RELEASE(event.widget);
        }
#if 0
        case WM_INITMENU:
          result = OnActivateMenu( HWNDFROMMP(mp2), TRUE);
          break;

        case WM_MENUEND:
          result = OnActivateMenu( HWNDFROMMP(mp2), FALSE);
          break;
#endif

        case WM_CONTROL: // remember this is resent to the orginator...
          result = OnControl( mp1, mp2);
          break;

        case WM_CLOSE:  // close request
        case WM_QUIT:   // interpret WM_QUIT as a close request, too,
                        // so that windows can be closed from the Window List
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
            nsCompositionEvent event(PR_TRUE, NS_COMPOSITION_QUERY, this);
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
          result = DispatchMouseEvent( NS_MOUSE_BUTTON_DOWN, mp1, mp2);
            // there's no need to clear this on button-up
          gLastButton1Down.x = XFROMMP(mp1);
          gLastButton1Down.y = YFROMMP(mp1);
          WinSetActiveWindow(HWND_DESKTOP, mWnd);
          result = PR_TRUE;
          break;
        case WM_BUTTON1UP:
          if (!mIsScrollBar)
            WinSetCapture( HWND_DESKTOP, 0); // release
          result = DispatchMouseEvent( NS_MOUSE_BUTTON_UP, mp1, mp2);
          break;
        case WM_BUTTON1DBLCLK:
          result = DispatchMouseEvent( NS_MOUSE_DOUBLECLICK, mp1, mp2);
          break;
    
        case WM_BUTTON2DOWN:
          if (!mIsScrollBar)
            WinSetCapture( HWND_DESKTOP, mWnd);
          result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, mp1, mp2, PR_FALSE,
                                      nsMouseEvent::eRightButton);
          break;
        case WM_BUTTON2UP:
          if (!mIsScrollBar)
            WinSetCapture( HWND_DESKTOP, 0); // release
          result = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, mp1, mp2, PR_FALSE,
                                      nsMouseEvent::eRightButton);
          break;
        case WM_BUTTON2DBLCLK:
          result = DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, mp1, mp2,
                                      PR_FALSE, nsMouseEvent::eRightButton);
          break;
        case WM_CONTEXTMENU:
          if (SHORT2FROMMP(mp2) == TRUE) {
            HWND hwndCurrFocus = WinQueryFocus(HWND_DESKTOP);
            if (hwndCurrFocus != mWnd) {
              WinSendMsg(hwndCurrFocus, msg, mp1, mp2);
            } else {
              result = DispatchMouseEvent(NS_CONTEXTMENU, mp1, mp2, PR_TRUE,
                                          nsMouseEvent::eRightButton);
            }
          } else {
            result = DispatchMouseEvent(NS_CONTEXTMENU, mp1, mp2, PR_FALSE,
                                        nsMouseEvent::eRightButton);
          }
          break;

          // if MB1 & MB2 are both pressed, perform a copy or paste;
          // see how far the mouse has moved since MB1-down to determine
          // the operation (this really ought to look for selected content)
        case WM_CHORD:
          if (WinGetKeyState(HWND_DESKTOP, VK_BUTTON1) & 
              WinGetKeyState(HWND_DESKTOP, VK_BUTTON2) &
              0x8000) {
            PRBool isCopy = PR_FALSE;
            if (abs(XFROMMP(mp1) - gLastButton1Down.x) >
                  (WinQuerySysValue(HWND_DESKTOP, SV_CXMOTIONSTART) / 2) ||
                abs(YFROMMP(mp1) - gLastButton1Down.y) >
                  (WinQuerySysValue(HWND_DESKTOP, SV_CYMOTIONSTART) / 2)) {
              isCopy = PR_TRUE;
            }

            nsKeyEvent event(PR_TRUE, NS_KEY_PRESS, this);
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
            // OS/2 does not set the Shift, Ctrl, or Alt on keyup
            if (SHORT1FROMMP(mp1) & (KC_VIRTUALKEY|KC_KEYUP|KC_LONEKEY)) {
              USHORT usVKey = SHORT2FROMMP(mp2);
              if (usVKey == VK_SHIFT)
                event.isShift = PR_TRUE;
              if (usVKey == VK_CTRL)
                event.isControl = PR_TRUE;
              if (usVKey == VK_ALTGRAF || usVKey == VK_ALT)
                event.isAlt = PR_TRUE;
            }
            result = DispatchWindowEvent( &event);
          }
          break;

        case WM_BUTTON3DOWN:
          result = DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, mp1, mp2, PR_FALSE,
                                      nsMouseEvent::eMiddleButton);
          break;
        case WM_BUTTON3UP:
          result = DispatchMouseEvent(NS_MOUSE_BUTTON_UP, mp1, mp2, PR_FALSE,
                                      nsMouseEvent::eMiddleButton);
          break;
        case WM_BUTTON3DBLCLK:
          result = DispatchMouseEvent(NS_MOUSE_DOUBLECLICK, mp1, mp2, PR_FALSE,
                                      nsMouseEvent::eMiddleButton);
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
          // don't propagate mouse move or the OS will change the pointer
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
              DispatchCommandEvent(appCommand);
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
          DEBUGFOCUS(WM_ACTIVATE);
          if (mp1)
            gJustGotActivate = PR_TRUE;
          else
            gJustGotDeactivate = PR_TRUE;
          break;

        case WM_FOCUSCHANGED:
        {
          PRBool isMozWindowTakingFocus = PR_TRUE;
          DEBUGFOCUS(WM_FOCUSCHANGED);

          // If the frame was activated earlier or mp1 is 0, dispatch
          // focus & activation events.  However, if the frame is minimized,
          // defer activation and let SetSizeMode() dispatch it after the
          // window has been restored by the user - otherwise, Show() will
          // restore it involuntarily.  

          if (SHORT1FROMMP(mp2)) {
            DEBUGFOCUS(NS_GOTFOCUS);
            result = DispatchFocus(NS_GOTFOCUS, isMozWindowTakingFocus);

            if (gJustGotActivate || mp1 == 0) {
              HWND hActive = WinQueryActiveWindow( HWND_DESKTOP);
              if (!(WinQueryWindowULong( hActive, QWL_STYLE) & WS_MINIMIZED)) {
                DEBUGFOCUS(NS_ACTIVATE);
                gJustGotActivate = PR_FALSE;
                gJustGotDeactivate = PR_FALSE;
                result = DispatchFocus(NS_ACTIVATE, isMozWindowTakingFocus);
              }
            }

            if ( WinIsChild( mWnd, HWNDFROMMP(mp1)) && mNextID == 1) {
              DEBUGFOCUS(NS_PLUGIN_ACTIVATE);
              result = DispatchFocus(NS_PLUGIN_ACTIVATE, isMozWindowTakingFocus);
              WinSetFocus(HWND_DESKTOP, mWnd);
            }
          }
          // We are losing focus
          else {
            char className[19];
            ::WinQueryClassName((HWND)mp1, 19, className);
            if (strcmp(className, WindowClass()) != 0 && 
                strcmp(className, WC_SCROLLBAR_STRING) != 0) {
              isMozWindowTakingFocus = PR_FALSE;
            }

            if (gJustGotDeactivate) {
              DEBUGFOCUS(NS_DEACTIVATE);
              gJustGotDeactivate = PR_FALSE;
              result = DispatchFocus(NS_DEACTIVATE, isMozWindowTakingFocus);
            }

            DEBUGFOCUS(NS_LOSTFOCUS);
            result = DispatchFocus(NS_LOSTFOCUS, isMozWindowTakingFocus);
          }

          break;
        }

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
  nsGUIEvent event(PR_TRUE, NS_MOVE, this);
  InitEvent( event);
  event.refPoint.x = aX;
  event.refPoint.y = aY;

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
  nsEventStatus eventStatus = nsEventStatus_eIgnore;

#ifdef NS_DEBUG
  HRGN debugPaintFlashRegion = NULL;
  HPS debugPaintFlashPS = NULL;

  if (debug_WantPaintFlashing()) {
    debugPaintFlashPS = ::WinGetPS(mWnd);
    debugPaintFlashRegion = ::GpiCreateRegion(debugPaintFlashPS, 0, NULL);
    ::WinQueryUpdateRegion(mWnd, debugPaintFlashRegion);
  } // if paint flashing
#endif

  if (mContext && (mEventCallback || mEventListener)) {
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
    if (!WinIsRectEmpty(0, &rcl)) {
      // call the event callback
      if (mEventCallback) {
        nsPaintEvent event(PR_TRUE, NS_PAINT, this);
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
        debug_DumpPaintEvent(stdout, this, &event, nsCAutoString("noname"),
                             (PRInt32)mWnd);
#endif // NS_DEBUG

#ifdef MOZ_CAIRO_GFX // Thebes code version, adapted from windows/nsWindow.cpp
        nsRefPtr<gfxASurface> targetSurface =
          new gfxOS2Surface(hPS, gfxIntSize(rect.width, rect.height));
        nsRefPtr<gfxContext> thebesContext = new gfxContext(targetSurface);

        nsCOMPtr<nsIRenderingContext> context;
        nsresult rv = mContext->CreateRenderingContextInstance(*getter_AddRefs(context));
        if (NS_FAILED(rv)) {
          NS_WARNING("CreateRenderingContextInstance failed");
          return PR_FALSE;
        }

        rv = context->Init(mContext, thebesContext);
        if (NS_FAILED(rv)) {
          NS_WARNING("context::Init failed");
          return PR_FALSE;
        }

        event.renderingContext = context;
        rc = DispatchWindowEvent(&event, eventStatus);
        event.renderingContext = nsnull;

        if (rc) {
          // Only update if DispatchWindowEvent returned TRUE; otherwise, nothing handled
          // this, and we'll just end up painting with black.
          thebesContext->PopGroupToSource();
          thebesContext->SetOperator(gfxContext::OPERATOR_SOURCE);
          thebesContext->Paint();
        }
#else   // code without Thebes follows
        static NS_DEFINE_CID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);

        if (NS_SUCCEEDED(CallCreateInstance(kRenderingContextCID, &event.renderingContext))) {
          nsIRenderingContextOS2 *winrc;

          if (NS_OK == event.renderingContext->QueryInterface(NS_GET_IID(nsIRenderingContextOS2), (void **)&winrc)) {
            nsIDrawingSurface* surf;

            //i know all of this seems a little backwards. i'll fix it, i swear. MMP

            if (NS_OK == winrc->CreateDrawingSurface(hPS, surf, event.widget)) {
              event.renderingContext->Init(mContext, surf);
              rc = DispatchWindowEvent(&event, eventStatus);
              event.renderingContext->DestroyDrawingSurface(surf);
            }

            NS_RELEASE(winrc);
          } // if event.renderingContext->QI
        } // instance of rendering context

        NS_RELEASE(event.renderingContext);
        NS_RELEASE(event.widget);
#endif  // this was the code without Thebes
      } // if (mEventCallback)
    } // if (!WinIsRectEmpty(0, &rcl))

    WinEndPaint(hPS);
    if (hpsDrag) {
      ReleaseIfDragHPS(hpsDrag);
    }
  } // if (mContext && (mEventCallback || mEventListener))

#ifdef NS_DEBUG
  if (debug_WantPaintFlashing()) {
    // Only flash paint events which have not ignored the paint message.
    // Those that ignore the paint message aren't painting anything so there
    // is only the overhead of the dispatching the paint event.
    if (nsEventStatus_eIgnore != eventStatus) {
      LONG CurMix = ::GpiQueryMix(debugPaintFlashPS);
      ::GpiSetMix(debugPaintFlashPS, FM_INVERT);

      ::GpiPaintRegion(debugPaintFlashPS, debugPaintFlashRegion);
      PR_Sleep(PR_MillisecondsToInterval(30));
      ::GpiPaintRegion(debugPaintFlashPS, debugPaintFlashRegion);
      PR_Sleep(PR_MillisecondsToInterval(30));

      ::GpiSetMix(debugPaintFlashPS, CurMix);
    } // if not eIgnore
    ::GpiDestroyRegion(debugPaintFlashPS, debugPaintFlashRegion);
    ::WinReleasePS(debugPaintFlashPS);
  } // if paint flashing
#endif

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
   nsSizeEvent event(PR_TRUE, NS_SIZE, this);
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
PRBool nsWindow::DispatchMouseEvent(PRUint32 aEventType, MPARAM mp1, MPARAM mp2,
                                    PRBool aIsContextMenuKey, PRInt16 aButton)
{
  PRBool result = PR_FALSE;

  if (nsnull == mEventCallback && nsnull == mMouseListener) {
    return result;
  }

  nsMouseEvent event(PR_TRUE, aEventType, this, nsMouseEvent::eReal,
                     aIsContextMenuKey
                     ? nsMouseEvent::eContextMenuKey
                     : nsMouseEvent::eNormal);
  event.button = aButton;

  // Mouse leave & enter messages don't seem to have position built in.
  if( aEventType && aEventType != NS_MOUSE_ENTER && aEventType != NS_MOUSE_EXIT)
  {
    POINTL ptl;
    if (aEventType == NS_CONTEXTMENU && aIsContextMenuKey) {
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
  if (aEventType == NS_MOUSE_DOUBLECLICK &&
      (aButton == nsMouseEvent::eLeftButton ||
       aButton == nsMouseEvent::eRightButton)) {
    event.message = NS_MOUSE_BUTTON_DOWN;
    event.button = (aButton == nsMouseEvent::eLeftButton) ?
                   nsMouseEvent::eLeftButton : nsMouseEvent::eRightButton;
    event.clickCount = 2;
  }
  else {
    event.clickCount = 1;
  }

  nsPluginEvent pluginEvent;

  switch (aEventType)//~~~
  {
    case NS_MOUSE_BUTTON_DOWN:
      switch (aButton) {
        case nsMouseEvent::eLeftButton:
          pluginEvent.event = WM_BUTTON1DOWN;
          break;
        case nsMouseEvent::eMiddleButton:
          pluginEvent.event = WM_BUTTON3DOWN;
          break;
        case nsMouseEvent::eRightButton:
          pluginEvent.event = WM_BUTTON2DOWN;
          break;
        default:
          break;
      }
      break;
    case NS_MOUSE_BUTTON_UP:
      switch (aButton) {
        case nsMouseEvent::eLeftButton:
          pluginEvent.event = WM_BUTTON1UP;
          break;
        case nsMouseEvent::eMiddleButton:
          pluginEvent.event = WM_BUTTON3UP;
          break;
        case nsMouseEvent::eRightButton:
          pluginEvent.event = WM_BUTTON2UP;
          break;
        default:
          break;
      }
      break;
    case NS_MOUSE_DOUBLECLICK:
      switch (aButton) {
        case nsMouseEvent::eLeftButton:
          pluginEvent.event = WM_BUTTON1DBLCLK;
          break;
        case nsMouseEvent::eMiddleButton:
          pluginEvent.event = WM_BUTTON3DBLCLK;
          break;
        case nsMouseEvent::eRightButton:
          pluginEvent.event = WM_BUTTON2DBLCLK;
          break;
        default:
          break;
      }
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
  pluginEvent.lParam = MAKELONG(event.refPoint.x, event.refPoint.y);

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

      if (rect.Contains(event.refPoint.x, event.refPoint.y)) {
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

    // Release the widget with NS_IF_RELEASE() just in case
    // the context menu key code in nsEventListenerManager::HandleEvent()
    // released it already.
    NS_IF_RELEASE(event.widget);
    return result;
  }

  if (nsnull != mMouseListener) {
    switch (aEventType) {
      case NS_MOUSE_MOVE: {
        result = ConvertStatus(mMouseListener->MouseMoved(event));
        nsRect rect;
        GetBounds(rect);
        if (rect.Contains(event.refPoint.x, event.refPoint.y)) {
          if (gCurrentWindow == NULL || gCurrentWindow != this) {
            gCurrentWindow = this;
          }
        } else {
          //printf("Mouse exit");
        }

      } break;

      case NS_MOUSE_BUTTON_DOWN:
        result = ConvertStatus(mMouseListener->MousePressed(event));
        break;

      case NS_MOUSE_BUTTON_UP:
        result = ConvertStatus(mMouseListener->MouseReleased(event));
//        result = ConvertStatus(mMouseListener->MouseClicked(event));
        break;

      case NS_MOUSE_CLICK:
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
    nsFocusEvent event(PR_TRUE, aEventType, this);
    InitEvent(event);

    //focus and blur event should go to their base widget loc, not current mouse pos
    event.refPoint.x = 0;
    event.refPoint.y = 0;

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
        nsMouseScrollEvent scrollEvent(PR_TRUE, NS_MOUSE_SCROLL, this);
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
        nsMouseScrollEvent scrollEvent(PR_TRUE, NS_MOUSE_SCROLL, this);
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
      if (titleLength > MAX_TITLEBAR_LENGTH) {
        title.get()[MAX_TITLEBAR_LENGTH] = '\0';
      }
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

// this implementation guarantees that sysmenus & minimized windows
// will always have some icon other than the sysmenu default

NS_METHOD nsWindow::SetIcon(const nsAString& aIconSpec) 
{
  static HPOINTER hDefaultIcon = 0;
  HPOINTER        hWorkingIcon = 0;

  // Assume the given string is a local identifier for an icon file.
  nsCOMPtr<nsILocalFile> iconFile;
  ResolveIconName(aIconSpec, NS_LITERAL_STRING(".ico"),
                  getter_AddRefs(iconFile));

  // if the file was found, try to use it
  if (iconFile) {
    nsCAutoString path;
    iconFile->GetNativePath(path);

    if (mFrameIcon)
      WinFreeFileIcon(mFrameIcon);

    mFrameIcon = WinLoadFileIcon(path.get(), FALSE);
    hWorkingIcon = mFrameIcon;
  }

  // if that doesn't work, use the app's icon (let's hope it can be
  // loaded because nobody should have to look at SPTR_APPICON - ugggh!)
  if (hWorkingIcon == 0) {
    if (hDefaultIcon == 0) {
      hDefaultIcon = WinLoadPointer(HWND_DESKTOP, 0, 1);
      if (hDefaultIcon == 0)
        hDefaultIcon =  WinQuerySysPointer(HWND_DESKTOP, SPTR_APPICON, FALSE);
    }
    hWorkingIcon = hDefaultIcon;
  }

  WinSendMsg(mFrameWnd, WM_SETICON, (MPARAM)hWorkingIcon, (MPARAM)0);
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
          dragService->FireDragEventAtSource(NS_DRAGDROP_DRAG);

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
      if (NS_SUCCEEDED(rv)) {
        mDragStatus = gDragStatus = (dragFlags & DND_DragStatus);

        if (dragFlags & DND_DispatchEnterEvent)
          DispatchDragDropEvent(NS_DRAGDROP_ENTER);

        if (dragFlags & DND_DispatchEvent)
          DispatchDragDropEvent(eventType);

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
// function to translate from a WM_CHAR to an NS_VK_ constant ---------------
PRUint32 WMChar2KeyCode(MPARAM mp1, MPARAM mp2)
{
  PRUint32 rc = SHORT1FROMMP(mp2);  // character code
  PRUint32 rcmask = rc & 0x00FF;    // masked character code for key up events
  USHORT sc = CHAR4FROMMP(mp1);     // scan code
  USHORT flags = SHORT1FROMMP(mp1); // flag word

  // First check for characters.
  // This is complicated by keystrokes such as Ctrl+K not having the KC_CHAR
  // bit set, but thankfully they do have the character actually there.
  //
  // Assume that `if not vkey or deadkey or valid number then char'
  if (!(flags & (KC_VIRTUALKEY | KC_DEADKEY)) ||
      (rcmask >= '0' && rcmask <= '9' &&             // handle keys on Numpad, too,
       (isNumPadScanCode(sc) ? isNumlockOn : 1)) ) { // if NumLock is on
    if (flags & KC_KEYUP) { // On OS/2 the scancode is in the upper byte of
                            // usChar when KC_KEYUP is set so mask it off
      rc = rcmask;
    } else { // not KC_KEYUP
      if (! (flags & KC_CHAR)) {
        if ((flags & KC_ALT) || (flags & KC_CTRL))
          rc = rcmask;
        else
          rc = 0;
      }
    }

    if (rc < 0xFF) {
      if (rc >= 'a' && rc <= 'z') { // The DOM_VK are for upper case only so
                                    // if rc is lower case upper case it.
        rc = rc - 'a' + NS_VK_A;
      } else if (rc >= 'A' && rc <= 'Z') { // Upper case
        rc = rc - 'A' + NS_VK_A;
      } else if (rc >= '0' && rc <= '9') {
        // Number keys, including Numpad if NumLock is not set
        rc = rc - '0' + NS_VK_0;
      } else {
        /* For some characters, map the scan code to the NS_VK value */
        /* This only happens in the char case NOT the VK case! */
        switch (sc) {
          case 0x02: rc = NS_VK_1;             break;
          case 0x03: rc = NS_VK_2;             break;
          case 0x04: rc = NS_VK_3;             break;
          case 0x05: rc = NS_VK_4;             break;
          case 0x06: rc = NS_VK_5;             break;
          case 0x07: rc = NS_VK_6;             break;
          case 0x08: rc = NS_VK_7;             break;
          case 0x09: rc = NS_VK_8;             break;
          case 0x0A: rc = NS_VK_9;             break;
          case 0x0B: rc = NS_VK_0;             break;
          case 0x0D: rc = NS_VK_EQUALS;        break;
          case 0x1A: rc = NS_VK_OPEN_BRACKET;  break;
          case 0x1B: rc = NS_VK_CLOSE_BRACKET; break;
          case 0x27: rc = NS_VK_SEMICOLON;     break;
          case 0x28: rc = NS_VK_QUOTE;         break;
          case 0x29: rc = NS_VK_BACK_QUOTE;    break;
          case 0x2B: rc = NS_VK_BACK_SLASH;    break;
          case 0x33: rc = NS_VK_COMMA;         break;
          case 0x34: rc = NS_VK_PERIOD;        break;
          case 0x35: rc = NS_VK_SLASH;         break;
          case 0x37: rc = NS_VK_MULTIPLY;      break;
          case 0x4A: rc = NS_VK_SUBTRACT;      break;
          case 0x4C: rc = NS_VK_CLEAR;         break; // numeric case is handled above
          case 0x4E: rc = NS_VK_ADD;           break;
          case 0x5C: rc = NS_VK_DIVIDE;        break;
          default: break;
        } // switch
      } // else
    } // if (rc < 0xFF)
  } else if (flags & KC_VIRTUALKEY) {
    USHORT vk = SHORT2FROMMP( mp2);
    if (flags & KC_KEYUP) { // On OS/2 there are extraneous bits in the upper byte of
                            // usChar when KC_KEYUP is set so mask them off
      rc = rcmask;
    }
    if (isNumPadScanCode(sc) &&
        (((flags & KC_ALT) && (sc != PMSCAN_PADPERIOD)) ||
          ((flags & (KC_CHAR | KC_SHIFT)) == KC_CHAR)  ||
          ((flags & KC_KEYUP) && rc != 0) )) {
      CHAR numpadMap[] = {NS_VK_NUMPAD7, NS_VK_NUMPAD8, NS_VK_NUMPAD9, 0,
                          NS_VK_NUMPAD4, NS_VK_NUMPAD5, NS_VK_NUMPAD6, 0,
                          NS_VK_NUMPAD1, NS_VK_NUMPAD2, NS_VK_NUMPAD3,
                          NS_VK_NUMPAD0, NS_VK_DECIMAL};
      // If this is the Numpad must not return VK for ALT+Numpad or ALT+NumLock+Numpad
      // NumLock+Numpad is OK
      if (numpadMap[sc - PMSCAN_PAD7] != 0) { // not plus or minus on Numpad
        if (flags & KC_ALT) // do not react on Alt plus ASCII-code sequences
          rc = 0;
        else
          rc = numpadMap[sc - PMSCAN_PAD7];
      } else {                                // plus or minus of Numpad
        rc = 0; // No virtual key for Alt+Numpad or NumLock+Numpad
      }
    } else if (!(flags & KC_CHAR) || isNumPadScanCode(sc) ||
               (vk == VK_BACKSPACE) || (vk == VK_TAB) || (vk == VK_BACKTAB) ||
               (vk == VK_ENTER) || (vk == VK_NEWLINE) || (vk == VK_SPACE)) {
      if (vk >= VK_F1 && vk <= VK_F24) {
        rc = NS_VK_F1 + (vk - VK_F1);
      }
      else switch (vk) {
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
      } // switch
    }
  } // KC_VIRTUALKEY

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
                FCF_CLOSEBUTTON | FCF_NOBYTEALIGN | FCF_AUTOICON |
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
