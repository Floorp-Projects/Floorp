/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsIRadioButton_h__
#define nsIRadioButton_h__

#include "nsIButton.h"

#define NS_IRADIOBUTTON_IID   \
{ 0x18032ad4, 0xb265, 0x11d2, \
{ 0xaa, 0x2a, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }       

/**
 * RadioButton widget. Can show itself in a checked or unchecked state. 
 * The RadioButton widget automatically shows itself checked or unchecked when clicked on.
 */

class nsIRadioButton : public nsISupports {

public:

   /**
    * Set the button label
    *
    * @param aText  button label
    * @result set to NS_OK if method successful
    */
  
    NS_IMETHOD SetLabel(const nsString &aText) = 0;
    
   /**
    * Get the button label
    *
    * @param aBuffer contains label upon return
    * @result set to NS_OK if method successful
    */
 
    NS_IMETHOD GetLabel(nsString &aBuffer) = 0;

    /**
     * Set the check state.
     * @param aState PR_TRUE show as checked. PR_FALSE show unchecked.
     * @result set to NS_OK if method successful
     */

    NS_IMETHOD SetState(const PRBool aState) = 0;
    
    /**
     * Get the check state.
     * @param aState PR_TRUE if checked. PR_FALSE if unchecked.
     * @result set to NS_OK if method successful
     */

    NS_IMETHOD GetState(PRBool& aState) = 0;

};

#endif  // nsIRadioButton_h__


