/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SDKDefines_h
#define SDKDefines_h

#import <Cocoa/Cocoa.h>

/**
 * This file contains header declarations from SDKs more recent than the minimum macOS SDK which we
 * require for building Firefox, which is currently the macOS 10.12 SDK.
 */

#if !defined(MAC_OS_X_VERSION_10_12_2) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12_2

@interface NSView (NSView10_12_2)
- (NSTouchBar*)makeTouchBar;
@end

#endif

#if !defined(MAC_OS_X_VERSION_10_13) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_13

using NSAppearanceName = NSString*;

@interface NSColor (NSColor10_13)
// "Available in 10.10", but not present in any SDK less than 10.13
@property(class, strong, readonly) NSColor* systemPurpleColor NS_AVAILABLE_MAC(10_10);
@end

@interface NSTask (NSTask10_13)
@property(copy) NSURL* executableURL NS_AVAILABLE_MAC(10_13);
@property(copy) NSArray<NSString*>* arguments;
- (BOOL)launchAndReturnError:(NSError**)error NS_AVAILABLE_MAC(10_13);
@end

enum : OSType {
  kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange = 'x420',
  kCVPixelFormatType_420YpCbCr10BiPlanarFullRange = 'xf20',
};

#endif

#if !defined(MAC_OS_X_VERSION_10_14) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_14

const NSAppearanceName NSAppearanceNameDarkAqua = @"NSAppearanceNameDarkAqua";

@interface NSWindow (NSWindow10_14)
@property(weak) NSObject<NSAppearanceCustomization>* appearanceSource NS_AVAILABLE_MAC(10_14);
@end

@interface NSApplication (NSApplication10_14)
@property(strong) NSAppearance* appearance NS_AVAILABLE_MAC(10_14);
@property(readonly, strong) NSAppearance* effectiveAppearance NS_AVAILABLE_MAC(10_14);
@end

@interface NSAppearance (NSAppearance10_14)
- (NSAppearanceName)bestMatchFromAppearancesWithNames:(NSArray<NSAppearanceName>*)appearances
    NS_AVAILABLE_MAC(10_14);
@end

@interface NSColor (NSColor10_14)
// Available in 10.10, but retroactively made public in 10.14.
@property(class, strong, readonly) NSColor* linkColor NS_AVAILABLE_MAC(10_10);
@end

enum {
  NSVisualEffectMaterialToolTip NS_ENUM_AVAILABLE_MAC(10_14) = 17,
};

#endif

#if !defined(MAC_OS_VERSION_11_0) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_VERSION_11_0
// The declarations below do not have NS_AVAILABLE_MAC(11_0) on them because we're building with a
// pre-macOS 11 SDK, so macOS 11 identifies itself as 10.16, and @available(macOS 11.0, *) checks
// won't work. You'll need to use an annoying double-whammy check for these:
//
// #if !defined(MAC_OS_VERSION_11_0) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_VERSION_11_0
//   if (nsCocoaFeatures::OnBigSurOrLater()) {
// #else
//   if (@available(macOS 11.0, *)) {
// #endif
//     ...
//   }
//

typedef NS_ENUM(NSInteger, NSTitlebarSeparatorStyle) {
  NSTitlebarSeparatorStyleAutomatic,
  NSTitlebarSeparatorStyleNone,
  NSTitlebarSeparatorStyleLine,
  NSTitlebarSeparatorStyleShadow
};

@interface NSWindow (NSWindow11_0)
@property NSTitlebarSeparatorStyle titlebarSeparatorStyle;
@end

@interface NSMenu (NSMenu11_0)
// In reality, NSMenu implements the NSAppearanceCustomization protocol, and picks up the appearance
// property from that protocol. But we can't tack on protocol implementations, so we just declare
// the property setter here.
- (void)setAppearance:(NSAppearance*)appearance;
@end

#endif

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
