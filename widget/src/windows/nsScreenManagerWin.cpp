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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
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

//
// We have to do this in order to have access to the multiple-monitor
// APIs that are only defined when WINVER is >= 0x0500. Don't worry,
// these won't actually be called unless they are present.
//
#undef WINVER
#define WINVER 0x0500

#include "nsScreenManagerWin.h"
#include "nsScreenWin.h"

// needed because there are unicode/ansi versions of this routine
// and we need to make sure we get the correct one.
#ifdef UNICODE
#define GetMonitorInfoQuoted "GetMonitorInfoW"
#else
#define GetMonitorInfoQuoted "GetMonitorInfoA"
#endif


#if _MSC_VER >= 1200
typedef HMONITOR (WINAPI *MonitorFromRectProc)(LPCRECT inRect, DWORD inFlag); 
typedef BOOL (WINAPI *EnumDisplayMonitorsProc)(HDC, LPCRECT, MONITORENUMPROC, LPARAM);

BOOL CALLBACK CountMonitors ( HMONITOR, HDC, LPRECT, LPARAM ioCount ) ;
#else
typedef void* HMONITOR;
#endif


class ScreenListItem
{
public:
  ScreenListItem ( HMONITOR inMon, nsIScreen* inScreen )
    : mMon(inMon), mScreen(inScreen) { } ;
  
  HMONITOR mMon;
  nsCOMPtr<nsIScreen> mScreen;
};


nsScreenManagerWin :: nsScreenManagerWin ( )
: mHasMultiMonitorAPIs(PR_FALSE), mGetMonitorInfoProc(nsnull), mMonitorFromRectProc(nsnull),
    mEnumDisplayMonitorsProc(nsnull), mNumberOfScreens(0)
{
  NS_INIT_REFCNT();

  // figure out if we can call the multiple monitor APIs that are only
  // available on Win98/2000.
  HMODULE lib = GetModuleHandle("user32.dll");
  if ( lib ) {
    mGetMonitorInfoProc = GetProcAddress ( lib, GetMonitorInfoQuoted );
    mMonitorFromRectProc = GetProcAddress ( lib, "MonitorFromRect" );
    mEnumDisplayMonitorsProc = GetProcAddress ( lib, "EnumDisplayMonitors" );
    if ( mGetMonitorInfoProc && mMonitorFromRectProc && mEnumDisplayMonitorsProc )
      mHasMultiMonitorAPIs = PR_TRUE;
  }

  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
}


nsScreenManagerWin :: ~nsScreenManagerWin()
{
  // walk our list of cached screens and delete them.
  for ( int i = 0; i < mScreenList.Count(); ++i ) {
    ScreenListItem* item = NS_REINTERPRET_CAST(ScreenListItem*, mScreenList[i]);
    delete item;
  }
}


// addref, release, QI
NS_IMPL_ISUPPORTS1(nsScreenManagerWin, nsIScreenManager)


//
// CreateNewScreenObject
//
// Utility routine. Creates a new screen object from the given device handle
//
// NOTE: For this "single-monitor" impl, we just always return the cached primary
//        screen. This should change when a multi-monitor impl is done.
//
nsIScreen* 
nsScreenManagerWin :: CreateNewScreenObject ( void* inScreen )
{
  nsIScreen* retScreen = nsnull;
  
  // look through our screen list, hoping to find it. If it's not there,
  // add it and return the new one.
  for ( int i = 0; i < mScreenList.Count(); ++i ) {
    ScreenListItem* curr = NS_REINTERPRET_CAST(ScreenListItem*, mScreenList[i]);
    if ( inScreen == curr->mMon ) {
      NS_IF_ADDREF(retScreen = curr->mScreen.get());
      return retScreen;
    }
  } // for each screen.
 
  retScreen = new nsScreenWin(inScreen);
  ScreenListItem* listItem = new ScreenListItem ( (HMONITOR)inScreen, retScreen );
  mScreenList.AppendElement ( listItem );

  NS_IF_ADDREF(retScreen);
  return retScreen;
}


//
// ScreenForRect 
//
// Returns the screen that contains the rectangle. If the rect overlaps
// multiple screens, it picks the screen with the greatest area of intersection.
//
// The coordinates are in pixels (not twips) and in screen coordinates.
//
NS_IMETHODIMP
nsScreenManagerWin :: ScreenForRect ( PRInt32 inLeft, PRInt32 inTop, PRInt32 inWidth, PRInt32 inHeight,
                                        nsIScreen **outScreen )
{
  if ( !(inWidth || inHeight) ) {
    NS_WARNING ( "trying to find screen for sizeless window, using primary monitor" );
    *outScreen = CreateNewScreenObject ( nsnull );    // addrefs
    return NS_OK;
  }

  RECT globalWindowBounds = { inLeft, inTop, inLeft + inWidth, inTop + inHeight };

  void* genScreen = nsnull;
#if _MSC_VER >= 1200
  if ( mHasMultiMonitorAPIs ) {
    MonitorFromRectProc proc = (MonitorFromRectProc)mMonitorFromRectProc;
    HMONITOR screen = (*proc)( &globalWindowBounds, MONITOR_DEFAULTTOPRIMARY );
    genScreen = screen;

    //XXX find the DC for this screen??
  }
#endif

  *outScreen = CreateNewScreenObject ( genScreen );    // addrefs
  
  return NS_OK;
    
} // ScreenForRect


//
// GetPrimaryScreen
//
// The screen with the menubar/taskbar. This shouldn't be needed very
// often.
//
NS_IMETHODIMP 
nsScreenManagerWin :: GetPrimaryScreen(nsIScreen** aPrimaryScreen) 
{
  *aPrimaryScreen = CreateNewScreenObject ( nsnull );    // addrefs  
  return NS_OK;
  
} // GetPrimaryScreen


#if _MSC_VER >= 1200
//
// CountMonitors
//
// Will be called once for every monitor in the system. Just 
// increments the parameter, which holds a ptr to a PRUin32 holding the
// count up to this point.
//
BOOL CALLBACK
CountMonitors ( HMONITOR, HDC, LPRECT, LPARAM ioParam )
{
  PRUint32* countPtr = NS_REINTERPRET_CAST(PRUint32*, ioParam);
  ++(*countPtr);

  return TRUE; // continue the enumeration

} // CountMonitors
#endif


//
// GetNumberOfScreens
//
// Returns how many physical screens are available.
//
NS_IMETHODIMP
nsScreenManagerWin :: GetNumberOfScreens(PRUint32 *aNumberOfScreens)
{
  if ( mNumberOfScreens )
    *aNumberOfScreens = mNumberOfScreens;
#if _MSC_VER >= 1200
  else if ( mHasMultiMonitorAPIs ) {
      PRUint32 count = 0;
      EnumDisplayMonitorsProc proc = (EnumDisplayMonitorsProc)mEnumDisplayMonitorsProc;
      BOOL result = (*proc)(nsnull, nsnull, (MONITORENUMPROC)CountMonitors, (LPARAM)&count);
      *aNumberOfScreens = mNumberOfScreens = count;      
  } // if there can be > 1 screen
#endif
  else
    *aNumberOfScreens = mNumberOfScreens = 1;

  return NS_OK;
  
} // GetNumberOfScreens

