/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrinterCUPS_h___
#define nsPrinterCUPS_h___

#include "nsPrinterBase.h"
#include "nsPrintSettingsImpl.h"
#include "nsCUPSShim.h"
#include "nsString.h"

#include "mozilla/DataMutex.h"
#include "mozilla/FunctionRef.h"
#include "mozilla/RecursiveMutex.h"

/**
 * @brief Interface to help implementing nsIPrinter using a CUPS printer.
 */
class nsPrinterCUPS final : public nsPrinterBase {
 public:
  NS_IMETHOD GetName(nsAString& aName) override;
  NS_IMETHOD GetSystemName(nsAString& aName) override;
  bool SupportsDuplex() const final;
  bool SupportsColor() const final;
  bool SupportsMonochrome() const final;
  bool SupportsCollation() const final;
  PrinterInfo CreatePrinterInfo() const final;
  MarginDouble GetMarginsForPaper(nsString aPaperId) const final {
    MOZ_ASSERT_UNREACHABLE(
        "The CUPS API requires us to always get the margin when fetching the "
        "paper list so there should be no need to query it separately");
    return {};
  }

  nsPrinterCUPS() = delete;

  nsPrinterCUPS(const mozilla::CommonPaperInfoArray* aArray,
                const nsCUPSShim& aShim, nsString aDisplayName,
                cups_dest_t* aPrinter)
      : nsPrinterBase(aArray),
        mShim(aShim),
        mDisplayName(std::move(aDisplayName)),
        mPrinter(aPrinter),
        mPrinterInfoMutex("nsPrinterCUPS::mPrinterInfoMutex") {}

  static void ForEachExtraMonochromeSetting(
      mozilla::FunctionRef<void(const nsACString&, const nsACString&)>);

 private:
  struct CUPSPrinterInfo {
    cups_dinfo_t* mPrinterInfo = nullptr;
    uint64_t mCUPSMajor = 0;
    uint64_t mCUPSMinor = 0;
    uint64_t mCUPSPatch = 0;

    // Whether we have attempted to fetch mPrinterInfo with CUPS_HTTP_DEFAULT.
    bool mTriedInitWithDefault = false;
    // Whether we have attempted to fetch mPrinterInfo with a connection.
    bool mTriedInitWithConnection = false;
    CUPSPrinterInfo() = default;
    CUPSPrinterInfo(const CUPSPrinterInfo&) = delete;
    CUPSPrinterInfo(CUPSPrinterInfo&&) = delete;
  };

  using PrinterInfoMutex =
      mozilla::DataMutexBase<CUPSPrinterInfo, mozilla::RecursiveMutex>;

  using PrinterInfoLock = PrinterInfoMutex::AutoLock;

  ~nsPrinterCUPS();

  /**
   * Retrieves the localized name for a given media (paper).
   * Returns nullptr if the name cannot be localized.
   */
  const char* LocalizeMediaName(http_t& aConnection, cups_size_t& aMedia) const;

  void GetPrinterName(nsAString& aName) const;

  // Little util for getting support flags using the direct CUPS names.
  bool Supports(const char* aOption, const char* aValue) const;

  // Returns support value if CUPS meets the minimum version, otherwise returns
  // |aDefault|
  bool IsCUPSVersionAtLeast(uint64_t aCUPSMajor, uint64_t aCUPSMinor,
                            uint64_t aCUPSPatch) const;

  class Connection {
   public:
    http_t* GetConnection(cups_dest_t* aDest);

    inline explicit Connection(const nsCUPSShim& aShim) : mShim(aShim) {}
    Connection() = delete;
    ~Connection();

   protected:
    const nsCUPSShim& mShim;
    http_t* mConnection = CUPS_HTTP_DEFAULT;
    bool mWasInited = false;
  };

  PrintSettingsInitializer DefaultSettings(Connection& aConnection) const;
  nsTArray<mozilla::PaperInfo> PaperList(Connection& aConnection) const;
  /**
   * Attempts to populate the CUPSPrinterInfo object.
   * This usually works with the CUPS default connection,
   * but has been known to require an established connection
   * on older versions of Ubuntu (18 and below).
   */
  PrinterInfoLock TryEnsurePrinterInfo(
      http_t* const aConnection = CUPS_HTTP_DEFAULT) const;

  const nsCUPSShim& mShim;
  nsString mDisplayName;
  cups_dest_t* mPrinter;
  mutable PrinterInfoMutex mPrinterInfoMutex;
};

// There's no standard setting in Core Printing for monochrome. Or rather,
// there is (PMSetColorMode) but it does nothing. Similarly, the relevant gtk
// setting only works on Windows, yay.
//
// So on CUPS the right setting to use depends on the print driver. So we set /
// look for a variety of driver-specific keys that are known to work across
// printers.
//
// We set all the known settings, because the alternative to that is parsing ppd
// files from the printer and find the relevant known choices that can apply,
// and that is a lot more complex, similarly sketchy (requires the same amount
// of driver-specific knowledge), and requires using deprecated CUPS APIs.
#define CUPS_EACH_MONOCHROME_PRINTER_SETTING(macro_)                           \
  macro_("ColorModel", "Gray")                    /* Generic */                \
      macro_("BRMonoColor", "Mono")               /* Brother */                \
      macro_("BRPrintQuality", "Black")           /* Brother */                \
      macro_("CNIJGrayScale", "1")                /* Canon */                  \
      macro_("INK", "MONO")                       /* Epson */                  \
      macro_("HPColorMode", "GrayscalePrint")     /* HP */                     \
      macro_("ColorMode", "Mono")                 /* Samsung */                \
      macro_("PrintoutMode", "Normal.Gray")       /* Foomatic */               \
      macro_("ProcessColorModel", "Mono")         /* Samsung */                \
      macro_("ARCMode", "CMBW")                   /* Sharp */                  \
      macro_("XRXColor", "BW")                    /* Xerox */                  \
      macro_("XROutputColor", "PrintAsGrayscale") /* Xerox, bug 1676191#c32 */ \
      macro_("SelectColor", "Grayscale")          /* Konica Minolta */         \
      macro_("OKControl", "Gray")                 /* Oki */                    \
      macro_("BLW", "TrueM")                      /* Lexmark */                \
      macro_("EPRendering", "None")               /* Epson */

#endif /* nsPrinterCUPS_h___ */
