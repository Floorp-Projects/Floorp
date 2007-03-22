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

#include "nsISupports.h"
#include "nsIWidget.h"
#include "nsString.h"
#include "nsIStringBundle.h"
#include "nsIDocument.h"
#include "nsIDocShell.h"
#include "nsIDocumentViewer.h"
#include "nsIMutationObserver.h"

#include "nsIDOMDocument.h"
#include "nsWidgetAtoms.h"

#include "nsGUIEvent.h"

// CIDs
#include "nsWidgetsCID.h"
static NS_DEFINE_CID(kMenuCID, NS_MENU_CID);

NS_IMPL_ISUPPORTS6(nsMenuBarX, nsIMenuBar, nsIMenuListener, nsIMutationObserver, 
                   nsIChangeManager, nsIMenuCommandDispatcher, nsISupportsWeakReference)

EventHandlerUPP nsMenuBarX::sCommandEventHandler = nsnull;
NativeMenuItemTarget* nsMenuBarX::sNativeEventTarget = nil;
NSWindow* nsMenuBarX::sEventTargetWindow = nil;
NSMenu* sApplicationMenu = nil;
BOOL gSomeMenuBarPainted = NO;

// We keep references to the first quit and pref item content nodes we find, which
// will be from the hidden window. We use these when the document for the current
// window does not have a quit or pref item. We don't need strong refs here because
// these items are always strong ref'd by their owning menu bar (instance variable).
static nsIContent* sQuitItemContent = nsnull;
static nsIContent* sPrefItemContent = nsnull;

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
: mParent(nsnull), mIsMenuBarAdded(PR_FALSE), mCurrentCommandID(eCommand_ID_Last), mDocument(nsnull)
{
  mRootMenu = [[NSMenu alloc] initWithTitle:@"MainMenuBar"];
  
  // create our global carbon event command handler shared by all windows
  if (!sCommandEventHandler)
    sCommandEventHandler = ::NewEventHandlerUPP(CommandEventHandler);
}


nsMenuBarX::~nsMenuBarX()
{
  mMenusArray.Clear(); // release all menus

  // the quit/pref items of a random window might have been used if there was no
  // hidden window, thus we need to invalidate the weak references.
  if (sQuitItemContent == mQuitItemContent)
    sQuitItemContent = nsnull;
  if (sPrefItemContent == mPrefItemContent)
    sPrefItemContent = nsnull;

  // make sure we unregister ourselves as a document observer
  if (mDocument)
    mDocument->RemoveMutationObserver(this);
  
  [mRootMenu release];
}

nsEventStatus 
nsMenuBarX::MenuItemSelected(const nsMenuEvent &aMenuEvent)
{
  // Dispatch menu event
  nsEventStatus eventStatus = nsEventStatus_eIgnore;
  
  for (PRInt32 i = mMenusArray.Count() - 1; i >= 0; i--) {
    nsCOMPtr<nsIMenu> menu = mMenusArray.ObjectAt(i);
    nsCOMPtr<nsIMenuListener> menuListener = do_QueryInterface(menu);
    if (menuListener) {
      eventStatus = menuListener->MenuItemSelected(aMenuEvent);
      if (nsEventStatus_eIgnore != eventStatus)
        return eventStatus;
    }
  }
  return eventStatus;
}


nsEventStatus 
nsMenuBarX::MenuSelected(const nsMenuEvent &aMenuEvent)
{
  // Dispatch event
  nsEventStatus eventStatus = nsEventStatus_eIgnore;
  
  nsCOMPtr<nsIMenuListener> menuListener;
  if (menuListener) {
    //TODO: MenuSelected is the right thing to call...
    //eventStatus = menuListener->MenuSelected(aMenuEvent);
    eventStatus = menuListener->MenuItemSelected(aMenuEvent);
    if (nsEventStatus_eIgnore != eventStatus)
      return eventStatus;
  }
  else {
    for (PRInt32 i = mMenusArray.Count() - 1; i >= 0; i--) {
      nsCOMPtr<nsIMenu> menu = mMenusArray.ObjectAt(i);
      nsCOMPtr<nsIMenuListener> thisListener = do_QueryInterface(menu);
      if (thisListener) {
        //TODO: MenuSelected is the right thing to call...
        //eventStatus = menuListener->MenuSelected(aMenuEvent);
        eventStatus = thisListener->MenuItemSelected(aMenuEvent);
        if (nsEventStatus_eIgnore != eventStatus)
          return eventStatus;
      }
    }
  }
  return eventStatus;
}


