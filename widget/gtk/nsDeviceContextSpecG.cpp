/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeviceContextSpecG.h"

#include "mozilla/gfx/PrintTargetPDF.h"
#include "mozilla/gfx/PrintTargetPS.h"
#include "mozilla/Logging.h"
#include "mozilla/Services.h"

#include "plstr.h"
#include "prenv.h" /* for PR_GetEnv */

#include "nsComponentManagerUtils.h"
#include "nsIObserverService.h"
#include "nsPrintfCString.h"
#include "nsReadableUtils.h"
#include "nsThreadUtils.h"

#include "nsCUPSShim.h"
#include "nsPrinterCUPS.h"

#include "nsPrintSettingsGTK.h"

#include "nsIFileStreams.h"
#include "nsIFile.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_print.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

// To check if we need to use flatpak portal for printing
#include "nsIGIOService.h"

using namespace mozilla;

using mozilla::gfx::IntSize;
using mozilla::gfx::PrintTarget;
using mozilla::gfx::PrintTargetPDF;
using mozilla::gfx::PrintTargetPS;

static LazyLogModule sDeviceContextSpecGTKLog("DeviceContextSpecGTK");

nsDeviceContextSpecGTK::nsDeviceContextSpecGTK()
    : mGtkPrintSettings(nullptr), mGtkPageSetup(nullptr) {}

nsDeviceContextSpecGTK::~nsDeviceContextSpecGTK() {
  if (mGtkPageSetup) {
    g_object_unref(mGtkPageSetup);
  }

  if (mGtkPrintSettings) {
    g_object_unref(mGtkPrintSettings);
  }

  if (mSpoolFile) {
    mSpoolFile->Remove(false);
  }
}

NS_IMPL_ISUPPORTS(nsDeviceContextSpecGTK, nsIDeviceContextSpec)

already_AddRefed<PrintTarget> nsDeviceContextSpecGTK::MakePrintTarget() {
  double width, height;
  mPrintSettings->GetEffectivePageSize(&width, &height);

  // convert twips to points
  width /= TWIPS_PER_POINT_FLOAT;
  height /= TWIPS_PER_POINT_FLOAT;

  nsresult rv;

  // We shouldn't be attempting to get a surface if we've already got a spool
  // file.
  MOZ_ASSERT(!mSpoolFile);

  // Spool file. Use Glib's temporary file function since we're
  // already dependent on the gtk software stack.
  gchar* buf;
  gint fd = g_file_open_tmp("XXXXXX.tmp", &buf, nullptr);
  if (-1 == fd) return nullptr;
  close(fd);

  rv = NS_NewNativeLocalFile(nsDependentCString(buf), false,
                             getter_AddRefs(mSpoolFile));
  if (NS_FAILED(rv)) {
    unlink(buf);
    g_free(buf);
    return nullptr;
  }

  mSpoolName = buf;
  g_free(buf);

  mSpoolFile->SetPermissions(0600);

  nsCOMPtr<nsIFileOutputStream> stream =
      do_CreateInstance("@mozilla.org/network/file-output-stream;1");
  rv = stream->Init(mSpoolFile, -1, -1, 0);
  if (NS_FAILED(rv)) return nullptr;

  int16_t format;
  mPrintSettings->GetOutputFormat(&format);

  // We assume PDF output if asked for native output.
  if (format == nsIPrintSettings::kOutputFormatNative) {
    format = nsIPrintSettings::kOutputFormatPDF;
  }

  IntSize size = IntSize::Truncate(width, height);
  if (format == nsIPrintSettings::kOutputFormatPDF) {
    return PrintTargetPDF::CreateOrNull(stream, size);
  }

  int32_t orientation;
  mPrintSettings->GetOrientation(&orientation);
  return PrintTargetPS::CreateOrNull(
      stream, size,
      orientation == nsIPrintSettings::kPortraitOrientation
          ? PrintTargetPS::PORTRAIT
          : PrintTargetPS::LANDSCAPE);
}

#define DECLARE_KNOWN_MONOCHROME_SETTING(key_, value_) {"cups-" key_, value_},

struct {
  const char* mKey;
  const char* mValue;
} kKnownMonochromeSettings[] = {
    CUPS_EACH_MONOCHROME_PRINTER_SETTING(DECLARE_KNOWN_MONOCHROME_SETTING)};

#undef DECLARE_KNOWN_MONOCHROME_SETTING

/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecGTK
 *  @update   dc 2/15/98
 *  @update   syd 3/2/99
 */
