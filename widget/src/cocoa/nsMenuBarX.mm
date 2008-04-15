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

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsINameSpaceManager.h"
#include "nsIMenu.h"
#include "nsIMenuItem.h"
#include "nsIContent.h"

#include "nsMenuBarX.h"
#include "nsMenuX.h"
#include "nsChildView.h"
#include "nsCocoaUtils.h"
#include "nsCocoaWindow.h"

#include "nsISupports.h"
#include "nsIWidget.h"
#include "nsString.h"
#include "nsIStringBundle.h"
#include "nsIDocument.h"
#include "nsIMutationObserver.h"

#include "nsIDOMDocument.h"
#include "nsWidgetAtoms.h"

#include "nsGUIEvent.h"

// CIDs
#include "nsWidgetsCID.h"
static NS_DEFINE_CID(kMenuCID, NS_MENU_CID);

NS_IMPL_ISUPPORTS3(nsMenuBarX, nsIMenuBar, nsIMutationObserver, nsISupportsWeakReference)

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


PRBool NodeIsHiddenOrCollapsed(nsIContent* inContent)
{
  return (inContent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::hidden,
                                 nsWidgetAtoms::_true, eCaseMatters) ||
          inContent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::collapsed,
                                 nsWidgetAtoms::_true, eCaseMatters));
}


nsMenuBarX::nsMenuBarX()
: mParent(nsnull),
  mIsMenuBarAdded(PR_FALSE),
  mCurrentCommandID(eCommand_ID_Last),
  mDocument(nsnull)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mRootMenu = [[GeckoNSMenu alloc] initWithTitle:@"MainMenuBar"];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


nsMenuBarX::~nsMenuBarX()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mMenusArray.Clear(); // release all menus

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
  
  [mRootMenu release];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}


// Do what's necessary to conform to the Aqua guidelines for menus.
void
nsMenuBarX::AquifyMenuBar()
{
  nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(mMenuBarContent->GetDocument()));
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


// Hide the item in the menu by setting the 'hidden' attribute. Returns it in |outHiddenNode| so
// the caller can hang onto it if they so choose. It is acceptable to pass nsull
// for |outHiddenNode| if the caller doesn't care about the hidden node.
void
nsMenuBarX::HideItem(nsIDOMDocument* inDoc, const nsAString & inID, nsIContent** outHiddenNode)
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


NS_IMETHODIMP
nsMenuBarX::MenuConstruct(const nsMenuEvent & aMenuEvent, nsIWidget* aParentWindow, void * aMenubarNode)
{
  nsIDOMNode* domNode  = static_cast<nsIDOMNode*>(aMenubarNode);
  mMenuBarContent = do_QueryInterface(domNode); // strong ref
  NS_ASSERTION(mMenuBarContent, "No content specified for this menubar");
  if (!mMenuBarContent)
    return NS_ERROR_FAILURE;

  SetParent(aParentWindow);

  AquifyMenuBar();

  nsCOMPtr<nsIDOMDocument> domDoc;
  domNode->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  if (!doc)
    return NS_ERROR_FAILURE;
  doc->AddMutationObserver(this);
  mDocument = doc;

  PRUint32 count = mMenuBarContent->GetChildCount();
  for (PRUint32 i = 0; i < count; i++) { 
    nsIContent *menu = mMenuBarContent->GetChildAt(i);
    if (menu) {
      if (menu->Tag() == nsWidgetAtoms::menu &&
          menu->IsNodeOfType(nsINode::eXUL)) {
        nsAutoString menuName;
        nsAutoString menuAccessKey(NS_LITERAL_STRING(" "));
        menu->GetAttr(kNameSpaceID_None, nsWidgetAtoms::label, menuName);
        menu->GetAttr(kNameSpaceID_None, nsWidgetAtoms::accesskey, menuAccessKey);

        // Create nsMenu, the menubar will own it
        nsCOMPtr<nsIMenu> pnsMenu(do_CreateInstance(kMenuCID));
        if (pnsMenu) {
          pnsMenu->Create(static_cast<nsIMenuBar*>(this), menuName, menuAccessKey, this, menu);
          AddMenu(pnsMenu);
        }
      } 
    }
  }
  
  // Give the aParentWindow this nsMenuBarX to hold onto.
  // The parent takes ownership.
  aParentWindow->SetMenuBar(this);
  
  return NS_OK;
}


