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

#ifndef _nsIProxy_h_
#define _nsIProxy_h_

#include "nsISupports.h"

/* 
    The nsIProxy interface allows setting and getting of proxy host and port. 
    This is for use by protocol handlers. If you are writing a protocol handler
    and would like to support proxy behaviour then derive from this as well as
    the nsIProtocolHandler class.

    -Gagan Saksena 02/25/99
*/

class nsIProxy : public nsISupports
{

public:
    /*
        Get and Set the Proxy Host 
    */
    NS_IMETHOD      GetProxyHost(const char* *o_ProxyHost) const = 0;
    NS_IMETHOD      SetProxyHost(const char* i_ProxyHost) = 0;

    /*
        Get and Set the Proxy Port 
		-1 on Set call indicates switch to default port
    */
    NS_IMETHOD_(PRInt32)
                    GetProxyPort(void) const = 0;
    NS_IMETHOD      SetProxyPort(PRInt32 i_ProxyPort) = 0; 


    static const nsIID& GetIID() { 
        // {0492D011-CD2F-11d2-B013-006097BFC036}
        static const nsIID NS_IPROXY_IID= 
            { 0x492d011, 0xcd2f, 0x11d2, { 0xb0, 0x13, 0x0, 0x60, 0x97, 0xbf, 0xc0, 0x36 } };
		return NS_IPROXY_IID; 
	};

};

#endif /* _nsIProxy_h_ */
