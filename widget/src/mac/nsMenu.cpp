/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIDOMXULDocument.h"
#include "nsIComponentManager.h"

#include "nsMenu.h"
#include "nsIMenu.h"
#include "nsIMenuBar.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"

#include "nsString.h"
#include "nsStringUtil.h"

#include <Appearance.h>
#include <TextUtils.h>
#include <ToolUtils.h>
#include <Devices.h>
#include <UnicodeConverter.h>
#include <Fonts.h>
#include <Sound.h>

#include "nsDynamicMDEF.h"

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

// IIDs
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIMenuIID,     NS_IMENU_IID);
static NS_DEFINE_IID(kIMenuBarIID,  NS_IMENUBAR_IID);
static NS_DEFINE_IID(kIMenuItemIID, NS_IMENUITEM_IID);

// CIDs
#include "nsWidgetsCID.h"
static NS_DEFINE_IID(kMenuBarCID,  NS_MENUBAR_CID);
static NS_DEFINE_IID(kMenuCID,     NS_MENU_CID);
static NS_DEFINE_IID(kMenuItemCID, NS_MENUITEM_CID);

//-------------------------------------------------------------------------
nsresult nsMenu::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
                                                                         
  *aInstancePtr = NULL;                                                  
                                                                                        
  if (aIID.Equals(kIMenuIID)) {                                         
    *aInstancePtr = (void*)(nsIMenu*) this;                                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*)this;                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }
  if (aIID.Equals(kIMenuListenerIID)) {                                      
    *aInstancePtr = (void*) ((nsIMenuListener*)this);                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                     
  return NS_NOINTERFACE;                                                 
}

NS_IMPL_ADDREF(nsMenu)
NS_IMPL_RELEASE(nsMenu)

//-------------------------------------------------------------------------
//
// nsMenu constructor
//
//-------------------------------------------------------------------------
nsMenu::nsMenu() : nsIMenu()
{
  NS_INIT_REFCNT();
  mNumMenuItems  = 0;
  mMenuParent    = nsnull;
  mMenuBarParent = nsnull;
  
  mMacMenuID = 0;
  mMacMenuHandle = nsnull;
  mListener      = nsnull;
  mConstructed   = nsnull;
  
  mDOMNode       = nsnull;
  mDOMElement    = nsnull;
  mWebShell      = nsnull;
  
  //
  // create a multi-destination Unicode converter which can handle all of the installed
  //	script systems
  //
  OSErr err = ::CreateUnicodeToTextRunInfoByScriptCode(0,NULL,&mUnicodeTextRunConverter);
  NS_ASSERTION(err==noErr,"nsMenu::nsMenu: CreateUnicodeToTextRunInfoByScriptCode failed.");	

}

//-------------------------------------------------------------------------
//
// nsMenu destructor
//
//-------------------------------------------------------------------------
nsMenu::~nsMenu()
{
  //printf("nsMenu::~nsMenu() called \n");
  
  OSErr		err;
  NS_IF_RELEASE(mListener);

  RemoveAll();
  
  err = ::DisposeUnicodeToTextRunInfo(&mUnicodeTextRunConverter);
  NS_ASSERTION(err==noErr,"nsMenu::~nsMenu: DisposeUnicodeToTextRunInfo failed.");	 
   
  // Don't destroy the 4 Golden Hierarchical Menu
  if((mMacMenuID > 5) || (mMacMenuID < 2)) {
    //printf("WARNING: DeleteMenu called!!! \n");
    ::DeleteMenu(mMacMenuID);
  }
  
  mDOMNode = nsnull;
}

