/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsPrintSettingsImpl.h"
#include "nsReadableUtils.h"
#include "nsIPrintSession.h"

#define DEFAULT_MARGIN_WIDTH 0.5

NS_IMPL_ISUPPORTS1(nsPrintSettings, nsIPrintSettings)

/** ---------------------------------------------------
 *  See documentation in nsPrintSettingsImpl.h
 *	@update 6/21/00 dwc
 */
nsPrintSettings::nsPrintSettings() :
  mPrintOptions(0L),
  mPrintRange(kRangeAllPages),
  mStartPageNum(1),
  mEndPageNum(1),
  mScaling(1.0),
  mPrintBGColors(PR_FALSE),
  mPrintBGImages(PR_FALSE),
  mPrintFrameTypeUsage(kUseInternalDefault),
  mPrintFrameType(kFramesAsIs),
  mHowToEnableFrameUI(kFrameEnableNone),
  mIsCancelled(PR_FALSE),
  mPrintSilent(PR_FALSE),
  mPrintPreview(PR_FALSE),
  mShrinkToFit(PR_TRUE),
  mShowPrintProgress(PR_TRUE),
  mPrintPageDelay(50),
  mPaperData(0),
  mPaperSizeType(kPaperSizeDefined),
  mPaperWidth(8.5),
  mPaperHeight(11.0),
  mPaperSizeUnit(kPaperSizeInches),
  mPrintReversed(PR_FALSE),
  mPrintInColor(PR_TRUE),
  mOrientation(kPortraitOrientation),
  mDownloadFonts(PR_FALSE),
  mNumCopies(1),
  mPrintToFile(PR_FALSE),
  mOutputFormat(kOutputFormatNative),
  mIsInitedFromPrinter(PR_FALSE),
  mIsInitedFromPrefs(PR_FALSE)
{

  /* member initializers and constructor code */
  PRInt32 marginWidth = NS_INCHES_TO_INT_TWIPS(DEFAULT_MARGIN_WIDTH);
  mMargin.SizeTo(marginWidth, marginWidth, marginWidth, marginWidth);
  mEdge.SizeTo(0, 0, 0, 0);
  mUnwriteableMargin.SizeTo(0,0,0,0);

  mPrintOptions = kPrintOddPages | kPrintEvenPages;

  mHeaderStrs[0].AssignLiteral("&T");
  mHeaderStrs[2].AssignLiteral("&U");

  mFooterStrs[0].AssignLiteral("&PT"); // Use &P (Page Num Only) or &PT (Page Num of Page Total)
  mFooterStrs[2].AssignLiteral("&D");

}

/** ---------------------------------------------------
 *  See documentation in nsPrintSettingsImpl.h
 *	@update 6/21/00 dwc
 */
