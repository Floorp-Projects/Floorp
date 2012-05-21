/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define MAC_OS_X_VERSION_MASK     0x0000FFFF // Not supported
#define MAC_OS_X_VERSION_10_4_HEX 0x00001040 // Not supported
#define MAC_OS_X_VERSION_10_5_HEX 0x00001050
#define MAC_OS_X_VERSION_10_6_HEX 0x00001060
#define MAC_OS_X_VERSION_10_7_HEX 0x00001070

// This API will not work for OS X 10.10, see Gestalt.h.

#include "nsCocoaFeatures.h"
#include "nsDebug.h"
#include "nsObjCExceptions.h"

#import <Cocoa/Cocoa.h>

PRInt32 nsCocoaFeatures::mOSXVersion = 0;

/* static */ PRInt32
nsCocoaFeatures::OSXVersion()
{
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

    if (!mOSXVersion) {
        // minor version is not accurate, use gestaltSystemVersionMajor, 
        // gestaltSystemVersionMinor, gestaltSystemVersionBugFix for these
        OSErr err = ::Gestalt(gestaltSystemVersion, reinterpret_cast<SInt32*>(&mOSXVersion));
        if (err != noErr) {
            // This should probably be changed when our minimum version changes
            NS_ERROR("Couldn't determine OS X version, assuming 10.5");
            mOSXVersion = MAC_OS_X_VERSION_10_5_HEX;
        }
        mOSXVersion &= MAC_OS_X_VERSION_MASK;
    }
    return mOSXVersion;

    NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(0);
}

/* static */ bool
nsCocoaFeatures::SupportCoreAnimationPlugins()
{
    // Disallow Core Animation on 10.5 because of crashes.
    // See Bug 711564.
    return (OSXVersion() >= MAC_OS_X_VERSION_10_6_HEX);
}

/* static */ bool
nsCocoaFeatures::OnSnowLeopardOrLater()
{
    return (OSXVersion() >= MAC_OS_X_VERSION_10_6_HEX);
}

/* static */ bool
nsCocoaFeatures::OnLionOrLater()
{
    return (OSXVersion() >= MAC_OS_X_VERSION_10_7_HEX);
}

