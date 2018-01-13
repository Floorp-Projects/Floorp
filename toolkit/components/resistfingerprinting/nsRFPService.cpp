/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRFPService.h"

#include <algorithm>
#include <time.h>

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"

#include "nsCOMPtr.h"
#include "nsCoord.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsXULAppAPI.h"
#include "nsPrintfCString.h"

#include "nsIObserverService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIXULAppInfo.h"
#include "nsIXULRuntime.h"
#include "nsJSUtils.h"

#include "prenv.h"

#include "js/Date.h"

using namespace mozilla;
using namespace std;

#define RESIST_FINGERPRINTING_PREF "privacy.resistFingerprinting"
#define RFP_TIMER_PREF "privacy.reduceTimerPrecision"
#define RFP_TIMER_VALUE_PREF "privacy.resistFingerprinting.reduceTimerPrecision.microseconds"
#define RFP_TIMER_VALUE_DEFAULT 20
#define RFP_SPOOFED_FRAMES_PER_SEC_PREF "privacy.resistFingerprinting.video_frames_per_sec"
#define RFP_SPOOFED_DROPPED_RATIO_PREF  "privacy.resistFingerprinting.video_dropped_ratio"
#define RFP_TARGET_VIDEO_RES_PREF "privacy.resistFingerprinting.target_video_res"
#define RFP_SPOOFED_FRAMES_PER_SEC_DEFAULT 30
#define RFP_SPOOFED_DROPPED_RATIO_DEFAULT  5
#define RFP_TARGET_VIDEO_RES_DEFAULT 480
#define PROFILE_INITIALIZED_TOPIC "profile-initial-state"

NS_IMPL_ISUPPORTS(nsRFPService, nsIObserver)

static StaticRefPtr<nsRFPService> sRFPService;
static bool sInitialized = false;
Atomic<bool, ReleaseAcquire> nsRFPService::sPrivacyResistFingerprinting;
Atomic<bool, ReleaseAcquire> nsRFPService::sPrivacyTimerPrecisionReduction;
Atomic<uint32_t, ReleaseAcquire> sResolutionUSec;
static uint32_t sVideoFramesPerSec;
static uint32_t sVideoDroppedRatio;
static uint32_t sTargetVideoRes;

