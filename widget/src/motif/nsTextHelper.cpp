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

#include <Xm/Text.h>

#define DBG 0


//-------------------------------------------------------------------------
void nsTextHelper::SetMaxTextLength(PRUint32 aChars)
{
  XmTextSetMaxLength(mWidget, (int)aChars);
}

//-------------------------------------------------------------------------
PRUint32  nsTextHelper::GetText(nsString& aTextBuffer, PRUint32 aBufferSize) 
{
  char * str = XmTextGetString(mWidget);
  aTextBuffer.SetLength(0);
  aTextBuffer.Append(str);
  PRUint32 len = (PRUint32)strlen(str);
  XtFree(str);
  return len;
}

//-------------------------------------------------------------------------
PRUint32  nsTextHelper::SetText(const nsString& aText)
{ 
  if (!mIsReadOnly) {
    NS_ALLOC_STR_BUF(buf, aText, 512);
    XmTextSetString(mWidget, buf);
    NS_FREE_STR_BUF(buf);
    return(aText.Length());
  }

  return 0;
}

//-------------------------------------------------------------------------
PRUint32  nsTextHelper::InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos)
{ 
  if (!mIsReadOnly) {
    NS_ALLOC_STR_BUF(buf, aText, 512);
    XmTextInsert(mWidget, aStartPosbuf);
    NS_FREE_STR_BUF(buf);
  }
  return(0);
}

//-------------------------------------------------------------------------
void  nsTextHelper::RemoveText()
{
  char blank[2];
  blank[0] = 0;

  if (!mIsReadOnly) {
    XmTextSetString(mWidget, buf);
  }
}

//-------------------------------------------------------------------------
void  nsTextHelper::SetPassword(PRBool aIsPassword)
{
  mIsPassword = aIsPassword;
}

//-------------------------------------------------------------------------
PRBool  nsTextHelper::SetReadOnly(PRBool aReadOnlyFlag)
{
  PRBool oldSetting = mIsReadOnly;
  mIsReadOnly = aReadOnlyFlag;
  XmTextSetEditable(mWidget, aReadOnlyFlag);
  return(oldSetting);
}

  
//-------------------------------------------------------------------------
void nsTextHelper::SelectAll()
{
  nsString text;
  PRUint32 numChars = GetText(text, 0);
  SetSelection(0, numChars);
}


//-------------------------------------------------------------------------
void  nsTextHelper::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
  XmTextPosition left  = (XmTextPosition)aStartSel;
  XmTextPosition right = (XmTextPosition)aEndSel;

  Time time;

  XmTextSetSelection(mWidget, left, right, time);
}


//-------------------------------------------------------------------------
void  nsTextHelper::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
  XmTextPosition left;
  XmTextPosition right;

  XmTextGetSelectionPosition(mWidget, &left, &right);
  *aStartSel = left;
  *aEndSel   = right;
}

//-------------------------------------------------------------------------
void  nsTextHelper::SetCaretPosition(PRUint32 aPosition)
{
  XmTextSetInsertionPosition(mWidget, (XmTextPosition)aPosition);
}

//-------------------------------------------------------------------------
PRUint32  nsTextHelper::GetCaretPosition()
{
  return (PRUin32)XmTextGetInsertionPosition(mWidget);
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
PRBool nsTextHelper::AutoErase()
{
  return(PR_TRUE);
}


