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
#include "nsIDocumentViewer.h"
#include "nsIContent.h"
#include "nsPresContext.h"

#include "nsMenuBarX.h"         // for MenuHelpers namespace
#include "nsMenuItemX.h"
#include "nsIMenu.h"
#include "nsIMenuBar.h"
#include "nsIWidget.h"
#include "nsIMenuListener.h"
#include "nsINameSpaceManager.h"
#include "nsWidgetAtoms.h"
#include "nsIServiceManager.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"

#include "nsGUIEvent.h"


#if 0
nsInstanceCounter   gMenuItemCounterX("nsMenuItemX");
#endif


NS_IMPL_ISUPPORTS4(nsMenuItemX, nsIMenuItem, nsIMenuListener, nsIChangeObserver, nsISupportsWeakReference)

//
// nsMenuItemX constructor
//
nsMenuItemX::nsMenuItemX()
{
  mMenuParent         = nsnull;
  mIsSeparator        = PR_FALSE;
  mKeyEquivalent.AssignLiteral(" ");
  mEnabled            = PR_TRUE;
  mIsChecked          = PR_FALSE;
  mMenuType           = eRegular;

#if 0
  ++gMenuItemCounterX;
#endif 
}

//
// nsMenuItemX destructor
//
nsMenuItemX::~nsMenuItemX()
{
  mManager->Unregister(mContent);

#if 0
  --gMenuItemCounterX;
#endif 
}


NS_METHOD nsMenuItemX::Create ( nsIMenu* aParent, const nsString & aLabel, PRBool aIsSeparator,
                                EMenuItemType aItemType, PRBool aEnabled, 
                                nsIChangeManager* aManager, nsIWebShell* aShell, nsIContent* aNode )
{
  mContent = aNode;         // addref
  mMenuParent = aParent;    // weak
  mWebShellWeakRef = do_GetWeakReference(aShell);
  
  mEnabled = aEnabled;
  mMenuType = aItemType;
  
  // register for AttributeChanged messages
  mManager = aManager;
  nsCOMPtr<nsIChangeObserver> obs = do_QueryInterface(NS_STATIC_CAST(nsIChangeObserver*,this));
  mManager->Register(mContent, obs);   // does not addref this
  
  mIsSeparator = aIsSeparator;
  mLabel = aLabel;
  return NS_OK;
}

NS_METHOD
nsMenuItemX::GetLabel(nsString &aText)
{
  aText = mLabel;
  return NS_OK;
}


NS_METHOD 
nsMenuItemX::GetEnabled(PRBool *aIsEnabled)
{
  *aIsEnabled = mEnabled;
  return NS_OK;
}


