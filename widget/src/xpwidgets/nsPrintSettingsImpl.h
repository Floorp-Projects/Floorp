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
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
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

  virtual nsPrintSettings& operator=(const nsPrintSettings& rhs);

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
 
  nsMargin      mMargin;
  PRInt32       mPrintOptions;

  // scriptable data members
  PRInt16       mPrintRange;
  PRInt32       mStartPageNum; // only used for ePrintRange_SpecifiedRange
  PRInt32       mEndPageNum;
  double        mScaling;
  PRBool        mPrintBGColors;  // print background colors
  PRBool        mPrintBGImages;  // print background images

  PRInt16       mPrintFrameTypeUsage;
  PRInt16       mPrintFrameType;
  PRBool        mHowToEnableFrameUI;
  PRBool        mIsCancelled;
  PRBool        mPrintSilent;
  PRBool        mPrintPreview;
  PRBool        mShrinkToFit;
  PRBool        mShowPrintProgress;
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

  PRBool        mPrintReversed;
  PRBool        mPrintInColor; // a false means grayscale
  PRInt32       mPaperSize;    // see page size consts
  PRInt32       mOrientation;  // see orientation consts
  nsString      mColorspace;
  nsString      mResolutionName;
  PRBool        mDownloadFonts;
  nsString      mPrintCommand;
  PRInt32       mNumCopies;
  nsXPIDLString mPrinter;
  PRBool        mPrintToFile;
  nsString      mToFileName;
  PRInt16       mOutputFormat;
  PRPackedBool  mIsInitedFromPrinter;
  PRPackedBool  mIsInitedFromPrefs;

};

#endif /* nsPrintSettings_h__ */
