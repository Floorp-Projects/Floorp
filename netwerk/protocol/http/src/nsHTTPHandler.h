/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _nsHTTPHandler_h_
#define _nsHTTPHandler_h_

/* 
    The nsHTTPHandler class is an example implementation of how a 
    pluggable protocol would be written by an external party. As 
    an example this class also uses the Proxy interface.

    Since this is a completely different process boundary, I am 
    keeping this as a singleton. It doesn't have to be that way.

    Currently this is being built with the Netlib dll. But after
    the registration stuff that DP is working on gets completed 
    this will move to the HTTP lib.

    -Gagan Saksena 02/25/99
*/
//TODO turnon the proxy stuff as well. 

#include "nsIHTTPProtocolHandler.h"
#include "nsIChannel.h"
#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
#include "nsCRT.h"

//Forward decl.
class nsHashtable;
class nsIChannel;
class nsHTTPChannel;

class nsHTTPHandler : public nsIHTTPProtocolHandler
		//, public nsIProxy 
{

public:

    //Functions from nsISupports
    NS_DECL_ISUPPORTS

    //Functions from nsIProtocolHandler
    /*
        GetDefaultPort returns the default port associated with this 
        protocol. 
    */
    NS_IMETHOD               GetDefaultPort(PRInt32 *result)
    {
        static const PRInt32 defaultPort = 80;
        *result = defaultPort;
        return NS_OK;
    };    

    /* 
        The GetScheme function uniquely identifies the scheme this handler 
		is associated with. 
    */
    NS_IMETHOD               GetScheme(char * *o_Scheme)
    {
        static const char* scheme = "http";
        *o_Scheme = nsCRT::strdup(scheme);
        return NS_OK;
    };

    NS_IMETHOD               MakeAbsolute(const char *aRelativeSpec, nsIURI *aBaseURI,
                                          char **_retval);

    NS_IMETHOD               NewChannel(const char* verb, nsIURI* url,
                                        nsILoadGroup *aGroup,
                                        nsIEventSinkGetter *eventSinkGetter,
                                        nsIChannel **_retval);
    
    NS_IMETHOD               NewURI(const char *aSpec, nsIURI *aBaseURI,
                                    nsIURI **_retval);

    //Functions from nsIHTTPProtocolHandler

#if 0
    //Functions from nsIProxy
    /*
        Get and Set the Proxy Host 
    */
    NS_IMETHOD      GetProxyHost(const char* *o_ProxyHost) const {return NS_ERROR_NOT_IMPLEMENTED;};
    NS_IMETHOD      SetProxyHost(const char* i_ProxyHost) {return NS_ERROR_NOT_IMPLEMENTED;};

    /*
        Get and Set the Proxy Port 
		-1 on Set call indicates switch to default port
    */
    NS_IMETHOD_(PRInt32)
                    GetProxyPort(void) const {return NS_ERROR_NOT_IMPLEMENTED;};
    NS_IMETHOD      SetProxyPort(PRInt32 i_ProxyPort) {return NS_ERROR_NOT_IMPLEMENTED;}; 

#endif
    // Follow the redirects automatically. This will trigger OnRedirect call on the sink
    NS_IMETHOD      FollowRedirects(PRBool bFollow=PR_TRUE);

    // Singleton function
    static nsHTTPHandler* GetInstance(void)
    {
        static nsHTTPHandler* pHandler = new nsHTTPHandler();
        NS_ADDREF(pHandler);
        return pHandler;
    };

    // Functions from nsIHTTPProtocolHandler
    NS_DECL_NSIHTTPPROTOCOLHANDLER

    /* 
        Pull out an existing transport from the list, or if none exists
        create one. 
    */
    virtual nsresult RequestTransport(nsIURI *i_Uri, 
                              nsHTTPChannel* i_Channel, 
                              nsIChannel** o_pTrans);
    /*
        Remove this transport from the list.
    */
    virtual nsresult ReleaseTransport(nsIChannel* i_pTrans);

protected:
    // None
    nsHTTPHandler(void);
    virtual ~nsHTTPHandler();

	// This is the array of connections that the handler thread maintains to 
    // verify unique requests. 
	nsCOMPtr<nsISupportsArray> mConnections;
    nsCOMPtr<nsISupportsArray> mPendingChannelList;
    nsCOMPtr<nsISupportsArray> mTransportList;
};

#endif /* _nsHTTPHandler_h_ */
