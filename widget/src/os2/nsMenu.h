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

#ifndef _nsMenu_h
#define _nsMenu_h

// A submenu which reflects a DOM content model

#include "nsIMenu.h"
#include "nsMenuBase.h"

class nsMenu : public nsIMenu, public nsDynamicMenu
{
 public:
   nsMenu();
  ~nsMenu();

   // nsISupports
   NS_DECL_ISUPPORTS

   // nsIMenu extras
   NS_IMETHOD Create( nsISupports *aParent, const nsString &aLabel);
   NS_IMETHOD GetParent( nsISupports *&aParent);
   NS_IMETHOD GetLabel( nsString &aText);
   NS_IMETHOD SetLabel( const nsString &aText);
   NS_IMETHOD SetEnabled(PRBool aIsEnabled);

   // Common methods
   DECL_DYNAMIC_MENU_METHODS

 protected:
   nsString         mLabel;
   nsISupports     *mParent;
};

#endif
