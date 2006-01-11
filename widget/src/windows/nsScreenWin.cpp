/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

//
// We have to do this in order to have access to the multiple-monitor
// APIs that are only defined when WINVER is >= 0x0500. Don't worry,
// these won't actually be called unless they are present.
//
#undef WINVER
#define WINVER 0x0500

#include "nsScreenWin.h"


// needed because there are unicode/ansi versions of this routine
// and we need to make sure we get the correct one.
#ifdef UNICODE
#define GetMonitorInfoQuoted "GetMonitorInfoW"
#else
#define GetMonitorInfoQuoted "GetMonitorInfoA"
#endif


#if _MSC_VER >= 1200
typedef BOOL (WINAPI *GetMonitorInfoProc)(HMONITOR inMon, LPMONITORINFO ioInfo); 
#endif


nsScreenWin :: nsScreenWin ( void* inScreen )
  : mScreen(inScreen), mHasMultiMonitorAPIs(PR_FALSE),
      mGetMonitorInfoProc(nsnull)
{
  NS_INIT_REFCNT();

#ifdef DEBUG
  HDC hDCScreen = ::GetDC(nsnull);
  NS_ASSERTION(hDCScreen,"GetDC Failure");
  NS_ASSERTION ( ::GetDeviceCaps(hDCScreen, TECHNOLOGY) == DT_RASDISPLAY, "Not a display screen");
  ::ReleaseDC(nsnull,hDCScreen);
#endif
   
#if _MSC_VER >= 1200
  // figure out if we can call the multiple monitor APIs that are only
  // available on Win98/2000.
  HMODULE lib = GetModuleHandle("user32.dll");
  if ( lib ) {
    mGetMonitorInfoProc = GetProcAddress ( lib, GetMonitorInfoQuoted );
    if ( mGetMonitorInfoProc )
      mHasMultiMonitorAPIs = PR_TRUE;
  }
  printf("has multiple monitor apis is %ld\n", mHasMultiMonitorAPIs);
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
NS_IMPL_ISUPPORTS(nsScreenWin, NS_GET_IID(nsIScreen))


NS_IMETHODIMP
nsScreenWin :: GetRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  BOOL success = FALSE;
#if _MSC_VER >= 1200
  if ( mScreen && mHasMultiMonitorAPIs ) {
    GetMonitorInfoProc proc = (GetMonitorInfoProc)mGetMonitorInfoProc;
    MONITORINFO info;
    info.cbSize = sizeof(MONITORINFO);
    success = (*proc)( (HMONITOR)mScreen, &info );
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
#if _MSC_VER >= 1200
  if ( mScreen && mHasMultiMonitorAPIs ) {
    GetMonitorInfoProc proc = (GetMonitorInfoProc)mGetMonitorInfoProc;
    MONITORINFO info;
    info.cbSize = sizeof(MONITORINFO);
    success = (*proc)( (HMONITOR)mScreen, &info );
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

  *aPixelDepth = ::GetDeviceCaps(hDCScreen, BITSPIXEL);

  ::ReleaseDC(nsnull, hDCScreen);
  return NS_OK;

} // GetPixelDepth


NS_IMETHODIMP 
nsScreenWin :: GetColorDepth(PRInt32 *aColorDepth)
{
  return GetPixelDepth(aColorDepth);

} // GetColorDepth


