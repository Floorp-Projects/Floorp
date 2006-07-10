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
#include "nsMenuX.h"
#include "nsMenuItemX.h"

#include "nsWidgetAtoms.h"

#include "nsIMenu.h"
#include "nsIMenuBar.h"
#include "nsIWidget.h"
#include "nsIMenuListener.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMDocumentEvent.h"

#include "nsGUIEvent.h"


#if DEBUG
nsInstanceCounter   gMenuItemCounterX("nsMenuItemX");
#endif


NS_IMPL_ISUPPORTS4(nsMenuItemX, nsIMenuItem, nsIMenuListener, nsIChangeObserver, nsISupportsWeakReference)

//
// nsMenuItemX constructor
//
nsMenuItemX::nsMenuItemX()
{
  mMenuParent         = nsnull;
  mManager            = nsnull;
  mIsSeparator        = PR_FALSE;
  mKeyEquivalent.AssignLiteral(" ");
  mEnabled            = PR_TRUE;
  mIsChecked          = PR_FALSE;
  mMenuType           = eRegular;

#if DEBUG
  ++gMenuItemCounterX;
#endif 
}

//
// nsMenuItemX destructor
//
nsMenuItemX::~nsMenuItemX()
{
  if (mManager) {
    if (mContent)
      mManager->Unregister(mContent);
    if (mCommandContent)
      mManager->Unregister(mCommandContent);
  }

#if DEBUG
  --gMenuItemCounterX;
#endif 
}


NS_METHOD nsMenuItemX::Create(nsIMenu* aParent, const nsString & aLabel, PRBool aIsSeparator,
                              EMenuItemType aItemType, nsIChangeManager* aManager,
                              nsIDocShell* aShell, nsIContent* aNode)
{
  mContent = aNode;         // addref
  mMenuParent = aParent;    // weak
  mDocShellWeakRef = do_GetWeakReference(aShell);
  
  mMenuType = aItemType;
  
  // register for AttributeChanged messages
  mManager = aManager;
  nsCOMPtr<nsIChangeObserver> obs = do_QueryInterface(NS_STATIC_CAST(nsIChangeObserver*,this));
  mManager->Register(mContent, obs);   // does not addref this
  
  mEnabled = !mContent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::disabled, nsWidgetAtoms::_true, eCaseMatters);
  
  mIsSeparator = aIsSeparator;
  mLabel = aLabel;
  
  // We need to pick up a command content node, it is highly unlikely that one
  // won't exist. If we find one, register for changes on it.
  nsCOMPtr<nsIDOMDocument> domDocument = do_QueryInterface(aNode->GetDocument());
  if (domDocument) {
    nsAutoString ourCommand;
    aNode->GetAttr(kNameSpaceID_None, nsWidgetAtoms::command, ourCommand);
    if (!ourCommand.IsEmpty()) {
      nsCOMPtr<nsIDOMElement> commandElt;
      domDocument->GetElementById(ourCommand, getter_AddRefs(commandElt));
      if (commandElt) {
        mCommandContent = do_QueryInterface(commandElt);
        mManager->Register(mCommandContent, obs);
      }
    }
  }
  
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
  if (mIsChecked)
      mContent->SetAttr(kNameSpaceID_None, nsWidgetAtoms::checked,
                        NS_LITERAL_STRING("true"), PR_TRUE);
  else
      mContent->UnsetAttr(kNameSpaceID_None, nsWidgetAtoms::checked, PR_TRUE);

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
    void              * aDocShell)
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
    // flip "checked" state if we're a checkbox menu, or an un-checked radio menu
    if (mMenuType == nsIMenuItem::eCheckbox || (mMenuType == nsIMenuItem::eRadio && !mIsChecked)) {
        if (!mContent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::autocheck,
                                   nsWidgetAtoms::_false, eCaseMatters))
            SetChecked(!mIsChecked);
            /* the AttributeChanged code will update all the internal state */
    }

    return MenuHelpersX::DispatchCommandTo(mDocShellWeakRef, mContent);
}


