/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set tw=80 ts=4 sts=4 sw=4 et cin: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bradley Baetz <bbaetz@student.usyd.edu.au>
 *   Darin Fisher <darin@meer.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <limits.h>
#include <ctype.h>

#include "prprf.h"
#include "prlog.h"
#include "prtime.h"
#include "prnetdb.h"

#include "nsFTPChannel.h"
#include "nsFtpConnectionThread.h"
#include "nsFtpControlConnection.h"
#include "nsFtpProtocolHandler.h"
#include "ftpCore.h"
#include "netCore.h"
#include "nsCRT.h"
#include "nsEscape.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsStreamUtils.h"
#include "nsICacheService.h"
#include "nsIURL.h"
#include "nsISocketTransport.h"
#include "nsIStreamListenerTee.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsIStringBundle.h"
#include "nsCPasswordManager.h"
#include "nsAuthInformationHolder.h"

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gFTPLog;
#endif
#define LOG(args)         PR_LOG(gFTPLog, PR_LOG_DEBUG, args)
#define LOG_ALWAYS(args)  PR_LOG(gFTPLog, PR_LOG_ALWAYS, args)

NS_IMPL_ISUPPORTS_INHERITED4(nsFtpState,
                             nsBaseContentStream,
                             nsIInputStreamCallback, 
                             nsITransportEventSink,
                             nsICacheListener,
                             nsIRequestObserver)

nsFtpState::nsFtpState()
    : nsBaseContentStream(PR_TRUE)
    , mState(FTP_INIT)
    , mNextState(FTP_S_USER)
    , mKeepRunning(PR_TRUE)
    , mReceivedControlData(PR_FALSE)
    , mTryingCachedControl(PR_FALSE)
    , mRETRFailed(PR_FALSE)
    , mFileSize(LL_MAXUINT)
    , mServerType(FTP_GENERIC_TYPE)
    , mAction(GET)
    , mAnonymous(PR_TRUE)
    , mRetryPass(PR_FALSE)
    , mInternalError(NS_OK)
    , mPort(21)
    , mIPv6Checked(PR_FALSE)
    , mControlStatus(NS_OK)
{
    LOG_ALWAYS(("FTP:(%x) nsFtpState created", this));

    // make sure handler stays around
    NS_ADDREF(gFtpHandler);
}

