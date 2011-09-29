/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
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
 * The Original Code is Native Menu Wrapper.
 *
 * The Initial Developer of the Original Code is
 * Thomas K. Dyas <tom.dyas@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#import <Cocoa/Cocoa.h>

#include "nsStandaloneNativeMenu.h"
#include "nsMenuUtilsX.h"
#include "nsIDOMElement.h"
#include "nsIMutationObserver.h"
#include "nsEvent.h"
#include "nsGUIEvent.h"
#include "nsWidgetAtoms.h"
#include "nsContentUtils.h"
#include "nsObjCExceptions.h"


NS_IMPL_ISUPPORTS2(nsStandaloneNativeMenu, nsIMutationObserver, nsIStandaloneNativeMenu)

nsStandaloneNativeMenu::nsStandaloneNativeMenu()
: mMenu(nsnull)
{
}

nsStandaloneNativeMenu::~nsStandaloneNativeMenu()
{
  if (mMenu) delete mMenu;
}

NS_IMETHODIMP
nsStandaloneNativeMenu::Init(nsIDOMElement * aDOMElement)
{
  NS_ASSERTION(mMenu == nsnull, "nsNativeMenu::Init - mMenu not null!");

  nsresult rv;

  nsCOMPtr<nsIContent> content = do_QueryInterface(aDOMElement, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIAtom * tag = content->Tag();
  if (!content->IsXUL() ||
      (tag != nsWidgetAtoms::menu && tag != nsWidgetAtoms::menupopup))
    return NS_ERROR_FAILURE;

  rv = nsMenuGroupOwnerX::Create(content);
  if (NS_FAILED(rv))
    return rv;

  mMenu = new nsMenuX();
  rv = mMenu->Create(this, this, content);
  if (NS_FAILED(rv)) {
    delete mMenu;
    mMenu = nsnull;
    return rv;
  }

  return NS_OK;
}

static void
UpdateMenu(nsMenuX * aMenu)
{
  aMenu->MenuOpened();
  aMenu->MenuClosed();

  PRUint32 itemCount = aMenu->GetItemCount();
  for (PRUint32 i = 0; i < itemCount; i++) {
    nsMenuObjectX * menuObject = aMenu->GetItemAt(i);
    if (menuObject->MenuObjectType() == eSubmenuObjectType) {
      UpdateMenu(static_cast<nsMenuX*>(menuObject));
    }
  }
}

NS_IMETHODIMP
nsStandaloneNativeMenu::MenuWillOpen(bool * aResult)
{
  NS_ASSERTION(mMenu != nsnull, "nsStandaloneNativeMenu::OnOpen - mMenu is null!");

  // Force an update on the mMenu by faking an open/close on all of
  // its submenus.
  UpdateMenu(mMenu);

  *aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsStandaloneNativeMenu::GetNativeMenu(void ** aVoidPointer)
{
  if (mMenu) {
    *aVoidPointer = mMenu->NativeData();
    [[(NSObject *)(*aVoidPointer) retain] autorelease];
    return NS_OK;
  }  else {
    *aVoidPointer = nsnull;
    return NS_ERROR_NOT_INITIALIZED;
  }
}

static NSMenuItem *
NativeMenuItemWithLocation(NSMenu * currentSubmenu, NSString * locationString)
{
  NSArray * indexes = [locationString componentsSeparatedByString:@"|"];
  NSUInteger indexCount = [indexes count];
  if (indexCount == 0)
    return nil;

  for (NSUInteger i = 0; i < indexCount; i++) {
    NSInteger targetIndex = [[indexes objectAtIndex:i] integerValue];
    NSInteger itemCount = [currentSubmenu numberOfItems];
    if (targetIndex < itemCount) {
      NSMenuItem* menuItem = [currentSubmenu itemAtIndex:targetIndex];

      // If this is the last index, just return the menu item.
      if (i == (indexCount - 1))
        return menuItem;

      // If this is not the last index, find the submenu and keep going.
      if ([menuItem hasSubmenu])
        currentSubmenu = [menuItem submenu];
      else
        return nil;
    }
  }

  return nil;
}

NS_IMETHODIMP
nsStandaloneNativeMenu::ActivateNativeMenuItemAt(const nsAString& indexString)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;
  
  if (!mMenu)
    return NS_ERROR_NOT_INITIALIZED;

  NSString * locationString = [NSString stringWithCharacters:indexString.BeginReading() length:indexString.Length()];
  NSMenu * menu = static_cast<NSMenu *> (mMenu->NativeData());
  NSMenuItem * item = NativeMenuItemWithLocation(menu, locationString);

  // We can't perform an action on an item with a submenu, that will raise
  // an obj-c exception.
  if (item && ![item hasSubmenu]) {
    NSMenu * parent = [item menu];
    if (parent) {
      // NSLog(@"Performing action for native menu item titled: %@\n",
      //       [[currentSubmenu itemAtIndex:targetIndex] title]);
      [parent performActionForItemAtIndex:[parent indexOfItem:item]];
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
  
  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
nsStandaloneNativeMenu::ForceUpdateNativeMenuAt(const nsAString& indexString)
{
  if (!mMenu)
    return NS_ERROR_NOT_INITIALIZED;

  NSString* locationString = [NSString stringWithCharacters:indexString.BeginReading() length:indexString.Length()];
  NSArray* indexes = [locationString componentsSeparatedByString:@"|"];
  unsigned int indexCount = [indexes count];
  if (indexCount == 0)
    return NS_OK;

  nsMenuX* currentMenu = mMenu;

  // now find the correct submenu
  for (unsigned int i = 1; currentMenu && i < indexCount; i++) {
    int targetIndex = [[indexes objectAtIndex:i] intValue];
    int visible = 0;
    PRUint32 length = currentMenu->GetItemCount();
    for (unsigned int j = 0; j < length; j++) {
      nsMenuObjectX* targetMenu = currentMenu->GetItemAt(j);
      if (!targetMenu)
        return NS_OK;
      if (!nsMenuUtilsX::NodeIsHiddenOrCollapsed(targetMenu->Content())) {
        visible++;
        if (targetMenu->MenuObjectType() == eSubmenuObjectType && visible == (targetIndex + 1)) {
          currentMenu = static_cast<nsMenuX*>(targetMenu);
          // fake open/close to cause lazy update to happen
          currentMenu->MenuOpened();
          currentMenu->MenuClosed();
          break;
        }
      }
    }
  }

  return NS_OK;
}
