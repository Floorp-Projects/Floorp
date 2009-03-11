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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

//
// We have to do this in order to have access to the multiple-monitor
// APIs that are only defined when WINVER is >= 0x0500. Don't worry,
// these won't actually be called unless they are present.
//
#undef WINVER
#define WINVER 0x0500
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0500

#include "nsScreenManagerWin.h"
#include "nsScreenWin.h"


BOOL CALLBACK CountMonitors ( HMONITOR, HDC, LPRECT, LPARAM ioCount ) ;

nsScreenManagerWin :: nsScreenManagerWin ( )
  : mNumberOfScreens(0)
{
  // nothing to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
}


nsScreenManagerWin :: ~nsScreenManagerWin()
{
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
  for ( int i = 0; i < mScreenList.Length(); ++i ) {
    ScreenListItem& curr = mScreenList[i];
    if ( inScreen == curr.mMon ) {
      NS_IF_ADDREF(retScreen = curr.mScreen.get());
      return retScreen;
    }
  } // for each screen.
 
  retScreen = new nsScreenWin(inScreen);
  mScreenList.AppendElement ( ScreenListItem ( (HMONITOR)inScreen, retScreen ) );

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

  void* genScreen = ::MonitorFromRect( &globalWindowBounds, MONITOR_DEFAULTTOPRIMARY );

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
  PRUint32* countPtr = reinterpret_cast<PRUint32*>(ioParam);
  ++(*countPtr);

  return TRUE; // continue the enumeration

} // CountMonitors


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
  else {
    PRUint32 count = 0;
    BOOL result = ::EnumDisplayMonitors(nsnull, nsnull, (MONITORENUMPROC)CountMonitors, (LPARAM)&count);
    if (!result)
      return NS_ERROR_FAILURE;
    *aNumberOfScreens = mNumberOfScreens = count;
  }

  return NS_OK;
  
} // GetNumberOfScreens

NS_IMETHODIMP
nsScreenManagerWin :: ScreenForNativeWidget(void *aWidget, nsIScreen **outScreen)
{
  HMONITOR mon = MonitorFromWindow ((HWND) aWidget, MONITOR_DEFAULTTOPRIMARY);
  *outScreen = CreateNewScreenObject (mon);
  return NS_OK;
}