//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsMenu::Create(nsISupports *aParent, const nsString &aLabel)
{
  if(aParent)
  {
    nsIMenuBar * menubar = nsnull;
    aParent->QueryInterface(kIMenuBarIID, (void**) &menubar);
    if(menubar)
    {
      mMenuBarParent = menubar;;
      NS_RELEASE(menubar); // Balance the QI
    }
    else
    {
      nsIMenu * menu = nsnull;
      aParent->QueryInterface(kIMenuIID, (void**) &menu);
      {
      	mMenuParent = menu;
      	NS_RELEASE(menu); // Balance the QI
      }
    }
  }
  
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetParent(nsISupports*& aParent)
{

  aParent = nsnull;
  if (nsnull != mMenuParent) {
    return mMenuParent->QueryInterface(kISupportsIID,(void**)&aParent);
  } else if (nsnull != mMenuBarParent) {
    return mMenuBarParent->QueryInterface(kISupportsIID,(void**)&aParent);
  }

  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetLabel(nsString &aText)
{
  aText = mLabel;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetLabel(const nsString &aText)
{
  mLabel = aText;
  
  if(gCurrentMenuDepth >= 2) {
    // We don't really want to create a new menu, we want to fill in the
    // appropriate preinserted Golden Child Menu
    mMacMenuID = gCurrentMenuDepth;
    if(mMacMenuID == 16)
      SysBeep(30);
      
    //mMacMenuHandle = NSStringNewChildMenu(mMacMenuID, mLabel);
    mMacMenuHandle = ::GetMenuHandle(mMacMenuID);
  } else {
    mMacMenuHandle = NSStringNewMenu(mMacMenuIDCount, mLabel);
    mMacMenuID = mMacMenuIDCount;
    if(mMacMenuID == 16)
      SysBeep(30);
      
    mMacMenuIDCount++;
    // Replace standard MDEF with our stub MDEF
    if(mMacMenuHandle) {    
      ::HLock((Handle)mMacMenuHandle);
      //gSystemMDEFHandle = (**mMacMenuHandle).menuProc;
      (**mMacMenuHandle).menuProc = gMDEF;
      ::HUnlock((Handle)mMacMenuHandle);
    }
  }
  
  //printf("MacMenuID = %d", mMacMenuID);
  
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetAccessKey(nsString &aText)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetAccessKey(const nsString &aText)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddItem(nsISupports* aItem)
{
  if(aItem)
  {
    // Figure out what we're adding
    nsIMenuItem * menuitem = nsnull;
    aItem->QueryInterface(kIMenuItemIID, (void**) &menuitem);  
    if(menuitem)
    {
      // case menuitem
      AddMenuItem(menuitem);
      NS_RELEASE(menuitem);
    }
    else
    {
	  nsIMenu * menu = nsnull;
	  aItem->QueryInterface(kIMenuIID, (void**) &menu);
	  if(menu)
	  {
	    // case menu
	    AddMenu(menu);
	    NS_RELEASE(menu);
	  }
	}
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenuItem(nsIMenuItem * aMenuItem)
{
  if(aMenuItem) {
    nsISupports * supports = nsnull;
    aMenuItem->QueryInterface(kISupportsIID, (void**)&supports);
    if(supports) {
	  mMenuItemVoidArray.AppendElement(supports);
      
	  nsString label;
	  aMenuItem->GetLabel(label);
	  //printf("%s \n", label.ToNewCString());
	  mNumMenuItems++;
	  ::InsertMenuItem(mMacMenuHandle, "\pa", mMenuItemVoidArray.Count());
	  NSStringSetMenuItemText(mMacMenuHandle, mMenuItemVoidArray.Count(), label);
	  
	  // I want to be internationalized too!
	  nsString keyEquivalent = " ";
	  aMenuItem->GetShortcutChar(keyEquivalent);
	  if(keyEquivalent != " ") {
	    keyEquivalent.ToUpperCase();
	    char* foo = keyEquivalent.ToNewCString();
	    short inKey = foo[0];
	    ::SetItemCmd(mMacMenuHandle, mMenuItemVoidArray.Count(), inKey);
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
	  
	  ::SetMenuItemModifiers(mMacMenuHandle, mMenuItemVoidArray.Count(), macModifiers);
	  
	  PRBool isEnabled;
	  aMenuItem->GetEnabled(&isEnabled);
	  if(isEnabled)
	    ::EnableMenuItem(mMacMenuHandle, mMenuItemVoidArray.Count());
	  else
	    ::DisableMenuItem(mMacMenuHandle, mMenuItemVoidArray.Count());
	}
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenu(nsIMenu * aMenu)
{
  // Add a submenu
  if(aMenu) {
    nsISupports * supports = nsnull;
    aMenu->QueryInterface(kISupportsIID, (void**)&supports);
    if(supports) {
      mMenuItemVoidArray.AppendElement(supports);
  
      // We have to add it as a menu item and then associate it with the item
      nsString label;
      aMenu->GetLabel(label);
      //printf("AddMenu %s \n", label.ToNewCString());
      mNumMenuItems++;

      ::InsertMenuItem(mMacMenuHandle, c2pstr(label.ToNewCString()), mMenuItemVoidArray.Count());
      //NSStringSetMenuItemText(mMacMenuHandle, mMenuItemVoidArray.Count(), label);
        
      PRInt16 temp = gCurrentMenuDepth;
      ::SetMenuItemHierarchicalID((MenuHandle) mMacMenuHandle, mMenuItemVoidArray.Count(), temp);
    }
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddSeparator() 
{
  // HACK - We're not really appending an nsMenuItem but it 
  // needs to be here to make sure that event dispatching isn't off by one.
  mMenuItemVoidArray.AppendElement(nsnull);
  ::InsertMenuItem(mMacMenuHandle, "\p(-", mMenuItemVoidArray.Count() );
  mNumMenuItems++;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetItemCount(PRUint32 &aCount)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetItemAt(const PRUint32 aPos, nsISupports *& aMenuItem)
{
  aMenuItem = (nsISupports*) mMenuItemVoidArray[aPos];
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::InsertItemAt(const PRUint32 aPos, nsISupports * aMenuItem)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveItem(const PRUint32 aPos)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveAll()
{
  while(mMenuItemVoidArray.Count())
  {
    --mNumMenuItems;
    
    if(mMenuItemVoidArray[mMenuItemVoidArray.Count() - 1]) {
      // Figure out what we're releasing
      nsIMenuItem * menuitem = nsnull;
      ((nsISupports*)mMenuItemVoidArray[mMenuItemVoidArray.Count() - 1])->QueryInterface(kIMenuItemIID, (void**) &menuitem);  
      if(menuitem)
      {
        // case menuitem
        menuitem->Release(); // Release our hold
        NS_IF_RELEASE(menuitem); // Balance QI
      }
      else
      {
	    nsIMenu * menu = nsnull;
	    ((nsISupports*)mMenuItemVoidArray[mMenuItemVoidArray.Count() - 1])->QueryInterface(kIMenuIID, (void**) &menu);
	    if(menu)
	    {
	      // case menu
	      menu->Release(); // Release our hold 
	      NS_IF_RELEASE(menu); // Balance QI
	    }
	  }
	}
	::DeleteMenuItem(mMacMenuHandle, mMenuItemVoidArray.Count());
	mMenuItemVoidArray.RemoveElementAt(mMenuItemVoidArray.Count() - 1);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetNativeData(void ** aData)
{
  *aData = mMacMenuHandle;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenuListener(nsIMenuListener * aMenuListener)
{
  mListener = aMenuListener;
  //NS_ADDREF(mListener);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
  if (aMenuListener == mListener) {
    //NS_IF_RELEASE(mListener);
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

		if (menuItemID > 2)			// don't handle the about or separator items yet
		{
			Str255		itemStr;
			::GetMenuItemText(GetMenuHandle(menuID), menuItemID, itemStr);
#if !TARGET_CARBON
			::OpenDeskAcc(itemStr);
#endif
			eventStatus = nsEventStatus_eConsumeNoDefault;
		}
	}
	else
#endif

  if(mMacMenuID == menuID)
  {
    // Call MenuItemSelected on the correct nsMenuItem
    PRInt16 menuItemID = LoWord(((nsMenuEvent)aMenuEvent).mCommand);
    nsIMenuListener * menuListener = nsnull;
    if(mMenuItemVoidArray[menuItemID-1]) {
	    ((nsIMenuItem*)mMenuItemVoidArray[menuItemID-1])->QueryInterface(kIMenuListenerIID, &menuListener);
		if(menuListener) {
		  eventStatus = menuListener->MenuItemSelected(aMenuEvent);
		  NS_IF_RELEASE(menuListener);
		  if(nsEventStatus_eIgnore != eventStatus)
		    return eventStatus;
		}
	}
  } 

  // Make sure none of our submenus are the ones that should be handling this
  for (int i = mMenuItemVoidArray.Count(); i > 0; i--)
  {
    if(nsnull != mMenuItemVoidArray[i-1])
    {
	    nsIMenu * submenu = nsnull;
	    ((nsISupports*)mMenuItemVoidArray[i-1])->QueryInterface(kIMenuIID, &submenu);
	    if(submenu)
	    {
		    nsIMenuListener * menuListener = nsnull;
		    ((nsISupports*)mMenuItemVoidArray[i-1])->QueryInterface(kIMenuListenerIID, &menuListener);
		    if(menuListener){
		      eventStatus = menuListener->MenuItemSelected(aMenuEvent);
		      NS_IF_RELEASE(menuListener);
		      if(nsEventStatus_eIgnore != eventStatus)
		        return eventStatus;
		    }
	    }
	}
  }
  
  return eventStatus;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  //printf("MenuSelected called for %s \n", mLabel.ToNewCString());
  nsEventStatus eventStatus = nsEventStatus_eIgnore;
      
  // Determine if this is the correct menu to handle the event
  MenuHandle selectedMenuHandle = (MenuHandle) aMenuEvent.mCommand;

  if(mMacMenuHandle == selectedMenuHandle)
  {
	  if(!mConstructed) {
	    MenuConstruct(
	      aMenuEvent,
	      nsnull, //mParentWindow 
	      mDOMNode,
		  mWebShell);
		mConstructed = true;
	  } else {
	    //printf("Menu already constructed \n");
	  }
	  eventStatus = nsEventStatus_eConsumeNoDefault;
	  
  } else {
    // Make sure none of our submenus are the ones that should be handling this
      for (int i = mMenuItemVoidArray.Count(); i > 0; i--)
	  {
	    if(nsnull != mMenuItemVoidArray[i-1])
	    {
		    nsIMenu * submenu = nsnull;
		    ((nsISupports*)mMenuItemVoidArray[i-1])->QueryInterface(kIMenuIID, &submenu);
		    if(submenu)
		    {
			    nsIMenuListener * menuListener = nsnull;
			    ((nsISupports*)mMenuItemVoidArray[i-1])->QueryInterface(kIMenuListenerIID, &menuListener);
			    if(menuListener){
			      eventStatus = menuListener->MenuSelected(aMenuEvent);
			      NS_IF_RELEASE(menuListener);
			      if(nsEventStatus_eIgnore != eventStatus) {
			        NS_RELEASE(submenu);
			        return eventStatus;
			      }
			    }  
			    NS_RELEASE(submenu);
		    }
		}
	  }
  
  }
  return eventStatus;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  //printf("MenuDeselect called for %s\n", mLabel.ToNewCString());
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
    void              * menuNode,
	void              * aWebShell)
{
   //printf("nsMenu::MenuConstruct called for %s = %d \n", mLabel.ToNewCString(), mMacMenuHandle);
   // Begin menuitem inner loop

   // Open the node.
   nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(mDOMNode);
   if (domElement)
     domElement->SetAttribute("open", "true");
  
   gCurrentMenuDepth++;
   
    // Now get the kids. Retrieve our menupopup child.
    nsCOMPtr<nsIDOMNode> menuPopupNode;
    mDOMNode->GetFirstChild(getter_AddRefs(menuPopupNode));
    while (menuPopupNode) {
      nsCOMPtr<nsIDOMElement> menuPopupElement(do_QueryInterface(menuPopupNode));
      if (menuPopupElement) {
        nsString menuPopupNodeType;
        menuPopupElement->GetNodeName(menuPopupNodeType);
        if (menuPopupNodeType.Equals("menupopup"))
          break;
      }
      nsCOMPtr<nsIDOMNode> oldMenuPopupNode(menuPopupNode);
      oldMenuPopupNode->GetNextSibling(getter_AddRefs(menuPopupNode));
    }

    if (!menuPopupNode)
      return nsEventStatus_eIgnore;
      
  // Now get the kids
  nsCOMPtr<nsIDOMNode> menuitemNode;
  menuPopupNode->GetFirstChild(getter_AddRefs(menuitemNode));

	unsigned short menuIndex = 0;

  while (menuitemNode) {
    
    nsCOMPtr<nsIDOMElement> menuitemElement(do_QueryInterface(menuitemNode));
    if (menuitemElement) {
      nsString menuitemNodeType;
      nsString menuitemName;
      
      nsString label;
      menuitemElement->GetAttribute("value", label);
      //printf("label = %s \n", label.ToNewCString());
      
      menuitemElement->GetNodeName(menuitemNodeType);
      if (menuitemNodeType.Equals("menuitem")) {
        // LoadMenuItem
        LoadMenuItem(this, menuitemElement, menuitemNode, menuIndex, (nsIWebShell*)aWebShell);
      } else if (menuitemNodeType.Equals("menuseparator")) {
        AddSeparator();
      } else if (menuitemNodeType.Equals("menu")) {
        // Load a submenu
        LoadSubMenu(this, menuitemElement, menuitemNode);
      }
    }
	++menuIndex;
    nsCOMPtr<nsIDOMNode> oldmenuitemNode(menuitemNode);
    oldmenuitemNode->GetNextSibling(getter_AddRefs(menuitemNode));
  } // end menu item innner loop
  
  //printf("  Done building, mMenuItemVoidArray.Count() = %d \n", mMenuItemVoidArray.Count());
  
  gCurrentMenuDepth--;
    
	//PreviousMenuStackUnwind(this, mMacMenuHandle);
	//PushMenu(this);
             
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  //printf("nsMenu::MenuDestruct() called for %s \n", mLabel.ToNewCString());
  
  RemoveAll();
  //printf("  mMenuItemVoidArray.Count() = %d \n", mMenuItemVoidArray.Count());
  // Close the node.
  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(mDOMNode);
  if(!domElement) {
      NS_ERROR("Unable to QI dom element.");
      return nsEventStatus_eIgnore;  
  }
  
  nsCOMPtr<nsIContent> contentNode = do_QueryInterface(domElement);
  if (!contentNode) {
      NS_ERROR("DOM Node doesn't support the nsIContent interface required to handle DOM events.");
      return nsEventStatus_eIgnore;
  }
  
  nsIDocument * tmp = nsnull;
  contentNode->GetDocument(tmp);
  if(tmp)
    domElement->RemoveAttribute("open");
  
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
/**
* Set DOMNode
*
*/
NS_METHOD nsMenu::SetDOMNode(nsIDOMNode * aMenuNode)
{
    mDOMNode = aMenuNode;
	return NS_OK;
}

//-------------------------------------------------------------------------
/**
* Set DOMElement
*
*/
NS_METHOD nsMenu::SetDOMElement(nsIDOMElement * aMenuElement)
{
    mDOMElement = aMenuElement;
	return NS_OK;
}
    
//-------------------------------------------------------------------------
/**
* Set WebShell
*
*/
NS_METHOD nsMenu::SetWebShell(nsIWebShell * aWebShell)
{
    mWebShell = aWebShell;
	return NS_OK;
}
//-------------------------------------------------------------------------
void nsMenu::NSStringSetMenuItemText(MenuHandle macMenuHandle, short menuItem, nsString& menuString)
{
	OSErr					err;
	const PRUnichar*		unicodeText;
	char*					scriptRunText;
	size_t					unicodeTextLengthInBytes, unicdeTextReadInBytes,
							scriptRunTextSizeInBytes, scriptRunTextLengthInBytes,
							scriptCodeRunListLength;
	ScriptCodeRun			convertedTextScript;
	short					themeFontID;
	Str255					themeFontName;
	SInt16					themeFontSize;
	Style					themeFontStyle;
	
	//
	// extract the Unicode text from the nsString and convert it into a single script run
	//
	unicodeText = menuString.GetUnicode();
	unicodeTextLengthInBytes = menuString.Length() * sizeof(PRUnichar);
	scriptRunTextSizeInBytes = unicodeTextLengthInBytes * 2;
	scriptRunText = new char[scriptRunTextSizeInBytes];
	
	err = ::ConvertFromUnicodeToScriptCodeRun(mUnicodeTextRunConverter,
				unicodeTextLengthInBytes,unicodeText,
				0, /* no flags*/
				0,NULL,NULL,NULL, /* no offset arrays */
				scriptRunTextSizeInBytes,&unicdeTextReadInBytes,&scriptRunTextLengthInBytes,
				scriptRunText,
				1 /* count of script runs*/,&scriptCodeRunListLength,&convertedTextScript);
	NS_ASSERTION(err==noErr,"nsMenu::NSStringSetMenuItemText: ConvertFromUnicodeToScriptCodeRun failed.");
	if (err!=noErr) { delete [] scriptRunText; return; }
	scriptRunText[scriptRunTextLengthInBytes] = 0;	// null terminate
	
	//
	// get a font from the script code
	//
	err = ::GetThemeFont(kThemeSystemFont,convertedTextScript.script,themeFontName,&themeFontSize,&themeFontStyle);
	NS_ASSERTION(err==noErr,"nsMenu::NSStringSetMenuItemText: GetThemeFont failed.");
	if (err!=noErr) { delete [] scriptRunText; return; }
	::GetFNum(themeFontName,&themeFontID);
	err = ::SetMenuItemFontID(macMenuHandle,menuItem,themeFontID);
	NS_ASSERTION(err==noErr,"nsMenu::NSStringSetMenuItemText: SetMenuItemFontID failed.");
	::SetMenuItemText(macMenuHandle,menuItem,c2pstr(scriptRunText));
	
	
	//
	// clean up and exit
	//
	delete [] scriptRunText;
			
}

//-------------------------------------------------------------------------
MenuHandle nsMenu::NSStringNewMenu(short menuID, nsString& menuTitle)
{
	OSErr					err;
	const PRUnichar*		unicodeText;
	char*					scriptRunText;
	size_t					unicodeTextLengthInBytes, unicdeTextReadInBytes,
							scriptRunTextSizeInBytes, scriptRunTextLengthInBytes,
							scriptCodeRunListLength;
	ScriptCodeRun			convertedTextScript;
	short					themeFontID;
	Str255					themeFontName;
	SInt16					themeFontSize;
	Style					themeFontStyle;
	MenuHandle				newMenuHandle;
	
	//
	// extract the Unicode text from the nsString and convert it into a single script run
	//
	unicodeText = menuTitle.GetUnicode();
	unicodeTextLengthInBytes = menuTitle.Length() * sizeof(PRUnichar);
	scriptRunTextSizeInBytes = unicodeTextLengthInBytes * 2;
	scriptRunText = new char[scriptRunTextSizeInBytes];
	
	err = ::ConvertFromUnicodeToScriptCodeRun(mUnicodeTextRunConverter,
				unicodeTextLengthInBytes,unicodeText,
				0, /* no flags*/
				0,NULL,NULL,NULL, /* no offset arrays */
				scriptRunTextSizeInBytes,&unicdeTextReadInBytes,&scriptRunTextLengthInBytes,
				scriptRunText,
				1 /* count of script runs*/,&scriptCodeRunListLength,&convertedTextScript);
	NS_ASSERTION(err==noErr,"nsMenu::NSStringNewMenu: ConvertFromUnicodeToScriptCodeRun failed.");
	if (err!=noErr) { delete [] scriptRunText; return NULL; }
	scriptRunText[scriptRunTextLengthInBytes] = 0;	// null terminate
	
	//
	// get a font from the script code
	//
	err = ::GetThemeFont(kThemeSystemFont,convertedTextScript.script,themeFontName,&themeFontSize,&themeFontStyle);
	NS_ASSERTION(err==noErr,"nsMenu::NSStringNewMenu: GetThemeFont failed.");
	if (err!=noErr) { delete [] scriptRunText; return NULL; }
	::GetFNum(themeFontName,&themeFontID);
	newMenuHandle = ::NewMenu(menuID,c2pstr(scriptRunText));
	NS_ASSERTION(newMenuHandle!=NULL,"nsMenu::NSStringNewMenu: NewMenu failed.");
	if (newMenuHandle==NULL) { delete [] scriptRunText; return NULL; }
	err = SetMenuFont(newMenuHandle,themeFontID,themeFontSize);
	NS_ASSERTION(err==noErr,"nsMenu::NSStringNewMenu: SetMenuFont failed.");
	if (err!=noErr) { delete [] scriptRunText; return NULL; }

	//
	// clean up and exit
	//
	delete [] scriptRunText;
	return newMenuHandle;
			
}

//-------------------------------------------------------------------------
MenuHandle nsMenu::NSStringNewChildMenu(short menuID, nsString& menuTitle)
{
	OSErr					err;
	const PRUnichar*		unicodeText;
	char*					scriptRunText;
	size_t					unicodeTextLengthInBytes, unicdeTextReadInBytes,
							scriptRunTextSizeInBytes, scriptRunTextLengthInBytes,
							scriptCodeRunListLength;
	ScriptCodeRun			convertedTextScript;
	short					themeFontID;
	Str255					themeFontName;
	SInt16					themeFontSize;
	Style					themeFontStyle;
	MenuHandle				newMenuHandle;
	
	//
	// extract the Unicode text from the nsString and convert it into a single script run
	//
	unicodeText = menuTitle.GetUnicode();
	unicodeTextLengthInBytes = menuTitle.Length() * sizeof(PRUnichar);
	scriptRunTextSizeInBytes = unicodeTextLengthInBytes * 2;
	scriptRunText = new char[scriptRunTextSizeInBytes];
	
	err = ::ConvertFromUnicodeToScriptCodeRun(mUnicodeTextRunConverter,
				unicodeTextLengthInBytes,unicodeText,
				0, /* no flags*/
				0,NULL,NULL,NULL, /* no offset arrays */
				scriptRunTextSizeInBytes,&unicdeTextReadInBytes,&scriptRunTextLengthInBytes,
				scriptRunText,
				1 /* count of script runs*/,&scriptCodeRunListLength,&convertedTextScript);
	NS_ASSERTION(err==noErr,"nsMenu::NSStringNewMenu: ConvertFromUnicodeToScriptCodeRun failed.");
	if (err!=noErr) { delete [] scriptRunText; return NULL; }
	scriptRunText[scriptRunTextLengthInBytes] = 0;	// null terminate
	
	//
	// get a font from the script code
	//
	err = ::GetThemeFont(kThemeSystemFont,convertedTextScript.script,themeFontName,&themeFontSize,&themeFontStyle);
	NS_ASSERTION(err==noErr,"nsMenu::NSStringNewMenu: GetThemeFont failed.");
	if (err!=noErr) { delete [] scriptRunText; return NULL; }
	::GetFNum(themeFontName,&themeFontID);
	//newMenuHandle = ::NewMenu(menuID,c2pstr(scriptRunText));
	newMenuHandle = ::GetMenuHandle(menuID);
	
	NS_ASSERTION(newMenuHandle!=NULL,"nsMenu::NSStringNewMenu: NewMenu failed.");
	if (newMenuHandle==NULL) { delete [] scriptRunText; return NULL; }
	err = SetMenuFont(newMenuHandle,themeFontID,themeFontSize);
	NS_ASSERTION(err==noErr,"nsMenu::NSStringNewMenu: SetMenuFont failed.");
	if (err!=noErr) { delete [] scriptRunText; return NULL; }

	//
	// clean up and exit
	//
	delete [] scriptRunText;
	return newMenuHandle;
			
}

//----------------------------------------
void nsMenu::LoadMenuItem(
  nsIMenu *    pParentMenu,
  nsIDOMElement * menuitemElement,
  nsIDOMNode *    menuitemNode,
  unsigned short  menuitemIndex,
  nsIWebShell *   aWebShell)
{
  static const char* NS_STRING_TRUE = "true";
  nsString disabled;
  nsString menuitemName;
  nsString menuitemCmd;

  menuitemElement->GetAttribute(nsAutoString("disabled"), disabled);
  menuitemElement->GetAttribute(nsAutoString("value"), menuitemName);
  menuitemElement->GetAttribute(nsAutoString("cmd"), menuitemCmd);
  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuItemCID, nsnull, kIMenuItemIID, (void**)&pnsMenuItem);
  if (NS_OK == rv) {
    pnsMenuItem->Create(pParentMenu, menuitemName, 0);   
    //printf("menuitem %s \n", menuitemName.ToNewCString());
          
    // Create MenuDelegate - this is the intermediator inbetween 
    // the DOM node and the nsIMenuItem
    // The nsWebShellWindow wacthes for Document changes and then notifies the 
    // the appropriate nsMenuDelegate object
    nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(menuitemNode));
    if (!domElement) {
        SysBeep(30);
		return;
    }
    
    nsAutoString cmdAtom("oncommand");
    nsString cmdName;

    domElement->GetAttribute(cmdAtom, cmdName);
    //printf("%s \n", cmdName.ToNewCString());

    pnsMenuItem->SetCommand(cmdName);
	// DO NOT use passed in wehshell because of messed up windows dynamic loading
	// code. 
    pnsMenuItem->SetWebShell(mWebShell);
    pnsMenuItem->SetDOMElement(domElement);
    pnsMenuItem->SetDOMNode(menuitemNode);

	//NS_ASSERTION(false, "get debugger");
    // Set key shortcut and modifiers
    nsAutoString keyAtom("key");
    nsString keyValue;
    domElement->GetAttribute(keyAtom, keyValue);

    // Try to find the key node.
    nsCOMPtr<nsIDocument> document;
    nsCOMPtr<nsIContent> content = do_QueryInterface(domElement);
    if (NS_FAILED(rv = content->GetDocument(*getter_AddRefs(document)))) {
      NS_ERROR("Unable to retrieve the document.");
      return; //rv;
    }

    // Turn the document into a XUL document so we can use getElementById
    nsCOMPtr<nsIDOMXULDocument> xulDocument = do_QueryInterface(document);
    if (xulDocument == nsnull) {
      NS_ERROR("not XUL!");
      return; //NS_ERROR_FAILURE;
    }
  
    nsIDOMElement * keyElement = nsnull;
    xulDocument->GetElementById(keyValue, &keyElement);
    
    if(keyElement){
        PRUint8 modifiers = knsMenuItemNoModifier;
	    nsAutoString shiftAtom("shift");
	    nsAutoString altAtom("alt");
	    nsAutoString commandAtom("command");
	    nsString shiftValue;
	    nsString altValue;
	    nsString commandValue;
		nsString controlValue;
	    nsString keyChar = " ";
	    
	    keyElement->GetAttribute(keyAtom, keyChar);
	    keyElement->GetAttribute(shiftAtom, shiftValue);
	    keyElement->GetAttribute(altAtom, altValue);
	    keyElement->GetAttribute(commandAtom, commandValue);
	    
		if(keyChar != " ") 
	      pnsMenuItem->SetShortcutChar(keyChar);
	      
		if(shiftValue == "true") 
		  modifiers |= knsMenuItemShiftModifier;
	    
	    if(altValue == "true")
	      modifiers |= knsMenuItemAltModifier;
	    
	    if(commandValue == "true")
	      modifiers |= knsMenuItemCommandModifier;

		if(controlValue == "true")
	      modifiers |= knsMenuItemControlModifier;
	      
        pnsMenuItem->SetModifiers(modifiers);
    }

	if(disabled == NS_STRING_TRUE)
      pnsMenuItem->SetEnabled(PR_FALSE);
    else
      pnsMenuItem->SetEnabled(PR_TRUE);

	nsISupports * supports = nsnull;
    pnsMenuItem->QueryInterface(kISupportsIID, (void**) &supports);
    pParentMenu->AddItem(supports); // Parent should now own menu item
    NS_RELEASE(supports);

	NS_RELEASE(pnsMenuItem);
  } 
  return;
}

//----------------------------------------
void nsMenu::LoadSubMenu(
  nsIMenu *       pParentMenu,
  nsIDOMElement * menuElement,
  nsIDOMNode *    menuNode)
{
  nsString menuName; 
  menuElement->GetAttribute(nsAutoString("value"), menuName);
  //printf("Creating Menu [%s] \n", menuName.ToNewCString()); // this leaks

  // Create nsMenu
  nsIMenu * pnsMenu = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuCID, nsnull, kIMenuIID, (void**)&pnsMenu);
  if (NS_OK == rv) {
    // Call Create
    nsISupports * supports = nsnull;
    pParentMenu->QueryInterface(kISupportsIID, (void**) &supports);
    pnsMenu->Create(supports, menuName);
    NS_RELEASE(supports); // Balance QI
    
    // Set nsMenu Name
    pnsMenu->SetLabel(menuName); 

    // Make nsMenu a child of parent nsMenu. The parent takes ownership
    supports = nsnull;
    pnsMenu->QueryInterface(kISupportsIID, (void**) &supports);
	pParentMenu->AddItem(supports);
	NS_RELEASE(supports);

	pnsMenu->SetWebShell(mWebShell);
	pnsMenu->SetDOMNode(menuNode);
	pnsMenu->SetDOMElement(menuElement);

	// We're done with the menu
	NS_RELEASE(pnsMenu);
  }     
}