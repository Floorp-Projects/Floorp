/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Conrad Carlen <ccarlen@netscape.com>
 *   Markus Stange <mstange@themasta.com>
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

#include "nsPrintSettingsX.h"
#include "nsObjCExceptions.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsServiceManagerUtils.h"

#include "plbase64.h"
#include "plstr.h"

#include "nsCocoaUtils.h"

#define PRINTING_PREF_BRANCH            "print."
#define MAC_OS_X_PAGE_SETUP_PREFNAME    "macosx.pagesetup-2"

NS_IMPL_ISUPPORTS_INHERITED1(nsPrintSettingsX, nsPrintSettings, nsPrintSettingsX)

nsPrintSettingsX::nsPrintSettingsX()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mPrintInfo = [[NSPrintInfo sharedPrintInfo] copy];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsPrintSettingsX::nsPrintSettingsX(const nsPrintSettingsX& src)
{
  *this = src;
}

nsPrintSettingsX::~nsPrintSettingsX()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mPrintInfo release];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsPrintSettingsX& nsPrintSettingsX::operator=(const nsPrintSettingsX& rhs)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (this == &rhs) {
    return *this;
  }
  
  nsPrintSettings::operator=(rhs);

  [mPrintInfo release];
  mPrintInfo = [rhs.mPrintInfo copy];

  return *this;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(*this);
}

nsresult nsPrintSettingsX::Init()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  InitUnwriteableMargin();

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// Should be called whenever the page format changes.
NS_IMETHODIMP nsPrintSettingsX::InitUnwriteableMargin()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  PMPaper paper;
  PMPaperMargins paperMargin;
  PMPageFormat pageFormat = GetPMPageFormat();
  ::PMGetPageFormatPaper(pageFormat, &paper);
  ::PMPaperGetMargins(paper, &paperMargin);
  mUnwriteableMargin.top    = NS_POINTS_TO_TWIPS(paperMargin.top);
  mUnwriteableMargin.left   = NS_POINTS_TO_TWIPS(paperMargin.left);
  mUnwriteableMargin.bottom = NS_POINTS_TO_TWIPS(paperMargin.bottom);
  mUnwriteableMargin.right  = NS_POINTS_TO_TWIPS(paperMargin.right);

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;  
}

void
nsPrintSettingsX::SetCocoaPrintInfo(NSPrintInfo* aPrintInfo)
{
  mPrintInfo = aPrintInfo;
}

NS_IMETHODIMP nsPrintSettingsX::ReadPageFormatFromPrefs()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsresult rv;
  nsCOMPtr<nsIPrefService> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  nsCOMPtr<nsIPrefBranch> prefBranch;
  rv = prefService->GetBranch(PRINTING_PREF_BRANCH, getter_AddRefs(prefBranch));
  if (NS_FAILED(rv))
    return rv;
      
  nsXPIDLCString encodedData;
  rv = prefBranch->GetCharPref(MAC_OS_X_PAGE_SETUP_PREFNAME, getter_Copies(encodedData));
  if (NS_FAILED(rv))
    return rv;

  // decode the base64
  char* decodedData = PL_Base64Decode(encodedData.get(), encodedData.Length(), nsnull);
  NSData* data = [NSData dataWithBytes:decodedData length:PL_strlen(decodedData)];
  if (!data)
    return NS_ERROR_FAILURE;

  PMPageFormat newPageFormat;
#ifdef NS_LEOPARD_AND_LATER
  OSStatus status = ::PMPageFormatCreateWithDataRepresentation((CFDataRef)data, &newPageFormat);
#else
  OSStatus status = ::PMUnflattenPageFormatWithCFData((CFDataRef)data, &newPageFormat);
