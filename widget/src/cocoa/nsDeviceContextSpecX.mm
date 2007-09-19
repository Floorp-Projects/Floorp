/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Patrick C. Beard <beard@netscape.com>
 *   Simon Fraser     <sfraser@netscape.com>
 *   Conrad Carlen    <ccarlen@netscape.com>
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

#include "nsDeviceContextSpecX.h"

#include "prmem.h"
#include "plstr.h"
#include "nsCRT.h"
#include <unistd.h>

#include "nsIServiceManager.h"
#include "nsIPrintOptions.h"
#include "nsIPrintSettingsX.h"

#include "gfxQuartzSurface.h"
#include "gfxImageSurface.h"


/** -------------------------------------------------------
 *  Construct the nsDeviceContextSpecX
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecX::nsDeviceContextSpecX()
: mPrintSession(NULL)
, mPageFormat(kPMNoPageFormat)
, mPrintSettings(kPMNoPrintSettings)
{
}

/** -------------------------------------------------------
 *  Destroy the nsDeviceContextSpecX
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecX::~nsDeviceContextSpecX()
{
  if (mPrintSession)
    ::PMRelease(mPrintSession);
}

NS_IMPL_ISUPPORTS1(nsDeviceContextSpecX, nsIDeviceContextSpec)

/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecMac
 *  @update   dc 12/02/98
 */
NS_IMETHODIMP nsDeviceContextSpecX::Init(nsIWidget *aWidget,
                                         nsIPrintSettings* aPS,
                                         PRBool aIsPrintPreview)
{
  nsresult rv;

  nsCOMPtr<nsIPrintSettingsX> printSettingsX(do_QueryInterface(aPS));
  if (!printSettingsX)
    return NS_ERROR_NO_INTERFACE;

  rv = printSettingsX->GetNativePrintSession(&mPrintSession);
  if (NS_FAILED(rv))
    return rv;  
  ::PMRetain(mPrintSession);

  rv = printSettingsX->GetPMPageFormat(&mPageFormat);
  if (NS_FAILED(rv))
    return rv;

  rv = printSettingsX->GetPMPrintSettings(&mPrintSettings);
  if (NS_FAILED(rv))
    return rv;

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecX::BeginDocument(PRUnichar*  aTitle, 
                                                  PRUnichar*  aPrintToFileName,
                                                  PRInt32     aStartPage, 
                                                  PRInt32     aEndPage)
{
    if (aTitle) {
      CFStringRef cfString = ::CFStringCreateWithCharacters(NULL, aTitle, nsCRT::strlen(aTitle));
      if (cfString) {
        ::PMSetJobNameCFString(mPrintSettings, cfString);
        ::CFRelease(cfString);
      }
    }

    OSStatus status;
    status = ::PMSetFirstPage(mPrintSettings, aStartPage, false);
    NS_ASSERTION(status == noErr, "PMSetFirstPage failed");
    status = ::PMSetLastPage(mPrintSettings, aEndPage, false);
    NS_ASSERTION(status == noErr, "PMSetLastPage failed");

    status = ::PMSessionBeginCGDocumentNoDialog(mPrintSession, mPrintSettings, mPageFormat);
    if (status != noErr)
      return NS_ERROR_ABORT;

    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecX::EndDocument()
{
    ::PMSessionEndDocumentNoDialog(mPrintSession);
    return NS_OK;
}

/*
NS_IMETHODIMP nsDeviceContextSpecX::AbortDocument()
{
    return EndDocument();
}
*/

NS_IMETHODIMP nsDeviceContextSpecX::BeginPage()
{
    PMSessionError(mPrintSession);
    OSStatus status = ::PMSessionBeginPageNoDialog(mPrintSession, mPageFormat, NULL);
    if (status != noErr) return NS_ERROR_ABORT;
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecX::EndPage()
{
    OSStatus status = ::PMSessionEndPageNoDialog(mPrintSession);
    if (status != noErr) return NS_ERROR_ABORT;
    return NS_OK;
}

void nsDeviceContextSpecX::GetPageRect(double* aTop, double* aLeft, double* aBottom, double* aRight)
{
    PMRect pageRect;
    ::PMGetAdjustedPageRect(mPageFormat, &pageRect);
    *aTop = pageRect.top, *aLeft = pageRect.left;
    *aBottom = pageRect.bottom, *aRight = pageRect.right;
}

NS_IMETHODIMP nsDeviceContextSpecX::GetSurfaceForPrinter(gfxASurface **surface)
{
    double top, left, bottom, right;
    GetPageRect(&top, &left, &bottom, &right);

    const double width = right - left;
    const double height = bottom - top;

    CGContextRef context;
    ::PMSessionGetCGGraphicsContext(mPrintSession, &context);

    nsRefPtr<gfxASurface> newSurface;

    if (context) {
        CGContextTranslateCTM(context, 0.0, height);
        CGContextScaleCTM(context, 1.0, -1.0);
        newSurface = new gfxQuartzSurface(context, gfxSize(width, height));
    } else {
        newSurface = new gfxQuartzSurface(gfxSize((PRInt32)width, (PRInt32)height), gfxASurface::ImageFormatARGB32);
    }

    if (!newSurface)
        return NS_ERROR_FAILURE;

    *surface = newSurface;
    NS_ADDREF(*surface);

    return NS_OK;
}
