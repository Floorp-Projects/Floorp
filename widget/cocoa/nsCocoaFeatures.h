/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCocoaFeatures_h_
#define nsCocoaFeatures_h_

#include <stdint.h>

/// Note that this class assumes we support the platform we are running on.
/// For better or worse, if the version is unknown or less than what we
/// support, we set it to the minimum supported version. GetSystemVersion
/// is the only call that returns the unadjusted values.
class nsCocoaFeatures {
 public:
  static int32_t macOSVersion();
  static int32_t macOSVersionMajor();
  static int32_t macOSVersionMinor();
  static int32_t macOSVersionBugFix();
  static bool OnBigSurOrLater();
  static bool OnMontereyOrLater();
  static bool OnVenturaOrLater();

  static bool IsAtLeastVersion(int32_t aMajor, int32_t aMinor,
                               int32_t aBugFix = 0);

  static bool ProcessIsRosettaTranslated();

  // These are utilities that do not change or depend on the value of
  // mOSVersion and instead just encapsulate the encoding algorithm. Note that
  // GetVersion actually adjusts to the lowest supported OS, so it will always
  // return a "supported" version. GetSystemVersion does not make any
  // modifications.
  static void GetSystemVersion(int& aMajor, int& aMinor, int& aBugFix);
  static int32_t GetVersion(int32_t aMajor, int32_t aMinor, int32_t aBugFix);
  static int32_t ExtractMajorVersion(int32_t aVersion);
  static int32_t ExtractMinorVersion(int32_t aVersion);
  static int32_t ExtractBugFixVersion(int32_t aVersion);

 private:
  nsCocoaFeatures() = delete;  // Prevent instantiation.
  static void InitializeVersionNumbers();

  static int32_t mOSVersion;
};

#endif  // nsCocoaFeatures_h_
