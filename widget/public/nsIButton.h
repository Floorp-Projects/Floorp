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

#ifndef nsIButton_h__
#define nsIButton_h__

#include "nsIWidget.h"
#include "nsString.h"

// {18032AD0-B265-11d1-AA2A-000000000000}
#define NS_IBUTTON_IID      \
{ 0x18032ad0, 0xb265, 0x11d1, \
    { 0xaa, 0x2a, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }

/* 
 * Push button widget.
 * Automatically shows itself as depressed when clicked on.
 */

class nsIButton : public nsIWidget {

public:
 /**
  * Set the button label
  * @param aText  button label
  **/
  virtual void SetLabel(const nsString &aText) = 0;
    
 /**
  * Get the button label
  * @param aBuffer buffer for button label
  **/
  virtual void GetLabel(nsString &aBuffer) = 0;

};

#endif
