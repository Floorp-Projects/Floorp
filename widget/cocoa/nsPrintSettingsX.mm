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

already_AddRefed<nsIPrintSettings> CreatePlatformPrintSettings(
    const PrintSettingsInitializer& aSettings) {
  RefPtr<nsPrintSettings> settings = new nsPrintSettingsX();
  settings->InitWithInitializer(aSettings);
  settings->SetDefaultFileName();
  return settings.forget();
}

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

  mWidthScale = rhs.mWidthScale;
  mHeightScale = rhs.mHeightScale;
  mAdjustedPaperWidth = rhs.mAdjustedPaperWidth;
  mAdjustedPaperHeight = rhs.mAdjustedPaperHeight;

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

  int32_t orientation;
  GetOrientation(&orientation);
  if (kLandscapeOrientation == orientation) {
    // Depending whether we're coming from the old system UI or the tab-modal
    // preview UI, the orientation may not actually have been set yet. So for
    // consistency, we always store physical (portrait-mode) width and height
    // here. They will be swapped if needed in GetEffectivePageSize (in line
    // with other implementations).
    std::swap(mAdjustedPaperWidth, mAdjustedPaperHeight);
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void nsPrintSettingsX::SetCocoaPrintInfo(NSPrintInfo* aPrintInfo) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mPrintInfo != aPrintInfo) {
    [mPrintInfo release];
    mPrintInfo = [aPrintInfo retain];
  }

  NSDictionary* dict = [mPrintInfo dictionary];
  NSString* printerName = [dict objectForKey:NSPrintPrinterName];
  if (printerName) {
    // Ensure the name is also stored in the base nsPrintSettings.
    nsAutoString name;
    nsCocoaUtils::GetStringForNSString(printerName, name);
    nsPrintSettings::SetPrinterName(name);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsresult nsPrintSettingsX::ReadPageFormatFromPrefs() {
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

nsresult nsPrintSettingsX::WritePageFormatToPrefs() {
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
  auto newSettings = MakeRefPtr<nsPrintSettingsX>();
  *newSettings = *this;
  newSettings.forget(_retval);
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

NS_IMETHODIMP nsPrintSettingsX::GetPaperWidth(double* aPaperWidth) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Only use NSPrintInfo data in the parent process.
  if (XRE_IsParentProcess()) {
    NSSize paperSize = [mPrintInfo paperSize];
    int32_t orientation;
    GetOrientation(&orientation);
    if (kLandscapeOrientation == orientation) {
      *aPaperWidth = paperSize.height / mHeightScale;
    } else {
      *aPaperWidth = paperSize.width / mWidthScale;
    }
  } else {
    nsPrintSettings::GetPaperWidth(aPaperWidth);
  }
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsPrintSettingsX::SetPaperWidth(double aPaperWidth) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  mPaperWidth = aPaperWidth;
  mAdjustedPaperWidth = aPaperWidth * mWidthScale;

  // Only use NSPrintInfo data in the parent process.
  if (XRE_IsParentProcess()) {
    NSSize paperSize = [mPrintInfo paperSize];
    if (mOrientation == nsIPrintSettings::kLandscapeOrientation) {
      paperSize.height = mPaperWidth * mHeightScale;
    } else {
      paperSize.width = mAdjustedPaperWidth;
    }
    [mPrintInfo setPaperSize:paperSize];
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsPrintSettingsX::GetPaperHeight(double* aPaperHeight) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Only use NSPrintInfo data in the parent process.
  if (XRE_IsParentProcess()) {
    NSSize paperSize = [mPrintInfo paperSize];
    int32_t orientation;
    GetOrientation(&orientation);
    if (kLandscapeOrientation == orientation) {
      *aPaperHeight = paperSize.width / mWidthScale;
    } else {
      *aPaperHeight = paperSize.height / mHeightScale;
    }
  } else {
    nsPrintSettings::GetPaperHeight(aPaperHeight);
  }
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsPrintSettingsX::SetPaperHeight(double aPaperHeight) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  mPaperHeight = aPaperHeight;
  mAdjustedPaperHeight = aPaperHeight * mHeightScale;

  // Only use NSPrintInfo data in the parent process.
  if (XRE_IsParentProcess()) {
    NSSize paperSize = [mPrintInfo paperSize];
    if (mOrientation == nsIPrintSettings::kLandscapeOrientation) {
      paperSize.width = mPaperHeight * mWidthScale;
    } else {
      paperSize.height = mAdjustedPaperHeight;
    }
    [mPrintInfo setPaperSize:paperSize];
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
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
  int32_t orientation;
  GetOrientation(&orientation);
  if (kLandscapeOrientation == orientation) {
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

NS_IMETHODIMP nsPrintSettingsX::SetPrinterName(const nsAString& aName) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // In this case (PrinterName) we store the state in the base class in both the
  // parent and content process since the platform specific NSPrintInfo isn't
  // capable of representing the printer name in the case of Save to PDF:
  nsPrintSettings::SetPrinterName(aName);

  // However, we do need to keep the NSPrinter in mPrintInfo in sync in the
  // parent process so that the object returned by GetCocoaPrintInfo is valid:
  if (XRE_IsParentProcess()) {
    NSString* name = nsCocoaUtils::ToNSString(aName);
    // If the name is our pseudo-printer "Save to PDF", this will silently fail
    // as no such printer is known.
    [mPrintInfo setPrinter:[NSPrinter printerWithName:name]];
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

  // This code used to set the scaling to NSPrintInfo on the parent process
  // and to nsPrintSettings::SetScaling() on content processes.
  // This was causing a double-scaling effect, for example, setting scaling
  // to 50% would cause the printed results to be 25% the size of the original.
  // See bug 1662389.
  //
  // XXX(nordzilla) It's not clear to me why this fixes the scaling problem
  // when other similar solutions are not as effective. I've tried simply
  // deleting the overriden getters and setters, and also just setting the
  // NSPrintInfo scaling to 1.0 in the constructor; but that does not yield
  // the same results as setting it here, particularly when printing from
  // the system dialog.
  //
  // It would be worth investigating the scaling issue further and
  // seeing if there is a cleaner way to fix it compared to this.
  // See bug 1662934.
  [mPrintInfo setScalingFactor:CGFloat(1.0)];
  nsPrintSettings::SetScaling(aScaling);

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrintSettingsX::GetScaling(double* aScaling) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsPrintSettings::GetScaling(aScaling);

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
nsPrintSettingsX::GetNumCopies(int32_t* aCopies) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Only use NSPrintInfo data in the parent process. The
  // child process' instance is not needed or used.
  if (XRE_IsParentProcess()) {
    NSDictionary* dict = [mPrintInfo dictionary];
    *aCopies = [[dict objectForKey:NSPrintCopies] intValue];
  } else {
    nsPrintSettings::GetNumCopies(aCopies);
  }
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrintSettingsX::SetNumCopies(int32_t aCopies) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Only use NSPrintInfo data in the parent process. The
  // child process' instance is not needed or used.
  if (XRE_IsParentProcess()) {
    NSMutableDictionary* dict = [mPrintInfo dictionary];
    [dict setObject:[NSNumber numberWithInt:aCopies] forKey:NSPrintCopies];
  } else {
    nsPrintSettings::SetNumCopies(aCopies);
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrintSettingsX::GetDuplex(int32_t* aDuplex) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (XRE_IsParentProcess()) {
    NSDictionary* settings = [mPrintInfo printSettings];
    NSNumber* value = [settings objectForKey:@"com_apple_print_PrintSettings_PMDuplexing"];
    if (value) {
      PMDuplexMode duplexSetting = [value unsignedShortValue];
      switch (duplexSetting) {
        case kPMDuplexNone:
          *aDuplex = kSimplex;
          break;
        case kPMDuplexNoTumble:
          *aDuplex = kDuplexHorizontal;
          break;
        case kPMDuplexTumble:
          *aDuplex = kDuplexVertical;
          break;
        default:
          MOZ_ASSERT_UNREACHABLE("Unknown duplex value");
          return NS_ERROR_FAILURE;
      }
    } else {
      // By default a printSettings dictionary doesn't initially contain the
      // duplex key at all, so this is not an error; its absence just means no
      // duplexing has been requested, so we return kSimplex.
      *aDuplex = kSimplex;
    }
  } else {
    nsPrintSettings::GetDuplex(aDuplex);
  }
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrintSettingsX::SetDuplex(int32_t aDuplex) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (XRE_IsParentProcess()) {
    // Map nsIPrintSetting constants to macOS settings:
    PMDuplexMode duplexSetting;
    switch (aDuplex) {
      case kSimplex:
        duplexSetting = kPMDuplexNone;
        break;
      case kDuplexVertical:
        duplexSetting = kPMDuplexNoTumble;
        break;
      case kDuplexHorizontal:
        duplexSetting = kPMDuplexTumble;
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unknown duplex value");
        return NS_ERROR_FAILURE;
    }
    NSMutableDictionary* settings = [mPrintInfo printSettings];
    [settings setObject:[NSNumber numberWithUnsignedShort:duplexSetting]
                 forKey:@"com_apple_print_PrintSettings_PMDuplexing"];
  } else {
    nsPrintSettings::SetDuplex(aDuplex);
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
