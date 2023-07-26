/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsLookAndFeel
#define __nsLookAndFeel

#include <bitset>
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
 public:
  nsLookAndFeel();
  virtual ~nsLookAndFeel();

  void NativeInit() final;
  void RefreshImpl() override;
  nsresult NativeGetInt(IntID, int32_t& aResult) override;
  nsresult NativeGetFloat(FloatID, float& aResult) override;
  nsresult NativeGetColor(ColorID, ColorScheme, nscolor& aResult) override;
  bool NativeGetFont(FontID aID, nsString& aFontName,
                     gfxFontStyle& aFontStyle) override;
  char16_t GetPasswordCharacterImpl() override;

 private:
  struct TitlebarColors {
    // NOTE: These are the DWM accent colors, which might not match the
    // UISettings/UWP accent color in some cases, see bug 1796730.
    mozilla::Maybe<nscolor> mAccent;
    mozilla::Maybe<nscolor> mAccentText;
    mozilla::Maybe<nscolor> mAccentInactive;
    mozilla::Maybe<nscolor> mAccentInactiveText;

    bool mUseAccent = false;

    struct Set {
      nscolor mBg = 0;
      nscolor mFg = 0;
      nscolor mBorder = 0;
    };

    Set mActiveLight;
    Set mActiveDark;

    Set mInactiveLight;
    Set mInactiveDark;

    const Set& Get(mozilla::ColorScheme aScheme, bool aActive) const {
      if (aScheme == mozilla::ColorScheme::Dark) {
        return aActive ? mActiveDark : mInactiveDark;
      }
      return aActive ? mActiveLight : mInactiveLight;
    }
  };

  TitlebarColors ComputeTitlebarColors();

  nscolor GetColorForSysColorIndex(int index);

  LookAndFeelFont GetLookAndFeelFontInternal(const LOGFONTW& aLogFont,
                                             bool aUseShellDlg);

  LookAndFeelFont GetLookAndFeelFont(LookAndFeel::FontID anID);

  // Cached colors and flags indicating success in their retrieval.
  mozilla::Maybe<nscolor> mColorMenuHoverText;

  mozilla::Maybe<nscolor> mDarkHighlight;
  mozilla::Maybe<nscolor> mDarkHighlightText;

  TitlebarColors mTitlebarColors;

  nscolor mColorAccent = 0;
  nscolor mColorAccentText = 0;

  nscolor mSysColorTable[SYS_COLOR_COUNT];

  bool mInitialized = false;

  void EnsureInit();
};

#endif