nsFtpState::~nsFtpState() 
{
    LOG_ALWAYS(("FTP:(%x) nsFtpState destroyed", this));

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
nsFtpState::OnControlDataAvailable(const char *aData, PRUint32 aDataLen)
{
    LOG(("FTP:(%p) control data available [%u]\n", this, aDataLen));
    mControlConnection->WaitData(this);  // queue up another call

    if (!mReceivedControlData) {
        // parameter can be null cause the channel fills them in.
        OnTransportStatus(nsnull, NS_NET_STATUS_BEGIN_FTP_TRANSACTION, 0, 0);
        mReceivedControlData = PR_TRUE;
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
        PRInt32 eolLength = strcspn(currLine, CRLF);
        PRInt32 currLineLength = strlen(currLine);

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
        nsCAutoString line;
        PRInt32 crlfLength = 0;

        if ((currLineLength > eolLength) &&
            (currLine[eolLength] == nsCRT::CR) &&
            (currLine[eolLength+1] == nsCRT::LF)) {
            crlfLength = 2; // CR +LF 
        } else {
            crlfLength = 1; // + LF or CR
        }

        line.Assign(currLine, eolLength + crlfLength);
        
        // Does this start with a response code?
        PRBool startNum = (line.Length() >= 3 &&
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
                ftpSink->OnFTPControlLog(PR_TRUE, mResponseMsg.get());
            
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

    LOG(("FTP:(%p) CC(%p) error [%x was-cached=%u]\n",
         this, mControlConnection.get(), status, mTryingCachedControl));

    mControlStatus = status;
    if (mTryingCachedControl && NS_SUCCEEDED(mInternalError)) {
        mTryingCachedControl = PR_FALSE;
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

    LOG(("FTP:(%x) trying cached control\n", this));
        
    // Look to see if we can use a cached control connection:
    nsFtpControlConnection *connection = nsnull;
    gFtpHandler->RemoveConnection(mChannel->URI(), &connection);

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
            mTryingCachedControl = PR_TRUE;
            
            // we're already connected to this server, skip login.
            mState = FTP_S_PASV;
            mResponseCode = 530;  // assume the control connection was dropped.
            mControlStatus = NS_OK;
            mReceivedControlData = PR_FALSE;  // For this request, we have not.

            // if we succeed, return.  Otherwise, we need to create a transport
            rv = mControlConnection->Connect(mChannel->ProxyInfo(), this);
            if (NS_SUCCEEDED(rv))
                return rv;
        }
        LOG(("FTP:(%p) cached CC(%p) is unusable\n", this,
            mControlConnection.get()));

        mControlConnection->WaitData(nsnull);
        mControlConnection = nsnull;
    }

    LOG(("FTP:(%p) creating CC\n", this));
        
    mState = FTP_READ_BUF;
    mNextState = FTP_S_USER;
    
    nsCAutoString host;
    rv = mChannel->URI()->GetAsciiHost(host);
    if (NS_FAILED(rv))
        return rv;

    mControlConnection = new nsFtpControlConnection(host, mPort);
    if (!mControlConnection)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = mControlConnection->Connect(mChannel->ProxyInfo(), this);
    if (NS_FAILED(rv)) {
        LOG(("FTP:(%p) CC(%p) failed to connect [rv=%x]\n", this,
            mControlConnection.get(), rv));
        mControlConnection = nsnull;
        return rv;
    }

    return mControlConnection->WaitData(this);
}

void 
nsFtpState::MoveToNextState(FTP_STATE nextState)
{
    if (NS_FAILED(mInternalError)) {
        mState = FTP_ERROR;
        LOG(("FTP:(%x) FAILED (%x)\n", this, mInternalError));
    } else {
        mState = FTP_READ_BUF;
        mNextState = nextState;
    }  
}

nsresult
nsFtpState::Process() 
{
    nsresult    rv = NS_OK;
    PRBool      processingRead = PR_TRUE;
    
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
            processingRead = PR_FALSE;
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
            } else {
                LOG(("FTP:(%x) FTP_ERROR - calling StopProcessing\n", this));
                rv = StopProcessing();
                NS_ASSERTION(NS_SUCCEEDED(rv), "StopProcessing failed.");
                processingRead = PR_FALSE;
            }
            break;
          
          case FTP_COMPLETE:
            LOG(("FTP:(%x) COMPLETE\n", this));
            rv = StopProcessing();
            NS_ASSERTION(NS_SUCCEEDED(rv), "StopProcessing failed.");
            processingRead = PR_FALSE;
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
    nsCAutoString usernameStr("USER ");

    if (mAnonymous) {
        usernameStr.AppendLiteral("anonymous");
    } else {
        if (mUsername.IsEmpty()) {
            nsCOMPtr<nsIAuthPrompt2> prompter;
            NS_QueryAuthPrompt2(NS_STATIC_CAST(nsIChannel*, mChannel),
                                getter_AddRefs(prompter));
            if (!prompter)
                return NS_ERROR_NOT_INITIALIZED;

            nsRefPtr<nsAuthInformationHolder> info =
                new nsAuthInformationHolder(nsIAuthInformation::AUTH_HOST,
                                            EmptyString(),
                                            EmptyCString());

            PRBool retval;
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
    if (mAnonymous) {
        // we just tried to login anonymously and failed.
        // kick back out to S_user() and try again after
        // gathering uname/pwd info from the user.
        mAnonymous = PR_FALSE;
        return FTP_S_USER;
    }
    return FTP_ERROR;
}


nsresult
nsFtpState::S_pass() {
    nsresult rv;
    nsCAutoString passwordStr("PASS ");

    mResponseMsg = "";

    if (mAnonymous) {
        if (!mPassword.IsEmpty()) {
            // XXX Is UTF-8 the best choice?
            AppendUTF16toUTF8(mPassword, passwordStr);
        } else {
            nsXPIDLCString anonPassword;
            PRBool useRealEmail = PR_FALSE;
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
            nsCOMPtr<nsIAuthPrompt2> prompter;
            NS_QueryAuthPrompt2(NS_STATIC_CAST(nsIChannel*, mChannel),
                                getter_AddRefs(prompter));
            if (!prompter)
                return NS_ERROR_NOT_INITIALIZED;

            nsRefPtr<nsAuthInformationHolder> info =
                new nsAuthInformationHolder(nsIAuthInformation::AUTH_HOST |
                                            nsIAuthInformation::ONLY_PASSWORD,
                                            EmptyString(),
                                            EmptyCString());

            info->SetUserInternal(mUsername);

            PRBool retval;
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
        mRetryPass = PR_FALSE;
        return FTP_S_USER;
    }
    if (mResponseCode/100 == 5 || mResponseCode==421) {
        // There is no difference between a too-many-users error,
        // a wrong-password error, or any other sort of error
        // So we need to tell wallet to forget the password if we had one,
        // and error out. That will then show the error message, and the
        // user can retry if they want to

        if (!mPassword.IsEmpty()) {
            nsCOMPtr<nsIPasswordManager> pm =
                    do_GetService(NS_PASSWORDMANAGER_CONTRACTID);
            if (pm) {
                nsCAutoString prePath;
                nsresult rv = mChannel->URI()->GetPrePath(prePath);
                NS_ASSERTION(NS_SUCCEEDED(rv), "Failed to get prepath");
                if (NS_SUCCEEDED(rv))
                    pm->RemoveUser(prePath, EmptyString());
            }
        }

        // If the login was anonymous, and it failed, try again with a username
        if (mAnonymous) {
            mAnonymous = PR_FALSE;
            return FTP_S_USER;
        }

        mRetryPass = PR_TRUE;
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
    if (mResponseCode/100 != 2) 
        return FTP_ERROR;
    nsCAutoString respStr(mResponseMsg);
    PRInt32 pos = respStr.FindChar('"');
    if (pos > -1) {
        respStr.Cut(0, pos+1);
        pos = respStr.FindChar('"');
        if (pos > -1) {
            respStr.Truncate(pos);
            if (mServerType == FTP_VMS_TYPE)
                ConvertDirspecFromVMS(respStr);
            if (respStr.Last() != '/')
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
            ( mResponseMsg.Find("OS/390") > -1)) {
            mServerType = FTP_UNIX_TYPE;
        } else if (( mResponseMsg.Find("WIN32", PR_TRUE) > -1) ||
                   ( mResponseMsg.Find("windows", PR_TRUE) > -1)) {
            mServerType = FTP_NT_TYPE;
        } else if (mResponseMsg.Find("OS/2", PR_TRUE) > -1) {
            mServerType = FTP_OS2_TYPE;
        } else if (mResponseMsg.Find("VMS", PR_TRUE) > -1) {
            mServerType = FTP_VMS_TYPE;
        } else {
            NS_ERROR("Server type list format unrecognized.");
            // Guessing causes crashes.
            // (Of course, the parsing code should be more robust...)
            nsresult rv;
            nsCOMPtr<nsIStringBundleService> bundleService =
                    do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
            if (NS_FAILED(rv))
                return FTP_ERROR;

            nsCOMPtr<nsIStringBundle> bundle;
            rv = bundleService->CreateBundle(NECKO_MSGS_URL,
                                             getter_AddRefs(bundle));
            if (NS_FAILED(rv))
                return FTP_ERROR;
            
            PRUnichar* ucs2Response = ToNewUnicode(mResponseMsg);
            const PRUnichar *formatStrings[1] = { ucs2Response };
            NS_NAMED_LITERAL_STRING(name, "UnsupportedFTPServer");

            nsXPIDLString formattedString;
            rv = bundle->FormatStringFromName(name.get(), formatStrings, 1,
                                              getter_Copies(formattedString));
            nsMemory::Free(ucs2Response);
            if (NS_FAILED(rv))
                return FTP_ERROR;

            // TODO(darin): this code should not be dictating UI like this!
            nsCOMPtr<nsIPrompt> prompter;
            mChannel->GetCallback(prompter);
            if (prompter)
                prompter->Alert(nsnull, formattedString.get());
            
            // since we just alerted the user, clear mResponseMsg,
            // which is displayed to the user.
            mResponseMsg = "";
            return FTP_ERROR;
        }
        
        return FTP_S_PWD;
    }

    if (mResponseCode/100 == 5) {   
        // server didn't like the SYST command.  Probably (500, 501, 502)
        // No clue.  We will just hope it is UNIX type server.
        mServerType = FTP_UNIX_TYPE;

        return FTP_S_PWD;
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
    nsCAutoString cwdStr;
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
    nsCAutoString sizeBuf(mPath);
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
        mChannel->SetContentLength64(mFileSize);
    }

    // We may want to be able to resume this
    return FTP_S_MDTM;
}

nsresult
nsFtpState::S_mdtm() {
    nsCAutoString mdtmBuf(mPath);
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
        }
    }

    nsCString entityID;
    entityID.Truncate();
    entityID.AppendInt(PRInt64(mFileSize));
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
    return mChannel->SetContentType(
        NS_LITERAL_CSTRING(APPLICATION_HTTP_INDEX_FORMAT));
}

nsresult
nsFtpState::S_list() {
    nsresult rv = SetContentType();
    if (NS_FAILED(rv)) 
        return FTP_ERROR;

    rv = mChannel->PushStreamConverter("text/ftp-dir",
                                       APPLICATION_HTTP_INDEX_FORMAT);
    if (NS_FAILED(rv)) {
        // clear mResponseMsg which is displayed to the user.
        // TODO: we should probably set this to something meaningful.
        mResponseMsg = "";
        return rv;
    }
    
    if (mCacheEntry) {
        // save off the server type if we are caching.
        nsCAutoString serverType;
        serverType.AppendInt(mServerType);
        mCacheEntry->SetMetaDataElement("servertype", serverType.get());

        // open cache entry for writing, and configure it to receive data.
        if (NS_FAILED(InstallCacheListener())) {
            mCacheEntry->Doom();
            mCacheEntry = nsnull;
        }
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
        if (HasPendingCallback())
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
    nsCAutoString retrStr(mPath);
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
        // We're going to grab a file, not a directory. So we need to clear
        // any cache entry, otherwise we'll have problems reading it later.
        // See bug 122548
        if (mCacheEntry) {
            (void)mCacheEntry->Doom();
            mCacheEntry = nsnull;
        }
        if (HasPendingCallback())
            mDataStream->AsyncWait(this, 0, 0, CallbackTarget());
        return FTP_READ_BUF;
    }
    
    // These error codes are related to problems with the connection.  
    // If we encounter any at this point, do not try CWD and abort.
    if (mResponseCode == 421 || mResponseCode == 425 || mResponseCode == 426)
        return FTP_ERROR;

    if (mResponseCode/100 == 5) {
        mRETRFailed = PR_TRUE;
        return FTP_S_PASV;
    }

    return FTP_S_CWD;
}


nsresult
nsFtpState::S_rest() {
    
    nsCAutoString restString("REST ");
    // The PRInt64 cast is needed to avoid ambiguity
    restString.AppendInt(PRInt64(mChannel->StartPos()), 10);
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

    nsCAutoString storStr;
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
        return FTP_COMPLETE;
    }

    if (mResponseCode/100 == 1) {
        LOG(("FTP:(%x) writing on DT\n", this));
        return FTP_READ_BUF;
    }

   return FTP_ERROR;
}


nsresult
nsFtpState::S_pasv() {
    nsresult rv;

    if (mIPv6Checked == PR_FALSE) {
        NS_ASSERTION(mIPv6ServerAddress.get() == nsnull, "already initialized");

        // Find IPv6 socket address, if server is IPv6
        mIPv6Checked = PR_TRUE;
        nsITransport *controlSocket = mControlConnection->Transport();
        if (!controlSocket)
            return FTP_ERROR;

        nsCOMPtr<nsISocketTransport> sTrans = do_QueryInterface(controlSocket);
        if (sTrans) {
            PRNetAddr addr;
            rv = sTrans->GetPeerAddr(&addr);
            if (NS_SUCCEEDED(rv) && addr.raw.family == PR_AF_INET6 &&
                        !PR_IsNetAddrType(&addr, PR_IpAddrV4Mapped)) {
                const size_t size = 100;
                mIPv6ServerAddress = new char[size];
                if (mIPv6ServerAddress &&
                            PR_NetAddrToString(&addr, mIPv6ServerAddress,
                                               size) != PR_SUCCESS)
                    mIPv6ServerAddress = nsnull;
            }
        }
    }

    const char *string;
    if (mIPv6ServerAddress) {
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
    PRInt32 port;

    nsCAutoString responseCopy(mResponseMsg);
    char *response = responseCopy.BeginWriting();

    char *ptr = response;

    // Make sure to ignore the address in the PASV response (bug 370559)
    nsCAutoString host;
    rv = mChannel->URI()->GetAsciiHost(host);
    if (NS_FAILED(rv))
        return FTP_ERROR;

    if (mIPv6ServerAddress) {
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
        PRInt32 h0, h1, h2, h3, p0, p1;

        PRUint32 fields = 0;
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

        port = ((PRInt32) (p0<<8)) + p1;
    }

    const char* hostStr =
            mIPv6ServerAddress ? mIPv6ServerAddress : host.get();

    PRBool newDataConn = PR_TRUE;
    if (mDataTransport) {
        // Reuse this connection only if its still alive, and the port
        // is the same
        nsCOMPtr<nsISocketTransport> strans = do_QueryInterface(mDataTransport);
        if (strans) {
            PRInt32 oldPort;
            nsresult rv = strans->GetPort(&oldPort);
            if (NS_SUCCEEDED(rv)) {
                if (oldPort == port) {
                    PRBool isAlive;
                    if (NS_SUCCEEDED(strans->IsAlive(&isAlive)) && isAlive)
                        newDataConn = PR_FALSE;
                }
            }
        }

        if (newDataConn) {
            mDataTransport->Close(NS_ERROR_ABORT);
            mDataTransport = nsnull;
            mDataStream = nsnull;
        }
    }

    if (newDataConn) {
        // now we know where to connect our data channel
        nsCOMPtr<nsISocketTransportService> sts =
            do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
        
        nsCOMPtr<nsISocketTransport> strans;
        rv =  sts->CreateTransport(nsnull, 0, nsDependentCString(hostStr),
                                   port, mChannel->ProxyInfo(),
                                   getter_AddRefs(strans)); // the data socket
        if (NS_FAILED(rv))
            return FTP_ERROR;
        mDataTransport = strans;
        
        LOG(("FTP:(%x) created DT (%s:%x)\n", this, hostStr, port));
        
        // hook ourself up as a proxy for status notifications
        rv = mDataTransport->SetEventSink(this, NS_GetCurrentThread());
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
                do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
            if (NS_FAILED(rv))
                return FTP_ERROR;
            
            nsCOMPtr<nsIAsyncStreamCopier> copier;
            rv = NS_NewAsyncStreamCopier(getter_AddRefs(copier),
                                         mChannel->UploadStream(),
                                         output,
                                         stEventTarget,
                                         PR_TRUE,   // upload stream is buffered
                                         PR_FALSE); // output is NOT buffered
            if (NS_FAILED(rv))
                return FTP_ERROR;
        
            rv = copier->AsyncCopy(this, nsnull);
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
                                             FTP_DATA_CHANNEL_SEG_SIZE,
                                             FTP_DATA_CHANNEL_SEG_COUNT,
                                             getter_AddRefs(input));
        NS_ENSURE_SUCCESS(rv, FTP_ERROR);
        mDataStream = do_QueryInterface(input);
    }

    if (mRETRFailed)
        return FTP_S_CWD;
    return FTP_S_SIZE;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

static inline
PRUint32 NowInSeconds()
{
    return PRUint32(PR_Now() / PR_USEC_PER_SEC);
}

PRUint32 nsFtpState::mSessionStartTime = NowInSeconds();

/* Is this cache entry valid to use for reading?
 * Since we make up an expiration time for ftp, use the following rules:
 * (see bug 103726)
 *
 * LOAD_FROM_CACHE                    : always use cache entry, even if expired
 * LOAD_BYPASS_CACHE                  : overwrite cache entry
 * LOAD_NORMAL|VALIDATE_ALWAYS        : overwrite cache entry
 * LOAD_NORMAL                        : honor expiration time
 * LOAD_NORMAL|VALIDATE_ONCE_PER_SESSION : overwrite cache entry if first access 
 *                                         this session, otherwise use cache entry 
 *                                         even if expired.
 * LOAD_NORMAL|VALIDATE_NEVER         : always use cache entry, even if expired
 *
 * Note that in theory we could use the mdtm time on the directory
 * In practice, the lack of a timezone plus the general lack of support for that
 * on directories means that its not worth it, I suspect. Revisit if we start
 * caching files - bbaetz
 */
PRBool
nsFtpState::CanReadCacheEntry()
{
    NS_ASSERTION(mCacheEntry, "must have a cache entry");

    nsCacheAccessMode access;
    nsresult rv = mCacheEntry->GetAccessGranted(&access);
    if (NS_FAILED(rv))
        return PR_FALSE;
    
    // If I'm not granted read access, then I can't reuse it...
    if (!(access & nsICache::ACCESS_READ))
        return PR_FALSE;

    if (mChannel->HasLoadFlag(nsIRequest::LOAD_FROM_CACHE))
        return PR_TRUE;

    if (mChannel->HasLoadFlag(nsIRequest::LOAD_BYPASS_CACHE))
        return PR_FALSE;
    
    if (mChannel->HasLoadFlag(nsIRequest::VALIDATE_ALWAYS))
        return PR_FALSE;
    
    PRUint32 time;
    if (mChannel->HasLoadFlag(nsIRequest::VALIDATE_ONCE_PER_SESSION)) {
        rv = mCacheEntry->GetLastModified(&time);
        if (NS_FAILED(rv))
            return PR_FALSE;
        return (mSessionStartTime > time);
    }

    if (mChannel->HasLoadFlag(nsIRequest::VALIDATE_NEVER))
        return PR_TRUE;

    // OK, now we just check the expiration time as usual
    rv = mCacheEntry->GetExpirationTime(&time);
    if (NS_FAILED(rv))
        return PR_FALSE;

    return (NowInSeconds() <= time);
}

nsresult
nsFtpState::InstallCacheListener()
{
    NS_ASSERTION(mCacheEntry, "must have a cache entry");

    nsCOMPtr<nsIOutputStream> out;
    mCacheEntry->OpenOutputStream(0, getter_AddRefs(out));
    NS_ENSURE_STATE(out);

    nsCOMPtr<nsIStreamListenerTee> tee =
            do_CreateInstance(NS_STREAMLISTENERTEE_CONTRACTID);
    NS_ENSURE_STATE(tee);

    nsresult rv = tee->Init(mChannel->StreamListener(), out);
    NS_ENSURE_SUCCESS(rv, rv);

    mChannel->SetStreamListener(tee);
    return NS_OK;
}

nsresult
nsFtpState::OpenCacheDataStream()
{
    NS_ASSERTION(mCacheEntry, "must have a cache entry");

    // Get a transport to the cached data...
    nsCOMPtr<nsIInputStream> input;
    mCacheEntry->OpenInputStream(0, getter_AddRefs(input));
    NS_ENSURE_STATE(input);

    nsCOMPtr<nsIStreamTransportService> sts =
            do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    NS_ENSURE_STATE(sts);

    nsCOMPtr<nsITransport> transport;
    sts->CreateInputTransport(input, -1, -1, PR_TRUE,
                              getter_AddRefs(transport));
    NS_ENSURE_STATE(transport);

    nsresult rv = transport->SetEventSink(this, NS_GetCurrentThread());
    NS_ENSURE_SUCCESS(rv, rv);

    // Open a non-blocking, buffered input stream...
    nsCOMPtr<nsIInputStream> transportInput;
    transport->OpenInputStream(0, FTP_DATA_CHANNEL_SEG_SIZE,
                                  FTP_DATA_CHANNEL_SEG_COUNT,
                                  getter_AddRefs(transportInput));
    NS_ENSURE_STATE(transportInput);

    mDataStream = do_QueryInterface(transportInput);
    NS_ENSURE_STATE(mDataStream);

    mDataTransport = transport;
    return NS_OK;
}

nsresult
nsFtpState::Init(nsFtpChannel *channel)
{
    // parameter validation
    NS_ASSERTION(channel, "FTP: needs a channel");

    mChannel = channel; // a straight ref ptr to the channel

    mKeepRunning = PR_TRUE;
    mSuppliedEntityID = channel->EntityID();
  
    if (channel->UploadStream())
        mAction = PUT;

    nsresult rv;
    nsCAutoString path;
    nsCOMPtr<nsIURL> url = do_QueryInterface(mChannel->URI());
    if (url) {
        rv = url->GetFilePath(path);
    } else {
        rv = mChannel->URI()->GetPath(path);
    }
    if (NS_FAILED(rv))
        return rv;

    // Skip leading slash
    char *fwdPtr = path.BeginWriting();
    if (fwdPtr && (*fwdPtr == '/'))
        fwdPtr++;
    if (*fwdPtr != '\0') {
        // now unescape it... %xx reduced inline to resulting character
        PRInt32 len = NS_UnescapeURL(fwdPtr);
        mPath.Assign(fwdPtr, len);

#ifdef DEBUG
        if (mPath.FindCharInSet(CRLF) >= 0)
            NS_ERROR("NewURI() should've prevented this!!!");
#endif
    }

    // pull any username and/or password out of the uri
    nsCAutoString uname;
    rv = mChannel->URI()->GetUsername(uname);
    if (NS_FAILED(rv))
        return rv;

    if (!uname.IsEmpty() && !uname.EqualsLiteral("anonymous")) {
        mAnonymous = PR_FALSE;
        CopyUTF8toUTF16(NS_UnescapeURL(uname), mUsername);
        
        // return an error if we find a CR or LF in the username
        if (uname.FindCharInSet(CRLF) >= 0)
            return NS_ERROR_MALFORMED_URI;
    }

    nsCAutoString password;
    rv = mChannel->URI()->GetPassword(password);
    if (NS_FAILED(rv))
        return rv;

    CopyUTF8toUTF16(NS_UnescapeURL(password), mPassword);

    // return an error if we find a CR or LF in the password
    if (mPassword.FindCharInSet(CRLF) >= 0)
        return NS_ERROR_MALFORMED_URI;

    // setup the connection cache key

    PRInt32 port;
    rv = mChannel->URI()->GetPort(&port);
    if (NS_FAILED(rv))
        return rv;

    if (port > 0)
        mPort = port;

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
        LOG(("FTP:Process() failed: %x\n", rv));
        mInternalError = NS_ERROR_FAILURE;
        mState = FTP_ERROR;
        CloseWithStatus(mInternalError);
    }
}

void
nsFtpState::KillControlConnection()
{
    mControlReadCarryOverBuf.Truncate(0);

    mIPv6Checked = PR_FALSE;
    mIPv6ServerAddress = nsnull;

    // if everything went okay, save the connection. 
    // FIX: need a better way to determine if we can cache the connections.
    //      there are some errors which do not mean that we need to kill the connection
    //      e.g. fnf.

    if (!mControlConnection)
        return;

    // kill the reference to ourselves in the control connection.
    mControlConnection->WaitData(nsnull);

    if (NS_SUCCEEDED(mInternalError) &&
        NS_SUCCEEDED(mControlStatus) &&
        mControlConnection->IsAlive()) {

        LOG_ALWAYS(("FTP:(%p) caching CC(%p)", this, mControlConnection.get()));

        // Store connection persistent data
        mControlConnection->mServerType = mServerType;           
        mControlConnection->mPassword = mPassword;
        mControlConnection->mPwd = mPwd;
        nsresult rv = gFtpHandler->InsertConnection(mChannel->URI(),
                                                    mControlConnection);
        // Can't cache it?  Kill it then.  
        mControlConnection->Disconnect(rv);
    } else {
        mControlConnection->Disconnect(NS_BINDING_ABORTED);
    }     

    mControlConnection = nsnull;
}

nsresult
nsFtpState::StopProcessing()
{
    // Only do this function once.
    if (!mKeepRunning)
        return NS_OK;
    mKeepRunning = PR_FALSE;

    LOG_ALWAYS(("FTP:(%x) nsFtpState stopping", this));

#ifdef DEBUG_dougt
    printf("FTP Stopped: [response code %d] [response msg follows:]\n%s\n", mResponseCode, mResponseMsg.get());
#endif

    if (NS_FAILED(mInternalError) && !mResponseMsg.IsEmpty()) {
        // check to see if the control status is bad.
        // web shell wont throw an alert.  we better:

        // XXX(darin): this code should not be dictating UI like this!
        nsCOMPtr<nsIPrompt> prompter;
        mChannel->GetCallback(prompter);
        if (prompter)
            prompter->Alert(nsnull, NS_ConvertASCIItoUTF16(mResponseMsg).get());
    }
    
    nsresult broadcastErrorCode = mControlStatus;
    if (NS_SUCCEEDED(broadcastErrorCode))
        broadcastErrorCode = mInternalError;

    mInternalError = broadcastErrorCode;

    KillControlConnection();

    // XXX This can fire before we are done loading data.  Is that a problem?
    OnTransportStatus(nsnull, NS_NET_STATUS_END_FTP_TRANSACTION, 0, 0);

    if (NS_FAILED(broadcastErrorCode))
        CloseWithStatus(broadcastErrorCode);

    return NS_OK;
}

nsresult 
nsFtpState::SendFTPCommand(const nsCSubstring& command)
{
    NS_ASSERTION(mControlConnection, "null control connection");        
    
    // we don't want to log the password:
    nsCAutoString logcmd(command);
    if (StringBeginsWith(command, NS_LITERAL_CSTRING("PASS "))) 
        logcmd = "PASS xxxxx";
    
    LOG(("FTP:(%x) writing \"%s\"\n", this, logcmd.get()));

    nsCOMPtr<nsIFTPEventSink> ftpSink;
    mChannel->GetFTPEventSink(ftpSink);
    if (ftpSink)
        ftpSink->OnFTPControlLog(PR_FALSE, logcmd.get());
    
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
    nsCAutoString fileStringCopy;

    // Get a writeable copy we can strtok with.
    fileStringCopy = fileString;
    t = nsCRT::strtok(fileStringCopy.BeginWriting(), "/", &nextToken);
    if (t)
        while (nsCRT::strtok(nextToken, "/", &nextToken))
            ntok++; // count number of terms (tokens)
    LOG(("FTP:(%x) ConvertFilespecToVMS ntok: %d\n", this, ntok));
    LOG(("FTP:(%x) ConvertFilespecToVMS from: \"%s\"\n", this, fileString.get()));

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
    LOG(("FTP:(%x) ConvertFilespecToVMS   to: \"%s\"\n", this, fileString.get()));
}

// Convert a unix-style dirspec to VMS format
// /foo/fred/barney/rubble -> foo:[fred.barney.rubble]
// /foo/fred -> foo:[fred]
// /foo -> foo:[000000]
// (null) -> (null)
void
nsFtpState::ConvertDirspecToVMS(nsCString& dirSpec)
{
    LOG(("FTP:(%x) ConvertDirspecToVMS from: \"%s\"\n", this, dirSpec.get()));
    if (!dirSpec.IsEmpty()) {
        if (dirSpec.Last() != '/')
            dirSpec.Append('/');
        // we can use the filespec routine if we make it look like a file name
        dirSpec.Append('x');
        ConvertFilespecToVMS(dirSpec);
        dirSpec.Truncate(dirSpec.Length()-1);
    }
    LOG(("FTP:(%x) ConvertDirspecToVMS   to: \"%s\"\n", this, dirSpec.get()));
}

// Convert an absolute VMS style dirspec to UNIX format
void
nsFtpState::ConvertDirspecFromVMS(nsCString& dirSpec)
{
    LOG(("FTP:(%x) ConvertDirspecFromVMS from: \"%s\"\n", this, dirSpec.get()));
    if (dirSpec.IsEmpty()) {
        dirSpec.Insert('.', 0);
    } else {
        dirSpec.Insert('/', 0);
        dirSpec.ReplaceSubstring(":[", "/");
        dirSpec.ReplaceChar('.', '/');
        dirSpec.ReplaceChar(']', '/');
    }
    LOG(("FTP:(%x) ConvertDirspecFromVMS   to: \"%s\"\n", this, dirSpec.get()));
}

//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsFtpState::OnTransportStatus(nsITransport *transport, nsresult status,
                              PRUint64 progress, PRUint64 progressMax)
{
    // Mix signals from both the control and data connections.

    // Ignore data transfer events on the control connection.
    if (mControlConnection && transport == mControlConnection->Transport()) {
        switch (status) {
        case NS_NET_STATUS_RESOLVING_HOST:
        case NS_NET_STATUS_CONNECTING_TO:
        case NS_NET_STATUS_CONNECTED_TO:
            break;
        default:
            return NS_OK;
        }
    }

    // Ignore the progressMax value from the socket.  We know the true size of
    // the file based on the response from our SIZE request.
    mChannel->OnTransportStatus(nsnull, status, progress, mFileSize);
    return NS_OK;
}

//-----------------------------------------------------------------------------


NS_IMETHODIMP
nsFtpState::OnCacheEntryAvailable(nsICacheEntryDescriptor *entry,
                                  nsCacheAccessMode access,
                                  nsresult status)
{
    // We may have been closed while we were waiting for this cache entry.
    if (IsClosed())
        return NS_OK;

    mCacheEntry = entry;
    if (CanReadCacheEntry() && ReadCacheEntry()) {
        mState = FTP_READ_CACHE;
    } else {
        Connect();
    }
    return NS_OK;
}

//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsFtpState::OnStartRequest(nsIRequest *request, nsISupports *context)
{
    return NS_OK;
}

NS_IMETHODIMP
nsFtpState::OnStopRequest(nsIRequest *request, nsISupports *context,
                          nsresult status)
{
    mUploadRequest = nsnull;

    // We're done uploading.  Let our consumer know that we're done.
    Close();
    return NS_OK;
}

//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsFtpState::Available(PRUint32 *result)
{
    if (mDataStream)
        return mDataStream->Available(result);

    return nsBaseContentStream::Available(result);
}

NS_IMETHODIMP
nsFtpState::ReadSegments(nsWriteSegmentFun writer, void *closure,
                         PRUint32 count, PRUint32 *result)
{
    // Insert a thunk here so that the input stream passed to the writer is this
    // input stream instead of mDataStream.

    if (mDataStream) {
        nsWriteSegmentThunk thunk = { this, writer, closure };
        return mDataStream->ReadSegments(NS_WriteSegmentThunk, &thunk, count,
                                         result);
    }

    return nsBaseContentStream::ReadSegments(writer, closure, count, result);
}

NS_IMETHODIMP
nsFtpState::CloseWithStatus(nsresult status)
{
    LOG(("FTP:(%p) close [%x]\n", this, status));

    // Shutdown the control connection processing if we are being closed with an
    // error.  Note: This method may be called several times.
    if (!IsClosed() && status != NS_BASE_STREAM_CLOSED && NS_FAILED(status)) {
        if (NS_SUCCEEDED(mInternalError))
            mInternalError = status;
        StopProcessing();
    }

    if (mUploadRequest) {
        mUploadRequest->Cancel(NS_ERROR_ABORT);
        mUploadRequest = nsnull;
    }

    if (mDataTransport) {
        // Shutdown the data transport.
        mDataTransport->Close(NS_ERROR_ABORT);
        mDataTransport = nsnull;
    }

    mDataStream = nsnull;
    mCacheEntry = nsnull;

    return nsBaseContentStream::CloseWithStatus(status);
}

void
nsFtpState::OnCallbackPending()
{
    // If this is the first call, then see if we could use the cache.  If we
    // aren't going to read from (or write to) the cache, then just proceed to
    // connect to the server.

    if (mState == FTP_INIT) {
        if (CheckCache()) {
            mState = FTP_WAIT_CACHE;
            return;
        } 
        if (mCacheEntry && CanReadCacheEntry() && ReadCacheEntry()) {
            mState = FTP_READ_CACHE;
            return;
        }
        Connect();
    } else if (mDataStream) {
        mDataStream->AsyncWait(this, 0, 0, CallbackTarget());
    }
}

PRBool
nsFtpState::ReadCacheEntry()
{
    NS_ASSERTION(mCacheEntry, "should have a cache entry");

    // make sure the channel knows wassup
    SetContentType();

    nsXPIDLCString serverType;
    mCacheEntry->GetMetaDataElement("servertype", getter_Copies(serverType));
    nsCAutoString serverNum(serverType.get());
    PRInt32 err;
    mServerType = serverNum.ToInteger(&err);
    
    mChannel->PushStreamConverter("text/ftp-dir",
                                  APPLICATION_HTTP_INDEX_FORMAT);
    
    mChannel->SetEntityID(EmptyCString());

    if (NS_FAILED(OpenCacheDataStream()))
        return PR_FALSE;

    if (HasPendingCallback())
        mDataStream->AsyncWait(this, 0, 0, CallbackTarget());

    return PR_TRUE;
}

PRBool
nsFtpState::CheckCache()
{
    // This function is responsible for setting mCacheEntry if there is a cache
    // entry that we can use.  It returns true if we end up waiting for access
    // to the cache.

    // In some cases, we don't want to use the cache:
    if (mChannel->UploadStream() || mChannel->ResumeRequested())
        return PR_FALSE;

    nsCOMPtr<nsICacheService> cache = do_GetService(NS_CACHESERVICE_CONTRACTID);
    if (!cache)
        return PR_FALSE;

    nsCOMPtr<nsICacheSession> session;
    cache->CreateSession("FTP",
                         nsICache::STORE_ANYWHERE,
                         nsICache::STREAM_BASED,
                         getter_AddRefs(session));
    if (!session)
        return PR_FALSE;
    session->SetDoomEntriesIfExpired(PR_FALSE);

    // Set cache access requested:
    nsCacheAccessMode accessReq;
    if (NS_IsOffline()) {
        accessReq = nsICache::ACCESS_READ; // can only read
    } else if (mChannel->HasLoadFlag(nsIRequest::LOAD_BYPASS_CACHE)) {
        accessReq = nsICache::ACCESS_WRITE; // replace cache entry
    } else {
        accessReq = nsICache::ACCESS_READ_WRITE; // normal browsing
    }

    // Check to see if we are not allowed to write to the cache:
    if (mChannel->HasLoadFlag(nsIRequest::INHIBIT_CACHING)) {
        accessReq &= ~nsICache::ACCESS_WRITE;
        if (accessReq == nsICache::ACCESS_NONE)
            return PR_FALSE;
    }

    // Generate cache key (remove trailing #ref if any):
    nsCAutoString key;
    mChannel->URI()->GetAsciiSpec(key);
    PRInt32 pos = key.RFindChar('#');
    if (pos != kNotFound)
        key.Truncate(pos);
    NS_ENSURE_FALSE(key.IsEmpty(), PR_FALSE);

    // Try to open a cache entry immediately, but if the cache entry is busy,
    // then wait for it to be available.

    session->OpenCacheEntry(key, accessReq, PR_FALSE,
                            getter_AddRefs(mCacheEntry));
    if (mCacheEntry)
        return PR_FALSE;  // great, we're ready to proceed!

    nsresult rv = session->AsyncOpenCacheEntry(key, accessReq, this);
    return NS_SUCCEEDED(rv);
}
