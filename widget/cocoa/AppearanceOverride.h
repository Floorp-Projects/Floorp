/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AppearanceOverride_h
#define AppearanceOverride_h

#import <Cocoa/Cocoa.h>

// Implements support for the browser.theme.toolbar-theme pref.
// Use MOZGlobalAppearance.sharedInstance.effectiveAppearance
// in all places where you would like the global override to be respected. The
// effectiveAppearance property can be key-value observed.
@interface MOZGlobalAppearance : NSObject <NSAppearanceCustomization>
@property(class, readonly) MOZGlobalAppearance* sharedInstance;
@end

#endif
