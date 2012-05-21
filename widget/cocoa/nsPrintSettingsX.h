/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintSettingsX_h_
#define nsPrintSettingsX_h_

#include "nsPrintSettingsImpl.h"  
#import <Cocoa/Cocoa.h>

class nsPrintSettingsX : public nsPrintSettings
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  nsPrintSettingsX();
  virtual ~nsPrintSettingsX();
  nsresult Init();
  NSPrintInfo* GetCocoaPrintInfo() { return mPrintInfo; }
  void SetCocoaPrintInfo(NSPrintInfo* aPrintInfo);
  virtual nsresult ReadPageFormatFromPrefs();
  virtual nsresult WritePageFormatToPrefs();

  PMPrintSettings GetPMPrintSettings();
  PMPrintSession GetPMPrintSession();
  PMPageFormat GetPMPageFormat();
  void SetPMPageFormat(PMPageFormat aPageFormat);

protected:
  nsPrintSettingsX(const nsPrintSettingsX& src);
  nsPrintSettingsX& operator=(const nsPrintSettingsX& rhs);

  nsresult _Clone(nsIPrintSettings **_retval);
  nsresult _Assign(nsIPrintSettings *aPS);

  // Re-initialize mUnwriteableMargin with values from mPageFormat.
  // Should be called whenever mPageFormat is initialized or overwritten.
  nsresult InitUnwriteableMargin();

  // The out param has a ref count of 1 on return so caller needs to PMRelase() when done.
  OSStatus CreateDefaultPageFormat(PMPrintSession aSession, PMPageFormat& outFormat);
  OSStatus CreateDefaultPrintSettings(PMPrintSession aSession, PMPrintSettings& outSettings);

  NSPrintInfo* mPrintInfo;
};

#endif // nsPrintSettingsX_h_
