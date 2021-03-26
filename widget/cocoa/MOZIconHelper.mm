/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Creates icons for display in native menu items on macOS.
 */

#include "MOZIconHelper.h"

#include "imgIContainer.h"
#include "nsCocoaUtils.h"

@implementation MOZIconHelper

// Returns an autoreleased empty NSImage.
+ (NSImage*)placeholderIconWithSize:(NSSize)aSize {
  return [[[NSImage alloc] initWithSize:aSize] autorelease];
}

// Returns an autoreleased NSImage.
+ (NSImage*)iconImageFromImageContainer:(imgIContainer*)aImage
                               withSize:(NSSize)aSize
                          computedStyle:(const mozilla::ComputedStyle*)aComputedStyle
                                subrect:(const nsIntRect&)aSubRect
                            scaleFactor:(CGFloat)aScaleFactor {
  bool isEntirelyBlack = false;
  NSImage* retainedImage = nil;
  nsresult rv;
  if (aScaleFactor != 0.0f) {
    rv = nsCocoaUtils::CreateNSImageFromImageContainer(aImage, imgIContainer::FRAME_CURRENT,
                                                       aComputedStyle, &retainedImage, aScaleFactor,
                                                       &isEntirelyBlack);
  } else {
    rv = nsCocoaUtils::CreateDualRepresentationNSImageFromImageContainer(
        aImage, imgIContainer::FRAME_CURRENT, aComputedStyle, &retainedImage, &isEntirelyBlack);
  }

  NSImage* image = [retainedImage autorelease];

  if (NS_FAILED(rv) || !image) {
    return nil;
  }

  int32_t origWidth = 0, origHeight = 0;
  aImage->GetWidth(&origWidth);
  aImage->GetHeight(&origHeight);

  // If the image region is invalid, don't draw the image to almost match
  // the behavior of other platforms.
  if (!aSubRect.IsEmpty() && (aSubRect.XMost() > origWidth || aSubRect.YMost() > origHeight)) {
    return nil;
  }

  bool createSubImage =
      !aSubRect.IsEmpty() && !(aSubRect.x == 0 && aSubRect.y == 0 && aSubRect.width == origWidth &&
                               aSubRect.height == origHeight);

  if (createSubImage) {
    // If aRect is set using CSS, we need to slice a piece out of the
    // overall image to use as the icon.
    NSRect subRect = NSMakeRect(aSubRect.x, aSubRect.y, aSubRect.width, aSubRect.height);
    NSImage* subImage = [NSImage imageWithSize:aSize
                                       flipped:NO
                                drawingHandler:^BOOL(NSRect subImageRect) {
                                  [image drawInRect:NSMakeRect(0, 0, aSize.width, aSize.height)
                                           fromRect:subRect
                                          operation:NSCompositingOperationCopy
                                           fraction:1.0f];
                                  return YES;
                                }];
    image = subImage;
  }

  // If all the color channels in the image are black, treat the image as a
  // template. This will cause macOS to use the image's alpha channel as a mask
  // and it will fill it with a color that looks good in the context that it's
  // used in. For example, for regular menu items, the image will be black, but
  // when the menu item is hovered (and its background is blue), it will be
  // filled with white.
  [image setTemplate:isEntirelyBlack];

  [image setSize:aSize];

  return image;
}

@end
