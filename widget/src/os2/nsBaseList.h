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

#ifndef _nsbaselist_h
#define _nsbaselist_h

// Share code between listbox & combobox controls.
// Avoid inheriting one from the other.

#include "nsstring.h"
#include "nsISupports.h"

class nsBaseList
{
 public:
   // nsIListWidget implementation
   NS_IMETHOD       AddItemAt( nsString &aItem, PRInt32 aPosition);
   virtual PRInt32  FindItem( nsString &aItem, PRInt32 aStartPos);
   virtual PRInt32  GetItemCount();
   virtual PRBool   RemoveItemAt( PRInt32 aPosition);
   virtual PRBool   GetItemAt( nsString& anItem, PRInt32 aPosition);
   NS_IMETHOD       GetSelectedItem( nsString &aItem);
   virtual PRInt32  GetSelectedIndex();
   NS_IMETHOD       SelectItem( PRInt32 aPosition);
   NS_IMETHOD       Deselect();

   // helper
   virtual MRESULT  SendMsg( ULONG msg, MPARAM mp1=0, MPARAM mp2=0) = 0;

 protected:
   PRBool CheckIndex( PRInt32 &index);
};

#define DECL_BASE_LIST_METHODS virtual nsresult AddItemAt( nsString &aItem, PRInt32 aPosition) \
   { return nsBaseList::AddItemAt( aItem,aPosition); }             \
   virtual PRInt32 FindItem( nsString &aItem, PRInt32 aStartPos)   \
   { return nsBaseList::FindItem( aItem, aStartPos); }             \
   virtual PRInt32 GetItemCount()                                  \
   { return nsBaseList::GetItemCount(); }                          \
   virtual PRBool RemoveItemAt( PRInt32 aPosition)                 \
   { return nsBaseList::RemoveItemAt( aPosition); }                \
   virtual PRBool GetItemAt( nsString& anItem, PRInt32 aPosition)  \
   { return nsBaseList::GetItemAt( anItem, aPosition); }           \
   NS_IMETHOD   GetSelectedItem( nsString &aItem)                  \
   { return nsBaseList::GetSelectedItem( aItem); }                 \
   virtual PRInt32 GetSelectedIndex()                              \
   { return nsBaseList::GetSelectedIndex(); }                      \
   NS_IMETHOD   SelectItem( PRInt32 aPosition)                     \
   { return nsBaseList::SelectItem( aPosition); }                  \
   NS_IMETHOD   Deselect()                                         \
   { return nsBaseList::Deselect(); }                              \
   virtual MRESULT SendMsg( ULONG msg, MPARAM mp1=0, MPARAM mp2=0) \
   { return mOS2Toolkit->SendMsg( mWnd, msg, mp1, mp2); }

#endif
