/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIDeviceContextSpec_h___
#define nsIDeviceContextSpec_h___

#include "gfxPoint.h"
#include "nsISupports.h"
#include "mozilla/StaticPrefs_print.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/PrintPromise.h"
#include "mozilla/MoveOnlyFunction.h"

class nsIWidget;
class nsIPrintSettings;

namespace mozilla {
namespace gfx {
class DrawEventRecorder;
class PrintTarget;
}  // namespace gfx
}  // namespace mozilla

#define NS_IDEVICE_CONTEXT_SPEC_IID                  \
  {                                                  \
    0xf407cfba, 0xbe28, 0x46c9, {                    \
      0x8a, 0xba, 0x04, 0x2d, 0xae, 0xbb, 0x4f, 0x23 \
    }                                                \
  }

class nsIDeviceContextSpec : public nsISupports {
 public:
  typedef mozilla::gfx::PrintTarget PrintTarget;
  using IntSize = mozilla::gfx::IntSize;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDEVICE_CONTEXT_SPEC_IID)

  /**
   * Initialize the device context spec.
   * @param aWidget         A widget a dialog can be hosted in
   * @param aPrintSettings  Print settings for the print operation
   * @param aIsPrintPreview True if creating Spec for PrintPreview
   * @return NS_OK or a suitable error code.
   */
  NS_IMETHOD Init(nsIPrintSettings* aPrintSettings, bool aIsPrintPreview) = 0;

  virtual already_AddRefed<PrintTarget> MakePrintTarget() = 0;

  /**
   * If required override to return a recorder to record the print.
   *
   * @param aDrawEventRecorder out param for the recorder to use
   * @return NS_OK or a suitable error code
   */
  NS_IMETHOD GetDrawEventRecorder(
      mozilla::gfx::DrawEventRecorder** aDrawEventRecorder) {
    MOZ_ASSERT(aDrawEventRecorder);
    *aDrawEventRecorder = nullptr;
    return NS_OK;
  }

  /**
   * @return DPI for printing.
   */
  float GetDPI() { return mozilla::StaticPrefs::print_default_dpi(); }

  /**
   * @return the printing scale to be applied to the context for printing.
   */
  float GetPrintingScale();

  /**
   * @return the point to translate the context to for printing.
   */
  gfxPoint GetPrintingTranslate();

  NS_IMETHOD BeginDocument(const nsAString& aTitle,
                           const nsAString& aPrintToFileName,
                           int32_t aStartPage, int32_t aEndPage) = 0;

  virtual RefPtr<mozilla::gfx::PrintEndDocumentPromise> EndDocument() = 0;
  /**
   * Note: not all print devices implement mixed page sizing. Internally,
   * aSizeInPoints gets handed off to a PrintTarget, and most PrintTarget
   * subclasses will ignore `aSizeInPoints`.
   */
  NS_IMETHOD BeginPage(const IntSize& aSizeInPoints) = 0;
  NS_IMETHOD EndPage() = 0;

 protected:
  using AsyncEndDocumentFunction = mozilla::MoveOnlyFunction<nsresult()>;
  static RefPtr<mozilla::gfx::PrintEndDocumentPromise> EndDocumentAsync(
      const char* aCallSite, AsyncEndDocumentFunction aFunction);

  static RefPtr<mozilla::gfx::PrintEndDocumentPromise>
  EndDocumentPromiseFromResult(nsresult aResult, const char* aSite);

  nsCOMPtr<nsIPrintSettings> mPrintSettings;

#ifdef MOZ_ENABLE_SKIA_PDF
  // This variable is independant of nsIPrintSettings::kOutputFormatPDF (i.e.
  // save-to-PDF). If set to true, then even when we print to a printer we
  // output and send it PDF.
  bool mPrintViaSkPDF = false;
#endif
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDeviceContextSpec, NS_IDEVICE_CONTEXT_SPEC_IID)
#endif
