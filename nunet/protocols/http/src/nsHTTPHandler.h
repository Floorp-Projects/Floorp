/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
