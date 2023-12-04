/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZIconHelper_h
#define MOZIconHelper_h

#import <Cocoa/Cocoa.h>

#include "nsRect.h"

class imgIContainer;
class nsPresContext;

namespace mozilla {
class ComputedStyle;
}

@interface MOZIconHelper : NSObject

// Returns an autoreleased empty NSImage.
+ (NSImage*)placeholderIconWithSize:(NSSize)aSize;

// Returns an autoreleased NSImage.
+ (NSImage*)iconImageFromImageContainer:(imgIContainer*)aImage
                               withSize:(NSSize)aSize
                            presContext:(const nsPresContext*)aPresContext
                          computedStyle:
                              (const mozilla::ComputedStyle*)aComputedStyle
                            scaleFactor:(CGFloat)aScaleFactor;

@end

#endif  // MOZIconHelper_h
