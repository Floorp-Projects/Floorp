/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintOptionsImpl.h"

#include "mozilla/embedding/PPrinting.h"
#include "mozilla/layout/RemotePrintJobChild.h"
#include "mozilla/RefPtr.h"
#include "nsIPrinterEnumerator.h"
#include "nsPrintingProxy.h"
#include "nsReadableUtils.h"
#include "nsPrintSettingsImpl.h"
#include "nsIPrintSession.h"
#include "nsServiceManagerUtils.h"

#include "nsArray.h"
#include "nsIDOMWindow.h"
#include "nsIDialogParamBlock.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIWindowWatcher.h"
#include "prprf.h"

#include "nsIStringEnumerator.h"
#include "nsISupportsPrimitives.h"
#include "stdlib.h"
#include "mozilla/Preferences.h"
#include "nsPrintfCString.h"
#include "nsIWebBrowserPrint.h"

using namespace mozilla;
using namespace mozilla::embedding;

typedef mozilla::layout::RemotePrintJobChild RemotePrintJobChild;

NS_IMPL_ISUPPORTS(nsPrintOptions, nsIPrintSettingsService)

// Pref Constants
static const char kMarginTop[]       = "print_margin_top";
static const char kMarginLeft[]      = "print_margin_left";
static const char kMarginBottom[]    = "print_margin_bottom";
static const char kMarginRight[]     = "print_margin_right";
static const char kEdgeTop[]         = "print_edge_top";
static const char kEdgeLeft[]        = "print_edge_left";
static const char kEdgeBottom[]      = "print_edge_bottom";
static const char kEdgeRight[]       = "print_edge_right";
static const char kUnwriteableMarginTop[]    = "print_unwriteable_margin_top";
static const char kUnwriteableMarginLeft[]   = "print_unwriteable_margin_left";
static const char kUnwriteableMarginBottom[] = "print_unwriteable_margin_bottom";
static const char kUnwriteableMarginRight[]  = "print_unwriteable_margin_right";

// Prefs for Print Options
static const char kPrintEvenPages[]       = "print_evenpages";
static const char kPrintOddPages[]        = "print_oddpages";
static const char kPrintHeaderStrLeft[]   = "print_headerleft";
static const char kPrintHeaderStrCenter[] = "print_headercenter";
static const char kPrintHeaderStrRight[]  = "print_headerright";
static const char kPrintFooterStrLeft[]   = "print_footerleft";
static const char kPrintFooterStrCenter[] = "print_footercenter";
static const char kPrintFooterStrRight[]  = "print_footerright";

// Additional Prefs
static const char kPrintReversed[]      = "print_reversed";
static const char kPrintInColor[]       = "print_in_color";
static const char kPrintPaperName[]     = "print_paper_name";
static const char kPrintPaperData[]     = "print_paper_data";
static const char kPrintPaperSizeUnit[] = "print_paper_size_unit";
static const char kPrintPaperWidth[]    = "print_paper_width";
static const char kPrintPaperHeight[]   = "print_paper_height";
static const char kPrintOrientation[]   = "print_orientation";
static const char kPrinterName[]        = "print_printer";
static const char kPrintToFile[]        = "print_to_file";
static const char kPrintToFileName[]    = "print_to_filename";
static const char kPrintPageDelay[]     = "print_page_delay";
static const char kPrintBGColors[]      = "print_bgcolor";
static const char kPrintBGImages[]      = "print_bgimages";
static const char kPrintShrinkToFit[]   = "print_shrink_to_fit";
static const char kPrintScaling[]       = "print_scaling";
static const char kPrintResolution[]    = "print_resolution";
static const char kPrintDuplex[]        = "print_duplex";

static const char kJustLeft[]   = "left";
static const char kJustCenter[] = "center";
static const char kJustRight[]  = "right";

#define NS_PRINTER_ENUMERATOR_CONTRACTID "@mozilla.org/gfx/printerenumerator;1"

nsPrintOptions::nsPrintOptions()
{
}

nsPrintOptions::~nsPrintOptions()
{
}

nsresult
nsPrintOptions::Init()
{
  return NS_OK;
}

