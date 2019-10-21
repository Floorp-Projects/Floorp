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

static const NSTouchBarItemIdentifier BaseIdentifier = @"com.mozilla.firefox.touchbar";

// Non-JS scrubber implemention for the Share Scrubber,
// since it is defined by an Apple API.
static NSTouchBarItemIdentifier ShareScrubberIdentifier =
    [TouchBarInput nativeIdentifierWithType:@"scrubber" withKey:@"share"];

// The search popover needs to show/hide depending on if the Urlbar is focused
// when it is created. We keep track of its identifier to accomodate this
// special handling.
static NSTouchBarItemIdentifier SearchPopoverIdentifier =
    [TouchBarInput nativeIdentifierWithType:@"popover" withKey:@"search-popover"];

// Used to tie action strings to buttons.
static char sIdentifierAssociationKey;

static const NSArray<NSString*>* kAllowedInputTypes = @[
  @"button",
  @"mainButton",
  @"scrubber",
  @"popover",
  @"scrollView",
  @"label",
];

// The default space between inputs, used where layout is not automatic.
static const uint32_t kInputSpacing = 8;
// The width of buttons in Apple's Share ScrollView. We use this in our
// ScrollViews to give them a native appearance.
static const uint32_t kScrollViewButtonWidth = 144;
static const uint32_t kInputIconSize = 16;

// The system default width for Touch Bar inputs is 128px. This is double.
#define MAIN_BUTTON_WIDTH 256

#pragma mark - NSTouchBarDelegate

- (instancetype)init {
  return [self initWithInputs:nil];
}

- (instancetype)initWithInputs:(NSMutableArray<TouchBarInput*>*)aInputs {
  if ((self = [super init])) {
    mTouchBarHelper = do_GetService(NS_TOUCHBARHELPER_CID);
    if (!mTouchBarHelper) {
      NS_ERROR("Unable to create Touch Bar Helper.");
      return nil;
    }

    self.delegate = self;
    self.mappedLayoutItems = [NSMutableDictionary dictionary];
    self.customizationAllowedItemIdentifiers = @[];

    if (!aInputs) {
      // This customization identifier is how users' custom layouts are saved by macOS.
      // If this changes, all users' layouts would be reset to the default layout.
      self.customizationIdentifier = [BaseIdentifier stringByAppendingPathExtension:@"defaultbar"];
      nsCOMPtr<nsIArray> allItems;

      nsresult rv = mTouchBarHelper->GetAllItems(getter_AddRefs(allItems));
      if (NS_FAILED(rv) || !allItems) {
        return nil;
      }

      uint32_t itemCount = 0;
      allItems->GetLength(&itemCount);
      // This is copied to self.customizationAllowedItemIdentifiers.
      // Required since [self.mappedItems allKeys] does not preserve order.
      // One slot is added for the spacer item.
      NSMutableArray* orderedIdentifiers = [NSMutableArray arrayWithCapacity:itemCount + 1];
      for (uint32_t i = 0; i < itemCount; ++i) {
        nsCOMPtr<nsITouchBarInput> input = do_QueryElementAt(allItems, i);
        if (!input) {
          continue;
        }

        TouchBarInput* convertedInput = [[TouchBarInput alloc] initWithXPCOM:input];

        // Add new input to dictionary for lookup of properties in delegate.
        self.mappedLayoutItems[[convertedInput nativeIdentifier]] = convertedInput;
        orderedIdentifiers[i] = [convertedInput nativeIdentifier];
      }
      [orderedIdentifiers addObject:@"NSTouchBarItemIdentifierFlexibleSpace"];
      self.customizationAllowedItemIdentifiers = [orderedIdentifiers copy];

      NSArray* defaultItemIdentifiers = @[
        [TouchBarInput nativeIdentifierWithType:@"button" withKey:@"back"],
        [TouchBarInput nativeIdentifierWithType:@"button" withKey:@"forward"],
        [TouchBarInput nativeIdentifierWithType:@"button" withKey:@"reload"],
        [TouchBarInput nativeIdentifierWithType:@"mainButton" withKey:@"open-location"],
        [TouchBarInput nativeIdentifierWithType:@"button" withKey:@"new-tab"],
        ShareScrubberIdentifier, SearchPopoverIdentifier
      ];
      self.defaultItemIdentifiers = [defaultItemIdentifiers copy];
    } else {
      NSMutableArray* defaultItemIdentifiers = [NSMutableArray arrayWithCapacity:[aInputs count]];
      for (TouchBarInput* input in aInputs) {
        self.mappedLayoutItems[[input nativeIdentifier]] = input;
        [defaultItemIdentifiers addObject:[input nativeIdentifier]];
      }
      self.defaultItemIdentifiers = [defaultItemIdentifiers copy];
    }
  }

  return self;
}

