/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsScreenWin.h"
#include "nsCoord.h"
#include "gfxWindowsPlatform.h"
#include "nsIWidget.h"

static uint32_t sScreenId;

nsScreenWin::nsScreenWin(HMONITOR inScreen)
  : mScreen(inScreen)
  , mId(++sScreenId)
{
#ifdef DEBUG
  HDC hDCScreen = ::GetDC(nullptr);
  NS_ASSERTION(hDCScreen,"GetDC Failure");
  NS_ASSERTION(::GetDeviceCaps(hDCScreen, TECHNOLOGY) == DT_RASDISPLAY, "Not a display screen");
  ::ReleaseDC(nullptr,hDCScreen);
#endif

  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
}


nsScreenWin::~nsScreenWin()
{
  // nothing to see here.
}


NS_IMETHODIMP
nsScreenWin::GetId(uint32_t *outId)
{
  *outId = mId;
  return NS_OK;
}


NS_IMETHODIMP
nsScreenWin::GetRect(int32_t *outLeft, int32_t *outTop, int32_t *outWidth, int32_t *outHeight)
{
  BOOL success = FALSE;
  if (mScreen) {
    MONITORINFO info;
    info.cbSize = sizeof(MONITORINFO);
    success = ::GetMonitorInfoW(mScreen, &info);
    if (success) {
      *outLeft = info.rcMonitor.left;
      *outTop = info.rcMonitor.top;
      *outWidth = info.rcMonitor.right - info.rcMonitor.left;
      *outHeight = info.rcMonitor.bottom - info.rcMonitor.top;
    }
  }
  if (!success) {
     HDC hDCScreen = ::GetDC(nullptr);
     NS_ASSERTION(hDCScreen,"GetDC Failure");
    
     *outTop = *outLeft = 0;
     *outWidth = ::GetDeviceCaps(hDCScreen, HORZRES);
     *outHeight = ::GetDeviceCaps(hDCScreen, VERTRES); 
     
     ::ReleaseDC(nullptr, hDCScreen);
  }
  return NS_OK;

} // GetRect


NS_IMETHODIMP
nsScreenWin::GetAvailRect(int32_t *outLeft, int32_t *outTop, int32_t *outWidth, int32_t *outHeight)
{
  BOOL success = FALSE;

  if (mScreen) {
    MONITORINFO info;
    info.cbSize = sizeof(MONITORINFO);
    success = ::GetMonitorInfoW(mScreen, &info);
    if (success) {
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

static double
GetDPIScale()
{
  double dpiScale= nsIWidget::DefaultScaleOverride();
  if (dpiScale <= 0.0) {
    dpiScale = gfxWindowsPlatform::GetPlatform()->GetDPIScale(); 
  }
  return dpiScale;
}

NS_IMETHODIMP
nsScreenWin::GetRectDisplayPix(int32_t *outLeft,  int32_t *outTop,
                               int32_t *outWidth, int32_t *outHeight)
{
  int32_t left, top, width, height;
  nsresult rv = GetRect(&left, &top, &width, &height);
  if (NS_FAILED(rv)) {
    return rv;
  }
  double scaleFactor = 1.0 / GetDPIScale();
  *outLeft = NSToIntRound(left * scaleFactor);
  *outTop = NSToIntRound(top * scaleFactor);
  *outWidth = NSToIntRound(width * scaleFactor);
  *outHeight = NSToIntRound(height * scaleFactor);
  return NS_OK;
}

NS_IMETHODIMP
nsScreenWin::GetAvailRectDisplayPix(int32_t *outLeft,  int32_t *outTop,
                                    int32_t *outWidth, int32_t *outHeight)
{
  int32_t left, top, width, height;
  nsresult rv = GetAvailRect(&left, &top, &width, &height);
  if (NS_FAILED(rv)) {
    return rv;
  }
  double scaleFactor = 1.0 / GetDPIScale();
  *outLeft = NSToIntRound(left * scaleFactor);
  *outTop = NSToIntRound(top * scaleFactor);
  *outWidth = NSToIntRound(width * scaleFactor);
  *outHeight = NSToIntRound(height * scaleFactor);
  return NS_OK;
}


NS_IMETHODIMP 
nsScreenWin :: GetPixelDepth(int32_t *aPixelDepth)
{
  //XXX not sure how to get this info for multiple monitors, this might be ok...
  HDC hDCScreen = ::GetDC(nullptr);
  NS_ASSERTION(hDCScreen,"GetDC Failure");

  int32_t depth = ::GetDeviceCaps(hDCScreen, BITSPIXEL);
  if (depth == 32) {
    // If a device uses 32 bits per pixel, it's still only using 8 bits
    // per color component, which is what our callers want to know.
    // (Some devices report 32 and some devices report 24.)
    depth = 24;
  }
  *aPixelDepth = depth;

  ::ReleaseDC(nullptr, hDCScreen);
  return NS_OK;

} // GetPixelDepth


NS_IMETHODIMP 
nsScreenWin::GetColorDepth(int32_t *aColorDepth)
{
  return GetPixelDepth(aColorDepth);

} // GetColorDepth


