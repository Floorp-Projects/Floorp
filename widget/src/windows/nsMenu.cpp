/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIDOMXULDocument.h"

#include "nsMenu.h"
#include "nsIMenu.h"
#include "nsIMenuItem.h"

#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include <windows.h>

#include "nsIAppShell.h"
#include "nsGUIEvent.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsGfxCIID.h"
#include "nsMenuItem.h"
#include "nsCOMPtr.h"
#include "nsIMenuListener.h"
#include "nsIComponentManager.h"

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
  if (aIID.Equals(kIMenuListenerIID)) {                                      
    *aInstancePtr = (void*)(nsIMenuListener*)this;                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }                                                     
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*)(nsISupports*)(nsIMenu*)this;                        
    NS_ADDREF_THIS();                                                    
    return NS_OK;                                                        
  }
  return NS_NOINTERFACE;                                                 
}

//-------------------------------------------------------------------------
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
  mMenu           = nsnull;
  mMenuBarParent  = nsnull;
  mMenuParent     = nsnull;
  mListener       = nsnull;
  nsresult result = NS_NewISupportsArray(&mItems);
  mDOMNode        = nsnull;
  mDOMElement     = nsnull;
  mWebShell       = nsnull;
  mConstructed    = false;
  mLabel          = " ";
}

