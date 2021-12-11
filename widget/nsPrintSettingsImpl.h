/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintSettingsImpl_h__
#define nsPrintSettingsImpl_h__

#include "nsIPrintSettings.h"
#include "nsIWeakReferenceUtils.h"
#include "nsMargin.h"
#include "nsPaper.h"
#include "nsString.h"

#define NUM_HEAD_FOOT 3

//*****************************************************************************
//***    nsPrintSettings
//*****************************************************************************

namespace mozilla {

/**
 * A struct that can be used off the main thread to collect printer-specific
 * info that can be used to initialized a default nsIPrintSettings object.
 */
struct PrintSettingsInitializer {
  nsString mPrinter;
  PaperInfo mPaperInfo;
  // If we fail to obtain printer capabilities, being given the option to print
  // in color to your monochrome printer is a lot less annoying than not being
  // given the option to print in color to your color printer.
  bool mPrintInColor = true;
  int mResolution = 0;
#ifdef XP_WIN
  CopyableTArray<uint8_t> mDevmodeWStorage;
#endif
};

}  // namespace mozilla

class nsPrintSettings : public nsIPrintSettings {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTSETTINGS
  using PrintSettingsInitializer = mozilla::PrintSettingsInitializer;

  nsPrintSettings();
  nsPrintSettings(const nsPrintSettings& aPS);

  /**
   * Initialize relevant members from the PrintSettingsInitializer.
   * This is specifically not a constructor so that we can ensure that the
   * relevant setters are dynamically dispatched to derived classes.
   */
  virtual void InitWithInitializer(const PrintSettingsInitializer& aSettings);

  nsPrintSettings& operator=(const nsPrintSettings& rhs);

  // Sets a default file name for the print settings.
  void SetDefaultFileName();

 protected:
  virtual ~nsPrintSettings();

  // May be implemented by the platform-specific derived class
  virtual nsresult _Clone(nsIPrintSettings** _retval);
  virtual nsresult _Assign(nsIPrintSettings* aPS);

  typedef enum { eHeader, eFooter } nsHeaderFooterEnum;

  // Members
  nsWeakPtr mSession;  // Should never be touched by Clone or Assign

  // mMargin, mEdge, and mUnwriteableMargin are stored in twips
  nsIntMargin mMargin;
  nsIntMargin mEdge;
  nsIntMargin mUnwriteableMargin;

  nsTArray<int32_t> mPageRanges;

  double mScaling;
  bool mPrintBGColors;  // print background colors
  bool mPrintBGImages;  // print background images

  bool mIsCancelled;
  bool mSaveOnCancel;
  bool mPrintSilent;
  bool mShrinkToFit;
  bool mShowPrintProgress;
  bool mShowMarginGuides;
  bool mHonorPageRuleMargins;
  bool mIsPrintSelectionRBEnabled;
  bool mPrintSelectionOnly;
  int32_t mPrintPageDelay;

  nsString mTitle;
  nsString mURL;
  nsString mHeaderStrs[NUM_HEAD_FOOT];
  nsString mFooterStrs[NUM_HEAD_FOOT];

  nsString mPaperId;
  double mPaperWidth;
  double mPaperHeight;
  int16_t mPaperSizeUnit;

  bool mPrintReversed;
  bool mPrintInColor;    // a false means grayscale
  int32_t mOrientation;  // see orientation consts
  int32_t mResolution;
  int32_t mDuplex;
  int32_t mNumCopies;
  int32_t mNumPagesPerSheet;
  nsString mPrinter;
  bool mPrintToFile;
  nsString mToFileName;
  int16_t mOutputFormat;
  bool mIsInitedFromPrinter;
  bool mIsInitedFromPrefs;
};

#endif /* nsPrintSettings_h__ */
