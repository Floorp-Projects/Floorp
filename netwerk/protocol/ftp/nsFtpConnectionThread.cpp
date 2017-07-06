/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set tw=80 ts=4 sts=4 sw=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ctype.h>

#include "prprf.h"
#include "mozilla/Logging.h"
#include "prtime.h"

#include "nsIOService.h"
#include "nsFTPChannel.h"
#include "nsFtpConnectionThread.h"
#include "nsFtpControlConnection.h"
#include "nsFtpProtocolHandler.h"
#include "netCore.h"
#include "nsCRT.h"
#include "nsEscape.h"
#include "nsMimeTypes.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsIAsyncStreamCopier.h"
#include "nsThreadUtils.h"
#include "nsStreamUtils.h"
#include "nsIURL.h"
#include "nsISocketTransport.h"
#include "nsIStreamListenerTee.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIStringBundle.h"
#include "nsAuthInformationHolder.h"
#include "nsIProtocolProxyService.h"
#include "nsICancelable.h"
#include "nsIOutputStream.h"
#include "nsIPrompt.h"
#include "nsIProtocolHandler.h"
#include "nsIProxyInfo.h"
#include "nsIRunnable.h"
#include "nsISocketTransportService.h"
#include "nsIURI.h"
#include "nsILoadInfo.h"
#include "NullPrincipal.h"
#include "nsIAuthPrompt2.h"
#include "nsIFTPChannelParentInternal.h"

using namespace mozilla;
using namespace mozilla::net;

extern LazyLogModule gFTPLog;
#define LOG(args)         MOZ_LOG(gFTPLog, mozilla::LogLevel::Debug, args)
#define LOG_INFO(args)  MOZ_LOG(gFTPLog, mozilla::LogLevel::Info, args)

// remove FTP parameters (starting with ";") from the path
static void
removeParamsFromPath(nsCString& path)
{
  int32_t index = path.FindChar(';');
  if (index >= 0) {
    path.SetLength(index);
  }
}

NS_IMPL_ISUPPORTS_INHERITED(nsFtpState,
                            nsBaseContentStream,
                            nsIInputStreamCallback, 
                            nsITransportEventSink,
                            nsIRequestObserver,
                            nsIProtocolProxyCallback)

nsFtpState::nsFtpState()
    : nsBaseContentStream(true)
    , mState(FTP_INIT)
    , mNextState(FTP_S_USER)
    , mKeepRunning(true)
    , mReceivedControlData(false)
    , mTryingCachedControl(false)
    , mRETRFailed(false)
    , mFileSize(kJS_MAX_SAFE_UINTEGER)
    , mServerType(FTP_GENERIC_TYPE)
    , mAction(GET)
    , mAnonymous(true)
    , mRetryPass(false)
    , mStorReplyReceived(false)
    , mInternalError(NS_OK)
    , mReconnectAndLoginAgain(false)
    , mCacheConnection(true)
    , mPort(21)
    , mAddressChecked(false)
    , mServerIsIPv6(false)
    , mUseUTF8(false)
    , mControlStatus(NS_OK)
    , mDeferredCallbackPending(false)
{
    LOG_INFO(("FTP:(%p) nsFtpState created", this));

    // make sure handler stays around
    NS_ADDREF(gFtpHandler);
}

nsFtpState::~nsFtpState() 
{
    LOG_INFO(("FTP:(%p) nsFtpState destroyed", this));

    if (mProxyRequest)
        mProxyRequest->Cancel(NS_ERROR_FAILURE);

    // release reference to handler
    nsFtpProtocolHandler *handler = gFtpHandler;
    NS_RELEASE(handler);
}

// nsIInputStreamCallback implementation
NS_IMETHODIMP
nsFtpState::OnInputStreamReady(nsIAsyncInputStream *aInStream)
{
    LOG(("FTP:(%p) data stream ready\n", this));

    // We are receiving a notification from our data stream, so just forward it
    // on to our stream callback.
    if (HasPendingCallback())
        DispatchCallbackSync();

    return NS_OK;
}

void
nsFtpState::OnControlDataAvailable(const char *aData, uint32_t aDataLen)
{
    LOG(("FTP:(%p) control data available [%u]\n", this, aDataLen));
    mControlConnection->WaitData(this);  // queue up another call

    if (!mReceivedControlData) {
        // parameter can be null cause the channel fills them in.
        OnTransportStatus(nullptr, NS_NET_STATUS_BEGIN_FTP_TRANSACTION, 0, 0);
        mReceivedControlData = true;
    }

    // Sometimes we can get two responses in the same packet, eg from LIST.
    // So we need to parse the response line by line

    nsCString buffer = mControlReadCarryOverBuf;

    // Clear the carryover buf - if we still don't have a line, then it will
    // be reappended below
    mControlReadCarryOverBuf.Truncate();

    buffer.Append(aData, aDataLen);

    const char* currLine = buffer.get();
    while (*currLine && mKeepRunning) {
        int32_t eolLength = strcspn(currLine, CRLF);
        int32_t currLineLength = strlen(currLine);

        // if currLine is empty or only contains CR or LF, then bail.  we can
        // sometimes get an ODA event with the full response line + CR without
        // the trailing LF.  the trailing LF might come in the next ODA event.
        // because we are happy enough to process a response line ending only
        // in CR, we need to take care to discard the extra LF (bug 191220).
        if (eolLength == 0 && currLineLength <= 1)
            break;

        if (eolLength == currLineLength) {
            mControlReadCarryOverBuf.Assign(currLine);
            break;
        }

        // Append the current segment, including the LF
        nsAutoCString line;
        int32_t crlfLength = 0;

        if ((currLineLength > eolLength) &&
            (currLine[eolLength] == nsCRT::CR) &&
            (currLine[eolLength+1] == nsCRT::LF)) {
            crlfLength = 2; // CR +LF 
        } else {
            crlfLength = 1; // + LF or CR
        }

        line.Assign(currLine, eolLength + crlfLength);
        
        // Does this start with a response code?
        bool startNum = (line.Length() >= 3 &&
                           isdigit(line[0]) &&
                           isdigit(line[1]) &&
                           isdigit(line[2]));

        if (mResponseMsg.IsEmpty()) {
            // If we get here, then we know that we have a complete line, and
            // that it is the first one

            NS_ASSERTION(line.Length() > 4 && startNum,
                         "Read buffer doesn't include response code");
            
            mResponseCode = atoi(PromiseFlatCString(Substring(line,0,3)).get());
        }

        mResponseMsg.Append(line);

        // This is the last line if its 3 numbers followed by a space
        if (startNum && line[3] == ' ') {
            // yup. last line, let's move on.
            if (mState == mNextState) {
                NS_ERROR("ftp read state mixup");
                mInternalError = NS_ERROR_FAILURE;
                mState = FTP_ERROR;
            } else {
                mState = mNextState;
            }

            nsCOMPtr<nsIFTPEventSink> ftpSink;
            mChannel->GetFTPEventSink(ftpSink);
            if (ftpSink)
                ftpSink->OnFTPControlLog(true, mResponseMsg.get());
            
            nsresult rv = Process();
            mResponseMsg.Truncate();
            if (NS_FAILED(rv)) {
                CloseWithStatus(rv);
                return;
            }
        }

        currLine = currLine + eolLength + crlfLength;
    }
}

void
nsFtpState::OnControlError(nsresult status)
{
    NS_ASSERTION(NS_FAILED(status), "expecting error condition");

    LOG(("FTP:(%p) CC(%p) error [%" PRIx32 " was-cached=%u]\n",
         this, mControlConnection.get(), static_cast<uint32_t>(status),
         mTryingCachedControl));

    mControlStatus = status;
    if (mReconnectAndLoginAgain && NS_SUCCEEDED(mInternalError)) {
        mReconnectAndLoginAgain = false;
        mAnonymous = false;
        mControlStatus = NS_OK;
        Connect();
    } else if (mTryingCachedControl && NS_SUCCEEDED(mInternalError)) {
        mTryingCachedControl = false;
        Connect();
    } else {
        CloseWithStatus(status);
    }
}

