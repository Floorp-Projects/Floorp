/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef __nsconnectioncacheobj__h____
#define __nsconnectioncacheobj__h____

#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsString.h"

// This class represents a cached connection for FTP connections.
// State pertinent to re-establishing an FTP "connection" is stored
// here between FTP URL loads.
//
// The cache object string key has the following syntax
// "HostPort" NOTE: no seperators.
class nsConnectionCacheObj {
public:
    nsConnectionCacheObj(nsIChannel *aChannel,
                   nsIInputStream *aInputStream,
                   nsIOutputStream *aOutputStream)
    { 
        mSocketTransport = aChannel;
        mInputStream = aInputStream;
        mOutputStream = aOutputStream;
        mServerType = 0;
        mList = PR_FALSE;
    };
    ~nsConnectionCacheObj() {;};

    nsCOMPtr<nsIChannel>       mSocketTransport;      // the connection
    nsCOMPtr<nsIInputStream>   mInputStream;          // to read from server
    nsCOMPtr<nsIOutputStream>  mOutputStream;         // to write to server
    PRUint32         mServerType;           // what kind of server is it.
    nsCAutoString    mCwd;                  // what dir are we in
    PRBool           mList;                 // are we sending LIST or NLST
    nsAutoString     mPassword;
};

#endif // __nsconnectioncacheobj__h____
