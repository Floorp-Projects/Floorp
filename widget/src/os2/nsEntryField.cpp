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

#include "nsEntryField.h"

NS_IMPL_ADDREF(nsEntryField)
NS_IMPL_RELEASE(nsEntryField)

nsresult nsEntryField::QueryInterface( const nsIID &aIID, void **aInstancePtr)
{
   nsresult result = nsWindow::QueryInterface( aIID, aInstancePtr);

   if( result == NS_NOINTERFACE && aIID.Equals( NS_GET_IID(nsITextWidget)))
   {
      *aInstancePtr = (void*) ((nsITextWidget*)this);
      NS_ADDREF_THIS();
      result = NS_OK;
   }

   return result;
}

// Textfield messages; all very straight-forward to translate into PM.

nsresult nsEntryField::GetText( nsString &aTextBuffer, PRUint32 aBufferSize, PRUint32 &size)
{
   nsWindow::GetWindowText( aTextBuffer, &size);
   return NS_OK;
}

nsresult nsEntryField::SetText( const nsString &aText, PRUint32 &len)
{
   SetTitle( aText);
   mText = aText;
   len = aText.Length();
   return NS_OK;
}

nsresult nsEntryField::InsertText( const nsString &aText, PRUint32 aStartPos,
                                       PRUint32 aEndPos, PRUint32& aActualSize)
{
   PRUint32 actualSize;
   nsString currentText;
   GetText( currentText, 256, actualSize);
   nsString newText( aText);
   currentText.Insert( newText, aStartPos);
   SetText( currentText, actualSize);
   aActualSize = aText.Length();
   mText = currentText;
   return NS_OK;
}

nsresult nsEntryField::RemoveText()
{
   PRUint32 dummy;
   SetText( nsString(), dummy);
   return NS_OK;
}

nsresult nsEntryField::SetPassword( PRBool aIsPassword)
{
   if( mWnd)
   {
      if( aIsPassword)
         AddToStyle( ES_UNREADABLE);
      else
         RemoveFromStyle( ES_UNREADABLE);
   }
   else
   {
      if( aIsPassword)
         mStyle |= ES_UNREADABLE;
      else
         mStyle &= ~ES_UNREADABLE;
   }
   return NS_OK;
}

nsresult nsEntryField::SetMaxTextLength( PRUint32 aChars)
{
   short sLength = aChars ? (short) aChars : 500; // !! hmm
   mOS2Toolkit->SendMsg( mWnd, EM_SETTEXTLIMIT, MPFROMSHORT(sLength));
   return NS_OK;
}

nsresult nsEntryField::SetReadOnly( PRBool aReadOnlyFlag, PRBool &old)
{
   BOOL bOldState;

   if( mWnd)
   {
      bOldState = (BOOL) mOS2Toolkit->SendMsg( mWnd, EM_QUERYREADONLY);
      mOS2Toolkit->SendMsg( mWnd, EM_SETREADONLY, MPFROMLONG(aReadOnlyFlag));
   }
   else
   {
      bOldState = mStyle & ES_READONLY;
      if( aReadOnlyFlag)
         mStyle |= ES_READONLY;
      else
         mStyle &= ~ES_READONLY;
   }

   old =  bOldState ? PR_TRUE : PR_FALSE;
   return NS_OK;
}

nsresult nsEntryField::SelectAll()
{
   SetSelection( 0, 32000);
   return NS_OK;
}

// Maybe off-by-one errors here, test & see
nsresult nsEntryField::SetSelection( PRUint32 aStartSel, PRUint32 aEndSel)
{
   mOS2Toolkit->SendMsg( mWnd, EM_SETSEL,
                         MPFROM2SHORT( (short)aStartSel, (short)aEndSel));
   return NS_OK;
}

nsresult nsEntryField::GetSelection( PRUint32 *aStartSel, PRUint32 *aEndSel)
{
   MRESULT rc = mOS2Toolkit->SendMsg( mWnd, EM_QUERYSEL);
   if( aStartSel)
      *aStartSel = SHORT1FROMMR( rc);
   if( aEndSel)
      *aEndSel = SHORT2FROMMR( rc);
   return NS_OK;
}

nsresult nsEntryField::SetCaretPosition( PRUint32 aPosition)
{
   SetSelection( aPosition, aPosition);
   return NS_OK;
}

// We're in a bit of trouble here 'cos we can't find out where the cursor
// is if there's a selection.  Oh well, do what windows does...
nsresult nsEntryField::GetCaretPosition( PRUint32 &rc)
{
   PRUint32 selStart, selEnd;
   GetSelection( &selStart, &selEnd);
   if( selStart == selEnd)
      rc = selStart;
   else
      rc = (PRUint32) -1;

   return NS_OK;
}

// platform hooks
PCSZ nsEntryField::WindowClass()
{
   return WC_ENTRYFIELD;
}

ULONG nsEntryField::WindowStyle()
{
   return mStyle | ES_MARGIN | ES_LEFT | ES_AUTOSCROLL | BASE_CONTROL_STYLE;
}

nsresult nsEntryField::PreCreateWidget( nsWidgetInitData *aInitData)
{
   if( nsnull != aInitData)
   {
      nsTextWidgetInitData *data = (nsTextWidgetInitData *) aInitData;
      if( data->mIsPassword)
         mStyle |= ES_UNREADABLE;
      if( data->mIsReadOnly)
         mStyle |= ES_READONLY;
   }
   return NS_OK;
}

ULONG nsEntryField::GetSWPFlags( ULONG flags)
{
   // add SWP_NOADJUST to stop ever-increasing entryfields
   return flags | SWP_NOADJUST;
}

void nsEntryField::PostCreateWidget()
{
   SetMaxTextLength( 0);
}
