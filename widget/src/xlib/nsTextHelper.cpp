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
  aTextBuffer.SetLength(0);
  aTextBuffer.Append(mText);
  aActualSize = mText.Length();
  return NS_OK;
}

NS_METHOD  nsTextHelper::SetText(const nsString &aText, PRUint32& aActualSize)
{
  aActualSize = aText.Length();
  mText = aText;
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

nsTextHelper::nsTextHelper() : nsWidget(), nsITextWidget()
{
}

nsTextHelper::~nsTextHelper()
{
}

PRBool nsTextHelper::AutoErase()
{
  return(PR_TRUE);
}
