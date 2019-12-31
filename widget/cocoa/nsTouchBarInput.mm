/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTouchBarInput.h"

#include "mozilla/MacStringHelpers.h"
#include "nsTouchBarInputIcon.h"

@implementation TouchBarInput
- (NSString*)key {
  return mKey;
}
- (NSString*)title {
  return mTitle;
}
- (nsCOMPtr<nsIURI>)imageURI {
  return mImageURI;
}
- (RefPtr<nsTouchBarInputIcon>)icon {
  return mIcon;
}
- (NSString*)type {
  return mType;
}
- (NSColor*)color {
  return mColor;
}
- (BOOL)isDisabled {
  return mDisabled;
}
- (NSTouchBarItemIdentifier)nativeIdentifier {
  return [TouchBarInput nativeIdentifierWithType:mType withKey:mKey];
}
- (nsCOMPtr<nsITouchBarInputCallback>)callback {
  return mCallback;
}
- (RefPtr<Document>)document {
  return mDocument;
}
- (BOOL)isIconPositionSet {
  return mIsIconPositionSet;
}
- (NSMutableArray<TouchBarInput*>*)children {
  return mChildren;
}
- (void)setKey:(NSString*)aKey {
  [aKey retain];
  [mKey release];
  mKey = aKey;
}

- (void)setTitle:(NSString*)aTitle {
  [aTitle retain];
  [mTitle release];
  mTitle = aTitle;
}

- (void)setImageURI:(nsCOMPtr<nsIURI>)aImageURI {
  mImageURI = aImageURI;
}

- (void)setIcon:(RefPtr<nsTouchBarInputIcon>)aIcon {
  mIcon = aIcon;
}

- (void)setType:(NSString*)aType {
  [aType retain];
  [mType release];
  mType = aType;
}

- (void)setColor:(NSColor*)aColor {
  [aColor retain];
  [mColor release];
  mColor = aColor;
}

- (void)setDisabled:(BOOL)aDisabled {
  mDisabled = aDisabled;
}

- (void)setCallback:(nsCOMPtr<nsITouchBarInputCallback>)aCallback {
  mCallback = aCallback;
}

- (void)setDocument:(RefPtr<Document>)aDocument {
  if (mIcon) {
    mIcon->Destroy();
    mIcon = nil;
  }
  mDocument = aDocument;
}

- (void)setIconPositionSet:(BOOL)aIsIconPositionSet {
  mIsIconPositionSet = aIsIconPositionSet;
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
         document:(RefPtr<Document>)aDocument
         children:(nsCOMPtr<nsIArray>)aChildren {
  if (self = [super init]) {
    [self setKey:aKey];
    [self setTitle:aTitle];
    [self setImageURI:aImageURI];
    [self setType:aType];
    [self setCallback:aCallback];
    [self setDocument:aDocument];
    [self setIconPositionSet:false];
    [self setDisabled:aDisabled];
    if (aColor) {
      [self setColor:[NSColor colorWithDisplayP3Red:((aColor >> 16) & 0xFF) / 255.0
                                              green:((aColor >> 8) & 0xFF) / 255.0
                                               blue:((aColor)&0xFF) / 255.0
                                              alpha:1.0]];
    }
    if (aChildren) {
      uint32_t itemCount = 0;
      aChildren->GetLength(&itemCount);
      NSMutableArray* orderedChildren = [NSMutableArray arrayWithCapacity:itemCount];
      for (uint32_t i = 0; i < itemCount; ++i) {
        nsCOMPtr<nsITouchBarInput> child = do_QueryElementAt(aChildren, i);
        if (!child) {
          continue;
        }
        TouchBarInput* convertedChild = [[TouchBarInput alloc] initWithXPCOM:child];
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

  RefPtr<Document> document;
  rv = aInput->GetDocument(getter_AddRefs(document));
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
                  document:document
                  children:children];
}

- (void)releaseJSObjects {
  if (mIcon) {
    mIcon->ReleaseJSObjects();
  }

  [self setCallback:nil];
  [self setImageURI:nil];
  [self setDocument:nil];
  for (TouchBarInput* child in mChildren) {
    [child releaseJSObjects];
  }
}

- (void)dealloc {
  if (mIcon) {
    mIcon->Destroy();
    mIcon = nil;
  }
  [mKey release];
  [mTitle release];
  [mType release];
  [mColor release];
  [mChildren removeAllObjects];
  [mChildren release];
  [super dealloc];
}

+ (NSTouchBarItemIdentifier)nativeIdentifierWithType:(NSString*)aType withKey:(NSString*)aKey {
  NSTouchBarItemIdentifier identifier;
  identifier = [BaseIdentifier stringByAppendingPathExtension:aType];
  if (aKey) {
    identifier = [identifier stringByAppendingPathExtension:aKey];
  }
  return identifier;
}

+ (NSTouchBarItemIdentifier)nativeIdentifierWithXPCOM:(nsCOMPtr<nsITouchBarInput>)aInput {
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

@end
