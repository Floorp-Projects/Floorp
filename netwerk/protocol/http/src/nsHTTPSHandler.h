/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 *      Gagan Saksena <gagan@netscape.com>
*/

#ifndef _nsHTTPSHandler_h_
#define _nsHTTPSHandler_h_

#include "nsHTTPHandler.h"

#include "nsIHTTPProtocolHandler.h"
#include "nsIChannel.h"
#include "nsISupportsArray.h"
#include "nsCRT.h"

class nsIChannel;
class nsHTTPChannel;

class nsHTTPSHandler : public nsHTTPHandler
{
public:
    
    nsHTTPSHandler(void);
    virtual ~nsHTTPSHandler();
    static NS_METHOD Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult);

    //Functions from nsIProtocolHandler
    /*
        GetDefaultPort returns the default port associated with this 
        protocol. 
    */
    NS_IMETHOD GetDefaultPort(PRInt32 *result)
    {
        static const PRInt32 defaultPort = 443;
        *result = defaultPort;
        return NS_OK;
    };    

    /* 
        The GetScheme function uniquely identifies the scheme this handler 
		is associated with. 
    */
    NS_IMETHOD GetScheme(char * *o_Scheme)
    {
        static const char* scheme = "https";
        *o_Scheme = nsCRT::strdup(scheme);
        return NS_OK;
    };

    /**
     *    Called to create a transport from RequestTransport to accually
     *    make a new channel.
     **/
    
    virtual nsresult CreateTransport(const char* host, 
                                     PRInt32 port, 
                                     const char* proxyHost, 
                                     PRInt32 proxyPort, 
                                     PRUint32 bufferSegmentSize, 
                                     PRUint32 bufferMaxSize,
                                     nsIChannel** o_pTrans);
    
    virtual nsresult CreateTransportOfType(const char* socketType,
                                           const char* host,
                                           PRInt32 port, 
                                           const char* proxyHost, 
                                           PRInt32 proxyPort, 
                                           PRUint32 bufferSegmentSize, 
                                           PRUint32 bufferMaxSize,
                                           nsIChannel** o_pTrans);
};

#endif /* _nsHTTPSHandler_h_ */