nsresult
nsFtpState::EstablishControlConnection()
{
    NS_ASSERTION(!mControlConnection, "we already have a control connection");
            
    nsresult rv;

    LOG(("FTP:(%p) trying cached control\n", this));
        
    // Look to see if we can use a cached control connection:
    RefPtr<nsFtpControlConnection> connection;
    // Don't use cached control if anonymous (bug #473371)
    if (!mChannel->HasLoadFlag(nsIRequest::LOAD_ANONYMOUS))
        gFtpHandler->RemoveConnection(mChannel->URI(), getter_AddRefs(connection));

    if (connection) {
        mControlConnection.swap(connection);
        if (mControlConnection->IsAlive())
        {
            // set stream listener of the control connection to be us.        
            mControlConnection->WaitData(this);
            
            // read cached variables into us. 
            mServerType = mControlConnection->mServerType;           
            mPassword   = mControlConnection->mPassword;
            mPwd        = mControlConnection->mPwd;
            mUseUTF8    = mControlConnection->mUseUTF8;
            mTryingCachedControl = true;

            // we have to set charset to connection if server supports utf-8
            if (mUseUTF8)
                mChannel->SetContentCharset(NS_LITERAL_CSTRING("UTF-8"));
            
            // we're already connected to this server, skip login.
            mState = FTP_S_PASV;
            mResponseCode = 530;  // assume the control connection was dropped.
            mControlStatus = NS_OK;
            mReceivedControlData = false;  // For this request, we have not.

            // if we succeed, return.  Otherwise, we need to create a transport
            rv = mControlConnection->Connect(mChannel->ProxyInfo(), this);
            if (NS_SUCCEEDED(rv))
                return rv;
        }
        LOG(("FTP:(%p) cached CC(%p) is unusable\n", this,
            mControlConnection.get()));

        mControlConnection->WaitData(nullptr);
        mControlConnection = nullptr;
    }

    LOG(("FTP:(%p) creating CC\n", this));
        
    mState = FTP_READ_BUF;
    mNextState = FTP_S_USER;
    
    nsAutoCString host;
    rv = mChannel->URI()->GetAsciiHost(host);
    if (NS_FAILED(rv))
        return rv;

    mControlConnection = new nsFtpControlConnection(host, mPort);
    if (!mControlConnection)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = mControlConnection->Connect(mChannel->ProxyInfo(), this);
    if (NS_FAILED(rv)) {
        LOG(("FTP:(%p) CC(%p) failed to connect [rv=%" PRIx32 "]\n", this,
             mControlConnection.get(), static_cast<uint32_t>(rv)));
        mControlConnection = nullptr;
        return rv;
    }

    return mControlConnection->WaitData(this);
}

void 
nsFtpState::MoveToNextState(FTP_STATE nextState)
{
    if (NS_FAILED(mInternalError)) {
        mState = FTP_ERROR;
        LOG(("FTP:(%p) FAILED (%" PRIx32 ")\n", this, static_cast<uint32_t>(mInternalError)));
    } else {
        mState = FTP_READ_BUF;
        mNextState = nextState;
    }  
}

nsresult
nsFtpState::Process() 
{
    nsresult    rv = NS_OK;
    bool        processingRead = true;
    
    while (mKeepRunning && processingRead) {
        switch (mState) {
          case FTP_COMMAND_CONNECT:
            KillControlConnection();
            LOG(("FTP:(%p) establishing CC", this));
            mInternalError = EstablishControlConnection();  // sets mState
            if (NS_FAILED(mInternalError)) {
                mState = FTP_ERROR;
                LOG(("FTP:(%p) FAILED\n", this));
            } else {
                LOG(("FTP:(%p) SUCCEEDED\n", this));
            }
            break;
    
          case FTP_READ_BUF:
            LOG(("FTP:(%p) Waiting for CC(%p)\n", this,
                mControlConnection.get()));
            processingRead = false;
            break;
          
          case FTP_ERROR: // xx needs more work to handle dropped control connection cases
            if ((mTryingCachedControl && mResponseCode == 530 &&
                mInternalError == NS_ERROR_FTP_PASV) ||
                (mResponseCode == 425 &&
                mInternalError == NS_ERROR_FTP_PASV)) {
                // The user was logged out during an pasv operation
                // we want to restart this request with a new control
                // channel.
                mState = FTP_COMMAND_CONNECT;
            } else if (mResponseCode == 421 && 
                       mInternalError != NS_ERROR_FTP_LOGIN) {
                // The command channel dropped for some reason.
                // Fire it back up, unless we were trying to login
                // in which case the server might just be telling us
                // that the max number of users has been reached...
                mState = FTP_COMMAND_CONNECT;
            } else if (mAnonymous && 
                       mInternalError == NS_ERROR_FTP_LOGIN) {
                // If the login was anonymous, and it failed, try again with a username
                // Don't reuse old control connection, see #386167
                mAnonymous = false;
                mState = FTP_COMMAND_CONNECT;
            } else {
                LOG(("FTP:(%p) FTP_ERROR - calling StopProcessing\n", this));
                rv = StopProcessing();
                NS_ASSERTION(NS_SUCCEEDED(rv), "StopProcessing failed.");
                processingRead = false;
            }
            break;
          
          case FTP_COMPLETE:
            LOG(("FTP:(%p) COMPLETE\n", this));
            rv = StopProcessing();
            NS_ASSERTION(NS_SUCCEEDED(rv), "StopProcessing failed.");
            processingRead = false;
            break;

// USER           
          case FTP_S_USER:
            rv = S_user();
            
            if (NS_FAILED(rv))
                mInternalError = NS_ERROR_FTP_LOGIN;
            
            MoveToNextState(FTP_R_USER);
            break;
            
          case FTP_R_USER:
            mState = R_user();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FTP_LOGIN;
            
            break;
// PASS            
          case FTP_S_PASS:
            rv = S_pass();
            
            if (NS_FAILED(rv))
                mInternalError = NS_ERROR_FTP_LOGIN;
            
            MoveToNextState(FTP_R_PASS);
            break;
            
          case FTP_R_PASS:
            mState = R_pass();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FTP_LOGIN;
            
            break;
// ACCT            
          case FTP_S_ACCT:
            rv = S_acct();
            
            if (NS_FAILED(rv))
                mInternalError = NS_ERROR_FTP_LOGIN;
            
            MoveToNextState(FTP_R_ACCT);
            break;
            
          case FTP_R_ACCT:
            mState = R_acct();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FTP_LOGIN;
            
            break;

// SYST            
          case FTP_S_SYST:
            rv = S_syst();
            
            if (NS_FAILED(rv))
                mInternalError = NS_ERROR_FTP_LOGIN;
            
            MoveToNextState(FTP_R_SYST);
            break;
            
          case FTP_R_SYST:
            mState = R_syst();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FTP_LOGIN;

            break;

// TYPE            
          case FTP_S_TYPE:
            rv = S_type();
            
            if (NS_FAILED(rv))
                mInternalError = rv;
            
            MoveToNextState(FTP_R_TYPE);
            break;
            
          case FTP_R_TYPE:
            mState = R_type();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FAILURE;
            
            break;
// CWD            
          case FTP_S_CWD:
            rv = S_cwd();
            
            if (NS_FAILED(rv))
                mInternalError = NS_ERROR_FTP_CWD;
            
            MoveToNextState(FTP_R_CWD);
            break;
            
          case FTP_R_CWD:
            mState = R_cwd();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FTP_CWD;
            break;
       
// LIST
          case FTP_S_LIST:
            rv = S_list();

            if (rv == NS_ERROR_NOT_RESUMABLE) {
                mInternalError = rv;
            } else if (NS_FAILED(rv)) {
                mInternalError = NS_ERROR_FTP_CWD;
            }
            
            MoveToNextState(FTP_R_LIST);
            break;

          case FTP_R_LIST:        
            mState = R_list();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FAILURE;

            break;

// SIZE            
          case FTP_S_SIZE:
            rv = S_size();
            
            if (NS_FAILED(rv))
                mInternalError = rv;
            
            MoveToNextState(FTP_R_SIZE);
            break;
            
          case FTP_R_SIZE: 
            mState = R_size();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FAILURE;
            
            break;

// REST        
          case FTP_S_REST:
            rv = S_rest();
            
            if (NS_FAILED(rv))
                mInternalError = rv;
            
            MoveToNextState(FTP_R_REST);
            break;
            
          case FTP_R_REST:
            mState = R_rest();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FAILURE;
            
            break;

// MDTM
          case FTP_S_MDTM:
            rv = S_mdtm();
            if (NS_FAILED(rv))
                mInternalError = rv;
            MoveToNextState(FTP_R_MDTM);
            break;

          case FTP_R_MDTM:
            mState = R_mdtm();

            // Don't want to overwrite a more explicit status code
            if (FTP_ERROR == mState && NS_SUCCEEDED(mInternalError))
                mInternalError = NS_ERROR_FAILURE;
            
            break;
            
// RETR        
          case FTP_S_RETR:
            rv = S_retr();
            
            if (NS_FAILED(rv)) 
                mInternalError = rv;
            
            MoveToNextState(FTP_R_RETR);
            break;
            
          case FTP_R_RETR:

            mState = R_retr();
            
            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FAILURE;
            
            break;
            
// STOR        
          case FTP_S_STOR:
            rv = S_stor();

            if (NS_FAILED(rv))
                mInternalError = rv;
            
            MoveToNextState(FTP_R_STOR);
            break;
            
          case FTP_R_STOR:
            mState = R_stor();

            if (FTP_ERROR == mState)
                mInternalError = NS_ERROR_FAILURE;

            break;
            
// PASV        
          case FTP_S_PASV:
            rv = S_pasv();

            if (NS_FAILED(rv))
                mInternalError = NS_ERROR_FTP_PASV;
            
            MoveToNextState(FTP_R_PASV);
            break;
            
          case FTP_R_PASV:
            mState = R_pasv();

            if (FTP_ERROR == mState) 
                mInternalError = NS_ERROR_FTP_PASV;

            break;
            
// PWD        
          case FTP_S_PWD:
            rv = S_pwd();

            if (NS_FAILED(rv))
                mInternalError = NS_ERROR_FTP_PWD;
            
            MoveToNextState(FTP_R_PWD);
            break;
            
          case FTP_R_PWD:
            mState = R_pwd();

            if (FTP_ERROR == mState) 
                mInternalError = NS_ERROR_FTP_PWD;

            break;

// FEAT for RFC2640 support
          case FTP_S_FEAT:
            rv = S_feat();

            if (NS_FAILED(rv))
                mInternalError = rv;

            MoveToNextState(FTP_R_FEAT);
            break;

          case FTP_R_FEAT:
            mState = R_feat();

            // Don't want to overwrite a more explicit status code
            if (FTP_ERROR == mState && NS_SUCCEEDED(mInternalError))
                mInternalError = NS_ERROR_FAILURE;
            break;

// OPTS for some non-RFC2640-compliant servers support
          case FTP_S_OPTS:
            rv = S_opts();

            if (NS_FAILED(rv))
                mInternalError = rv;

            MoveToNextState(FTP_R_OPTS);
            break;

          case FTP_R_OPTS:
            mState = R_opts();

            // Don't want to overwrite a more explicit status code
            if (FTP_ERROR == mState && NS_SUCCEEDED(mInternalError))
                mInternalError = NS_ERROR_FAILURE;
            break;

          default:
            ;
            
        }
    }

    return rv;
}

