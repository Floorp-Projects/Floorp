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
#ifndef nsITextWidget_h__
#define nsITextWidget_h__

#include "nsIWidget.h"
#include "nsString.h"

// {F8030011-C342-11d1-97F0-00609703C14E}
#define NS_ITEXTWIDGET_IID \
{ 0xf8030011, 0xc342, 0x11d1, { 0x97, 0xf0, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }


struct nsTextWidgetInitData : public nsWidgetInitData {
  nsTextWidgetInitData()
    : mIsPassword(PR_FALSE),
      mIsReadOnly(PR_FALSE)
  {
  }

  PRBool mIsPassword;
  PRBool mIsReadOnly;
};


/**
 *
 * Single line text editor. 
 * Unlike a nsIWidget, The text editor must automatically clear 
 * itself to the background color when paint messages are generated.
 *
 */

class nsITextWidget : public nsISupports
{

  public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITEXTWIDGET_IID)

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
     * Indicates a password will be entered. 
     *
     * @param aIsPassword PR_TRUE shows contents as asterisks. PR_FALSE shows
     * contents as normal text.
     * @result NS_Ok if no errors
     */
    
    NS_IMETHOD SetPassword(PRBool aIsPassword) = 0;

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

#endif // nsITextWidget_h__

