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
    FTP_S_USER,		// send username
    FTP_R_USER,
    FTP_S_PASS,		// send password
    FTP_R_PASS,
//	FTP_S_REST,		// send restart
//	FTP_R_REST,
	FTP_S_SYST,		// send system (interrogates server)
	FTP_R_SYST,
    FTP_S_ACCT,		// send account
    FTP_R_ACCT,
	FTP_S_MACB,
	FTP_R_MACB,
	FTP_S_PWD ,		// send parent working directory (pwd)
	FTP_R_PWD ,
    FTP_S_PASV,		// send passive
    FTP_R_PASV,
    FTP_S_PORT,		// send port
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
    NS_IMETHOD Open(void);
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
                               PRUint32 aSourceOffset,
                               PRUint32 aLength);

    // nsFtpProtocolConnection methods:
    nsFtpProtocolConnection();
    virtual ~nsFtpProtocolConnection();

    nsresult Init(nsIUrl* aUrl, nsISupports* aEventSink, PLEventQueue* aEventQueue);

private:
	void SetSystInternals(void);

protected:
    nsIUrl*                 mUrl;
    nsIFtpEventSink*        mEventSink;
    PLEventQueue*           mEventQueue;

// these members should be hung off of a specific transport connection
    PRInt32                 mServerType;
    PRBool                  mPasv;
	PRBool					mList;					// use LIST instead of NLST
// end "these ...."

    PRBool                  mConnected;
	PRBool					mUseDefaultPath;		// use PWD to figure out path
    FTP_STATE               mState;
    nsITransport*           mCPipe;                 // the command channel
    nsITransport*           mDPipe;                 // the data channel
    PRInt32                 mResponseCode;          // the last command response code.
	nsString2				mResponseMsg;			// the last command response text
    nsString2               mUsername;
    nsString2               mPassword;
};

#endif /* nsFtpProtocolConnection_h___ */
