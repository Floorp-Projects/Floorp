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

#ifndef nsIToolbarItemHolder_h___
#define nsIToolbarItemHolder_h___

#include "nsISupports.h"

class nsIWidget;

// {8556F9D2-5235-11d2-8DC1-00609703C14E}
#define NS_ITOOLBARITEMHOLDER_IID      \
{ 0x8556f9d2, 0x5235, 0x11d2, \
  { 0x8d, 0xc1, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }

class nsIToolbarItemHolder : public nsISupports
{

public:

    /**
     * Set the widget into the holder
     *
     */
    NS_IMETHOD SetWidget(nsIWidget * aWidget) = 0;


    /**
     * Gets the widget from the holder
     */
    NS_IMETHOD GetWidget(nsIWidget *&aWidget) = 0;

};

#endif /* nsIToolbarItemHolder_h___ */

