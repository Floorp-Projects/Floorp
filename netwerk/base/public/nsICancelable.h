/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsICancelable_h___
#define nsICancelable_h___

#include "nsISupports.h"

// XXX regenerate:
#define NS_ICANCELABLE_IID                           \
{ /* 677d9a90-93ee-11d2-816a-006008119d7a */         \
    0x677d9a90,                                      \
    0x93ee,                                          \
    0x11d2,                                          \
    {0x81, 0x6a, 0x00, 0x6f, 0xc8, 0x11, 0x9d, 0x7a} \
}

class nsICancelable : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICANCELABLE_IID);

    NS_IMETHOD Cancel(void) = 0;

    NS_IMETHOD Suspend(void) = 0;

    NS_IMETHOD Resume(void) = 0;

};

#endif /* nsICancelable_h___ */
