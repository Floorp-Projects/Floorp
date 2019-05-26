/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsXPLookAndFeel
#define __nsXPLookAndFeel

#include "mozilla/LookAndFeel.h"
#include "mozilla/ServoStyleConsts.h"
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

  //
  // All these routines will return NS_OK if they have a value,
  // in which case the nsLookAndFeel should use that value;
  // otherwise we'll return NS_ERROR_NOT_AVAILABLE, in which case, the
  // platform-specific nsLookAndFeel should use its own values instead.
  //
  nsresult GetColorImpl(ColorID aID, bool aUseStandinsForNativeColors,
                        nscolor& aResult);
  virtual nsresult GetIntImpl(IntID aID, int32_t& aResult);
  virtual nsresult GetFloatImpl(FloatID aID, float& aResult);

  // This one is different: there are no override prefs (fixme?), so
  // there is no XP implementation, only per-system impls.
  virtual bool GetFontImpl(FontID aID, nsString& aName,
                           gfxFontStyle& aStyle) = 0;

  virtual void RefreshImpl();

  virtual char16_t GetPasswordCharacterImpl() { return char16_t('*'); }

  virtual bool GetEchoPasswordImpl() { return false; }

  virtual uint32_t GetPasswordMaskDelayImpl() { return 600; }

  virtual nsTArray<LookAndFeelInt> GetIntCacheImpl();
  virtual void SetIntCacheImpl(
      const nsTArray<LookAndFeelInt>& aLookAndFeelIntCache) {}
  void SetShouldRetainCacheImplForTest(bool aValue) {
    mShouldRetainCacheForTest = aValue;
  }

  virtual void NativeInit() = 0;

  void SetPrefersReducedMotionOverrideForTest(bool aValue) {
    sIsInPrefersReducedMotionForTest = true;
    sPrefersReducedMotionForTest = aValue;
  }
  void ResetPrefersReducedMotionOverrideForTest() {
    sIsInPrefersReducedMotionForTest = false;
    sPrefersReducedMotionForTest = false;
  }

 protected:
  nsXPLookAndFeel();

  static void IntPrefChanged(nsLookAndFeelIntPref* data);
  static void FloatPrefChanged(nsLookAndFeelFloatPref* data);
  static void ColorPrefChanged(unsigned int index, const char* prefName);
  void InitFromPref(nsLookAndFeelIntPref* aPref);
  void InitFromPref(nsLookAndFeelFloatPref* aPref);
  void InitColorFromPref(int32_t aIndex);
  virtual nsresult NativeGetColor(ColorID aID, nscolor& aResult) = 0;
  bool IsSpecialColor(ColorID aID, nscolor& aColor);
  bool ColorIsNotCSSAccessible(ColorID aID);
  nscolor GetStandinForNativeColor(ColorID aID);

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
  static bool sUseNativeColors;
  static bool sFindbarModalHighlight;

  static nsXPLookAndFeel* sInstance;
  static bool sShutdown;

  static bool sIsInPrefersReducedMotionForTest;
  static bool sPrefersReducedMotionForTest;

  // True if we shouldn't clear the cache value in RefreshImpl().
  // NOTE: This should be used only for testing.
  bool mShouldRetainCacheForTest;
};

#endif
