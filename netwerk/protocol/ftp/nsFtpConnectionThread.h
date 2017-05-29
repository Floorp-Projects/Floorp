/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsFtpConnectionThread__h_
#define __nsFtpConnectionThread__h_

#include "nsBaseContentStream.h"

#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIAsyncInputStream.h"
#include "nsAutoPtr.h"
#include "nsITransport.h"
#include "mozilla/net/DNS.h"
#include "nsFtpControlConnection.h"
#include "nsIProtocolProxyCallback.h"

// ftp server types
#define FTP_GENERIC_TYPE     0
#define FTP_UNIX_TYPE        1
#define FTP_VMS_TYPE         8
#define FTP_NT_TYPE          9
#define FTP_OS2_TYPE         11

// ftp states
typedef enum _FTP_STATE {
///////////////////////
//// Internal states
    FTP_INIT,
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
    FTP_S_MDTM, FTP_R_MDTM,
    FTP_S_REST, FTP_R_REST,
    FTP_S_RETR, FTP_R_RETR,
    FTP_S_STOR, FTP_R_STOR,
    FTP_S_LIST, FTP_R_LIST,
    FTP_S_PASV, FTP_R_PASV,
    FTP_S_PWD,  FTP_R_PWD,
    FTP_S_FEAT, FTP_R_FEAT,
    FTP_S_OPTS, FTP_R_OPTS
} FTP_STATE;

// higher level ftp actions
typedef enum _FTP_ACTION {GET, PUT} FTP_ACTION;

class nsFtpChannel;
class nsICancelable;
class nsIProxyInfo;
class nsIStreamListener;

// The nsFtpState object is the content stream for the channel.  It implements
// nsIInputStreamCallback, so it can read data from the control connection.  It
// implements nsITransportEventSink so it can mix status events from both the
// control connection and the data connection.

class nsFtpState final : public nsBaseContentStream,
                         public nsIInputStreamCallback,
                         public nsITransportEventSink,
                         public nsIRequestObserver,
                         public nsFtpControlConnectionListener,
                         public nsIProtocolProxyCallback
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIINPUTSTREAMCALLBACK
    NS_DECL_NSITRANSPORTEVENTSINK
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSIPROTOCOLPROXYCALLBACK

    // Override input stream methods:
    NS_IMETHOD CloseWithStatus(nsresult status) override;
    NS_IMETHOD Available(uint64_t *result) override;
    NS_IMETHOD ReadSegments(nsWriteSegmentFun fun, void *closure,
                            uint32_t count, uint32_t *result) override;

    // nsFtpControlConnectionListener methods:
    virtual void OnControlDataAvailable(const char *data, uint32_t dataLen) override;
    virtual void OnControlError(nsresult status) override;

    nsFtpState();
    nsresult Init(nsFtpChannel *channel);

protected:
    // Notification from nsBaseContentStream::AsyncWait
    virtual void OnCallbackPending() override;

private:
    virtual ~nsFtpState();

    ///////////////////////////////////
    // BEGIN: STATE METHODS
    nsresult        S_user(); FTP_STATE       R_user();
    nsresult        S_pass(); FTP_STATE       R_pass();
    nsresult        S_syst(); FTP_STATE       R_syst();
    nsresult        S_acct(); FTP_STATE       R_acct();

    nsresult        S_type(); FTP_STATE       R_type();
    nsresult        S_cwd();  FTP_STATE       R_cwd();

    nsresult        S_size(); FTP_STATE       R_size();
    nsresult        S_mdtm(); FTP_STATE       R_mdtm();
    nsresult        S_list(); FTP_STATE       R_list();

    nsresult        S_rest(); FTP_STATE       R_rest();
    nsresult        S_retr(); FTP_STATE       R_retr();
    nsresult        S_stor(); FTP_STATE       R_stor();
    nsresult        S_pasv(); FTP_STATE       R_pasv();
    nsresult        S_pwd();  FTP_STATE       R_pwd();
    nsresult        S_feat(); FTP_STATE       R_feat();
    nsresult        S_opts(); FTP_STATE       R_opts();
    // END: STATE METHODS
    ///////////////////////////////////

    // internal methods
    void        MoveToNextState(FTP_STATE nextState);
    nsresult    Process();

    void KillControlConnection();
    nsresult StopProcessing();
    nsresult EstablishControlConnection();
    nsresult SendFTPCommand(const nsCSubstring& command);
    void ConvertFilespecToVMS(nsCString& fileSpec);
    void ConvertDirspecToVMS(nsCString& fileSpec);
    void ConvertDirspecFromVMS(nsCString& fileSpec);
    nsresult BuildStreamConverter(nsIStreamListener** convertStreamListener);
    nsresult SetContentType();

    /**
     * This method is called to kick-off the FTP state machine.  mState is
     * reset to FTP_COMMAND_CONNECT, and the FTP state machine progresses from
     * there.  This method is initially called (indirectly) from the channel's
     * AsyncOpen implementation.
     */
    void Connect();

    ///////////////////////////////////
    // Private members

        // ****** state machine vars
    FTP_STATE           mState;             // the current state
    FTP_STATE           mNextState;         // the next state
    bool                mKeepRunning;       // thread event loop boolean
    int32_t             mResponseCode;      // the last command response code
    nsCString           mResponseMsg;       // the last command response text

        // ****** channel/transport/stream vars 
    RefPtr<nsFtpControlConnection> mControlConnection;       // cacheable control connection (owns mCPipe)
    bool                            mReceivedControlData;  
    bool                            mTryingCachedControl;     // retrying the password
    bool                            mRETRFailed;              // Did we already try a RETR and it failed?
    uint64_t                        mFileSize;
    nsCString                       mModTime;

        // ****** consumer vars
    RefPtr<nsFtpChannel>          mChannel;         // our owning FTP channel we pass through our events
    nsCOMPtr<nsIProxyInfo>          mProxyInfo;

        // ****** connection cache vars
    int32_t             mServerType;    // What kind of server are we talking to

        // ****** protocol interpretation related state vars
    nsString            mUsername;      // username
    nsString            mPassword;      // password
    FTP_ACTION          mAction;        // the higher level action (GET/PUT)
    bool                mAnonymous;     // try connecting anonymous (default)
    bool                mRetryPass;     // retrying the password
    bool                mStorReplyReceived; // FALSE if waiting for STOR
                                            // completion status from server
    nsresult            mInternalError; // represents internal state errors
    bool                mReconnectAndLoginAgain;
    bool                mCacheConnection;

        // ****** URI vars
    int32_t                mPort;       // the port to connect to
    nsString               mFilename;   // url filename (if any)
    nsCString              mPath;       // the url's path
    nsCString              mPwd;        // login Path

        // ****** other vars
    nsCOMPtr<nsITransport>        mDataTransport;
    nsCOMPtr<nsIAsyncInputStream> mDataStream;
    nsCOMPtr<nsIRequest>    mUploadRequest;
    bool                    mAddressChecked;
    bool                    mServerIsIPv6;
    bool                    mUseUTF8;

    mozilla::net::NetAddr   mServerAddress;

    // ***** control read gvars
    nsresult                mControlStatus;
    nsCString               mControlReadCarryOverBuf;

    nsCString mSuppliedEntityID;

    nsCOMPtr<nsICancelable>  mProxyRequest;
    bool                     mDeferredCallbackPending;
};

#endif //__nsFtpConnectionThread__h_
