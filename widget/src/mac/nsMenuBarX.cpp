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
#include "nsDynamicMDEF.h"

#include "nsISupports.h"
#include "nsIWidget.h"
#include "nsString.h"
#include "nsIStringBundle.h"
#include "nsIDocument.h"
#include "nsIDocShell.h"
#include "nsIDocumentViewer.h"
#include "nsIDocumentObserver.h"

#include "nsIDOMDocument.h"
#include "nsWidgetAtoms.h"

#include <Menus.h>
#include <TextUtils.h>
#include <Balloons.h>
#include <Traps.h>
#include <Resources.h>
#include <Appearance.h>
#include <Gestalt.h>
#include "nsMacResources.h"

#include "nsGUIEvent.h"

#if !XP_MACOSX
#include "MenuSharing.h"
#endif

// CIDs
#include "nsWidgetsCID.h"
static NS_DEFINE_CID(kMenuCID, NS_MENU_CID);

NS_IMPL_ISUPPORTS6(nsMenuBarX, nsIMenuBar, nsIMenuListener, nsIDocumentObserver, 
                    nsIChangeManager, nsIMenuCommandDispatcher, nsISupportsWeakReference)

MenuRef nsMenuBarX::sAppleMenu = nsnull;
EventHandlerUPP nsMenuBarX::sCommandEventHandler = nsnull;


//
// nsMenuBarX constructor
//
nsMenuBarX::nsMenuBarX()
  : mNumMenus(0), mParent(nsnull), mIsMenuBarAdded(PR_FALSE), mDocument(nsnull), mCurrentCommandID(1)
{
  OSStatus status = ::CreateNewMenu(0, 0, &mRootMenu);
  NS_ASSERTION(status == noErr, "nsMenuBarX::nsMenuBarX:  creation of root menu failed.");
  
  // create our global carbon event command handler shared by all windows
  if ( !sCommandEventHandler )
    sCommandEventHandler = ::NewEventHandlerUPP(CommandEventHandler);
}

//
// nsMenuBarX destructor
//
nsMenuBarX::~nsMenuBarX()
{
  mMenusArray.Clear();    // release all menus

  // make sure we unregister ourselves as a document observer
  if ( mDocument ) {
    nsCOMPtr<nsIDocumentObserver> observer ( do_QueryInterface(NS_STATIC_CAST(nsIMenuBar*,this)) );
    mDocument->RemoveObserver(observer);
  }

  if ( mRootMenu )
    ::ReleaseMenu(mRootMenu);
}

nsEventStatus 
nsMenuBarX::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  // Dispatch menu event
  nsEventStatus eventStatus = nsEventStatus_eIgnore;

  PRUint32  numItems;
  mMenusArray.Count(&numItems);
  
  for (PRUint32 i = numItems; i > 0; --i)
  {
    nsCOMPtr<nsISupports>     menuSupports = getter_AddRefs(mMenusArray.ElementAt(i - 1));
    nsCOMPtr<nsIMenuListener> menuListener = do_QueryInterface(menuSupports);
    if(menuListener)
    {
      eventStatus = menuListener->MenuItemSelected(aMenuEvent);
      if (nsEventStatus_eIgnore != eventStatus)
        return eventStatus;
    }
  }
  return eventStatus;
}


nsEventStatus 
nsMenuBarX::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  // Dispatch event
  nsEventStatus eventStatus = nsEventStatus_eIgnore;

  nsCOMPtr<nsIMenuListener> menuListener;
  //((nsISupports*)mMenuVoidArray[i-1])->QueryInterface(NS_GET_IID(nsIMenuListener), (void**)&menuListener);
  //printf("gPreviousMenuStack.Count() = %d \n", gPreviousMenuStack.Count());
#if !TARGET_CARBON
  nsCOMPtr<nsIMenu> theMenu;
  gPreviousMenuStack.GetMenuAt(gPreviousMenuStack.Count() - 1, getter_AddRefs(theMenu));
  menuListener = do_QueryInterface(theMenu);
