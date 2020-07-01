/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeviceContextSpecX.h"

#import <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>
#include <unistd.h>

#ifdef MOZ_ENABLE_SKIA_PDF
#  include "mozilla/gfx/PrintTargetSkPDF.h"
#endif
#include "mozilla/gfx/PrintTargetCG.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Telemetry.h"

#include "nsCocoaUtils.h"
#include "nsCRT.h"
#include "nsDirectoryServiceDefs.h"
#include "nsILocalFileMac.h"
#include "nsPrintSettingsX.h"
#include "nsQueryObject.h"
#include "nsStringEnumerator.h"
#include "prenv.h"

// This must be the last include:
#include "nsObjCExceptions.h"

using namespace mozilla;
using mozilla::gfx::IntSize;
using mozilla::gfx::PrintTarget;
using mozilla::gfx::PrintTargetCG;
#ifdef MOZ_ENABLE_SKIA_PDF
using mozilla::gfx::PrintTargetSkPDF;
#endif
using mozilla::gfx::SurfaceFormat;

static LazyLogModule sDeviceContextSpecXLog("DeviceContextSpecX");
// Macro to make lines shorter
#define DO_PR_DEBUG_LOG(x) MOZ_LOG(sDeviceContextSpecXLog, mozilla::LogLevel::Debug, x)

//----------------------------------------------------------------------
// nsPrinterErnumeratorX

NS_IMPL_ISUPPORTS(nsPrinterEnumeratorX, nsIPrinterEnumerator);

NS_IMETHODIMP nsPrinterEnumeratorX::GetPrinterNameList(nsIStringEnumerator** aPrinterNameList) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ENSURE_ARG_POINTER(aPrinterNameList);
  *aPrinterNameList = nullptr;

  NSArray<NSString*>* printerNames = [NSPrinter printerNames];
  size_t nameCount = [printerNames count];
  nsTArray<nsString>* printerNameList = new nsTArray<nsString>(nameCount);

  for (size_t i = 0; i < nameCount; ++i) {
    NSString* name = [printerNames objectAtIndex:i];
    nsAutoString nsName;
    nsCocoaUtils::GetStringForNSString(name, nsName);
    printerNameList->AppendElement(nsName);
  }

  // If there are no printers available after all checks, return an error
  if (printerNameList->IsEmpty()) {
    delete printerNameList;
    return NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE;
  }

  return NS_NewAdoptingStringEnumerator(aPrinterNameList, printerNameList);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsPrinterEnumeratorX::GetDefaultPrinterName(nsAString& aDefaultPrinterName) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  DO_PR_DEBUG_LOG(("nsPrinterEnumeratorX::GetDefaultPrinterName()\n"));

  aDefaultPrinterName.Truncate();
  NSArray<NSString*>* printerNames = [NSPrinter printerNames];
  if ([printerNames count] > 0) {
    NSString* name = [printerNames objectAtIndex:0];
    nsCocoaUtils::GetStringForNSString(name, aDefaultPrinterName);
  }

  DO_PR_DEBUG_LOG(("GetDefaultPrinterName(): default printer='%s'.\n",
                   NS_ConvertUTF16toUTF8(aDefaultPrinterName).get()));
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsPrinterEnumeratorX::InitPrintSettingsFromPrinter(const nsAString& aPrinterName,
                                                   nsIPrintSettings* aPrintSettings) {
  DO_PR_DEBUG_LOG(("nsPrinterEnumeratorX::InitPrintSettingsFromPrinter()"));

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
      CopyUTF8toUTF16(MakeStringSpan(path), filename);
      filename.AppendLiteral("/mozilla.pdf");
    } else {
      filename.AssignLiteral("mozilla.pdf");
    }

    DO_PR_DEBUG_LOG(("Setting default filename to '%s'\n", NS_ConvertUTF16toUTF8(filename).get()));
    aPrintSettings->SetToFileName(filename);
  }

  aPrintSettings->SetIsInitializedFromPrinter(true);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsDeviceContentSpecX

nsDeviceContextSpecX::nsDeviceContextSpecX()
    : mPrintSession(NULL),
      mPageFormat(kPMNoPageFormat),
      mPrintSettings(kPMNoPrintSettings)
#ifdef MOZ_ENABLE_SKIA_PDF
      ,
      mPrintViaSkPDF(false)
#endif
{
}

nsDeviceContextSpecX::~nsDeviceContextSpecX() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mPrintSession) ::PMRelease(mPrintSession);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

