/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocumentViewer.h"
#include "nsIDocumentObserver.h"
#include "nsIComponentManager.h"
#include "nsIDocShell.h"
#include "prinrval.h"

#include "nsMenuX.h"
#include "nsMenubarX.h"
#include "nsIMenu.h"
#include "nsIMenuBar.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"
#include "nsIPresContext.h"

#include "nsString.h"
#include "nsStringUtil.h"

#include "nsINameSpaceManager.h"
#include "nsWidgetAtoms.h"
#include "nsIXBLService.h"
#include "nsIServiceManager.h"

#include <Appearance.h>
#include <TextUtils.h>
#include <ToolUtils.h>
#include <Devices.h>
#include <UnicodeConverter.h>
#include <Fonts.h>
#include <Sound.h>
#include <Balloons.h>
#include <CarbonEvents.h>

#include "nsGUIEvent.h"

#include "nsDynamicMDEF.h"


// keep track of the menuID of the menu the mouse is currently over. Yes, this is ugly,
// but necessary to work around bugs in Carbon with ::MenuSelect() sometimes returning
// the wrong menuID.
static MenuID gCurrentlyTrackedMenuID = 0;

const PRInt16 kMacMenuIDX = nsMenuBarX::kAppleMenuID + 1;
static PRInt16 gMacMenuIDCountX = kMacMenuIDX;
static PRBool gConstructingMenu = PR_FALSE;
  
#if DEBUG
nsInstanceCounter   gMenuCounterX("nsMenuX");
#endif

// CIDs
#include "nsWidgetsCID.h"
static NS_DEFINE_CID(kMenuCID,     NS_MENU_CID);
static NS_DEFINE_CID(kMenuItemCID, NS_MENUITEM_CID);

// Refcounted class for dummy menu items, like separators and help menu items.
class nsDummyMenuItemX : public nsISupports {
public:
    NS_DECL_ISUPPORTS

    nsDummyMenuItemX()
    {
        NS_INIT_REFCNT();
    }
};

NS_IMETHODIMP_(nsrefcnt) nsDummyMenuItemX::AddRef() { return ++mRefCnt; }
NS_METHOD nsDummyMenuItemX::Release() { return --mRefCnt; }
NS_IMPL_QUERY_INTERFACE0(nsDummyMenuItemX)
static nsDummyMenuItemX gDummyMenuItemX;

//-------------------------------------------------------------------------
NS_IMPL_ISUPPORTS4(nsMenuX, nsIMenu, nsIMenuListener, nsIChangeObserver, nsISupportsWeakReference)

//
// nsMenuX constructor
//
nsMenuX::nsMenuX()
    :   mNumMenuItems(0), mParent(nsnull), mManager(nsnull),
        mMacMenuID(0), mMacMenuHandle(nsnull), mHelpMenuOSItemsCount(0),
        mIsHelpMenu(PR_FALSE), mIsEnabled(PR_TRUE), mDestroyHandlerCalled(PR_FALSE),
        mNeedsRebuild(PR_TRUE), mConstructed(PR_FALSE)
{
  NS_INIT_REFCNT();

#if DEBUG
  ++gMenuCounterX;
#endif 
}


//
// nsMenuX destructor
//
nsMenuX::~nsMenuX()
{
  RemoveAll();

  if (mMacMenuHandle != NULL)
    ::ReleaseMenu(mMacMenuHandle);

  // alert the change notifier we don't care no more
  mManager->Unregister(mMenuContent);

#if DEBUG
  --gMenuCounterX;
#endif
}


