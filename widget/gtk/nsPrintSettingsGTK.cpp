/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintSettingsGTK.h"
#include "nsIFile.h"
#include "nsNetUtil.h"
#include <stdlib.h>
#include <algorithm>

static
gboolean ref_printer(GtkPrinter *aPrinter, gpointer aData)
{
  ((nsPrintSettingsGTK*) aData)->SetGtkPrinter(aPrinter);
  return TRUE;
}

static
gboolean printer_enumerator(GtkPrinter *aPrinter, gpointer aData)
{
  if (gtk_printer_is_default(aPrinter))
    return ref_printer(aPrinter, aData);

  return FALSE; // Keep 'em coming...
}

static
GtkPaperSize* moz_gtk_paper_size_copy_to_new_custom(GtkPaperSize* oldPaperSize)
{
  // We make a "custom-ified" copy of the paper size so it can be changed later.
  return gtk_paper_size_new_custom(gtk_paper_size_get_name(oldPaperSize),
                                   gtk_paper_size_get_display_name(oldPaperSize),
                                   gtk_paper_size_get_width(oldPaperSize, GTK_UNIT_INCH),
                                   gtk_paper_size_get_height(oldPaperSize, GTK_UNIT_INCH),
                                   GTK_UNIT_INCH);
}

NS_IMPL_ISUPPORTS_INHERITED(nsPrintSettingsGTK, 
                            nsPrintSettings, 
                            nsPrintSettingsGTK)

/** ---------------------------------------------------
 */
nsPrintSettingsGTK::nsPrintSettingsGTK() :
  mPageSetup(nullptr),
  mPrintSettings(nullptr),
  mGTKPrinter(nullptr),
  mPrintSelectionOnly(false)
{
  // The aim here is to set up the objects enough that silent printing works well.
  // These will be replaced anyway if the print dialog is used.
  mPrintSettings = gtk_print_settings_new();
  mPageSetup = gtk_page_setup_new();
  InitUnwriteableMargin();

  SetOutputFormat(nsIPrintSettings::kOutputFormatNative);

  GtkPaperSize* defaultPaperSize = gtk_paper_size_new(nullptr);
  mPaperSize = moz_gtk_paper_size_copy_to_new_custom(defaultPaperSize);
  gtk_paper_size_free(defaultPaperSize);
  SaveNewPageSize();
}

/** ---------------------------------------------------
 */
nsPrintSettingsGTK::~nsPrintSettingsGTK()
{
  if (mPageSetup) {
    g_object_unref(mPageSetup);
    mPageSetup = nullptr;
  }
  if (mPrintSettings) {
    g_object_unref(mPrintSettings);
    mPrintSettings = nullptr;
  }
  if (mGTKPrinter) {
    g_object_unref(mGTKPrinter);
    mGTKPrinter = nullptr;
  }
  gtk_paper_size_free(mPaperSize);
}

/** ---------------------------------------------------
 */
nsPrintSettingsGTK::nsPrintSettingsGTK(const nsPrintSettingsGTK& aPS) :
  mPageSetup(nullptr),
  mPrintSettings(nullptr),
  mGTKPrinter(nullptr),
  mPrintSelectionOnly(false)
{
  *this = aPS;
}

/** ---------------------------------------------------
 */
nsPrintSettingsGTK& nsPrintSettingsGTK::operator=(const nsPrintSettingsGTK& rhs)
{
  if (this == &rhs) {
    return *this;
  }
  
  nsPrintSettings::operator=(rhs);

  if (mPageSetup)
    g_object_unref(mPageSetup);
  mPageSetup = gtk_page_setup_copy(rhs.mPageSetup);
  // NOTE: No need to re-initialize mUnwriteableMargin here (even
  // though mPageSetup is changing). It'll be copied correctly by
  // nsPrintSettings::operator=.

  if (mPrintSettings)
    g_object_unref(mPrintSettings);
  mPrintSettings = gtk_print_settings_copy(rhs.mPrintSettings);

  if (mGTKPrinter)
    g_object_unref(mGTKPrinter);
  mGTKPrinter = (GtkPrinter*) g_object_ref(rhs.mGTKPrinter);

  mPrintSelectionOnly = rhs.mPrintSelectionOnly;

  return *this;
}