///////////////////////////////////
// STATE METHODS
///////////////////////////////////
nsresult
nsFtpState::S_user() {
    // some servers on connect send us a 421 or 521.  (84525) (141784)
    if ((mResponseCode == 421) || (mResponseCode == 521))
        return NS_ERROR_FAILURE;

    nsresult rv;
    nsAutoCString usernameStr("USER ");

    mResponseMsg = "";

    if (mAnonymous) {
        mReconnectAndLoginAgain = true;
        usernameStr.AppendLiteral("anonymous");
    } else {
        mReconnectAndLoginAgain = false;
        if (mUsername.IsEmpty()) {

            // No prompt for anonymous requests (bug #473371)
            if (mChannel->HasLoadFlag(nsIRequest::LOAD_ANONYMOUS))
              return NS_ERROR_FAILURE;

            nsCOMPtr<nsIAuthPrompt2> prompter;
            NS_QueryAuthPrompt2(static_cast<nsIChannel*>(mChannel),
                                getter_AddRefs(prompter));
            if (!prompter)
                return NS_ERROR_NOT_INITIALIZED;

            RefPtr<nsAuthInformationHolder> info =
                new nsAuthInformationHolder(nsIAuthInformation::AUTH_HOST,
                                            EmptyString(),
                                            EmptyCString());

            bool retval;
            rv = prompter->PromptAuth(mChannel, nsIAuthPrompt2::LEVEL_NONE,
                                      info, &retval);

            // if the user canceled or didn't supply a username we want to fail
            if (NS_FAILED(rv) || !retval || info->User().IsEmpty())
                return NS_ERROR_FAILURE;

            mUsername = info->User();
            mPassword = info->Password();
        }
        // XXX Is UTF-8 the best choice?
        AppendUTF16toUTF8(mUsername, usernameStr);
    }
    usernameStr.Append(CRLF);

    return SendFTPCommand(usernameStr);
}

FTP_STATE
nsFtpState::R_user() {
    mReconnectAndLoginAgain = false;
    if (mResponseCode/100 == 3) {
        // send off the password
        return FTP_S_PASS;
    }
    if (mResponseCode/100 == 2) {
        // no password required, we're already logged in
        return FTP_S_SYST;
    }
    if (mResponseCode/100 == 5) {
        // problem logging in. typically this means the server
        // has reached it's user limit.
        return FTP_ERROR;
    }
    // LOGIN FAILED
    return FTP_ERROR;
}


nsresult
nsFtpState::S_pass() {
    nsresult rv;
    nsAutoCString passwordStr("PASS ");

    mResponseMsg = "";

    if (mAnonymous) {
        if (!mPassword.IsEmpty()) {
            // XXX Is UTF-8 the best choice?
            AppendUTF16toUTF8(mPassword, passwordStr);
        } else {
            nsXPIDLCString anonPassword;
            bool useRealEmail = false;
            nsCOMPtr<nsIPrefBranch> prefs =
                    do_GetService(NS_PREFSERVICE_CONTRACTID);
            if (prefs) {
                rv = prefs->GetBoolPref("advanced.mailftp", &useRealEmail);
                if (NS_SUCCEEDED(rv) && useRealEmail) {
                    prefs->GetCharPref("network.ftp.anonymous_password",
                                       getter_Copies(anonPassword));
                }
            }
            if (!anonPassword.IsEmpty()) {
                passwordStr.AppendASCII(anonPassword);
            } else {
                // We need to default to a valid email address - bug 101027
                // example.com is reserved (rfc2606), so use that
                passwordStr.AppendLiteral("mozilla@example.com");
            }
        }
    } else {
        if (mPassword.IsEmpty() || mRetryPass) {
            
            // No prompt for anonymous requests (bug #473371)
            if (mChannel->HasLoadFlag(nsIRequest::LOAD_ANONYMOUS))
                return NS_ERROR_FAILURE;

            nsCOMPtr<nsIAuthPrompt2> prompter;
            NS_QueryAuthPrompt2(static_cast<nsIChannel*>(mChannel),
                                getter_AddRefs(prompter));
            if (!prompter)
                return NS_ERROR_NOT_INITIALIZED;

            RefPtr<nsAuthInformationHolder> info =
                new nsAuthInformationHolder(nsIAuthInformation::AUTH_HOST |
                                            nsIAuthInformation::ONLY_PASSWORD,
                                            EmptyString(),
                                            EmptyCString());

            info->SetUserInternal(mUsername);

            bool retval;
            rv = prompter->PromptAuth(mChannel, nsIAuthPrompt2::LEVEL_NONE,
                                      info, &retval);

            // we want to fail if the user canceled. Note here that if they want
            // a blank password, we will pass it along.
            if (NS_FAILED(rv) || !retval)
                return NS_ERROR_FAILURE;

            mPassword = info->Password();
        }
        // XXX Is UTF-8 the best choice?
        AppendUTF16toUTF8(mPassword, passwordStr);
    }
    passwordStr.Append(CRLF);

    return SendFTPCommand(passwordStr);
}

FTP_STATE
nsFtpState::R_pass() {
    if (mResponseCode/100 == 3) {
        // send account info
        return FTP_S_ACCT;
    }
    if (mResponseCode/100 == 2) {
        // logged in
        return FTP_S_SYST;
    }
    if (mResponseCode == 503) {
        // start over w/ the user command.
        // note: the password was successful, and it's stored in mPassword
        mRetryPass = false;
        return FTP_S_USER;
    }
    if (mResponseCode/100 == 5 || mResponseCode==421) {
        // There is no difference between a too-many-users error,
        // a wrong-password error, or any other sort of error

        if (!mAnonymous)
            mRetryPass = true;

        return FTP_ERROR;
    }
    // unexpected response code
    return FTP_ERROR;
}

nsresult
nsFtpState::S_pwd() {
    return SendFTPCommand(NS_LITERAL_CSTRING("PWD" CRLF));
}

FTP_STATE
nsFtpState::R_pwd() {
    // Error response to PWD command isn't fatal, but don't cache the connection
    // if CWD command is sent since correct mPwd is needed for further requests.
    if (mResponseCode/100 != 2)
        return FTP_S_TYPE;

    nsAutoCString respStr(mResponseMsg);
    int32_t pos = respStr.FindChar('"');
    if (pos > -1) {
        respStr.Cut(0, pos+1);
        pos = respStr.FindChar('"');
        if (pos > -1) {
            respStr.Truncate(pos);
            if (mServerType == FTP_VMS_TYPE)
                ConvertDirspecFromVMS(respStr);
            if (respStr.IsEmpty() || respStr.Last() != '/')
                respStr.Append('/');
            mPwd = respStr;
        }
    }
    return FTP_S_TYPE;
}

