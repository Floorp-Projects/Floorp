/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsRFPService_h__
#define __nsRFPService_h__

#include <cstdint>
#include <tuple>
#include "ErrorList.h"
#include "PLDHashTable.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/ContentBlockingLog.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/TypedEnumBits.h"
#include "js/RealmOptions.h"
#include "nsHashtablesFwd.h"
#include "nsICookieJarSettings.h"
#include "nsIFingerprintingWebCompatService.h"
#include "nsIObserver.h"
#include "nsISupports.h"
#include "nsIRFPService.h"
#include "nsStringFwd.h"

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
#  define SPOOFED_UA_OS "Macintosh; Intel Mac OS X 10.15"
#  define SPOOFED_APPVERSION "5.0 (Macintosh)"
#  define SPOOFED_OSCPU "Intel Mac OS X 10.15"
#  define SPOOFED_PLATFORM "MacIntel"
#elif defined(MOZ_WIDGET_ANDROID)
#  define SPOOFED_UA_OS "Android 10; Mobile"
#  define SPOOFED_APPVERSION "5.0 (Android 10)"
#  define SPOOFED_OSCPU "Linux armv81"
#  define SPOOFED_PLATFORM "Linux armv81"
#else
// For Linux and other platforms, like BSDs, SunOS and etc, we will use Linux
// platform.
#  define SPOOFED_UA_OS "X11; Linux x86_64"
#  define SPOOFED_APPVERSION "5.0 (X11)"
#  define SPOOFED_OSCPU "Linux x86_64"
#  define SPOOFED_PLATFORM "Linux x86_64"
#endif

#define LEGACY_BUILD_ID "20181001000000"
#define LEGACY_UA_GECKO_TRAIL "20100101"

#define SPOOFED_POINTER_INTERFACE MouseEvent_Binding::MOZ_SOURCE_MOUSE

// For the HTTP User-Agent header, we use a simpler set of spoofed values
// that do not reveal the specific desktop platform.
#if defined(MOZ_WIDGET_ANDROID)
#  define SPOOFED_HTTP_UA_OS "Android 10; Mobile"
#else
#  define SPOOFED_HTTP_UA_OS "Windows NT 10.0"
#endif

struct JSContext;

class nsIChannel;

namespace mozilla {
class WidgetKeyboardEvent;
class OriginAttributes;
class OriginAttributesPattern;
namespace dom {
class Document;
enum class CanvasContextType : uint8_t;
}  // namespace dom

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
  nsString mKey;
  KeyNameIndex mKeyIdx;
  SpoofingKeyboardCode mSpoofingCode;
};

class KeyboardHashKey : public PLDHashEntryHdr {
 public:
  typedef const KeyboardHashKey& KeyType;
  typedef const KeyboardHashKey* KeyTypePointer;

  KeyboardHashKey(const KeyboardLangs aLang, const KeyboardRegions aRegion,
                  const KeyNameIndexType aKeyIdx, const nsAString& aKey);

  explicit KeyboardHashKey(KeyTypePointer aOther);

  KeyboardHashKey(KeyboardHashKey&& aOther) noexcept;

  ~KeyboardHashKey();

  bool KeyEquals(KeyTypePointer aOther) const;

  static KeyTypePointer KeyToPointer(KeyType aKey);

  static PLDHashNumber HashKey(KeyTypePointer aKey);

  enum { ALLOW_MEMMOVE = true };

  KeyboardLangs mLang;
  KeyboardRegions mRegion;
  KeyNameIndexType mKeyIdx;
  nsString mKey;
};

// ============================================================================

// Reduce Timer Precision (RTP) Caller Type
enum class RTPCallerType : uint8_t {
  Normal = 0,
  SystemPrincipal = (1 << 0),
  ResistFingerprinting = (1 << 1),
  CrossOriginIsolated = (1 << 2)
};

inline JS::RTPCallerTypeToken RTPCallerTypeToToken(RTPCallerType aType) {
  return JS::RTPCallerTypeToken{uint8_t(aType)};
}

inline RTPCallerType RTPCallerTypeFromToken(JS::RTPCallerTypeToken aToken) {
  MOZ_RELEASE_ASSERT(
      aToken.value == uint8_t(RTPCallerType::Normal) ||
      aToken.value == uint8_t(RTPCallerType::SystemPrincipal) ||
      aToken.value == uint8_t(RTPCallerType::ResistFingerprinting) ||
      aToken.value == uint8_t(RTPCallerType::CrossOriginIsolated));
  return static_cast<RTPCallerType>(aToken.value);
}

enum TimerPrecisionType {
  DangerouslyNone = 1,
  UnconditionalAKAHighRes = 2,
  Normal = 3,
  RFP = 4,
};

// ============================================================================

enum class CanvasFeatureUsage : uint8_t {
  None = 0,
  KnownFingerprintText = 1 << 0,
  SetFont = 1 << 1,
  FillRect = 1 << 2,
  LineTo = 1 << 3,
  Stroke = 1 << 4
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(CanvasFeatureUsage);

class CanvasUsage {
 public:
  nsIntSize mSize;
  dom::CanvasContextType mType;
  CanvasFeatureUsage mFeatureUsage;

