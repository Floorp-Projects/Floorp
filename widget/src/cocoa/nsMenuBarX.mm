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
 *   Josh Aas <josh@mozilla.com>
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

#include "nsMenuBarX.h"
#include "nsMenuX.h"
#include "nsMenuItemX.h"
#include "nsMenuUtilsX.h"
#include "nsCocoaUtils.h"
#include "nsCocoaWindow.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsWidgetAtoms.h"
#include "nsGUIEvent.h"
#include "nsObjCExceptions.h"
#include "nsHashtable.h"
#include "nsThreadUtils.h"

#include "nsIContent.h"
#include "nsIWidget.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"

NS_IMPL_ISUPPORTS1(nsMenuBarX, nsIMutationObserver)

NativeMenuItemTarget* nsMenuBarX::sNativeEventTarget = nil;
nsMenuBarX* nsMenuBarX::sLastGeckoMenuBarPainted = nsnull;
NSMenu* sApplicationMenu = nil;
BOOL gSomeMenuBarPainted = NO;

// We keep references to the first quit and pref item content nodes we find, which
// will be from the hidden window. We use these when the document for the current
// window does not have a quit or pref item. We don't need strong refs here because
// these items are always strong ref'd by their owning menu bar (instance variable).
static nsIContent* sAboutItemContent = nsnull;
static nsIContent* sPrefItemContent  = nsnull;
static nsIContent* sQuitItemContent  = nsnull;

// Special command IDs that we know Mac OS X does not use for anything else. We use
// these in place of carbon's IDs for these commands in order to stop Carbon from
// messing with our event handlers. See bug 346883.
enum {
  eCommand_ID_About = 1,
  eCommand_ID_Prefs = 2,
  eCommand_ID_Quit  = 3,
  eCommand_ID_Last  = 4
};


NS_IMPL_ISUPPORTS1(nsNativeMenuServiceX, nsINativeMenuService)

NS_IMETHODIMP nsNativeMenuServiceX::CreateNativeMenuBar(nsIWidget* aParent, nsIContent* aMenuBarNode)
{
  NS_ASSERTION(NS_IsMainThread(), "Attempting to create native menu bar on wrong thread!");

  nsRefPtr<nsMenuBarX> mb = new nsMenuBarX();
  if (!mb)
    return NS_ERROR_OUT_OF_MEMORY;

  return mb->Create(aParent, aMenuBarNode);
}


nsMenuBarX::nsMenuBarX()
: mParentWindow(nsnull),
  mCurrentCommandID(eCommand_ID_Last),
  mDocument(nsnull)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mNativeMenu = [[GeckoNSMenu alloc] initWithTitle:@"MainMenuBar"];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


nsMenuBarX::~nsMenuBarX()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (nsMenuBarX::sLastGeckoMenuBarPainted == this)
    nsMenuBarX::sLastGeckoMenuBarPainted = nsnull;

  // the quit/pref items of a random window might have been used if there was no
  // hidden window, thus we need to invalidate the weak references.
  if (sAboutItemContent == mAboutItemContent)
    sAboutItemContent = nsnull;
  if (sQuitItemContent == mQuitItemContent)
    sQuitItemContent = nsnull;
  if (sPrefItemContent == mPrefItemContent)
    sPrefItemContent = nsnull;

  // make sure we unregister ourselves as a document observer
  if (mDocument)
    mDocument->RemoveMutationObserver(this);

  // We have to manually clear the array here because clearing causes menu items
  // to call back into the menu bar to unregister themselves. We don't want to
  // depend on member variable ordering to ensure that the array gets cleared
  // before the registration hash table is destroyed.
  mMenuArray.Clear();

  [mNativeMenu release];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


nsresult nsMenuBarX::Create(nsIWidget* aParent, nsIContent* aContent)
{
  if (!aParent || !aContent)
    return NS_ERROR_INVALID_ARG;

  mParentWindow = aParent;
  mContent = aContent;

  AquifyMenuBar();

  nsIDocument* doc = aContent->GetOwnerDoc();
  if (!doc)
    return NS_ERROR_FAILURE;
  doc->AddMutationObserver(this);
  mDocument = doc;

  ConstructNativeMenus();

  // Give this to the parent window. The parent takes ownership.
  return mParentWindow->SetMenuBar(this);
}


