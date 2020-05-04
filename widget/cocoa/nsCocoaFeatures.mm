/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file makes some assumptions about the versions of macOS.
// We are assuming that the major, minor and bugfix versions are each less than
// 256.
// There are MOZ_ASSERTs for that.

// The formula for the version integer is (major << 16) + (minor << 8) + bugfix.

#define MACOS_VERSION_MASK 0x00FFFFFF
#define MACOS_MAJOR_VERSION_MASK 0x00FFFFFF
#define MACOS_MINOR_VERSION_MASK 0x00FFFFFF
#define MACOS_BUGFIX_VERSION_MASK 0x00FFFFFF
#define MACOS_VERSION_10_0_HEX 0x000A0000
#define MACOS_VERSION_10_9_HEX 0x000A0900
#define MACOS_VERSION_10_10_HEX 0x000A0A00
#define MACOS_VERSION_10_11_HEX 0x000A0B00
#define MACOS_VERSION_10_12_HEX 0x000A0C00
#define MACOS_VERSION_10_13_HEX 0x000A0D00
#define MACOS_VERSION_10_14_HEX 0x000A0E00
#define MACOS_VERSION_10_15_HEX 0x000A0F00
//#define MACOS_VERSION_10_16_HEX 0x000A1000
//#define MACOS_VERSION_11_0_HEX 0x000B0000

#include "nsCocoaFeatures.h"
#include "nsCocoaUtils.h"
#include "nsDebug.h"
#include "nsObjCExceptions.h"

#import <Cocoa/Cocoa.h>

/*static*/ int32_t nsCocoaFeatures::mOSVersion = 0;

// This should not be called with unchecked aMajor, which should be >= 10.
inline int32_t AssembleVersion(int32_t aMajor, int32_t aMinor, int32_t aBugFix) {
  MOZ_ASSERT(aMajor >= 10);
  return (aMajor << 16) + (aMinor << 8) + aBugFix;
}

int32_t nsCocoaFeatures::ExtractMajorVersion(int32_t aVersion) {
  MOZ_ASSERT((aVersion & MACOS_VERSION_MASK) == aVersion);
  return (aVersion & 0xFF0000) >> 16;
}

int32_t nsCocoaFeatures::ExtractMinorVersion(int32_t aVersion) {
  MOZ_ASSERT((aVersion & MACOS_VERSION_MASK) == aVersion);
  return (aVersion & 0xFF00) >> 8;
}

int32_t nsCocoaFeatures::ExtractBugFixVersion(int32_t aVersion) {
  MOZ_ASSERT((aVersion & MACOS_VERSION_MASK) == aVersion);
  return aVersion & 0xFF;
}

static int intAtStringIndex(NSArray* array, int index) {
  return [(NSString*)[array objectAtIndex:index] integerValue];
}

void nsCocoaFeatures::GetSystemVersion(int& major, int& minor, int& bugfix) {
  major = minor = bugfix = 0;

  NSString* versionString = [[NSDictionary
      dictionaryWithContentsOfFile:@"/System/Library/CoreServices/SystemVersion.plist"]
      objectForKey:@"ProductVersion"];
  if (!versionString) {
    NS_ERROR("Couldn't read /System/Library/CoreServices/SystemVersion.plist to determine macOS "
             "version.");
    return;
  }
  NSArray* versions = [versionString componentsSeparatedByString:@"."];
  NSUInteger count = [versions count];
  if (count > 0) {
    major = intAtStringIndex(versions, 0);
    if (count > 1) {
      minor = intAtStringIndex(versions, 1);
      if (count > 2) {
        bugfix = intAtStringIndex(versions, 2);
      }
    }
  }
}