/** -------------------------------------------
 */
nsresult nsPrintSettingsGTK::_Clone(nsIPrintSettings **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nullptr;
  
  nsPrintSettingsGTK *newSettings = new nsPrintSettingsGTK(*this);
  if (!newSettings)
    return NS_ERROR_FAILURE;
  *_retval = newSettings;
  NS_ADDREF(*_retval);
  return NS_OK;
}


/** -------------------------------------------
 */
NS_IMETHODIMP
nsPrintSettingsGTK::_Assign(nsIPrintSettings *aPS)
{
  nsPrintSettingsGTK *printSettingsGTK = static_cast<nsPrintSettingsGTK*>(aPS);
  if (!printSettingsGTK)
    return NS_ERROR_UNEXPECTED;
  *this = *printSettingsGTK;
  return NS_OK;
}

/** ---------------------------------------------------
 */
void
nsPrintSettingsGTK::SetGtkPageSetup(GtkPageSetup *aPageSetup)
{
  if (mPageSetup)
    g_object_unref(mPageSetup);
  
  mPageSetup = (GtkPageSetup*) g_object_ref(aPageSetup);
  InitUnwriteableMargin();

  // We make a custom copy of the GtkPaperSize so it can be mutable. If a
  // GtkPaperSize wasn't made as custom, its properties are immutable.
  GtkPaperSize* newPaperSize = gtk_page_setup_get_paper_size(aPageSetup);
  if (newPaperSize) { // Yes, this can be null
    gtk_paper_size_free(mPaperSize);
    mPaperSize = moz_gtk_paper_size_copy_to_new_custom(newPaperSize);
  }
  // If newPaperSize was not null, we must update our twin too (GtkPrintSettings).
  // If newPaperSize was null, we must set this object to use mPaperSize.
  SaveNewPageSize();
}

/** ---------------------------------------------------
 */
void
nsPrintSettingsGTK::SetGtkPrintSettings(GtkPrintSettings *aPrintSettings)
{
  if (mPrintSettings)
    g_object_unref(mPrintSettings);
  
  mPrintSettings = (GtkPrintSettings*) g_object_ref(aPrintSettings);

  GtkPaperSize* newPaperSize = gtk_print_settings_get_paper_size(aPrintSettings);
  if (newPaperSize) {
    gtk_paper_size_free(mPaperSize);
    mPaperSize = moz_gtk_paper_size_copy_to_new_custom(newPaperSize);
  }
  SaveNewPageSize();
}

/** ---------------------------------------------------
 */
void
nsPrintSettingsGTK::SetGtkPrinter(GtkPrinter *aPrinter)
{
  if (mGTKPrinter)
    g_object_unref(mGTKPrinter);

  mGTKPrinter = (GtkPrinter*) g_object_ref(aPrinter);
}

NS_IMETHODIMP nsPrintSettingsGTK::GetOutputFormat(int16_t *aOutputFormat)
{
  NS_ENSURE_ARG_POINTER(aOutputFormat);

  int16_t format;
  nsresult rv = nsPrintSettings::GetOutputFormat(&format);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (format == nsIPrintSettings::kOutputFormatNative) {
    const gchar* fmtGTK =
      gtk_print_settings_get(mPrintSettings,
                             GTK_PRINT_SETTINGS_OUTPUT_FILE_FORMAT);
    if (fmtGTK) {
      if (nsDependentCString(fmtGTK).EqualsIgnoreCase("pdf")) {
        format = nsIPrintSettings::kOutputFormatPDF;
      } else {
        format = nsIPrintSettings::kOutputFormatPS;
      }
    } else if (GTK_IS_PRINTER(mGTKPrinter)) {
      // Prior to gtk 2.24, gtk_printer_accepts_pdf() and
      // gtk_printer_accepts_ps() always returned true regardless of the
      // printer's capability.
      bool shouldTrustGTK =
        (gtk_major_version > 2 ||
         (gtk_major_version == 2 && gtk_minor_version >= 24));
      bool acceptsPDF = shouldTrustGTK && gtk_printer_accepts_pdf(mGTKPrinter);

      format = acceptsPDF ? nsIPrintSettings::kOutputFormatPDF
                          : nsIPrintSettings::kOutputFormatPS;
    }
  }

  *aOutputFormat = format;
  return NS_OK;
}