//-------------------------------------------------------------------------
//
// nsMenu destructor
//
//-------------------------------------------------------------------------
nsMenu::~nsMenu()
{
  NS_IF_RELEASE(mListener);

  // Remove all references to the items
  mItems->Clear();
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
      mMenuBarParent = menubar;

      NS_RELEASE(menubar); // Balance the QI
	} else {
      nsIMenu * menu = nsnull;
      aParent->QueryInterface(kIMenuIID, (void**) &menu);
      if(menu)
	  {
        mMenuParent = menu;

		NS_RELEASE(menu); // Balance the QI
	  }
	}
  }

  mLabel = aLabel;
  mMenu = CreateMenu();
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
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetAccessKey(nsString &aText)
{
  aText = mAccessKey;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetAccessKey(const nsString &aText)
{
  mAccessKey = aText;
  return NS_OK;
}

//-------------------------------------------------------------------------
/**
* Set enabled state
*
*/
NS_METHOD nsMenu::SetEnabled(PRBool aIsEnabled)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
/**
* Get enabled state
*
*/
NS_METHOD nsMenu::GetEnabled(PRBool* aIsEnabled)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
/**
* Query if this is the help menu
*
*/
NS_METHOD nsMenu::IsHelpMenu(PRBool* aIsHelpMenu)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddItem(nsISupports * aItem)

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
// This does not return a ref counted object
// This is NOT an nsIMenu method
nsIMenuBar * nsMenu::GetMenuBar(nsIMenu * aMenu)
{
  if (!aMenu) {
    return nsnull;
  }

  nsMenu * menu = (nsMenu *)aMenu;

  if (menu->GetMenuBarParent()) {
    return menu->GetMenuBarParent();
  }

  if (menu->GetMenuParent()) {
    return GetMenuBar(menu->GetMenuParent());
  }

  return nsnull;
}

//-------------------------------------------------------------------------
// This does not return a ref counted object
// This is NOT an nsIMenu method
nsIWidget *  nsMenu::GetParentWidget()
{
  nsIWidget  * parent  = nsnull;
  nsIMenuBar * menuBar = GetMenuBar(this);
  if (menuBar) {
    menuBar->GetParent(parent);
  } 

  return parent;
}


//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenuItem(nsIMenuItem * aMenuItem)
{
  PRUint32 cnt;
  nsresult rv = mItems->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  return InsertItemAt(cnt, (nsISupports *)aMenuItem);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenu(nsIMenu * aMenu)
{
  PRUint32 cnt;
  nsresult rv = mItems->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  return InsertItemAt(cnt, (nsISupports *)aMenu);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddSeparator() 
{
  PRUint32 cnt;
  nsresult rv = mItems->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  InsertSeparator(cnt);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetItemCount(PRUint32 &aCount)
{
  return mItems->Count(&aCount);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetItemAt(const PRUint32 aCount, nsISupports *& aMenuItem)
{
  aMenuItem = (nsISupports *)mItems->ElementAt(aCount);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::InsertItemAt(const PRUint32 aCount, nsISupports * aMenuItem)
{
  nsString name;
  BOOL status = FALSE;

  mItems->InsertElementAt(aMenuItem, (PRInt32)aCount);

  nsCOMPtr<nsIMenuItem> menuItem(do_QueryInterface(aMenuItem));
  if (menuItem) {
    menuItem->GetLabel(name);
#ifdef DEBUG
    printf("%s \n", NS_ConvertUCS2toUTF8(name).get());
#endif
    nsIWidget * win = GetParentWidget();
    PRInt32 id = ((nsWindow *)win)->GetNewCmdMenuId();
    ((nsMenuItem *)((nsIMenuItem *)menuItem))->SetCmdId(id);   

	//NS_ASSERTION(false, "get debugger");

	PRUint8 modifiers = knsMenuItemNoModifier;
    menuItem->GetModifiers(&modifiers);

    if(modifiers != knsMenuItemNoModifier) {
          //strcat the shortcut to the end of the name
          nsString shortcut = "\t";
  
          name.Append(shortcut);

          //Ctrl + Alt + Shift
          PRBool isFirst = PR_TRUE;
          if((knsMenuItemCommandModifier & modifiers) || (knsMenuItemControlModifier & modifiers))
		  {
            isFirst = PR_FALSE;
		  }

          if(knsMenuItemAltModifier & modifiers)
		  {
	        if(isFirst)
	 	      isFirst = PR_FALSE;
	        else
		      name.Append(" + ");

            name.Append("Alt");
		  }

          if(knsMenuItemShiftModifier & modifiers)
		  {
	        if(isFirst)
		      isFirst = PR_FALSE;
	        else
		      name.Append(" + ");

	        name.Append("Shift");
		  }
        
          menuItem->GetShortcutChar(shortcut);

          if(!isFirst)
            name.Append(" + ");

          name.Append(shortcut);
	}

    char * nameStr = GetACPString(name);

    MENUITEMINFO menuInfo;
    menuInfo.cbSize     = sizeof(menuInfo);
    menuInfo.fMask      = MIIM_TYPE | MIIM_ID;
    menuInfo.fType      = MFT_STRING;
    menuInfo.dwTypeData = nameStr;
    menuInfo.wID        = (DWORD)id;
    menuInfo.cch        = strlen(nameStr);

    status = ::InsertMenuItem(mMenu, aCount, TRUE, &menuInfo);

    delete[] nameStr;
  } else {
    nsCOMPtr<nsIMenu> menu(do_QueryInterface(aMenuItem));
    if (menu) {
      nsString name;
      menu->GetLabel(name);
      //mItems->AppendElement((nsISupports *)(nsIMenu *)menu);

      char * nameStr = GetACPString(name);

      HMENU nativeMenuHandle;
      void * voidData;
      menu->GetNativeData(&voidData);

      nativeMenuHandle = (HMENU)voidData;

      MENUITEMINFO menuInfo;

      menuInfo.cbSize     = sizeof(menuInfo);
      menuInfo.fMask      = MIIM_SUBMENU | MIIM_TYPE;
      menuInfo.hSubMenu   = nativeMenuHandle;
      menuInfo.fType      = MFT_STRING;
      menuInfo.dwTypeData = nameStr;

      BOOL status = ::InsertMenuItem(mMenu, aCount, TRUE, &menuInfo);

      delete[] nameStr;
    }
  }

  return (status ? NS_OK : NS_ERROR_FAILURE);
}


//-------------------------------------------------------------------------
NS_METHOD nsMenu::InsertSeparator(const PRUint32 aCount)
{
  nsISupports * supports = nsnull;
  QueryInterface(kISupportsIID, (void**) &supports);
  if(supports){
	nsMenuItem * item = new nsMenuItem();
    item->Create(supports, "", PR_TRUE);
	NS_RELEASE(supports);
    mItems->InsertElementAt((nsISupports *)(nsIMenuItem *)item, (PRInt32)aCount);
  }
    MENUITEMINFO menuInfo;

    menuInfo.cbSize = sizeof(menuInfo);
    menuInfo.fMask  = MIIM_TYPE;
    menuInfo.fType  = MFT_SEPARATOR;

    BOOL status = ::InsertMenuItem(mMenu, aCount, TRUE, &menuInfo);
  
  return (status ? NS_OK : NS_ERROR_FAILURE);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveItem(const PRUint32 aCount)
{

  //nsISupports * supports = (nsISupports *)mItems->ElementAt(aCount);
  mItems->RemoveElementAt(aCount);

  return (::RemoveMenu(mMenu, aCount, MF_BYPOSITION) ? NS_OK:NS_ERROR_FAILURE);
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveAll()
{
  while (PR_TRUE) {
    PRUint32 cnt;
    nsresult rv = mItems->Count(&cnt);
    if (NS_FAILED(rv)) return rv;
    if (cnt == 0)
      break;
    mItems->RemoveElementAt(0);
    ::RemoveMenu(mMenu, 0, MF_BYPOSITION);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetNativeData(void ** aData)

{
  *aData = (void *)mMenu;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetNativeData(void * aData)

{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenuListener(nsIMenuListener * aMenuListener)
{
  NS_IF_RELEASE(mListener);
  mListener = aMenuListener;
  NS_IF_ADDREF(mListener);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
  if (aMenuListener == mListener) {
    NS_IF_RELEASE(mListener);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
// nsIMenuListener interface
//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
#ifdef DEBUG_saari
  char* menuLabel = GetACPString(mLabel);
  printf("Menu Item Selected %s\n", menuLabel);
  delete[] menuLabel;
#endif
  if (nsnull != mListener) {
    NS_ASSERTION(false, "get debugger");
    mListener->MenuSelected(aMenuEvent);
  }
  return nsEventStatus_eIgnore;
}


nsEventStatus nsMenu::MenuSelected(const nsMenuEvent & aMenuEvent)
{
#ifdef DEBUG
  //printf("nsMenu::MenuSelected called\n");
#endif
  
  if(mConstructed){
	MenuDestruct(aMenuEvent);
    mConstructed = false;
  }

  if(!mConstructed) {
    MenuConstruct(
      aMenuEvent,
      mParentWindow, 
      mDOMNode,
	  mWebShell);
	mConstructed = true;
  }

  if (nsnull != mListener) {
    NS_ASSERTION(false, "get debugger");
    mListener->MenuSelected(aMenuEvent);
  }
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
#if 0
  printf("nsMenu::MenuDeselected called\n");  
  MenuDestruct(aMenuEvent);
  mConstructed = false;
#endif

  if (nsnull != mListener) {
    NS_ASSERTION(false, "get debugger");
    mListener->MenuDeselected(aMenuEvent);
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
#ifdef DEBUG
   //printf("nsMenu::MenuConstruct called \n");
#endif

   // Begin menuitem inner loop

  // Open the node.
  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(mDOMNode);
  if (domElement)
    domElement->SetAttribute("open", "true");

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

  nsCOMPtr<nsIDOMNode> menuitemNode;
  menuPopupNode->GetFirstChild(getter_AddRefs(menuitemNode));

	unsigned short menuIndex = 0;

  while (menuitemNode) {
    nsCOMPtr<nsIDOMElement> menuitemElement(do_QueryInterface(menuitemNode));
    if (menuitemElement) {
      nsString menuitemNodeType;
      nsString menuitemName;
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
  
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
#ifdef DEBUG
  //printf("nsMenu::MenuDestruct called \n");
#endif

  // We cannot call RemoveAll() yet because menu item selection may need it
  //RemoveAll();
  
  // Close the node.
  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(mDOMNode);
  if (domElement)
    domElement->RemoveAttribute("open");

  // Now remove the kids
  while (PR_TRUE) {
    PRUint32 cnt;
    nsresult rv = mItems->Count(&cnt);
    if (NS_FAILED(rv)) break;   // XXX error?
    if (cnt == 0)
      break;
    mItems->RemoveElementAt(0);
    ::RemoveMenu(mMenu, 0, MF_BYPOSITION);
  }
  return nsEventStatus_eIgnore;
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
  menuitemElement->GetAttribute(nsAutoString("label"), menuitemName);
  menuitemElement->GetAttribute(nsAutoString("cmd"), menuitemCmd);
  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuItemCID, nsnull, kIMenuItemIID, (void**)&pnsMenuItem);
  if (NS_OK == rv) {
    pnsMenuItem->Create(pParentMenu, menuitemName, 0);   
	

          
    // Create MenuDelegate - this is the intermediator inbetween 
    // the DOM node and the nsIMenuItem
    // The nsWebShellWindow wacthes for Document changes and then notifies the 
    // the appropriate nsMenuDelegate object
    nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(menuitemNode));
    if (!domElement) {
		return;
    }
    
    nsAutoString cmdAtom("oncommand");
    nsString cmdName;

    domElement->GetAttribute(cmdAtom, cmdName);
#ifdef DEBUG
    printf("%s \n", NS_ConvertUCS2toUTF8(cmdName).get());
#endif
    pnsMenuItem->SetCommand(cmdName);
	// DO NOT use passed in wehshell because of messed up windows dynamic loading
	// code. 
    pnsMenuItem->SetWebShell(mWebShell);
    pnsMenuItem->SetDOMElement(domElement);

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
  
    nsCOMPtr<nsIDOMElement> keyElement;
    xulDocument->GetElementById(keyValue, getter_AddRefs(keyElement));
    
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


	if(disabled == NS_STRING_TRUE )
		::EnableMenuItem(mMenu, menuitemIndex, MF_BYPOSITION | MF_GRAYED);

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
  menuElement->GetAttribute(nsAutoString("label"), menuName);
#ifdef DEBUG
  //printf("Creating Menu [%s] \n", NS_ConvertUCS2toUTF8(menuName).get());
#endif

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


//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetDOMNode(nsIDOMNode * menuNode)
{
  mDOMNode = menuNode;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetDOMNode(nsIDOMNode ** menuNode)
{
  if(menuNode) {
    *menuNode = mDOMNode;
    NS_IF_ADDREF(menuNode);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetDOMElement(nsIDOMElement * menuElement)
{
  mDOMElement = menuElement;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetWebShell(nsIWebShell * aWebShell)
{
  mWebShell = aWebShell;
  return NS_OK;
}


char* GetACPString(const nsString& aStr)
{
   int acplen = aStr.Length() * 2 + 1;
   char * acp = new char[acplen];
   if(acp)
   {
      int outlen = ::WideCharToMultiByte( CP_ACP, 0, 
                      aStr.get(), aStr.Length(),
                      acp, acplen, NULL, NULL);
      if (outlen >= 0)
         acp[outlen] = '\0';  // null terminate
   }
   return acp;
}