nsEventStatus 
nsMenuBarX::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}


nsEventStatus 
nsMenuBarX::CheckRebuild(PRBool & aNeedsRebuild)
{
  aNeedsRebuild = PR_TRUE;
  return nsEventStatus_eIgnore;
}


nsEventStatus
nsMenuBarX::SetRebuild(PRBool aNeedsRebuild)
{
  return nsEventStatus_eIgnore;
}


void
nsMenuBarX::GetDocument(nsIDocShell* inDocShell, nsIDocument** outDocument)
{
  *outDocument = nsnull;
  
  if (inDocShell) {
    nsCOMPtr<nsIContentViewer> cv;
    inDocShell->GetContentViewer(getter_AddRefs(cv));
    if (cv) {
      // get the document
      nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
      if (!docv)
        return;
      docv->GetDocument(outDocument); // addrefs
    }
  }
}


void
nsMenuBarX::RegisterAsDocumentObserver(nsIDocShell* inDocShell)
{
  nsCOMPtr<nsIDocument> doc;
  GetDocument(inDocShell, getter_AddRefs(doc));
  if (!doc)
    return;
  
  // register ourselves
  doc->AddMutationObserver(this);
  // also get pointer to doc, just in case docshell goes away
  // we can still remove ourself as doc observer directly from doc
  mDocument = doc;
} // RegisterAsDocumentObesrver


//
// AquifyMenuBar
//
// Do what's necessary to conform to the Aqua guidelines for menus. Initially, this
// means removing 'Quit' from the file menu and 'Preferences' from the edit menu, along
// with their various separators (if present).
//
void
nsMenuBarX::AquifyMenuBar()
{
  nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(mMenuBarContent->GetDocument()));
  if (domDoc) {
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
} // AquifyMenuBar


//
// InstallCommandEventHandler
//
// Grab our window and install an event handler to handle command events which are
// used to drive the action when the user chooses an item from a menu. We have to install
// it on the window because the menubar isn't in the event chain for a menu command event.
//
OSStatus
nsMenuBarX::InstallCommandEventHandler()
{
   OSStatus err = noErr;
   NSWindow* myWindow = NS_REINTERPRET_CAST(NSWindow*, mParent->GetNativeData(NS_NATIVE_WINDOW));
   WindowRef myWindowRef = (WindowRef)[myWindow windowRef];
   NS_ASSERTION(myWindowRef, "Can't get WindowRef to install command handler!");
   if (myWindowRef && sCommandEventHandler) {
     const EventTypeSpec commandEventList[] = {{kEventClassCommand, kEventCommandProcess}};
     err = ::InstallWindowEventHandler(myWindowRef, sCommandEventHandler, GetEventTypeCount(commandEventList), commandEventList, this, NULL);
     NS_ASSERTION(err == noErr, "Uh oh, command handler not installed");
   }
   return err;
} // InstallCommandEventHandler


//
// CommandEventHandler
//
// Processes Command carbon events from enabling/selecting of items in the menu.
//
pascal OSStatus
nsMenuBarX::CommandEventHandler(EventHandlerCallRef inHandlerChain, EventRef inEvent, void* userData)
{
  OSStatus handled = eventNotHandledErr;
  
  HICommand command;
  OSErr err1 = ::GetEventParameter(inEvent, kEventParamDirectObject, typeHICommand,
                                   NULL, sizeof(HICommand), NULL, &command);
  if (err1)
    return handled;
  
  nsMenuBarX* self = NS_REINTERPRET_CAST(nsMenuBarX*, userData);

  switch (command.commandID) {
    case eCommand_ID_Prefs:
    {
      nsIContent* mostSpecificContent = sPrefItemContent;
      if (self->mPrefItemContent)
        mostSpecificContent = self->mPrefItemContent;
      
      nsEventStatus status = self->ExecuteCommand(mostSpecificContent);
      if (status == nsEventStatus_eConsumeNoDefault) // event handled, no other processing
        handled = noErr;
      break;
    }

    case eCommand_ID_Quit:
    {
      nsIContent* mostSpecificContent = sQuitItemContent;
      if (self->mQuitItemContent)
        mostSpecificContent = self->mQuitItemContent;

      // If we have some content for quit we execute it. Otherwise we send a native app terminate
      // message. If you want to stop a quit from happening, provide quit content and return
      // the event as unhandled.
      if (mostSpecificContent) {
        nsEventStatus status = self->ExecuteCommand(mostSpecificContent);
        if (status == nsEventStatus_eConsumeNoDefault) // event handled, no other processing
          handled = noErr;
      }
      else {
        [NSApp terminate:nil];
        handled = noErr;
      }

      break;
    }

    case eCommand_ID_About:
    {
      // the 'about' command is special because we don't have a
      // nsIMenu or nsIMenuItem for the application menu. Grovel for the
      // content node with an id of "aboutName" and call it
      // directly.
      nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(self->mDocument);
      if (domDoc) {
        nsCOMPtr<nsIDOMElement> domElement;
        domDoc->GetElementById(NS_LITERAL_STRING("aboutName"),
                               getter_AddRefs(domElement));
        nsCOMPtr<nsIContent> aboutContent(do_QueryInterface(domElement));
        self->ExecuteCommand(aboutContent);
      }
      handled = noErr;
      break;
    }

    default:
    {
      // given the commandID, look it up in our hashtable and dispatch to
      // that content node. Recall that we store weak pointers to the content
      // nodes in the hash table.
      nsPRUint32Key key(command.commandID);
      nsIMenuItem* content = NS_REINTERPRET_CAST(nsIMenuItem*, self->mObserverTable.Get(&key));
      if (content) {
        content->DoCommand();
        handled = noErr;
      }
      break;
    }
  } // switch on commandID
  
  return handled;
} // CommandEventHandler