//
// Create
//
NS_METHOD 
nsMenuX::Create(nsISupports * aParent, const nsAReadableString &aLabel, const nsAReadableString &aAccessKey, 
                nsIChangeManager* aManager, nsIWebShell* aShell, nsIContent* aNode )
{
  mWebShellWeakRef = getter_AddRefs(NS_GetWeakReference(aShell));
  mMenuContent = aNode;

  // register this menu to be notified when changes are made to our content object
  mManager = aManager;			// weak ref
  nsCOMPtr<nsIChangeObserver> changeObs ( do_QueryInterface(NS_STATIC_CAST(nsIChangeObserver*, this)) );
  mManager->Register(mMenuContent, changeObs);

  NS_ASSERTION ( mMenuContent, "Menu not given a dom node at creation time" );
  NS_ASSERTION ( mManager, "No change manager given, can't tell content model updates" );

  mParent = aParent;
  // our parent could be either a menu bar (if we're toplevel) or a menu (if we're a submenu)
  PRBool isValidParent = PR_FALSE;
  if (aParent) {
    nsCOMPtr<nsIMenuBar> menubar = do_QueryInterface(aParent);
    nsCOMPtr<nsIMenu> menu = do_QueryInterface(aParent);
    isValidParent = (menubar || menu);
  }
  NS_ASSERTION(isValidParent, "Menu parent not a menu bar or menu!" );

  SetLabel(aLabel);
  SetAccessKey(aAccessKey);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuX::GetParent(nsISupports*& aParent)
{
  aParent = mParent;
  NS_IF_ADDREF(aParent);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuX::GetLabel(nsString &aText)
{
  aText = mLabel;
  return NS_OK;
}

static OSStatus InstallMyMenuEventHandler(MenuRef menuRef, void* userData);

//-------------------------------------------------------------------------
NS_METHOD nsMenuX::SetLabel(const nsAReadableString &aText)
{
  mLabel = aText;

  // first time? create the menu handle, attach event handler to it.
  if (mMacMenuHandle == nsnull) {
    mMacMenuID = gMacMenuIDCountX++;
    mMacMenuHandle = NSStringNewMenu(mMacMenuID, mLabel);
  }
  
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuX::GetAccessKey(nsString &aText)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuX::SetAccessKey(const nsAReadableString &aText)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuX::AddItem(nsISupports* aItem)
{
    nsresult rv = NS_ERROR_FAILURE;
    if (aItem) {
        // Figure out what we're adding
        nsCOMPtr<nsIMenuItem> menuItem(do_QueryInterface(aItem));
        if (menuItem) {
            rv = AddMenuItem(menuItem);
        } else {
            nsCOMPtr<nsIMenu> menu(do_QueryInterface(aItem));
            if (menu)
                rv = AddMenu(menu);
        }
    }
    return rv;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuX::AddMenuItem(nsIMenuItem * aMenuItem)
{
  if(!aMenuItem) return NS_ERROR_NULL_POINTER;

  mMenuItemsArray.AppendElement(aMenuItem);    // owning ref
  PRUint32 currItemIndex;
  mMenuItemsArray.Count(&currItemIndex);

  mNumMenuItems++;

  nsAutoString label;
  aMenuItem->GetLabel(label);
  CFStringRef labelRef = ::CFStringCreateWithCharacters(kCFAllocatorDefault, (UniChar*)label.get(), label.Length());
  ::InsertMenuItemTextWithCFString(mMacMenuHandle, labelRef, currItemIndex, 0, 0);
  ::CFRelease(labelRef);
  
  // I want to be internationalized too!
  nsAutoString keyEquivalent(NS_LITERAL_STRING(" "));
  aMenuItem->GetShortcutChar(keyEquivalent);
  if (keyEquivalent != NS_LITERAL_STRING(" ")) {
    keyEquivalent.ToUpperCase();
    char keyStr[2];
    keyEquivalent.ToCString(keyStr, sizeof(keyStr));
    short inKey = keyStr[0];
    ::SetItemCmd(mMacMenuHandle, currItemIndex, inKey);
    //::SetMenuItemKeyGlyph(mMacMenuHandle, mNumMenuItems, 0x61);
  }

  PRUint8 modifiers;
  aMenuItem->GetModifiers(&modifiers);
  PRUint8 macModifiers = kMenuNoModifiers;
  if (knsMenuItemShiftModifier & modifiers)
    macModifiers |= kMenuShiftModifier;

  if (knsMenuItemAltModifier & modifiers)
    macModifiers |= kMenuOptionModifier;

  if (knsMenuItemControlModifier & modifiers)
    macModifiers |= kMenuControlModifier;

  if (!(knsMenuItemCommandModifier & modifiers))
    macModifiers |= kMenuNoCommandModifier;

  ::SetMenuItemModifiers(mMacMenuHandle, currItemIndex, macModifiers);

  PRBool isEnabled;
  aMenuItem->GetEnabled(&isEnabled);
  if(isEnabled)
    ::EnableMenuItem(mMacMenuHandle, currItemIndex);
  else
    ::DisableMenuItem(mMacMenuHandle, currItemIndex);

  PRBool isChecked;
  aMenuItem->GetChecked(&isChecked);
  if(isChecked)
    ::CheckMenuItem(mMacMenuHandle, currItemIndex, true);
  else
    ::CheckMenuItem(mMacMenuHandle, currItemIndex, false);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuX::AddMenu(nsIMenu * aMenu)
{
  // Add a submenu
  if (!aMenu) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsISupports>  supports = do_QueryInterface(aMenu);
  if (!supports) return NS_ERROR_NO_INTERFACE;

  mMenuItemsArray.AppendElement(supports);   // owning ref
  PRUint32 currItemIndex;
  mMenuItemsArray.Count(&currItemIndex);

  mNumMenuItems++;

  // We have to add it as a menu item and then associate it with the item
  nsAutoString label;
  aMenu->GetLabel(label);
  //printf("AddMenu %s \n", label.ToNewCString());

  CFStringRef labelRef = ::CFStringCreateWithCharacters(kCFAllocatorDefault, (UniChar*)label.get(), label.Length());
  ::InsertMenuItemTextWithCFString(mMacMenuHandle, labelRef, currItemIndex, 0, 0);
  ::CFRelease(labelRef);

  PRBool isEnabled;
  aMenu->GetEnabled(&isEnabled);
  if (isEnabled)
    ::EnableMenuItem(mMacMenuHandle, currItemIndex);
  else
    ::DisableMenuItem(mMacMenuHandle, currItemIndex);	    

  MenuHandle childMenu;
  if (aMenu->GetNativeData(&(void*)childMenu) == NS_OK)
    ::SetMenuItemHierarchicalMenu((MenuHandle) mMacMenuHandle, currItemIndex, childMenu);
  
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuX::AddSeparator() 
{
  // HACK - We're not really appending an nsMenuItem but it 
  // needs to be here to make sure that event dispatching isn't off by one.
  mMenuItemsArray.AppendElement(&gDummyMenuItemX);   // owning ref
  PRUint32  numItems;
  mMenuItemsArray.Count(&numItems);
  ::InsertMenuItem(mMacMenuHandle, "\p(-", numItems);
  mNumMenuItems++;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuX::GetItemCount(PRUint32 &aCount)
{
  return mMenuItemsArray.Count(&aCount);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuX::GetItemAt(const PRUint32 aPos, nsISupports *& aMenuItem)
{
  mMenuItemsArray.GetElementAt(aPos, &aMenuItem);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuX::InsertItemAt(const PRUint32 aPos, nsISupports * aMenuItem)
{
  NS_ASSERTION(0, "Not implemented");
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuX::RemoveItem(const PRUint32 aPos)
{
  NS_WARNING("Not implemented");
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuX::RemoveAll()
{
  if (mMacMenuHandle != NULL)
    ::DeleteMenuItems(mMacMenuHandle, 1, ::CountMenuItems(mMacMenuHandle));
  mMenuItemsArray.Clear();    // remove all items
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuX::GetNativeData(void ** aData)
{
  *aData = mMacMenuHandle;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuX::SetNativeData(void * aData)
{
  mMacMenuHandle = (MenuHandle) aData;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuX::AddMenuListener(nsIMenuListener * aMenuListener)
{
  mListener = aMenuListener;    // strong ref
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuX::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
  if (aMenuListener == mListener)
    mListener = nsnull;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// nsIMenuListener interface
//
//-------------------------------------------------------------------------
nsEventStatus nsMenuX::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  //printf("MenuItemSelected called \n");
  nsEventStatus eventStatus = nsEventStatus_eIgnore;

  // Determine if this is the correct menu to handle the event. We can't use
  // the HiWord of the command because of MenuSelect bugs in Carbon. Use
  // the menuID we've been tracking manually instead.
  // PRInt16 menuID = HiWord(((nsMenuEvent)aMenuEvent).mCommand);
  MenuID menuID = gCurrentlyTrackedMenuID;

  if( menuID == nsMenuBarX::kAppleMenuID ) {
    PRInt16 menuItemID = LoWord(((nsMenuEvent)aMenuEvent).mCommand);
    if (menuItemID == 1) {
			/* handle about app here */
			nsresult rv = NS_ERROR_FAILURE;
		 
	    // Go find the about menu item
	    if (!mMenuContent)
      	return nsEventStatus_eConsumeNoDefault;
	    nsCOMPtr<nsIDocument> doc; 
	    mMenuContent->GetDocument(*getter_AddRefs(doc));
	    if (!doc)
      	return nsEventStatus_eConsumeNoDefault;
	    nsCOMPtr<nsIDOMXULDocument> xulDoc = do_QueryInterface(doc);
	    if (!xulDoc) {
      	NS_ERROR("nsIDOMDocument to nsIDOMXULDocument QI failed.");
      	return nsEventStatus_eConsumeNoDefault;
	  	}
	    
	    // "aboutName" is the element id for the "About &shortBrandName;"
	    // <menuitem/>. This is the glue code which causes any script code
	    // in the <menuitem/> to be executed.
	    nsCOMPtr<nsIDOMElement> domElement;
	    xulDoc->GetElementById(NS_LITERAL_STRING("aboutName"), getter_AddRefs(domElement));
	    if (!domElement)
	      	return nsEventStatus_eConsumeNoDefault;
	    
	    // Now get the pres context so we can execute the command
	  	nsCOMPtr<nsIWebShell> webShell = do_QueryReferent(mWebShellWeakRef);
	  	if (!webShell)
	  	    return nsEventStatus_eConsumeNoDefault;
	  	nsCOMPtr<nsIPresContext> presContext;
	  	MenuHelpersX::WebShellToPresContext(webShell, getter_AddRefs(presContext));

	  	nsEventStatus status = nsEventStatus_eIgnore;
	  	nsMouseEvent event;
	  	event.eventStructType = NS_MOUSE_EVENT;
	  	event.message = NS_XUL_COMMAND;

	  	nsCOMPtr<nsIContent> contentNode = do_QueryInterface(domElement);
	  	if (!contentNode)
	      	return nsEventStatus_eConsumeNoDefault;

	  	rv = contentNode->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
	  	return nsEventStatus_eConsumeNoDefault;
		}
	}
	else if (mMacMenuID == menuID)
  {
    // Call MenuItemSelected on the correct nsMenuItem
    PRInt16 menuItemID = LoWord(((nsMenuEvent)aMenuEvent).mCommand);
    
    nsCOMPtr<nsISupports>     menuSupports = getter_AddRefs(mMenuItemsArray.ElementAt(menuItemID - 1));
    NS_ASSERTION(menuSupports, "Somehow our item list was torn down prematurely");
    nsCOMPtr<nsIMenuListener> menuListener = do_QueryInterface(menuSupports);
    if (menuListener)
    {
      // call our ondestroy handler now because the menu is going away.
      // do it now before sending the event into the dom in case our window
      // goes away.
      OnDestroy();
      
      eventStatus = menuListener->MenuItemSelected(aMenuEvent);
      if(nsEventStatus_eIgnore != eventStatus)
        return eventStatus;
    }
  } 

  // Make sure none of our submenus are the ones that should be handling this
  PRUint32    numItems;
  mMenuItemsArray.Count(&numItems);
  for (PRUint32 i = numItems; i > 0; i--)
  {
    nsCOMPtr<nsISupports>     menuSupports = getter_AddRefs(mMenuItemsArray.ElementAt(i - 1));    
    nsCOMPtr<nsIMenu>         submenu = do_QueryInterface(menuSupports);
    nsCOMPtr<nsIMenuListener> menuListener = do_QueryInterface(submenu);
    if (menuListener)
    {
      // call our ondestroy handler now because the menu is going away.
      // do it now before sending the event into the dom in case our window
      // goes away.
      OnDestroy();
      
      eventStatus = menuListener->MenuItemSelected(aMenuEvent);
      if(nsEventStatus_eIgnore != eventStatus)
        return eventStatus;
    }
  }
  
  return eventStatus;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuX::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  //printf("MenuSelected called for %s \n", mLabel.ToNewCString());
  nsEventStatus eventStatus = nsEventStatus_eIgnore;

  // Determine if this is the correct menu to handle the event
  MenuHandle selectedMenuHandle = (MenuHandle) aMenuEvent.mCommand;

  if (mMacMenuHandle == selectedMenuHandle) {
    if (mIsHelpMenu && mConstructed){
      RemoveAll();
      mConstructed = false;
      SetRebuild(PR_TRUE);
    }

    // Open the node.
    mMenuContent->SetAttr(kNameSpaceID_None, nsWidgetAtoms::open, NS_LITERAL_STRING("true"), PR_TRUE);
  

    // Fire our oncreate handler. If we're told to stop, don't build the menu at all
    PRBool keepProcessing = OnCreate();

    if (!mIsHelpMenu && !mNeedsRebuild || !keepProcessing)
      return nsEventStatus_eConsumeNoDefault;

    if(!mConstructed || mNeedsRebuild) {
      if (mNeedsRebuild)
        RemoveAll();

      nsCOMPtr<nsIWebShell> webShell = do_QueryReferent(mWebShellWeakRef);
      if (!webShell) {
        NS_ERROR("No web shell");
        return nsEventStatus_eConsumeNoDefault;
      }
      if (mIsHelpMenu) {
        HelpMenuConstruct(aMenuEvent, nsnull /* mParentWindow */, nsnull, webShell);	      
        mConstructed = true;
      } else {
        MenuConstruct(aMenuEvent, nsnull /* mParentWindow */, nsnull, webShell);
        mConstructed = true;
      }	
    } 

    OnCreated();  // Now that it's built, fire the popupShown event.

    eventStatus = nsEventStatus_eConsumeNoDefault;  
  } 
  else {
    // Make sure none of our submenus are the ones that should be handling this
    PRUint32    numItems;
    mMenuItemsArray.Count(&numItems);
    for (PRUint32 i = numItems; i > 0; i--) {
      nsCOMPtr<nsISupports>     menuSupports = getter_AddRefs(mMenuItemsArray.ElementAt(i - 1));    
      nsCOMPtr<nsIMenu>         submenu = do_QueryInterface(menuSupports);
      nsCOMPtr<nsIMenuListener> menuListener = do_QueryInterface(submenu);
      if (menuListener) {
        eventStatus = menuListener->MenuSelected(aMenuEvent);
        if (nsEventStatus_eIgnore != eventStatus)
          return eventStatus;
      }  
    }
  }

  return eventStatus;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuX::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  // Destroy the menu
  if (mConstructed) {
    MenuDestruct(aMenuEvent);
    mConstructed = false;
  }
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuX::MenuConstruct(
    const nsMenuEvent & aMenuEvent,
    nsIWidget         * aParentWindow, 
    void              * /* menuNode */,
	  void              * aWebShell)
{
  mConstructed = false;
  gConstructingMenu = PR_TRUE;
  
  // reset destroy handler flag so that we'll know to fire it next time this menu goes away.
  mDestroyHandlerCalled = PR_FALSE;
  
  //printf("nsMenuX::MenuConstruct called for %s = %d \n", mLabel.ToNewCString(), mMacMenuHandle);
  // Begin menuitem inner loop
  
  // Retrieve our menupopup.
  nsCOMPtr<nsIContent> menuPopup;
  GetMenuPopupContent(getter_AddRefs(menuPopup));
  if (!menuPopup)
    return nsEventStatus_eIgnore;
      
  // Iterate over the kids
  PRInt32 count;
  menuPopup->ChildCount(count);
  for ( PRInt32 i = 0; i < count; ++i ) {
    nsCOMPtr<nsIContent> child;
    menuPopup->ChildAt(i, *getter_AddRefs(child));
    if ( child ) {
      // depending on the type, create a menu item, separator, or submenu
      nsCOMPtr<nsIAtom> tag;
      child->GetTag ( *getter_AddRefs(tag) );
      if ( tag == nsWidgetAtoms::menuitem )
        LoadMenuItem(this, child);
      else if ( tag == nsWidgetAtoms::menuseparator )
        AddSeparator();
      else if ( tag == nsWidgetAtoms::menu )
        LoadSubMenu(this, child);
    }
  } // for each menu item
  
  gConstructingMenu = PR_FALSE;
  mNeedsRebuild = PR_FALSE;
  //printf("  Done building, mMenuItemVoidArray.Count() = %d \n", mMenuItemVoidArray.Count());
  
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuX::HelpMenuConstruct(
    const nsMenuEvent & aMenuEvent,
    nsIWidget         * aParentWindow, 
    void              * /* menuNode */,
    void              * aWebShell)
{
  //printf("nsMenuX::MenuConstruct called for %s = %d \n", mLabel.ToNewCString(), mMacMenuHandle);
 
  int numHelpItems = ::CountMenuItems(mMacMenuHandle);
  for (int i=0; i < numHelpItems; ++i) {
    mMenuItemsArray.AppendElement(&gDummyMenuItemX);
  }
     
  // Retrieve our menupopup.
  nsCOMPtr<nsIContent> menuPopup;
  GetMenuPopupContent(getter_AddRefs(menuPopup));
  if (!menuPopup)
    return nsEventStatus_eIgnore;
      
  // Iterate over the kids
  PRInt32 count;
  menuPopup->ChildCount(count);
  for ( PRInt32 i = 0; i < count; ++i ) {
    nsCOMPtr<nsIContent> child;
    menuPopup->ChildAt(i, *getter_AddRefs(child));
    if ( child ) {      
      // depending on the type, create a menu item, separator, or submenu
      nsCOMPtr<nsIAtom> tag;
      child->GetTag ( *getter_AddRefs(tag) );
      if ( tag == nsWidgetAtoms::menuitem )
        LoadMenuItem(this, child);
      else if ( tag == nsWidgetAtoms::menuseparator )
        AddSeparator();
      else if ( tag == nsWidgetAtoms::menu )
        LoadSubMenu(this, child);
    }   
  } // for each menu item
  
  //printf("  Done building, mMenuItemVoidArray.Count() = %d \n", mMenuItemVoidArray.Count());
             
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuX::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  //printf("nsMenuX::MenuDestruct() called for %s \n", mLabel.ToNewCString());
  
  // Fire our ondestroy handler. If we're told to stop, don't destroy the menu
  PRBool keepProcessing = OnDestroy();
  if ( keepProcessing ) {
    if(mNeedsRebuild) {
        mConstructed = false;
        //printf("  mMenuItemVoidArray.Count() = %d \n", mMenuItemVoidArray.Count());
    } 
    // Close the node.
    mMenuContent->UnsetAttr(kNameSpaceID_None, nsWidgetAtoms::open, PR_TRUE);

    OnDestroyed();
  }
  
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuX::CheckRebuild(PRBool & aNeedsRebuild)
{
  aNeedsRebuild = PR_TRUE; //mNeedsRebuild;
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuX::SetRebuild(PRBool aNeedsRebuild)
{
  if(!gConstructingMenu) {
    mNeedsRebuild = aNeedsRebuild;
    //if(mNeedsRebuild)
    //  RemoveAll();
  }
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
/**
* Set enabled state
*
*/
NS_METHOD nsMenuX::SetEnabled(PRBool aIsEnabled)
{
  mIsEnabled = aIsEnabled;

  if ( aIsEnabled )
    ::EnableMenuItem(mMacMenuHandle, 0);
  else
    ::DisableMenuItem(mMacMenuHandle, 0);

  return NS_OK;
}

//-------------------------------------------------------------------------
/**
* Get enabled state
*
*/
NS_METHOD nsMenuX::GetEnabled(PRBool* aIsEnabled)
{
  NS_ENSURE_ARG_POINTER(aIsEnabled);
  *aIsEnabled = mIsEnabled;
  return NS_OK;
}

//-------------------------------------------------------------------------
/**
* Query if this is the help menu
*
*/
NS_METHOD nsMenuX::IsHelpMenu(PRBool* aIsHelpMenu)
{
  NS_ENSURE_ARG_POINTER(aIsHelpMenu);
  *aIsHelpMenu = mIsHelpMenu;
  return NS_OK;
}


//-------------------------------------------------------------------------
/**
* Get GetMenuContent
*
*/
NS_METHOD nsMenuX::GetMenuContent(nsIContent ** aMenuContent)
{
  NS_ENSURE_ARG_POINTER(aMenuContent);
  NS_IF_ADDREF(*aMenuContent = mMenuContent);
	return NS_OK;
}


/*
    Support for Carbon Menu Manager.
 */

static pascal OSStatus MyMenuEventHandler(EventHandlerCallRef myHandler, EventRef event, void* userData)
{
  OSStatus result = eventNotHandledErr;

  UInt32 kind = ::GetEventKind(event);
  if (kind == kEventMenuOpening || kind == kEventMenuClosed) {
    nsISupports* supports = reinterpret_cast<nsISupports*>(userData);
    nsCOMPtr<nsIMenuListener> listener(do_QueryInterface(supports));
    if (listener) {
      MenuRef menuRef;
      ::GetEventParameter(event, kEventParamDirectObject, typeMenuRef, NULL, sizeof(menuRef), NULL, &menuRef);
      nsMenuEvent menuEvent;
      menuEvent.message = NS_MENU_SELECTED;
      menuEvent.eventStructType = NS_MENU_EVENT;
      menuEvent.point.x = 0;
      menuEvent.point.y = 0;
      menuEvent.widget = nsnull;
      menuEvent.time = PR_IntervalNow();
      menuEvent.mCommand = (PRUint32) menuRef;
      if (kind == kEventMenuOpening) {
        gCurrentlyTrackedMenuID = ::GetMenuID(menuRef);    // remember which menu ID we're over for later
        listener->MenuSelected(menuEvent);
      }
      else
        listener->MenuDeselected(menuEvent);
    }
  }
  else if ( kind == kEventMenuTargetItem ) {
    // remember which menu ID we're over for later
    MenuRef menuRef;
    ::GetEventParameter(event, kEventParamDirectObject, typeMenuRef, NULL, sizeof(menuRef), NULL, &menuRef);
    gCurrentlyTrackedMenuID = ::GetMenuID(menuRef);
  }
  
  return result;
}

static OSStatus InstallMyMenuEventHandler(MenuRef menuRef, void* userData)
{
  // install the event handler for the various carbon menu events.
  static EventTypeSpec eventList[] = {
      { kEventClassMenu, kEventMenuBeginTracking },
      { kEventClassMenu, kEventMenuEndTracking },
      { kEventClassMenu, kEventMenuChangeTrackingMode },
      { kEventClassMenu, kEventMenuOpening },
      { kEventClassMenu, kEventMenuClosed },
      { kEventClassMenu, kEventMenuTargetItem },
      { kEventClassMenu, kEventMenuMatchKey },
      { kEventClassMenu, kEventMenuEnableItems }
  };
  static EventHandlerUPP gMyMenuEventHandlerUPP = NewEventHandlerUPP(&MyMenuEventHandler);
  return ::InstallMenuEventHandler(menuRef, gMyMenuEventHandlerUPP,
                                   sizeof(eventList) / sizeof(EventTypeSpec), eventList,
                                   userData, NULL);
}

//-------------------------------------------------------------------------
MenuHandle nsMenuX::NSStringNewMenu(short menuID, nsString& menuTitle)
{
  MenuRef menuRef;
  OSStatus status = ::CreateNewMenu(menuID, 0, &menuRef);
  NS_ASSERTION(status == noErr,"nsMenuX::NSStringNewMenu: NewMenu failed.");
  CFStringRef titleRef = ::CFStringCreateWithCharacters(kCFAllocatorDefault, (UniChar*)menuTitle.get(), menuTitle.Length());
  NS_ASSERTION(titleRef,"nsMenuX::NSStringNewMenu: CFStringCreateWithCharacters failed.");
  if (titleRef) {
    ::SetMenuTitleWithCFString(menuRef, titleRef);
    ::CFRelease(titleRef);
  }
  
  status = ::InstallMyMenuEventHandler(menuRef, this);
  NS_ASSERTION(status == noErr,"nsMenuX::NSStringNewMenu: InstallMyMenuEventHandler failed.");

  return menuRef;
}


//----------------------------------------
void nsMenuX::LoadMenuItem( nsIMenu* inParentMenu, nsIContent* inMenuItemContent )
{
  if ( !inMenuItemContent )
    return;

  // if menu should be hidden, bail
  nsAutoString hidden;
  inMenuItemContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::hidden, hidden);
  if ( hidden == NS_LITERAL_STRING("true") )
    return;

  // Create nsMenuItem
  nsCOMPtr<nsIMenuItem> pnsMenuItem = do_CreateInstance ( kMenuItemCID ) ;
  if ( pnsMenuItem ) {
    nsAutoString disabled;
    nsAutoString checked;
    nsAutoString type;
    nsAutoString menuitemName;
    nsAutoString menuitemCmd;
    
    inMenuItemContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, disabled);
    inMenuItemContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::checked, checked);
    inMenuItemContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::type, type);
    inMenuItemContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::label, menuitemName);
    inMenuItemContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::command, menuitemCmd);

    //printf("menuitem %s \n", menuitemName.ToNewCString());
              
    PRBool enabled = ! (disabled == NS_LITERAL_STRING("true"));
    
    nsIMenuItem::EMenuItemType itemType = nsIMenuItem::eRegular;
    if ( type == NS_LITERAL_STRING("checkbox") )
      itemType = nsIMenuItem::eCheckbox;
    else if ( type == NS_LITERAL_STRING("radio") )
      itemType = nsIMenuItem::eRadio;
      
    nsCOMPtr<nsIWebShell>  webShell = do_QueryReferent(mWebShellWeakRef);
    if (!webShell)
      return;

    // Create the item.
    pnsMenuItem->Create(inParentMenu, menuitemName, PR_FALSE, itemType, 
                          enabled, mManager, webShell, inMenuItemContent);   

    //
    // Set key shortcut and modifiers
    //
    
    nsAutoString keyValue;
    inMenuItemContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::key, keyValue);

    // Try to find the key node. Get the document so we can do |GetElementByID|
    nsCOMPtr<nsIDocument> document;
    inMenuItemContent->GetDocument(*getter_AddRefs(document));
    if ( !document ) 
      return;
    nsCOMPtr<nsIDOMXULDocument> xulDocument = do_QueryInterface(document);
    if ( !xulDocument )
      return;
  
    nsCOMPtr<nsIDOMElement> keyElement;
    if (!keyValue.IsEmpty())
      xulDocument->GetElementById(keyValue, getter_AddRefs(keyElement));
    if ( keyElement ) {
      nsCOMPtr<nsIContent> keyContent ( do_QueryInterface(keyElement) );
      nsAutoString keyChar; keyChar.AssignWithConversion(" ");
      keyContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::key, keyChar);
	    if(keyChar != NS_LITERAL_STRING(" ")) 
        pnsMenuItem->SetShortcutChar(keyChar);
        
      PRUint8 modifiers = knsMenuItemNoModifier;
	    nsAutoString modifiersStr;
      keyContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::modifiers, modifiersStr);
		  char* str = modifiersStr.ToNewCString();
		  char* newStr;
		  char* token = nsCRT::strtok( str, ", ", &newStr );
		  while( token != NULL ) {
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
		    
		    token = nsCRT::strtok( newStr, ", ", &newStr );
		  }
		  nsMemory::Free(str);

	    pnsMenuItem->SetModifiers ( modifiers );
    }

    if ( checked == NS_LITERAL_STRING("true") )
      pnsMenuItem->SetChecked(PR_TRUE);
    else
      pnsMenuItem->SetChecked(PR_FALSE);
      
    nsCOMPtr<nsISupports> supports ( do_QueryInterface(pnsMenuItem) );
    inParentMenu->AddItem(supports);         // Parent now owns menu item
  }
}


void 
nsMenuX::LoadSubMenu( nsIMenu * pParentMenu, nsIContent* inMenuItemContent )
{
  // if menu should be hidden, bail
  nsAutoString hidden; 
  inMenuItemContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::hidden, hidden);
  if ( hidden == NS_LITERAL_STRING("true") )
    return;
  
  nsAutoString menuName; 
  inMenuItemContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::label, menuName);
  //printf("Creating Menu [%s] \n", menuName.ToNewCString()); // this leaks

  // Create nsMenu
  nsCOMPtr<nsIMenu> pnsMenu ( do_CreateInstance(kMenuCID) );
  if (pnsMenu) {
    // Call Create
    nsCOMPtr<nsIWebShell> webShell = do_QueryReferent(mWebShellWeakRef);
    if (!webShell)
        return;
    nsCOMPtr<nsISupports> supports(do_QueryInterface(pParentMenu));
    pnsMenu->Create(supports, menuName, NS_LITERAL_STRING(""), mManager, webShell, inMenuItemContent);

    // set if it's enabled or disabled
    nsAutoString disabled;
    inMenuItemContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, disabled);
    if ( disabled == NS_LITERAL_STRING("true") )
      pnsMenu->SetEnabled ( PR_FALSE );
    else
      pnsMenu->SetEnabled ( PR_TRUE );

    // Make nsMenu a child of parent nsMenu. The parent takes ownership
    nsCOMPtr<nsISupports> supports2 ( do_QueryInterface(pnsMenu) );
	  pParentMenu->AddItem(supports2);
  }     
}



//
// OnCreate
//
// Fire our oncreate handler. Returns TRUE if we should keep processing the event,
// FALSE if the handler wants to stop the creation of the menu
//
PRBool
nsMenuX::OnCreate()
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_EVENT;
  event.message = NS_XUL_POPUP_SHOWING;
  event.isShift = PR_FALSE;
  event.isControl = PR_FALSE;
  event.isAlt = PR_FALSE;
  event.isMeta = PR_FALSE;
  event.clickCount = 0;
  event.widget = nsnull;
  
  nsCOMPtr<nsIContent> popupContent;
  GetMenuPopupContent(getter_AddRefs(popupContent));

  nsCOMPtr<nsIWebShell> webShell = do_QueryReferent(mWebShellWeakRef);
  if (!webShell) {
    NS_ERROR("No web shell");
    return PR_FALSE;
  }
  nsCOMPtr<nsIPresContext> presContext;
  MenuHelpersX::WebShellToPresContext(webShell, getter_AddRefs(presContext) );
  if ( presContext ) {
    nsresult rv = NS_OK;
    nsIContent* dispatchTo = popupContent ? popupContent : mMenuContent;
    rv = dispatchTo->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
    if ( NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault )
      return PR_FALSE;
 }

  // the menu is going to show and the oncreate handler has executed. We
  // now need to walk our menu items, checking to see if any of them have
  // a command attribute. If so, several apptributes must potentially
  // be updated.
  if (popupContent) {
    nsCOMPtr<nsIDocument> doc;
    popupContent->GetDocument(*getter_AddRefs(doc));
    nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(doc));

    PRInt32 count;
    popupContent->ChildCount(count);
    for (PRInt32 i = 0; i < count; i++) {
      nsCOMPtr<nsIContent> grandChild;
      popupContent->ChildAt(i, *getter_AddRefs(grandChild));
      nsCOMPtr<nsIAtom> tag;
      grandChild->GetTag(*getter_AddRefs(tag));
      if (tag.get() == nsWidgetAtoms::menuitem) {
        // See if we have a command attribute.
        nsAutoString command;
        grandChild->GetAttr(kNameSpaceID_None, nsWidgetAtoms::command, command);
        if (!command.IsEmpty()) {
          // We do! Look it up in our document
          nsCOMPtr<nsIDOMElement> commandElt;
          domDoc->GetElementById(command, getter_AddRefs(commandElt));
          nsCOMPtr<nsIContent> commandContent(do_QueryInterface(commandElt));

          if ( commandContent ) {
            nsAutoString commandDisabled, menuDisabled;
            commandContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, commandDisabled);
            grandChild->GetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, menuDisabled);
            if (!commandDisabled.Equals(menuDisabled)) {
              // The menu's disabled state needs to be updated to match the command.
              if (commandDisabled.IsEmpty()) 
                grandChild->UnsetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, PR_TRUE);
              else grandChild->SetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, commandDisabled, PR_TRUE);
            }

            // The menu's value and checked states need to be updated to match the command.
            // Note that (unlike the disabled state) if the command has *no* value for either, we
            // assume the menu is supplying its own.
            nsAutoString commandChecked, menuChecked;
            commandContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::checked, commandChecked);
            grandChild->GetAttr(kNameSpaceID_None, nsWidgetAtoms::checked, menuChecked);
            if (!commandChecked.Equals(menuChecked)) {
              if (!commandChecked.IsEmpty()) 
                grandChild->SetAttr(kNameSpaceID_None, nsWidgetAtoms::checked, commandChecked, PR_TRUE);
            }

            nsAutoString commandValue, menuValue;
            commandContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::label, commandValue);
            grandChild->GetAttr(kNameSpaceID_None, nsWidgetAtoms::label, menuValue);
            if (!commandValue.Equals(menuValue)) {
              if (!commandValue.IsEmpty()) 
                grandChild->SetAttr(kNameSpaceID_None, nsWidgetAtoms::label, commandValue, PR_TRUE);
            }
          }
        }
      }
    }
  }
  
  return PR_TRUE;
}

