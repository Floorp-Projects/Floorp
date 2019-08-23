/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRFPService.h"

#include <algorithm>
#include <memory>
#include <time.h>

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Logging.h"
#include "mozilla/Mutex.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/KeyboardEventBinding.h"

#include "nsCOMPtr.h"
#include "nsCoord.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsXULAppAPI.h"
#include "nsPrintfCString.h"

#include "nsICryptoHash.h"
#include "nsIObserverService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIRandomGenerator.h"
#include "nsIXULAppInfo.h"
#include "nsIXULRuntime.h"
#include "nsJSUtils.h"

#include "prenv.h"
#include "nss.h"

#include "js/Date.h"

using namespace mozilla;
using namespace std;

static mozilla::LazyLogModule gResistFingerprintingLog(
    "nsResistFingerprinting");

#define RESIST_FINGERPRINTING_PREF "privacy.resistFingerprinting"
#define RFP_TIMER_PREF "privacy.reduceTimerPrecision"
#define RFP_TIMER_UNCONDITIONAL_PREF \
  "privacy.reduceTimerPrecision.unconditional"
#define RFP_TIMER_UNCONDITIONAL_VALUE 20
#define RFP_TIMER_VALUE_PREF \
  "privacy.resistFingerprinting.reduceTimerPrecision.microseconds"
#define RFP_JITTER_VALUE_PREF \
  "privacy.resistFingerprinting.reduceTimerPrecision.jitter"
#define PROFILE_INITIALIZED_TOPIC "profile-initial-state"

static constexpr uint32_t kVideoFramesPerSec = 30;
static constexpr uint32_t kVideoDroppedRatio = 5;

#define RFP_DEFAULT_SPOOFING_KEYBOARD_LANG KeyboardLang::EN
#define RFP_DEFAULT_SPOOFING_KEYBOARD_REGION KeyboardRegion::US

NS_IMPL_ISUPPORTS(nsRFPService, nsIObserver)

static StaticRefPtr<nsRFPService> sRFPService;
static bool sInitialized = false;
nsDataHashtable<KeyboardHashKey, const SpoofingKeyboardCode*>*
    nsRFPService::sSpoofingKeyboardCodes = nullptr;
static mozilla::StaticMutex sLock;

/* static */
nsRFPService* nsRFPService::GetOrCreate() {
  if (!sInitialized) {
    sRFPService = new nsRFPService();
    nsresult rv = sRFPService->Init();

    if (NS_FAILED(rv)) {
      sRFPService = nullptr;
      return nullptr;
    }

    ClearOnShutdown(&sRFPService);
    sInitialized = true;
  }

  return sRFPService;
}

/* static */
double nsRFPService::TimerResolution() {
  double prefValue = StaticPrefs::
      privacy_resistFingerprinting_reduceTimerPrecision_microseconds();
  if (nsRFPService::IsResistFingerprintingEnabled()) {
    return max(100000.0, prefValue);
  }
  return prefValue;
}

/* static */
bool nsRFPService::IsResistFingerprintingEnabled() {
  return StaticPrefs::privacy_resistFingerprinting();
}

/* static */
bool nsRFPService::IsTimerPrecisionReductionEnabled(TimerPrecisionType aType) {
  if (aType == TimerPrecisionType::RFPOnly) {
    return IsResistFingerprintingEnabled();
  }

  return (StaticPrefs::privacy_reduceTimerPrecision() ||
          IsResistFingerprintingEnabled()) &&
         TimerResolution() > 0;
}

/*
 * The below is a simple time-based Least Recently Used cache used to store the
 * result of a cryptographic hash function. It has LRU_CACHE_SIZE slots, and
 * will be used from multiple threads. It is thread-safe.
 */
#define LRU_CACHE_SIZE (45)
#define HASH_DIGEST_SIZE_BITS (256)
#define HASH_DIGEST_SIZE_BYTES (HASH_DIGEST_SIZE_BITS / 8)

