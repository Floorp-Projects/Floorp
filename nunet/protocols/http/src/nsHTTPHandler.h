/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#include "nsIHTTPHandler.h"
#include "nsIProtocolInstance.h"

//Forward decl.
class nsITimer; //TODO check with pnunn if a new version is available, where?

class nsHTTPHandler : public nsIHTTPHandler//, public nsIProxy 
{

public:

    //Functions from nsISupports
    NS_DECL_ISUPPORTS

    //Functions from nsIHTTPHandler
    NS_METHOD       GetProtocolInstance(nsICoolURL* i_URL, 
					  	nsIProtocolInstance* *o_Instance);

    //Functions from nsIProxy
    /*
        Get and Set the Proxy Host 
    */
    NS_METHOD      GetProxyHost(const char* *o_ProxyHost) const {return NS_ERROR_NOT_IMPLEMENTED;};
    NS_METHOD      SetProxyHost(const char* i_ProxyHost) {return NS_ERROR_NOT_IMPLEMENTED;};

    /*
        Get and Set the Proxy Port 
		-1 on Set call indicates switch to default port
    */
    NS_METHOD_(PRInt32)
                   GetProxyPort(void) const {return NS_ERROR_NOT_IMPLEMENTED;};
    NS_METHOD      SetProxyPort(PRInt32 i_ProxyPort) {return NS_ERROR_NOT_IMPLEMENTED;}; 

    // Singleton function
    static nsHTTPHandler* GetInstance(void)
    {
        static nsHTTPHandler* pHandler = new nsHTTPHandler();
        return pHandler;
    };

protected:
    nsHTTPHandler(void);
    ~nsHTTPHandler();

    //This is the timer that polls on the sockets.
    nsITimer* m_pTimer; 

};

#endif /* _nsHTTPHandler_h_ */
