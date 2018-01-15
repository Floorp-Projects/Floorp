/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsRFPService_h__
#define __nsRFPService_h__

#include "mozilla/Atomics.h"
#include "nsIObserver.h"

#include "nsString.h"

// Defines regarding spoofed values of Navigator object. These spoofed values
// are returned when 'privacy.resistFingerprinting' is true.
// We decided to give different spoofed values according to the platform. The
// reason is that it is easy to detect the real platform. So there is no benefit
// for hiding the platform: it only brings breakages, like keyboard shortcuts won't
// work in MAC OS if we spoof it as a window platform.
#ifdef XP_WIN
#define SPOOFED_UA_OS      "Windows NT 6.1; Win64; x64"
#define SPOOFED_APPVERSION "5.0 (Windows)"
#define SPOOFED_OSCPU      "Windows NT 6.1; Win64; x64"
#define SPOOFED_PLATFORM   "Win64"
#elif defined(XP_MACOSX)
#define SPOOFED_UA_OS      "Macintosh; Intel Mac OS X 10.13"
#define SPOOFED_APPVERSION "5.0 (Macintosh)"
#define SPOOFED_OSCPU      "Intel Mac OS X 10.13"
#define SPOOFED_PLATFORM   "MacIntel"
#elif defined(MOZ_WIDGET_ANDROID)
#define SPOOFED_UA_OS      "Android 6.0; Mobile"
#define SPOOFED_APPVERSION "5.0 (Android 6.0)"
#define SPOOFED_OSCPU      "Linux armv7l"
#define SPOOFED_PLATFORM   "Linux armv7l"
#else
// For Linux and other platforms, like BSDs, SunOS and etc, we will use Linux
// platform.
#define SPOOFED_UA_OS      "X11; Linux x86_64"
#define SPOOFED_APPVERSION "5.0 (X11)"
#define SPOOFED_OSCPU      "Linux x86_64"
#define SPOOFED_PLATFORM   "Linux x86_64"
#endif

#define SPOOFED_APPNAME    "Netscape"
#define LEGACY_BUILD_ID    "20100101"

namespace mozilla {

class nsRFPService final : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  static nsRFPService* GetOrCreate();
  static bool IsResistFingerprintingEnabled()
  {
    return sPrivacyResistFingerprinting;
  }
  static bool IsTimerPrecisionReductionEnabled()
  {
    return sPrivacyTimerPrecisionReduction || IsResistFingerprintingEnabled();
  }

  // The following Reduce methods can be called off main thread.
  static double ReduceTimePrecisionAsMSecs(double aTime);
  static double ReduceTimePrecisionAsUSecs(double aTime);
  static double ReduceTimePrecisionAsSecs(double aTime);

  // This method calculates the video resolution (i.e. height x width) based
  // on the video quality (480p, 720p, etc).
  static uint32_t CalculateTargetVideoResolution(uint32_t aVideoQuality);

  // Methods for getting spoofed media statistics and the return value will
  // depend on the video resolution.
  static uint32_t GetSpoofedTotalFrames(double aTime);
  static uint32_t GetSpoofedDroppedFrames(double aTime, uint32_t aWidth, uint32_t aHeight);
  static uint32_t GetSpoofedPresentedFrames(double aTime, uint32_t aWidth, uint32_t aHeight);

  // This method generates the spoofed value of User Agent.
  static nsresult GetSpoofedUserAgent(nsACString &userAgent);

private:
  nsresult Init();

  nsRFPService() {}

  ~nsRFPService() {}

  void UpdateTimers();
  void UpdateRFPPref();
  void StartShutdown();

  static Atomic<bool, ReleaseAcquire> sPrivacyResistFingerprinting;
  static Atomic<bool, ReleaseAcquire> sPrivacyTimerPrecisionReduction;

  nsCString mInitialTZValue;
};

} // mozilla namespace

#endif /* __nsRFPService_h__ */
