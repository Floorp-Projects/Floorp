/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTouchBar.h"

#include <objc/runtime.h>

#include "mozilla/MacStringHelpers.h"
#include "mozilla/dom/Document.h"
#include "nsArrayUtils.h"
#include "nsCocoaUtils.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIArray.h"
#include "nsTouchBarInputIcon.h"
#include "nsWidgetsCID.h"

@implementation nsTouchBar

// Used to tie action strings to buttons.
static char sIdentifierAssociationKey;

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
      // This customization identifier is how users' custom layouts are saved by
      // macOS. If this changes, all users' layouts would be reset to the
      // default layout.
      self.customizationIdentifier = [kTouchBarBaseIdentifier
          stringByAppendingPathExtension:@"defaultbar"];
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
      NSMutableArray* orderedIdentifiers =
          [NSMutableArray arrayWithCapacity:itemCount + 1];
      for (uint32_t i = 0; i < itemCount; ++i) {
        nsCOMPtr<nsITouchBarInput> input = do_QueryElementAt(allItems, i);
        if (!input) {
          continue;
        }

        TouchBarInput* convertedInput;
        NSTouchBarItemIdentifier newInputIdentifier =
            [TouchBarInput nativeIdentifierWithXPCOM:input];
        if (!newInputIdentifier) {
          continue;
        }

        // If there is already an input in mappedLayoutItems with this
        // identifier, that means updateItem fired before this initialization.
        // The input cached by updateItem is more current, so we should use that
        // one.
        if (self.mappedLayoutItems[newInputIdentifier]) {
          convertedInput = self.mappedLayoutItems[newInputIdentifier];
        } else {
          convertedInput = [[TouchBarInput alloc] initWithXPCOM:input];
          // Add new input to dictionary for lookup of properties in delegate.
          self.mappedLayoutItems[[convertedInput nativeIdentifier]] =
              convertedInput;
        }

        orderedIdentifiers[i] = [convertedInput nativeIdentifier];
      }
      [orderedIdentifiers addObject:@"NSTouchBarItemIdentifierFlexibleSpace"];
      self.customizationAllowedItemIdentifiers = [orderedIdentifiers copy];

      NSArray* defaultItemIdentifiers = @[
        [TouchBarInput nativeIdentifierWithType:@"button" withKey:@"back"],
        [TouchBarInput nativeIdentifierWithType:@"button" withKey:@"forward"],
        [TouchBarInput nativeIdentifierWithType:@"button" withKey:@"reload"],
        [TouchBarInput nativeIdentifierWithType:@"mainButton"
                                        withKey:@"open-location"],
        [TouchBarInput nativeIdentifierWithType:@"button" withKey:@"new-tab"],
        [TouchBarInput shareScrubberIdentifier],
        [TouchBarInput searchPopoverIdentifier]
      ];
      self.defaultItemIdentifiers = [defaultItemIdentifiers copy];
    } else {
      NSMutableArray* defaultItemIdentifiers =
          [NSMutableArray arrayWithCapacity:[aInputs count]];
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
  if (!mTouchBarHelper) {
    return nil;
  }

  TouchBarInput* input = self.mappedLayoutItems[aIdentifier];
  if (!input) {
    return nil;
  }

  if ([input baseType] == TouchBarInputBaseType::kScrubber) {
    // We check the identifier rather than the baseType here as a special case.
    if (![aIdentifier
            isEqualToString:[TouchBarInput shareScrubberIdentifier]]) {
      // We're only supporting the Share scrubber for now.
      return nil;
    }
    return [self makeShareScrubberForIdentifier:aIdentifier];
  }

  if ([input baseType] == TouchBarInputBaseType::kPopover) {
    NSPopoverTouchBarItem* newPopoverItem =
        [[NSPopoverTouchBarItem alloc] initWithIdentifier:aIdentifier];
    [newPopoverItem setCustomizationLabel:[input title]];
    // We initialize popoverTouchBar here because we only allow setting this
    // property on popover creation. Updating popoverTouchBar for every update
    // of the popover item would be very expensive.
    newPopoverItem.popoverTouchBar =
        [[nsTouchBar alloc] initWithInputs:[input children]];
    [self updatePopover:newPopoverItem withIdentifier:[input nativeIdentifier]];
    return newPopoverItem;
  }

  // Our new item, which will be initialized depending on aIdentifier.
  NSCustomTouchBarItem* newItem =
      [[NSCustomTouchBarItem alloc] initWithIdentifier:aIdentifier];
  [newItem setCustomizationLabel:[input title]];

  if ([input baseType] == TouchBarInputBaseType::kScrollView) {
    [self updateScrollView:newItem withIdentifier:[input nativeIdentifier]];
    return newItem;
  } else if ([input baseType] == TouchBarInputBaseType::kLabel) {
    NSTextField* label = [NSTextField labelWithString:@""];
    [self updateLabel:label withIdentifier:[input nativeIdentifier]];
    newItem.view = label;
    return newItem;
  }

  // The cases of a button or main button require the same setup.
  NSButton* button = [NSButton buttonWithTitle:@""
                                        target:self
                                        action:@selector(touchBarAction:)];
  newItem.view = button;

  if ([input baseType] == TouchBarInputBaseType::kButton &&
      ![[input type] hasPrefix:@"scrollView"]) {
    [self updateButton:newItem withIdentifier:[input nativeIdentifier]];
  } else if ([input baseType] == TouchBarInputBaseType::kMainButton) {
    [self updateMainButton:newItem withIdentifier:[input nativeIdentifier]];
  }
  return newItem;
}

