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

#ifndef _nsRadioButton_h
#define _nsRadioButton_h

#include "nsWindow.h"
#include "nsIRadioButton.h"

// WC_BUTTON, BS_RADIOBUTTON wrapper for NS_RADIOBUTTON_CID
//
// !! Really want to share code with (at least) nsCheckButton, but there's
// !! so little of it a `nsBaseSelectionButton' seems like overkill.
// !! Maybe if this 'GetPreferredSize' thing get's going it would be better.

class nsRadioButton : public nsWindow, public nsIRadioButton
{
 public:
   nsRadioButton() {}

   // nsISupports
   NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);                           
   NS_IMETHOD_(nsrefcnt) AddRef(void);                                       
   NS_IMETHOD_(nsrefcnt) Release(void);          

   // nsIRadioButton
   NS_DECL_LABEL
   NS_IMETHOD GetState( PRBool &aState);
   NS_IMETHOD SetState( const PRBool aState);

 protected:
   // message stopping
   virtual PRBool OnMove( PRInt32 aX, PRInt32 aY) { return PR_FALSE; }
   virtual PRBool OnResize( PRInt32 aX, PRInt32 aY)  { return PR_FALSE; }

   // Creation hooks
   virtual PCSZ    WindowClass();
   virtual ULONG   WindowStyle();
};

#endif
