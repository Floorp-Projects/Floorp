/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#ifndef _nstextwidget_h
#define _nstextwidget_h

#include "nswindow.h"
#include "nsITextWidget.h"

// WC_ENTRYFIELD wrapper, for NS_TEXTFIELD_CID

class nsEntryField : public nsWindow, public nsITextWidget
{
 public:
   nsEntryField() : nsWindow(), mStyle( 0) {}

   // nsISupports
   NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);                           
   NS_IMETHOD_(nsrefcnt) AddRef(void);                                       
   NS_IMETHOD_(nsrefcnt) Release(void);          

   // nsITextWidget
   NS_IMETHOD GetText( nsString &aTextBuffer, PRUint32 aBufferSize, PRUint32 &);
   NS_IMETHOD SetText( const nsString &aText, PRUint32 &);
   NS_IMETHOD InsertText( const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos, PRUint32 &);
   NS_IMETHOD RemoveText();
   NS_IMETHOD SetPassword( PRBool aIsPassword);
   NS_IMETHOD SetMaxTextLength( PRUint32 aChars);
   NS_IMETHOD SetReadOnly( PRBool aReadOnlyFlag, PRBool &);
   NS_IMETHOD SelectAll();
   NS_IMETHOD SetSelection( PRUint32 aStartSel, PRUint32 aEndSel);
   NS_IMETHOD GetSelection( PRUint32 *aStartSel, PRUint32 *aEndSel);
   NS_IMETHOD SetCaretPosition( PRUint32 aPosition);
   NS_IMETHOD GetCaretPosition( PRUint32 &pos);

   // platform hooks
   virtual PCSZ  WindowClass();
   virtual ULONG WindowStyle();
   NS_IMETHOD    PreCreateWidget( nsWidgetInitData *aInitData);
   virtual ULONG GetSWPFlags( ULONG flags);
   virtual void  PostCreateWidget();

 protected:
   ULONG    mStyle;
   nsString mText;
};

#endif
