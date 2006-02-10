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
 *   Simon Fraser <sfraser@netscape.com>
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

#include <PMApplication.h>

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsWatchTask.h"
#include "nsPrintOptionsX.h"
#include "nsIPref.h"

#include "nsGfxUtils.h"
#include "plbase64.h"
#include "prmem.h"


#define MAC_OS_X_PAGE_SETUP_PREFNAME  "print.macosx.pagesetup"

/** ---------------------------------------------------
 */
nsPrintOptionsX::nsPrintOptionsX()
: mPageFormat(kPMNoPageFormat)
{
  OSStatus status = ::PMNewPageFormat(&mPageFormat);
  NS_ASSERTION(status == noErr, "Error creating print settings");
  
  status = ::PMBegin();
  NS_ASSERTION(status == noErr, "Error from PMBegin()");

  nsresult rv = ReadPageSetupFromPrefs();
  if (NS_FAILED(rv))
    ::PMDefaultPageFormat(mPageFormat);
  else
  {
    Boolean valid;
    ::PMValidatePageFormat(mPageFormat, &valid);
  }
  
  ::PMEnd();  
}

/** ---------------------------------------------------
 */
nsPrintOptionsX::~nsPrintOptionsX()
{
  if (mPageFormat)
    ::PMDisposePageFormat(mPageFormat);
}

/** ---------------------------------------------------
 */
NS_IMETHODIMP
nsPrintOptionsX::ShowNativeDialog(void) 
{
  NS_ASSERTION(mPageFormat != kPMNoPageFormat, "No page format");
  if (mPageFormat == kPMNoPageFormat)
    return NS_ERROR_NOT_INITIALIZED;
  
  OSStatus status = ::PMBegin();
  if (status != noErr) return NS_ERROR_FAILURE;
  
  Boolean validated;
  ::PMValidatePageFormat(mPageFormat, &validated);

  Boolean   accepted = false;
  status = ::PMPageSetupDialog(mPageFormat, &accepted);

  ::PMEnd();
  
  if (status != noErr)
    return NS_ERROR_FAILURE;
    
  if (!accepted)
    return NS_ERROR_ABORT;

  return NS_OK;
} 


NS_IMETHODIMP 
nsPrintOptionsX::ReadPrefs()
{
  // it doesn't really matter if this fails
  nsresult  rv = ReadPageSetupFromPrefs();
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to write page setup to prefs");
  
  return nsPrintOptions::ReadPrefs();
}

NS_IMETHODIMP 
nsPrintOptionsX::WritePrefs()
{
  // it doesn't really matter if this fails
  nsresult  rv = WritePageSetupToPrefs();
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to save page setup to prefs");
  
  return nsPrintOptions::WritePrefs();
}


/* [noscript] voidPtr GetNativeData (in short aDataType); */
NS_IMETHODIMP
nsPrintOptionsX::GetNativeData(PRInt16 aDataType, void * *_retval)
{
  nsresult rv = NS_OK;
  
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  
  switch (aDataType)
  {
    case kNativeDataPrintRecord:
      // we need to clone and pass out
      PMPageFormat    pageFormat = kPMNoPageFormat;
      OSStatus status = ::PMNewPageFormat(&pageFormat);
      if (status != noErr) return NS_ERROR_FAILURE;
      
      status = ::PMCopyPageFormat(mPageFormat, pageFormat);
      if (status != noErr) {
        ::PMDisposePageFormat(pageFormat);
        return NS_ERROR_FAILURE;
      }
      
      *_retval = pageFormat;
      break;
      
    default:
      rv = NS_ERROR_FAILURE;
      break;
  }

  return rv;
}


#pragma mark -

nsresult
nsPrintOptionsX::ReadPageSetupFromPrefs()
{
  nsresult rv;
  nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;
    
  nsXPIDLCString  encodedData;
  rv = prefs->GetCharPref(MAC_OS_X_PAGE_SETUP_PREFNAME, getter_Copies(encodedData));
  if (NS_FAILED(rv))
    return rv;

  // decode the base64
  PRInt32   encodedDataLen = nsCRT::strlen(encodedData.get());
  char* decodedData = ::PL_Base64Decode(encodedData.get(), encodedDataLen, nsnull);
  if (!decodedData)
    return NS_ERROR_FAILURE;

  Handle    decodedDataHandle = nsnull;
  OSErr err = ::PtrToHand(decodedData, &decodedDataHandle, (encodedDataLen * 3) / 4);
  PR_Free(decodedData);
  if (err != noErr)
    return NS_ERROR_OUT_OF_MEMORY;

  StHandleOwner   handleOwner(decodedDataHandle);  

  PMPageFormat  newPageFormat = kPMNoPageFormat;
  OSStatus  status = ::PMUnflattenPageFormat(decodedDataHandle, &newPageFormat);
  if (status != noErr) 
    return NS_ERROR_FAILURE;

  status = ::PMCopyPageFormat(newPageFormat, mPageFormat);
  ::PMDisposePageFormat(newPageFormat);
  newPageFormat = kPMNoPageFormat;

  return (status == noErr) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsPrintOptionsX::WritePageSetupToPrefs()
{
  if (mPageFormat == kPMNoPageFormat)
    return NS_ERROR_NOT_INITIALIZED;
    
  nsresult rv;
  nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  Handle    pageFormatHandle = nsnull;
  OSStatus  err = ::PMFlattenPageFormat(mPageFormat, &pageFormatHandle);
  if (err != noErr)
    return NS_ERROR_FAILURE;
    
  StHandleOwner   handleOwner(pageFormatHandle);
  StHandleLocker  handleLocker(pageFormatHandle);
  
  nsXPIDLCString  encodedData;
  encodedData.Adopt(::PL_Base64Encode(*pageFormatHandle, ::GetHandleSize(pageFormatHandle), nsnull));
  if (!encodedData.get())
    return NS_ERROR_OUT_OF_MEMORY;

  return prefs->SetCharPref(MAC_OS_X_PAGE_SETUP_PREFNAME, encodedData);
}


