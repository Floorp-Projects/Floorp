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

#ifndef nsINetService_h___
#define nsINetService_h___

#include "nsISupports.h"

class nsIUrl;
class nsIProtocolConnection;
class nsIConnectionGroup;
class nsIProtocolHandler;

#define NS_INETSERVICE_IID                           \
{ /* 3c70f340-ea35-11d2-931b-00104ba0fd40 */         \
    0x3c70f340,                                      \
    0xea35,                                          \
    0x11d2,                                          \
    {0x93, 0x1b, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

#define NS_NETSERVICE_CID                            \
{ /* 451ec5e0-ea35-11d2-931b-00104ba0fd40 */         \
    0x451ec5e0,                                      \
    0xea35,                                          \
    0x11d2,                                          \
    {0x93, 0x1b, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

class nsINetService : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_INETSERVICE_IID);

    NS_IMETHOD GetProtocolHandler(const char* scheme,
                                  nsIProtocolHandler* *result) = 0;

    NS_IMETHOD NewConnectionGroup(nsIConnectionGroup* *result) = 0;

    /**
     * Returns the absolute URL given a (possibly) relative spec, and 
     * a base URL. Returns a string that must be freed with delete[].
     */
    NS_IMETHOD MakeAbsoluteUrl(const char* aSpec,
                               nsIUrl* aBaseUrl,
                               char* *result) = 0;

    /**
     * This method constructs a new URL by first determining the scheme
     * of the url spec, and then delegating the construction of the URL
     * to the protocol handler for that scheme. QueryInterface can be used
     * on the resulting URL object to obtain a more specific type of URL.
     */
    NS_IMETHOD NewUrl(nsIUrl* *result, 
                      const char* aSpec,
                      nsIUrl* aBaseUrl) = 0;

    NS_IMETHOD NewConnection(nsIUrl* url,
                             nsISupports* eventSink,
                             nsIConnectionGroup* group,
                             nsIProtocolConnection* *result) = 0;

    /**
     * @return NS_OK if there are active connections
     * @return NS_COMFALSE if there are not.
     */
    NS_IMETHOD HasActiveConnections() = 0;

};

#endif /* nsIINetService_h___ */