NS_IMETHODIMP nsMenuBarX::Create(nsIWidget *aParent)
{
  SetParent(aParent);
  return NS_OK;
}


NS_IMETHODIMP nsMenuBarX::GetParent(nsIWidget *&aParent)
{
  NS_IF_ADDREF(aParent = mParent);
  return NS_OK;
}


NS_IMETHODIMP nsMenuBarX::SetParent(nsIWidget *aParent)
{
  mParent = aParent; // weak ref  
  return NS_OK;
}


NS_IMETHODIMP nsMenuBarX::AddMenu(nsIMenu * aMenu)
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

  // keep track of all added menus
  mMenusArray.AppendObject(aMenu); // owner
  
  NSMenu* nativeMenu = NULL;
  aMenu->GetNativeData((void**)&nativeMenu);
  
  nsCOMPtr<nsIContent> menu;
  aMenu->GetMenuContent(getter_AddRefs(menu));
  if (menu->GetChildCount() > 0 &&
      !NodeIsHiddenOrCollapsed(menu)) {
    NSMenuItem* newMenuItem = [[[NSMenuItem alloc] initWithTitle:[nativeMenu title] action:NULL keyEquivalent:@""] autorelease];
    [mRootMenu addItem:newMenuItem];
    [newMenuItem setSubmenu:nativeMenu];
  }
  
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}


// for creating menu items destined for the Application menu
NSMenuItem* nsMenuBarX::CreateNativeAppMenuItem(nsIMenu* inMenu, const nsAString& nodeID, SEL action,
                                                int tag, NativeMenuItemTarget* target)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  nsCOMPtr<nsIContent> menu;
  inMenu->GetMenuContent(getter_AddRefs(menu));
  if (!menu)
    return nil;

  nsCOMPtr<nsIDocument> doc = menu->GetDocument();
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
      PRUint8 geckoModifiers = MenuHelpersX::GeckoModifiersForNodeAttribute(modifiersStr);
      macKeyModifiers = MenuHelpersX::MacModifiersForGeckoModifiers(geckoModifiers);
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
nsresult
nsMenuBarX::CreateApplicationMenu(nsIMenu* inMenu)
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


NS_IMETHODIMP nsMenuBarX::GetMenuCount(PRUint32 &aCount)
{
  aCount = mMenusArray.Count();
  return NS_OK;
}


NS_IMETHODIMP nsMenuBarX::GetMenuAt(const PRUint32 aCount, nsIMenu *& aMenu)
{ 
  aMenu = NULL;
  nsCOMPtr<nsIMenu> menu = mMenusArray.ObjectAt(aCount);
  if (!menu)
    return NS_OK;
  
  return CallQueryInterface(menu, &aMenu); // addref
}


NS_IMETHODIMP nsMenuBarX::InsertMenuAt(const PRUint32 aCount, nsIMenu *& aMenu)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsMenuBarX::RemoveMenu(const PRUint32 aCount)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  mMenusArray.RemoveObjectAt(aCount);
  [mRootMenu removeItemAtIndex:aCount];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}


NS_IMETHODIMP nsMenuBarX::RemoveAll()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsMenuBarX::GetNativeData(void *& aData)
{
  aData = (void*)mRootMenu;
  return NS_OK;
}


NS_IMETHODIMP nsMenuBarX::SetNativeData(void* aData)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP nsMenuBarX::Paint()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NSMenu* mainMenu = [NSApp mainMenu];
  NS_ASSERTION([mainMenu numberOfItems] > 0, "Main menu does not have any items, something is terribly wrong!");

  // Swap out first item into incoming menu bar. We have to keep the same menu item for the
  // Application menu and its submenu is global so we keep passing it along.
  NSMenuItem* firstMenuItem = [[mainMenu itemAtIndex:0] retain];
  [mainMenu removeItemAtIndex:0];
  [mRootMenu insertItem:firstMenuItem atIndex:0];
  [firstMenuItem release];

  // Set menu bar and event target.
  [NSApp setMainMenu:mRootMenu];
  nsMenuBarX::sLastGeckoMenuBarPainted = this;

  gSomeMenuBarPainted = YES;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}


//
// nsIMutationObserver
//

