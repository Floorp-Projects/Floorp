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

#ifndef nsICheckButton_h__
#define nsICheckButton_h__

// {961085F5-BD28-11d1-97EF-00609703C14E}
#define NS_ICHECKBUTTON_IID \
{ 0x961085f5, 0xbd28, 0x11d1, { 0x97, 0xef, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

#include "nsISupports.h"
#include "nsIWidget.h"


/**
 * Checkbox widget. 
 * Can show itself in a checked or unchecked state. 
 * The checkbox widget does not automatically show itself checked or unchecked when clicked on.
 */
class nsICheckButton : public nsISupports {

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

#endif  // nsICheckButton_h__

