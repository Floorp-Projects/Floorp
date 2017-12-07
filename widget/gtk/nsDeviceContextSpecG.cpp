/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeviceContextSpecG.h"

#include "mozilla/gfx/PrintTargetPDF.h"
#include "mozilla/gfx/PrintTargetPS.h"
#include "mozilla/Logging.h"

#include "plstr.h"
#include "prenv.h" /* for PR_GetEnv */

#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsStringEnumerator.h"
#include "nsIServiceManager.h" 
#include "nsThreadUtils.h"

#include "nsPSPrinters.h"

#include "nsPrintSettingsGTK.h"

#include "nsIFileStreams.h"
#include "nsIFile.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

#include "mozilla/Preferences.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

using namespace mozilla;

using mozilla::gfx::IntSize;
using mozilla::gfx::PrintTarget;
using mozilla::gfx::PrintTargetPDF;
using mozilla::gfx::PrintTargetPS;

static LazyLogModule sDeviceContextSpecGTKLog("DeviceContextSpecGTK");
/* Macro to make lines shorter */
#define DO_PR_DEBUG_LOG(x) MOZ_LOG(sDeviceContextSpecGTKLog, mozilla::LogLevel::Debug, x)

//----------------------------------------------------------------------------------
// The printer data is shared between the PrinterEnumerator and the nsDeviceContextSpecGTK
// The PrinterEnumerator creates the printer info
// but the nsDeviceContextSpecGTK cleans it up
// If it gets created (via the Page Setup Dialog) but the user never prints anything
// then it will never be delete, so this class takes care of that.
class GlobalPrinters {
public:
  static GlobalPrinters* GetInstance()   { return &mGlobalPrinters; }
  ~GlobalPrinters()                      { FreeGlobalPrinters(); }

  void      FreeGlobalPrinters();
  nsresult  InitializeGlobalPrinters();

  bool      PrintersAreAllocated()       { return mGlobalPrinterList != nullptr; }
  uint32_t  GetNumPrinters()
    { return mGlobalPrinterList ? mGlobalPrinterList->Length() : 0; }
  nsString* GetStringAt(int32_t aInx)    { return &mGlobalPrinterList->ElementAt(aInx); }
  void      GetDefaultPrinterName(nsAString& aDefaultPrinterName);

protected:
  GlobalPrinters() {}

  static GlobalPrinters mGlobalPrinters;
  static nsTArray<nsString>* mGlobalPrinterList;
};

//---------------
// static members
GlobalPrinters GlobalPrinters::mGlobalPrinters;
nsTArray<nsString>* GlobalPrinters::mGlobalPrinterList = nullptr;
//---------------

nsDeviceContextSpecGTK::nsDeviceContextSpecGTK()
  : mGtkPrintSettings(nullptr)
  , mGtkPageSetup(nullptr)
{
  DO_PR_DEBUG_LOG(("nsDeviceContextSpecGTK::nsDeviceContextSpecGTK()\n"));
}

nsDeviceContextSpecGTK::~nsDeviceContextSpecGTK()
{
  DO_PR_DEBUG_LOG(("nsDeviceContextSpecGTK::~nsDeviceContextSpecGTK()\n"));

  if (mGtkPageSetup) {
    g_object_unref(mGtkPageSetup);
  }

  if (mGtkPrintSettings) {
    g_object_unref(mGtkPrintSettings);
  }
}

NS_IMPL_ISUPPORTS(nsDeviceContextSpecGTK,
                  nsIDeviceContextSpec)