NS_IMPL_ISUPPORTS(nsDeviceContextSpecX, nsIDeviceContextSpec)

NS_IMETHODIMP nsDeviceContextSpecX::Init(nsIWidget* aWidget, nsIPrintSettings* aPS,
                                         bool aIsPrintPreview) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  RefPtr<nsPrintSettingsX> settings(do_QueryObject(aPS));
  if (!settings) return NS_ERROR_NO_INTERFACE;

  bool toFile;
  settings->GetPrintToFile(&toFile);

  bool toPrinter = !toFile && !aIsPrintPreview;
  if (!toPrinter) {
    double width, height;
    settings->GetFilePageSize(&width, &height);
    settings->SetCocoaPaperSize(width, height);
  }

  mPrintSession = settings->GetPMPrintSession();
  ::PMRetain(mPrintSession);
  mPageFormat = settings->GetPMPageFormat();
  mPrintSettings = settings->GetPMPrintSettings();

#ifdef MOZ_ENABLE_SKIA_PDF
  nsAutoString printViaPdf;
  mozilla::Preferences::GetString("print.print_via_pdf_encoder", printViaPdf);
  if (printViaPdf.EqualsLiteral("skia-pdf")) {
    // Annoyingly, PMPrinterPrintWithFile does not pay attention to the
    // kPMDestination* value set in the PMPrintSession; it always sends the PDF
    // to the specified printer.  This means that if we create the PDF using
    // SkPDF then we need to manually handle user actions like "Open PDF in
    // Preview" and "Save as PDF...".
    // TODO: Currently we do not support using SkPDF for kPMDestinationFax or
    // kPMDestinationProcessPDF ("Add PDF to iBooks, etc.), and we only support
    // it for kPMDestinationFile if the destination file is a PDF.
    // XXX Could PMWorkflowSubmitPDFWithSettings/PMPrinterPrintWithProvider help?
    OSStatus status = noErr;
    PMDestinationType destination;
    status = ::PMSessionGetDestinationType(mPrintSession, mPrintSettings, &destination);
    if (status == noErr) {
      if (destination == kPMDestinationPrinter || destination == kPMDestinationPreview) {
        mPrintViaSkPDF = true;
      } else if (destination == kPMDestinationFile) {
        CFURLRef destURL;
        status = ::PMSessionCopyDestinationLocation(mPrintSession, mPrintSettings, &destURL);
        if (status == noErr) {
          CFStringRef destPathRef = CFURLCopyFileSystemPath(destURL, kCFURLPOSIXPathStyle);
          NSString* destPath = (NSString*)destPathRef;
          NSString* destPathExt = [destPath pathExtension];
          if ([destPathExt isEqualToString:@"pdf"]) {
            mPrintViaSkPDF = true;
          }
        }
      }
    }
  }
