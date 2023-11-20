/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintSettingsX.h"
#include "nsObjCExceptions.h"

#include "plbase64.h"

#include "nsCocoaUtils.h"
#include "nsXULAppAPI.h"

#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_print.h"
#include "nsPrinterCUPS.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS_INHERITED(nsPrintSettingsX, nsPrintSettings, nsPrintSettingsX)

nsPrintSettingsX::nsPrintSettingsX() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  mDestination = kPMDestinationInvalid;

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

already_AddRefed<nsIPrintSettings> CreatePlatformPrintSettings(
    const PrintSettingsInitializer& aSettings) {
  RefPtr<nsPrintSettings> settings = new nsPrintSettingsX();
  settings->InitWithInitializer(aSettings);
  settings->SetDefaultFileName();
  return settings.forget();
}

nsPrintSettingsX& nsPrintSettingsX::operator=(const nsPrintSettingsX& rhs) {
  if (this == &rhs) {
    return *this;
  }

  nsPrintSettings::operator=(rhs);

  mDestination = rhs.mDestination;
  mDisposition = rhs.mDisposition;

  // We don't copy mSystemPrintInfo here, so any copied printSettings will start
  // out without a wrapped printInfo, just using our internal settings. The
  // system printInfo is used *only* by the nsPrintSettingsX to which it was
  // originally passed (when the user ran a system print UI dialog).

  return *this;
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
  if (!printSettingsX) {
    return NS_ERROR_UNEXPECTED;
  }
  *this = *printSettingsX;
  return NS_OK;
}

struct KnownMonochromeSetting {
  const NSString* mName;
  const NSString* mValue;
};

#define DECLARE_KNOWN_MONOCHROME_SETTING(key_, value_) \
  { @key_, @value_ }                                   \
  ,
static const KnownMonochromeSetting kKnownMonochromeSettings[] = {
    CUPS_EACH_MONOCHROME_PRINTER_SETTING(DECLARE_KNOWN_MONOCHROME_SETTING)};
#undef DECLARE_KNOWN_MONOCHROME_SETTING

