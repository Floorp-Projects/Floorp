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

#ifndef nsIDialog_h__
#define nsIDialog_h__

#include "nsIWidget.h"

// {4A781D61-3D28-11d2-8DB8-00609703C14E}
#define NS_IDIALOG_IID \
{ 0x4a781d61, 0x3d28, 0x11d2, \
{ 0x8d, 0xb8, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }


/**
 * The base class for all the widgets. It provides the interface for
 * all basic and necessary functionality.
 */
class nsIDialog : public nsIWidget {

  public:

   /**
    * Set the button label
    *
    * @param aText  button label
    */
  
    virtual void SetLabel(const nsString &aText) = 0;
    
   /**
    * Get the button label
    *
    * @param aBuffer contains label upon return
    */
 
    virtual void GetLabel(nsString &aBuffer) = 0;




};

#endif // nsIDialog_h__