nsresult
nsFtpState::S_syst() {
    return SendFTPCommand(NS_LITERAL_CSTRING("SYST" CRLF));
}

FTP_STATE
nsFtpState::R_syst() {
    if (mResponseCode/100 == 2) {
        if (( mResponseMsg.Find("L8") > -1) || 
            ( mResponseMsg.Find("UNIX") > -1) || 
            ( mResponseMsg.Find("BSD") > -1) ||
            ( mResponseMsg.Find("MACOS Peter's Server") > -1) ||
            ( mResponseMsg.Find("MACOS WebSTAR FTP") > -1) ||
            ( mResponseMsg.Find("MVS") > -1) ||
            ( mResponseMsg.Find("OS/390") > -1) ||
            ( mResponseMsg.Find("OS/400") > -1)) {
            mServerType = FTP_UNIX_TYPE;
        } else if (( mResponseMsg.Find("WIN32", true) > -1) ||
                   ( mResponseMsg.Find("windows", true) > -1)) {
            mServerType = FTP_NT_TYPE;
        } else if (mResponseMsg.Find("OS/2", true) > -1) {
            mServerType = FTP_OS2_TYPE;
        } else if (mResponseMsg.Find("VMS", true) > -1) {
            mServerType = FTP_VMS_TYPE;
        } else {
            NS_ERROR("Server type list format unrecognized.");
            // Guessing causes crashes.
            // (Of course, the parsing code should be more robust...)
            nsCOMPtr<nsIStringBundleService> bundleService =
                do_GetService(NS_STRINGBUNDLE_CONTRACTID);
            if (!bundleService)
                return FTP_ERROR;

            nsCOMPtr<nsIStringBundle> bundle;
            nsresult rv = bundleService->CreateBundle(NECKO_MSGS_URL,
                                                      getter_AddRefs(bundle));
            if (NS_FAILED(rv))
                return FTP_ERROR;
            
            char16_t* ucs2Response = ToNewUnicode(mResponseMsg);
            const char16_t *formatStrings[1] = { ucs2Response };

            nsXPIDLString formattedString;
            rv = bundle->FormatStringFromName(u"UnsupportedFTPServer", formatStrings, 1,
                                              getter_Copies(formattedString));
            free(ucs2Response);
            if (NS_FAILED(rv))
                return FTP_ERROR;

            // TODO(darin): this code should not be dictating UI like this!
            nsCOMPtr<nsIPrompt> prompter;
            mChannel->GetCallback(prompter);
            if (prompter)
                prompter->Alert(nullptr, formattedString.get());
            
            // since we just alerted the user, clear mResponseMsg,
            // which is displayed to the user.
            mResponseMsg = "";
            return FTP_ERROR;
        }
        
        return FTP_S_FEAT;
    }

    if (mResponseCode/100 == 5) {   
        // server didn't like the SYST command.  Probably (500, 501, 502)
        // No clue.  We will just hope it is UNIX type server.
        mServerType = FTP_UNIX_TYPE;

        return FTP_S_FEAT;
    }
    return FTP_ERROR;
}

nsresult
nsFtpState::S_acct() {
    return SendFTPCommand(NS_LITERAL_CSTRING("ACCT noaccount" CRLF));
}

FTP_STATE
nsFtpState::R_acct() {
    if (mResponseCode/100 == 2)
        return FTP_S_SYST;

    return FTP_ERROR;
}

nsresult
nsFtpState::S_type() {
    return SendFTPCommand(NS_LITERAL_CSTRING("TYPE I" CRLF));
}

FTP_STATE
nsFtpState::R_type() {
    if (mResponseCode/100 != 2) 
        return FTP_ERROR;
    
    return FTP_S_PASV;
}

nsresult
nsFtpState::S_cwd() {
    // Don't cache the connection if PWD command failed
    if (mPwd.IsEmpty())
        mCacheConnection = false;

    nsAutoCString cwdStr;
    if (mAction != PUT)
        cwdStr = mPath;
    if (cwdStr.IsEmpty() || cwdStr.First() != '/')
        cwdStr.Insert(mPwd,0);
    if (mServerType == FTP_VMS_TYPE)
        ConvertDirspecToVMS(cwdStr);
    cwdStr.Insert("CWD ",0);
    cwdStr.Append(CRLF);

    return SendFTPCommand(cwdStr);
}

FTP_STATE
nsFtpState::R_cwd() {
    if (mResponseCode/100 == 2) {
        if (mAction == PUT)
            return FTP_S_STOR;
        
        return FTP_S_LIST;
    }
    
    return FTP_ERROR;
}

nsresult
nsFtpState::S_size() {
    nsAutoCString sizeBuf(mPath);
    if (sizeBuf.IsEmpty() || sizeBuf.First() != '/')
        sizeBuf.Insert(mPwd,0);
    if (mServerType == FTP_VMS_TYPE)
        ConvertFilespecToVMS(sizeBuf);
    sizeBuf.Insert("SIZE ",0);
    sizeBuf.Append(CRLF);

    return SendFTPCommand(sizeBuf);
}

FTP_STATE
nsFtpState::R_size() {
    if (mResponseCode/100 == 2) {
        PR_sscanf(mResponseMsg.get() + 4, "%llu", &mFileSize);
        mChannel->SetContentLength(mFileSize);
    }

    // We may want to be able to resume this
    return FTP_S_MDTM;
}

nsresult
nsFtpState::S_mdtm() {
    nsAutoCString mdtmBuf(mPath);
    if (mdtmBuf.IsEmpty() || mdtmBuf.First() != '/')
        mdtmBuf.Insert(mPwd,0);
    if (mServerType == FTP_VMS_TYPE)
        ConvertFilespecToVMS(mdtmBuf);
    mdtmBuf.Insert("MDTM ",0);
    mdtmBuf.Append(CRLF);

    return SendFTPCommand(mdtmBuf);
}

FTP_STATE
nsFtpState::R_mdtm() {
    if (mResponseCode == 213) {
        mResponseMsg.Cut(0,4);
        mResponseMsg.Trim(" \t\r\n");
        // yyyymmddhhmmss
        if (mResponseMsg.Length() != 14) {
            NS_ASSERTION(mResponseMsg.Length() == 14, "Unknown MDTM response");
        } else {
            mModTime = mResponseMsg;

            // Save lastModified time for downloaded files.
            nsAutoCString timeString;
            nsresult error;
            PRExplodedTime exTime;

            mResponseMsg.Mid(timeString, 0, 4);
            exTime.tm_year  = timeString.ToInteger(&error);
            mResponseMsg.Mid(timeString, 4, 2);
            exTime.tm_month = timeString.ToInteger(&error) - 1; //january = 0
            mResponseMsg.Mid(timeString, 6, 2);
            exTime.tm_mday  = timeString.ToInteger(&error);
            mResponseMsg.Mid(timeString, 8, 2);
            exTime.tm_hour  = timeString.ToInteger(&error);
            mResponseMsg.Mid(timeString, 10, 2);
            exTime.tm_min   = timeString.ToInteger(&error);
            mResponseMsg.Mid(timeString, 12, 2);
            exTime.tm_sec   = timeString.ToInteger(&error);
            exTime.tm_usec  = 0;

            exTime.tm_params.tp_gmt_offset = 0;
            exTime.tm_params.tp_dst_offset = 0;

            PR_NormalizeTime(&exTime, PR_GMTParameters);
            exTime.tm_params = PR_LocalTimeParameters(&exTime);

            PRTime time = PR_ImplodeTime(&exTime);
            (void)mChannel->SetLastModifiedTime(time);
        }
    }

    nsCString entityID;
    entityID.Truncate();
    entityID.AppendInt(int64_t(mFileSize));
    entityID.Append('/');
    entityID.Append(mModTime);
    mChannel->SetEntityID(entityID);

    // We weren't asked to resume
    if (!mChannel->ResumeRequested())
        return FTP_S_RETR;

    //if (our entityID == supplied one (if any))
    if (mSuppliedEntityID.IsEmpty() || entityID.Equals(mSuppliedEntityID))
        return FTP_S_REST;

    mInternalError = NS_ERROR_ENTITY_CHANGED;
    mResponseMsg.Truncate();
    return FTP_ERROR;
}