- (void)dealloc {
  for (NSTouchBarItemIdentifier identifier in self.mappedLayoutItems) {
    NSTouchBarItem* item = [self itemForIdentifier:identifier];
    if (!item) {
      continue;
    }
    if ([item isKindOfClass:[NSPopoverTouchBarItem class]]) {
      [(NSPopoverTouchBarItem*)item setCollapsedRepresentationImage:nil];
      [(nsTouchBar*)[(NSPopoverTouchBarItem*)item popoverTouchBar] release];
    } else if ([[item view] isKindOfClass:[NSScrollView class]]) {
      [[(NSScrollView*)[item view] documentView] release];
      [(NSScrollView*)[item view] release];
    }

    [item release];
  }

  [self.defaultItemIdentifiers release];
  [self.customizationAllowedItemIdentifiers release];
  [self.scrollViewButtons removeAllObjects];
  [self.scrollViewButtons release];
  [self.mappedLayoutItems removeAllObjects];
  [self.mappedLayoutItems release];
  [super dealloc];
}

- (NSTouchBarItem*)touchBar:(NSTouchBar*)aTouchBar
      makeItemForIdentifier:(NSTouchBarItemIdentifier)aIdentifier {
  TouchBarInput* input = self.mappedLayoutItems[aIdentifier];

  if (!input) {
    return nil;
  }

  // Checking to see if our new item is of an accepted type.
  if (![kAllowedInputTypes
          containsObject:[[[input type] componentsSeparatedByString:@"-"] lastObject]]) {
    return nil;
  }

  if ([[input type] hasSuffix:@"scrubber"]) {
    // We check the identifier rather than the type here as a special case.
    if (![aIdentifier isEqualToString:ShareScrubberIdentifier]) {
      // We're only supporting the Share scrubber for now.
      return nil;
    }
    return [self makeShareScrubberForIdentifier:aIdentifier];
  }

  if ([[input type] hasSuffix:@"popover"]) {
    NSPopoverTouchBarItem* newPopoverItem =
        [[NSPopoverTouchBarItem alloc] initWithIdentifier:aIdentifier];
    // We initialize popoverTouchBar here because we only allow setting this
    // property on popover creation. Updating popoverTouchBar for every update
    // of the popover item would be very expensive.
    newPopoverItem.popoverTouchBar = [[nsTouchBar alloc] initWithInputs:[input children]];
    [self updatePopover:newPopoverItem input:input];
    return newPopoverItem;
  }

  // Our new item, which will be initialized depending on aIdentifier.
  NSCustomTouchBarItem* newItem = [[NSCustomTouchBarItem alloc] initWithIdentifier:aIdentifier];

  if ([[input type] hasSuffix:@"scrollView"]) {
    [self updateScrollView:newItem input:input];
    return newItem;
  } else if ([[input type] hasSuffix:@"label"]) {
    NSTextField* label = [NSTextField labelWithString:@""];
    [self updateLabel:label input:input];
    newItem.view = label;
    return newItem;
  }

  // The cases of a button or main button require the same setup.
  NSButton* button = [NSButton buttonWithTitle:@"" target:self action:@selector(touchBarAction:)];
  newItem.view = button;

  if ([[input type] hasSuffix:@"button"] && ![[input type] hasPrefix:@"scrollView"]) {
    [self updateButton:button input:input];
  } else if ([[input type] hasSuffix:@"mainButton"]) {
    [self updateMainButton:button input:input];
  }
  return newItem;
}

