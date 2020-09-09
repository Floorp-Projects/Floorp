/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeviceContextSpecWin.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/gfx/PrintTargetPDF.h"
#include "mozilla/gfx/PrintTargetWindows.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Telemetry.h"

#include <wchar.h>
#include <windef.h>
#include <winspool.h>

#include "nsIWidget.h"

#include "nsTArray.h"
#include "nsIPrintSettingsWin.h"

#include "nsPrinterWin.h"
#include "nsReadableUtils.h"
#include "nsString.h"

#include "gfxWindowsSurface.h"

#include "nsIFileStreams.h"
#include "nsWindowsHelpers.h"

#include "mozilla/gfx/Logging.h"

#ifdef MOZ_ENABLE_SKIA_PDF
#  include "mozilla/gfx/PrintTargetSkPDF.h"
#  include "mozilla/gfx/PrintTargetEMF.h"
#  include "nsIUUIDGenerator.h"
#  include "nsDirectoryServiceDefs.h"
#  include "nsPrintfCString.h"
#  include "nsThreadUtils.h"
#endif

static mozilla::LazyLogModule kWidgetPrintingLogMod("printing-widget");
#define PR_PL(_p1) MOZ_LOG(kWidgetPrintingLogMod, mozilla::LogLevel::Debug, _p1)

using namespace mozilla;
using namespace mozilla::gfx;

#ifdef MOZ_ENABLE_SKIA_PDF
using namespace mozilla::widget;
#endif

static const wchar_t kDriverName[] = L"WINSPOOL";

//----------------------------------------------------------------------------------
//---------------
// static members
//----------------------------------------------------------------------------------
nsDeviceContextSpecWin::nsDeviceContextSpecWin()
    : mDevMode(nullptr)
#ifdef MOZ_ENABLE_SKIA_PDF
      ,
      mPrintViaSkPDF(false)
#endif
{
}

//----------------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsDeviceContextSpecWin, nsIDeviceContextSpec)

nsDeviceContextSpecWin::~nsDeviceContextSpecWin() {
  SetDevMode(nullptr);

  if (nsCOMPtr<nsIPrintSettingsWin> ps = do_QueryInterface(mPrintSettings)) {
    ps->SetDeviceName(EmptyString());
    ps->SetDriverName(EmptyString());
    ps->SetDevMode(nullptr);
  }
}

static bool GetDefaultPrinterName(nsAString& aDefaultPrinterName) {
  DWORD length = 0;
  GetDefaultPrinterW(nullptr, &length);

  if (length) {
    aDefaultPrinterName.SetLength(length);
    if (GetDefaultPrinterW((LPWSTR)aDefaultPrinterName.BeginWriting(),
                           &length)) {
      // `length` includes the terminating null, so we subtract that from our
      // string length.
      aDefaultPrinterName.SetLength(length - 1);
      PR_PL(("DEFAULT PRINTER [%s]\n",
             NS_ConvertUTF16toUTF8(aDefaultPrinterName).get()));
      return true;
    }
  }

  aDefaultPrinterName.Truncate();
  PR_PL(("NO DEFAULT PRINTER\n"));
  return false;
}