void nsMenuBarX::ConstructNativeMenus()
{
  PRUint32 count = mContent->GetChildCount();
  for (PRUint32 i = 0; i < count; i++) { 
    nsIContent *menuContent = mContent->GetChildAt(i);
    if (menuContent &&
        menuContent->Tag() == nsWidgetAtoms::menu &&
        menuContent->IsNodeOfType(nsINode::eXUL)) {
      nsMenuX* newMenu = new nsMenuX();
      if (newMenu) {
        nsresult rv = newMenu->Create(this, this, menuContent);
        if (NS_SUCCEEDED(rv))
          InsertMenuAtIndex(newMenu, GetMenuCount());
        else
          delete newMenu;
      }
    }
  }  
}


PRUint32 nsMenuBarX::GetMenuCount()
{
  return mMenuArray.Length();
}


bool nsMenuBarX::MenuContainsAppMenu()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  return ([mNativeMenu numberOfItems] > 0 &&
          [[mNativeMenu itemAtIndex:0] submenu] == sApplicationMenu);

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(false);
}


nsresult nsMenuBarX::InsertMenuAtIndex(nsMenuX* aMenu, PRUint32 aIndex)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // If we haven't created a global Application menu yet, do it.
  if (!sApplicationMenu) {
    nsresult rv = NS_OK; // avoid warning about rv being unused
    rv = CreateApplicationMenu(aMenu);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Can't create Application menu");

    // Hook the new Application menu up to the menu bar.
    NSMenu* mainMenu = [NSApp mainMenu];
    NS_ASSERTION([mainMenu numberOfItems] > 0, "Main menu does not have any items, something is terribly wrong!");
    [[mainMenu itemAtIndex:0] setSubmenu:sApplicationMenu];
  }

  // add menu to array that owns our menus
  mMenuArray.InsertElementAt(aIndex, aMenu);

  // hook up submenus
  nsIContent* menuContent = aMenu->Content();
  if (menuContent->GetChildCount() > 0 &&
      !nsMenuUtilsX::NodeIsHiddenOrCollapsed(menuContent)) {
    int insertionIndex = nsMenuUtilsX::CalculateNativeInsertionPoint(this, aMenu);
    if (MenuContainsAppMenu())
      insertionIndex++;
    [mNativeMenu insertItem:aMenu->NativeMenuItem() atIndex:insertionIndex];
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}


void nsMenuBarX::RemoveMenuAtIndex(PRUint32 aIndex)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NS_ASSERTION(aIndex < mMenuArray.Length(), "Attempting submenu removal with bad index!");

  // Our native menu and our internal menu object array might be out of sync.
  // This happens, for example, when a submenu is hidden. Because of this we
  // should not assume that a native submenu is hooked up.
  NSMenuItem* nativeMenuItem = mMenuArray[aIndex]->NativeMenuItem();
  int nativeMenuItemIndex = [mNativeMenu indexOfItem:nativeMenuItem];
  if (nativeMenuItemIndex != -1)
    [mNativeMenu removeItemAtIndex:nativeMenuItemIndex];

  mMenuArray.RemoveElementAt(aIndex);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