nsresult 
nsFtpState::SetContentType()
{
    // FTP directory URLs don't always end in a slash.  Make sure they do.
    // This check needs to be here rather than a more obvious place
    // (e.g. LIST command processing) so that it ensures the terminating
    // slash is appended for the new request case.

    if (!mPath.IsEmpty() && mPath.Last() != '/') {
        nsCOMPtr<nsIURL> url = (do_QueryInterface(mChannel->URI()));
        nsAutoCString filePath;
        if(NS_SUCCEEDED(url->GetFilePath(filePath))) {
            filePath.Append('/');
            url->SetFilePath(filePath);
        }
    }
    return mChannel->SetContentType(
        NS_LITERAL_CSTRING(APPLICATION_HTTP_INDEX_FORMAT));
}

nsresult
nsFtpState::S_list() {
    nsresult rv = SetContentType();
    if (NS_FAILED(rv)) 
        // XXX Invalid cast of FTP_STATE to nsresult -- FTP_ERROR has
        // value < 0x80000000 and will pass NS_SUCCEEDED() (bug 778109)
        return (nsresult)FTP_ERROR;

    rv = mChannel->PushStreamConverter("text/ftp-dir",
                                       APPLICATION_HTTP_INDEX_FORMAT);
    if (NS_FAILED(rv)) {
        // clear mResponseMsg which is displayed to the user.
        // TODO: we should probably set this to something meaningful.
        mResponseMsg = "";
        return rv;
    }

    // dir listings aren't resumable
    NS_ENSURE_TRUE(!mChannel->ResumeRequested(), NS_ERROR_NOT_RESUMABLE);

    mChannel->SetEntityID(EmptyCString());

    const char *listString;
    if (mServerType == FTP_VMS_TYPE) {
        listString = "LIST *.*;0" CRLF;
    } else {
        listString = "LIST" CRLF;
    }

    return SendFTPCommand(nsDependentCString(listString));
}

FTP_STATE
nsFtpState::R_list() {
    if (mResponseCode/100 == 1) {
        // OK, time to start reading from the data connection.
        if (mDataStream && HasPendingCallback())
            mDataStream->AsyncWait(this, 0, 0, CallbackTarget());
        return FTP_READ_BUF;
    }

    if (mResponseCode/100 == 2) {
        //(DONE)
        mNextState = FTP_COMPLETE;
        return FTP_COMPLETE;
    }
    return FTP_ERROR;
}

nsresult
nsFtpState::S_retr() {
    nsAutoCString retrStr(mPath);
    if (retrStr.IsEmpty() || retrStr.First() != '/')
        retrStr.Insert(mPwd,0);
    if (mServerType == FTP_VMS_TYPE)
        ConvertFilespecToVMS(retrStr);
    retrStr.Insert("RETR ",0);
    retrStr.Append(CRLF);
    return SendFTPCommand(retrStr);
}

FTP_STATE
nsFtpState::R_retr() {
    if (mResponseCode/100 == 2) {
        //(DONE)
        mNextState = FTP_COMPLETE;
        return FTP_COMPLETE;
    }

    if (mResponseCode/100 == 1) {
        if (mDataStream && HasPendingCallback())
            mDataStream->AsyncWait(this, 0, 0, CallbackTarget());
        return FTP_READ_BUF;
    }
    
    // These error codes are related to problems with the connection.  
    // If we encounter any at this point, do not try CWD and abort.
    if (mResponseCode == 421 || mResponseCode == 425 || mResponseCode == 426)
        return FTP_ERROR;

    if (mResponseCode/100 == 5) {
        mRETRFailed = true;
        return FTP_S_PASV;
    }

    return FTP_S_CWD;
}


nsresult
nsFtpState::S_rest() {
    
    nsAutoCString restString("REST ");
    // The int64_t cast is needed to avoid ambiguity
    restString.AppendInt(int64_t(mChannel->StartPos()), 10);
    restString.Append(CRLF);

    return SendFTPCommand(restString);
}

FTP_STATE
nsFtpState::R_rest() {
    if (mResponseCode/100 == 4) {
        // If REST fails, then we can't resume
        mChannel->SetEntityID(EmptyCString());

        mInternalError = NS_ERROR_NOT_RESUMABLE;
        mResponseMsg.Truncate();

        return FTP_ERROR;
    }
   
    return FTP_S_RETR; 
}

nsresult
nsFtpState::S_stor() {
    NS_ENSURE_STATE(mChannel->UploadStream());

    NS_ASSERTION(mAction == PUT, "Wrong state to be here");
    
    nsCOMPtr<nsIURL> url = do_QueryInterface(mChannel->URI());
    NS_ASSERTION(url, "I thought you were a nsStandardURL");

    nsAutoCString storStr;
    url->GetFilePath(storStr);
    NS_ASSERTION(!storStr.IsEmpty(), "What does it mean to store a empty path");
        
    // kill the first slash since we want to be relative to CWD.
    if (storStr.First() == '/')
        storStr.Cut(0,1);

    if (mServerType == FTP_VMS_TYPE)
        ConvertFilespecToVMS(storStr);

    NS_UnescapeURL(storStr);
    storStr.Insert("STOR ",0);
    storStr.Append(CRLF);

    return SendFTPCommand(storStr);
}

FTP_STATE
nsFtpState::R_stor() {
    if (mResponseCode/100 == 2) {
        //(DONE)
        mNextState = FTP_COMPLETE;
        mStorReplyReceived = true;

        // Call Close() if it was not called in nsFtpState::OnStoprequest()
        if (!mUploadRequest && !IsClosed())
            Close();

        return FTP_COMPLETE;
    }

    if (mResponseCode/100 == 1) {
        LOG(("FTP:(%p) writing on DT\n", this));
        return FTP_READ_BUF;
    }

   mStorReplyReceived = true;
   return FTP_ERROR;
}


nsresult
nsFtpState::S_pasv() {
    if (!mAddressChecked) {
        // Find socket address
        mAddressChecked = true;
        mServerAddress.raw.family = AF_INET;
        mServerAddress.inet.ip = htonl(INADDR_ANY);
        mServerAddress.inet.port = htons(0);

        nsITransport *controlSocket = mControlConnection->Transport();
        if (!controlSocket)
            // XXX Invalid cast of FTP_STATE to nsresult -- FTP_ERROR has
            // value < 0x80000000 and will pass NS_SUCCEEDED() (bug 778109)
            return (nsresult)FTP_ERROR;

        nsCOMPtr<nsISocketTransport> sTrans = do_QueryInterface(controlSocket);
        if (sTrans) {
            nsresult rv = sTrans->GetPeerAddr(&mServerAddress);
            if (NS_SUCCEEDED(rv)) {
                if (!IsIPAddrAny(&mServerAddress))
                    mServerIsIPv6 = (mServerAddress.raw.family == AF_INET6) &&
                                    !IsIPAddrV4Mapped(&mServerAddress);
                else {
                    /*
                     * In case of SOCKS5 remote DNS resolution, we do
                     * not know the remote IP address. Still, if it is
                     * an IPV6 host, then the external address of the
                     * socks server should also be IPv6, and this is the
                     * self address of the transport.
                     */
                    NetAddr selfAddress;
                    rv = sTrans->GetSelfAddr(&selfAddress);
                    if (NS_SUCCEEDED(rv))
                        mServerIsIPv6 = (selfAddress.raw.family == AF_INET6) &&
                                        !IsIPAddrV4Mapped(&selfAddress);
                }
            }
        }
    }

    const char *string;
    if (mServerIsIPv6) {
        string = "EPSV" CRLF;
    } else {
        string = "PASV" CRLF;
    }

    return SendFTPCommand(nsDependentCString(string));
    
}

