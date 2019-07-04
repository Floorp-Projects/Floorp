/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTouchBar.h"

#include "mozilla/MacStringHelpers.h"
#include "mozilla/Telemetry.h"
#include "nsArrayUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIArray.h"

@implementation nsTouchBar

static NSTouchBarItemIdentifier CustomButtonIdentifier = @"com.mozilla.firefox.touchbar.button";
static NSTouchBarItemIdentifier CustomMainButtonIdentifier =
    @"com.mozilla.firefox.touchbar.mainbutton";
static NSTouchBarItemIdentifier ScrubberIdentifier = @"com.mozilla.firefox.touchbar.scrubber";

// Non-JS scrubber implemention for the Share Scrubber,
// since it is defined by an Apple API.
static NSTouchBarItemIdentifier ShareScrubberIdentifier =
    [ScrubberIdentifier stringByAppendingPathExtension:@"share"];

// Used to tie action strings to buttons.
static char sIdentifierAssociationKey;

// The system default width for Touch Bar inputs is 128px. This is double.
#define MAIN_BUTTON_WIDTH 256

#pragma mark - NSTouchBarDelegate

- (instancetype)init {
  if ((self = [super init])) {
    mTouchBarHelper = do_GetService(NS_TOUCHBARHELPER_CID);
    if (!mTouchBarHelper) {
      return nil;
    }

    self.delegate = self;
    self.mappedLayoutItems = [NSMutableDictionary dictionary];
    nsCOMPtr<nsIArray> layoutItems;

    nsresult rv = mTouchBarHelper->GetLayout(getter_AddRefs(layoutItems));
    if (NS_FAILED(rv) || !layoutItems) {
      return nil;
    }

    uint32_t itemCount = 0;
    layoutItems->GetLength(&itemCount);
    // This is copied to self.defaultItemIdentifiers. Required since
    // [self.mappedLayoutItems allKeys] does not preserve order.
    NSMutableArray* orderedLayoutIdentifiers = [NSMutableArray arrayWithCapacity:itemCount];
    for (uint32_t i = 0; i < itemCount; ++i) {
      nsCOMPtr<nsITouchBarInput> input = do_QueryElementAt(layoutItems, i);
      if (!input) {
        continue;
      }

      TouchBarInput* convertedInput = [[TouchBarInput alloc] initWithXPCOM:input];

      // Add new input to dictionary for lookup of properties in delegate.
      self.mappedLayoutItems[[convertedInput nativeIdentifier]] = convertedInput;
      orderedLayoutIdentifiers[i] = [convertedInput nativeIdentifier];
    }

    self.defaultItemIdentifiers = [orderedLayoutIdentifiers copy];
  }

  return self;
}

- (void)dealloc {
  for (NSTouchBarItemIdentifier identifier in self.mappedLayoutItems) {
    NSTouchBarItem* item = [self itemForIdentifier:identifier];
    [item release];
  }

  [self.defaultItemIdentifiers release];

  [self.mappedLayoutItems removeAllObjects];
  [self.mappedLayoutItems release];
  [super dealloc];
}

- (NSTouchBarItem*)touchBar:(NSTouchBar*)aTouchBar
      makeItemForIdentifier:(NSTouchBarItemIdentifier)aIdentifier {
  if ([aIdentifier hasPrefix:ScrubberIdentifier]) {
    if (![aIdentifier isEqualToString:ShareScrubberIdentifier]) {
      // We're only supporting the Share scrubber for now.
      return nil;
    }
    return [self makeShareScrubberForIdentifier:aIdentifier];
  }

  // The cases of a button or main button require the same setup.
  NSButton* button = [NSButton buttonWithTitle:@"" target:self action:@selector(touchBarAction:)];
  NSCustomTouchBarItem* item = [[NSCustomTouchBarItem alloc] initWithIdentifier:aIdentifier];
  item.view = button;

  TouchBarInput* input = self.mappedLayoutItems[aIdentifier];
  if ([aIdentifier hasPrefix:CustomButtonIdentifier]) {
    return [self updateButton:item input:input];
  } else if ([aIdentifier hasPrefix:CustomMainButtonIdentifier]) {
    return [self updateMainButton:item input:input];
  }

  return nil;
}

- (void)updateItem:(TouchBarInput*)aInput {
  NSTouchBarItem* item = [self itemForIdentifier:[aInput nativeIdentifier]];
  if (!item) {
    return;
  }
  if ([[aInput nativeIdentifier] hasPrefix:CustomButtonIdentifier]) {
    [self updateButton:(NSCustomTouchBarItem*)item input:aInput];
  } else if ([[aInput nativeIdentifier] hasPrefix:CustomMainButtonIdentifier]) {
    [self updateMainButton:(NSCustomTouchBarItem*)item input:aInput];
  }

  [self.mappedLayoutItems[[aInput nativeIdentifier]] release];
  self.mappedLayoutItems[[aInput nativeIdentifier]] = aInput;
}