#endif
  if (menuListener) {
    //TODO: MenuSelected is the right thing to call...
    //eventStatus = menuListener->MenuSelected(aMenuEvent);
    eventStatus = menuListener->MenuItemSelected(aMenuEvent);
    if (nsEventStatus_eIgnore != eventStatus)
      return eventStatus;
  } else {
    // If it's the help menu, gPreviousMenuStack won't be accurate so we need to get the listener a different way 
    // We'll do it the old fashioned way of looping through and finding it
    PRUint32  numItems;
    mMenusArray.Count(&numItems);
    for (PRUint32 i = numItems; i > 0; --i)
    {
      nsCOMPtr<nsISupports>     menuSupports = getter_AddRefs(mMenusArray.ElementAt(i - 1));
      nsCOMPtr<nsIMenuListener> thisListener = do_QueryInterface(menuSupports);
	    if (thisListener)
	    {
        //TODO: MenuSelected is the right thing to call...
  	    //eventStatus = menuListener->MenuSelected(aMenuEvent);
  	    eventStatus = thisListener->MenuItemSelected(aMenuEvent);
  	    if(nsEventStatus_eIgnore != eventStatus)
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
nsMenuBarX :: GetDocument ( nsIWebShell* inWebShell, nsIDocument** outDocument )
{
  *outDocument = nsnull;
  
  nsCOMPtr<nsIDocShell> docShell ( do_QueryInterface(inWebShell) );
  nsCOMPtr<nsIContentViewer> cv;
  if ( docShell ) {
    docShell->GetContentViewer(getter_AddRefs(cv));
    if (cv) {
      // get the document
      nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
      if (!docv)
        return;
      docv->GetDocument(outDocument);    // addrefs
    }
  }
}


//
// RegisterAsDocumentObserver
//
// Name says it all.
//
void
nsMenuBarX :: RegisterAsDocumentObserver ( nsIWebShell* inWebShell )
{
  nsCOMPtr<nsIDocument> doc;
  GetDocument(inWebShell, getter_AddRefs(doc));
  if (!doc)
    return;

  // register ourselves
  nsCOMPtr<nsIDocumentObserver> observer ( do_QueryInterface(NS_STATIC_CAST(nsIMenuBar*,this)) );
  doc->AddObserver(observer);
  // also get pointer to doc, just in case webshell goes away
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
nsMenuBarX :: AquifyMenuBar ( )
{
  nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(mMenuBarContent->GetDocument()));
  if ( domDoc ) {
    // remove quit item and its separator
    HideItem ( domDoc, NS_LITERAL_STRING("menu_FileQuitSeparator"), nsnull );
    HideItem ( domDoc, NS_LITERAL_STRING("menu_FileQuitItem"), getter_AddRefs(mQuitItemContent) );
  
    // remove prefs item and its separator, but save off the pref content node
    // so we can invoke its command later.
    HideItem ( domDoc, NS_LITERAL_STRING("menu_PrefsSeparator"), nsnull );
    HideItem ( domDoc, NS_LITERAL_STRING("menu_preferences"), getter_AddRefs(mPrefItemContent) );
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
nsMenuBarX :: InstallCommandEventHandler ( )
{
  OSStatus err = noErr;
  
  WindowRef myWindow = NS_REINTERPRET_CAST(WindowRef, mParent->GetNativeData(NS_NATIVE_DISPLAY));
  NS_ASSERTION ( myWindow, "Can't get WindowRef to install command handler!" );
  if ( myWindow && sCommandEventHandler ) {
    const EventTypeSpec commandEventList[] = { {kEventClassCommand, kEventCommandProcess},
                                               {kEventClassCommand, kEventCommandUpdateStatus} };
    err = ::InstallWindowEventHandler ( myWindow, sCommandEventHandler, 2, commandEventList, this, NULL );
    NS_ASSERTION ( err == noErr, "Uh oh, command handler not installed" );
  }

  return err;
  
} // InstallCommandEventHandler


//
// CommandEventHandler
//
// Processes Command carbon events from enabling/selecting of items in the menu.
//
pascal OSStatus
nsMenuBarX :: CommandEventHandler ( EventHandlerCallRef inHandlerChain, EventRef inEvent, void* userData )
{
  OSStatus handled = eventNotHandledErr;

  HICommand command;
  OSErr err1 = ::GetEventParameter ( inEvent, kEventParamDirectObject, typeHICommand,
                                        NULL, sizeof(HICommand), NULL, &command );	
  if ( err1 )
    return handled;
    
  nsMenuBarX* self = NS_REINTERPRET_CAST(nsMenuBarX*, userData);
  switch ( ::GetEventKind(inEvent) ) {
    // user selected a menu item. See if it's one we handle.
    case kEventCommandProcess:
    {
      switch ( command.commandID ) {
        case kHICommandPreferences:
        {
          nsEventStatus status = self->ExecuteCommand(self->mPrefItemContent);
          if ( status == nsEventStatus_eConsumeNoDefault )    // event handled, no other processing
            handled = noErr;
          break;
        }
        
        case kHICommandQuit:
        {
          nsEventStatus status = self->ExecuteCommand(self->mQuitItemContent);
          if ( status == nsEventStatus_eConsumeNoDefault )    // event handled, no other processing
            handled = noErr;
          break;
        }
        
        case kHICommandAbout:
        {
          // the 'about' command is special because we don't have a
          // nsIMenu or nsIMenuItem for the apple menu. Grovel for the
          // content node with an id of "aboutName" and call it
          // directly.
          nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(self->mDocument);
	        if ( domDoc ) {
      	    nsCOMPtr<nsIDOMElement> domElement;
      	    domDoc->GetElementById(NS_LITERAL_STRING("aboutName"),
                                   getter_AddRefs(domElement));
      	    nsCOMPtr<nsIContent> aboutContent ( do_QueryInterface(domElement) );
      	    self->ExecuteCommand(aboutContent);
          }
          handled = noErr;
          break;
        }

#if !XP_MACOSX
        case 'SHMN':
        { // Shared menu support
          MenuItemIndex index = command.menu.menuItemIndex;
          if (index)
          {
            (void)SharedMenuHit(GetMenuID(command.menu.menuRef), command.menu.menuItemIndex);
            ::HiliteMenu(0);
            handled = noErr;
          }
          break;
        }
#endif

        default:
        {
          // given the commandID, look it up in our hashtable and dispatch to
          // that content node. Recall that we store weak pointers to the content
          // nodes in the hash table.
          nsPRUint32Key key ( command.commandID );
          nsIMenuItem* content = NS_REINTERPRET_CAST(nsIMenuItem*, self->mObserverTable.Get(&key));
          if ( content ) {
            content->DoCommand();
            handled = noErr;
          }        
          break; 
        }        

      } // switch on commandID
      break;
    }
    
    // enable/disable menu id's
    case kEventCommandUpdateStatus:
    {
      // only enable the preferences item in the app menu if we found a pref
      // item DOM node in this menubar.
      if ( command.commandID == kHICommandPreferences ) {
        if ( self->mPrefItemContent )
          ::EnableMenuCommand ( nsnull, kHICommandPreferences );
        else
          ::DisableMenuCommand ( nsnull, kHICommandPreferences );
        handled = noErr;
      }
      break;
    }
  } // switch on event type
  
  return handled;
  
} // CommandEventHandler


//
// ExecuteCommand
//
// Execute the menu item by sending a command message to the 
// DOM node specified in |inDispatchTo|.
//
nsEventStatus
nsMenuBarX :: ExecuteCommand ( nsIContent* inDispatchTo )
{
  if (!inDispatchTo)
    return nsEventStatus_eIgnore;

  return MenuHelpersX::DispatchCommandTo(mWebShellWeakRef, inDispatchTo);
} // ExecuteCommand


//
// HideItem
//
// Hide the item in the menu by setting the 'hidden' attribute. Returns it in |outHiddenNode| so
// the caller can hang onto it if they so choose. It is acceptable to pass nsull
// for |outHiddenNode| if the caller doesn't care about the hidden node.
//
void
nsMenuBarX :: HideItem ( nsIDOMDocument* inDoc, const nsAString & inID, nsIContent** outHiddenNode )
{
  nsCOMPtr<nsIDOMElement> menuItem;
  inDoc->GetElementById(inID, getter_AddRefs(menuItem));  
  nsCOMPtr<nsIContent> menuContent ( do_QueryInterface(menuItem) );
  if ( menuContent ) {
    menuContent->SetAttr ( kNameSpaceID_None, nsWidgetAtoms::hidden, NS_LITERAL_STRING("true"), PR_FALSE );
    if ( outHiddenNode ) {
      *outHiddenNode = menuContent.get();
      NS_IF_ADDREF(*outHiddenNode);
    }
  }

} // HideItem


nsEventStatus
nsMenuBarX::MenuConstruct( const nsMenuEvent & aMenuEvent, nsIWidget* aParentWindow, 
                            void * menubarNode, void * aWebShell )
{
  mWebShellWeakRef = do_GetWeakReference(NS_STATIC_CAST(nsIWebShell*, aWebShell));
  nsIDOMNode* aDOMNode  = NS_STATIC_CAST(nsIDOMNode*, menubarNode);
  mMenuBarContent = do_QueryInterface(aDOMNode);           // strong ref
  NS_ASSERTION ( mMenuBarContent, "No content specified for this menubar" );
  if ( !mMenuBarContent )
    return nsEventStatus_eIgnore;
    
  Create(aParentWindow);
  
  // if we're on X (using aqua UI guidelines for menus), remove quit and prefs
  // from our menubar.
  SInt32 result = 0L;
  OSStatus err = ::Gestalt ( gestaltMenuMgrAttr, &result );
  if ( !err && (result & gestaltMenuMgrAquaLayoutMask) )
    AquifyMenuBar();
  err = InstallCommandEventHandler();
  if ( err )
    return nsEventStatus_eIgnore;

  nsCOMPtr<nsIWebShell> webShell = do_QueryReferent(mWebShellWeakRef);
  if (webShell) RegisterAsDocumentObserver(webShell);

  // set this as a nsMenuListener on aParentWindow
  aParentWindow->AddMenuListener((nsIMenuListener *)this);

  PRUint32 count = mMenuBarContent->GetChildCount();
  for ( PRUint32 i = 0; i < count; ++i ) { 
    nsIContent *menu = mMenuBarContent->GetChildAt(i);
    if ( menu ) {
      if (menu->Tag() == nsWidgetAtoms::menu &&
          menu->IsContentOfType(nsIContent::eXUL)) {
        nsAutoString menuName;
        nsAutoString menuAccessKey(NS_LITERAL_STRING(" "));
        menu->GetAttr(kNameSpaceID_None, nsWidgetAtoms::label, menuName);
        menu->GetAttr(kNameSpaceID_None, nsWidgetAtoms::accesskey, menuAccessKey);
			  
        // Don't create the whole menu yet, just add in the top level names
              
        // Create nsMenu, the menubar will own it
        nsCOMPtr<nsIMenu> pnsMenu ( do_CreateInstance(kMenuCID) );
        if ( pnsMenu ) {
          pnsMenu->Create(NS_STATIC_CAST(nsIMenuBar*, this), menuName, menuAccessKey, 
                          NS_STATIC_CAST(nsIChangeManager *, this), 
                          NS_REINTERPRET_CAST(nsIWebShell*, aWebShell), menu);

          // Make nsMenu a child of nsMenuBar. nsMenuBar takes ownership
          AddMenu(pnsMenu); 
                  
          nsAutoString menuIDstring;
          menu->GetAttr(kNameSpaceID_None, nsWidgetAtoms::id, menuIDstring);
          if ( menuIDstring.EqualsLiteral("menu_Help") ) {
            nsMenuEvent event;
            MenuHandle handle = nsnull;
#if !TARGET_CARBON
            ::HMGetHelpMenuHandle(&handle);
#endif
            event.mCommand = (unsigned int) handle;
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


//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::Create(nsIWidget *aParent)
{
  SetParent(aParent);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::GetParent(nsIWidget *&aParent)
{
  NS_IF_ADDREF(aParent = mParent);
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::SetParent(nsIWidget *aParent)
{
  mParent = aParent;    // weak ref  
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::AddMenu(nsIMenu * aMenu)
{
  // keep track of all added menus.
  mMenusArray.AppendElement(aMenu);    // owner

  if (mNumMenus == 0) {
    // if apple menu hasn't been created, create it.
    if ( !sAppleMenu ) {
      nsresult rv = CreateAppleMenu(aMenu);
      NS_ASSERTION ( NS_SUCCEEDED(rv), "Can't create Apple menu" );
    }
    
    // add shared Apple menu to our menubar
    if ( sAppleMenu ) {
      // InsertMenuItem() is 1-based, so the apple/application menu needs to
      // be at index 1. |mNumMenus| will be incremented below, so the following menu (File)
      // won't overwrite the apple menu by reusing the ID.
      mNumMenus = 1;
      ::InsertMenuItem(mRootMenu, "\pA", mNumMenus);
      OSStatus status = ::SetMenuItemHierarchicalMenu(mRootMenu, 1, sAppleMenu);
    }
  }

  MenuRef menuRef = nsnull;
  aMenu->GetNativeData((void**)&menuRef);

  PRBool helpMenu;
  aMenu->IsHelpMenu(&helpMenu);
  if(!helpMenu) {
    nsCOMPtr<nsIContent> menu;
    aMenu->GetMenuContent(getter_AddRefs(menu));
    nsAutoString menuHidden;
    menu->GetAttr(kNameSpaceID_None, nsWidgetAtoms::hidden, menuHidden);
    if( !menuHidden.EqualsLiteral("true")) {
    	// make sure we only increment |mNumMenus| if the menu is visible, since
    	// we use it as an index of where to insert the next menu.
      mNumMenus++;
      
      ::InsertMenuItem(mRootMenu, "\pPlaceholder", mNumMenus);
      OSStatus status = ::SetMenuItemHierarchicalMenu(mRootMenu, mNumMenus, menuRef);
      NS_ASSERTION(status == noErr, "nsMenuBarX::AddMenu: SetMenuItemHierarchicalMenu failed.");
    }
  }

  return NS_OK;
}


//
// CreateAppleMenu
//
// build the Apple menu shared by all menu bars.
//
nsresult
nsMenuBarX :: CreateAppleMenu ( nsIMenu* inMenu )
{
  Str32 menuStr = { 1, kMenuAppleLogoFilledGlyph };
  OSStatus s = ::CreateNewMenu(kAppleMenuID, 0, &sAppleMenu);

  if ( s == noErr && sAppleMenu )  {
    ::SetMenuTitle(sAppleMenu, menuStr);
    
    // this code reads the "label" attribute from the <menuitem/> with
    // id="aboutName" and puts its label in the Apple Menu
    nsAutoString label;
    nsCOMPtr<nsIContent> menu;
    inMenu->GetMenuContent(getter_AddRefs(menu));
    if (menu) {
      nsCOMPtr<nsIDocument> doc = menu->GetDocument();
      if (doc) {
        nsCOMPtr<nsIDOMDocument> domdoc ( do_QueryInterface(doc) );
        if ( domdoc ) {
          nsCOMPtr<nsIDOMElement> aboutMenuItem;
          domdoc->GetElementById(NS_LITERAL_STRING("aboutName"), getter_AddRefs(aboutMenuItem));
          if (aboutMenuItem)
            aboutMenuItem->GetAttribute(NS_LITERAL_STRING("label"), label);
        }
      }
    }

    CFStringRef labelRef = ::CFStringCreateWithCharacters(kCFAllocatorDefault, (UniChar*)label.get(), label.Length());
    if ( labelRef ) {
      ::InsertMenuItemTextWithCFString(sAppleMenu, labelRef, 1, 0, 0);
      ::CFRelease(labelRef);
    }
    
    ::SetMenuItemCommandID(sAppleMenu, 1, kHICommandAbout);

    ::AppendMenu(sAppleMenu, "\p-");
  }

  return (s == noErr && sAppleMenu) ? NS_OK : NS_ERROR_FAILURE;
}

        
//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::GetMenuCount(PRUint32 &aCount)
{
  aCount = mNumMenus;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::GetMenuAt(const PRUint32 aCount, nsIMenu *& aMenu)
{ 
  aMenu = NULL;
  nsCOMPtr<nsISupports> supports = getter_AddRefs(mMenusArray.ElementAt(aCount));
  if (!supports) return NS_OK;
  
  return CallQueryInterface(supports, &aMenu); // addref
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::InsertMenuAt(const PRUint32 aCount, nsIMenu *& aMenu)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::RemoveMenu(const PRUint32 aCount)
{
  mMenusArray.RemoveElementAt(aCount);
  ::DeleteMenuItem(mRootMenu, aCount + 1);    // MenuManager is 1-based
  ::DrawMenuBar();
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::RemoveAll()
{
  NS_ASSERTION(0, "Not implemented!");
  // mMenusArray.Clear();    // maybe?
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::GetNativeData(void *& aData)
{
    aData = (void *) mRootMenu;
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::SetNativeData(void* aData)
{
#if 0
    Handle menubarHandle = (Handle)aData;
    if (mMacMBarHandle && mMacMBarHandle != menubarHandle)
        ::DisposeHandle(mMacMBarHandle);
    mMacMBarHandle = menubarHandle;
#endif
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuBarX::Paint()
{
    // hack to correctly swap menu bars.
    // hopefully this is fast enough.
    ::SetRootMenu(mRootMenu);
    ::DrawMenuBar();
    return NS_OK;
}

#pragma mark -

//
// nsIDocumentObserver
// this is needed for menubar changes
//

NS_IMPL_NSIDOCUMENTOBSERVER_LOAD_STUB(nsMenuBarX)
NS_IMPL_NSIDOCUMENTOBSERVER_REFLOW_STUB(nsMenuBarX)
NS_IMPL_NSIDOCUMENTOBSERVER_STATE_STUB(nsMenuBarX)
NS_IMPL_NSIDOCUMENTOBSERVER_STYLE_STUB(nsMenuBarX)

void
nsMenuBarX::BeginUpdate( nsIDocument * aDocument, nsUpdateType aUpdateType )
{
}

void
nsMenuBarX::EndUpdate( nsIDocument * aDocument, nsUpdateType aUpdateType )
{
}

void
nsMenuBarX::CharacterDataChanged( nsIDocument * aDocument,
                                  nsIContent * aContent, PRBool aAppend)
{
}

void
nsMenuBarX::ContentAppended( nsIDocument * aDocument, nsIContent  * aContainer,
                              PRInt32 aNewIndexInContainer)
{
  if ( aContainer == mMenuBarContent ) {
    //Register(aContainer, );
    //InsertMenu ( aNewIndexInContainer );
  }
  else {
    nsCOMPtr<nsIChangeObserver> obs;
    Lookup ( aContainer, getter_AddRefs(obs) );
    if ( obs )
      obs->ContentInserted ( aDocument, aContainer, aNewIndexInContainer );
    else {
      nsCOMPtr<nsIContent> parent = aContainer->GetParent();
      if(parent) {
        Lookup ( parent, getter_AddRefs(obs) );
        if ( obs )
          obs->ContentInserted ( aDocument, aContainer, aNewIndexInContainer );
      }
    }
  }
}

void
nsMenuBarX::DocumentWillBeDestroyed( nsIDocument * aDocument )
{
  mDocument = nsnull;
}


void
nsMenuBarX::AttributeChanged( nsIDocument * aDocument, nsIContent * aContent,
                              PRInt32 aNameSpaceID, nsIAtom * aAttribute,
                              PRInt32 aModType )
{
  // lookup and dispatch to registered thang.
  nsCOMPtr<nsIChangeObserver> obs;
  Lookup ( aContent, getter_AddRefs(obs) );
  if ( obs )
    obs->AttributeChanged ( aDocument, aNameSpaceID, aAttribute );
}

void
nsMenuBarX::ContentRemoved( nsIDocument * aDocument, nsIContent * aContainer,
                            nsIContent * aChild, PRInt32 aIndexInContainer )
{  
  if ( aContainer == mMenuBarContent ) {
    Unregister(aChild);
    RemoveMenu ( aIndexInContainer );
  }
  else {
    nsCOMPtr<nsIChangeObserver> obs;
    Lookup ( aContainer, getter_AddRefs(obs) );
    if ( obs )
      obs->ContentRemoved ( aDocument, aChild, aIndexInContainer );
    else {
      nsCOMPtr<nsIContent> parent = aContainer->GetParent();
      if(parent) {
        Lookup ( parent, getter_AddRefs(obs) );
        if ( obs )
          obs->ContentRemoved ( aDocument, aChild, aIndexInContainer );
      }
    }
  }
}

void
nsMenuBarX::ContentInserted( nsIDocument * aDocument, nsIContent * aContainer,
                             nsIContent * aChild, PRInt32 aIndexInContainer )
{  
  if ( aContainer == mMenuBarContent ) {
    //Register(aChild, );
    //InsertMenu ( aIndexInContainer );
  }
  else {
    nsCOMPtr<nsIChangeObserver> obs;
    Lookup ( aContainer, getter_AddRefs(obs) );
    if ( obs )
      obs->ContentInserted ( aDocument, aChild, aIndexInContainer );
    else {
      nsCOMPtr<nsIContent> parent = aContainer->GetParent();
      if(parent) {
        Lookup ( parent, getter_AddRefs(obs) );
        if ( obs )
          obs->ContentInserted ( aDocument, aChild, aIndexInContainer );
      }
    }
  }
}

#pragma mark - 

//
// nsIChangeManager
//
// We don't use a |nsSupportsHashtable| because we know that the lifetime of all these items
// is bouded by the lifetime of the menubar. No need to add any more strong refs to the
// picture because the containment hierarchy already uses strong refs.
//

NS_IMETHODIMP 
nsMenuBarX :: Register ( nsIContent *aContent, nsIChangeObserver *aMenuObject )
{
  nsVoidKey key ( aContent );
  mObserverTable.Put ( &key, aMenuObject );
  
  return NS_OK;
}


NS_IMETHODIMP 
nsMenuBarX :: Unregister ( nsIContent *aContent )
{
  nsVoidKey key ( aContent );
  mObserverTable.Remove ( &key );
  
  return NS_OK;
}


NS_IMETHODIMP 
nsMenuBarX :: Lookup ( nsIContent *aContent, nsIChangeObserver **_retval )
{
  *_retval = nsnull;
  
  nsVoidKey key ( aContent );
  *_retval = NS_REINTERPRET_CAST(nsIChangeObserver*, mObserverTable.Get(&key));
  NS_IF_ADDREF ( *_retval );
  
  return NS_OK;
}


#pragma mark -


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
nsMenuBarX :: Register ( nsIMenuItem* inMenuItem, PRUint32* outCommandID )
{
  // no real need to check for uniqueness. We always start afresh with each
  // window at 1. Even if we did get close to the reserved Apple command id's,
  // those don't start until at least '    ', which is integer 538976288. If
  // we have that many menu items in one window, I think we have other problems.
  
  // put it in the table, set out param for client
  nsPRUint32Key key ( mCurrentCommandID );
  mObserverTable.Put ( &key, inMenuItem );
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
nsMenuBarX :: Unregister ( PRUint32 inCommandID )
{
  nsPRUint32Key key ( inCommandID );
  mObserverTable.Remove ( &key );

  return NS_OK;
}


#pragma mark -


//
// WebShellToPresContext
//
// Helper to dig out a pres context from a webshell. A common thing to do before
// sending an event into the dom.
//
nsresult
MenuHelpersX::WebShellToPresContext (nsIWebShell* inWebShell, nsIPresContext** outContext )
{
  NS_ENSURE_ARG_POINTER(outContext);
  *outContext = nsnull;
  if (!inWebShell)
    return NS_ERROR_INVALID_ARG;
  
  nsresult retval = NS_OK;
  
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(inWebShell));

  nsCOMPtr<nsIContentViewer> contentViewer;
  docShell->GetContentViewer(getter_AddRefs(contentViewer));
  if ( contentViewer ) {
    nsCOMPtr<nsIDocumentViewer> docViewer ( do_QueryInterface(contentViewer) );
    if ( docViewer )
      docViewer->GetPresContext(outContext);     // AddRefs for us
    else
      retval = NS_ERROR_FAILURE;
  }
  else
    retval = NS_ERROR_FAILURE;
  
  return retval;
  
} // WebShellToPresContext

nsEventStatus
MenuHelpersX::DispatchCommandTo(nsIWeakReference* aWebShellWeakRef,
                                nsIContent* aTargetContent)
{
  NS_PRECONDITION(aTargetContent, "null ptr");

  nsCOMPtr<nsIWebShell> webShell = do_QueryReferent(aWebShellWeakRef);
  if (!webShell)
    return nsEventStatus_eConsumeNoDefault;
  nsCOMPtr<nsIPresContext> presContext;
  MenuHelpersX::WebShellToPresContext(webShell, getter_AddRefs(presContext));

  nsEventStatus status = nsEventStatus_eConsumeNoDefault;
  nsMouseEvent event(NS_XUL_COMMAND);

  // FIXME: Should probably figure out how to init this with the actual
  // pressed keys, but this is a big old edge case anyway. -dwh

  // See if we have a command element.  If so, we execute on the
  // command instead of on our content element.
  nsAutoString command;
  aTargetContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::command, command);
  if (!command.IsEmpty()) {
    nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(aTargetContent->GetDocument()));
    nsCOMPtr<nsIDOMElement> commandElt;
    domDoc->GetElementById(command, getter_AddRefs(commandElt));
    nsCOMPtr<nsIContent> commandContent(do_QueryInterface(commandElt));
    if (commandContent)
      commandContent->HandleDOMEvent(presContext, &event, nsnull,
                                     NS_EVENT_FLAG_INIT, &status);
  }
  else
    aTargetContent->HandleDOMEvent(presContext, &event, nsnull,
                                   NS_EVENT_FLAG_INIT, &status);

  return status;
}
