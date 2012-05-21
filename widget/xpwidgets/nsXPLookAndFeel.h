/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsXPLookAndFeel
#define __nsXPLookAndFeel

#include "mozilla/LookAndFeel.h"

class nsLookAndFeel;

struct nsLookAndFeelIntPref
{
  const char* name;
  mozilla::LookAndFeel::IntID id;
  bool isSet;
  PRInt32 intVar;
};

struct nsLookAndFeelFloatPref
{
  const char* name;
  mozilla::LookAndFeel::FloatID id;
  bool isSet;
  float floatVar;
};

#define CACHE_BLOCK(x)     ((x) >> 5)
#define CACHE_BIT(x)       (1 << ((x) & 31))

#define COLOR_CACHE_SIZE   (CACHE_BLOCK(LookAndFeel::eColorID_LAST_COLOR) + 1)
#define IS_COLOR_CACHED(x) (CACHE_BIT(x) & nsXPLookAndFeel::sCachedColorBits[CACHE_BLOCK(x)])
#define CLEAR_COLOR_CACHE(x) nsXPLookAndFeel::sCachedColors[(x)] =0; \
              nsXPLookAndFeel::sCachedColorBits[CACHE_BLOCK(x)] &= ~(CACHE_BIT(x));
#define CACHE_COLOR(x, y)  nsXPLookAndFeel::sCachedColors[(x)] = y; \
              nsXPLookAndFeel::sCachedColorBits[CACHE_BLOCK(x)] |= CACHE_BIT(x);

class nsXPLookAndFeel: public mozilla::LookAndFeel
{
public:
  virtual ~nsXPLookAndFeel();

  static nsLookAndFeel* GetInstance();
  static void Shutdown();

  void Init();

  //
  // All these routines will return NS_OK if they have a value,
  // in which case the nsLookAndFeel should use that value;
  // otherwise we'll return NS_ERROR_NOT_AVAILABLE, in which case, the
  // platform-specific nsLookAndFeel should use its own values instead.
  //
  nsresult GetColorImpl(ColorID aID, nscolor &aResult);
  virtual nsresult GetIntImpl(IntID aID, PRInt32 &aResult);
  virtual nsresult GetFloatImpl(FloatID aID, float &aResult);

  // This one is different: there are no override prefs (fixme?), so
  // there is no XP implementation, only per-system impls.
  virtual bool GetFontImpl(FontID aID, nsString& aName,
                           gfxFontStyle& aStyle) = 0;

  virtual void RefreshImpl();

  virtual PRUnichar GetPasswordCharacterImpl()
  {
    return PRUnichar('*');
  }

  virtual bool GetEchoPasswordImpl()
  {
    return false;
  }

protected:
  nsXPLookAndFeel();

  static void IntPrefChanged(nsLookAndFeelIntPref *data);
  static void FloatPrefChanged(nsLookAndFeelFloatPref *data);
  static void ColorPrefChanged(unsigned int index, const char *prefName);
  void InitFromPref(nsLookAndFeelIntPref* aPref);
  void InitFromPref(nsLookAndFeelFloatPref* aPref);
  void InitColorFromPref(PRInt32 aIndex);
  virtual nsresult NativeGetColor(ColorID aID, nscolor &aResult) = 0;
  bool IsSpecialColor(ColorID aID, nscolor &aColor);

  static int OnPrefChanged(const char* aPref, void* aClosure);

  static bool sInitialized;
  static nsLookAndFeelIntPref sIntPrefs[];
  static nsLookAndFeelFloatPref sFloatPrefs[];
  /* this length must not be shorter than the length of the longest string in the array
   * see nsXPLookAndFeel.cpp
   */
  static const char sColorPrefs[][38];
  static PRInt32 sCachedColors[LookAndFeel::eColorID_LAST_COLOR];
  static PRInt32 sCachedColorBits[COLOR_CACHE_SIZE];
  static bool sUseNativeColors;

  static nsLookAndFeel* sInstance;
  static bool sShutdown;
};

#endif
