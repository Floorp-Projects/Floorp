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
#include "nsTextWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

#include <Xm/Text.h>
#include "nslog.h"

NS_IMPL_LOG(nsTextHelperLog, 0)
#define PRINTF NS_LOG_PRINTF(nsTextHelperLog)
#define FLUSH  NS_LOG_FLUSH(nsTextHelperLog)

#define DBG 0

//-------------------------------------------------------------------------
//
// nsTextHelper constructor
//
//-------------------------------------------------------------------------

nsTextHelper::nsTextHelper() : nsWindow(), nsITextAreaWidget(), nsITextWidget()
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
NS_METHOD nsTextHelper::SetMaxTextLength(PRUint32 aChars)
{
  XmTextSetMaxLength(mWidget, (int)aChars);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD  nsTextHelper::GetText(nsString& aTextBuffer, PRUint32 aBufferSize, PRUint32& aActualSize) 
{
  if (!mIsPassword) {
    char * str = XmTextGetString(mWidget);
    aTextBuffer.SetLength(0);
    aTextBuffer.Append(str);
    PRUint32 len = (PRUint32)strlen(str);
    XtFree(str);
    aActualSize = len;
  } else {
    PasswordData * data;
    XtVaGetValues(mWidget, XmNuserData, &data, NULL);
    aTextBuffer = data->mPassword;
    aActualSize = aTextBuffer.Length();
  }
  return(NS_OK);
}

//-------------------------------------------------------------------------
NS_METHOD  nsTextHelper::SetText(const nsString& aText, PRUint32& aActualSize)
{ 
  if (!mIsPassword) {
    NS_ALLOC_STR_BUF(buf, aText, 512);
    XmTextSetString(mWidget, buf);
    NS_FREE_STR_BUF(buf);
  } else {
    PasswordData * data;
    XtVaGetValues(mWidget, XmNuserData, &data, NULL);
    data->mPassword = aText;
    data->mIgnore   = True;
    char * buf = new char[aText.Length()+1];
    memset(buf, '*', aText.Length());
    buf[aText.Length()] = 0;
    XmTextSetString(mWidget, buf);
    data->mIgnore = False;
  }
  aActualSize = aText.Length();
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD  nsTextHelper::InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos, PRUint32& aActualSize)
{ 

  if (!mIsPassword) {
    NS_ALLOC_STR_BUF(buf, aText, 512);
    XmTextInsert(mWidget, aStartPos, buf);
    NS_FREE_STR_BUF(buf);
  } else {
    PasswordData * data;
    XtVaGetValues(mWidget, XmNuserData, &data, NULL);
    data->mIgnore   = True;
    nsString newText(aText);
    data->mPassword.Insert(newText, aStartPos, aText.Length());
    char * buf = new char[data->mPassword.Length()+1];
    memset(buf, '*', data->mPassword.Length());
    buf[data->mPassword.Length()] = 0;
    XmTextInsert(mWidget, aStartPos, buf);
    data->mIgnore = False;
  }
  aActualSize = aText.Length();
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD  nsTextHelper::RemoveText()
{
  char blank[2];
  blank[0] = 0;

  XmTextSetString(mWidget, blank);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD  nsTextHelper::SetPassword(PRBool aIsPassword)
{
  mIsPassword = aIsPassword;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD  nsTextHelper::SetReadOnly(PRBool aReadOnlyFlag, PRBool& aOldReadOnlyFlag)
{
  NS_ASSERTION(nsnull != mWidget, 
               "SetReadOnly - Widget is NULL, Create may not have been called!");
  aOldReadOnlyFlag = mIsReadOnly;
  mIsReadOnly = aReadOnlyFlag;
  XmTextSetEditable(mWidget, aReadOnlyFlag?False:True);
  return NS_OK;
}

  
//-------------------------------------------------------------------------
NS_METHOD nsTextHelper::SelectAll()
{
  nsString text;
  PRUint32 actualSize = 0;
  PRUint32 numChars = GetText(text, 0, actualSize);
  SetSelection(0, numChars);
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsTextHelper::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
  XmTextPosition left  = (XmTextPosition)aStartSel;
  XmTextPosition right = (XmTextPosition)aEndSel;

  XmTextSetSelection(mWidget, left, right, 0);
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsTextHelper::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
  XmTextPosition left;
  XmTextPosition right;

  if (XmTextGetSelectionPosition(mWidget, &left, &right)) {
    *aStartSel = (PRUint32)left;
    *aEndSel   = (PRUint32)right;
  } else {
    PRINTF("nsTextHelper::GetSelection Error getting positions\n");
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD  nsTextHelper::SetCaretPosition(PRUint32 aPosition)
{
  XmTextSetInsertionPosition(mWidget, (XmTextPosition)aPosition);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD  nsTextHelper::GetCaretPosition(PRUint32& aPosition)
{
  aPosition = (PRUint32)XmTextGetInsertionPosition(mWidget);
  return NS_OK;
}




