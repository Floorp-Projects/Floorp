/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef __nsFtpState__h_
#define __nsFtpState__h_

#include "ftpCore.h"
#include "nsIThread.h"
#include "nsIRunnable.h"
#include "nsISocketTransportService.h"
#include "nsIServiceManager.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "prtime.h"
#include "nsString.h"
#include "nsIFTPChannel.h"
#include "nsIProtocolHandler.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsAutoLock.h"
#include "nsIEventQueueService.h"
#include "nsIPrompt.h"
#include "nsIAuthPrompt.h"
#include "nsITransport.h"
#include "nsIProxyInfo.h"

#include "nsFtpControlConnection.h"

#include "nsICacheEntryDescriptor.h"

// ftp server types
#define FTP_GENERIC_TYPE     0
#define FTP_UNIX_TYPE        1
#define FTP_NT_TYPE          9

// ftp states
typedef enum _FTP_STATE {
///////////////////////
//// Internal states
    FTP_COMMAND_CONNECT,
    FTP_READ_BUF,
    FTP_ERROR,
    FTP_COMPLETE,

///////////////////////
//// Command channel connection setup states
    FTP_S_USER, FTP_R_USER,
    FTP_S_PASS, FTP_R_PASS,
    FTP_S_SYST, FTP_R_SYST,
    FTP_S_ACCT, FTP_R_ACCT,
    FTP_S_TYPE, FTP_R_TYPE,
    FTP_S_CWD,  FTP_R_CWD,
    FTP_S_SIZE, FTP_R_SIZE,
    FTP_S_REST, FTP_R_REST,
    FTP_S_RETR, FTP_R_RETR,
    FTP_S_STOR, FTP_R_STOR,
    FTP_S_LIST, FTP_R_LIST,
    FTP_S_PASV, FTP_R_PASV
} FTP_STATE;

// higher level ftp actions
typedef enum _FTP_ACTION {GET, PUT} FTP_ACTION;

class DataRequestForwarder;

class nsFtpState : public nsIStreamListener,
                   public nsIRequest {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSIREQUEST

    nsFtpState();
    virtual ~nsFtpState();

    nsresult Init(nsIFTPChannel *aChannel, 
                  nsIPrompt *aPrompter, 
                  nsIAuthPrompt *aAuthPrompter, 
                  nsIFTPEventSink *sink, 
                  nsICacheEntryDescriptor* cacheEntry,
                  nsIProxyInfo* proxyInfo);

    // use this to provide a stream to be written to the server.
    nsresult SetWriteStream(nsIInputStream* aInStream);

    nsresult Connect();

    // lets the data forwarder tell us when the the data pipe has been created. 
    nsresult DataConnectionEstablished();    
private:
    ///////////////////////////////////
    // BEGIN: STATE METHODS
    nsresult        S_user(); FTP_STATE       R_user();
    nsresult        S_pass(); FTP_STATE       R_pass();
    nsresult        S_syst(); FTP_STATE       R_syst();
    nsresult        S_acct(); FTP_STATE       R_acct();

    nsresult        S_type(); FTP_STATE       R_type();
    nsresult        S_cwd();  FTP_STATE       R_cwd();

    nsresult        S_size(); FTP_STATE       R_size();
    nsresult        S_list(); FTP_STATE       R_list();

    nsresult        S_rest(); FTP_STATE       R_rest();
    nsresult        S_retr(); FTP_STATE       R_retr();
    nsresult        S_stor(); FTP_STATE       R_stor();
    nsresult        S_pasv();     FTP_STATE   R_pasv();
    // END: STATE METHODS
    ///////////////////////////////////

    // internal methods
    void        SetDirMIMEType(nsString& aString);
    void        MoveToNextState(FTP_STATE nextState);
    nsresult    Process();

    void KillControlConnection();
    nsresult StopProcessing();
    nsresult EstablishControlConnection();
    nsresult SendFTPCommand(nsCString& command);
    nsresult BuildStreamConverter(nsIStreamListener** convertStreamListener);
    nsresult SetContentType();

    ///////////////////////////////////
    // Private members

        // ****** state machine vars
    FTP_STATE           mState;             // the current state
    FTP_STATE           mNextState;         // the next state
    PRPackedBool        mKeepRunning;       // thread event loop boolean
    PRInt32             mResponseCode;      // the last command response code
    nsCAutoString       mResponseMsg;       // the last command response text

        // ****** channel/transport/stream vars 
    nsFtpControlConnection*         mControlConnection;       // cacheable control connection (owns mCPipe)
    PRPackedBool                    mReceivedControlData;  
    PRPackedBool                    mTryingCachedControl;     // retrying the password
    PRPackedBool                    mWaitingForDConn;         // Are we wait for a data connection
    nsCOMPtr<nsITransport>          mDPipe;                   // the data transport
    nsCOMPtr<nsIRequest>            mDPipeRequest;
    DataRequestForwarder*           mDRequestForwarder;


        // ****** consumer vars
    nsCOMPtr<nsIFTPChannel>         mChannel;         // our owning FTP channel we pass through our events
    nsCOMPtr<nsIProxyInfo>          mProxyInfo;

        // ****** connection cache vars
    PRInt32             mServerType;    // What kind of server are we talking to
    PRPackedBool        mList;          // Use LIST instead of NLST

        // ****** protocol interpretation related state vars
    nsAutoString        mUsername;      // username
    nsAutoString        mPassword;      // password
    FTP_ACTION          mAction;        // the higher level action (GET/PUT)
    PRPackedBool        mAnonymous;     // try connecting anonymous (default)
    PRPackedBool        mRetryPass;     // retrying the password
    nsresult            mInternalError; // represents internal state errors

        // ****** URI vars
    nsCOMPtr<nsIURI>       mURL;        // the uri we're connecting to
    PRInt32                mPort;       // the port to connect to
    nsAutoString           mFilename;   // url filename (if any)
    nsXPIDLCString         mPath;       // the url's path

        // ****** other vars
    PRUint8                mSuspendCount;// number of times we've been suspended.
    PRUint32               mBufferSegmentSize;
    PRUint32               mBufferMaxSize;
    PRLock                 *mLock;
    nsCOMPtr<nsIInputStream> mWriteStream; // This stream is written to the server.
    PRPackedBool           mFireCallbacks; // Fire the listener callback.
    PRPackedBool           mIPv6Checked;
    PRBool                 mGenerateHTMLContent;
    PRBool                 mGenerateRawContent;
    nsCOMPtr<nsIPrompt>    mPrompter;
    nsCOMPtr<nsIFTPEventSink>       mFTPEventSink;
    nsCOMPtr<nsIAuthPrompt> mAuthPrompter;

    char                   *mIPv6ServerAddress; // Server IPv6 address; null if server not IPv6

    // ***** control read gvars
    nsresult                mControlStatus;
    PRPackedBool            mControlReadContinue;
    PRPackedBool            mControlReadBrokenLine;
    nsCAutoString           mControlReadCarryOverBuf;

    nsCOMPtr<nsICacheEntryDescriptor> mCacheEntry;
};


#endif //__nsFtpState__h_