NSPrintInfo* nsPrintSettingsX::CreateOrCopyPrintInfo(bool aWithScaling) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // If we have a printInfo that came from the system print UI, use it so that
  // any printer-specific settings we don't know about will still be used.
  if (mSystemPrintInfo) {
    NSPrintInfo* sysPrintInfo = [mSystemPrintInfo copy];
    // Any required scaling will be done by Gecko, so we don't want it here.
    [sysPrintInfo setScalingFactor:1.0f];
    return sysPrintInfo;
  }

  // Note that the app shared `sharedPrintInfo` object is special!  The system
  // print dialog and print settings dialog update it with the values chosen
  // by the user.  Using that object here to initialize new nsPrintSettingsX
  // objects could mask bugs in our code where we fail to save and/or restore
  // print settings ourselves (e.g., bug 1636725).  On other platforms we only
  // initialize new nsPrintSettings objects from the settings that we save to
  // prefs.  Perhaps we should stop using sharedPrintInfo here for consistency?
  NSPrintInfo* printInfo = [[NSPrintInfo sharedPrintInfo] copy];

  NSSize paperSize;
  if (GetSheetOrientation() == kPortraitOrientation) {
    paperSize.width = CocoaPointsFromPaperSize(mPaperWidth);
    paperSize.height = CocoaPointsFromPaperSize(mPaperHeight);
  } else {
    paperSize.width = CocoaPointsFromPaperSize(mPaperHeight);
    paperSize.height = CocoaPointsFromPaperSize(mPaperWidth);
  }
  [printInfo setPaperSize:paperSize];

  if (paperSize.width > paperSize.height) {
    [printInfo setOrientation:NSPaperOrientationLandscape];
  } else {
    [printInfo setOrientation:NSPaperOrientationPortrait];
  }

  [printInfo setTopMargin:mUnwriteableMargin.top];
  [printInfo setRightMargin:mUnwriteableMargin.right];
  [printInfo setBottomMargin:mUnwriteableMargin.bottom];
  [printInfo setLeftMargin:mUnwriteableMargin.left];

  // If the printer name is the name of our pseudo print-to-PDF printer, the
  // following `setPrinter` call will fail silently, since macOS doesn't know
  // anything about it. That's OK, because mPrinter is our canonical source of
  // truth.
  // Actually, it seems Mac OS X 10.12 (the oldest version of Mac that we
  // support) hangs if the printer name is not recognized. For now we explicitly
  // check for our print-to-PDF printer, but that is not ideal since we should
  // really localize the name of this printer at some point. Once we drop
  // support for 10.12 we should remove this check.
  if (!mPrinter.EqualsLiteral("Mozilla Save to PDF")) {
    [printInfo setPrinter:[NSPrinter printerWithName:nsCocoaUtils::ToNSString(
                                                         mPrinter)]];
  }

  // Scaling is handled by gecko, we do NOT want the cocoa printing system to
  // add a second scaling on top of that. So we only set the true scaling factor
  // here if the caller explicitly asked for it.
  [printInfo setScalingFactor:CGFloat(aWithScaling ? mScaling : 1.0f)];

  const bool allPages = mPageRanges.IsEmpty();

  NSMutableDictionary* dict = [printInfo dictionary];
  [dict setObject:[NSNumber numberWithInt:mNumCopies] forKey:NSPrintCopies];
  [dict setObject:[NSNumber numberWithBool:allPages] forKey:NSPrintAllPages];

  int32_t start = 1;
  int32_t end = 1;
  for (size_t i = 0; i < mPageRanges.Length(); i += 2) {
    start = std::min(start, mPageRanges[i]);
    end = std::max(end, mPageRanges[i + 1]);
  }

  [dict setObject:[NSNumber numberWithInt:start] forKey:NSPrintFirstPage];
  [dict setObject:[NSNumber numberWithInt:end] forKey:NSPrintLastPage];

  NSURL* jobSavingURL = nullptr;
  if (!mToFileName.IsEmpty()) {
    jobSavingURL =
        [NSURL fileURLWithPath:nsCocoaUtils::ToNSString(mToFileName)];
    if (jobSavingURL) {
      // Note: the PMPrintSettingsSetJobName call in nsPrintDialogServiceX::Show
      // seems to mean that we get a sensible file name pre-populated in the
      // dialog there, although our mToFileName is expected to be a full path,
      // and it's less clear where the rest of the path (the directory to save
      // to) in nsPrintDialogServiceX::Show comes from (perhaps from the use
      // of `sharedPrintInfo` to initialize new nsPrintSettingsX objects).
      [dict setObject:jobSavingURL forKey:NSPrintJobSavingURL];
    }
  }

  if (mDisposition.IsEmpty()) {
    // NOTE: It's unclear what to do for kOutputDestinationStream but this is
    // only for the native print dialog where that can't happen.
    if (mOutputDestination == kOutputDestinationFile) {
      [printInfo setJobDisposition:NSPrintSaveJob];
    } else {
      [printInfo setJobDisposition:NSPrintSpoolJob];
    }
  } else {
    [printInfo setJobDisposition:nsCocoaUtils::ToNSString(mDisposition)];
  }

  PMDuplexMode duplexSetting;
  switch (mDuplex) {
    default:
      // This can't happen :) but if it does, we treat it as "none".
      MOZ_FALLTHROUGH_ASSERT("Unknown duplex value");
    case kDuplexNone:
      duplexSetting = kPMDuplexNone;
      break;
    case kDuplexFlipOnLongEdge:
      duplexSetting = kPMDuplexNoTumble;
      break;
    case kDuplexFlipOnShortEdge:
      duplexSetting = kPMDuplexTumble;
      break;
  }

  NSMutableDictionary* printSettings = [printInfo printSettings];
  [printSettings setObject:[NSNumber numberWithUnsignedShort:duplexSetting]
                    forKey:@"com_apple_print_PrintSettings_PMDuplexing"];

  if (mDestination != kPMDestinationInvalid) {
    // Required to support PDF-workflow destinations such as Save to Web
    // Receipts.
    [printSettings
        setObject:[NSNumber numberWithUnsignedShort:mDestination]
           forKey:@"com_apple_print_PrintSettings_PMDestinationType"];
    if (jobSavingURL) {
      [printSettings
          setObject:[jobSavingURL absoluteString]
             forKey:@"com_apple_print_PrintSettings_PMOutputFilename"];
    }
  }

  if (StaticPrefs::print_cups_monochrome_enabled() && !GetPrintInColor()) {
    for (const auto& setting : kKnownMonochromeSettings) {
      [printSettings setObject:setting.mValue forKey:setting.mName];
    }
    auto applySetting = [&](const nsACString& aKey, const nsACString& aValue) {
      [printSettings setObject:nsCocoaUtils::ToNSString(aValue)
                        forKey:nsCocoaUtils::ToNSString(aKey)];
    };
    nsPrinterCUPS::ForEachExtraMonochromeSetting(applySetting);
  }

  return printInfo;

  NS_OBJC_END_TRY_BLOCK_RETURN(nullptr);
}

