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

#ifndef nsITransport_h___
#define nsITransport_h___

#include "nsICancelable.h"

class nsIStreamListener;
class nsIStreamObserver;

// XXX regenerate:
#define NS_ITRANSPORT_IID                            \
{ /* 677d9a90-93ee-11d2-816a-006008119d7a */         \
    0x677d9a90,                                      \
    0x93ee,                                          \
    0x11d2,                                          \
    {0x81, 0x6a, 0x00, 0x60, 0xe8, 0x11, 0x9d, 0x7a} \
}

class nsITransport : public nsICancelable
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITRANSPORT_IID);

    NS_IMETHOD AsyncRead(nsIStreamListener* listener) = 0;

    NS_IMETHOD AsyncWrite(nsIStreamObserver* observer) = 0;
};

#endif /* nsIITransport_h___ */