#endif

  int16_t outputFormat;
  aPS->GetOutputFormat(&outputFormat);

  if (outputFormat == nsIPrintSettings::kOutputFormatPDF) {
    // We don't actually currently support/use kOutputFormatPDF on mac, but
    // this is for completeness in case we add that (we probably need to in
    // order to support adding links into saved PDFs, for example).
    Telemetry::ScalarAdd(Telemetry::ScalarID::PRINTING_TARGET_TYPE, u"pdf_file"_ns, 1);
  } else {
    PMDestinationType destination;
    OSStatus status = ::PMSessionGetDestinationType(mPrintSession, mPrintSettings, &destination);
    if (status == noErr &&
        (destination == kPMDestinationFile || destination == kPMDestinationPreview ||
         destination == kPMDestinationProcessPDF)) {
      Telemetry::ScalarAdd(Telemetry::ScalarID::PRINTING_TARGET_TYPE, u"pdf_file"_ns, 1);
    } else {
      Telemetry::ScalarAdd(Telemetry::ScalarID::PRINTING_TARGET_TYPE, u"unknown"_ns, 1);
    }
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsDeviceContextSpecX::BeginDocument(const nsAString& aTitle,
                                                  const nsAString& aPrintToFileName,
                                                  int32_t aStartPage, int32_t aEndPage) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsDeviceContextSpecX::EndDocument() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

#ifdef MOZ_ENABLE_SKIA_PDF
  if (mPrintViaSkPDF) {
    OSStatus status = noErr;

    nsCOMPtr<nsILocalFileMac> tmpPDFFile = do_QueryInterface(mTempFile);
    if (!tmpPDFFile) {
      return NS_ERROR_FAILURE;
    }
    CFURLRef pdfURL;
    nsresult rv = tmpPDFFile->GetCFURL(&pdfURL);
    NS_ENSURE_SUCCESS(rv, rv);

    PMDestinationType destination;
    status = ::PMSessionGetDestinationType(mPrintSession, mPrintSettings, &destination);

    switch (destination) {
      case kPMDestinationPrinter: {
        PMPrinter currentPrinter = NULL;
        status = ::PMSessionGetCurrentPrinter(mPrintSession, &currentPrinter);
        if (status != noErr) {
          return NS_ERROR_FAILURE;
        }
        CFStringRef mimeType = CFSTR("application/pdf");
        status =
            ::PMPrinterPrintWithFile(currentPrinter, mPrintSettings, mPageFormat, mimeType, pdfURL);
        break;
      }
      case kPMDestinationPreview: {
        // XXXjwatt Or should we use CocoaFileUtils::RevealFileInFinder(pdfURL);
        CFStringRef pdfPath = CFURLCopyFileSystemPath(pdfURL, kCFURLPOSIXPathStyle);
        NSString* path = (NSString*)pdfPath;
        NSWorkspace* ws = [NSWorkspace sharedWorkspace];
        [ws openFile:path];
        break;
      }
      case kPMDestinationFile: {
        CFURLRef destURL;
        status = ::PMSessionCopyDestinationLocation(mPrintSession, mPrintSettings, &destURL);
        if (status == noErr) {
          CFStringRef sourcePathRef = CFURLCopyFileSystemPath(pdfURL, kCFURLPOSIXPathStyle);
          NSString* sourcePath = (NSString*)sourcePathRef;
#  ifdef DEBUG
          CFStringRef destPathRef = CFURLCopyFileSystemPath(destURL, kCFURLPOSIXPathStyle);
          NSString* destPath = (NSString*)destPathRef;
          NSString* destPathExt = [destPath pathExtension];
          MOZ_ASSERT([destPathExt isEqualToString:@"pdf"],
                     "nsDeviceContextSpecX::Init only allows '.pdf' for now");
          // We could use /usr/sbin/cupsfilter to convert the PDF to PS, but
          // currently we don't.
#  endif
          NSFileManager* fileManager = [NSFileManager defaultManager];
          if ([fileManager fileExistsAtPath:sourcePath]) {
            NSURL* src = static_cast<NSURL*>(pdfURL);
            NSURL* dest = static_cast<NSURL*>(destURL);
            bool ok = [fileManager replaceItemAtURL:dest
                                      withItemAtURL:src
                                     backupItemName:nil
                                            options:NSFileManagerItemReplacementUsingNewMetadataOnly
                                   resultingItemURL:nil
                                              error:nil];
            if (!ok) {
              return NS_ERROR_FAILURE;
            }
          }
        }
        break;
      }
      default:
        MOZ_ASSERT_UNREACHABLE("nsDeviceContextSpecX::Init doesn't set "
                               "mPrintViaSkPDF for other values");
    }

    return (status == noErr) ? NS_OK : NS_ERROR_FAILURE;
  }
#endif

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void nsDeviceContextSpecX::GetPaperRect(double* aTop, double* aLeft, double* aBottom,
                                        double* aRight) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  PMRect paperRect;
  ::PMGetAdjustedPaperRect(mPageFormat, &paperRect);

  *aTop = paperRect.top;
  *aLeft = paperRect.left;
  *aBottom = paperRect.bottom;
  *aRight = paperRect.right;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

already_AddRefed<PrintTarget> nsDeviceContextSpecX::MakePrintTarget() {
  double top, left, bottom, right;
  GetPaperRect(&top, &left, &bottom, &right);
  const double width = right - left;
  const double height = bottom - top;
  IntSize size = IntSize::Floor(width, height);

#ifdef MOZ_ENABLE_SKIA_PDF
  if (mPrintViaSkPDF) {
    nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(mTempFile));
    NS_ENSURE_SUCCESS(rv, nullptr);
    nsAutoCString tempPath("tmp-printing.pdf");
    mTempFile->AppendNative(tempPath);
    rv = mTempFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
    NS_ENSURE_SUCCESS(rv, nullptr);
    mTempFile->GetNativePath(tempPath);
    auto stream = MakeUnique<SkFILEWStream>(tempPath.get());
    return PrintTargetSkPDF::CreateOrNull(std::move(stream), size);
  }
#endif

  return PrintTargetCG::CreateOrNull(mPrintSession, mPageFormat, mPrintSettings, size);
}
