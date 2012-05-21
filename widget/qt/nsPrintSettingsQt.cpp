/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QPrinter>
#include <QDebug>
#include "nsPrintSettingsQt.h"
#include "nsILocalFile.h"
#include "nsCRTGlue.h"

NS_IMPL_ISUPPORTS_INHERITED1(nsPrintSettingsQt,
                             nsPrintSettings,
                             nsPrintSettingsQt)

nsPrintSettingsQt::nsPrintSettingsQt():
    mQPrinter(new QPrinter())
{
}

nsPrintSettingsQt::~nsPrintSettingsQt()
{
    //smart pointer should take care of cleanup
}

nsPrintSettingsQt::nsPrintSettingsQt(const nsPrintSettingsQt& aPS):
    mQPrinter(aPS.mQPrinter)
{
}

nsPrintSettingsQt& 
nsPrintSettingsQt::operator=(const nsPrintSettingsQt& rhs)
{
    if (this == &rhs) {
        return *this;
    }

    nsPrintSettings::operator=(rhs);
    mQPrinter = rhs.mQPrinter;
    return *this;
}

nsresult 
nsPrintSettingsQt::_Clone(nsIPrintSettings** _retval)
{
    NS_ENSURE_ARG_POINTER(_retval);

    nsPrintSettingsQt* newSettings = new nsPrintSettingsQt(*this);
    *_retval = newSettings;
    NS_ADDREF(*_retval);
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::_Assign(nsIPrintSettings* aPS)
{
    nsPrintSettingsQt* printSettingsQt = static_cast<nsPrintSettingsQt*>(aPS);
    if (!printSettingsQt)
        return NS_ERROR_UNEXPECTED;
    *this = *printSettingsQt;
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetPrintRange(PRInt16* aPrintRange)
{
    NS_ENSURE_ARG_POINTER(aPrintRange);

    QPrinter::PrintRange range = mQPrinter->printRange();
    if (range == QPrinter::PageRange) {
        *aPrintRange = kRangeSpecifiedPageRange;
    } else if (range == QPrinter::Selection) {
        *aPrintRange = kRangeSelection;
    } else {
        *aPrintRange = kRangeAllPages;
    }

    return NS_OK;
}

NS_IMETHODIMP 
nsPrintSettingsQt::SetPrintRange(PRInt16 aPrintRange)
{
    if (aPrintRange == kRangeSelection) {
        mQPrinter->setPrintRange(QPrinter::Selection);
    } else if (aPrintRange == kRangeSpecifiedPageRange) {
        mQPrinter->setPrintRange(QPrinter::PageRange);
    } else {
        mQPrinter->setPrintRange(QPrinter::AllPages);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetStartPageRange(PRInt32* aStartPageRange)
{
    NS_ENSURE_ARG_POINTER(aStartPageRange);
    PRInt32 start = mQPrinter->fromPage();
    *aStartPageRange = start;
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetStartPageRange(PRInt32 aStartPageRange)
{
    PRInt32 endRange = mQPrinter->toPage();
    mQPrinter->setFromTo(aStartPageRange, endRange);
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetEndPageRange(PRInt32* aEndPageRange)
{
    NS_ENSURE_ARG_POINTER(aEndPageRange);
    PRInt32 end = mQPrinter->toPage();
    *aEndPageRange = end;
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetEndPageRange(PRInt32 aEndPageRange)
{
    PRInt32 startRange = mQPrinter->fromPage();
    mQPrinter->setFromTo(startRange, aEndPageRange);
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetPrintReversed(bool* aPrintReversed)
{
    NS_ENSURE_ARG_POINTER(aPrintReversed);
    if (mQPrinter->pageOrder() == QPrinter::LastPageFirst) {
        *aPrintReversed = true;
    } else {
        *aPrintReversed = false;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetPrintReversed(bool aPrintReversed)
{
    if (aPrintReversed) {
        mQPrinter->setPageOrder(QPrinter::LastPageFirst);
    } else {
        mQPrinter->setPageOrder(QPrinter::FirstPageFirst);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetPrintInColor(bool* aPrintInColor)
{
    NS_ENSURE_ARG_POINTER(aPrintInColor);
    if (mQPrinter->colorMode() == QPrinter::Color) {
        *aPrintInColor = true;
    } else {
        *aPrintInColor = false;
    }
    return NS_OK;
}
NS_IMETHODIMP
nsPrintSettingsQt::SetPrintInColor(bool aPrintInColor)
{
    if (aPrintInColor) {
        mQPrinter->setColorMode(QPrinter::Color);
    } else {
        mQPrinter->setColorMode(QPrinter::GrayScale);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetOrientation(PRInt32* aOrientation)
{
    NS_ENSURE_ARG_POINTER(aOrientation);
    QPrinter::Orientation orientation = mQPrinter->orientation();
    if (orientation == QPrinter::Landscape) {
        *aOrientation = kLandscapeOrientation;
    } else {
        *aOrientation = kPortraitOrientation;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetOrientation(PRInt32 aOrientation)
{
    if (aOrientation == kLandscapeOrientation) {
        mQPrinter->setOrientation(QPrinter::Landscape);
    } else {
        mQPrinter->setOrientation(QPrinter::Portrait);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetToFileName(PRUnichar** aToFileName)
{
    NS_ENSURE_ARG_POINTER(aToFileName);
    QString filename;
    filename = mQPrinter->outputFileName();
    *aToFileName = ToNewUnicode(
            nsDependentString((PRUnichar*)filename.data()));
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetToFileName(const PRUnichar* aToFileName)
{
    nsCOMPtr<nsILocalFile> file;
    nsresult rv = NS_NewLocalFile(nsDependentString(aToFileName), true,
                                getter_AddRefs(file));
    NS_ENSURE_SUCCESS(rv, rv);

    QString filename((const QChar*)aToFileName, NS_strlen(aToFileName));
    mQPrinter->setOutputFileName(filename);

    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetPrinterName(PRUnichar** aPrinter)
{
    NS_ENSURE_ARG_POINTER(aPrinter);
    *aPrinter = ToNewUnicode(nsDependentString(
                (const PRUnichar*)mQPrinter->printerName().constData()));
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetPrinterName(const PRUnichar* aPrinter)
{
    QString printername((const QChar*)aPrinter, NS_strlen(aPrinter));
    mQPrinter->setPrinterName(printername);
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetNumCopies(PRInt32* aNumCopies)
{
    NS_ENSURE_ARG_POINTER(aNumCopies);
    *aNumCopies = mQPrinter->numCopies();
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetNumCopies(PRInt32 aNumCopies)
{
    mQPrinter->setNumCopies(aNumCopies);
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetScaling(double* aScaling)
{
    NS_ENSURE_ARG_POINTER(aScaling);
    qDebug()<<Q_FUNC_INFO;
    qDebug()<<"Scaling not implemented in Qt port";
    *aScaling = 1.0; //FIXME
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetScaling(double aScaling)
{
    qDebug()<<Q_FUNC_INFO;
    qDebug()<<"Scaling not implemented in Qt port"; //FIXME
    return NS_ERROR_NOT_IMPLEMENTED;
}

static const char* const indexToPaperName[] =
{ "A4", "B5", "Letter", "Legal", "Executive",
  "A0", "A1", "A2", "A3", "A5", "A6", "A7", "A8", "A9",
  "B0", "B1", "B10", "B2", "B3", "B4", "B6", "B7", "B8", "B9",
  "C5E", "Comm10E", "DLE", "Folio", "Ledger", "Tabloid"
};

static const QPrinter::PageSize indexToQtPaperEnum[] =
{
    QPrinter::A4, QPrinter::B5, QPrinter::Letter, QPrinter::Legal,
    QPrinter::Executive, QPrinter::A0, QPrinter::A1, QPrinter::A2, QPrinter::A3,
    QPrinter::A5, QPrinter::A6, QPrinter::A7, QPrinter::A8, QPrinter::A9,
    QPrinter::B0, QPrinter::B1, QPrinter::B10, QPrinter::B2, QPrinter::B3,
    QPrinter::B4, QPrinter::B6, QPrinter::B7, QPrinter::B8, QPrinter::B9,
    QPrinter::C5E, QPrinter::Comm10E, QPrinter::DLE, QPrinter::Folio,
    QPrinter::Ledger, QPrinter::Tabloid
};

NS_IMETHODIMP
nsPrintSettingsQt::GetPaperName(PRUnichar** aPaperName)
{
    PR_STATIC_ASSERT(sizeof(indexToPaperName)/
        sizeof(char*) == QPrinter::NPageSize);
    PR_STATIC_ASSERT(sizeof(indexToQtPaperEnum)/
        sizeof(QPrinter::PageSize) == QPrinter::NPageSize);

    QPrinter::PaperSize size = mQPrinter->paperSize();
    QString name(indexToPaperName[size]);
    *aPaperName = ToNewUnicode(nsDependentString
        ((const PRUnichar*)name.constData()));
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetPaperName(const PRUnichar* aPaperName)
{
    QString ref((QChar*)aPaperName, NS_strlen(aPaperName));
    for (PRUint32 i = 0; i < QPrinter::NPageSize; i++)
    {
        if (ref == QString(indexToPaperName[i])) {
            mQPrinter->setPageSize(indexToQtPaperEnum[i]);
            return NS_OK;
        }
    }
    return NS_ERROR_FAILURE;
}

QPrinter::Unit GetQtUnit(PRInt16 aGeckoUnit)
{
    if (aGeckoUnit == nsIPrintSettings::kPaperSizeMillimeters) {
        return QPrinter::Millimeter;
    } else {
        return QPrinter::Inch;
    }
}

#define SETUNWRITEABLEMARGIN\
    mQPrinter->setPageMargins(\
            NS_TWIPS_TO_INCHES(mUnwriteableMargin.left),\
            NS_TWIPS_TO_INCHES(mUnwriteableMargin.top),\
            NS_TWIPS_TO_INCHES(mUnwriteableMargin.right),\
            NS_TWIPS_TO_INCHES(mUnwriteableMargin.bottom),\
            QPrinter::Inch);

NS_IMETHODIMP
nsPrintSettingsQt::SetUnwriteableMarginInTwips(nsIntMargin& aUnwriteableMargin)
{
    nsPrintSettings::SetUnwriteableMarginInTwips(aUnwriteableMargin);
    SETUNWRITEABLEMARGIN
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetUnwriteableMarginTop(double aUnwriteableMarginTop)
{
    nsPrintSettings::SetUnwriteableMarginTop(aUnwriteableMarginTop);
    SETUNWRITEABLEMARGIN
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetUnwriteableMarginLeft(double aUnwriteableMarginLeft)
{
    nsPrintSettings::SetUnwriteableMarginLeft(aUnwriteableMarginLeft);
    SETUNWRITEABLEMARGIN
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetUnwriteableMarginBottom(double aUnwriteableMarginBottom)
{
    nsPrintSettings::SetUnwriteableMarginBottom(aUnwriteableMarginBottom);
    SETUNWRITEABLEMARGIN
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetUnwriteableMarginRight(double aUnwriteableMarginRight)
{
    nsPrintSettings::SetUnwriteableMarginRight(aUnwriteableMarginRight);
    SETUNWRITEABLEMARGIN
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetPaperWidth(double* aPaperWidth)
{
    NS_ENSURE_ARG_POINTER(aPaperWidth);
    QSizeF papersize = mQPrinter->paperSize(GetQtUnit(mPaperSizeUnit));
    *aPaperWidth = papersize.width();
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetPaperWidth(double aPaperWidth)
{
    QSizeF papersize = mQPrinter->paperSize(GetQtUnit(mPaperSizeUnit));
    papersize.setWidth(aPaperWidth);
    mQPrinter->setPaperSize(papersize, GetQtUnit(mPaperSizeUnit));
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetPaperHeight(double* aPaperHeight)
{
    NS_ENSURE_ARG_POINTER(aPaperHeight);
    QSizeF papersize = mQPrinter->paperSize(GetQtUnit(mPaperSizeUnit));
    *aPaperHeight = papersize.height();
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetPaperHeight(double aPaperHeight)
{
    QSizeF papersize = mQPrinter->paperSize(GetQtUnit(mPaperSizeUnit));
    papersize.setHeight(aPaperHeight);
    mQPrinter->setPaperSize(papersize, GetQtUnit(mPaperSizeUnit));
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::SetPaperSizeUnit(PRInt16 aPaperSizeUnit)
{
    mPaperSizeUnit = aPaperSizeUnit;
    return NS_OK;
}

NS_IMETHODIMP
nsPrintSettingsQt::GetEffectivePageSize(double* aWidth, double* aHeight)
{
    QSizeF papersize = mQPrinter->paperSize(QPrinter::Inch);
    if (mQPrinter->orientation() == QPrinter::Landscape) {
        *aWidth  = NS_INCHES_TO_INT_TWIPS(papersize.height());
        *aHeight = NS_INCHES_TO_INT_TWIPS(papersize.width());
    } else {
        *aWidth  = NS_INCHES_TO_INT_TWIPS(papersize.width());
        *aHeight = NS_INCHES_TO_INT_TWIPS(papersize.height());
    }
    return NS_OK;
}

