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
  nsresult NativeGetColor(ColorID, ColorScheme, nscolor& aColor) override;
  nsresult NativeGetInt(IntID, int32_t& aResult) override;
  nsresult NativeGetFloat(FloatID, float& aResult) override;
  bool NativeGetFont(FontID aID, nsString& aFontName,
                     gfxFontStyle& aFontStyle) override;

  virtual char16_t GetPasswordCharacterImpl() override {
    // unicode value for the bullet character, used for password textfields.
    return 0x2022;
  }

  void RecordLookAndFeelSpecificTelemetry() override {
    RecordAccessibilityTelemetry();
  }

  // Having a separate, static method allows us to rely on the same
  // chunk of telemetry logging code at initialization and when we
  // recieve an event that changes the value of our telemetry probe.
  static void RecordAccessibilityTelemetry();

 protected:
  static bool SystemWantsDarkTheme();
  static bool IsSystemOrientationRTL();
  static nscolor ProcessSelectionBackground(nscolor aColor,
                                            ColorScheme aScheme);
};

#endif  // nsLookAndFeel_h_
