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

#include "nsTextArea.h"

NS_IMPL_ADDREF(nsTextArea)
NS_IMPL_RELEASE(nsTextArea)
nsresult nsTextArea::QueryInterface( const nsIID &aIID, void **aInstancePtr)
{
   nsresult result = nsWindow::QueryInterface( aIID, aInstancePtr);

   if( result == NS_NOINTERFACE && aIID.Equals( NS_GET_IID(nsITextAreaWidget)))
   {
      *aInstancePtr = (void*) ((nsITextAreaWidget*)this);
      NS_ADDREF_THIS();
      result = NS_OK;
   }

   return result;
}

// MLE notes: in Warp 4 (and I guess Warp3 fp 28-ish) MLE's *do* respond
//            correctly to colour-changes via presparams.
//
//            Should really add code to deal with text lengths > 64k.
//
//            No DBCS support.
//
// Format is currently left as MLFIE_CFTEXT; need to wait & see what it
// should be.  MLFIE_WINFMT could well be the right thing to do; set this
// using the PostCreateWidget() hook.
//
// May also need strategic MLM_DISABLEREFRESH

nsresult nsTextArea::GetText( nsString &aTextBuffer, PRUint32 aBufferSize, PRUint32 &len)
{
   if( mToolkit != nsnull)
   {
      IPT clength = (IPT) mOS2Toolkit->SendMsg( mWnd, MLM_QUERYTEXTLENGTH);
      // now get bytes
      long length = (long) mOS2Toolkit->SendMsg( mWnd, MLM_QUERYFORMATTEXTLENGTH,
                                                 0, MPFROMLONG(length));
      // ..buffer..
      NS_ALLOC_CHAR_BUF(buf,256,length+1)
   
      mOS2Toolkit->SendMsg( mWnd, MLM_SETIMPORTEXPORT,
                            MPFROMP(buf), MPFROMLONG(length+1));
      IPT start = 0;
      mOS2Toolkit->SendMsg( mWnd, MLM_EXPORT, MPFROMP(&start), MPFROMP(&length));
   
      aTextBuffer.SetLength(0);
      aTextBuffer.AppendWithConversion(buf);
   
      NS_FREE_CHAR_BUF(buf)
   
      len = clength;
   }
   else
   {
      aTextBuffer = mText;
      len = aTextBuffer.Length();
   }

   return NS_OK;
}

nsresult nsTextArea::SetText( const nsString &aText, PRUint32 &len)
{
   if( mOS2Toolkit != nsnull)
   {
      RemoveText();
   
      NS_ALLOC_STR_BUF(buf,aText,512)
   
      mOS2Toolkit->SendMsg( mWnd, MLM_SETIMPORTEXPORT,
                            MPFROMP(buf), MPFROMLONG(aText.Length() + 1));
      IPT ipt = 0;
      mOS2Toolkit->SendMsg( mWnd, MLM_IMPORT,
                            MPFROMP(&ipt), MPFROMLONG(aText.Length()));
   
      NS_FREE_STR_BUF(buf)
   }

   len = aText.Length();
   mText = aText;

   return NS_OK;
}

nsresult nsTextArea::InsertText( const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos, PRUint32 &len)
{
   PRUint32 dummy;
   nsString currentText;
   GetText( currentText, 256, dummy);
   nsString newText( aText);
   currentText.Insert( newText, aStartPos);
   SetText( currentText, dummy);
   len = aText.Length();
   return NS_OK;
}

nsresult nsTextArea::RemoveText()
{
   if( mOS2Toolkit != nsnull)
   {
      IPT length = (IPT) mOS2Toolkit->SendMsg( mWnd, MLM_QUERYTEXTLENGTH);
      mOS2Toolkit->SendMsg( mWnd, MLM_DELETE, MPFROMLONG(0), MPFROMLONG(length));
   }
   return NS_OK;
}

nsresult nsTextArea::SetPassword( PRBool aIsPassword)
{
   // no...
   return NS_OK;
}

nsresult nsTextArea::SetMaxTextLength( PRUint32 aChars)
{
   if( mToolkit != nsnull)
   {
      long lSize = aChars ? (long)aChars : -1;
      mOS2Toolkit->SendMsg( mWnd, MLM_SETTEXTLIMIT, MPFROMLONG(lSize));
   }

   return NS_OK;
}

nsresult nsTextArea::SetReadOnly( PRBool aReadOnlyFlag, PRBool &bOld)
{
   BOOL bOldState;
   if( mWnd)
   {
      bOldState = (BOOL) mOS2Toolkit->SendMsg( mWnd, MLM_QUERYREADONLY);
      mOS2Toolkit->SendMsg( mWnd, MLM_SETREADONLY, MPFROMSHORT(aReadOnlyFlag));
   }
   else
   {
      bOldState = mStyle & MLS_READONLY;
      if( aReadOnlyFlag)
         mStyle |= MLS_READONLY;
      else
         mStyle &= ~MLS_READONLY;
   }
   bOld = bOldState ? PR_TRUE : PR_FALSE;
   return NS_OK;
}

nsresult nsTextArea::SelectAll()
{
   if( mToolkit != nsnull)
   {
      IPT length = (IPT) mOS2Toolkit->SendMsg( mWnd, MLM_QUERYTEXTLENGTH);
      SetSelection( 0, length);
   }

   return NS_OK;
}

nsresult nsTextArea::SetSelection( PRUint32 aStartSel, PRUint32 aEndSel)
{
   if( mToolkit != nsnull)
      mOS2Toolkit->SendMsg( mWnd, MLM_SETSEL,
                            MPFROMLONG(aStartSel), MPFROMLONG(aEndSel));
   return NS_OK;
}

nsresult nsTextArea::GetSelection( PRUint32 *aStartSel, PRUint32 *aEndSel)
{
   if( mToolkit != nsnull)
   {
      MRESULT rc = mOS2Toolkit->SendMsg( mWnd, MLM_QUERYSEL,
                                         MPFROMLONG(MLFQS_MINMAXSEL));
      if( aStartSel)
         *aStartSel = SHORT1FROMMR(rc);
      if( aEndSel)
         *aEndSel = SHORT2FROMMR(rc);
   }

   return NS_OK;
}

nsresult nsTextArea::SetCaretPosition( PRUint32 aPosition)
{
   if( mToolkit != nsnull)
      mOS2Toolkit->SendMsg( mWnd, MLM_SETSEL,
                            MPFROMLONG(aPosition), MPFROMLONG(aPosition));
   return NS_OK;
}

nsresult nsTextArea::GetCaretPosition( PRUint32 &ret)
{
   if( mToolkit != nsnull)
   {
      MRESULT rc = mOS2Toolkit->SendMsg( mWnd, MLM_QUERYSEL,
                                         MPFROMLONG(MLFQS_CURSORSEL));
      ret = (PRUint32) rc;
   }

   return NS_OK;
}

// platform hooks
PCSZ nsTextArea::WindowClass()
{
   return WC_MLE;
}

ULONG nsTextArea::WindowStyle()
{  // no wordwrap, intentional; should be in interface...
   return mStyle | MLS_BORDER | MLS_HSCROLL | MLS_VSCROLL | BASE_CONTROL_STYLE;
}

nsresult nsTextArea::PreCreateWidget( nsWidgetInitData *aInitData)
{
   if( nsnull != aInitData)
   {
      nsTextWidgetInitData *data = (nsTextWidgetInitData *) aInitData;
      if( data->mIsReadOnly)
         mStyle |= MLS_READONLY;
   }
   return NS_OK;
}
