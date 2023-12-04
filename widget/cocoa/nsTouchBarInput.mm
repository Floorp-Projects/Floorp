/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTouchBarInput.h"

#include "mozilla/MacStringHelpers.h"
#include "nsArrayUtils.h"
#include "nsCocoaUtils.h"
#include "nsTouchBar.h"
#include "nsTouchBarInputIcon.h"

@implementation TouchBarInput

- (nsCOMPtr<nsIURI>)imageURI {
  return mImageURI;
}

- (void)setImageURI:(nsCOMPtr<nsIURI>)aImageURI {
  mImageURI = aImageURI;
}

- (RefPtr<nsTouchBarInputIcon>)icon {
  return mIcon;
}

- (void)setIcon:(RefPtr<nsTouchBarInputIcon>)aIcon {
  mIcon = aIcon;
}

- (TouchBarInputBaseType)baseType {
  return mBaseType;
}

- (NSString*)type {
  return mType;
}

- (void)setType:(NSString*)aType {
  [aType retain];
  [mType release];
  if ([aType hasSuffix:@"button"]) {
    mBaseType = TouchBarInputBaseType::kButton;
  } else if ([aType hasSuffix:@"label"]) {
    mBaseType = TouchBarInputBaseType::kLabel;
  } else if ([aType hasSuffix:@"mainButton"]) {
    mBaseType = TouchBarInputBaseType::kMainButton;
  } else if ([aType hasSuffix:@"popover"]) {
    mBaseType = TouchBarInputBaseType::kPopover;
  } else if ([aType hasSuffix:@"scrollView"]) {
    mBaseType = TouchBarInputBaseType::kScrollView;
  } else if ([aType hasSuffix:@"scrubber"]) {
    mBaseType = TouchBarInputBaseType::kScrubber;
  }
  mType = aType;
}

- (NSTouchBarItemIdentifier)nativeIdentifier {
  return [TouchBarInput nativeIdentifierWithType:mType withKey:self.key];
}

- (nsCOMPtr<nsITouchBarInputCallback>)callback {
  return mCallback;
}

- (void)setCallback:(nsCOMPtr<nsITouchBarInputCallback>)aCallback {
  mCallback = aCallback;
}

- (NSMutableArray<TouchBarInput*>*)children {
  return mChildren;
}

- (void)setChildren:(NSMutableArray<TouchBarInput*>*)aChildren {
  [aChildren retain];
  for (TouchBarInput* child in mChildren) {
    [child releaseJSObjects];
  }
  [mChildren removeAllObjects];
  [mChildren release];
  mChildren = aChildren;
}

- (id)initWithKey:(NSString*)aKey
            title:(NSString*)aTitle
         imageURI:(nsCOMPtr<nsIURI>)aImageURI
             type:(NSString*)aType
         callback:(nsCOMPtr<nsITouchBarInputCallback>)aCallback
            color:(uint32_t)aColor
         disabled:(BOOL)aDisabled
         children:(nsCOMPtr<nsIArray>)aChildren {
  if (self = [super init]) {
    mType = nil;

    self.key = aKey;
    self.title = aTitle;
    self.type = aType;
    self.disabled = aDisabled;
    [self setImageURI:aImageURI];
    [self setCallback:aCallback];
    if (aColor) {
      [self setColor:[NSColor
                         colorWithDisplayP3Red:((aColor >> 16) & 0xFF) / 255.0
                                         green:((aColor >> 8) & 0xFF) / 255.0
                                          blue:((aColor) & 0xFF) / 255.0
                                         alpha:1.0]];
    }
    if (aChildren) {
      uint32_t itemCount = 0;
      aChildren->GetLength(&itemCount);
      NSMutableArray* orderedChildren =
          [NSMutableArray arrayWithCapacity:itemCount];
      for (uint32_t i = 0; i < itemCount; ++i) {
        nsCOMPtr<nsITouchBarInput> child = do_QueryElementAt(aChildren, i);
        if (!child) {
          continue;
        }
        TouchBarInput* convertedChild =
            [[TouchBarInput alloc] initWithXPCOM:child];
        if (convertedChild) {
          orderedChildren[i] = convertedChild;
        }
      }
      [self setChildren:orderedChildren];
    }
  }

  return self;
}

