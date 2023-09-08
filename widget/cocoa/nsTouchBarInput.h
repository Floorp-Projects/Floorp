/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTouchBarInput_h_
#define nsTouchBarInput_h_

#import <Cocoa/Cocoa.h>

#include "nsITouchBarInput.h"
#include "nsCOMPtr.h"

using namespace mozilla::dom;

enum class TouchBarInputBaseType : uint8_t {
  kButton,
  kLabel,
  kMainButton,
  kPopover,
  kScrollView,
  kScrubber
};

class nsTouchBarInputIcon;

/**
 * NSObject representation of nsITouchBarInput.
 */
@interface TouchBarInput : NSObject {
  nsCOMPtr<nsIURI> mImageURI;
  RefPtr<nsTouchBarInputIcon> mIcon;
  TouchBarInputBaseType mBaseType;
  NSString* mType;
  nsCOMPtr<nsITouchBarInputCallback> mCallback;
  NSMutableArray<TouchBarInput*>* mChildren;
}

@property(strong) NSString* key;
@property(strong) NSString* type;
@property(strong) NSString* title;
@property(strong) NSColor* color;
@property(nonatomic, getter=isDisabled) BOOL disabled;

- (nsCOMPtr<nsIURI>)imageURI;
- (RefPtr<nsTouchBarInputIcon>)icon;
- (TouchBarInputBaseType)baseType;
- (NSTouchBarItemIdentifier)nativeIdentifier;
- (nsCOMPtr<nsITouchBarInputCallback>)callback;
- (NSMutableArray<TouchBarInput*>*)children;
- (void)setImageURI:(nsCOMPtr<nsIURI>)aImageURI;
- (void)setIcon:(RefPtr<nsTouchBarInputIcon>)aIcon;
- (void)setCallback:(nsCOMPtr<nsITouchBarInputCallback>)aCallback;
- (void)setChildren:(NSMutableArray<TouchBarInput*>*)aChildren;

- (id)initWithKey:(NSString*)aKey
            title:(NSString*)aTitle
         imageURI:(nsCOMPtr<nsIURI>)aImageURI
             type:(NSString*)aType
         callback:(nsCOMPtr<nsITouchBarInputCallback>)aCallback
            color:(uint32_t)aColor
         disabled:(BOOL)aDisabled
         children:(nsCOMPtr<nsIArray>)aChildren;

- (TouchBarInput*)initWithXPCOM:(nsCOMPtr<nsITouchBarInput>)aInput;

- (void)releaseJSObjects;

- (void)dealloc;

/**
 * We make these helper methods static so that other classes can query a
 * TouchBarInput's nativeIdentifier (e.g. nsTouchBarUpdater looking up a
 * popover in mappedLayoutItems).
 */
+ (NSTouchBarItemIdentifier)nativeIdentifierWithType:(NSString*)aType
                                             withKey:(NSString*)aKey;
+ (NSTouchBarItemIdentifier)nativeIdentifierWithXPCOM:
    (nsCOMPtr<nsITouchBarInput>)aInput;

// Non-JS scrubber implemention for the Share Scrubber,
// since it is defined by an Apple API.
+ (NSTouchBarItemIdentifier)shareScrubberIdentifier;

// The search popover needs to show/hide depending on if the Urlbar is focused
// when it is created. We keep track of its identifier to accommodate this
// special handling.
+ (NSTouchBarItemIdentifier)searchPopoverIdentifier;

@end

#endif  // nsTouchBarInput_h_