//
// ExecuteCommand
//
// Execute the menu item by sending a command message to the 
// DOM node specified in |inDispatchTo|.
//
nsEventStatus
nsMenuBarX::ExecuteCommand(nsIContent* inDispatchTo)
{
  if (!inDispatchTo)
    return nsEventStatus_eIgnore;
  
  return MenuHelpersX::DispatchCommandTo(mDocShellWeakRef, inDispatchTo);
} // ExecuteCommand


//
// HideItem
//
// Hide the item in the menu by setting the 'hidden' attribute. Returns it in |outHiddenNode| so
// the caller can hang onto it if they so choose. It is acceptable to pass nsull
// for |outHiddenNode| if the caller doesn't care about the hidden node.
//
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
} // HideItem


nsEventStatus
nsMenuBarX::MenuConstruct(const nsMenuEvent & aMenuEvent, nsIWidget* aParentWindow, 
                          void * menubarNode, void * aDocShell)
{
  mDocShellWeakRef = do_GetWeakReference(NS_STATIC_CAST(nsIDocShell*, aDocShell));
  nsIDOMNode* aDOMNode  = NS_STATIC_CAST(nsIDOMNode*, menubarNode);
  mMenuBarContent = do_QueryInterface(aDOMNode); // strong ref
  NS_ASSERTION(mMenuBarContent, "No content specified for this menubar");
  if (!mMenuBarContent)
    return nsEventStatus_eIgnore;
  
  Create(aParentWindow);
  
  AquifyMenuBar();
  
  OSStatus err = InstallCommandEventHandler();
  if (err)
    return nsEventStatus_eIgnore;
  
  nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(mDocShellWeakRef);
  if (docShell)
    RegisterAsDocumentObserver(docShell);
  
  // set this as a nsMenuListener on aParentWindow
  aParentWindow->AddMenuListener((nsIMenuListener *)this);
  
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

        // Don't create the whole menu yet, just add in the top level names
        
        // Create nsMenu, the menubar will own it
        nsCOMPtr<nsIMenu> pnsMenu(do_CreateInstance(kMenuCID));
        if (pnsMenu) {
          pnsMenu->Create(NS_STATIC_CAST(nsIMenuBar*, this), menuName, menuAccessKey, 
                          NS_STATIC_CAST(nsIChangeManager *, this), 
                          NS_REINTERPRET_CAST(nsIDocShell*, aDocShell), menu);
          
          // Make nsMenu a child of nsMenuBar. nsMenuBar takes ownership.
          AddMenu(pnsMenu);
          
          if (menu->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::id,
                                NS_LITERAL_STRING("menu_Help"), eCaseMatters)) {
            nsMenuEvent event(PR_TRUE, 0, nsnull);
            event.mCommand = (unsigned int)nsnull;
            nsCOMPtr<nsIMenuListener> listener(do_QueryInterface(pnsMenu));
            listener->MenuSelected(event);
          }
        }
      } 
    }
  } // for each menu
  
  // Give the aParentWindow this nsMenuBarX to hold onto.
  // The parent takes ownership
  aParentWindow->SetMenuBar(this);
  
  return nsEventStatus_eIgnore;
}


