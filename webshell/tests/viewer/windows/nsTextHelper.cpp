/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsTextHelper.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"
#include <windows.h>


NS_METHOD nsTextHelper::PreCreateWidget(nsWidgetInitData *aInitData)
{
  if (nsnull != aInitData) {
    nsTextWidgetInitData* data = (nsTextWidgetInitData *) aInitData;
    mIsPassword = data->mIsPassword;
    mIsReadOnly = data->mIsReadOnly;
  }
  return NS_OK;
}

NS_METHOD nsTextHelper::SetMaxTextLength(PRUint32 aChars)
{
  ::SendMessage(mWnd, EM_SETLIMITTEXT, (WPARAM) (INT)aChars, 0);
  return NS_OK;
}

NS_METHOD  nsTextHelper::GetText(nsString& aTextBuffer, PRUint32 aBufferSize, PRUint32& aActualSize) {

  int length = GetWindowTextLength(mWnd);
  int bufLength = length + 1;
  NS_ALLOC_CHAR_BUF(buf, 512, bufLength);
  int charsCopied = GetWindowText(mWnd, buf, bufLength);
  aTextBuffer.SetLength(0);
  aTextBuffer.AppendWithConversion(buf);
  NS_FREE_CHAR_BUF(buf);
  aActualSize = charsCopied;
  return NS_OK;
}

NS_METHOD  nsTextHelper::SetText(const nsString &aText, PRUint32& aActualSize)
{ 
  mText = aText;

  NS_ALLOC_STR_BUF(buf, aText, 512);
  SetWindowText(mWnd, buf);
  NS_FREE_STR_BUF(buf);
  aActualSize = aText.Length();
  return NS_OK;
}

NS_METHOD  nsTextHelper::InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos, PRUint32& aActualSize)
{ 
  nsString currentText;

  PRUint32 actualSize;
  GetText(currentText, 256, actualSize);
  nsString newText(aText);
  currentText.Insert(newText.GetUnicode(), aStartPos, aText.Length());
  SetText(currentText,actualSize);
  aActualSize = aText.Length();

  mText = currentText;

  return NS_OK;
}
NS_METHOD  nsTextHelper::RemoveText()
{
  SetWindowText(mWnd, "");
  return NS_OK;
}
NS_METHOD  nsTextHelper::SetPassword(PRBool aIsPassword)
{
  mIsPassword = aIsPassword;
  return NS_OK;
}
NS_METHOD nsTextHelper::SetReadOnly(PRBool aReadOnlyFlag, PRBool& aOldFlag)
{
  aOldFlag = mIsReadOnly;
  mIsReadOnly = aReadOnlyFlag;
  // Update the widget
  ::SendMessage(mWnd, EM_SETREADONLY, (WPARAM) (BOOL)aReadOnlyFlag, (LPARAM)0);
  return NS_OK;
}
  
NS_METHOD nsTextHelper::SelectAll()
{
  ::SendMessage(mWnd, EM_SETSEL, (WPARAM) 0, (LPARAM)-1);
  return NS_OK;
}

NS_METHOD  nsTextHelper::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
  ::SendMessage(mWnd, EM_SETSEL, (WPARAM) (INT)aStartSel, (INT) (LPDWORD)aEndSel); 
  return NS_OK;
}


NS_METHOD  nsTextHelper::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
  ::SendMessage(mWnd, EM_GETSEL, (WPARAM) (LPDWORD)aStartSel, (LPARAM) (LPDWORD)aEndSel); 
  return NS_OK;
}

NS_METHOD  nsTextHelper::SetCaretPosition(PRUint32 aPosition)
{
  SetSelection(aPosition, aPosition);
  return NS_OK;
}

NS_METHOD  nsTextHelper::GetCaretPosition(PRUint32& aPos)
{
  PRUint32 start;
  PRUint32 end;
  GetSelection(&start, &end);
  if (start == end) {
    aPos = start;
  }
  else {
    aPos =  PRUint32(-1);/* XXX is this right??? scary cast! */
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsTextHelper constructor
//
//-------------------------------------------------------------------------

nsTextHelper::nsTextHelper() : nsWindow(), nsITextWidget()
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
    DWORD style = ES_AUTOHSCROLL | WS_BORDER | WS_CHILD | WS_CLIPSIBLINGS | ES_NOHIDESEL;
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

//-------------------------------------------------------------------------
//
// Clear window before paint
//
//-------------------------------------------------------------------------

PRBool nsTextHelper::AutoErase()
{
  return(PR_TRUE);
}


