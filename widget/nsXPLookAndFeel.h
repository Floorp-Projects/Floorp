/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsXPLookAndFeel
#define __nsXPLookAndFeel

#include "mozilla/Maybe.h"
#include "mozilla/LookAndFeel.h"
#include "mozilla/widget/LookAndFeelTypes.h"
#include "nsTArray.h"

class nsLookAndFeel;

class nsXPLookAndFeel : public mozilla::LookAndFeel {
 public:
  using FullLookAndFeel = mozilla::widget::FullLookAndFeel;
  using LookAndFeelFont = mozilla::widget::LookAndFeelFont;
  using LookAndFeelTheme = mozilla::widget::LookAndFeelTheme;

  virtual ~nsXPLookAndFeel();

  static nsXPLookAndFeel* GetInstance();
  static void Shutdown();

  void Init();

  // Gets the pref name for a given color, just for debugging purposes.
  static const char* GetColorPrefName(ColorID);

  // These functions will return a value specified by an override pref, if it
  // exists, and otherwise will call into the NativeGetXxx function to get the
  // platform-specific value.
  //
  // NS_ERROR_NOT_AVAILABLE is returned if there is neither an override pref or
  // a platform-specific value.
  nsresult GetColorValue(ColorID, ColorScheme, UseStandins, nscolor& aResult);
  nsresult GetIntValue(IntID aID, int32_t& aResult);
  nsresult GetFloatValue(FloatID aID, float& aResult);
  // Same, but returns false if there is no platform-specific value.
  // (There are no override prefs for font values.)
  bool GetFontValue(FontID aID, nsString& aName, gfxFontStyle& aStyle);

  virtual nsresult NativeGetInt(IntID aID, int32_t& aResult) = 0;
  virtual nsresult NativeGetFloat(FloatID aID, float& aResult) = 0;
  virtual nsresult NativeGetColor(ColorID, ColorScheme, nscolor& aResult) = 0;
  virtual bool NativeGetFont(FontID aID, nsString& aName,
                             gfxFontStyle& aStyle) = 0;

  virtual void RefreshImpl();

  virtual char16_t GetPasswordCharacterImpl() { return char16_t('*'); }

  virtual bool GetEchoPasswordImpl() { return false; }

  virtual uint32_t GetPasswordMaskDelayImpl() { return 600; }

  virtual bool GetDefaultDrawInTitlebar() { return true; }

  static bool LookAndFeelFontToStyle(const LookAndFeelFont&, nsString& aName,
                                     gfxFontStyle&);
  static LookAndFeelFont StyleToLookAndFeelFont(const nsAString& aName,
                                                const gfxFontStyle&);

  virtual void SetDataImpl(FullLookAndFeel&& aTables) {}

  virtual void NativeInit() = 0;

  virtual void GetGtkContentTheme(LookAndFeelTheme&) {}
  virtual void GetThemeInfo(nsACString&) {}

 protected:
  nsXPLookAndFeel() = default;

  static nscolor GetStandinForNativeColor(ColorID, ColorScheme);

  // A platform-agnostic dark-color scheme, for platforms where we don't have
  // "native" dark colors, like Windows and Android.
  static mozilla::Maybe<nscolor> GenericDarkColor(ColorID);
  mozilla::Maybe<nscolor> GetUncachedColor(ColorID, ColorScheme, UseStandins);

  void RecordTelemetry();
  virtual void RecordLookAndFeelSpecificTelemetry() {}

  static void OnPrefChanged(const char* aPref, void* aClosure);

  static bool sInitialized;

  static nsXPLookAndFeel* sInstance;
  static bool sShutdown;
};

#endif
