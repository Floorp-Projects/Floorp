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

#ifndef nsITextAreaWidget_h__
#define nsITextAreaWidget_h__

#include "nsIWidget.h"
#include "nsITextWidget.h"
#include "nsString.h"

// {F8030012-C342-11d1-97F0-00609703C14E}
#define NS_ITEXTAREAWIDGET_IID \
{ 0xf8030012, 0xc342, 0x11d1, { 0x97, 0xf0, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

/**
 * Multi-line text editor. 
 * See nsITextWidget for capabilities. 
 * Displays a scrollbar when the text content exceeds the number of lines 
 * displayed.
 * Unlike a nsIWidget, The textarea must automatically clear 
 * itself to the background color when paint messages are generated.
 */

class nsITextAreaWidget : public nsISupports 
{

  public:

    /**
     * Get the text of this component.
     *
     * @param aTextBuffer  on return contains the text of this component
     * @param aBufferSize  the size of the buffer passed in
     * @param aActualSize  the number of char copied
     * @result NS_Ok if no errors
     *
     */

    NS_IMETHOD GetText(nsString &aTextBuffer, PRUint32 aBufferSize, PRUint32& aActualSize) = 0;

    /**
     * Set the text of this component.
     *
     * @param   aText -- an object containing a copy of the text
     * @return  the number of chars in the text string
     * @result NS_Ok if no errors
     *
     */

    NS_IMETHOD SetText(const nsString &aText, PRUint32& aActualSize) = 0;

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
     * @result NS_Ok if no errors
     */

    NS_IMETHOD InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos, PRUint32& aActualSize) = 0;

    /**
     * Remove any content from this text widget
     * @result NS_Ok if no errors
     */

    NS_IMETHOD RemoveText(void) = 0;

    /**
     * Sets the maximum number of characters the widget can hold 
     *
     * @param aChars maximum number of characters for this widget. if 0 then there isn't any limit
     * @result NS_Ok if no errors
     */
    
    NS_IMETHOD SetMaxTextLength(PRUint32 aChars) = 0;


    /**
     * Set the text widget to be read-only
     *
     * @param  aReadOnlyFlag PR_TRUE the widget is read-only,
     *         PR_FALSE indicates the widget is writable. 
     * @param  PR_TRUE if it was read only. PR_FALSE if it was writable           
     * @result NS_Ok if no errors
     */

    NS_IMETHOD SetReadOnly(PRBool aNewReadOnlyFlag, PRBool& aOldReadOnlyFlag) = 0;
    
    /**
     * Select all of the contents
     * @result NS_Ok if no errors
     */

    NS_IMETHOD SelectAll() = 0;

    /**
     * Set the selection in this text component
     * @param aStartSel starting selection position in characters
     * @param aEndSel ending selection position in characters 
     * @result NS_Ok if no errors
     */

    NS_IMETHOD SetSelection(PRUint32 aStartSel, PRUint32 aEndSel) = 0;

    /**
     * Get the selection in this text component
     * @param aStartSel starting selection position in characters
     * @param aEndSel ending selection position in characters 
     * @result NS_Ok if no errors
     */

    NS_IMETHOD GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel) = 0;

    /**
     * Set the caret position
     * @param aPosition caret position in characters
     * @result NS_Ok if no errors
     */

    NS_IMETHOD SetCaretPosition(PRUint32 aPosition) = 0;

    /**
     * Get the caret position
     * @return caret position in characters
     * @result NS_Ok if no errors
     */

    NS_IMETHOD GetCaretPosition(PRUint32& aPosition) = 0;
};

#endif // nsITextAreaWidget_h__

