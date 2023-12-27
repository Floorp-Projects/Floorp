/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeviceContextSpecG.h"

#include "mozilla/gfx/PrintPromise.h"
#include "mozilla/gfx/PrintTargetPDF.h"
#include "mozilla/Logging.h"
#include "mozilla/Services.h"
#include "mozilla/GUniquePtr.h"
#include "mozilla/WidgetUtilsGtk.h"

#include "prenv.h" /* for PR_GetEnv */

#include "nsComponentManagerUtils.h"
#include "nsIObserverService.h"
#include "nsPrintfCString.h"
#include "nsQueryObject.h"
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

#include <dlfcn.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// To check if we need to use flatpak portal for printing
#include "nsIGIOService.h"

using namespace mozilla;

using mozilla::gfx::IntSize;
using mozilla::gfx::PrintEndDocumentPromise;
using mozilla::gfx::PrintTarget;
using mozilla::gfx::PrintTargetPDF;

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
  mPrintSettings->GetEffectiveSheetSize(&width, &height);

  // convert twips to points
  width /= TWIPS_PER_POINT_FLOAT;
  height /= TWIPS_PER_POINT_FLOAT;

  // We shouldn't be attempting to get a surface if we've already got a spool
  // file.
  MOZ_ASSERT(!mSpoolFile);

  auto stream = [&]() -> nsCOMPtr<nsIOutputStream> {
    if (mPrintSettings->GetOutputDestination() ==
        nsIPrintSettings::kOutputDestinationStream) {
      nsCOMPtr<nsIOutputStream> out;
      mPrintSettings->GetOutputStream(getter_AddRefs(out));
      return out;
    }
    // Spool file. Use Glib's temporary file function since we're
    // already dependent on the gtk software stack.
    gchar* buf;
    gint fd = g_file_open_tmp("XXXXXX.tmp", &buf, nullptr);
    if (-1 == fd) {
      return nullptr;
    }
    close(fd);
    if (NS_FAILED(NS_NewNativeLocalFile(nsDependentCString(buf), false,
                                        getter_AddRefs(mSpoolFile)))) {
      unlink(buf);
      g_free(buf);
      return nullptr;
    }
    mSpoolName = buf;
    g_free(buf);
    mSpoolFile->SetPermissions(0600);
    nsCOMPtr<nsIFileOutputStream> stream =
        do_CreateInstance("@mozilla.org/network/file-output-stream;1");
    if (NS_FAILED(stream->Init(mSpoolFile, -1, -1, 0))) {
      return nullptr;
    }
    return stream;
  }();

  return PrintTargetPDF::CreateOrNull(stream, IntSize::Ceil(width, height));
}

#define DECLARE_KNOWN_MONOCHROME_SETTING(key_, value_) {"cups-" key_, value_},

struct {
  const char* mKey;
  const char* mValue;
} kKnownMonochromeSettings[] = {
    CUPS_EACH_MONOCHROME_PRINTER_SETTING(DECLARE_KNOWN_MONOCHROME_SETTING)};

#undef DECLARE_KNOWN_MONOCHROME_SETTING

// https://developer.gnome.org/gtk3/stable/GtkPaperSize.html#gtk-paper-size-new-from-ipp
static GUniquePtr<GtkPaperSize> GtkPaperSizeFromIpp(const gchar* aIppName,
                                                    gdouble aWidth,
                                                    gdouble aHeight) {
  static auto sPtr = (GtkPaperSize * (*)(const gchar*, gdouble, gdouble))
      dlsym(RTLD_DEFAULT, "gtk_paper_size_new_from_ipp");
  if (gtk_check_version(3, 16, 0)) {
    return nullptr;
  }
  return GUniquePtr<GtkPaperSize>(sPtr(aIppName, aWidth, aHeight));
}

static bool PaperSizeAlmostEquals(GtkPaperSize* aSize,
                                  GtkPaperSize* aOtherSize) {
  const double kEpsilon = 1.0;  // millimetres
  // GTK stores sizes internally in millimetres so just use that.
  if (fabs(gtk_paper_size_get_height(aSize, GTK_UNIT_MM) -
           gtk_paper_size_get_height(aOtherSize, GTK_UNIT_MM)) > kEpsilon) {
    return false;
  }
  if (fabs(gtk_paper_size_get_width(aSize, GTK_UNIT_MM) -
           gtk_paper_size_get_width(aOtherSize, GTK_UNIT_MM)) > kEpsilon) {
    return false;
  }
  return true;
}

