/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMenuGroupOwnerX_h_
#define nsMenuGroupOwnerX_h_

#import <Cocoa/Cocoa.h>

#include "mozilla/WeakPtr.h"

#include "nsStubMutationObserver.h"
#include "nsHashKeys.h"
#include "nsIObserver.h"
#include "nsMenuBarX.h"
#include "nsTHashMap.h"
#include "nsString.h"

class nsMenuItemX;
class nsChangeObserver;
class nsIWidget;
class nsIContent;

@class MOZMenuItemRepresentedObject;

// Fixed command IDs that work even without a JS listener, for our fallback menu
// bar. Dynamic command IDs start counting from eCommand_ID_Last.
enum {
  eCommand_ID_About = 1,
  eCommand_ID_Prefs = 2,
  eCommand_ID_Quit = 3,
  eCommand_ID_HideApp = 4,
  eCommand_ID_HideOthers = 5,
  eCommand_ID_ShowAll = 6,
  eCommand_ID_Update = 7,
  eCommand_ID_TouchBar = 8,
  eCommand_ID_Account = 9,
  eCommand_ID_Last = 10
};

// The menu group owner observes DOM mutations, notifies registered
// nsChangeObservers, and manages command registration. There is one owner per
// menubar, and one per standalone native menu.
class nsMenuGroupOwnerX : public nsMultiMutationObserver, public nsIObserver {
 public:
  // Both parameters can be null.
  nsMenuGroupOwnerX(mozilla::dom::Element* aElement,
                    nsMenuBarX* aMenuBarIfMenuBar);

  void RegisterForContentChanges(nsIContent* aContent,
                                 nsChangeObserver* aMenuObject);
  void UnregisterForContentChanges(nsIContent* aContent);
  uint32_t RegisterForCommand(nsMenuItemX* aMenuItem);
  void UnregisterCommand(uint32_t aCommandID);
  nsMenuItemX* GetMenuItemForCommandID(uint32_t aCommandID);

  void RegisterForLocaleChanges();
  void UnregisterForLocaleChanges();

  // The representedObject that's used for all menu items under this menu group
  // owner.
  MOZMenuItemRepresentedObject* GetRepresentedObject() {
    return mRepresentedObject;
  }

  // If this is the group owner for a menubar, return the menubar, otherwise
  // nullptr.
  nsMenuBarX* GetMenuBar() { return mMenuBar.get(); }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIMUTATIONOBSERVER

 protected:
  virtual ~nsMenuGroupOwnerX();

  nsChangeObserver* LookupContentChangeObserver(nsIContent* aContent);

  RefPtr<nsIContent> mContent;

  // Unique command id (per menu-bar) to give to next item that asks.
  uint32_t mCurrentCommandID = eCommand_ID_Last;

  // stores observers for content change notification
  nsTHashMap<nsPtrHashKey<nsIContent>, nsChangeObserver*>
      mContentToObserverTable;

  // stores mapping of command IDs to menu objects
  nsTHashMap<nsUint32HashKey, nsMenuItemX*> mCommandToMenuObjectTable;

  MOZMenuItemRepresentedObject* mRepresentedObject = nil;  // [strong]
  mozilla::WeakPtr<nsMenuBarX> mMenuBar;
};

@interface MOZMenuItemRepresentedObject : NSObject
- (id)initWithMenuGroupOwner:(nsMenuGroupOwnerX*)aMenuGroupOwner;
- (void)setMenuGroupOwner:(nsMenuGroupOwnerX*)aMenuGroupOwner;
- (nsMenuGroupOwnerX*)menuGroupOwner;
@end

#endif  // nsMenuGroupOwner_h_
