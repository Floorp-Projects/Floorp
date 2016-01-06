/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDeviceContextSpecX.h"

#include "nsCRT.h"
#include <unistd.h>

#include "nsAutoPtr.h"
#include "nsQueryObject.h"
#include "nsIServiceManager.h"
#include "nsIPrintOptions.h"
#include "nsPrintSettingsX.h"

#include "gfxQuartzSurface.h"

// This must be the last include:
#include "nsObjCExceptions.h"

nsDeviceContextSpecX::nsDeviceContextSpecX()
: mPrintSession(NULL)
, mPageFormat(kPMNoPageFormat)
, mPrintSettings(kPMNoPrintSettings)
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

  mPrintSession = settings->GetPMPrintSession();
  ::PMRetain(mPrintSession);
  mPageFormat = settings->GetPMPageFormat();
  mPrintSettings = settings->GetPMPrintSettings();

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

    OSStatus status;
    status = ::PMSetFirstPage(mPrintSettings, aStartPage, false);
    NS_ASSERTION(status == noErr, "PMSetFirstPage failed");
    status = ::PMSetLastPage(mPrintSettings, aEndPage, false);
    NS_ASSERTION(status == noErr, "PMSetLastPage failed");

    status = ::PMSessionBeginCGDocumentNoDialog(mPrintSession, mPrintSettings, mPageFormat);
    if (status != noErr)
      return NS_ERROR_ABORT;

    return NS_OK;

    NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsDeviceContextSpecX::EndDocument()
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

    ::PMSessionEndDocumentNoDialog(mPrintSession);
    return NS_OK;

    NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsDeviceContextSpecX::BeginPage()
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

    PMSessionError(mPrintSession);
    OSStatus status = ::PMSessionBeginPageNoDialog(mPrintSession, mPageFormat, NULL);
    if (status != noErr) return NS_ERROR_ABORT;
    return NS_OK;

    NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsDeviceContextSpecX::EndPage()
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

    OSStatus status = ::PMSessionEndPageNoDialog(mPrintSession);
    if (status != noErr) return NS_ERROR_ABORT;
    return NS_OK;

    NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void nsDeviceContextSpecX::GetPaperRect(double* aTop, double* aLeft, double* aBottom, double* aRight)
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

    PMRect paperRect;
    ::PMGetAdjustedPaperRect(mPageFormat, &paperRect);

    *aTop = paperRect.top, *aLeft = paperRect.left;
    *aBottom = paperRect.bottom, *aRight = paperRect.right;

    NS_OBJC_END_TRY_ABORT_BLOCK;
}

NS_IMETHODIMP nsDeviceContextSpecX::GetSurfaceForPrinter(gfxASurface **surface)
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

    double top, left, bottom, right;
    GetPaperRect(&top, &left, &bottom, &right);
    const double width = right - left;
    const double height = bottom - top;

    CGContextRef context;
    ::PMSessionGetCGGraphicsContext(mPrintSession, &context);

    RefPtr<gfxASurface> newSurface;

    if (context) {
        // Initially, origin is at bottom-left corner of the paper.
        // Here, we translate it to top-left corner of the paper.
        CGContextTranslateCTM(context, 0, height);
        CGContextScaleCTM(context, 1.0, -1.0);
        newSurface = new gfxQuartzSurface(context, gfxSize(width, height));
    } else {
        newSurface = new gfxQuartzSurface(gfxSize((int32_t)width, (int32_t)height), gfxImageFormat::ARGB32);
    }

    if (!newSurface)
        return NS_ERROR_FAILURE;

    *surface = newSurface;
    NS_ADDREF(*surface);

    return NS_OK;

    NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
