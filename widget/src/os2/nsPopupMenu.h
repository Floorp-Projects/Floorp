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

#ifndef _nsPopupMenu_h
#define _nsPopupMenu_h

// A menu for popping up.

#include "nsIPopupMenu.h"
#include "nsMenuBase.h"

class nsWindow;
class nsIMenuItem;

class nsPopupMenu : public nsIPopUpMenu, public nsMenuBase
{
 public:
   nsPopupMenu();
  ~nsPopupMenu();

   // nsISupports
   NS_DECL_ISUPPORTS

   // nsIPopupMenu
   NS_IMETHOD Create( nsIWidget *aParent);
   NS_IMETHOD GetParent( nsIWidget *&aParent);
   NS_IMETHOD ShowMenu( PRInt32 aX, PRInt32 aY);
   NS_IMETHOD AddMenu(nsIMenu * aMenu);

   // Common methods with nsMenu
   DECL_MENU_BASE_METHODS

   // nsIPopUpMenu hasn't been revamped along with the other menu classes
   // in the `dynamic menu' effort, so don't try too hard getting these
   // old-style methods implemented -- it'll change soon. (ask saari if
   // concerned).
   NS_IMETHOD AddItem( nsIMenuItem *aMenuItem)
     { return nsMenuBase::InsertItemAt( aMenuItem); }
   NS_IMETHOD GetItemAt( const PRUint32 aCount, nsIMenuItem *&aThing)
     { return NS_ERROR_NOT_IMPLEMENTED; }
   NS_IMETHOD InsertItemAt( const PRUint32 aCount, nsIMenuItem *&aMenuItem)
     { return NS_ERROR_NOT_IMPLEMENTED; }
   NS_IMETHOD InsertItemAt( const PRUint32 aCount, const nsString &aMenuItemName)
     { return NS_ERROR_NOT_IMPLEMENTED; }
   NS_IMETHOD GetNativeData( void *&aData)
     { return nsMenuBase::GetNativeData( &aData); }

 protected:
   ULONG WindowStyle();

   nsWindow *mParent;
};

#endif
