/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Ventnor <m.ventnor@gmail.com>
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

#ifndef nsPrintSettingsGTK_h_
#define nsPrintSettingsGTK_h_

#include "nsPrintSettingsImpl.h"

extern "C" {
#include <gtk/gtk.h>
#include <gtk/gtkprinter.h>
#include <gtk/gtkprintjob.h>
}

#define NS_PRINTSETTINGSGTK_IID \
{ 0x758df520, 0xc7c3, 0x11dc, { 0x95, 0xff, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66 } }
 

//*****************************************************************************
//***    nsPrintSettingsGTK
//*****************************************************************************

class nsPrintSettingsGTK : public nsPrintSettings
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_PRINTSETTINGSGTK_IID)

  nsPrintSettingsGTK();
  virtual ~nsPrintSettingsGTK();

  // We're overriding these methods because we want to read/write with GTK objects,
  // not local variables. This allows a simpler settings implementation between
  // Gecko and GTK.

  GtkPageSetup* GetGtkPageSetup() { return mPageSetup; };
  void SetGtkPageSetup(GtkPageSetup *aPageSetup);

  GtkPrintSettings* GetGtkPrintSettings() { return mPrintSettings; };
  void SetGtkPrintSettings(GtkPrintSettings *aPrintSettings);

  GtkPrinter* GetGtkPrinter() { return mGTKPrinter; };
  void SetGtkPrinter(GtkPrinter *aPrinter);

  PRBool GetForcePrintSelectionOnly() { return mPrintSelectionOnly; };
  void SetForcePrintSelectionOnly(PRBool aPrintSelectionOnly) { mPrintSelectionOnly = aPrintSelectionOnly; };

  // If not printing the selection, this is stored in the GtkPrintSettings. Printing the
  // selection is stored as a protected boolean (mPrintSelectionOnly).
  NS_IMETHOD GetPrintRange(PRInt16 *aPrintRange);
  NS_IMETHOD SetPrintRange(PRInt16 aPrintRange);

  // The page range is stored as as single range in the GtkPrintSettings object.
  NS_IMETHOD GetStartPageRange(PRInt32 *aStartPageRange);
  NS_IMETHOD SetStartPageRange(PRInt32 aStartPageRange);
  NS_IMETHOD GetEndPageRange(PRInt32 *aEndPageRange);
  NS_IMETHOD SetEndPageRange(PRInt32 aEndPageRange);

  // Reversed, color, orientation and file name are all stored in the GtkPrintSettings.
  // Orientation is also stored in the GtkPageSetup and its setting takes priority when getting the orientation.
  NS_IMETHOD GetPrintReversed(PRBool *aPrintReversed);
  NS_IMETHOD SetPrintReversed(PRBool aPrintReversed);

  NS_IMETHOD GetPrintInColor(PRBool *aPrintInColor);
  NS_IMETHOD SetPrintInColor(PRBool aPrintInColor);

  NS_IMETHOD GetOrientation(PRInt32 *aOrientation);
  NS_IMETHOD SetOrientation(PRInt32 aOrientation);

  NS_IMETHOD GetToFileName(PRUnichar * *aToFileName);
  NS_IMETHOD SetToFileName(const PRUnichar * aToFileName);

  // Gets/Sets the printer name in the GtkPrintSettings. If no printer name is specified there,
  // you will get back the name of the current internal GtkPrinter.
  NS_IMETHOD GetPrinterName(PRUnichar * *aPrinter);
  NS_IMETHOD SetPrinterName(const PRUnichar * aPrinter);

  // Number of copies is stored/gotten from the GtkPrintSettings.
  NS_IMETHOD GetNumCopies(PRInt32 *aNumCopies);
  NS_IMETHOD SetNumCopies(PRInt32 aNumCopies);

  NS_IMETHOD GetScaling(double *aScaling);
  NS_IMETHOD SetScaling(double aScaling);

  // A name recognised by GTK is strongly advised here, as this is used to create a GtkPaperSize.
  NS_IMETHOD GetPaperName(PRUnichar * *aPaperName);
  NS_IMETHOD SetPaperName(const PRUnichar * aPaperName);

  NS_IMETHOD SetUnwriteableMarginInTwips(nsIntMargin& aUnwriteableMargin);
  NS_IMETHOD SetUnwriteableMarginTop(double aUnwriteableMarginTop);
  NS_IMETHOD SetUnwriteableMarginLeft(double aUnwriteableMarginLeft);
  NS_IMETHOD SetUnwriteableMarginBottom(double aUnwriteableMarginBottom);
  NS_IMETHOD SetUnwriteableMarginRight(double aUnwriteableMarginRight);

  NS_IMETHOD GetPaperWidth(double *aPaperWidth);
  NS_IMETHOD SetPaperWidth(double aPaperWidth);

  NS_IMETHOD GetPaperHeight(double *aPaperHeight);
  NS_IMETHOD SetPaperHeight(double aPaperHeight);

  NS_IMETHOD SetPaperSizeUnit(PRInt16 aPaperSizeUnit);

  NS_IMETHOD GetEffectivePageSize(double *aWidth, double *aHeight);

  NS_IMETHOD SetupSilentPrinting();

protected:
  nsPrintSettingsGTK(const nsPrintSettingsGTK& src);
  nsPrintSettingsGTK& operator=(const nsPrintSettingsGTK& rhs);

  virtual nsresult _Clone(nsIPrintSettings **_retval);
  virtual nsresult _Assign(nsIPrintSettings *aPS);

  GtkUnit GetGTKUnit(PRInt16 aGeckoUnit);
  void SaveNewPageSize();

  /**
   * Re-initialize mUnwriteableMargin with values from mPageSetup.
   * Should be called whenever mPageSetup is initialized or overwritten.
   */
  void InitUnwriteableMargin();

  /**
   * On construction:
   * - mPrintSettings, mPageSetup and mPaperSize are just new objects with defaults determined by GTK.
   * - mGTKPrinter is NULL!!! Remember to be careful when accessing this property.
   */
  GtkPageSetup* mPageSetup;
  GtkPrintSettings* mPrintSettings;
  GtkPrinter* mGTKPrinter;
  GtkPaperSize* mPaperSize;

  PRBool mPrintSelectionOnly;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsPrintSettingsGTK, NS_PRINTSETTINGSGTK_IID)


#endif // nsPrintSettingsGTK_h_
