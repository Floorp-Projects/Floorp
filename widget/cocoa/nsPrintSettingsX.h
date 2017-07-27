/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintSettingsX_h_
#define nsPrintSettingsX_h_

#include "nsPrintSettingsImpl.h"  
#import <Cocoa/Cocoa.h>

// The constants for paper orientation were renamed in 10.9. __MAC_10_9 is
// defined on OS X 10.9 and later. Although 10.8 and earlier are not supported
// at this time, this allows for building on those older OS versions. The
// values are consistent across OS versions so the rename does not affect
// runtime, just compilation.
#ifdef __MAC_10_9
#define NS_PAPER_ORIENTATION_PORTRAIT   (NSPaperOrientationPortrait)
#define NS_PAPER_ORIENTATION_LANDSCAPE  (NSPaperOrientationLandscape)
#else
#define NS_PAPER_ORIENTATION_PORTRAIT   (NSPortraitOrientation)
#define NS_PAPER_ORIENTATION_LANDSCAPE  (NSLandscapeOrientation)
#endif

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
  virtual nsresult GetEffectivePageSize(double *aWidth,
      double *aHeight) override;
  void GetFilePageSize(double *aWidth, double *aHeight);

  // In addition to setting the paper width and height, these
  // overrides set the adjusted width and height returned from
  // GetEffectivePageSize. This is needed when a paper size is
  // set manually without using a print dialog a la reftest-paged.
  virtual nsresult SetPaperWidth(double aPaperWidth) override;
  virtual nsresult SetPaperHeight(double aPaperWidth) override;

  PMPrintSettings GetPMPrintSettings();
  PMPrintSession GetPMPrintSession();
  PMPageFormat GetPMPageFormat();
  void SetPMPageFormat(PMPageFormat aPageFormat);

  // Re-initialize mUnwriteableMargin with values from mPageFormat.
  // Should be called whenever mPageFormat is initialized or overwritten.
  nsresult InitUnwriteableMargin();

  // Re-initialize mAdjustedPaper{Width,Height} with values from mPageFormat.
  // Should be called whenever mPageFormat is initialized or overwritten.
  nsresult InitAdjustedPaperSize();

  void SetInchesScale(float aWidthScale, float aHeightScale);
  void GetInchesScale(float *aWidthScale, float *aHeightScale);

  NS_IMETHOD SetScaling(double aScaling) override;
  NS_IMETHOD GetScaling(double *aScaling) override;

  NS_IMETHOD SetToFileName(const char16_t * aToFileName) override;

  NS_IMETHOD GetOrientation(int32_t *aOrientation) override;
  NS_IMETHOD SetOrientation(int32_t aOrientation) override;

  NS_IMETHOD SetUnwriteableMarginTop(double aUnwriteableMarginTop) override;
  NS_IMETHOD SetUnwriteableMarginLeft(double aUnwriteableMarginLeft) override;
  NS_IMETHOD SetUnwriteableMarginBottom(double aUnwriteableMarginBottom) override;
  NS_IMETHOD SetUnwriteableMarginRight(double aUnwriteableMarginRight) override;

  void SetAdjustedPaperSize(double aWidth, double aHeight);
  void GetAdjustedPaperSize(double *aWidth, double *aHeight);
  nsresult SetCocoaPaperSize(double aWidth, double aHeight);

protected:
  virtual ~nsPrintSettingsX();

  nsPrintSettingsX(const nsPrintSettingsX& src);
  nsPrintSettingsX& operator=(const nsPrintSettingsX& rhs);

  nsresult _Clone(nsIPrintSettings **_retval) override;
  nsresult _Assign(nsIPrintSettings *aPS) override;

  int GetCocoaUnit(int16_t aGeckoUnit);

  // The out param has a ref count of 1 on return so caller needs to PMRelase() when done.
  OSStatus CreateDefaultPageFormat(PMPrintSession aSession, PMPageFormat& outFormat);
  OSStatus CreateDefaultPrintSettings(PMPrintSession aSession, PMPrintSettings& outSettings);

  NSPrintInfo* mPrintInfo;

  // Scaling factors used to convert the NSPrintInfo
  // paper size units to inches
  float mWidthScale;
  float mHeightScale;
  double mAdjustedPaperWidth;
  double mAdjustedPaperHeight;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsPrintSettingsX, NS_PRINTSETTINGSX_IID)

#endif // nsPrintSettingsX_h_
