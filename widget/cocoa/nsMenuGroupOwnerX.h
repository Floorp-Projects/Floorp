/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMenuGroupOwnerX_h_
#define nsMenuGroupOwnerX_h_

#import <Cocoa/Cocoa.h>

#include "nsMenuBaseX.h"
#include "nsIMutationObserver.h"
#include "nsHashKeys.h"
#include "nsTHashMap.h"
#include "nsString.h"

class nsMenuItemX;
class nsChangeObserver;
class nsIWidget;
class nsIContent;

@class MOZMenuItemRepresentedObject;

class nsMenuGroupOwnerX : public nsMenuObjectX, public nsIMutationObserver {
 public:
  nsMenuGroupOwnerX();

  nsresult Create(mozilla::dom::Element* aContent);

  void RegisterForContentChanges(nsIContent* aContent, nsChangeObserver* aMenuObject);
  void UnregisterForContentChanges(nsIContent* aContent);
  uint32_t RegisterForCommand(nsMenuItemX* aMenuItem);
  void UnregisterCommand(uint32_t aCommandID);
  nsMenuItemX* GetMenuItemForCommandID(uint32_t aCommandID);

  // The representedObject that's used for all menu items under this menu group owner.
  MOZMenuItemRepresentedObject* GetRepresentedObject() { return mRepresentedObject; }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMUTATIONOBSERVER

 protected:
  virtual ~nsMenuGroupOwnerX();

  nsChangeObserver* LookupContentChangeObserver(nsIContent* aContent);

  RefPtr<nsIContent> mContent;

  uint32_t mCurrentCommandID;  // unique command id (per menu-bar) to
                               // give to next item that asks

  // stores observers for content change notification
  nsTHashMap<nsPtrHashKey<nsIContent>, nsChangeObserver*> mContentToObserverTable;

  // stores mapping of command IDs to menu objects
  nsTHashMap<nsUint32HashKey, nsMenuItemX*> mCommandToMenuObjectTable;

  MOZMenuItemRepresentedObject* mRepresentedObject = nil;  // [strong]
};

@interface MOZMenuItemRepresentedObject : NSObject
- (id)initWithMenuGroupOwner:(nsMenuGroupOwnerX*)aMenuGroupOwner;
- (void)setMenuGroupOwner:(nsMenuGroupOwnerX*)aMenuGroupOwner;
- (nsMenuGroupOwnerX*)menuGroupOwner;
@end

#endif  // nsMenuGroupOwner_h_