NS_IMETHODIMP
nsPrintOptions::SerializeToPrintData(nsIPrintSettings* aSettings,
                                     nsIWebBrowserPrint* aWBP,
                                     PrintData* data)
{
  nsCOMPtr<nsIPrintSession> session;
  nsresult rv = aSettings->GetPrintSession(getter_AddRefs(session));
  if (NS_SUCCEEDED(rv) && session) {
    RefPtr<RemotePrintJobChild> remotePrintJob;
    rv = session->GetRemotePrintJob(getter_AddRefs(remotePrintJob));
    if (NS_SUCCEEDED(rv)) {
      data->remotePrintJobChild() = remotePrintJob;
    }
  }

  aSettings->GetStartPageRange(&data->startPageRange());
  aSettings->GetEndPageRange(&data->endPageRange());

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

  aSettings->GetPrintBGColors(&data->printBGColors());
  aSettings->GetPrintBGImages(&data->printBGImages());
  aSettings->GetPrintRange(&data->printRange());

  // I have no idea if I'm doing this string copying correctly...
  nsXPIDLString title;
  aSettings->GetTitle(getter_Copies(title));
  data->title() = title;

  nsXPIDLString docURL;
  aSettings->GetDocURL(getter_Copies(docURL));
  data->docURL() = docURL;

  // Header strings...
  nsXPIDLString headerStrLeft;
  aSettings->GetHeaderStrLeft(getter_Copies(headerStrLeft));
  data->headerStrLeft() = headerStrLeft;

  nsXPIDLString headerStrCenter;
  aSettings->GetHeaderStrCenter(getter_Copies(headerStrCenter));
  data->headerStrCenter() = headerStrCenter;

  nsXPIDLString headerStrRight;
  aSettings->GetHeaderStrRight(getter_Copies(headerStrRight));
  data->headerStrRight() = headerStrRight;

  // Footer strings...
  nsXPIDLString footerStrLeft;
  aSettings->GetFooterStrLeft(getter_Copies(footerStrLeft));
  data->footerStrLeft() = footerStrLeft;

  nsXPIDLString footerStrCenter;
  aSettings->GetFooterStrCenter(getter_Copies(footerStrCenter));
  data->footerStrCenter() = footerStrCenter;

  nsXPIDLString footerStrRight;
  aSettings->GetFooterStrRight(getter_Copies(footerStrRight));
  data->footerStrRight() = footerStrRight;

  aSettings->GetHowToEnableFrameUI(&data->howToEnableFrameUI());
  aSettings->GetIsCancelled(&data->isCancelled());
  aSettings->GetPrintFrameTypeUsage(&data->printFrameTypeUsage());
  aSettings->GetPrintFrameType(&data->printFrameType());
  aSettings->GetPrintSilent(&data->printSilent());
  aSettings->GetShrinkToFit(&data->shrinkToFit());
  aSettings->GetShowPrintProgress(&data->showPrintProgress());

  nsXPIDLString paperName;
  aSettings->GetPaperName(getter_Copies(paperName));
  data->paperName() = paperName;

  aSettings->GetPaperData(&data->paperData());
  aSettings->GetPaperWidth(&data->paperWidth());
  aSettings->GetPaperHeight(&data->paperHeight());
  aSettings->GetPaperSizeUnit(&data->paperSizeUnit());

  aSettings->GetPrintReversed(&data->printReversed());
  aSettings->GetPrintInColor(&data->printInColor());
  aSettings->GetOrientation(&data->orientation());

  aSettings->GetNumCopies(&data->numCopies());

  nsXPIDLString printerName;
  aSettings->GetPrinterName(getter_Copies(printerName));
  data->printerName() = printerName;

  aSettings->GetPrintToFile(&data->printToFile());

  nsXPIDLString toFileName;
  aSettings->GetToFileName(getter_Copies(toFileName));
  data->toFileName() = toFileName;

  aSettings->GetOutputFormat(&data->outputFormat());
  aSettings->GetPrintPageDelay(&data->printPageDelay());
  aSettings->GetResolution(&data->resolution());
  aSettings->GetDuplex(&data->duplex());
  aSettings->GetIsInitializedFromPrinter(&data->isInitializedFromPrinter());
  aSettings->GetIsInitializedFromPrefs(&data->isInitializedFromPrefs());

  aSettings->GetPrintOptionsBits(&data->optionFlags());

  // Initialize the platform-specific values that don't
  // default-initialize, so that we don't send uninitialized data over
  // IPC (which leads to valgrind warnings, and, for bools, fatal
  // assertions).
  // data->driverName() default-initializes
  // data->deviceName() default-initializes
  data->printableWidthInInches() = 0;
  data->printableHeightInInches() = 0;
  data->isFramesetDocument() = false;
  data->isFramesetFrameSelected() = false;
  data->isIFrameSelected() = false;
  data->isRangeSelection() = false;
  // data->GTKPrintSettings() default-initializes
  // data->printJobName() default-initializes
  data->printAllPages() = true;
  data->mustCollate() = false;
  // data->disposition() default-initializes
  data->pagesAcross() = 1;
  data->pagesDown() = 1;
  data->printTime() = 0;
  data->detailedErrorReporting() = true;
  // data->faxNumber() default-initializes
  data->addHeaderAndFooter() = false;
  data->fileNameExtensionHidden() = false;

  return NS_OK;
}

NS_IMETHODIMP
nsPrintOptions::DeserializeToPrintSettings(const PrintData& data,
                                           nsIPrintSettings* settings)
{
  nsCOMPtr<nsIPrintSession> session;
  nsresult rv = settings->GetPrintSession(getter_AddRefs(session));
  if (NS_SUCCEEDED(rv) && session) {
    session->SetRemotePrintJob(
      static_cast<RemotePrintJobChild*>(data.remotePrintJobChild()));
  }
  settings->SetStartPageRange(data.startPageRange());
  settings->SetEndPageRange(data.endPageRange());

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
  settings->SetPrintRange(data.printRange());

  // I have no idea if I'm doing this string copying correctly...
  settings->SetTitle(data.title().get());
  settings->SetDocURL(data.docURL().get());

  // Header strings...
  settings->SetHeaderStrLeft(data.headerStrLeft().get());
  settings->SetHeaderStrCenter(data.headerStrCenter().get());
  settings->SetHeaderStrRight(data.headerStrRight().get());

  // Footer strings...
  settings->SetFooterStrLeft(data.footerStrLeft().get());
  settings->SetFooterStrCenter(data.footerStrCenter().get());
  settings->SetFooterStrRight(data.footerStrRight().get());

  settings->SetHowToEnableFrameUI(data.howToEnableFrameUI());
  settings->SetIsCancelled(data.isCancelled());
  settings->SetPrintFrameTypeUsage(data.printFrameTypeUsage());
  settings->SetPrintFrameType(data.printFrameType());
  settings->SetPrintSilent(data.printSilent());
  settings->SetShrinkToFit(data.shrinkToFit());
  settings->SetShowPrintProgress(data.showPrintProgress());

  settings->SetPaperName(data.paperName().get());

  settings->SetPaperData(data.paperData());
  settings->SetPaperWidth(data.paperWidth());
  settings->SetPaperHeight(data.paperHeight());
  settings->SetPaperSizeUnit(data.paperSizeUnit());

  settings->SetPrintReversed(data.printReversed());
  settings->SetPrintInColor(data.printInColor());
  settings->SetOrientation(data.orientation());

  settings->SetNumCopies(data.numCopies());

  settings->SetPrinterName(data.printerName().get());

  settings->SetPrintToFile(data.printToFile());

  settings->SetToFileName(data.toFileName().get());

  settings->SetOutputFormat(data.outputFormat());
  settings->SetPrintPageDelay(data.printPageDelay());
  settings->SetResolution(data.resolution());
  settings->SetDuplex(data.duplex());
  settings->SetIsInitializedFromPrinter(data.isInitializedFromPrinter());
  settings->SetIsInitializedFromPrefs(data.isInitializedFromPrefs());

  settings->SetPrintOptionsBits(data.optionFlags());

  return NS_OK;
}


/** ---------------------------------------------------
 *  Helper function - Creates the "prefix" for the pref
 *  It is either "print."
 *  or "print.printer_<print name>."
 */
const char*
nsPrintOptions::GetPrefName(const char * aPrefName,
                            const nsAString& aPrinterName)
{
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

//----------------------------------------------------------------------
// Testing of read/write prefs
// This define controls debug output
#ifdef DEBUG_rods_X
static void WriteDebugStr(const char* aArg1, const char* aArg2,
                          const char16_t* aStr)
{
  nsString str(aStr);
  char16_t s = '&';
  char16_t r = '_';
  str.ReplaceChar(s, r);

  printf("%s %s = %s \n", aArg1, aArg2, ToNewUTF8String(str));
}
const char* kWriteStr = "Write Pref:";
const char* kReadStr  = "Read Pref:";
#define DUMP_STR(_a1, _a2, _a3)  WriteDebugStr((_a1), GetPrefName((_a2), \
aPrefName), (_a3));
#define DUMP_BOOL(_a1, _a2, _a3) printf("%s %s = %s \n", (_a1), \
GetPrefName((_a2), aPrefName), (_a3)?"T":"F");
#define DUMP_INT(_a1, _a2, _a3)  printf("%s %s = %d \n", (_a1), \
GetPrefName((_a2), aPrefName), (_a3));
#define DUMP_DBL(_a1, _a2, _a3)  printf("%s %s = %10.5f \n", (_a1), \
GetPrefName((_a2), aPrefName), (_a3));
#else
#define DUMP_STR(_a1, _a2, _a3)
#define DUMP_BOOL(_a1, _a2, _a3)
#define DUMP_INT(_a1, _a2, _a3)
#define DUMP_DBL(_a1, _a2, _a3)
#endif /* DEBUG_rods_X */
//----------------------------------------------------------------------