void nsMenuBarX::ForceUpdateNativeMenuAt(const nsAString& indexString)
{
  NSString* locationString = [NSString stringWithCharacters:indexString.BeginReading() length:indexString.Length()];
  NSArray* indexes = [locationString componentsSeparatedByString:@"|"];
  unsigned int indexCount = [indexes count];
  if (indexCount == 0)
    return;

  nsMenuX* currentMenu = NULL;
  int targetIndex = [[indexes objectAtIndex:0] intValue];
  int visible = 0;
  PRUint32 length = mMenuArray.Length();
  // first find a menu in the menu bar
  for (unsigned int i = 0; i < length; i++) {
    nsMenuX* menu = mMenuArray[i];
    if (!nsMenuUtilsX::NodeIsHiddenOrCollapsed(menu->Content())) {
      visible++;
      if (visible == (targetIndex + 1)) {
        currentMenu = menu;
        break;
      }
    }
  }

  if (!currentMenu)
    return;

  // fake open/close to cause lazy update to happen so submenus populate
  nsMenuEvent menuEvent(PR_TRUE, NS_MENU_SELECTED, nsnull);
  menuEvent.time = PR_IntervalNow();
  menuEvent.mCommand = (PRUint32)_NSGetCarbonMenu(static_cast<NSMenu*>(currentMenu->NativeData()));
  currentMenu->MenuOpened(menuEvent);
  currentMenu->MenuClosed(menuEvent);

  // now find the correct submenu
  for (unsigned int i = 1; currentMenu && i < indexCount; i++) {
    targetIndex = [[indexes objectAtIndex:i] intValue];
    visible = 0;
    length = currentMenu->GetItemCount();
    for (unsigned int j = 0; j < length; j++) {
      nsMenuObjectX* targetMenu = currentMenu->GetItemAt(j);
      if (!targetMenu)
        return;
      if (!nsMenuUtilsX::NodeIsHiddenOrCollapsed(targetMenu->Content())) {
        visible++;
        if (targetMenu->MenuObjectType() == eSubmenuObjectType && visible == (targetIndex + 1)) {
          currentMenu = static_cast<nsMenuX*>(targetMenu);
          // fake open/close to cause lazy update to happen
          nsMenuEvent menuEvent(PR_TRUE, NS_MENU_SELECTED, nsnull);
          menuEvent.time = PR_IntervalNow();
          menuEvent.mCommand = (PRUint32)_NSGetCarbonMenu(static_cast<NSMenu*>(currentMenu->NativeData()));
          currentMenu->MenuOpened(menuEvent);
          currentMenu->MenuClosed(menuEvent);
          break;
        }
      }
    }
  }
}


// Calling this forces a full reload of the menu system, reloading all native
// menus and their items.
// Without this testing is hard because changes to the DOM affect the native
// menu system lazily.
void nsMenuBarX::ForceNativeMenuReload()
{
  // tear down everything
  while (GetMenuCount() > 0)
    RemoveMenuAtIndex(0);

  // construct everything
  ConstructNativeMenus();
}


nsMenuX* nsMenuBarX::GetMenuAt(PRUint32 aIndex)
{
  if (mMenuArray.Length() <= aIndex) {
    NS_ERROR("Requesting menu at invalid index!");
    return NULL;
  }
  return mMenuArray[aIndex];
}


nsresult nsMenuBarX::Paint()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Don't try to optimize anything in this painting by checking
  // sLastGeckoMenuBarPainted because the menubar can be manipulated by
  // native dialogs and sheet code and other things besides this paint method.

  // We have to keep the same menu item for the Application menu so we keep
  // passing it along.
  NSMenu* outgoingMenu = [NSApp mainMenu];
  NS_ASSERTION([outgoingMenu numberOfItems] > 0, "Main menu does not have any items, something is terribly wrong!");

  NSMenuItem* appMenuItem = [[outgoingMenu itemAtIndex:0] retain];
  [outgoingMenu removeItemAtIndex:0];
  [mNativeMenu insertItem:appMenuItem atIndex:0];
  [appMenuItem release];

  // Set menu bar and event target.
  [NSApp setMainMenu:mNativeMenu];
  nsMenuBarX::sLastGeckoMenuBarPainted = this;

  gSomeMenuBarPainted = YES;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}


// Hide the item in the menu by setting the 'hidden' attribute. Returns it in |outHiddenNode| so
// the caller can hang onto it if they so choose. It is acceptable to pass nsull
// for |outHiddenNode| if the caller doesn't care about the hidden node.
void nsMenuBarX::HideItem(nsIDOMDocument* inDoc, const nsAString & inID, nsIContent** outHiddenNode)
{
  nsCOMPtr<nsIDOMElement> menuItem;
  inDoc->GetElementById(inID, getter_AddRefs(menuItem));  
  nsCOMPtr<nsIContent> menuContent(do_QueryInterface(menuItem));
  if (menuContent) {
    menuContent->SetAttr(kNameSpaceID_None, nsWidgetAtoms::hidden, NS_LITERAL_STRING("true"), PR_FALSE);
    if (outHiddenNode) {
      *outHiddenNode = menuContent.get();
      NS_IF_ADDREF(*outHiddenNode);
    }
  }
}


