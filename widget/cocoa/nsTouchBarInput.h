/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTouchBarInput_h_
#define nsTouchBarInput_h_

#import <Cocoa/Cocoa.h>

#include "nsITouchBarInput.h"
#include "nsTouchBarNativeAPIDefines.h"

using namespace mozilla::dom;

class nsTouchBarInputIcon;

/**
 * NSObject representation of nsITouchBarInput.
 */
@interface TouchBarInput : NSObject {
  NSString* mKey;
  NSString* mTitle;
  nsCOMPtr<nsIURI> mImageURI;
  RefPtr<nsTouchBarInputIcon> mIcon;
  NSString* mType;
  NSColor* mColor;
  BOOL mDisabled;
  nsCOMPtr<nsITouchBarInputCallback> mCallback;
  RefPtr<Document> mDocument;
  BOOL mIsIconPositionSet;
  NSMutableArray<TouchBarInput*>* mChildren;
}

- (NSString*)key;
- (NSString*)title;
- (nsCOMPtr<nsIURI>)imageURI;
- (RefPtr<nsTouchBarInputIcon>)icon;
- (NSString*)type;
- (NSColor*)color;
- (BOOL)isDisabled;
- (NSTouchBarItemIdentifier)nativeIdentifier;
- (nsCOMPtr<nsITouchBarInputCallback>)callback;
- (RefPtr<Document>)document;
- (BOOL)isIconPositionSet;
- (NSMutableArray<TouchBarInput*>*)children;
- (void)setKey:(NSString*)aKey;
- (void)setTitle:(NSString*)aTitle;
- (void)setImageURI:(nsCOMPtr<nsIURI>)aImageURI;
- (void)setIcon:(RefPtr<nsTouchBarInputIcon>)aIcon;
- (void)setType:(NSString*)aType;
- (void)setColor:(NSColor*)aColor;
- (void)setDisabled:(BOOL)aDisabled;
- (void)setCallback:(nsCOMPtr<nsITouchBarInputCallback>)aCallback;
- (void)setDocument:(RefPtr<Document>)aDocument;
- (void)setIconPositionSet:(BOOL)aIsIconPositionSet;
- (void)setChildren:(NSMutableArray<TouchBarInput*>*)aChildren;

- (id)initWithKey:(NSString*)aKey
            title:(NSString*)aTitle
         imageURI:(nsCOMPtr<nsIURI>)aImageURI
             type:(NSString*)aType
         callback:(nsCOMPtr<nsITouchBarInputCallback>)aCallback
            color:(uint32_t)aColor
         disabled:(BOOL)aDisabled
         document:(RefPtr<Document>)aDocument
         children:(nsCOMPtr<nsIArray>)aChildren;

- (TouchBarInput*)initWithXPCOM:(nsCOMPtr<nsITouchBarInput>)aInput;

- (void)releaseJSObjects;

- (void)dealloc;

/**
 * We make these helper methods static so that other classes can query a
 * TouchBarInput's nativeIdentifier (e.g. nsTouchBarUpdater looking up a
 * popover in mappedLayoutItems).
 */
+ (NSTouchBarItemIdentifier)nativeIdentifierWithType:(NSString*)aType withKey:(NSString*)aKey;
+ (NSTouchBarItemIdentifier)nativeIdentifierWithXPCOM:(nsCOMPtr<nsITouchBarInput>)aInput;

@end

#endif  // nsTouchBarInput_h_
