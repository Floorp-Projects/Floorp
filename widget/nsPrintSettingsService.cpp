/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintSettingsService.h"

#include "mozilla/embedding/PPrinting.h"
#include "mozilla/layout/RemotePrintJobChild.h"
#include "mozilla/RefPtr.h"
#include "nsCoord.h"
#include "nsIPrinterList.h"
#include "nsPrintingProxy.h"
#include "nsReadableUtils.h"
#include "nsPrintSettingsImpl.h"
#include "nsIPrintSession.h"
#include "nsServiceManagerUtils.h"
#include "nsSize.h"

#include "nsArray.h"
#include "nsXPCOM.h"
#include "nsXULAppAPI.h"

#include "nsIStringEnumerator.h"
#include "stdlib.h"
#include "mozilla/StaticPrefs_print.h"
#include "mozilla/Preferences.h"
#include "nsPrintfCString.h"

using namespace mozilla;
using namespace mozilla::embedding;

typedef mozilla::layout::RemotePrintJobChild RemotePrintJobChild;

NS_IMPL_ISUPPORTS(nsPrintSettingsService, nsIPrintSettingsService)

// Pref Constants
static const char kMarginTop[] = "print_margin_top";
static const char kMarginLeft[] = "print_margin_left";
static const char kMarginBottom[] = "print_margin_bottom";
static const char kMarginRight[] = "print_margin_right";
static const char kEdgeTop[] = "print_edge_top";
static const char kEdgeLeft[] = "print_edge_left";
static const char kEdgeBottom[] = "print_edge_bottom";
static const char kEdgeRight[] = "print_edge_right";
static const char kUnwriteableMarginTop[] = "print_unwriteable_margin_top";
static const char kUnwriteableMarginLeft[] = "print_unwriteable_margin_left";
static const char kUnwriteableMarginBottom[] =
    "print_unwriteable_margin_bottom";
static const char kUnwriteableMarginRight[] = "print_unwriteable_margin_right";

// Prefs for Print Options
static const char kPrintHeaderStrLeft[] = "print_headerleft";
static const char kPrintHeaderStrCenter[] = "print_headercenter";
static const char kPrintHeaderStrRight[] = "print_headerright";
static const char kPrintFooterStrLeft[] = "print_footerleft";
static const char kPrintFooterStrCenter[] = "print_footercenter";
static const char kPrintFooterStrRight[] = "print_footerright";

// Additional Prefs
static const char kPrintReversed[] = "print_reversed";
static const char kPrintInColor[] = "print_in_color";
static const char kPrintPaperId[] = "print_paper_id";
static const char kPrintPaperSizeUnit[] = "print_paper_size_unit";
static const char kPrintPaperWidth[] = "print_paper_width";
static const char kPrintPaperHeight[] = "print_paper_height";
static const char kPrintOrientation[] = "print_orientation";
static const char kPrinterName[] = "print_printer";
static const char kPrintToFile[] = "print_to_file";
static const char kPrintToFileName[] = "print_to_filename";
static const char kPrintPageDelay[] = "print_page_delay";
static const char kPrintBGColors[] = "print_bgcolor";
static const char kPrintBGImages[] = "print_bgimages";
static const char kPrintShrinkToFit[] = "print_shrink_to_fit";
static const char kPrintScaling[] = "print_scaling";
static const char kPrintResolution[] = "print_resolution";
static const char kPrintDuplex[] = "print_duplex";

static const char kJustLeft[] = "left";
static const char kJustCenter[] = "center";
static const char kJustRight[] = "right";

#define NS_PRINTER_LIST_CONTRACTID "@mozilla.org/gfx/printerlist;1"

nsresult nsPrintSettingsService::Init() { return NS_OK; }