FTP_STATE
nsFtpState::R_pasv() {
    if (mResponseCode/100 != 2)
        return FTP_ERROR;

    nsresult rv;
    int32_t port;

    nsAutoCString responseCopy(mResponseMsg);
    char *response = responseCopy.BeginWriting();

    char *ptr = response;

    // Make sure to ignore the address in the PASV response (bug 370559)

    if (mServerIsIPv6) {
        // The returned string is of the form
        // text (|||ppp|)
        // Where '|' can be any single character
        char delim;
        while (*ptr && *ptr != '(')
            ptr++;
        if (*ptr++ != '(')
            return FTP_ERROR;
        delim = *ptr++;
        if (!delim || *ptr++ != delim ||
                      *ptr++ != delim || 
                      *ptr < '0' || *ptr > '9')
            return FTP_ERROR;
        port = 0;
        do {
            port = port * 10 + *ptr++ - '0';
        } while (*ptr >= '0' && *ptr <= '9');
        if (*ptr++ != delim || *ptr != ')')
            return FTP_ERROR;
    } else {
        // The returned address string can be of the form
        // (xxx,xxx,xxx,xxx,ppp,ppp) or
        //  xxx,xxx,xxx,xxx,ppp,ppp (without parens)
        int32_t h0, h1, h2, h3, p0, p1;

        int32_t fields = 0;
        // First try with parens
        while (*ptr && *ptr != '(')
            ++ptr;
        if (*ptr) {
            ++ptr;
            fields = PR_sscanf(ptr, 
                               "%ld,%ld,%ld,%ld,%ld,%ld",
                               &h0, &h1, &h2, &h3, &p0, &p1);
        }
        if (!*ptr || fields < 6) {
            // OK, lets try w/o parens
            ptr = response;
            while (*ptr && *ptr != ',')
                ++ptr;
            if (*ptr) {
                // backup to the start of the digits
                do {
                    ptr--;
                } while ((ptr >=response) && (*ptr >= '0') && (*ptr <= '9'));
                ptr++; // get back onto the numbers
                fields = PR_sscanf(ptr, 
                                   "%ld,%ld,%ld,%ld,%ld,%ld",
                                   &h0, &h1, &h2, &h3, &p0, &p1);
            }
        }

        NS_ASSERTION(fields == 6, "Can't parse PASV response");
        if (fields < 6)
            return FTP_ERROR;

        port = ((int32_t) (p0<<8)) + p1;
    }

    bool newDataConn = true;
    if (mDataTransport) {
        // Reuse this connection only if its still alive, and the port
        // is the same
        nsCOMPtr<nsISocketTransport> strans = do_QueryInterface(mDataTransport);
        if (strans) {
            int32_t oldPort;
            nsresult rv = strans->GetPort(&oldPort);
            if (NS_SUCCEEDED(rv)) {
                if (oldPort == port) {
                    bool isAlive;
                    if (NS_SUCCEEDED(strans->IsAlive(&isAlive)) && isAlive)
                        newDataConn = false;
                }
            }
        }

        if (newDataConn) {
            mDataTransport->Close(NS_ERROR_ABORT);
            mDataTransport = nullptr;
            mDataStream = nullptr;
        }
    }

    if (newDataConn) {
        // now we know where to connect our data channel
        nsCOMPtr<nsISocketTransportService> sts =
            do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID);
        if (!sts)
            return FTP_ERROR;
       
        nsCOMPtr<nsISocketTransport> strans;

        nsAutoCString host;
        if (!IsIPAddrAny(&mServerAddress)) {
            char buf[kIPv6CStrBufSize];
            NetAddrToString(&mServerAddress, buf, sizeof(buf));
            host.Assign(buf);
        } else {
            /*
             * In case of SOCKS5 remote DNS resolving, the peer address
             * fetched previously will be invalid (0.0.0.0): it is unknown
             * to us. But we can pass on the original hostname to the
             * connect for the data connection.
             */
            rv = mChannel->URI()->GetAsciiHost(host);
            if (NS_FAILED(rv))
                return FTP_ERROR;
        }

        rv =  sts->CreateTransport(nullptr, 0, host,
                                   port, mChannel->ProxyInfo(),
                                   getter_AddRefs(strans)); // the data socket
        if (NS_FAILED(rv))
            return FTP_ERROR;
        mDataTransport = strans;

        strans->SetQoSBits(gFtpHandler->GetDataQoSBits());
        
        LOG(("FTP:(%p) created DT (%s:%x)\n", this, host.get(), port));
        
        // hook ourself up as a proxy for status notifications
        rv = mDataTransport->SetEventSink(this, GetCurrentThreadEventTarget());
        NS_ENSURE_SUCCESS(rv, FTP_ERROR);

        if (mAction == PUT) {
            NS_ASSERTION(!mRETRFailed, "Failed before uploading");

            // nsIUploadChannel requires the upload stream to support ReadSegments.
            // therefore, we can open an unbuffered socket output stream.
            nsCOMPtr<nsIOutputStream> output;
            rv = mDataTransport->OpenOutputStream(nsITransport::OPEN_UNBUFFERED,
                                                  0, 0, getter_AddRefs(output));
            if (NS_FAILED(rv))
                return FTP_ERROR;

            // perform the data copy on the socket transport thread.  we do this
            // because "output" is a socket output stream, so the result is that
            // all work will be done on the socket transport thread.
            nsCOMPtr<nsIEventTarget> stEventTarget =
                do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID);
            if (!stEventTarget)
                return FTP_ERROR;
            
            nsCOMPtr<nsIAsyncStreamCopier> copier;
            rv = NS_NewAsyncStreamCopier(getter_AddRefs(copier),
                                         mChannel->UploadStream(),
                                         output,
                                         stEventTarget,
                                         true,   // upload stream is buffered
                                         false); // output is NOT buffered
            if (NS_FAILED(rv))
                return FTP_ERROR;
        
            rv = copier->AsyncCopy(this, nullptr);
            if (NS_FAILED(rv))
                return FTP_ERROR;

            // hold a reference to the copier so we can cancel it if necessary.
            mUploadRequest = copier;

            // update the current working directory before sending the STOR
            // command.  this is needed since we might be reusing a control
            // connection.
            return FTP_S_CWD;
        }

        //
        // else, we are reading from the data connection...
        //

        // open a buffered, asynchronous socket input stream
        nsCOMPtr<nsIInputStream> input;
        rv = mDataTransport->OpenInputStream(0,
                                             nsIOService::gDefaultSegmentSize,
                                             nsIOService::gDefaultSegmentCount,
                                             getter_AddRefs(input));
        NS_ENSURE_SUCCESS(rv, FTP_ERROR);
        mDataStream = do_QueryInterface(input);
    }

    if (mRETRFailed || mPath.IsEmpty() || mPath.Last() == '/')
        return FTP_S_CWD;
    return FTP_S_SIZE;
}

nsresult
nsFtpState::S_feat() {
    return SendFTPCommand(NS_LITERAL_CSTRING("FEAT" CRLF));
}

FTP_STATE
nsFtpState::R_feat() {
    if (mResponseCode/100 == 2) {
        if (mResponseMsg.Find(NS_LITERAL_CSTRING(CRLF " UTF8" CRLF), true) > -1) {
            // This FTP server supports UTF-8 encoding
            mChannel->SetContentCharset(NS_LITERAL_CSTRING("UTF-8"));
            mUseUTF8 = true;
            return FTP_S_OPTS;
        }
    }

    mUseUTF8 = false;
    return FTP_S_PWD;
}

nsresult
nsFtpState::S_opts() {
     // This command is for compatibility of old FTP spec (IETF Draft)
    return SendFTPCommand(NS_LITERAL_CSTRING("OPTS UTF8 ON" CRLF));
}

FTP_STATE
nsFtpState::R_opts() {
    // Ignore error code because "OPTS UTF8 ON" is for compatibility of
    // FTP server using IETF draft
    return FTP_S_PWD;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

nsresult
nsFtpState::Init(nsFtpChannel *channel)
{
    // parameter validation
    NS_ASSERTION(channel, "FTP: needs a channel");

    mChannel = channel; // a straight ref ptr to the channel

    mKeepRunning = true;
    mSuppliedEntityID = channel->EntityID();

    if (channel->UploadStream())
        mAction = PUT;

    nsresult rv;
    nsCOMPtr<nsIURL> url = do_QueryInterface(mChannel->URI());

    nsAutoCString host;
    if (url) {
        rv = url->GetAsciiHost(host);
    } else {
        rv = mChannel->URI()->GetAsciiHost(host);
    }
    if (NS_FAILED(rv) || host.IsEmpty()) {
        return NS_ERROR_MALFORMED_URI;
    }

    nsAutoCString path;
    if (url) {
        rv = url->GetFilePath(path);
    } else {
        rv = mChannel->URI()->GetPath(path);
    }
    if (NS_FAILED(rv))
        return rv;

    removeParamsFromPath(path);
    
    // FTP parameters such as type=i are ignored
    if (url) {
        url->SetFilePath(path);
    } else {
        mChannel->URI()->SetPath(path);
    }
        
    // Skip leading slash
    char *fwdPtr = path.BeginWriting();
    if (!fwdPtr)
        return NS_ERROR_OUT_OF_MEMORY;
    if (*fwdPtr == '/')
        fwdPtr++;
    if (*fwdPtr != '\0') {
        // now unescape it... %xx reduced inline to resulting character
        int32_t len = NS_UnescapeURL(fwdPtr);
        mPath.Assign(fwdPtr, len);

#ifdef DEBUG
        if (mPath.FindCharInSet(CRLF) >= 0)
            NS_ERROR("NewURI() should've prevented this!!!");
#endif
    }

    // pull any username and/or password out of the uri
    nsAutoCString uname;
    rv = mChannel->URI()->GetUsername(uname);
    if (NS_FAILED(rv))
        return rv;

    if (!uname.IsEmpty() && !uname.EqualsLiteral("anonymous")) {
        mAnonymous = false;
        CopyUTF8toUTF16(NS_UnescapeURL(uname), mUsername);
        
        // return an error if we find a CR or LF in the username
        if (uname.FindCharInSet(CRLF) >= 0)
            return NS_ERROR_MALFORMED_URI;
    }

    nsAutoCString password;
    rv = mChannel->URI()->GetPassword(password);
    if (NS_FAILED(rv))
        return rv;

    CopyUTF8toUTF16(NS_UnescapeURL(password), mPassword);

    // return an error if we find a CR or LF in the password
    if (mPassword.FindCharInSet(CRLF) >= 0)
        return NS_ERROR_MALFORMED_URI;

    int32_t port;
    rv = mChannel->URI()->GetPort(&port);
    if (NS_FAILED(rv))
        return rv;

    if (port > 0)
        mPort = port;

    // Lookup Proxy information asynchronously if it isn't already set
    // on the channel and if we aren't configured explicitly to go directly
    nsCOMPtr<nsIProtocolProxyService> pps =
        do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID);

    if (pps && !mChannel->ProxyInfo()) {
        pps->AsyncResolve(static_cast<nsIChannel*>(mChannel), 0, this, nullptr,
                          getter_AddRefs(mProxyRequest));
    }

    return NS_OK;
}

