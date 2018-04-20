/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCocoaFeatures_h_
#define nsCocoaFeatures_h_

#include <stdint.h>

/// Note that this class assumes we support the platform we are running on.
/// For better or worse, if the version is unknown or less than what we
/// support, we set it to the minimum supported version.  GetSystemVersion
/// is the only call that returns the unadjusted values.
class nsCocoaFeatures {
public:
  static int32_t OSXVersion();
  static int32_t OSXVersionMajor();
  static int32_t OSXVersionMinor();
  static int32_t OSXVersionBugFix();
  static bool OnYosemiteOrLater();
  static bool OnElCapitanOrLater();
  static bool OnSierraOrLater();
  static bool OnHighSierraOrLater();
  static bool OnSierraExactly();

  static bool IsAtLeastVersion(int32_t aMajor, int32_t aMinor, int32_t aBugFix=0);

  // These are utilities that do not change or depend on the value of mOSXVersion
  // and instead just encapsulate the encoding algorithm.  Note that GetVersion
  // actually adjusts to the lowest supported OS, so it will always return
  // a "supported" version.  GetSystemVersion does not make any modifications.
  static void GetSystemVersion(int &aMajor, int &aMinor, int &aBugFix);
  static int32_t GetVersion(int32_t aMajor, int32_t aMinor, int32_t aBugFix);
  static int32_t ExtractMajorVersion(int32_t aVersion);
  static int32_t ExtractMinorVersion(int32_t aVersion);
  static int32_t ExtractBugFixVersion(int32_t aVersion);

private:
  static void InitializeVersionNumbers();

  static int32_t mOSXVersion;
};

// C-callable helper for cairo-quartz-font.c and SkFontHost_mac.cpp
extern "C" {
    bool Gecko_OnSierraExactly();
    bool Gecko_OnHighSierraOrLater();
}

#endif // nsCocoaFeatures_h_