void
nsMenuBarX::CharacterDataWillChange(nsIDocument * aDocument,
                                    nsIContent * aContent,
                                    CharacterDataChangeInfo * aInfo)
{
}

void
nsMenuBarX::CharacterDataChanged(nsIDocument * aDocument,
                                 nsIContent * aContent,
                                 CharacterDataChangeInfo * aInfo)
{
}


void
nsMenuBarX::ContentAppended(nsIDocument* aDocument, nsIContent* aContainer,
                            PRInt32 aNewIndexInContainer)
{
  if (aContainer != mMenuBarContent) {
    nsChangeObserver* obs = LookupContentChangeObserver(aContainer);
    if (obs)
      obs->ObserveContentInserted(aDocument, aContainer, aNewIndexInContainer);
    else {
      nsCOMPtr<nsIContent> parent = aContainer->GetParent();
      if (parent) {
        obs = LookupContentChangeObserver(parent);
        if (obs)
          obs->ObserveContentInserted(aDocument, aContainer, aNewIndexInContainer);
      }
    }
  }
}


void
nsMenuBarX::NodeWillBeDestroyed(const nsINode * aNode)
{
  mDocument = nsnull;
}


void
nsMenuBarX::AttributeChanged(nsIDocument * aDocument, nsIContent * aContent,
                             PRInt32 aNameSpaceID, nsIAtom * aAttribute,
                             PRInt32 aModType, PRUint32 aStateMask)
{
  // lookup and dispatch to registered thang
  nsChangeObserver* obs = LookupContentChangeObserver(aContent);
  if (obs)
    obs->ObserveAttributeChanged(aDocument, aContent, aAttribute);
}


void
nsMenuBarX::ContentRemoved(nsIDocument * aDocument, nsIContent * aContainer,
                           nsIContent * aChild, PRInt32 aIndexInContainer)
{
  if (aContainer == mMenuBarContent) {
    UnregisterForContentChanges(aChild);
    RemoveMenu(aIndexInContainer);
  }
  else {
    nsChangeObserver* obs = LookupContentChangeObserver(aContainer);
    if (obs) {
      obs->ObserveContentRemoved(aDocument, aChild, aIndexInContainer);
    }
    else {
      nsCOMPtr<nsIContent> parent = aContainer->GetParent();
      if (parent) {
        obs = LookupContentChangeObserver(parent);
        if (obs)
          obs->ObserveContentRemoved(aDocument, aChild, aIndexInContainer);
      }
    }
  }
}


void
nsMenuBarX::ContentInserted(nsIDocument * aDocument, nsIContent * aContainer,
                             nsIContent * aChild, PRInt32 aIndexInContainer)
{
  if (aContainer != mMenuBarContent) {
    nsChangeObserver* obs = LookupContentChangeObserver(aContainer);
    if (obs)
      obs->ObserveContentInserted(aDocument, aChild, aIndexInContainer);
    else {
      nsCOMPtr<nsIContent> parent = aContainer->GetParent();
      if (parent) {
        obs = LookupContentChangeObserver(parent);
        if (obs)
          obs->ObserveContentInserted(aDocument, aChild, aIndexInContainer);
      }
    }
  }
}


void
nsMenuBarX::ParentChainChanged(nsIContent *aContent)
{
}


// For change management, we don't use a |nsSupportsHashtable| because we know that the
// lifetime of all these items is bounded by the lifetime of the menubar. No need to add
// any more strong refs to the picture because the containment hierarchy already uses
// strong refs.
void
nsMenuBarX::RegisterForContentChanges(nsIContent *aContent, nsChangeObserver *aMenuObject)
{
  nsVoidKey key(aContent);
  mObserverTable.Put(&key, aMenuObject);
}


void
nsMenuBarX::UnregisterForContentChanges(nsIContent *aContent)
{
  nsVoidKey key(aContent);
  mObserverTable.Remove(&key);
}


nsChangeObserver*
nsMenuBarX::LookupContentChangeObserver(nsIContent* aContent)
{
  nsVoidKey key(aContent);
  return reinterpret_cast<nsChangeObserver*>(mObserverTable.Get(&key));
}


// Given a menu item, creates a unique 4-character command ID and
// maps it to the item. Returns the id for use by the client.
PRUint32
nsMenuBarX::RegisterForCommand(nsIMenuItem* inMenuItem)
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
void
nsMenuBarX::UnregisterCommand(PRUint32 inCommandID)
{
  nsPRUint32Key key(inCommandID);
  mObserverTable.Remove(&key);
}