PRBool
nsMenuX::OnCreated()
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_EVENT;
  event.message = NS_XUL_POPUP_SHOWN;
  event.isShift = PR_FALSE;
  event.isControl = PR_FALSE;
  event.isAlt = PR_FALSE;
  event.isMeta = PR_FALSE;
  event.clickCount = 0;
  event.widget = nsnull;
  
  nsCOMPtr<nsIContent> popupContent;
  GetMenuPopupContent(getter_AddRefs(popupContent));

  nsCOMPtr<nsIWebShell> webShell = do_QueryReferent(mWebShellWeakRef);
  if (!webShell) {
    NS_ERROR("No web shell");
    return PR_FALSE;
  }
  nsCOMPtr<nsIPresContext> presContext;
  MenuHelpersX::WebShellToPresContext(webShell, getter_AddRefs(presContext) );
  if ( presContext ) {
    nsresult rv = NS_OK;
    nsIContent* dispatchTo = popupContent ? popupContent : mMenuContent;
    rv = dispatchTo->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
    if ( NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault )
      return PR_FALSE;
 }
  
  return PR_TRUE;
}

//
// OnDestroy
//
// Fire our ondestroy handler. Returns TRUE if we should keep processing the event,
// FALSE if the handler wants to stop the destruction of the menu
//
PRBool
nsMenuX::OnDestroy()
{
  if ( mDestroyHandlerCalled )
    return PR_TRUE;

  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_EVENT;
  event.message = NS_XUL_POPUP_HIDING;
  event.isShift = PR_FALSE;
  event.isControl = PR_FALSE;
  event.isAlt = PR_FALSE;
  event.isMeta = PR_FALSE;
  event.clickCount = 0;
  event.widget = nsnull;
  
  nsCOMPtr<nsIWebShell>  webShell = do_QueryReferent(mWebShellWeakRef);
  if (!webShell) {
    NS_WARNING("No web shell so can't run the OnDestroy");
    return PR_FALSE;
  }

  nsCOMPtr<nsIContent> popupContent;
  GetMenuPopupContent(getter_AddRefs(popupContent));

  nsCOMPtr<nsIPresContext> presContext;
  MenuHelpersX::WebShellToPresContext (webShell, getter_AddRefs(presContext) );
  if (presContext )  {
    nsresult rv = NS_OK;
    nsIContent* dispatchTo = popupContent ? popupContent : mMenuContent;
    rv = dispatchTo->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);

    mDestroyHandlerCalled = PR_TRUE;
    
    if ( NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault )
      return PR_FALSE;
  }
  return PR_TRUE;
}

