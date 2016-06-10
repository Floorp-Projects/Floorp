/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef nsDeviceContextSpecQt_h___
#define nsDeviceContextSpecQt_h___

#include "nsIDeviceContextSpec.h"
#include "nsIPrintSettings.h"
#include "nsIPrintOptions.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#include "nsCRT.h" /* should be <limits.h>? */

class nsDeviceContextSpecQt : public nsIDeviceContextSpec
{
public:
    nsDeviceContextSpecQt();

    NS_DECL_ISUPPORTS

    virtual already_AddRefed<PrintTarget> MakePrintTarget() final;

    NS_IMETHOD Init(nsIWidget* aWidget,
                    nsIPrintSettings* aPS,
                    bool aIsPrintPreview);
    NS_IMETHOD BeginDocument(const nsAString& aTitle,
                             const nsAString& aPrintToFileName,
                             int32_t aStartPage,
                             int32_t aEndPage);
    NS_IMETHOD EndDocument();
    NS_IMETHOD BeginPage() { return NS_OK; }
    NS_IMETHOD EndPage() { return NS_OK; }

protected:
    virtual ~nsDeviceContextSpecQt();

    nsCOMPtr<nsIPrintSettings> mPrintSettings;
    bool mToPrinter : 1;      /* If true, print to printer */
    bool mIsPPreview : 1;     /* If true, is print preview */
    char   mPath[PATH_MAX];     /* If toPrinter = false, dest file */
    char   mPrinter[256];       /* Printer name */
    nsCString         mSpoolName;
    nsCOMPtr<nsIFile> mSpoolFile;
};

class nsPrinterEnumeratorQt : public nsIPrinterEnumerator
{
public:
    nsPrinterEnumeratorQt();
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPRINTERENUMERATOR

protected:
    virtual ~nsPrinterEnumeratorQt();

};

#endif /* !nsDeviceContextSpecQt_h___ */
