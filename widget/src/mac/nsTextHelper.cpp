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

#error "This file is obsolete: nsTextAreaWidget is WASTE-based / nsTextWidget is a control"

#include "nsTextHelper.h"
#include "nsTextWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

#define DBG 0

//-------------------------------------------------------------------------
//
// nsTextHelper constructor
//
//-------------------------------------------------------------------------

nsTextHelper::nsTextHelper(nsISupports *aOuter):nsITextWidget(aOuter)
{
LongRect		destRect,viewRect;
PRUint32		teFlags=0;

  mIsReadOnly = PR_FALSE;
  mIsPassword = PR_FALSE;
 	WENew(&destRect,&viewRect,teFlags,&mTE_Data);

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
void nsTextHelper::SetMaxTextLength(PRUint32 aChars)
{



}

//-------------------------------------------------------------------------
PRUint32  nsTextHelper::GetText(nsString& aTextBuffer, PRUint32 aBufferSize) 
{
PRUint32	len;

  if (!mIsPassword) 
  	{
    //char * str = XmTextGetString(mWidget);
    aTextBuffer.SetLength(0);
    //aTextBuffer.Append(str);
    //PRUint32 len = (PRUint32)strlen(str);
    //XtFree(str);
    return len;
  	} 
  else 
  	{
    PasswordData * data;
    //XtVaGetValues(mWidget, XmNuserData, &data, NULL);
    aTextBuffer = data->mPassword;
    return aTextBuffer.Length();
  }
}

//-------------------------------------------------------------------------
PRUint32  nsTextHelper::SetText(const nsString& aText)
{ 
  //printf("SetText Password %d\n", mIsPassword);
  if (!mIsPassword) 
  	{
    NS_ALLOC_STR_BUF(buf, aText, 512);
    //XmTextSetString(mWidget, buf);
    NS_FREE_STR_BUF(buf);
  	} 
  else 
  	{
    PasswordData * data;
    //(mWidget, XmNuserData, &data, NULL);
    data->mPassword = aText;
    data->mIgnore   = PR_TRUE;
    char * buf = new char[aText.Length()+1];
    memset(buf, '*', aText.Length());
    buf[aText.Length()] = 0;
    //printf("SetText [%s]  [%s]\n", data->mPassword.ToNewCString(), buf);
    //XmTextSetString(mWidget, buf);
    data->mIgnore = PR_FALSE;
  	}
  return(aText.Length());
}

//-------------------------------------------------------------------------
PRUint32  nsTextHelper::InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos)
{ 

  if (!mIsPassword) 
  	{
    NS_ALLOC_STR_BUF(buf, aText, 512);
    //XmTextInsert(mWidget, aStartPos, buf);
    NS_FREE_STR_BUF(buf);
  	} 
 	else 
 		{
    PasswordData * data;
    //XtVaGetValues(mWidget, XmNuserData, &data, NULL);
    data->mIgnore   = PR_TRUE;
    nsString newText(aText);
    data->mPassword.Insert(newText, aStartPos, aText.Length());
    char * buf = new char[data->mPassword.Length()+1];
    memset(buf, '*', data->mPassword.Length());
    buf[data->mPassword.Length()] = 0;
    //printf("SetText [%s]  [%s]\n", data->mPassword.ToNewCString(), buf);
    //XmTextInsert(mWidget, aStartPos, buf);
    data->mIgnore = PR_FALSE;
  	}
  return(aText.Length());

}

//-------------------------------------------------------------------------
void  nsTextHelper::RemoveText()
{
  char blank[2];
  blank[0] = 0;

  //XmTextSetString(mWidget, blank);
}

//-------------------------------------------------------------------------
void  nsTextHelper::SetPassword(PRBool aIsPassword)
{
  mIsPassword = aIsPassword;
}

//-------------------------------------------------------------------------
PRBool  nsTextHelper::SetReadOnly(PRBool aReadOnlyFlag)
{
  NS_ASSERTION(mWidget != nsnull, 
               "SetReadOnly - Widget is NULL, Create may not have been called!");
  PRBool oldSetting = mIsReadOnly;
  mIsReadOnly = aReadOnlyFlag;
  //XmTextSetEditable(mWidget, aReadOnlyFlag?False:True);

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
  //XmTextPosition left  = (XmTextPosition)aStartSel;
  //XmTextPosition right = (XmTextPosition)aEndSel;

  //Time time;
printf("SetSel %d %d\n", left, right);
  //XmTextSetSelection(mWidget, left, right, 0);
}


//-------------------------------------------------------------------------
void  nsTextHelper::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
  //XmTextPosition left;
  //XmTextPosition right;

	/*
  if (XmTextGetSelectionPosition(mWidget, &left, &right)) {
    printf("left %d right %d\n", left, right);
    *aStartSel = (PRUint32)left;
    *aEndSel   = (PRUint32)right;
  } else {
    printf("nsTextHelper::GetSelection Error getting positions\n");
  }
  */
}

//-------------------------------------------------------------------------
void  nsTextHelper::SetCaretPosition(PRUint32 aPosition)
{
  //XmTextSetInsertionPosition(mWidget, (XmTextPosition)aPosition);
}

//-------------------------------------------------------------------------
PRUint32  nsTextHelper::GetCaretPosition()
{
  //return (PRUint32)XmTextGetInsertionPosition(mWidget);
}


//-------------------------------------------------------------------------
PRBool nsTextHelper::AutoErase()
{
  return(PR_TRUE);
}


