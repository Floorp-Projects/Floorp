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
#include "nsIDocumentViewer.h"
#include "nsIContent.h"
#include "nsIPresContext.h"

#include "nsMenu.h"         // for MenuHelpers namespace
#include "nsMenuItem.h"
#include "nsIMenu.h"
#include "nsIMenuBar.h"
#include "nsIWidget.h"
#include "nsIMenuListener.h"

#include "nsStringUtil.h"

NS_IMPL_ISUPPORTS3(nsMenuItem, nsIMenuItem, nsIMenuListener, nsIChangeObserver)


//
// nsMenuItem constructor
//
nsMenuItem::nsMenuItem()
{
  NS_INIT_REFCNT();
  mMenuParent         = nsnull;
  mTarget             = nsnull;
  mIsSeparator        = PR_FALSE;
  mWebShell           = nsnull;
  mDOMNode            = nsnull;
  mKeyEquivalent.AssignWithConversion(" ");
  mEnabled            = PR_TRUE;
  mIsChecked          = PR_FALSE;
  mMenuType           = eRegular;
}

//
// nsMenuItem destructor
//
nsMenuItem::~nsMenuItem()
{
  //printf("nsMenuItem::~nsMenuItem() called \n");
  NS_IF_RELEASE(mTarget);
  
  // if we're a radio menu, we've been registered to get AttributeChanged, so
  // make sure we unregister when we go away.
  if ( mMenuType == eRadio ) {
    nsCOMPtr<nsIContent> content ( do_QueryInterface(mDOMNode) );
    mManager->Unregister ( content );
  }
}


NS_METHOD nsMenuItem::Create ( nsIMenu* aParent, const nsString & aLabel, PRBool aIsSeparator,
                                EMenuItemType aItemType, PRBool aEnabled, 
                                nsIChangeManager* aManager, nsIWebShell* aShell, nsIDOMNode* aNode )
{
  mDOMNode = aNode;
  mDOMElement = do_QueryInterface ( aNode );
  mMenuParent = aParent;
  mWebShell = aShell;
  
  mEnabled = aEnabled;
  mMenuType = aItemType;
  
  // if we're a radio menu, register for AttributeChanged messages
  mManager = aManager;
  if ( aItemType == eRadio ) {
    nsCOMPtr<nsIContent> content ( do_QueryInterface(mDOMNode) );
    nsCOMPtr<nsIChangeObserver> obs ( do_QueryInterface(NS_STATIC_CAST(nsIChangeObserver*,this)) );
    mManager->Register ( content, obs );
  }
  
  mIsSeparator = aIsSeparator;
  mLabel = aLabel;
  return NS_OK;
}

NS_METHOD
nsMenuItem::GetLabel(nsString &aText)
{
  aText = mLabel;
  return NS_OK;
}


NS_METHOD 
nsMenuItem::GetEnabled(PRBool *aIsEnabled)
{
  *aIsEnabled = mEnabled;
  return NS_OK;
}


