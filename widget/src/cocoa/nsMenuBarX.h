/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Josh Aas <josh@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMenuBarX_h_
#define nsMenuBarX_h_

#import <Cocoa/Cocoa.h>

#include "nsMenuBaseX.h"
#include "nsIMutationObserver.h"
#include "nsHashtable.h"
#include "nsINativeMenuService.h"
#include "nsAutoPtr.h"
#include "nsString.h"

class nsMenuX;
class nsMenuItemX;
class nsChangeObserver;
class nsIWidget;
class nsIContent;
class nsIDocument;

// The native menu service for creating native menu bars.
class nsNativeMenuServiceX : public nsINativeMenuService
{
public:
  NS_DECL_ISUPPORTS
  NS_IMETHOD CreateNativeMenuBar(nsIWidget* aParent, nsIContent* aMenuBarNode);
};

// Objective-C class used to allow us to have keyboard commands
// look like they are doing something but actually do nothing.
// We allow mouse actions to work normally.
@interface GeckoNSMenu : NSMenu
{
}
- (BOOL)performKeyEquivalent:(NSEvent*)theEvent;
- (void)actOnKeyEquivalent:(NSEvent*)theEvent;
- (void)performMenuUserInterfaceEffectsForEvent:(NSEvent*)theEvent;
@end

// Objective-C class used as action target for menu items
@interface NativeMenuItemTarget : NSObject
{
}
-(IBAction)menuItemHit:(id)sender;
@end

// Once instantiated, this object lives until its DOM node or its parent window is destroyed.
// Do not hold references to this, they can become invalid any time the DOM node can be destroyed.
class nsMenuBarX : public nsMenuObjectX,
                   public nsIMutationObserver
{
public:
  nsMenuBarX();
  virtual ~nsMenuBarX();

  static NativeMenuItemTarget* sNativeEventTarget;
  static nsMenuBarX*           sLastGeckoMenuBarPainted;

  // The following content nodes have been removed from the menu system.
  // We save them here for use in command handling.
  nsCOMPtr<nsIContent> mAboutItemContent;
  nsCOMPtr<nsIContent> mPrefItemContent;
  nsCOMPtr<nsIContent> mQuitItemContent;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMUTATIONOBSERVER

  // nsMenuObjectX
  void*             NativeData()     {return (void*)mNativeMenu;}
  nsMenuObjectTypeX MenuObjectType() {return eMenuBarObjectType;}

  // nsMenuBarX
  nsresult          Create(nsIWidget* aParent, nsIContent* aContent);
  void              SetParent(nsIWidget* aParent);
  void              RegisterForContentChanges(nsIContent* aContent, nsChangeObserver* aMenuObject);
  void              UnregisterForContentChanges(nsIContent* aContent);
  PRUint32          RegisterForCommand(nsMenuItemX* aItem);
  void              UnregisterCommand(PRUint32 aCommandID);
  PRUint32          GetMenuCount();
  bool              MenuContainsAppMenu();
  nsMenuX*          GetMenuAt(PRUint32 aIndex);
  nsMenuItemX*      GetMenuItemForCommandID(PRUint32 inCommandID);
  nsresult          Paint();
  void              ForceUpdateNativeMenuAt(const nsAString& indexString);
  void              ForceNativeMenuReload(); // used for testing
  static char       GetLocalizedAccelKey(char *shortcutID);

protected:
  void              ConstructNativeMenus();
  nsresult          InsertMenuAtIndex(nsMenuX* aMenu, PRUint32 aIndex);
  void              RemoveMenuAtIndex(PRUint32 aIndex);
  nsChangeObserver* LookupContentChangeObserver(nsIContent* aContent);
  void              HideItem(nsIDOMDocument* inDoc, const nsAString & inID, nsIContent** outHiddenNode);
  void              AquifyMenuBar();
  NSMenuItem*       CreateNativeAppMenuItem(nsMenuX* inMenu, const nsAString& nodeID, SEL action,
                                            int tag, NativeMenuItemTarget* target);
  nsresult          CreateApplicationMenu(nsMenuX* inMenu);

  nsTArray< nsAutoPtr<nsMenuX> > mMenuArray;
  nsIWidget*         mParentWindow;        // [weak]
  PRUint32           mCurrentCommandID;    // unique command id (per menu-bar) to give to next item that asks
  nsIDocument*       mDocument;            // pointer to document
  GeckoNSMenu*       mNativeMenu;            // root menu, representing entire menu bar
  nsHashtable        mObserverTable;       // stores observers for content change notification
};

#endif // nsMenuBarX_h_
