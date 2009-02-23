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

#ifndef nsMenuItemX_h_
#define nsMenuItemX_h_

#include "nsMenuBaseX.h"
#include "nsChangeObserver.h"
#include "nsAutoPtr.h"

#import <Cocoa/Cocoa.h>

class nsString;
class nsMenuItemIconX;
class nsMenuX;
class nsMenuBarX;

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
  void*             NativeData()     {return (void*)mNativeMenuItem;}
  nsMenuObjectTypeX MenuObjectType() {return eMenuItemObjectType;}

  // nsMenuItemX
  nsresult      Create(nsMenuX* aParent, const nsString& aLabel, EMenuItemType aItemType,
                       nsMenuBarX* aMenuBar, nsIContent* aNode);
  nsresult      SetChecked(PRBool aIsChecked);
  EMenuItemType GetMenuItemType();
  void          DoCommand();
  nsresult      DispatchDOMEvent(const nsString &eventName, PRBool* preventDefaultCalled);
  void          SetupIcon();

protected:
  void UncheckRadioSiblings(nsIContent* inCheckedElement);
  void SetKeyEquiv(PRUint8 aModifiers, const nsString &aText);

  EMenuItemType             mType;
  // nsMenuItemX objects should always have a valid native menu item.
  NSMenuItem*               mNativeMenuItem;      // [strong]
  nsMenuX*                  mMenuParent;          // [weak]
  nsMenuBarX*               mMenuBar;             // [weak]
  nsCOMPtr<nsIContent>      mCommandContent;
  // The icon object should never outlive its creating nsMenuItemX object.
  nsRefPtr<nsMenuItemIconX> mIcon;
  PRBool                    mIsChecked;
};

#endif // nsMenuItemX_h_