- (bool)updateItem:(TouchBarInput*)aInput {
  if (!mTouchBarHelper) {
    return false;
  }

  NSTouchBarItem* item = [self itemForIdentifier:[aInput nativeIdentifier]];

  // If we can't immediately find item, there are three possibilities:
  //   * It is a button in a ScrollView, or
  //   * It is contained within a popover, or
  //   * It simply does not exist.
  // We check for each possibility here.
  if (!self.mappedLayoutItems[[aInput nativeIdentifier]]) {
    if ([self maybeUpdateScrollViewChild:aInput]) {
      return true;
    }
    if ([self maybeUpdatePopoverChild:aInput]) {
      return true;
    }
    return false;
  }

  // Update our canonical copy of the input.
  [self replaceMappedLayoutItem:aInput];

  if ([aInput baseType] == TouchBarInputBaseType::kButton) {
    [(NSCustomTouchBarItem*)item setCustomizationLabel:[aInput title]];
    [self updateButton:(NSCustomTouchBarItem*)item
        withIdentifier:[aInput nativeIdentifier]];
  } else if ([aInput baseType] == TouchBarInputBaseType::kMainButton) {
    [(NSCustomTouchBarItem*)item setCustomizationLabel:[aInput title]];
    [self updateMainButton:(NSCustomTouchBarItem*)item
            withIdentifier:[aInput nativeIdentifier]];
  } else if ([aInput baseType] == TouchBarInputBaseType::kScrollView) {
    [(NSCustomTouchBarItem*)item setCustomizationLabel:[aInput title]];
    [self updateScrollView:(NSCustomTouchBarItem*)item
            withIdentifier:[aInput nativeIdentifier]];
  } else if ([aInput baseType] == TouchBarInputBaseType::kPopover) {
    [(NSPopoverTouchBarItem*)item setCustomizationLabel:[aInput title]];
    [self updatePopover:(NSPopoverTouchBarItem*)item
         withIdentifier:[aInput nativeIdentifier]];
    for (TouchBarInput* child in [aInput children]) {
      [(nsTouchBar*)[(NSPopoverTouchBarItem*)item popoverTouchBar]
          updateItem:child];
    }
  } else if ([aInput baseType] == TouchBarInputBaseType::kLabel) {
    [self updateLabel:(NSTextField*)item.view
        withIdentifier:[aInput nativeIdentifier]];
  }

  return true;
}

