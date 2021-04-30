/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
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

  void WithAltThemeConfigured(const std::function<void(bool)>&);

  void GetGtkContentTheme(LookAndFeelTheme&) override;

  static void ConfigureTheme(const LookAndFeelTheme& aTheme);

  bool IsCSDAvailable() const { return mCSDAvailable; }

  static const nscolor kBlack = NS_RGB(0, 0, 0);
  static const nscolor kWhite = NS_RGB(255, 255, 255);

 protected:
  static bool WidgetUsesImage(WidgetNodeType aNodeType);
  void RecordLookAndFeelSpecificTelemetry() override;
  static bool ShouldHonorThemeScrollbarColors();

  // We use up to two themes (one light, one dark), which might have different
  // sets of fonts and colors.
  struct PerThemeData {
    nsCString mName;

    bool mIsDark = false;
    bool mHighContrast = false;
    bool mPreferDarkTheme = false;

    // NOTE(emilio): This is unused, but if we need to we can use it to override
    // system colors with standins like we do for the non-native theme.
    bool mCompatibleWithHTMLLightColors = false;

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

    float mCaretRatio = 0.0f;
    char16_t mInvisibleCharacter = 0;
    bool mMenuSupportsDrag = false;

    void Init();
    nsresult GetColor(ColorID, nscolor&) const;
    bool GetFont(FontID, nsString& aFontName, gfxFontStyle&) const;
    void InitCellHighlightColors();
  };

  PerThemeData mSystemTheme;

  // If the system theme is light, a dark theme. Otherwise, a light theme. The
  // alternative theme to the current one is preferred, but otherwise we fall
  // back to Adwaita / Adwaita Dark, respectively.
  PerThemeData mAltTheme;

  const PerThemeData& LightTheme() const {
    return mSystemTheme.mIsDark ? mAltTheme : mSystemTheme;
  }

  const PerThemeData& DarkTheme() const {
    return mSystemTheme.mIsDark ? mSystemTheme : mAltTheme;
  }

  int32_t mCaretBlinkTime = 0;
  bool mCSDAvailable = false;
  bool mCSDHideTitlebarByDefault = false;
  bool mCSDMaximizeButton = false;
  bool mCSDMinimizeButton = false;
  bool mCSDCloseButton = false;
  bool mCSDReversedPlacement = false;
  bool mPrefersReducedMotion = false;
  bool mInitialized = false;
  int32_t mCSDMaximizeButtonPosition = 0;
  int32_t mCSDMinimizeButtonPosition = 0;
  int32_t mCSDCloseButtonPosition = 0;

  void EnsureInit();
  // Returns whether the current theme or theme variant was changed.
  bool ConfigureContentGtkTheme();
};

#endif
