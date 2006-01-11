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

#include "nsScreenWin.h"


nsScreenWin :: nsScreenWin ( HDC inScreen )
  : mScreen(inScreen)
{
  NS_INIT_REFCNT();

  NS_ASSERTION ( inScreen, "Passing null device to nsScreenWin" );
  NS_ASSERTION ( ::GetDeviceCaps(inScreen, TECHNOLOGY) == DT_RASDISPLAY, "Not a display screen");
  
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
nsScreenWin :: GetWidth(PRInt32 *aWidth)
{
  *aWidth = ::GetDeviceCaps(mScreen, HORZRES);
  return NS_OK;

} // GetWidth


NS_IMETHODIMP 
nsScreenWin :: GetHeight(PRInt32 *aHeight)
{
  *aHeight = ::GetDeviceCaps(mScreen, VERTRES);
  return NS_OK;

} // GetHeight


NS_IMETHODIMP 
nsScreenWin :: GetPixelDepth(PRInt32 *aPixelDepth)
{
  *aPixelDepth = ::GetDeviceCaps(mScreen, BITSPIXEL);
  return NS_OK;

} // GetPixelDepth


NS_IMETHODIMP 
nsScreenWin :: GetColorDepth(PRInt32 *aColorDepth)
{
  return GetPixelDepth(aColorDepth);

} // GetColorDepth


NS_IMETHODIMP 
nsScreenWin :: GetAvailWidth(PRInt32 *aAvailWidth)
{
  // XXX Needs to be rewritten for a non-primary monitor?
  RECT workArea;
  ::SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
  *aAvailWidth = workArea.right - workArea.left;
  return NS_OK;

} // GetAvailWidth


NS_IMETHODIMP 
nsScreenWin :: GetAvailHeight(PRInt32 *aAvailHeight)
{
  RECT workArea;
  ::SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
  *aAvailHeight = workArea.bottom - workArea.top;
  return NS_OK;

} // GetAvailHeight


NS_IMETHODIMP 
nsScreenWin :: GetAvailLeft(PRInt32 *aAvailLeft)
{
  RECT workArea;
  ::SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
  *aAvailLeft = workArea.left;
  return NS_OK;

} // GetAvailLeft


NS_IMETHODIMP 
nsScreenWin :: GetAvailTop(PRInt32 *aAvailTop)
{
  RECT workArea;
  ::SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
  *aAvailTop = workArea.top;
  
  return NS_OK;

} // GetAvailTop

