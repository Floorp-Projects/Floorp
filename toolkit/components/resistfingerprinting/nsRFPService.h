/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsRFPService_h__
#define __nsRFPService_h__

#include "mozilla/Atomics.h"
#include "mozilla/EventForwards.h"
#include "mozilla/Mutex.h"
#include "mozilla/dom/Document.h"
#include "nsIObserver.h"

#include "nsDataHashtable.h"
#include "nsString.h"

// Defines regarding spoofed values of Navigator object. These spoofed values
// are returned when 'privacy.resistFingerprinting' is true.
// We decided to give different spoofed values according to the platform. The
// reason is that it is easy to detect the real platform. So there is no benefit
// for hiding the platform: it only brings breakages, like keyboard shortcuts
// won't work in macOS if we spoof it as a Windows platform.
#ifdef XP_WIN
#  define SPOOFED_UA_OS "Windows NT 10.0; Win64; x64"
#  define SPOOFED_APPVERSION "5.0 (Windows)"
#  define SPOOFED_OSCPU "Windows NT 10.0; Win64; x64"
#  define SPOOFED_PLATFORM "Win32"
#elif defined(XP_MACOSX)
#  define SPOOFED_UA_OS "Macintosh; Intel Mac OS X 10.14"
#  define SPOOFED_APPVERSION "5.0 (Macintosh)"
#  define SPOOFED_OSCPU "Intel Mac OS X 10.14"
#  define SPOOFED_PLATFORM "MacIntel"
#elif defined(MOZ_WIDGET_ANDROID)
#  define SPOOFED_UA_OS "Android 8.1; Mobile"
#  define SPOOFED_APPVERSION "5.0 (Android 8.1)"
#  define SPOOFED_OSCPU "Linux armv7l"
#  define SPOOFED_PLATFORM "Linux armv7l"
#else
// For Linux and other platforms, like BSDs, SunOS and etc, we will use Linux
// platform.
#  define SPOOFED_UA_OS "X11; Linux x86_64"
#  define SPOOFED_APPVERSION "5.0 (X11)"
#  define SPOOFED_OSCPU "Linux x86_64"
#  define SPOOFED_PLATFORM "Linux x86_64"
#endif

#define SPOOFED_APPNAME "Netscape"
#define LEGACY_BUILD_ID "20181001000000"
#define LEGACY_UA_GECKO_TRAIL "20100101"

#define SPOOFED_POINTER_INTERFACE MouseEvent_Binding::MOZ_SOURCE_MOUSE

// For the HTTP User-Agent header, we use a simpler set of spoofed values
// that do not reveal the specific desktop platform.
#if defined(MOZ_WIDGET_ANDROID)
#  define SPOOFED_HTTP_UA_OS "Android 6.0; Mobile"
#else
#  define SPOOFED_HTTP_UA_OS "Windows NT 10.0"
#endif

// Forward declare LRUCache, defined in nsRFPService.cpp
class LRUCache;

namespace mozilla {

enum KeyboardLang { EN = 0x01 };

#define RFP_KEYBOARD_LANG_STRING_EN "en"

typedef uint8_t KeyboardLangs;

enum KeyboardRegion { US = 0x01 };

#define RFP_KEYBOARD_REGION_STRING_US "US"

typedef uint8_t KeyboardRegions;

// This struct has the information about how to spoof the keyboardEvent.code,
// keyboardEvent.keycode and modifier states.
struct SpoofingKeyboardCode {
  CodeNameIndex mCode;
  uint8_t mKeyCode;
  Modifiers mModifierStates;
};

struct SpoofingKeyboardInfo {
  KeyNameIndex mKeyIdx;
  nsString mKey;
  SpoofingKeyboardCode mSpoofingCode;
};

class KeyboardHashKey : public PLDHashEntryHdr {
 public:
  typedef const KeyboardHashKey& KeyType;
  typedef const KeyboardHashKey* KeyTypePointer;

  KeyboardHashKey(const KeyboardLangs aLang, const KeyboardRegions aRegion,
                  const KeyNameIndexType aKeyIdx, const nsAString& aKey)
      : mLang(aLang), mRegion(aRegion), mKeyIdx(aKeyIdx), mKey(aKey) {}

  explicit KeyboardHashKey(KeyTypePointer aOther)
      : mLang(aOther->mLang),
        mRegion(aOther->mRegion),
        mKeyIdx(aOther->mKeyIdx),
        mKey(aOther->mKey) {}

  KeyboardHashKey(KeyboardHashKey&& aOther)
      : PLDHashEntryHdr(std::move(aOther)),
        mLang(std::move(aOther.mLang)),
        mRegion(std::move(aOther.mRegion)),
        mKeyIdx(std::move(aOther.mKeyIdx)),
        mKey(std::move(aOther.mKey)) {}

  ~KeyboardHashKey() {}

  bool KeyEquals(KeyTypePointer aOther) const {
    return mLang == aOther->mLang && mRegion == aOther->mRegion &&
           mKeyIdx == aOther->mKeyIdx && mKey == aOther->mKey;
  }

  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }

  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    PLDHashNumber hash = mozilla::HashString(aKey->mKey);
    return mozilla::AddToHash(hash, aKey->mRegion, aKey->mKeyIdx, aKey->mLang);
  }

  enum { ALLOW_MEMMOVE = true };

  KeyboardLangs mLang;
  KeyboardRegions mRegion;
  KeyNameIndexType mKeyIdx;
  nsString mKey;
};

