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
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIDOMDocument.h"
#include "nsIDocumentViewer.h"
#include "nsIDocumentObserver.h"
#include "nsIComponentManager.h"
#include "nsIDocShell.h"

#include "nsMenu.h"
#include "nsMenubar.h"
#include "nsIMenu.h"
#include "nsIMenuBar.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"
#include "nsIPresContext.h"
#include "nsGUIEvent.h"

#include "nsString.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"

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

#include "nsDynamicMDEF.h"

extern MenuHandle gLevel2HierMenu;
extern MenuHandle gLevel3HierMenu;
extern MenuHandle gLevel4HierMenu;
extern MenuHandle gLevel5HierMenu;

// Beginning ID for top level menus. IDs 2-5 are the 4 Golden Hierarchical Menus
const PRInt16 kMacMenuID = 6; 

#ifdef APPLE_MENU_HACK
const PRInt16 kAppleMenuID = 1;
#endif /* APPLE_MENU_HACK */

// Submenu depth tracking
PRInt16 gMenuDepth = 0;
PRInt16 gCurrentMenuDepth = 1;

extern Handle gMDEF; // Our stub MDEF
extern Handle gSystemMDEFHandle;
PRInt16 mMacMenuIDCount = kMacMenuID;

static PRBool gConstructingMenu = PR_FALSE;
  
#if DEBUG
nsInstanceCounter   gMenuCounter("nsMenu");
#endif

// CIDs
#include "nsWidgetsCID.h"
static NS_DEFINE_CID(kMenuCID,     NS_MENU_CID);
static NS_DEFINE_CID(kMenuItemCID, NS_MENUITEM_CID);


// Refcounted class for dummy menu items, like separators and help menu items.
class nsDummyMenuItem : public nsISupports
{
public:
  NS_DECL_ISUPPORTS
  
  nsDummyMenuItem()
  {
  }
};

NS_IMPL_ISUPPORTS0(nsDummyMenuItem)

//-------------------------------------------------------------------------
NS_IMPL_ISUPPORTS4(nsMenu, nsIMenu, nsIMenuListener, nsIChangeObserver, nsISupportsWeakReference)

//
// nsMenu constructor
//
nsMenu::nsMenu()
{
  mNumMenuItems  = 0;
  mParent    = nsnull;
  mManager = nsnull;
  
  mMacMenuID = 0;
  mMacMenuHandle = nsnull;
  mIsHelpMenu    = PR_FALSE;
  mHelpMenuOSItemsCount = 0;
  mIsEnabled     = PR_TRUE;
  mConstructed   = nsnull;
  mDestroyHandlerCalled = PR_FALSE;
  
  mNeedsRebuild = PR_TRUE;
    
  //
  // create a multi-destination Unicode converter which can handle all of the installed
  //	script systems
  //
  // To fix the menu bar problem, we add the primary script by using 0x80000000 with one 
  // script. This will make sure the converter first try to convert to the system script
  // before it try other script.
  //
  // Mac OS 8 and 9 Developer Document > Text and Other Interionational Service >
  //  Text Encoding Converter Manager
  // Inside Macintosh: Programming With the Text Encoding Conversion Manager
  //   CreateUnicodeToTextRunInfoByScriptCode
  // iNumberOfScriptCodes
  //   The number of desired scripts. .... If you set the high-order bit for this parameter, the 
  // Unicode converter assumes that the iScripts parameter contains a singel element specifying 
  // the preferred script. This feature i ssupported beginning with the Text
  // Encoding Conversion Manager 1.2
  // Also .. from About Eariler Release:
  // .. TEC Manager 1.2 was included with Mac OS 8 in July 1997...

  ScriptCode ps[1];
  ps[0] = ::GetScriptManagerVariable(smSysScript);
  
  OSErr err = ::CreateUnicodeToTextRunInfoByScriptCode(0x80000000,ps,&mUnicodeTextRunConverter);
  NS_ASSERTION(err==noErr,"nsMenu::nsMenu: CreateUnicodeToTextRunInfoByScriptCode failed.");	
  
#if DEBUG
  ++gMenuCounter;
#endif 
}


//
// nsMenu destructor
//
nsMenu::~nsMenu()
{
  RemoveAll();
  
  OSErr err = ::DisposeUnicodeToTextRunInfo(&mUnicodeTextRunConverter);
  NS_ASSERTION(err==noErr,"nsMenu::~nsMenu: DisposeUnicodeToTextRunInfo failed.");	 
   
  // Don't destroy the 4 Golden Hierarchical Menu
  if( !IsSpecialHierarchicalMenu(mMacMenuID) && !mIsHelpMenu) {
    //printf("WARNING: DeleteMenu called!!! \n");
    ::DeleteMenu(mMacMenuID);
  }
  
  // alert the change notifier we don't care no more
  mManager->Unregister(mMenuContent);

#if DEBUG
  --gMenuCounter;
#endif
}


