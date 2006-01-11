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

#include "nsScreenGtk.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <stdlib.h>


nsScreenGtk :: nsScreenGtk (  )
{
  NS_INIT_REFCNT();

  mScreenNum = 0;
  mXOrg = 0;
  mYOrg = 0;
  // these always default to the full screen size
  mWidth = gdk_screen_width();
  mHeight = gdk_screen_height();
}


nsScreenGtk :: ~nsScreenGtk()
{
  // nothing to see here.
}


// addref, release, QI
NS_IMPL_ISUPPORTS1(nsScreenGtk, nsIScreen)


NS_IMETHODIMP
nsScreenGtk :: GetRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  *outLeft = mXOrg;
  *outTop = mYOrg;
  *outWidth = mWidth;
  *outHeight = mHeight;

  return NS_OK;
  
} // GetRect


NS_IMETHODIMP
nsScreenGtk :: GetAvailRect(PRInt32 *outLeft, PRInt32 *outTop, PRInt32 *outWidth, PRInt32 *outHeight)
{
  *outLeft = mXOrg;
  *outTop = mYOrg;
  *outWidth = mWidth;
  *outHeight = mHeight;

  return NS_OK;
  
} // GetAvailRect


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
nsScreenGtk :: GetGammaValue(double *aGammaValue)
{
  // XXX - Add more query options (XSolarisGetVisualGamma, XF86VidModeGetGamma)

#if defined(__sgi)
  *aGammaValue = 2.2/1.7;
  FILE *infile = fopen("/etc/config/system.glGammaVal", "r");
  if (infile) {
    char tmpline[80];

    fgets(tmpline, sizeof(tmpline), infile);
    fclose(infile);
    double sgi_gamma = atof(tmpline);
    if (sgi_gamma > 0.0)
      *aGammaValue = 2.2/sgi_gamma;
  }
#elif defined(NeXT)
  *aGammaValue = 1.0;
#else
  *aGammaValue = 2.2;
#endif

  return NS_OK;
} // GetGammaValue
