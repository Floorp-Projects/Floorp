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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

// popup menu

#include "nsPopupMenu.h"
#include "nsWindow.h"

// XPCom
NS_IMPL_ADDREF(nsPopupMenu)
NS_IMPL_RELEASE(nsPopupMenu)

nsresult nsPopupMenu::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
   if( !aInstancePtr)
      return NS_ERROR_NULL_POINTER;

   *aInstancePtr = 0;

   if( aIID.Equals(NS_GET_IID(nsIPopupMenu)))
   {
      *aInstancePtr = (void*) ((nsIPopUpMenu*) this);
      NS_ADDREF_THIS();
      return NS_OK;
   }
   if( aIID.Equals(((nsISupports*)(nsIPopUpMenu*)this)->GetIID()))
   {
      *aInstancePtr = (void*) ((nsISupports*)this);
      NS_ADDREF_THIS();
      return NS_OK;
   }
 
   return NS_NOINTERFACE;
}


nsPopupMenu::nsPopupMenu() : mParent(0), nsMenuBase()
{
   NS_INIT_REFCNT();
}

nsPopupMenu::~nsPopupMenu()
{
   NS_IF_RELEASE(mParent);
}

// Creation: parent to the desktop, owner to the `parent'
nsresult nsPopupMenu::Create( nsIWidget *aParent)
{
   // find the owner hwnd & nsWindow (for the toolkit)
   HWND hwndOwner = (HWND) aParent->GetNativeData( NS_NATIVE_WINDOW);
   mParent = NS_HWNDToWindow( hwndOwner);
   NS_ADDREF(mParent);

   nsIToolkit *aToolkit = mParent->GetToolkit();

   nsMenuBase::Create( HWND_DESKTOP, (nsToolkit*) aToolkit);
   NS_RELEASE(aToolkit);

   WinSetOwner( mWnd, hwndOwner);

   return NS_OK;
}

nsresult nsPopupMenu::GetParent( nsIWidget *&aParent)
{
   NS_IF_RELEASE(aParent);
   aParent = mParent;
   NS_IF_ADDREF(aParent);
   return NS_OK;
}

ULONG nsPopupMenu::WindowStyle()
{
   return 0;
}

nsresult nsPopupMenu::ShowMenu( PRInt32 aX, PRInt32 aY)
{
   POINTL pos = { aX, aY };
   mParent->NS2PM(pos);

   WinPopupMenu( HWND_DESKTOP,
                 (HWND) mParent->GetNativeData( NS_NATIVE_WIDGET),
                 mWnd, pos.x, pos.y, 0,
                 PU_HCONSTRAIN | PU_VCONSTRAIN | PU_NONE |
                 PU_KEYBOARD | PU_MOUSEBUTTON1 | PU_MOUSEBUTTON2);
   return NS_OK;
}

NS_METHOD nsPopUpMenu::AddMenu(nsIMenu * aMenu)
{
  return NS_OK;

}