class LRUCache final {
 public:
  LRUCache() : mLock("mozilla.resistFingerprinting.LRUCache") {
    this->cache.SetLength(LRU_CACHE_SIZE);
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(LRUCache)

  nsCString Get(long long aKeyPart1, long long aKeyPart2) {
    for (auto& cacheEntry : this->cache) {
      // Read optimistically befor locking
      if (cacheEntry.keyPart1 == aKeyPart1 &&
          cacheEntry.keyPart2 == aKeyPart2) {
        MutexAutoLock lock(mLock);

        // Double check after we have a lock
        if (MOZ_UNLIKELY(cacheEntry.keyPart1 != aKeyPart1 ||
                         cacheEntry.keyPart2 != aKeyPart2)) {
          // Got evicted in a race
          long long tmp_keyPart1 = cacheEntry.keyPart1;
          long long tmp_keyPart2 = cacheEntry.keyPart2;
          MOZ_LOG(gResistFingerprintingLog, LogLevel::Verbose,
                  ("LRU Cache HIT-MISS with %lli != %lli and %lli != %lli",
                   aKeyPart1, tmp_keyPart1, aKeyPart2, tmp_keyPart2));
          return EmptyCString();
        }

        cacheEntry.accessTime = PR_Now();
        MOZ_LOG(gResistFingerprintingLog, LogLevel::Verbose,
                ("LRU Cache HIT with %lli %lli", aKeyPart1, aKeyPart2));
        return cacheEntry.data;
      }
    }

    return EmptyCString();
  }

  void Store(long long aKeyPart1, long long aKeyPart2,
             const nsCString& aValue) {
    MOZ_DIAGNOSTIC_ASSERT(aValue.Length() == HASH_DIGEST_SIZE_BYTES);
    MutexAutoLock lock(mLock);

    CacheEntry* lowestKey = &this->cache[0];
    for (auto& cacheEntry : this->cache) {
      if (MOZ_UNLIKELY(cacheEntry.keyPart1 == aKeyPart1 &&
                       cacheEntry.keyPart2 == aKeyPart2)) {
        // Another thread inserted before us, don't insert twice
        MOZ_LOG(
            gResistFingerprintingLog, LogLevel::Verbose,
            ("LRU Cache DOUBLE STORE with %lli %lli", aKeyPart1, aKeyPart2));
        return;
      }
      if (cacheEntry.accessTime < lowestKey->accessTime) {
        lowestKey = &cacheEntry;
      }
    }

    lowestKey->keyPart1 = aKeyPart1;
    lowestKey->keyPart2 = aKeyPart2;
    lowestKey->data = aValue;
    lowestKey->accessTime = PR_Now();
    MOZ_LOG(gResistFingerprintingLog, LogLevel::Verbose,
            ("LRU Cache STORE with %lli %lli", aKeyPart1, aKeyPart2));
  }

 private:
  ~LRUCache() = default;

  struct CacheEntry {
    Atomic<long long, Relaxed> keyPart1;
    Atomic<long long, Relaxed> keyPart2;
    PRTime accessTime = 0;
    nsCString data;

    CacheEntry() {
      this->keyPart1 = 0xFFFFFFFFFFFFFFFF;
      this->keyPart2 = 0xFFFFFFFFFFFFFFFF;
      this->accessTime = 0;
      this->data = nullptr;
    }
    CacheEntry(const CacheEntry& obj) {
      this->keyPart1.exchange(obj.keyPart1);
      this->keyPart2.exchange(obj.keyPart2);
      this->accessTime = obj.accessTime;
      this->data = obj.data;
    }
  };

  AutoTArray<CacheEntry, LRU_CACHE_SIZE> cache;
  mozilla::Mutex mLock;
};

// We make a single LRUCache
static StaticRefPtr<LRUCache> sCache;

/**
 * The purpose of this function is to deterministicly generate a random midpoint
 * between a lower clamped value and an upper clamped value. Assuming a clamping
 * resolution of 100, here is an example:
 *
 * |---------------------------------------|--------------------------|
 * lower clamped value (e.g. 300)          |           upper clamped value (400)
 *                              random midpoint (e.g. 360)
 *
 * If our actual timestamp (e.g. 325) is below the midpoint, we keep it clamped
 * downwards. If it were equal to or above the midpoint (e.g. 365) we would
 * round it upwards to the largest clamped value (in this example: 400).
 *
 * The question is: does time go backwards?
 *
 * The midpoint is deterministicly random and generated from three components:
 * a secret seed, a per-timeline (context) 'mix-in', and a clamped time.
 *
 * When comparing times across different seed values: time may go backwards.
 * For a clamped time of 300, one seed may generate a midpoint of 305 and
 * another 395. So comparing an (actual) timestamp of 325 and 351 could see the
 * 325 clamped up to 400 and the 351 clamped down to 300. The seed is
 * per-process, so this case occurs when one can compare timestamps
 * cross-process. This is uncommon (because we don't have site isolation.) The
 * circumstances this could occur are BroadcastChannel, Storage Notification,
 * and in theory (but not yet implemented) SharedWorker. This should be an
 * exhaustive list (at time of comment writing!).
 *
 * Aside from cross-process communication, derived timestamps across different
 * time origins may go backwards. (Specifically, derived means adding two
 * timestamps together to get an (approximate) absolute time.)
 * Assume a page and a worker. If one calls performance.now() in the page and
 * then triggers a call to performance.now() in the worker, the following
 * invariant should hold true:
 *             page.performance.timeOrigin + page.performance.now() <
 *                      worker.performance.timeOrigin + worker.performance.now()
 *
 * We break this invariant.
 *
 * The 'Context Mix-in' is a securely generated random seed that is unique for
 * each timeline that starts over at zero. It is needed to ensure that the
 * sequence of midpoints (as calculated by the secret seed and clamped time)
 * does not repeat. In RelativeTimeline.h, we define a 'RelativeTimeline' class
 * that can be inherited by any object that has a relative timeline. The most
 * obvious examples are Documents and Workers. An attacker could let time go
 * forward and observe (roughly) where the random midpoints fall. Then they
 * create a new object, time starts back over at zero, and they know
 * (approximately) where the random midpoints are.
 *
 * When the timestamp given is a non-relative timestamp (e.g. it is relative to
 * the unix epoch) it is not possible to replay a sequence of random values.
 * Thus, providing a zero context pointer is an indicator that the timestamp
 * given is absolute and does not need any additional randomness.
 *
 * @param aClampedTimeUSec [in]  The clamped input time in microseconds.
 * @param aResolutionUSec  [in]  The current resolution for clamping in
 *                               microseconds.
 * @param aMidpointOut     [out] The midpoint, in microseconds, between [0,
 *                               aResolutionUSec].
 * @param aContextMixin    [in]  An opaque random value for relative
 *                               timestamps. 0 for absolute timestamps
 * @param aSecretSeed      [in]  TESTING ONLY. When provided, the current seed
 *                               will be replaced with this value.
 * @return                 A nsresult indicating success of failure. If the
 *                         function failed, nothing is written to aMidpointOut
 */

/* static */
nsresult nsRFPService::RandomMidpoint(long long aClampedTimeUSec,
                                      long long aResolutionUSec,
                                      int64_t aContextMixin,
                                      long long* aMidpointOut,
                                      uint8_t* aSecretSeed /* = nullptr */) {
  nsresult rv;
  const int kSeedSize = 16;
  const int kClampTimesPerDigest = HASH_DIGEST_SIZE_BITS / 32;
  static uint8_t* sSecretMidpointSeed = nullptr;

  if (MOZ_UNLIKELY(!aMidpointOut)) {
    return NS_ERROR_INVALID_ARG;
  }

  RefPtr<LRUCache> cache;
  {
    StaticMutexAutoLock lock(sLock);
    cache = sCache;
  }

  if (!cache) {
    return NS_ERROR_FAILURE;
  }

  /*
   * Below, we will call a cryptographic hash function. That's expensive. We
   * look for ways to make it more efficient.
   *
   * We only need as much output from the hash function as the maximum
   * resolution we will ever support, because we will reduce the output modulo
   * that value. The maximum resolution we think is likely is in the low seconds
   * value, or about 1-10 million microseconds. 2**24 is 16 million, so we only
   * need 24 bits of output. Practically speaking though, it's way easier to
   * work with 32 bits.
   *
   * So we're using 32 bits of output and throwing away the other DIGEST_SIZE -
   * 32 (in the case of SHA-256, DIGEST_SIZE is 256.)  That's a lot of waste.
   *
   * Instead of throwing it away, we're going to use all of it. We can handle
   * DIGEST_SIZE / 32 Clamped Time's per hash function - call that , so we
   * reduce aClampedTime to a multiple of kClampTimesPerDigest (just like we
   * reduced the real time value to aClampedTime!)
   *
   * Then we hash _that_ value (assuming it's not in the cache) and index into
   * the digest result the appropriate bit offset.
   */
  long long reducedResolution = aResolutionUSec * kClampTimesPerDigest;
  long long extraClampedTime =
      (aClampedTimeUSec / reducedResolution) * reducedResolution;

  nsCString hashResult = cache->Get(extraClampedTime, aContextMixin);

  if (hashResult.Length() != HASH_DIGEST_SIZE_BYTES) {  // Cache Miss =(
    // If someone has pased in the testing-only parameter, replace our seed with
    // it
    if (aSecretSeed != nullptr) {
      StaticMutexAutoLock lock(sLock);

      delete[] sSecretMidpointSeed;

      sSecretMidpointSeed = new uint8_t[kSeedSize];
      memcpy(sSecretMidpointSeed, aSecretSeed, kSeedSize);
    }

    // If we don't have a seed, we need to get one.
    if (MOZ_UNLIKELY(!sSecretMidpointSeed)) {
      nsCOMPtr<nsIRandomGenerator> randomGenerator =
          do_GetService("@mozilla.org/security/random-generator;1", &rv);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      StaticMutexAutoLock lock(sLock);
      if (MOZ_LIKELY(!sSecretMidpointSeed)) {
        rv = randomGenerator->GenerateRandomBytes(kSeedSize,
                                                  &sSecretMidpointSeed);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
    }

    /*
     * Use a cryptographicly secure hash function, but do _not_ use an HMAC.
     * Obviously we're not using this data for authentication purposes, but
     * even still an HMAC is a perfect fit here, as we're hashing a value
     * using a seed that never changes, and an input that does. So why not
     * use one?
     *
     * Basically - we don't need to, it's two invocations of the hash function,
     * and speed really counts here.
     *
     * With authentication off the table, the properties we would get by
     * using an HMAC here would be:
     *  - Resistence to length extension
     *  - Resistence to collision attacks on the underlying hash function
     *  - Resistence to chosen prefix attacks
     *
     * There is no threat of length extension here. Nor is there any real
     * practical threat of collision: not only are we using a good hash
     * function (you may mock me in 10 years if it is broken) but we don't
     * provide the attacker much control over the input. Nor do we let them
     * have the prefix.
     */

    // Then hash extraClampedTime and store it in the cache
    nsCOMPtr<nsICryptoHash> hasher =
        do_CreateInstance("@mozilla.org/security/hash;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = hasher->Init(nsICryptoHash::SHA256);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = hasher->Update(sSecretMidpointSeed, kSeedSize);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = hasher->Update((const uint8_t*)&aContextMixin, sizeof(aContextMixin));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = hasher->Update((const uint8_t*)&extraClampedTime,
                        sizeof(extraClampedTime));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCStringN<HASH_DIGEST_SIZE_BYTES> derivedSecret;
    rv = hasher->Finish(false, derivedSecret);
    NS_ENSURE_SUCCESS(rv, rv);

    // Finally, store it in the cache
    cache->Store(extraClampedTime, aContextMixin, derivedSecret);
    hashResult = derivedSecret;
  }

  // Offset the appropriate index into the hash output, and then turn it into a
  // random midpoint between 0 and aResolutionUSec. Sometimes out input time is
  // negative, we ride the negative out to the end until we start doing pointer
  // math. (We also triple check we're in bounds.)
  int byteOffset =
      abs(((aClampedTimeUSec - extraClampedTime) / aResolutionUSec) * 4);
  if (MOZ_UNLIKELY(byteOffset > (HASH_DIGEST_SIZE_BYTES - 4))) {
    byteOffset = 0;
  }
  uint32_t deterministiclyRandomValue = *BitwiseCast<uint32_t*>(
      PromiseFlatCString(hashResult).get() + byteOffset);
  deterministiclyRandomValue %= aResolutionUSec;
  *aMidpointOut = deterministiclyRandomValue;

  return NS_OK;
}

/**
 * Given a precision value, this function will reduce a given input time to the
 * nearest multiple of that precision.
 *
 * It will check if it is appropriate to clamp the input time according to the
 * values of the privacy.resistFingerprinting and privacy.reduceTimerPrecision
 * preferences.  Note that while it will check these prefs, it will use
 * whatever precision is given to it, so if one desires a minimum precision for
 * Resist Fingerprinting, it is the caller's responsibility to provide the
 * correct value. This means you should pass TimerResolution(), which enforces
 * a minimum vale on the precision based on preferences.
 *
 * It ensures the given precision value is greater than zero, if it is not it
 * returns the input time.
 *
 * @param aTime           [in] The input time to be clamped.
 * @param aTimeScale      [in] The units the input time is in (Seconds,
 *                             Milliseconds, or Microseconds).
 * @param aResolutionUSec [in] The precision (in microseconds) to clamp to.
 * @param aContextMixin   [in] An opaque random value for relative timestamps.
 *                             0 for absolute timestamps
 * @return                 If clamping is appropriate, the clamped value of the
 *                         input, otherwise the input.
 */
/* static */
double nsRFPService::ReduceTimePrecisionImpl(double aTime, TimeScale aTimeScale,
                                             double aResolutionUSec,
                                             int64_t aContextMixin,
                                             TimerPrecisionType aType) {
  // This boolean will serve as a flag indicating we are clamping the time
  // unconditionally. We do this when timer reduction preference is off; but we
  // still want to apply 20us clamping to al timestamps to avoid leaking
  // nano-second precision.
  bool unconditionalClamping = false;
  if (!IsTimerPrecisionReductionEnabled(aType)) {
    if (!StaticPrefs::privacy_reduceTimerPrecision_unconditional()) {
      return aTime;
    }
    unconditionalClamping = true;
    aResolutionUSec = RFP_TIMER_UNCONDITIONAL_VALUE;  // 20 microseconds
    aContextMixin = 0;  // Just clarifies our logging statement at the end,
                        // otherwise unused
  }

  if (aResolutionUSec <= 0) {
    return aTime;
  }

  // Increase the time as needed until it is in microseconds.
  // Note that a double can hold up to 2**53 with integer precision. This gives
  // us only until June 5, 2255 in time-since-the-epoch with integer precision.
  // So we will be losing microseconds precision after that date.
  // We think this is okay, and we codify it in some tests.
  double timeScaled = aTime * (1000000 / aTimeScale);
  // Cut off anything less than a microsecond.
  long long timeAsInt = timeScaled;

  // If we have a blank context mixin, this indicates we (should) have an
  // absolute timestamp. We check the time, and if it less than a unix timestamp
  // about 10 years in the past, we output to the log and, in debug builds,
  // assert. This is an error case we want to understand and fix: we must have
  // given a relative timestamp with a mixin of 0 which is incorrect. Anyone
  // running a debug build _probably_ has an accurate clock, and if they don't,
  // they'll hopefully find this message and understand why things are crashing.
  const long long kFeb282008 = 1204233985000;
  if (!unconditionalClamping && aContextMixin == 0 &&
      aType == TimerPrecisionType::All && timeAsInt < kFeb282008) {
    MOZ_LOG(
        gResistFingerprintingLog, LogLevel::Error,
        ("About to assert. aTime=%lli<%lli aContextMixin=%" PRId64 " aType=%s",
         timeAsInt, kFeb282008, aContextMixin,
         (aType == TimerPrecisionType::RFPOnly ? "RFPOnly" : "All")));
    MOZ_ASSERT(
        false,
        "ReduceTimePrecisionImpl was given a relative time "
        "with an empty context mix-in (or your clock is 10+ years off.) "
        "Run this with MOZ_LOG=nsResistFingerprinting:1 to get more details.");
  }

  // Cast the resolution (in microseconds) to an int.
  long long resolutionAsInt = aResolutionUSec;
  // Perform the clamping.
  // We do a cast back to double to perform the division with doubles, then
  // floor the result and the rest occurs with integer precision. This is
  // because it gives consistency above and below zero. Above zero, performing
  // the division in integers truncates decimals, taking the result closer to
  // zero (a floor). Below zero, performing the division in integers truncates
  // decimals, taking the result closer to zero (a ceil). The impact of this is
  // that comparing two clamped values that should be related by a constant
  // (e.g. 10s) that are across the zero barrier will no longer work. We need to
  // round consistently towards positive infinity or negative infinity (we chose
  // negative.) This can't be done with a truncation, it must be done with
  // floor.
  long long clamped =
      floor(double(timeAsInt) / resolutionAsInt) * resolutionAsInt;

  long long midpoint = 0;
  long long clampedAndJittered = clamped;
  if (!unconditionalClamping &&
      StaticPrefs::privacy_resistFingerprinting_reduceTimerPrecision_jitter()) {
    if (!NS_FAILED(RandomMidpoint(clamped, resolutionAsInt, aContextMixin,
                                  &midpoint)) &&
        timeAsInt >= clamped + midpoint) {
      clampedAndJittered += resolutionAsInt;
    }
  }

  // Cast it back to a double and reduce it to the correct units.
  double ret = double(clampedAndJittered) / (1000000.0 / aTimeScale);

  MOZ_LOG(
      gResistFingerprintingLog, LogLevel::Verbose,
      ("Given: (%.*f, Scaled: %.*f, Converted: %lli), Rounding %s with (%lli, "
       "Originally %.*f), "
       "Intermediate: (%lli), Clamped: (%lli) Jitter: (%i Context: %" PRId64
       " Midpoint: %lli) "
       "Final: (%lli Converted: %.*f)",
       DBL_DIG - 1, aTime, DBL_DIG - 1, timeScaled, timeAsInt,
       (unconditionalClamping ? "unconditionally" : "normally"),
       resolutionAsInt, DBL_DIG - 1, aResolutionUSec,
       (long long)floor(double(timeAsInt) / resolutionAsInt), clamped,
       StaticPrefs::privacy_resistFingerprinting_reduceTimerPrecision_jitter(),
       aContextMixin, midpoint, clampedAndJittered, DBL_DIG - 1, ret));

  return ret;
}

/* static */
double nsRFPService::ReduceTimePrecisionAsUSecs(
    double aTime, int64_t aContextMixin,
    TimerPrecisionType aType /* = TimerPrecisionType::All */) {
  return nsRFPService::ReduceTimePrecisionImpl(
      aTime, MicroSeconds, TimerResolution(), aContextMixin, aType);
}

/* static */
double nsRFPService::ReduceTimePrecisionAsUSecsWrapper(double aTime) {
  return nsRFPService::ReduceTimePrecisionImpl(
      aTime, MicroSeconds, TimerResolution(),
      0, /* For absolute timestamps (all the JS engine does), supply zero
            context mixin */
      TimerPrecisionType::All);
}

/* static */
double nsRFPService::ReduceTimePrecisionAsMSecs(
    double aTime, int64_t aContextMixin,
    TimerPrecisionType aType /* = TimerPrecisionType::All */) {
  return nsRFPService::ReduceTimePrecisionImpl(
      aTime, MilliSeconds, TimerResolution(), aContextMixin, aType);
}

/* static */
double nsRFPService::ReduceTimePrecisionAsSecs(
    double aTime, int64_t aContextMixin,
    TimerPrecisionType aType /* = TimerPrecisionType::All */) {
  return nsRFPService::ReduceTimePrecisionImpl(
      aTime, Seconds, TimerResolution(), aContextMixin, aType);
}

/* static */
uint32_t nsRFPService::CalculateTargetVideoResolution(uint32_t aVideoQuality) {
  return aVideoQuality * NSToIntCeil(aVideoQuality * 16 / 9.0);
}

/* static */
uint32_t nsRFPService::GetSpoofedTotalFrames(double aTime) {
  double precision = TimerResolution() / 1000 / 1000;
  double time = floor(aTime / precision) * precision;

  return NSToIntFloor(time * kVideoFramesPerSec);
}

/* static */
uint32_t nsRFPService::GetSpoofedDroppedFrames(double aTime, uint32_t aWidth,
                                               uint32_t aHeight) {
  uint32_t targetRes = CalculateTargetVideoResolution(
      StaticPrefs::privacy_resistFingerprinting_target_video_res());

  // The video resolution is less than or equal to the target resolution, we
  // report a zero dropped rate for this case.
  if (targetRes >= aWidth * aHeight) {
    return 0;
  }

  double precision = TimerResolution() / 1000 / 1000;
  double time = floor(aTime / precision) * precision;
  // Bound the dropped ratio from 0 to 100.
  uint32_t boundedDroppedRatio = min(kVideoDroppedRatio, 100U);

  return NSToIntFloor(time * kVideoFramesPerSec *
                      (boundedDroppedRatio / 100.0));
}

/* static */
uint32_t nsRFPService::GetSpoofedPresentedFrames(double aTime, uint32_t aWidth,
                                                 uint32_t aHeight) {
  uint32_t targetRes = CalculateTargetVideoResolution(
      StaticPrefs::privacy_resistFingerprinting_target_video_res());

  // The target resolution is greater than the current resolution. For this
  // case, there will be no dropped frames, so we report total frames directly.
  if (targetRes >= aWidth * aHeight) {
    return GetSpoofedTotalFrames(aTime);
  }

  double precision = TimerResolution() / 1000 / 1000;
  double time = floor(aTime / precision) * precision;
  // Bound the dropped ratio from 0 to 100.
  uint32_t boundedDroppedRatio = min(kVideoDroppedRatio, 100U);

  return NSToIntFloor(time * kVideoFramesPerSec *
                      ((100 - boundedDroppedRatio) / 100.0));
}

static uint32_t GetSpoofedVersion() {
  // If we can't get the current Firefox version, use a hard-coded ESR version.
  const uint32_t kKnownEsrVersion = 60;

  nsresult rv;
  nsCOMPtr<nsIXULAppInfo> appInfo =
      do_GetService("@mozilla.org/xre/app-info;1", &rv);
  NS_ENSURE_SUCCESS(rv, kKnownEsrVersion);

  nsAutoCString appVersion;
  rv = appInfo->GetVersion(appVersion);
  NS_ENSURE_SUCCESS(rv, kKnownEsrVersion);

  // The browser version will be spoofed as the last ESR version.
  // By doing so, the anonymity group will cover more versions instead of one
  // version.
  uint32_t firefoxVersion = appVersion.ToInteger(&rv);
  NS_ENSURE_SUCCESS(rv, kKnownEsrVersion);

#ifdef DEBUG
  // If we are running in Firefox ESR, determine whether the formula of ESR
  // version has changed.  Once changed, we must update the formula in this
  // function.
  if (!strcmp(NS_STRINGIFY(MOZ_UPDATE_CHANNEL), "esr")) {
    MOZ_ASSERT(((firefoxVersion % 8) == 4),
               "Please update ESR version formula in nsRFPService.cpp");
  }
#endif  // DEBUG

  // Starting with Firefox 52, a new ESR version will be released every
  // eight Firefox versions: 52, 60, 68, ...
  // We infer the last and closest ESR version based on this rule.
  return firefoxVersion - ((firefoxVersion - 4) % 8);
}

/* static */
void nsRFPService::GetSpoofedUserAgent(nsACString& userAgent,
                                       bool isForHTTPHeader) {
  // This function generates the spoofed value of User Agent.
  // We spoof the values of the platform and Firefox version, which could be
  // used as fingerprinting sources to identify individuals.
  // Reference of the format of User Agent:
  // https://developer.mozilla.org/en-US/docs/Web/API/NavigatorID/userAgent
  // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/User-Agent

  uint32_t spoofedVersion = GetSpoofedVersion();
  const char* spoofedOS = isForHTTPHeader ? SPOOFED_HTTP_UA_OS : SPOOFED_UA_OS;
  userAgent.Assign(nsPrintfCString(
      "Mozilla/5.0 (%s; rv:%d.0) Gecko/%s Firefox/%d.0", spoofedOS,
      spoofedVersion, LEGACY_UA_GECKO_TRAIL, spoofedVersion));
}

static const char* gCallbackPrefs[] = {
    RESIST_FINGERPRINTING_PREF,   RFP_TIMER_PREF,
    RFP_TIMER_UNCONDITIONAL_PREF, RFP_TIMER_VALUE_PREF,
    RFP_JITTER_VALUE_PREF,        nullptr,
};

nsresult nsRFPService::Init() {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  NS_ENSURE_TRUE(obs, NS_ERROR_NOT_AVAILABLE);

  rv = obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  NS_ENSURE_SUCCESS(rv, rv);

#if defined(XP_WIN)
  rv = obs->AddObserver(this, PROFILE_INITIALIZED_TOPIC, false);
  NS_ENSURE_SUCCESS(rv, rv);
#endif

  Preferences::RegisterCallbacks(PREF_CHANGE_METHOD(nsRFPService::PrefChanged),
                                 gCallbackPrefs, this);

  // We backup the original TZ value here.
  const char* tzValue = PR_GetEnv("TZ");
  if (tzValue != nullptr) {
    mInitialTZValue = nsCString(tzValue);
  }

  // Call Update here to cache the values of the prefs and set the timezone.
  UpdateRFPPref();

  // Create the LRU Cache when we initialize, to avoid accidently trying to
  // create it (and call ClearOnShutdown) on a non-main-thread
  if (sCache == nullptr) {
    sCache = new LRUCache();
  }

  return rv;
}

// This function updates only timing-related fingerprinting items
void nsRFPService::UpdateTimers() {
  MOZ_ASSERT(NS_IsMainThread());

  if (StaticPrefs::privacy_resistFingerprinting() ||
      StaticPrefs::privacy_reduceTimerPrecision()) {
    JS::SetTimeResolutionUsec(
        TimerResolution(),
        StaticPrefs::
            privacy_resistFingerprinting_reduceTimerPrecision_jitter());
    JS::SetReduceMicrosecondTimePrecisionCallback(
        nsRFPService::ReduceTimePrecisionAsUSecsWrapper);
  } else if (StaticPrefs::privacy_reduceTimerPrecision_unconditional()) {
    JS::SetTimeResolutionUsec(RFP_TIMER_UNCONDITIONAL_VALUE, false);
    JS::SetReduceMicrosecondTimePrecisionCallback(
        nsRFPService::ReduceTimePrecisionAsUSecsWrapper);
  } else if (sInitialized) {
    JS::SetTimeResolutionUsec(0, false);
  }
}

// This function updates every fingerprinting item necessary except
// timing-related
void nsRFPService::UpdateRFPPref() {
  MOZ_ASSERT(NS_IsMainThread());

  UpdateTimers();

  bool privacyResistFingerprinting =
      StaticPrefs::privacy_resistFingerprinting();
  if (privacyResistFingerprinting) {
    PR_SetEnv("TZ=UTC");
  } else if (sInitialized) {
    // We will not touch the TZ value if 'privacy.resistFingerprinting' is false
    // during the time of initialization.
    if (!mInitialTZValue.IsEmpty()) {
      nsAutoCString tzValue = NS_LITERAL_CSTRING("TZ=") + mInitialTZValue;
      static char* tz = nullptr;

      // If the tz has been set before, we free it first since it will be
      // allocated a new value later.
      if (tz != nullptr) {
        free(tz);
      }
      // PR_SetEnv() needs the input string been leaked intentionally, so
      // we copy it here.
      tz = ToNewCString(tzValue);
      if (tz != nullptr) {
        PR_SetEnv(tz);
      }
    } else {
#if defined(XP_WIN)
      // For Windows, we reset the TZ to an empty string. This will make Windows
      // to use its system timezone.
      PR_SetEnv("TZ=");
#else
      // For POSIX like system, we reset the TZ to the /etc/localtime, which is
      // the system timezone.
      PR_SetEnv("TZ=:/etc/localtime");
#endif
    }
  }

  // If and only if the time zone was changed above, propagate the change to the
  // <time.h> functions and the JS runtime.
  if (privacyResistFingerprinting || sInitialized) {
    // localtime_r (and other functions) may not call tzset, so do this here
    // after changing TZ to ensure all <time.h> functions use the new time zone.
#if defined(XP_WIN)
    _tzset();
#else
    tzset();
#endif

    nsJSUtils::ResetTimeZone();
  }
}

void nsRFPService::StartShutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();

  StaticMutexAutoLock lock(sLock);
  { sCache = nullptr; }

  if (obs) {
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
  }
  Preferences::UnregisterCallbacks(
      PREF_CHANGE_METHOD(nsRFPService::PrefChanged), gCallbackPrefs, this);
}

/* static */
void nsRFPService::MaybeCreateSpoofingKeyCodes(const KeyboardLangs aLang,
                                               const KeyboardRegions aRegion) {
  if (sSpoofingKeyboardCodes == nullptr) {
    sSpoofingKeyboardCodes =
        new nsDataHashtable<KeyboardHashKey, const SpoofingKeyboardCode*>();
  }

  if (KeyboardLang::EN == aLang) {
    switch (aRegion) {
      case KeyboardRegion::US:
        MaybeCreateSpoofingKeyCodesForEnUS();
        break;
    }
  }
}

/* static */
void nsRFPService::MaybeCreateSpoofingKeyCodesForEnUS() {
  MOZ_ASSERT(sSpoofingKeyboardCodes);

  static bool sInitialized = false;
  const KeyboardLangs lang = KeyboardLang::EN;
  const KeyboardRegions reg = KeyboardRegion::US;

  if (sInitialized) {
    return;
  }

  static const SpoofingKeyboardInfo spoofingKeyboardInfoTable[] = {
#define KEY(key_, _codeNameIdx, _keyCode, _modifier) \
  {KEY_NAME_INDEX_USE_STRING,                        \
   NS_LITERAL_STRING(key_),                          \
   {CODE_NAME_INDEX_##_codeNameIdx, _keyCode, _modifier}},
#define CONTROL(keyNameIdx_, _codeNameIdx, _keyCode) \
  {KEY_NAME_INDEX_##keyNameIdx_,                     \
   EmptyString(),                                    \
   {CODE_NAME_INDEX_##_codeNameIdx, _keyCode, MODIFIER_NONE}},
#include "KeyCodeConsensus_En_US.h"
#undef CONTROL
#undef KEY
  };

  for (const auto& keyboardInfo : spoofingKeyboardInfoTable) {
    KeyboardHashKey key(lang, reg, keyboardInfo.mKeyIdx, keyboardInfo.mKey);
    MOZ_ASSERT(!sSpoofingKeyboardCodes->Lookup(key),
               "Double-defining key code; fix your KeyCodeConsensus file");
    sSpoofingKeyboardCodes->Put(key, &keyboardInfo.mSpoofingCode);
  }

  sInitialized = true;
}

/* static */
void nsRFPService::GetKeyboardLangAndRegion(const nsAString& aLanguage,
                                            KeyboardLangs& aLocale,
                                            KeyboardRegions& aRegion) {
  nsAutoString langStr;
  nsAutoString regionStr;
  uint32_t partNum = 0;

  for (const nsAString& part : aLanguage.Split('-')) {
    if (partNum == 0) {
      langStr = part;
    } else {
      regionStr = part;
      break;
    }

    partNum++;
  }

  // We test each language here as well as the region. There are some cases that
  // only the language is given, we will use the default region code when this
  // happens. The default region should depend on the given language.
  if (langStr.EqualsLiteral(RFP_KEYBOARD_LANG_STRING_EN)) {
    aLocale = KeyboardLang::EN;
    // Give default values first.
    aRegion = KeyboardRegion::US;

    if (regionStr.EqualsLiteral(RFP_KEYBOARD_REGION_STRING_US)) {
      aRegion = KeyboardRegion::US;
    }
  } else {
    // There is no spoofed keyboard locale for the given language. We use the
    // default one in this case.
    aLocale = RFP_DEFAULT_SPOOFING_KEYBOARD_LANG;
    aRegion = RFP_DEFAULT_SPOOFING_KEYBOARD_REGION;
  }
}

/* static */
bool nsRFPService::GetSpoofedKeyCodeInfo(
    const dom::Document* aDoc, const WidgetKeyboardEvent* aKeyboardEvent,
    SpoofingKeyboardCode& aOut) {
  MOZ_ASSERT(aKeyboardEvent);

  KeyboardLangs keyboardLang = RFP_DEFAULT_SPOOFING_KEYBOARD_LANG;
  KeyboardRegions keyboardRegion = RFP_DEFAULT_SPOOFING_KEYBOARD_REGION;
  // If the document is given, we use the content language which is get from the
  // document. Otherwise, we use the default one.
  if (aDoc != nullptr) {
    nsAutoString language;
    aDoc->GetContentLanguage(language);

    // If the content-langauge is not given, we try to get langauge from the
    // HTML lang attribute.
    if (language.IsEmpty()) {
      dom::Element* elm = aDoc->GetHtmlElement();

      if (elm != nullptr) {
        elm->GetLang(language);
      }
    }

    // If two or more languages are given, per HTML5 spec, we should consider
    // it as 'unknown'. So we use the default one.
    if (!language.IsEmpty() && !language.Contains(char16_t(','))) {
      language.StripWhitespace();
      GetKeyboardLangAndRegion(language, keyboardLang, keyboardRegion);
    }
  }

  MaybeCreateSpoofingKeyCodes(keyboardLang, keyboardRegion);

  KeyNameIndex keyIdx = aKeyboardEvent->mKeyNameIndex;
  nsAutoString keyName;

  if (keyIdx == KEY_NAME_INDEX_USE_STRING) {
    keyName = aKeyboardEvent->mKeyValue;
  }

  KeyboardHashKey key(keyboardLang, keyboardRegion, keyIdx, keyName);
  const SpoofingKeyboardCode* keyboardCode = sSpoofingKeyboardCodes->Get(key);

  if (keyboardCode != nullptr) {
    aOut = *keyboardCode;
    return true;
  }

  return false;
}

/* static */
bool nsRFPService::GetSpoofedModifierStates(
    const dom::Document* aDoc, const WidgetKeyboardEvent* aKeyboardEvent,
    const Modifiers aModifier, bool& aOut) {
  MOZ_ASSERT(aKeyboardEvent);

  // For modifier or control keys, we don't need to hide its modifier states.
  if (aKeyboardEvent->mKeyNameIndex != KEY_NAME_INDEX_USE_STRING) {
    return false;
  }

  // We will spoof the modifer state for Alt, Shift, and AltGraph.
  // We don't spoof the Control key, because it is often used
  // for command key combinations in web apps.
  if ((aModifier & (MODIFIER_ALT | MODIFIER_SHIFT | MODIFIER_ALTGRAPH)) != 0) {
    SpoofingKeyboardCode keyCodeInfo;

    if (GetSpoofedKeyCodeInfo(aDoc, aKeyboardEvent, keyCodeInfo)) {
      aOut = ((keyCodeInfo.mModifierStates & aModifier) != 0);
      return true;
    }
  }

  return false;
}

/* static */
bool nsRFPService::GetSpoofedCode(const dom::Document* aDoc,
                                  const WidgetKeyboardEvent* aKeyboardEvent,
                                  nsAString& aOut) {
  MOZ_ASSERT(aKeyboardEvent);

  SpoofingKeyboardCode keyCodeInfo;

  if (!GetSpoofedKeyCodeInfo(aDoc, aKeyboardEvent, keyCodeInfo)) {
    return false;
  }

  WidgetKeyboardEvent::GetDOMCodeName(keyCodeInfo.mCode, aOut);

  // We need to change the 'Left' with 'Right' if the location indicates
  // it's a right key.
  if (aKeyboardEvent->mLocation ==
          dom::KeyboardEvent_Binding::DOM_KEY_LOCATION_RIGHT &&
      StringEndsWith(aOut, NS_LITERAL_STRING("Left"))) {
    aOut.ReplaceLiteral(aOut.Length() - 4, 4, u"Right");
  }

  return true;
}

/* static */
bool nsRFPService::GetSpoofedKeyCode(const dom::Document* aDoc,
                                     const WidgetKeyboardEvent* aKeyboardEvent,
                                     uint32_t& aOut) {
  MOZ_ASSERT(aKeyboardEvent);

  SpoofingKeyboardCode keyCodeInfo;

  if (GetSpoofedKeyCodeInfo(aDoc, aKeyboardEvent, keyCodeInfo)) {
    aOut = keyCodeInfo.mKeyCode;
    return true;
  }

  return false;
}

void nsRFPService::PrefChanged(const char* aPref) {
  nsDependentCString pref(aPref);

  if (pref.EqualsLiteral(RFP_TIMER_PREF) ||
      pref.EqualsLiteral(RFP_TIMER_UNCONDITIONAL_PREF) ||
      pref.EqualsLiteral(RFP_TIMER_VALUE_PREF) ||
      pref.EqualsLiteral(RFP_JITTER_VALUE_PREF)) {
    UpdateTimers();
  } else if (pref.EqualsLiteral(RESIST_FINGERPRINTING_PREF)) {
    UpdateRFPPref();

#if defined(XP_WIN)
    if (!XRE_IsE10sParentProcess()) {
      // Windows does not follow POSIX. Updates to the TZ environment variable
      // are not reflected immediately on that platform as they are on UNIX
      // systems without this call.
      _tzset();
    }
#endif
  }
}

NS_IMETHODIMP
nsRFPService::Observe(nsISupports* aObject, const char* aTopic,
                      const char16_t* aMessage) {
  if (strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, aTopic) == 0) {
    StartShutdown();
  }
#if defined(XP_WIN)
  else if (!strcmp(PROFILE_INITIALIZED_TOPIC, aTopic)) {
    // If we're e10s, then we don't need to run this, since the child process
    // will simply inherit the environment variable from the parent process, in
    // which case it's unnecessary to call _tzset().
    if (XRE_IsParentProcess() && !XRE_IsE10sParentProcess()) {
      // Windows does not follow POSIX. Updates to the TZ environment variable
      // are not reflected immediately on that platform as they are on UNIX
      // systems without this call.
      _tzset();
    }

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    NS_ENSURE_TRUE(obs, NS_ERROR_NOT_AVAILABLE);

    nsresult rv = obs->RemoveObserver(this, PROFILE_INITIALIZED_TOPIC);
    NS_ENSURE_SUCCESS(rv, rv);
  }
#endif

  return NS_OK;
}