#endif
  if (status == noErr) {
    SetPMPageFormat(newPageFormat);
  }
  InitUnwriteableMargin();

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsPrintSettingsX::WritePageFormatToPrefs()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  PMPageFormat pageFormat = GetPMPageFormat();
  if (pageFormat == kPMNoPageFormat)
    return NS_ERROR_NOT_INITIALIZED;

  nsresult rv;
  nsCOMPtr<nsIPrefService> prefService(do_GetService(NS_PREFSERVICE_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  nsCOMPtr<nsIPrefBranch> prefBranch;
  rv = prefService->GetBranch(PRINTING_PREF_BRANCH, getter_AddRefs(prefBranch));
  if (NS_FAILED(rv))
    return rv;

  NSData* data = nil;
#ifdef NS_LEOPARD_AND_LATER
  OSStatus err = ::PMPageFormatCreateDataRepresentation(pageFormat, (CFDataRef*)&data, kPMDataFormatXMLDefault);
#else
  OSStatus err = ::PMFlattenPageFormatToCFData(pageFormat, (CFDataRef*)&data);
#endif
  if (err != noErr)
    return NS_ERROR_FAILURE;

  nsXPIDLCString encodedData;
  encodedData.Adopt(PL_Base64Encode((char*)[data bytes], [data length], nsnull));
  if (!encodedData.get())
    return NS_ERROR_OUT_OF_MEMORY;

  return prefBranch->SetCharPref(MAC_OS_X_PAGE_SETUP_PREFNAME, encodedData);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult nsPrintSettingsX::_Clone(nsIPrintSettings **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;
  
  nsPrintSettingsX *newSettings = new nsPrintSettingsX(*this);
  if (!newSettings)
    return NS_ERROR_FAILURE;
  *_retval = newSettings;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettingsX::_Assign(nsIPrintSettings *aPS)
{
  nsPrintSettingsX *printSettingsX = static_cast<nsPrintSettingsX*>(aPS);
  if (!printSettingsX)
    return NS_ERROR_UNEXPECTED;
  *this = *printSettingsX;
  return NS_OK;
}

// The methods below provide wrappers for the different ways of accessing the
// Core Printing PM* objects from the NSPrintInfo object. On 10.4 we need to
// use secret methods which have been made public in 10.5 with slightly
// different names.

// Secret 10.4 methods (from Appkit class dump):
@interface NSPrintInfo (NSTemporaryCompatibility)
- (struct OpaquePMPrintSession *)_pmPrintSession;
- (void)setPMPageFormat:(struct OpaquePMPageFormat *)arg1;
- (struct OpaquePMPageFormat *)pmPageFormat;
- (void)setPMPrintSettings:(struct OpaquePMPrintSettings *)arg1;
- (struct OpaquePMPrintSettings *)pmPrintSettings;
@end

#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4
// Official 10.5+ methods:
@interface NSPrintInfo (OfficialPMAccessors)
- (void*)PMPageFormat;
- (void*)PMPrintSession; 
- (void*)PMPrintSettings; 
- (void)updateFromPMPageFormat; 
- (void)updateFromPMPrintSettings; 
@end
#endif

PMPrintSettings
nsPrintSettingsX::GetPMPrintSettings()
{
  if ([mPrintInfo respondsToSelector:@selector(PMPrintSettings)])
    return static_cast<PMPrintSettings>([mPrintInfo PMPrintSettings]); // 10.5+

  if ([mPrintInfo respondsToSelector:@selector(pmPrintSettings)])
    return static_cast<PMPrintSettings>([mPrintInfo pmPrintSettings]); // 10.4

  NS_ASSERTION(PR_FALSE, "no way of getting PMPrintSettings from NSPrintInfo");
  PMPrintSettings printSettings;
  PMCreatePrintSettings(&printSettings);
  return printSettings;
}

PMPrintSession
nsPrintSettingsX::GetPMPrintSession()
{
  if ([mPrintInfo respondsToSelector:@selector(PMPrintSession)])
    return static_cast<PMPrintSession>([mPrintInfo PMPrintSession]); // 10.5+

  if ([mPrintInfo respondsToSelector:@selector(_pmPrintSession)])
    return static_cast<PMPrintSession>([mPrintInfo _pmPrintSession]); // 10.4

  NS_ASSERTION(PR_FALSE, "no way of getting PMPrintSession from NSPrintInfo");
  PMPrintSession printSession;
  PMCreateSession(&printSession);
  return printSession;
}

PMPageFormat
nsPrintSettingsX::GetPMPageFormat()
{
  if ([mPrintInfo respondsToSelector:@selector(PMPageFormat)])
    return static_cast<PMPageFormat>([mPrintInfo PMPageFormat]); // 10.5+

  if ([mPrintInfo respondsToSelector:@selector(pmPageFormat)])
    return static_cast<PMPageFormat>([mPrintInfo pmPageFormat]); // 10.4

  NS_ASSERTION(PR_FALSE, "no way of getting PMPageFormat from NSPrintInfo");
  PMPageFormat pageFormat;
  PMCreatePageFormat(&pageFormat);
  return pageFormat;
}

void
nsPrintSettingsX::SetPMPageFormat(PMPageFormat aPageFormat)
{
  PMPageFormat oldPageFormat = GetPMPageFormat();
  ::PMCopyPageFormat(aPageFormat, oldPageFormat);
  if ([mPrintInfo respondsToSelector:@selector(updateFromPMPageFormat)]) {
    [mPrintInfo updateFromPMPageFormat]; // 10.5+
  } else if ([mPrintInfo respondsToSelector:@selector(setPMPageFormat:)]) {
    [mPrintInfo setPMPageFormat:oldPageFormat]; // 10.4
  }
}

