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

#include "nsMenu.h"
#include "nsIMenu.h"
#include "nsIMenuBar.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"
#include "nsIPresContext.h"

#include "nsString.h"
#include "nsStringUtil.h"

#include <Appearance.h>
#include <TextUtils.h>
#include <ToolUtils.h>
#include <Devices.h>
#include <UnicodeConverter.h>
#include <Fonts.h>
#include <Sound.h>
#include <Balloons.h>

#include "nsDynamicMDEF.h"

// Beginning ID for top level menus. IDs 2-5 are the 4 Golden Hierarchical Menus
const PRInt16 kMacMenuID = 6; 

#ifdef APPLE_MENU_HACK
const PRInt16 kAppleMenuID = 1;
#endif /* APPLE_MENU_HACK */

// Submenu depth tracking
PRInt16 gMenuDepth = 0;
PRInt16 gCurrentMenuDepth = 1;

extern nsIMenuBar * gMacMenubar;
extern Handle gMDEF; // Our stub MDEF
extern Handle gSystemMDEFHandle;
PRInt16 mMacMenuIDCount = kMacMenuID;

static PRBool gConstructingMenu = PR_FALSE;
  
// CIDs
#include "nsWidgetsCID.h"
static NS_DEFINE_CID(kMenuBarCID,  NS_MENUBAR_CID);
static NS_DEFINE_CID(kMenuCID,     NS_MENU_CID);
static NS_DEFINE_CID(kMenuItemCID, NS_MENUITEM_CID);