- (NSTouchBarItem*)updateButton:(NSCustomTouchBarItem*)aButton input:(TouchBarInput*)aInput {
  NSButton* button = (NSButton*)aButton.view;
  if (!button) {
    return nil;
  }

  button.title = [aInput title];
  if ([aInput image]) {
    button.image = [aInput image];
    [button setImagePosition:NSImageOnly];
  }

  [button setEnabled:![aInput isDisabled]];

  if ([aInput color]) {
    button.bezelColor = [aInput color];
  }

  objc_setAssociatedObject(button, &sIdentifierAssociationKey, [aInput nativeIdentifier],
                           OBJC_ASSOCIATION_RETAIN);

  aButton.customizationLabel = [aInput title];

  return aButton;
}

- (NSTouchBarItem*)updateMainButton:(NSCustomTouchBarItem*)aMainButton
                              input:(TouchBarInput*)aInput {
  aMainButton = (NSCustomTouchBarItem*)[self updateButton:aMainButton input:aInput];
  NSButton* button = (NSButton*)aMainButton.view;
  button.imageHugsTitle = YES;
  // If empty, string is still being localized. Display a blank input instead.
  if ([button.title isEqualToString:@""]) {
    [button setImagePosition:NSNoImage];
  } else {
    [button setImagePosition:NSImageLeft];
  }

  [button.widthAnchor constraintGreaterThanOrEqualToConstant:MAIN_BUTTON_WIDTH].active = YES;
  [button setContentHuggingPriority:1.0 forOrientation:NSLayoutConstraintOrientationHorizontal];
  return aMainButton;
}

- (NSTouchBarItem*)makeShareScrubberForIdentifier:(NSTouchBarItemIdentifier)aIdentifier {
  TouchBarInput* input = self.mappedLayoutItems[aIdentifier];
  // System-default share menu
  NSSharingServicePickerTouchBarItem* servicesItem =
      [[NSSharingServicePickerTouchBarItem alloc] initWithIdentifier:aIdentifier];
  servicesItem.buttonImage = [input image];
  servicesItem.delegate = self;
  return servicesItem;
}

- (void)touchBarAction:(id)aSender {
  NSTouchBarItemIdentifier identifier =
      objc_getAssociatedObject(aSender, &sIdentifierAssociationKey);
  if (!identifier || [identifier isEqualToString:@""]) {
    return;
  }

  TouchBarInput* input = self.mappedLayoutItems[identifier];
  if (!input) {
    return;
  }

  nsCOMPtr<nsITouchBarInputCallback> callback = [input callback];
  callback->OnCommand();
}

- (void)releaseJSObjects {
  mTouchBarHelper = nil;

  for (NSTouchBarItemIdentifier identifier in self.mappedLayoutItems) {
    TouchBarInput* input = self.mappedLayoutItems[identifier];
    [input setCallback:nil];
  }
}

#pragma mark - TouchBar Utilities

+ (NSImage*)getTouchBarIconNamed:(NSString*)aImageName {
  nsCOMPtr<nsIFile> resDir;
  nsAutoCString resPath;
  NSString* pathToImage;

  nsresult rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(resDir));
  resDir->AppendNative(NS_LITERAL_CSTRING("res"));
  resDir->AppendNative(NS_LITERAL_CSTRING("touchbar"));

  rv = resDir->GetNativePath(resPath);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nil;
  }

  pathToImage = [NSString stringWithUTF8String:(const char*)resPath.get()];
  pathToImage = [pathToImage stringByAppendingPathComponent:aImageName];
  NSImage* image = [[[NSImage alloc] initWithContentsOfFile:pathToImage] autorelease];
  // A nil image will fail gracefully to a labelled button

  return image;
}

#pragma mark - NSSharingServicePickerTouchBarItemDelegate

