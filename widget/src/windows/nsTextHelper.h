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
#ifndef nsTextHelper_h__
#define nsTextHelper_h__

#include "nsdefs.h"
#include "nsITextWidget.h"
#include "nsWindow.h"

/**
 * Base class for nsTextAreaWidget and nsTextWidget
 */

class nsTextHelper : public nsWindow, public nsITextWidget
{

public:
    nsTextHelper(nsISupports *aOuter);
    virtual ~nsTextHelper();
    
    virtual         PRUint32        GetText(nsString& aTextBuffer, PRUint32 aBufferSize);
    virtual         PRUint32        SetText(const nsString &aText);
    virtual         PRUint32        InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos);
    virtual         void            RemoveText();
    virtual         void            SetPassword(PRBool aIsPassword);
    virtual         PRBool          SetReadOnly(PRBool aReadOnlyFlag);
    virtual         void            SetSelection(PRUint32 aStartSel, PRUint32 aEndSel);
    virtual         void            GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel);
    virtual         void            SetCaretPosition(PRUint32 aPosition);
    virtual         PRUint32        GetCaretPosition();
    virtual         LPCTSTR         WindowClass();
    virtual         DWORD           WindowStyle();

protected:

    PRBool  mIsPassword;
    PRBool  mIsReadOnly;

    virtual DWORD           WindowExStyle();

};

#endif // nsTextHelper_h__
