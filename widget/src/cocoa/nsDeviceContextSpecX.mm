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

#include "nsToolkit.h"

#include "gfxQuartzSurface.h"
#include "gfxImageSurface.h"

// These symbols don't exist on 10.3, but we use them on 10.4 and greater
#if MAC_OS_X_VERSION_MAX_ALLOWED < MAX_OS_X_VERSION_10_4
extern PMSessionGetCGGraphicsContext(PMPrintSession, CGContextRef *);
extern PMSessionBeginCGDocumentNoDialog(PMPrintSession, PMPrintSettings, PMPageFormat);
#endif

// These functions don't exist on 10.3, which we build against. However, we want to be able to use them on
// 10.4, so we need to access them at runtime.
typedef OSStatus (*fpPMSessionBeginCGDocumentNoDialog_type)(PMPrintSession, PMPrintSettings, PMPageFormat);
typedef OSStatus (*fpPMSessionGetCGGraphicsContext_type)(PMPrintSession, CGContextRef*);
static fpPMSessionBeginCGDocumentNoDialog_type fpPMSessionBeginCGDocumentNoDialog = NULL;
static fpPMSessionGetCGGraphicsContext_type fpPMSessionGetCGGraphicsContext = NULL;

/** -------------------------------------------------------
 *  Construct the nsDeviceContextSpecX
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecX::nsDeviceContextSpecX()
: mPrintSession(NULL)
, mPageFormat(kPMNoPageFormat)
, mPrintSettings(kPMNoPrintSettings)
, mBeganPrinting(PR_FALSE)
{
    //We have to load these at runtime to use them on 10.4, since we build against 10.3.
    if (nsToolkit::OnTigerOrLater()) {
        CFBundleRef bundle = ::CFBundleGetBundleWithIdentifier(CFSTR("com.apple.ApplicationServices"));
        if (bundle) {
            fpPMSessionBeginCGDocumentNoDialog = (fpPMSessionBeginCGDocumentNoDialog_type)
              ::CFBundleGetFunctionPointerForName(bundle, CFSTR("PMSessionBeginCGDocumentNoDialog"));
            fpPMSessionGetCGGraphicsContext = (fpPMSessionGetCGGraphicsContext_type)
              ::CFBundleGetFunctionPointerForName(bundle, CFSTR("PMSessionGetCGGraphicsContext"));
        }
    }
}

/** -------------------------------------------------------
 *  Destroy the nsDeviceContextSpecX
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecX::~nsDeviceContextSpecX()
{
  if (mPrintSession)
    ::PMRelease(mPrintSession);
  ClosePrintManager();
}

#ifdef MOZ_CAIRO_GFX
NS_IMPL_ISUPPORTS1(nsDeviceContextSpecX, nsIDeviceContextSpec)
#else
NS_IMPL_ISUPPORTS2(nsDeviceContextSpecX, nsIDeviceContextSpec, nsIPrintingContext)
#endif
/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecMac
 *  @update   dc 12/02/98
 */
NS_IMETHODIMP nsDeviceContextSpecX::Init(nsIWidget *aWidget,
                                         nsIPrintSettings* aPS,
                                         PRBool aIsPrintPreview)
{
  return Init(aPS, aIsPrintPreview);
}

NS_IMETHODIMP nsDeviceContextSpecX::Init(nsIPrintSettings* aPS, PRBool aIsPrintPreview)
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

NS_IMETHODIMP nsDeviceContextSpecX::PrintManagerOpen(PRBool* aIsOpen)
{
    *aIsOpen = mBeganPrinting;
    return NS_OK;
}

/** -------------------------------------------------------
 * Closes the printmanager if it is open.
 * @update   dc 12/03/98
 */
NS_IMETHODIMP nsDeviceContextSpecX::ClosePrintManager()
{
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

    if (nsToolkit::OnTigerOrLater() && fpPMSessionBeginCGDocumentNoDialog) {
        //On 10.4 and above, we can easily use CoreGraphics
        status = fpPMSessionBeginCGDocumentNoDialog(mPrintSession, mPrintSettings, mPageFormat);
    } else {
        //Not so on 10.3. The following (taken from Apple sample code) allows us to get a CGContextRef later on 10.3
        CFStringRef strings[1];
        CFArrayRef graphicsContextsArray;

        strings[0] = kPMGraphicsContextCoreGraphics;
        graphicsContextsArray = ::CFArrayCreate(kCFAllocatorDefault, (const void **)strings, 1, &kCFTypeArrayCallBacks);

        if (graphicsContextsArray != NULL) {
            ::PMSessionSetDocumentFormatGeneration(mPrintSession, kPMDocumentFormatPDF, graphicsContextsArray, NULL);
            ::CFRelease(graphicsContextsArray);
        }
        //Actually create the document
        status = ::PMSessionBeginDocumentNoDialog(mPrintSession, mPrintSettings, mPageFormat);
    }
    
    if (status != noErr) return NS_ERROR_ABORT;

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

NS_IMETHODIMP nsDeviceContextSpecX::GetPrinterResolution(double* aResolution)
{
    PMPrinter printer;
    OSStatus status = ::PMSessionGetCurrentPrinter(mPrintSession, &printer);
    if (status != noErr)
        return NS_ERROR_FAILURE;
      
    PMResolution defaultResolution;
    status = ::PMPrinterGetPrinterResolution(printer, kPMDefaultResolution, &defaultResolution);
    if (status != noErr)
        return NS_ERROR_FAILURE;
    
    *aResolution = defaultResolution.hRes;
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecX::GetPageRect(double* aTop, double* aLeft, double* aBottom, double* aRight)
{
    PMRect pageRect;
    ::PMGetAdjustedPageRect(mPageFormat, &pageRect);
    *aTop = pageRect.top, *aLeft = pageRect.left;
    *aBottom = pageRect.bottom, *aRight = pageRect.right;
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecX::GetSurfaceForPrinter(gfxASurface **surface)
{
    double top, left, bottom, right;
    GetPageRect(&top, &left, &bottom, &right);

    const double width = right - left;
    const double height = bottom - top;

    //On 10.4 or later, just use CG natively. On 10.3, though, we have to request CG specifically.
    CGContextRef context;
    if (nsToolkit::OnTigerOrLater() && fpPMSessionGetCGGraphicsContext) {
        fpPMSessionGetCGGraphicsContext(mPrintSession, &context);
    } else {
        ::PMSessionGetGraphicsContext(mPrintSession, kPMGraphicsContextCoreGraphics, (void **)&context);
    }
    
    nsRefPtr<gfxASurface> newSurface;

    if (context)
        newSurface = new gfxQuartzSurface(context, PR_FALSE, gfxSize(width, height));
    else
        newSurface = new gfxImageSurface(gfxIntSize(width, height), gfxASurface::ImageFormatARGB32);

    if (!newSurface)
        return NS_ERROR_FAILURE;

    *surface = newSurface;
    NS_ADDREF(*surface);

    return NS_OK;
}
