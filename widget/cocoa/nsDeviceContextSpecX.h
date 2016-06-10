/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceContextSpecX_h_
#define nsDeviceContextSpecX_h_

#include "nsIDeviceContextSpec.h"

#include <ApplicationServices/ApplicationServices.h>

class nsDeviceContextSpecX : public nsIDeviceContextSpec
{
public:
    NS_DECL_ISUPPORTS

    nsDeviceContextSpecX();

    NS_IMETHOD Init(nsIWidget *aWidget, nsIPrintSettings* aPS, bool aIsPrintPreview) override;
    virtual already_AddRefed<PrintTarget> MakePrintTarget() final;
    NS_IMETHOD BeginDocument(const nsAString& aTitle,
                             const nsAString& aPrintToFileName,
                             int32_t          aStartPage,
                             int32_t          aEndPage) override;
    NS_IMETHOD EndDocument() override;
    NS_IMETHOD BeginPage() override;
    NS_IMETHOD EndPage() override;

    void GetPaperRect(double* aTop, double* aLeft, double* aBottom, double* aRight);

protected:
    virtual ~nsDeviceContextSpecX();

protected:
    PMPrintSession    mPrintSession;              // printing context.
    PMPageFormat      mPageFormat;                // page format.
    PMPrintSettings   mPrintSettings;             // print settings.
};

#endif //nsDeviceContextSpecX_h_
