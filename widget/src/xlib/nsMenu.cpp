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

#include "nsMenu.h"
#include "nsIMenu.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"

NS_IMPL_ISUPPORTS2(nsMenu, nsIMenu, nsIMenuListener)

nsMenu::nsMenu()
{
  NS_INIT_ISUPPORTS();
  mMenuBarParent = nsnull;
  mMenuParent    = nsnull;
  mListener      = nsnull;
  //  nsresult result = NS_NewISupportsArray(&mItems);
}

nsMenu::~nsMenu()
{
  //  NS_IF_RELEASE(mMenuBarParent);
  NS_IF_RELEASE(mMenuParent);
  NS_IF_RELEASE(mListener);

  // Remove all references to the items
  mItems->Clear();
}

NS_METHOD nsMenu::Create(nsISupports * aParent, const nsAReadableString &aLabel,
                         const nsAReadableString &aAccessKey, 
                         nsIChangeManager* aManager, nsIWebShell* aShell,
                         nsIContent* aContent )
{
  return NS_OK;
}

NS_METHOD nsMenu::GetParent(nsISupports*& aParent)
{
  return NS_OK;
}

NS_METHOD nsMenu::GetLabel(nsString &aText)
{
  return NS_OK;
}

NS_METHOD nsMenu::SetLabel(const nsAReadableString &aText)
{
  return NS_OK;
}

NS_METHOD nsMenu::GetEnabled(PRBool* aIsEnabled)
{
  return NS_OK;
}

NS_METHOD nsMenu::IsHelpMenu(PRBool* aIsHelpMenu)
{
  return NS_OK;
}

NS_METHOD nsMenu::AddItem(nsISupports * aItem)
{
  return NS_OK;
}

nsIMenuBar * nsMenu::GetMenuBar(nsIMenu * aMenu)
{
  return NS_OK;
}

nsIWidget *  nsMenu::GetParentWidget()
{
  return nsnull;
}

NS_METHOD nsMenu::AddMenuItem(nsIMenuItem * aMenuItem)
{
  return NS_OK;
}

NS_METHOD nsMenu::AddMenu(nsIMenu * aMenu)
{
  return NS_OK;
}

NS_METHOD nsMenu::AddSeparator() 
{
  return NS_OK;
}

NS_METHOD nsMenu::GetItemCount(PRUint32 &aCount)
{
  return NS_OK;
}

NS_METHOD nsMenu::GetItemAt(const PRUint32 aCount, nsISupports *& aMenuItem)
{
  return NS_OK;
}

NS_METHOD nsMenu::InsertItemAt(const PRUint32 aCount, nsISupports * aMenuItem)
{
  return NS_OK;
}

NS_METHOD nsMenu::InsertSeparator(const PRUint32 aCount)
{
  return NS_OK;
}

NS_METHOD nsMenu::RemoveItem(const PRUint32 aCount)
{
  return NS_OK;
}

NS_METHOD nsMenu::RemoveAll()
{
  return NS_OK;
}

NS_METHOD nsMenu::SetNativeData(void * aData)
{
  return NS_OK;
}

NS_METHOD nsMenu::GetNativeData(void ** aData)
{
  return NS_OK;
}

NS_METHOD nsMenu::AddMenuListener(nsIMenuListener * aMenuListener)
{
  return NS_OK;
}

NS_METHOD nsMenu::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
  return NS_OK;
}

NS_METHOD nsMenu::SetEnabled(PRBool aIsEnabled)
{
  return NS_OK;
}

nsEventStatus nsMenu::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenu::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenu::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenu::MenuConstruct(const nsMenuEvent & aMenuEvent,
                                    nsIWidget         * aParentWindow, 
                                    void              * menubarNode,
                                    void              * aWebShell)
{
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenu::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  return nsEventStatus_eIgnore;
}

NS_METHOD nsMenu::SetDOMNode(nsIDOMNode * menuNode)
{
  return NS_OK;
}

NS_METHOD nsMenu::SetDOMElement(nsIDOMElement * menuElement)
{
  return NS_OK;
}

NS_METHOD nsMenu::SetWebShell(nsIWebShell * aWebShell)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::GetAccessKey(nsString &aText)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenu::SetAccessKey(const nsAReadableString &aText)
{
  return NS_OK;
}
