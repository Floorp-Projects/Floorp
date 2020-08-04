/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintSettingsX.h"
#include "nsObjCExceptions.h"

#include "plbase64.h"
#include "plstr.h"

#include "nsCocoaUtils.h"
#include "nsXULAppAPI.h"

#include "mozilla/Preferences.h"

using namespace mozilla;

#define MAC_OS_X_PAGE_SETUP_PREFNAME "print.macosx.pagesetup-2"
#define COCOA_PAPER_UNITS_PER_INCH 72.0

NS_IMPL_ISUPPORTS_INHERITED(nsPrintSettingsX, nsPrintSettings, nsPrintSettingsX)

nsPrintSettingsX::nsPrintSettingsX() : mAdjustedPaperWidth{0.0}, mAdjustedPaperHeight{0.0} {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Note that the app shared `sharedPrintInfo` object is special!  The system
  // print dialog and print settings dialog update it with the values chosen
  // by the user.  Using that object here to initialize new nsPrintSettingsX
  // objects could mask bugs in our code where we fail to save and/or restore
  // print settings ourselves (e.g., bug 1636725).  On other platforms we only
  // initialize new nsPrintSettings objects from the settings that we save to
  // prefs.  Perhaps we should stop using sharedPrintInfo here for consistency?
  mPrintInfo = [[NSPrintInfo sharedPrintInfo] copy];
  mWidthScale = COCOA_PAPER_UNITS_PER_INCH;
  mHeightScale = COCOA_PAPER_UNITS_PER_INCH;

  /*
   * Don't save print settings after the user cancels out of the
   * print dialog. For saving print settings after a cancellation
   * to work properly, in addition to changing |mSaveOnCancel|,
   * the print dialog implementation must be updated to save changed
   * settings and serialize them back to the child process.
   */
  mSaveOnCancel = false;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsPrintSettingsX::nsPrintSettingsX(const nsPrintSettingsX& src) { *this = src; }

nsPrintSettingsX::~nsPrintSettingsX() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mPrintInfo release];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsPrintSettingsX& nsPrintSettingsX::operator=(const nsPrintSettingsX& rhs) {
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

nsresult nsPrintSettingsX::Init() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  InitUnwriteableMargin();
  InitAdjustedPaperSize();

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// Should be called whenever the page format changes.
NS_IMETHODIMP nsPrintSettingsX::InitUnwriteableMargin() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  PMPaper paper;
  PMPaperMargins paperMargin;
  PMPageFormat pageFormat = GetPMPageFormat();
  ::PMGetPageFormatPaper(pageFormat, &paper);
  ::PMPaperGetMargins(paper, &paperMargin);
  mUnwriteableMargin.top = NS_POINTS_TO_INT_TWIPS(paperMargin.top);
  mUnwriteableMargin.left = NS_POINTS_TO_INT_TWIPS(paperMargin.left);
  mUnwriteableMargin.bottom = NS_POINTS_TO_INT_TWIPS(paperMargin.bottom);
  mUnwriteableMargin.right = NS_POINTS_TO_INT_TWIPS(paperMargin.right);

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsPrintSettingsX::InitAdjustedPaperSize() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  PMPageFormat pageFormat = GetPMPageFormat();

  PMRect paperRect;
  ::PMGetAdjustedPaperRect(pageFormat, &paperRect);

  mAdjustedPaperWidth = paperRect.right - paperRect.left;
  mAdjustedPaperHeight = paperRect.bottom - paperRect.top;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void nsPrintSettingsX::SetCocoaPrintInfo(NSPrintInfo* aPrintInfo) {
  if (mPrintInfo != aPrintInfo) {
    [mPrintInfo release];
    mPrintInfo = [aPrintInfo retain];
  }
}

NS_IMETHODIMP nsPrintSettingsX::ReadPageFormatFromPrefs() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsAutoCString encodedData;
  nsresult rv = Preferences::GetCString(MAC_OS_X_PAGE_SETUP_PREFNAME, encodedData);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // decode the base64
  char* decodedData = PL_Base64Decode(encodedData.get(), encodedData.Length(), nullptr);
  NSData* data = [NSData dataWithBytes:decodedData length:strlen(decodedData)];
  if (!data) return NS_ERROR_FAILURE;

  PMPageFormat newPageFormat;
  OSStatus status = ::PMPageFormatCreateWithDataRepresentation((CFDataRef)data, &newPageFormat);
  if (status == noErr) {
    SetPMPageFormat(newPageFormat);
  }
  InitUnwriteableMargin();

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsPrintSettingsX::WritePageFormatToPrefs() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  PMPageFormat pageFormat = GetPMPageFormat();
  if (pageFormat == kPMNoPageFormat) return NS_ERROR_NOT_INITIALIZED;

  NSData* data = nil;
  OSStatus err = ::PMPageFormatCreateDataRepresentation(pageFormat, (CFDataRef*)&data,
                                                        kPMDataFormatXMLDefault);
  if (err != noErr) return NS_ERROR_FAILURE;

  nsAutoCString encodedData;
  encodedData.Adopt(PL_Base64Encode((char*)[data bytes], [data length], nullptr));
  if (!encodedData.get()) return NS_ERROR_OUT_OF_MEMORY;

  return Preferences::SetCString(MAC_OS_X_PAGE_SETUP_PREFNAME, encodedData);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult nsPrintSettingsX::_Clone(nsIPrintSettings** _retval) {
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nullptr;

  nsPrintSettingsX* newSettings = new nsPrintSettingsX(*this);
  if (!newSettings) return NS_ERROR_FAILURE;
  *_retval = newSettings;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettingsX::_Assign(nsIPrintSettings* aPS) {
  nsPrintSettingsX* printSettingsX = static_cast<nsPrintSettingsX*>(aPS);
  if (!printSettingsX) return NS_ERROR_UNEXPECTED;
  *this = *printSettingsX;
  return NS_OK;
}

PMPrintSettings nsPrintSettingsX::GetPMPrintSettings() {
  return static_cast<PMPrintSettings>([mPrintInfo PMPrintSettings]);
}

PMPrintSession nsPrintSettingsX::GetPMPrintSession() {
  return static_cast<PMPrintSession>([mPrintInfo PMPrintSession]);
}

PMPageFormat nsPrintSettingsX::GetPMPageFormat() {
  return static_cast<PMPageFormat>([mPrintInfo PMPageFormat]);
}

void nsPrintSettingsX::SetPMPageFormat(PMPageFormat aPageFormat) {
  PMPageFormat oldPageFormat = GetPMPageFormat();
  ::PMCopyPageFormat(aPageFormat, oldPageFormat);
  [mPrintInfo updateFromPMPageFormat];
}

void nsPrintSettingsX::SetInchesScale(float aWidthScale, float aHeightScale) {
  if (aWidthScale > 0 && aHeightScale > 0) {
    mWidthScale = aWidthScale;
    mHeightScale = aHeightScale;
  }
}

void nsPrintSettingsX::GetInchesScale(float* aWidthScale, float* aHeightScale) {
  *aWidthScale = mWidthScale;
  *aHeightScale = mHeightScale;
}

NS_IMETHODIMP nsPrintSettingsX::SetPaperWidth(double aPaperWidth) {
  mPaperWidth = aPaperWidth;
  mAdjustedPaperWidth = aPaperWidth * mWidthScale;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettingsX::SetPaperHeight(double aPaperHeight) {
  mPaperHeight = aPaperHeight;
  mAdjustedPaperHeight = aPaperHeight * mHeightScale;
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsX::GetEffectivePageSize(double* aWidth, double* aHeight) {
  if (kPaperSizeInches == GetCocoaUnit(mPaperSizeUnit)) {
    *aWidth = NS_INCHES_TO_TWIPS(mAdjustedPaperWidth / mWidthScale);
    *aHeight = NS_INCHES_TO_TWIPS(mAdjustedPaperHeight / mHeightScale);
  } else {
    *aWidth = NS_MILLIMETERS_TO_TWIPS(mAdjustedPaperWidth / mWidthScale);
    *aHeight = NS_MILLIMETERS_TO_TWIPS(mAdjustedPaperHeight / mHeightScale);
  }
  if (kLandscapeOrientation == mOrientation) {
    std::swap(*aWidth, *aHeight);
  }
  return NS_OK;
}

void nsPrintSettingsX::GetFilePageSize(double* aWidth, double* aHeight) {
  double height, width;
  if (kPaperSizeInches == GetCocoaUnit(mPaperSizeUnit)) {
    width = NS_INCHES_TO_TWIPS(mAdjustedPaperWidth / mWidthScale);
    height = NS_INCHES_TO_TWIPS(mAdjustedPaperHeight / mHeightScale);
  } else {
    width = NS_MILLIMETERS_TO_TWIPS(mAdjustedPaperWidth / mWidthScale);
    height = NS_MILLIMETERS_TO_TWIPS(mAdjustedPaperHeight / mHeightScale);
  }
  width /= TWIPS_PER_POINT_FLOAT;
  height /= TWIPS_PER_POINT_FLOAT;

  *aWidth = width;
  *aHeight = height;
}

void nsPrintSettingsX::SetAdjustedPaperSize(double aWidth, double aHeight) {
  mAdjustedPaperWidth = aWidth;
  mAdjustedPaperHeight = aHeight;
}

void nsPrintSettingsX::GetAdjustedPaperSize(double* aWidth, double* aHeight) {
  *aWidth = mAdjustedPaperWidth;
  *aHeight = mAdjustedPaperHeight;
}

NS_IMETHODIMP nsPrintSettingsX::SetPrintRange(int16_t aPrintRange) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // In this case (PrintRange) we store the state in the base class in both the
  // parent and content process since the platform specific NSPrintInfo isn't
  // capable of representing the kRangeSelection state:
  nsPrintSettings::SetPrintRange(aPrintRange);

  // However, we do need to keep NSPrintAllPages on mPrintInfo in sync in the
  // parent process so that the object returned by GetCocoaPrintInfo is valid:
  if (XRE_IsParentProcess()) {
    BOOL allPages = aPrintRange == nsIPrintSettings::kRangeSpecifiedPageRange ? NO : YES;
    NSMutableDictionary* dict = [mPrintInfo dictionary];
    [dict setObject:[NSNumber numberWithBool:allPages] forKey:NSPrintAllPages];
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsPrintSettingsX::GetStartPageRange(int32_t* aStartPageRange) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;
  MOZ_ASSERT(aStartPageRange);

  // Only use NSPrintInfo data in the parent process.
  if (XRE_IsParentProcess()) {
    NSDictionary* dict = [mPrintInfo dictionary];
    *aStartPageRange = [[dict objectForKey:NSPrintFirstPage] intValue];
  } else {
    nsPrintSettings::GetStartPageRange(aStartPageRange);
  }
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsPrintSettingsX::SetStartPageRange(int32_t aStartPageRange) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Only use NSPrintInfo data in the parent process.
  if (XRE_IsParentProcess()) {
    NSMutableDictionary* dict = [mPrintInfo dictionary];
    [dict setObject:[NSNumber numberWithInt:aStartPageRange] forKey:NSPrintFirstPage];
  } else {
    nsPrintSettings::SetStartPageRange(aStartPageRange);
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsPrintSettingsX::GetEndPageRange(int32_t* aEndPageRange) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;
  MOZ_ASSERT(aEndPageRange);

  // Only use NSPrintInfo data in the parent process.
  if (XRE_IsParentProcess()) {
    NSDictionary* dict = [mPrintInfo dictionary];
    *aEndPageRange = [[dict objectForKey:NSPrintLastPage] intValue];
  } else {
    nsPrintSettings::GetEndPageRange(aEndPageRange);
  }
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsPrintSettingsX::SetEndPageRange(int32_t aEndPageRange) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Only use NSPrintInfo data in the parent process.
  if (XRE_IsParentProcess()) {
    NSMutableDictionary* dict = [mPrintInfo dictionary];
    [dict setObject:[NSNumber numberWithInt:aEndPageRange] forKey:NSPrintLastPage];
  } else {
    nsPrintSettings::SetEndPageRange(aEndPageRange);
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrintSettingsX::SetScaling(double aScaling) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Only use NSPrintInfo data in the parent process. The
  // child process' instance is not needed or used.
  if (XRE_IsParentProcess()) {
    [mPrintInfo setScalingFactor:CGFloat(aScaling)];
  } else {
    nsPrintSettings::SetScaling(aScaling);
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrintSettingsX::GetScaling(double* aScaling) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Only use NSPrintInfo data in the parent process. The
  // child process' instance is not needed or used.
  if (XRE_IsParentProcess()) {
    // Limit scaling precision to whole number percent values
    *aScaling = round(double([mPrintInfo scalingFactor]) * 100.0) / 100.0;
  } else {
    nsPrintSettings::GetScaling(aScaling);
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrintSettingsX::SetToFileName(const nsAString& aToFileName) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (XRE_IsContentProcess() && Preferences::GetBool("print.print_via_parent")) {
    // On content sandbox, NSPrintJobSavingURL will returns error since
    // sandbox disallows file access.
    return nsPrintSettings::SetToFileName(aToFileName);
  }

  if (!aToFileName.IsEmpty()) {
    NSURL* jobSavingURL = [NSURL fileURLWithPath:nsCocoaUtils::ToNSString(aToFileName)];
    if (jobSavingURL) {
      // Note: the PMPrintSettingsSetJobName call in nsPrintDialogServiceX::Show
      // seems to mean that we get a sensible file name pre-populated in the
      // dialog there, although our aToFileName is expected to be a full path,
      // and it's less clear where the rest of the path (the directory to save
      // to) in nsPrintDialogServiceX::Show comes from (perhaps from the use
      // of `sharedPrintInfo` to initialize new nsPrintSettingsX objects).
      // Note: we do not set NSPrintJobDisposition on the dict, as that would
      // cause "silent" print jobs to invisibly go to a file instead of the
      // printer. It's up to the print UI to determine if it should be saved
      // to a file instead.
      NSMutableDictionary* printInfoDict = [mPrintInfo dictionary];
      [printInfoDict setObject:jobSavingURL forKey:NSPrintJobSavingURL];
    }
    mToFileName = aToFileName;
  } else {
    mToFileName.Truncate();
  }
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void nsPrintSettingsX::SetDispositionSaveToFile() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mPrintInfo setJobDisposition:NSPrintSaveJob];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

NS_IMETHODIMP
nsPrintSettingsX::GetOrientation(int32_t* aOrientation) {
  // Only use NSPrintInfo data in the parent process. The
  // child process' instance is not needed or used.
  if (XRE_IsParentProcess()) {
    if ([mPrintInfo orientation] == NSPaperOrientationPortrait) {
      *aOrientation = nsIPrintSettings::kPortraitOrientation;
    } else {
      *aOrientation = nsIPrintSettings::kLandscapeOrientation;
    }
  } else {
    nsPrintSettings::GetOrientation(aOrientation);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsX::SetOrientation(int32_t aOrientation) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Only use NSPrintInfo data in the parent process. The
  // child process' instance is not needed or used.
  if (XRE_IsParentProcess()) {
    if (aOrientation == nsIPrintSettings::kPortraitOrientation) {
      [mPrintInfo setOrientation:NSPaperOrientationPortrait];
    } else {
      [mPrintInfo setOrientation:NSPaperOrientationLandscape];
    }
  } else {
    nsPrintSettings::SetOrientation(aOrientation);
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrintSettingsX::SetUnwriteableMarginTop(double aUnwriteableMarginTop) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsPrintSettings::SetUnwriteableMarginTop(aUnwriteableMarginTop);

  // Only use NSPrintInfo data in the parent process. The
  // child process' instance is not needed or used.
  if (XRE_IsParentProcess()) {
    [mPrintInfo setTopMargin:aUnwriteableMarginTop];
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrintSettingsX::SetUnwriteableMarginLeft(double aUnwriteableMarginLeft) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsPrintSettings::SetUnwriteableMarginLeft(aUnwriteableMarginLeft);

  // Only use NSPrintInfo data in the parent process. The
  // child process' instance is not needed or used.
  if (XRE_IsParentProcess()) {
    [mPrintInfo setLeftMargin:aUnwriteableMarginLeft];
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrintSettingsX::SetUnwriteableMarginBottom(double aUnwriteableMarginBottom) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsPrintSettings::SetUnwriteableMarginBottom(aUnwriteableMarginBottom);

  // Only use NSPrintInfo data in the parent process. The
  // child process' instance is not needed or used.
  if (XRE_IsParentProcess()) {
    [mPrintInfo setBottomMargin:aUnwriteableMarginBottom];
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrintSettingsX::SetUnwriteableMarginRight(double aUnwriteableMarginRight) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsPrintSettings::SetUnwriteableMarginRight(aUnwriteableMarginRight);

  // Only use NSPrintInfo data in the parent process. The
  // child process' instance is not needed or used.
  if (XRE_IsParentProcess()) {
    [mPrintInfo setRightMargin:aUnwriteableMarginRight];
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

int nsPrintSettingsX::GetCocoaUnit(int16_t aGeckoUnit) {
  if (aGeckoUnit == kPaperSizeMillimeters)
    return kPaperSizeMillimeters;
  else
    return kPaperSizeInches;
}

nsresult nsPrintSettingsX::SetCocoaPaperSize(double aWidth, double aHeight) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if ([mPrintInfo orientation] == NSPaperOrientationPortrait) {
    [mPrintInfo setPaperSize:NSMakeSize(aWidth, aHeight)];
  } else {
    [mPrintInfo setPaperSize:NSMakeSize(aHeight, aWidth)];
  }
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void nsPrintSettingsX::SetPrinterNameFromPrintInfo() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Don't attempt to call this from child processes.
  // Process sandboxing prevents access to the print
  // server and printer-related settings.
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(mPrintInfo);

  NSString* nsPrinterNameValue = [[mPrintInfo printer] name];
  if (nsPrinterNameValue) {
    nsAutoString printerName;
    nsCocoaUtils::GetStringForNSString(nsPrinterNameValue, printerName);
    mPrinter.Assign(printerName);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}
