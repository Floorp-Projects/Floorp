/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

// normal menu

#include "nsMenu.h"
#include "nsIMenuBar.h"
#include "nsMenuItem.h"
#include "nsIContextMenu.h"

#include "nsIWebShell.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"

#include "nsCOMPtr.h"

NS_IMPL_ADDREF(nsMenu)
NS_IMPL_RELEASE(nsMenu)

nsresult nsMenu::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if( !aInstancePtr)
    return NS_ERROR_NULL_POINTER;

  *aInstancePtr = 0;

  if( aIID.Equals(nsIMenu::GetIID()))
  {
     *aInstancePtr = (void*) ((nsIMenu*) this);
     NS_ADDREF_THIS();
     return NS_OK;
  }
  if( aIID.Equals(nsIMenuListener::GetIID()))
  {
     *aInstancePtr = (void*) ((nsIMenuListener*)this);
     NS_ADDREF_THIS();
     return NS_OK;
  }
  if( aIID.Equals(((nsISupports*)(nsIMenu*)this)->GetIID()))
  {
     *aInstancePtr = (void*) ((nsISupports*) ((nsIMenu*)this));
     NS_ADDREF_THIS();
     return NS_OK;
  }

  return NS_NOINTERFACE;
}

nsMenu::nsMenu() : mParent(nsnull)
{
   NS_INIT_REFCNT();
}

nsMenu::~nsMenu()
{
}

nsresult nsMenu::Create( nsISupports *aThing, const nsString &aLabel)
{
   if( !aThing)
      return NS_ERROR_NULL_POINTER;

   void           *pvHwnd = 0;
   nsIMenu        *aMenu = nsnull;
   nsIContextMenu *aPopup = nsnull;
   nsIMenuBar     *aBar = nsnull;

   if( NS_SUCCEEDED( aThing->QueryInterface( nsIMenuBar::GetIID(),
                                             (void**) &aBar)))
   {
      aBar->GetNativeData( pvHwnd);
      NS_RELEASE(aBar);
   }
   else if( NS_SUCCEEDED( aThing->QueryInterface( nsIMenu::GetIID(),
                                                  (void**) &aMenu)))
   {
      aMenu->GetNativeData( &pvHwnd);
      NS_RELEASE(aMenu);
   }
   else if( NS_SUCCEEDED( aThing->QueryInterface( nsIContextMenu::GetIID(),
                                                  (void**) &aPopup)))
   {
      aPopup->GetNativeData( &pvHwnd);
      NS_RELEASE(aPopup);
   }

   // This is a bit dubious, as there's no guarantee that this menu
   // is being created in the same thread as the parent was.  But this
   // is probably moot...

   nsMenuBase *pPBase = (nsMenuBase*) WinQueryWindowPtr( (HWND) pvHwnd, QWL_USER);

   nsMenuBase::Create( HWND_DESKTOP, pPBase->GetTK());

   // Connect up to parent menu component
   WinSetOwner( mWnd, (HWND) pvHwnd);
   mParent = aThing;

   // record text
   mLabel = aLabel;

   return NS_OK;
}

nsresult nsMenu::GetParent( nsISupports * &aParent)
{
   NS_IF_RELEASE(aParent);
   aParent = mParent;
   NS_IF_ADDREF(aParent);
   return NS_OK;
}

nsresult nsMenu::GetLabel( nsString &aText)
{
   aText = mLabel;
   return NS_OK;
}

nsresult nsMenu::SetLabel( const nsString &aText)
{
   mLabel = aText;
   return NS_OK;
}

NS_METHOD nsMenu::SetEnabled(PRBool aIsEnabled)
{
  return NS_OK;
}
