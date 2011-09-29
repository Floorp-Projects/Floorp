/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Ventnor <m.ventnor@gmail.com>
 *   Florian Hänel <heeen@gmx.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
    virtual ~nsPrintSettingsQt();

    NS_IMETHOD GetPrintRange(PRInt16* aPrintRange);
    NS_IMETHOD SetPrintRange(PRInt16 aPrintRange);

    NS_IMETHOD GetStartPageRange(PRInt32* aStartPageRange);
    NS_IMETHOD SetStartPageRange(PRInt32 aStartPageRange);
    NS_IMETHOD GetEndPageRange(PRInt32* aEndPageRange);
    NS_IMETHOD SetEndPageRange(PRInt32 aEndPageRange);

    NS_IMETHOD GetPrintReversed(bool* aPrintReversed);
    NS_IMETHOD SetPrintReversed(bool aPrintReversed);

    NS_IMETHOD GetPrintInColor(bool* aPrintInColor);
    NS_IMETHOD SetPrintInColor(bool aPrintInColor);

    NS_IMETHOD GetOrientation(PRInt32* aOrientation);
    NS_IMETHOD SetOrientation(PRInt32 aOrientation);

    NS_IMETHOD GetToFileName(PRUnichar** aToFileName);
    NS_IMETHOD SetToFileName(const PRUnichar* aToFileName);

    NS_IMETHOD GetPrinterName(PRUnichar** aPrinter);
    NS_IMETHOD SetPrinterName(const PRUnichar* aPrinter);

    NS_IMETHOD GetNumCopies(PRInt32* aNumCopies);
    NS_IMETHOD SetNumCopies(PRInt32 aNumCopies);

    NS_IMETHOD GetScaling(double* aScaling);
    NS_IMETHOD SetScaling(double aScaling);

    NS_IMETHOD GetPaperName(PRUnichar** aPaperName);
    NS_IMETHOD SetPaperName(const PRUnichar* aPaperName);

    NS_IMETHOD SetUnwriteableMarginInTwips(nsIntMargin& aUnwriteableMargin);
    NS_IMETHOD SetUnwriteableMarginTop(double aUnwriteableMarginTop);
    NS_IMETHOD SetUnwriteableMarginLeft(double aUnwriteableMarginLeft);
    NS_IMETHOD SetUnwriteableMarginBottom(double aUnwriteableMarginBottom);
    NS_IMETHOD SetUnwriteableMarginRight(double aUnwriteableMarginRight);

    NS_IMETHOD GetPaperWidth(double* aPaperWidth);
    NS_IMETHOD SetPaperWidth(double aPaperWidth);

    NS_IMETHOD GetPaperHeight(double* aPaperHeight);
    NS_IMETHOD SetPaperHeight(double aPaperHeight);

    NS_IMETHOD SetPaperSizeUnit(PRInt16 aPaperSizeUnit);

    NS_IMETHOD GetEffectivePageSize(double* aWidth, double* aHeight);

protected:
    nsPrintSettingsQt(const nsPrintSettingsQt& src);
    nsPrintSettingsQt& operator=(const nsPrintSettingsQt& rhs);

    virtual nsresult _Clone(nsIPrintSettings** _retval);
    virtual nsresult _Assign(nsIPrintSettings* aPS);

    QSharedPointer<QPrinter> mQPrinter;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsPrintSettingsQt, NS_PRINTSETTINGSQT_IID)
#endif // nsPrintSettingsQt_h_
