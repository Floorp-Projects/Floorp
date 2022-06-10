/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SDKDefines_h
#define SDKDefines_h

#import <Cocoa/Cocoa.h>

/**
 * This file contains header declarations from SDKs more recent than the minimum macOS SDK which we
 * require for building Firefox, which is currently the macOS 11 SDK.
 */

#if !defined(MAC_OS_VERSION_12_0) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_VERSION_12_0

typedef CFTypeRef AXTextMarkerRef;
typedef CFTypeRef AXTextMarkerRangeRef;

extern "C" {
CFTypeID AXTextMarkerGetTypeID();
AXTextMarkerRef AXTextMarkerCreate(CFAllocatorRef allocator, const UInt8* bytes, CFIndex length);
const UInt8* AXTextMarkerGetBytePtr(AXTextMarkerRef text_marker);
CFIndex AXTextMarkerGetLength(AXTextMarkerRef text_marker);
CFTypeID AXTextMarkerRangeGetTypeID();
AXTextMarkerRangeRef AXTextMarkerRangeCreate(CFAllocatorRef allocator, AXTextMarkerRef start_marker,
                                             AXTextMarkerRef end_marker);
AXTextMarkerRef AXTextMarkerRangeCopyStartMarker(AXTextMarkerRangeRef text_marker_range);
AXTextMarkerRef AXTextMarkerRangeCopyEndMarker(AXTextMarkerRangeRef text_marker_range);
}

@interface NSScreen (NSScreen12_0)
// https://developer.apple.com/documentation/appkit/nsscreen/3882821-safeareainsets?language=objc&changes=latest_major
@property(readonly) NSEdgeInsets safeAreaInsets;
@end

#endif

#endif  // SDKDefines_h
