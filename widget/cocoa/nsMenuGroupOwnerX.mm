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
#include "nsThreadUtils.h"

#include "mozilla/dom/Element.h"
#include "nsIWidget.h"
#include "nsIDocument.h"

#include "nsINode.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(nsMenuGroupOwnerX, nsIMutationObserver)


nsMenuGroupOwnerX::nsMenuGroupOwnerX()
: mCurrentCommandID(eCommand_ID_Last)
{
  mInfoSet = [[NSMutableSet setWithCapacity:10] retain];
}


nsMenuGroupOwnerX::~nsMenuGroupOwnerX()
{
  MOZ_ASSERT(mContentToObserverTable.Count() == 0, "have outstanding mutation observers!\n");

  // The MenuItemInfo objects in mInfoSet may live longer than we do.  So when
  // we get destroyed we need to invalidate all their mMenuGroupOwner pointers.
  NSEnumerator* counter = [mInfoSet objectEnumerator];
  MenuItemInfo* info;
  while ((info = (MenuItemInfo*) [counter nextObject])) {
    [info setMenuGroupOwner:nil];
  }
  [mInfoSet release];
}


nsresult nsMenuGroupOwnerX::Create(mozilla::dom::Element* aContent)
{
  if (!aContent)
    return NS_ERROR_INVALID_ARG;

  mContent = aContent;

  return NS_OK;
}


//
// nsIMutationObserver
//


void nsMenuGroupOwnerX::CharacterDataWillChange(nsIContent* aContent,
                                                const CharacterDataChangeInfo&)
{
}


void nsMenuGroupOwnerX::CharacterDataChanged(nsIContent* aContent,
                                             const CharacterDataChangeInfo&)
{
}


void nsMenuGroupOwnerX::ContentAppended(nsIContent* aFirstNewContent)
{
  for (nsIContent* cur = aFirstNewContent; cur; cur = cur->GetNextSibling()) {
    ContentInserted(cur);
  }
}


void nsMenuGroupOwnerX::NodeWillBeDestroyed(const nsINode * aNode)
{
}


void nsMenuGroupOwnerX::AttributeWillChange(dom::Element* aContent,
                                            int32_t aNameSpaceID,
                                            nsAtom* aAttribute,
                                            int32_t aModType,
                                            const nsAttrValue* aNewValue)
{
}

void nsMenuGroupOwnerX::NativeAnonymousChildListChange(nsIContent* aContent,
                                                       bool aIsRemove)
{
}

void nsMenuGroupOwnerX::AttributeChanged(dom::Element* aElement,
                                         int32_t aNameSpaceID,
                                         nsAtom* aAttribute,
                                         int32_t aModType,
                                         const nsAttrValue* aOldValue)
{
  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  nsChangeObserver* obs = LookupContentChangeObserver(aElement);
  if (obs)
    obs->ObserveAttributeChanged(aElement->OwnerDoc(), aElement, aAttribute);
}


void nsMenuGroupOwnerX::ContentRemoved(nsIContent* aChild,
                                       nsIContent* aPreviousSibling)
{
  nsIContent* container = aChild->GetParent();
  if (!container) {
    return;
  }

  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  nsChangeObserver* obs = LookupContentChangeObserver(container);
  if (obs)
    obs->ObserveContentRemoved(aChild->OwnerDoc(), container, aChild,
                               aPreviousSibling);
  else if (container != mContent) {
    // We do a lookup on the parent container in case things were removed
    // under a "menupopup" item. That is basically a wrapper for the contents
    // of a "menu" node.
    nsCOMPtr<nsIContent> parent = container->GetParent();
    if (parent) {
      obs = LookupContentChangeObserver(parent);
      if (obs)
        obs->ObserveContentRemoved(aChild->OwnerDoc(), aChild->GetParent(),
                                   aChild, aPreviousSibling);
    }
  }
}


void nsMenuGroupOwnerX::ContentInserted(nsIContent* aChild)
{
  nsIContent* container = aChild->GetParent();
  if (!container) {
    return;
  }

  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  nsChangeObserver* obs = LookupContentChangeObserver(container);
  if (obs)
    obs->ObserveContentInserted(aChild->OwnerDoc(), container, aChild);
  else if (container != mContent) {
    // We do a lookup on the parent container in case things were removed
    // under a "menupopup" item. That is basically a wrapper for the contents
    // of a "menu" node.
    nsCOMPtr<nsIContent> parent = container->GetParent();
    if (parent) {
      obs = LookupContentChangeObserver(parent);
      if (obs)
        obs->ObserveContentInserted(aChild->OwnerDoc(), container, aChild);
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
  if (!mContentToObserverTable.Contains(aContent)) {
    aContent->AddMutationObserver(this);
  }
  mContentToObserverTable.Put(aContent, aMenuObject);
}


void nsMenuGroupOwnerX::UnregisterForContentChanges(nsIContent *aContent)
{
  if (mContentToObserverTable.Contains(aContent)) {
    aContent->RemoveMutationObserver(this);
  }
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

void nsMenuGroupOwnerX::AddMenuItemInfoToSet(MenuItemInfo* info)
{
  [mInfoSet addObject:info];
}