// Do what is necessary to conform to the Aqua guidelines for menus.
void nsMenuBarX::AquifyMenuBar()
{
  nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(mContent->GetDocument()));
  if (domDoc) {
    // remove the "About..." item and its separator
    HideItem(domDoc, NS_LITERAL_STRING("aboutSeparator"), nsnull);
    HideItem(domDoc, NS_LITERAL_STRING("aboutName"), getter_AddRefs(mAboutItemContent));
    if (!sAboutItemContent)
      sAboutItemContent = mAboutItemContent;

    // remove quit item and its separator
    HideItem(domDoc, NS_LITERAL_STRING("menu_FileQuitSeparator"), nsnull);
    HideItem(domDoc, NS_LITERAL_STRING("menu_FileQuitItem"), getter_AddRefs(mQuitItemContent));
    if (!sQuitItemContent)
      sQuitItemContent = mQuitItemContent;
    
    // remove prefs item and its separator, but save off the pref content node
    // so we can invoke its command later.
    HideItem(domDoc, NS_LITERAL_STRING("menu_PrefsSeparator"), nsnull);
    HideItem(domDoc, NS_LITERAL_STRING("menu_preferences"), getter_AddRefs(mPrefItemContent));
    if (!sPrefItemContent)
      sPrefItemContent = mPrefItemContent;
    
    // hide items that we use for the Application menu
    HideItem(domDoc, NS_LITERAL_STRING("menu_mac_services"), nsnull);
    HideItem(domDoc, NS_LITERAL_STRING("menu_mac_hide_app"), nsnull);
    HideItem(domDoc, NS_LITERAL_STRING("menu_mac_hide_others"), nsnull);
    HideItem(domDoc, NS_LITERAL_STRING("menu_mac_show_all"), nsnull);
  }
}


// for creating menu items destined for the Application menu
NSMenuItem* nsMenuBarX::CreateNativeAppMenuItem(nsMenuX* inMenu, const nsAString& nodeID, SEL action,
                                                int tag, NativeMenuItemTarget* target)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsCOMPtr<nsIDocument> doc = inMenu->Content()->GetDocument();
  if (!doc)
    return nil;

  nsCOMPtr<nsIDOMDocument> domdoc(do_QueryInterface(doc));
  if (!domdoc)
    return nil;

  // Get information from the gecko menu item
  nsAutoString label;
  nsAutoString modifiers;
  nsAutoString key;
  nsCOMPtr<nsIDOMElement> menuItem;
  domdoc->GetElementById(nodeID, getter_AddRefs(menuItem));
  if (menuItem) {
    menuItem->GetAttribute(NS_LITERAL_STRING("label"), label);
    menuItem->GetAttribute(NS_LITERAL_STRING("modifiers"), modifiers);
    menuItem->GetAttribute(NS_LITERAL_STRING("key"), key);
  }
  else {
    return nil;
  }

  // Get more information about the key equivalent. Start by
  // finding the key node we need.
  NSString* keyEquiv = nil;
  unsigned int macKeyModifiers = 0;
  if (!key.IsEmpty()) {
    nsCOMPtr<nsIDOMElement> keyElement;
    domdoc->GetElementById(key, getter_AddRefs(keyElement));
    if (keyElement) {
      nsCOMPtr<nsIContent> keyContent (do_QueryInterface(keyElement));
      // first grab the key equivalent character
      nsAutoString keyChar(NS_LITERAL_STRING(" "));
      keyContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::key, keyChar);
      if (!keyChar.EqualsLiteral(" ")) {
        keyEquiv = [[NSString stringWithCharacters:keyChar.get() length:keyChar.Length()] lowercaseString];
      }
      // now grab the key equivalent modifiers
      nsAutoString modifiersStr;
      keyContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::modifiers, modifiersStr);
      PRUint8 geckoModifiers = nsMenuUtilsX::GeckoModifiersForNodeAttribute(modifiersStr);
      macKeyModifiers = nsMenuUtilsX::MacModifiersForGeckoModifiers(geckoModifiers);
    }
  }
  // get the label into NSString-form
  NSString* labelString = [NSString stringWithCharacters:label.get() length:label.Length()];
  
  if (!labelString)
    labelString = @"";
  if (!keyEquiv)
    keyEquiv = @"";

  // put together the actual NSMenuItem
  NSMenuItem* newMenuItem = [[NSMenuItem alloc] initWithTitle:labelString action:action keyEquivalent:keyEquiv];
  
  [newMenuItem setTag:tag];
  [newMenuItem setTarget:target];
  [newMenuItem setKeyEquivalentModifierMask:macKeyModifiers];
  
  return newMenuItem;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}


