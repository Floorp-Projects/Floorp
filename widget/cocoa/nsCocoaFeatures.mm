/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file makes some assumptions about the versions of OS X.
// Instead of making it work major version 11 right now,
// we will wait until that happens and worry about it then.
// There are MOZ_ASSERTs to remind us to do that.
// We are assuming that the minor version is less than 16.
// There are MOZ_ASSERTs for that as well.

// The formula for the version integer based on OS X version 10.minor.bugfix is
// 0x1000 + (minor << 4) + bugifix.  See AssembleVersion() below.

#define MAC_OS_X_VERSION_MASK      0x0000FFFF
#define MAC_OS_X_VERSION_10_0_HEX  0x00001000
#define MAC_OS_X_VERSION_10_6_HEX  0x00001060
#define MAC_OS_X_VERSION_10_7_HEX  0x00001070
#define MAC_OS_X_VERSION_10_8_HEX  0x00001080
#define MAC_OS_X_VERSION_10_9_HEX  0x00001090
#define MAC_OS_X_VERSION_10_10_HEX 0x000010A0

#include "nsCocoaFeatures.h"
#include "nsCocoaUtils.h"
#include "nsDebug.h"
#include "nsObjCExceptions.h"

#import <Cocoa/Cocoa.h>

int32_t nsCocoaFeatures::mOSXVersion = 0;

inline int32_t AssembleVersion(int32_t aMajor, int32_t aMinor, int32_t aBugFix)
{
    MOZ_ASSERT(aMajor == 10);
    return MAC_OS_X_VERSION_10_0_HEX + (aMinor << 4) + aBugFix;
}

inline int32_t ExtractMajorVersion(int32_t aVersion)
{
    MOZ_ASSERT((aVersion & MAC_OS_X_VERSION_MASK) == aVersion);
    MOZ_ASSERT((aVersion & MAC_OS_X_VERSION_10_0_HEX) == MAC_OS_X_VERSION_10_0_HEX);
    return 10;
}

inline int32_t ExtractMinorVersion(int32_t aVersion)
{
    MOZ_ASSERT((aVersion & MAC_OS_X_VERSION_MASK) == aVersion);
    return (aVersion & 0xF0) >> 4;
}

inline int32_t ExtractBugFixVersion(int32_t aVersion)
{
    MOZ_ASSERT((aVersion & MAC_OS_X_VERSION_MASK) == aVersion);
    return aVersion & 0x0F;
}

static int intAtStringIndex(NSArray *array, int index)
{
    return [(NSString *)[array objectAtIndex:index] integerValue];
}

static void GetSystemVersion(int &major, int &minor, int &bugfix)
{
    major = minor = bugfix = 0;
    
    NSString* versionString = [[NSDictionary dictionaryWithContentsOfFile:
                                @"/System/Library/CoreServices/SystemVersion.plist"] objectForKey:@"ProductVersion"];
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

static int32_t GetFullVersion(int32_t aMajor, int32_t aMinor, int32_t aBugFix)
{
    int32_t osxVersion;
    if (aMajor < 10) {
        aMajor = 10;
        NS_ERROR("Couldn't determine OS X version, assuming 10.6");
        osxVersion = MAC_OS_X_VERSION_10_6_HEX;
    } else if (aMinor < 6) {
        aMinor = 6;
        NS_ERROR("OS X version too old, assuming 10.6");
        osxVersion = MAC_OS_X_VERSION_10_6_HEX;
    } else {
        MOZ_ASSERT(aMajor == 10);
        MOZ_ASSERT(aMinor < 16);
        MOZ_ASSERT(aBugFix >= 0);
        MOZ_ASSERT(aBugFix < 16);
        osxVersion = AssembleVersion(aMajor, aMinor, aBugFix);
    }
    MOZ_ASSERT(aMajor == ExtractMajorVersion(osxVersion));
    MOZ_ASSERT(aMinor == ExtractMinorVersion(osxVersion));
    MOZ_ASSERT(aBugFix == ExtractBugFixVersion(osxVersion));
    return osxVersion;
}

/*static*/ void
nsCocoaFeatures::InitializeVersionNumbers()
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

    // Provide an autorelease pool to avoid leaking Cocoa objects,
    // as this gets called before the main autorelease pool is in place.
    nsAutoreleasePool localPool;

    int major, minor, bugfix;
    GetSystemVersion(major, minor, bugfix);
    mOSXVersion = GetFullVersion(major, minor, bugfix);

    NS_OBJC_END_TRY_ABORT_BLOCK;
}

/* static */ int32_t
nsCocoaFeatures::OSXVersion()
{
    // Don't let this be called while we're first setting the value...
    MOZ_ASSERT((mOSXVersion & MAC_OS_X_VERSION_MASK) >= 0);
    if (!mOSXVersion) {
        mOSXVersion = -1;
        InitializeVersionNumbers();
    }
    return mOSXVersion;
}

/* static */ int32_t
nsCocoaFeatures::OSXVersionMajor()
{
    MOZ_ASSERT((OSXVersion() & MAC_OS_X_VERSION_10_0_HEX) == MAC_OS_X_VERSION_10_0_HEX);
    return 10;
}

/* static */ int32_t
nsCocoaFeatures::OSXVersionMinor()
{
    return ExtractMinorVersion(OSXVersion());
}

/* static */ int32_t
nsCocoaFeatures::OSXVersionBugFix()
{
    return ExtractBugFixVersion(OSXVersion());
}

/* static */ bool
nsCocoaFeatures::SupportCoreAnimationPlugins()
{
    // Disallow Core Animation on 10.5 because of crashes.
    // See Bug 711564.
    return (OSXVersion() >= MAC_OS_X_VERSION_10_6_HEX);
}

/* static */ bool
nsCocoaFeatures::OnLionOrLater()
{
    return (OSXVersion() >= MAC_OS_X_VERSION_10_7_HEX);
}

/* static */ bool
nsCocoaFeatures::OnMountainLionOrLater()
{
    return (OSXVersion() >= MAC_OS_X_VERSION_10_8_HEX);
}

/* static */ bool
nsCocoaFeatures::OnMavericksOrLater()
{
    return (OSXVersion() >= MAC_OS_X_VERSION_10_9_HEX);
}

/* static */ bool
nsCocoaFeatures::OnYosemiteOrLater()
{
    return (OSXVersion() >= MAC_OS_X_VERSION_10_10_HEX);
}

/* static */ bool
nsCocoaFeatures::AccelerateByDefault()
{
    return IsAtLeastVersion(10, 6, 3);
}

/* static */ bool
nsCocoaFeatures::IsAtLeastVersion(int32_t aMajor, int32_t aMinor, int32_t aBugFix)
{
    return OSXVersion() >= GetFullVersion(aMajor, aMinor, aBugFix);
}