nsIMenuItem*
nsMenuBarX::GetMenuItemForCommandID(PRUint32 inCommandID)
{
  nsPRUint32Key key(inCommandID);
  return reinterpret_cast<nsIMenuItem*>(mObserverTable.Get(&key));
}


nsEventStatus
MenuHelpersX::DispatchCommandTo(nsIContent* aTargetContent)
{
  NS_PRECONDITION(aTargetContent, "null ptr");

  nsEventStatus status = nsEventStatus_eConsumeNoDefault;
  nsXULCommandEvent event(PR_TRUE, NS_XUL_COMMAND, nsnull);
  
  // FIXME: Should probably figure out how to init this with the actual
  // pressed keys, but this is a big old edge case anyway. -dwh
  
  aTargetContent->DispatchDOMEvent(&event, nsnull, nsnull, &status);
  return status;
}


NSString* MenuHelpersX::CreateTruncatedCocoaLabel(const nsString& itemLabel)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // ::TruncateThemeText() doesn't take the number of characters to truncate to, it takes a pixel with
  // to fit the string in. Ugh. I talked it over with sfraser and we couldn't come up with an 
  // easy way to compute what this should be given the system font, etc, so we're just going
  // to hard code it to something reasonable and bigger fonts will just have to deal.
  const short kMaxItemPixelWidth = 300;
  NSMutableString *label = [[NSMutableString stringWithCharacters:itemLabel.get() length:itemLabel.Length()] retain];
  ::TruncateThemeText((CFMutableStringRef)label, kThemeMenuItemFont, kThemeStateActive, kMaxItemPixelWidth, truncMiddle, NULL);
  return label; // caller releases

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}


PRUint8 MenuHelpersX::GeckoModifiersForNodeAttribute(const nsString& modifiersAttribute)
{
  PRUint8 modifiers = knsMenuItemNoModifier;
  char* str = ToNewCString(modifiersAttribute);
  char* newStr;
  char* token = nsCRT::strtok(str, ", \t", &newStr);
  while (token != NULL) {
    if (PL_strcmp(token, "shift") == 0)
      modifiers |= knsMenuItemShiftModifier;
    else if (PL_strcmp(token, "alt") == 0) 
      modifiers |= knsMenuItemAltModifier;
    else if (PL_strcmp(token, "control") == 0) 
      modifiers |= knsMenuItemControlModifier;
    else if ((PL_strcmp(token, "accel") == 0) ||
             (PL_strcmp(token, "meta") == 0)) {
      modifiers |= knsMenuItemCommandModifier;
    }
    token = nsCRT::strtok(newStr, ", \t", &newStr);
  }
  nsMemory::Free(str);

  return modifiers;
}


unsigned int MenuHelpersX::MacModifiersForGeckoModifiers(PRUint8 geckoModifiers)
{
  unsigned int macModifiers = 0;
  
  if (geckoModifiers & knsMenuItemShiftModifier)
    macModifiers |= NSShiftKeyMask;
  if (geckoModifiers & knsMenuItemAltModifier)
    macModifiers |= NSAlternateKeyMask;
  if (geckoModifiers & knsMenuItemControlModifier)
    macModifiers |= NSControlKeyMask;
  if (geckoModifiers & knsMenuItemCommandModifier)
    macModifiers |= NSCommandKeyMask;
  
  return macModifiers;
}


nsIMenuBar* MenuHelpersX::GetHiddenWindowMenuBar()
{
  nsIWidget* hiddenWindowWidgetNoCOMPtr = nsCocoaUtils::GetHiddenWindowWidget();
  if (hiddenWindowWidgetNoCOMPtr)
    return static_cast<nsCocoaWindow*>(hiddenWindowWidgetNoCOMPtr)->GetMenuBar();
  else
    return nsnull;
}


