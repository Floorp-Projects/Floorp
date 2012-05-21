/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsScreenWin.h"


nsScreenWin :: nsScreenWin ( HMONITOR inScreen )
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


NS_IMETHODIMP
nsScreenWin :: GetRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  BOOL success = FALSE;
  if ( mScreen ) {
    MONITORINFO info;
    info.cbSize = sizeof(MONITORINFO);
    success = ::GetMonitorInfoW( mScreen, &info );
    if ( success ) {
      *outLeft = info.rcMonitor.left;
      *outTop = info.rcMonitor.top;
      *outWidth = info.rcMonitor.right - info.rcMonitor.left;
      *outHeight = info.rcMonitor.bottom - info.rcMonitor.top;
    }
  }
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

  if ( mScreen ) {
    MONITORINFO info;
    info.cbSize = sizeof(MONITORINFO);
    success = ::GetMonitorInfoW( mScreen, &info );
    if ( success ) {
      *outLeft = info.rcWork.left;
      *outTop = info.rcWork.top;
      *outWidth = info.rcWork.right - info.rcWork.left;
      *outHeight = info.rcWork.bottom - info.rcWork.top;
    }
  }
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