//----------------------------------------------------------------------------------
NS_IMETHODIMP nsDeviceContextSpecWin::Init(nsIWidget* aWidget,
                                           nsIPrintSettings* aPrintSettings,
                                           bool aIsPrintPreview) {
  mPrintSettings = aPrintSettings;

  // Get the Printer Name to be used and output format.
  nsAutoString printerName;
  if (mPrintSettings) {
    mPrintSettings->GetOutputFormat(&mOutputFormat);
    mPrintSettings->GetPrinterName(printerName);
  }

  // If there is no name then use the default printer
  if (printerName.IsEmpty()) {
    GetDefaultPrinterName(printerName);
  }

  // Gather telemetry on the print target type.
  //
  // Unfortunately, if we're not using our own internal save-to-pdf codepaths,
  // there isn't a good way to determine whether a print is going to be to a
  // physical printer or to a file or some other non-physical output. We do our
  // best by checking for what seems to be the most common save-to-PDF virtual
  // printers.
  //
  // We use StringBeginsWith below, since printer names are often followed by a
  // version number or other product differentiating string.  (True for doPDF,
  // novaPDF, PDF-XChange and Soda PDF, for example.)
  if (mOutputFormat == nsIPrintSettings::kOutputFormatPDF) {
    Telemetry::ScalarAdd(Telemetry::ScalarID::PRINTING_TARGET_TYPE,
                         u"pdf_file"_ns, 1);
  } else if (StringBeginsWith(printerName, u"Microsoft Print to PDF"_ns) ||
             StringBeginsWith(printerName, u"Adobe PDF"_ns) ||
             StringBeginsWith(printerName, u"Bullzip PDF Printer"_ns) ||
             StringBeginsWith(printerName, u"CutePDF Writer"_ns) ||
             StringBeginsWith(printerName, u"doPDF"_ns) ||
             StringBeginsWith(printerName, u"Foxit Reader PDF Printer"_ns) ||
             StringBeginsWith(printerName, u"Nitro PDF Creator"_ns) ||
             StringBeginsWith(printerName, u"novaPDF"_ns) ||
             StringBeginsWith(printerName, u"PDF-XChange"_ns) ||
             StringBeginsWith(printerName, u"PDF24 PDF"_ns) ||
             StringBeginsWith(printerName, u"PDFCreator"_ns) ||
             StringBeginsWith(printerName, u"PrimoPDF"_ns) ||
             StringBeginsWith(printerName, u"Soda PDF"_ns) ||
             StringBeginsWith(printerName, u"Solid PDF Creator"_ns) ||
             StringBeginsWith(printerName,
                              u"Universal Document Converter"_ns)) {
    Telemetry::ScalarAdd(Telemetry::ScalarID::PRINTING_TARGET_TYPE,
                         u"pdf_file"_ns, 1);
  } else if (printerName.EqualsLiteral("Microsoft XPS Document Writer")) {
    Telemetry::ScalarAdd(Telemetry::ScalarID::PRINTING_TARGET_TYPE,
                         u"xps_file"_ns, 1);
  } else {
    nsAString::const_iterator start, end;
    printerName.BeginReading(start);
    printerName.EndReading(end);
    if (CaseInsensitiveFindInReadable(u"pdf"_ns, start, end)) {
      Telemetry::ScalarAdd(Telemetry::ScalarID::PRINTING_TARGET_TYPE,
                           u"pdf_unknown"_ns, 1);
    } else {
      Telemetry::ScalarAdd(Telemetry::ScalarID::PRINTING_TARGET_TYPE,
                           u"unknown"_ns, 1);
    }
  }

  nsresult rv = NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE;
  if (aPrintSettings) {
#ifdef MOZ_ENABLE_SKIA_PDF
    nsAutoString printViaPdf;
    Preferences::GetString("print.print_via_pdf_encoder", printViaPdf);
    if (printViaPdf.EqualsLiteral("skia-pdf")) {
      mPrintViaSkPDF = true;
    }
#endif

    // If we're in the child and printing via the parent or we're printing to
    // PDF we only need information from the print settings.
    if ((XRE_IsContentProcess() &&
         Preferences::GetBool("print.print_via_parent")) ||
        mOutputFormat == nsIPrintSettings::kOutputFormatPDF) {
      return NS_OK;
    }

    nsCOMPtr<nsIPrintSettingsWin> psWin(do_QueryInterface(aPrintSettings));
    if (psWin) {
      nsAutoString deviceName;
      nsAutoString driverName;
      psWin->GetDeviceName(deviceName);
      psWin->GetDriverName(driverName);

      LPDEVMODEW devMode;
      psWin->GetDevMode(&devMode);  // creates new memory (makes a copy)

      if (!deviceName.IsEmpty() && !driverName.IsEmpty() && devMode) {
        // Scaling is special, it is one of the few
        // devMode items that we control in layout
        if (devMode->dmFields & DM_SCALE) {
          double scale = double(devMode->dmScale) / 100.0f;
          if (scale != 1.0) {
            aPrintSettings->SetScaling(scale);
            devMode->dmScale = 100;
          }
        }

        SetDeviceName(deviceName);
        SetDriverName(driverName);
        SetDevMode(devMode);

        return NS_OK;
      } else {
        PR_PL(
            ("***** nsDeviceContextSpecWin::Init - "
             "deviceName/driverName/devMode was NULL!\n"));
        if (devMode) ::HeapFree(::GetProcessHeap(), 0, devMode);
      }
    }
  } else {
    PR_PL(("***** nsDeviceContextSpecWin::Init - aPrintSettingswas NULL!\n"));
  }

  if (printerName.IsEmpty()) {
    return rv;
  }

  return GetDataFromPrinter(printerName, mPrintSettings);
}

