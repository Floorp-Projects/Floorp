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

#ifndef _nscontextmenu_h
#define _nscontextmenu_h

#include "nsIContextMenu.h"
#include "nsMenuBase.h"

class nsWindow;

// Pop-up menu

class nsContextMenu : public nsIContextMenu, public nsDynamicMenu
{
 public:
   nsContextMenu();
   virtual ~nsContextMenu();

   // nsISupports
   NS_DECL_ISUPPORTS

   // nsIContextMenu extras
   NS_IMETHOD Create( nsISupports *aParent,
                      const nsString &str1, const nsString &str2);
   NS_IMETHOD GetParent( nsISupports *&aParent);
   NS_IMETHOD SetLocation( PRInt32 aX, PRInt32 aY);

   // nsIMenuListener overrides
   nsEventStatus MenuSelected( const nsMenuEvent &aMenuEvent);
   nsEventStatus MenuDeselected( const nsMenuEvent &aMenuEvent);

   // Common methods
   DECL_DYNAMIC_MENU_METHODS

 protected:
   nsWindow *mOwner;
   PRInt32   mX, mY;
   PRBool    mShowing;
   nsString  mAnchor;
   nsString  mAlignment;
   nsString  mAnchorAlignment;
   nsIWidget* mParentWindow;
};

#endif