PRBool
nsMenuX::OnDestroyed()
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_EVENT;
  event.message = NS_XUL_POPUP_HIDDEN;
  event.isShift = PR_FALSE;
  event.isControl = PR_FALSE;
  event.isAlt = PR_FALSE;
  event.isMeta = PR_FALSE;
  event.clickCount = 0;
  event.widget = nsnull;
  
  nsCOMPtr<nsIWebShell>  webShell = do_QueryReferent(mWebShellWeakRef);
  if (!webShell) {
    NS_WARNING("No web shell so can't run the OnDestroy");
    return PR_FALSE;
  }

  nsCOMPtr<nsIContent> popupContent;
  GetMenuPopupContent(getter_AddRefs(popupContent));

  nsCOMPtr<nsIPresContext> presContext;
  MenuHelpersX::WebShellToPresContext (webShell, getter_AddRefs(presContext) );
  if (presContext )  {
    nsresult rv = NS_OK;
    nsIContent* dispatchTo = popupContent ? popupContent : mMenuContent;
    rv = dispatchTo->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);

    mDestroyHandlerCalled = PR_TRUE;
    
    if ( NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault )
      return PR_FALSE;
  }
  return PR_TRUE;
}
//
// GetMenuPopupContent
//
// Find the |menupopup| child in the |popup| representing this menu. It should be one
// of a very few children so we won't be iterating over a bazillion menu items to find
// it (so the strcmp won't kill us).
//
void
nsMenuX::GetMenuPopupContent(nsIContent** aResult)
{
  if (!aResult )
    return;
  *aResult = nsnull;
  
  nsresult rv;
  nsCOMPtr<nsIXBLService> xblService = 
           do_GetService("@mozilla.org/xbl;1", &rv);
  if ( !xblService )
    return;
  
  PRInt32 count;
  mMenuContent->ChildCount(count);

  for (PRInt32 i = 0; i < count; i++) {
    PRInt32 dummy;
    nsCOMPtr<nsIContent> child;
    mMenuContent->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsIAtom> tag;
    xblService->ResolveTag(child, &dummy, getter_AddRefs(tag));
    if (tag && tag.get() == nsWidgetAtoms::menupopup) {
      *aResult = child.get();
      NS_ADDREF(*aResult);
      return;
    }
  }

} // GetMenuPopupContent


