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

    -Gagan Saksena 11/12/99
*/
class nsAuthEngine
{

public:

    nsAuthEngine(void);
    virtual ~nsAuthEngine();
    /*
       enum authType { Basic, Digest }
    */

    // Set an auth string
    NS_IMETHOD      SetAuthString(nsIURI* i_URI, 
                        const char* i_AuthString /*, int* type */);

    // Get an auth string
    NS_IMETHOD      GetAuthString(nsIURI* i_URI, 
                        char** o_AuthString /*, int* type */);
    /*
       Expire all existing auth list entries.
    */
    NS_IMETHOD      Logout(void);

protected:

    nsCOMPtr<nsISupportsArray>  mAuthList; 

};

#endif /* _nsAuthEngine_h_ */
