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
#ifndef nsITextWidget_h__
#define nsITextWidget_h__

#include "nsIWidget.h"
#include "nsString.h"

// {F8030011-C342-11d1-97F0-00609703C14E}
#define NS_ITEXTWIDGET_IID \
{ 0xf8030011, 0xc342, 0x11d1, { 0x97, 0xf0, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } };

/**
 *
 * Single line text editor. 
 *
 */

class nsITextWidget : public nsIWidget
{

  public:

    /**
     * Get the text of this component.
     *
     * @param aTextBuffer  on return contains the text of this component
     * @param aBufferSize  the size of the buffer passed in
     * @return             the number of char copied
     *
     */

    virtual PRUint32 GetText(nsString &aTextBuffer, PRUint32 aBufferSize) = 0;

    /**
     * Set the text of this component.
     *
     * @param   aTextBuffer   on return it contains the text contents.
     * @return  the number of chars in the text string
     *
     */

    virtual PRUint32 SetText(const nsString &aText) = 0;

    /**
     * Insert text into this component.
     * When aStartPos and aEndPos are a valid range this function performs a replace.
     * When aStartPos and aEndPos are equal this function performs an insert.
     * When aStartPos and aEndPos are both -1 (0xFFFFFFFF) this function performs an append.
     * If aStartPos and aEndPos are out of range they are rounded to the closest end.
     *
     * @param   aText     the text to set
     * @param   aStartPos starting position for inserting text
     * @param   aEndPos   ending position for inserting text
     */

    virtual PRUint32 InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos) = 0;

    /**
     * Remove any content from this text widget
     */

    virtual void RemoveText() = 0;

    /**
     * Indicates a password will be entered. 
     *
     * @param aIsPassword PR_TRUE shows contents as asterisks. PR_FALSE shows
     * contents as normal text.
     */
    
    virtual void SetPassword(PRBool aIsPassword) = 0;

    /**
     * Sets the maximum number of characters the widget can hold 
     *
     * @param aChars maximum number of characters for this widget. if 0 then there isn't any limit
     */
    
    virtual void SetMaxTextLength(PRUint32 aChars) = 0;


    /**
     * Set the text widget to be read-only
     *
     * @param      aReadOnlyFlag PR_TRUE the widget is read-only,
     *             PR_FALSE indicates the widget is writable. 
     * @return     PR_TRUE if it was read only. PR_FALSE if it was writable           
     */

    virtual PRBool SetReadOnly(PRBool aReadOnlyFlag) = 0;

    /**
     * Set the selection in this text component
     * @param aStartSel starting selection position in characters
     * @param aEndSel ending selection position in characters 
     */

    virtual void SetSelection(PRUint32 aStartSel, PRUint32 aEndSel) = 0;

    /**
     * Get the selection in this text component
     * @param aStartSel starting selection position in characters
     * @param aEndSel ending selection position in characters 
     */

    virtual void GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel) = 0;

    /**
     * Set the caret position
     * @param aPosition caret position in characters
     */

    virtual void SetCaretPosition(PRUint32 aPosition) = 0;

    /**
     * Get the caret position
     * @return caret position in characters
     */

    virtual PRUint32 GetCaretPosition() = 0;

};

#endif // nsITextWidget_h__

