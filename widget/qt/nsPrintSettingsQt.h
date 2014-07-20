/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintSettingsQt_h_
#define nsPrintSettingsQt_h_

#include <QSharedPointer>
#include "nsPrintSettingsImpl.h"
#define NS_PRINTSETTINGSQT_IID \
{0x5bc4c746, 0x8970, 0x43a3, {0xbf, 0xb1, 0x5d, 0xe1, 0x74, 0xaf, 0x7c, 0xea}}

class QPrinter;
class nsPrintSettingsQt : public nsPrintSettings
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_PRINTSETTINGSQT_IID)

    nsPrintSettingsQt();

    NS_IMETHOD GetPrintRange(int16_t* aPrintRange);
    NS_IMETHOD SetPrintRange(int16_t aPrintRange);

    NS_IMETHOD GetStartPageRange(int32_t* aStartPageRange);
    NS_IMETHOD SetStartPageRange(int32_t aStartPageRange);
    NS_IMETHOD GetEndPageRange(int32_t* aEndPageRange);
    NS_IMETHOD SetEndPageRange(int32_t aEndPageRange);

    NS_IMETHOD GetPrintReversed(bool* aPrintReversed);
    NS_IMETHOD SetPrintReversed(bool aPrintReversed);

    NS_IMETHOD GetPrintInColor(bool* aPrintInColor);
    NS_IMETHOD SetPrintInColor(bool aPrintInColor);

    NS_IMETHOD GetOrientation(int32_t* aOrientation);
    NS_IMETHOD SetOrientation(int32_t aOrientation);

    NS_IMETHOD GetToFileName(char16_t** aToFileName);
    NS_IMETHOD SetToFileName(const char16_t* aToFileName);

    NS_IMETHOD GetPrinterName(char16_t** aPrinter);
    NS_IMETHOD SetPrinterName(const char16_t* aPrinter);

    NS_IMETHOD GetNumCopies(int32_t* aNumCopies);
    NS_IMETHOD SetNumCopies(int32_t aNumCopies);

    NS_IMETHOD GetScaling(double* aScaling);
    NS_IMETHOD SetScaling(double aScaling);

    NS_IMETHOD GetPaperName(char16_t** aPaperName);
    NS_IMETHOD SetPaperName(const char16_t* aPaperName);

    NS_IMETHOD SetUnwriteableMarginInTwips(nsIntMargin& aUnwriteableMargin);
    NS_IMETHOD SetUnwriteableMarginTop(double aUnwriteableMarginTop);
    NS_IMETHOD SetUnwriteableMarginLeft(double aUnwriteableMarginLeft);
    NS_IMETHOD SetUnwriteableMarginBottom(double aUnwriteableMarginBottom);
    NS_IMETHOD SetUnwriteableMarginRight(double aUnwriteableMarginRight);

    NS_IMETHOD GetPaperWidth(double* aPaperWidth);
    NS_IMETHOD SetPaperWidth(double aPaperWidth);

    NS_IMETHOD GetPaperHeight(double* aPaperHeight);
    NS_IMETHOD SetPaperHeight(double aPaperHeight);

    NS_IMETHOD SetPaperSizeUnit(int16_t aPaperSizeUnit);

    NS_IMETHOD GetEffectivePageSize(double* aWidth, double* aHeight);

protected:
    virtual ~nsPrintSettingsQt();

    nsPrintSettingsQt(const nsPrintSettingsQt& src);
    nsPrintSettingsQt& operator=(const nsPrintSettingsQt& rhs);

    virtual nsresult _Clone(nsIPrintSettings** _retval);
    virtual nsresult _Assign(nsIPrintSettings* aPS);

    QSharedPointer<QPrinter> mQPrinter;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsPrintSettingsQt, NS_PRINTSETTINGSQT_IID)
#endif // nsPrintSettingsQt_h_
