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

#ifndef nsMenuItem_h__
#define nsMenuItem_h__


#include "nsIMenuItem.h"
#include "nsString.h"
#include "nsIMenuListener.h"
#include "nsIChangeManager.h"
#include "nsWeakReference.h"


class nsIMenu;
class nsIWidget;

/**
 * Native Motif MenuItem wrapper
 */

class nsMenuItem :  public nsIMenuItem,
                    public nsIMenuListener,
                    public nsIChangeObserver,
                    public nsSupportsWeakReference
{
public:
  nsMenuItem();
  virtual ~nsMenuItem();

  // nsISupports
  NS_DECL_ISUPPORTS
  NS_DECL_NSICHANGEOBSERVER

  // nsIMenuItem Methods
  NS_IMETHOD Create ( nsIMenu* aParent, const nsString & aLabel, PRBool aIsSeparator,
                        EMenuItemType aItemType, PRBool aEnabled, 
                        nsIChangeManager* aManager, nsIWebShell* aShell, nsIDOMNode* aNode ) ;
  NS_IMETHOD GetLabel(nsString &aText);
  NS_IMETHOD SetShortcutChar(const nsString &aText);
  NS_IMETHOD GetShortcutChar(nsString &aText);
  NS_IMETHOD GetEnabled(PRBool *aIsEnabled);
  NS_IMETHOD SetChecked(PRBool aIsEnabled);
  NS_IMETHOD GetChecked(PRBool *aIsEnabled);
  NS_IMETHOD GetMenuItemType(EMenuItemType *aIsCheckbox);
  NS_IMETHOD GetTarget(nsIWidget *& aTarget);
  NS_IMETHOD GetNativeData(void*& aData);
  NS_IMETHOD AddMenuListener(nsIMenuListener * aMenuListener);
  NS_IMETHOD RemoveMenuListener(nsIMenuListener * aMenuListener);
  NS_IMETHOD IsSeparator(PRBool & aIsSep);

  NS_IMETHOD DoCommand();
  NS_IMETHOD SetModifiers(PRUint8 aModifiers);
  NS_IMETHOD GetModifiers(PRUint8 * aModifiers);
    
  // nsIMenuListener interface
  nsEventStatus MenuItemSelected(const nsMenuEvent & aMenuEvent);
  nsEventStatus MenuSelected(const nsMenuEvent & aMenuEvent);
  nsEventStatus MenuDeselected(const nsMenuEvent & aMenuEvent);
  nsEventStatus MenuConstruct(const nsMenuEvent & aMenuEvent, nsIWidget * aParentWindow, 
                                void * menuNode, void * aWebShell);
  nsEventStatus MenuDestruct(const nsMenuEvent & aMenuEvent);

protected:

  void UncheckRadioSiblings ( nsIDOMElement* inCheckedElement ) ;

  nsString        mLabel;
  nsString        mKeyEquivalent;

  nsIMenu*                  mMenuParent;          // weak, parent owns us
  nsIChangeManager*         mManager;             // weak

  nsCOMPtr<nsIWidget>       mTarget;              // never set?
  nsCOMPtr<nsIMenuListener> mXULCommandListener;
  
  nsWeakPtr                 mWebShellWeakRef;     // weak ref to webshell
  nsCOMPtr<nsIDOMNode>      mDOMNode; 
  
  PRUint8           mModifiers;
  PRPackedBool      mIsSeparator;
  PRPackedBool      mEnabled;
  PRPackedBool      mIsChecked;
  EMenuItemType     mMenuType;
};

#endif // nsMenuItem_h__
