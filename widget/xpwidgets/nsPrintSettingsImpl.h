/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintSettingsImpl_h__
#define nsPrintSettingsImpl_h__

#include "gfxCore.h"
#include "nsIPrintSettings.h"  
#include "nsMargin.h"  
#include "nsString.h"
#include "nsWeakReference.h"  

#define NUM_HEAD_FOOT 3

//*****************************************************************************
//***    nsPrintSettings
//*****************************************************************************

class nsPrintSettings : public nsIPrintSettings
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTSETTINGS

  nsPrintSettings();
  nsPrintSettings(const nsPrintSettings& aPS);
  virtual ~nsPrintSettings();

  nsPrintSettings& operator=(const nsPrintSettings& rhs);

protected:
  // May be implemented by the platform-specific derived class                       
  virtual nsresult _Clone(nsIPrintSettings **_retval);
  virtual nsresult _Assign(nsIPrintSettings *aPS);
  
  typedef enum {
    eHeader,
    eFooter
  } nsHeaderFooterEnum;


  nsresult GetMarginStrs(PRUnichar * *aTitle, nsHeaderFooterEnum aType, PRInt16 aJust);
  nsresult SetMarginStrs(const PRUnichar * aTitle, nsHeaderFooterEnum aType, PRInt16 aJust);

  // Members
  nsWeakPtr     mSession; // Should never be touched by Clone or Assign
 
  // mMargin, mEdge, and mUnwriteableMargin are stored in twips
  nsIntMargin   mMargin;
  nsIntMargin   mEdge;
  nsIntMargin   mUnwriteableMargin;

  PRInt32       mPrintOptions;

  // scriptable data members
  PRInt16       mPrintRange;
  PRInt32       mStartPageNum; // only used for ePrintRange_SpecifiedRange
  PRInt32       mEndPageNum;
  double        mScaling;
  bool          mPrintBGColors;  // print background colors
  bool          mPrintBGImages;  // print background images

  PRInt16       mPrintFrameTypeUsage;
  PRInt16       mPrintFrameType;
  PRInt16       mHowToEnableFrameUI;
  bool          mIsCancelled;
  bool          mPrintSilent;
  bool          mPrintPreview;
  bool          mShrinkToFit;
  bool          mShowPrintProgress;
  PRInt32       mPrintPageDelay;

  nsString      mTitle;
  nsString      mURL;
  nsString      mPageNumberFormat;
  nsString      mHeaderStrs[NUM_HEAD_FOOT];
  nsString      mFooterStrs[NUM_HEAD_FOOT];

  nsString      mPaperName;
  nsString      mPlexName;
  PRInt16       mPaperData;
  PRInt16       mPaperSizeType;
  double        mPaperWidth;
  double        mPaperHeight;
  PRInt16       mPaperSizeUnit;

  bool          mPrintReversed;
  bool          mPrintInColor; // a false means grayscale
  PRInt32       mOrientation;  // see orientation consts
  nsString      mColorspace;
  nsString      mResolutionName;
  bool          mDownloadFonts;
  nsString      mPrintCommand;
  PRInt32       mNumCopies;
  nsXPIDLString mPrinter;
  bool          mPrintToFile;
  nsString      mToFileName;
  PRInt16       mOutputFormat;
  bool          mIsInitedFromPrinter;
  bool          mIsInitedFromPrefs;

};

#endif /* nsPrintSettings_h__ */
