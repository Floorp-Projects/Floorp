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

#ifndef nsIProtocolHandler_h___
#define nsIProtocolHandler_h___

#include "nsISupports.h"

class nsIConnectionGroup;
class nsIUrl;
class nsIProtocolConnection;

#define NS_IPROTOCOLHANDLER_IID                      \
{ /* 5da8b1b0-ea35-11d2-931b-00104ba0fd40 */         \
    0x5da8b1b0,                                      \
    0xea35,                                          \
    0x11d2,                                          \
    {0x93, 0x1b, 0x00, 0x10, 0x4b, 0xa0, 0xfd, 0x40} \
}

class nsIProtocolHandler : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPROTOCOLHANDLER_IID);

    NS_IMETHOD GetScheme(const char* *result) = 0;

    NS_IMETHOD GetDefaultPort(PRInt32 *result) = 0;    

    NS_IMETHOD MakeAbsoluteUrl(const char* aSpec,
                               nsIUrl* aBaseUrl,
                               char* *result) = 0;

    /**
     * Makes a URL object that is suitable for loading by this protocol.
     * In the usual case (when only the accessors provided by nsIUrl are 
     * needed), this method just constructs a typical URL using the
     * component manager with kTypicalUrlCID.
     */
    NS_IMETHOD NewUrl(const char* aSpec,
                      nsIUrl* aBaseUrl,
                      nsIUrl* *result) = 0;

    NS_IMETHOD NewConnection(nsIUrl* url,
                             nsISupports* eventSink,
                             nsIProtocolConnection* *result) = 0;
};

#define NS_NETWORK_PROTOCOL_PROGID              "component://netscape/network/protocol"
#define NS_NETWORK_PROTOCOL_PROGID_PREFIX       NS_NETWORK_PROTOCOL_PROGID "?name="
#define NS_NETWORK_PROTOCOL_PROGID_PREFIX_LENGTH 43     // nsCRT::strlen(NS_NETWORK_PROTOCOL_PROGID_PREFIX)

#endif /* nsIIProtocolHandler_h___ */