enum TimerPrecisionType { All = 1, RFPOnly = 2 };

class nsRFPService final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static nsRFPService* GetOrCreate();
  static bool IsResistFingerprintingEnabled();
  static bool IsTimerPrecisionReductionEnabled(TimerPrecisionType aType);
  static double TimerResolution();

  enum TimeScale { Seconds = 1, MilliSeconds = 1000, MicroSeconds = 1000000 };

  // The following Reduce methods can be called off main thread.
  static double ReduceTimePrecisionAsUSecs(
      double aTime, int64_t aContextMixin,
      TimerPrecisionType aType = TimerPrecisionType::All);
  static double ReduceTimePrecisionAsMSecs(
      double aTime, int64_t aContextMixin,
      TimerPrecisionType aType = TimerPrecisionType::All);
  static double ReduceTimePrecisionAsSecs(
      double aTime, int64_t aContextMixin,
      TimerPrecisionType aType = TimerPrecisionType::All);

  // Used by the JS Engine, as it doesn't know about the TimerPrecisionType enum
  static double ReduceTimePrecisionAsUSecsWrapper(double aTime);

  // Public only for testing purposes
  static double ReduceTimePrecisionImpl(double aTime, TimeScale aTimeScale,
                                        double aResolutionUSec,
                                        int64_t aContextMixin,
                                        TimerPrecisionType aType);
  static nsresult RandomMidpoint(long long aClampedTimeUSec,
                                 long long aResolutionUSec,
                                 int64_t aContextMixin, long long* aMidpointOut,
                                 uint8_t* aSecretSeed = nullptr);

  // This method calculates the video resolution (i.e. height x width) based
  // on the video quality (480p, 720p, etc).
  static uint32_t CalculateTargetVideoResolution(uint32_t aVideoQuality);

  // Methods for getting spoofed media statistics and the return value will
  // depend on the video resolution.
  static uint32_t GetSpoofedTotalFrames(double aTime);
  static uint32_t GetSpoofedDroppedFrames(double aTime, uint32_t aWidth,
                                          uint32_t aHeight);
  static uint32_t GetSpoofedPresentedFrames(double aTime, uint32_t aWidth,
                                            uint32_t aHeight);

  // This method generates the spoofed value of User Agent.
  static void GetSpoofedUserAgent(nsACString& userAgent, bool isForHTTPHeader);

  /**
   * This method for getting spoofed modifier states for the given keyboard
   * event.
   *
   * @param aDoc           [in]  the owner's document for getting content
   *                             language.
   * @param aKeyboardEvent [in]  the keyboard event that needs to be spoofed.
   * @param aModifier      [in]  the modifier that needs to be spoofed.
   * @param aOut           [out] the spoofed state for the given modifier.
   * @return               true if there is a spoofed state for the modifier.
   */
  static bool GetSpoofedModifierStates(
      const mozilla::dom::Document* aDoc,
      const WidgetKeyboardEvent* aKeyboardEvent, const Modifiers aModifier,
      bool& aOut);

  /**
   * This method for getting spoofed code for the given keyboard event.
   *
   * @param aDoc           [in]  the owner's document for getting content
   *                             language.
   * @param aKeyboardEvent [in]  the keyboard event that needs to be spoofed.
   * @param aOut           [out] the spoofed code.
   * @return               true if there is a spoofed code in the fake keyboard
   *                       layout.
   */
  static bool GetSpoofedCode(const dom::Document* aDoc,
                             const WidgetKeyboardEvent* aKeyboardEvent,
                             nsAString& aOut);

  /**
   * This method for getting spoofed keyCode for the given keyboard event.
   *
   * @param aDoc           [in]  the owner's document for getting content
   *                             language.
   * @param aKeyboardEvent [in]  the keyboard event that needs to be spoofed.
   * @param aOut           [out] the spoofed keyCode.
   * @return               true if there is a spoofed keyCode in the fake
   *                       keyboard layout.
   */
  static bool GetSpoofedKeyCode(const mozilla::dom::Document* aDoc,
                                const WidgetKeyboardEvent* aKeyboardEvent,
                                uint32_t& aOut);

 private:
  nsresult Init();

  nsRFPService() {}

  ~nsRFPService() {}

  void UpdateTimers();
  void UpdateRFPPref();
  void StartShutdown();

  void PrefChanged(const char* aPref);

  static void MaybeCreateSpoofingKeyCodes(const KeyboardLangs aLang,
                                          const KeyboardRegions aRegion);
  static void MaybeCreateSpoofingKeyCodesForEnUS();

  static void GetKeyboardLangAndRegion(const nsAString& aLanguage,
                                       KeyboardLangs& aLocale,
                                       KeyboardRegions& aRegion);
  static bool GetSpoofedKeyCodeInfo(const mozilla::dom::Document* aDoc,
                                    const WidgetKeyboardEvent* aKeyboardEvent,
                                    SpoofingKeyboardCode& aOut);

  static nsDataHashtable<KeyboardHashKey, const SpoofingKeyboardCode*>*
      sSpoofingKeyboardCodes;

  nsCString mInitialTZValue;
};

}  // namespace mozilla

#endif /* __nsRFPService_h__ */
