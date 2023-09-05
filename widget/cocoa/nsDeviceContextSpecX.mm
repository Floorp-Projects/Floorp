/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeviceContextSpecX.h"

#import <Cocoa/Cocoa.h>
#include "mozilla/gfx/PrintPromise.h"
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

#include "AppleUtils.h"
#include "nsCocoaUtils.h"
#include "nsCRT.h"
#include "nsCUPSShim.h"
#include "nsDirectoryServiceDefs.h"
#include "nsILocalFileMac.h"
#include "nsIOutputStream.h"
#include "nsPaper.h"
#include "nsPrinterListCUPS.h"
#include "nsPrintSettingsX.h"
#include "nsQueryObject.h"
#include "prenv.h"

// This must be the last include:
#include "nsObjCExceptions.h"

using namespace mozilla;
using mozilla::gfx::IntSize;
using mozilla::gfx::PrintEndDocumentPromise;
using mozilla::gfx::PrintTarget;
using mozilla::gfx::PrintTargetCG;
#ifdef MOZ_ENABLE_SKIA_PDF
using mozilla::gfx::PrintTargetSkPDF;
#endif

//----------------------------------------------------------------------
// nsDeviceContentSpecX

nsDeviceContextSpecX::nsDeviceContextSpecX() = default;

nsDeviceContextSpecX::~nsDeviceContextSpecX() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (mPrintSession) {
    ::PMRelease(mPrintSession);
  }
  if (mPageFormat) {
    ::PMRelease(mPageFormat);
  }
  if (mPMPrintSettings) {
    ::PMRelease(mPMPrintSettings);
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

NS_IMPL_ISUPPORTS(nsDeviceContextSpecX, nsIDeviceContextSpec)

NS_IMETHODIMP nsDeviceContextSpecX::Init(nsIPrintSettings* aPS,
                                         bool aIsPrintPreview) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  RefPtr<nsPrintSettingsX> settings(do_QueryObject(aPS));
  if (!settings) {
    return NS_ERROR_NO_INTERFACE;
  }
  // Note: unlike other platforms, we don't set our base class's mPrintSettings
  // here since we don't need it currently (we do set mPMPrintSettings below).

  NSPrintInfo* printInfo = settings->CreateOrCopyPrintInfo();
  if (!printInfo) {
    return NS_ERROR_FAILURE;
  }
  if (aPS->GetOutputDestination() ==
      nsIPrintSettings::kOutputDestinationStream) {
    aPS->GetOutputStream(getter_AddRefs(mOutputStream));
    if (!mOutputStream) {
      return NS_ERROR_FAILURE;
    }
  }
  mPrintSession = static_cast<PMPrintSession>([printInfo PMPrintSession]);
  mPageFormat = static_cast<PMPageFormat>([printInfo PMPageFormat]);
  mPMPrintSettings = static_cast<PMPrintSettings>([printInfo PMPrintSettings]);
  MOZ_ASSERT(mPrintSession && mPageFormat && mPMPrintSettings);
  ::PMRetain(mPrintSession);
  ::PMRetain(mPageFormat);
  ::PMRetain(mPMPrintSettings);
  [printInfo release];

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
    // XXX Could PMWorkflowSubmitPDFWithSettings/PMPrinterPrintWithProvider
    // help?
    OSStatus status = noErr;
    PMDestinationType destination;
    status = ::PMSessionGetDestinationType(mPrintSession, mPMPrintSettings,
                                           &destination);
    if (status == noErr) {
      if (destination == kPMDestinationPrinter ||
          destination == kPMDestinationPreview) {
        mPrintViaSkPDF = true;
      } else if (destination == kPMDestinationFile) {
        AutoCFRelease<CFURLRef> destURL(nullptr);
        status = ::PMSessionCopyDestinationLocation(
            mPrintSession, mPMPrintSettings, destURL.receive());
        if (status == noErr) {
          AutoCFRelease<CFStringRef> destPathRef =
              CFURLCopyFileSystemPath(destURL, kCFURLPOSIXPathStyle);
          NSString* destPath = (NSString*)CFStringRef(destPathRef);
          NSString* destPathExt = [destPath pathExtension];
          if ([destPathExt isEqualToString:@"pdf"]) {
            mPrintViaSkPDF = true;
          }
        }
      }
    }
  }
#endif

  int16_t outputFormat = aPS->GetOutputFormat();

  if (outputFormat == nsIPrintSettings::kOutputFormatPDF) {
    // We don't actually currently support/use kOutputFormatPDF on mac, but
    // this is for completeness in case we add that (we probably need to in
    // order to support adding links into saved PDFs, for example).
    Telemetry::ScalarAdd(Telemetry::ScalarID::PRINTING_TARGET_TYPE,
                         u"pdf_file"_ns, 1);
  } else {
    PMDestinationType destination;
    OSStatus status = ::PMSessionGetDestinationType(
        mPrintSession, mPMPrintSettings, &destination);
    if (status == noErr && (destination == kPMDestinationFile ||
                            destination == kPMDestinationPreview ||
                            destination == kPMDestinationProcessPDF)) {
      Telemetry::ScalarAdd(Telemetry::ScalarID::PRINTING_TARGET_TYPE,
                           u"pdf_file"_ns, 1);
    } else {
      Telemetry::ScalarAdd(Telemetry::ScalarID::PRINTING_TARGET_TYPE,
                           u"unknown"_ns, 1);
    }
  }

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