void
nsFtpState::Connect()
{
    mState = FTP_COMMAND_CONNECT;
    mNextState = FTP_S_USER;

    nsresult rv = Process();

    // check for errors.
    if (NS_FAILED(rv)) {
        LOG(("FTP:Process() failed: %" PRIx32 "\n", static_cast<uint32_t>(rv)));
        mInternalError = NS_ERROR_FAILURE;
        mState = FTP_ERROR;
        CloseWithStatus(mInternalError);
    }
}

void
nsFtpState::KillControlConnection()
{
    mControlReadCarryOverBuf.Truncate(0);

    mAddressChecked = false;
    mServerIsIPv6 = false;

    // if everything went okay, save the connection. 
    // FIX: need a better way to determine if we can cache the connections.
    //      there are some errors which do not mean that we need to kill the connection
    //      e.g. fnf.

    if (!mControlConnection)
        return;

    // kill the reference to ourselves in the control connection.
    mControlConnection->WaitData(nullptr);

    if (NS_SUCCEEDED(mInternalError) &&
        NS_SUCCEEDED(mControlStatus) &&
        mControlConnection->IsAlive() &&
        mCacheConnection) {

        LOG_INFO(("FTP:(%p) caching CC(%p)", this, mControlConnection.get()));

        // Store connection persistent data
        mControlConnection->mServerType = mServerType;           
        mControlConnection->mPassword = mPassword;
        mControlConnection->mPwd = mPwd;
        mControlConnection->mUseUTF8 = mUseUTF8;
        
        nsresult rv = NS_OK;
        // Don't cache controlconnection if anonymous (bug #473371)
        if (!mChannel->HasLoadFlag(nsIRequest::LOAD_ANONYMOUS))
            rv = gFtpHandler->InsertConnection(mChannel->URI(),
                                               mControlConnection);
        // Can't cache it?  Kill it then.  
        mControlConnection->Disconnect(rv);
    } else {
        mControlConnection->Disconnect(NS_BINDING_ABORTED);
    }     

    mControlConnection = nullptr;
}

class nsFtpAsyncAlert : public Runnable
{
public:
  nsFtpAsyncAlert(nsIPrompt* aPrompter, nsString aResponseMsg)
    : mozilla::Runnable("nsFtpAsyncAlert")
    , mPrompter(aPrompter)
    , mResponseMsg(aResponseMsg)
  {
    }
protected:
    virtual ~nsFtpAsyncAlert()
    {
    }
public:
    NS_IMETHOD Run() override
    {
        if (mPrompter) {
            mPrompter->Alert(nullptr, mResponseMsg.get());
        }
        return NS_OK;
    }
private:
    nsCOMPtr<nsIPrompt> mPrompter;
    nsString mResponseMsg;
};


nsresult
nsFtpState::StopProcessing()
{
    // Only do this function once.
    if (!mKeepRunning)
        return NS_OK;
    mKeepRunning = false;

    LOG_INFO(("FTP:(%p) nsFtpState stopping", this));

    if (NS_FAILED(mInternalError) && !mResponseMsg.IsEmpty()) {
        // check to see if the control status is bad.
        // web shell wont throw an alert.  we better:

        // XXX(darin): this code should not be dictating UI like this!
        nsCOMPtr<nsIPrompt> prompter;
        mChannel->GetCallback(prompter);
        if (prompter) {
            nsCOMPtr<nsIRunnable> alertEvent;
            if (mUseUTF8) {
                alertEvent = new nsFtpAsyncAlert(prompter,
                    NS_ConvertUTF8toUTF16(mResponseMsg));
            } else {
                alertEvent = new nsFtpAsyncAlert(prompter,
                    NS_ConvertASCIItoUTF16(mResponseMsg));
            }
            NS_DispatchToMainThread(alertEvent);
        }
        nsCOMPtr<nsIFTPChannelParentInternal> ftpChanP;
        mChannel->GetCallback(ftpChanP);
        if (ftpChanP) {
          ftpChanP->SetErrorMsg(mResponseMsg.get(), mUseUTF8);
        }
    }

    nsresult broadcastErrorCode = mControlStatus;
    if (NS_SUCCEEDED(broadcastErrorCode))
        broadcastErrorCode = mInternalError;

    mInternalError = broadcastErrorCode;

    KillControlConnection();

    // XXX This can fire before we are done loading data.  Is that a problem?
    OnTransportStatus(nullptr, NS_NET_STATUS_END_FTP_TRANSACTION, 0, 0);

    if (NS_FAILED(broadcastErrorCode))
        CloseWithStatus(broadcastErrorCode);

    return NS_OK;
}

nsresult 
nsFtpState::SendFTPCommand(const nsACString& command)
{
    NS_ASSERTION(mControlConnection, "null control connection");        
    
    // we don't want to log the password:
    nsAutoCString logcmd(command);
    if (StringBeginsWith(command, NS_LITERAL_CSTRING("PASS "))) 
        logcmd = "PASS xxxxx";
    
    LOG(("FTP:(%p) writing \"%s\"\n", this, logcmd.get()));

    nsCOMPtr<nsIFTPEventSink> ftpSink;
    mChannel->GetFTPEventSink(ftpSink);
    if (ftpSink)
        ftpSink->OnFTPControlLog(false, logcmd.get());
    
    if (mControlConnection)
        return mControlConnection->Write(command);

    return NS_ERROR_FAILURE;
}

// Convert a unix-style filespec to VMS format
// /foo/fred/barney/file.txt -> foo:[fred.barney]file.txt
// /foo/file.txt -> foo:[000000]file.txt
void
nsFtpState::ConvertFilespecToVMS(nsCString& fileString)
{
    int ntok=1;
    char *t, *nextToken;
    nsAutoCString fileStringCopy;

    // Get a writeable copy we can strtok with.
    fileStringCopy = fileString;
    t = nsCRT::strtok(fileStringCopy.BeginWriting(), "/", &nextToken);
    if (t)
        while (nsCRT::strtok(nextToken, "/", &nextToken))
            ntok++; // count number of terms (tokens)
    LOG(("FTP:(%p) ConvertFilespecToVMS ntok: %d\n", this, ntok));
    LOG(("FTP:(%p) ConvertFilespecToVMS from: \"%s\"\n", this, fileString.get()));

    if (fileString.First() == '/') {
        // absolute filespec
        //   /        -> []
        //   /a       -> a (doesn't really make much sense)
        //   /a/b     -> a:[000000]b
        //   /a/b/c   -> a:[b]c
        //   /a/b/c/d -> a:[b.c]d
        if (ntok == 1) {
            if (fileString.Length() == 1) {
                // Just a slash
                fileString.Truncate();
                fileString.AppendLiteral("[]");
            } else {
                // just copy the name part (drop the leading slash)
                fileStringCopy = fileString;
                fileString = Substring(fileStringCopy, 1,
                                       fileStringCopy.Length()-1);
            }
        } else {
            // Get another copy since the last one was written to.
            fileStringCopy = fileString;
            fileString.Truncate();
            fileString.Append(nsCRT::strtok(fileStringCopy.BeginWriting(), 
                              "/", &nextToken));
            fileString.AppendLiteral(":[");
            if (ntok > 2) {
                for (int i=2; i<ntok; i++) {
                    if (i > 2) fileString.Append('.');
                    fileString.Append(nsCRT::strtok(nextToken,
                                      "/", &nextToken));
                }
            } else {
                fileString.AppendLiteral("000000");
            }
            fileString.Append(']');
            fileString.Append(nsCRT::strtok(nextToken, "/", &nextToken));
        }
    } else {
       // relative filespec
        //   a       -> a
        //   a/b     -> [.a]b
        //   a/b/c   -> [.a.b]c
        if (ntok == 1) {
            // no slashes, just use the name as is
        } else {
            // Get another copy since the last one was written to.
            fileStringCopy = fileString;
            fileString.Truncate();
            fileString.AppendLiteral("[.");
            fileString.Append(nsCRT::strtok(fileStringCopy.BeginWriting(),
                              "/", &nextToken));
            if (ntok > 2) {
                for (int i=2; i<ntok; i++) {
                    fileString.Append('.');
                    fileString.Append(nsCRT::strtok(nextToken,
                                      "/", &nextToken));
                }
            }
            fileString.Append(']');
            fileString.Append(nsCRT::strtok(nextToken, "/", &nextToken));
        }
    }
    LOG(("FTP:(%p) ConvertFilespecToVMS   to: \"%s\"\n", this, fileString.get()));
}