int32_t nsCocoaFeatures::GetVersion(int32_t aMajor, int32_t aMinor, int32_t aBugFix) {
  int32_t macOSVersion;
  if (aMajor < 10) {
    aMajor = 10;
    NS_ERROR("Couldn't determine macOS version, assuming 10.9");
    macOSVersion = MACOS_VERSION_10_9_HEX;
  } else if (aMajor == 10 && aMinor < 9) {
    aMinor = 9;
    NS_ERROR("macOS version too old, assuming 10.9");
    macOSVersion = MACOS_VERSION_10_9_HEX;
  } else {
    MOZ_ASSERT(aMajor >= 10);
    MOZ_ASSERT(aMajor < 256);
    MOZ_ASSERT(aMinor >= 0);
    MOZ_ASSERT(aMinor < 256);
    MOZ_ASSERT(aBugFix >= 0);
    MOZ_ASSERT(aBugFix < 256);
    macOSVersion = AssembleVersion(aMajor, aMinor, aBugFix);
  }
  MOZ_ASSERT(aMajor == ExtractMajorVersion(macOSVersion));
  MOZ_ASSERT(aMinor == ExtractMinorVersion(macOSVersion));
  MOZ_ASSERT(aBugFix == ExtractBugFixVersion(macOSVersion));
  return macOSVersion;
}

/*static*/ void nsCocoaFeatures::InitializeVersionNumbers() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Provide an autorelease pool to avoid leaking Cocoa objects,
  // as this gets called before the main autorelease pool is in place.
  nsAutoreleasePool localPool;

  int major, minor, bugfix;
  GetSystemVersion(major, minor, bugfix);
  mOSVersion = GetVersion(major, minor, bugfix);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

/* static */ int32_t nsCocoaFeatures::macOSVersion() {
  // Don't let this be called while we're first setting the value...
  MOZ_ASSERT((mOSVersion & MACOS_VERSION_MASK) >= 0);
  if (!mOSVersion) {
    mOSVersion = -1;
    InitializeVersionNumbers();
  }
  return mOSVersion;
}

/* static */ int32_t nsCocoaFeatures::macOSVersionMajor() {
  return ExtractMajorVersion(macOSVersion());
}

/* static */ int32_t nsCocoaFeatures::macOSVersionMinor() {
  return ExtractMinorVersion(macOSVersion());
}

/* static */ int32_t nsCocoaFeatures::macOSVersionBugFix() {
  return ExtractBugFixVersion(macOSVersion());
}

/* static */ bool nsCocoaFeatures::OnYosemiteOrLater() {
  return (macOSVersion() >= MACOS_VERSION_10_10_HEX);
}

/* static */ bool nsCocoaFeatures::OnElCapitanOrLater() {
  return (macOSVersion() >= MACOS_VERSION_10_11_HEX);
}

/* static */ bool nsCocoaFeatures::OnSierraExactly() {
  return (macOSVersion() >= MACOS_VERSION_10_12_HEX) && (macOSVersion() < MACOS_VERSION_10_13_HEX);
}

/* Version of OnSierraExactly as global function callable from cairo & skia */
bool Gecko_OnSierraExactly() { return nsCocoaFeatures::OnSierraExactly(); }

/* static */ bool nsCocoaFeatures::OnSierraOrLater() {
  return (macOSVersion() >= MACOS_VERSION_10_12_HEX);
}

/* static */ bool nsCocoaFeatures::OnHighSierraOrLater() {
  return (macOSVersion() >= MACOS_VERSION_10_13_HEX);
}

bool Gecko_OnHighSierraOrLater() { return nsCocoaFeatures::OnHighSierraOrLater(); }

/* static */ bool nsCocoaFeatures::OnMojaveOrLater() {
  return (macOSVersion() >= MACOS_VERSION_10_14_HEX);
}

/* static */ bool nsCocoaFeatures::OnCatalinaOrLater() {
  return (macOSVersion() >= MACOS_VERSION_10_15_HEX);
}

/* static */ bool nsCocoaFeatures::IsAtLeastVersion(int32_t aMajor, int32_t aMinor,
                                                    int32_t aBugFix) {
  return macOSVersion() >= GetVersion(aMajor, aMinor, aBugFix);
}
