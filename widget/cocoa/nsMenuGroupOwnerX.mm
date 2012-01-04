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
 *   Thomas K. Dyas <tom.dyas@gmail.com>
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

#include "nsMenuGroupOwnerX.h"
#include "nsMenuBarX.h"
#include "nsMenuX.h"
#include "nsMenuItemX.h"
#include "nsMenuUtilsX.h"
#include "nsCocoaUtils.h"
#include "nsCocoaWindow.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsGUIEvent.h"
#include "nsObjCExceptions.h"
#include "nsHashtable.h"
#include "nsThreadUtils.h"

#include "mozilla/dom/Element.h"
#include "nsIWidget.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"

#include "nsINode.h"

namespace dom = mozilla::dom;

NS_IMPL_ISUPPORTS1(nsMenuGroupOwnerX, nsIMutationObserver)


nsMenuGroupOwnerX::nsMenuGroupOwnerX()
: mCurrentCommandID(eCommand_ID_Last),
  mDocument(nsnull)
{
  mContentToObserverTable.Init();
  mCommandToMenuObjectTable.Init();
}


nsMenuGroupOwnerX::~nsMenuGroupOwnerX()
{
  // make sure we unregister ourselves as a document observer
  if (mDocument)
    mDocument->RemoveMutationObserver(this);
}


nsresult nsMenuGroupOwnerX::Create(nsIContent* aContent)
{
  if (!aContent)
    return NS_ERROR_INVALID_ARG;

  mContent = aContent;

  nsIDocument* doc = aContent->OwnerDoc();
  if (!doc)
    return NS_ERROR_FAILURE;
  doc->AddMutationObserver(this);
  mDocument = doc;

  return NS_OK;
}


//
// nsIMutationObserver
//


void nsMenuGroupOwnerX::CharacterDataWillChange(nsIDocument* aDocument,
                                                nsIContent* aContent,
                                                CharacterDataChangeInfo* aInfo)
{
}


void nsMenuGroupOwnerX::CharacterDataChanged(nsIDocument* aDocument,
                                             nsIContent* aContent,
                                             CharacterDataChangeInfo* aInfo)
{
}


void nsMenuGroupOwnerX::ContentAppended(nsIDocument* aDocument,
                                        nsIContent* aContainer,
                                        nsIContent* aFirstNewContent,
                                        PRInt32 /* unused */)
{
  for (nsIContent* cur = aFirstNewContent; cur; cur = cur->GetNextSibling()) {
    ContentInserted(aDocument, aContainer, cur, 0);
  }
}


void nsMenuGroupOwnerX::NodeWillBeDestroyed(const nsINode * aNode)
{
  // our menu bar node is being destroyed
  mDocument = nsnull;
}


void nsMenuGroupOwnerX::AttributeWillChange(nsIDocument* aDocument,
                                            dom::Element* aContent,
                                            PRInt32 aNameSpaceID,
                                            nsIAtom* aAttribute,
                                            PRInt32 aModType)
{
}


void nsMenuGroupOwnerX::AttributeChanged(nsIDocument* aDocument,
                                         dom::Element* aElement,
                                         PRInt32 aNameSpaceID,
                                         nsIAtom* aAttribute,
                                         PRInt32 aModType)
{
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  nsChangeObserver* obs = LookupContentChangeObserver(aElement);
  if (obs)
    obs->ObserveAttributeChanged(aDocument, aElement, aAttribute);
}


void nsMenuGroupOwnerX::ContentRemoved(nsIDocument * aDocument,
                                       nsIContent * aContainer,
                                       nsIContent * aChild,
                                       PRInt32 aIndexInContainer,
                                       nsIContent * aPreviousSibling)
{
  if (!aContainer) {
    return;
  }

  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  nsChangeObserver* obs = LookupContentChangeObserver(aContainer);
  if (obs)
    obs->ObserveContentRemoved(aDocument, aChild, aIndexInContainer);
  else if (aContainer != mContent) {
    // We do a lookup on the parent container in case things were removed
    // under a "menupopup" item. That is basically a wrapper for the contents
    // of a "menu" node.
    nsCOMPtr<nsIContent> parent = aContainer->GetParent();
    if (parent) {
      obs = LookupContentChangeObserver(parent);
      if (obs)
        obs->ObserveContentRemoved(aDocument, aChild, aIndexInContainer);
    }
  }
}


void nsMenuGroupOwnerX::ContentInserted(nsIDocument * aDocument,
                                        nsIContent * aContainer,
                                        nsIContent * aChild,
                                        PRInt32 /* unused */)
{
  if (!aContainer) {
    return;
  }

  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  nsChangeObserver* obs = LookupContentChangeObserver(aContainer);
  if (obs)
    obs->ObserveContentInserted(aDocument, aContainer, aChild);
  else if (aContainer != mContent) {
    // We do a lookup on the parent container in case things were removed
    // under a "menupopup" item. That is basically a wrapper for the contents
    // of a "menu" node.
    nsCOMPtr<nsIContent> parent = aContainer->GetParent();
    if (parent) {
      obs = LookupContentChangeObserver(parent);
      if (obs)
        obs->ObserveContentInserted(aDocument, aContainer, aChild);
    }
  }
}


void nsMenuGroupOwnerX::ParentChainChanged(nsIContent *aContent)
{
}


// For change management, we don't use a |nsSupportsHashtable| because
// we know that the lifetime of all these items is bounded by the
// lifetime of the menubar. No need to add any more strong refs to the
// picture because the containment hierarchy already uses strong refs.
void nsMenuGroupOwnerX::RegisterForContentChanges(nsIContent *aContent,
                                                  nsChangeObserver *aMenuObject)
{
  mContentToObserverTable.Put(aContent, aMenuObject);
}


void nsMenuGroupOwnerX::UnregisterForContentChanges(nsIContent *aContent)
{
  mContentToObserverTable.Remove(aContent);
}


nsChangeObserver* nsMenuGroupOwnerX::LookupContentChangeObserver(nsIContent* aContent)
{
  nsChangeObserver * result;
  if (mContentToObserverTable.Get(aContent, &result))
    return result;
  else
    return nsnull;
}


// Given a menu item, creates a unique 4-character command ID and
// maps it to the item. Returns the id for use by the client.
PRUint32 nsMenuGroupOwnerX::RegisterForCommand(nsMenuItemX* inMenuItem)
{
  // no real need to check for uniqueness. We always start afresh with each
  // window at 1. Even if we did get close to the reserved Apple command id's,
  // those don't start until at least '    ', which is integer 538976288. If
  // we have that many menu items in one window, I think we have other
  // problems.

  // make id unique
  ++mCurrentCommandID;

  mCommandToMenuObjectTable.Put(mCurrentCommandID, inMenuItem);

  return mCurrentCommandID;
}


// Removes the mapping between the given 4-character command ID
// and its associated menu item.
void nsMenuGroupOwnerX::UnregisterCommand(PRUint32 inCommandID)
{
  mCommandToMenuObjectTable.Remove(inCommandID);
}


nsMenuItemX* nsMenuGroupOwnerX::GetMenuItemForCommandID(PRUint32 inCommandID)
{
  nsMenuItemX * result;
  if (mCommandToMenuObjectTable.Get(inCommandID, &result))
    return result;
  else
    return nsnull;
}
