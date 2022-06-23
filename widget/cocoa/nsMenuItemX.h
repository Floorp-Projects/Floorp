/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMenuItemX_h_
#define nsMenuItemX_h_

#include "mozilla/RefPtr.h"
#include "nsISupports.h"
#include "nsMenuGroupOwnerX.h"
#include "nsMenuItemIconX.h"
#include "nsChangeObserver.h"
#include "nsStringFwd.h"

#import <Cocoa/Cocoa.h>

class nsMenuItemIconX;
class nsMenuX;
class nsMenuParentX;

namespace mozilla {
namespace dom {
class Element;
}
}  // namespace mozilla

enum {
  knsMenuItemNoModifier = 0,
  knsMenuItemShiftModifier = (1 << 0),
  knsMenuItemAltModifier = (1 << 1),
  knsMenuItemControlModifier = (1 << 2),
  knsMenuItemCommandModifier = (1 << 3)
};

enum EMenuItemType {
  eRegularMenuItemType = 0,
  eCheckboxMenuItemType,
  eRadioMenuItemType,
  eSeparatorMenuItemType
};

// Once instantiated, this object lives until its DOM node or its parent window
// is destroyed. Do not hold references to this, they can become invalid any
// time the DOM node can be destroyed.
class nsMenuItemX final : public nsChangeObserver,
                          public nsMenuItemIconX::Listener {
 public:
  nsMenuItemX(nsMenuX* aParent, const nsString& aLabel, EMenuItemType aItemType,
              nsMenuGroupOwnerX* aMenuGroupOwner, nsIContent* aNode);

  bool IsVisible() const { return mIsVisible; }

  // Unregisters nsMenuX from the nsMenuGroupOwner, and nulls out the group
  // owner pointer. This is needed because nsMenuX is reference-counted and can
  // outlive its owner, and the menu group owner asserts that everything has
  // been unregistered when it is destroyed.
  void DetachFromGroupOwner();

  // Nulls out our reference to the parent.
  // This is needed because nsMenuX is reference-counted and can outlive its
  // parent.
  void DetachFromParent() { mMenuParent = nullptr; }

  NS_INLINE_DECL_REFCOUNTING(nsMenuItemX)

  NS_DECL_CHANGEOBSERVER

  // nsMenuItemIconX::Listener
  void IconUpdated() override;

  // nsMenuItemX
  nsresult SetChecked(bool aIsChecked);
  EMenuItemType GetMenuItemType();
  void DoCommand(NSEventModifierFlags aModifierFlags, int16_t aButton);
  nsresult DispatchDOMEvent(const nsString& eventName,
                            bool* preventDefaultCalled);
  void SetupIcon();
  nsMenuX* ParentMenu() { return mMenuParent; }
  nsIContent* Content() { return mContent; }
  NSMenuItem* NativeNSMenuItem() { return mNativeMenuItem; }

  void Dump(uint32_t aIndent) const;

 protected:
  virtual ~nsMenuItemX();

  void UncheckRadioSiblings(nsIContent* aCheckedElement);
  void SetKeyEquiv();

  nsCOMPtr<nsIContent> mContent;  // XUL <menuitem> or <menuseparator>

  EMenuItemType mType;

  // nsMenuItemX objects should always have a valid native menu item.
  NSMenuItem* mNativeMenuItem = nil;             // [strong]
  nsMenuX* mMenuParent = nullptr;                // [weak]
  nsMenuGroupOwnerX* mMenuGroupOwner = nullptr;  // [weak]
  RefPtr<mozilla::dom::Element> mCommandElement;
  mozilla::UniquePtr<nsMenuItemIconX> mIcon;  // always non-null
  bool mIsChecked = false;
  bool mIsVisible = false;
};

#endif  // nsMenuItemX_h_
