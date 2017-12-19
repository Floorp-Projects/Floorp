/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintSettingsServiceX.h"

#include "nsCOMPtr.h"
#include "nsQueryObject.h"
#include "nsIServiceManager.h"
#include "nsPrintSettingsX.h"
#include "nsIWebBrowserPrint.h"
#include "nsCocoaUtils.h"

using namespace mozilla::embedding;

nsPrintOptionsX::nsPrintOptionsX()
{
}

nsPrintOptionsX::~nsPrintOptionsX()
{
}

NS_IMETHODIMP
nsPrintOptionsX::SerializeToPrintData(nsIPrintSettings* aSettings,
                                      nsIWebBrowserPrint* aWBP,
                                      PrintData* data)
{
  nsresult rv = nsPrintOptions::SerializeToPrintData(aSettings, aWBP, data);
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
    return SerializeToPrintDataParent(aSettings, aWBP, data);
  }

  return SerializeToPrintDataChild(aSettings, aWBP, data);
}

nsresult
nsPrintOptionsX::SerializeToPrintDataChild(nsIPrintSettings* aSettings,
                                           nsIWebBrowserPrint* aWBP,
                                           PrintData* data)
{
  // If we are in the child process, we don't need to populate
  // nsPrintSettingsX completely. The parent discards almost all of
  // this data (bug 1328975). Furthermore, reading some of the
  // printer/printing settings from the OS causes a connection to the
  // printer to be made which is blocked by sandboxing and results in hangs.
  if (aWBP) {
    // When serializing an nsIWebBrowserPrint, we need to pass up the first
    // document name. We could pass up the entire collection of document
    // names, but the OS X printing prompt code only really cares about
    // the first one, so we just send the first to save IPC traffic.
    char16_t** docTitles;
    uint32_t titleCount;
    nsresult rv = aWBP->EnumerateDocumentNames(&titleCount, &docTitles);
    if (NS_SUCCEEDED(rv)) {
      if (titleCount > 0) {
        data->printJobName().Assign(docTitles[0]);
      }

      for (int32_t i = titleCount - 1; i >= 0; i--) {
        free(docTitles[i]);
      }
      free(docTitles);
      docTitles = nullptr;
    }
  }

  return NS_OK;
}

nsresult
nsPrintOptionsX::SerializeToPrintDataParent(nsIPrintSettings* aSettings,
                                            nsIWebBrowserPrint* aWBP,
                                            PrintData* data)
{
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

  NSString* printerName = [dict objectForKey: NSPrintPrinterName];
  if (printerName) {
    nsCocoaUtils::GetStringForNSString(printerName, data->printerName());
  }

  NSString* faxNumber = [dict objectForKey: NSPrintFaxNumber];
  if (faxNumber) {
    nsCocoaUtils::GetStringForNSString(faxNumber, data->faxNumber());
  }

  NSURL* printToFileURL = [dict objectForKey: NSPrintJobSavingURL];
  if (printToFileURL) {
    nsCocoaUtils::GetStringForNSString([printToFileURL absoluteString],
                                       data->toFileName());
  }

  NSDate* printTime = [dict objectForKey: NSPrintTime];
  if (printTime) {
    NSTimeInterval timestamp = [printTime timeIntervalSinceReferenceDate];
    data->printTime() = timestamp;
  }

  NSString* disposition = [dict objectForKey: NSPrintJobDisposition];
  if (disposition) {
    nsCocoaUtils::GetStringForNSString(disposition, data->disposition());
  }

  NSString* paperName = [dict objectForKey: NSPrintPaperName];
  if (paperName) {
    nsCocoaUtils::GetStringForNSString(paperName, data->paperName());
  }

  float scalingFactor = [[dict objectForKey: NSPrintScalingFactor] floatValue];
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

  data->numCopies() = [[dict objectForKey: NSPrintCopies] intValue];
  data->printAllPages() = [[dict objectForKey: NSPrintAllPages] boolValue];
  data->startPageRange() = [[dict objectForKey: NSPrintFirstPage] intValue];
  data->endPageRange() = [[dict objectForKey: NSPrintLastPage] intValue];
  data->mustCollate() = [[dict objectForKey: NSPrintMustCollate] boolValue];
  data->printReversed() = [[dict objectForKey: NSPrintReversePageOrder] boolValue];
  data->pagesAcross() = [[dict objectForKey: NSPrintPagesAcross] unsignedShortValue];
  data->pagesDown() = [[dict objectForKey: NSPrintPagesDown] unsignedShortValue];
  data->detailedErrorReporting() = [[dict objectForKey: NSPrintDetailedErrorReporting] boolValue];
  data->addHeaderAndFooter() = [[dict objectForKey: NSPrintHeaderAndFooter] boolValue];
  data->fileNameExtensionHidden() =
    [[dict objectForKey: NSPrintJobSavingFileNameExtensionHidden] boolValue];

  return NS_OK;
}

NS_IMETHODIMP
nsPrintOptionsX::DeserializeToPrintSettings(const PrintData& data,
                                            nsIPrintSettings* settings)
{
  nsresult rv = nsPrintOptions::DeserializeToPrintSettings(data, settings);
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
  settingsX->SetAdjustedPaperSize(data.adjustedPaperWidth(),
                                  data.adjustedPaperHeight());

  return NS_OK;
}

nsresult
nsPrintOptionsX::ReadPrefs(nsIPrintSettings* aPS, const nsAString& aPrinterName, uint32_t aFlags)
{
  nsresult rv;
  
  rv = nsPrintOptions::ReadPrefs(aPS, aPrinterName, aFlags);
  NS_ASSERTION(NS_SUCCEEDED(rv), "nsPrintOptions::ReadPrefs() failed");
  
  RefPtr<nsPrintSettingsX> printSettingsX(do_QueryObject(aPS));
  if (!printSettingsX)
    return NS_ERROR_NO_INTERFACE;
  rv = printSettingsX->ReadPageFormatFromPrefs();
  
  return NS_OK;
}

nsresult nsPrintOptionsX::_CreatePrintSettings(nsIPrintSettings **_retval)
{
  nsresult rv;
  *_retval = nullptr;

  nsPrintSettingsX* printSettings = new nsPrintSettingsX; // does not initially ref count
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

nsresult
nsPrintOptionsX::WritePrefs(nsIPrintSettings* aPS, const nsAString& aPrinterName, uint32_t aFlags)
{
  nsresult rv;

  rv = nsPrintOptions::WritePrefs(aPS, aPrinterName, aFlags);
  NS_ASSERTION(NS_SUCCEEDED(rv), "nsPrintOptions::WritePrefs() failed");

  RefPtr<nsPrintSettingsX> printSettingsX(do_QueryObject(aPS));
  if (!printSettingsX)
    return NS_ERROR_NO_INTERFACE;
  rv = printSettingsX->WritePageFormatToPrefs();
  NS_ASSERTION(NS_SUCCEEDED(rv), "nsPrintSettingsX::WritePageFormatToPrefs() failed");

  return NS_OK;
}
