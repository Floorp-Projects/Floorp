/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsLookAndFeel
#define __nsLookAndFeel

#include "nsXPLookAndFeel.h"
#include "gfxFont.h"
#include "mozilla/RangedArray.h"
#include "nsIWindowsRegKey.h"

/*
 * Gesture System Metrics
 */
#ifndef SM_DIGITIZER
#define SM_DIGITIZER         94
#define TABLET_CONFIG_NONE   0x00000000
#define NID_INTEGRATED_TOUCH 0x00000001
#define NID_EXTERNAL_TOUCH   0x00000002
#define NID_INTEGRATED_PEN   0x00000004
#define NID_EXTERNAL_PEN     0x00000008
#define NID_MULTI_INPUT      0x00000040
#define NID_READY            0x00000080
#endif

/*
 * Tablet mode detection
 */
#ifndef SM_SYSTEMDOCKED
#define SM_CONVERTIBLESLATEMODE 0x00002003
#define SM_SYSTEMDOCKED         0x00002004
#endif

class nsLookAndFeel final : public nsXPLookAndFeel
{
  static OperatingSystemVersion GetOperatingSystemVersion();
public:
  nsLookAndFeel();
  virtual ~nsLookAndFeel();

  nsresult NativeGetColor(ColorID aID, nscolor &aResult) override;
  nsresult GetIntImpl(IntID aID, int32_t &aResult) override;
  nsresult GetFloatImpl(FloatID aID, float &aResult) override;
  bool GetFontImpl(FontID aID, nsString& aFontName,
                   gfxFontStyle& aFontStyle,
                   float aDevPixPerCSSPixel) override;
  void RefreshImpl() override;
  char16_t GetPasswordCharacterImpl() override;

  nsTArray<LookAndFeelInt> GetIntCacheImpl() override;
  void SetIntCacheImpl(const nsTArray<LookAndFeelInt>& aLookAndFeelIntCache) override;

private:
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

  // Content process cached values that get shipped over from the browser
  // process.
  int32_t mUseAccessibilityTheme;
  int32_t mUseDefaultTheme; // is the current theme a known default?
  int32_t mNativeThemeId; // see LookAndFeel enum 'WindowsTheme'

  struct CachedSystemFont {
    CachedSystemFont()
      : mCacheValid(false)
    {}

    bool mCacheValid;
    bool mHaveFont;
    nsString mFontName;
    gfxFontStyle mFontStyle;
  };

  mozilla::RangedArray<CachedSystemFont,
                       FontID_MINIMUM,
                       FontID_MAXIMUM + 1 - FontID_MINIMUM> mSystemFontCache;

  nsCOMPtr<nsIWindowsRegKey> mDwmKey;
};

#endif
