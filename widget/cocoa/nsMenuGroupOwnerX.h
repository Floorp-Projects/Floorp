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
#include "nsDataHashtable.h"
#include "nsString.h"

class nsMenuItemX;
class nsChangeObserver;
class nsIWidget;
class nsIContent;

class nsMenuGroupOwnerX : public nsMenuObjectX, public nsIMutationObserver
{
public:
  nsMenuGroupOwnerX();

  nsresult Create(nsIContent * aContent);

  void RegisterForContentChanges(nsIContent* aContent,
                                 nsChangeObserver* aMenuObject);
  void UnregisterForContentChanges(nsIContent* aContent);
  uint32_t RegisterForCommand(nsMenuItemX* aItem);
  void UnregisterCommand(uint32_t aCommandID);
  nsMenuItemX* GetMenuItemForCommandID(uint32_t inCommandID);
  void AddMenuItemInfoToSet(MenuItemInfo* info);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMUTATIONOBSERVER

protected:
  virtual ~nsMenuGroupOwnerX();

  nsChangeObserver* LookupContentChangeObserver(nsIContent* aContent);

  uint32_t  mCurrentCommandID;  // unique command id (per menu-bar) to
                                // give to next item that asks

  // stores observers for content change notification
  nsDataHashtable<nsPtrHashKey<nsIContent>, nsChangeObserver *> mContentToObserverTable;

  // stores mapping of command IDs to menu objects
  nsDataHashtable<nsUint32HashKey, nsMenuItemX *> mCommandToMenuObjectTable;

  // Stores references to all the MenuItemInfo objects created with weak
  // references to us.  They may live longer than we do, so when we're
  // destroyed we need to clear all their weak references.  This avoids
  // crashes in -[NativeMenuItemTarget menuItemHit:].  See bug 1131473.
  NSMutableSet* mInfoSet;
};

#endif // nsMenuGroupOwner_h_
