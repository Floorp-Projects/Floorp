/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintSettingsServiceX.h"

#include "nsCOMPtr.h"
#include "nsQueryObject.h"
#include "nsPrintSettingsX.h"
#include "nsCocoaUtils.h"

using namespace mozilla::embedding;

NS_IMETHODIMP
nsPrintSettingsServiceX::SerializeToPrintData(nsIPrintSettings* aSettings, PrintData* data) {
  nsresult rv = nsPrintSettingsService::SerializeToPrintData(aSettings, data);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<nsPrintSettingsX> settingsX(do_QueryObject(aSettings));
  if (NS_WARN_IF(!settingsX)) {
    return NS_ERROR_FAILURE;
  }

  double adjustedWidth, adjustedHeight;
  settingsX->GetAdjustedPaperSize(&adjustedWidth, &adjustedHeight);
  data->adjustedPaperWidth() = adjustedWidth;
  data->adjustedPaperHeight() = adjustedHeight;

  if (XRE_IsParentProcess()) {
    return SerializeToPrintDataParent(aSettings, data);
  }

  return NS_OK;
}

nsresult nsPrintSettingsServiceX::SerializeToPrintDataParent(nsIPrintSettings* aSettings,
                                                             PrintData* data) {
  RefPtr<nsPrintSettingsX> settingsX(do_QueryObject(aSettings));
  if (NS_WARN_IF(!settingsX)) {
    return NS_ERROR_FAILURE;
  }

  NSPrintInfo* printInfo = settingsX->GetCocoaPrintInfo();
  if (NS_WARN_IF(!printInfo)) {
    return NS_ERROR_FAILURE;
  }

  NSDictionary* dict = [printInfo dictionary];
  if (NS_WARN_IF(!dict)) {
    return NS_ERROR_FAILURE;
  }

  NSString* printerName = [dict objectForKey:NSPrintPrinterName];
  if (printerName) {
    nsCocoaUtils::GetStringForNSString(printerName, data->printerName());
  }

  NSString* faxNumber = [dict objectForKey:NSPrintFaxNumber];
  if (faxNumber) {
    nsCocoaUtils::GetStringForNSString(faxNumber, data->faxNumber());
  }

  NSURL* printToFileURL = [dict objectForKey:NSPrintJobSavingURL];
  if (printToFileURL) {
    nsCocoaUtils::GetStringForNSString([printToFileURL absoluteString], data -> toFileName());
  }

  NSDate* printTime = [dict objectForKey:NSPrintTime];
  if (printTime) {
    NSTimeInterval timestamp = [printTime timeIntervalSinceReferenceDate];
    data->printTime() = timestamp;
  }

  NSString* disposition = [dict objectForKey:NSPrintJobDisposition];
  if (disposition) {
    nsCocoaUtils::GetStringForNSString(disposition, data->disposition());
  }

  NSString* paperName = [dict objectForKey:NSPrintPaperName];
  if (paperName) {
    nsCocoaUtils::GetStringForNSString(paperName, data->paperName());
  }

  float scalingFactor = [[dict objectForKey:NSPrintScalingFactor] floatValue];
  data->scalingFactor() = scalingFactor;

  int32_t orientation;
  if ([printInfo orientation] == NS_PAPER_ORIENTATION_PORTRAIT) {
    orientation = nsIPrintSettings::kPortraitOrientation;
  } else {
    orientation = nsIPrintSettings::kLandscapeOrientation;
  }
  data->orientation() = orientation;

  NSSize paperSize = [printInfo paperSize];
  float widthScale, heightScale;
  settingsX->GetInchesScale(&widthScale, &heightScale);
  if (orientation == nsIPrintSettings::kLandscapeOrientation) {
    // switch widths and heights
    data->widthScale() = heightScale;
    data->heightScale() = widthScale;
    data->paperWidth() = paperSize.height / heightScale;
    data->paperHeight() = paperSize.width / widthScale;
  } else {
    data->widthScale() = widthScale;
    data->heightScale() = heightScale;
    data->paperWidth() = paperSize.width / widthScale;
    data->paperHeight() = paperSize.height / heightScale;
  }

  data->numCopies() = [[dict objectForKey:NSPrintCopies] intValue];
  data->printAllPages() = [[dict objectForKey:NSPrintAllPages] boolValue];
  data->startPageRange() = [[dict objectForKey:NSPrintFirstPage] intValue];
  data->endPageRange() = [[dict objectForKey:NSPrintLastPage] intValue];
  data->mustCollate() = [[dict objectForKey:NSPrintMustCollate] boolValue];
  data->printReversed() = [[dict objectForKey:NSPrintReversePageOrder] boolValue];
  data->pagesAcross() = [[dict objectForKey:NSPrintPagesAcross] unsignedShortValue];
  data->pagesDown() = [[dict objectForKey:NSPrintPagesDown] unsignedShortValue];
  data->detailedErrorReporting() = [[dict objectForKey:NSPrintDetailedErrorReporting] boolValue];
  data->addHeaderAndFooter() = [[dict objectForKey:NSPrintHeaderAndFooter] boolValue];
  data->fileNameExtensionHidden() =
      [[dict objectForKey:NSPrintJobSavingFileNameExtensionHidden] boolValue];

  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsServiceX::DeserializeToPrintSettings(const PrintData& data,
                                                    nsIPrintSettings* settings) {
  nsresult rv = nsPrintSettingsService::DeserializeToPrintSettings(data, settings);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (data.orientation() == nsIPrintSettings::kPortraitOrientation) {
    settings->SetOrientation(nsIPrintSettings::kPortraitOrientation);
  } else {
    settings->SetOrientation(nsIPrintSettings::kLandscapeOrientation);
  }

  RefPtr<nsPrintSettingsX> settingsX(do_QueryObject(settings));
  if (NS_WARN_IF(!settingsX)) {
    return NS_ERROR_FAILURE;
  }
  settingsX->SetAdjustedPaperSize(data.adjustedPaperWidth(), data.adjustedPaperHeight());

  return NS_OK;
}

nsresult nsPrintSettingsServiceX::ReadPrefs(nsIPrintSettings* aPS, const nsAString& aPrinterName,
                                            uint32_t aFlags) {
  nsresult rv;

  rv = nsPrintSettingsService::ReadPrefs(aPS, aPrinterName, aFlags);
  NS_ASSERTION(NS_SUCCEEDED(rv), "nsPrintSettingsService::ReadPrefs() failed");

  RefPtr<nsPrintSettingsX> printSettingsX(do_QueryObject(aPS));
  if (!printSettingsX) return NS_ERROR_NO_INTERFACE;
  rv = printSettingsX->ReadPageFormatFromPrefs();

  return NS_OK;
}

nsresult nsPrintSettingsServiceX::_CreatePrintSettings(nsIPrintSettings** _retval) {
  nsresult rv;
  *_retval = nullptr;

  nsPrintSettingsX* printSettings = new nsPrintSettingsX;  // does not initially ref count
  NS_ENSURE_TRUE(printSettings, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*_retval = printSettings);

  rv = printSettings->Init();
  if (NS_FAILED(rv)) {
    NS_RELEASE(*_retval);
    return rv;
  }

  InitPrintSettingsFromPrefs(*_retval, false, nsIPrintSettings::kInitSaveAll);
  return rv;
}

nsresult nsPrintSettingsServiceX::WritePrefs(nsIPrintSettings* aPS, const nsAString& aPrinterName,
                                             uint32_t aFlags) {
  nsresult rv;

  rv = nsPrintSettingsService::WritePrefs(aPS, aPrinterName, aFlags);
  NS_ASSERTION(NS_SUCCEEDED(rv), "nsPrintSettingsService::WritePrefs() failed");

  RefPtr<nsPrintSettingsX> printSettingsX(do_QueryObject(aPS));
  if (!printSettingsX) return NS_ERROR_NO_INTERFACE;
  rv = printSettingsX->WritePageFormatToPrefs();
  NS_ASSERTION(NS_SUCCEEDED(rv), "nsPrintSettingsX::WritePageFormatToPrefs() failed");

  return NS_OK;
}