  CanvasUsage(nsIntSize aSize, dom::CanvasContextType aType,
              CanvasFeatureUsage aFeatureUsage)
      : mSize(aSize), mType(aType), mFeatureUsage(aFeatureUsage) {}
};

// ============================================================================

// NOLINTNEXTLINE(bugprone-macro-parentheses)
#define ITEM_VALUE(name, val) name = val,

// The definition for fingerprinting protections. Each enum represents one
// fingerprinting protection that targets one specific WebAPI our fingerprinting
// surface. The enums can be found in RFPTargets.inc.
enum class RFPTarget : uint64_t {
#include "RFPTargets.inc"
};

#undef ITEM_VALUE

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(RFPTarget);

// ============================================================================

class nsRFPService final : public nsIObserver, public nsIRFPService {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIRFPSERVICE

  static already_AddRefed<nsRFPService> GetOrCreate();

  // _Rarely_ you will need to know if RFP is enabled, or if FPP is enabled.
  // 98% of the time you should use nsContentUtils::ShouldResistFingerprinting
  // as the difference will not matter to you.
  static bool IsRFPPrefEnabled(bool aIsPrivateMode);

  static bool IsRFPEnabledFor(
      bool aIsPrivateMode, RFPTarget aTarget,
      const Maybe<RFPTarget>& aOverriddenFingerprintingSettings);

  // --------------------------------------------------------------------------
  static double TimerResolution(RTPCallerType aRTPCallerType);

  enum TimeScale { Seconds = 1, MilliSeconds = 1000, MicroSeconds = 1000000 };

  // The following Reduce methods can be called off main thread.
  static double ReduceTimePrecisionAsUSecs(double aTime, int64_t aContextMixin,
                                           RTPCallerType aRTPCallerType);
  static double ReduceTimePrecisionAsMSecs(double aTime, int64_t aContextMixin,
                                           RTPCallerType aRTPCallerType);
  static double ReduceTimePrecisionAsMSecsRFPOnly(double aTime,
                                                  int64_t aContextMixin,
                                                  RTPCallerType aRTPCallerType);
  static double ReduceTimePrecisionAsSecs(double aTime, int64_t aContextMixin,
                                          RTPCallerType aRTPCallerType);
  static double ReduceTimePrecisionAsSecsRFPOnly(double aTime,
                                                 int64_t aContextMixin,
                                                 RTPCallerType aRTPCallerType);
  // Public only for testing purposes
  static double ReduceTimePrecisionImpl(double aTime, TimeScale aTimeScale,
                                        double aResolutionUSec,
                                        int64_t aContextMixin,
                                        TimerPrecisionType aType);
  static nsresult RandomMidpoint(long long aClampedTimeUSec,
                                 long long aResolutionUSec,
                                 int64_t aContextMixin, long long* aMidpointOut,
                                 uint8_t* aSecretSeed = nullptr);

  // --------------------------------------------------------------------------

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

  // --------------------------------------------------------------------------

  // This method generates the spoofed value of User Agent.
  static void GetSpoofedUserAgent(nsACString& userAgent, bool isForHTTPHeader);

  // --------------------------------------------------------------------------

  // This method generates the locale string (e.g. "en-US") that should be
  // spoofed by the JavaScript engine.
  static nsCString GetSpoofedJSLocale();

  // --------------------------------------------------------------------------

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

  // --------------------------------------------------------------------------

  // The method to generate the key for randomization. It can return nothing if
  // the session key is not available due to the randomization is disabled.
  static Maybe<nsTArray<uint8_t>> GenerateKey(nsIChannel* aChannel);

  // The method to add random noises to the image data based on the random key
  // of the given cookieJarSettings.
  static nsresult RandomizePixels(nsICookieJarSettings* aCookieJarSettings,
                                  uint8_t* aData, uint32_t aWidth,
                                  uint32_t aHeight, uint32_t aSize,
                                  mozilla::gfx::SurfaceFormat aSurfaceFormat);

  // --------------------------------------------------------------------------

  // The method for getting the granular fingerprinting protection override of
  // the given channel. Due to WebCompat reason, there can be a granular
  // overrides to replace default enabled RFPTargets for the context of the
  // channel. The method will return Nothing() to indicate using the default
  // RFPTargets
  static Maybe<RFPTarget> GetOverriddenFingerprintingSettingsForChannel(
      nsIChannel* aChannel);

  // The method for getting the granular fingerprinting protection override of
  // the given first-party and third-party URIs. It will return the granular
  // overrides if there is one defined for the context of the first-party URI
  // and third-party URI. Otherwise, it will return Nothing() to indicate using
  // the default RFPTargets.
  static Maybe<RFPTarget> GetOverriddenFingerprintingSettingsForURI(
      nsIURI* aFirstPartyURI, nsIURI* aThirdPartyURI);

