/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsTextHelper.h"
#include "nsTextWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

#include "nsWindow.h"

#include <qlineedit.h>
#include <qmultilineedit.h>

#define DBG 0

//-------------------------------------------------------------------------
//
// nsTextHelper constructor
//
//-------------------------------------------------------------------------

nsTextHelper::nsTextHelper() : nsWidget(), nsITextWidget()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextHelper::nsTextHelper()\n"));
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
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextHelper::~nsTextHelper()\n"));
}

//-------------------------------------------------------------------------
//
// Set initial parameters
//
//-------------------------------------------------------------------------
NS_METHOD nsTextHelper::PreCreateWidget(nsWidgetInitData *aInitData)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextHelper::PreCreateWidget()\n"));
    if (nsnull != aInitData) 
    {
        nsTextWidgetInitData* data = (nsTextWidgetInitData *) aInitData;
        mIsPassword = data->mIsPassword;
        mIsReadOnly = data->mIsReadOnly;
    }
    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsTextHelper::SetMaxTextLength(PRUint32 aChars)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextHelper::SetMaxTextLength()\n"));
    if (mWidget->isA("nsQLineEdit"))
    {
        ((QLineEdit *)mWidget)->setMaxLength((int) aChars);
    }
    else
    {
        //((QMultiLineEdit *)mWidget)->setMaxLength((int) aChars);
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsTextHelper::GetText(nsString& aTextBuffer, 
                                PRUint32 aBufferSize, 
                                PRUint32& aActualSize)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextHelper::GetText()\n"));
    QString string;

    if (mWidget->isA("nsQLineEdit"))
    {
        string = ((QLineEdit *)mWidget)->text();
    }
    else
    {
        string = ((QMultiLineEdit *)mWidget)->text();
    }

    aTextBuffer.SetLength(0);
    aTextBuffer.AppendWithConversion((const char *) string);

    aActualSize = (PRUint32) string.length();

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsTextHelper::SetText(const nsString& aText, PRUint32& aActualSize)
{
    char *buf = aText.ToNewCString();

    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextHelper::SetText to \"%s\"\n",
                                       buf));

    QString string = buf;

    //mWidget->setUpdatesEnabled(PR_FALSE);
    if (mWidget->isA("nsQLineEdit"))
    {
        ((QLineEdit *)mWidget)->setText(string);
    }
    else
    {
        ((QMultiLineEdit *)mWidget)->setText(string);
    }
    //mWidget->setUpdatesEnabled(PR_TRUE);
    aActualSize = aText.Length();

    delete[] buf;

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsTextHelper::InsertText(const nsString &aText, 
                                   PRUint32 aStartPos, 
                                   PRUint32 aEndPos, 
                                   PRUint32& aActualSize)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextHelper::InsertText()\n"));
    nsString currentText;
    PRUint32 actualSize;
    GetText(currentText, 256, actualSize);
    nsString newText(aText);
    currentText.Insert(newText, aStartPos);
    SetText(currentText,actualSize);
    aActualSize = aText.Length();

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsTextHelper::RemoveText()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextHelper::RemoveText()\n"));
    if (mWidget->isA("nsQLineEdit"))
    {
        ((QLineEdit *)mWidget)->clear();
    }
    else
    {
        ((QMultiLineEdit *)mWidget)->clear();
    }

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsTextHelper::SetPassword(PRBool aIsPassword)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextHelper::SetPassword()\n"));
    if (mWidget->isA("nsQLineEdit"))
    {
        QLineEdit::EchoMode mode = aIsPassword ? 
            QLineEdit::Password : 
            QLineEdit::Normal;
        ((QLineEdit *)mWidget)->setEchoMode(mode);
    }

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsTextHelper::SetReadOnly(PRBool aReadOnlyFlag, 
                                    PRBool& aOldReadOnlyFlag)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextHelper::SetReadOnly()\n"));
    NS_ASSERTION(nsnull != mWidget,
                 "SetReadOnly - Widget is NULL, Create may not have been called!");

    aOldReadOnlyFlag = mIsReadOnly;

    mIsReadOnly = aReadOnlyFlag?PR_FALSE:PR_TRUE;

    if (mWidget->isA("nsQLineEdit"))
    {
        ((QLineEdit *)mWidget)->setEnabled(mIsReadOnly);
    }
    else
    {        
        ((QMultiLineEdit *)mWidget)->setReadOnly(mIsReadOnly);
    }

    return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsTextHelper::SelectAll()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextHelper::SelectAll()\n"));
    if (mWidget->isA("nsQLineEdit"))
    {
        ((QLineEdit *)mWidget)->selectAll();
    }
    else
    {        
        ((QMultiLineEdit *)mWidget)->selectAll();
    }

    return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsTextHelper::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextHelper::SetSelection()\n"));
    if (mWidget->isA("nsQLineEdit"))
    {
        if (aStartSel || aEndSel)
        {
            ((QLineEdit *)mWidget)->setSelection(aStartSel, aEndSel - aStartSel);
        }
        else
        {
            ((QLineEdit *)mWidget)->deselect();
        }
    }
    else
    {        
        //((QMultiLineEdit *)mWidget)->setSelection(aStartSel, 
        //                                          aEndSel - aStartSel);
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsTextHelper::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextHelper::GetSelection()\n"));
    if (mWidget->isA("nsQLineEdit"))
    {
        if (((QLineEdit *)mWidget)->hasMarkedText())
        {
            QString string = ((QLineEdit *)mWidget)->markedText();
            PRUint32 actualSize = 0;
            nsString text;

            GetText(text, 0, actualSize);
        
            PRInt32 start = text.Find((const char *)string);

            if (start >= 0)
            {
                *aStartSel = start;
                *aEndSel   = *aStartSel + string.length();
            }
            else
            {
                return NS_ERROR_FAILURE;
            }
        }
        else
        {
            *aStartSel = 0;
            *aEndSel   = 0;
        }
    }
    else
    {
        return NS_ERROR_FAILURE;
    }

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsTextHelper::SetCaretPosition(PRUint32 aPosition)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextHelper::SetCaretPosition()\n"));
    if (mWidget->isA("nsQLineEdit"))
    {
        ((QLineEdit *)mWidget)->setCursorPosition((int) aPosition);
    }
    else
    {
        // based on the index into the text field, calculate the row and
        // column.

        int row = 0;
        int col = 0;

        ((QMultiLineEdit *)mWidget)->setCursorPosition(row, col);
    }


    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsTextHelper::GetCaretPosition(PRUint32& aPosition)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsTextHelper::GetCaretPosition()\n"));
    if (mWidget->isA("nsQLineEdit"))
    {
        aPosition = (PRUint32) ((QLineEdit *)mWidget)->cursorPosition();
    }
    return NS_OK;
}