- (bool)updateItem:(TouchBarInput*)aInput {
  NSTouchBarItem* item = [self itemForIdentifier:[aInput nativeIdentifier]];
  // If we can't immediately find item, there are three possibilities:
  //   * It is a button in a ScrollView, which can't be found with itemForIdentifier; or
  //   * It is contained within a popover; or
  //   * It simply does not exist.
  // We check for each possibility here.
  if (!item) {
    if ([self maybeUpdateScrollViewChild:aInput]) {
      [self replaceMappedLayoutItem:aInput];
      return true;
    }
    if ([self maybeUpdatePopoverChild:aInput]) {
      [self replaceMappedLayoutItem:aInput];
      return true;
    }
    return false;
  }

  if ([[aInput type] hasSuffix:@"button"]) {
    [(NSCustomTouchBarItem*)item setCustomizationLabel:[aInput title]];
    [self updateButton:(NSButton*)item.view input:aInput];
  } else if ([[aInput type] hasSuffix:@"mainButton"]) {
    [(NSCustomTouchBarItem*)item setCustomizationLabel:[aInput title]];
    [self updateMainButton:(NSButton*)item.view input:aInput];
  } else if ([[aInput type] hasSuffix:@"scrollView"]) {
    [(NSCustomTouchBarItem*)item setCustomizationLabel:[aInput title]];
    [self updateScrollView:(NSCustomTouchBarItem*)item input:aInput];
  } else if ([[aInput type] hasSuffix:@"popover"]) {
    [(NSPopoverTouchBarItem*)item setCustomizationLabel:[aInput title]];
    [self updatePopover:(NSPopoverTouchBarItem*)item input:aInput];
  } else if ([[aInput type] hasSuffix:@"label"]) {
    [self updateLabel:(NSTextField*)item.view input:aInput];
  }

  [self replaceMappedLayoutItem:aInput];
  return true;
}

- (bool)maybeUpdatePopoverChild:(TouchBarInput*)aInput {
  for (NSTouchBarItemIdentifier identifier in self.mappedLayoutItems) {
    TouchBarInput* potentialPopover = self.mappedLayoutItems[identifier];
    if (![[potentialPopover type] hasSuffix:@"popover"]) {
      continue;
    }
    NSTouchBarItem* popover = [self itemForIdentifier:[potentialPopover nativeIdentifier]];
    if (popover) {
      if ([(nsTouchBar*)[(NSPopoverTouchBarItem*)popover popoverTouchBar] updateItem:aInput]) {
        return true;
      }
    }
  }
  return false;
}

- (bool)maybeUpdateScrollViewChild:(TouchBarInput*)aInput {
  NSButton* scrollViewButton = self.scrollViewButtons[[aInput nativeIdentifier]];
  if (scrollViewButton) {
    // ScrollView buttons are similar to mainButtons except for their width.
    [self updateMainButton:scrollViewButton input:aInput];
    uint32_t buttonSize =
        MAX(scrollViewButton.attributedTitle.size.width + kInputIconSize + kInputSpacing,
            kScrollViewButtonWidth);
    [[scrollViewButton widthAnchor] constraintGreaterThanOrEqualToConstant:buttonSize].active = YES;
  }
  // Updating the TouchBarInput* in the ScrollView's mChildren array.
  for (NSTouchBarItemIdentifier identifier in self.mappedLayoutItems) {
    TouchBarInput* potentialScrollView = self.mappedLayoutItems[identifier];
    if (![[potentialScrollView type] hasSuffix:@"scrollView"]) {
      continue;
    }
    for (uint32_t i = 0; i < [[potentialScrollView children] count]; ++i) {
      TouchBarInput* child = [potentialScrollView children][i];
      if (![[child nativeIdentifier] isEqualToString:[aInput nativeIdentifier]]) {
        continue;
      }
      [[potentialScrollView children] replaceObjectAtIndex:i withObject:aInput];
      [child release];
      return true;
    }
  }
  return false;
}

- (void)replaceMappedLayoutItem:(TouchBarInput*)aItem {
  [self.mappedLayoutItems[[aItem nativeIdentifier]] release];
  self.mappedLayoutItems[[aItem nativeIdentifier]] = aItem;
}