/* static */
nsRFPService*
nsRFPService::GetOrCreate()
{
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
bool
nsRFPService::IsResistFingerprintingEnabled()
{
  return sPrivacyResistFingerprinting;
}

/* static */
bool
nsRFPService::IsTimerPrecisionReductionEnabled()
{
  return (sPrivacyTimerPrecisionReduction || IsResistFingerprintingEnabled()) &&
         sResolutionUSec != 0;
}

/* static */
double
nsRFPService::ReduceTimePrecisionAsMSecs(double aTime)
{
  if (!IsTimerPrecisionReductionEnabled()) {
    return aTime;
  }
  const double resolutionMSec = sResolutionUSec / 1000.0;
  return floor(aTime / resolutionMSec) * resolutionMSec;
}

/* static */
double
nsRFPService::ReduceTimePrecisionAsUSecs(double aTime)
{
  if (!IsTimerPrecisionReductionEnabled()) {
    return aTime;
  }
  return floor(aTime / sResolutionUSec) * sResolutionUSec;
}

/* static */
uint32_t
nsRFPService::CalculateTargetVideoResolution(uint32_t aVideoQuality)
{
  return aVideoQuality * NSToIntCeil(aVideoQuality * 16 / 9.0);
}

/* static */
double
nsRFPService::ReduceTimePrecisionAsSecs(double aTime)
{
  if (!IsTimerPrecisionReductionEnabled()) {
    return aTime;
  }
  if (sResolutionUSec < 1000000) {
    // The resolution is smaller than one sec.  Use the reciprocal to avoid
    // floating point error.
    const double resolutionSecReciprocal = 1000000.0 / sResolutionUSec;
    return floor(aTime * resolutionSecReciprocal) / resolutionSecReciprocal;
  }
  const double resolutionSec = sResolutionUSec / 1000000.0;
  return floor(aTime / resolutionSec) * resolutionSec;
}

/* static */
uint32_t
nsRFPService::GetSpoofedTotalFrames(double aTime)
{
  double time = ReduceTimePrecisionAsSecs(aTime);

  return NSToIntFloor(time * sVideoFramesPerSec);
}

/* static */
uint32_t
nsRFPService::GetSpoofedDroppedFrames(double aTime, uint32_t aWidth, uint32_t aHeight)
{
  uint32_t targetRes = CalculateTargetVideoResolution(sTargetVideoRes);

  // The video resolution is less than or equal to the target resolution, we
  // report a zero dropped rate for this case.
  if (targetRes >= aWidth * aHeight) {
    return 0;
  }

  double time = ReduceTimePrecisionAsSecs(aTime);
  // Bound the dropped ratio from 0 to 100.
  uint32_t boundedDroppedRatio = min(sVideoDroppedRatio, 100u);

  return NSToIntFloor(time * sVideoFramesPerSec * (boundedDroppedRatio / 100.0));
}

/* static */
uint32_t
nsRFPService::GetSpoofedPresentedFrames(double aTime, uint32_t aWidth, uint32_t aHeight)
{
  uint32_t targetRes = CalculateTargetVideoResolution(sTargetVideoRes);

  // The target resolution is greater than the current resolution. For this case,
  // there will be no dropped frames, so we report total frames directly.
  if (targetRes >= aWidth * aHeight) {
    return GetSpoofedTotalFrames(aTime);
  }

  double time = ReduceTimePrecisionAsSecs(aTime);
  // Bound the dropped ratio from 0 to 100.
  uint32_t boundedDroppedRatio = min(sVideoDroppedRatio, 100u);

  return NSToIntFloor(time * sVideoFramesPerSec * ((100 - boundedDroppedRatio) / 100.0));
}

/* static */
nsresult
nsRFPService::GetSpoofedUserAgent(nsACString &userAgent)
{
  // This function generates the spoofed value of User Agent.
  // We spoof the values of the platform and Firefox version, which could be
  // used as fingerprinting sources to identify individuals.
  // Reference of the format of User Agent:
  // https://developer.mozilla.org/en-US/docs/Web/API/NavigatorID/userAgent
  // https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/User-Agent

  nsresult rv;
  nsCOMPtr<nsIXULAppInfo> appInfo =
    do_GetService("@mozilla.org/xre/app-info;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString appVersion;
  rv = appInfo->GetVersion(appVersion);
  NS_ENSURE_SUCCESS(rv, rv);

  // The browser version will be spoofed as the last ESR version.
  // By doing so, the anonymity group will cover more versions instead of one
  // version.
  uint32_t firefoxVersion = appVersion.ToInteger(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Starting from Firefox 10, Firefox ESR was released once every seven
  // Firefox releases, e.g. Firefox 10, 17, 24, 31, and so on.
  // We infer the last and closest ESR version based on this rule.
  nsCOMPtr<nsIXULRuntime> runtime =
    do_GetService("@mozilla.org/xre/runtime;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString updateChannel;
  rv = runtime->GetDefaultUpdateChannel(updateChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we are running in Firefox ESR, determine whether the formula of ESR
  // version has changed.  Once changed, we must update the formula in this
  // function.
  if (updateChannel.EqualsLiteral("esr")) {
    MOZ_ASSERT(((firefoxVersion % 7) == 3),
      "Please udpate ESR version formula in nsRFPService.cpp");
  }

  uint32_t spoofedVersion = firefoxVersion - ((firefoxVersion - 3) % 7);
  userAgent.Assign(nsPrintfCString(
    "Mozilla/5.0 (%s; rv:%d.0) Gecko/%s Firefox/%d.0",
    SPOOFED_UA_OS, spoofedVersion, LEGACY_BUILD_ID, spoofedVersion));

  return rv;
}

nsresult
nsRFPService::Init()
{
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

  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  NS_ENSURE_TRUE(prefs, NS_ERROR_NOT_AVAILABLE);

  rv = prefs->AddObserver(RESIST_FINGERPRINTING_PREF, this, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = prefs->AddObserver(RFP_TIMER_PREF, this, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = prefs->AddObserver(RFP_TIMER_VALUE_PREF, this, false);
  NS_ENSURE_SUCCESS(rv, rv);

  Preferences::AddAtomicBoolVarCache(&sPrivacyTimerPrecisionReduction,
                                     RFP_TIMER_PREF,
                                     true);

  Preferences::AddAtomicUintVarCache(&sResolutionUSec,
                                     RFP_TIMER_VALUE_PREF,
                                     RFP_TIMER_VALUE_DEFAULT);
  Preferences::AddUintVarCache(&sVideoFramesPerSec,
                               RFP_SPOOFED_FRAMES_PER_SEC_PREF,
                               RFP_SPOOFED_FRAMES_PER_SEC_DEFAULT);
  Preferences::AddUintVarCache(&sVideoDroppedRatio,
                               RFP_SPOOFED_DROPPED_RATIO_PREF,
                               RFP_SPOOFED_DROPPED_RATIO_DEFAULT);
  Preferences::AddUintVarCache(&sTargetVideoRes,
                               RFP_TARGET_VIDEO_RES_PREF,
                               RFP_TARGET_VIDEO_RES_DEFAULT);

  // We backup the original TZ value here.
  const char* tzValue = PR_GetEnv("TZ");
  if (tzValue) {
    mInitialTZValue = nsCString(tzValue);
  }

  // Call Update here to cache the values of the prefs and set the timezone.
  UpdateRFPPref();

  return rv;
}

// This function updates only timing-related fingerprinting items
void
nsRFPService::UpdateTimers() {
  MOZ_ASSERT(NS_IsMainThread());

  if (sPrivacyResistFingerprinting || sPrivacyTimerPrecisionReduction) {
    JS::SetTimeResolutionUsec(sResolutionUSec);
  } else if (sInitialized) {
    JS::SetTimeResolutionUsec(0);
  }
}


// This function updates every fingerprinting item necessary except timing-related
void
nsRFPService::UpdateRFPPref()
{
  MOZ_ASSERT(NS_IsMainThread());
  sPrivacyResistFingerprinting = Preferences::GetBool(RESIST_FINGERPRINTING_PREF);

  UpdateTimers();

  if (sPrivacyResistFingerprinting) {
    PR_SetEnv("TZ=UTC");
  } else if (sInitialized) {
    // We will not touch the TZ value if 'privacy.resistFingerprinting' is false during
    // the time of initialization.
    if (!mInitialTZValue.IsEmpty()) {
      nsAutoCString tzValue = NS_LITERAL_CSTRING("TZ=") + mInitialTZValue;
      static char* tz = nullptr;

      // If the tz has been set before, we free it first since it will be allocated
      // a new value later.
      if (tz) {
        free(tz);
      }
      // PR_SetEnv() needs the input string been leaked intentionally, so
      // we copy it here.
      tz = ToNewCString(tzValue);
      if (tz) {
        PR_SetEnv(tz);
      }
    } else {
#if defined(XP_WIN)
      // For Windows, we reset the TZ to an empty string. This will make Windows to use
      // its system timezone.
      PR_SetEnv("TZ=");
#else
      // For POSIX like system, we reset the TZ to the /etc/localtime, which is the
      // system timezone.
      PR_SetEnv("TZ=:/etc/localtime");
#endif
    }
  }

  nsJSUtils::ResetTimeZone();
}

void
nsRFPService::StartShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();

  if (obs) {
    obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);

    nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);

    if (prefs) {
      prefs->RemoveObserver(RESIST_FINGERPRINTING_PREF, this);
      prefs->RemoveObserver(RFP_TIMER_PREF, this);
      prefs->RemoveObserver(RFP_TIMER_VALUE_PREF, this);
    }
  }
}

NS_IMETHODIMP
nsRFPService::Observe(nsISupports* aObject, const char* aTopic,
                      const char16_t* aMessage)
{
  if (!strcmp(NS_PREFBRANCH_PREFCHANGE_TOPIC_ID, aTopic)) {
    NS_ConvertUTF16toUTF8 pref(aMessage);

    if (pref.EqualsLiteral(RFP_TIMER_PREF) || pref.EqualsLiteral(RFP_TIMER_VALUE_PREF)) {
      UpdateTimers();
    }
    else if (pref.EqualsLiteral(RESIST_FINGERPRINTING_PREF)) {
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

  if (!strcmp(NS_XPCOM_SHUTDOWN_OBSERVER_ID, aTopic)) {
    StartShutdown();
  }
#if defined(XP_WIN)
  else if (!strcmp(PROFILE_INITIALIZED_TOPIC, aTopic)) {
    // If we're e10s, then we don't need to run this, since the child process will
    // simply inherit the environment variable from the parent process, in which
    // case it's unnecessary to call _tzset().
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
