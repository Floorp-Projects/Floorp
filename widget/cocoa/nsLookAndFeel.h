/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLookAndFeel_h_
#define nsLookAndFeel_h_
#include "nsXPLookAndFeel.h"

class nsLookAndFeel: public nsXPLookAndFeel {
public:
  nsLookAndFeel();
  virtual ~nsLookAndFeel();

  virtual nsresult NativeGetColor(ColorID aID, nscolor &aResult);
  virtual nsresult GetIntImpl(IntID aID, int32_t &aResult);
  virtual nsresult GetFloatImpl(FloatID aID, float &aResult);
  virtual bool GetFontImpl(FontID aID, nsString& aFontName,
                           gfxFontStyle& aFontStyle,
                           float aDevPixPerCSSPixel);
  virtual char16_t GetPasswordCharacterImpl()
  {
    // unicode value for the bullet character, used for password textfields.
    return 0x2022;
  }

  static bool UseOverlayScrollbars();

  virtual nsTArray<LookAndFeelInt> GetIntCacheImpl();
  virtual void SetIntCacheImpl(const nsTArray<LookAndFeelInt>& aLookAndFeelIntCache);

  virtual void RefreshImpl();

protected:
  static bool SystemWantsOverlayScrollbars();
  static bool AllowOverlayScrollbarsOverlap();

private:
  int32_t mUseOverlayScrollbars;
  bool mUseOverlayScrollbarsCached;

  int32_t mAllowOverlayScrollbarsOverlap;
  bool mAllowOverlayScrollbarsOverlapCached;
};

#endif // nsLookAndFeel_h_
