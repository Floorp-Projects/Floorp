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
  nscolor mColorTextSelectBackground;
  nscolor mColorTextSelectBackgroundDisabled;
  nscolor mColorHighlight;
  nscolor mColorAlternateSelectedControlText;
  nscolor mColorControlText;
  nscolor mColorText;
  nscolor mColorWindowText;
  nscolor mColorGrid;
  nscolor mColorActiveBorder;
  nscolor mColorGrayText;
  nscolor mColorControlBackground;
  nscolor mColorScrollbar;
  nscolor mColorThreeDHighlight;
  nscolor mColorDialog;
  nscolor mColorDragTargetZone;
  nscolor mColorChromeActive;
  nscolor mColorChromeInactive;
  nscolor mColorFocusRing;
  nscolor mColorTextSelect;
  nscolor mColorDisabledToolbarText;
  nscolor mColorMenuSelect;
  nscolor mColorCellHighlight;
  nscolor mColorEvenTreeRow;
  nscolor mColorOddTreeRow;
  nscolor mColorMenuFontSmoothingBg;
  nscolor mColorSourceListFontSmoothingBg;
  nscolor mColorSourceListSelectionFontSmoothingBg;
  nscolor mColorActiveSourceListSelectionFontSmoothingBg;

  bool mInitialized;

  void EnsureInit();
};

#endif  // nsLookAndFeel_h_
