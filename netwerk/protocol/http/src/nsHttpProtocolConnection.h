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

#ifndef nsHttpProtocolConnection_h___
#define nsHttpProtocolConnection_h___

#include "nsIHttpProtocolConnection.h"
#include "nsIStreamListener.h"

class nsIConnectionGroup;
class nsIHttpEventSink;
class nsIString;
class nsITransport;
class nsHttpProtocolHandler;

class nsHttpProtocolConnection : public nsIHttpProtocolConnection,
                                 public nsIStreamListener
{
public:
    NS_DECL_ISUPPORTS

    // nsICancelable methods:
    NS_IMETHOD Cancel(void);
    NS_IMETHOD Suspend(void);
    NS_IMETHOD Resume(void);

    // nsIProtocolConnection methods:
    NS_IMETHOD Open(void);
    NS_IMETHOD GetContentType(char* *contentType);
    NS_IMETHOD GetInputStream(nsIInputStream* *result);
    NS_IMETHOD GetOutputStream(nsIOutputStream* *result);

    // nsIHttpProtocolConnection methods:
    NS_IMETHOD GetHeader(const char* header);
    NS_IMETHOD AddHeader(const char* header, const char* value);
    NS_IMETHOD RemoveHeader(const char* header);
    NS_IMETHOD Get(void);
    NS_IMETHOD GetByteRange(PRUint32 from, PRUint32 to);
    NS_IMETHOD Put(void);
    NS_IMETHOD Post(void);

    // nsIStreamObserver methods:
    NS_IMETHOD OnStartBinding(nsISupports* context);
    NS_IMETHOD OnStopBinding(nsISupports* context,
                             nsresult aStatus,
                             nsIString* aMsg);

    // nsIStreamListener methods:
    NS_IMETHOD OnDataAvailable(nsISupports* context,
                               nsIInputStream *aIStream, 
                               PRUint32 aLength);

    // nsHttpProtocolConnection methods:
    nsHttpProtocolConnection();
    virtual ~nsHttpProtocolConnection();

    nsresult Init(nsIUrl* url, nsISupports* eventSink, 
                  nsHttpProtocolHandler* handler,
                  PLEventQueue* eventQueue);
    nsresult GetExistingTransport(const char* host, PRInt32 port,
                                  nsITransport* *result);

    enum State {
        UNCONNECTED,
        POSTING,
        CONNECTED,
        WAITING_REPLY
    };

protected:
    nsHttpProtocolHandler*      mHandler;
    nsIUrl*                     mUrl;
    nsIHttpEventSink*           mEventSink;
    State                       mState;
    nsITransport*               mTransport;
    PLEventQueue*               mEventQueue;
};

#endif /* nsHttpProtocolConnection_h___ */
