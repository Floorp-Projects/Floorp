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
#if WINVER < 0x0601
typedef enum _AR_STATE
{
  AR_ENABLED        = 0x0,
  AR_DISABLED       = 0x1,
  AR_SUPPRESSED     = 0x2,
  AR_REMOTESESSION  = 0x4,
  AR_MULTIMON       = 0x8,
  AR_NOSENSOR       = 0x10,
  AR_NOT_SUPPORTED  = 0x20,
  AR_DOCKED         = 0x40,
  AR_LAPTOP         = 0x80
} AR_STATE, *PAR_STATE;
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
  int32_t mUseAccessibilityTheme;
};

#endif