nsPrintSettings::nsPrintSettings(const nsPrintSettings& aPS)
{
  *this = aPS;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintSettingsImpl.h
 *	@update 6/21/00 dwc
 */
nsPrintSettings::~nsPrintSettings()
{
}

/* [noscript] attribute nsIPrintSession printSession; */
NS_IMETHODIMP nsPrintSettings::GetPrintSession(nsIPrintSession **aPrintSession)
{
  NS_ENSURE_ARG_POINTER(aPrintSession);
  *aPrintSession = nsnull;
  
  nsCOMPtr<nsIPrintSession> session = do_QueryReferent(mSession);
  if (!session)
    return NS_ERROR_NOT_INITIALIZED;
  *aPrintSession = session;
  NS_ADDREF(*aPrintSession);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPrintSession(nsIPrintSession *aPrintSession)
{
  // Clearing it by passing NULL is not allowed. That's why we
  // use a weak ref so that it doesn't have to be cleared.
  NS_ENSURE_ARG(aPrintSession);
  
  mSession = do_GetWeakReference(aPrintSession);
  if (!mSession) {
    // This may happen if the implementation of this object does
    // not support weak references - programmer error.
    NS_ERROR("Could not get a weak reference from aPrintSession");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

/* attribute long startPageRange; */
NS_IMETHODIMP nsPrintSettings::GetStartPageRange(PRInt32 *aStartPageRange)
{
  //NS_ENSURE_ARG_POINTER(aStartPageRange);
  *aStartPageRange = mStartPageNum;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetStartPageRange(PRInt32 aStartPageRange)
{
  mStartPageNum = aStartPageRange;
  return NS_OK;
}

/* attribute long endPageRange; */
NS_IMETHODIMP nsPrintSettings::GetEndPageRange(PRInt32 *aEndPageRange)
{
  //NS_ENSURE_ARG_POINTER(aEndPageRange);
  *aEndPageRange = mEndPageNum;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetEndPageRange(PRInt32 aEndPageRange)
{
  mEndPageNum = aEndPageRange;
  return NS_OK;
}

/* attribute boolean printReversed; */
NS_IMETHODIMP nsPrintSettings::GetPrintReversed(bool *aPrintReversed)
{
  //NS_ENSURE_ARG_POINTER(aPrintReversed);
  *aPrintReversed = mPrintReversed;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPrintReversed(bool aPrintReversed)
{
  mPrintReversed = aPrintReversed;
  return NS_OK;
}

/* attribute boolean printInColor; */
NS_IMETHODIMP nsPrintSettings::GetPrintInColor(bool *aPrintInColor)
{
  //NS_ENSURE_ARG_POINTER(aPrintInColor);
  *aPrintInColor = mPrintInColor;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPrintInColor(bool aPrintInColor)
{
  mPrintInColor = aPrintInColor;
  return NS_OK;
}

/* attribute short orientation; */
NS_IMETHODIMP nsPrintSettings::GetOrientation(PRInt32 *aOrientation)
{
  NS_ENSURE_ARG_POINTER(aOrientation);
  *aOrientation = mOrientation;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetOrientation(PRInt32 aOrientation)
{
  mOrientation = aOrientation;
  return NS_OK;
}

/* attribute wstring colorspace; */
NS_IMETHODIMP nsPrintSettings::GetColorspace(PRUnichar * *aColorspace)
{
  NS_ENSURE_ARG_POINTER(aColorspace);
  if (!mColorspace.IsEmpty()) {
    *aColorspace = ToNewUnicode(mColorspace);
  } else {
    *aColorspace = nsnull;
  }
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetColorspace(const PRUnichar * aColorspace)
{
  if (aColorspace) {
    mColorspace = aColorspace;
  } else {
    mColorspace.SetLength(0);
  }
  return NS_OK;
}

/* attribute wstring resolutionname; */
NS_IMETHODIMP nsPrintSettings::GetResolutionName(PRUnichar * *aResolutionName)
{
  NS_ENSURE_ARG_POINTER(aResolutionName);
  if (!mResolutionName.IsEmpty()) {
    *aResolutionName = ToNewUnicode(mResolutionName);
  } else {
    *aResolutionName = nsnull;
  }
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetResolutionName(const PRUnichar * aResolutionName)
{
  if (aResolutionName) {
    mResolutionName = aResolutionName;
  } else {
    mResolutionName.SetLength(0);
  }
  return NS_OK;
}

/* attribute boolean downloadFonts; */
NS_IMETHODIMP nsPrintSettings::GetDownloadFonts(bool *aDownloadFonts)
{
  //NS_ENSURE_ARG_POINTER(aDownloadFonts);
  *aDownloadFonts = mDownloadFonts;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetDownloadFonts(bool aDownloadFonts)
{
  mDownloadFonts = aDownloadFonts;
  return NS_OK;
}

/* attribute wstring printer; */
NS_IMETHODIMP nsPrintSettings::GetPrinterName(PRUnichar * *aPrinter)
{
   NS_ENSURE_ARG_POINTER(aPrinter);

   *aPrinter = ToNewUnicode(mPrinter);
   NS_ENSURE_TRUE(*aPrinter, NS_ERROR_OUT_OF_MEMORY);

   return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::SetPrinterName(const PRUnichar * aPrinter)
{
  if (!aPrinter || !mPrinter.Equals(aPrinter)) {
    mIsInitedFromPrinter = PR_FALSE;
    mIsInitedFromPrefs   = PR_FALSE;
  }

  mPrinter.Assign(aPrinter);
  return NS_OK;
}

/* attribute long numCopies; */
NS_IMETHODIMP nsPrintSettings::GetNumCopies(PRInt32 *aNumCopies)
{
  NS_ENSURE_ARG_POINTER(aNumCopies);
  *aNumCopies = mNumCopies;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetNumCopies(PRInt32 aNumCopies)
{
  mNumCopies = aNumCopies;
  return NS_OK;
}

/* attribute wstring printCommand; */
NS_IMETHODIMP nsPrintSettings::GetPrintCommand(PRUnichar * *aPrintCommand)
{
  //NS_ENSURE_ARG_POINTER(aPrintCommand);
  *aPrintCommand = ToNewUnicode(mPrintCommand);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPrintCommand(const PRUnichar * aPrintCommand)
{
  if (aPrintCommand) {
    mPrintCommand = aPrintCommand;
  } else {
    mPrintCommand.SetLength(0);
  }
  return NS_OK;
}

/* attribute boolean printToFile; */
NS_IMETHODIMP nsPrintSettings::GetPrintToFile(bool *aPrintToFile)
{
  //NS_ENSURE_ARG_POINTER(aPrintToFile);
  *aPrintToFile = mPrintToFile;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPrintToFile(bool aPrintToFile)
{
  mPrintToFile = aPrintToFile;
  return NS_OK;
}

/* attribute wstring toFileName; */
NS_IMETHODIMP nsPrintSettings::GetToFileName(PRUnichar * *aToFileName)
{
  //NS_ENSURE_ARG_POINTER(aToFileName);
  *aToFileName = ToNewUnicode(mToFileName);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetToFileName(const PRUnichar * aToFileName)
{
  if (aToFileName) {
    mToFileName = aToFileName;
  } else {
    mToFileName.SetLength(0);
  }
  return NS_OK;
}

/* attribute short outputFormat; */
NS_IMETHODIMP nsPrintSettings::GetOutputFormat(PRInt16 *aOutputFormat)
{
  NS_ENSURE_ARG_POINTER(aOutputFormat);
  *aOutputFormat = mOutputFormat;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetOutputFormat(PRInt16 aOutputFormat)
{
  mOutputFormat = aOutputFormat;
  return NS_OK;
}

/* attribute long printPageDelay; */
NS_IMETHODIMP nsPrintSettings::GetPrintPageDelay(PRInt32 *aPrintPageDelay)
{
  *aPrintPageDelay = mPrintPageDelay;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPrintPageDelay(PRInt32 aPrintPageDelay)
{
  mPrintPageDelay = aPrintPageDelay;
  return NS_OK;
}

/* attribute boolean isInitializedFromPrinter; */
NS_IMETHODIMP nsPrintSettings::GetIsInitializedFromPrinter(bool *aIsInitializedFromPrinter)
{
  NS_ENSURE_ARG_POINTER(aIsInitializedFromPrinter);
  *aIsInitializedFromPrinter = (bool)mIsInitedFromPrinter;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetIsInitializedFromPrinter(bool aIsInitializedFromPrinter)
{
  mIsInitedFromPrinter = (bool)aIsInitializedFromPrinter;
  return NS_OK;
}

/* attribute boolean isInitializedFromPrefs; */
NS_IMETHODIMP nsPrintSettings::GetIsInitializedFromPrefs(bool *aInitializedFromPrefs)
{
  NS_ENSURE_ARG_POINTER(aInitializedFromPrefs);
  *aInitializedFromPrefs = (bool)mIsInitedFromPrefs;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetIsInitializedFromPrefs(bool aInitializedFromPrefs)
{
  mIsInitedFromPrefs = (bool)aInitializedFromPrefs;
  return NS_OK;
}

/* attribute double marginTop; */
NS_IMETHODIMP nsPrintSettings::GetMarginTop(double *aMarginTop)
{
  NS_ENSURE_ARG_POINTER(aMarginTop);
  *aMarginTop = NS_TWIPS_TO_INCHES(mMargin.top);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetMarginTop(double aMarginTop)
{
  mMargin.top = NS_INCHES_TO_INT_TWIPS(float(aMarginTop));
  return NS_OK;
}

/* attribute double marginLeft; */
NS_IMETHODIMP nsPrintSettings::GetMarginLeft(double *aMarginLeft)
{
  NS_ENSURE_ARG_POINTER(aMarginLeft);
  *aMarginLeft = NS_TWIPS_TO_INCHES(mMargin.left);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetMarginLeft(double aMarginLeft)
{
  mMargin.left = NS_INCHES_TO_INT_TWIPS(float(aMarginLeft));
  return NS_OK;
}

/* attribute double marginBottom; */
NS_IMETHODIMP nsPrintSettings::GetMarginBottom(double *aMarginBottom)
{
  NS_ENSURE_ARG_POINTER(aMarginBottom);
  *aMarginBottom = NS_TWIPS_TO_INCHES(mMargin.bottom);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetMarginBottom(double aMarginBottom)
{
  mMargin.bottom = NS_INCHES_TO_INT_TWIPS(float(aMarginBottom));
  return NS_OK;
}

/* attribute double marginRight; */
NS_IMETHODIMP nsPrintSettings::GetMarginRight(double *aMarginRight)
{
  NS_ENSURE_ARG_POINTER(aMarginRight);
  *aMarginRight = NS_TWIPS_TO_INCHES(mMargin.right);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetMarginRight(double aMarginRight)
{
  mMargin.right = NS_INCHES_TO_INT_TWIPS(float(aMarginRight));
  return NS_OK;
}

/* attribute double edgeTop; */
NS_IMETHODIMP nsPrintSettings::GetEdgeTop(double *aEdgeTop)
{
  NS_ENSURE_ARG_POINTER(aEdgeTop);
  *aEdgeTop = NS_TWIPS_TO_INCHES(mEdge.top);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetEdgeTop(double aEdgeTop)
{
  mEdge.top = NS_INCHES_TO_INT_TWIPS(float(aEdgeTop));
  return NS_OK;
}

/* attribute double edgeLeft; */
NS_IMETHODIMP nsPrintSettings::GetEdgeLeft(double *aEdgeLeft)
{
  NS_ENSURE_ARG_POINTER(aEdgeLeft);
  *aEdgeLeft = NS_TWIPS_TO_INCHES(mEdge.left);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetEdgeLeft(double aEdgeLeft)
{
  mEdge.left = NS_INCHES_TO_INT_TWIPS(float(aEdgeLeft));
  return NS_OK;
}

/* attribute double edgeBottom; */
NS_IMETHODIMP nsPrintSettings::GetEdgeBottom(double *aEdgeBottom)
{
  NS_ENSURE_ARG_POINTER(aEdgeBottom);
  *aEdgeBottom = NS_TWIPS_TO_INCHES(mEdge.bottom);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetEdgeBottom(double aEdgeBottom)
{
  mEdge.bottom = NS_INCHES_TO_INT_TWIPS(float(aEdgeBottom));
  return NS_OK;
}

/* attribute double edgeRight; */
NS_IMETHODIMP nsPrintSettings::GetEdgeRight(double *aEdgeRight)
{
  NS_ENSURE_ARG_POINTER(aEdgeRight);
  *aEdgeRight = NS_TWIPS_TO_INCHES(mEdge.right);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetEdgeRight(double aEdgeRight)
{
  mEdge.right = NS_INCHES_TO_INT_TWIPS(float(aEdgeRight));
  return NS_OK;
}

/* attribute double unwriteableMarginTop; */
NS_IMETHODIMP nsPrintSettings::GetUnwriteableMarginTop(double *aUnwriteableMarginTop)
{
  NS_ENSURE_ARG_POINTER(aUnwriteableMarginTop);
  *aUnwriteableMarginTop = NS_TWIPS_TO_INCHES(mUnwriteableMargin.top);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetUnwriteableMarginTop(double aUnwriteableMarginTop)
{
  if (aUnwriteableMarginTop >= 0.0) {
    mUnwriteableMargin.top = NS_INCHES_TO_INT_TWIPS(aUnwriteableMarginTop);
  }
  return NS_OK;
}

/* attribute double unwriteableMarginLeft; */
NS_IMETHODIMP nsPrintSettings::GetUnwriteableMarginLeft(double *aUnwriteableMarginLeft)
{
  NS_ENSURE_ARG_POINTER(aUnwriteableMarginLeft);
  *aUnwriteableMarginLeft = NS_TWIPS_TO_INCHES(mUnwriteableMargin.left);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetUnwriteableMarginLeft(double aUnwriteableMarginLeft)
{
  if (aUnwriteableMarginLeft >= 0.0) {
    mUnwriteableMargin.left = NS_INCHES_TO_INT_TWIPS(aUnwriteableMarginLeft);
  }
  return NS_OK;
}

/* attribute double unwriteableMarginBottom; */
NS_IMETHODIMP nsPrintSettings::GetUnwriteableMarginBottom(double *aUnwriteableMarginBottom)
{
  NS_ENSURE_ARG_POINTER(aUnwriteableMarginBottom);
  *aUnwriteableMarginBottom = NS_TWIPS_TO_INCHES(mUnwriteableMargin.bottom);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetUnwriteableMarginBottom(double aUnwriteableMarginBottom)
{
  if (aUnwriteableMarginBottom >= 0.0) {
    mUnwriteableMargin.bottom = NS_INCHES_TO_INT_TWIPS(aUnwriteableMarginBottom);
  }
  return NS_OK;
}

/* attribute double unwriteableMarginRight; */
NS_IMETHODIMP nsPrintSettings::GetUnwriteableMarginRight(double *aUnwriteableMarginRight)
{
  NS_ENSURE_ARG_POINTER(aUnwriteableMarginRight);
  *aUnwriteableMarginRight = NS_TWIPS_TO_INCHES(mUnwriteableMargin.right);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetUnwriteableMarginRight(double aUnwriteableMarginRight)
{
  if (aUnwriteableMarginRight >= 0.0) {
    mUnwriteableMargin.right = NS_INCHES_TO_INT_TWIPS(aUnwriteableMarginRight);
  }
  return NS_OK;
}

/* attribute double scaling; */
NS_IMETHODIMP nsPrintSettings::GetScaling(double *aScaling)
{
  NS_ENSURE_ARG_POINTER(aScaling);
  *aScaling = mScaling;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::SetScaling(double aScaling)
{
  mScaling = aScaling;
  return NS_OK;
}

/* attribute boolean printBGColors; */
NS_IMETHODIMP nsPrintSettings::GetPrintBGColors(bool *aPrintBGColors)
{
  NS_ENSURE_ARG_POINTER(aPrintBGColors);
  *aPrintBGColors = mPrintBGColors;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPrintBGColors(bool aPrintBGColors)
{
  mPrintBGColors = aPrintBGColors;
  return NS_OK;
}

/* attribute boolean printBGImages; */
NS_IMETHODIMP nsPrintSettings::GetPrintBGImages(bool *aPrintBGImages)
{
  NS_ENSURE_ARG_POINTER(aPrintBGImages);
  *aPrintBGImages = mPrintBGImages;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPrintBGImages(bool aPrintBGImages)
{
  mPrintBGImages = aPrintBGImages;
  return NS_OK;
}

/* attribute long printRange; */
NS_IMETHODIMP nsPrintSettings::GetPrintRange(PRInt16 *aPrintRange)
{
  NS_ENSURE_ARG_POINTER(aPrintRange);
  *aPrintRange = mPrintRange;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPrintRange(PRInt16 aPrintRange)
{
  mPrintRange = aPrintRange;
  return NS_OK;
}

/* attribute wstring docTitle; */
NS_IMETHODIMP nsPrintSettings::GetTitle(PRUnichar * *aTitle)
{
  NS_ENSURE_ARG_POINTER(aTitle);
  if (!mTitle.IsEmpty()) {
    *aTitle = ToNewUnicode(mTitle);
  } else {
    *aTitle = nsnull;
  }
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetTitle(const PRUnichar * aTitle)
{
  if (aTitle) {
    mTitle = aTitle;
  } else {
    mTitle.SetLength(0);
  }
  return NS_OK;
}

/* attribute wstring docURL; */
NS_IMETHODIMP nsPrintSettings::GetDocURL(PRUnichar * *aDocURL)
{
  NS_ENSURE_ARG_POINTER(aDocURL);
  if (!mURL.IsEmpty()) {
    *aDocURL = ToNewUnicode(mURL);
  } else {
    *aDocURL = nsnull;
  }
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetDocURL(const PRUnichar * aDocURL)
{
  if (aDocURL) {
    mURL = aDocURL;
  } else {
    mURL.SetLength(0);
  }
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintSettingsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintSettings::GetPrintOptions(PRInt32 aType, bool *aTurnOnOff)
{
  NS_ENSURE_ARG_POINTER(aTurnOnOff);
  *aTurnOnOff = mPrintOptions & aType ? PR_TRUE : PR_FALSE;
  return NS_OK;
}
/** ---------------------------------------------------
 *  See documentation in nsPrintSettingsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintSettings::SetPrintOptions(PRInt32 aType, bool aTurnOnOff)
{
  if (aTurnOnOff) {
    mPrintOptions |=  aType;
  } else {
    mPrintOptions &= ~aType;
  }
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintSettingsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintSettings::GetPrintOptionsBits(PRInt32 *aBits)
{
  NS_ENSURE_ARG_POINTER(aBits);
  *aBits = mPrintOptions;
  return NS_OK;
}

/* attribute wstring docTitle; */
nsresult 
nsPrintSettings::GetMarginStrs(PRUnichar * *aTitle, 
                              nsHeaderFooterEnum aType, 
                              PRInt16 aJust)
{
  NS_ENSURE_ARG_POINTER(aTitle);
  *aTitle = nsnull;
  if (aType == eHeader) {
    switch (aJust) {
      case kJustLeft:   *aTitle = ToNewUnicode(mHeaderStrs[0]);break;
      case kJustCenter: *aTitle = ToNewUnicode(mHeaderStrs[1]);break;
      case kJustRight:  *aTitle = ToNewUnicode(mHeaderStrs[2]);break;
    } //switch
  } else {
    switch (aJust) {
      case kJustLeft:   *aTitle = ToNewUnicode(mFooterStrs[0]);break;
      case kJustCenter: *aTitle = ToNewUnicode(mFooterStrs[1]);break;
      case kJustRight:  *aTitle = ToNewUnicode(mFooterStrs[2]);break;
    } //switch
  }
  return NS_OK;
}

nsresult
nsPrintSettings::SetMarginStrs(const PRUnichar * aTitle, 
                              nsHeaderFooterEnum aType, 
                              PRInt16 aJust)
{
  NS_ENSURE_ARG_POINTER(aTitle);
  if (aType == eHeader) {
    switch (aJust) {
      case kJustLeft:   mHeaderStrs[0] = aTitle;break;
      case kJustCenter: mHeaderStrs[1] = aTitle;break;
      case kJustRight:  mHeaderStrs[2] = aTitle;break;
    } //switch
  } else {
    switch (aJust) {
      case kJustLeft:   mFooterStrs[0] = aTitle;break;
      case kJustCenter: mFooterStrs[1] = aTitle;break;
      case kJustRight:  mFooterStrs[2] = aTitle;break;
    } //switch
  }
  return NS_OK;
}

/* attribute wstring Header String Left */
NS_IMETHODIMP nsPrintSettings::GetHeaderStrLeft(PRUnichar * *aTitle)
{
  return GetMarginStrs(aTitle, eHeader, kJustLeft);
}
NS_IMETHODIMP nsPrintSettings::SetHeaderStrLeft(const PRUnichar * aTitle)
{
  return SetMarginStrs(aTitle, eHeader, kJustLeft);
}

/* attribute wstring Header String Center */
NS_IMETHODIMP nsPrintSettings::GetHeaderStrCenter(PRUnichar * *aTitle)
{
  return GetMarginStrs(aTitle, eHeader, kJustCenter);
}
NS_IMETHODIMP nsPrintSettings::SetHeaderStrCenter(const PRUnichar * aTitle)
{
  return SetMarginStrs(aTitle, eHeader, kJustCenter);
}

/* attribute wstring Header String Right */
NS_IMETHODIMP nsPrintSettings::GetHeaderStrRight(PRUnichar * *aTitle)
{
  return GetMarginStrs(aTitle, eHeader, kJustRight);
}
NS_IMETHODIMP nsPrintSettings::SetHeaderStrRight(const PRUnichar * aTitle)
{
  return SetMarginStrs(aTitle, eHeader, kJustRight);
}


/* attribute wstring Footer String Left */
NS_IMETHODIMP nsPrintSettings::GetFooterStrLeft(PRUnichar * *aTitle)
{
  return GetMarginStrs(aTitle, eFooter, kJustLeft);
}
NS_IMETHODIMP nsPrintSettings::SetFooterStrLeft(const PRUnichar * aTitle)
{
  return SetMarginStrs(aTitle, eFooter, kJustLeft);
}

/* attribute wstring Footer String Center */
NS_IMETHODIMP nsPrintSettings::GetFooterStrCenter(PRUnichar * *aTitle)
{
  return GetMarginStrs(aTitle, eFooter, kJustCenter);
}
NS_IMETHODIMP nsPrintSettings::SetFooterStrCenter(const PRUnichar * aTitle)
{
  return SetMarginStrs(aTitle, eFooter, kJustCenter);
}

/* attribute wstring Footer String Right */
NS_IMETHODIMP nsPrintSettings::GetFooterStrRight(PRUnichar * *aTitle)
{
  return GetMarginStrs(aTitle, eFooter, kJustRight);
}
NS_IMETHODIMP nsPrintSettings::SetFooterStrRight(const PRUnichar * aTitle)
{
  return SetMarginStrs(aTitle, eFooter, kJustRight);
}

/* attribute short printFrameTypeUsage; */
NS_IMETHODIMP nsPrintSettings::GetPrintFrameTypeUsage(PRInt16 *aPrintFrameTypeUsage)
{
  NS_ENSURE_ARG_POINTER(aPrintFrameTypeUsage);
  *aPrintFrameTypeUsage = mPrintFrameTypeUsage;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPrintFrameTypeUsage(PRInt16 aPrintFrameTypeUsage)
{
  mPrintFrameTypeUsage = aPrintFrameTypeUsage;
  return NS_OK;
}

/* attribute long printFrameType; */
NS_IMETHODIMP nsPrintSettings::GetPrintFrameType(PRInt16 *aPrintFrameType)
{
  NS_ENSURE_ARG_POINTER(aPrintFrameType);
  *aPrintFrameType = (PRInt32)mPrintFrameType;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPrintFrameType(PRInt16 aPrintFrameType)
{
  mPrintFrameType = aPrintFrameType;
  return NS_OK;
}

/* attribute boolean printSilent; */
NS_IMETHODIMP nsPrintSettings::GetPrintSilent(bool *aPrintSilent)
{
  NS_ENSURE_ARG_POINTER(aPrintSilent);
  *aPrintSilent = mPrintSilent;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPrintSilent(bool aPrintSilent)
{
  mPrintSilent = aPrintSilent;
  return NS_OK;
}

/* attribute boolean shrinkToFit; */
NS_IMETHODIMP nsPrintSettings::GetShrinkToFit(bool *aShrinkToFit)
{
  NS_ENSURE_ARG_POINTER(aShrinkToFit);
  *aShrinkToFit = mShrinkToFit;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetShrinkToFit(bool aShrinkToFit)
{
  mShrinkToFit = aShrinkToFit;
  return NS_OK;
}

/* attribute boolean showPrintProgress; */
NS_IMETHODIMP nsPrintSettings::GetShowPrintProgress(bool *aShowPrintProgress)
{
  NS_ENSURE_ARG_POINTER(aShowPrintProgress);
  *aShowPrintProgress = mShowPrintProgress;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetShowPrintProgress(bool aShowPrintProgress)
{
  mShowPrintProgress = aShowPrintProgress;
  return NS_OK;
}

/* attribute wstring paperName; */
NS_IMETHODIMP nsPrintSettings::GetPaperName(PRUnichar * *aPaperName)
{
  NS_ENSURE_ARG_POINTER(aPaperName);
  if (!mPaperName.IsEmpty()) {
    *aPaperName = ToNewUnicode(mPaperName);
  } else {
    *aPaperName = nsnull;
  }
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPaperName(const PRUnichar * aPaperName)
{
  if (aPaperName) {
    mPaperName = aPaperName;
  } else {
    mPaperName.SetLength(0);
  }
  return NS_OK;
}

/* attribute wstring plexName; */
NS_IMETHODIMP nsPrintSettings::GetPlexName(PRUnichar * *aPlexName)
{
  NS_ENSURE_ARG_POINTER(aPlexName);
  if (!mPlexName.IsEmpty()) {
    *aPlexName = ToNewUnicode(mPlexName);
  } else {
    *aPlexName = nsnull;
  }
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPlexName(const PRUnichar * aPlexName)
{
  if (aPlexName) {
    mPlexName = aPlexName;
  } else {
    mPlexName.SetLength(0);
  }
  return NS_OK;
}

/* attribute boolean howToEnableFrameUI; */
NS_IMETHODIMP nsPrintSettings::GetHowToEnableFrameUI(PRInt16 *aHowToEnableFrameUI)
{
  NS_ENSURE_ARG_POINTER(aHowToEnableFrameUI);
  *aHowToEnableFrameUI = mHowToEnableFrameUI;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetHowToEnableFrameUI(PRInt16 aHowToEnableFrameUI)
{
  mHowToEnableFrameUI = aHowToEnableFrameUI;
  return NS_OK;
}

/* attribute long isCancelled; */
NS_IMETHODIMP nsPrintSettings::GetIsCancelled(bool *aIsCancelled)
{
  NS_ENSURE_ARG_POINTER(aIsCancelled);
  *aIsCancelled = mIsCancelled;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetIsCancelled(bool aIsCancelled)
{
  mIsCancelled = aIsCancelled;
  return NS_OK;
}

/* attribute double paperWidth; */
NS_IMETHODIMP nsPrintSettings::GetPaperWidth(double *aPaperWidth)
{
  NS_ENSURE_ARG_POINTER(aPaperWidth);
  *aPaperWidth = mPaperWidth;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPaperWidth(double aPaperWidth)
{
  mPaperWidth = aPaperWidth;
  return NS_OK;
}

/* attribute double paperHeight; */
NS_IMETHODIMP nsPrintSettings::GetPaperHeight(double *aPaperHeight)
{
  NS_ENSURE_ARG_POINTER(aPaperHeight);
  *aPaperHeight = mPaperHeight;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPaperHeight(double aPaperHeight)
{
  mPaperHeight = aPaperHeight;
  return NS_OK;
}

/* attribute short PaperSizeUnit; */
NS_IMETHODIMP nsPrintSettings::GetPaperSizeUnit(PRInt16 *aPaperSizeUnit)
{
  NS_ENSURE_ARG_POINTER(aPaperSizeUnit);
  *aPaperSizeUnit = mPaperSizeUnit;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPaperSizeUnit(PRInt16 aPaperSizeUnit)
{
  mPaperSizeUnit = aPaperSizeUnit;
  return NS_OK;
}

/* attribute short PaperSizeType; */
NS_IMETHODIMP nsPrintSettings::GetPaperSizeType(PRInt16 *aPaperSizeType)
{
  NS_ENSURE_ARG_POINTER(aPaperSizeType);
  *aPaperSizeType = mPaperSizeType;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPaperSizeType(PRInt16 aPaperSizeType)
{
  mPaperSizeType = aPaperSizeType;
  return NS_OK;
}

/* attribute short PaperData; */
NS_IMETHODIMP nsPrintSettings::GetPaperData(PRInt16 *aPaperData)
{
  NS_ENSURE_ARG_POINTER(aPaperData);
  *aPaperData = mPaperData;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPaperData(PRInt16 aPaperData)
{
  mPaperData = aPaperData;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintSettings::SetMarginInTwips(nsIntMargin& aMargin)
{
  mMargin = aMargin;
  return NS_OK;
}

NS_IMETHODIMP 
nsPrintSettings::SetEdgeInTwips(nsIntMargin& aEdge)
{
  mEdge = aEdge;
  return NS_OK;
}

// NOTE: Any subclass implementation of this function should make sure
// to check for negative margin values in aUnwriteableMargin (which 
// would indicate that we should use the system default unwriteable margin.)
NS_IMETHODIMP 
nsPrintSettings::SetUnwriteableMarginInTwips(nsIntMargin& aUnwriteableMargin)
{
  if (aUnwriteableMargin.top >= 0) {
    mUnwriteableMargin.top = aUnwriteableMargin.top;
  }
  if (aUnwriteableMargin.left >= 0) {
    mUnwriteableMargin.left = aUnwriteableMargin.left;
  }
  if (aUnwriteableMargin.bottom >= 0) {
    mUnwriteableMargin.bottom = aUnwriteableMargin.bottom;
  }
  if (aUnwriteableMargin.right >= 0) {
    mUnwriteableMargin.right = aUnwriteableMargin.right;
  }
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
NS_IMETHODIMP 
nsPrintSettings::GetMarginInTwips(nsIntMargin& aMargin)
{
  aMargin = mMargin;
  return NS_OK;
}

NS_IMETHODIMP 
nsPrintSettings::GetEdgeInTwips(nsIntMargin& aEdge)
{
  aEdge = mEdge;
  return NS_OK;
}

NS_IMETHODIMP 
nsPrintSettings::GetUnwriteableMarginInTwips(nsIntMargin& aUnwriteableMargin)
{
  aUnwriteableMargin = mUnwriteableMargin;
  return NS_OK;
}

/** ---------------------------------------------------
 * Stub - platform-specific implementations can use this function.
 */
NS_IMETHODIMP
nsPrintSettings::SetupSilentPrinting()
{
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 */
NS_IMETHODIMP 
nsPrintSettings::GetEffectivePageSize(double *aWidth, double *aHeight)
{
  if (mPaperSizeUnit == kPaperSizeInches) {
    *aWidth  = NS_INCHES_TO_TWIPS(float(mPaperWidth));
    *aHeight = NS_INCHES_TO_TWIPS(float(mPaperHeight));
  } else {
    *aWidth  = NS_MILLIMETERS_TO_TWIPS(float(mPaperWidth));
    *aHeight = NS_MILLIMETERS_TO_TWIPS(float(mPaperHeight));
  }
  if (kLandscapeOrientation == mOrientation) {
    double temp = *aWidth;
    *aWidth = *aHeight;
    *aHeight = temp;
  }
  return NS_OK;
}

nsresult 
nsPrintSettings::_Clone(nsIPrintSettings **_retval)
{
  nsPrintSettings* printSettings = new nsPrintSettings(*this);
  return printSettings->QueryInterface(NS_GET_IID(nsIPrintSettings), (void**)_retval); // ref counts
}

/* nsIPrintSettings clone (); */
NS_IMETHODIMP 
nsPrintSettings::Clone(nsIPrintSettings **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  return _Clone(_retval);
}

/* void assign (in nsIPrintSettings aPS); */
nsresult 
nsPrintSettings::_Assign(nsIPrintSettings *aPS)
{
  nsPrintSettings *ps = static_cast<nsPrintSettings*>(aPS);
  *this = *ps;
  return NS_OK;
}

/* void assign (in nsIPrintSettings aPS); */
NS_IMETHODIMP 
nsPrintSettings::Assign(nsIPrintSettings *aPS)
{
  NS_ENSURE_ARG(aPS);
  return _Assign(aPS);
}

//-------------------------------------------
nsPrintSettings& nsPrintSettings::operator=(const nsPrintSettings& rhs)
{
  if (this == &rhs) {
    return *this;
  }

  mStartPageNum        = rhs.mStartPageNum;
  mEndPageNum          = rhs.mEndPageNum;
  mMargin              = rhs.mMargin;
  mEdge                = rhs.mEdge;
  mUnwriteableMargin   = rhs.mUnwriteableMargin;
  mScaling             = rhs.mScaling;
  mPrintBGColors       = rhs.mPrintBGColors;
  mPrintBGImages       = rhs.mPrintBGImages;
  mPrintRange          = rhs.mPrintRange;
  mTitle               = rhs.mTitle;
  mURL                 = rhs.mURL;
  mHowToEnableFrameUI  = rhs.mHowToEnableFrameUI;
  mIsCancelled         = rhs.mIsCancelled;
  mPrintFrameTypeUsage = rhs.mPrintFrameTypeUsage;
  mPrintFrameType      = rhs.mPrintFrameType;
  mPrintSilent         = rhs.mPrintSilent;
  mShrinkToFit         = rhs.mShrinkToFit;
  mShowPrintProgress   = rhs.mShowPrintProgress;
  mPaperName           = rhs.mPaperName;
  mPlexName            = rhs.mPlexName;
  mPaperSizeType       = rhs.mPaperSizeType;
  mPaperData           = rhs.mPaperData;
  mPaperWidth          = rhs.mPaperWidth;
  mPaperHeight         = rhs.mPaperHeight;
  mPaperSizeUnit       = rhs.mPaperSizeUnit;
  mPrintReversed       = rhs.mPrintReversed;
  mPrintInColor        = rhs.mPrintInColor;
  mOrientation         = rhs.mOrientation;
  mPrintCommand        = rhs.mPrintCommand;
  mNumCopies           = rhs.mNumCopies;
  mPrinter             = rhs.mPrinter;
  mPrintToFile         = rhs.mPrintToFile;
  mToFileName          = rhs.mToFileName;
  mOutputFormat        = rhs.mOutputFormat;
  mPrintPageDelay      = rhs.mPrintPageDelay;

  for (PRInt32 i=0;i<NUM_HEAD_FOOT;i++) {
    mHeaderStrs[i] = rhs.mHeaderStrs[i];
    mFooterStrs[i] = rhs.mFooterStrs[i];
  }

  return *this;
}

