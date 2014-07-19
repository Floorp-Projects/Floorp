/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMenuBaseX_h_
#define nsMenuBaseX_h_

#import <Foundation/Foundation.h>

#include "nsCOMPtr.h"
#include "nsIContent.h"

enum nsMenuObjectTypeX {
  eMenuBarObjectType,
  eSubmenuObjectType,
  eMenuItemObjectType,
  eStandaloneNativeMenuObjectType,
};

// All menu objects subclass this.
// Menu bars are owned by their top-level nsIWidgets.
// All other objects are memory-managed based on the DOM.
// Content removal deletes them immediately and nothing else should.
// Do not attempt to hold strong references to them or delete them.
class nsMenuObjectX
{
public:
  virtual ~nsMenuObjectX() { }
  virtual nsMenuObjectTypeX MenuObjectType()=0;
  virtual void*             NativeData()=0;
  nsIContent*               Content() { return mContent; }

  /**
   * Called when an icon of a menu item somewhere in this menu has updated.
   * Menu objects with parents need to propagate the notification to their
   * parent.
   */
  virtual void IconUpdated() {}

protected:
  nsCOMPtr<nsIContent> mContent;
};


//
// Object stored as "representedObject" for all menu items
//

class nsMenuGroupOwnerX;

@interface MenuItemInfo : NSObject
{
  nsMenuGroupOwnerX * mMenuGroupOwner;
}

- (id) initWithMenuGroupOwner:(nsMenuGroupOwnerX *)aMenuGroupOwner;
- (nsMenuGroupOwnerX *) menuGroupOwner;
- (void) setMenuGroupOwner:(nsMenuGroupOwnerX *)aMenuGroupOwner;

@end


// Special command IDs that we know Mac OS X does not use for anything else.
// We use these in place of carbon's IDs for these commands in order to stop
// Carbon from messing with our event handlers. See bug 346883.

enum {
  eCommand_ID_About      = 1,
  eCommand_ID_Prefs      = 2,
  eCommand_ID_Quit       = 3,
  eCommand_ID_HideApp    = 4,
  eCommand_ID_HideOthers = 5,
  eCommand_ID_ShowAll    = 6,
  eCommand_ID_Update     = 7,
  eCommand_ID_Last       = 8
};

#endif // nsMenuBaseX_h_
