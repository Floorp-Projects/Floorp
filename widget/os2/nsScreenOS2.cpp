/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsScreenOS2.h"

nsScreenOS2 :: nsScreenOS2 (  )
{
  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
}


nsScreenOS2 :: ~nsScreenOS2()
{
  // nothing to see here.
}


NS_IMETHODIMP
nsScreenOS2 :: GetRect(int32_t *outLeft, int32_t *outTop, int32_t *outWidth, int32_t *outHeight)
{
  LONG alArray[2];

  HPS hps = ::WinGetScreenPS( HWND_DESKTOP);
  HDC hdc = ::GpiQueryDevice (hps);
  ::DevQueryCaps(hdc, CAPS_WIDTH, 2, alArray);
  ::WinReleasePS(hps);

  *outTop = 0;
  *outLeft = 0;
  *outWidth = alArray[0];
  *outHeight = alArray[1];


  return NS_OK;
  
} // GetRect


NS_IMETHODIMP
nsScreenOS2 :: GetAvailRect(int32_t *outLeft, int32_t *outTop, int32_t *outWidth, int32_t *outHeight)
{
  static APIRET rc = 0;
  static BOOL (APIENTRY * pfnQueryDesktopWorkArea)(HWND hwndDesktop, PWRECT pwrcWorkArea) = nullptr;

  GetRect(outLeft, outTop, outWidth, outHeight);

  // This height is different based on whether or not 
  // "Show on top of maximed windows" is checked
  LONG lWorkAreaHeight = *outHeight;

  if ( !rc && !pfnQueryDesktopWorkArea )
  {
      HMODULE hmod = 0;
      rc = DosQueryModuleHandle( "PMMERGE", &hmod );
      if ( !rc )
      {
          rc = DosQueryProcAddr( hmod, 5469, nullptr, (PFN*) &pfnQueryDesktopWorkArea ); // WinQueryDesktopWorkArea
      }
  }
  if ( pfnQueryDesktopWorkArea && !rc )
  {
      RECTL rectl;
      pfnQueryDesktopWorkArea( HWND_DESKTOP, &rectl );
      lWorkAreaHeight = rectl.yTop - rectl.yBottom;
  }

  HWND hwndWarpCenter = WinWindowFromID( HWND_DESKTOP, 0x555 );
  if (hwndWarpCenter) {
    /* If "Show on top of maximized windows is checked" */
    if (lWorkAreaHeight != *outHeight) {
      SWP swp;
      WinQueryWindowPos( hwndWarpCenter, &swp );
      if (swp.y != 0) {
         /* WarpCenter is at the top */
         *outTop += swp.cy;
      }
      *outHeight -= swp.cy;
    }
  }

  return NS_OK;
  
} // GetAvailRect


NS_IMETHODIMP 
nsScreenOS2 :: GetPixelDepth(int32_t *aPixelDepth)
{
  LONG lCap;

  HPS hps = ::WinGetScreenPS( HWND_DESKTOP);
  HDC hdc = ::GpiQueryDevice (hps);

  ::DevQueryCaps(hdc, CAPS_COLOR_BITCOUNT, 1, &lCap);

  *aPixelDepth = (int32_t)lCap;

  
  ::WinReleasePS(hps);

  return NS_OK;

} // GetPixelDepth


NS_IMETHODIMP 
nsScreenOS2 :: GetColorDepth(int32_t *aColorDepth)
{
  return GetPixelDepth ( aColorDepth );

} // GetColorDepth


