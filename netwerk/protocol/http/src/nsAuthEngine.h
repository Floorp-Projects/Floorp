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

#ifndef _nsAuthEngine_h_
#define _nsAuthEngine_h_

#include "nsCOMPtr.h"
#include "nsISupportsArray.h"
// Forward Decl
class nsIURI;
/* 
    The nsAuthEngine class is the central class for handling
    authentication lists, checks etc. 

    There should be a class nsProxyAuthEngine that shares all this...
    I will move it that way... someday... 

    -Gagan Saksena 11/12/99
*/
class nsAuthEngine
{

public:

    nsAuthEngine(void);
    virtual ~nsAuthEngine();

    nsresult        Init();

    // Get an auth string
    NS_IMETHOD      GetAuthString(nsIURI* i_URI, char** o_AuthString);

    // Set an auth string
    NS_IMETHOD      SetAuthString(nsIURI* i_URI, const char* i_AuthString);

    // Get a proxy auth string with host/port
    NS_IMETHOD      GetProxyAuthString(const char* host, 
                        PRInt32 port, 
                        char** o_AuthString);

    // Set a proxy auth string
    NS_IMETHOD      SetProxyAuthString(const char* host,
                        PRInt32 port,
                        const char* i_AuthString);

    /*
       Expire all existing auth list entries including proxy auths. 
    */
    NS_IMETHOD      Logout(void);

protected:

    NS_IMETHOD      SetAuth(nsIURI* i_URI, 
                        const char* i_AuthString, 
                        PRBool bProxyAuth = PR_FALSE);

    nsCOMPtr<nsISupportsArray>  mAuthList; 
    // this needs to be a list becuz pac can produce more ...
    nsCOMPtr<nsISupportsArray>  mProxyAuthList; 

};

inline nsresult
nsAuthEngine::SetAuthString(nsIURI* i_URI, const char* i_AuthString)
{
    return SetAuth(i_URI, i_AuthString);
}

#endif /* _nsAuthEngine_h_ */
