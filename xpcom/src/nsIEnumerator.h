/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef __nsIEnumerator_h
#define __nsIEnumerator_h

#include "nsISupports.h"

// {646F4FB0-B1F2-11d1-AA29-000000000000}
#define NS_IENUMERATOR_IID \
{0x646f4fb0, 0xb1f2, 0x11d1, \
    { 0xaa, 0x29, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } };


class nsIEnumerator : public nsISupports {
public:
    NS_IMETHOD_(nsISupports*)    Next() = 0;
    NS_IMETHOD_(void)            Reset()= 0;
};

#endif // __nsIFactory_h

