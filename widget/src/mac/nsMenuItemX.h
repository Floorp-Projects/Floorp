/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMenuItemX_h__
#define nsMenuItemX_h__


#include "nsIMenuItem.h"
#include "nsString.h"
#include "nsIMenuListener.h"
#include "nsIChangeManager.h"
#include "nsWeakReference.h"
#include "nsIWidget.h"

class nsIMenu;

/**
 * Native Motif MenuItem wrapper
 */

class nsMenuItemX : public nsIMenuItem,
                    public nsIMenuListener,
                    public nsIChangeObserver,
                    public nsSupportsWeakReference
{
public:
  nsMenuItemX();
  virtual ~nsMenuItemX();

  // nsISupports
  NS_DECL_ISUPPORTS
  NS_DECL_NSICHANGEOBSERVER

  // nsIMenuItem Methods
  NS_IMETHOD Create ( nsIMenu* aParent, const nsString & aLabel, PRBool aIsSeparator,
                        EMenuItemType aItemType, PRBool aEnabled, 
                        nsIChangeManager* aManager, nsIWebShell* aShell, nsIContent* aNode ) ;
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
  nsEventStatus CheckRebuild(PRBool & aMenuEvent);
  nsEventStatus SetRebuild(PRBool aMenuEvent);

protected:

  void UncheckRadioSiblings ( nsIContent* inCheckedElement ) ;

  nsString        mLabel;
  nsString        mKeyEquivalent;

  nsIMenu*                  mMenuParent;          // weak, parent owns us
  nsIChangeManager*         mManager;             // weak

  nsCOMPtr<nsIWidget>       mTarget;              // never set?
  nsCOMPtr<nsIMenuListener> mXULCommandListener;
  
  nsWeakPtr                 mWebShellWeakRef;     // weak ref to webshell
  nsCOMPtr<nsIContent>      mContent; 
  
  PRUint8           mModifiers;
  PRPackedBool      mIsSeparator;
  PRPackedBool      mEnabled;
  PRPackedBool      mIsChecked;
  EMenuItemType     mMenuType;
};

#endif // nsMenuItem_h__
