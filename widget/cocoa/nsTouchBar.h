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
  NSTouchBarItemIdentifier mNativeIdentifier;
  nsCOMPtr<nsITouchBarInputCallback> mCallback;
  RefPtr<Document> mDocument;
  BOOL mIsIconPositionSet;
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
- (void)setKey:(NSString*)aKey;
- (void)setTitle:(NSString*)aTitle;
- (void)setImageURI:(nsCOMPtr<nsIURI>)aImageURI;
- (void)setIcon:(RefPtr<nsTouchBarInputIcon>)aIcon;
- (void)setType:(NSString*)aType;
- (void)setColor:(NSColor*)aColor;
- (void)setDisabled:(BOOL)aDisabled;
- (void)setNativeIdentifier:(NSString*)aNativeIdentifier;
- (void)setCallback:(nsCOMPtr<nsITouchBarInputCallback>)aCallback;
- (void)setDocument:(RefPtr<Document>)aDocument;
- (void)setIconPositionSet:(BOOL)aIsIconPositionSet;

- (id)initWithKey:(NSString*)aKey
            title:(NSString*)aTitle
         imageURI:(nsCOMPtr<nsIURI>)aImageURI
             type:(NSString*)aType
         callback:(nsCOMPtr<nsITouchBarInputCallback>)aCallback
            color:(uint32_t)aColor
         disabled:(BOOL)aDisabled
         document:(RefPtr<Document>)aDocument;

- (TouchBarInput*)initWithXPCOM:(nsCOMPtr<nsITouchBarInput>)aInput;

- (void)dealloc;

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
 * Returns an instance of nsTouchBar based on implementation details
 * fetched from the frontend through nsTouchBarHelper.
 */
- (instancetype)init;

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
 */
- (void)updateItem:(TouchBarInput*)aInput;

/**
 * Update or create various subclasses of TouchBarItem.
 */
- (NSTouchBarItem*)updateButton:(NSCustomTouchBarItem*)aButton input:(TouchBarInput*)aInput;
- (NSTouchBarItem*)updateMainButton:(NSCustomTouchBarItem*)aMainButton input:(TouchBarInput*)aInput;
- (NSTouchBarItem*)makeShareScrubberForIdentifier:(NSTouchBarItemIdentifier)aIdentifier;

/**
 *  Redirects button actions to the appropriate handler and handles telemetry.
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