already_AddRefed<PrintTarget> nsDeviceContextSpecGTK::MakePrintTarget()
{
  double width, height;
  mPrintSettings->GetEffectivePageSize(&width, &height);

  // convert twips to points
  width  /= TWIPS_PER_POINT_FLOAT;
  height /= TWIPS_PER_POINT_FLOAT;

  DO_PR_DEBUG_LOG(("Making PrintTarget: width = %f, height = %f\n", width, height));
  nsresult rv;

  // We shouldn't be attempting to get a surface if we've already got a spool
  // file.
  MOZ_ASSERT(!mSpoolFile);

  // Spool file. Use Glib's temporary file function since we're
  // already dependent on the gtk software stack.
  gchar *buf;
  gint fd = g_file_open_tmp("XXXXXX.tmp", &buf, nullptr);
  if (-1 == fd)
    return nullptr;
  close(fd);

  rv = NS_NewNativeLocalFile(nsDependentCString(buf), false,
                             getter_AddRefs(mSpoolFile));
  if (NS_FAILED(rv)) {
    unlink(buf);
    return nullptr;
  }

  mSpoolName = buf;
  g_free(buf);

  mSpoolFile->SetPermissions(0600);

  nsCOMPtr<nsIFileOutputStream> stream = do_CreateInstance("@mozilla.org/network/file-output-stream;1");
  rv = stream->Init(mSpoolFile, -1, -1, 0);
  if (NS_FAILED(rv))
    return nullptr;

  int16_t format;
  mPrintSettings->GetOutputFormat(&format);

  // Determine the real format with some GTK magic
  if (format == nsIPrintSettings::kOutputFormatNative) {
    if (mIsPPreview) {
      // There is nothing to detect on Print Preview, use PDF.
      format = nsIPrintSettings::kOutputFormatPDF;
    } else {
      return nullptr;
    }
  }

  IntSize size = IntSize::Truncate(width, height);

  if (format == nsIPrintSettings::kOutputFormatPDF) {
    return PrintTargetPDF::CreateOrNull(stream, size);
  }

  int32_t orientation;
  mPrintSettings->GetOrientation(&orientation);
  return PrintTargetPS::CreateOrNull(stream,
                                     size,
                                     orientation == nsIPrintSettings::kPortraitOrientation
                                       ? PrintTargetPS::PORTRAIT
                                       : PrintTargetPS::LANDSCAPE);
}

/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecGTK
 *  @update   dc 2/15/98
 *  @update   syd 3/2/99
 */
NS_IMETHODIMP nsDeviceContextSpecGTK::Init(nsIWidget *aWidget,
                                           nsIPrintSettings* aPS,
                                           bool aIsPrintPreview)
{
  DO_PR_DEBUG_LOG(("nsDeviceContextSpecGTK::Init(aPS=%p)\n", aPS));

  if (gtk_major_version < 2 ||
      (gtk_major_version == 2 && gtk_minor_version < 10))
    return NS_ERROR_NOT_AVAILABLE;  // I'm so sorry bz

  mPrintSettings = do_QueryInterface(aPS);
  if (!mPrintSettings)
    return NS_ERROR_NO_INTERFACE;

  mIsPPreview = aIsPrintPreview;

  // This is only set by embedders
  bool toFile;
  aPS->GetPrintToFile(&toFile);

  mToPrinter = !toFile && !aIsPrintPreview;

  mGtkPrintSettings = mPrintSettings->GetGtkPrintSettings();
  mGtkPageSetup = mPrintSettings->GetGtkPageSetup();

  // This is a horrible workaround for some printer driver bugs that treat custom page sizes different
  // to standard ones. If our paper object matches one of a standard one, use a standard paper size
  // object instead. See bug 414314 for more info.
  GtkPaperSize* geckosHackishPaperSize = gtk_page_setup_get_paper_size(mGtkPageSetup);
  GtkPaperSize* standardGtkPaperSize = gtk_paper_size_new(gtk_paper_size_get_name(geckosHackishPaperSize));

  mGtkPageSetup = gtk_page_setup_copy(mGtkPageSetup);
  mGtkPrintSettings = gtk_print_settings_copy(mGtkPrintSettings);

  GtkPaperSize* properPaperSize;
  if (gtk_paper_size_is_equal(geckosHackishPaperSize, standardGtkPaperSize)) {
    properPaperSize = standardGtkPaperSize;
  } else {
    properPaperSize = geckosHackishPaperSize;
  }
  gtk_print_settings_set_paper_size(mGtkPrintSettings, properPaperSize);
  gtk_page_setup_set_paper_size_and_default_margins(mGtkPageSetup, properPaperSize);
  gtk_paper_size_free(standardGtkPaperSize);

  return NS_OK;
}

