/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMenuGroupOwnerX.h"
#include "nsMenuBarX.h"
#include "nsMenuX.h"
#include "nsMenuItemX.h"
#include "nsMenuUtilsX.h"
#include "nsCocoaUtils.h"
#include "nsCocoaWindow.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsObjCExceptions.h"
#include "nsHashtable.h"
#include "nsThreadUtils.h"

#include "mozilla/dom/Element.h"
#include "nsIWidget.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"

#include "nsINode.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS1(nsMenuGroupOwnerX, nsIMutationObserver)


nsMenuGroupOwnerX::nsMenuGroupOwnerX()
: mCurrentCommandID(eCommand_ID_Last),
  mDocument(nullptr)
{
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
                                        int32_t /* unused */)
{
  for (nsIContent* cur = aFirstNewContent; cur; cur = cur->GetNextSibling()) {
    ContentInserted(aDocument, aContainer, cur, 0);
  }
}


void nsMenuGroupOwnerX::NodeWillBeDestroyed(const nsINode * aNode)
{
  // our menu bar node is being destroyed
  mDocument = nullptr;
}


void nsMenuGroupOwnerX::AttributeWillChange(nsIDocument* aDocument,
                                            dom::Element* aContent,
                                            int32_t aNameSpaceID,
                                            nsIAtom* aAttribute,
                                            int32_t aModType)
{
}


void nsMenuGroupOwnerX::AttributeChanged(nsIDocument* aDocument,
                                         dom::Element* aElement,
                                         int32_t aNameSpaceID,
                                         nsIAtom* aAttribute,
                                         int32_t aModType)
{
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  nsChangeObserver* obs = LookupContentChangeObserver(aElement);
  if (obs)
    obs->ObserveAttributeChanged(aDocument, aElement, aAttribute);
}


void nsMenuGroupOwnerX::ContentRemoved(nsIDocument * aDocument,
                                       nsIContent * aContainer,
                                       nsIContent * aChild,
                                       int32_t aIndexInContainer,
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
                                        int32_t /* unused */)
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
    return nullptr;
}


// Given a menu item, creates a unique 4-character command ID and
// maps it to the item. Returns the id for use by the client.
uint32_t nsMenuGroupOwnerX::RegisterForCommand(nsMenuItemX* inMenuItem)
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
void nsMenuGroupOwnerX::UnregisterCommand(uint32_t inCommandID)
{
  mCommandToMenuObjectTable.Remove(inCommandID);
}


nsMenuItemX* nsMenuGroupOwnerX::GetMenuItemForCommandID(uint32_t inCommandID)
{
  nsMenuItemX * result;
  if (mCommandToMenuObjectTable.Get(inCommandID, &result))
    return result;
  else
    return nullptr;
}
