/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintSettingsWin_h__
#define nsPrintSettingsWin_h__

#include "nsPrintSettingsImpl.h"
#include "nsIPrintSettingsWin.h"
#include <windows.h>

//*****************************************************************************
//***    nsPrintSettingsWin
//*****************************************************************************
class nsPrintSettingsWin : public nsPrintSettings, public nsIPrintSettingsWin {
  virtual ~nsPrintSettingsWin();

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPRINTSETTINGSWIN

  nsPrintSettingsWin();
  nsPrintSettingsWin(const nsPrintSettingsWin& aPS);

  /**
   * @param aPaperSize the Windows dmPaperSize
   * @param aPaperSizeUnit will be set to the nsIPrintSettings paper size unit
   *                       associated with aPaperSize or left unchanged if
   *                       aPaperSize is not recognized
   */
  static void PaperSizeUnitFromDmPaperSize(short aPaperSize,
                                           int16_t& aPaperSizeUnit);

  void InitWithInitializer(const PrintSettingsInitializer& aSettings) final;

  /**
   * Makes a new copy
   */
  virtual nsresult _Clone(nsIPrintSettings** _retval);

  /**
   * Assigns values
   */
  virtual nsresult _Assign(nsIPrintSettings* aPS);

  /**
   * Assignment
   */
  nsPrintSettingsWin& operator=(const nsPrintSettingsWin& rhs);

 protected:
  void CopyDevMode(DEVMODEW* aInDevMode, DEVMODEW*& aOutDevMode);
  void InitUnwriteableMargin(HDC aHdc);

  nsString mDeviceName;
  nsString mDriverName;
  LPDEVMODEW mDevMode;
};

#endif /* nsPrintSettingsWin_h__ */