- (void)updateButton:(NSButton*)aButton input:(TouchBarInput*)aInput {
  if (!aButton || !aInput) {
    return;
  }

  aButton.title = [aInput title];
  if (![aInput isIconPositionSet]) {
    [aButton setImagePosition:NSImageOnly];
    [aInput setIconPositionSet:true];
  }

  if ([aInput imageURI]) {
    RefPtr<nsTouchBarInputIcon> icon = [aInput icon];
    if (!icon) {
      icon = new nsTouchBarInputIcon([aInput document], aButton);
      [aInput setIcon:icon];
    }
    icon->SetupIcon([aInput imageURI]);
  }
  [aButton setEnabled:![aInput isDisabled]];

  if ([aInput color]) {
    aButton.bezelColor = [aInput color];
  }

  objc_setAssociatedObject(aButton, &sIdentifierAssociationKey, [aInput nativeIdentifier],
                           OBJC_ASSOCIATION_RETAIN);
}

- (void)updateMainButton:(NSButton*)aMainButton input:(TouchBarInput*)aInput {
  if (!aMainButton || !aInput) {
    return;
  }
  // If empty, string is still being localized. Display a blank input instead.
  if ([[aInput title] isEqualToString:@""]) {
    [aMainButton setImagePosition:NSNoImage];
  } else {
    [aMainButton setImagePosition:NSImageLeft];
  }
  aMainButton.imageHugsTitle = YES;
  [aInput setIconPositionSet:true];

  [self updateButton:aMainButton input:aInput];
  [aMainButton.widthAnchor constraintGreaterThanOrEqualToConstant:MAIN_BUTTON_WIDTH].active = YES;
  [aMainButton setContentHuggingPriority:1.0
                          forOrientation:NSLayoutConstraintOrientationHorizontal];
}

- (void)updatePopover:(NSPopoverTouchBarItem*)aPopoverItem input:(TouchBarInput*)aInput {
  if (!aPopoverItem || !aInput) {
    return;
  }
  aPopoverItem.showsCloseButton = YES;
  if ([aInput imageURI]) {
    RefPtr<nsTouchBarInputIcon> icon = [aInput icon];
    if (!icon) {
      icon = new nsTouchBarInputIcon([aInput document], nil, nil, aPopoverItem);
      [aInput setIcon:icon];
    }
    icon->SetupIcon([aInput imageURI]);
  } else if ([aInput title]) {
    aPopoverItem.collapsedRepresentationLabel = [aInput title];
  }

  // Special handling to show/hide the search popover if the Urlbar is focused.
  if ([[aInput nativeIdentifier] isEqualToString:SearchPopoverIdentifier]) {
    // We can reach this code during window shutdown. We only want to toggle
    // showPopover if we are in a normal running state.
    if (!mTouchBarHelper) {
      return;
    }
    bool urlbarIsFocused = false;
    mTouchBarHelper->GetIsUrlbarFocused(&urlbarIsFocused);
    if (urlbarIsFocused) {
      [aPopoverItem showPopover:self];
    }
  }
}