NS_IMETHODIMP nsMenuItemX::DispatchDOMEvent(const nsString &eventName, PRBool *preventDefaultCalled)
{
  if (!mContent)
    return NS_ERROR_FAILURE;
  
  // get owner document for content
  nsCOMPtr<nsIDocument> parentDoc = mContent->GetOwnerDoc();
  if (!parentDoc) {
    NS_WARNING("Failed to get owner nsIDocument for menu item content");
    return NS_ERROR_FAILURE;
  }
  
  // get interface for creating DOM events from content owner document
  nsCOMPtr<nsIDOMDocumentEvent> DOMEventFactory = do_QueryInterface(parentDoc);
  if (!DOMEventFactory) {
    NS_WARNING("Failed to QI parent nsIDocument to nsIDOMDocumentEvent");
    return NS_ERROR_FAILURE;
  }
  
  // create DOM event
  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = DOMEventFactory->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create nsIDOMEvent");
    return rv;
  }
  event->InitEvent(eventName, PR_TRUE, PR_TRUE);
  
  // mark DOM event as trusted
  nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(event));
  privateEvent->SetTrusted(PR_TRUE);
  
  // send DOM event
  nsCOMPtr<nsIDOMEventTarget> eventTarget = do_QueryInterface(mContent);
  rv = eventTarget->DispatchEvent(event, preventDefaultCalled);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to send DOM event via nsIDOMEventTarget");
    return rv;
  }
  
  return NS_OK;
}

   
//-------------------------------------------------------------------------
NS_METHOD nsMenuItemX::GetModifiers(PRUint8 * aModifiers) 
{
    *aModifiers = mModifiers; 
    return NS_OK; 
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItemX::SetModifiers(PRUint8 aModifiers)
{
    mModifiers = aModifiers;
    return NS_OK;
}
 
//-------------------------------------------------------------------------
NS_METHOD nsMenuItemX::SetShortcutChar(const nsString &aText)
{
    mKeyEquivalent = aText;
    return NS_OK;
} 

//-------------------------------------------------------------------------
NS_METHOD nsMenuItemX::GetShortcutChar(nsString &aText)
{
    aText = mKeyEquivalent;
    return NS_OK;
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
        if (sibling->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::name,
                                 myGroupName, eCaseMatters))
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
nsMenuItemX::AttributeChanged(nsIDocument *aDocument, PRInt32 aNameSpaceID, nsIContent *aContent, nsIAtom *aAttribute)
{
  if (aContent == mContent) {
    if (aAttribute == nsWidgetAtoms::checked) {
      // if we're a radio menu, uncheck our sibling radio items. No need to
      // do any of this if we're just a normal check menu.
      if (mMenuType == eRadio) {
        if (mContent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::checked,
                                  nsWidgetAtoms::_true, eCaseMatters))
          UncheckRadioSiblings(mContent);
      }
      
      nsCOMPtr<nsIMenuListener> listener = do_QueryInterface(mMenuParent);
      listener->SetRebuild(PR_TRUE);
    } 
    else if (aAttribute == nsWidgetAtoms::disabled || aAttribute == nsWidgetAtoms::hidden ||
             aAttribute == nsWidgetAtoms::collapsed || aAttribute == nsWidgetAtoms::label )  {
      nsCOMPtr<nsIMenuListener> listener = do_QueryInterface(mMenuParent);
      listener->SetRebuild(PR_TRUE);
    }    
  }
  else if (aContent == mCommandContent &&
    aAttribute == nsWidgetAtoms::disabled &&
    mMenuParent && mCommandContent) {
    nsAutoString menuItemDisabled;
    nsAutoString commandDisabled;
    mContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, menuItemDisabled);
    mCommandContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, commandDisabled);
    if (!commandDisabled.Equals(menuItemDisabled)) {
      // The menu's disabled state needs to be updated to match the command.
      if (commandDisabled.IsEmpty())
        mContent->UnsetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, PR_TRUE);
      else
        mContent->SetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, commandDisabled, PR_TRUE);
    }
    // we need to get our native menu item to update itself
    mMenuParent->ChangeNativeEnabledStatusForMenuItem(this, !commandDisabled.EqualsLiteral("true"));
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsMenuItemX :: ContentRemoved(nsIDocument *aDocument, nsIContent *aChild, PRInt32 aIndexInContainer)
{
  if (aChild == mCommandContent) {
    mManager->Unregister(mCommandContent);
    mCommandContent = nsnull;
  }
  
  nsCOMPtr<nsIMenuListener> listener = do_QueryInterface(mMenuParent);
  listener->SetRebuild(PR_TRUE);
  return NS_OK;
}


NS_IMETHODIMP
nsMenuItemX :: ContentInserted(nsIDocument *aDocument, nsIContent *aChild, PRInt32 aIndexInContainer)
{
  
  nsCOMPtr<nsIMenuListener> listener = do_QueryInterface(mMenuParent);
  listener->SetRebuild(PR_TRUE);
  return NS_OK;
  
} // ContentInserted