nsEventStatus 
nsMenuBarX::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
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
  // if no menus have been added yet, add a menu item as a placeholder for
  // the application menu (the NSMenu of which gets swapped in on Paint())
  if (mMenusArray.Count() == 0) {
    [mRootMenu insertItem:[[[NSMenuItem alloc] initWithTitle:@"AppMenu" action:NULL keyEquivalent:@""] autorelease] atIndex:0];
    
    // if we haven't generated an application menu yet, then we can use this
    // nsIMenu to create one
    if (!sApplicationMenu) {
      nsresult rv = NS_OK; // avoid warning about rv being unused
      rv = CreateApplicationMenu(aMenu);
      NS_ASSERTION(NS_SUCCEEDED(rv), "Can't create Application menu");
    }
  }

  // keep track of all added menus
  mMenusArray.AppendObject(aMenu); // owner
  
  NSMenu* menuRef = NULL;
  aMenu->GetNativeData((void**)&menuRef);
  
  nsCOMPtr<nsIContent> menu;
  aMenu->GetMenuContent(getter_AddRefs(menu));
  if (menu->GetChildCount() > 0 &&
      !NodeIsHiddenOrCollapsed(menu)) {
    NSMenuItem* newMenuItem = [[[NSMenuItem alloc] initWithTitle:@"SomeMenuItem" action:NULL keyEquivalent:@""] autorelease];
    [mRootMenu addItem:newMenuItem];
    [newMenuItem setSubmenu:menuRef];
  }
  
  return NS_OK;
}


// for creating menu items destined for the Application menu
NSMenuItem* nsMenuBarX::CreateNativeAppMenuItem(nsIMenu* inMenu, const nsAString& nodeID, SEL action,
                                                int tag, NativeMenuItemTarget* target)
{
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
      char* str = ToNewCString(modifiersStr);
      PRUint8 geckoModifiers = MenuHelpersX::GeckoModifiersForNodeAttribute(str);
      nsMemory::Free(str);
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
}


//
// CreateApplicationMenu
//
// build the Application menu shared by all menu bars.
//
nsresult
nsMenuBarX::CreateApplicationMenu(nsIMenu* inMenu)
{
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
}


