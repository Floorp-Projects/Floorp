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

#ifndef nsHttpProtocolHandler_h___
#define nsHttpProtocolHandler_h___

#include "nsIProtocolHandler.h"

class nsITransport;
class nsHashtable;

// XXX regenerate:
#define NS_HTTPPROTOCOLHANDLER_CID                   \
{ /* 59321440-f125-11d2-9322-000000000000 */         \
    0x59321440,                                      \
    0xf125,                                          \
    0x11d2,                                          \
    {0x93, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} \
}

class nsHttpProtocolHandler : public nsIProtocolHandler
{
public:
    NS_DECL_ISUPPORTS

    // nsIProtocolHandler methods:
    NS_IMETHOD GetScheme(const char* *result);
    NS_IMETHOD GetDefaultPort(PRInt32 *result);    
    NS_IMETHOD MakeAbsoluteUrl(const char* aSpec,
                               nsIUrl* aBaseUrl,
                               char* *result);
    NS_IMETHOD NewUrl(const char* aSpec,
                      nsIUrl* aBaseUrl,
                      nsIUrl* *result);
    NS_IMETHOD NewConnection(nsIUrl* url,
                             nsISupports* eventSink,
                             PLEventQueue* eventQueue,
                             nsIProtocolConnection* *result);

    // nsHttpProtocolHandler methods:
    nsHttpProtocolHandler();
    virtual ~nsHttpProtocolHandler();

    nsresult Init(void);
    nsresult GetTransport(const char* host, PRInt32 port,
                          nsITransport* *result);
    nsresult ReleaseTransport(const char* host, PRInt32 port,
                              nsITransport* trans);

protected:
    nsHashtable*        mConnectionPool;
};

////////////////////////////////////////////////////////////////////////////////

#include "nsHashtable.h"
#include "plstr.h"

class nsSocketTransportKey : public nsCStringKey 
{
private:
    PRInt32     mPort;
  
public:
    nsSocketTransportKey(const char* host, PRInt32 port)
        : nsCStringKey(host), mPort(port)
    {
    }
  
    ~nsSocketTransportKey(void) {
    }
  
    PRBool Equals(const nsHashKey *aKey) const {
        return mPort == ((nsSocketTransportKey*)aKey)->mPort
            && nsCStringKey::Equals(aKey);
    }

    nsHashKey *Clone(void) const {
        return new nsSocketTransportKey(mStr, mPort);
    }
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsHttpProtocolHandler_h___ */