static void
#if (MOZ_WIDGET_GTK == 3)
print_callback(GtkPrintJob *aJob, gpointer aData, const GError *aError) {
#else
print_callback(GtkPrintJob *aJob, gpointer aData, GError *aError) {
#endif
  g_object_unref(aJob);
  ((nsIFile*) aData)->Remove(false);
}

static void
ns_release_macro(gpointer aData) {
  nsIFile* spoolFile = (nsIFile*) aData;
  NS_RELEASE(spoolFile);
}

/* static */
gboolean nsDeviceContextSpecGTK::PrinterEnumerator(GtkPrinter *aPrinter,
                                                   gpointer aData) {
  nsDeviceContextSpecGTK *spec = (nsDeviceContextSpecGTK*)aData;

  // Find the printer whose name matches the one inside the settings.
  nsString printerName;
  nsresult rv =
    spec->mPrintSettings->GetPrinterName(printerName);
  if (NS_SUCCEEDED(rv) && !printerName.IsVoid()) {
    NS_ConvertUTF16toUTF8 requestedName(printerName);
    const char* currentName = gtk_printer_get_name(aPrinter);
    if (requestedName.Equals(currentName)) {
      spec->mPrintSettings->SetGtkPrinter(aPrinter);

      // Bug 1145916 - attempting to kick off a print job for this printer
      // during this tick of the event loop will result in the printer backend
      // misunderstanding what the capabilities of the printer are due to a
      // GTK bug (https://bugzilla.gnome.org/show_bug.cgi?id=753041). We
      // sidestep this by deferring the print to the next tick.
      NS_DispatchToCurrentThread(
        NewRunnableMethod("nsDeviceContextSpecGTK::StartPrintJob",
                          spec,
                          &nsDeviceContextSpecGTK::StartPrintJob));
      return TRUE;
    }
  }

  // We haven't found it yet - keep searching...
  return FALSE;
}

void nsDeviceContextSpecGTK::StartPrintJob() {
  GtkPrintJob* job = gtk_print_job_new(mTitle.get(),
                                       mPrintSettings->GetGtkPrinter(),
                                       mGtkPrintSettings,
                                       mGtkPageSetup);

  if (!gtk_print_job_set_source_file(job, mSpoolName.get(), nullptr))
    return;

  NS_ADDREF(mSpoolFile.get());
  gtk_print_job_send(job, print_callback, mSpoolFile, ns_release_macro);
}

void
nsDeviceContextSpecGTK::EnumeratePrinters()
{
  gtk_enumerate_printers(&nsDeviceContextSpecGTK::PrinterEnumerator, this,
                         nullptr, TRUE);
}

NS_IMETHODIMP
nsDeviceContextSpecGTK::BeginDocument(const nsAString& aTitle,
                                      const nsAString& aPrintToFileName,
                                      int32_t aStartPage, int32_t aEndPage)
{
  // Print job names exceeding 255 bytes are safe with GTK version 3.18.2 or
  // newer. This is a workaround for old GTK.
  if (gtk_check_version(3,18,2) != nullptr) {
    PrintTarget::AdjustPrintJobNameForIPP(aTitle, mTitle);
  } else {
    CopyUTF16toUTF8(aTitle, mTitle);
  }

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::EndDocument()
{
  if (mToPrinter) {
    // At this point, we might have a GtkPrinter set up in nsPrintSettingsGTK,
    // or we might not. In the single-process case, we probably will, as this
    // is populated by the print settings dialog, or set to the default
    // printer.
    // In the multi-process case, we proxy the print settings dialog over to
    // the parent process, and only get the name of the printer back on the
    // content process side. In that case, we need to enumerate the printers
    // on the content side, and find a printer with a matching name.

    GtkPrinter* printer = mPrintSettings->GetGtkPrinter();
    if (printer) {
      // We have a printer, so we can print right away.
      StartPrintJob();
    } else {
      // We don't have a printer. We have to enumerate the printers and find
      // one with a matching name.
      NS_DispatchToCurrentThread(
        NewRunnableMethod("nsDeviceContextSpecGTK::EnumeratePrinters",
                          this,
                          &nsDeviceContextSpecGTK::EnumeratePrinters));
    }
  } else {
    // Handle print-to-file ourselves for the benefit of embedders
    nsString targetPath;
    nsCOMPtr<nsIFile> destFile;
    mPrintSettings->GetToFileName(targetPath);

    nsresult rv = NS_NewLocalFile(targetPath, false, getter_AddRefs(destFile));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString destLeafName;
    rv = destFile->GetLeafName(destLeafName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> destDir;
    rv = destFile->GetParent(getter_AddRefs(destDir));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mSpoolFile->MoveTo(destDir, destLeafName);
    NS_ENSURE_SUCCESS(rv, rv);

    // This is the standard way to get the UNIX umask. Ugh.
    mode_t mask = umask(0);
    umask(mask);
    // If you're not familiar with umasks, they contain the bits of what NOT to set in the permissions
    // (thats because files and directories have different numbers of bits for their permissions)
    destFile->SetPermissions(0666 & ~(mask));
  }
  return NS_OK;
}

//  Printer Enumerator
nsPrinterEnumeratorGTK::nsPrinterEnumeratorGTK()
{
}

NS_IMPL_ISUPPORTS(nsPrinterEnumeratorGTK, nsIPrinterEnumerator)

NS_IMETHODIMP nsPrinterEnumeratorGTK::GetPrinterNameList(nsIStringEnumerator **aPrinterNameList)
{
  NS_ENSURE_ARG_POINTER(aPrinterNameList);
  *aPrinterNameList = nullptr;
  
  nsresult rv = GlobalPrinters::GetInstance()->InitializeGlobalPrinters();
  if (NS_FAILED(rv)) {
    return rv;
  }

  uint32_t numPrinters = GlobalPrinters::GetInstance()->GetNumPrinters();
  nsTArray<nsString> *printers = new nsTArray<nsString>(numPrinters);
  if (!printers) {
    GlobalPrinters::GetInstance()->FreeGlobalPrinters();
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  uint32_t count = 0;
  while( count < numPrinters )
  {
    printers->AppendElement(*GlobalPrinters::GetInstance()->GetStringAt(count++));
  }
  GlobalPrinters::GetInstance()->FreeGlobalPrinters();

  return NS_NewAdoptingStringEnumerator(aPrinterNameList, printers);
}

NS_IMETHODIMP nsPrinterEnumeratorGTK::GetDefaultPrinterName(nsAString& aDefaultPrinterName)
{
  DO_PR_DEBUG_LOG(("nsPrinterEnumeratorGTK::GetDefaultPrinterName()\n"));

  GlobalPrinters::GetInstance()->GetDefaultPrinterName(aDefaultPrinterName);

  DO_PR_DEBUG_LOG(("GetDefaultPrinterName(): default printer='%s'.\n", NS_ConvertUTF16toUTF8(aDefaultPrinterName).get()));
  return NS_OK;
}

NS_IMETHODIMP
nsPrinterEnumeratorGTK::InitPrintSettingsFromPrinter(const nsAString& aPrinterName,
                                                     nsIPrintSettings *aPrintSettings)
{
  DO_PR_DEBUG_LOG(("nsPrinterEnumeratorGTK::InitPrintSettingsFromPrinter()"));

  NS_ENSURE_ARG_POINTER(aPrintSettings);

  // Set a default file name.
  nsAutoString filename;
  nsresult rv = aPrintSettings->GetToFileName(filename);
  if (NS_FAILED(rv) || filename.IsEmpty()) {
    const char* path = PR_GetEnv("PWD");
    if (!path) {
      path = PR_GetEnv("HOME");
    }

    if (path) {
      CopyUTF8toUTF16(path, filename);
      filename.AppendLiteral("/mozilla.pdf");
    } else {
      filename.AssignLiteral("mozilla.pdf");
    }

    DO_PR_DEBUG_LOG(("Setting default filename to '%s'\n",
                     NS_ConvertUTF16toUTF8(filename).get()));
    aPrintSettings->SetToFileName(filename);
  }

  aPrintSettings->SetIsInitializedFromPrinter(true);

  return NS_OK;    
}

//----------------------------------------------------------------------
nsresult GlobalPrinters::InitializeGlobalPrinters ()
{
  if (PrintersAreAllocated()) {
    return NS_OK;
  }

  mGlobalPrinterList = new nsTArray<nsString>();

  nsPSPrinterList psMgr;
  if (psMgr.Enabled()) {
    /* Get the list of PostScript-module printers */
    // XXX: this function is the only user of GetPrinterList
    // So it may be interesting to convert the nsCStrings
    // in this function, we would save one loop here
    nsTArray<nsCString> printerList;
    psMgr.GetPrinterList(printerList);
    for (uint32_t i = 0; i < printerList.Length(); i++)
    {
      mGlobalPrinterList->AppendElement(NS_ConvertUTF8toUTF16(printerList[i]));
    }
  }

  /* If there are no printers available after all checks, return an error */
  if (!mGlobalPrinterList->Length())
  {
    /* Make sure we do not cache an empty printer list */
    FreeGlobalPrinters();

    return NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE;
  }

  return NS_OK;
}

//----------------------------------------------------------------------
void GlobalPrinters::FreeGlobalPrinters()
{
  if (mGlobalPrinterList) {
    delete mGlobalPrinterList;
    mGlobalPrinterList = nullptr;
  }  
}

void 
GlobalPrinters::GetDefaultPrinterName(nsAString& aDefaultPrinterName)
{
  aDefaultPrinterName.Truncate();
  
  bool allocate = !GlobalPrinters::GetInstance()->PrintersAreAllocated();
  
  if (allocate) {
    nsresult rv = GlobalPrinters::GetInstance()->InitializeGlobalPrinters();
    if (NS_FAILED(rv)) {
      return;
    }
  }
  NS_ASSERTION(GlobalPrinters::GetInstance()->PrintersAreAllocated(), "no GlobalPrinters");

  if (GlobalPrinters::GetInstance()->GetNumPrinters() == 0)
    return;
  
  aDefaultPrinterName = *GlobalPrinters::GetInstance()->GetStringAt(0);

  if (allocate) {  
    GlobalPrinters::GetInstance()->FreeGlobalPrinters();
  }  
}