nsresult
nsMenuX::GetNextVisibleMenu(nsIMenu** outNextVisibleMenu)
{
  *outNextVisibleMenu = nsnull;
  
  nsCOMPtr<nsIMenuBar> menubarParent = do_QueryInterface(mParent);
  if (!menubarParent) return NS_ERROR_FAILURE;

  PRUint32    numMenus;
  menubarParent->GetMenuCount(numMenus);
  
  PRBool      gotThisMenu = PR_FALSE;
  PRUint32    thisMenuIndex;
  // Find this menu
  for (PRUint32 i = 0; i < numMenus; i ++)
  {
    nsCOMPtr<nsIMenu> thisMenu;
    menubarParent->GetMenuAt(i, *getter_AddRefs(thisMenu));
    if (!thisMenu) continue;
    
    if (gotThisMenu) {  // we're looking for the next visible
      nsCOMPtr<nsIContent> menuContent;
      thisMenu->GetMenuContent(getter_AddRefs(menuContent));
      
      nsAutoString hiddenValue, collapsedValue;
      menuContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::hidden, hiddenValue);
      menuContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::collapsed, collapsedValue);
      if ( hiddenValue != NS_LITERAL_STRING("true") && collapsedValue != NS_LITERAL_STRING("true"))
      {
        NS_IF_ADDREF(*outNextVisibleMenu = thisMenu);
        break;
      }
    
    }
    else {   // we're still looking for this
      if (thisMenu.get() == (nsIMenu *)this) {
        gotThisMenu = PR_TRUE;
        thisMenuIndex = i;
      }
    }
  }

  return NS_OK;
}

