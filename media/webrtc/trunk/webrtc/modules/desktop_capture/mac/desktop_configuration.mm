/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/mac/desktop_configuration.h"

#include <math.h>
#include <algorithm>
#include <Cocoa/Cocoa.h>

#include "webrtc/system_wrappers/interface/logging.h"

#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

@interface NSScreen (LionAPI)
- (CGFloat)backingScaleFactor;
- (NSRect)convertRectToBacking:(NSRect)aRect;
@end

#endif  // 10.7

namespace webrtc {

namespace {

DesktopRect NSRectToDesktopRect(const NSRect& ns_rect) {
  return DesktopRect::MakeLTRB(
      static_cast<int>(floor(ns_rect.origin.x)),
      static_cast<int>(floor(ns_rect.origin.y)),
      static_cast<int>(ceil(ns_rect.origin.x + ns_rect.size.width)),
      static_cast<int>(ceil(ns_rect.origin.y + ns_rect.size.height)));
}

DesktopRect JoinRects(const DesktopRect& a,
                              const DesktopRect& b) {
  return DesktopRect::MakeLTRB(
      std::min(a.left(), b.left()),
      std::min(a.top(), b.top()),
      std::max(a.right(), b.right()),
      std::max(a.bottom(), b.bottom()));
}

// Inverts the position of |rect| from bottom-up coordinates to top-down,
// relative to |bounds|.
void InvertRectYOrigin(const DesktopRect& bounds,
                       DesktopRect* rect) {
  assert(bounds.top() == 0);
  *rect = DesktopRect::MakeXYWH(
      rect->left(), bounds.bottom() - rect->bottom(),
      rect->width(), rect->height());
}

MacDisplayConfiguration GetConfigurationForScreen(NSScreen* screen) {
  MacDisplayConfiguration display_config;

  // Fetch the NSScreenNumber, which is also the CGDirectDisplayID.
  NSDictionary* device_description = [screen deviceDescription];
  display_config.id = static_cast<CGDirectDisplayID>(
      [[device_description objectForKey:@"NSScreenNumber"] intValue]);

  // Determine the display's logical & physical dimensions.
  NSRect ns_bounds = [screen frame];
  display_config.bounds = NSRectToDesktopRect(ns_bounds);

  // If the host is running Mac OS X 10.7+ or later, query the scaling factor
  // between logical and physical (aka "backing") pixels, otherwise assume 1:1.
  if ([screen respondsToSelector:@selector(backingScaleFactor)] &&
      [screen respondsToSelector:@selector(convertRectToBacking:)]) {
    display_config.dip_to_pixel_scale = [screen backingScaleFactor];
    NSRect ns_pixel_bounds = [screen convertRectToBacking: ns_bounds];
    display_config.pixel_bounds = NSRectToDesktopRect(ns_pixel_bounds);
  } else {
    display_config.pixel_bounds = display_config.bounds;
  }

  return display_config;
}

} // namespace

MacDisplayConfiguration::MacDisplayConfiguration()
    : id(0),
      dip_to_pixel_scale(1.0f) {
}

MacDesktopConfiguration::MacDesktopConfiguration()
    : dip_to_pixel_scale(1.0f) {
}

MacDesktopConfiguration::~MacDesktopConfiguration() {
}

// static
MacDesktopConfiguration MacDesktopConfiguration::GetCurrent(Origin origin) {
  MacDesktopConfiguration desktop_config;

  NSArray* screens = [NSScreen screens];
  assert(screens);

  // Iterator over the monitors, adding the primary monitor and monitors whose
  // DPI match that of the primary monitor.
  for (NSUInteger i = 0; i < [screens count]; ++i) {
    MacDisplayConfiguration display_config =
        GetConfigurationForScreen([screens objectAtIndex: i]);

    // Handling mixed-DPI is hard, so we only return displays that match the
    // "primary" display's DPI.  The primary display is always the first in the
    // list returned by [NSScreen screens].
    if (i == 0) {
      desktop_config.dip_to_pixel_scale = display_config.dip_to_pixel_scale;
    } else if (desktop_config.dip_to_pixel_scale !=
               display_config.dip_to_pixel_scale) {
      continue;
    }

    // Cocoa uses bottom-up coordinates, so if the caller wants top-down then
    // we need to invert the positions of secondary monitors relative to the
    // primary one (the primary monitor's position is (0,0) in both systems).
    if (i > 0 && origin == TopLeftOrigin) {
      InvertRectYOrigin(desktop_config.displays[0].bounds,
                        &display_config.bounds);
      InvertRectYOrigin(desktop_config.displays[0].pixel_bounds,
                        &display_config.pixel_bounds);
    }

    // Add the display to the configuration.
    desktop_config.displays.push_back(display_config);

    // Update the desktop bounds to account for this display.
    desktop_config.bounds =
        JoinRects(desktop_config.bounds, display_config.bounds);
    desktop_config.pixel_bounds =
        JoinRects(desktop_config.pixel_bounds, display_config.pixel_bounds);
  }

  return desktop_config;
}

}  // namespace webrtc
