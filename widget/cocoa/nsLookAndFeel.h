/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLookAndFeel_h_
#define nsLookAndFeel_h_
#include "nsXPLookAndFeel.h"

class nsLookAndFeel final : public nsXPLookAndFeel {
 public:
  nsLookAndFeel();
  virtual ~nsLookAndFeel();

  void NativeInit() final;
  virtual void RefreshImpl() override;
  nsresult NativeGetColor(ColorID, ColorScheme, nscolor& aResult) override;
  nsresult NativeGetInt(IntID, int32_t& aResult) override;
  nsresult NativeGetFloat(FloatID, float& aResult) override;
  bool NativeGetFont(FontID aID, nsString& aFontName,
                     gfxFontStyle& aFontStyle) override;

  virtual char16_t GetPasswordCharacterImpl() override {
    // unicode value for the bullet character, used for password textfields.
    return 0x2022;
  }

 protected:
  static bool SystemWantsDarkTheme();
  static nscolor ProcessSelectionBackground(nscolor aColor);

 private:
  struct ColorSet {
    void Refresh(ColorScheme);

    mozilla::Maybe<nscolor> Get(ColorID) const;

    nscolor mColorTextSelectBackground = 0;
    nscolor mColorTextSelectBackgroundDisabled = 0;
    nscolor mColorHighlight = 0;
    nscolor mColorAlternateSelectedControlText = 0;
    nscolor mColorControlText = 0;
    nscolor mColorText = 0;
    nscolor mColorWindowText = 0;
    nscolor mColorGrid = 0;
    nscolor mColorActiveBorder = 0;
    nscolor mColorGrayText = 0;
    nscolor mColorControlBackground = 0;
    nscolor mColorScrollbar = 0;
    nscolor mColorThreeDHighlight = 0;
    nscolor mColorDialog = 0;
    nscolor mColorDragTargetZone = 0;
    nscolor mColorChromeActive = 0;
    nscolor mColorChromeInactive = 0;
    nscolor mColorFocusRing = 0;
    nscolor mColorTextSelect = 0;
    nscolor mColorDisabledToolbarText = 0;
    nscolor mColorMenuSelect = 0;
    nscolor mColorCellHighlight = 0;
    nscolor mColorEvenTreeRow = 0;
    nscolor mColorOddTreeRow = 0;
    nscolor mColorMenuFontSmoothingBg = 0;
    nscolor mColorSourceListFontSmoothingBg = 0;
    nscolor mColorSourceListSelectionFontSmoothingBg = 0;
    nscolor mColorActiveSourceListSelectionFontSmoothingBg = 0;
  };

  bool mInitialized = false;

  ColorSet mLightColorSet;
  ColorSet mDarkColorSet;

  void EnsureInit();
};

#endif  // nsLookAndFeel_h_