//----------------------------------------------------------

already_AddRefed<PrintTarget> nsDeviceContextSpecWin::MakePrintTarget() {
  NS_ASSERTION(mDevMode || mOutputFormat == nsIPrintSettings::kOutputFormatPDF,
               "DevMode can't be NULL here unless we're printing to PDF.");

#ifdef MOZ_ENABLE_SKIA_PDF
  if (mPrintViaSkPDF) {
    double width, height;
    mPrintSettings->GetEffectivePageSize(&width, &height);
    if (width <= 0 || height <= 0) {
      return nullptr;
    }

    // convert twips to points
    width /= TWIPS_PER_POINT_FLOAT;
    height /= TWIPS_PER_POINT_FLOAT;
    IntSize size = IntSize::Truncate(width, height);

    if (mOutputFormat == nsIPrintSettings::kOutputFormatPDF) {
      nsString filename;
      mPrintSettings->GetToFileName(filename);

      nsAutoCString printFile(NS_ConvertUTF16toUTF8(filename).get());
      auto skStream = MakeUnique<SkFILEWStream>(printFile.get());
      return PrintTargetSkPDF::CreateOrNull(std::move(skStream), size);
    }

    if (mDevMode) {
      NS_WARNING_ASSERTION(!mDriverName.IsEmpty(), "No driver!");
      HDC dc =
          ::CreateDCW(mDriverName.get(), mDeviceName.get(), nullptr, mDevMode);
      if (!dc) {
        gfxCriticalError(gfxCriticalError::DefaultOptions(false))
            << "Failed to create device context in GetSurfaceForPrinter";
        return nullptr;
      }
      return PrintTargetEMF::CreateOrNull(dc, size);
    }
  }
#endif

  if (mOutputFormat == nsIPrintSettings::kOutputFormatPDF) {
    nsString filename;
    mPrintSettings->GetToFileName(filename);

    double width, height;
    mPrintSettings->GetEffectivePageSize(&width, &height);
    if (width <= 0 || height <= 0) {
      return nullptr;
    }

    // convert twips to points
    width /= TWIPS_PER_POINT_FLOAT;
    height /= TWIPS_PER_POINT_FLOAT;

    nsCOMPtr<nsIFile> file = do_CreateInstance("@mozilla.org/file/local;1");
    nsresult rv = file->InitWithPath(filename);
    if (NS_FAILED(rv)) {
      return nullptr;
    }

    nsCOMPtr<nsIFileOutputStream> stream =
        do_CreateInstance("@mozilla.org/network/file-output-stream;1");
    rv = stream->Init(file, -1, -1, 0);
    if (NS_FAILED(rv)) {
      return nullptr;
    }

    return PrintTargetPDF::CreateOrNull(stream,
                                        IntSize::Truncate(width, height));
  }

  if (mDevMode) {
    NS_WARNING_ASSERTION(!mDriverName.IsEmpty(), "No driver!");
    HDC dc =
        ::CreateDCW(mDriverName.get(), mDeviceName.get(), nullptr, mDevMode);
    if (!dc) {
      gfxCriticalError(gfxCriticalError::DefaultOptions(false))
          << "Failed to create device context in GetSurfaceForPrinter";
      return nullptr;
    }

    // The PrintTargetWindows takes over ownership of this DC
    return PrintTargetWindows::CreateOrNull(dc);
  }

  return nullptr;
}