// build the Application menu shared by all menu bars
nsresult nsMenuBarX::CreateApplicationMenu(nsMenuX* inMenu)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // At this point, the application menu is the application menu from
  // the nib in cocoa widgets. We do not have a way to create an application
  // menu manually, so we grab the one from the nib and use that.
  sApplicationMenu = [[[[NSApp mainMenu] itemAtIndex:0] submenu] retain];
  
/*
  We support the following menu items here:

  Menu Item             DOM Node ID             Notes
  
  ==================
  = About This App = <- aboutName
  ==================
  = Preferences... = <- menu_preferences
  ==================
  = Services     > = <- menu_mac_services    <- (do not define key equivalent)
  ==================
  = Hide App       = <- menu_mac_hide_app
  = Hide Others    = <- menu_mac_hide_others
  = Show All       = <- menu_mac_show_all
  ==================
  = Quit           = <- menu_FileQuitItem
  ==================
  
  If any of them are ommitted from the application's DOM, we just don't add
  them. We always add a "Quit" item, but if an app developer does not provide a
  DOM node with the right ID for the Quit item, we add it in English. App
  developers need only add each node with a label and a key equivalent (if they
  want one). Other attributes are optional. Like so:
  
  <menuitem id="menu_preferences"
         label="&preferencesCmdMac.label;"
           key="open_prefs_key"/>
  
  We need to use this system for localization purposes, until we have a better way
  to define the Application menu to be used on Mac OS X.
*/

  if (sApplicationMenu) {
    // This code reads attributes we are going to care about from the DOM elements
    
    NSMenuItem *itemBeingAdded = nil;
    
    // Add the About menu item
    itemBeingAdded = CreateNativeAppMenuItem(inMenu, NS_LITERAL_STRING("aboutName"), @selector(menuItemHit:),
                                             eCommand_ID_About, nsMenuBarX::sNativeEventTarget);
    if (itemBeingAdded) {
      [sApplicationMenu addItem:itemBeingAdded];
      [itemBeingAdded release];
      itemBeingAdded = nil;
      
      // Add separator after About menu
      [sApplicationMenu addItem:[NSMenuItem separatorItem]];      
    }
    
    // Add the Preferences menu item
    itemBeingAdded = CreateNativeAppMenuItem(inMenu, NS_LITERAL_STRING("menu_preferences"), @selector(menuItemHit:),
                                             eCommand_ID_Prefs, nsMenuBarX::sNativeEventTarget);
    if (itemBeingAdded) {
      [sApplicationMenu addItem:itemBeingAdded];
      [itemBeingAdded release];
      itemBeingAdded = nil;
      
      // Add separator after Preferences menu
      [sApplicationMenu addItem:[NSMenuItem separatorItem]];      
    }

    // Add Services menu item
    itemBeingAdded = CreateNativeAppMenuItem(inMenu, NS_LITERAL_STRING("menu_mac_services"), nil,
                                             0, nil);
    if (itemBeingAdded) {
      [sApplicationMenu addItem:itemBeingAdded];
      
      // set this menu item up as the Mac OS X Services menu
      NSMenu* servicesMenu = [[NSMenu alloc] initWithTitle:@""];
      [itemBeingAdded setSubmenu:servicesMenu];
      [NSApp setServicesMenu:servicesMenu];
      
      [itemBeingAdded release];
      itemBeingAdded = nil;
      
      // Add separator after Services menu
      [sApplicationMenu addItem:[NSMenuItem separatorItem]];      
    }
    
    BOOL addHideShowSeparator = FALSE;
    
    // Add menu item to hide this application
    itemBeingAdded = CreateNativeAppMenuItem(inMenu, NS_LITERAL_STRING("menu_mac_hide_app"), @selector(hide:),
                                             0, NSApp);
    if (itemBeingAdded) {
      [sApplicationMenu addItem:itemBeingAdded];
      [itemBeingAdded release];
      itemBeingAdded = nil;
      
      addHideShowSeparator = TRUE;
    }
    
    // Add menu item to hide other applications
    itemBeingAdded = CreateNativeAppMenuItem(inMenu, NS_LITERAL_STRING("menu_mac_hide_others"), @selector(hideOtherApplications:),
                                             0, NSApp);
    if (itemBeingAdded) {
      [sApplicationMenu addItem:itemBeingAdded];
      [itemBeingAdded release];
      itemBeingAdded = nil;
      
      addHideShowSeparator = TRUE;
    }
    
    // Add menu item to show all applications
    itemBeingAdded = CreateNativeAppMenuItem(inMenu, NS_LITERAL_STRING("menu_mac_show_all"), @selector(unhideAllApplications:),
                                             0, NSApp);
    if (itemBeingAdded) {
      [sApplicationMenu addItem:itemBeingAdded];
      [itemBeingAdded release];
      itemBeingAdded = nil;
      
      addHideShowSeparator = TRUE;
    }
    
    // Add a separator after the hide/show menus if at least one exists
    if (addHideShowSeparator)
      [sApplicationMenu addItem:[NSMenuItem separatorItem]];
    
    // Add quit menu item
    itemBeingAdded = CreateNativeAppMenuItem(inMenu, NS_LITERAL_STRING("menu_FileQuitItem"), @selector(menuItemHit:),
                                             eCommand_ID_Quit, nsMenuBarX::sNativeEventTarget);
    if (itemBeingAdded) {
      [sApplicationMenu addItem:itemBeingAdded];
      [itemBeingAdded release];
      itemBeingAdded = nil;
    }
    else {
      // the current application does not have a DOM node for "Quit". Add one
      // anyway, in English.
      NSMenuItem* defaultQuitItem = [[[NSMenuItem alloc] initWithTitle:@"Quit" action:@selector(menuItemHit:)
                                                         keyEquivalent:@"q"] autorelease];
      [defaultQuitItem setTarget:nsMenuBarX::sNativeEventTarget];
      [defaultQuitItem setTag:eCommand_ID_Quit];
      [sApplicationMenu addItem:defaultQuitItem];
    }
  }
  
  return (sApplicationMenu) ? NS_OK : NS_ERROR_FAILURE;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}