- (TouchBarInput*)initWithXPCOM:(nsCOMPtr<nsITouchBarInput>)aInput {
  nsAutoString keyStr;
  nsresult rv = aInput->GetKey(keyStr);
  if (NS_FAILED(rv)) {
    return nil;
  }

  nsAutoString titleStr;
  rv = aInput->GetTitle(titleStr);
  if (NS_FAILED(rv)) {
    return nil;
  }

  nsCOMPtr<nsIURI> imageURI;
  rv = aInput->GetImage(getter_AddRefs(imageURI));
  if (NS_FAILED(rv)) {
    return nil;
  }

  nsAutoString typeStr;
  rv = aInput->GetType(typeStr);
  if (NS_FAILED(rv)) {
    return nil;
  }

  nsCOMPtr<nsITouchBarInputCallback> callback;
  rv = aInput->GetCallback(getter_AddRefs(callback));
  if (NS_FAILED(rv)) {
    return nil;
  }

  uint32_t colorInt;
  rv = aInput->GetColor(&colorInt);
  if (NS_FAILED(rv)) {
    return nil;
  }

  bool disabled = false;
  rv = aInput->GetDisabled(&disabled);
  if (NS_FAILED(rv)) {
    return nil;
  }

  nsCOMPtr<nsIArray> children;
  rv = aInput->GetChildren(getter_AddRefs(children));
  if (NS_FAILED(rv)) {
    return nil;
  }

  return [self initWithKey:nsCocoaUtils::ToNSString(keyStr)
                     title:nsCocoaUtils::ToNSString(titleStr)
                  imageURI:imageURI
                      type:nsCocoaUtils::ToNSString(typeStr)
                  callback:callback
                     color:colorInt
                  disabled:(BOOL)disabled
                  children:children];
}

- (void)releaseJSObjects {
  if (mIcon) {
    mIcon->Destroy();
    mIcon = nil;
  }
  [self setCallback:nil];
  [self setImageURI:nil];
  for (TouchBarInput* child in mChildren) {
    [child releaseJSObjects];
  }
}

- (void)dealloc {
  if (mIcon) {
    mIcon->Destroy();
    mIcon = nil;
  }
  [mType release];
  [mChildren removeAllObjects];
  [mChildren release];
  [super dealloc];
}

+ (NSTouchBarItemIdentifier)nativeIdentifierWithType:(NSString*)aType
                                             withKey:(NSString*)aKey {
  NSTouchBarItemIdentifier identifier;
  identifier = [kTouchBarBaseIdentifier stringByAppendingPathExtension:aType];
  if (aKey) {
    identifier = [identifier stringByAppendingPathExtension:aKey];
  }
  return identifier;
}

+ (NSTouchBarItemIdentifier)nativeIdentifierWithXPCOM:
    (nsCOMPtr<nsITouchBarInput>)aInput {
  nsAutoString keyStr;
  nsresult rv = aInput->GetKey(keyStr);
  if (NS_FAILED(rv)) {
    return nil;
  }
  NSString* key = nsCocoaUtils::ToNSString(keyStr);

  nsAutoString typeStr;
  rv = aInput->GetType(typeStr);
  if (NS_FAILED(rv)) {
    return nil;
  }
  NSString* type = nsCocoaUtils::ToNSString(typeStr);

  return [TouchBarInput nativeIdentifierWithType:type withKey:key];
}

+ (NSTouchBarItemIdentifier)shareScrubberIdentifier {
  return [TouchBarInput nativeIdentifierWithType:@"scrubber" withKey:@"share"];
}

+ (NSTouchBarItemIdentifier)searchPopoverIdentifier {
  return [TouchBarInput nativeIdentifierWithType:@"popover"
                                         withKey:@"search-popover"];
}

@end
