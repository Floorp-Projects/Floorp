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

// ftp implementation header

#ifndef nsFtpProtocolConnection_h___
#define nsFtpProtocolConnection_h___

#include "nsIFtpProtocolConnection.h"
#include "nsIStreamListener.h"
#include "nsITransport.h"

#include "nsString2.h"

class nsIConnectionGroup;
class nsIFtpEventSink;

class nsFtpProtocolConnection : public nsIFtpProtocolConnection
                                /*,public nsIStreamListener*/ {
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

    // nsIFtpProtocolConnection methods:
    NS_IMETHOD Get(void);
    NS_IMETHOD Put(void);
/*
    // nsIStreamObserver methods:
    NS_IMETHOD OnStartBinding(nsISupports* context);
    NS_IMETHOD OnStopBinding(nsISupports* context,
                             nsresult aStatus,
                             nsIString* aMsg);

    // nsIStreamListener methods:
    NS_IMETHOD OnDataAvailable(nsISupports* context,
                               nsIInputStream *aIStream, 
                               PRUint32 aSourceOffset,
                               PRUint32 aLength);
*/
    // nsFtpProtocolConnection methods:
    NS_IMETHOD SetStreamListener(nsIStreamListener* aListener);

    nsFtpProtocolConnection();
    virtual ~nsFtpProtocolConnection();

    nsresult Init(nsIUrl* aUrl, nsISupports* aEventSink, PLEventQueue* aEventQueue);

protected:
    nsIUrl*                 mUrl;
    nsIFtpEventSink*        mEventSink;
    PLEventQueue*           mEventQueue;

    PRBool                  mConnected;
    nsIStreamListener*      mListener;
};

#endif /* nsFtpProtocolConnection_h___ */
