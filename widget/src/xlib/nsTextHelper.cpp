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

NS_METHOD nsTextHelper::PreCreateWidget(nsWidgetInitData *aInitData)
{
  return NS_OK;
}

NS_METHOD nsTextHelper::SetMaxTextLength(PRUint32 aChars)
{
  return NS_OK;
}

NS_METHOD  nsTextHelper::GetText(nsString& aTextBuffer, PRUint32 aBufferSize, PRUint32& aActualSize)
{
  return NS_OK;
}

NS_METHOD  nsTextHelper::SetText(const nsString &aText, PRUint32& aActualSize)
{ 
  return NS_OK;
}

NS_METHOD  nsTextHelper::InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos, PRUint32& aActualSize)
{
  return NS_OK;
}

NS_METHOD  nsTextHelper::RemoveText()
{
  return NS_OK;
}

NS_METHOD  nsTextHelper::SetPassword(PRBool aIsPassword)
{
  return NS_OK;
}

NS_METHOD nsTextHelper::SetReadOnly(PRBool aReadOnlyFlag, PRBool& aOldFlag)
{
  return NS_OK;
}

NS_METHOD nsTextHelper::SelectAll()
{
  return NS_OK;
}

NS_METHOD  nsTextHelper::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
  return NS_OK;
}

NS_METHOD  nsTextHelper::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
  return NS_OK;
}

NS_METHOD  nsTextHelper::SetCaretPosition(PRUint32 aPosition)
{
  return NS_OK;
}

NS_METHOD  nsTextHelper::GetCaretPosition(PRUint32& aPos)
{
  return NS_OK;
}

nsTextHelper::nsTextHelper() : nsWidget(), nsITextAreaWidget(), nsITextWidget()
{
}

nsTextHelper::~nsTextHelper()
{
}

PRBool nsTextHelper::AutoErase()
{
  return(PR_TRUE);
}
