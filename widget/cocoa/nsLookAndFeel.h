/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLookAndFeel_h_
#define nsLookAndFeel_h_
#include "nsXPLookAndFeel.h"

class nsLookAndFeel final : public nsXPLookAndFeel
{
public:
  nsLookAndFeel();
  virtual ~nsLookAndFeel();

  void NativeInit() final;
  virtual void RefreshImpl() override;
  virtual nsresult NativeGetColor(ColorID aID, nscolor &aResult) override;
  virtual nsresult GetIntImpl(IntID aID, int32_t &aResult) override;
  virtual nsresult GetFloatImpl(FloatID aID, float &aResult) override;
  virtual bool GetFontImpl(FontID aID, nsString& aFontName,
                           gfxFontStyle& aFontStyle,
                           float aDevPixPerCSSPixel) override;

  virtual char16_t GetPasswordCharacterImpl() override
  {
    // unicode value for the bullet character, used for password textfields.
    return 0x2022;
  }

  static bool UseOverlayScrollbars();

  virtual nsTArray<LookAndFeelInt> GetIntCacheImpl() override;
  virtual void SetIntCacheImpl(const nsTArray<LookAndFeelInt>& aLookAndFeelIntCache) override;

protected:
  static bool SystemWantsOverlayScrollbars();
  static bool AllowOverlayScrollbarsOverlap();

  static bool SystemWantsDarkTheme();

private:
  int32_t mUseOverlayScrollbars;
  bool mUseOverlayScrollbarsCached;

  int32_t mAllowOverlayScrollbarsOverlap;
  bool mAllowOverlayScrollbarsOverlapCached;

  nscolor mColorTextSelectBackground;
  nscolor mColorTextSelectBackgroundDisabled;
  nscolor mColorHighlight;
  nscolor mColorMenuHover;
  nscolor mColorTextSelectForeground;
  nscolor mColorMenuHoverText;
  nscolor mColorButtonText;
  bool mHasColorButtonText;
  nscolor mColorButtonHoverText;
  nscolor mColorText;
  nscolor mColorWindowText;
  nscolor mColorActiveCaption;
  nscolor mColorActiveBorder;
  nscolor mColorGrayText;
  nscolor mColorInactiveBorder;
  nscolor mColorInactiveCaption;
  nscolor mColorScrollbar;
  nscolor mColorThreeDHighlight;
  nscolor mColorMenu;
  nscolor mColorWindowFrame;
  nscolor mColorFieldText;
  nscolor mColorDialog;
  nscolor mColorDialogText;
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
  nscolor mColorActiveSourceListSelection;

  bool mInitialized;

  void EnsureInit();
};

#endif // nsLookAndFeel_h_
