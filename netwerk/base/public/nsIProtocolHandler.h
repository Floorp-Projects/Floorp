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

// XXX regenerate:
#define NS_IPROTOCOLHANDLER_IID                      \
{ /* 677d9a90-93ee-11d2-816a-006008119d7a */         \
    0x677d9a90,                                      \
    0x93ee,                                          \
    0x11d2,                                          \
    {0x81, 0x6a, 0x00, 0x60, 0xa8, 0x11, 0x9d, 0x7a} \
}

class nsIProtocolHandler : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPROTOCOLHANDLER_IID);

    NS_IMETHOD GetScheme(const char* *o_Scheme) = 0;

    NS_IMETHOD GetDefaultPort(PRInt32 *result) = 0;    

    NS_IMETHOD Open(nsIUrl* url, nsISupports* eventSink,
                    nsIConnectionGroup* group,
                    nsIProtocolConnection* *result) = 0;
};

#define NS_NETWORK_PROTOCOL_PROGID              "component://netscape/network/protocol?name="
#define NS_NETWORK_PROTOCOL_PROGID_PREFIX       NS_NETWORK_PROTOCOL_PROGID "?name="

#endif /* nsIIProtocolHandler_h___ */
