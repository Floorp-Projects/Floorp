/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLookAndFeel_h_
#define nsLookAndFeel_h_
#include "nsXPLookAndFeel.h"

class nsLookAndFeel final : public nsXPLookAndFeel {
 public:
  explicit nsLookAndFeel(const LookAndFeelCache* aCache);
  virtual ~nsLookAndFeel();

  void NativeInit() final;
  virtual void RefreshImpl() override;
  nsresult NativeGetColor(ColorID aID, nscolor& aResult) override;
  nsresult NativeGetInt(IntID aID, int32_t& aResult) override;
  nsresult NativeGetFloat(FloatID aID, float& aResult) override;
  bool NativeGetFont(FontID aID, nsString& aFontName,
                     gfxFontStyle& aFontStyle) override;

  virtual char16_t GetPasswordCharacterImpl() override {
    // unicode value for the bullet character, used for password textfields.
    return 0x2022;
  }

  static bool UseOverlayScrollbars();

  LookAndFeelCache GetCacheImpl() override;
  void SetCacheImpl(const LookAndFeelCache& aCache) override;

 protected:
  void DoSetCache(const LookAndFeelCache& aCache);
  static bool AllowOverlayScrollbarsOverlap();

  static bool SystemWantsDarkTheme();
  static nscolor ProcessSelectionBackground(nscolor aColor);

 private:
  int32_t mUseOverlayScrollbars;
  bool mUseOverlayScrollbarsCached;

  int32_t mAllowOverlayScrollbarsOverlap;
  bool mAllowOverlayScrollbarsOverlapCached;

  int32_t mSystemUsesDarkTheme;
  bool mSystemUsesDarkThemeCached;

  int32_t mPrefersReducedMotion = -1;
  bool mPrefersReducedMotionCached = false;

  int32_t mUseAccessibilityTheme;
  bool mUseAccessibilityThemeCached;

  nscolor mColorTextSelectBackground;
  nscolor mColorTextSelectBackgroundDisabled;
  nscolor mColorHighlight;
  nscolor mColorMenuHover;
  nscolor mColorTextSelectForeground;
  nscolor mColorMenuHoverText;
  nscolor mColorButtonText;
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

#endif  // nsLookAndFeel_h_
