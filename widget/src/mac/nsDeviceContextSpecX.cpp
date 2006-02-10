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
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Patrick C. Beard <beard@netscape.com>
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

#include "nsDeviceContextSpecX.h"
#include "prmem.h"
#include "plstr.h"

/** -------------------------------------------------------
 *  Construct the nsDeviceContextSpecX
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecX::nsDeviceContextSpecX()
    :   mPrintSession(0), mPageFormat(kPMNoPageFormat), mPrintSettings(kPMNoPrintSettings),
        mSavedPort(0)
{
    NS_INIT_REFCNT();
}

/** -------------------------------------------------------
 *  Destroy the nsDeviceContextSpecX
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecX::~nsDeviceContextSpecX()
{
  ClosePrintManager();
}

NS_IMPL_ISUPPORTS2(nsDeviceContextSpecX, nsIDeviceContextSpec, nsIPrintingContext)

/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecMac
 *  @update   dc 12/02/98
 */
NS_IMETHODIMP nsDeviceContextSpecX::Init(PRBool	aQuiet)
{
    // create a print session. then a default print settings.
    OSStatus status = ::PMCreateSession(&mPrintSession);
    if (status != noErr) return NS_ERROR_FAILURE;
    
    status = ::PMCreatePageFormat(&mPageFormat);
    if (status != noErr) return NS_ERROR_FAILURE;
    
    status = ::PMSessionDefaultPageFormat(mPrintSession, mPageFormat);
    if (status != noErr) return NS_ERROR_FAILURE;
    
    status = ::PMCreatePrintSettings(&mPrintSettings);
    if (status != noErr) return NS_ERROR_FAILURE;
    
    status = ::PMSessionDefaultPrintSettings(mPrintSession, mPrintSettings);
    if (status != noErr) return NS_ERROR_FAILURE;

    if (! aQuiet) {
        Boolean accepted = false;
        status = ::PMSessionPrintDialog(mPrintSession, mPrintSettings, mPageFormat, &accepted);
        if (! accepted)
            return NS_ERROR_ABORT;
    }

    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecX::PrintManagerOpen(PRBool* aIsOpen)
{
    *aIsOpen = (mPrintSession != 0);
    return NS_OK;
}

/** -------------------------------------------------------
 * Closes the printmanager if it is open.
 * @update   dc 12/03/98
 */
NS_IMETHODIMP nsDeviceContextSpecX::ClosePrintManager()
{
  if (mPrintSettings != kPMNoPrintSettings)
    ::PMRelease(mPrintSettings);
  if (mPageFormat != kPMNoPageFormat)
    ::PMRelease(mPageFormat);
  if (mPrintSession)
    ::PMRelease(mPrintSession);
	return NS_OK;
}  

NS_IMETHODIMP nsDeviceContextSpecX::BeginDocument()
{
    OSStatus status = ::PMSessionBeginDocument(mPrintSession, mPrintSettings, mPageFormat);
    if (status != noErr) return NS_ERROR_FAILURE;
    
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecX::EndDocument()
{
    PMSessionEndDocument(mPrintSession);
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecX::BeginPage()
{
	// see http://devworld.apple.com/techpubs/carbon/graphics/CarbonPrintingManager/Carbon_Printing_Manager/Functions/PMSessionBeginPage.html
    OSStatus status = ::PMSessionBeginPage(mPrintSession, mPageFormat, NULL);
    if (status != noErr) return NS_ERROR_FAILURE;
    
    ::GetPort(&mSavedPort);
    GrafPtr printingPort;
    status = ::PMSessionGetGraphicsContext(mPrintSession, kPMGraphicsContextQuickdraw,
                                           &(void*)printingPort);
    if (status != noErr) return NS_ERROR_FAILURE;
    ::SetPort(printingPort);
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecX::EndPage()
{
    OSStatus status = ::PMSessionEndPage(mPrintSession);
    if (mSavedPort) {
        ::SetPort(mSavedPort);
        mSavedPort = 0;
    }
    if (status != noErr) return NS_ERROR_FAILURE;
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecX::GetPrinterResolution(double* aResolution)
{
    PMPrinter currentPrinter;
    OSStatus status = ::PMSessionGetCurrentPrinter(mPrintSession, &currentPrinter);
    if (status != noErr) return NS_ERROR_FAILURE;

    PMResolution defaultResolution;
    status = ::PMPrinterGetPrinterResolution(currentPrinter, kPMDefaultResolution, &defaultResolution);
    if (status == noErr)
        *aResolution = defaultResolution.hRes;
    ::PMRelease(currentPrinter);
    
    return (status == noErr ? NS_OK : NS_ERROR_FAILURE);
}

NS_IMETHODIMP nsDeviceContextSpecX::GetPageRect(double* aTop, double* aLeft, double* aBottom, double* aRight)
{
    PMRect pageRect;
    PMGetAdjustedPageRect(mPageFormat, &pageRect);
    *aTop = pageRect.top, *aLeft = pageRect.left;
    *aBottom = pageRect.bottom, *aRight = pageRect.right;
    return NS_OK;
}