- (bool)maybeUpdatePopoverChild:(TouchBarInput*)aInput {
  for (NSTouchBarItemIdentifier identifier in self.mappedLayoutItems) {
    TouchBarInput* potentialPopover = self.mappedLayoutItems[identifier];
    if ([potentialPopover baseType] != TouchBarInputBaseType::kPopover) {
      continue;
    }
    NSTouchBarItem* popover =
        [self itemForIdentifier:[potentialPopover nativeIdentifier]];
    if (popover) {
      if ([(nsTouchBar*)[(NSPopoverTouchBarItem*)popover popoverTouchBar]
              updateItem:aInput]) {
        return true;
      }
    }
  }
  return false;
}

- (bool)maybeUpdateScrollViewChild:(TouchBarInput*)aInput {
  NSCustomTouchBarItem* scrollViewButton =
      self.scrollViewButtons[[aInput nativeIdentifier]];
  if (scrollViewButton) {
    // ScrollView buttons are similar to mainButtons except for their width.
    [self updateMainButton:scrollViewButton
            withIdentifier:[aInput nativeIdentifier]];
    NSButton* button = (NSButton*)scrollViewButton.view;
    uint32_t buttonSize =
        MAX(button.attributedTitle.size.width + kInputIconSize + kInputSpacing,
            kScrollViewButtonWidth);
    [[button widthAnchor] constraintGreaterThanOrEqualToConstant:buttonSize]
        .active = YES;
  }
  // Updating the TouchBarInput* in the ScrollView's mChildren array.
  for (NSTouchBarItemIdentifier identifier in self.mappedLayoutItems) {
    TouchBarInput* potentialScrollView = self.mappedLayoutItems[identifier];
    if ([potentialScrollView baseType] != TouchBarInputBaseType::kScrollView) {
      continue;
    }
    for (uint32_t i = 0; i < [[potentialScrollView children] count]; ++i) {
      TouchBarInput* child = [potentialScrollView children][i];
      if (![[child nativeIdentifier]
              isEqualToString:[aInput nativeIdentifier]]) {
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

- (void)updateButton:(NSCustomTouchBarItem*)aButton
      withIdentifier:(NSTouchBarItemIdentifier)aIdentifier {
  if (!aButton || !aIdentifier) {
    return;
  }

  TouchBarInput* input = self.mappedLayoutItems[aIdentifier];
  if (!input) {
    return;
  }

  NSButton* button = (NSButton*)[aButton view];
  button.title = [input title];
  if ([input imageURI]) {
    [button setImagePosition:NSImageOnly];
    [self loadIconForInput:input forItem:aButton];
    // Because we are hiding the title, NSAccessibility also does not get it.
    // Therefore, set an accessibility label as alternative text for image-only
    // buttons.
    [button setAccessibilityLabel:[input title]];
  }

  [button setEnabled:![input isDisabled]];
  if ([input color]) {
    button.bezelColor = [input color];
  }

  objc_setAssociatedObject(button, &sIdentifierAssociationKey, aIdentifier,
                           OBJC_ASSOCIATION_RETAIN);
}

- (void)updateMainButton:(NSCustomTouchBarItem*)aMainButton
          withIdentifier:(NSTouchBarItemIdentifier)aIdentifier {
  if (!aMainButton || !aIdentifier) {
    return;
  }

  TouchBarInput* input = self.mappedLayoutItems[aIdentifier];
  if (!input) {
    return;
  }

  [self updateButton:aMainButton withIdentifier:aIdentifier];
  NSButton* button = (NSButton*)[aMainButton view];

  // If empty, string is still being localized. Display a blank input instead.
  if ([[input title] isEqualToString:@""]) {
    [button setImagePosition:NSNoImage];
  } else {
    [button setImagePosition:NSImageLeft];
  }
  button.imageHugsTitle = YES;
  [button.widthAnchor constraintGreaterThanOrEqualToConstant:MAIN_BUTTON_WIDTH]
      .active = YES;
  [button setContentHuggingPriority:1.0
                     forOrientation:NSLayoutConstraintOrientationHorizontal];
}

- (void)updatePopover:(NSPopoverTouchBarItem*)aPopoverItem
       withIdentifier:(NSTouchBarItemIdentifier)aIdentifier {
  if (!aPopoverItem || !aIdentifier) {
    return;
  }

  TouchBarInput* input = self.mappedLayoutItems[aIdentifier];
  if (!input) {
    return;
  }

  aPopoverItem.showsCloseButton = YES;
  if ([input imageURI]) {
    [self loadIconForInput:input forItem:aPopoverItem];
  } else if ([input title]) {
    aPopoverItem.collapsedRepresentationLabel = [input title];
  }

  // Special handling to show/hide the search popover if the Urlbar is focused.
  if ([[input nativeIdentifier]
          isEqualToString:[TouchBarInput searchPopoverIdentifier]]) {
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

- (void)updateScrollView:(NSCustomTouchBarItem*)aScrollViewItem
          withIdentifier:(NSTouchBarItemIdentifier)aIdentifier {
  if (!aScrollViewItem || !aIdentifier) {
    return;
  }

  TouchBarInput* input = self.mappedLayoutItems[aIdentifier];
  if (!input || ![input children]) {
    return;
  }

  NSMutableDictionary* constraintViews = [NSMutableDictionary dictionary];
  NSView* documentView = [[NSView alloc] initWithFrame:NSZeroRect];
  NSString* layoutFormat = @"H:|-8-";
  NSSize size = NSMakeSize(kInputSpacing, 30);
  // Layout strings allow only alphanumeric characters. We will use this
  // NSCharacterSet to strip illegal characters.
  NSCharacterSet* charactersToRemove =
      [[NSCharacterSet alphanumericCharacterSet] invertedSet];

  for (TouchBarInput* childInput in [input children]) {
    if ([childInput baseType] != TouchBarInputBaseType::kButton) {
      continue;
    }
    [self replaceMappedLayoutItem:childInput];
    NSCustomTouchBarItem* newItem = [[NSCustomTouchBarItem alloc]
        initWithIdentifier:[childInput nativeIdentifier]];
    NSButton* button = [NSButton buttonWithTitle:[childInput title]
                                          target:self
                                          action:@selector(touchBarAction:)];
    newItem.view = button;
    // ScrollView buttons are similar to mainButtons except for their width.
    [self updateMainButton:newItem
            withIdentifier:[childInput nativeIdentifier]];
    uint32_t buttonSize =
        MAX(button.attributedTitle.size.width + kInputIconSize + kInputSpacing,
            kScrollViewButtonWidth);
    [[button widthAnchor] constraintGreaterThanOrEqualToConstant:buttonSize]
        .active = YES;

    NSCustomTouchBarItem* tempItem =
        self.scrollViewButtons[[childInput nativeIdentifier]];
    self.scrollViewButtons[[childInput nativeIdentifier]] = newItem;
    [tempItem release];

    button.translatesAutoresizingMaskIntoConstraints = NO;
    [documentView addSubview:button];
    NSString* layoutKey = [[[childInput nativeIdentifier]
        componentsSeparatedByCharactersInSet:charactersToRemove]
        componentsJoinedByString:@""];

    // Iteratively create our layout string.
    layoutFormat = [layoutFormat
        stringByAppendingString:[NSString
                                    stringWithFormat:@"[%@]-8-", layoutKey]];
    [constraintViews setObject:button forKey:layoutKey];
    size.width += kInputSpacing + buttonSize;
  }
  layoutFormat =
      [layoutFormat stringByAppendingString:[NSString stringWithFormat:@"|"]];
  NSArray* hConstraints = [NSLayoutConstraint
      constraintsWithVisualFormat:layoutFormat
                          options:NSLayoutFormatAlignAllCenterY
                          metrics:nil
                            views:constraintViews];
  NSScrollView* scrollView = [[NSScrollView alloc]
      initWithFrame:CGRectMake(0, 0, size.width, size.height)];
  [documentView setFrame:NSMakeRect(0, 0, size.width, size.height)];
  [NSLayoutConstraint activateConstraints:hConstraints];
  scrollView.documentView = documentView;

  aScrollViewItem.view = scrollView;
}

- (void)updateLabel:(NSTextField*)aLabel
     withIdentifier:(NSTouchBarItemIdentifier)aIdentifier {
  if (!aLabel || !aIdentifier) {
    return;
  }

  TouchBarInput* input = self.mappedLayoutItems[aIdentifier];
  if (!input || ![input title]) {
    return;
  }
  [aLabel setStringValue:[input title]];
}

- (NSTouchBarItem*)makeShareScrubberForIdentifier:
    (NSTouchBarItemIdentifier)aIdentifier {
  TouchBarInput* input = self.mappedLayoutItems[aIdentifier];
  // System-default share menu
  NSSharingServicePickerTouchBarItem* servicesItem =
      [[NSSharingServicePickerTouchBarItem alloc]
          initWithIdentifier:aIdentifier];

  // buttonImage needs to be set to nil while we wait for our icon to load.
  // Otherwise, the default Apple share icon is automatically loaded.
  servicesItem.buttonImage = nil;

  [self loadIconForInput:input forItem:servicesItem];

  servicesItem.delegate = self;
  return servicesItem;
}

- (void)showPopover:(TouchBarInput*)aPopover showing:(bool)aShowing {
  if (!aPopover) {
    return;
  }
  NSPopoverTouchBarItem* popoverItem = (NSPopoverTouchBarItem*)[self
      itemForIdentifier:[aPopover nativeIdentifier]];
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

- (void)loadIconForInput:(TouchBarInput*)aInput forItem:(NSTouchBarItem*)aItem {
  if (!aInput || ![aInput imageURI] || !aItem || !mTouchBarHelper) {
    return;
  }

  RefPtr<nsTouchBarInputIcon> icon = [aInput icon];

  if (!icon) {
    RefPtr<Document> document;
    nsresult rv = mTouchBarHelper->GetDocument(getter_AddRefs(document));
    if (NS_FAILED(rv) || !document) {
      return;
    }
    icon = new nsTouchBarInputIcon(document, aInput, aItem);
    [aInput setIcon:icon];
  }
  icon->SetupIcon([aInput imageURI]);
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
    // popover (search-popover), so this would infinitely loop if there was no
    // check.
    if ([input baseType] == TouchBarInputBaseType::kPopover &&
        [input children]) {
      NSTouchBarItem* item = [self itemForIdentifier:identifier];
      [(nsTouchBar*)[(NSPopoverTouchBarItem*)item popoverTouchBar]
          releaseJSObjects];
    }

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

  return @[ urlToShare, titleToShare ];
}

- (NSArray<NSSharingService*>*)
       sharingServicePicker:(NSSharingServicePicker*)aSharingServicePicker
    sharingServicesForItems:(NSArray*)aItems
    proposedSharingServices:(NSArray<NSSharingService*>*)aProposedServices {
  // redundant services
  NSArray* excludedServices = @[
    @"com.apple.share.System.add-to-safari-reading-list",
  ];

  NSArray* sharingServices = [aProposedServices
      filteredArrayUsingPredicate:[NSPredicate
                                      predicateWithFormat:@"NOT (name IN %@)",
                                                          excludedServices]];

  return sharingServices;
}

@end
