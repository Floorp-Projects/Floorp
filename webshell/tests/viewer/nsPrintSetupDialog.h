/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsPrintSetupDialog_h___
#define nsPrintSetupDialog_h___

#include "nsBaseDialog.h"
#include "nsIDOMElement.h"

class PrintSetupInfo {
public:
  PRBool    mPortrait;
  PRBool    mBevelLines;
  PRBool    mBlackText;
  PRBool    mBlackLines;
  PRBool    mLastPageFirst;
  PRBool    mPrintBackgrounds;
  float     mTopMargin;
  float     mBottomMargin;
  float     mLeftMargin;
  float     mRightMargin;
  PRBool    mDocTitle;
  PRBool    mDocLocation;
  nsString  mHeaderText;
  PRBool    mPageNum;
  PRBool    mPageTotal;
  PRBool    mDatePrinted;
  nsString  mFooterText;

  PrintSetupInfo() : mHeaderText(""), mFooterText("") {}
  PrintSetupInfo(const PrintSetupInfo & aPSI);

};

class nsBrowserWindow;

/**
 * Implement Navigator Find Dialog
 */
class nsPrintSetupDialog : public nsBaseDialog
{
public:

  nsPrintSetupDialog(nsBrowserWindow * aBrowserWindow);
  virtual ~nsPrintSetupDialog();

  virtual void DoClose();

  // nsIWindowListener Methods

  virtual void MouseClick(nsIDOMEvent* aMouseEvent, nsIXPBaseWindow * aWindow, PRBool &aStatus);
  virtual void Initialize(nsIXPBaseWindow * aWindow);
  virtual void Destroy(nsIXPBaseWindow * aWindow);

  // new methods
  virtual void GetSetupInfo(PrintSetupInfo & anInfo);
  virtual void SetSetupInfo(const PrintSetupInfo & anInfo);

protected:
  void SetSetupInfoInternal(const PrintSetupInfo & anInfo);

  nsIDOMElement   * mOKBtn;
  PrintSetupInfo  * mPrinterSetupInfo;

};

#endif /* nsPrintSetupDialog_h___ */