NS_METHOD nsMenuItem::SetChecked(PRBool aIsEnabled)
{
  mIsChecked = aIsEnabled;
  if ( mMenuType == eRadio && aIsEnabled )
    UncheckRadioSiblings ( mDOMElement );

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetChecked(PRBool *aIsEnabled)
{
  *aIsEnabled = mIsChecked;
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetMenuItemType(EMenuItemType *aType)
{
  *aType = mMenuType;
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetTarget(nsIWidget *& aTarget)
{
  aTarget = mTarget;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetNativeData(void *& aData)
{
  //aData = (void *)mMenu;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::AddMenuListener(nsIMenuListener * aMenuListener)
{
	mXULCommandListener = aMenuListener;
	return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::IsSeparator(PRBool & aIsSep)
{
  aIsSep = mIsSeparator;
  return NS_OK;
}


//-------------------------------------------------------------------------
// nsIMenuListener interface
//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  switch ( mMenuType ) {
    case eCheckbox:
      SetChecked(!mIsChecked);
      break;

    case eRadio:
    {
      // we only want to muck with things if we were selected and we're not
      // already checked. 
      if ( mIsChecked )
        break;       
      SetChecked(PR_TRUE);
      UncheckRadioSiblings(mDOMElement);
      break;
    }
      
    case eRegular:
      break;          // do nothing special
      
  } // which menu type
      
  DoCommand();
  return nsEventStatus_eConsumeNoDefault;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuSelected(const nsMenuEvent & aMenuEvent)
{
	//if(mXULCommandListener)
	//	return mXULCommandListener->MenuSelected(aMenuEvent);
		
    DoCommand();
  	return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
// nsIMenuListener interface
//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  	return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuConstruct(
    const nsMenuEvent & aMenuEvent,
    nsIWidget         * aParentWindow, 
    void              * menuNode,
	void              * aWebShell)
{
  	return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  	return nsEventStatus_eIgnore;
}


//-------------------------------------------------------------------------
/**
* Executes the "cached" JavaScript Command 
* @return NS_OK if the command was executed properly, otherwise an error code
*/
NS_METHOD nsMenuItem::DoCommand()
{
  nsresult rv = NS_ERROR_FAILURE;
 
  nsCOMPtr<nsIPresContext> presContext;
  MenuHelpers::WebShellToPresContext ( mWebShell, getter_AddRefs(presContext) );

  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_MOUSE_EVENT;
  event.message = NS_MENU_ACTION;

  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  if(!element) {
      NS_ERROR("Unable to QI dom element.");
      return rv;  
  }
  
  nsCOMPtr<nsIContent> contentNode;
  contentNode = do_QueryInterface(element);
  if (!contentNode) {
      NS_ERROR("DOM Node doesn't support the nsIContent interface required to handle DOM events.");
      return rv;
  }

  rv = contentNode->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);

  return nsEventStatus_eConsumeNoDefault;

}
    
   
   //-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetModifiers(PRUint8 * aModifiers) 
{
  nsresult res = NS_OK;
  *aModifiers = mModifiers; 
  return res; 
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetModifiers(PRUint8 aModifiers)
{
    nsresult res = NS_OK;
    
    mModifiers = aModifiers;
    return res;
}
 
//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetShortcutChar(const nsString &aText)
{
    nsresult res = NS_OK;
    mKeyEquivalent = aText;
    return res;
} 

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetShortcutChar(nsString &aText)
{
    nsresult res = NS_OK;
    aText = mKeyEquivalent;
    return res;
} 

//
// UncheckRadioSiblings
//
// walk the sibling list looking for nodes with the same name and
// uncheck them all.
//
void
nsMenuItem :: UncheckRadioSiblings ( nsIDOMElement* inCheckedElement )
{
  nsCOMPtr<nsIDOMNode> checkedNode ( do_QueryInterface(inCheckedElement) );

  nsAutoString myGroupName;
  inCheckedElement->GetAttribute(NS_ConvertASCIItoUCS2("name"), myGroupName);
  
  nsCOMPtr<nsIDOMNode> parent;
  checkedNode->GetParentNode(getter_AddRefs(parent));
  if ( !parent )
    return;
  nsCOMPtr<nsIDOMNode> currSibling;
  parent->GetFirstChild(getter_AddRefs(currSibling));
  while ( currSibling ) {
    // skip this node
    if ( currSibling.get() != checkedNode ) {        
      nsCOMPtr<nsIDOMElement> currElement = do_QueryInterface(currSibling);
      if ( !currElement )
        break;
      
      // if the current sibling is in the same group, clear it
      nsAutoString currGroupName;
      currElement->GetAttribute(NS_ConvertASCIItoUCS2("name"), currGroupName);
      if ( currGroupName == myGroupName )
        currElement->SetAttribute(NS_ConvertASCIItoUCS2("checked"), NS_ConvertASCIItoUCS2("false"));
    }
    
    // advance to the next node
    nsIDOMNode* next;
    currSibling->GetNextSibling(&next);
    currSibling = dont_AddRef(next);
    
  } // for each sibling

} // UncheckRadioSiblings

#pragma mark -

//
// nsIChangeObserver
//


NS_IMETHODIMP
nsMenuItem :: AttributeChanged ( nsIDocument *aDocument, PRInt32 aNameSpaceID, nsIAtom *aAttribute,
                                    PRInt32 aHint)
{
  nsCOMPtr<nsIAtom> checkedAtom = NS_NewAtom("checked");
  nsAutoString checked;
  if ( aAttribute == checkedAtom.get() ) {
    mDOMElement->GetAttribute(NS_ConvertASCIItoUCS2("checked"), checked);
    if ( checked.EqualsWithConversion("true") )
      UncheckRadioSiblings ( mDOMElement );
  }
  return NS_OK;
    
} // AttributeChanged


NS_IMETHODIMP
nsMenuItem :: ContentRemoved(nsIDocument *aDocument, nsIContent *aChild, PRInt32 aIndexInContainer)
{
  return NS_OK;
  
} // ContentRemoved

