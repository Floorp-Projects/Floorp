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

#include <Pt.h>
#include "nsPhWidgetLog.h"

NS_METHOD nsTextHelper::PreCreateWidget(nsWidgetInitData *aInitData)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsTextHelper::PreCreateWidget\n"));
  
  if (nsnull != aInitData)
  {
    nsTextWidgetInitData* data = (nsTextWidgetInitData *) aInitData;
    mIsPassword = data->mIsPassword;
    mIsReadOnly = data->mIsReadOnly;
  }

  return NS_OK;
}

NS_METHOD nsTextHelper::SetMaxTextLength(PRUint32 aChars)
{
  PtArg_t arg[2];
  
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsTextHelper::SetMaxLength to %d\n", aChars));

  if (mWidget)
  {
    PtSetArg(&arg[0], Pt_ARG_MAX_LENGTH, aChars, 0);
    PtSetResources(mWidget, 1, arg);
  }  

  return NS_OK;
}

NS_METHOD  nsTextHelper::GetText(nsString& aTextBuffer, PRUint32 aBufferSize, PRUint32& aActualSize)
{
 PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsTextHelper::GetText\n"));

 PtArg_t arg[2];
 int length;
 char *string;
    

  if (mWidget)
  {
    PtSetArg(&arg[0], Pt_ARG_TEXT_STRING, &string, 0);
    PtGetResources(mWidget, 1, arg);

	aTextBuffer.SetLength(0);
    aTextBuffer.Append(string);
    aActualSize = aTextBuffer.Length();
  }
  
  return NS_OK;
}

NS_METHOD  nsTextHelper::SetText(const nsString &aText, PRUint32& aActualSize)
{ 
 PtArg_t arg[2];

  mText = aText;

  if (mWidget)
  {
    NS_ALLOC_STR_BUF(buf, aText, aText.Length());

    PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsTextHelper::SetText to <%s> for <%p>\n", buf, mWidget));

    PtSetArg(&arg[0], Pt_ARG_TEXT_STRING, buf, 0);
    PtSetResources(mWidget, 1, arg);

    NS_FREE_STR_BUF(buf);
  }

  aActualSize = aText.Length();
  return NS_OK;
}

NS_METHOD  nsTextHelper::InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos, PRUint32& aActualSize)
{ 
 PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsTextHelper::InsertText from %d to %d\n", aStartPos, aEndPos));

 PtArg_t   arg[2];
 int       ret;
 nsString  currentText;
 PRUint32  currentTextLength;
  if (mWidget)
  {
    NS_ALLOC_STR_BUF(buf, aText, aText.Length());
	ret=PtTextModifyText(mWidget,0,0,aStartPos,buf,aText.Length());
	if (!ret)
 	{
      PR_LOG(PhWidLog, PR_LOG_ERROR,("nsTextHelper::InsertText failed in call to PtTextModifyText\n"));
	} 

    NS_FREE_STR_BUF(buf);
  }
  aActualSize = aText.Length();

  /* Re-get the text and store in the local variable mText */
  GetText(currentText, 0, currentTextLength);
  mText = currentText;
  
  return NS_OK;
}

NS_METHOD  nsTextHelper::RemoveText()
{
 PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsTextHelper::RemoveText\n"));
 PtArg_t arg[2];

  mText.SetLength(0);

  if (mWidget)
  {
    PtSetArg(&arg[0], Pt_ARG_TEXT_STRING, "", 0);
    PtSetResources(mWidget, 1, arg);
  }

  return NS_OK;
}

NS_METHOD  nsTextHelper::SetPassword(PRBool aIsPassword)
{
  mIsPassword = aIsPassword;
  return NS_OK;
}

NS_METHOD nsTextHelper::SetReadOnly(PRBool aReadOnlyFlag, PRBool& aOldFlag)
{
 PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsTextHelper::SetReadOnnly %d %d\n", aReadOnlyFlag, mIsReadOnly));

 PtArg_t arg[2];
 int	 temp;
 
  aOldFlag = mIsReadOnly;
  mIsReadOnly = aReadOnlyFlag;

  // Update the widget
  if (mWidget)
  {
    if (mIsReadOnly)
        temp = 0;
	else
	    temp = 1;

    PtSetArg(&arg[0], Pt_ARG_TEXT_FLAGS, temp, Pt_EDITABLE);
    PtSetResources(mWidget, 1, arg);
  }

  return NS_OK;
}
  
NS_METHOD nsTextHelper::SelectAll()
{
 PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsTextHelper::SelectAll\n"));

 int start, end;
 
  if (mWidget)
  {
    start = 0;
	end = SHRT_MAX;
    PtTextSetSelection(mWidget, &start, &end);
  }

  return NS_OK;
}

NS_METHOD  nsTextHelper::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsTextHelper::SetSelection %d to %d\n", aStartSel, aEndSel));

/* The text widget is 0 based! */

  if (mWidget)
  {
    int start, end, err=0;
    start = aStartSel;
    end = aEndSel;

    err=PtTextSetSelection(mWidget, &start, &end);
    PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsTextHelper::SetSelection after err=%d start=%d end=%d\n", err, start, end));
  }

  return NS_OK;
}


NS_METHOD  nsTextHelper::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
/*revisit not sure if this is 1 or 0 based! */

  if (mWidget)
  {
    int start, end;
	
    PtTextGetSelection(mWidget, &start, &end);

    *aStartSel = start;
    *aEndSel = end;	
	
  }

  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsTextHelper::GetSelection aStartSel=<%d> aEndSel=<%d>\n", *aStartSel, *aEndSel));

  return NS_OK;
}

NS_METHOD  nsTextHelper::SetCaretPosition(PRUint32 aPosition)
{
 PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsTextHelper::SetCaretPosition at %d", aPosition));

 PtArg_t arg[2];

  if (mWidget)
  {
    short CursPos = aPosition;
	
    PtSetArg(&arg[0], Pt_ARG_CURSOR_POSITION, CursPos, 0);
    PtSetResources(mWidget, 1, arg);
  }

  return NS_OK;
}

NS_METHOD  nsTextHelper::GetCaretPosition(PRUint32& aPos)
{
 PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsTextHelper::GetCaretPosition\n"));

 PtArg_t arg[2];
 short *CaretPosition;
 
  if (mWidget)
  {
    PtSetArg(&arg[0], Pt_ARG_CURSOR_POSITION, &CaretPosition, 0);
    PtGetResources(mWidget, 1, arg);
  }

  aPos = PRUint32(*CaretPosition);
  
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsTextHelper::GetCaretPosition aPos=<%d>\n", aPos));

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// nsTextHelper constructor
//
//-------------------------------------------------------------------------

nsTextHelper::nsTextHelper() : nsWidget(), nsITextAreaWidget(), nsITextWidget()
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
 PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsTextHelper::~nsTextHelper Destructor called for <%p>\n", mWidget));
}