void nsMenuBarX::SetParent(nsIWidget* aParent)
{
  mParentWindow = aParent;
}


//
// nsIMutationObserver
//


void nsMenuBarX::CharacterDataWillChange(nsIDocument* aDocument,
                                         nsIContent* aContent,
                                         CharacterDataChangeInfo* aInfo)
{
}


void nsMenuBarX::CharacterDataChanged(nsIDocument* aDocument,
                                      nsIContent* aContent,
                                      CharacterDataChangeInfo* aInfo)
{
}


void nsMenuBarX::ContentAppended(nsIDocument* aDocument, nsIContent* aContainer,
                                 PRInt32 aNewIndexInContainer)
{
  PRUint32 childCount = aContainer->GetChildCount();
  while ((PRUint32)aNewIndexInContainer < childCount) {
    nsIContent *child = aContainer->GetChildAt(aNewIndexInContainer);
    ContentInserted(aDocument, aContainer, child, aNewIndexInContainer);
    aNewIndexInContainer++;
  }
}


void nsMenuBarX::NodeWillBeDestroyed(const nsINode * aNode)
{
  // our menu bar node is being destroyed
  mDocument = nsnull;
}


void nsMenuBarX::AttributeChanged(nsIDocument * aDocument, nsIContent * aContent,
                                  PRInt32 aNameSpaceID, nsIAtom * aAttribute,
                                  PRInt32 aModType, PRUint32 aStateMask)
{
  nsChangeObserver* obs = LookupContentChangeObserver(aContent);
  if (obs)
    obs->ObserveAttributeChanged(aDocument, aContent, aAttribute);
}