NS_METHOD nsMenuItemX::SetChecked(PRBool aIsEnabled)
{
  mIsChecked = aIsEnabled;
  
  // update the content model. This will also handle unchecking our siblings
  // if we are a radiomenu
  mContent->SetAttr(kNameSpaceID_None, nsWidgetAtoms::checked, 
                    mIsChecked ? NS_LITERAL_STRING("true") : NS_LITERAL_STRING("false"), PR_TRUE);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItemX::GetChecked(PRBool *aIsEnabled)
{
  *aIsEnabled = mIsChecked;
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsMenuItemX::GetMenuItemType(EMenuItemType *aType)
{
  *aType = mMenuType;
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsMenuItemX::GetTarget(nsIWidget *& aTarget)
{
  NS_IF_ADDREF(aTarget = mTarget);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItemX::GetNativeData(void *& aData)
{
  //aData = (void *)mMenu;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItemX::AddMenuListener(nsIMenuListener * aMenuListener)
{
  mXULCommandListener = aMenuListener;    // addref
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItemX::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
  if (mXULCommandListener.get() == aMenuListener)
    mXULCommandListener = nsnull;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItemX::IsSeparator(PRBool & aIsSep)
{
  aIsSep = mIsSeparator;
  return NS_OK;
}


//-------------------------------------------------------------------------
// nsIMenuListener interface
//-------------------------------------------------------------------------
nsEventStatus nsMenuItemX::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  // this is all handled by Carbon Events
  return nsEventStatus_eConsumeNoDefault;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuItemX::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
// nsIMenuListener interface
//-------------------------------------------------------------------------
nsEventStatus nsMenuItemX::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
    return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuItemX::MenuConstruct(
    const nsMenuEvent & aMenuEvent,
    nsIWidget         * aParentWindow, 
    void              * menuNode,
    void              * aWebShell)
{
    return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuItemX::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
    return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuItemX::CheckRebuild(PRBool & aNeedsRebuild)
{
  aNeedsRebuild = PR_TRUE; 
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuItemX::SetRebuild(PRBool aNeedsRebuild)
{
  //mNeedsRebuild = aNeedsRebuild; 
  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
/**
* Executes the "cached" JavaScript Command 
* @return NS_OK if the command was executed properly, otherwise an error code
*/
NS_METHOD nsMenuItemX::DoCommand()
{
  nsCOMPtr<nsPresContext> presContext;
  nsCOMPtr<nsIWebShell> webShell = do_QueryReferent(mWebShellWeakRef);
  if (!webShell)
    return nsEventStatus_eConsumeNoDefault;
  MenuHelpersX::WebShellToPresContext(webShell, getter_AddRefs(presContext));

  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event(NS_XUL_COMMAND);

  // See if we have a command element.  If so, we execute on the command instead
  // of on our content element.
  nsAutoString command;
  mContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::command, command);
  if (!command.IsEmpty()) {
    nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(mContent->GetDocument()));
    nsCOMPtr<nsIDOMElement> commandElt;
    domDoc->GetElementById(command, getter_AddRefs(commandElt));
    nsCOMPtr<nsIContent> commandContent(do_QueryInterface(commandElt));
    if (commandContent)
      commandContent->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
  }
  else
    mContent->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
  
  return nsEventStatus_eConsumeNoDefault;
}
    
   
   //-------------------------------------------------------------------------
NS_METHOD nsMenuItemX::GetModifiers(PRUint8 * aModifiers) 
{
    nsresult res = NS_OK;
    *aModifiers = mModifiers; 
    return res; 
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItemX::SetModifiers(PRUint8 aModifiers)
{
    nsresult res = NS_OK;
    
    mModifiers = aModifiers;
    return res;
}
 
//-------------------------------------------------------------------------
NS_METHOD nsMenuItemX::SetShortcutChar(const nsString &aText)
{
    nsresult res = NS_OK;
    mKeyEquivalent = aText;
    return res;
} 

//-------------------------------------------------------------------------
NS_METHOD nsMenuItemX::GetShortcutChar(nsString &aText)
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
nsMenuItemX :: UncheckRadioSiblings(nsIContent* inCheckedContent)
{
  nsAutoString myGroupName;
  inCheckedContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::name, myGroupName);
  if ( ! myGroupName.Length() )        // no groupname, nothing to do
    return;
  
  nsCOMPtr<nsIContent> parent = inCheckedContent->GetParent();
  if ( !parent )
    return;

  // loop over siblings
  PRUint32 count = parent->GetChildCount();
  for ( PRUint32 i = 0; i < count; ++i ) {
    nsIContent *sibling = parent->GetChildAt(i);
    if ( sibling ) {      
      if ( sibling != inCheckedContent ) {                    // skip this node
        // if the current sibling is in the same group, clear it
        nsAutoString currGroupName;
        sibling->GetAttr(kNameSpaceID_None, nsWidgetAtoms::name, currGroupName);
        if ( currGroupName == myGroupName )
          sibling->SetAttr(kNameSpaceID_None, nsWidgetAtoms::checked, NS_LITERAL_STRING("false"), PR_TRUE);
      }
    }    
  } // for each sibling

} // UncheckRadioSiblings

#pragma mark -

//
// nsIChangeObserver
//


NS_IMETHODIMP
nsMenuItemX :: AttributeChanged ( nsIDocument *aDocument, PRInt32 aNameSpaceID, nsIAtom *aAttribute )
{
  if (aAttribute == nsWidgetAtoms::checked) {
    // if we're a radio menu, uncheck our sibling radio items. No need to
    // do any of this if we're just a normal check menu.
    if ( mMenuType == eRadio ) {
      nsAutoString checked;
      mContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::checked, checked);
      if (checked.EqualsLiteral("true") ) 
        UncheckRadioSiblings(mContent);
    }
    
    nsCOMPtr<nsIMenuListener> listener = do_QueryInterface(mMenuParent);
    listener->SetRebuild(PR_TRUE);
    
  } 
  else if (aAttribute == nsWidgetAtoms::disabled || aAttribute == nsWidgetAtoms::hidden ||
             aAttribute == nsWidgetAtoms::collapsed )  {
    nsCOMPtr<nsIMenuListener> listener = do_QueryInterface(mMenuParent);
    listener->SetRebuild(PR_TRUE);
  }
  
  return NS_OK;

} // AttributeChanged


NS_IMETHODIMP
nsMenuItemX :: ContentRemoved(nsIDocument *aDocument, nsIContent *aChild, PRInt32 aIndexInContainer)
{
  
  nsCOMPtr<nsIMenuListener> listener = do_QueryInterface(mMenuParent);
  listener->SetRebuild(PR_TRUE);
  return NS_OK;
  
} // ContentRemoved

NS_IMETHODIMP
nsMenuItemX :: ContentInserted(nsIDocument *aDocument, nsIContent *aChild, PRInt32 aIndexInContainer)
{
  
  nsCOMPtr<nsIMenuListener> listener = do_QueryInterface(mMenuParent);
  listener->SetRebuild(PR_TRUE);
  return NS_OK;
  
} // ContentInserted
