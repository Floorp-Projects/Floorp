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
 *   Peter Hartshorn <peter@igelaus.com.au>
 *   Ken Faulkner <faulkner@igelaus.com.au>
 */

#include "nsScreenXlib.h"
#include "xlibrgb.h"
#include <stdlib.h>

nsScreenXlib :: nsScreenXlib (  )
{
  NS_INIT_REFCNT();

  // nothing else to do. I guess we could cache a bunch of information
  // here, but we want to ask the device at runtime in case anything
  // has changed.
}


nsScreenXlib :: ~nsScreenXlib()
{
  // nothing to see here.
}


// addref, release, QI
NS_IMPL_ISUPPORTS1(nsScreenXlib, nsIScreen)


NS_IMETHODIMP
nsScreenXlib :: GetRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  XlibRgbHandle *xrh;
  if (!(xrh = xxlib_find_handle(XXLIBRGB_DEFAULT_HANDLE)))
    abort();
    
  *outTop = 0;
  *outLeft = 0;
  *outWidth = XDisplayWidth(xxlib_rgb_get_display(xrh), 0);
  *outHeight = XDisplayHeight(xxlib_rgb_get_display(xrh), 0);

  return NS_OK;
  
} // GetRect


NS_IMETHODIMP
nsScreenXlib :: GetAvailRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  XlibRgbHandle *xrh;
  if (!(xrh = xxlib_find_handle(XXLIBRGB_DEFAULT_HANDLE)))
    abort();
    
  *outTop = 0;
  *outLeft = 0;
  *outWidth  = XDisplayWidth(xxlib_rgb_get_display(xrh), 0);
  *outHeight = XDisplayHeight(xxlib_rgb_get_display(xrh), 0);

  return NS_OK;
  
} // GetAvailRect


NS_IMETHODIMP 
nsScreenXlib :: GetPixelDepth(PRInt32 *aPixelDepth)
{
  XlibRgbHandle *xrh;
  if (!(xrh = xxlib_find_handle(XXLIBRGB_DEFAULT_HANDLE)))
    abort();
  
  *aPixelDepth = xxlib_rgb_get_depth(xrh);

  return NS_OK;

} // GetPixelDepth


NS_IMETHODIMP 
nsScreenXlib :: GetColorDepth(PRInt32 *aColorDepth)
{
  return GetPixelDepth ( aColorDepth );

} // GetColorDepth


