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

#ifndef nsFtpProtocolConnection_h___
#define nsFtpProtocolConnection_h___

#include "nsIFtpProtocolConnection.h"
#include "nsIStreamListener.h"

class nsIConnectionGroup;
class nsIFtpEventSink;


// ftp server types
#define FTP_GENERIC_TYPE     0
#define FTP_UNIX_TYPE        1
#define FTP_DCTS_TYPE        2
#define FTP_NCSA_TYPE        3
#define FTP_PETER_LEWIS_TYPE 4
#define FTP_MACHTEN_TYPE     5
#define FTP_CMS_TYPE         6
#define FTP_TCPC_TYPE        7
#define FTP_VMS_TYPE         8
#define FTP_NT_TYPE          9
#define FTP_WEBSTAR_TYPE     10

// ftp states
typedef enum _FTP_STATE {
    FTP_CONNECT,
    FTP_S_PASV,
    FTP_R_PASV,
    FTP_S_PORT,
    FTP_R_PORT,
    FTP_COMPLETE
} FTP_STATE;

class nsFtpProtocolConnection : public nsIFtpProtocolConnection,
                                public nsIStreamListener {
public:
    NS_DECL_ISUPPORTS

    // nsICancelable methods:
    NS_IMETHOD Cancel(void);
    NS_IMETHOD Suspend(void);
    NS_IMETHOD Resume(void);

    // nsIProtocolConnection methods:
    NS_IMETHOD Open(nsIUrl* url, nsISupports* eventSink);
    NS_IMETHOD GetContentType(char* *contentType);
    NS_IMETHOD GetInputStream(nsIInputStream* *result);
    NS_IMETHOD GetOutputStream(nsIOutputStream* *result);

    // nsIFtpProtocolConnection methods:
    NS_IMETHOD Get(void);
    NS_IMETHOD Put(void);
    NS_IMETHOD UsePASV(PRBool aComm);

    // nsIStreamObserver methods:
    NS_IMETHOD OnStartBinding(nsISupports* context);
    NS_IMETHOD OnStopBinding(nsISupports* context,
                             nsresult aStatus,
                             nsIString* aMsg);

    // nsIStreamListener methods:
    NS_IMETHOD OnDataAvailable(nsISupports* context,
                               nsIInputStream *aIStream, 
                               PRUint32 aLength);

    // nsFtpProtocolConnection methods:
    nsFtpProtocolConnection();
    virtual ~nsFtpProtocolConnection();

    nsresult Init(nsIUrl* url, nsISupports* eventSink);

protected:
    nsIUrl*                 mUrl;
    nsIFtpEventSink*        mEventSink;
    PRInt32                 mServerType;
    PRBool                  mPasv;
    PRBool                  mConnected;
    FTP_STATE               mState;
};

#endif /* nsFtpProtocolConnection_h___ */
