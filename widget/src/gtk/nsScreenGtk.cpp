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

#include "nsScreenGtk.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>


nsScreenGtk :: nsScreenGtk (  )
{
  NS_INIT_REFCNT();

  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
}


nsScreenGtk :: ~nsScreenGtk()
{
  // nothing to see here.
}


// addref, release, QI
NS_IMPL_ISUPPORTS(nsScreenGtk, NS_GET_IID(nsIScreen))


NS_IMETHODIMP 
nsScreenGtk :: GetWidth(PRInt32 *aWidth)
{
  *aWidth = gdk_screen_width();
  return NS_OK;

} // GetWidth


NS_IMETHODIMP 
nsScreenGtk :: GetHeight(PRInt32 *aHeight)
{
  *aHeight = gdk_screen_height();
  return NS_OK;

} // GetHeight


NS_IMETHODIMP 
nsScreenGtk :: GetPixelDepth(PRInt32 *aPixelDepth)
{
  GdkVisual * rgb_visual = gdk_rgb_get_visual();
  *aPixelDepth = rgb_visual->depth;

  return NS_OK;

} // GetPixelDepth


NS_IMETHODIMP 
nsScreenGtk :: GetColorDepth(PRInt32 *aColorDepth)
{
  return GetPixelDepth ( aColorDepth );

} // GetColorDepth


NS_IMETHODIMP 
nsScreenGtk :: GetAvailWidth(PRInt32 *aAvailWidth)
{
  return GetWidth(aAvailWidth);

} // GetAvailWidth


NS_IMETHODIMP 
nsScreenGtk :: GetAvailHeight(PRInt32 *aAvailHeight)
{
  return GetHeight(aAvailHeight);

} // GetAvailHeight


NS_IMETHODIMP 
nsScreenGtk :: GetAvailLeft(PRInt32 *aAvailLeft)
{
  *aAvailLeft = 0;
  return NS_OK;

} // GetAvailLeft


NS_IMETHODIMP 
nsScreenGtk :: GetAvailTop(PRInt32 *aAvailTop)
{
  *aAvailTop = 0;
  return NS_OK;

} // GetAvailTop