- (NSArray*)itemsForSharingServicePickerTouchBarItem:
    (NSSharingServicePickerTouchBarItem*)aPickerTouchBarItem {
  NSURL* urlToShare = nil;
  NSString* titleToShare = @"";
  nsAutoString url;
  nsAutoString title;
  if (mTouchBarHelper) {
    nsresult rv = mTouchBarHelper->GetActiveUrl(url);
    if (!NS_FAILED(rv)) {
      urlToShare = [NSURL URLWithString:nsCocoaUtils::ToNSString(url)];
      // NSURL URLWithString returns nil if the URL is invalid. At this point,
      // it is too late to simply shut down the share menu, so we default to
      // about:blank if the share button is clicked when the URL is invalid.
      if (urlToShare == nil) {
        urlToShare = [NSURL URLWithString:@"about:blank"];
      }
    }

    rv = mTouchBarHelper->GetActiveTitle(title);
    if (!NS_FAILED(rv)) {
      titleToShare = nsCocoaUtils::ToNSString(title);
    }
  }

  // If the user has gotten this far, they have clicked the share button so it
  // is logged.
  Telemetry::AccumulateCategorical(Telemetry::LABELS_TOUCHBAR_BUTTON_PRESSES::Share);

  return @[ urlToShare, titleToShare ];
}

- (NSArray<NSSharingService*>*)sharingServicePicker:(NSSharingServicePicker*)aSharingServicePicker
                            sharingServicesForItems:(NSArray*)aItems
                            proposedSharingServices:(NSArray<NSSharingService*>*)aProposedServices {
  // redundant services
  NSArray* excludedServices = @[
    @"com.apple.share.System.add-to-safari-reading-list",
  ];

  NSArray* sharingServices = [aProposedServices
      filteredArrayUsingPredicate:[NSPredicate
                                      predicateWithFormat:@"NOT (name IN %@)", excludedServices]];

  return sharingServices;
}

@end

@implementation TouchBarInput
- (NSString*)key {
  return mKey;
}
- (NSString*)title {
  return mTitle;
}
- (NSImage*)image {
  return mImage;
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
  return mNativeIdentifier;
}
- (nsCOMPtr<nsITouchBarInputCallback>)callback {
  return mCallback;
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

- (void)setImage:(NSImage*)aImage {
  [aImage retain];
  [mImage release];
  mImage = aImage;
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

- (void)setNativeIdentifier:(NSTouchBarItemIdentifier)aNativeIdentifier {
  [aNativeIdentifier retain];
  [mNativeIdentifier release];
  mNativeIdentifier = aNativeIdentifier;
}

- (void)setCallback:(nsCOMPtr<nsITouchBarInputCallback>)aCallback {
  mCallback = aCallback;
}

- (id)initWithKey:(NSString*)aKey
            title:(NSString*)aTitle
            image:(NSString*)aImage
             type:(NSString*)aType
         callback:(nsCOMPtr<nsITouchBarInputCallback>)aCallback
            color:(uint32_t)aColor
         disabled:(BOOL)aDisabled {
  if (self = [super init]) {
    [self setKey:aKey];
    [self setTitle:aTitle];
    [self setImage:[nsTouchBar getTouchBarIconNamed:aImage]];
    [self setType:aType];
    [self setCallback:aCallback];
    if (aColor) {
      [self setColor:[NSColor colorWithDisplayP3Red:((aColor >> 16) & 0xFF) / 255.0
                                              green:((aColor >> 8) & 0xFF) / 255.0
                                               blue:((aColor)&0xFF) / 255.0
                                              alpha:1.0]];
    }
    [self setDisabled:aDisabled];

    NSTouchBarItemIdentifier TypeIdentifier = @"";
    if ([aType isEqualToString:@"scrubber"]) {
      TypeIdentifier = ScrubberIdentifier;
    } else if ([aType isEqualToString:@"mainButton"]) {
      TypeIdentifier = CustomMainButtonIdentifier;
    } else {
      TypeIdentifier = CustomButtonIdentifier;
    }

    if (!aKey) {
      [self setNativeIdentifier:TypeIdentifier];
    } else if ([aKey isEqualToString:@"share"]) {
      [self setNativeIdentifier:[TypeIdentifier stringByAppendingPathExtension:aKey]];
    } else {
      [self setNativeIdentifier:[TypeIdentifier stringByAppendingPathExtension:aKey]];
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

  nsAutoString imageStr;
  rv = aInput->GetImage(imageStr);
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

  return [self initWithKey:nsCocoaUtils::ToNSString(keyStr)
                     title:nsCocoaUtils::ToNSString(titleStr)
                     image:nsCocoaUtils::ToNSString(imageStr)
                      type:nsCocoaUtils::ToNSString(typeStr)
                  callback:callback
                     color:colorInt
                  disabled:(BOOL)disabled];
}

- (void)dealloc {
  [mKey release];
  [mTitle release];
  [mImage release];
  [mType release];
  [mColor release];
  [mNativeIdentifier release];
  [super dealloc];
}

@end
