/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintSettingsX.h"
#include "nsObjCExceptions.h"

#include "plbase64.h"
#include "plstr.h"

#include "nsCocoaUtils.h"

#include "mozilla/Preferences.h"

using namespace mozilla;

#define MAC_OS_X_PAGE_SETUP_PREFNAME    "print.macosx.pagesetup-2"
#define COCOA_PAPER_UNITS_PER_INCH      72.0

NS_IMPL_ISUPPORTS_INHERITED(nsPrintSettingsX, nsPrintSettings, nsPrintSettingsX)

nsPrintSettingsX::nsPrintSettingsX()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mPrintInfo = [[NSPrintInfo sharedPrintInfo] copy];
  mWidthScale = COCOA_PAPER_UNITS_PER_INCH;
  mHeightScale = COCOA_PAPER_UNITS_PER_INCH;

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
  InitAdjustedPaperSize();

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
  mUnwriteableMargin.top    = NS_POINTS_TO_INT_TWIPS(paperMargin.top);
  mUnwriteableMargin.left   = NS_POINTS_TO_INT_TWIPS(paperMargin.left);
  mUnwriteableMargin.bottom = NS_POINTS_TO_INT_TWIPS(paperMargin.bottom);
  mUnwriteableMargin.right  = NS_POINTS_TO_INT_TWIPS(paperMargin.right);

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;  
}

NS_IMETHODIMP nsPrintSettingsX::InitAdjustedPaperSize()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  PMPageFormat pageFormat = GetPMPageFormat();

  PMRect paperRect;
  ::PMGetAdjustedPaperRect(pageFormat, &paperRect);

  mAdjustedPaperWidth = paperRect.right - paperRect.left;
  mAdjustedPaperHeight = paperRect.bottom - paperRect.top;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void
nsPrintSettingsX::SetCocoaPrintInfo(NSPrintInfo* aPrintInfo)
{
  if (mPrintInfo != aPrintInfo) {
    [mPrintInfo release];
    mPrintInfo = [aPrintInfo retain];
  }
}

