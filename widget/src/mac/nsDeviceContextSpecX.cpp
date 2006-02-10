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
 *  Simon Fraser     <sfraser@netscape.com>
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

#define PM_USE_SESSION_APIS 0
#include "nsDeviceContextSpecX.h"

#include "prmem.h"
#include "plstr.h"

#include "nsIServiceManager.h"
#include "nsIPrintOptions.h"


#include "CoreServices.h"
#include "nsFileSpec.h"
#include "nsPDECommon.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryService.h"

static Boolean  LoadPrinterPlugin();

static Boolean  gPlugInNotLoaded = true;

/** -------------------------------------------------------
 *  Construct the nsDeviceContextSpecX
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecX::nsDeviceContextSpecX()
: mPrintingContext(0)
, mPageFormat(kPMNoPageFormat)
, mPrintSettings(kPMNoPrintSettings)
, mSavedPort(0)
, mBeganPrinting(PR_FALSE)
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
NS_IMETHODIMP nsDeviceContextSpecX::Init(nsIPrintSettings* aPS, PRBool	aQuiet)
{
  nsresult rv;
  OSStatus status;
   
  nsCOMPtr<nsIPrintOptions> printOptionsService = do_GetService("@mozilla.org/gfx/printoptions;1", &rv);
  if (NS_FAILED(rv)) return rv;

	// are we doing a printpreview.. then don't start setting up for the printing
	PRBool	doingPrintPreview;
	
	aPS->GetIsPrintPreview(&doingPrintPreview);

  // Because page setup can get called at any time, we can't use the session APIs here.
	if(!doingPrintPreview) {
		status = ::PMBegin();
		if (status != noErr) return NS_ERROR_FAILURE;
	}

  mBeganPrinting = PR_TRUE;
  
  PMPageFormat    optionsPageFormat = kPMNoPageFormat;
  rv = printOptionsService->GetNativeData(nsIPrintOptions::kNativeDataPrintRecord, (void **)&optionsPageFormat);
  if (NS_FAILED(rv)) return rv;
  
  status = ::PMNewPageFormat(&mPageFormat);
  if (status != noErr) return NS_ERROR_FAILURE;
  
  if (optionsPageFormat != kPMNoPageFormat)
  {
    status = ::PMCopyPageFormat(optionsPageFormat, mPageFormat);
    ::PMDisposePageFormat(optionsPageFormat);
  }
  else
    status = ::PMDefaultPageFormat(mPageFormat);

  if (status != noErr) return NS_ERROR_FAILURE;
  
  Boolean validated;
  ::PMValidatePageFormat(mPageFormat, &validated);
  
  status = ::PMNewPrintSettings(&mPrintSettings);
  if (status != noErr) return NS_ERROR_FAILURE;
  
  status = ::PMDefaultPrintSettings(mPrintSettings);
  if (status != noErr) return NS_ERROR_FAILURE;

  if (! aQuiet) {
  Boolean plugInExtended,accepted=false;
  PRBool  isOn;
  PRInt16 howToEnableFrameUI = nsIPrintSettings::kFrameEnableNone;
  nsPrintExtensions       printData = {false,false,false,false,false,false,false,false};

		::InitCursor();

    // set the values for the plugin here
    aPS->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB, &isOn);
    printData.mHaveSelection = (Boolean) isOn;

    aPS->GetHowToEnableFrameUI(&howToEnableFrameUI);
    if (howToEnableFrameUI == nsIPrintSettings::kFrameEnableAll) {
      printData.mHaveFrames = true;
      printData.mHaveFrameSelected = true;
    }
    
    if (howToEnableFrameUI == nsIPrintSettings::kFrameEnableAsIsAndEach) {
      printData.mHaveFrames = true;
      printData.mHaveFrameSelected = false;
    }
        
    aPS->GetShrinkToFit(&isOn);
    printData.mShrinkToFit = isOn;    
        
    if( gPlugInNotLoaded ) {
      plugInExtended = LoadPrinterPlugin();
    }

    status = PMSetPrintSettingsExtendedData(mPrintSettings,kPDE_Creator,sizeof(printData),&printData);

    status = ::PMPrintDialog(mPrintSettings, mPageFormat, &accepted);

    if (! accepted)
        return NS_ERROR_ABORT;

    if (status != noErr)
      return NS_ERROR_FAILURE;


    // get the data from the plugin
    if(status == noErr){
      UInt32 bytesNeeded;

      status = PMGetPrintSettingsExtendedData(mPrintSettings, kPDE_Creator, &bytesNeeded, NULL);
     
      if(status == noErr && bytesNeeded == sizeof(printData) ){
        status = PMGetPrintSettingsExtendedData(mPrintSettings, kPDE_Creator,&bytesNeeded, &printData);        
        
        // set the correct data fields
        if( printData.mPrintSelection){
          aPS->SetPrintRange(nsIPrintSettings::kRangeSelection);
        } else {
          aPS->SetPrintRange(nsIPrintSettings::kRangeAllPages);
        }
        
        if(printData.mPrintFrameAsIs){
          aPS->SetPrintFrameType(nsIPrintSettings::kFramesAsIs);
        }
        
        if(printData.mPrintSelectedFrame){
          aPS->SetPrintFrameType(nsIPrintSettings::kSelectedFrame);
        }

        if(printData.mPrintFramesSeperatly){
          aPS->SetPrintFrameType(nsIPrintSettings::kEachFrameSep);
        }
        
        if(printData.mShrinkToFit){
          aPS->SetShrinkToFit(PR_TRUE);
        } else {
          aPS->SetShrinkToFit(PR_FALSE);
        }
      }
    }
  }
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
  if (mPrintSettings != kPMNoPrintSettings)
    ::PMDisposePrintSettings(mPrintSettings);

  if (mPageFormat != kPMNoPageFormat)
    ::PMDisposePageFormat(mPageFormat);

  if (mBeganPrinting)
    ::PMEnd();
    
	return NS_OK;
}  

NS_IMETHODIMP nsDeviceContextSpecX::BeginDocument()
{
    OSStatus status = ::PMBeginDocument(mPrintSettings, mPageFormat, &mPrintingContext);
    if (status != noErr) return NS_ERROR_ABORT;
    
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecX::EndDocument()
{
    ::PMEndDocument(mPrintingContext);
    mPrintingContext = 0;
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecX::AbortDocument()
{
    return EndDocument();
}

NS_IMETHODIMP nsDeviceContextSpecX::BeginPage()
{
	// see http://devworld.apple.com/techpubs/carbon/graphics/CarbonPrintingManager/Carbon_Printing_Manager/Functions/PMSessionBeginPage.html
    OSStatus status = ::PMBeginPage(mPrintingContext, NULL);
    if (status != noErr) return NS_ERROR_ABORT;
    
    ::GetPort(&mSavedPort);
    GrafPtr printingPort;
    status = ::PMGetGrafPtr(mPrintingContext, &printingPort);
    if (status != noErr) return NS_ERROR_ABORT;
    ::SetPort(printingPort);
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecX::EndPage()
{
    OSStatus status = ::PMEndPage(mPrintingContext);
    if (mSavedPort)
    {
        ::SetPort(mSavedPort);
        mSavedPort = 0;
    }
    if (status != noErr) return NS_ERROR_ABORT;
    return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecX::GetPrinterResolution(double* aResolution)
{
    PMResolution defaultResolution;
    OSStatus status = ::PMGetPrinterResolution(kPMDefaultResolution, &defaultResolution);
    if (status == noErr)
        *aResolution = defaultResolution.hRes;
    
    return (status == noErr ? NS_OK : NS_ERROR_FAILURE);
}

NS_IMETHODIMP nsDeviceContextSpecX::GetPageRect(double* aTop, double* aLeft, double* aBottom, double* aRight)
{
    PMRect pageRect;
    ::PMGetAdjustedPageRect(mPageFormat, &pageRect);
    *aTop = pageRect.top, *aLeft = pageRect.left;
    *aBottom = pageRect.bottom, *aRight = pageRect.right;
    return NS_OK;
}



Boolean
LoadPrinterPlugin()
{
Boolean     result=false;
FSSpec      spec;

#ifndef XP_MACOSX
  // get the relative path for the essential files folder.. then load the printer plugin
  nsCOMPtr<nsILocalFile> mozFile;
  nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
  if (!directoryService) {
    return false;
  } else {
    directoryService->Get( NS_XPCOM_CURRENT_PROCESS_DIR,NS_GET_IID(nsIFile), getter_AddRefs(mozFile));
    if (!mozFile) {
      return false;
    }
    
    mozFile->Append(NS_LITERAL_CSTRING("Essential Files"));
    mozFile->Append(NS_LITERAL_CSTRING("PrintDialogPDE.plugin"));
    nsCOMPtr<nsILocalFileMac> mozMacFile(do_QueryInterface(mozFile));
    mozMacFile->GetFSSpec(&spec);
    

    FSRef ref;
    OSErr err = FSpMakeFSRef(&spec,&ref);
    char path[512];

    err = FSRefMakePath(&ref,(UInt8*)path,sizeof(path)-1);
    
    if(err == noErr){
      CFStringRef pathRef = CFStringCreateWithCString(NULL,path,kCFStringEncodingUTF8);
      if(pathRef) {
        CFURLRef url = CFURLCreateWithFileSystemPath(NULL,pathRef,kCFURLPOSIXPathStyle,TRUE);
        if(url !=NULL){
          CFPlugInRef	plugin = ::CFPlugInCreate(NULL,url);    
          if(plugin){
            result = true;
            gPlugInNotLoaded = false;
          }
        }
      }
    }

  }
#endif
  return result;
}