/**
 * Reimplementation of nsPrintSettings functions so that we get the values
 * from the GTK objects rather than our own variables.
 */

NS_IMETHODIMP nsPrintSettingsGTK::GetPrintRange(int16_t *aPrintRange)
{
  NS_ENSURE_ARG_POINTER(aPrintRange);
  if (mPrintSelectionOnly) {
    *aPrintRange = kRangeSelection;
    return NS_OK;
  }

  GtkPrintPages gtkRange = gtk_print_settings_get_print_pages(mPrintSettings);
  if (gtkRange == GTK_PRINT_PAGES_RANGES)
    *aPrintRange = kRangeSpecifiedPageRange;
  else
    *aPrintRange = kRangeAllPages;

  return NS_OK;
}
NS_IMETHODIMP nsPrintSettingsGTK::SetPrintRange(int16_t aPrintRange)
{
  if (aPrintRange == kRangeSelection) {
    mPrintSelectionOnly = true;
    return NS_OK;
  }

  mPrintSelectionOnly = false;
  if (aPrintRange == kRangeSpecifiedPageRange)
    gtk_print_settings_set_print_pages(mPrintSettings, GTK_PRINT_PAGES_RANGES);
  else
    gtk_print_settings_set_print_pages(mPrintSettings, GTK_PRINT_PAGES_ALL);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::GetStartPageRange(int32_t *aStartPageRange)
{
  gint ctRanges;
  GtkPageRange* lstRanges = gtk_print_settings_get_page_ranges(mPrintSettings, &ctRanges);

  // Make sure we got a range.
  if (ctRanges < 1) {
    *aStartPageRange = 1;
  } else {
    // GTK supports multiple page ranges; gecko only supports 1. So find
    // the lowest start page.
    int32_t start(lstRanges[0].start);
    for (gint ii = 1; ii < ctRanges; ii++) {
      start = std::min(lstRanges[ii].start, start);
    }
    *aStartPageRange = start + 1;
  }

  g_free(lstRanges);
  return NS_OK;
}
NS_IMETHODIMP
nsPrintSettingsGTK::SetStartPageRange(int32_t aStartPageRange)
{
  int32_t endRange;
  GetEndPageRange(&endRange);

  GtkPageRange gtkRange;
  gtkRange.start = aStartPageRange - 1;
  gtkRange.end = endRange - 1;

  gtk_print_settings_set_page_ranges(mPrintSettings, &gtkRange, 1);

  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::GetEndPageRange(int32_t *aEndPageRange)
{
  gint ctRanges;
  GtkPageRange* lstRanges = gtk_print_settings_get_page_ranges(mPrintSettings, &ctRanges);

  if (ctRanges < 1) {
    *aEndPageRange = 1;
  } else {
    int32_t end(lstRanges[0].end);
    for (gint ii = 1; ii < ctRanges; ii++) {
      end = std::max(lstRanges[ii].end, end);
    }
    *aEndPageRange = end + 1;
  }

  g_free(lstRanges);
  return NS_OK;
}
NS_IMETHODIMP
nsPrintSettingsGTK::SetEndPageRange(int32_t aEndPageRange)
{
  int32_t startRange;
  GetStartPageRange(&startRange);

  GtkPageRange gtkRange;
  gtkRange.start = startRange - 1;
  gtkRange.end = aEndPageRange - 1;

  gtk_print_settings_set_page_ranges(mPrintSettings, &gtkRange, 1);

  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::GetPrintReversed(bool *aPrintReversed)
{
  *aPrintReversed = gtk_print_settings_get_reverse(mPrintSettings);
  return NS_OK;
}
NS_IMETHODIMP
nsPrintSettingsGTK::SetPrintReversed(bool aPrintReversed)
{
  gtk_print_settings_set_reverse(mPrintSettings, aPrintReversed);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::GetPrintInColor(bool *aPrintInColor)
{
  *aPrintInColor = gtk_print_settings_get_use_color(mPrintSettings);
  return NS_OK;
}
NS_IMETHODIMP
nsPrintSettingsGTK::SetPrintInColor(bool aPrintInColor)
{
  gtk_print_settings_set_use_color(mPrintSettings, aPrintInColor);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::GetOrientation(int32_t *aOrientation)
{
  NS_ENSURE_ARG_POINTER(aOrientation);

  GtkPageOrientation gtkOrient = gtk_page_setup_get_orientation(mPageSetup);
  switch (gtkOrient) {
    case GTK_PAGE_ORIENTATION_LANDSCAPE:
    case GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE:
      *aOrientation = kLandscapeOrientation;
      break;

    case GTK_PAGE_ORIENTATION_PORTRAIT:
    case GTK_PAGE_ORIENTATION_REVERSE_PORTRAIT:
    default:
      *aOrientation = kPortraitOrientation;
  }
  return NS_OK;
}
NS_IMETHODIMP
nsPrintSettingsGTK::SetOrientation(int32_t aOrientation)
{
  GtkPageOrientation gtkOrient;
  if (aOrientation == kLandscapeOrientation)
    gtkOrient = GTK_PAGE_ORIENTATION_LANDSCAPE;
  else
    gtkOrient = GTK_PAGE_ORIENTATION_PORTRAIT;

  gtk_print_settings_set_orientation(mPrintSettings, gtkOrient);
  gtk_page_setup_set_orientation(mPageSetup, gtkOrient);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::GetToFileName(char16_t * *aToFileName)
{
  // Get the gtk output filename
  const char* gtk_output_uri = gtk_print_settings_get(mPrintSettings, GTK_PRINT_SETTINGS_OUTPUT_URI);
  if (!gtk_output_uri) {
    *aToFileName = ToNewUnicode(mToFileName);
    return NS_OK;
  }

  // Convert to an nsIFile
  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_GetFileFromURLSpec(nsDependentCString(gtk_output_uri),
                                      getter_AddRefs(file));
  if (NS_FAILED(rv))
    return rv;

  // Extract the path
  nsAutoString path;
  rv = file->GetPath(path);
  NS_ENSURE_SUCCESS(rv, rv);

  *aToFileName = ToNewUnicode(path);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::SetToFileName(const char16_t * aToFileName)
{
  if (aToFileName[0] == 0) {
    mToFileName.SetLength(0);
    gtk_print_settings_set(mPrintSettings, GTK_PRINT_SETTINGS_OUTPUT_URI,
                           nullptr);
    return NS_OK;
  }

  if (StringEndsWith(nsDependentString(aToFileName), NS_LITERAL_STRING(".ps"))) {
    gtk_print_settings_set(mPrintSettings, GTK_PRINT_SETTINGS_OUTPUT_FILE_FORMAT, "ps");
  } else {
    gtk_print_settings_set(mPrintSettings, GTK_PRINT_SETTINGS_OUTPUT_FILE_FORMAT, "pdf");
  }

  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_NewLocalFile(nsDependentString(aToFileName), true,
                                getter_AddRefs(file));
  NS_ENSURE_SUCCESS(rv, rv);

  // Convert the nsIFile to a URL
  nsAutoCString url;
  rv = NS_GetURLSpecFromFile(file, url);
  NS_ENSURE_SUCCESS(rv, rv);

  gtk_print_settings_set(mPrintSettings, GTK_PRINT_SETTINGS_OUTPUT_URI, url.get());
  mToFileName = aToFileName;

  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::GetPrinterName(char16_t * *aPrinter)
{
  const char* gtkPrintName = gtk_print_settings_get_printer(mPrintSettings);
  if (!gtkPrintName) {
    if (GTK_IS_PRINTER(mGTKPrinter)) {
      gtkPrintName = gtk_printer_get_name(mGTKPrinter);
    } else {
      // This mimics what nsPrintSettingsImpl does when we try to Get before we Set
      nsXPIDLString nullPrintName;
      *aPrinter = ToNewUnicode(nullPrintName);
      return NS_OK;
    }
  }
  *aPrinter = ToNewUnicode(nsDependentCString(gtkPrintName));
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::SetPrinterName(const char16_t * aPrinter)
{
  NS_ConvertUTF16toUTF8 gtkPrinter(aPrinter);

  if (StringBeginsWith(gtkPrinter, NS_LITERAL_CSTRING("CUPS/"))) {
    // Strip off "CUPS/"; GTK might recognize the rest
    gtkPrinter.Cut(0, strlen("CUPS/"));
  }

  // Give mPrintSettings the passed-in printer name if either...
  // - it has no printer name stored yet
  // - it has an existing printer name that's different from
  //   the name passed to this function.
  const char* oldPrinterName = gtk_print_settings_get_printer(mPrintSettings);
  if (!oldPrinterName || !gtkPrinter.Equals(oldPrinterName)) {
    mIsInitedFromPrinter = false;
    mIsInitedFromPrefs = false;
    gtk_print_settings_set_printer(mPrintSettings, gtkPrinter.get());
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::GetNumCopies(int32_t *aNumCopies)
{
  NS_ENSURE_ARG_POINTER(aNumCopies);
  *aNumCopies = gtk_print_settings_get_n_copies(mPrintSettings);
  return NS_OK;
}
NS_IMETHODIMP
nsPrintSettingsGTK::SetNumCopies(int32_t aNumCopies)
{
  gtk_print_settings_set_n_copies(mPrintSettings, aNumCopies);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::GetScaling(double *aScaling)
{
  *aScaling = gtk_print_settings_get_scale(mPrintSettings) / 100.0;
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::SetScaling(double aScaling)
{
  gtk_print_settings_set_scale(mPrintSettings, aScaling * 100.0);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::GetPaperName(char16_t * *aPaperName)
{
  NS_ENSURE_ARG_POINTER(aPaperName);
  *aPaperName = ToNewUnicode(NS_ConvertUTF8toUTF16(gtk_paper_size_get_name(mPaperSize)));
  return NS_OK;
}
NS_IMETHODIMP
nsPrintSettingsGTK::SetPaperName(const char16_t * aPaperName)
{
  NS_ConvertUTF16toUTF8 gtkPaperName(aPaperName);

  // Convert these Gecko names to GTK names
  if (gtkPaperName.EqualsIgnoreCase("letter"))
    gtkPaperName.AssignLiteral(GTK_PAPER_NAME_LETTER);
  else if (gtkPaperName.EqualsIgnoreCase("legal"))
    gtkPaperName.AssignLiteral(GTK_PAPER_NAME_LEGAL);

  // Try to get the display name from the name so our paper size fits in the Page Setup dialog.
  GtkPaperSize* paperSize = gtk_paper_size_new(gtkPaperName.get());
  char* displayName = strdup(gtk_paper_size_get_display_name(paperSize));
  gtk_paper_size_free(paperSize);

  paperSize = gtk_paper_size_new_custom(gtkPaperName.get(), displayName,
                                        gtk_paper_size_get_width(mPaperSize, GTK_UNIT_INCH),
                                        gtk_paper_size_get_height(mPaperSize, GTK_UNIT_INCH),
                                        GTK_UNIT_INCH);

  free(displayName);
  gtk_paper_size_free(mPaperSize);
  mPaperSize = paperSize;
  SaveNewPageSize();
  return NS_OK;
}

GtkUnit
nsPrintSettingsGTK::GetGTKUnit(int16_t aGeckoUnit)
{
  if (aGeckoUnit == kPaperSizeMillimeters)
    return GTK_UNIT_MM;
  else
    return GTK_UNIT_INCH;
}

void
nsPrintSettingsGTK::SaveNewPageSize()
{
  gtk_print_settings_set_paper_size(mPrintSettings, mPaperSize);
  gtk_page_setup_set_paper_size(mPageSetup, mPaperSize);
}

void
nsPrintSettingsGTK::InitUnwriteableMargin()
{
  mUnwriteableMargin.SizeTo(
   NS_INCHES_TO_INT_TWIPS(gtk_page_setup_get_top_margin(mPageSetup, GTK_UNIT_INCH)),
   NS_INCHES_TO_INT_TWIPS(gtk_page_setup_get_right_margin(mPageSetup, GTK_UNIT_INCH)),
   NS_INCHES_TO_INT_TWIPS(gtk_page_setup_get_bottom_margin(mPageSetup, GTK_UNIT_INCH)),
   NS_INCHES_TO_INT_TWIPS(gtk_page_setup_get_left_margin(mPageSetup, GTK_UNIT_INCH))
  );
}

/**
 * NOTE: Need a custom set of SetUnwriteableMargin functions, because
 * whenever we change mUnwriteableMargin, we must pass the change
 * down to our GTKPageSetup object.  (This is needed in order for us
 * to give the correct default values in nsPrintDialogGTK.)
 *
 * It's important that the following functions pass 
 * mUnwriteableMargin values rather than aUnwriteableMargin values
 * to gtk_page_setup_set_[blank]_margin, because the two may not be
 * the same.  (Specifically, negative values of aUnwriteableMargin
 * are ignored by the nsPrintSettings::SetUnwriteableMargin functions.)
 */
NS_IMETHODIMP 
nsPrintSettingsGTK::SetUnwriteableMarginInTwips(nsIntMargin& aUnwriteableMargin)
{
  nsPrintSettings::SetUnwriteableMarginInTwips(aUnwriteableMargin);
  gtk_page_setup_set_top_margin(mPageSetup,
           NS_TWIPS_TO_INCHES(mUnwriteableMargin.top), GTK_UNIT_INCH);
  gtk_page_setup_set_left_margin(mPageSetup,
           NS_TWIPS_TO_INCHES(mUnwriteableMargin.left), GTK_UNIT_INCH);
  gtk_page_setup_set_bottom_margin(mPageSetup,
           NS_TWIPS_TO_INCHES(mUnwriteableMargin.bottom), GTK_UNIT_INCH);
  gtk_page_setup_set_right_margin(mPageSetup,
           NS_TWIPS_TO_INCHES(mUnwriteableMargin.right), GTK_UNIT_INCH);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::SetUnwriteableMarginTop(double aUnwriteableMarginTop)
{
  nsPrintSettings::SetUnwriteableMarginTop(aUnwriteableMarginTop);
  gtk_page_setup_set_top_margin(mPageSetup,
           NS_TWIPS_TO_INCHES(mUnwriteableMargin.top), GTK_UNIT_INCH);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::SetUnwriteableMarginLeft(double aUnwriteableMarginLeft)
{
  nsPrintSettings::SetUnwriteableMarginLeft(aUnwriteableMarginLeft);
  gtk_page_setup_set_left_margin(mPageSetup,
           NS_TWIPS_TO_INCHES(mUnwriteableMargin.left), GTK_UNIT_INCH);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::SetUnwriteableMarginBottom(double aUnwriteableMarginBottom)
{
  nsPrintSettings::SetUnwriteableMarginBottom(aUnwriteableMarginBottom);
  gtk_page_setup_set_bottom_margin(mPageSetup,
           NS_TWIPS_TO_INCHES(mUnwriteableMargin.bottom), GTK_UNIT_INCH);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::SetUnwriteableMarginRight(double aUnwriteableMarginRight)
{
  nsPrintSettings::SetUnwriteableMarginRight(aUnwriteableMarginRight);
  gtk_page_setup_set_right_margin(mPageSetup,
           NS_TWIPS_TO_INCHES(mUnwriteableMargin.right), GTK_UNIT_INCH);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::GetPaperWidth(double *aPaperWidth)
{
  NS_ENSURE_ARG_POINTER(aPaperWidth);
  *aPaperWidth = gtk_paper_size_get_width(mPaperSize, GetGTKUnit(mPaperSizeUnit));
  return NS_OK;
}
NS_IMETHODIMP
nsPrintSettingsGTK::SetPaperWidth(double aPaperWidth)
{
  gtk_paper_size_set_size(mPaperSize,
                          aPaperWidth,
                          gtk_paper_size_get_height(mPaperSize, GetGTKUnit(mPaperSizeUnit)),
                          GetGTKUnit(mPaperSizeUnit));
  SaveNewPageSize();
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::GetPaperHeight(double *aPaperHeight)
{
  NS_ENSURE_ARG_POINTER(aPaperHeight);
  *aPaperHeight = gtk_paper_size_get_height(mPaperSize, GetGTKUnit(mPaperSizeUnit));
  return NS_OK;
}
NS_IMETHODIMP
nsPrintSettingsGTK::SetPaperHeight(double aPaperHeight)
{
  gtk_paper_size_set_size(mPaperSize,
                          gtk_paper_size_get_width(mPaperSize, GetGTKUnit(mPaperSizeUnit)),
                          aPaperHeight,
                          GetGTKUnit(mPaperSizeUnit));
  SaveNewPageSize();
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::SetPaperSizeUnit(int16_t aPaperSizeUnit)
{
  // Convert units internally. e.g. they might have set the values while we're still in mm but
  // they change to inch just afterwards, expecting that their sizes are in inches.
  gtk_paper_size_set_size(mPaperSize,
                          gtk_paper_size_get_width(mPaperSize, GetGTKUnit(mPaperSizeUnit)),
                          gtk_paper_size_get_height(mPaperSize, GetGTKUnit(mPaperSizeUnit)),
                          GetGTKUnit(aPaperSizeUnit));
  SaveNewPageSize();

  mPaperSizeUnit = aPaperSizeUnit;
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::GetEffectivePageSize(double *aWidth, double *aHeight)
{
  *aWidth  = NS_INCHES_TO_INT_TWIPS(gtk_paper_size_get_width(mPaperSize, GTK_UNIT_INCH));
  *aHeight = NS_INCHES_TO_INT_TWIPS(gtk_paper_size_get_height(mPaperSize, GTK_UNIT_INCH));

  GtkPageOrientation gtkOrient = gtk_page_setup_get_orientation(mPageSetup);

  if (gtkOrient == GTK_PAGE_ORIENTATION_LANDSCAPE ||
      gtkOrient == GTK_PAGE_ORIENTATION_REVERSE_LANDSCAPE) {
    double temp = *aWidth;
    *aWidth = *aHeight;
    *aHeight = temp;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::SetupSilentPrinting()
{
  // We have to get a printer here, rather than when the print settings are constructed.
  // This is because when we request sync, GTK makes us wait in the *event loop* while waiting
  // for the enumeration to finish. We must do this when event loop runs are expected.
  gtk_enumerate_printers(printer_enumerator, this, nullptr, TRUE);

  // XXX If no default printer set, get the first one.
  if (!GTK_IS_PRINTER(mGTKPrinter))
    gtk_enumerate_printers(ref_printer, this, nullptr, TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::GetPageRanges(nsTArray<int32_t> &aPages)
{
  gint ctRanges;
  GtkPageRange* lstRanges = gtk_print_settings_get_page_ranges(mPrintSettings, &ctRanges);

  aPages.Clear();

  if (ctRanges > 1) {
    for (gint i = 0; i < ctRanges; i++) {
      aPages.AppendElement(lstRanges[i].start+1);
      aPages.AppendElement(lstRanges[i].end+1);
    }
  }

  g_free(lstRanges);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::GetResolution(int32_t *aResolution)
{
  if (!gtk_print_settings_has_key(mPrintSettings, GTK_PRINT_SETTINGS_RESOLUTION))
    return NS_ERROR_FAILURE;
  *aResolution = gtk_print_settings_get_resolution(mPrintSettings);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::SetResolution(int32_t aResolution)
{
  gtk_print_settings_set_resolution(mPrintSettings, aResolution);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::GetDuplex(int32_t *aDuplex)
{
  if (!gtk_print_settings_has_key(mPrintSettings, GTK_PRINT_SETTINGS_DUPLEX)) {
    *aDuplex = GTK_PRINT_DUPLEX_SIMPLEX;
  } else {
    *aDuplex = gtk_print_settings_get_duplex(mPrintSettings);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsGTK::SetDuplex(int32_t aDuplex)
{
  MOZ_ASSERT(aDuplex >= GTK_PRINT_DUPLEX_SIMPLEX &&
             aDuplex <= GTK_PRINT_DUPLEX_VERTICAL,
             "value is out of bounds for GtkPrintDuplex enum");
  gtk_print_settings_set_duplex(mPrintSettings, static_cast<GtkPrintDuplex>(aDuplex));
  return NS_OK;
}

