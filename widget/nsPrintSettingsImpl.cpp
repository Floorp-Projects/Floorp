/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintSettingsImpl.h"

#include "prenv.h"
#include "nsCoord.h"
#include "nsPaper.h"
#include "nsReadableUtils.h"
#include "nsIPrintSession.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/RefPtr.h"

#define DEFAULT_MARGIN_WIDTH 0.5

NS_IMPL_ISUPPORTS(nsPrintSettings, nsIPrintSettings)

nsPrintSettings::nsPrintSettings()
    : mScaling(1.0),
      mPrintBGColors(false),
      mPrintBGImages(false),
      mIsCancelled(false),
      mSaveOnCancel(true),
      mPrintSilent(false),
      mShrinkToFit(true),
      mShowPrintProgress(true),
      mShowMarginGuides(false),
      mHonorPageRuleMargins(true),
      mIsPrintSelectionRBEnabled(false),
      mPrintSelectionOnly(false),
      mPrintPageDelay(50),
      mPaperWidth(8.5),
      mPaperHeight(11.0),
      mPaperSizeUnit(kPaperSizeInches),
      mPrintReversed(false),
      mPrintInColor(true),
      mOrientation(kPortraitOrientation),
      mResolution(0),
      mDuplex(0),
      mNumCopies(1),
      mNumPagesPerSheet(1),
      mPrintToFile(false),
      mOutputFormat(kOutputFormatNative),
      mIsInitedFromPrinter(false),
      mIsInitedFromPrefs(false) {
  /* member initializers and constructor code */
  int32_t marginWidth = NS_INCHES_TO_INT_TWIPS(DEFAULT_MARGIN_WIDTH);
  mMargin.SizeTo(marginWidth, marginWidth, marginWidth, marginWidth);
  mEdge.SizeTo(0, 0, 0, 0);
  mUnwriteableMargin.SizeTo(0, 0, 0, 0);

  mHeaderStrs[0].AssignLiteral("&T");
  mHeaderStrs[2].AssignLiteral("&U");

  mFooterStrs[0].AssignLiteral(
      "&PT");  // Use &P (Page Num Only) or &PT (Page Num of Page Total)
  mFooterStrs[2].AssignLiteral("&D");
}

void nsPrintSettings::InitWithInitializer(
    const PrintSettingsInitializer& aSettings) {
  const double kInchesPerPoint = 1.0 / 72.0;

  SetPrinterName(aSettings.mPrinter);
  SetPrintInColor(aSettings.mPrintInColor);
  SetResolution(aSettings.mResolution);
  // The paper ID used by nsPrintSettings is the non-localizable identifier
  // exposed as "id" by the paper, not the potentially localized human-friendly
  // "name", which could change, e.g. if the user changes their system locale.
  SetPaperId(aSettings.mPaperInfo.mId);
  SetPaperWidth(aSettings.mPaperInfo.mSize.Width() * kInchesPerPoint);
  SetPaperHeight(aSettings.mPaperInfo.mSize.Height() * kInchesPerPoint);
  SetPaperSizeUnit(nsIPrintSettings::kPaperSizeInches);

  if (aSettings.mPaperInfo.mUnwriteableMargin) {
    const auto& margin = aSettings.mPaperInfo.mUnwriteableMargin.value();
    // Margins are stored internally in TWIPS, but the setters expect inches.
    SetUnwriteableMarginTop(margin.top * kInchesPerPoint);
    SetUnwriteableMarginRight(margin.right * kInchesPerPoint);
    SetUnwriteableMarginBottom(margin.bottom * kInchesPerPoint);
    SetUnwriteableMarginLeft(margin.left * kInchesPerPoint);
  }

  // Set this last because other setters may overwrite its value.
  SetIsInitializedFromPrinter(true);
}

nsPrintSettings::nsPrintSettings(const nsPrintSettings& aPS) { *this = aPS; }

nsPrintSettings::~nsPrintSettings() = default;

