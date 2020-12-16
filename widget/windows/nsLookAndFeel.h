/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsLookAndFeel
#define __nsLookAndFeel

#include <windows.h>

#include "nsXPLookAndFeel.h"
#include "gfxFont.h"
#include "mozilla/RangedArray.h"
#include "nsIWindowsRegKey.h"

/*
 * Gesture System Metrics
 */
#ifndef SM_DIGITIZER
#  define SM_DIGITIZER 94
#  define TABLET_CONFIG_NONE 0x00000000
#  define NID_INTEGRATED_TOUCH 0x00000001
#  define NID_EXTERNAL_TOUCH 0x00000002
#  define NID_INTEGRATED_PEN 0x00000004
#  define NID_EXTERNAL_PEN 0x00000008
#  define NID_MULTI_INPUT 0x00000040
#  define NID_READY 0x00000080
#endif

/*
 * Tablet mode detection
 */
#ifndef SM_SYSTEMDOCKED
#  define SM_CONVERTIBLESLATEMODE 0x00002003
#  define SM_SYSTEMDOCKED 0x00002004
#endif

/*
 * Color constant inclusive bounds for GetSysColor
 */
#define SYS_COLOR_MIN 0
#define SYS_COLOR_MAX 30
#define SYS_COLOR_COUNT (SYS_COLOR_MAX - SYS_COLOR_MIN + 1)

class nsLookAndFeel final : public nsXPLookAndFeel {
  static OperatingSystemVersion GetOperatingSystemVersion();

 public:
  explicit nsLookAndFeel(const LookAndFeelCache* aCache);
  virtual ~nsLookAndFeel();

  void NativeInit() final;
  void RefreshImpl() override;
  nsresult NativeGetInt(IntID aID, int32_t& aResult) override;
  nsresult NativeGetFloat(FloatID aID, float& aResult) override;
  nsresult NativeGetColor(ColorID aID, nscolor& aResult) override;
  bool NativeGetFont(FontID aID, nsString& aFontName,
                     gfxFontStyle& aFontStyle) override;
  char16_t GetPasswordCharacterImpl() override;

  LookAndFeelCache GetCacheImpl() override;
  void SetCacheImpl(const LookAndFeelCache& aCache) override;

 private:
  void DoSetCache(const LookAndFeelCache& aCache);

  /**
   * Fetches the Windows accent color from the Windows settings if
   * the accent color is set to apply to the title bar, otherwise
   * returns an error code.
   */
  nsresult GetAccentColor(nscolor& aColor);

  /**
   * If the Windows accent color from the Windows settings is set
   * to apply to the title bar, this computes the color that should
   * be used for text that is to be written over a background that has
   * the accent color.  Otherwise, (if the accent color should not
   * apply to the title bar) this returns an error code.
   */
  nsresult GetAccentColorText(nscolor& aColor);

  nscolor GetColorForSysColorIndex(int index);

  LookAndFeelFont GetLookAndFeelFontInternal(const LOGFONTW& aLogFont,
                                             bool aUseShellDlg);

  LookAndFeelFont GetLookAndFeelFont(LookAndFeel::FontID anID);

  bool GetSysFont(LookAndFeel::FontID anID, nsString& aFontName,
                  gfxFontStyle& aFontStyle);

  // Content process cached values that get shipped over from the browser
  // process.
  int32_t mUseAccessibilityTheme;
  int32_t mUseDefaultTheme;  // is the current theme a known default?
  int32_t mNativeThemeId;    // see LookAndFeel enum 'WindowsTheme'
  int32_t mCaretBlinkTime;

  // Cached colors and flags indicating success in their retrieval.
  nscolor mColorMenuHoverText;
  bool mHasColorMenuHoverText;
  nscolor mColorAccent;
  bool mHasColorAccent;
  nscolor mColorAccentText;
  bool mHasColorAccentText;
  nscolor mColorMediaText;
  bool mHasColorMediaText;
  nscolor mColorCommunicationsText;
  bool mHasColorCommunicationsText;

  nscolor mSysColorTable[SYS_COLOR_COUNT];

  bool mInitialized;

  void EnsureInit();

  struct CachedSystemFont {
    CachedSystemFont() : mCacheValid(false) {}

    bool mCacheValid;
    bool mHaveFont;
    nsString mFontName;
    gfxFontStyle mFontStyle;
  };

  mozilla::RangedArray<CachedSystemFont, size_t(FontID::MINIMUM),
                       size_t(FontID::MAXIMUM) + 1 - size_t(FontID::MINIMUM)>
      mSystemFontCache;

  mozilla::RangedArray<LookAndFeelFont, size_t(FontID::MINIMUM),
                       size_t(FontID::MAXIMUM) + 1 - size_t(FontID::MINIMUM)>
      mFontCache;

  nsCOMPtr<nsIWindowsRegKey> mDwmKey;
};

#endif
