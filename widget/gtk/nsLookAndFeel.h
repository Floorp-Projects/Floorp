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

enum WidgetNodeType : int;
struct _GtkStyle;

class nsLookAndFeel final : public nsXPLookAndFeel {
 public:
  nsLookAndFeel();
  virtual ~nsLookAndFeel();

  void NativeInit() final;
  void RefreshImpl() override;
  nsresult NativeGetInt(IntID aID, int32_t& aResult) override;
  nsresult NativeGetFloat(FloatID aID, float& aResult) override;
  nsresult NativeGetColor(ColorID, ColorScheme, nscolor& aResult) override;
  bool NativeGetFont(FontID aID, nsString& aFontName,
                     gfxFontStyle& aFontStyle) override;

  char16_t GetPasswordCharacterImpl() override;
  bool GetEchoPasswordImpl() override;

  void WithThemeConfiguredForContent(
      const std::function<void(const LookAndFeelTheme&, bool)>& aFn) override;
  bool FromParentTheme(IntID) override;
  bool FromParentTheme(ColorID) override;

  static void ConfigureTheme(const LookAndFeelTheme& aTheme);

  bool IsCSDAvailable() const { return mCSDAvailable; }

  static const nscolor kBlack = NS_RGB(0, 0, 0);
  static const nscolor kWhite = NS_RGB(255, 255, 255);

 protected:
  bool WidgetUsesImage(WidgetNodeType aNodeType);
  void RecordLookAndFeelSpecificTelemetry() override;
  bool ShouldHonorThemeScrollbarColors();

  // Cached fonts
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
  nscolor mAccentColor = kWhite;
  nscolor mAccentColorForeground = kWhite;
  nscolor mMozScrollbar = kWhite;
  nscolor mInfoBarText = kBlack;
  nscolor mMozColHeaderText = kBlack;
  nscolor mMozColHeaderHoverText = kBlack;
  nscolor mThemedScrollbar = kWhite;
  nscolor mThemedScrollbarInactive = kWhite;
  nscolor mThemedScrollbarThumb = kBlack;
  nscolor mThemedScrollbarThumbHover = kBlack;
  nscolor mThemedScrollbarThumbActive = kBlack;
  nscolor mThemedScrollbarThumbInactive = kBlack;
  char16_t mInvisibleCharacter = 0;
  float mCaretRatio = 0.0f;
  int32_t mCaretBlinkTime = 0;
  bool mMenuSupportsDrag = false;
  bool mCSDAvailable = false;
  bool mCSDHideTitlebarByDefault = false;
  bool mCSDMaximizeButton = false;
  bool mCSDMinimizeButton = false;
  bool mCSDCloseButton = false;
  bool mCSDReversedPlacement = false;
  bool mSystemUsesDarkTheme = false;
  bool mPrefersReducedMotion = false;
  bool mHighContrast = false;
  bool mInitialized = false;
  int32_t mCSDMaximizeButtonPosition = 0;
  int32_t mCSDMinimizeButtonPosition = 0;
  int32_t mCSDCloseButtonPosition = 0;

  void EnsureInit();
  // Returns whether the current theme or theme variant was changed.
  bool ConfigureContentGtkTheme();

 private:
  nsresult InitCellHighlightColors();
};

#endif