- (void)updateScrollView:(NSCustomTouchBarItem*)aScrollViewItem input:(TouchBarInput*)aInput {
  if (!aScrollViewItem || ![aInput children]) {
    return;
  }
  NSMutableDictionary* constraintViews = [NSMutableDictionary dictionary];
  NSView* documentView = [[NSView alloc] initWithFrame:NSZeroRect];
  NSString* layoutFormat = @"H:|-8-";
  NSSize size = NSMakeSize(kInputSpacing, 30);
  // Layout strings allow only alphanumeric characters. We will use this
  // NSCharacterSet to strip illegal characters.
  NSCharacterSet* charactersToRemove = [[NSCharacterSet alphanumericCharacterSet] invertedSet];

  for (TouchBarInput* childInput in [aInput children]) {
    if (![[childInput type] hasSuffix:@"button"]) {
      continue;
    }
    NSButton* button = [NSButton buttonWithTitle:[childInput title]
                                          target:self
                                          action:@selector(touchBarAction:)];
    // ScrollView buttons are similar to mainButtons except for their width.
    [self updateMainButton:button input:childInput];
    uint32_t buttonSize = MAX(button.attributedTitle.size.width + kInputIconSize + kInputSpacing,
                              kScrollViewButtonWidth);
    [[button widthAnchor] constraintGreaterThanOrEqualToConstant:buttonSize].active = YES;

    self.scrollViewButtons[[childInput nativeIdentifier]] = button;

    button.translatesAutoresizingMaskIntoConstraints = NO;
    [documentView addSubview:button];
    NSString* layoutKey = [[[childInput nativeIdentifier]
        componentsSeparatedByCharactersInSet:charactersToRemove] componentsJoinedByString:@""];

    // Iteratively create our layout string.
    layoutFormat =
        [layoutFormat stringByAppendingString:[NSString stringWithFormat:@"[%@]-8-", layoutKey]];
    [constraintViews setObject:button forKey:layoutKey];
    size.width += kInputSpacing + buttonSize;
  }
  layoutFormat = [layoutFormat stringByAppendingString:[NSString stringWithFormat:@"|"]];
  NSArray* hConstraints =
      [NSLayoutConstraint constraintsWithVisualFormat:layoutFormat
                                              options:NSLayoutFormatAlignAllCenterY
                                              metrics:nil
                                                views:constraintViews];
  NSScrollView* scrollView =
      [[NSScrollView alloc] initWithFrame:CGRectMake(0, 0, size.width, size.height)];
  [documentView setFrame:NSMakeRect(0, 0, size.width, size.height)];
  [NSLayoutConstraint activateConstraints:hConstraints];
  scrollView.documentView = documentView;

  aScrollViewItem.view = scrollView;
}

- (void)updateLabel:(NSTextField*)aLabel input:aInput {
  if (!aLabel || ![aInput title]) {
    return;
  }
  [aLabel setStringValue:[aInput title]];
}

- (NSTouchBarItem*)makeShareScrubberForIdentifier:(NSTouchBarItemIdentifier)aIdentifier {
  TouchBarInput* input = self.mappedLayoutItems[aIdentifier];
  // System-default share menu
  NSSharingServicePickerTouchBarItem* servicesItem =
      [[NSSharingServicePickerTouchBarItem alloc] initWithIdentifier:aIdentifier];

  // buttonImage needs to be set to nil while we wait for our icon to load.
  // Otherwise, the default Apple share icon is automatically loaded.
  servicesItem.buttonImage = nil;
  if ([input imageURI]) {
    RefPtr<nsTouchBarInputIcon> icon = [input icon];
    if (!icon) {
      icon = new nsTouchBarInputIcon([input document], nil, servicesItem);
      [input setIcon:icon];
    }
    icon->SetupIcon([input imageURI]);
  }

  servicesItem.delegate = self;
  return servicesItem;
}

- (void)showPopover:(TouchBarInput*)aPopover showing:(bool)aShowing {
  if (!aPopover) {
    return;
  }
  NSPopoverTouchBarItem* popoverItem =
      (NSPopoverTouchBarItem*)[self itemForIdentifier:[aPopover nativeIdentifier]];
  if (!popoverItem) {
    return;
  }
  if (aShowing) {
    [popoverItem showPopover:self];
  } else {
    [popoverItem dismissPopover:self];
  }
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
  if (!callback) {
    NSLog(@"Touch Bar action attempted with no valid callback! Identifier: %@",
          [input nativeIdentifier]);
    return;
  }
  callback->OnCommand();
}

- (void)releaseJSObjects {
  mTouchBarHelper = nil;

  for (NSTouchBarItemIdentifier identifier in self.mappedLayoutItems) {
    TouchBarInput* input = self.mappedLayoutItems[identifier];
    if (!input) {
      continue;
    }

    // Childless popovers contain the default Touch Bar as its popoverTouchBar.
    // We check for [input children] since the default Touch Bar contains a
    // popover (search-popover), so this would infinitely loop if there was no check.
    if ([[input type] hasSuffix:@"popover"] && [input children]) {
      NSTouchBarItem* item = [self itemForIdentifier:identifier];
      [(nsTouchBar*)[(NSPopoverTouchBarItem*)item popoverTouchBar] releaseJSObjects];
    }

    [input setCallback:nil];
    [input setDocument:nil];
    [input setImageURI:nil];

    [input releaseJSObjects];
  }
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
  mCallback = nil;
  mImageURI = nil;
  mDocument = nil;
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

@end