float nsDeviceContextSpecWin::GetDPI() {
  // To match the previous printing code we need to return 72 when printing to
  // PDF and 144 when printing to a Windows surface.
#ifdef MOZ_ENABLE_SKIA_PDF
  if (mPrintViaSkPDF) {
    return 72.0f;
  }
#endif
  return mOutputFormat == nsIPrintSettings::kOutputFormatPDF ? 72.0f : 144.0f;
}

float nsDeviceContextSpecWin::GetPrintingScale() {
  MOZ_ASSERT(mPrintSettings);
#ifdef MOZ_ENABLE_SKIA_PDF
  if (mPrintViaSkPDF) {
    return 1.0f;  // PDF is vector based, so we don't need a scale
  }
#endif
  // To match the previous printing code there is no scaling for PDF.
  if (mOutputFormat == nsIPrintSettings::kOutputFormatPDF) {
    return 1.0f;
  }

  // The print settings will have the resolution stored from the real device.
  int32_t resolution;
  mPrintSettings->GetResolution(&resolution);
  return float(resolution) / GetDPI();
}

gfxPoint nsDeviceContextSpecWin::GetPrintingTranslate() {
  // The underlying surface on windows is the size of the printable region. When
  // the region is smaller than the actual paper size the (0, 0) coordinate
  // refers top-left of that unwritable region. To instead have (0, 0) become
  // the top-left of the actual paper, translate it's coordinate system by the
  // unprintable region's width.
  double marginTop, marginLeft;
  mPrintSettings->GetUnwriteableMarginTop(&marginTop);
  mPrintSettings->GetUnwriteableMarginLeft(&marginLeft);
  int32_t resolution;
  mPrintSettings->GetResolution(&resolution);
  return gfxPoint(-marginLeft * resolution, -marginTop * resolution);
}

//----------------------------------------------------------------------------------
void nsDeviceContextSpecWin::SetDeviceName(const nsAString& aDeviceName) {
  mDeviceName = aDeviceName;
}

//----------------------------------------------------------------------------------
void nsDeviceContextSpecWin::SetDriverName(const nsAString& aDriverName) {
  mDriverName = aDriverName;
}

//----------------------------------------------------------------------------------
void nsDeviceContextSpecWin::SetDevMode(LPDEVMODEW aDevMode) {
  if (mDevMode) {
    ::HeapFree(::GetProcessHeap(), 0, mDevMode);
  }

  mDevMode = aDevMode;
}

//------------------------------------------------------------------
void nsDeviceContextSpecWin::GetDevMode(LPDEVMODEW& aDevMode) {
  aDevMode = mDevMode;
}

#define DISPLAY_LAST_ERROR