NS_IMETHODIMP nsPrintSettingsX::ReadPageFormatFromPrefs()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsAutoCString encodedData;
  nsresult rv =
    Preferences::GetCString(MAC_OS_X_PAGE_SETUP_PREFNAME, &encodedData);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // decode the base64
  char* decodedData = PL_Base64Decode(encodedData.get(), encodedData.Length(), nullptr);
  NSData* data = [NSData dataWithBytes:decodedData length:strlen(decodedData)];
  if (!data)
    return NS_ERROR_FAILURE;

  PMPageFormat newPageFormat;
  OSStatus status = ::PMPageFormatCreateWithDataRepresentation((CFDataRef)data, &newPageFormat);
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

  NSData* data = nil;
  OSStatus err = ::PMPageFormatCreateDataRepresentation(pageFormat, (CFDataRef*)&data, kPMDataFormatXMLDefault);
  if (err != noErr)
    return NS_ERROR_FAILURE;

  nsAutoCString encodedData;
  encodedData.Adopt(PL_Base64Encode((char*)[data bytes], [data length], nullptr));
  if (!encodedData.get())
    return NS_ERROR_OUT_OF_MEMORY;

  return Preferences::SetCString(MAC_OS_X_PAGE_SETUP_PREFNAME, encodedData);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult nsPrintSettingsX::_Clone(nsIPrintSettings **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nullptr;
  
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

PMPrintSettings
nsPrintSettingsX::GetPMPrintSettings()
{
  return static_cast<PMPrintSettings>([mPrintInfo PMPrintSettings]);
}

PMPrintSession
nsPrintSettingsX::GetPMPrintSession()
{
  return static_cast<PMPrintSession>([mPrintInfo PMPrintSession]);
}

PMPageFormat
nsPrintSettingsX::GetPMPageFormat()
{
  return static_cast<PMPageFormat>([mPrintInfo PMPageFormat]);
}

void
nsPrintSettingsX::SetPMPageFormat(PMPageFormat aPageFormat)
{
  PMPageFormat oldPageFormat = GetPMPageFormat();
  ::PMCopyPageFormat(aPageFormat, oldPageFormat);
  [mPrintInfo updateFromPMPageFormat];
}

void
nsPrintSettingsX::SetInchesScale(float aWidthScale, float aHeightScale)
{
  if (aWidthScale > 0 && aHeightScale > 0) {
    mWidthScale = aWidthScale;
    mHeightScale = aHeightScale;
  }
}

void
nsPrintSettingsX::GetInchesScale(float *aWidthScale, float *aHeightScale)
{
  *aWidthScale = mWidthScale;
  *aHeightScale = mHeightScale;
}

NS_IMETHODIMP nsPrintSettingsX::SetPaperWidth(double aPaperWidth)
{
  mPaperWidth = aPaperWidth;
  mAdjustedPaperWidth = aPaperWidth * mWidthScale;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettingsX::SetPaperHeight(double aPaperHeight)
{
  mPaperHeight = aPaperHeight;
  mAdjustedPaperHeight = aPaperHeight * mHeightScale;
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsX::GetEffectivePageSize(double *aWidth, double *aHeight)
{
  *aWidth  = NS_INCHES_TO_TWIPS(mAdjustedPaperWidth / mWidthScale);
  *aHeight = NS_INCHES_TO_TWIPS(mAdjustedPaperHeight / mHeightScale);
  return NS_OK;
}

void
nsPrintSettingsX::GetFilePageSize(double *aWidth, double *aHeight)
{
  double height, width;
  if (kPaperSizeInches == GetCocoaUnit(mPaperSizeUnit)) {
    width  = NS_INCHES_TO_TWIPS(mAdjustedPaperWidth / mWidthScale);
    height = NS_INCHES_TO_TWIPS(mAdjustedPaperHeight / mHeightScale);
  } else {
    width  = NS_MILLIMETERS_TO_TWIPS(mAdjustedPaperWidth / mWidthScale);
    height = NS_MILLIMETERS_TO_TWIPS(mAdjustedPaperHeight / mHeightScale);
  }
  width /= TWIPS_PER_POINT_FLOAT;
  height /= TWIPS_PER_POINT_FLOAT;

  *aWidth = width;
  *aHeight = height;
}

void nsPrintSettingsX::SetAdjustedPaperSize(double aWidth, double aHeight)
{
  mAdjustedPaperWidth = aWidth;
  mAdjustedPaperHeight = aHeight;
}

void nsPrintSettingsX::GetAdjustedPaperSize(double *aWidth, double *aHeight)
{
  *aWidth = mAdjustedPaperWidth;
  *aHeight = mAdjustedPaperHeight;
}

NS_IMETHODIMP
nsPrintSettingsX::SetScaling(double aScaling)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSMutableDictionary* printInfoDict = [mPrintInfo dictionary];
  [printInfoDict setObject: [NSNumber numberWithFloat: aScaling]
   forKey: NSPrintScalingFactor];
  NSPrintInfo* newPrintInfo =
      [[NSPrintInfo alloc] initWithDictionary: printInfoDict];
  if (NS_WARN_IF(!newPrintInfo)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  SetCocoaPrintInfo(newPrintInfo);
  [newPrintInfo release];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrintSettingsX::GetScaling(double *aScaling)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSDictionary* printInfoDict = [mPrintInfo dictionary];

  *aScaling = [[printInfoDict objectForKey: NSPrintScalingFactor] doubleValue];

  // Limit scaling precision to whole number percent values
  *aScaling = round(*aScaling * 100.0) / 100.0;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrintSettingsX::SetToFileName(const char16_t *aToFileName)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSMutableDictionary* printInfoDict = [mPrintInfo dictionary];
  nsString filename = nsDependentString(aToFileName);

  NSURL* jobSavingURL =
      [NSURL fileURLWithPath: nsCocoaUtils::ToNSString(filename)];
  if (jobSavingURL) {
    [printInfoDict setObject: NSPrintSaveJob forKey: NSPrintJobDisposition];
    [printInfoDict setObject: jobSavingURL forKey: NSPrintJobSavingURL];
  }
  NSPrintInfo* newPrintInfo =
      [[NSPrintInfo alloc] initWithDictionary: printInfoDict];
  if (NS_WARN_IF(!newPrintInfo)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  SetCocoaPrintInfo(newPrintInfo);
  [newPrintInfo release];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrintSettingsX::GetOrientation(int32_t *aOrientation)
{
  if ([mPrintInfo orientation] == NS_PAPER_ORIENTATION_PORTRAIT) {
    *aOrientation = nsIPrintSettings::kPortraitOrientation;
  } else {
    *aOrientation = nsIPrintSettings::kLandscapeOrientation;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsX::SetOrientation(int32_t aOrientation)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSMutableDictionary* printInfoDict = [mPrintInfo dictionary];
  if (aOrientation == nsIPrintSettings::kPortraitOrientation) {
    [printInfoDict setObject: [NSNumber numberWithInt: NS_PAPER_ORIENTATION_PORTRAIT]
     forKey: NSPrintOrientation];
  } else {
    [printInfoDict setObject: [NSNumber numberWithInt: NS_PAPER_ORIENTATION_LANDSCAPE]
     forKey: NSPrintOrientation];
  }
  NSPrintInfo* newPrintInfo =
      [[NSPrintInfo alloc] initWithDictionary: printInfoDict];
  if (NS_WARN_IF(!newPrintInfo)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  SetCocoaPrintInfo(newPrintInfo);
  [newPrintInfo release];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrintSettingsX::SetUnwriteableMarginTop(double aUnwriteableMarginTop)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsPrintSettings::SetUnwriteableMarginTop(aUnwriteableMarginTop);
  NSMutableDictionary* printInfoDict = [mPrintInfo dictionary];
  [printInfoDict setObject : [NSNumber numberWithDouble: aUnwriteableMarginTop]
   forKey : NSPrintTopMargin];
  NSPrintInfo* newPrintInfo =
      [[NSPrintInfo alloc] initWithDictionary: printInfoDict];
  if (NS_WARN_IF(!newPrintInfo)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  SetCocoaPrintInfo(newPrintInfo);
  [newPrintInfo release];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrintSettingsX::SetUnwriteableMarginLeft(double aUnwriteableMarginLeft)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsPrintSettings::SetUnwriteableMarginLeft(aUnwriteableMarginLeft);
  NSMutableDictionary* printInfoDict = [mPrintInfo dictionary];
  [printInfoDict setObject : [NSNumber numberWithDouble: aUnwriteableMarginLeft]
   forKey : NSPrintLeftMargin];
  NSPrintInfo* newPrintInfo =
      [[NSPrintInfo alloc] initWithDictionary: printInfoDict];
  if (NS_WARN_IF(!newPrintInfo)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  SetCocoaPrintInfo(newPrintInfo);
  [newPrintInfo release];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrintSettingsX::SetUnwriteableMarginBottom(double aUnwriteableMarginBottom)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsPrintSettings::SetUnwriteableMarginBottom(aUnwriteableMarginBottom);
  NSMutableDictionary* printInfoDict = [mPrintInfo dictionary];
  [printInfoDict setObject : [NSNumber numberWithDouble: aUnwriteableMarginBottom]
   forKey : NSPrintBottomMargin];
  NSPrintInfo* newPrintInfo =
      [[NSPrintInfo alloc] initWithDictionary: printInfoDict];
  if (NS_WARN_IF(!newPrintInfo)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  SetCocoaPrintInfo(newPrintInfo);
  [newPrintInfo release];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrintSettingsX::SetUnwriteableMarginRight(double aUnwriteableMarginRight)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsPrintSettings::SetUnwriteableMarginRight(aUnwriteableMarginRight);
  NSMutableDictionary* printInfoDict = [mPrintInfo dictionary];
  [printInfoDict setObject : [NSNumber numberWithDouble: aUnwriteableMarginRight]
   forKey : NSPrintRightMargin];
  NSPrintInfo* newPrintInfo =
      [[NSPrintInfo alloc] initWithDictionary: printInfoDict];
  if (NS_WARN_IF(!newPrintInfo)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  SetCocoaPrintInfo(newPrintInfo);
  [newPrintInfo release];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

int
nsPrintSettingsX::GetCocoaUnit(int16_t aGeckoUnit)
{
  if (aGeckoUnit == kPaperSizeMillimeters)
    return kPaperSizeMillimeters;
  else
    return kPaperSizeInches;
}

nsresult nsPrintSettingsX::SetCocoaPaperSize(double aWidth, double aHeight)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSSize paperSize;
  NSMutableDictionary* printInfoDict = [mPrintInfo dictionary];
  if ([mPrintInfo orientation] == NS_PAPER_ORIENTATION_PORTRAIT) {
    // switch widths and heights
    paperSize = NSMakeSize(aWidth, aHeight);
    [printInfoDict setObject: [NSValue valueWithSize: paperSize]
     forKey: NSPrintPaperSize];
  } else {
    paperSize = NSMakeSize(aHeight, aWidth);
    [printInfoDict setObject: [NSValue valueWithSize: paperSize]
     forKey: NSPrintPaperSize];
  }
  NSPrintInfo* newPrintInfo =
      [[NSPrintInfo alloc] initWithDictionary: printInfoDict];
  if (NS_WARN_IF(!newPrintInfo)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  SetCocoaPrintInfo(newPrintInfo);
  [newPrintInfo release];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
