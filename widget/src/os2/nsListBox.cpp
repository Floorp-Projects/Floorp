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

#include "nsListBox.h"

// listbox; need more code here to allow multiple selections.

NS_IMPL_ADDREF(nsListBox)
NS_IMPL_RELEASE(nsListBox)

nsresult nsListBox::QueryInterface( const nsIID &aIID, void **aInstancePtr)
{
   nsresult result = nsWindow::QueryInterface( aIID, aInstancePtr);

   if( result == NS_NOINTERFACE)
   {
      if( aIID.Equals( nsIListBox::GetIID()))
      {
         *aInstancePtr = (void*) ((nsIListBox*)this);
         NS_ADDREF_THIS();
         result = NS_OK;
      }
      else if( aIID.Equals( nsIListWidget::GetIID()))
      {
         *aInstancePtr = (void*) ((nsIListWidget*)this);
         NS_ADDREF_THIS();
         result = NS_OK;
      }
   }

   return result;
}

#define LS_U_MULTISEL (LS_MULTIPLESEL | LS_EXTENDEDSEL)

nsresult nsListBox::SetMultipleSelection( PRBool aMultipleSelections)
{
   if( mWnd)
   {
      if( aMultipleSelections)
         AddToStyle( LS_U_MULTISEL);
      else
         RemoveFromStyle( LS_U_MULTISEL);
   }
   else
   {
      if( aMultipleSelections)
         mStyle |= LS_U_MULTISEL;
      else
         mStyle &= ~LS_U_MULTISEL;
   }
   return NS_OK;
}

// Helper
PRInt32 nsListBox::GetSelections( PRInt32 *aIndices, PRInt32 aSize)
{
   PRInt32 cItems = 0;
   short   sItemStart = LIT_FIRST;

   for(;;)
   {
      sItemStart = SHORT1FROMMR( SendMsg( LM_QUERYSELECTION,
                                          MPFROMSHORT(sItemStart)));
      if( sItemStart == LIT_NONE)
         break;

      if( aIndices && cItems < aSize)
         aIndices[cItems] = sItemStart;
      cItems++;
   }

   return cItems;
}

PRInt32 nsListBox::GetSelectedCount()
{
   return GetSelections( 0, 0);
}

nsresult nsListBox::GetSelectedIndices( PRInt32 aIndices[], PRInt32 aSize)
{
   GetSelections( aIndices, aSize);
   return NS_OK;
}

nsresult nsListBox::SetSelectedIndices( PRInt32 aIndices[], PRInt32 aSize)
{
   // clear selection
   Deselect();

   for( int i = 0; i < aSize; i++)
      SendMsg( LM_SELECTITEM, MPFROMSHORT( aIndices[i]), MPFROMLONG( TRUE));

   return NS_OK;
}

// Platform hooks
nsresult nsListBox::PreCreateWidget( nsWidgetInitData *aInitData)
{
   if( nsnull != aInitData)
   {
      nsListBoxInitData *data = (nsListBoxInitData *) aInitData;
      if( data->mMultiSelect)
         mStyle |= LS_U_MULTISEL;
   }
   return NS_OK;
}

PCSZ nsListBox::WindowClass()
{
   return WC_LISTBOX;
}

ULONG nsListBox::WindowStyle()
{
   return mStyle | LS_NOADJUSTPOS | BASE_CONTROL_STYLE;
}