//-------------------------------------------------------------------------
NS_IMETHODIMP nsMenuBarX::GetMenuCount(PRUint32 &aCount)
{
  aCount = mMenusArray.Count();
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsMenuBarX::GetMenuAt(const PRUint32 aCount, nsIMenu *& aMenu)
{ 
  aMenu = NULL;
  nsCOMPtr<nsIMenu> menu = mMenusArray.ObjectAt(aCount);
  if (!menu)
    return NS_OK;
  
  return CallQueryInterface(menu, &aMenu); // addref
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsMenuBarX::InsertMenuAt(const PRUint32 aCount, nsIMenu *& aMenu)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsMenuBarX::RemoveMenu(const PRUint32 aCount)
{
  mMenusArray.RemoveObjectAt(aCount);
  [mRootMenu removeItemAtIndex:aCount];
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsMenuBarX::RemoveAll()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsMenuBarX::GetNativeData(void *& aData)
{
  aData = (void *) mRootMenu;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsMenuBarX::SetNativeData(void* aData)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsMenuBarX::Paint()
{
  // swap in the shared Application menu
  // if application menu hasn't been created, create it.
  if (sApplicationMenu) {
    // an NSMenu can't have multiple supermenus, so when we paint a menu bar we unhook the
    // application menu from its current supermenu and hook it up to this menu bar's
    // application menu item. This way all changes to the application menu perist across
    // all instances of nsMenuBarX. We could assume 0 for indexOfItemWithSubmenu, but lets
    // be safe... If the algorithm for that starts looking at 0 it will still be fast.
    NSMenu* supermenu = [sApplicationMenu supermenu];
    if (supermenu) {
      int supermenuItemIndex = [supermenu indexOfItemWithSubmenu:sApplicationMenu];
      [[supermenu itemAtIndex:supermenuItemIndex] setSubmenu:nil];
    }
    [[mRootMenu itemAtIndex:0] setSubmenu:sApplicationMenu];
  }
  
  [NSApp setMainMenu:mRootMenu];
  nsMenuBarX::sEventTargetWindow = (NSWindow*)mParent->GetNativeData(NS_NATIVE_WINDOW);

  gSomeMenuBarPainted = YES;
  
  return NS_OK;
}


//
// nsIMutationObserver
// this is needed for menubar changes
//

void
nsMenuBarX::CharacterDataChanged(nsIDocument * aDocument,
                                 nsIContent * aContent,
                                 CharacterDataChangeInfo * aInfo)
{
}

void
nsMenuBarX::ContentAppended(nsIDocument * aDocument, nsIContent  * aContainer,
                            PRInt32 aNewIndexInContainer)
{
  if (aContainer != mMenuBarContent) {
    nsCOMPtr<nsIChangeObserver> obs;
    Lookup(aContainer, getter_AddRefs(obs));
    if (obs)
      obs->ContentInserted(aDocument, aContainer, aNewIndexInContainer);
    else {
      nsCOMPtr<nsIContent> parent = aContainer->GetParent();
      if (parent) {
        Lookup(parent, getter_AddRefs(obs));
        if (obs)
          obs->ContentInserted(aDocument, aContainer, aNewIndexInContainer);
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
                             PRInt32 aModType)
{
  // lookup and dispatch to registered thang
  nsCOMPtr<nsIChangeObserver> obs;
  Lookup(aContent, getter_AddRefs(obs));
  if (obs)
    obs->AttributeChanged(aDocument, aNameSpaceID, aContent, aAttribute);
}

void
nsMenuBarX::ContentRemoved(nsIDocument * aDocument, nsIContent * aContainer,
                            nsIContent * aChild, PRInt32 aIndexInContainer)
{  
  if (aContainer == mMenuBarContent) {
    Unregister(aChild);
    RemoveMenu(aIndexInContainer);
  }
  else {
    nsCOMPtr<nsIChangeObserver> obs;
    Lookup (aContainer, getter_AddRefs(obs));
    if (obs)
      obs->ContentRemoved(aDocument, aChild, aIndexInContainer);
    else {
      nsCOMPtr<nsIContent> parent = aContainer->GetParent();
      if (parent) {
        Lookup (parent, getter_AddRefs(obs));
        if (obs)
          obs->ContentRemoved(aDocument, aChild, aIndexInContainer);
      }
    }
  }
}

void
nsMenuBarX::ContentInserted(nsIDocument * aDocument, nsIContent * aContainer,
                             nsIContent * aChild, PRInt32 aIndexInContainer)
{  
  if (aContainer != mMenuBarContent) {
    nsCOMPtr<nsIChangeObserver> obs;
    Lookup (aContainer, getter_AddRefs(obs));
    if (obs)
      obs->ContentInserted (aDocument, aChild, aIndexInContainer);
    else {
      nsCOMPtr<nsIContent> parent = aContainer->GetParent();
      if (parent) {
        Lookup (parent, getter_AddRefs(obs));
        if (obs)
          obs->ContentInserted(aDocument, aChild, aIndexInContainer);
      }
    }
  }
}

void
nsMenuBarX::ParentChainChanged(nsIContent *aContent)
{
}

//
// nsIChangeManager
//
// We don't use a |nsSupportsHashtable| because we know that the lifetime of all these items
// is bounded by the lifetime of the menubar. No need to add any more strong refs to the
// picture because the containment hierarchy already uses strong refs.
//

NS_IMETHODIMP
nsMenuBarX::Register(nsIContent *aContent, nsIChangeObserver *aMenuObject)
{
  nsVoidKey key(aContent);
  mObserverTable.Put(&key, aMenuObject);
  return NS_OK;
}


NS_IMETHODIMP
nsMenuBarX::Unregister(nsIContent *aContent)
{
  nsVoidKey key(aContent);
  mObserverTable.Remove(&key);
  return NS_OK;
}


NS_IMETHODIMP
nsMenuBarX::Lookup(nsIContent *aContent, nsIChangeObserver **_retval)
{
  *_retval = nsnull;
  
  nsVoidKey key (aContent);
  *_retval = NS_REINTERPRET_CAST(nsIChangeObserver*, mObserverTable.Get(&key));
  NS_IF_ADDREF (*_retval);
  
  return NS_OK;
}


//
// Implementation methods for nsIMenuCommandDispatcher
//


//
// Register
//
// Given a menu item, creates a unique 4-character command ID and
// maps it to the item. Returns the id for use by the client.
//
NS_IMETHODIMP
nsMenuBarX::Register(nsIMenuItem* inMenuItem, PRUint32* outCommandID)
{
  // no real need to check for uniqueness. We always start afresh with each
  // window at 1. Even if we did get close to the reserved Apple command id's,
  // those don't start until at least '    ', which is integer 538976288. If
  // we have that many menu items in one window, I think we have other problems.

  // put it in the table, set out param for client
  nsPRUint32Key key(mCurrentCommandID);
  mObserverTable.Put(&key, inMenuItem);
  *outCommandID = mCurrentCommandID;
  
  // make id unique for next time
  ++mCurrentCommandID;
  
  return NS_OK;
}


// 
// Unregister
//
// Removes the mapping between the given 4-character command ID
// and its associated menu item.
//
NS_IMETHODIMP
nsMenuBarX::Unregister(PRUint32 inCommandID)
{
  nsPRUint32Key key(inCommandID);
  mObserverTable.Remove(&key);
  return NS_OK;
}


nsEventStatus
MenuHelpersX::DispatchCommandTo(nsIWeakReference* aDocShellWeakRef,
                                nsIContent* aTargetContent)
{
  NS_PRECONDITION(aTargetContent, "null ptr");
  
  nsCOMPtr<nsIDocShell> docShell = do_QueryReferent(aDocShellWeakRef);
  if (!docShell)
    return nsEventStatus_eConsumeNoDefault;
  
  nsEventStatus status = nsEventStatus_eConsumeNoDefault;
  nsXULCommandEvent event(PR_TRUE, NS_XUL_COMMAND, nsnull);
  
  // FIXME: Should probably figure out how to init this with the actual
  // pressed keys, but this is a big old edge case anyway. -dwh
  
  aTargetContent->DispatchDOMEvent(&event, nsnull, nsnull, &status);
  return status;
}

NSString* MenuHelpersX::CreateTruncatedCocoaLabel(nsString itemLabel)
{
  // ::TruncateThemeText() doesn't take the number of characters to truncate to, it takes a pixel with
  // to fit the string in. Ugh. I talked it over with sfraser and we couldn't come up with an 
  // easy way to compute what this should be given the system font, etc, so we're just going
  // to hard code it to something reasonable and bigger fonts will just have to deal.
  const short kMaxItemPixelWidth = 300;
  NSMutableString *label = [[NSMutableString stringWithCharacters:itemLabel.get() length:itemLabel.Length()] retain];
  ::TruncateThemeText((CFMutableStringRef)label, kThemeMenuItemFont, kThemeStateActive, kMaxItemPixelWidth, truncMiddle, NULL);
  return label; // caller releases
}

PRUint8 MenuHelpersX::GeckoModifiersForNodeAttribute(char* modifiersAttribute)
{
  PRUint8 modifiers = knsMenuItemNoModifier;
  char* newStr;
  char* token = nsCRT::strtok(modifiersAttribute, ", \t", &newStr);
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

// Objective-C class used as action target for menu items

@implementation NativeMenuItemTarget

// called when some menu item in this menu gets hit
-(IBAction)menuItemHit:(id)sender
{
  // just abort if we don't have a target window for events
  if (!nsMenuBarX::sEventTargetWindow) {
    NS_WARNING("No target window for keyboard events!");
    return;
  }
  
  MenuRef senderMenuRef = _NSGetCarbonMenu([sender menu]);
  int senderCarbonMenuItemIndex = [[sender menu] indexOfItem:sender] + 1;
  
  // If this carbon menu item has never had a command ID assigned to it, give it
  // one from the sender's tag
  MenuCommand menuCommand;
  ::GetMenuItemCommandID(senderMenuRef, senderCarbonMenuItemIndex, &menuCommand);
  if (menuCommand == 0) {
    menuCommand = (MenuCommand)[sender tag];
    ::SetMenuItemCommandID(senderMenuRef, senderCarbonMenuItemIndex, menuCommand);
  }
  
  // set up an HICommand to send
  HICommand menuHICommand;
  menuHICommand.commandID = menuCommand;
  menuHICommand.menu.menuRef = senderMenuRef;
  menuHICommand.menu.menuItemIndex = senderCarbonMenuItemIndex;
  
  // send Carbon Event
  EventRef newEvent;
  OSErr err = ::CreateEvent(NULL, kEventClassCommand, kEventCommandProcess, 0, kEventAttributeUserEvent, &newEvent);
  if (err == noErr) {
    err = ::SetEventParameter(newEvent, kEventParamDirectObject, typeHICommand, sizeof(HICommand), &menuHICommand);
    if (err == noErr) {
      err = ::SendEventToEventTarget(newEvent, GetWindowEventTarget((WindowRef)[nsMenuBarX::sEventTargetWindow windowRef]));
      NS_ASSERTION(err == noErr, "Carbon event for menu hit either not sent or not handled!");
    }
    ReleaseEvent(newEvent);
  }
}

@end
