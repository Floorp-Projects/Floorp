/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef MyService_h__
#define MyService_h__

#include "nsISupports.h"

class IMyService : public nsISupports {
public:
    
    NS_IMETHOD
    Doit(void) = 0;

};

#define NS_IMYSERVICE_IID                            \
{ /* fedc3380-3648-11d2-8163-006008119d7a */         \
    0xfedc3380,                                      \
    0x3648,                                          \
    0x11d2,                                          \
    {0x81, 0x63, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

#define NS_IMYSERVICE_CID                            \
{ /* 34876550-364b-11d2-8163-006008119d7a */         \
    0x34876550,                                      \
    0x364b,                                          \
    0x11d2,                                          \
    {0x81, 0x63, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

#endif // MyService_h__