// It would be nice if we could localize these edit menu names.
static NSMenuItem* standardEditMenuItem = nil;
NSMenuItem* MenuHelpersX::GetStandardEditMenuItem()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if (standardEditMenuItem)
    return standardEditMenuItem;

  NSMenuItem* standardEditMenuItem = [[NSMenuItem alloc] initWithTitle:@"Edit" action:nil keyEquivalent:@""];
  NSMenu* standardEditMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
  [standardEditMenuItem setSubmenu:standardEditMenu];
  [standardEditMenu release];

  // Add Undo
  NSMenuItem* undoItem = [[NSMenuItem alloc] initWithTitle:@"Undo" action:@selector(undo:) keyEquivalent:@"z"];
  [standardEditMenu addItem:undoItem];
  [undoItem release];

  // Add Redo
  NSMenuItem* redoItem = [[NSMenuItem alloc] initWithTitle:@"Redo" action:@selector(redo:) keyEquivalent:@"Z"];
  [standardEditMenu addItem:redoItem];
  [redoItem release];

  // Add separator
  [standardEditMenu addItem:[NSMenuItem separatorItem]];
  
  // Add Cut
  NSMenuItem* cutItem = [[NSMenuItem alloc] initWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@"x"];
  [standardEditMenu addItem:cutItem];
  [cutItem release];

  // Add Copy
  NSMenuItem* copyItem = [[NSMenuItem alloc] initWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
  [standardEditMenu addItem:copyItem];
  [copyItem release];

  // Add Paste
  NSMenuItem* pasteItem = [[NSMenuItem alloc] initWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"];
  [standardEditMenu addItem:pasteItem];
  [pasteItem release];

  // Add Delete
  NSMenuItem* deleteItem = [[NSMenuItem alloc] initWithTitle:@"Delete" action:@selector(delete:) keyEquivalent:@""];
  [standardEditMenu addItem:deleteItem];
  [deleteItem release];

  // Add Select All
  NSMenuItem* selectAllItem = [[NSMenuItem alloc] initWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];
  [standardEditMenu addItem:selectAllItem];
  [selectAllItem release];

  standardEditMenuItem = standardEditMenuItem;

  return standardEditMenuItem;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
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
// When this variable is set to YES, don't do special command processing.
static BOOL gMenuEffectsOnly = NO;

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


- (void)performMenuUserInterfaceEffectsForEvent:(NSEvent*)theEvent
{
  gMenuEffectsOnly = YES;
  [self performKeyEquivalent:theEvent];
  gMenuEffectsOnly = NO;
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

  // We want to avoid processing app-global commands when we are asked to
  // perform native menu effects only. This avoids sending events twice,
  // which can lead to major problems.
  if (!gMenuEffectsOnly) {
    // Do special processing if this is for an app-global command.
    if (tag == eCommand_ID_About) {
      nsIContent* mostSpecificContent = sAboutItemContent;
      if (menuBar && menuBar->mAboutItemContent)
        mostSpecificContent = menuBar->mAboutItemContent;
      MenuHelpersX::DispatchCommandTo(mostSpecificContent);
    }
    else if (tag == eCommand_ID_Prefs) {
      nsIContent* mostSpecificContent = sPrefItemContent;
      if (menuBar && menuBar->mPrefItemContent)
        mostSpecificContent = menuBar->mPrefItemContent;
      MenuHelpersX::DispatchCommandTo(mostSpecificContent);
    }
    else if (tag == eCommand_ID_Quit) {
      nsIContent* mostSpecificContent = sQuitItemContent;
      if (menuBar && menuBar->mQuitItemContent)
        mostSpecificContent = menuBar->mQuitItemContent;
      // If we have some content for quit we execute it. Otherwise we send a native app terminate
      // message. If you want to stop a quit from happening, provide quit content and return
      // the event as unhandled.
      if (mostSpecificContent) {
        MenuHelpersX::DispatchCommandTo(mostSpecificContent);
      }
      else {
        [NSApp terminate:nil];
        return;
      }
    }
  }

  // Don't do anything unless this is not a keyboard command and
  // this isn't for the hidden window menu. We assume that if there
  // is no main window then the hidden window menu bar is up, even
  // if that isn't true for some reason we better play it safe if
  // there is no main window.
  if (gPerformKeyEquivOnStack && [NSApp mainWindow])
    return;

  // given the commandID, look it up in our hashtable and dispatch to
  // that content node. Recall that we store weak pointers to the content
  // nodes in the hash table.
  if (menuBar) {
    nsIMenuItem* content = menuBar->GetMenuItemForCommandID(static_cast<PRUint32>(tag));
    if (content)
      content->DoCommand();    
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

@end