//----------------------------------------------------------------------------------
// Setup the object's data member with the selected printer's data
nsresult nsDeviceContextSpecWin::GetDataFromPrinter(const nsAString& aName,
                                                    nsIPrintSettings* aPS) {
  nsresult rv = NS_ERROR_FAILURE;

  nsHPRINTER hPrinter = nullptr;
  const nsString& flat = PromiseFlatString(aName);
  wchar_t* name =
      (wchar_t*)flat.get();  // Windows APIs use non-const name argument

  BOOL status = ::OpenPrinterW(name, &hPrinter, nullptr);
  if (status) {
    nsAutoPrinter autoPrinter(hPrinter);

    LPDEVMODEW pDevMode;

    // Allocate a buffer of the correct size.
    LONG needed =
        ::DocumentPropertiesW(nullptr, hPrinter, name, nullptr, nullptr, 0);
    if (needed < 0) {
      PR_PL(
          ("**** nsDeviceContextSpecWin::GetDataFromPrinter - Couldn't get "
           "size of DEVMODE using DocumentPropertiesW(pDeviceName = \"%s\"). "
           "GetLastEror() = %08x\n",
           NS_ConvertUTF16toUTF8(aName).get(), GetLastError()));
      return NS_ERROR_FAILURE;
    }

    pDevMode =
        (LPDEVMODEW)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, needed);
    if (!pDevMode) return NS_ERROR_FAILURE;

    // Get the default DevMode for the printer and modify it for our needs.
    LONG ret = ::DocumentPropertiesW(nullptr, hPrinter, name, pDevMode, nullptr,
                                     DM_OUT_BUFFER);

    if (ret == IDOK && aPS) {
      nsCOMPtr<nsIPrintSettingsWin> psWin = do_QueryInterface(aPS);
      MOZ_ASSERT(psWin);
      psWin->CopyToNative(pDevMode);
      // Sets back the changes we made to the DevMode into the Printer Driver
      ret = ::DocumentPropertiesW(nullptr, hPrinter, name, pDevMode, pDevMode,
                                  DM_IN_BUFFER | DM_OUT_BUFFER);

      // We need to copy the final DEVMODE settings back to our print settings,
      // because they may have been set from invalid prefs.
      if (ret == IDOK) {
        // We need to get information from the device as well.
        nsAutoHDC printerDC(::CreateICW(kDriverName, name, nullptr, pDevMode));
        if (NS_WARN_IF(!printerDC)) {
          ::HeapFree(::GetProcessHeap(), 0, pDevMode);
          return NS_ERROR_FAILURE;
        }

        psWin->CopyFromNative(printerDC, pDevMode);
      }
    }

    if (ret != IDOK) {
      ::HeapFree(::GetProcessHeap(), 0, pDevMode);
      PR_PL(
          ("***** nsDeviceContextSpecWin::GetDataFromPrinter - "
           "DocumentProperties call failed code: %d/0x%x\n",
           ret, ret));
      DISPLAY_LAST_ERROR
      return NS_ERROR_FAILURE;
    }

    SetDevMode(
        pDevMode);  // cache the pointer and takes responsibility for the memory

    SetDeviceName(aName);

    SetDriverName(nsDependentString(kDriverName));

    rv = NS_OK;
  } else {
    rv = NS_ERROR_GFX_PRINTER_NAME_NOT_FOUND;
    PR_PL(
        ("***** nsDeviceContextSpecWin::GetDataFromPrinter - Couldn't open "
         "printer: [%s]\n",
         NS_ConvertUTF16toUTF8(aName).get()));
    DISPLAY_LAST_ERROR
  }
  return rv;
}

//***********************************************************
//  Printer List
//***********************************************************

nsPrinterListWin::~nsPrinterListWin() = default;

// Helper to get the array of PRINTER_INFO_4 records from the OS into a
// caller-supplied byte array; returns the number of records present.
static unsigned GetPrinterInfo4(nsTArray<BYTE>& aBuffer) {
  const DWORD kLevel = 4;
  DWORD needed = 0;
  DWORD count = 0;
  const DWORD kFlags = PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS;
  BOOL ok = EnumPrinters(kFlags,
                         nullptr,  // Name
                         kLevel,   // Level
                         nullptr,  // pPrinterEnum
                         0,        // cbBuf (buffer size)
                         &needed,  // Bytes needed in buffer
                         &count);
  if (needed > 0) {
    aBuffer.SetLength(needed);
    ok = EnumPrinters(kFlags, nullptr, kLevel, aBuffer.Elements(),
                      aBuffer.Length(), &needed, &count);
  }
  if (!ok || !count) {
    return 0;
  }
  return count;
}

