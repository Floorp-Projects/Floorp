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

#include "nsWidgetDefs.h"
#include "nsBaseList.h"
#include "nsStringUtil.h"

// Basic things for talking to listboxen.
// Code shared by listbox & combobox controls.
//
// Very straight-forward; potential DBCS/Unicode problems as ever.

// return false if index is silly
PRBool nsBaseList::CheckIndex( PRInt32 &index)
{
   short sItems = GetItemCount();
   if( index == -1)
      index = sItems - 1;

   return index >= sItems ? PR_FALSE : PR_TRUE;
}

nsresult nsBaseList::AddItemAt( nsString &aItem, PRInt32 aPosition)
{
   NS_ALLOC_STR_BUF(buf, aItem, 256);

   short iPos = aPosition;
   if( iPos == -1)
      iPos = LIT_END;

   SendMsg( LM_INSERTITEM, MPFROMSHORT(iPos), MPFROMP(buf));

   NS_FREE_STR_BUF(buf);

   return NS_OK;
}

// Despite what the comment says, return the found index.
PRInt32 nsBaseList::FindItem( nsString &aItem, PRInt32 aStartPos)
{
   PRInt32 ret = -1;

   if( PR_TRUE == CheckIndex( aStartPos))
   {
      NS_ALLOC_STR_BUF(buf, aItem, 256);
   
      short sStart = aStartPos;
      if( sStart == 0)
         sStart = LIT_FIRST;
   
      MRESULT rc = SendMsg( LM_SEARCHSTRING,
                            MPFROM2SHORT(LSS_CASESENSITIVE,sStart),
                            MPFROMP(buf));
      ret = SHORT1FROMMR(rc);
   
      if( ret == LIT_NONE)
         ret = -1;
   
      NS_FREE_STR_BUF(buf);
   }
   return ret;
}

PRInt32 nsBaseList::GetItemCount()
{
   return SHORT1FROMMR( SendMsg( LM_QUERYITEMCOUNT));
}

PRBool nsBaseList::RemoveItemAt( PRInt32 aPosition)
{
   PRBool rc = CheckIndex( aPosition);

   if( rc == PR_TRUE)
      SendMsg( LM_DELETEITEM, MPFROMSHORT(aPosition));

   return rc;
}

PRBool nsBaseList::GetItemAt( nsString& anItem, PRInt32 aPosition)
{
   PRBool rc = CheckIndex( aPosition);

   if( rc == PR_TRUE)
   {
      // first get text length
      short len = SHORT1FROMMR( SendMsg( LM_QUERYITEMTEXTLENGTH,
                                         MPFROMSHORT(aPosition)));
   
      NS_ALLOC_CHAR_BUF(buf,256,len+1);
   
      // now get text & fill in string
      SendMsg( LM_QUERYITEMTEXT, MPFROM2SHORT(aPosition,len+1), MPFROMP(buf));
      anItem.AssignWithConversion(buf);
   
      NS_FREE_CHAR_BUF(buf);
   }

   return rc;
}

nsresult nsBaseList::GetSelectedItem( nsString &aItem)
{
   PRInt32 sel = GetSelectedIndex();
   if( sel != -1)
      GetItemAt( aItem, sel);
   return NS_OK;
}

PRInt32 nsBaseList::GetSelectedIndex()
{
   short sel = SHORT1FROMMR( SendMsg( LM_QUERYSELECTION,
                                      MPFROMSHORT(LIT_FIRST)));
   if( sel == LIT_NONE)
      sel = -1;

   return sel;
}

nsresult nsBaseList::SelectItem( PRInt32 aPosition)
{
   if( PR_TRUE == CheckIndex( aPosition))
      SendMsg( LM_SELECTITEM, MPFROMSHORT(aPosition), MPFROMLONG(TRUE));
   return NS_OK;
}

nsresult nsBaseList::Deselect()
{
   SendMsg( LM_SELECTITEM, MPFROMSHORT(LIT_NONE));
   return NS_OK;
}