// Convert a unix-style dirspec to VMS format
// /foo/fred/barney/rubble -> foo:[fred.barney.rubble]
// /foo/fred -> foo:[fred]
// /foo -> foo:[000000]
// (null) -> (null)
void
nsFtpState::ConvertDirspecToVMS(nsCString& dirSpec)
{
    LOG(("FTP:(%p) ConvertDirspecToVMS from: \"%s\"\n", this, dirSpec.get()));
    if (!dirSpec.IsEmpty()) {
        if (dirSpec.Last() != '/')
            dirSpec.Append('/');
        // we can use the filespec routine if we make it look like a file name
        dirSpec.Append('x');
        ConvertFilespecToVMS(dirSpec);
        dirSpec.Truncate(dirSpec.Length()-1);
    }
    LOG(("FTP:(%p) ConvertDirspecToVMS   to: \"%s\"\n", this, dirSpec.get()));
}

// Convert an absolute VMS style dirspec to UNIX format
void
nsFtpState::ConvertDirspecFromVMS(nsCString& dirSpec)
{
    LOG(("FTP:(%p) ConvertDirspecFromVMS from: \"%s\"\n", this, dirSpec.get()));
    if (dirSpec.IsEmpty()) {
        dirSpec.Insert('.', 0);
    } else {
        dirSpec.Insert('/', 0);
        dirSpec.ReplaceSubstring(":[", "/");
        dirSpec.ReplaceChar('.', '/');
        dirSpec.ReplaceChar(']', '/');
    }
    LOG(("FTP:(%p) ConvertDirspecFromVMS   to: \"%s\"\n", this, dirSpec.get()));
}

//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsFtpState::OnTransportStatus(nsITransport *transport, nsresult status,
                              int64_t progress, int64_t progressMax)
{
    // Mix signals from both the control and data connections.

    // Ignore data transfer events on the control connection.
    if (mControlConnection && transport == mControlConnection->Transport()) {
        switch (status) {
        case NS_NET_STATUS_RESOLVING_HOST:
        case NS_NET_STATUS_RESOLVED_HOST:
        case NS_NET_STATUS_CONNECTING_TO:
        case NS_NET_STATUS_CONNECTED_TO:
        case NS_NET_STATUS_TLS_HANDSHAKE_STARTING:
        case NS_NET_STATUS_TLS_HANDSHAKE_ENDED:
            break;
        default:
            return NS_OK;
        }
    }

    // Ignore the progressMax value from the socket.  We know the true size of
    // the file based on the response from our SIZE request. Additionally, only
    // report the max progress based on where we started/resumed.
    mChannel->OnTransportStatus(nullptr, status, progress,
                                mFileSize - mChannel->StartPos());
    return NS_OK;
}

//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsFtpState::OnStartRequest(nsIRequest *request, nsISupports *context)
{
    mStorReplyReceived = false;
    return NS_OK;
}

NS_IMETHODIMP
nsFtpState::OnStopRequest(nsIRequest *request, nsISupports *context,
                          nsresult status)
{
    mUploadRequest = nullptr;

    // Close() will be called when reply to STOR command is received
    // see bug #389394
    if (!mStorReplyReceived)
      return NS_OK;

    // We're done uploading.  Let our consumer know that we're done.
    Close();
    return NS_OK;
}

//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsFtpState::Available(uint64_t *result)
{
    if (mDataStream)
        return mDataStream->Available(result);

    return nsBaseContentStream::Available(result);
}

NS_IMETHODIMP
nsFtpState::ReadSegments(nsWriteSegmentFun writer, void *closure,
                         uint32_t count, uint32_t *result)
{
    // Insert a thunk here so that the input stream passed to the writer is this
    // input stream instead of mDataStream.

    if (mDataStream) {
        nsWriteSegmentThunk thunk = { this, writer, closure };
        nsresult rv;
        rv = mDataStream->ReadSegments(NS_WriteSegmentThunk, &thunk, count,
                                       result);
        return rv;
    }

    return nsBaseContentStream::ReadSegments(writer, closure, count, result);
}

NS_IMETHODIMP
nsFtpState::CloseWithStatus(nsresult status)
{
    LOG(("FTP:(%p) close [%" PRIx32 "]\n", this, static_cast<uint32_t>(status)));

    // Shutdown the control connection processing if we are being closed with an
    // error.  Note: This method may be called several times.
    if (!IsClosed() && status != NS_BASE_STREAM_CLOSED && NS_FAILED(status)) {
        if (NS_SUCCEEDED(mInternalError))
            mInternalError = status;
        StopProcessing();
    }

    if (mUploadRequest) {
        mUploadRequest->Cancel(NS_ERROR_ABORT);
        mUploadRequest = nullptr;
    }

    if (mDataTransport) {
        // Shutdown the data transport.
        mDataTransport->Close(NS_ERROR_ABORT);
        mDataTransport = nullptr;
    }

    mDataStream = nullptr;

    return nsBaseContentStream::CloseWithStatus(status);
}

static nsresult
CreateHTTPProxiedChannel(nsIChannel *channel, nsIProxyInfo *pi, nsIChannel **newChannel)
{
    nsresult rv;
    nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIProtocolHandler> handler;
    rv = ioService->GetProtocolHandler("http", getter_AddRefs(handler));
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIProxiedProtocolHandler> pph = do_QueryInterface(handler, &rv);
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));

    nsCOMPtr<nsILoadInfo> loadInfo;
    channel->GetLoadInfo(getter_AddRefs(loadInfo));

    return pph->NewProxiedChannel2(uri, pi, 0, nullptr, loadInfo, newChannel);
}

NS_IMETHODIMP
nsFtpState::OnProxyAvailable(nsICancelable *request, nsIChannel *channel,
                             nsIProxyInfo *pi, nsresult status)
{
  mProxyRequest = nullptr;

  // failed status code just implies DIRECT processing

  if (NS_SUCCEEDED(status)) {
    nsAutoCString type;
    if (pi && NS_SUCCEEDED(pi->GetType(type)) && type.EqualsLiteral("http")) {
        // Proxy the FTP url via HTTP
        // This would have been easier to just return a HTTP channel directly
        // from nsIIOService::NewChannelFromURI(), but the proxy type cannot
        // be reliabliy determined synchronously without jank due to pac, etc..
        LOG(("FTP:(%p) Configured to use a HTTP proxy channel\n", this));

        nsCOMPtr<nsIChannel> newChannel;
        if (NS_SUCCEEDED(CreateHTTPProxiedChannel(channel, pi,
                                                  getter_AddRefs(newChannel))) &&
            NS_SUCCEEDED(mChannel->Redirect(newChannel,
                                            nsIChannelEventSink::REDIRECT_INTERNAL,
                                            true))) {
            LOG(("FTP:(%p) Redirected to use a HTTP proxy channel\n", this));
            return NS_OK;
        }
    }
    else if (pi) {
        // Proxy using the FTP protocol routed through a socks proxy
        LOG(("FTP:(%p) Configured to use a SOCKS proxy channel\n", this));
        mChannel->SetProxyInfo(pi);
    }
  }

  if (mDeferredCallbackPending) {
      mDeferredCallbackPending = false;
      OnCallbackPending();
  }
  return NS_OK;
}

void
nsFtpState::OnCallbackPending()
{
    if (mState == FTP_INIT) {
        if (mProxyRequest) {
            mDeferredCallbackPending = true;
            return;
        }
        Connect();
    } else if (mDataStream) {
        mDataStream->AsyncWait(this, 0, 0, CallbackTarget());
    }
}