nsTArray<nsPrinterListBase::PrinterInfo> nsPrinterListWin::Printers() const {
  PR_PL(("nsPrinterListWin::Printers\n"));

  AutoTArray<BYTE, 1024> buffer;
  unsigned count = GetPrinterInfo4(buffer);

  if (!count) {
    PR_PL(("[No printers found]\n"));
    return {};
  }

  const auto* printers =
      reinterpret_cast<const PRINTER_INFO_4*>(buffer.Elements());
  nsTArray<PrinterInfo> list;
  for (unsigned i = 0; i < count; i++) {
    list.AppendElement(PrinterInfo{nsString(printers[i].pPrinterName)});
    PR_PL(("Printer Name: %s\n",
           NS_ConvertUTF16toUTF8(printers[i].pPrinterName).get()));
  }

  return list;
}

Maybe<nsPrinterListBase::PrinterInfo> nsPrinterListWin::NamedPrinter(
    nsString aName) const {
  Maybe<PrinterInfo> rv;

  AutoTArray<BYTE, 1024> buffer;
  unsigned count = GetPrinterInfo4(buffer);

  const auto* printers =
      reinterpret_cast<const PRINTER_INFO_4*>(buffer.Elements());
  for (unsigned i = 0; i < count; ++i) {
    if (aName.Equals(nsString(printers[i].pPrinterName))) {
      rv.emplace(PrinterInfo{aName});
      break;
    }
  }

  return rv;
}

RefPtr<nsIPrinter> nsPrinterListWin::CreatePrinter(PrinterInfo aInfo) const {
  return nsPrinterWin::Create(std::move(aInfo.mName));
}

nsresult nsPrinterListWin::SystemDefaultPrinterName(nsAString& aName) const {
  if (GetDefaultPrinterName(aName)) {
    return NS_OK;
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsPrinterListWin::InitPrintSettingsFromPrinter(
    const nsAString& aPrinterName, nsIPrintSettings* aPrintSettings) {
  NS_ENSURE_ARG_POINTER(aPrintSettings);

  if (aPrinterName.IsEmpty()) {
    return NS_OK;
  }

  // When printing to PDF on Windows there is no associated printer driver.
  int16_t outputFormat;
  aPrintSettings->GetOutputFormat(&outputFormat);
  if (outputFormat == nsIPrintSettings::kOutputFormatPDF) {
    return NS_OK;
  }

  RefPtr<nsDeviceContextSpecWin> devSpecWin = new nsDeviceContextSpecWin();
  if (!devSpecWin) return NS_ERROR_OUT_OF_MEMORY;

  // If the settings have already been initialized from prefs then pass these to
  // GetDataFromPrinter, so that they are saved to the printer.
  bool initializedFromPrefs;
  nsresult rv =
      aPrintSettings->GetIsInitializedFromPrefs(&initializedFromPrefs);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (initializedFromPrefs) {
    // If we pass in print settings to GetDataFromPrinter it already copies
    // things back to the settings, so we can return here.
    return devSpecWin->GetDataFromPrinter(aPrinterName, aPrintSettings);
  }

  devSpecWin->GetDataFromPrinter(aPrinterName);

  LPDEVMODEW devmode;
  devSpecWin->GetDevMode(devmode);
  if (NS_WARN_IF(!devmode)) {
    return NS_ERROR_FAILURE;
  }

  aPrintSettings->SetPrinterName(aPrinterName);

  // We need to get information from the device as well.
  const nsString& flat = PromiseFlatString(aPrinterName);
  char16ptr_t printerName = flat.get();
  HDC dc = ::CreateICW(kDriverName, printerName, nullptr, devmode);
  if (NS_WARN_IF(!dc)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPrintSettingsWin> psWin = do_QueryInterface(aPrintSettings);
  MOZ_ASSERT(psWin);
  psWin->CopyFromNative(dc, devmode);
  ::DeleteDC(dc);

  return NS_OK;
}
