/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsLookAndFeel
#define __nsLookAndFeel

#include "X11UndefineNone.h"
#include "nsXPLookAndFeel.h"
#include "nsCOMPtr.h"
#include "gfxFont.h"

struct _GtkStyle;

class nsLookAndFeel final : public nsXPLookAndFeel {
 public:
  nsLookAndFeel();
  virtual ~nsLookAndFeel();

  void NativeInit() final;
  void RefreshImpl() override;
  nsresult NativeGetColor(ColorID aID, nscolor& aResult) override;
  nsresult GetIntImpl(IntID aID, int32_t& aResult) override;
  nsresult GetFloatImpl(FloatID aID, float& aResult) override;
  bool GetFontImpl(FontID aID, nsString& aFontName,
                   gfxFontStyle& aFontStyle) override;

  char16_t GetPasswordCharacterImpl() override;
  bool GetEchoPasswordImpl() override;

  nsTArray<LookAndFeelInt> GetIntCacheImpl() override;
  void SetIntCacheImpl(
      const nsTArray<LookAndFeelInt>& aLookAndFeelIntCache) override;

  bool IsCSDAvailable() const { return mCSDAvailable; }

  static const nscolor kBlack = NS_RGB(0, 0, 0);
  static const nscolor kWhite = NS_RGB(255, 255, 255);

 protected:
  // Cached fonts
  bool mDefaultFontCached = false;
  bool mButtonFontCached = false;
  bool mFieldFontCached = false;
  bool mMenuFontCached = false;
  nsString mDefaultFontName;
  nsString mButtonFontName;
  nsString mFieldFontName;
  nsString mMenuFontName;
  gfxFontStyle mDefaultFontStyle;
  gfxFontStyle mButtonFontStyle;
  gfxFontStyle mFieldFontStyle;
  gfxFontStyle mMenuFontStyle;

  // Cached colors
  nscolor mInfoBackground = kWhite;
  nscolor mInfoText = kBlack;
  nscolor mMenuBackground = kWhite;
  nscolor mMenuBarText = kBlack;
  nscolor mMenuBarHoverText = kBlack;
  nscolor mMenuText = kBlack;
  nscolor mMenuTextInactive = kWhite;
  nscolor mMenuHover = kWhite;
  nscolor mMenuHoverText = kBlack;
  nscolor mButtonDefault = kWhite;
  nscolor mButtonText = kBlack;
  nscolor mButtonHoverText = kBlack;
  nscolor mButtonHoverFace = kWhite;
  nscolor mButtonActiveText = kBlack;
  nscolor mFrameOuterLightBorder = kBlack;
  nscolor mFrameInnerDarkBorder = kBlack;
  nscolor mOddCellBackground = kWhite;
  nscolor mNativeHyperLinkText = kBlack;
  nscolor mComboBoxText = kBlack;
  nscolor mComboBoxBackground = kWhite;
  nscolor mFieldText = kBlack;
  nscolor mFieldBackground = kWhite;
  nscolor mMozWindowText = kBlack;
  nscolor mMozWindowBackground = kWhite;
  nscolor mMozWindowActiveBorder = kBlack;
  nscolor mMozWindowInactiveBorder = kBlack;
  nscolor mMozWindowInactiveCaption = kWhite;
  nscolor mMozCellHighlightBackground = kWhite;
  nscolor mMozCellHighlightText = kBlack;
  nscolor mTextSelectedText = kBlack;
  nscolor mTextSelectedBackground = kWhite;
  nscolor mMozScrollbar = kWhite;
  nscolor mInfoBarText = kBlack;
  char16_t mInvisibleCharacter = 0;
  float mCaretRatio = 0.0f;
  int32_t mCaretBlinkTime = 0;
  int32_t mPrefersReducedMotion = -1;
  bool mMenuSupportsDrag = false;
  bool mCSDAvailable = false;
  bool mCSDHideTitlebarByDefault = false;
  bool mCSDMaximizeButton = false;
  bool mCSDMinimizeButton = false;
  bool mCSDCloseButton = false;
  bool mCSDReversedPlacement = false;
  bool mSystemUsesDarkTheme = false;
  bool mHighContrast = false;
  bool mInitialized = false;
  bool mPrefersReducedMotionCached = false;

  void EnsureInit();
  void ConfigureContentGtkTheme();

 private:
  nsresult InitCellHighlightColors();
};

#endif
