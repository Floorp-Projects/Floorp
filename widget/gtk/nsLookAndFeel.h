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
typedef struct _GDBusProxy GDBusProxy;
typedef struct _GtkCssProvider GtkCssProvider;

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

  bool GetDefaultDrawInTitlebar() override;

  void GetThemeInfo(nsACString&) override;

  static const nscolor kBlack = NS_RGB(0, 0, 0);
  static const nscolor kWhite = NS_RGB(255, 255, 255);
  void OnColorSchemeSettingChanged();

  struct ColorPair {
    nscolor mBg = kWhite;
    nscolor mFg = kBlack;
  };

 protected:
  static bool WidgetUsesImage(WidgetNodeType aNodeType);
  void RecordLookAndFeelSpecificTelemetry() override;
  static bool ShouldHonorThemeScrollbarColors();
  mozilla::Maybe<ColorScheme> ComputeColorSchemeSetting();

  void WatchDBus();
  void UnwatchDBus();

  enum class ThemeFamily : uint8_t {
    // Adwaita, the default GTK theme.
    Adwaita,
    // Breeze, the default KDE theme.
    Breeze,
    // Yaru, the default Ubuntu theme.
    Yaru,
    Other,
  };

  // We use up to two themes (one light, one dark), which might have different
  // sets of fonts and colors.
  struct PerThemeData {
    nsCString mName;
    bool mIsDark = false;
    bool mHighContrast = false;
    bool mPreferDarkTheme = false;

    ThemeFamily mFamily = ThemeFamily::Other;

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
    nscolor mGrayText = kBlack;
    ColorPair mInfo;
    ColorPair mMenu;
    ColorPair mMenuHover;
    ColorPair mHeaderBar;
    ColorPair mHeaderBarInactive;
    ColorPair mButton;
    ColorPair mButtonHover;
    nscolor mButtonActiveText = kBlack;
    nscolor mFrameOuterLightBorder = kBlack;
    nscolor mFrameInnerDarkBorder = kBlack;
    nscolor mOddCellBackground = kWhite;
    nscolor mNativeHyperLinkText = kBlack;
    nscolor mNativeVisitedHyperLinkText = kBlack;
    // FIXME: This doesn't seem like it'd be sound since we use Window for
    // -moz-Combobox... But I guess we rely on chrome code not setting
    // appearance: none on selects or overriding the color if they do.
    nscolor mComboBoxText = kBlack;
    ColorPair mField;
    ColorPair mWindow;

    nscolor mMozWindowActiveBorder = kBlack;
    nscolor mMozWindowInactiveBorder = kBlack;

    ColorPair mCellHighlight;
    ColorPair mSelectedText;
    ColorPair mAccent;
    ColorPair mSelectedItem;

    nscolor mMozColHeaderText = kBlack;
    nscolor mMozColHeaderHoverText = kBlack;

    ColorPair mTitlebar;
    ColorPair mTitlebarInactive;

    nscolor mThemedScrollbar = kWhite;
    nscolor mThemedScrollbarInactive = kWhite;
    nscolor mThemedScrollbarThumb = kBlack;
    nscolor mThemedScrollbarThumbHover = kBlack;
    nscolor mThemedScrollbarThumbActive = kBlack;
    nscolor mThemedScrollbarThumbInactive = kBlack;

    float mCaretRatio = 0.0f;
    int32_t mTitlebarRadius = 0;
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

  const PerThemeData& EffectiveTheme() const {
    return mSystemThemeOverridden ? mAltTheme : mSystemTheme;
  }

  uint32_t mDBusID = 0;
  RefPtr<GDBusProxy> mDBusSettingsProxy;
  mozilla::Maybe<ColorScheme> mColorSchemePreference;
  int32_t mCaretBlinkTime = 0;
  int32_t mCaretBlinkCount = -1;
  bool mCSDMaximizeButton = false;
  bool mCSDMinimizeButton = false;
  bool mCSDCloseButton = false;
  bool mCSDReversedPlacement = false;
  bool mPrefersReducedMotion = false;
  bool mInitialized = false;
  bool mSystemThemeOverridden = false;
  int32_t mCSDMaximizeButtonPosition = 0;
  int32_t mCSDMinimizeButtonPosition = 0;
  int32_t mCSDCloseButtonPosition = 0;

  RefPtr<GtkCssProvider> mRoundedCornerProvider;
  void UpdateRoundedBottomCornerStyles();

  void ClearRoundedCornerProvider();

  void EnsureInit() {
    if (mInitialized) {
      return;
    }
    Initialize();
  }

  void Initialize();

  void RestoreSystemTheme();
  void InitializeGlobalSettings();
  // Returns whether we found an alternative theme.
  bool ConfigureAltTheme();
  void ConfigureAndInitializeAltTheme();
  void ConfigureFinalEffectiveTheme();
};

#endif