/**
 *  This will either read in the generic prefs (not specific to a printer)
 *  or read the prefs in using the printer name to qualify.
 *  It is either "print.attr_name" or "print.printer_HPLasr5.attr_name"
 */
nsresult
nsPrintOptions::ReadPrefs(nsIPrintSettings* aPS, const nsAString& aPrinterName,
                          uint32_t aFlags)
{
  NS_ENSURE_ARG_POINTER(aPS);

  if (aFlags & nsIPrintSettings::kInitSaveMargins) {
    int32_t halfInch = NS_INCHES_TO_INT_TWIPS(0.5);
    nsIntMargin margin(halfInch, halfInch, halfInch, halfInch);
    ReadInchesToTwipsPref(GetPrefName(kMarginTop, aPrinterName), margin.top,
                          kMarginTop);
    DUMP_INT(kReadStr, kMarginTop, margin.top);
    ReadInchesToTwipsPref(GetPrefName(kMarginLeft, aPrinterName), margin.left,
                          kMarginLeft);
    DUMP_INT(kReadStr, kMarginLeft, margin.left);
    ReadInchesToTwipsPref(GetPrefName(kMarginBottom, aPrinterName),
                          margin.bottom, kMarginBottom);
    DUMP_INT(kReadStr, kMarginBottom, margin.bottom);
    ReadInchesToTwipsPref(GetPrefName(kMarginRight, aPrinterName), margin.right,
                          kMarginRight);
    DUMP_INT(kReadStr, kMarginRight, margin.right);
    aPS->SetMarginInTwips(margin);
  }

  if (aFlags & nsIPrintSettings::kInitSaveEdges) {
    nsIntMargin margin(0,0,0,0);
    ReadInchesIntToTwipsPref(GetPrefName(kEdgeTop, aPrinterName), margin.top,
                             kEdgeTop);
    DUMP_INT(kReadStr, kEdgeTop, margin.top);
    ReadInchesIntToTwipsPref(GetPrefName(kEdgeLeft, aPrinterName), margin.left,
                             kEdgeLeft);
    DUMP_INT(kReadStr, kEdgeLeft, margin.left);
    ReadInchesIntToTwipsPref(GetPrefName(kEdgeBottom, aPrinterName),
                             margin.bottom, kEdgeBottom);
    DUMP_INT(kReadStr, kEdgeBottom, margin.bottom);
    ReadInchesIntToTwipsPref(GetPrefName(kEdgeRight, aPrinterName), margin.right,
                             kEdgeRight);
    DUMP_INT(kReadStr, kEdgeRight, margin.right);
    aPS->SetEdgeInTwips(margin);
  }

  if (aFlags & nsIPrintSettings::kInitSaveUnwriteableMargins) {
    nsIntMargin margin;
    ReadInchesIntToTwipsPref(GetPrefName(kUnwriteableMarginTop, aPrinterName), margin.top,
                             kUnwriteableMarginTop);
    DUMP_INT(kReadStr, kUnwriteableMarginTop, margin.top);
    ReadInchesIntToTwipsPref(GetPrefName(kUnwriteableMarginLeft, aPrinterName), margin.left,
                             kUnwriteableMarginLeft);
    DUMP_INT(kReadStr, kUnwriteableMarginLeft, margin.left);
    ReadInchesIntToTwipsPref(GetPrefName(kUnwriteableMarginBottom, aPrinterName),
                             margin.bottom, kUnwriteableMarginBottom);
    DUMP_INT(kReadStr, kUnwriteableMarginBottom, margin.bottom);
    ReadInchesIntToTwipsPref(GetPrefName(kUnwriteableMarginRight, aPrinterName), margin.right,
                             kUnwriteableMarginRight);
    DUMP_INT(kReadStr, kUnwriteableMarginRight, margin.right);
    aPS->SetUnwriteableMarginInTwips(margin);
  }

  bool     b;
  nsAutoString str;
  int32_t  iVal;
  double   dbl;

#define GETBOOLPREF(_prefname, _retval)                 \
  NS_SUCCEEDED(                                         \
    Preferences::GetBool(                               \
      GetPrefName(_prefname, aPrinterName), _retval     \
    )                                                   \
  )

#define GETSTRPREF(_prefname, _retval)                  \
  NS_SUCCEEDED(                                         \
    Preferences::GetString(                             \
      GetPrefName(_prefname, aPrinterName), _retval     \
    )                                                   \
  )

#define GETINTPREF(_prefname, _retval)                  \
  NS_SUCCEEDED(                                         \
    Preferences::GetInt(                                \
      GetPrefName(_prefname, aPrinterName), _retval     \
    )                                                   \
  )

#define GETDBLPREF(_prefname, _retval)                  \
  NS_SUCCEEDED(                                         \
    ReadPrefDouble(                                     \
      GetPrefName(_prefname, aPrinterName), _retval     \
    )                                                   \
  )

  // Paper size prefs are read as a group
  if (aFlags & nsIPrintSettings::kInitSavePaperSize) {
    int32_t sizeUnit;
    double width, height;

    bool success = GETINTPREF(kPrintPaperSizeUnit, &sizeUnit)
                  && GETDBLPREF(kPrintPaperWidth, width)
                  && GETDBLPREF(kPrintPaperHeight, height)
                  && GETSTRPREF(kPrintPaperName, &str);

    // Bug 315687: Sanity check paper size to avoid paper size values in
    // mm when the size unit flag is inches. The value 100 is arbitrary
    // and can be changed.
    if (success) {
      success = (sizeUnit != nsIPrintSettings::kPaperSizeInches)
             || (width < 100.0)
             || (height < 100.0);
#if defined(XP_WIN)
      // Work around legacy invalid prefs where the size unit gets set to
      // millimeters, but the height and width remains as the default inches
      // ones for letter. See bug 1276717.
      if (sizeUnit == nsIPrintSettings::kPaperSizeMillimeters &&
          height == 11L && width == 8.5L) {

        // As an extra precaution only override, when the resolution is also
        // set to the legacy invalid, uninitialized value. We'll just broadly
        // assume that anything outside of a million DPI is invalid.
        if (GETINTPREF(kPrintResolution, &iVal) &&
            (iVal < 0 || iVal > 1000000)) {
          height = -1L;
          width = -1L;
        }
      }
#endif
    }

    if (success) {
      aPS->SetPaperSizeUnit(sizeUnit);
      DUMP_INT(kReadStr, kPrintPaperSizeUnit, sizeUnit);
      aPS->SetPaperWidth(width);
      DUMP_DBL(kReadStr, kPrintPaperWidth, width);
      aPS->SetPaperHeight(height);
      DUMP_DBL(kReadStr, kPrintPaperHeight, height);
      aPS->SetPaperName(str.get());
      DUMP_STR(kReadStr, kPrintPaperName, str.get());
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveOddEvenPages) {
  if (GETBOOLPREF(kPrintEvenPages, &b)) {
    aPS->SetPrintOptions(nsIPrintSettings::kPrintEvenPages, b);
    DUMP_BOOL(kReadStr, kPrintEvenPages, b);
  }
  }

  if (aFlags & nsIPrintSettings::kInitSaveOddEvenPages) {
    if (GETBOOLPREF(kPrintOddPages, &b)) {
      aPS->SetPrintOptions(nsIPrintSettings::kPrintOddPages, b);
      DUMP_BOOL(kReadStr, kPrintOddPages, b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveHeaderLeft) {
    if (GETSTRPREF(kPrintHeaderStrLeft, &str)) {
      aPS->SetHeaderStrLeft(str.get());
      DUMP_STR(kReadStr, kPrintHeaderStrLeft, str.get());
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveHeaderCenter) {
    if (GETSTRPREF(kPrintHeaderStrCenter, &str)) {
      aPS->SetHeaderStrCenter(str.get());
      DUMP_STR(kReadStr, kPrintHeaderStrCenter, str.get());
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveHeaderRight) {
    if (GETSTRPREF(kPrintHeaderStrRight, &str)) {
      aPS->SetHeaderStrRight(str.get());
      DUMP_STR(kReadStr, kPrintHeaderStrRight, str.get());
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveFooterLeft) {
    if (GETSTRPREF(kPrintFooterStrLeft, &str)) {
      aPS->SetFooterStrLeft(str.get());
      DUMP_STR(kReadStr, kPrintFooterStrLeft, str.get());
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveFooterCenter) {
    if (GETSTRPREF(kPrintFooterStrCenter, &str)) {
      aPS->SetFooterStrCenter(str.get());
      DUMP_STR(kReadStr, kPrintFooterStrCenter, str.get());
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveFooterRight) {
    if (GETSTRPREF(kPrintFooterStrRight, &str)) {
      aPS->SetFooterStrRight(str.get());
      DUMP_STR(kReadStr, kPrintFooterStrRight, str.get());
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveBGColors) {
    if (GETBOOLPREF(kPrintBGColors, &b)) {
      aPS->SetPrintBGColors(b);
      DUMP_BOOL(kReadStr, kPrintBGColors, b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveBGImages) {
    if (GETBOOLPREF(kPrintBGImages, &b)) {
      aPS->SetPrintBGImages(b);
      DUMP_BOOL(kReadStr, kPrintBGImages, b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveReversed) {
    if (GETBOOLPREF(kPrintReversed, &b)) {
      aPS->SetPrintReversed(b);
      DUMP_BOOL(kReadStr, kPrintReversed, b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveInColor) {
    if (GETBOOLPREF(kPrintInColor, &b)) {
      aPS->SetPrintInColor(b);
      DUMP_BOOL(kReadStr, kPrintInColor, b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSavePaperData) {
    if (GETINTPREF(kPrintPaperData, &iVal)) {
      aPS->SetPaperData(iVal);
      DUMP_INT(kReadStr, kPrintPaperData, iVal);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveOrientation) {
    if (GETINTPREF(kPrintOrientation, &iVal)) {
      aPS->SetOrientation(iVal);
      DUMP_INT(kReadStr, kPrintOrientation, iVal);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSavePrintToFile) {
    if (GETBOOLPREF(kPrintToFile, &b)) {
      aPS->SetPrintToFile(b);
      DUMP_BOOL(kReadStr, kPrintToFile, b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveToFileName) {
    if (GETSTRPREF(kPrintToFileName, &str)) {
      aPS->SetToFileName(str.get());
      DUMP_STR(kReadStr, kPrintToFileName, str.get());
    }
  }

  if (aFlags & nsIPrintSettings::kInitSavePageDelay) {
    if (GETINTPREF(kPrintPageDelay, &iVal)) {
      aPS->SetPrintPageDelay(iVal);
      DUMP_INT(kReadStr, kPrintPageDelay, iVal);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveShrinkToFit) {
    if (GETBOOLPREF(kPrintShrinkToFit, &b)) {
      aPS->SetShrinkToFit(b);
      DUMP_BOOL(kReadStr, kPrintShrinkToFit, b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveScaling) {
    if (GETDBLPREF(kPrintScaling, dbl)) {
      aPS->SetScaling(dbl);
      DUMP_DBL(kReadStr, kPrintScaling, dbl);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveResolution) {
    if (GETINTPREF(kPrintResolution, &iVal)) {
      aPS->SetResolution(iVal);
      DUMP_INT(kReadStr, kPrintResolution, iVal);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveDuplex) {
    if (GETINTPREF(kPrintDuplex, &iVal)) {
      aPS->SetDuplex(iVal);
      DUMP_INT(kReadStr, kPrintDuplex, iVal);
    }
  }

  // Not Reading In:
  //   Number of Copies

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *  @update 1/12/01 rods
 */
nsresult
nsPrintOptions::WritePrefs(nsIPrintSettings *aPS, const nsAString& aPrinterName,
                           uint32_t aFlags)
{
  NS_ENSURE_ARG_POINTER(aPS);

  nsIntMargin margin;
  if (aFlags & nsIPrintSettings::kInitSaveMargins) {
    if (NS_SUCCEEDED(aPS->GetMarginInTwips(margin))) {
      WriteInchesFromTwipsPref(GetPrefName(kMarginTop, aPrinterName),
                               margin.top);
      DUMP_INT(kWriteStr, kMarginTop, margin.top);
      WriteInchesFromTwipsPref(GetPrefName(kMarginLeft, aPrinterName),
                               margin.left);
      DUMP_INT(kWriteStr, kMarginLeft, margin.top);
      WriteInchesFromTwipsPref(GetPrefName(kMarginBottom, aPrinterName),
                               margin.bottom);
      DUMP_INT(kWriteStr, kMarginBottom, margin.top);
      WriteInchesFromTwipsPref(GetPrefName(kMarginRight, aPrinterName),
                               margin.right);
      DUMP_INT(kWriteStr, kMarginRight, margin.top);
    }
  }

  nsIntMargin edge;
  if (aFlags & nsIPrintSettings::kInitSaveEdges) {
    if (NS_SUCCEEDED(aPS->GetEdgeInTwips(edge))) {
      WriteInchesIntFromTwipsPref(GetPrefName(kEdgeTop, aPrinterName),
                                  edge.top);
      DUMP_INT(kWriteStr, kEdgeTop, edge.top);
      WriteInchesIntFromTwipsPref(GetPrefName(kEdgeLeft, aPrinterName),
                                  edge.left);
      DUMP_INT(kWriteStr, kEdgeLeft, edge.top);
      WriteInchesIntFromTwipsPref(GetPrefName(kEdgeBottom, aPrinterName),
                                  edge.bottom);
      DUMP_INT(kWriteStr, kEdgeBottom, edge.top);
      WriteInchesIntFromTwipsPref(GetPrefName(kEdgeRight, aPrinterName),
                                  edge.right);
      DUMP_INT(kWriteStr, kEdgeRight, edge.top);
    }
  }

  nsIntMargin unwriteableMargin;
  if (aFlags & nsIPrintSettings::kInitSaveUnwriteableMargins) {
    if (NS_SUCCEEDED(aPS->GetUnwriteableMarginInTwips(unwriteableMargin))) {
      WriteInchesIntFromTwipsPref(GetPrefName(kUnwriteableMarginTop, aPrinterName),
                                  unwriteableMargin.top);
      DUMP_INT(kWriteStr, kUnwriteableMarginTop, unwriteableMargin.top);
      WriteInchesIntFromTwipsPref(GetPrefName(kUnwriteableMarginLeft, aPrinterName),
                                  unwriteableMargin.left);
      DUMP_INT(kWriteStr, kUnwriteableMarginLeft, unwriteableMargin.top);
      WriteInchesIntFromTwipsPref(GetPrefName(kUnwriteableMarginBottom, aPrinterName),
                                  unwriteableMargin.bottom);
      DUMP_INT(kWriteStr, kUnwriteableMarginBottom, unwriteableMargin.top);
      WriteInchesIntFromTwipsPref(GetPrefName(kUnwriteableMarginRight, aPrinterName),
                                  unwriteableMargin.right);
      DUMP_INT(kWriteStr, kUnwriteableMarginRight, unwriteableMargin.top);
    }
  }

  // Paper size prefs are saved as a group
  if (aFlags & nsIPrintSettings::kInitSavePaperSize) {
    int16_t sizeUnit;
    double width, height;
    char16_t *name;

    if (
      NS_SUCCEEDED(aPS->GetPaperSizeUnit(&sizeUnit)) &&
      NS_SUCCEEDED(aPS->GetPaperWidth(&width)) &&
      NS_SUCCEEDED(aPS->GetPaperHeight(&height)) &&
      NS_SUCCEEDED(aPS->GetPaperName(&name))
    ) {
      DUMP_INT(kWriteStr, kPrintPaperSizeUnit, sizeUnit);
      Preferences::SetInt(GetPrefName(kPrintPaperSizeUnit, aPrinterName),
                          int32_t(sizeUnit));
      DUMP_DBL(kWriteStr, kPrintPaperWidth, width);
      WritePrefDouble(GetPrefName(kPrintPaperWidth, aPrinterName), width);
      DUMP_DBL(kWriteStr, kPrintPaperHeight, height);
      WritePrefDouble(GetPrefName(kPrintPaperHeight, aPrinterName), height);
      DUMP_STR(kWriteStr, kPrintPaperName, name);
      Preferences::SetString(GetPrefName(kPrintPaperName, aPrinterName), name);
    }
  }

  bool       b;
  char16_t* uStr;
  int32_t    iVal;
  int16_t    iVal16;
  double     dbl;

  if (aFlags & nsIPrintSettings::kInitSaveOddEvenPages) {
    if (NS_SUCCEEDED(aPS->GetPrintOptions(nsIPrintSettings::kPrintEvenPages,
                                          &b))) {
          DUMP_BOOL(kWriteStr, kPrintEvenPages, b);
          Preferences::SetBool(GetPrefName(kPrintEvenPages, aPrinterName), b);
        }
  }

  if (aFlags & nsIPrintSettings::kInitSaveOddEvenPages) {
    if (NS_SUCCEEDED(aPS->GetPrintOptions(nsIPrintSettings::kPrintOddPages,
                                          &b))) {
          DUMP_BOOL(kWriteStr, kPrintOddPages, b);
          Preferences::SetBool(GetPrefName(kPrintOddPages, aPrinterName), b);
        }
  }

  if (aFlags & nsIPrintSettings::kInitSaveHeaderLeft) {
    if (NS_SUCCEEDED(aPS->GetHeaderStrLeft(&uStr))) {
      DUMP_STR(kWriteStr, kPrintHeaderStrLeft, uStr);
      Preferences::SetString(GetPrefName(kPrintHeaderStrLeft, aPrinterName),
                             uStr);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveHeaderCenter) {
    if (NS_SUCCEEDED(aPS->GetHeaderStrCenter(&uStr))) {
      DUMP_STR(kWriteStr, kPrintHeaderStrCenter, uStr);
      Preferences::SetString(GetPrefName(kPrintHeaderStrCenter, aPrinterName),
                             uStr);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveHeaderRight) {
    if (NS_SUCCEEDED(aPS->GetHeaderStrRight(&uStr))) {
      DUMP_STR(kWriteStr, kPrintHeaderStrRight, uStr);
      Preferences::SetString(GetPrefName(kPrintHeaderStrRight, aPrinterName),
                             uStr);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveFooterLeft) {
    if (NS_SUCCEEDED(aPS->GetFooterStrLeft(&uStr))) {
      DUMP_STR(kWriteStr, kPrintFooterStrLeft, uStr);
      Preferences::SetString(GetPrefName(kPrintFooterStrLeft, aPrinterName),
                             uStr);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveFooterCenter) {
    if (NS_SUCCEEDED(aPS->GetFooterStrCenter(&uStr))) {
      DUMP_STR(kWriteStr, kPrintFooterStrCenter, uStr);
      Preferences::SetString(GetPrefName(kPrintFooterStrCenter, aPrinterName),
                             uStr);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveFooterRight) {
    if (NS_SUCCEEDED(aPS->GetFooterStrRight(&uStr))) {
      DUMP_STR(kWriteStr, kPrintFooterStrRight, uStr);
      Preferences::SetString(GetPrefName(kPrintFooterStrRight, aPrinterName),
                             uStr);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveBGColors) {
    if (NS_SUCCEEDED(aPS->GetPrintBGColors(&b))) {
      DUMP_BOOL(kWriteStr, kPrintBGColors, b);
      Preferences::SetBool(GetPrefName(kPrintBGColors, aPrinterName), b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveBGImages) {
    if (NS_SUCCEEDED(aPS->GetPrintBGImages(&b))) {
      DUMP_BOOL(kWriteStr, kPrintBGImages, b);
      Preferences::SetBool(GetPrefName(kPrintBGImages, aPrinterName), b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveReversed) {
    if (NS_SUCCEEDED(aPS->GetPrintReversed(&b))) {
      DUMP_BOOL(kWriteStr, kPrintReversed, b);
      Preferences::SetBool(GetPrefName(kPrintReversed, aPrinterName), b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveInColor) {
    if (NS_SUCCEEDED(aPS->GetPrintInColor(&b))) {
      DUMP_BOOL(kWriteStr, kPrintInColor, b);
      Preferences::SetBool(GetPrefName(kPrintInColor, aPrinterName), b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSavePaperData) {
    if (NS_SUCCEEDED(aPS->GetPaperData(&iVal16))) {
      DUMP_INT(kWriteStr, kPrintPaperData, iVal16);
      Preferences::SetInt(GetPrefName(kPrintPaperData, aPrinterName),
                          int32_t(iVal16));
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveOrientation) {
    if (NS_SUCCEEDED(aPS->GetOrientation(&iVal))) {
      DUMP_INT(kWriteStr, kPrintOrientation, iVal);
      Preferences::SetInt(GetPrefName(kPrintOrientation, aPrinterName), iVal);
    }
  }

  // Only the general version of this pref is saved
  if ((aFlags & nsIPrintSettings::kInitSavePrinterName)
      && aPrinterName.IsEmpty()) {
    if (NS_SUCCEEDED(aPS->GetPrinterName(&uStr))) {
      DUMP_STR(kWriteStr, kPrinterName, uStr);
      Preferences::SetString(kPrinterName, uStr);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSavePrintToFile) {
    if (NS_SUCCEEDED(aPS->GetPrintToFile(&b))) {
      DUMP_BOOL(kWriteStr, kPrintToFile, b);
      Preferences::SetBool(GetPrefName(kPrintToFile, aPrinterName), b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveToFileName) {
    if (NS_SUCCEEDED(aPS->GetToFileName(&uStr))) {
      DUMP_STR(kWriteStr, kPrintToFileName, uStr);
      Preferences::SetString(GetPrefName(kPrintToFileName, aPrinterName), uStr);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSavePageDelay) {
    if (NS_SUCCEEDED(aPS->GetPrintPageDelay(&iVal))) {
      DUMP_INT(kWriteStr, kPrintPageDelay, iVal);
      Preferences::SetInt(GetPrefName(kPrintPageDelay, aPrinterName), iVal);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveShrinkToFit) {
    if (NS_SUCCEEDED(aPS->GetShrinkToFit(&b))) {
      DUMP_BOOL(kWriteStr, kPrintShrinkToFit, b);
      Preferences::SetBool(GetPrefName(kPrintShrinkToFit, aPrinterName), b);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveScaling) {
    if (NS_SUCCEEDED(aPS->GetScaling(&dbl))) {
      DUMP_DBL(kWriteStr, kPrintScaling, dbl);
      WritePrefDouble(GetPrefName(kPrintScaling, aPrinterName), dbl);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveResolution) {
    if (NS_SUCCEEDED(aPS->GetResolution(&iVal))) {
      DUMP_INT(kWriteStr, kPrintResolution, iVal);
      Preferences::SetInt(GetPrefName(kPrintResolution, aPrinterName), iVal);
    }
  }

  if (aFlags & nsIPrintSettings::kInitSaveDuplex) {
    if (NS_SUCCEEDED(aPS->GetDuplex(&iVal))) {
      DUMP_INT(kWriteStr, kPrintDuplex, iVal);
      Preferences::SetInt(GetPrefName(kPrintDuplex, aPrinterName), iVal);
    }
  }

  // Not Writing Out:
  //   Number of Copies

  return NS_OK;
}

nsresult nsPrintOptions::_CreatePrintSettings(nsIPrintSettings **_retval)
{
  // does not initially ref count
  nsPrintSettings * printSettings = new nsPrintSettings();
  NS_ENSURE_TRUE(printSettings, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*_retval = printSettings); // ref count

  nsXPIDLString printerName;
  nsresult rv = GetDefaultPrinterName(getter_Copies(printerName));
  NS_ENSURE_SUCCESS(rv, rv);
  (*_retval)->SetPrinterName(printerName.get());

  (void)InitPrintSettingsFromPrefs(*_retval, false,
                                   nsIPrintSettings::kInitSaveAll);

  return NS_OK;
}

NS_IMETHODIMP
nsPrintOptions::GetGlobalPrintSettings(nsIPrintSettings **aGlobalPrintSettings)
{
  nsresult rv;

  rv = _CreatePrintSettings(getter_AddRefs(mGlobalPrintSettings));
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*aGlobalPrintSettings = mGlobalPrintSettings.get());

  return rv;
}

NS_IMETHODIMP
nsPrintOptions::GetNewPrintSettings(nsIPrintSettings * *aNewPrintSettings)
{
  return _CreatePrintSettings(aNewPrintSettings);
}

NS_IMETHODIMP
nsPrintOptions::GetDefaultPrinterName(char16_t * *aDefaultPrinterName)
{
  nsresult rv;
  nsCOMPtr<nsIPrinterEnumerator> prtEnum =
           do_GetService(NS_PRINTER_ENUMERATOR_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Look up the printer from the last print job
  nsAutoString lastPrinterName;
  Preferences::GetString(kPrinterName, &lastPrinterName);
  if (!lastPrinterName.IsEmpty()) {
    // Verify it's still a valid printer
    nsCOMPtr<nsIStringEnumerator> printers;
    rv = prtEnum->GetPrinterNameList(getter_AddRefs(printers));
    if (NS_SUCCEEDED(rv)) {
      bool isValid = false;
      bool hasMore;
      while (NS_SUCCEEDED(printers->HasMore(&hasMore)) && hasMore) {
        nsAutoString printer;
        if (NS_SUCCEEDED(printers->GetNext(printer)) && lastPrinterName.Equals(printer)) {
          isValid = true;
          break;
        }
      }
      if (isValid) {
        *aDefaultPrinterName = ToNewUnicode(lastPrinterName);
        return NS_OK;
      }
    }
  }

  // There is no last printer preference, or it doesn't name a valid printer.
  // Return the default from the printer enumeration.
  return prtEnum->GetDefaultPrinterName(aDefaultPrinterName);
}

NS_IMETHODIMP
nsPrintOptions::InitPrintSettingsFromPrinter(const char16_t *aPrinterName,
                                             nsIPrintSettings *aPrintSettings)
{
  // Don't get print settings from the printer in the child when printing via
  // parent, these will be retrieved in the parent later in the print process.
  if (XRE_IsContentProcess() && Preferences::GetBool("print.print_via_parent")) {
    return NS_OK;
  }

  NS_ENSURE_ARG_POINTER(aPrintSettings);
  NS_ENSURE_ARG_POINTER(aPrinterName);

#ifdef DEBUG
  nsXPIDLString printerName;
  aPrintSettings->GetPrinterName(getter_Copies(printerName));
  if (!printerName.Equals(aPrinterName)) {
    NS_WARNING("Printer names should match!");
  }
#endif

  bool isInitialized;
  aPrintSettings->GetIsInitializedFromPrinter(&isInitialized);
  if (isInitialized)
    return NS_OK;

  nsresult rv;
  nsCOMPtr<nsIPrinterEnumerator> prtEnum =
           do_GetService(NS_PRINTER_ENUMERATOR_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = prtEnum->InitPrintSettingsFromPrinter(aPrinterName, aPrintSettings);
  NS_ENSURE_SUCCESS(rv, rv);

  aPrintSettings->SetIsInitializedFromPrinter(true);
  return rv;
}

#ifndef MOZ_X11
/** ---------------------------------------------------
 *  Helper function - Returns either the name or sets the length to zero
 */
static nsresult
GetAdjustedPrinterName(nsIPrintSettings* aPS, bool aUsePNP,
                       nsAString& aPrinterName)
{
  NS_ENSURE_ARG_POINTER(aPS);

  aPrinterName.Truncate();
  if (!aUsePNP)
    return NS_OK;

  // Get the Printer Name from the PrintSettings
  // to use as a prefix for Pref Names
  char16_t* prtName = nullptr;

  nsresult rv = aPS->GetPrinterName(&prtName);
  NS_ENSURE_SUCCESS(rv, rv);

  aPrinterName = nsDependentString(prtName);

  // Convert any whitespaces, carriage returns or newlines to _
  // The below algorithm is supposedly faster than using iterators
  NS_NAMED_LITERAL_STRING(replSubstr, "_");
  const char* replaceStr = " \n\r";

  int32_t x;
  for (x=0; x < (int32_t)strlen(replaceStr); x++) {
    char16_t uChar = replaceStr[x];

    int32_t i = 0;
    while ((i = aPrinterName.FindChar(uChar, i)) != kNotFound) {
      aPrinterName.Replace(i, 1, replSubstr);
      i++;
    }
  }
  return NS_OK;
}
#endif

NS_IMETHODIMP
nsPrintOptions::InitPrintSettingsFromPrefs(nsIPrintSettings* aPS,
                                           bool aUsePNP, uint32_t aFlags)
{
  NS_ENSURE_ARG_POINTER(aPS);

  bool isInitialized;
  aPS->GetIsInitializedFromPrefs(&isInitialized);

  if (isInitialized)
    return NS_OK;

  nsAutoString prtName;
  // read any non printer specific prefs
  // with empty printer name
  nsresult rv = ReadPrefs(aPS, prtName, aFlags);
  NS_ENSURE_SUCCESS(rv, rv);

  // Do not use printer name in Linux because GTK backend does not support
  // per printer settings.
#ifndef MOZ_X11
  // Get the Printer Name from the PrintSettings
  // to use as a prefix for Pref Names
  rv = GetAdjustedPrinterName(aPS, aUsePNP, prtName);
  NS_ENSURE_SUCCESS(rv, rv);

  if (prtName.IsEmpty()) {
    NS_WARNING("Caller should supply a printer name.");
    return NS_OK;
  }

  // Now read any printer specific prefs
  rv = ReadPrefs(aPS, prtName, aFlags);
  if (NS_SUCCEEDED(rv))
    aPS->SetIsInitializedFromPrefs(true);
#endif

  return NS_OK;
}

/**
 *  Save all of the printer settings; if we can find a printer name, save
 *  printer-specific preferences. Otherwise, save generic ones.
 */
nsresult
nsPrintOptions::SavePrintSettingsToPrefs(nsIPrintSettings *aPS,
                                         bool aUsePrinterNamePrefix,
                                         uint32_t aFlags)
{
  NS_ENSURE_ARG_POINTER(aPS);

  if (GeckoProcessType_Content == XRE_GetProcessType()) {
    // If we're in the content process, we can't directly write to the
    // Preferences service - we have to proxy the save up to the
    // parent process.
    RefPtr<nsPrintingProxy> proxy = nsPrintingProxy::GetInstance();
    return proxy->SavePrintSettings(aPS, aUsePrinterNamePrefix, aFlags);
  }

  nsAutoString prtName;

  // Do not use printer name in Linux because GTK backend does not support
  // per printer settings.
#ifndef MOZ_X11
  // Get the printer name from the PrinterSettings for an optional prefix.
  nsresult rv = GetAdjustedPrinterName(aPS, aUsePrinterNamePrefix, prtName);
  NS_ENSURE_SUCCESS(rv, rv);
#endif

  // Write the prefs, with or without a printer name prefix.
  return WritePrefs(aPS, prtName, aFlags);
}


//-----------------------------------------------------
//-- Protected Methods --------------------------------
//-----------------------------------------------------
nsresult
nsPrintOptions::ReadPrefDouble(const char * aPrefId, double& aVal)
{
  NS_ENSURE_ARG_POINTER(aPrefId);

  nsAutoCString str;
  nsresult rv = Preferences::GetCString(aPrefId, &str);
  if (NS_SUCCEEDED(rv) && !str.IsEmpty()) {
    aVal = atof(str.get());
  }
  return rv;
}

nsresult
nsPrintOptions::WritePrefDouble(const char * aPrefId, double aVal)
{
  NS_ENSURE_ARG_POINTER(aPrefId);

  nsPrintfCString str("%6.2f", aVal);
  NS_ENSURE_TRUE(!str.IsEmpty(), NS_ERROR_FAILURE);

  return Preferences::SetCString(aPrefId, str);
}

void
nsPrintOptions::ReadInchesToTwipsPref(const char * aPrefId, int32_t& aTwips,
                                      const char * aMarginPref)
{
  nsAutoString str;
  nsresult rv = Preferences::GetString(aPrefId, &str);
  if (NS_FAILED(rv) || str.IsEmpty()) {
    rv = Preferences::GetString(aMarginPref, &str);
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

void
nsPrintOptions::WriteInchesFromTwipsPref(const char * aPrefId, int32_t aTwips)
{
  double inches = NS_TWIPS_TO_INCHES(aTwips);
  nsAutoCString inchesStr;
  inchesStr.AppendFloat(inches);

  Preferences::SetCString(aPrefId, inchesStr);
}

void
nsPrintOptions::ReadInchesIntToTwipsPref(const char * aPrefId, int32_t& aTwips,
                                         const char * aMarginPref)
{
  int32_t value;
  nsresult rv = Preferences::GetInt(aPrefId, &value);
  if (NS_FAILED(rv)) {
    rv = Preferences::GetInt(aMarginPref, &value);
  }
  if (NS_SUCCEEDED(rv)) {
    aTwips = NS_INCHES_TO_INT_TWIPS(float(value)/100.0f);
  } else {
    aTwips = 0;
  }
}

void
nsPrintOptions::WriteInchesIntFromTwipsPref(const char * aPrefId, int32_t aTwips)
{
  Preferences::SetInt(aPrefId,
                      int32_t(NS_TWIPS_TO_INCHES(aTwips) * 100.0f + 0.5f));
}

void
nsPrintOptions::ReadJustification(const char * aPrefId, int16_t& aJust,
                                  int16_t aInitValue)
{
  aJust = aInitValue;
  nsAutoString justStr;
  if (NS_SUCCEEDED(Preferences::GetString(aPrefId, &justStr))) {
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
void
nsPrintOptions::WriteJustification(const char * aPrefId, int16_t aJust)
{
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
  } //switch
}

//----------------------------------------------------------------------
// Testing of read/write prefs
// This define turns on the testing module below
// so at start up it writes and reads the prefs.
#ifdef DEBUG_rods_X
class Tester {
  public:
    Tester();
};
Tester::Tester()
{
  nsCOMPtr<nsIPrintSettings> ps;
  nsresult rv;
  nsCOMPtr<nsIPrintOptions> printService =
      do_GetService("@mozilla.org/gfx/printsettings-service;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = printService->CreatePrintSettings(getter_AddRefs(ps));
  }

  if (ps) {
    ps->SetPrintOptions(nsIPrintSettings::kPrintOddPages,  true);
    ps->SetPrintOptions(nsIPrintSettings::kPrintEvenPages,  false);
    ps->SetMarginTop(1.0);
    ps->SetMarginLeft(1.0);
    ps->SetMarginBottom(1.0);
    ps->SetMarginRight(1.0);
    ps->SetScaling(0.5);
    ps->SetPrintBGColors(true);
    ps->SetPrintBGImages(true);
    ps->SetPrintRange(15);
    ps->SetHeaderStrLeft(NS_ConvertUTF8toUTF16("Left").get());
    ps->SetHeaderStrCenter(NS_ConvertUTF8toUTF16("Center").get());
    ps->SetHeaderStrRight(NS_ConvertUTF8toUTF16("Right").get());
    ps->SetFooterStrLeft(NS_ConvertUTF8toUTF16("Left").get());
    ps->SetFooterStrCenter(NS_ConvertUTF8toUTF16("Center").get());
    ps->SetFooterStrRight(NS_ConvertUTF8toUTF16("Right").get());
    ps->SetPaperName(NS_ConvertUTF8toUTF16("Paper Name").get());
    ps->SetPaperData(1);
    ps->SetPaperWidth(100.0);
    ps->SetPaperHeight(50.0);
    ps->SetPaperSizeUnit(nsIPrintSettings::kPaperSizeMillimeters);
    ps->SetPrintReversed(true);
    ps->SetPrintInColor(true);
    ps->SetOrientation(nsIPrintSettings::kLandscapeOrientation);
    ps->SetNumCopies(2);
    ps->SetPrinterName(NS_ConvertUTF8toUTF16("Printer Name").get());
    ps->SetPrintToFile(true);
    ps->SetToFileName(NS_ConvertUTF8toUTF16("File Name").get());
    ps->SetPrintPageDelay(1000);
    ps->SetShrinkToFit(true);

    struct SettingsType {
      const char* mName;
      uint32_t    mFlag;
    };
    SettingsType gSettings[] = {
      {"OddEven", nsIPrintSettings::kInitSaveOddEvenPages},
      {kPrintHeaderStrLeft, nsIPrintSettings::kInitSaveHeaderLeft},
      {kPrintHeaderStrCenter, nsIPrintSettings::kInitSaveHeaderCenter},
      {kPrintHeaderStrRight, nsIPrintSettings::kInitSaveHeaderRight},
      {kPrintFooterStrLeft, nsIPrintSettings::kInitSaveFooterLeft},
      {kPrintFooterStrCenter, nsIPrintSettings::kInitSaveFooterCenter},
      {kPrintFooterStrRight, nsIPrintSettings::kInitSaveFooterRight},
      {kPrintBGColors, nsIPrintSettings::kInitSaveBGColors},
      {kPrintBGImages, nsIPrintSettings::kInitSaveBGImages},
      {kPrintShrinkToFit, nsIPrintSettings::kInitSaveShrinkToFit},
      {kPrintPaperSize, nsIPrintSettings::kInitSavePaperSize},
      {kPrintPaperData, nsIPrintSettings::kInitSavePaperData},
      {kPrintReversed, nsIPrintSettings::kInitSaveReversed},
      {kPrintInColor, nsIPrintSettings::kInitSaveInColor},
      {kPrintOrientation, nsIPrintSettings::kInitSaveOrientation},
      {kPrinterName, nsIPrintSettings::kInitSavePrinterName},
      {kPrintToFile, nsIPrintSettings::kInitSavePrintToFile},
      {kPrintToFileName, nsIPrintSettings::kInitSaveToFileName},
      {kPrintPageDelay, nsIPrintSettings::kInitSavePageDelay},
      {"Margins", nsIPrintSettings::kInitSaveMargins},
      {"All", nsIPrintSettings::kInitSaveAll},
      {nullptr, 0}};

      nsString prefix; prefix.AssignLiteral("Printer Name");
      int32_t i = 0;
      while (gSettings[i].mName != nullptr) {
        printf("------------------------------------------------\n");
        printf("%d) %s -> 0x%X\n", i, gSettings[i].mName, gSettings[i].mFlag);
        printService->SavePrintSettingsToPrefs(ps, true, gSettings[i].mFlag);
        printService->InitPrintSettingsFromPrefs(ps, true,
                                                 gSettings[i].mFlag);
        i++;
      }
  }

}
Tester gTester;
#endif