NS_IMETHODIMP
nsPrintSettingsService::SerializeToPrintData(nsIPrintSettings* aSettings,
                                             PrintData* data) {
  aSettings->GetPageRanges(data->pageRanges());

  aSettings->GetEdgeTop(&data->edgeTop());
  aSettings->GetEdgeLeft(&data->edgeLeft());
  aSettings->GetEdgeBottom(&data->edgeBottom());
  aSettings->GetEdgeRight(&data->edgeRight());

  aSettings->GetMarginTop(&data->marginTop());
  aSettings->GetMarginLeft(&data->marginLeft());
  aSettings->GetMarginBottom(&data->marginBottom());
  aSettings->GetMarginRight(&data->marginRight());
  aSettings->GetUnwriteableMarginTop(&data->unwriteableMarginTop());
  aSettings->GetUnwriteableMarginLeft(&data->unwriteableMarginLeft());
  aSettings->GetUnwriteableMarginBottom(&data->unwriteableMarginBottom());
  aSettings->GetUnwriteableMarginRight(&data->unwriteableMarginRight());

  aSettings->GetScaling(&data->scaling());

  data->printBGColors() = aSettings->GetPrintBGColors();
  data->printBGImages() = aSettings->GetPrintBGImages();

  data->honorPageRuleMargins() = aSettings->GetHonorPageRuleMargins();
  data->showMarginGuides() = aSettings->GetShowMarginGuides();
  data->isPrintSelectionRBEnabled() = aSettings->GetIsPrintSelectionRBEnabled();
  data->printSelectionOnly() = aSettings->GetPrintSelectionOnly();

  aSettings->GetTitle(data->title());
  aSettings->GetDocURL(data->docURL());

  aSettings->GetHeaderStrLeft(data->headerStrLeft());
  aSettings->GetHeaderStrCenter(data->headerStrCenter());
  aSettings->GetHeaderStrRight(data->headerStrRight());

  aSettings->GetFooterStrLeft(data->footerStrLeft());
  aSettings->GetFooterStrCenter(data->footerStrCenter());
  aSettings->GetFooterStrRight(data->footerStrRight());

  aSettings->GetIsCancelled(&data->isCancelled());
  aSettings->GetPrintSilent(&data->printSilent());
  aSettings->GetShrinkToFit(&data->shrinkToFit());
  aSettings->GetShowPrintProgress(&data->showPrintProgress());

  aSettings->GetPaperId(data->paperId());
  aSettings->GetPaperWidth(&data->paperWidth());
  aSettings->GetPaperHeight(&data->paperHeight());
  aSettings->GetPaperSizeUnit(&data->paperSizeUnit());

  aSettings->GetPrintReversed(&data->printReversed());
  aSettings->GetPrintInColor(&data->printInColor());
  aSettings->GetOrientation(&data->orientation());

  aSettings->GetNumCopies(&data->numCopies());
  aSettings->GetNumPagesPerSheet(&data->numPagesPerSheet());

  aSettings->GetPrinterName(data->printerName());

  aSettings->GetPrintToFile(&data->printToFile());

  aSettings->GetToFileName(data->toFileName());

  aSettings->GetOutputFormat(&data->outputFormat());
  aSettings->GetPrintPageDelay(&data->printPageDelay());
  aSettings->GetResolution(&data->resolution());
  aSettings->GetDuplex(&data->duplex());
  aSettings->GetIsInitializedFromPrinter(&data->isInitializedFromPrinter());
  aSettings->GetIsInitializedFromPrefs(&data->isInitializedFromPrefs());

  // Initialize the platform-specific values that don't
  // default-initialize, so that we don't send uninitialized data over
  // IPC (which leads to valgrind warnings, and, for bools, fatal
  // assertions).
  // data->driverName() default-initializes
  // data->deviceName() default-initializes
  // data->GTKPrintSettings() default-initializes

  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsService::DeserializeToPrintSettings(const PrintData& data,
                                                   nsIPrintSettings* settings) {
  nsCOMPtr<nsIPrintSession> session;
  nsresult rv = settings->GetPrintSession(getter_AddRefs(session));
  if (NS_SUCCEEDED(rv) && session) {
    session->SetRemotePrintJob(
        static_cast<RemotePrintJobChild*>(data.remotePrintJobChild()));
  }

  settings->SetPageRanges(data.pageRanges());

  settings->SetEdgeTop(data.edgeTop());
  settings->SetEdgeLeft(data.edgeLeft());
  settings->SetEdgeBottom(data.edgeBottom());
  settings->SetEdgeRight(data.edgeRight());

  settings->SetMarginTop(data.marginTop());
  settings->SetMarginLeft(data.marginLeft());
  settings->SetMarginBottom(data.marginBottom());
  settings->SetMarginRight(data.marginRight());
  settings->SetUnwriteableMarginTop(data.unwriteableMarginTop());
  settings->SetUnwriteableMarginLeft(data.unwriteableMarginLeft());
  settings->SetUnwriteableMarginBottom(data.unwriteableMarginBottom());
  settings->SetUnwriteableMarginRight(data.unwriteableMarginRight());

  settings->SetScaling(data.scaling());

  settings->SetPrintBGColors(data.printBGColors());
  settings->SetPrintBGImages(data.printBGImages());
  settings->SetHonorPageRuleMargins(data.honorPageRuleMargins());
  settings->SetShowMarginGuides(data.showMarginGuides());
  settings->SetIsPrintSelectionRBEnabled(data.isPrintSelectionRBEnabled());
  settings->SetPrintSelectionOnly(data.printSelectionOnly());

  settings->SetTitle(data.title());
  settings->SetDocURL(data.docURL());

  // Header strings...
  settings->SetHeaderStrLeft(data.headerStrLeft());
  settings->SetHeaderStrCenter(data.headerStrCenter());
  settings->SetHeaderStrRight(data.headerStrRight());

  // Footer strings...
  settings->SetFooterStrLeft(data.footerStrLeft());
  settings->SetFooterStrCenter(data.footerStrCenter());
  settings->SetFooterStrRight(data.footerStrRight());

  settings->SetIsCancelled(data.isCancelled());
  settings->SetPrintSilent(data.printSilent());
  settings->SetShrinkToFit(data.shrinkToFit());
  settings->SetShowPrintProgress(data.showPrintProgress());

  settings->SetPaperId(data.paperId());

  settings->SetPaperWidth(data.paperWidth());
  settings->SetPaperHeight(data.paperHeight());
  settings->SetPaperSizeUnit(data.paperSizeUnit());

  settings->SetPrintReversed(data.printReversed());
  settings->SetPrintInColor(data.printInColor());
  settings->SetOrientation(data.orientation());

  settings->SetNumCopies(data.numCopies());
  settings->SetNumPagesPerSheet(data.numPagesPerSheet());

  settings->SetPrinterName(data.printerName());

  settings->SetPrintToFile(data.printToFile());

  settings->SetToFileName(data.toFileName());

  settings->SetOutputFormat(data.outputFormat());
  settings->SetPrintPageDelay(data.printPageDelay());
  settings->SetResolution(data.resolution());
  settings->SetDuplex(data.duplex());
  settings->SetIsInitializedFromPrinter(data.isInitializedFromPrinter());
  settings->SetIsInitializedFromPrefs(data.isInitializedFromPrefs());

  return NS_OK;
}

/** ---------------------------------------------------
 *  Helper function - Creates the "prefix" for the pref
 *  It is either "print."
 *  or "print.printer_<print name>."
 */
const char* nsPrintSettingsService::GetPrefName(const char* aPrefName,
                                                const nsAString& aPrinterName) {
  if (!aPrefName || !*aPrefName) {
    NS_ERROR("Must have a valid pref name!");
    return aPrefName;
  }

  mPrefName.AssignLiteral("print.");

  if (aPrinterName.Length()) {
    mPrefName.AppendLiteral("printer_");
    AppendUTF16toUTF8(aPrinterName, mPrefName);
    mPrefName.Append('.');
  }
  mPrefName += aPrefName;

  return mPrefName.get();
}

/**
 *  This will either read in the generic prefs (not specific to a printer)
 *  or read the prefs in using the printer name to qualify.
 *  It is either "print.attr_name" or "print.printer_HPLasr5.attr_name"
 */
nsresult nsPrintSettingsService::ReadPrefs(nsIPrintSettings* aPS,
                                           const nsAString& aPrinterName,
                                           uint32_t aFlags) {
  NS_ENSURE_ARG_POINTER(aPS);

  bool b;
  nsAutoString str;
  int32_t iVal;
  double dbl;

#define GETBOOLPREF(_prefname, _retval) \
  NS_SUCCEEDED(                         \
      Preferences::GetBool(GetPrefName(_prefname, aPrinterName), _retval))

#define GETSTRPREF(_prefname, _retval) \
  NS_SUCCEEDED(                        \
      Preferences::GetString(GetPrefName(_prefname, aPrinterName), _retval))

#define GETINTPREF(_prefname, _retval) \
  NS_SUCCEEDED(                        \
      Preferences::GetInt(GetPrefName(_prefname, aPrinterName), _retval))

#define GETDBLPREF(_prefname, _retval) \
  NS_SUCCEEDED(ReadPrefDouble(GetPrefName(_prefname, aPrinterName), _retval))

  bool gotPaperSizeFromPrefs = false;
  int16_t paperSizeUnit;
  double paperWidth, paperHeight;

  // Paper size prefs are read as a group
  if (aFlags & nsIPrintSettings::kInitSavePaperSize) {
    gotPaperSizeFromPrefs = GETINTPREF(kPrintPaperSizeUnit, &iVal) &&
                            GETDBLPREF(kPrintPaperWidth, paperWidth) &&
                            GETDBLPREF(kPrintPaperHeight, paperHeight) &&
                            GETSTRPREF(kPrintPaperId, str);
    paperSizeUnit = (int16_t)iVal;

    if (gotPaperSizeFromPrefs &&
        paperSizeUnit != nsIPrintSettings::kPaperSizeInches &&
        paperSizeUnit != nsIPrintSettings::kPaperSizeMillimeters) {
      gotPaperSizeFromPrefs = false;
    }

    if (gotPaperSizeFromPrefs) {
      // Bug 315687: Sanity check paper size to avoid paper size values in
      // mm when the size unit flag is inches. The value 100 is arbitrary
      // and can be changed.
      gotPaperSizeFromPrefs =
          (paperSizeUnit != nsIPrintSettings::kPaperSizeInches) ||
          (paperWidth < 100.0) || (paperHeight < 100.0);
    }

    if (gotPaperSizeFromPrefs) {
      aPS->SetPaperSizeUnit(paperSizeUnit);
      aPS->SetPaperWidth(paperWidth);
      aPS->SetPaperHeight(paperHeight);
      aPS->SetPaperId(str);
    }
  }

  nsIntSize pageSizeInTwips;  // to sanity check margins
  if (!gotPaperSizeFromPrefs) {
    aPS->GetPaperSizeUnit(&paperSizeUnit);
    aPS->GetPaperWidth(&paperWidth);
    aPS->GetPaperHeight(&paperHeight);
  }
  if (paperSizeUnit == nsIPrintSettings::kPaperSizeMillimeters) {
    pageSizeInTwips = nsIntSize((int)NS_MILLIMETERS_TO_TWIPS(paperWidth),
                                (int)NS_MILLIMETERS_TO_TWIPS(paperHeight));
  } else {
    pageSizeInTwips = nsIntSize((int)NS_INCHES_TO_TWIPS(paperWidth),
                                (int)NS_INCHES_TO_TWIPS(paperHeight));
  }

  auto MarginIsOK = [&pageSizeInTwips](const nsIntMargin& aMargin) {
    return aMargin.top >= 0 && aMargin.right >= 0 && aMargin.bottom >= 0 &&
           aMargin.left >= 0 && aMargin.LeftRight() < pageSizeInTwips.width &&
           aMargin.TopBottom() < pageSizeInTwips.height;
  };

  if (aFlags & nsIPrintSettings::kInitSaveUnwriteableMargins) {
    nsIntMargin margin;
    ReadInchesIntToTwipsPref(GetPrefName(kUnwriteableMarginTop, aPrinterName),
                             margin.top, kUnwriteableMarginTop);
    ReadInchesIntToTwipsPref(GetPrefName(kUnwriteableMarginLeft, aPrinterName),
                             margin.left, kUnwriteableMarginLeft);
    ReadInchesIntToTwipsPref(
        GetPrefName(kUnwriteableMarginBottom, aPrinterName), margin.bottom,
        kUnwriteableMarginBottom);
    ReadInchesIntToTwipsPref(GetPrefName(kUnwriteableMarginRight, aPrinterName),
                             margin.right, kUnwriteableMarginRight);
    // SetUnwriteableMarginInTwips does its own validation and drops negative
    // values individually.  We still want to block overly large values though,
    // so we do that part of MarginIsOK manually.
    if (margin.LeftRight() < pageSizeInTwips.width &&
        margin.TopBottom() < pageSizeInTwips.height) {
      aPS->SetUnwriteableMarginInTwips(margin);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveMargins) {
    int32_t halfInch = NS_INCHES_TO_INT_TWIPS(0.5);
    nsIntMargin margin(halfInch, halfInch, halfInch, halfInch);
    ReadInchesToTwipsPref(GetPrefName(kMarginTop, aPrinterName), margin.top,
                          kMarginTop);
    ReadInchesToTwipsPref(GetPrefName(kMarginLeft, aPrinterName), margin.left,
                          kMarginLeft);
    ReadInchesToTwipsPref(GetPrefName(kMarginBottom, aPrinterName),
                          margin.bottom, kMarginBottom);
    ReadInchesToTwipsPref(GetPrefName(kMarginRight, aPrinterName), margin.right,
                          kMarginRight);
    if (MarginIsOK(margin)) {
      aPS->SetMarginInTwips(margin);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveEdges) {
    nsIntMargin margin(0, 0, 0, 0);
    ReadInchesIntToTwipsPref(GetPrefName(kEdgeTop, aPrinterName), margin.top,
                             kEdgeTop);
    ReadInchesIntToTwipsPref(GetPrefName(kEdgeLeft, aPrinterName), margin.left,
                             kEdgeLeft);
    ReadInchesIntToTwipsPref(GetPrefName(kEdgeBottom, aPrinterName),
                             margin.bottom, kEdgeBottom);
    ReadInchesIntToTwipsPref(GetPrefName(kEdgeRight, aPrinterName),
                             margin.right, kEdgeRight);
    if (MarginIsOK(margin)) {
      aPS->SetEdgeInTwips(margin);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveHeaderLeft) {
    if (GETSTRPREF(kPrintHeaderStrLeft, str)) {
      aPS->SetHeaderStrLeft(str);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveHeaderCenter) {
    if (GETSTRPREF(kPrintHeaderStrCenter, str)) {
      aPS->SetHeaderStrCenter(str);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveHeaderRight) {
    if (GETSTRPREF(kPrintHeaderStrRight, str)) {
      aPS->SetHeaderStrRight(str);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveFooterLeft) {
    if (GETSTRPREF(kPrintFooterStrLeft, str)) {
      aPS->SetFooterStrLeft(str);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveFooterCenter) {
    if (GETSTRPREF(kPrintFooterStrCenter, str)) {
      aPS->SetFooterStrCenter(str);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveFooterRight) {
    if (GETSTRPREF(kPrintFooterStrRight, str)) {
      aPS->SetFooterStrRight(str);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveBGColors) {
    if (GETBOOLPREF(kPrintBGColors, &b)) {
      aPS->SetPrintBGColors(b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveBGImages) {
    if (GETBOOLPREF(kPrintBGImages, &b)) {
      aPS->SetPrintBGImages(b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveReversed) {
    if (GETBOOLPREF(kPrintReversed, &b)) {
      aPS->SetPrintReversed(b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveInColor) {
    if (GETBOOLPREF(kPrintInColor, &b)) {
      aPS->SetPrintInColor(b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveOrientation) {
    if (GETINTPREF(kPrintOrientation, &iVal) &&
        (iVal == nsIPrintSettings::kPortraitOrientation ||
         iVal == nsIPrintSettings::kLandscapeOrientation)) {
      aPS->SetOrientation(iVal);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSavePrintToFile) {
    if (GETBOOLPREF(kPrintToFile, &b)) {
      aPS->SetPrintToFile(b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveToFileName) {
    if (GETSTRPREF(kPrintToFileName, str)) {
      if (StringEndsWith(str, u".ps"_ns)) {
        // We only support PDF since bug 1425188 landed.  Users may still have
        // prefs with .ps filenames if they last saved a file as Postscript
        // though, so we fix that up here.  (The pref values will be
        // overwritten the next time they save to file as a PDF.)
        str.Truncate(str.Length() - 2);
        str.AppendLiteral("pdf");
      }
      aPS->SetToFileName(str);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSavePageDelay) {
    // milliseconds
    if (GETINTPREF(kPrintPageDelay, &iVal) && iVal >= 0 && iVal <= 1000) {
      aPS->SetPrintPageDelay(iVal);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveShrinkToFit) {
    if (GETBOOLPREF(kPrintShrinkToFit, &b)) {
      aPS->SetShrinkToFit(b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveScaling) {
    // The limits imposed here are fairly arbitrary and mainly intended to
    // purge bad values which tend to be negative and/or very large.  If we
    // get complaints from users that settings outside these values "aren't
    // saved" then we can consider increasing them.
    if (GETDBLPREF(kPrintScaling, dbl) && dbl >= 0.05 && dbl <= 20) {
      aPS->SetScaling(dbl);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveResolution) {
    // DPI. Again, an arbitrary range mainly to purge bad values that have made
    // their way into user prefs.
    if (GETINTPREF(kPrintResolution, &iVal) && iVal >= 50 && iVal <= 12000) {
      aPS->SetResolution(iVal);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveDuplex) {
    if (GETINTPREF(kPrintDuplex, &iVal)) {
      aPS->SetDuplex(iVal);
    }
  }

  // Not Reading In:
  //   Number of Copies

  return NS_OK;
}

nsresult nsPrintSettingsService::WritePrefs(nsIPrintSettings* aPS,
                                            const nsAString& aPrinterName,
                                            uint32_t aFlags) {
  NS_ENSURE_ARG_POINTER(aPS);

  if (aFlags & nsIPrintSettings::kInitSaveMargins) {
    nsIntMargin margin = aPS->GetMarginInTwips();
    WriteInchesFromTwipsPref(GetPrefName(kMarginTop, aPrinterName), margin.top);
    WriteInchesFromTwipsPref(GetPrefName(kMarginLeft, aPrinterName),
                             margin.left);
    WriteInchesFromTwipsPref(GetPrefName(kMarginBottom, aPrinterName),
                             margin.bottom);
    WriteInchesFromTwipsPref(GetPrefName(kMarginRight, aPrinterName),
                             margin.right);
  }

  if (aFlags & nsIPrintSettings::kInitSaveEdges) {
    nsIntMargin edge = aPS->GetEdgeInTwips();
    WriteInchesIntFromTwipsPref(GetPrefName(kEdgeTop, aPrinterName), edge.top);
    WriteInchesIntFromTwipsPref(GetPrefName(kEdgeLeft, aPrinterName),
                                edge.left);
    WriteInchesIntFromTwipsPref(GetPrefName(kEdgeBottom, aPrinterName),
                                edge.bottom);
    WriteInchesIntFromTwipsPref(GetPrefName(kEdgeRight, aPrinterName),
                                edge.right);
  }

  if (aFlags & nsIPrintSettings::kInitSaveUnwriteableMargins) {
    nsIntMargin unwriteableMargin = aPS->GetUnwriteableMarginInTwips();
    WriteInchesIntFromTwipsPref(
        GetPrefName(kUnwriteableMarginTop, aPrinterName),
        unwriteableMargin.top);
    WriteInchesIntFromTwipsPref(
        GetPrefName(kUnwriteableMarginLeft, aPrinterName),
        unwriteableMargin.left);
    WriteInchesIntFromTwipsPref(
        GetPrefName(kUnwriteableMarginBottom, aPrinterName),
        unwriteableMargin.bottom);
    WriteInchesIntFromTwipsPref(
        GetPrefName(kUnwriteableMarginRight, aPrinterName),
        unwriteableMargin.right);
  }

  // Paper size prefs are saved as a group
  if (aFlags & nsIPrintSettings::kInitSavePaperSize) {
    int16_t sizeUnit;
    double width, height;
    nsString name;

    if (NS_SUCCEEDED(aPS->GetPaperSizeUnit(&sizeUnit)) &&
        NS_SUCCEEDED(aPS->GetPaperWidth(&width)) &&
        NS_SUCCEEDED(aPS->GetPaperHeight(&height)) &&
        NS_SUCCEEDED(aPS->GetPaperId(name))) {
      Preferences::SetInt(GetPrefName(kPrintPaperSizeUnit, aPrinterName),
                          int32_t(sizeUnit));
      WritePrefDouble(GetPrefName(kPrintPaperWidth, aPrinterName), width);
      WritePrefDouble(GetPrefName(kPrintPaperHeight, aPrinterName), height);
      Preferences::SetString(GetPrefName(kPrintPaperId, aPrinterName), name);
    }
  }

  bool b;
  nsString uStr;
  int32_t iVal;
  double dbl;

  if (aFlags & nsIPrintSettings::kInitSaveHeaderLeft) {
    if (NS_SUCCEEDED(aPS->GetHeaderStrLeft(uStr))) {
      Preferences::SetString(GetPrefName(kPrintHeaderStrLeft, aPrinterName),
                             uStr);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveHeaderCenter) {
    if (NS_SUCCEEDED(aPS->GetHeaderStrCenter(uStr))) {
      Preferences::SetString(GetPrefName(kPrintHeaderStrCenter, aPrinterName),
                             uStr);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveHeaderRight) {
    if (NS_SUCCEEDED(aPS->GetHeaderStrRight(uStr))) {
      Preferences::SetString(GetPrefName(kPrintHeaderStrRight, aPrinterName),
                             uStr);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveFooterLeft) {
    if (NS_SUCCEEDED(aPS->GetFooterStrLeft(uStr))) {
      Preferences::SetString(GetPrefName(kPrintFooterStrLeft, aPrinterName),
                             uStr);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveFooterCenter) {
    if (NS_SUCCEEDED(aPS->GetFooterStrCenter(uStr))) {
      Preferences::SetString(GetPrefName(kPrintFooterStrCenter, aPrinterName),
                             uStr);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveFooterRight) {
    if (NS_SUCCEEDED(aPS->GetFooterStrRight(uStr))) {
      Preferences::SetString(GetPrefName(kPrintFooterStrRight, aPrinterName),
                             uStr);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveBGColors) {
    b = aPS->GetPrintBGColors();
    Preferences::SetBool(GetPrefName(kPrintBGColors, aPrinterName), b);
  }

  if (aFlags & nsIPrintSettings::kInitSaveBGImages) {
    b = aPS->GetPrintBGImages();
    Preferences::SetBool(GetPrefName(kPrintBGImages, aPrinterName), b);
  }

  if (aFlags & nsIPrintSettings::kInitSaveReversed) {
    if (NS_SUCCEEDED(aPS->GetPrintReversed(&b))) {
      Preferences::SetBool(GetPrefName(kPrintReversed, aPrinterName), b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveInColor) {
    if (NS_SUCCEEDED(aPS->GetPrintInColor(&b))) {
      Preferences::SetBool(GetPrefName(kPrintInColor, aPrinterName), b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveOrientation) {
    if (NS_SUCCEEDED(aPS->GetOrientation(&iVal))) {
      Preferences::SetInt(GetPrefName(kPrintOrientation, aPrinterName), iVal);
    }
  }

  // Only the general version of this pref is saved
  if ((aFlags & nsIPrintSettings::kInitSavePrinterName) &&
      aPrinterName.IsEmpty()) {
    if (NS_SUCCEEDED(aPS->GetPrinterName(uStr))) {
      Preferences::SetString(kPrinterName, uStr);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSavePrintToFile) {
    if (NS_SUCCEEDED(aPS->GetPrintToFile(&b))) {
      Preferences::SetBool(GetPrefName(kPrintToFile, aPrinterName), b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveToFileName) {
    if (NS_SUCCEEDED(aPS->GetToFileName(uStr))) {
      Preferences::SetString(GetPrefName(kPrintToFileName, aPrinterName), uStr);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSavePageDelay) {
    if (NS_SUCCEEDED(aPS->GetPrintPageDelay(&iVal))) {
      Preferences::SetInt(GetPrefName(kPrintPageDelay, aPrinterName), iVal);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveShrinkToFit) {
    if (NS_SUCCEEDED(aPS->GetShrinkToFit(&b))) {
      Preferences::SetBool(GetPrefName(kPrintShrinkToFit, aPrinterName), b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveScaling) {
    if (NS_SUCCEEDED(aPS->GetScaling(&dbl))) {
      WritePrefDouble(GetPrefName(kPrintScaling, aPrinterName), dbl);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveResolution) {
    if (NS_SUCCEEDED(aPS->GetResolution(&iVal))) {
      Preferences::SetInt(GetPrefName(kPrintResolution, aPrinterName), iVal);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveDuplex) {
    if (NS_SUCCEEDED(aPS->GetDuplex(&iVal))) {
      Preferences::SetInt(GetPrefName(kPrintDuplex, aPrinterName), iVal);
    }
  }

  // Not Writing Out:
  //   Number of Copies

  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsService::GetDefaultPrintSettingsForPrinting(
    nsIPrintSettings** aPrintSettings) {
  nsresult rv = GetNewPrintSettings(aPrintSettings);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIPrintSettings* settings = *aPrintSettings;

  nsAutoString printerName;
  settings->GetPrinterName(printerName);
  if (printerName.IsEmpty()) {
    GetLastUsedPrinterName(printerName);
    settings->SetPrinterName(printerName);
  }
  InitPrintSettingsFromPrinter(printerName, settings);
  InitPrintSettingsFromPrefs(settings, true, nsIPrintSettings::kInitSaveAll);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsService::GetNewPrintSettings(
    nsIPrintSettings** aNewPrintSettings) {
  return _CreatePrintSettings(aNewPrintSettings);
}

NS_IMETHODIMP
nsPrintSettingsService::GetLastUsedPrinterName(
    nsAString& aLastUsedPrinterName) {
  aLastUsedPrinterName.Truncate();
  Preferences::GetString(kPrinterName, aLastUsedPrinterName);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsService::InitPrintSettingsFromPrinter(
    const nsAString& aPrinterName, nsIPrintSettings* aPrintSettings) {
  // Don't get print settings from the printer in the child when printing via
  // parent, these will be retrieved in the parent later in the print process.
  if (XRE_IsContentProcess() && StaticPrefs::print_print_via_parent()) {
    return NS_OK;
  }

  NS_ENSURE_ARG_POINTER(aPrintSettings);

#ifdef DEBUG
  nsString printerName;
  aPrintSettings->GetPrinterName(printerName);
  if (!printerName.Equals(aPrinterName)) {
    NS_WARNING("Printer names should match!");
  }
#endif

  bool isInitialized;
  aPrintSettings->GetIsInitializedFromPrinter(&isInitialized);
  if (isInitialized) return NS_OK;

  nsresult rv;
  nsCOMPtr<nsIPrinterList> printerList =
      do_GetService(NS_PRINTER_LIST_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = printerList->InitPrintSettingsFromPrinter(aPrinterName, aPrintSettings);
  NS_ENSURE_SUCCESS(rv, rv);

  aPrintSettings->SetIsInitializedFromPrinter(true);
  return rv;
}

/** ---------------------------------------------------
 *  Helper function - Returns either the name or sets the length to zero
 */
static nsresult GetAdjustedPrinterName(nsIPrintSettings* aPS, bool aUsePNP,
                                       nsAString& aPrinterName) {
  NS_ENSURE_ARG_POINTER(aPS);

  aPrinterName.Truncate();
  if (!aUsePNP) return NS_OK;

  // Get the Printer Name from the PrintSettings
  // to use as a prefix for Pref Names
  nsresult rv = aPS->GetPrinterName(aPrinterName);
  NS_ENSURE_SUCCESS(rv, rv);

  // Convert any whitespaces, carriage returns or newlines to _
  // The below algorithm is supposedly faster than using iterators
  constexpr auto replSubstr = u"_"_ns;
  const char* replaceStr = " \n\r";

  int32_t x;
  for (x = 0; x < (int32_t)strlen(replaceStr); x++) {
    char16_t uChar = replaceStr[x];

    int32_t i = 0;
    while ((i = aPrinterName.FindChar(uChar, i)) != kNotFound) {
      aPrinterName.Replace(i, 1, replSubstr);
      i++;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsService::InitPrintSettingsFromPrefs(nsIPrintSettings* aPS,
                                                   bool aUsePNP,
                                                   uint32_t aFlags) {
  NS_ENSURE_ARG_POINTER(aPS);

  bool isInitialized;
  aPS->GetIsInitializedFromPrefs(&isInitialized);

  if (isInitialized) {
    return NS_OK;
  }

  auto globalPrintSettings = aFlags;
#ifndef MOZ_WIDGET_ANDROID
  globalPrintSettings &= nsIPrintSettings::kGlobalSettings;
#endif

  nsAutoString prtName;
  // read any non printer specific prefs
  // with empty printer name
  nsresult rv = ReadPrefs(aPS, prtName, globalPrintSettings);
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the Printer Name from the PrintSettings to use as a prefix for Pref
  // Names
  rv = GetAdjustedPrinterName(aPS, aUsePNP, prtName);
  NS_ENSURE_SUCCESS(rv, rv);

  if (prtName.IsEmpty()) {
    NS_WARNING("Caller should supply a printer name.");
    return NS_OK;
  }

  // Now read any printer specific prefs
  rv = ReadPrefs(aPS, prtName, aFlags);
  if (NS_SUCCEEDED(rv)) aPS->SetIsInitializedFromPrefs(true);

  return NS_OK;
}

/**
 *  Save all of the printer settings; if we can find a printer name, save
 *  printer-specific preferences. Otherwise, save generic ones.
 */
nsresult nsPrintSettingsService::SavePrintSettingsToPrefs(
    nsIPrintSettings* aPS, bool aUsePrinterNamePrefix, uint32_t aFlags) {
  NS_ENSURE_ARG_POINTER(aPS);

  if (GeckoProcessType_Content == XRE_GetProcessType()) {
    // If we're in the content process, we can't directly write to the
    // Preferences service - we have to proxy the save up to the
    // parent process.
    RefPtr<nsPrintingProxy> proxy = nsPrintingProxy::GetInstance();
    return proxy->SavePrintSettings(aPS, aUsePrinterNamePrefix, aFlags);
  }

  nsAutoString prtName;

  // Get the printer name from the PrinterSettings for an optional prefix.
  nsresult rv = GetAdjustedPrinterName(aPS, aUsePrinterNamePrefix, prtName);
  NS_ENSURE_SUCCESS(rv, rv);

  // Write the prefs, with or without a printer name prefix.
  return WritePrefs(aPS, prtName, aFlags);
}

//-----------------------------------------------------
//-- Protected Methods --------------------------------
//-----------------------------------------------------
nsresult nsPrintSettingsService::ReadPrefDouble(const char* aPrefId,
                                                double& aVal) {
  NS_ENSURE_ARG_POINTER(aPrefId);

  nsAutoCString str;
  nsresult rv = Preferences::GetCString(aPrefId, str);
  if (NS_FAILED(rv) || str.IsEmpty()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  double value = str.ToDouble(&rv);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aVal = value;
  return NS_OK;
}

nsresult nsPrintSettingsService::WritePrefDouble(const char* aPrefId,
                                                 double aVal) {
  NS_ENSURE_ARG_POINTER(aPrefId);

  nsAutoCString str;
  // We cast to a float so we only get up to 6 digits precision in the prefs.
  str.AppendFloat((float)aVal);
  return Preferences::SetCString(aPrefId, str);
}

void nsPrintSettingsService::ReadInchesToTwipsPref(const char* aPrefId,
                                                   int32_t& aTwips,
                                                   const char* aMarginPref) {
  nsAutoString str;
  nsresult rv = Preferences::GetString(aPrefId, str);
  if (NS_FAILED(rv) || str.IsEmpty()) {
    rv = Preferences::GetString(aMarginPref, str);
  }
  if (NS_SUCCEEDED(rv) && !str.IsEmpty()) {
    nsresult errCode;
    float inches = str.ToFloat(&errCode);
    if (NS_SUCCEEDED(errCode)) {
      aTwips = NS_INCHES_TO_INT_TWIPS(inches);
    } else {
      aTwips = 0;
    }
  }
}

void nsPrintSettingsService::WriteInchesFromTwipsPref(const char* aPrefId,
                                                      int32_t aTwips) {
  double inches = NS_TWIPS_TO_INCHES(aTwips);
  nsAutoCString inchesStr;
  inchesStr.AppendFloat(inches);

  Preferences::SetCString(aPrefId, inchesStr);
}

void nsPrintSettingsService::ReadInchesIntToTwipsPref(const char* aPrefId,
                                                      int32_t& aTwips,
                                                      const char* aMarginPref) {
  int32_t value;
  nsresult rv = Preferences::GetInt(aPrefId, &value);
  if (NS_FAILED(rv)) {
    rv = Preferences::GetInt(aMarginPref, &value);
  }
  if (NS_SUCCEEDED(rv)) {
    aTwips = NS_INCHES_TO_INT_TWIPS(float(value) / 100.0f);
  } else {
    aTwips = 0;
  }
}

void nsPrintSettingsService::WriteInchesIntFromTwipsPref(const char* aPrefId,
                                                         int32_t aTwips) {
  Preferences::SetInt(aPrefId,
                      int32_t(NS_TWIPS_TO_INCHES(aTwips) * 100.0f + 0.5f));
}

void nsPrintSettingsService::ReadJustification(const char* aPrefId,
                                               int16_t& aJust,
                                               int16_t aInitValue) {
  aJust = aInitValue;
  nsAutoString justStr;
  if (NS_SUCCEEDED(Preferences::GetString(aPrefId, justStr))) {
    if (justStr.EqualsASCII(kJustRight)) {
      aJust = nsIPrintSettings::kJustRight;
    } else if (justStr.EqualsASCII(kJustCenter)) {
      aJust = nsIPrintSettings::kJustCenter;
    } else {
      aJust = nsIPrintSettings::kJustLeft;
    }
  }
}

//---------------------------------------------------
void nsPrintSettingsService::WriteJustification(const char* aPrefId,
                                                int16_t aJust) {
  switch (aJust) {
    case nsIPrintSettings::kJustLeft:
      Preferences::SetCString(aPrefId, kJustLeft);
      break;

    case nsIPrintSettings::kJustCenter:
      Preferences::SetCString(aPrefId, kJustCenter);
      break;

    case nsIPrintSettings::kJustRight:
      Preferences::SetCString(aPrefId, kJustRight);
      break;
  }  // switch
}
