/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTouchBar_h_
#define nsTouchBar_h_

#import <Cocoa/Cocoa.h>

#include "nsITouchBarHelper.h"
#include "nsITouchBarInput.h"
#include "nsTouchBarInputIcon.h"
#include "nsTouchBarNativeAPIDefines.h"

using namespace mozilla::dom;

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

/**
 * Our TouchBar is its own delegate. This is adequate for our purposes,
 * since the current implementation only defines Touch Bar buttons for the
 * main Firefox window. If modals and other windows were to have custom
 * Touch Bar views, each window would have to be a NSTouchBarDelegate so
 * they could define their own custom sets of buttons.
 */
@interface nsTouchBar : NSTouchBar <NSTouchBarDelegate,
                                    NSSharingServicePickerTouchBarItemDelegate,
                                    NSSharingServiceDelegate> {
  /**
   * Link to the frontend API that determines which buttons appear
   * in the Touch Bar
   */
  nsCOMPtr<nsITouchBarHelper> mTouchBarHelper;
}

/**
 * Contains TouchBarInput representations of the inputs currently in
 * the Touch Bar. Populated in `init` and updated by nsITouchBarUpdater.
 */
@property(strong) NSMutableDictionary<NSTouchBarItemIdentifier, TouchBarInput*>* mappedLayoutItems;

/**
 * Stores buttons displayed in a NSScrollView. They must be stored separately
 * because they are generic NSButtons and not NSTouchBarItems. As such, they
 * cannot be retrieved with [NSTouchBar itemForIdentifier].
 */
@property(strong) NSMutableDictionary<NSTouchBarItemIdentifier, NSButton*>* scrollViewButtons;

/**
 * Returns an instance of nsTouchBar based on implementation details
 * fetched from the frontend through nsTouchBarHelper.
 */
- (instancetype)init;

/**
 * If aInputs is not nil, a nsTouchBar containing the inputs specified is
 * initialized. Otherwise, a nsTouchBar is initialized containing a default set
 * of inputs.
 */
- (instancetype)initWithInputs:(NSMutableArray<TouchBarInput*>*)aInputs;

- (void)dealloc;

/**
 * Creates a new NSTouchBarItem and adds it to the Touch Bar.
 * Reads the passed identifier and creates the
 * appropriate item type (eg. NSCustomTouchBarItem).
 * Required as a member of NSTouchBarDelegate.
 */
- (NSTouchBarItem*)touchBar:(NSTouchBar*)aTouchBar
      makeItemForIdentifier:(NSTouchBarItemIdentifier)aIdentifier;

/**
 * Updates an input on the Touch Bar by redirecting to one of the specific
 * TouchBarItem types updaters.
 * Returns true if the input was successfully updated.
 */
- (bool)updateItem:(TouchBarInput*)aInput;

/**
 * Helper function for updateItem. Checks to see if a given input exists within
 * any of this Touch Bar's popovers and updates it if it exists.
 */
- (bool)maybeUpdatePopoverChild:(TouchBarInput*)aInput;

/**
 * Helper function for updateItem. Checks to see if a given input exists within
 * any of this Touch Bar's scroll views and updates it if it exists.
 */
- (bool)maybeUpdateScrollViewChild:(TouchBarInput*)aInput;

/**
 * Helper function for updateItem. Replaces an item in the
 * self.mappedLayoutItems dictionary.
 */
- (void)replaceMappedLayoutItem:(TouchBarInput*)aItem;

/**
 * Update or create various subclasses of TouchBarItem.
 */
- (void)updateButton:(NSButton*)aButton withIdentifier:(NSTouchBarItemIdentifier)aIdentifier;
- (void)updateMainButton:(NSButton*)aMainButton
          withIdentifier:(NSTouchBarItemIdentifier)aIdentifier;
- (void)updatePopover:(NSPopoverTouchBarItem*)aPopoverItem
       withIdentifier:(NSTouchBarItemIdentifier)aIdentifier;
- (void)updateScrollView:(NSCustomTouchBarItem*)aScrollViewItem
          withIdentifier:(NSTouchBarItemIdentifier)aIdentifier;
- (void)updateLabel:(NSTextField*)aLabel withIdentifier:(NSTouchBarItemIdentifier)aIdentifier;
- (NSTouchBarItem*)makeShareScrubberForIdentifier:(NSTouchBarItemIdentifier)aIdentifier;

/**
 * If aShowing is true, aPopover is shown. Otherwise, it is hidden.
 */
- (void)showPopover:(TouchBarInput*)aPopover showing:(bool)aShowing;

/**
 *  Redirects button actions to the appropriate handler.
 */
- (void)touchBarAction:(id)aSender;

- (NSArray*)itemsForSharingServicePickerTouchBarItem:
    (NSSharingServicePickerTouchBarItem*)aPickerTouchBarItem;

- (NSArray<NSSharingService*>*)sharingServicePicker:(NSSharingServicePicker*)aSharingServicePicker
                            sharingServicesForItems:(NSArray*)aItems
                            proposedSharingServices:(NSArray<NSSharingService*>*)aProposedServices;

- (void)releaseJSObjects;

@end  // nsTouchBar

#endif  // nsTouchBar_h_
