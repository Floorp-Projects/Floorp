/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsTextHelper.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"
#include <windows.h>


void nsTextHelper::PreCreateWidget(void *aInitData)
{
  if (nsnull != aInitData) {
    nsWidgetInitData* data = (nsWidgetInitData *) aInitData;
    mIsPassword = (0 == data->arg1) ? PR_FALSE : PR_TRUE;
  }
}

void nsTextHelper::SetMaxTextLength(PRUint32 aChars)
{
  ::SendMessage(mWnd, EM_SETLIMITTEXT, (WPARAM) (INT)aChars, 0);
}

PRUint32  nsTextHelper::GetText(nsString& aTextBuffer, PRUint32 aBufferSize) {

  int length = GetWindowTextLength(mWnd);
  int bufLength = length + 1;
  NS_ALLOC_CHAR_BUF(buf, 512, bufLength);
  int charsCopied = GetWindowText(mWnd, buf, bufLength);
  aTextBuffer.SetLength(0);
  aTextBuffer.Append(buf);
  NS_FREE_CHAR_BUF(buf);

  return(0);
}

PRUint32  nsTextHelper::SetText(const nsString &aText)
{ 
  NS_ALLOC_STR_BUF(buf, aText, 512);
  SetWindowText(mWnd, buf);
  NS_FREE_STR_BUF(buf);
  return(aText.Length());
}

PRUint32  nsTextHelper::InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos)
{ 
   // NOT IMPLEMENTED
  return(0);
}

void  nsTextHelper::RemoveText()
{
  SetWindowText(mWnd, "");
}

void  nsTextHelper::SetPassword(PRBool aIsPassword)
{
  mIsPassword = aIsPassword;
}

PRBool  nsTextHelper::SetReadOnly(PRBool aReadOnlyFlag)
{
  PRBool oldSetting = mIsReadOnly;
  mIsReadOnly = aReadOnlyFlag;
  return(oldSetting);
}

void  nsTextHelper::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
  ::SendMessage(mWnd, EM_SETSEL, (WPARAM) (INT)aStartSel, (INT) (LPDWORD)aEndSel); 
}


void  nsTextHelper::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
  ::SendMessage(mWnd, EM_GETSEL, (WPARAM) (LPDWORD)aStartSel, (LPARAM) (LPDWORD)aEndSel); 
}

void  nsTextHelper::SetCaretPosition(PRUint32 aPosition)
{
}

PRUint32  nsTextHelper::GetCaretPosition()
{
  return(0);
}

//-------------------------------------------------------------------------
//
// nsTextHelper constructor
//
//-------------------------------------------------------------------------

nsTextHelper::nsTextHelper(nsISupports *aOuter) : nsWindow(aOuter)
{
  mIsReadOnly = PR_FALSE;
  mIsPassword = PR_FALSE;
}

//-------------------------------------------------------------------------
//
// nsTextHelper destructor
//
//-------------------------------------------------------------------------
nsTextHelper::~nsTextHelper()
{
}

//-------------------------------------------------------------------------
//
// return the window class name and initialize the class if needed
//
//-------------------------------------------------------------------------
LPCTSTR nsTextHelper::WindowClass()
{
    return("EDIT");
}

//-------------------------------------------------------------------------
//
// return window styles
//
//-------------------------------------------------------------------------
DWORD nsTextHelper::WindowStyle()
{
    DWORD style = ES_AUTOHSCROLL | WS_BORDER | WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | ES_NOHIDESEL;
    if (mIsPassword)
      style = style | ES_PASSWORD;

    if (mIsReadOnly)
      style = style | ES_READONLY;
  
    return style; 
}

//-------------------------------------------------------------------------
//
// return window extended styles
//
//-------------------------------------------------------------------------

DWORD nsTextHelper::WindowExStyle()
{
    return 0;
}