#pragma mark -

//
// nsIChangeObserver
//


NS_IMETHODIMP
nsMenuX::AttributeChanged(nsIDocument *aDocument, PRInt32 aNameSpaceID, nsIAtom *aAttribute,
                               PRInt32 aHint)
{
  if (gConstructingMenu)
    return NS_OK;

  // ignore the |open| attribute, which is by far the most common
  if ( aAttribute == nsWidgetAtoms::open )
    return NS_OK;
    
  nsCOMPtr<nsIMenuBar> menubarParent = do_QueryInterface(mParent);

  if(aAttribute == nsWidgetAtoms::disabled) {
    SetRebuild(PR_TRUE);
   
    nsAutoString valueString;
    mMenuContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, valueString);
    if(valueString == NS_LITERAL_STRING("true"))
      SetEnabled(PR_FALSE);
    else
      SetEnabled(PR_TRUE);
      
    ::DrawMenuBar();
  } 
  else if(aAttribute == nsWidgetAtoms::label) {
    SetRebuild(PR_TRUE);
    
    mMenuContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::label, mLabel);

    // reuse the existing menu, to avoid invalidating root menu bar.
    NS_ASSERTION(mMacMenuHandle != NULL, "nsMenuX::AttributeChanged: invalid menu handle.");
    RemoveAll();
    CFStringRef titleRef = ::CFStringCreateWithCharacters(kCFAllocatorDefault, (UniChar*)mLabel.get(), mLabel.Length());
    NS_ASSERTION(titleRef, "nsMenuX::AttributeChanged: CFStringCreateWithCharacters failed.");
    ::SetMenuTitleWithCFString(mMacMenuHandle, titleRef);
    ::CFRelease(titleRef);
    
    if (menubarParent)
        ::DrawMenuBar();

  }
  else if(aAttribute == nsWidgetAtoms::hidden || aAttribute == nsWidgetAtoms::collapsed) {
    SetRebuild(PR_TRUE);
      
      nsAutoString hiddenValue, collapsedValue;
      mMenuContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::hidden, hiddenValue);
      mMenuContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::collapsed, collapsedValue);
      if (hiddenValue == NS_LITERAL_STRING("true") || collapsedValue == NS_LITERAL_STRING("true")) {
        // hide this menu
        NS_ASSERTION(PR_FALSE, "nsMenuX::AttributeChanged: WRITE HIDE CODE.");
      }
      else
      {
        // Need to get the menuID of the next visible menu
        SInt16  nextMenuID = -1;    // default to the submenu case
        
        if (menubarParent) // this is a top-level menu
        {
          nsCOMPtr<nsIMenu> nextVisibleMenu;
          GetNextVisibleMenu(getter_AddRefs(nextVisibleMenu));
          
          if (nextVisibleMenu)
          {
            MenuHandle   nextHandle;
            nextVisibleMenu->GetNativeData((void **)&nextHandle);
	        nextMenuID = (nextHandle) ? ::GetMenuID(nextHandle) : mMacMenuID + 1;
          }
          else
          {
             nextMenuID = mMacMenuID + 1;
          }
        }
        
        // show this menu
        NS_ASSERTION(PR_FALSE, "nsMenuX::AttributeChanged: WRITE SHOW CODE.");
      }
      if (menubarParent) {
        ::DrawMenuBar();
      }
  }

  return NS_OK;
  
} // AttributeChanged


NS_IMETHODIMP
nsMenuX :: ContentRemoved(nsIDocument *aDocument, nsIContent *aChild, PRInt32 aIndexInContainer)
{  
  if (gConstructingMenu)
    return NS_OK;

    SetRebuild(PR_TRUE);

  RemoveItem(aIndexInContainer);
  mManager->Unregister(aChild);

  return NS_OK;
  
} // ContentRemoved


NS_IMETHODIMP
nsMenuX :: ContentInserted(nsIDocument *aDocument, nsIContent *aChild, PRInt32 aIndexInContainer)
{  
  if(gConstructingMenu)
    return NS_OK;

    SetRebuild(PR_TRUE);
  
  return NS_OK;
  
} // ContentInserted
