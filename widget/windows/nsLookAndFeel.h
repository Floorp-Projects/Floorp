/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsLookAndFeel
#define __nsLookAndFeel

#include "nsXPLookAndFeel.h"

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

class nsLookAndFeel: public nsXPLookAndFeel {
  static OperatingSystemVersion GetOperatingSystemVersion();
public:
  nsLookAndFeel();
  virtual ~nsLookAndFeel();

  virtual nsresult NativeGetColor(ColorID aID, nscolor &aResult);
  virtual nsresult GetIntImpl(IntID aID, int32_t &aResult);
  virtual nsresult GetFloatImpl(FloatID aID, float &aResult);
  virtual bool GetFontImpl(FontID aID, nsString& aFontName,
                           gfxFontStyle& aFontStyle,
                           float aDevPixPerCSSPixel);
  virtual char16_t GetPasswordCharacterImpl();

  virtual nsTArray<LookAndFeelInt> GetIntCacheImpl();
  virtual void SetIntCacheImpl(const nsTArray<LookAndFeelInt>& aLookAndFeelIntCache);

private:
  // Content process cached values that get shipped over from the browser
  // process.
  int32_t mUseAccessibilityTheme;
  int32_t mUseDefaultTheme; // is the current theme a known default?
  int32_t mNativeThemeId; // see LookAndFeel enum 'WindowsTheme'
};

#endif
