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

#ifndef nsITransportService_h___
#define nsITransportService_h___

#include "nsISupports.h"
#include "plevent.h"

class nsITransport;
class nsIStreamListener;

// XXX regenerate:
#define NS_ITRANSPORTSERVICE_IID                     \
{ /* 677d9a90-93ee-11d2-816a-006008119d7a */         \
    0x677d9a90,                                      \
    0x93ee,                                          \
    0x11d2,                                          \
    {0x81, 0x6a, 0x00, 0x60, 0xd8, 0x11, 0x9d, 0x7a} \
}

// XXX regenerate:
#define NS_TRANSPORTSERVICE_CID                      \
{ /* 677d9a90-93ee-11d2-816a-006008119d7a */         \
    0x677d9a90,                                      \
    0x93ee,                                          \
    0x11d2,                                          \
    {0x87, 0x6a, 0x00, 0x60, 0xd8, 0x11, 0x9d, 0x7a} \
}

class nsITransportService : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITRANSPORTSERVICE_IID);

    NS_IMETHOD GetFileTransport(PLEventQueue* appEventQueue,
                                nsIStreamListener* listener,
                                const char* path, 
                                nsITransport* *result) = 0;

    NS_IMETHOD GetSocketTransport(PLEventQueue* appEventQueue,
                                  nsIStreamListener* listener,
                                  const char* host, PRInt32 port,
                                  nsITransport* *result) = 0;

    NS_IMETHOD GetJarTransport(PLEventQueue* appEventQueue,
                               nsIStreamListener* listener,
                               const char* jarFilePath,
                               const char* jarEntryPath,
                               nsITransport* *result) = 0;

    /**
     * @return NS_OK if there are active transports
     * @return NS_COMFALSE if there are not.
     */
    NS_IMETHOD HasActiveTransports() = 0;
};

#endif /* nsIITransportService_h___ */
