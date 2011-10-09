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

#include "nsMenuItemX.h"
#include "nsMenuBarX.h"
#include "nsMenuX.h"
#include "nsMenuItemIconX.h"
#include "nsMenuUtilsX.h"

#include "nsObjCExceptions.h"

#include "nsCOMPtr.h"
#include "nsWidgetAtoms.h"
#include "nsGUIEvent.h"

#include "mozilla/dom/Element.h"
#include "nsIWidget.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMElement.h"

nsMenuItemX::nsMenuItemX()
{
  mType           = eRegularMenuItemType;
  mNativeMenuItem = nil;
  mMenuParent     = nsnull;
  mMenuGroupOwner = nsnull;
  mIsChecked      = false;

  MOZ_COUNT_CTOR(nsMenuItemX);
}

nsMenuItemX::~nsMenuItemX()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Prevent the icon object from outliving us.
  if (mIcon)
    mIcon->Destroy();

  // autorelease the native menu item so that anything else happening to this
  // object happens before the native menu item actually dies
  [mNativeMenuItem autorelease];

  if (mContent)
    mMenuGroupOwner->UnregisterForContentChanges(mContent);
  if (mCommandContent)
    mMenuGroupOwner->UnregisterForContentChanges(mCommandContent);

  MOZ_COUNT_DTOR(nsMenuItemX);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsresult nsMenuItemX::Create(nsMenuX* aParent, const nsString& aLabel, EMenuItemType aItemType,
                             nsMenuGroupOwnerX* aMenuGroupOwner, nsIContent* aNode)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  mType = aItemType;
  mMenuParent = aParent;
  mContent = aNode;

  mMenuGroupOwner = aMenuGroupOwner;
  NS_ASSERTION(mMenuGroupOwner, "No menu owner given, must have one!");

  mMenuGroupOwner->RegisterForContentChanges(mContent, this);

  nsIDocument *doc = mContent->GetCurrentDoc();

  // if we have a command associated with this menu item, register for changes
  // to the command DOM node
  if (doc) {
    nsAutoString ourCommand;
    mContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::command, ourCommand);

    if (!ourCommand.IsEmpty()) {
      nsIContent *commandElement = doc->GetElementById(ourCommand);

      if (commandElement) {
        mCommandContent = commandElement;
        // register to observe the command DOM element
        mMenuGroupOwner->RegisterForContentChanges(mCommandContent, this);
      }
    }
  }

  // decide enabled state based on command content if it exists, otherwise do it based
  // on our own content
  bool isEnabled;
  if (mCommandContent)
    isEnabled = !mCommandContent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::disabled, nsWidgetAtoms::_true, eCaseMatters);
  else
    isEnabled = !mContent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::disabled, nsWidgetAtoms::_true, eCaseMatters);

  // set up the native menu item
  if (mType == eSeparatorMenuItemType) {
    mNativeMenuItem = [[NSMenuItem separatorItem] retain];
  }
  else {
    NSString *newCocoaLabelString = nsMenuUtilsX::GetTruncatedCocoaLabel(aLabel);
    mNativeMenuItem = [[NSMenuItem alloc] initWithTitle:newCocoaLabelString action:nil keyEquivalent:@""];

    [mNativeMenuItem setEnabled:(BOOL)isEnabled];

    SetChecked(mContent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::checked,
                                     nsWidgetAtoms::_true, eCaseMatters));
    SetKeyEquiv();
  }

  mIcon = new nsMenuItemIconX(this, mContent, mNativeMenuItem);
  if (!mIcon)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult nsMenuItemX::SetChecked(bool aIsChecked)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  mIsChecked = aIsChecked;
  
  // update the content model. This will also handle unchecking our siblings
  // if we are a radiomenu
  mContent->SetAttr(kNameSpaceID_None, nsWidgetAtoms::checked, 
                    mIsChecked ? NS_LITERAL_STRING("true") : NS_LITERAL_STRING("false"), true);
  
  // update native menu item
  if (mIsChecked)
    [mNativeMenuItem setState:NSOnState];
  else
    [mNativeMenuItem setState:NSOffState];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

EMenuItemType nsMenuItemX::GetMenuItemType()
{
  return mType;
}

// Executes the "cached" javaScript command.
// Returns NS_OK if the command was executed properly, otherwise an error code.
void nsMenuItemX::DoCommand()
{
  // flip "checked" state if we're a checkbox menu, or an un-checked radio menu
  if (mType == eCheckboxMenuItemType ||
      (mType == eRadioMenuItemType && !mIsChecked)) {
    if (!mContent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::autocheck,
                               nsWidgetAtoms::_false, eCaseMatters))
    SetChecked(!mIsChecked);
    /* the AttributeChanged code will update all the internal state */
  }

  nsMenuUtilsX::DispatchCommandTo(mContent);
}

nsresult nsMenuItemX::DispatchDOMEvent(const nsString &eventName, bool *preventDefaultCalled)
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
  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(parentDoc);
  if (!domDoc) {
    NS_WARNING("Failed to QI parent nsIDocument to nsIDOMDocument");
    return NS_ERROR_FAILURE;
  }

  // create DOM event
  nsCOMPtr<nsIDOMEvent> event;
  nsresult rv = domDoc->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create nsIDOMEvent");
    return rv;
  }
  event->InitEvent(eventName, true, true);

  // mark DOM event as trusted
  nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(event));
  privateEvent->SetTrusted(true);

  // send DOM event
  nsCOMPtr<nsIDOMEventTarget> eventTarget = do_QueryInterface(mContent);
  rv = eventTarget->DispatchEvent(event, preventDefaultCalled);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to send DOM event via nsIDOMEventTarget");
    return rv;
  }

  return NS_OK;  
}

