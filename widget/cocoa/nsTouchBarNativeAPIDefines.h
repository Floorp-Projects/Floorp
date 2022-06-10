/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTouchBarNativeAPIDefines_h
#define nsTouchBarNativeAPIDefines_h

#import <Cocoa/Cocoa.h>

#if !defined(MAC_OS_X_VERSION_10_12_2) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12_2
@interface NSApplication (TouchBarMenu)
- (IBAction)toggleTouchBarCustomizationPalette:(id)sender;
@end

typedef NSString* NSTouchBarItemIdentifier;
__attribute__((weak_import))
@interface NSTouchBarItem : NSObject
@property(readonly) NSView* view;
@property(readonly) NSString* customizationLabel;
- (instancetype)initWithIdentifier:(NSTouchBarItemIdentifier)aIdentifier;
@end

@protocol NSSharingServicePickerTouchBarItemDelegate
@end

__attribute__((weak_import))
@interface NSSharingServicePickerTouchBarItem : NSTouchBarItem
@property(strong) id<NSSharingServicePickerTouchBarItemDelegate> delegate;
@property(strong) NSImage* buttonImage;
@end

__attribute__((weak_import))
@interface NSCustomTouchBarItem : NSTouchBarItem
@property(strong) NSView* view;
@property(strong) NSString* customizationLabel;
@end

@protocol NSTouchBarDelegate
@end

typedef NSString* NSTouchBarCustomizationIdentifier;
__attribute__((weak_import))
@interface NSTouchBar : NSObject
@property(strong) NSArray<NSTouchBarItemIdentifier>* defaultItemIdentifiers;
@property(strong) id<NSTouchBarDelegate> delegate;
@property(strong) NSTouchBarCustomizationIdentifier customizationIdentifier;
@property(strong) NSArray<NSTouchBarItemIdentifier>* customizationAllowedItemIdentifiers;
- (NSTouchBarItem*)itemForIdentifier:(NSTouchBarItemIdentifier)aIdentifier;
@end

__attribute__((weak_import))
@interface NSPopoverTouchBarItem : NSTouchBarItem
@property(strong) NSString* customizationLabel;
@property(strong) NSView* collapsedRepresentation;
@property(strong) NSImage* collapsedRepresentationImage;
@property(strong) NSString* collapsedRepresentationLabel;
@property(strong) NSTouchBar* popoverTouchBar;
@property BOOL showsCloseButton;
- (void)showPopover:(id)sender;
- (void)dismissPopover:(id)sender;
@end

@interface NSButton (TouchBarButton)
@property(strong) NSColor* bezelColor;
@end
#endif  // !defined(MAC_OS_X_VERSION_10_12_2)
#endif  // nsTouchBarNativeAPIDefines_h
