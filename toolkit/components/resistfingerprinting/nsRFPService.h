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
#define SPOOFED_APPNAME    "Netscape"
#define SPOOFED_APPVERSION "5.0 (Windows)"
#define SPOOFED_OSCPU      "Windows NT 6.1"
#define SPOOFED_PLATFORM   "Win64"

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

private:
  nsresult Init();

  nsRFPService() {}

  ~nsRFPService() {}

  void UpdatePref();
  void StartShutdown();

  static Atomic<bool, ReleaseAcquire> sPrivacyResistFingerprinting;

  nsCString mInitialTZValue;
};

} // mozilla namespace

#endif /* __nsRFPService_h__ */
