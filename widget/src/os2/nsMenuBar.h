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

#ifndef _nsMenubar_h
#define _nsMenubar_h

// menubar; WC_MENU with MS_ACTIONBAR

#include "nsMenuBase.h"
#include "nsIMenuBar.h"

class nsMenuBar : public nsIMenuBar, public nsMenuBase
{
 public:
   nsMenuBar();
  ~nsMenuBar();

   // nsISupports
   NS_DECL_ISUPPORTS

   // nsIMenuBar
   NS_IMETHOD Create( nsIWidget *aParent);
   NS_IMETHOD GetParent( nsIWidget *&aParent);
   NS_IMETHOD SetParent( nsIWidget *aParent);
   NS_IMETHOD AddMenu( nsIMenu *aMenu);
   NS_IMETHOD InsertMenuAt( const PRUint32 aCount, nsIMenu *&aMenu);
   NS_IMETHOD GetMenuCount( PRUint32 &aCount)
   { return nsMenuBase::GetItemCount( aCount); }
   NS_IMETHOD GetMenuAt( const PRUint32 aCount, nsIMenu *&aMenu);
   NS_IMETHOD RemoveMenu( const PRUint32 aCount);
   NS_IMETHOD RemoveAll();
   NS_IMETHOD GetNativeData( void *&aData)
   { return nsMenuBase::GetNativeData( &aData); }
   NS_IMETHOD Paint();

   // strange mac method
   NS_IMETHOD SetNativeData( void *aData) { return NS_OK; }

   // nsIMenuListener interface
   nsEventStatus MenuConstruct( const nsMenuEvent &aMenuEvent,
                                nsIWidget         *aParentWindow, 
                                void              *menubarNode,
                                void              *aWebShell);
 protected:
   ULONG WindowStyle();
   void  UpdateFrame();

   HWND       mWndFrame; // hang on to the frame hwnd so we can update it
   nsIWidget *mParent;   // nsIWidget for the frame/client window
};

#endif