void nsPrintSettingsX::SetFromPrintInfo(NSPrintInfo* aPrintInfo,
                                        bool aAdoptPrintInfo) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // Set page-size/margins.
  NSSize paperSize = [aPrintInfo paperSize];
  const bool areSheetsOfPaperPortraitMode =
      ([aPrintInfo orientation] == NSPaperOrientationPortrait);

  // If our MacOS print settings say that we're producing portrait-mode sheets
  // of paper, then our page format must also be portrait-mode; unless we've
  // got a pages-per-sheet value with orthogonal pages/sheets, in which case
  // it's reversed.
  const bool arePagesPortraitMode =
      (areSheetsOfPaperPortraitMode != HasOrthogonalPagesPerSheet());

  if (arePagesPortraitMode) {
    mOrientation = nsIPrintSettings::kPortraitOrientation;
    SetPaperWidth(PaperSizeFromCocoaPoints(paperSize.width));
    SetPaperHeight(PaperSizeFromCocoaPoints(paperSize.height));
  } else {
    mOrientation = nsIPrintSettings::kLandscapeOrientation;
    SetPaperWidth(PaperSizeFromCocoaPoints(paperSize.height));
    SetPaperHeight(PaperSizeFromCocoaPoints(paperSize.width));
  }

  mUnwriteableMargin.top = static_cast<int32_t>([aPrintInfo topMargin]);
  mUnwriteableMargin.right = static_cast<int32_t>([aPrintInfo rightMargin]);
  mUnwriteableMargin.bottom = static_cast<int32_t>([aPrintInfo bottomMargin]);
  mUnwriteableMargin.left = static_cast<int32_t>([aPrintInfo leftMargin]);

  if (aAdoptPrintInfo) {
    // Keep a reference to the printInfo; it may have settings that we don't
    // know how to handle otherwise.
    if (mSystemPrintInfo != aPrintInfo) {
      if (mSystemPrintInfo) {
        [mSystemPrintInfo release];
      }
      mSystemPrintInfo = aPrintInfo;
      [mSystemPrintInfo retain];
    }
  } else {
    // Clear any stored printInfo.
    if (mSystemPrintInfo) {
      [mSystemPrintInfo release];
      mSystemPrintInfo = nullptr;
    }
  }

  nsCocoaUtils::GetStringForNSString([[aPrintInfo printer] name], mPrinter);

  // Only get the scaling value if shrink-to-fit is not selected:
  bool isShrinkToFitChecked;
  GetShrinkToFit(&isShrinkToFitChecked);
  if (!isShrinkToFitChecked) {
    // Limit scaling precision to whole percentage values.
    mScaling = round(double([aPrintInfo scalingFactor]) * 100.0) / 100.0;
  }

  mOutputDestination = [&] {
    if ([aPrintInfo jobDisposition] == NSPrintSaveJob) {
      return kOutputDestinationFile;
    }
    return kOutputDestinationPrinter;
  }();

  NSDictionary* dict = [aPrintInfo dictionary];
  const char* filePath =
      [[dict objectForKey:NSPrintJobSavingURL] fileSystemRepresentation];
  if (filePath && *filePath) {
    CopyUTF8toUTF16(Span(filePath, strlen(filePath)), mToFileName);
  }

  nsCocoaUtils::GetStringForNSString([aPrintInfo jobDisposition], mDisposition);

  mNumCopies = [[dict objectForKey:NSPrintCopies] intValue];
  mPageRanges.Clear();
  if (![[dict objectForKey:NSPrintAllPages] boolValue]) {
    mPageRanges.AppendElement([[dict objectForKey:NSPrintFirstPage] intValue]);
    mPageRanges.AppendElement([[dict objectForKey:NSPrintLastPage] intValue]);
  }

  NSDictionary* printSettings = [aPrintInfo printSettings];
  NSNumber* value =
      [printSettings objectForKey:@"com_apple_print_PrintSettings_PMDuplexing"];
  if (value) {
    PMDuplexMode duplexSetting = [value unsignedShortValue];
    switch (duplexSetting) {
      default:
        // An unknown value is treated as None.
        MOZ_FALLTHROUGH_ASSERT("Unknown duplex value");
      case kPMDuplexNone:
        mDuplex = kDuplexNone;
        break;
      case kPMDuplexNoTumble:
        mDuplex = kDuplexFlipOnLongEdge;
        break;
      case kPMDuplexTumble:
        mDuplex = kDuplexFlipOnShortEdge;
        break;
    }
  } else {
    // By default a printSettings dictionary doesn't initially contain the
    // duplex key at all, so this is not an error; its absence just means no
    // duplexing has been requested, so we return kDuplexNone.
    mDuplex = kDuplexNone;
  }

  value = [printSettings
      objectForKey:@"com_apple_print_PrintSettings_PMDestinationType"];
  if (value) {
    mDestination = [value unsignedShortValue];
  }

  const bool color = [&] {
    if (StaticPrefs::print_cups_monochrome_enabled()) {
      for (const auto& setting : kKnownMonochromeSettings) {
        NSString* value = [printSettings objectForKey:setting.mName];
        if (!value) {
          continue;
        }
        if ([setting.mValue isEqualToString:value]) {
          return false;
        }
      }
    }
    return true;
  }();

  SetPrintInColor(color);

  SetIsInitializedFromPrinter(true);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}
