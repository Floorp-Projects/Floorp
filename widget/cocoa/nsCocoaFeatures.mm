/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define MAC_OS_X_VERSION_MASK     0x0000FFFF
#define MAC_OS_X_VERSION_10_6_HEX 0x00001060
#define MAC_OS_X_VERSION_10_7_HEX 0x00001070
#define MAC_OS_X_VERSION_10_8_HEX 0x00001080
#define MAC_OS_X_VERSION_10_9_HEX 0x00001090

// This API will not work for OS X 10.10, see Gestalt.h.

#include "nsCocoaFeatures.h"
#include "nsCocoaUtils.h"
#include "nsDebug.h"
#include "nsObjCExceptions.h"

#import <Cocoa/Cocoa.h>

int32_t nsCocoaFeatures::mOSXVersion = 0;
int32_t nsCocoaFeatures::mOSXVersionMajor = 0;
int32_t nsCocoaFeatures::mOSXVersionMinor = 0;
int32_t nsCocoaFeatures::mOSXVersionBugFix = 0;

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

/*static*/ void
nsCocoaFeatures::InitializeVersionNumbers()
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

    // Provide an autorelease pool to avoid leaking Cocoa objects,
    // as this gets called before the main autorelease pool is in place.
    nsAutoreleasePool localPool;

    int major, minor, bugfix;
    GetSystemVersion(major, minor, bugfix);

    mOSXVersionMajor = major;
    mOSXVersionMinor = minor;
    mOSXVersionBugFix = bugfix;

    if (major < 10) {
        NS_ERROR("Couldn't determine OS X version, assuming 10.6");
        mOSXVersion = MAC_OS_X_VERSION_10_6_HEX;
        mOSXVersionMajor = 10;
        mOSXVersionMinor = 6;
        mOSXVersionBugFix = 0;
    } else if (minor < 6) {
        NS_ERROR("OS X version too old, assuming 10.6");
        mOSXVersion = MAC_OS_X_VERSION_10_6_HEX;
        mOSXVersionMinor = 6;
        mOSXVersionBugFix = 0;
    } else {
        mOSXVersion = 0x1000 + (minor << 4);
    }

    NS_OBJC_END_TRY_ABORT_BLOCK;
}

/* static */ int32_t
nsCocoaFeatures::OSXVersion()
{
    if (!mOSXVersion) {
        InitializeVersionNumbers();
    }
    return mOSXVersion;
}

/* static */ int32_t
nsCocoaFeatures::OSXVersionMajor()
{
    if (!mOSXVersion) {
        InitializeVersionNumbers();
    }
    return mOSXVersionMajor;
}

/* static */ int32_t
nsCocoaFeatures::OSXVersionMinor()
{
    if (!mOSXVersion) {
        InitializeVersionNumbers();
    }
    return mOSXVersionMinor;
}

/* static */ int32_t
nsCocoaFeatures::OSXVersionBugFix()
{
    if (!mOSXVersion) {
        InitializeVersionNumbers();
    }
    return mOSXVersionBugFix;
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