void nsMenuBarX::ContentRemoved(nsIDocument * aDocument, nsIContent * aContainer,
                                nsIContent * aChild, PRInt32 aIndexInContainer)
{
  if (aContainer == mContent) {
    RemoveMenuAtIndex(aIndexInContainer);
  }
  else {
    nsChangeObserver* obs = LookupContentChangeObserver(aContainer);
    if (obs) {
      obs->ObserveContentRemoved(aDocument, aChild, aIndexInContainer);
    }
    else {
      // We do a lookup on the parent container in case things were removed
      // under a "menupopup" item. That is basically a wrapper for the contents
      // of a "menu" node.
      nsCOMPtr<nsIContent> parent = aContainer->GetParent();
      if (parent) {
        obs = LookupContentChangeObserver(parent);
        if (obs)
          obs->ObserveContentRemoved(aDocument, aChild, aIndexInContainer);
      }
    }
  }
}


void nsMenuBarX::ContentInserted(nsIDocument * aDocument, nsIContent * aContainer,
                                 nsIContent * aChild, PRInt32 aIndexInContainer)
{
  if (aContainer == mContent) {
    nsMenuX* newMenu = new nsMenuX();
    if (newMenu) {
      nsresult rv = newMenu->Create(this, this, aChild);
      if (NS_SUCCEEDED(rv))
        InsertMenuAtIndex(newMenu, aIndexInContainer);
      else
        delete newMenu;
    }
  }
  else {
    nsChangeObserver* obs = LookupContentChangeObserver(aContainer);
    if (obs)
      obs->ObserveContentInserted(aDocument, aChild, aIndexInContainer);
    else {
      // We do a lookup on the parent container in case things were removed
      // under a "menupopup" item. That is basically a wrapper for the contents
      // of a "menu" node.
      nsCOMPtr<nsIContent> parent = aContainer->GetParent();
      if (parent) {
        obs = LookupContentChangeObserver(parent);
        if (obs)
          obs->ObserveContentInserted(aDocument, aChild, aIndexInContainer);
      }
    }
  }
}


void nsMenuBarX::ParentChainChanged(nsIContent *aContent)
{
}


// For change management, we don't use a |nsSupportsHashtable| because we know that the
// lifetime of all these items is bounded by the lifetime of the menubar. No need to add
// any more strong refs to the picture because the containment hierarchy already uses
// strong refs.
void nsMenuBarX::RegisterForContentChanges(nsIContent *aContent, nsChangeObserver *aMenuObject)
{
  nsVoidKey key(aContent);
  mObserverTable.Put(&key, aMenuObject);
}


void nsMenuBarX::UnregisterForContentChanges(nsIContent *aContent)
{
  nsVoidKey key(aContent);
  mObserverTable.Remove(&key);
}


nsChangeObserver* nsMenuBarX::LookupContentChangeObserver(nsIContent* aContent)
{
  nsVoidKey key(aContent);
  return reinterpret_cast<nsChangeObserver*>(mObserverTable.Get(&key));
}


// Given a menu item, creates a unique 4-character command ID and
// maps it to the item. Returns the id for use by the client.
PRUint32 nsMenuBarX::RegisterForCommand(nsMenuItemX* inMenuItem)
{
  // no real need to check for uniqueness. We always start afresh with each
  // window at 1. Even if we did get close to the reserved Apple command id's,
  // those don't start until at least '    ', which is integer 538976288. If
  // we have that many menu items in one window, I think we have other problems.

  // make id unique
  ++mCurrentCommandID;

  // put it in the table, set out param for client
  nsPRUint32Key key(mCurrentCommandID);
  mObserverTable.Put(&key, inMenuItem);

  return mCurrentCommandID;
}


// Removes the mapping between the given 4-character command ID
// and its associated menu item.
void nsMenuBarX::UnregisterCommand(PRUint32 inCommandID)
{
  nsPRUint32Key key(inCommandID);
  mObserverTable.Remove(&key);
}


nsMenuItemX* nsMenuBarX::GetMenuItemForCommandID(PRUint32 inCommandID)
{
  nsPRUint32Key key(inCommandID);
  return reinterpret_cast<nsMenuItemX*>(mObserverTable.Get(&key));
}


//
// Objective-C class used to allow us to have keyboard commands
// look like they are doing something but actually do nothing.
// We allow mouse actions to work normally.
//