// Prefer the ppd name because some printers don't deal well even with standard
// ipp names.
static GUniquePtr<GtkPaperSize> PpdSizeFromIppName(const gchar* aIppName) {
  static constexpr struct {
    const char* mCups;
    const char* mGtk;
  } kMap[] = {
      {CUPS_MEDIA_A3, GTK_PAPER_NAME_A3},
      {CUPS_MEDIA_A4, GTK_PAPER_NAME_A4},
      {CUPS_MEDIA_A5, GTK_PAPER_NAME_A5},
      {CUPS_MEDIA_LETTER, GTK_PAPER_NAME_LETTER},
      {CUPS_MEDIA_LEGAL, GTK_PAPER_NAME_LEGAL},
      // Other gtk sizes with no standard CUPS constant: _EXECUTIVE and _B5
  };

  for (const auto& entry : kMap) {
    if (!strcmp(entry.mCups, aIppName)) {
      return GUniquePtr<GtkPaperSize>(gtk_paper_size_new(entry.mGtk));
    }
  }

  return nullptr;
}

// This is a horrible workaround for some printer driver bugs that treat custom
// page sizes different to standard ones. If our paper object matches one of a
// standard one, use a standard paper size object instead.
//
// We prefer ppd to ipp to custom sizes.
//
// See bug 414314, bug 1691798, and bug 1717292 for more info.
static GUniquePtr<GtkPaperSize> GetStandardGtkPaperSize(
    GtkPaperSize* aGeckoPaperSize) {
  // We should get an ipp name from cups, try to get a ppd from that first.
  const gchar* geckoName = gtk_paper_size_get_name(aGeckoPaperSize);
  if (auto ppd = PpdSizeFromIppName(geckoName)) {
    return ppd;
  }

  // We try gtk_paper_size_new_from_ipp next, because even though
  // gtk_paper_size_new tries to deal with ipp, it has some rounding issues that
  // the ipp equivalent doesn't have, see
  // https://gitlab.gnome.org/GNOME/gtk/-/issues/3685.
  if (auto ipp = GtkPaperSizeFromIpp(
          geckoName, gtk_paper_size_get_width(aGeckoPaperSize, GTK_UNIT_POINTS),
          gtk_paper_size_get_height(aGeckoPaperSize, GTK_UNIT_POINTS))) {
    if (!gtk_paper_size_is_custom(ipp.get())) {
      if (auto ppd = PpdSizeFromIppName(gtk_paper_size_get_name(ipp.get()))) {
        return ppd;
      }
      return ipp;
    }
  }

  GUniquePtr<GtkPaperSize> size(gtk_paper_size_new(geckoName));
  // gtk_paper_size_is_equal compares just paper names. The name in Gecko
  // might come from CUPS, which is an ipp size, and gets normalized by gtk.
  // So check also for the same actual paper size.
  if (gtk_paper_size_is_equal(size.get(), aGeckoPaperSize) ||
      PaperSizeAlmostEquals(aGeckoPaperSize, size.get())) {
    return size;
  }

  // Not the same after all, so use our custom paper sizes instead.
  return nullptr;
}

/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecGTK
 *  @update   dc 2/15/98
 *  @update   syd 3/2/99
 */
NS_IMETHODIMP nsDeviceContextSpecGTK::Init(nsIPrintSettings* aPS,
                                           bool aIsPrintPreview) {
  RefPtr<nsPrintSettingsGTK> settings = do_QueryObject(aPS);
  if (!settings) {
    return NS_ERROR_NO_INTERFACE;
  }
  mPrintSettings = aPS;

  mGtkPrintSettings = settings->GetGtkPrintSettings();
  mGtkPageSetup = settings->GetGtkPageSetup();

  GtkPaperSize* geckoPaperSize = gtk_page_setup_get_paper_size(mGtkPageSetup);
  GUniquePtr<GtkPaperSize> gtkPaperSize =
      GetStandardGtkPaperSize(geckoPaperSize);

  mGtkPageSetup = gtk_page_setup_copy(mGtkPageSetup);
  mGtkPrintSettings = gtk_print_settings_copy(mGtkPrintSettings);

  if (!aPS->GetPrintInColor() && StaticPrefs::print_cups_monochrome_enabled()) {
    for (const auto& setting : kKnownMonochromeSettings) {
      gtk_print_settings_set(mGtkPrintSettings, setting.mKey, setting.mValue);
    }
    auto applySetting = [&](const nsACString& aKey, const nsACString& aVal) {
      nsAutoCString extra;
      extra.AppendASCII("cups-");
      extra.Append(aKey);
      gtk_print_settings_set(mGtkPrintSettings, extra.get(),
                             nsAutoCString(aVal).get());
    };
    nsPrinterCUPS::ForEachExtraMonochromeSetting(applySetting);
  }

  GtkPaperSize* properPaperSize =
      gtkPaperSize ? gtkPaperSize.get() : geckoPaperSize;
  gtk_print_settings_set_paper_size(mGtkPrintSettings, properPaperSize);
  gtk_page_setup_set_paper_size_and_default_margins(mGtkPageSetup,
                                                    properPaperSize);
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

  if (spec->mHasEnumerationFoundAMatch) {
    // We're already done, but we're letting the enumeration run its course,
    // to avoid a GTK bug.
    return FALSE;
  }

  // Find the printer whose name matches the one inside the settings.
  nsString printerName;
  nsresult rv = spec->mPrintSettings->GetPrinterName(printerName);
  if (NS_SUCCEEDED(rv) && !printerName.IsVoid()) {
    NS_ConvertUTF16toUTF8 requestedName(printerName);
    const char* currentName = gtk_printer_get_name(aPrinter);
    if (requestedName.Equals(currentName)) {
      nsPrintSettingsGTK::From(spec->mPrintSettings)->SetGtkPrinter(aPrinter);

      // Bug 1145916 - attempting to kick off a print job for this printer
      // during this tick of the event loop will result in the printer backend
      // misunderstanding what the capabilities of the printer are due to a
      // GTK bug (https://bugzilla.gnome.org/show_bug.cgi?id=753041). We
      // sidestep this by deferring the print to the next tick.
      NS_DispatchToCurrentThread(
          NewRunnableMethod("nsDeviceContextSpecGTK::StartPrintJob", spec,
                            &nsDeviceContextSpecGTK::StartPrintJob));

      // We're already done, but we need to let the enumeration run its course,
      // to avoid a GTK bug. So we record that we've found a match and
      // then return FALSE.
      // TODO: If/when we can be sure that GTK handles this OK, we could
      // return TRUE to avoid some needless enumeration.
      spec->mHasEnumerationFoundAMatch = true;
      return FALSE;
    }
  }

  // We haven't found it yet - keep searching...
  return FALSE;
}