NS_IMETHODIMP nsDeviceContextSpecGTK::Init(nsIWidget* aWidget,
                                           nsIPrintSettings* aPS,
                                           bool aIsPrintPreview) {
  if (gtk_major_version < 2 ||
      (gtk_major_version == 2 && gtk_minor_version < 10))
    return NS_ERROR_NOT_AVAILABLE;  // I'm so sorry bz

  mPrintSettings = do_QueryInterface(aPS);
  if (!mPrintSettings) return NS_ERROR_NO_INTERFACE;

  // This is only set by embedders
  bool toFile;
  aPS->GetPrintToFile(&toFile);

  mToPrinter = !toFile && !aIsPrintPreview;

  mGtkPrintSettings = mPrintSettings->GetGtkPrintSettings();
  mGtkPageSetup = mPrintSettings->GetGtkPageSetup();

  // This is a horrible workaround for some printer driver bugs that treat
  // custom page sizes different to standard ones. If our paper object matches
  // one of a standard one, use a standard paper size object instead. See bug
  // 414314 for more info.
  GtkPaperSize* geckosHackishPaperSize =
      gtk_page_setup_get_paper_size(mGtkPageSetup);
  GtkPaperSize* standardGtkPaperSize =
      gtk_paper_size_new(gtk_paper_size_get_name(geckosHackishPaperSize));

  mGtkPageSetup = gtk_page_setup_copy(mGtkPageSetup);
  mGtkPrintSettings = gtk_print_settings_copy(mGtkPrintSettings);

  if (!aPS->GetPrintInColor() && StaticPrefs::print_cups_monochrome_enabled()) {
    for (const auto& setting : kKnownMonochromeSettings) {
      gtk_print_settings_set(mGtkPrintSettings, setting.mKey, setting.mValue);
    }
  }

  GtkPaperSize* properPaperSize;
  if (gtk_paper_size_is_equal(geckosHackishPaperSize, standardGtkPaperSize)) {
    properPaperSize = standardGtkPaperSize;
  } else {
    properPaperSize = geckosHackishPaperSize;
  }
  gtk_print_settings_set_paper_size(mGtkPrintSettings, properPaperSize);
  gtk_page_setup_set_paper_size_and_default_margins(mGtkPageSetup,
                                                    properPaperSize);
  gtk_paper_size_free(standardGtkPaperSize);

  return NS_OK;
}

static void print_callback(GtkPrintJob* aJob, gpointer aData,
                           const GError* aError) {
  g_object_unref(aJob);
  ((nsIFile*)aData)->Remove(false);
}

/* static */
gboolean nsDeviceContextSpecGTK::PrinterEnumerator(GtkPrinter* aPrinter,
                                                   gpointer aData) {
  nsDeviceContextSpecGTK* spec = (nsDeviceContextSpecGTK*)aData;

  // Find the printer whose name matches the one inside the settings.
  nsString printerName;
  nsresult rv = spec->mPrintSettings->GetPrinterName(printerName);
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
          NewRunnableMethod("nsDeviceContextSpecGTK::StartPrintJob", spec,
                            &nsDeviceContextSpecGTK::StartPrintJob));
      return TRUE;
    }
  }

  // We haven't found it yet - keep searching...
  return FALSE;
}

void nsDeviceContextSpecGTK::StartPrintJob() {
  GtkPrintJob* job =
      gtk_print_job_new(mTitle.get(), mPrintSettings->GetGtkPrinter(),
                        mGtkPrintSettings, mGtkPageSetup);

  if (!gtk_print_job_set_source_file(job, mSpoolName.get(), nullptr)) return;

  // Now gtk owns the print job, and will be released via our callback.
  gtk_print_job_send(job, print_callback, mSpoolFile.forget().take(),
                     [](gpointer aData) {
                       auto* spoolFile = static_cast<nsIFile*>(aData);
                       NS_RELEASE(spoolFile);
                     });
}

void nsDeviceContextSpecGTK::EnumeratePrinters() {
  gtk_enumerate_printers(&nsDeviceContextSpecGTK::PrinterEnumerator, this,
                         nullptr, TRUE);
}

NS_IMETHODIMP
nsDeviceContextSpecGTK::BeginDocument(const nsAString& aTitle,
                                      const nsAString& aPrintToFileName,
                                      int32_t aStartPage, int32_t aEndPage) {
  // Print job names exceeding 255 bytes are safe with GTK version 3.18.2 or
  // newer. This is a workaround for old GTK.
  if (gtk_check_version(3, 18, 2) != nullptr) {
    PrintTarget::AdjustPrintJobNameForIPP(aTitle, mTitle);
  } else {
    CopyUTF16toUTF8(aTitle, mTitle);
  }

  return NS_OK;
}

NS_IMETHODIMP nsDeviceContextSpecGTK::EndDocument() {
  if (mToPrinter) {
    // At this point, we might have a GtkPrinter set up in nsPrintSettingsGTK,
    // or we might not. In the single-process case, we probably will, as this
    // is populated by the print settings dialog, or set to the default
    // printer.
    // In the multi-process case, we proxy the print settings dialog over to
    // the parent process, and only get the name of the printer back on the
    // content process side. In that case, we need to enumerate the printers
    // on the content side, and find a printer with a matching name.

    if (mPrintSettings->GetGtkPrinter()) {
      // We have a printer, so we can print right away.
      StartPrintJob();
    } else {
      // We don't have a printer. We have to enumerate the printers and find
      // one with a matching name.
      NS_DispatchToCurrentThread(
          NewRunnableMethod("nsDeviceContextSpecGTK::EnumeratePrinters", this,
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

    mSpoolFile = nullptr;

    // This is the standard way to get the UNIX umask. Ugh.
    mode_t mask = umask(0);
    umask(mask);
    // If you're not familiar with umasks, they contain the bits of what NOT to
    // set in the permissions (thats because files and directories have
    // different numbers of bits for their permissions)
    destFile->SetPermissions(0666 & ~(mask));

    // Notify flatpak printing portal that file is completely written
    nsCOMPtr<nsIGIOService> giovfs = do_GetService(NS_GIOSERVICE_CONTRACTID);
    bool shouldUsePortal;
    if (giovfs) {
      giovfs->ShouldUseFlatpakPortal(&shouldUsePortal);
      if (shouldUsePortal) {
        // Use the name of the file for printing to match with
        // nsFlatpakPrintPortal
        nsCOMPtr<nsIObserverService> os =
            mozilla::services::GetObserverService();
        // Pass filename to be sure that observer process the right data
        os->NotifyObservers(nullptr, "print-to-file-finished",
                            targetPath.get());
      }
    }
  }
  return NS_OK;
}
