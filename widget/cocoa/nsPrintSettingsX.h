/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintSettingsX_h_
#define nsPrintSettingsX_h_

#include "nsPrintSettingsImpl.h"  
#import <Cocoa/Cocoa.h>

#define NS_PRINTSETTINGSX_IID \
{ 0x0DF2FDBD, 0x906D, 0x4726, \
  { 0x9E, 0x4D, 0xCF, 0xE0, 0x87, 0x8D, 0x70, 0x7C } }

class nsPrintSettingsX : public nsPrintSettings
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_PRINTSETTINGSX_IID)
  NS_DECL_ISUPPORTS_INHERITED

  nsPrintSettingsX();
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
  virtual ~nsPrintSettingsX();

  nsPrintSettingsX(const nsPrintSettingsX& src);
  nsPrintSettingsX& operator=(const nsPrintSettingsX& rhs);

  nsresult _Clone(nsIPrintSettings **_retval) override;
  nsresult _Assign(nsIPrintSettings *aPS) override;

  // Re-initialize mUnwriteableMargin with values from mPageFormat.
  // Should be called whenever mPageFormat is initialized or overwritten.
  nsresult InitUnwriteableMargin();

  // The out param has a ref count of 1 on return so caller needs to PMRelase() when done.
  OSStatus CreateDefaultPageFormat(PMPrintSession aSession, PMPageFormat& outFormat);
  OSStatus CreateDefaultPrintSettings(PMPrintSession aSession, PMPrintSettings& outSettings);

  NSPrintInfo* mPrintInfo;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsPrintSettingsX, NS_PRINTSETTINGSX_IID)

#endif // nsPrintSettingsX_h_
