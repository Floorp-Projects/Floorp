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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIDnsService_h___
#define nsIDnsService_h___

#include "nsISupports.h"
#include "nsIDnsListener.h"

class nsIDnsListener;
class nsICancelable;


#define NS_IDNSSERVICE_IID                           \
{ /* 718e7c80-f8b8-11d2-b951-c80918051d3c */         \
    0x718e7c80,                                      \
    0xf8b8,                                          \
    0x11d2,                                          \
    { 0xb9, 0x51, 0xc8, 0x09, 0x18, 0x05, 0x1d, 0x3c } \
}

#define NS_DNSSERVICE_CID                            \
{ /* 718e7c81-f8b8-11d2-b951-c80918051d3c */         \
    0x718e7c81,                                      \
    0xf8b8,                                          \
    0x11d2,                                          \
    { 0xb9, 0x51, 0xc8, 0x09, 0x18, 0x05, 0x1d, 0x3c } \
}


class nsIDnsService : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDNSSERVICE_IID);

    NS_IMETHOD Lookup(const char*      hostname,
                      nsIDnsListener*  listener,
                      nsICancelable*  *dnsRequest);
		

};

#endif /* nsIDnsService_h___ */
