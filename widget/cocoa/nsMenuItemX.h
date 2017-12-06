/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMenuItemX_h_
#define nsMenuItemX_h_

#include "mozilla/RefPtr.h"
#include "nsMenuBaseX.h"
#include "nsMenuGroupOwnerX.h"
#include "nsChangeObserver.h"
#include "nsStringFwd.h"

#import <Cocoa/Cocoa.h>

class nsMenuItemIconX;
class nsMenuX;

namespace mozilla {
namespace dom {
class Element;
}
}

enum {
  knsMenuItemNoModifier      = 0,
  knsMenuItemShiftModifier   = (1 << 0),
  knsMenuItemAltModifier     = (1 << 1),
  knsMenuItemControlModifier = (1 << 2),
  knsMenuItemCommandModifier = (1 << 3)
};

enum EMenuItemType {
  eRegularMenuItemType = 0,
  eCheckboxMenuItemType,
  eRadioMenuItemType,
  eSeparatorMenuItemType
};


// Once instantiated, this object lives until its DOM node or its parent window is destroyed.
// Do not hold references to this, they can become invalid any time the DOM node can be destroyed.
class nsMenuItemX : public nsMenuObjectX,
                    public nsChangeObserver
{
public:
  nsMenuItemX();
  virtual ~nsMenuItemX();

  NS_DECL_CHANGEOBSERVER

  // nsMenuObjectX
  void*             NativeData() override {return (void*)mNativeMenuItem;}
  nsMenuObjectTypeX MenuObjectType() override {return eMenuItemObjectType;}

  // nsMenuItemX
  nsresult      Create(nsMenuX* aParent, const nsString& aLabel, EMenuItemType aItemType,
                       nsMenuGroupOwnerX* aMenuGroupOwner, nsIContent* aNode);
  nsresult      SetChecked(bool aIsChecked);
  EMenuItemType GetMenuItemType();
  void          DoCommand();
  nsresult      DispatchDOMEvent(const nsString &eventName, bool* preventDefaultCalled);
  void          SetupIcon();

protected:
  void UncheckRadioSiblings(nsIContent* inCheckedElement);
  void SetKeyEquiv();

  EMenuItemType                 mType;

  // nsMenuItemX objects should always have a valid native menu item.
  NSMenuItem*                   mNativeMenuItem;      // [strong]
  nsMenuX*                      mMenuParent;          // [weak]
  nsMenuGroupOwnerX*            mMenuGroupOwner;      // [weak]
  RefPtr<mozilla::dom::Element> mCommandElement;
  // The icon object should never outlive its creating nsMenuItemX object.
  RefPtr<nsMenuItemIconX>       mIcon;
  bool                          mIsChecked;
};

#endif // nsMenuItemX_h_