NS_IMETHODIMP nsPrintSettings::GetPrintSession(
    nsIPrintSession** aPrintSession) {
  NS_ENSURE_ARG_POINTER(aPrintSession);
  *aPrintSession = nullptr;

  nsCOMPtr<nsIPrintSession> session = do_QueryReferent(mSession);
  if (!session) return NS_ERROR_NOT_INITIALIZED;
  *aPrintSession = session;
  NS_ADDREF(*aPrintSession);
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::SetPrintSession(nsIPrintSession* aPrintSession) {
  // Clearing it by passing nullptr is not allowed. That's why we
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

NS_IMETHODIMP nsPrintSettings::GetPrintReversed(bool* aPrintReversed) {
  *aPrintReversed = mPrintReversed;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPrintReversed(bool aPrintReversed) {
  mPrintReversed = aPrintReversed;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetPrintInColor(bool* aPrintInColor) {
  *aPrintInColor = mPrintInColor;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPrintInColor(bool aPrintInColor) {
  mPrintInColor = aPrintInColor;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetOrientation(int32_t* aOrientation) {
  *aOrientation = mOrientation;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetOrientation(int32_t aOrientation) {
  mOrientation = aOrientation;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetResolution(int32_t* aResolution) {
  NS_ENSURE_ARG_POINTER(aResolution);
  *aResolution = mResolution;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetResolution(const int32_t aResolution) {
  mResolution = aResolution;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetDuplex(int32_t* aDuplex) {
  NS_ENSURE_ARG_POINTER(aDuplex);
  *aDuplex = mDuplex;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetDuplex(const int32_t aDuplex) {
  mDuplex = aDuplex;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetPrinterName(nsAString& aPrinter) {
  aPrinter = mPrinter;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::SetPrinterName(const nsAString& aPrinter) {
  if (!mPrinter.Equals(aPrinter)) {
    mIsInitedFromPrinter = false;
    mIsInitedFromPrefs = false;
  }

  mPrinter.Assign(aPrinter);
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetNumCopies(int32_t* aNumCopies) {
  NS_ENSURE_ARG_POINTER(aNumCopies);
  *aNumCopies = mNumCopies;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetNumCopies(int32_t aNumCopies) {
  mNumCopies = aNumCopies;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetNumPagesPerSheet(int32_t* aNumPagesPerSheet) {
  NS_ENSURE_ARG_POINTER(aNumPagesPerSheet);
  *aNumPagesPerSheet = mNumPagesPerSheet;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetNumPagesPerSheet(int32_t aNumPagesPerSheet) {
  mNumPagesPerSheet = aNumPagesPerSheet;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetPrintToFile(bool* aPrintToFile) {
  NS_ENSURE_ARG_POINTER(aPrintToFile);
  *aPrintToFile = mPrintToFile;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPrintToFile(bool aPrintToFile) {
  mPrintToFile = aPrintToFile;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetToFileName(nsAString& aToFileName) {
  aToFileName = mToFileName;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetToFileName(const nsAString& aToFileName) {
  mToFileName = aToFileName;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetOutputFormat(int16_t* aOutputFormat) {
  NS_ENSURE_ARG_POINTER(aOutputFormat);
  *aOutputFormat = mOutputFormat;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetOutputFormat(int16_t aOutputFormat) {
  mOutputFormat = aOutputFormat;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetPrintPageDelay(int32_t* aPrintPageDelay) {
  *aPrintPageDelay = mPrintPageDelay;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPrintPageDelay(int32_t aPrintPageDelay) {
  mPrintPageDelay = aPrintPageDelay;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetIsInitializedFromPrinter(
    bool* aIsInitializedFromPrinter) {
  NS_ENSURE_ARG_POINTER(aIsInitializedFromPrinter);
  *aIsInitializedFromPrinter = mIsInitedFromPrinter;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetIsInitializedFromPrinter(
    bool aIsInitializedFromPrinter) {
  mIsInitedFromPrinter = aIsInitializedFromPrinter;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetIsInitializedFromPrefs(
    bool* aInitializedFromPrefs) {
  NS_ENSURE_ARG_POINTER(aInitializedFromPrefs);
  *aInitializedFromPrefs = mIsInitedFromPrefs;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetIsInitializedFromPrefs(
    bool aInitializedFromPrefs) {
  mIsInitedFromPrefs = aInitializedFromPrefs;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetMarginTop(double* aMarginTop) {
  NS_ENSURE_ARG_POINTER(aMarginTop);
  *aMarginTop = NS_TWIPS_TO_INCHES(mMargin.top);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetMarginTop(double aMarginTop) {
  mMargin.top = NS_INCHES_TO_INT_TWIPS(float(aMarginTop));
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetMarginLeft(double* aMarginLeft) {
  NS_ENSURE_ARG_POINTER(aMarginLeft);
  *aMarginLeft = NS_TWIPS_TO_INCHES(mMargin.left);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetMarginLeft(double aMarginLeft) {
  mMargin.left = NS_INCHES_TO_INT_TWIPS(float(aMarginLeft));
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetMarginBottom(double* aMarginBottom) {
  NS_ENSURE_ARG_POINTER(aMarginBottom);
  *aMarginBottom = NS_TWIPS_TO_INCHES(mMargin.bottom);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetMarginBottom(double aMarginBottom) {
  mMargin.bottom = NS_INCHES_TO_INT_TWIPS(float(aMarginBottom));
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetMarginRight(double* aMarginRight) {
  NS_ENSURE_ARG_POINTER(aMarginRight);
  *aMarginRight = NS_TWIPS_TO_INCHES(mMargin.right);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetMarginRight(double aMarginRight) {
  mMargin.right = NS_INCHES_TO_INT_TWIPS(float(aMarginRight));
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetEdgeTop(double* aEdgeTop) {
  NS_ENSURE_ARG_POINTER(aEdgeTop);
  *aEdgeTop = NS_TWIPS_TO_INCHES(mEdge.top);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetEdgeTop(double aEdgeTop) {
  mEdge.top = NS_INCHES_TO_INT_TWIPS(float(aEdgeTop));
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetEdgeLeft(double* aEdgeLeft) {
  NS_ENSURE_ARG_POINTER(aEdgeLeft);
  *aEdgeLeft = NS_TWIPS_TO_INCHES(mEdge.left);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetEdgeLeft(double aEdgeLeft) {
  mEdge.left = NS_INCHES_TO_INT_TWIPS(float(aEdgeLeft));
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetEdgeBottom(double* aEdgeBottom) {
  NS_ENSURE_ARG_POINTER(aEdgeBottom);
  *aEdgeBottom = NS_TWIPS_TO_INCHES(mEdge.bottom);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetEdgeBottom(double aEdgeBottom) {
  mEdge.bottom = NS_INCHES_TO_INT_TWIPS(float(aEdgeBottom));
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetEdgeRight(double* aEdgeRight) {
  NS_ENSURE_ARG_POINTER(aEdgeRight);
  *aEdgeRight = NS_TWIPS_TO_INCHES(mEdge.right);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetEdgeRight(double aEdgeRight) {
  mEdge.right = NS_INCHES_TO_INT_TWIPS(float(aEdgeRight));
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetUnwriteableMarginTop(
    double* aUnwriteableMarginTop) {
  NS_ENSURE_ARG_POINTER(aUnwriteableMarginTop);
  *aUnwriteableMarginTop = NS_TWIPS_TO_INCHES(mUnwriteableMargin.top);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetUnwriteableMarginTop(
    double aUnwriteableMarginTop) {
  if (aUnwriteableMarginTop >= 0.0) {
    mUnwriteableMargin.top = NS_INCHES_TO_INT_TWIPS(aUnwriteableMarginTop);
  }
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetUnwriteableMarginLeft(
    double* aUnwriteableMarginLeft) {
  NS_ENSURE_ARG_POINTER(aUnwriteableMarginLeft);
  *aUnwriteableMarginLeft = NS_TWIPS_TO_INCHES(mUnwriteableMargin.left);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetUnwriteableMarginLeft(
    double aUnwriteableMarginLeft) {
  if (aUnwriteableMarginLeft >= 0.0) {
    mUnwriteableMargin.left = NS_INCHES_TO_INT_TWIPS(aUnwriteableMarginLeft);
  }
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetUnwriteableMarginBottom(
    double* aUnwriteableMarginBottom) {
  NS_ENSURE_ARG_POINTER(aUnwriteableMarginBottom);
  *aUnwriteableMarginBottom = NS_TWIPS_TO_INCHES(mUnwriteableMargin.bottom);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetUnwriteableMarginBottom(
    double aUnwriteableMarginBottom) {
  if (aUnwriteableMarginBottom >= 0.0) {
    mUnwriteableMargin.bottom =
        NS_INCHES_TO_INT_TWIPS(aUnwriteableMarginBottom);
  }
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetUnwriteableMarginRight(
    double* aUnwriteableMarginRight) {
  NS_ENSURE_ARG_POINTER(aUnwriteableMarginRight);
  *aUnwriteableMarginRight = NS_TWIPS_TO_INCHES(mUnwriteableMargin.right);
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetUnwriteableMarginRight(
    double aUnwriteableMarginRight) {
  if (aUnwriteableMarginRight >= 0.0) {
    mUnwriteableMargin.right = NS_INCHES_TO_INT_TWIPS(aUnwriteableMarginRight);
  }
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetScaling(double* aScaling) {
  NS_ENSURE_ARG_POINTER(aScaling);
  *aScaling = mScaling;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::SetScaling(double aScaling) {
  mScaling = aScaling;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetPrintBGColors(bool* aPrintBGColors) {
  NS_ENSURE_ARG_POINTER(aPrintBGColors);
  *aPrintBGColors = mPrintBGColors;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPrintBGColors(bool aPrintBGColors) {
  mPrintBGColors = aPrintBGColors;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetPrintBGImages(bool* aPrintBGImages) {
  NS_ENSURE_ARG_POINTER(aPrintBGImages);
  *aPrintBGImages = mPrintBGImages;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPrintBGImages(bool aPrintBGImages) {
  mPrintBGImages = aPrintBGImages;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetTitle(nsAString& aTitle) {
  aTitle = mTitle;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetTitle(const nsAString& aTitle) {
  mTitle = aTitle;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetDocURL(nsAString& aDocURL) {
  aDocURL = mURL;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetDocURL(const nsAString& aDocURL) {
  mURL = aDocURL;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetHeaderStrLeft(nsAString& aTitle) {
  aTitle = mHeaderStrs[0];
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetHeaderStrLeft(const nsAString& aTitle) {
  mHeaderStrs[0] = aTitle;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetHeaderStrCenter(nsAString& aTitle) {
  aTitle = mHeaderStrs[1];
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetHeaderStrCenter(const nsAString& aTitle) {
  mHeaderStrs[1] = aTitle;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetHeaderStrRight(nsAString& aTitle) {
  aTitle = mHeaderStrs[2];
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetHeaderStrRight(const nsAString& aTitle) {
  mHeaderStrs[2] = aTitle;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetFooterStrLeft(nsAString& aTitle) {
  aTitle = mFooterStrs[0];
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetFooterStrLeft(const nsAString& aTitle) {
  mFooterStrs[0] = aTitle;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetFooterStrCenter(nsAString& aTitle) {
  aTitle = mFooterStrs[1];
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetFooterStrCenter(const nsAString& aTitle) {
  mFooterStrs[1] = aTitle;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetFooterStrRight(nsAString& aTitle) {
  aTitle = mFooterStrs[2];
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetFooterStrRight(const nsAString& aTitle) {
  mFooterStrs[2] = aTitle;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetPrintSilent(bool* aPrintSilent) {
  NS_ENSURE_ARG_POINTER(aPrintSilent);
  *aPrintSilent = mPrintSilent;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPrintSilent(bool aPrintSilent) {
  mPrintSilent = aPrintSilent;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetShrinkToFit(bool* aShrinkToFit) {
  NS_ENSURE_ARG_POINTER(aShrinkToFit);
  *aShrinkToFit = mShrinkToFit;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetShrinkToFit(bool aShrinkToFit) {
  mShrinkToFit = aShrinkToFit;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetShowPrintProgress(bool* aShowPrintProgress) {
  NS_ENSURE_ARG_POINTER(aShowPrintProgress);
  *aShowPrintProgress = mShowPrintProgress;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetShowPrintProgress(bool aShowPrintProgress) {
  mShowPrintProgress = aShowPrintProgress;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetShowMarginGuides(bool* aShowMarginGuides) {
  *aShowMarginGuides = mShowMarginGuides;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::SetShowMarginGuides(bool aShowMarginGuides) {
  mShowMarginGuides = aShowMarginGuides;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetHonorPageRuleMargins(bool* aResult) {
  *aResult = mHonorPageRuleMargins;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::SetHonorPageRuleMargins(bool aHonor) {
  mHonorPageRuleMargins = aHonor;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetIsPrintSelectionRBEnabled(
    bool* aIsPrintSelectionRBEnabled) {
  *aIsPrintSelectionRBEnabled = mIsPrintSelectionRBEnabled;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetIsPrintSelectionRBEnabled(
    bool aIsPrintSelectionRBEnabled) {
  mIsPrintSelectionRBEnabled = aIsPrintSelectionRBEnabled;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetPrintSelectionOnly(bool* aResult) {
  *aResult = mPrintSelectionOnly;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::SetPrintSelectionOnly(bool aSelectionOnly) {
  mPrintSelectionOnly = aSelectionOnly;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetPaperId(nsAString& aPaperId) {
  aPaperId = mPaperId;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPaperId(const nsAString& aPaperId) {
  mPaperId = aPaperId;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetIsCancelled(bool* aIsCancelled) {
  NS_ENSURE_ARG_POINTER(aIsCancelled);
  *aIsCancelled = mIsCancelled;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetIsCancelled(bool aIsCancelled) {
  mIsCancelled = aIsCancelled;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetSaveOnCancel(bool* aSaveOnCancel) {
  *aSaveOnCancel = mSaveOnCancel;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetPaperWidth(double* aPaperWidth) {
  NS_ENSURE_ARG_POINTER(aPaperWidth);
  *aPaperWidth = mPaperWidth;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPaperWidth(double aPaperWidth) {
  mPaperWidth = aPaperWidth;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetPaperHeight(double* aPaperHeight) {
  NS_ENSURE_ARG_POINTER(aPaperHeight);
  *aPaperHeight = mPaperHeight;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPaperHeight(double aPaperHeight) {
  mPaperHeight = aPaperHeight;
  return NS_OK;
}

NS_IMETHODIMP nsPrintSettings::GetPaperSizeUnit(int16_t* aPaperSizeUnit) {
  NS_ENSURE_ARG_POINTER(aPaperSizeUnit);
  *aPaperSizeUnit = mPaperSizeUnit;
  return NS_OK;
}
NS_IMETHODIMP nsPrintSettings::SetPaperSizeUnit(int16_t aPaperSizeUnit) {
  mPaperSizeUnit = aPaperSizeUnit;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintSettingsService.h
 *	@update 6/21/00 dwc
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP
nsPrintSettings::SetMarginInTwips(nsIntMargin& aMargin) {
  mMargin = aMargin;
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettings::SetEdgeInTwips(nsIntMargin& aEdge) {
  mEdge = aEdge;
  return NS_OK;
}

// NOTE: Any subclass implementation of this function should make sure
// to check for negative margin values in aUnwriteableMargin (which
// would indicate that we should use the system default unwriteable margin.)
NS_IMETHODIMP
nsPrintSettings::SetUnwriteableMarginInTwips(nsIntMargin& aUnwriteableMargin) {
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

nsIntMargin nsPrintSettings::GetMarginInTwips() { return mMargin; }

nsIntMargin nsPrintSettings::GetEdgeInTwips() { return mEdge; }

nsIntMargin nsPrintSettings::GetUnwriteableMarginInTwips() {
  return mUnwriteableMargin;
}

/** ---------------------------------------------------
 * Stub - platform-specific implementations can use this function.
 */
NS_IMETHODIMP
nsPrintSettings::SetupSilentPrinting() { return NS_OK; }

/** ---------------------------------------------------
 *  See documentation in nsPrintSettingsService.h
 */
NS_IMETHODIMP
nsPrintSettings::GetEffectivePageSize(double* aWidth, double* aHeight) {
  if (mPaperSizeUnit == kPaperSizeInches) {
    *aWidth = NS_INCHES_TO_TWIPS(float(mPaperWidth));
    *aHeight = NS_INCHES_TO_TWIPS(float(mPaperHeight));
  } else {
    MOZ_ASSERT(mPaperSizeUnit == kPaperSizeMillimeters,
               "unexpected paper size unit");
    *aWidth = NS_MILLIMETERS_TO_TWIPS(float(mPaperWidth));
    *aHeight = NS_MILLIMETERS_TO_TWIPS(float(mPaperHeight));
  }
  if (kLandscapeOrientation == mOrientation) {
    double temp = *aWidth;
    *aWidth = *aHeight;
    *aHeight = temp;
  }
  return NS_OK;
}

bool nsPrintSettings::HasOrthogonalSheetsAndPages() {
  return mNumPagesPerSheet == 2 || mNumPagesPerSheet == 6;
}

void nsPrintSettings::GetEffectiveSheetSize(double* aWidth, double* aHeight) {
  mozilla::DebugOnly<nsresult> rv = GetEffectivePageSize(aWidth, aHeight);

  // Our GetEffectivePageSize impls only return NS_OK, so this should hold:
  MOZ_ASSERT(NS_SUCCEEDED(rv), "Uh oh, GetEffectivePageSize failed");

  if (HasOrthogonalSheetsAndPages()) {
    std::swap(*aWidth, *aHeight);
  }
}

int32_t nsPrintSettings::GetSheetOrientation() {
  if (HasOrthogonalSheetsAndPages()) {
    // Sheet orientation is rotated with respect to the page orientation.
    return kLandscapeOrientation == mOrientation ? kPortraitOrientation
                                                 : kLandscapeOrientation;
  }

  // Sheet orientation is the same as the page orientation.
  return mOrientation;
}

NS_IMETHODIMP
nsPrintSettings::SetPageRanges(const nsTArray<int32_t>& aPages) {
  // Needs to be a set of (start, end) pairs.
  if (aPages.Length() % 2 != 0) {
    return NS_ERROR_FAILURE;
  }
  mPageRanges = aPages.Clone();
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettings::GetPageRanges(nsTArray<int32_t>& aPages) {
  aPages = mPageRanges.Clone();
  return NS_OK;
}

bool nsIPrintSettings::IsPageSkipped(int32_t aPageNum,
                                     const nsTArray<int32_t>& aRanges) {
  MOZ_RELEASE_ASSERT(aRanges.Length() % 2 == 0);
  if (aRanges.IsEmpty()) {
    return false;
  }
  for (size_t i = 0; i < aRanges.Length(); i += 2) {
    if (aRanges[i] <= aPageNum && aPageNum <= aRanges[i + 1]) {
      // The page is included in this piece of the custom range,
      // so it's not skipped.
      return false;
    }
  }
  return true;
}

nsresult nsPrintSettings::_Clone(nsIPrintSettings** _retval) {
  RefPtr<nsPrintSettings> printSettings = new nsPrintSettings(*this);
  printSettings.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettings::Clone(nsIPrintSettings** _retval) {
  NS_ENSURE_ARG_POINTER(_retval);
  return _Clone(_retval);
}

nsresult nsPrintSettings::_Assign(nsIPrintSettings* aPS) {
  nsPrintSettings* ps = static_cast<nsPrintSettings*>(aPS);
  *this = *ps;
  return NS_OK;
}

NS_IMETHODIMP
nsPrintSettings::Assign(nsIPrintSettings* aPS) {
  NS_ENSURE_ARG(aPS);
  return _Assign(aPS);
}

//-------------------------------------------
nsPrintSettings& nsPrintSettings::operator=(const nsPrintSettings& rhs) {
  if (this == &rhs) {
    return *this;
  }

  mPageRanges = rhs.mPageRanges.Clone();
  mMargin = rhs.mMargin;
  mEdge = rhs.mEdge;
  mUnwriteableMargin = rhs.mUnwriteableMargin;
  mScaling = rhs.mScaling;
  mPrintBGColors = rhs.mPrintBGColors;
  mPrintBGImages = rhs.mPrintBGImages;
  mTitle = rhs.mTitle;
  mURL = rhs.mURL;
  mIsCancelled = rhs.mIsCancelled;
  mSaveOnCancel = rhs.mSaveOnCancel;
  mPrintSilent = rhs.mPrintSilent;
  mShrinkToFit = rhs.mShrinkToFit;
  mShowPrintProgress = rhs.mShowPrintProgress;
  mShowMarginGuides = rhs.mShowMarginGuides;
  mHonorPageRuleMargins = rhs.mHonorPageRuleMargins;
  mIsPrintSelectionRBEnabled = rhs.mIsPrintSelectionRBEnabled;
  mPrintSelectionOnly = rhs.mPrintSelectionOnly;
  mPaperId = rhs.mPaperId;
  mPaperWidth = rhs.mPaperWidth;
  mPaperHeight = rhs.mPaperHeight;
  mPaperSizeUnit = rhs.mPaperSizeUnit;
  mPrintReversed = rhs.mPrintReversed;
  mPrintInColor = rhs.mPrintInColor;
  mOrientation = rhs.mOrientation;
  mResolution = rhs.mResolution;
  mDuplex = rhs.mDuplex;
  mNumCopies = rhs.mNumCopies;
  mNumPagesPerSheet = rhs.mNumPagesPerSheet;
  mPrinter = rhs.mPrinter;
  mPrintToFile = rhs.mPrintToFile;
  mToFileName = rhs.mToFileName;
  mOutputFormat = rhs.mOutputFormat;
  mIsInitedFromPrinter = rhs.mIsInitedFromPrinter;
  mIsInitedFromPrefs = rhs.mIsInitedFromPrefs;
  mPrintPageDelay = rhs.mPrintPageDelay;

  for (int32_t i = 0; i < NUM_HEAD_FOOT; i++) {
    mHeaderStrs[i] = rhs.mHeaderStrs[i];
    mFooterStrs[i] = rhs.mFooterStrs[i];
  }

  return *this;
}

void nsPrintSettings::SetDefaultFileName() {
  nsAutoString filename;
  nsresult rv = GetToFileName(filename);
  if (NS_FAILED(rv) || filename.IsEmpty()) {
    const char* path = PR_GetEnv("PWD");
    if (!path) {
      path = PR_GetEnv("HOME");
    }

    if (path) {
      CopyUTF8toUTF16(mozilla::MakeStringSpan(path), filename);
      filename.AppendLiteral("/mozilla.pdf");
    } else {
      filename.AssignLiteral("mozilla.pdf");
    }

    SetToFileName(filename);
  }
}