  // --------------------------------------------------------------------------

  static void MaybeReportCanvasFingerprinter(nsTArray<CanvasUsage>& aUses,
                                             nsIChannel* aChannel,
                                             nsACString& aOriginNoSuffix);

  static void MaybeReportFontFingerprinter(nsIChannel* aChannel,
                                           nsACString& aOriginNoSuffix);

  // --------------------------------------------------------------------------

  // A helper function to check if there is a suspicious fingerprinting
  // activity from given content blocking origin logs. It returns true if we
  // detect suspicious fingerprinting activities.
  static bool CheckSuspiciousFingerprintingActivity(
      nsTArray<ContentBlockingLog::LogEntry>& aLogs);

 private:
  nsresult Init();

  nsRFPService() = default;

  ~nsRFPService() = default;

  void UpdateFPPOverrideList();
  void StartShutdown();

  void PrefChanged(const char* aPref);
  static void PrefChanged(const char* aPref, void* aSelf);

  static Maybe<RFPTarget> TextToRFPTarget(const nsAString& aText);

  // --------------------------------------------------------------------------

  static void MaybeCreateSpoofingKeyCodes(const KeyboardLangs aLang,
                                          const KeyboardRegions aRegion);
  static void MaybeCreateSpoofingKeyCodesForEnUS();

  static void GetKeyboardLangAndRegion(const nsAString& aLanguage,
                                       KeyboardLangs& aLocale,
                                       KeyboardRegions& aRegion);
  static bool GetSpoofedKeyCodeInfo(const mozilla::dom::Document* aDoc,
                                    const WidgetKeyboardEvent* aKeyboardEvent,
                                    SpoofingKeyboardCode& aOut);

  static nsTHashMap<KeyboardHashKey, const SpoofingKeyboardCode*>*
      sSpoofingKeyboardCodes;

  // --------------------------------------------------------------------------

  // Used by the JS Engine
  static double ReduceTimePrecisionAsUSecsWrapper(
      double aTime, JS::RTPCallerTypeToken aCallerType, JSContext* aCx);

  static TimerPrecisionType GetTimerPrecisionType(RTPCallerType aRTPCallerType);

  static TimerPrecisionType GetTimerPrecisionTypeRFPOnly(
      RTPCallerType aRTPCallerType);

  static void TypeToText(TimerPrecisionType aType, nsACString& aText);

  // --------------------------------------------------------------------------

  // A helper function to generate canvas key from the given image data and
  // randomization key.
  static nsresult GenerateCanvasKeyFromImageData(
      nsICookieJarSettings* aCookieJarSettings, uint8_t* aImageData,
      uint32_t aSize, nsTArray<uint8_t>& aCanvasKey);

  // Generate the session key if it hasn't been generated.
  nsresult GetBrowsingSessionKey(const OriginAttributes& aOriginAttributes,
                                 nsID& aBrowsingSessionKey);
  void ClearBrowsingSessionKey(const OriginAttributesPattern& aPattern);
  void ClearBrowsingSessionKey(const OriginAttributes& aOriginAttributes);

  // The keys that represent the browsing session. The lifetime of the key ties
  // to the browsing session. For normal windows, the key is generated when
  // loading the first http channel after the browser startup and persists until
  // the browser shuts down. For private windows, the key is generated when
  // opening a http channel on a private window and reset after all private
  // windows close, i.e. private browsing session ends.
  //
  // The key will be used to generate the randomization noise used to fiddle the
  // browser fingerprints. Note that this key lives and can only be accessed in
  // the parent process.
  nsTHashMap<nsCStringHashKey, nsID> mBrowsingSessionKeys;

  nsCOMPtr<nsIFingerprintingWebCompatService> mWebCompatService;
  nsTHashMap<nsCStringHashKey, RFPTarget> mFingerprintingOverrides;

  // A helper function to create the domain key for the fingerprinting
  // overrides. The key can be in the following five formats.
  // 1. {first-party domain}: The override only apply to the first-party domain.
  // 2. {first-party domain, *}: The overrides apply to every contexts under the
  //    top-level domain, including itself.
  // 3. {*, third-party domain}: The overrides apply to the third-party domain
  //    under any top-level domain.
  // 4. {first-party domain, third-party domain}: the overrides apply to the
  //    specific third-party domain under the given first-party domain.
  // 5. {*}: A global overrides that will apply to every context.
  static nsresult CreateOverrideDomainKey(nsIFingerprintingOverride* aOverride,
                                          nsACString& aDomainKey);

  // A helper function to create the RFPTarget bitfield based on the given
  // overrides text and the based overrides bitfield. The function will parse
  // the text and update the based overrides bitfield accordingly. Then, it will
  // return the updated bitfield.
  static RFPTarget CreateOverridesFromText(
      const nsString& aOverridesText, RFPTarget aBaseOverrides = RFPTarget(0));
};

}  // namespace mozilla

#endif /* __nsRFPService_h__ */
