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
#include "plevent.h"

class nsIStreamListener;
class nsIStreamObserver;
class nsIInputStream;
class nsIOutputStream;

#define NS_ITRANSPORT_IID                            \
{ /* 7aa38100-ea35-11d2-931b-00104ba0fd40 */         \
    0x7aa38100,                                      \
    0xea35,                                          \
    0x11d2,                                          \
    {0x93, 0x1b, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

class nsITransport : public nsICancelable
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITRANSPORT_IID);

    // Async routines: By calling these the calling thread agrees
    // to eventually return to an event loop that will be notified
    // with incoming data, calling the listener.

    NS_IMETHOD AsyncRead(nsISupports* context,
                         PLEventQueue* appEventQueue,
                         nsIStreamListener* listener) = 0;

    NS_IMETHOD AsyncWrite(nsIInputStream* fromStream,
                          nsISupports* context,
                          PLEventQueue* appEventQueue,
                          nsIStreamObserver* observer) = 0;

    // Synchronous routines

    NS_IMETHOD OpenInputStream(nsIInputStream* *result) = 0;

    NS_IMETHOD OpenOutputStream(nsIOutputStream* *result) = 0;

};

#endif /* nsITransport_h___ */
