/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeviceContextSpecX.h"

#include "mozilla/gfx/PrintTargetCG.h"
#ifdef MOZ_ENABLE_SKIA_PDF
#include "mozilla/gfx/PrintTargetSkPDF.h"
#endif
#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"
#include "nsCRT.h"
#include "nsDirectoryServiceDefs.h"
#include "nsILocalFileMac.h"
#include <unistd.h>

#include "nsQueryObject.h"
#include "nsIServiceManager.h"
#include "nsPrintSettingsX.h"

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

nsDeviceContextSpecX::nsDeviceContextSpecX()
: mPrintSession(NULL)
, mPageFormat(kPMNoPageFormat)
, mPrintSettings(kPMNoPrintSettings)
#ifdef MOZ_ENABLE_SKIA_PDF
, mPrintViaSkPDF(false)
#endif
{
}

nsDeviceContextSpecX::~nsDeviceContextSpecX()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mPrintSession)
    ::PMRelease(mPrintSession);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

NS_IMPL_ISUPPORTS(nsDeviceContextSpecX, nsIDeviceContextSpec)

NS_IMETHODIMP nsDeviceContextSpecX::Init(nsIWidget *aWidget,
                                         nsIPrintSettings* aPS,
                                         bool aIsPrintPreview)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  RefPtr<nsPrintSettingsX> settings(do_QueryObject(aPS));
  if (!settings)
    return NS_ERROR_NO_INTERFACE;

  bool toFile;
  settings->GetPrintToFile(&toFile);

  bool toPrinter = !toFile && !aIsPrintPreview;
  if (!toPrinter) {
    double width, height;
    settings->GetEffectivePageSize(&width, &height);
    width /= TWIPS_PER_POINT_FLOAT;
    height /= TWIPS_PER_POINT_FLOAT;

    settings->SetCocoaPaperSize(width, height);
  }

  mPrintSession = settings->GetPMPrintSession();
  ::PMRetain(mPrintSession);
  mPageFormat = settings->GetPMPageFormat();
  mPrintSettings = settings->GetPMPrintSettings();

#ifdef MOZ_ENABLE_SKIA_PDF
  const nsAdoptingString& printViaPdf =
    mozilla::Preferences::GetString("print.print_via_pdf_encoder");
  if (printViaPdf == NS_LITERAL_STRING("skia-pdf")) {
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
    status = ::PMSessionGetDestinationType(mPrintSession, mPrintSettings,
                                           &destination);
    if (status == noErr) {
      if (destination == kPMDestinationPrinter ||
          destination == kPMDestinationPreview){
        mPrintViaSkPDF = true;
      } else if (destination == kPMDestinationFile) {
        CFURLRef destURL;
        status = ::PMSessionCopyDestinationLocation(mPrintSession,
                                                    mPrintSettings, &destURL);
        if (status == noErr) {
          CFStringRef destPathRef =
            CFURLCopyFileSystemPath(destURL, kCFURLPOSIXPathStyle);
          NSString* destPath = (NSString*) destPathRef;
          NSString* destPathExt = [destPath pathExtension];
          if ([destPathExt isEqualToString: @"pdf"]) {
            mPrintViaSkPDF = true;
          }
        }
      }
    }
  }