//-------------------------------------------------------------------------
NS_IMPL_ISUPPORTS3(nsMenu, nsIMenu, nsIMenuListener, nsIDocumentObserver)

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
  mIsHelpMenu    = PR_FALSE;
  mHelpMenuOSItemsCount = 0;
  mIsEnabled     = PR_TRUE;
  mListener      = nsnull;
  mConstructed   = nsnull;
  mDestroyHandlerCalled = PR_FALSE;
  
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

     nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mWebShell));
     
  	  nsCOMPtr<nsIContentViewer> cv;
	  docShell->GetContentViewer(getter_AddRefs(cv));
	  if (cv) {
	   
	    nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
	    if (!docv)
	      return;

	    nsCOMPtr<nsIDocument> doc;
	    docv->GetDocument(*getter_AddRefs(doc));
	    if (!doc)
	      return;

	    doc->RemoveObserver(NS_STATIC_CAST(nsIDocumentObserver*, this));
	  }
	  
  RemoveAll();
  
  err = ::DisposeUnicodeToTextRunInfo(&mUnicodeTextRunConverter);
  NS_ASSERTION(err==noErr,"nsMenu::~nsMenu: DisposeUnicodeToTextRunInfo failed.");	 
   
  // Don't destroy the 4 Golden Hierarchical Menu
  if((mMacMenuID > 5) || (mMacMenuID < 2) && !mIsHelpMenu) {
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
    nsCOMPtr<nsIMenuBar> menubar ( do_QueryInterface(aParent) );
    if ( menubar )
      mMenuBarParent = menubar;
    else
    {
      nsCOMPtr<nsIMenu> menu ( do_QueryInterface(aParent) );
      if ( menu )
      	mMenuParent = menu;
    }
  }
  
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetParent(nsISupports*& aParent)
{

  aParent = nsnull;
  if ( mMenuParent )
    return mMenuParent->QueryInterface(NS_GET_IID(nsISupports),(void**)&aParent);
  else if ( mMenuBarParent )
    return mMenuBarParent->QueryInterface(NS_GET_IID(nsISupports),(void**)&aParent);

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
#if DEBUG_saari
    if(mMacMenuID == 16)
      SysBeep(30);
#endif

    //mMacMenuHandle = NSStringNewChildMenu(mMacMenuID, mLabel);
    mMacMenuHandle = ::GetMenuHandle(mMacMenuID);
  } else {
#if !TARGET_CARBON
    // Look at the label and figure out if it is the "Help" menu

    if(mDOMElement) {
      nsString menuIDstring;
      mDOMElement->GetAttribute(NS_ConvertASCIItoUCS2("id"), menuIDstring);
      if(menuIDstring.EqualsWithConversion("menu_Help")) {
      mIsHelpMenu = PR_TRUE;
      ::HMGetHelpMenuHandle(&mMacMenuHandle);
      mMacMenuID = kHMHelpMenuID;
      
      int numHelpItems = ::CountMenuItems(mMacMenuHandle);
        if ( mHelpMenuOSItemsCount == 0)
          mHelpMenuOSItemsCount = numHelpItems;
        for(int i=0; i<numHelpItems; ++i) {
          mMenuItemVoidArray.AppendElement(nsnull);
        }
     
      return NS_OK;
      }
    }
#endif
    mMacMenuHandle = NSStringNewMenu(mMacMenuIDCount, mLabel);
    mMacMenuID = mMacMenuIDCount;
#if DEBUG_saari
    if(mMacMenuID == 16)
      SysBeep(30);
#endif
      
    mMacMenuIDCount++;
#if !TARGET_CARBON
    // Replace standard MDEF with our stub MDEF
    if(mMacMenuHandle) {    
      SInt8 state = ::HGetState((Handle)mMacMenuHandle);
      ::HLock((Handle)mMacMenuHandle);
      //gSystemMDEFHandle = (**mMacMenuHandle).menuProc;
      (**mMacMenuHandle).menuProc = gMDEF;
      ::HSetState((Handle)mMacMenuHandle, state);
    }
#endif
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
  if(aItem) {
    // Figure out what we're adding
    nsCOMPtr<nsIMenuItem> menuitem ( do_QueryInterface(aItem) );
    if ( menuitem )
      AddMenuItem(menuitem);
    else {
      nsCOMPtr<nsIMenu> menu ( do_QueryInterface(aItem) );
      if ( menu )
        AddMenu(menu);
    }
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::AddMenuItem(nsIMenuItem * aMenuItem)
{
  if(aMenuItem) {
    nsISupports * supports = nsnull;
    aMenuItem->QueryInterface(NS_GET_IID(nsISupports), (void**)&supports);
    if(supports) {
	  mMenuItemVoidArray.AppendElement(supports);
	  PRInt32 currItemIndex = mMenuItemVoidArray.Count();
      
	  nsString label;
	  aMenuItem->GetLabel(label);
	  //printf("%s \n", label.ToNewCString());
	  //printf("%d = mMacMenuID\n", mMacMenuID);
	  mNumMenuItems++;
	  
	  if(mIsHelpMenu) {
	    char labelStr[256];
	    ::InsertMenuItem(mMacMenuHandle, c2pstr(label.ToCString(labelStr, sizeof(labelStr))),
	                     mMenuItemVoidArray.Count());
	  } else {
	    ::InsertMenuItem(mMacMenuHandle, "\pa", currItemIndex);
	    NSStringSetMenuItemText(mMacMenuHandle, currItemIndex, label);
	  }
	  
	  // I want to be internationalized too!
	  nsString keyEquivalent; keyEquivalent.AssignWithConversion(" ");
	  aMenuItem->GetShortcutChar(keyEquivalent);
	  if(!keyEquivalent.EqualsWithConversion(" ")) {
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
    aMenu->QueryInterface(NS_GET_IID(nsISupports), (void**)&supports);
    if(supports) {
      mMenuItemVoidArray.AppendElement(supports);
      PRInt32 currItemIndex = mMenuItemVoidArray.Count();
  
      // We have to add it as a menu item and then associate it with the item
      nsString label;
      aMenu->GetLabel(label);
      //printf("AddMenu %s \n", label.ToNewCString());
      mNumMenuItems++;

      ::InsertMenuItem(mMacMenuHandle, "\pb", currItemIndex);
      NSStringSetMenuItemText(mMacMenuHandle, currItemIndex, label);

  	  PRBool isEnabled;
  	  aMenu->GetEnabled(&isEnabled);
  	  if(isEnabled)
  	    ::EnableMenuItem(mMacMenuHandle, currItemIndex);
  	  else
  	    ::DisableMenuItem(mMacMenuHandle, currItemIndex);	    
        
      PRInt16 temp = gCurrentMenuDepth;
      ::SetMenuItemHierarchicalID((MenuHandle) mMacMenuHandle, currItemIndex, temp);
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
#ifdef notdef
#if !TARGET_CARBON
  MenuHandle helpmh;
  ::HMGetHelpMenuHandle(&helpmh);
  if ( helpmh != mMacMenuHandle)
    helpmh = nsnull;
#endif
#endif

  while(mMenuItemVoidArray.Count())
  {
    --mNumMenuItems;
    
    if(mMenuItemVoidArray[mMenuItemVoidArray.Count() - 1]) {
      // Figure out what we're releasing
      nsIMenuItem * menuitem = nsnull;
      ((nsISupports*)mMenuItemVoidArray[mMenuItemVoidArray.Count() - 1])->QueryInterface(NS_GET_IID(nsIMenuItem), (void**) &menuitem);  
      if(menuitem)
      {
        // case menuitem
        menuitem->Release(); // Release our hold
        NS_IF_RELEASE(menuitem); // Balance QI
      }
      else
      {
	    nsIMenu * menu = nsnull;
	    ((nsISupports*)mMenuItemVoidArray[mMenuItemVoidArray.Count() - 1])->QueryInterface(NS_GET_IID(nsIMenu), (void**) &menu);
	    if(menu)
	    {
	      // case menu
	      menu->Release(); // Release our hold 
	      NS_IF_RELEASE(menu); // Balance QI
	    }
	  }
	}
	  /* don't delete the actual Mac menu item if it's a MacOS item */
	  if ( (mMenuItemVoidArray.Count() - mHelpMenuOSItemsCount) > 0)
	  {
	::DeleteMenuItem(mMacMenuHandle, mMenuItemVoidArray.Count());
    }
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
NS_METHOD nsMenu::SetNativeData(void * aData)
{
  mMacMenuHandle = (MenuHandle) aData;
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
		else if (menuItemID == 1)
		{
			/* handle about app here */
			nsresult rv = NS_ERROR_FAILURE;
		 
		    // Go find the about menu item
		    nsCOMPtr<nsIDOMDocument> domDoc; 
		    if (!mDOMNode) {
		      	NS_ERROR("nsMenu mDOMNode is null.");
		      	return nsEventStatus_eConsumeNoDefault;
		  	}
		    mDOMNode->GetOwnerDocument(getter_AddRefs(domDoc));
		    
		    if (!domDoc) {
		      	NS_ERROR("No owner document for nsMenu DOM node.");
		      	return nsEventStatus_eConsumeNoDefault;
		  	}
		    nsCOMPtr<nsIDOMXULDocument> xulDoc = do_QueryInterface(domDoc);
		    if (!xulDoc) {
		      	NS_ERROR("nsIDOMDocument to nsIDOMXULDocument QI failed.");
		      	return nsEventStatus_eConsumeNoDefault;
		  	}
		    
		    // "releaseName" is the current node id for the About Mozilla/Netscape
		    // menu node.
		    nsCOMPtr<nsIDOMElement> domElement;
		    xulDoc->GetElementById(NS_ConvertASCIItoUCS2("releaseName"), getter_AddRefs(domElement));
		    if (!domElement) {
		      	NS_ERROR("GetElementById failed.");
		      	return nsEventStatus_eConsumeNoDefault;
		  	}
		    
		    // Now get the pres context so we can execute the command
		  	nsCOMPtr<nsIPresContext> presContext;
		  	MenuHelpers::WebShellToPresContext ( mWebShell, getter_AddRefs(presContext) );

		  	nsEventStatus status = nsEventStatus_eIgnore;
		  	nsMouseEvent event;
		  	event.eventStructType = NS_MOUSE_EVENT;
		  	event.message = NS_MENU_ACTION;

		  	nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
		  	if(!element) {
		      	NS_ERROR("Unable to QI dom element.");
		      	return nsEventStatus_eConsumeNoDefault; 
		  	}
		  
		  	nsCOMPtr<nsIContent> contentNode;
		  	contentNode = do_QueryInterface(domElement);
		  	if (!contentNode) {
		      	NS_ERROR("DOM Node doesn't support the nsIContent interface required to handle DOM events.");
		      	return nsEventStatus_eConsumeNoDefault;
		  	}

		  	rv = contentNode->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);

		  	return nsEventStatus_eConsumeNoDefault;
		}
	}
	else
#endif
  if ((kHMHelpMenuID == menuID) && (menuID != mMacMenuID))
  {
    /* 'this' is not correct; we need to find the help nsMenu */
	  nsIMenuBar *mb = mMenuBarParent;
	  if ( mb == nsnull )
	  {
	    mb = gMacMenubar;
	    if ( mb == nsnull )
	    {
        return nsEventStatus_eIgnore;
      }
    }

    /* set up a default event to query with */
    nsMenuEvent event;
    MenuHandle handle;
#if !TARGET_CARBON
    // XXX fix me for carbon!
    ::HMGetHelpMenuHandle(&handle);
#endif
    event.mCommand = (unsigned int) handle;

    /* loop through the top-level menus in the menubar */
	  PRUint32 numMenus = 0;
	  mb->GetMenuCount(numMenus);
	  numMenus--;
	  for (PRInt32 i = numMenus; i >= 0; i--)
	  {
	    nsCOMPtr<nsIMenu> menu;
	    mb->GetMenuAt(i, *getter_AddRefs(menu));
	    if (menu)
	    {
        nsCOMPtr<nsIMenuListener> listener(do_QueryInterface(menu));
        if (listener)
        {
          nsString label;
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
  }
  else

  if(mMacMenuID == menuID)
  {
    // Call MenuItemSelected on the correct nsMenuItem
    PRInt16 menuItemID = LoWord(((nsMenuEvent)aMenuEvent).mCommand);
    if(mMenuItemVoidArray[menuItemID-1]) {
      nsCOMPtr<nsIMenuListener> menuListener ( do_QueryInterface((nsIMenuItem*)mMenuItemVoidArray[menuItemID-1]) );
      if(menuListener) {
        // call our ondestroy handler now because the menu is going away.
        // do it now before sending the event into the dom in case our window
        // goes away.
        OnDestroy();
        
        eventStatus = menuListener->MenuItemSelected(aMenuEvent);
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
	    nsCOMPtr<nsIMenu> submenu ( do_QueryInterface((nsISupports*)mMenuItemVoidArray[i-1]) );
	    if(submenu)
	    {
		    nsCOMPtr<nsIMenuListener> menuListener ( do_QueryInterface((nsISupports*)mMenuItemVoidArray[i-1]) );
		    if(menuListener){
          // call our ondestroy handler now because the menu is going away.
          // do it now before sending the event into the dom in case our window
          // goes away.
          OnDestroy();
          
		      eventStatus = menuListener->MenuItemSelected(aMenuEvent);
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
    if (mIsHelpMenu && mConstructed){
	    RemoveAll();
	    mConstructed = false;
	  }
    
	  if(!mConstructed) {
	    if(mIsHelpMenu) {
	      HelpMenuConstruct(
	        aMenuEvent,
	        nsnull, //mParentWindow 
	        mDOMNode,
		    mWebShell);	      
		    mConstructed = true;
	    } else {
	      MenuConstruct(
	        aMenuEvent,
	        nsnull, //mParentWindow 
	        mDOMNode,
		    mWebShell);
		  mConstructed = true;
		}
		
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
		    nsCOMPtr<nsIMenu> submenu ( do_QueryInterface((nsISupports*)mMenuItemVoidArray[i-1]) );
		    if(submenu)
		    {
			    nsCOMPtr<nsIMenuListener> menuListener ( do_QueryInterface((nsISupports*)mMenuItemVoidArray[i-1]) );
			    if(menuListener){
			      eventStatus = menuListener->MenuSelected(aMenuEvent);
			      if(nsEventStatus_eIgnore != eventStatus)
			        return eventStatus;
			    }  
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
  gConstructingMenu = PR_TRUE;
  
  // reset destroy handler flag so that we'll know to fire it next time this menu goes away.
  mDestroyHandlerCalled = PR_FALSE;
  
  //printf("nsMenu::MenuConstruct called for %s = %d \n", mLabel.ToNewCString(), mMacMenuHandle);
  // Begin menuitem inner loop
  
  // Open the node.
  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(mDOMNode);
  if (domElement)
    domElement->SetAttribute(NS_ConvertASCIItoUCS2("open"), NS_ConvertASCIItoUCS2("true"));
  
  gCurrentMenuDepth++;
   
  // Now get the kids. Retrieve our menupopup child.
  nsCOMPtr<nsIDOMNode> menuPopupNode;
  GetMenuPopupElement ( getter_AddRefs(menuPopupNode) );
  if (!menuPopupNode)
    return nsEventStatus_eIgnore;
      
  // Now get the kids
  nsCOMPtr<nsIDOMNode> menuitemNode;
  menuPopupNode->GetFirstChild(getter_AddRefs(menuitemNode));

  unsigned short menuIndex = 0;

  // Fire our oncreate handler. If we're told to stop, don't build the menu at all
  PRBool keepProcessing = OnCreate();
  if ( keepProcessing ) {
    while (menuitemNode) {
      
      nsCOMPtr<nsIDOMElement> menuitemElement(do_QueryInterface(menuitemNode));
      if (menuitemElement) {
        nsString menuitemNodeType;
        nsString menuitemName;
        
        nsString label;
        menuitemElement->GetAttribute(NS_ConvertASCIItoUCS2("value"), label);
        //printf("label = %s \n", label.ToNewCString());
        
        menuitemElement->GetNodeName(menuitemNodeType);
        if (menuitemNodeType.EqualsWithConversion("menuitem")) {
          // LoadMenuItem
          LoadMenuItem(this, menuitemElement, menuitemNode, menuIndex, (nsIWebShell*)aWebShell);
        } else if (menuitemNodeType.EqualsWithConversion("menuseparator")) {
          AddSeparator();
        } else if (menuitemNodeType.EqualsWithConversion("menu")) {
          // Load a submenu
          LoadSubMenu(this, menuitemElement, menuitemNode);
        }
      }
      ++menuIndex;
      nsCOMPtr<nsIDOMNode> oldmenuitemNode(menuitemNode);
      oldmenuitemNode->GetNextSibling(getter_AddRefs(menuitemNode));
    } // end menu item innner loop
  }
  
  gConstructingMenu = PR_FALSE;
  //printf("  Done building, mMenuItemVoidArray.Count() = %d \n", mMenuItemVoidArray.Count());
  
  gCurrentMenuDepth--;
              
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenu::HelpMenuConstruct(
    const nsMenuEvent & aMenuEvent,
    nsIWidget         * aParentWindow, 
    void              * menuNode,
	void              * aWebShell)
{
  //printf("nsMenu::MenuConstruct called for %s = %d \n", mLabel.ToNewCString(), mMacMenuHandle);
  // Begin menuitem inner loop

  int numHelpItems = ::CountMenuItems(mMacMenuHandle);
  for(int i=0; i<numHelpItems; ++i)
    mMenuItemVoidArray.AppendElement(nsnull);
     
  // Open the node.
  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(mDOMNode);
  if (domElement)
   domElement->SetAttribute(NS_ConvertASCIItoUCS2("open"), NS_ConvertASCIItoUCS2("true"));

  gCurrentMenuDepth++;
   
  // Now get the kids. Retrieve our menupopup child.
  nsCOMPtr<nsIDOMNode> menuPopupNode;
  GetMenuPopupElement ( getter_AddRefs(menuPopupNode) );
  if (!menuPopupNode)
    return nsEventStatus_eIgnore;
      
  // Now get the kids
  nsCOMPtr<nsIDOMNode> menuitemNode;
  menuPopupNode->GetFirstChild(getter_AddRefs(menuitemNode));

	unsigned short menuIndex = 0;

  // Fire our oncreate handler. If we're told to stop, don't build the menu at all
  PRBool keepProcessing = OnCreate();
  if ( keepProcessing ) {
    while (menuitemNode) {
      
      nsCOMPtr<nsIDOMElement> menuitemElement(do_QueryInterface(menuitemNode));
      if (menuitemElement) {
        nsString menuitemNodeType;
        nsString menuitemName;
        
        nsString label;
        menuitemElement->GetAttribute(NS_ConvertASCIItoUCS2("value"), label);
        //printf("label = %s \n", label.ToNewCString());
        
        menuitemElement->GetNodeName(menuitemNodeType);
        if (menuitemNodeType.EqualsWithConversion("menuitem")) {
          // LoadMenuItem
          LoadMenuItem(this, menuitemElement, menuitemNode, menuIndex, (nsIWebShell*)aWebShell);
        } else if (menuitemNodeType.EqualsWithConversion("menuseparator")) {
          AddSeparator();
        } else if (menuitemNodeType.EqualsWithConversion("menu")) {
          // Load a submenu
          LoadSubMenu(this, menuitemElement, menuitemNode);
        }
      }
  	++menuIndex;
      nsCOMPtr<nsIDOMNode> oldmenuitemNode(menuitemNode);
      oldmenuitemNode->GetNextSibling(getter_AddRefs(menuitemNode));
    } // end menu item innner loop
  }
  
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
  
  // Fire our ondestroy handler. If we're told to stop, don't destroy the menu
  PRBool keepProcessing = OnDestroy();
  if ( keepProcessing ) {
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
    
    nsCOMPtr<nsIDocument> document;
    contentNode->GetDocument(*getter_AddRefs(document));
        
    if(document)
      domElement->RemoveAttribute(NS_ConvertASCIItoUCS2("open"));
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
  if ( gCurrentMenuDepth < 2 ) {
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
    
    // add ourself as a document observer
     nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(mWebShell));

	  nsCOMPtr<nsIContentViewer> cv;
	  docShell->GetContentViewer(getter_AddRefs(cv));
	  if (cv) {
	   
	    nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
	    if (!docv)
	      return NS_OK;

	    nsCOMPtr<nsIDocument> doc;
	    docv->GetDocument(*getter_AddRefs(doc));
	    if (!doc)
	      return NS_OK;

        nsCOMPtr<nsIDocumentObserver> observer = do_QueryInterface(NS_STATIC_CAST(nsIMenu*, this));
        if(observer){
	      doc->AddObserver(observer);
	    }
	  }
	
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
	scriptRunText = new char[scriptRunTextSizeInBytes + 1];
	
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
#if DEBUG
	if (err != noErr)
		NS_WARNING("nsMenu::NSStringSetMenuItemText: SetMenuItemFontID failed.");
#endif

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
	scriptRunText = new char[scriptRunTextSizeInBytes + 1];	// +1 for the null terminator.
	
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
  nsString checked;
  nsString type;
  nsString menuitemName;
  nsString menuitemCmd;

  menuitemElement->GetAttribute(NS_ConvertASCIItoUCS2("disabled"), disabled);
  menuitemElement->GetAttribute(NS_ConvertASCIItoUCS2("checked"), checked);
  menuitemElement->GetAttribute(NS_ConvertASCIItoUCS2("type"), type);
  menuitemElement->GetAttribute(NS_ConvertASCIItoUCS2("value"), menuitemName);
  menuitemElement->GetAttribute(NS_ConvertASCIItoUCS2("cmd"), menuitemCmd);
  // Create nsMenuItem
  nsIMenuItem * pnsMenuItem = nsnull;
  nsresult rv = nsComponentManager::CreateInstance(kMenuItemCID, nsnull, NS_GET_IID(nsIMenuItem), (void**)&pnsMenuItem);
  if (NS_OK == rv) {
    pnsMenuItem->Create(pParentMenu, menuitemName, 0);   
    //printf("menuitem %s \n", menuitemName.ToNewCString());
          
    // Create MenuDelegate - this is the intermediator inbetween 
    // the DOM node and the nsIMenuItem
    // The nsWebShellWindow wacthes for Document changes and then notifies the 
    // the appropriate nsMenuDelegate object
    nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(menuitemNode));
    if (!domElement) {
#if DEBUG_saari
      SysBeep(30);
#endif
		  return;
    }
    
    nsAutoString cmdAtom; cmdAtom.AssignWithConversion("oncommand");
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
    nsAutoString keyAtom; keyAtom.AssignWithConversion("key");
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
	    nsAutoString shiftAtom; shiftAtom.AssignWithConversion("shift");
	    nsAutoString altAtom; altAtom.AssignWithConversion("alt");
	    nsAutoString commandAtom; commandAtom.AssignWithConversion("command");
	    nsString shiftValue;
	    nsString altValue;
	    nsString commandValue;
		nsString controlValue;
	    nsString keyChar; keyChar.AssignWithConversion(" ");
	    
	    keyElement->GetAttribute(keyAtom, keyChar);
	    keyElement->GetAttribute(shiftAtom, shiftValue);
	    keyElement->GetAttribute(altAtom, altValue);
	    keyElement->GetAttribute(commandAtom, commandValue);
	    
      nsAutoString xulkey;
      keyElement->GetAttribute(NS_ConvertASCIItoUCS2("xulkey"), xulkey);
      if (xulkey.EqualsWithConversion("true"))
        modifiers |= knsMenuItemCommandModifier;

		if(!keyChar.EqualsWithConversion(" ")) 
	      pnsMenuItem->SetShortcutChar(keyChar);
	      
		if(shiftValue.EqualsWithConversion("true")) 
		  modifiers |= knsMenuItemShiftModifier;
	    
	    if(altValue.EqualsWithConversion("true"))
	      modifiers |= knsMenuItemAltModifier;
	    
	    if(commandValue.EqualsWithConversion("true"))
	      modifiers |= knsMenuItemCommandModifier;

		if(controlValue.EqualsWithConversion("true"))
	      modifiers |= knsMenuItemControlModifier;
	      
        pnsMenuItem->SetModifiers(modifiers);
    }

	if(disabled.EqualsWithConversion(NS_STRING_TRUE))
      pnsMenuItem->SetEnabled(PR_FALSE);
    else
      pnsMenuItem->SetEnabled(PR_TRUE);

	if(checked.EqualsWithConversion(NS_STRING_TRUE))
      pnsMenuItem->SetChecked(PR_TRUE);
    else
      pnsMenuItem->SetChecked(PR_FALSE);
      
    if(type.EqualsWithConversion("checkbox"))
      pnsMenuItem->SetMenuItemType(nsIMenuItem::eCheckbox);
    else if ( type.EqualsWithConversion("radio") )
      pnsMenuItem->SetMenuItemType(nsIMenuItem::eRadio);
      
	nsISupports * supports = nsnull;
    pnsMenuItem->QueryInterface(NS_GET_IID(nsISupports), (void**) &supports);
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
  menuElement->GetAttribute(NS_ConvertASCIItoUCS2("value"), menuName);
  //printf("Creating Menu [%s] \n", menuName.ToNewCString()); // this leaks

  // Create nsMenu
  nsCOMPtr<nsIMenu> pnsMenu;
  nsresult rv = nsComponentManager::CreateInstance(kMenuCID, nsnull, NS_GET_IID(nsIMenu), getter_AddRefs(pnsMenu));
  if ( pnsMenu ) {
    // Call Create
    nsCOMPtr<nsISupports> supports ( do_QueryInterface(pParentMenu) );
    pnsMenu->Create(supports, menuName);
    
    // Set nsMenu Name
    pnsMenu->SetLabel(menuName); 

    // set if it's enabled or disabled
    nsAutoString disabled;
    menuElement->GetAttribute(NS_ConvertASCIItoUCS2("disabled"), disabled);
    if ( disabled.EqualsWithConversion("true") )
      pnsMenu->SetEnabled ( PR_FALSE );
    else
      pnsMenu->SetEnabled ( PR_TRUE );

    // Make nsMenu a child of parent nsMenu. The parent takes ownership
    nsCOMPtr<nsISupports> supports2 ( do_QueryInterface(pnsMenu) );
	pParentMenu->AddItem(supports2);

	pnsMenu->SetWebShell(mWebShell);
	pnsMenu->SetDOMNode(menuNode);
	pnsMenu->SetDOMElement(menuElement);

  }     
}///////////////////////////////////////////////////////////////
// nsIDocumentObserver
// this is needed for menubar changes
///////////////////////////////////////////////////////////////
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::BeginUpdate(
  nsIDocument * aDocument)
{
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::EndUpdate(
  nsIDocument * aDocument)
{
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::BeginLoad(
  nsIDocument * aDocument)
{
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::EndLoad(
  nsIDocument * aDocument)
{
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::BeginReflow(
  nsIDocument  * aDocument, 
  nsIPresShell * aShell)
{
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::EndReflow(
  nsIDocument  * aDocument, 
  nsIPresShell * aShell)
{
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::ContentChanged(
  nsIDocument * aDocument,
  nsIContent  * aContent,
  nsISupports * aSubContent)
{
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::ContentStatesChanged(
  nsIDocument * aDocument,
  nsIContent  * aContent1,
  nsIContent  * aContent2)
{
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::AttributeChanged(
  nsIDocument * aDocument,
  nsIContent  * aContent,
  PRInt32       aNameSpaceID,
  nsIAtom     * aAttribute,
  PRInt32       aHint)
{
  //printf("AttributeChanged\n");
  
  if(gConstructingMenu)
    return NS_OK;

  nsCOMPtr<nsIAtom> openAtom = NS_NewAtom("open");
  if(aAttribute != openAtom.get()) {
	    
    nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
    if(!element) {
      NS_ERROR("Unable to QI dom element.");
	  return NS_OK;  
	}
		  
    nsCOMPtr<nsIContent> contentNode;
    contentNode = do_QueryInterface(element);
    if (!contentNode) {
      NS_ERROR("DOM Node doesn't support the nsIContent interface required to handle DOM events.");
	  return NS_OK;
    }
		  
    if(aContent == contentNode.get()){
      nsCOMPtr<nsIAtom> disabledAtom = NS_NewAtom("disabled");
      nsCOMPtr<nsIAtom> valueAtom = NS_NewAtom("value");
      if(aAttribute == disabledAtom.get()) {
        nsCOMPtr<nsIDOMElement> element(do_QueryInterface(aContent));
        nsString valueString;
        element->GetAttribute(NS_ConvertASCIItoUCS2("disabled"), valueString);
        if(valueString.EqualsWithConversion("true"))
          SetEnabled(PR_FALSE);
        else
          SetEnabled(PR_TRUE);
          
        ::DrawMenuBar();
      } else if(aAttribute == valueAtom.get()) {
        nsCOMPtr<nsIDOMElement> element(do_QueryInterface(aContent));
        element->GetAttribute(NS_ConvertASCIItoUCS2("value"), mLabel);
        ::DeleteMenu(mMacMenuID);
        
        mMacMenuHandle = NSStringNewMenu(mMacMenuID, mLabel);

        // Replace standard MDEF with our stub MDEF
#if !TARGET_CARBON
        if(mMacMenuHandle) {    
          SInt8 state = ::HGetState((Handle)mMacMenuHandle);
          ::HLock((Handle)mMacMenuHandle);
          //gSystemMDEFHandle = (**mMacMenuHandle).menuProc;
          (**mMacMenuHandle).menuProc = gMDEF;
          ::HSetState((Handle)mMacMenuHandle, state);
        }
        ::InsertMenu(mMacMenuHandle, mMacMenuID+1);
        if(mMenuBarParent) {
          mMenuBarParent->SetNativeData(::GetMenuBar());
          ::DrawMenuBar();
        }
#endif
      }
      
    }
  }
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::ContentAppended(
  nsIDocument * aDocument,
  nsIContent  * aContainer,
  PRInt32       aNewIndexInContainer)
{
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::ContentInserted(
  nsIDocument * aDocument,
  nsIContent  * aContainer,
  nsIContent  * aChild,
  PRInt32       aIndexInContainer)
{
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::ContentReplaced(
  nsIDocument * aDocument,
  nsIContent  * aContainer,
  nsIContent  * aOldChild,
  nsIContent  * aNewChild,
  PRInt32       aIndexInContainer)
{
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::ContentRemoved(
  nsIDocument * aDocument,
  nsIContent  * aContainer,
  nsIContent  * aChild,
  PRInt32       aIndexInContainer)
{  
  if(gConstructingMenu)
    return NS_OK;
    
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  if(!element) {
    NS_ERROR("Unable to QI dom element.");
    return NS_OK;  
  }
		  
  nsCOMPtr<nsIContent> contentNode;
  contentNode = do_QueryInterface(element);
  if (!contentNode) {
    NS_ERROR("DOM Node doesn't support the nsIContent interface required to handle DOM events.");
    return NS_OK;
  }
  
  if(aChild == contentNode.get()) {
    if(mMenuParent) {
      mMenuParent->RemoveItem(aIndexInContainer);
    } else if(mMenuBarParent) {
      mMenuBarParent->RemoveMenu(aIndexInContainer);
    }
  }    
    
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::StyleSheetAdded(
  nsIDocument   * aDocument,
  nsIStyleSheet * aStyleSheet)
{
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::StyleSheetRemoved(
  nsIDocument   * aDocument,
  nsIStyleSheet * aStyleSheet)
{
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::StyleSheetDisabledStateChanged(
  nsIDocument   * aDocument,
  nsIStyleSheet * aStyleSheet,
  PRBool          aDisabled)
{
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::StyleRuleChanged(
  nsIDocument   * aDocument,
  nsIStyleSheet * aStyleSheet,
  nsIStyleRule  * aStyleRule,
  PRInt32         aHint)
{
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::StyleRuleAdded(
  nsIDocument   * aDocument,
  nsIStyleSheet * aStyleSheet,
  nsIStyleRule  * aStyleRule)
{
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::StyleRuleRemoved(
  nsIDocument   * aDocument,
  nsIStyleSheet * aStyleSheet,
  nsIStyleRule  * aStyleRule)
{
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMenu::DocumentWillBeDestroyed(
  nsIDocument * aDocument)
{
  return NS_OK;
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
  event.message = NS_MENU_CREATE;
  event.isShift = PR_FALSE;
  event.isControl = PR_FALSE;
  event.isAlt = PR_FALSE;
  event.isMeta = PR_FALSE;
  event.clickCount = 0;
  event.widget = nsnull;
  
  nsCOMPtr<nsIPresContext> presContext;
  MenuHelpers::WebShellToPresContext ( mWebShell, getter_AddRefs(presContext) );
  if ( presContext ) {
    nsresult rv;
    nsCOMPtr<nsIDOMNode> menuPopup;
    GetMenuPopupElement(getter_AddRefs(menuPopup));
    nsCOMPtr<nsIContent> popupContent ( do_QueryInterface(menuPopup) );
    if ( popupContent ) 
      rv = popupContent->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
    else {
      nsCOMPtr<nsIContent> me ( do_QueryInterface(mDOMNode) );
      if ( me )
        rv = me->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
    }
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
  event.message = NS_MENU_DESTROY;
  event.isShift = PR_FALSE;
  event.isControl = PR_FALSE;
  event.isAlt = PR_FALSE;
  event.isMeta = PR_FALSE;
  event.clickCount = 0;
  event.widget = nsnull;
  
  nsCOMPtr<nsIPresContext> presContext;
  MenuHelpers::WebShellToPresContext ( mWebShell, getter_AddRefs(presContext) );
  if ( presContext ) {    
    nsresult rv;
    nsCOMPtr<nsIDOMNode> menuPopup;
    GetMenuPopupElement(getter_AddRefs(menuPopup));
    nsCOMPtr<nsIContent> popupContent ( do_QueryInterface(menuPopup) );
    if ( popupContent ) 
      rv = popupContent->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
    else {
      nsCOMPtr<nsIContent> me ( do_QueryInterface(mDOMNode) );
      if ( me )
        rv = me->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
    }

    mDestroyHandlerCalled = PR_TRUE;
    
    if ( NS_FAILED(rv) || status == nsEventStatus_eConsumeNoDefault )
      return PR_FALSE;
  }
  return PR_TRUE;
}


//
// GetMenuPopupElement
//
// Find the |menupopup| child from the node representing this menu. It should be one
// of a very few children so we won't be iterating over a bazillion menu items to find
// it (so the strcmp won't kill us).
//
void
nsMenu::GetMenuPopupElement(nsIDOMNode** aResult)
{
  if ( !aResult )
    return;
    
  nsCOMPtr<nsIDOMNode> menuPopupNode;
  mDOMNode->GetFirstChild(getter_AddRefs(menuPopupNode));
  while (menuPopupNode) {
    nsCOMPtr<nsIDOMElement> menuPopupElement(do_QueryInterface(menuPopupNode));
    if (menuPopupElement) {
      nsString menuPopupNodeType;
      menuPopupElement->GetNodeName(menuPopupNodeType);
      if (menuPopupNodeType.EqualsWithConversion("menupopup")) {
        *aResult = menuPopupNode.get();
        NS_ADDREF(*aResult);        
        return;
      }
    }
    nsCOMPtr<nsIDOMNode> oldMenuPopupNode(menuPopupNode);
    oldMenuPopupNode->GetNextSibling(getter_AddRefs(menuPopupNode));
  }

} // GetMenuPopupElement


#pragma mark -


//
// WebShellToPresContext
//
// Helper to dig out a pres context from a webshell. A common thing to do before
// sending an event into the dom.
//
nsresult
MenuHelpers :: WebShellToPresContext ( nsIWebShell* inWebShell, nsIPresContext** outContext )
{
  if ( !inWebShell || !outContext )
    return NS_ERROR_INVALID_ARG ;
  
  nsresult retval = NS_OK;
  
  nsCOMPtr<nsIDocShell> docShell(do_QueryInterface(inWebShell));

  nsCOMPtr<nsIContentViewer> contentViewer;
  docShell->GetContentViewer(getter_AddRefs(contentViewer));
  if ( contentViewer ) {
    nsCOMPtr<nsIDocumentViewer> docViewer ( do_QueryInterface(contentViewer) );
    if ( docViewer )
      docViewer->GetPresContext(*outContext);     // AddRefs for us
    else
      retval = NS_ERROR_FAILURE;
  }
  else
    retval = NS_ERROR_FAILURE;
  
  return retval;
  
} // WebShellToPresContext
