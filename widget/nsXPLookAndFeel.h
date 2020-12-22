/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsXPLookAndFeel
#define __nsXPLookAndFeel

#include "mozilla/LookAndFeel.h"
#include "mozilla/ServoStyleConsts.h"
#include "mozilla/widget/LookAndFeelTypes.h"
#include "nsTArray.h"

class nsLookAndFeel;

struct nsLookAndFeelIntPref {
  const char* name;
  mozilla::LookAndFeel::IntID id;
  bool isSet;
  int32_t intVar;
};

struct nsLookAndFeelFloatPref {
  const char* name;
  mozilla::LookAndFeel::FloatID id;
  bool isSet;
  float floatVar;
};

#define CACHE_BLOCK(x) (uint32_t(x) >> 5)
#define CACHE_BIT(x) (1 << (uint32_t(x) & 31))

#define COLOR_CACHE_SIZE (CACHE_BLOCK(uint32_t(LookAndFeel::ColorID::End)) + 1)
#define IS_COLOR_CACHED(x) \
  (CACHE_BIT(x) & nsXPLookAndFeel::sCachedColorBits[CACHE_BLOCK(x)])
#define CLEAR_COLOR_CACHE(x)                       \
  nsXPLookAndFeel::sCachedColors[uint32_t(x)] = 0; \
  nsXPLookAndFeel::sCachedColorBits[CACHE_BLOCK(x)] &= ~(CACHE_BIT(x));
#define CACHE_COLOR(x, y)                          \
  nsXPLookAndFeel::sCachedColors[uint32_t(x)] = y; \
  nsXPLookAndFeel::sCachedColorBits[CACHE_BLOCK(x)] |= CACHE_BIT(x);

class nsXPLookAndFeel : public mozilla::LookAndFeel {
 public:
  virtual ~nsXPLookAndFeel();

  static nsXPLookAndFeel* GetInstance();
  static void Shutdown();

  void Init();

  // These functions will return a value specified by an override pref, if it
  // exists, and otherwise will call into the NativeGetXxx function to get the
  // platform-specific value.
  //
  // NS_ERROR_NOT_AVAILABLE is returned if there is neither an override pref or
  // a platform-specific value.
  nsresult GetColorValue(ColorID aID, bool aUseStandinsForNativeColors,
                         nscolor& aResult);
  nsresult GetIntValue(IntID aID, int32_t& aResult);
  nsresult GetFloatValue(FloatID aID, float& aResult);
  // Same, but returns false if there is no platform-specific value.
  // (There are no override prefs for font values.)
  bool GetFontValue(FontID aID, nsString& aName, gfxFontStyle& aStyle) {
    return NativeGetFont(aID, aName, aStyle);
  }

  virtual nsresult NativeGetInt(IntID aID, int32_t& aResult) = 0;
  virtual nsresult NativeGetFloat(FloatID aID, float& aResult) = 0;
  virtual nsresult NativeGetColor(ColorID aID, nscolor& aResult) = 0;
  virtual bool NativeGetFont(FontID aID, nsString& aName,
                             gfxFontStyle& aStyle) = 0;

  virtual void RefreshImpl();

  virtual char16_t GetPasswordCharacterImpl() { return char16_t('*'); }

  virtual bool GetEchoPasswordImpl() { return false; }

  virtual uint32_t GetPasswordMaskDelayImpl() { return 600; }

  using FullLookAndFeel = mozilla::widget::FullLookAndFeel;
  using LookAndFeelCache = mozilla::widget::LookAndFeelCache;
  using LookAndFeelInt = mozilla::widget::LookAndFeelInt;
  using LookAndFeelFont = mozilla::widget::LookAndFeelFont;
  using LookAndFeelColor = mozilla::widget::LookAndFeelColor;
  using LookAndFeelTheme = mozilla::widget::LookAndFeelTheme;

  virtual LookAndFeelCache GetCacheImpl();
  virtual void SetCacheImpl(const LookAndFeelCache& aCache) {}
  virtual void SetDataImpl(FullLookAndFeel&& aTables) {}

  virtual void NativeInit() = 0;

  virtual void WithThemeConfiguredForContent(
      const std::function<void(const LookAndFeelTheme& aTheme)>& aFn) {
    aFn(LookAndFeelTheme{});
  }

 protected:
  nsXPLookAndFeel() = default;

  static void IntPrefChanged(nsLookAndFeelIntPref* data);
  static void FloatPrefChanged(nsLookAndFeelFloatPref* data);
  static void ColorPrefChanged(unsigned int index, const char* prefName);
  void InitFromPref(nsLookAndFeelIntPref* aPref);
  void InitFromPref(nsLookAndFeelFloatPref* aPref);
  void InitColorFromPref(int32_t aIndex);
  bool IsSpecialColor(ColorID aID, nscolor& aColor);
  bool ColorIsNotCSSAccessible(ColorID aID);
  nscolor GetStandinForNativeColor(ColorID aID);
  void RecordTelemetry();
  virtual void RecordLookAndFeelSpecificTelemetry() {}

  static void OnPrefChanged(const char* aPref, void* aClosure);

  static bool sInitialized;
  static nsLookAndFeelIntPref sIntPrefs[];
  static nsLookAndFeelFloatPref sFloatPrefs[];
  /* this length must not be shorter than the length of the longest string in
   * the array see nsXPLookAndFeel.cpp
   */
  static const char sColorPrefs[][41];
  static int32_t sCachedColors[size_t(LookAndFeel::ColorID::End)];
  static int32_t sCachedColorBits[COLOR_CACHE_SIZE];

  static nsXPLookAndFeel* sInstance;
  static bool sShutdown;
};

#endif
