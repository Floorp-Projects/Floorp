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
#include "nsObjCExceptions.h"

#include "prmem.h"
#include "plstr.h"
#include "nsCRT.h"
#include <unistd.h>

#include "nsIServiceManager.h"
#include "nsIPrintOptions.h"
#include "nsPrintSettingsX.h"

#include "gfxQuartzSurface.h"
#include "gfxImageSurface.h"

nsDeviceContextSpecX::nsDeviceContextSpecX()
: mPrintSession(NULL)
, mPageFormat(kPMNoPageFormat)
, mPrintSettings(kPMNoPrintSettings)
{
}

nsDeviceContextSpecX::~nsDeviceContextSpecX()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mPrintSession)
    ::PMRelease(mPrintSession);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

NS_IMPL_ISUPPORTS1(nsDeviceContextSpecX, nsIDeviceContextSpec)

NS_IMETHODIMP nsDeviceContextSpecX::Init(nsIWidget *aWidget,
                                         nsIPrintSettings* aPS,
                                         PRBool aIsPrintPreview)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsCOMPtr<nsPrintSettingsX> settings(do_QueryInterface(aPS));
  if (!settings)
    return NS_ERROR_NO_INTERFACE;

  mPrintSession = settings->GetPMPrintSession();
  ::PMRetain(mPrintSession);
  mPageFormat = settings->GetPMPageFormat();
  mPrintSettings = settings->GetPMPrintSettings();

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsDeviceContextSpecX::BeginDocument(PRUnichar*  aTitle, 
                                                  PRUnichar*  aPrintToFileName,
                                                  PRInt32     aStartPage, 
                                                  PRInt32     aEndPage)
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

    if (aTitle) {
      CFStringRef cfString = ::CFStringCreateWithCharacters(NULL, aTitle, nsCRT::strlen(aTitle));
      if (cfString) {
        ::PMPrintSettingsSetJobName(mPrintSettings, cfString);
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

    NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsDeviceContextSpecX::EndDocument()
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

    ::PMSessionEndDocumentNoDialog(mPrintSession);
    return NS_OK;

    NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsDeviceContextSpecX::BeginPage()
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

    PMSessionError(mPrintSession);
    OSStatus status = ::PMSessionBeginPageNoDialog(mPrintSession, mPageFormat, NULL);
    if (status != noErr) return NS_ERROR_ABORT;
    return NS_OK;

    NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsDeviceContextSpecX::EndPage()
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

    OSStatus status = ::PMSessionEndPageNoDialog(mPrintSession);
    if (status != noErr) return NS_ERROR_ABORT;
    return NS_OK;

    NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void nsDeviceContextSpecX::GetPaperRect(double* aTop, double* aLeft, double* aBottom, double* aRight)
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

    PMRect paperRect;
    ::PMGetAdjustedPaperRect(mPageFormat, &paperRect);

    *aTop = paperRect.top, *aLeft = paperRect.left;
    *aBottom = paperRect.bottom, *aRight = paperRect.right;

    NS_OBJC_END_TRY_ABORT_BLOCK;
}

NS_IMETHODIMP nsDeviceContextSpecX::GetSurfaceForPrinter(gfxASurface **surface)
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

    double top, left, bottom, right;
    GetPaperRect(&top, &left, &bottom, &right);
    const double width = right - left;
    const double height = bottom - top;

    CGContextRef context;
    ::PMSessionGetCGGraphicsContext(mPrintSession, &context);

    nsRefPtr<gfxASurface> newSurface;

    if (context) {
        // Initially, origin is at bottom-left corner of the paper.
        // Here, we translate it to top-left corner of the paper.
        CGContextTranslateCTM(context, 0, height);
        CGContextScaleCTM(context, 1.0, -1.0);
        newSurface = new gfxQuartzSurface(context, gfxSize(width, height), PR_TRUE);
    } else {
        newSurface = new gfxQuartzSurface(gfxSize((PRInt32)width, (PRInt32)height), gfxASurface::ImageFormatARGB32, PR_TRUE);
    }

    if (!newSurface)
        return NS_ERROR_FAILURE;

    *surface = newSurface;
    NS_ADDREF(*surface);

    return NS_OK;

    NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
