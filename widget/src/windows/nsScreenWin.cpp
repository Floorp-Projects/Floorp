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

#include "nsScreenWin.h"

#ifdef WINCE
#ifdef WINCE_WINDOWS_MOBILE
#include "sipapi.h"
#endif
#define GetMonitorInfoW GetMonitorInfo
#endif


nsScreenWin :: nsScreenWin ( void* inScreen )
  : mScreen(inScreen)
{
#ifdef DEBUG
  HDC hDCScreen = ::GetDC(nsnull);
  NS_ASSERTION(hDCScreen,"GetDC Failure");
  NS_ASSERTION ( ::GetDeviceCaps(hDCScreen, TECHNOLOGY) == DT_RASDISPLAY, "Not a display screen");
  ::ReleaseDC(nsnull,hDCScreen);
#endif

  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
}


nsScreenWin :: ~nsScreenWin()
{
  // nothing to see here.
}


// addref, release, QI
NS_IMPL_ISUPPORTS1(nsScreenWin, nsIScreen)


NS_IMETHODIMP
nsScreenWin :: GetRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  BOOL success = FALSE;
#if _MSC_VER >= 1200
  if ( mScreen ) {
    MONITORINFO info;
    info.cbSize = sizeof(MONITORINFO);
    success = ::GetMonitorInfoW( (HMONITOR)mScreen, &info );
    if ( success ) {
      *outLeft = info.rcMonitor.left;
      *outTop = info.rcMonitor.top;
      *outWidth = info.rcMonitor.right - info.rcMonitor.left;
      *outHeight = info.rcMonitor.bottom - info.rcMonitor.top;
    }
  }
#endif
  if (!success) {
     HDC hDCScreen = ::GetDC(nsnull);
     NS_ASSERTION(hDCScreen,"GetDC Failure");
    
     *outTop = *outLeft = 0;
     *outWidth = ::GetDeviceCaps(hDCScreen, HORZRES);
     *outHeight = ::GetDeviceCaps(hDCScreen, VERTRES); 
     
     ::ReleaseDC(nsnull, hDCScreen);
  }
  return NS_OK;

} // GetRect


NS_IMETHODIMP
nsScreenWin :: GetAvailRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  BOOL success = FALSE;
#ifdef WINCE_WINDOWS_MOBILE
  SIPINFO sipInfo;
  memset(&sipInfo, 0, sizeof(SIPINFO));
  sipInfo.cbSize = sizeof(SIPINFO);
  if (SipGetInfo(&sipInfo) && !(sipInfo.fdwFlags & SIPF_OFF)) {
    *outLeft = sipInfo.rcVisibleDesktop.left;
    *outTop = sipInfo.rcVisibleDesktop.top;
    *outWidth = sipInfo.rcVisibleDesktop.right - sipInfo.rcVisibleDesktop.left;
    *outHeight = sipInfo.rcVisibleDesktop.bottom - sipInfo.rcVisibleDesktop.top;
    return NS_OK;
  }
#endif

#if _MSC_VER >= 1200
  if ( mScreen ) {
    MONITORINFO info;
    info.cbSize = sizeof(MONITORINFO);
    success = ::GetMonitorInfoW( (HMONITOR)mScreen, &info );
    if ( success ) {
      *outLeft = info.rcWork.left;
      *outTop = info.rcWork.top;
      *outWidth = info.rcWork.right - info.rcWork.left;
      *outHeight = info.rcWork.bottom - info.rcWork.top;
    }
  }
#endif
  if (!success) {
    RECT workArea;
    ::SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
    *outLeft = workArea.left;
    *outTop = workArea.top;
    *outWidth = workArea.right - workArea.left;
    *outHeight = workArea.bottom - workArea.top;
  }

  return NS_OK;
  
} // GetAvailRect



NS_IMETHODIMP 
nsScreenWin :: GetPixelDepth(PRInt32 *aPixelDepth)
{
  //XXX not sure how to get this info for multiple monitors, this might be ok...
  HDC hDCScreen = ::GetDC(nsnull);
  NS_ASSERTION(hDCScreen,"GetDC Failure");

  PRInt32 depth = ::GetDeviceCaps(hDCScreen, BITSPIXEL);
  if (depth == 32) {
    // If a device uses 32 bits per pixel, it's still only using 8 bits
    // per color component, which is what our callers want to know.
    // (Some devices report 32 and some devices report 24.)
    depth = 24;
  }
  *aPixelDepth = depth;

  ::ReleaseDC(nsnull, hDCScreen);
  return NS_OK;

} // GetPixelDepth


NS_IMETHODIMP 
nsScreenWin :: GetColorDepth(PRInt32 *aColorDepth)
{
  return GetPixelDepth(aColorDepth);

} // GetColorDepth