#endif

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsDeviceContextSpecX::BeginDocument(const nsAString& aTitle, 
                                                  const nsAString& aPrintToFileName,
                                                  int32_t          aStartPage, 
                                                  int32_t          aEndPage)
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

    if (!aTitle.IsEmpty()) {
      CFStringRef cfString =
        ::CFStringCreateWithCharacters(NULL, reinterpret_cast<const UniChar*>(aTitle.BeginReading()),
                                             aTitle.Length());
      if (cfString) {
        ::PMPrintSettingsSetJobName(mPrintSettings, cfString);
        ::CFRelease(cfString);
      }
    }

    return NS_OK;

    NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsDeviceContextSpecX::EndDocument()
{
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
    status = ::PMSessionGetDestinationType(mPrintSession, mPrintSettings,
                                           &destination);

    switch (destination) {
    case kPMDestinationPrinter: {
      PMPrinter currentPrinter = NULL;
      status = ::PMSessionGetCurrentPrinter(mPrintSession, &currentPrinter);
      if (status != noErr) {
        return NS_ERROR_FAILURE;
      }
      CFStringRef mimeType = CFSTR("application/pdf");
      status = ::PMPrinterPrintWithFile(currentPrinter, mPrintSettings,
                                        mPageFormat, mimeType, pdfURL);
      break;
    }
    case kPMDestinationPreview: {
      // XXXjwatt Or should we use CocoaFileUtils::RevealFileInFinder(pdfURL);
      CFStringRef pdfPath = CFURLCopyFileSystemPath(pdfURL,
                                                    kCFURLPOSIXPathStyle);
      NSString* path = (NSString*) pdfPath;
      NSWorkspace* ws = [NSWorkspace sharedWorkspace];
      [ws openFile: path];
      break;
    }
    case kPMDestinationFile: {
      CFURLRef destURL;
      status = ::PMSessionCopyDestinationLocation(mPrintSession,
                                                  mPrintSettings, &destURL);
      if (status == noErr) {
        CFStringRef sourcePathRef =
          CFURLCopyFileSystemPath(pdfURL, kCFURLPOSIXPathStyle);
        NSString* sourcePath = (NSString*) sourcePathRef;
#ifdef DEBUG
        CFStringRef destPathRef =
          CFURLCopyFileSystemPath(destURL, kCFURLPOSIXPathStyle);
        NSString* destPath = (NSString*) destPathRef;
        NSString* destPathExt = [destPath pathExtension];
        MOZ_ASSERT([destPathExt isEqualToString: @"pdf"],
                   "nsDeviceContextSpecX::Init only allows '.pdf' for now");
        // We could use /usr/sbin/cupsfilter to convert the PDF to PS, but
        // currently we don't.
#endif
        NSFileManager* fileManager = [NSFileManager defaultManager];
        if ([fileManager fileExistsAtPath:sourcePath]) {
          NSURL* src = static_cast<NSURL*>(pdfURL);
          NSURL* dest = static_cast<NSURL*>(destURL);
          bool ok = [fileManager replaceItemAtURL:dest withItemAtURL:src
                                 backupItemName:nil
                                 options:NSFileManagerItemReplacementUsingNewMetadataOnly
                                 resultingItemURL:nil error:nil];
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

void nsDeviceContextSpecX::GetPaperRect(double* aTop, double* aLeft, double* aBottom, double* aRight)
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

    PMRect paperRect;
    ::PMGetAdjustedPaperRect(mPageFormat, &paperRect);

    *aTop = paperRect.top;
    *aLeft = paperRect.left;
    *aBottom = paperRect.bottom;
    *aRight = paperRect.right;

    NS_OBJC_END_TRY_ABORT_BLOCK;
}

already_AddRefed<PrintTarget> nsDeviceContextSpecX::MakePrintTarget()
{
    double top, left, bottom, right;
    GetPaperRect(&top, &left, &bottom, &right);
    const double width = right - left;
    const double height = bottom - top;
    IntSize size = IntSize::Floor(width, height);

#ifdef MOZ_ENABLE_SKIA_PDF
    if (mPrintViaSkPDF) {
      nsresult rv =
        NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(mTempFile));
      NS_ENSURE_SUCCESS(rv, nullptr);
      nsAutoCString tempPath("tmp-printing.pdf");
      mTempFile->AppendNative(tempPath);
      rv = mTempFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0600);
      NS_ENSURE_SUCCESS(rv, nullptr);
      mTempFile->GetNativePath(tempPath);
      auto stream = MakeUnique<SkFILEWStream>(tempPath.get());
      return PrintTargetSkPDF::CreateOrNull(Move(stream), size);
    }
#endif

    return PrintTargetCG::CreateOrNull(mPrintSession, mPageFormat,
                                       mPrintSettings, size);
}