// Walk the sibling list looking for nodes with the same name and
// uncheck them all.
void nsMenuItemX::UncheckRadioSiblings(nsIContent* inCheckedContent)
{
  nsAutoString myGroupName;
  inCheckedContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::name, myGroupName);
  if (!myGroupName.Length()) // no groupname, nothing to do
    return;
  
  nsCOMPtr<nsIContent> parent = inCheckedContent->GetParent();
  if (!parent)
    return;

  // loop over siblings
  PRUint32 count = parent->GetChildCount();
  for (PRUint32 i = 0; i < count; i++) {
    nsIContent *sibling = parent->GetChildAt(i);
    if (sibling) {      
      if (sibling != inCheckedContent) { // skip this node
        // if the current sibling is in the same group, clear it
        if (sibling->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::name,
                                 myGroupName, eCaseMatters))
          sibling->SetAttr(kNameSpaceID_None, nsWidgetAtoms::checked, NS_LITERAL_STRING("false"), true);
      }
    }    
  }
}

void nsMenuItemX::SetKeyEquiv()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Set key shortcut and modifiers
  nsAutoString keyValue;
  mContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::key, keyValue);
  if (!keyValue.IsEmpty() && mContent->GetCurrentDoc()) {
    nsIContent *keyContent = mContent->GetCurrentDoc()->GetElementById(keyValue);
    if (keyContent) {
      nsAutoString keyChar(NS_LITERAL_STRING(" "));
      keyContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::key, keyChar);

      nsAutoString modifiersStr;
      keyContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::modifiers, modifiersStr);
      PRUint8 modifiers = nsMenuUtilsX::GeckoModifiersForNodeAttribute(modifiersStr);

      unsigned int macModifiers = nsMenuUtilsX::MacModifiersForGeckoModifiers(modifiers);
      [mNativeMenuItem setKeyEquivalentModifierMask:macModifiers];

      NSString *keyEquivalent = [[NSString stringWithCharacters:(unichar*)keyChar.get()
                                                         length:keyChar.Length()] lowercaseString];
      if ([keyEquivalent isEqualToString:@" "])
        [mNativeMenuItem setKeyEquivalent:@""];
      else
        [mNativeMenuItem setKeyEquivalent:keyEquivalent];

      return;
    }
  }

  // if the key was removed, clear the key
  [mNativeMenuItem setKeyEquivalent:@""];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

//
// nsChangeObserver
//

void
nsMenuItemX::ObserveAttributeChanged(nsIDocument *aDocument, nsIContent *aContent, nsIAtom *aAttribute)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!aContent)
    return;
  
  if (aContent == mContent) { // our own content node changed
    if (aAttribute == nsWidgetAtoms::checked) {
      // if we're a radio menu, uncheck our sibling radio items. No need to
      // do any of this if we're just a normal check menu.
      if (mType == eRadioMenuItemType) {
        if (mContent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::checked,
                                  nsWidgetAtoms::_true, eCaseMatters))
          UncheckRadioSiblings(mContent);
      }
      mMenuParent->SetRebuild(true);
    }
    else if (aAttribute == nsWidgetAtoms::hidden ||
             aAttribute == nsWidgetAtoms::collapsed ||
             aAttribute == nsWidgetAtoms::label) {
      mMenuParent->SetRebuild(true);
    }
    else if (aAttribute == nsWidgetAtoms::key) {
      SetKeyEquiv();
    }
    else if (aAttribute == nsWidgetAtoms::image) {
      SetupIcon();
    }
    else if (aAttribute == nsWidgetAtoms::disabled) {
      if (aContent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::disabled, nsWidgetAtoms::_true, eCaseMatters))
        [mNativeMenuItem setEnabled:NO];
      else
        [mNativeMenuItem setEnabled:YES];
    }
  }
  else if (aContent == mCommandContent) {
    // the only thing that really matters when the menu isn't showing is the
    // enabled state since it enables/disables keyboard commands
    if (aAttribute == nsWidgetAtoms::disabled) {
      // first we sync our menu item DOM node with the command DOM node
      nsAutoString commandDisabled;
      nsAutoString menuDisabled;
      aContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, commandDisabled);
      mContent->GetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, menuDisabled);
      if (!commandDisabled.Equals(menuDisabled)) {
        // The menu's disabled state needs to be updated to match the command.
        if (commandDisabled.IsEmpty()) 
          mContent->UnsetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, true);
        else
          mContent->SetAttr(kNameSpaceID_None, nsWidgetAtoms::disabled, commandDisabled, true);
      }
      // now we sync our native menu item with the command DOM node
      if (aContent->AttrValueIs(kNameSpaceID_None, nsWidgetAtoms::disabled, nsWidgetAtoms::_true, eCaseMatters))
        [mNativeMenuItem setEnabled:NO];
      else
        [mNativeMenuItem setEnabled:YES];
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsMenuItemX::ObserveContentRemoved(nsIDocument *aDocument, nsIContent *aChild, PRInt32 aIndexInContainer)
{
  if (aChild == mCommandContent) {
    mMenuGroupOwner->UnregisterForContentChanges(mCommandContent);
    mCommandContent = nsnull;
  }

  mMenuParent->SetRebuild(true);
}

void nsMenuItemX::ObserveContentInserted(nsIDocument *aDocument, nsIContent* aContainer,
                                         nsIContent *aChild)
{
  mMenuParent->SetRebuild(true);
}

void nsMenuItemX::SetupIcon()
{
  if (mIcon)
    mIcon->SetupIcon();
}