// This tells us whether or not pKE is on the stack from a GeckoNSMenu. If it
// is nil, it is not on the stack. The non-nil value is the object that put it
// on the stack first.
static GeckoNSMenu* gPerformKeyEquivOnStack = nil;
// If this is YES, act on key equivs.
static BOOL gActOnKeyEquiv = NO;
// When this variable is set to NO, don't do special command processing.
static BOOL gActOnSpecialCommands = YES;

@implementation GeckoNSMenu

- (BOOL)performKeyEquivalent:(NSEvent *)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NS_ASSERTION(gPerformKeyEquivOnStack != self, "GeckoNSMenu pKE re-entering for the same object!");

  // Don't bother doing this if we don't have any items. It appears as though
  // the OS will sometimes expect this sort of check.
  if ([self numberOfItems] <= 0)
    return NO;

  if (!gPerformKeyEquivOnStack)
    gPerformKeyEquivOnStack = self;
  BOOL rv = [super performKeyEquivalent:theEvent];
  if (gPerformKeyEquivOnStack == self)
    gPerformKeyEquivOnStack = nil;
  return rv;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}


-(void)actOnKeyEquivalent:(NSEvent *)theEvent
{
  gActOnKeyEquiv = YES;
  [self performKeyEquivalent:theEvent];
  gActOnKeyEquiv = NO;
}


- (void)performMenuUserInterfaceEffectsForEvent:(NSEvent*)theEvent
{
  gActOnSpecialCommands = NO;
  [self performKeyEquivalent:theEvent];
  gActOnSpecialCommands = YES;
}

@end


//
// Objective-C class used as action target for menu items
//


@implementation NativeMenuItemTarget

// called when some menu item in this menu gets hit
-(IBAction)menuItemHit:(id)sender
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  int tag = [sender tag];
  nsMenuBarX* menuBar = nsMenuBarX::sLastGeckoMenuBarPainted;
  if (!menuBar)
    return;

  // We want to avoid processing app-global commands when we are asked to
  // perform native menu effects only. This avoids sending events twice,
  // which can lead to major problems.
  if (gActOnSpecialCommands) {
    // Do special processing if this is for an app-global command.
    if (tag == eCommand_ID_About) {
      nsIContent* mostSpecificContent = sAboutItemContent;
      if (menuBar && menuBar->mAboutItemContent)
        mostSpecificContent = menuBar->mAboutItemContent;
      nsMenuUtilsX::DispatchCommandTo(mostSpecificContent);
    }
    else if (tag == eCommand_ID_Prefs) {
      nsIContent* mostSpecificContent = sPrefItemContent;
      if (menuBar && menuBar->mPrefItemContent)
        mostSpecificContent = menuBar->mPrefItemContent;
      nsMenuUtilsX::DispatchCommandTo(mostSpecificContent);
    }
    else if (tag == eCommand_ID_Quit) {
      nsIContent* mostSpecificContent = sQuitItemContent;
      if (menuBar && menuBar->mQuitItemContent)
        mostSpecificContent = menuBar->mQuitItemContent;
      // If we have some content for quit we execute it. Otherwise we send a native app terminate
      // message. If you want to stop a quit from happening, provide quit content and return
      // the event as unhandled.
      if (mostSpecificContent) {
        nsMenuUtilsX::DispatchCommandTo(mostSpecificContent);
      }
      else {
        [NSApp terminate:nil];
        return;
      }
    }
    // Quit now if the "active" menu bar has changed (as the result of
    // processing an app-global command above).  This resolves bmo bug
    // 430506.
    if (menuBar != nsMenuBarX::sLastGeckoMenuBarPainted)
      return;
  }

  // Don't do anything unless this is not a keyboard command and
  // this isn't for the hidden window menu. We assume that if there
  // is no main window then the hidden window menu bar is up, even
  // if that isn't true for some reason we better play it safe if
  // there is no main window.
  if (gPerformKeyEquivOnStack && !gActOnKeyEquiv && [NSApp mainWindow])
    return;

  // given the commandID, look it up in our hashtable and dispatch to
  // that menu item.
  if (menuBar) {
    nsMenuItemX* menuItem = menuBar->GetMenuItemForCommandID(static_cast<PRUint32>(tag));
    if (menuItem)
      menuItem->DoCommand();
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

@end
