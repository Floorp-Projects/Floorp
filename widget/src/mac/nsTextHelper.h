/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#ifndef nsTextHelper_h__
#define nsTextHelper_h__

#error "This file is obsolete: nsTextAreaWidget is WASTE-based / nsTextWidget is a control"

#include "nsITextWidget.h"
#include "nsWindow.h"
#include "WASTE.h"

/**
 * Base class for nsTextAreaWidget and nsTextWidget
 */

class nsTextHelper :  public nsITextWidget
{

public:
    nsTextHelper();
    virtual ~nsTextHelper();

    NS_IMETHOD        SelectAll();
    NS_IMETHOD        SetMaxTextLength(PRUint32 aChars);
    NS_IMETHOD        GetText(nsString& aTextBuffer, PRUint32 aBufferSize, PRUint32& aActualSize);
    NS_IMETHOD        SetText(const nsString &aText, PRUint32& aActualSize);
    NS_IMETHOD        InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos, PRUint32& aActualSize);
    NS_IMETHOD        RemoveText();
    NS_IMETHOD        SetPassword(PRBool aIsPassword);
    NS_IMETHOD        SetReadOnly(PRBool aNewReadOnlyFlag, PRBool& aOldReadOnlyFlag);
    NS_IMETHOD        SetSelection(PRUint32 aStartSel, PRUint32 aEndSel);
    NS_IMETHOD        GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel);
    NS_IMETHOD        SetCaretPosition(PRUint32 aPosition);
    NS_IMETHOD        GetCaretPosition(PRUint32& aPosition);



    virtual PRBool    AutoErase();

protected:

    PRBool  		mIsPassword;
    PRBool  		mIsReadOnly;
   	WEReference	mTE_Data;


};

#endif // nsTextHelper_h__