//
// Create
//
NS_METHOD 
nsMenu::Create( nsISupports * aParent, const nsAString &aLabel, const nsAString &aAccessKey, 
                     nsIChangeManager* aManager, nsIWebShell* aShell, nsIContent* aNode )
{
  mWebShellWeakRef = do_GetWeakReference(aShell);
  mMenuContent = aNode;
  
  // register this menu to be notified when changes are made to our content object
  mManager = aManager;			// weak ref
  nsCOMPtr<nsIChangeObserver> changeObs ( do_QueryInterface(NS_STATIC_CAST(nsIChangeObserver*, this)) );
  mManager->Register(mMenuContent, changeObs);
  
  NS_ASSERTION ( mMenuContent, "Menu not given a dom node at creation time" );
  NS_ASSERTION ( mManager, "No change manager given, can't tell content model updates" );
  
  // our parent could be either a menu bar (if we're toplevel) or a menu (if we're a submenu)
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
NS_METHOD nsMenu::GetParent(nsISupports*& aParent)
{
  aParent = mParent;
  NS_IF_ADDREF(aParent);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetLabel(nsString &aText)
{
  aText = mLabel;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetLabel(const nsAString &aText)
{
  mLabel = aText;
  
  if(gCurrentMenuDepth >= 2) {
    // We don't really want to create a new menu, we want to fill in the
    // appropriate preinserted Golden Child Menu
    mMacMenuID = gCurrentMenuDepth;

    mMacMenuHandle = ::GetMenuHandle(mMacMenuID);
  } 
  else {
    nsAutoString menuIDstring;
    mMenuContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::id, menuIDstring);
    if(menuIDstring.EqualsLiteral("menu_Help"))
    {
      mIsHelpMenu = PR_TRUE;
      ::HMGetHelpMenuHandle(&mMacMenuHandle);
      mMacMenuID = kHMHelpMenuID;
    
      int numHelpItems = ::CountMenuItems(mMacMenuHandle);
      if (mHelpMenuOSItemsCount == 0)
        mHelpMenuOSItemsCount = numHelpItems;
      for (int i=0; i < numHelpItems; ++i)
      {
        nsDummyMenuItem*  dummyItem = new nsDummyMenuItem;
        mMenuItemsArray.AppendElement(dummyItem);   // owned
      }
   
      return NS_OK;
    }
    mMacMenuHandle = NSStringNewMenu(mMacMenuIDCount, mLabel);
    mMacMenuID = mMacMenuIDCount;
      
    mMacMenuIDCount++;
    // Replace standard MDEF with our stub MDEF
    if(mMacMenuHandle && gMDEF)  
      (**mMacMenuHandle).menuProc = gMDEF;
  }
  
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetAccessKey(nsString &aText)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetAccessKey(const nsAString &aText)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddItem(nsISupports* aItem)
{
  if(aItem) {
    // Figure out what we're adding
    nsCOMPtr<nsIMenuItem> menuitem(do_QueryInterface(aItem));
    if (menuitem) {
      AddMenuItem(menuitem);
    } else {
      nsCOMPtr<nsIMenu> menu(do_QueryInterface(aItem));
      if (menu)
        AddMenu(menu);
    }
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenuItem(nsIMenuItem * aMenuItem)
{
  if(!aMenuItem) return NS_ERROR_NULL_POINTER;
  
  nsCOMPtr<nsISupports> supports = do_QueryInterface(aMenuItem);
  if (!supports) return NS_ERROR_NO_INTERFACE;

  mMenuItemsArray.AppendElement(supports);    // owning ref
  PRUint32 currItemIndex;
  mMenuItemsArray.Count(&currItemIndex);
    
  mNumMenuItems++;

  nsAutoString label;
  aMenuItem->GetLabel(label);
  //printf("%s \n", NS_LossyConvertUCS2toASCII(label).get());
  //printf("%d = mMacMenuID\n", mMacMenuID);
  ::InsertMenuItem(mMacMenuHandle, "\p(Blank menu item", currItemIndex);
  MenuHelpers::SetMenuItemText(mMacMenuHandle, currItemIndex, label, mUnicodeTextRunConverter);
	  
	  // I want to be internationalized too!
  nsAutoString keyEquivalent(NS_LITERAL_STRING(" "));
  aMenuItem->GetShortcutChar(keyEquivalent);
  if(!keyEquivalent.EqualsLiteral(" ")) {
    ToUpperCase(keyEquivalent);
    char keyStr[2];
    keyEquivalent.ToCString(keyStr, sizeof(keyStr));
    short inKey = keyStr[0];
    ::SetItemCmd(mMacMenuHandle, currItemIndex, inKey);
    //::SetMenuItemKeyGlyph(mMacMenuHandle, mNumMenuItems, 0x61);
  }
  
  PRUint8 modifiers;
  aMenuItem->GetModifiers(&modifiers);
  PRUint8 macModifiers = kMenuNoModifiers;
  if(knsMenuItemShiftModifier & modifiers)
    macModifiers |= kMenuShiftModifier;
    
  if(knsMenuItemAltModifier & modifiers)
    macModifiers |= kMenuOptionModifier;

  if(knsMenuItemControlModifier & modifiers)
    macModifiers |= kMenuControlModifier;

  if(!(knsMenuItemCommandModifier & modifiers))
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
NS_METHOD nsMenu::AddMenu(nsIMenu * aMenu)
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
  //printf("AddMenu %s \n", NS_LossyConvertUCS2toASCII(label).get());

  ::InsertMenuItem(mMacMenuHandle, "\p(Blank Menu", currItemIndex);
  MenuHelpers::SetMenuItemText(mMacMenuHandle, currItemIndex, label, mUnicodeTextRunConverter);

  PRBool isEnabled;
  aMenu->GetEnabled(&isEnabled);
  if(isEnabled)
    ::EnableMenuItem(mMacMenuHandle, currItemIndex);
  else
    ::DisableMenuItem(mMacMenuHandle, currItemIndex);	    
    
  PRInt16 temp = gCurrentMenuDepth;
  ::SetMenuItemHierarchicalID((MenuHandle) mMacMenuHandle, currItemIndex, temp);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddSeparator() 
{
  // HACK - We're not really appending an nsMenuItem but it 
  // needs to be here to make sure that event dispatching isn't off by one.
  nsDummyMenuItem*  dummyItem = new nsDummyMenuItem;
  mMenuItemsArray.AppendElement(dummyItem);   // owning ref
  PRUint32  numItems;
  mMenuItemsArray.Count(&numItems);
  ::InsertMenuItem(mMacMenuHandle, "\p(-", numItems);
  mNumMenuItems++;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetItemCount(PRUint32 &aCount)
{
  return mMenuItemsArray.Count(&aCount);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetItemAt(const PRUint32 aPos, nsISupports *& aMenuItem)
{
  mMenuItemsArray.GetElementAt(aPos, &aMenuItem);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::InsertItemAt(const PRUint32 aPos, nsISupports * aMenuItem)
{
  NS_ASSERTION(0, "Not implemented");
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveItem(const PRUint32 aPos)
{
  NS_WARNING("Not implemented");
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveAll()
{
  PRUint32 curItem;
  mMenuItemsArray.Count(&curItem);
  while (curItem > 0)
  {
	  /* don't delete the actual Mac menu item if it's a MacOS item */
    if (curItem > mHelpMenuOSItemsCount)
	    ::DeleteMenuItem(mMacMenuHandle, curItem);
    
    curItem --;
  }

  mMenuItemsArray.Clear();    // remove all items
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetNativeData(void ** aData)
{
  *aData = mMacMenuHandle;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetNativeData(void * aData)
{
  mMacMenuHandle = (MenuHandle) aData;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenuListener(nsIMenuListener * aMenuListener)
{
  mListener = aMenuListener;    // strong ref
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
  if (aMenuListener == mListener) {
    mListener = nsnull;
  }
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// nsIMenuListener interface
//
//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  //printf("MenuItemSelected called \n");
  nsEventStatus eventStatus = nsEventStatus_eIgnore;
      
  // Determine if this is the correct menu to handle the event
  PRInt16 menuID = HiWord(((nsMenuEvent)aMenuEvent).mCommand);

#ifdef APPLE_MENU_HACK
  if(kAppleMenuID == menuID)
	{
    PRInt16 menuItemID = LoWord(((nsMenuEvent)aMenuEvent).mCommand);

		if (menuItemID > 2)	{		// don't handle the about or separator items yet
			Str255		itemStr;
			::GetMenuItemText(GetMenuHandle(menuID), menuItemID, itemStr);
			::OpenDeskAcc(itemStr);
			eventStatus = nsEventStatus_eConsumeNoDefault;
		}
		else if (menuItemID == 1) {
			/* handle about app here */
			nsresult rv = NS_ERROR_FAILURE;
		 
	    // Go find the about menu item
	    if (!mMenuContent)
      	return nsEventStatus_eConsumeNoDefault;
	    nsCOMPtr<nsIDocument> doc = mMenuContent->GetDocument(); 
	    if (!doc)
      	return nsEventStatus_eConsumeNoDefault;
	    nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(doc);
	    if (!domDoc) {
      	NS_ERROR("nsIDocument to nsIDOMDocument QI failed.");
      	return nsEventStatus_eConsumeNoDefault;
	  	}
	    
	    // "aboutName" is the element id for the "About &shortBrandName;"
	    // <menuitem/>. This is the glue code which causes any script code
	    // in the <menuitem/> to be executed.
	    nsCOMPtr<nsIDOMElement> domElement;
	    domDoc->GetElementById(NS_LITERAL_STRING("aboutName"),
                             getter_AddRefs(domElement));
	    if (!domElement)
	      	return nsEventStatus_eConsumeNoDefault;
	    
	    // Now get the pres context so we can execute the command
	  	nsCOMPtr<nsIPresContext> presContext;
	  	nsCOMPtr<nsIWebShell>    webShell = do_QueryReferent(mWebShellWeakRef);
	  	if (!webShell)
	  	    return nsEventStatus_eConsumeNoDefault;
	  	MenuHelpers::WebShellToPresContext(webShell, getter_AddRefs(presContext));

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
	else
#endif
  if ((kHMHelpMenuID == menuID) && (menuID != mMacMenuID))
  {
    /* 'this' is not correct; we need to find the help nsMenu */
	  nsCOMPtr<nsIMenuBar> mb = do_QueryInterface(mParent);
	  if ( !mb ) {
	    nsCOMPtr<nsIMenuBar> menuBar = do_QueryReferent(gMacMenubar);
	    if (!menuBar)
        return nsEventStatus_eIgnore;

      mb = menuBar;
    }

    /* set up a default event to query with */
    nsMenuEvent event;
    MenuHandle handle;
    // XXX fix me for carbon!
    ::HMGetHelpMenuHandle(&handle);
    event.mCommand = (unsigned int) handle;

    /* loop through the top-level menus in the menubar */
	  PRUint32 numMenus = 0;
	  mb->GetMenuCount(numMenus);
	  numMenus--;
	  for (PRInt32 i = numMenus; i >= 0; i--)
	  {
	    nsCOMPtr<nsIMenu> menu;
	    mb->GetMenuAt(i, *getter_AddRefs(menu));
      nsCOMPtr<nsIMenuListener> listener(do_QueryInterface(menu));
      if (listener)
      {
        nsAutoString label;
        menu->GetLabel(label);
        /* ask if this is the right menu */
        eventStatus = listener->MenuSelected(event);
        if(eventStatus != nsEventStatus_eIgnore)
        {
          // call our ondestroy handler now because the menu is going away.
          // do it now before sending the event into the dom in case our window
          // goes away.
          OnDestroy();
          
          /* call back into this method with the proper "this" */
          eventStatus = listener->MenuItemSelected(aMenuEvent);
          return eventStatus;
        }
      }
    } 
  }
  else if (mMacMenuID == menuID)
  {
    // Call MenuItemSelected on the correct nsMenuItem
    PRInt16 menuItemID = LoWord(((nsMenuEvent)aMenuEvent).mCommand);
    
    nsCOMPtr<nsISupports>     menuSupports = getter_AddRefs(mMenuItemsArray.ElementAt(menuItemID - 1));
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


//
// MenuSelected
//
// Called from the MDEF when our menu is selected in the menubar. Calls
// the oncreate handler and builds the menu.
//
nsEventStatus nsMenu::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  //printf("MenuSelected called for %s \n", NS_LossyConvertUCS2toASCII(mLabel).get());
  nsEventStatus eventStatus = nsEventStatus_eIgnore;
      
  // Determine if this is the correct menu to handle the event
  MenuHandle selectedMenuHandle = (MenuHandle) aMenuEvent.mCommand;

  if(mMacMenuHandle == selectedMenuHandle) {
    if (mIsHelpMenu && mConstructed) {
      RemoveAll();
      mConstructed = false;
      mNeedsRebuild = PR_TRUE;
	  }
	  
    // Open the node.
    mMenuContent->SetAttr(kNameSpaceID_None, nsWidgetAtoms::open, NS_LITERAL_STRING("true"), PR_TRUE);
      
    // Fire our oncreate handler. If we're told to stop, don't build the menu at all
    PRBool keepProcessing = OnCreate();

    if(!mIsHelpMenu && !mNeedsRebuild || !keepProcessing) 
      return nsEventStatus_eConsumeNoDefault;
      
  	if(!mConstructed || mNeedsRebuild) {
  	  if(mNeedsRebuild)
  	    RemoveAll();
  	  
  	  nsCOMPtr<nsIWebShell> webShell = do_QueryReferent(mWebShellWeakRef);
      if (!webShell) {
        NS_ERROR("No web shell");
        return nsEventStatus_eConsumeNoDefault;
      }
  	  if(mIsHelpMenu) {
  	    HelpMenuConstruct(aMenuEvent, nsnull /* mParentWindow */, nsnull, webShell);	      
  		  mConstructed = true;
  	  } 
  	  else {
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
    for (PRUint32 i = numItems; i > 0; i--)
	  {
      nsCOMPtr<nsISupports>     menuSupports = getter_AddRefs(mMenuItemsArray.ElementAt(i - 1));    
      nsCOMPtr<nsIMenu>         submenu = do_QueryInterface(menuSupports);
	    nsCOMPtr<nsIMenuListener> menuListener = do_QueryInterface(submenu);
	    if(menuListener) {
	      eventStatus = menuListener->MenuSelected(aMenuEvent);
	      if(nsEventStatus_eIgnore != eventStatus)
	        return eventStatus;
	    }  
    } 
  }

  return eventStatus;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  //printf("MenuDeselect called for %s\n", NS_LossyConvertUCS2toASCII(mLabel).get());
  // Destroy the menu
  if(mConstructed) {
    MenuDestruct(aMenuEvent);
    mConstructed = false;
  }
  
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuConstruct(
    const nsMenuEvent & aMenuEvent,
    nsIWidget         * aParentWindow,
    void              * /* domNode */, 
	  void              * aWebShell)
{
  mConstructed = false;
  gConstructingMenu = PR_TRUE;
  
  // reset destroy handler flag so that we'll know to fire it next time this menu goes away.
  mDestroyHandlerCalled = PR_FALSE;
  
  //printf("nsMenu::MenuConstruct called for %s = %d \n", NS_LossyConvertUCS2toASCII(mLabel).get(), mMacMenuHandle);
  
  // Begin menuitem inner loop
  
  gCurrentMenuDepth++;
   
  // Retrieve our menupopup.
  nsCOMPtr<nsIContent> menuPopup;
  GetMenuPopupContent(getter_AddRefs(menuPopup));
  if (!menuPopup)
    return nsEventStatus_eIgnore;
      
  // Iterate over the kids
  PRUint32 count = menuPopup->GetChildCount();
  for ( PRUint32 i = 0; i < count; ++i ) {
    nsIContent *child = menuPopup->GetChildAt(i);
    if ( child ) {
      // depending on the type, create a menu item, separator, or submenu
      nsIAtom *tag = child->Tag();
      if ( tag == nsWidgetAtoms::menuitem )
        LoadMenuItem(this, child);
      else if ( tag == nsWidgetAtoms::menuseparator )
        LoadSeparator(child);
      else if ( tag == nsWidgetAtoms::menu )
        LoadSubMenu(this, child);
    }
  } // for each menu item
  
  gConstructingMenu = PR_FALSE;
  mNeedsRebuild = PR_FALSE;
  //printf("  Done building, mMenuItemVoidArray.Count() = %d \n", mMenuItemVoidArray.Count());
  
  gCurrentMenuDepth--;
              
  return nsEventStatus_eIgnore;
}


nsEventStatus
nsMenu::HelpMenuConstruct( const nsMenuEvent & aMenuEvent, nsIWidget* aParentWindow,
                             void* /* unused */, void* aWebShell)
{
  //printf("nsMenu::MenuConstruct called for %s = %d \n", NS_LossyConvertUCS2toASCII(mLabel).get(), mMacMenuHandle);

  int numHelpItems = ::CountMenuItems(mMacMenuHandle);
  for (int i=0; i < numHelpItems; ++i) {
    nsDummyMenuItem*  dummyItem = new nsDummyMenuItem;
    mMenuItemsArray.AppendElement(dummyItem);
  }
     
  gCurrentMenuDepth++;
   
  // Retrieve our menupopup.
  nsCOMPtr<nsIContent> menuPopup;
  GetMenuPopupContent(getter_AddRefs(menuPopup));
  if (!menuPopup)
    return nsEventStatus_eIgnore;
      
  // Iterate over the kids
  PRUint32 count = menuPopup->GetChildCount();
  for ( PRUint32 i = 0; i < count; ++i ) {
    nsIContent *child = menuPopup->GetChildAt(i);
    if ( child ) {
      // depending on the type, create a menu item, separator, or submenu
      nsIAtom *tag = child->Tag();
      if ( tag == nsWidgetAtoms::menuitem )
        LoadMenuItem(this, child);
      else if ( tag == nsWidgetAtoms::menuseparator )
        AddSeparator();
      else if ( tag == nsWidgetAtoms::menu )
        LoadSubMenu(this, child);
    }   
  } // for each menu item
  
  //printf("  Done building, mMenuItemVoidArray.Count() = %d \n", mMenuItemVoidArray.Count());
  
  gCurrentMenuDepth--;
             
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  //printf("nsMenu::MenuDestruct() called for %s \n", NS_LossyConvertUCS2toASCII(mLabel).get());
  
  // Fire our ondestroy handler. If we're told to stop, don't destroy the menu
  PRBool keepProcessing = OnDestroy();
  if ( keepProcessing )
  {
    if( mMacMenuHandle == gLevel2HierMenu ||
        mMacMenuHandle == gLevel3HierMenu ||
        mMacMenuHandle == gLevel4HierMenu ||
        mMacMenuHandle == gLevel5HierMenu)
    {
      mNeedsRebuild = PR_TRUE;    
    }

    if(mNeedsRebuild) {
      RemoveAll();
      mConstructed = false;
      //printf("  mMenuItemVoidArray.Count() = %d \n", mMenuItemVoidArray.Count());
      // Close the node.
      mNeedsRebuild = PR_TRUE;
    } 
    mMenuContent->UnsetAttr(kNameSpaceID_None, nsWidgetAtoms::open, PR_TRUE);

    OnDestroyed();
  }
  
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::CheckRebuild(PRBool & aNeedsRebuild)
{
  aNeedsRebuild = PR_TRUE; //mNeedsRebuild;
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::SetRebuild(PRBool aNeedsRebuild)
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
NS_METHOD nsMenu::SetEnabled(PRBool aIsEnabled)
{
  mIsEnabled = aIsEnabled;
  
  // If we're at the depth of a top-level menu, enable/disable the menu explicity.
  // Otherwise we're working with a single "golden child" menu shared by all hierarchicals
  // so if we touch it, it will affect the display of every other hierarchical spawnded from
  // this menu (which would be bad).
  if ( gCurrentMenuDepth < 2 && !IsSpecialHierarchicalMenu(mMacMenuID) ) {
    if ( aIsEnabled )
      ::EnableMenuItem(mMacMenuHandle, 0);
    else
      ::DisableMenuItem(mMacMenuHandle, 0);
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
/**
* Get enabled state
*
*/
NS_METHOD nsMenu::GetEnabled(PRBool* aIsEnabled)
{
  *aIsEnabled = mIsEnabled;

  return NS_OK;
}

//-------------------------------------------------------------------------
/**
* Query if this is the help menu
*
*/
NS_METHOD nsMenu::IsHelpMenu(PRBool* aIsHelpMenu)
{
  *aIsHelpMenu = mIsHelpMenu;
  return NS_OK;
}


//-------------------------------------------------------------------------
/**
* Get GetMenuContent
*
*/
NS_METHOD nsMenu::GetMenuContent(nsIContent ** aMenuContent)
{
  NS_ENSURE_ARG_POINTER(aMenuContent);
  NS_IF_ADDREF(*aMenuContent = mMenuContent);
	return NS_OK;
}



//-------------------------------------------------------------------------
MenuHandle nsMenu::NSStringNewMenu(short menuID, nsString& menuTitle)
{
	MenuHandle newMenuHandle = nsnull;
	short themeFontID;
	SInt16 themeFontSize;
	Style themeFontStyle;
  char* scriptRunText = MenuHelpers::ConvertToScriptRun ( menuTitle, mUnicodeTextRunConverter, &themeFontID,
                                                          &themeFontSize, &themeFontStyle );
  if ( scriptRunText ) {
    newMenuHandle = ::NewMenu(menuID,c2pstr(scriptRunText));
    NS_ASSERTION(newMenuHandle!=NULL,"nsMenu::NSStringNewMenu: NewMenu failed.");
    if (newMenuHandle==NULL) { nsMemory::Free(scriptRunText); return NULL; }
    OSErr err = ::SetMenuFont(newMenuHandle,themeFontID,themeFontSize);
    NS_ASSERTION(err==noErr,"nsMenu::NSStringNewMenu: SetMenuFont failed.");
    if (err!=noErr) { nsMemory::Free(scriptRunText); return NULL; }

	  nsMemory::Free(scriptRunText);
	}
	
	return newMenuHandle;			
}


//----------------------------------------
void
nsMenu::LoadMenuItem( nsIMenu* inParentMenu, nsIContent* inMenuItemContent )
{
  if ( !inMenuItemContent )
    return;

  // if menu should be hidden, bail
  nsAutoString hidden;
  inMenuItemContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::hidden, hidden);
  if ( hidden.EqualsLiteral("true") )
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

    //printf("menuitem %s \n", NS_LossyConvertUCS2toASCII(menuitemName).get());
              
    PRBool enabled = ! (disabled.EqualsLiteral("true"));
    
    nsIMenuItem::EMenuItemType itemType = nsIMenuItem::eRegular;
    if ( type.EqualsLiteral("checkbox") )
      itemType = nsIMenuItem::eCheckbox;
    else if ( type.EqualsLiteral("radio") )
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
    nsCOMPtr<nsIDOMDocument> domDocument =
      do_QueryInterface(inMenuItemContent->GetDocument());
    if ( !domDocument )
      return;
  
    nsCOMPtr<nsIDOMElement> keyElement;
    if (!keyValue.IsEmpty())
      domDocument->GetElementById(keyValue, getter_AddRefs(keyElement));
    if ( keyElement ) {
      nsCOMPtr<nsIContent> keyContent ( do_QueryInterface(keyElement) );
      nsAutoString keyChar(NS_LITERAL_STRING(" "));
      keyContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::key, keyChar);
	    if(!keyChar.EqualsLiteral(" ")) 
        pnsMenuItem->SetShortcutChar(keyChar);
        
      PRUint8 modifiers = knsMenuItemNoModifier;
	    nsAutoString modifiersStr;
      keyContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::modifiers, modifiersStr);
		  char* str = ToNewCString(modifiersStr);
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

    if ( checked.EqualsLiteral("true") )
      pnsMenuItem->SetChecked(PR_TRUE);
    else
      pnsMenuItem->SetChecked(PR_FALSE);
      
    nsCOMPtr<nsISupports> supports ( do_QueryInterface(pnsMenuItem) );
    inParentMenu->AddItem(supports);         // Parent now owns menu item
  }

} // LoadMenuItem


void 
nsMenu::LoadSubMenu( nsIMenu * pParentMenu, nsIContent* inMenuItemContent )
{
  // if menu should be hidden, bail
  nsAutoString hidden; 
  inMenuItemContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::hidden, hidden);
  if ( hidden.EqualsLiteral("true") )
    return;
  
  nsAutoString menuName; 
  inMenuItemContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::label, menuName);
  //printf("Creating Menu [%s] \n", NS_LossyConvertUCS2toASCII(menuName).get());

  // Create nsMenu
  nsCOMPtr<nsIMenu> pnsMenu ( do_CreateInstance(kMenuCID) );
  if (pnsMenu) {
    // Call Create
    nsCOMPtr<nsIWebShell> webShell = do_QueryReferent(mWebShellWeakRef);
    if (!webShell)
        return;
    nsCOMPtr<nsISupports> supports(do_QueryInterface(pParentMenu));
    pnsMenu->Create(supports, menuName, EmptyString(), mManager, webShell, inMenuItemContent);

    // set if it's enabled or disabled
    nsAutoString disabled;
    inMenuItemContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, disabled);
    if ( disabled.EqualsLiteral("true") )
      pnsMenu->SetEnabled ( PR_FALSE );
    else
      pnsMenu->SetEnabled ( PR_TRUE );

    // Make nsMenu a child of parent nsMenu. The parent takes ownership
    nsCOMPtr<nsISupports> supports2 ( do_QueryInterface(pnsMenu) );
	  pParentMenu->AddItem(supports2);
  }     
}

void
nsMenu::LoadSeparator ( nsIContent* inMenuItemContent ) 
{
  // if item should be hidden, bail
  nsAutoString hidden;
  inMenuItemContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::hidden, hidden);
  if ( hidden.EqualsLiteral("true") )
    return;

  AddSeparator();
}


//
// OnCreate
//
// Fire our oncreate handler. Returns TRUE if we should keep processing the event,
// FALSE if the handler wants to stop the creation of the menu
//
PRBool
nsMenu::OnCreate()
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
  MenuHelpers::WebShellToPresContext(webShell, getter_AddRefs(presContext) );
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
    nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(popupContent->GetDocument()));

    PRUint32 count = popupContent->GetChildCount();
    for (PRUint32 i = 0; i < count; i++) {
      nsIContent *grandChild = popupContent->GetChildAt(i);
      if (grandChild->Tag() == nsWidgetAtoms::menuitem) {
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
nsMenu::OnCreated()
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
  MenuHelpers::WebShellToPresContext(webShell, getter_AddRefs(presContext) );
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
nsMenu::OnDestroy()
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
  MenuHelpers::WebShellToPresContext (webShell, getter_AddRefs(presContext) );
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
nsMenu::OnDestroyed()
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
  MenuHelpers::WebShellToPresContext (webShell, getter_AddRefs(presContext) );
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
nsMenu::GetMenuPopupContent(nsIContent** aResult)
{
  if (!aResult )
    return;
  *aResult = nsnull;
  
  nsresult rv;
  nsCOMPtr<nsIXBLService> xblService = 
           do_GetService("@mozilla.org/xbl;1", &rv);
  if ( !xblService )
    return;
  
  PRUint32 count = mMenuContent->GetChildCount();

  for (PRUint32 i = 0; i < count; i++) {
    PRInt32 dummy;
    nsIContent *child = mMenuContent->GetChildAt(i);
    nsCOMPtr<nsIAtom> tag;
    xblService->ResolveTag(child, &dummy, getter_AddRefs(tag));
    if (tag == nsWidgetAtoms::menupopup) {
      *aResult = child.get();
      NS_ADDREF(*aResult);
      return;
    }
  }

} // GetMenuPopupContent


nsresult
nsMenu::GetNextVisibleMenu(nsIMenu** outNextVisibleMenu)
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
      if ( !hiddenValue.EqualsLiteral("true") && !collapsedValue.EqualsLiteral("true"))
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
nsMenu::AttributeChanged(nsIDocument *aDocument, PRInt32 aNameSpaceID, nsIAtom *aAttribute)
{
  if(gConstructingMenu)
    return NS_OK;

  // ignore the |open| attribute, which is by far the most common
  if ( aAttribute == nsWidgetAtoms::open )
    return NS_OK;

  nsCOMPtr<nsIMenuBar> menuBarParent = do_QueryInterface(mParent);

  if(aAttribute == nsWidgetAtoms::disabled) {
    mNeedsRebuild = PR_TRUE;
   
    nsAutoString valueString;
    mMenuContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, valueString);
    if(valueString.EqualsLiteral("true"))
      SetEnabled(PR_FALSE);
    else
      SetEnabled(PR_TRUE);
      
    ::DrawMenuBar();
  } 
  else if(aAttribute == nsWidgetAtoms::label) {
    mNeedsRebuild = PR_TRUE;
    
    mMenuContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::label, mLabel);

    // if we're a submenu, we don't have to go through all the gymnastics below
    // to remove ourselves from the menubar and re-add the menu. We just invalidate
    // our parent to change the text of the item.
    if ( IsSpecialHierarchicalMenu(mMacMenuID) ) {
      nsCOMPtr<nsIMenuListener> listener = do_QueryInterface(mParent);
      listener->SetRebuild(PR_TRUE);
      return NS_OK;
    }
    
    ::DeleteMenu(mMacMenuID);
    
    mMacMenuHandle = NSStringNewMenu(mMacMenuID, mLabel);

    // Replace standard MDEF with our stub MDEF
    if(mMacMenuHandle && gMDEF) 
      (**mMacMenuHandle).menuProc = gMDEF;

    // Need to get the menuID of the next visible menu
    SInt16  nextMenuID = -1;    // default to the submenu case
    
    if (menuBarParent) // this is a top-level menu
    {
      nsCOMPtr<nsIMenu> nextVisibleMenu;
      GetNextVisibleMenu(getter_AddRefs(nextVisibleMenu));
      
      if (nextVisibleMenu) {
        MenuHandle nextHandle;
        nextVisibleMenu->GetNativeData((void **)&nextHandle);
        nextMenuID = (nextHandle) ? (**nextHandle).menuID : mMacMenuID + 1;        
      }
      else
         nextMenuID = mMacMenuID + 1;
    }

    ::InsertMenu(mMacMenuHandle, nextMenuID);
    
    if(menuBarParent) {
      menuBarParent->SetNativeData(::GetMenuBar());
      ::DrawMenuBar();
    }
  }
  else if(aAttribute == nsWidgetAtoms::hidden || aAttribute == nsWidgetAtoms::collapsed) {
    mNeedsRebuild = PR_TRUE;
    
    nsAutoString hiddenValue, collapsedValue;
    mMenuContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::hidden, hiddenValue);
    mMenuContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::collapsed, collapsedValue);
    if(hiddenValue.EqualsLiteral("true") || collapsedValue.EqualsLiteral("true")) {
      if ( !IsSpecialHierarchicalMenu(mMacMenuID) ) {
        // kill this menu, but not if we're special. baaaad things would happen if
        // we did that.
        ::DeleteMenu(mMacMenuID);
      }
    }
    else {
      // Need to get the menuID of the next visible menu
      SInt16  nextMenuID = -1;    // default to the submenu case
      
      if (menuBarParent) // this is a top-level menu
      {
        nsCOMPtr<nsIMenu> nextVisibleMenu;
        GetNextVisibleMenu(getter_AddRefs(nextVisibleMenu));
        
        if (nextVisibleMenu) {
          MenuHandle nextHandle;
          nextVisibleMenu->GetNativeData((void **)&nextHandle);
          nextMenuID = (nextHandle) ? (**nextHandle).menuID : mMacMenuID + 1;
        }
        else
           nextMenuID = mMacMenuID + 1;
      }
      
      // show this menu
      ::InsertMenu(mMacMenuHandle, nextMenuID);
    }
    if(menuBarParent) {
      menuBarParent->SetNativeData(::GetMenuBar());
      ::DrawMenuBar();
    }
  }

  return NS_OK;
  
} // AttributeChanged


NS_IMETHODIMP
nsMenu :: ContentRemoved(nsIDocument *aDocument, nsIContent *aChild, PRInt32 aIndexInContainer)
{  
  if(gConstructingMenu)
    return NS_OK;

  mNeedsRebuild = PR_TRUE;

  RemoveItem(aIndexInContainer);
  mManager->Unregister ( aChild );
  
  return NS_OK;
  
} // ContentRemoved


NS_IMETHODIMP
nsMenu :: ContentInserted(nsIDocument *aDocument, nsIContent *aChild, PRInt32 aIndexInContainer)
{  
  if(gConstructingMenu)
    return NS_OK;

  mNeedsRebuild = PR_TRUE;
  
  return NS_OK;
  
} // ContentInserted


//
// IsSpecialHierarchicalMenu
//
// To get dynamic hierarchical menus, the mdef uses 4 "golden child" menus that it swaps in and out.
// Use this to identify them. They will have id's in the range [2, 5].
//
PRBool
nsMenu :: IsSpecialHierarchicalMenu ( PRInt32 inMenuId )
{
  return (mMacMenuID <= 5) && (mMacMenuID >= 2);
}