NS_IMETHODIMP nsDeviceContextSpecX::BeginDocument(
    const nsAString& aTitle, const nsAString& aPrintToFileName,
    int32_t aStartPage, int32_t aEndPage) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

RefPtr<PrintEndDocumentPromise> nsDeviceContextSpecX::EndDocument() {
  return nsIDeviceContextSpec::EndDocumentPromiseFromResult(DoEndDocument(),
                                                            __func__);
}

nsresult nsDeviceContextSpecX::DoEndDocument() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

#ifdef MOZ_ENABLE_SKIA_PDF
  if (mPrintViaSkPDF) {
    OSStatus status = noErr;

    nsCOMPtr<nsILocalFileMac> tmpPDFFile = do_QueryInterface(mTempFile);
    if (!tmpPDFFile) {
      return NS_ERROR_FAILURE;
    }
    AutoCFRelease<CFURLRef> pdfURL(nullptr);
    // Note that the caller is responsible to release pdfURL according to
    // nsILocalFileMac.idl, even though we didn't follow the Core Foundation
    // naming conventions here (the method should've been called CopyCFURL).
    nsresult rv = tmpPDFFile->GetCFURL(pdfURL.receive());
    NS_ENSURE_SUCCESS(rv, rv);

    PMDestinationType destination;
    status = ::PMSessionGetDestinationType(mPrintSession, mPMPrintSettings,
                                           &destination);

    switch (destination) {
      case kPMDestinationPrinter: {
        PMPrinter currentPrinter = NULL;
        status = ::PMSessionGetCurrentPrinter(mPrintSession, &currentPrinter);
        if (status != noErr) {
          return NS_ERROR_FAILURE;
        }
        CFStringRef mimeType = CFSTR("application/pdf");
        status = ::PMPrinterPrintWithFile(currentPrinter, mPMPrintSettings,
                                          mPageFormat, mimeType, pdfURL);
        break;
      }
      case kPMDestinationPreview: {
        // XXXjwatt Or should we use CocoaFileUtils::RevealFileInFinder(pdfURL);
        AutoCFRelease<CFStringRef> pdfPath =
            CFURLCopyFileSystemPath(pdfURL, kCFURLPOSIXPathStyle);
        NSString* path = (NSString*)CFStringRef(pdfPath);
        NSWorkspace* ws = [NSWorkspace sharedWorkspace];
        [ws openFile:path];
        break;
      }
      case kPMDestinationFile: {
        AutoCFRelease<CFURLRef> destURL(nullptr);
        status = ::PMSessionCopyDestinationLocation(
            mPrintSession, mPMPrintSettings, destURL.receive());
        if (status == noErr) {
          AutoCFRelease<CFStringRef> sourcePathRef =
              CFURLCopyFileSystemPath(pdfURL, kCFURLPOSIXPathStyle);
          NSString* sourcePath = (NSString*)CFStringRef(sourcePathRef);
#  ifdef DEBUG
          AutoCFRelease<CFStringRef> destPathRef =
              CFURLCopyFileSystemPath(destURL, kCFURLPOSIXPathStyle);
          NSString* destPath = (NSString*)CFStringRef(destPathRef);
          NSString* destPathExt = [destPath pathExtension];
          MOZ_ASSERT([destPathExt isEqualToString:@"pdf"],
                     "nsDeviceContextSpecX::Init only allows '.pdf' for now");
          // We could use /usr/sbin/cupsfilter to convert the PDF to PS, but
          // currently we don't.
#  endif
          NSFileManager* fileManager = [NSFileManager defaultManager];
          if ([fileManager fileExistsAtPath:sourcePath]) {
            NSURL* src = static_cast<NSURL*>(CFURLRef(pdfURL));
            NSURL* dest = static_cast<NSURL*>(CFURLRef(destURL));
            bool ok = [fileManager
                replaceItemAtURL:dest
                   withItemAtURL:src
                  backupItemName:nil
                         options:
                             NSFileManagerItemReplacementUsingNewMetadataOnly
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

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

void nsDeviceContextSpecX::GetPaperRect(double* aTop, double* aLeft,
                                        double* aBottom, double* aRight) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  PMRect paperRect;
  ::PMGetAdjustedPaperRect(mPageFormat, &paperRect);

  *aTop = paperRect.top;
  *aLeft = paperRect.left;
  *aBottom = paperRect.bottom;
  *aRight = paperRect.right;

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

already_AddRefed<PrintTarget> nsDeviceContextSpecX::MakePrintTarget() {
  double top, left, bottom, right;
  GetPaperRect(&top, &left, &bottom, &right);
  const double width = right - left;
  const double height = bottom - top;
  IntSize size = IntSize::Ceil(width, height);

#ifdef MOZ_ENABLE_SKIA_PDF
  if (mPrintViaSkPDF) {
    // TODO: Add support for stream printing via SkPDF if we enable that again.
    nsresult rv =
        NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(mTempFile));
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

  return PrintTargetCG::CreateOrNull(mOutputStream, mPrintSession, mPageFormat,
                                     mPMPrintSettings, size);
}