void nsDeviceContextSpecGTK::StartPrintJob() {
  GtkPrintJob* job = gtk_print_job_new(
      mTitle.get(), nsPrintSettingsGTK::From(mPrintSettings)->GetGtkPrinter(),
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
  mHasEnumerationFoundAMatch = false;
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

RefPtr<PrintEndDocumentPromise> nsDeviceContextSpecGTK::EndDocument() {
  switch (mPrintSettings->GetOutputDestination()) {
    case nsIPrintSettings::kOutputDestinationPrinter: {
      // At this point, we might have a GtkPrinter set up in nsPrintSettingsGTK,
      // or we might not. In the single-process case, we probably will, as this
      // is populated by the print settings dialog, or set to the default
      // printer.
      // In the multi-process case, we proxy the print settings dialog over to
      // the parent process, and only get the name of the printer back on the
      // content process side. In that case, we need to enumerate the printers
      // on the content side, and find a printer with a matching name.

      if (nsPrintSettingsGTK::From(mPrintSettings)->GetGtkPrinter()) {
        // We have a printer, so we can print right away.
        StartPrintJob();
      } else {
        // We don't have a printer. We have to enumerate the printers and find
        // one with a matching name.
        NS_DispatchToCurrentThread(
            NewRunnableMethod("nsDeviceContextSpecGTK::EnumeratePrinters", this,
                              &nsDeviceContextSpecGTK::EnumeratePrinters));
      }
      break;
    }
    case nsIPrintSettings::kOutputDestinationFile: {
      // Handle print-to-file ourselves for the benefit of embedders
      nsString targetPath;
      nsCOMPtr<nsIFile> destFile;
      mPrintSettings->GetToFileName(targetPath);

      nsresult rv =
          NS_NewLocalFile(targetPath, false, getter_AddRefs(destFile));
      if (NS_FAILED(rv)) {
        return PrintEndDocumentPromise::CreateAndReject(rv, __func__);
      }

      return nsIDeviceContextSpec::EndDocumentAsync(
          __func__,
          [destFile = std::move(destFile),
           spoolFile = std::move(mSpoolFile)]() -> nsresult {
            nsAutoString destLeafName;
            auto rv = destFile->GetLeafName(destLeafName);
            NS_ENSURE_SUCCESS(rv, rv);

            nsCOMPtr<nsIFile> destDir;
            rv = destFile->GetParent(getter_AddRefs(destDir));
            NS_ENSURE_SUCCESS(rv, rv);

            rv = spoolFile->MoveTo(destDir, destLeafName);
            NS_ENSURE_SUCCESS(rv, rv);

            // This is the standard way to get the UNIX umask. Ugh.
            mode_t mask = umask(0);
            umask(mask);
            // If you're not familiar with umasks, they contain the bits of what
            // NOT to set in the permissions (thats because files and
            // directories have different numbers of bits for their permissions)
            destFile->SetPermissions(0666 & ~(mask));
            return NS_OK;
          });

      break;
    }
    case nsIPrintSettings::kOutputDestinationStream:
      // Nothing to do, handled in MakePrintTarget.
      MOZ_ASSERT(!mSpoolFile);
      break;
  }
  return PrintEndDocumentPromise::CreateAndResolve(true, __func__);
}
