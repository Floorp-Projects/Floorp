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

#include <gtk/gtk.h>

#include "nsTextHelper.h"
#include "nsTextWidget.h"
#include "nsString.h"

NS_IMPL_ADDREF_INHERITED(nsTextHelper, nsWidget)
NS_IMPL_RELEASE_INHERITED(nsTextHelper, nsWidget)

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
}

//-------------------------------------------------------------------------
//
// Set initial parameters
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsTextHelper::PreCreateWidget(nsWidgetInitData *aInitData)
{
  if (nsnull != aInitData) {
    nsTextWidgetInitData* data = (nsTextWidgetInitData *) aInitData;
    mIsPassword = data->mIsPassword;
    mIsReadOnly = data->mIsReadOnly;
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsTextHelper::SetMaxTextLength(PRUint32 aChars)
{
  // This is a normal entry only thing, not a text box
  gtk_entry_set_max_length(GTK_ENTRY(mTextWidget), (int)aChars);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP  nsTextHelper::GetText(nsString& aTextBuffer, PRUint32 aBufferSize, PRUint32& aActualSize)
{
  char *str = nsnull;
  if (GTK_IS_ENTRY(mTextWidget))
    {
      str = gtk_entry_get_text(GTK_ENTRY(mTextWidget));
    }
  else if (GTK_IS_TEXT(mTextWidget))
    {
      str = gtk_editable_get_chars (GTK_EDITABLE (mTextWidget), 0,
                                    gtk_text_get_length (GTK_TEXT (mTextWidget)));
    }
  aTextBuffer.SetLength(0);
  aTextBuffer.Append(str);
  PRUint32 len = (PRUint32)strlen(str);
  aActualSize = len;

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP  nsTextHelper::SetText(const nsString& aText, PRUint32& aActualSize)
{
  if (GTK_IS_ENTRY(mTextWidget)) {
    gtk_entry_set_text(GTK_ENTRY(mTextWidget),
                       (const gchar *)nsAutoCString(aText));
  } else if (GTK_IS_TEXT(mTextWidget)) {
    gtk_editable_delete_text(GTK_EDITABLE(mTextWidget), 0,
                             gtk_text_get_length(GTK_TEXT (mTextWidget)));
    gtk_text_insert(GTK_TEXT(mTextWidget),
                    nsnull, nsnull, nsnull,
                    (const char *)nsAutoCString(aText),
                    aText.Length());
  }

  aActualSize = aText.Length();

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP  nsTextHelper::InsertText(const nsString &aText,
                                    PRUint32 aStartPos,
                                    PRUint32 aEndPos,
                                    PRUint32& aActualSize)
{
  gtk_editable_insert_text(GTK_EDITABLE(mTextWidget),
                           (const gchar *)nsAutoCString(aText),
                           (gint)aText.Length(), (gint*)&aStartPos);

  aActualSize = aText.Length();

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP  nsTextHelper::RemoveText()
{
  if (GTK_IS_ENTRY(mTextWidget)) {
    gtk_entry_set_text(GTK_ENTRY(mTextWidget), "");
  } else if (GTK_IS_TEXT(mTextWidget)) {
    gtk_editable_delete_text(GTK_EDITABLE(mTextWidget), 0,
                             gtk_text_get_length(GTK_TEXT (mTextWidget)));
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP  nsTextHelper::SetPassword(PRBool aIsPassword)
{
  mIsPassword = aIsPassword?PR_FALSE:PR_TRUE;
  if (GTK_IS_ENTRY(mTextWidget)) {
    gtk_entry_set_visibility(GTK_ENTRY(mTextWidget), mIsPassword);
  }
  // this won't work for gtk_texts
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP  nsTextHelper::SetReadOnly(PRBool aReadOnlyFlag, PRBool& aOldReadOnlyFlag)
{
  NS_ASSERTION(nsnull != mTextWidget,
               "SetReadOnly - Widget is NULL, Create may not have been called!");
  aOldReadOnlyFlag = mIsReadOnly;
  mIsReadOnly = aReadOnlyFlag?PR_FALSE:PR_TRUE;
  gtk_editable_set_editable(GTK_EDITABLE(mTextWidget), mIsReadOnly);
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_IMETHODIMP nsTextHelper::SelectAll()
{
  nsString text;
  PRUint32 actualSize = 0;
  PRUint32 numChars = GetText(text, 0, actualSize);
  SetSelection(0, numChars);
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_IMETHODIMP nsTextHelper::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
  gtk_editable_select_region(GTK_EDITABLE(mTextWidget), aStartSel, aEndSel);
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_IMETHODIMP nsTextHelper::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
#if 0
  XmTextPosition left;
  XmTextPosition right;

  if (XmTextGetSelectionPosition(mTextWidget, &left, &right)) {
    *aStartSel = (PRUint32)left;
    *aEndSel   = (PRUint32)right;
  } else {
    printf("nsTextHelper::GetSelection Error getting positions\n");
    return NS_ERROR_FAILURE;
  }
#endif
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP  nsTextHelper::SetCaretPosition(PRUint32 aPosition)
{
  gtk_editable_set_position(GTK_EDITABLE(mTextWidget), aPosition);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP  nsTextHelper::GetCaretPosition(PRUint32& aPosition)
{
  aPosition = (PRUint32)GTK_EDITABLE(mTextWidget)->current_pos;
  return NS_OK;
}
