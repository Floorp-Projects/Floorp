/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsFtpConnectionThread.h"
#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsISocketTransportService.h"
#include "nsIURI.h"
#include "nsIBufferInputStream.h" // for our internal stream state
#include "nsIInputStream.h"
#include "nsIEventQueueService.h"
#include "nsIStringStream.h"
#include "nsIPipe.h"
#include "nsIBufferOutputStream.h"
#include "nsIMIMEService.h"
#include "nsXPIDLString.h" 
#include "nsIStreamConverterService.h"
#include "nsIIOService.h"
#include "prprf.h"
#include "prlog.h"
#include "netCore.h"
#include "ftpCore.h"
#include "nsIPrompt.h"
#include "nsProxiedService.h"
#include "nsINetSupportDialogService.h"
#include "nsFtpProtocolHandler.h"

static NS_DEFINE_CID(kIOServiceCID,                 NS_IOSERVICE_CID);
static NS_DEFINE_CID(kStreamConverterServiceCID,    NS_STREAMCONVERTERSERVICE_CID);
static NS_DEFINE_CID(kMIMEServiceCID,               NS_MIMESERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID,         NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kNetSupportDialogCID, NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kFTPHandlerCID, NS_FTPPROTOCOLHANDLER_CID);
static NS_DEFINE_IID(kProxyObjectManagerCID,        NS_PROXYEVENT_MANAGER_CID);
static NS_DEFINE_IID(kIFTPContextIID, NS_IFTPCONTEXT_IID);

#define FTP_CRLF "\r\n" 

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gFTPLog;
#endif /* PR_LOGGING */

class nsFTPContext : nsIFTPContext {
public:
    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIFTPContext methods
    NS_IMETHOD IsCmdResponse(PRBool *_retval) {
        *_retval = mCmdResponse;
        return NS_OK;
    };

    NS_IMETHOD SetContentType(const char *aContentType) {
        mContentType = aContentType;
        return NS_OK;
    };

    NS_IMETHOD GetContentType(char * *aContentType) {
        *aContentType = mContentType.ToNewCString();
        return NS_OK;
    };

    NS_IMETHOD SetContentLength(PRInt32 aLength) {
        mLength = aLength;
        return NS_OK;
    };

    NS_IMETHOD GetContentLength(PRInt32 *aLength) {
        *aLength = mLength;
        return NS_OK;
    }

    // nsFTPContext methods
    nsFTPContext() {
        NS_INIT_REFCNT();
        mCmdResponse = PR_TRUE;
        mLength = -1;
    };

    virtual ~nsFTPContext() {};

    PRBool          mCmdResponse;
    nsAutoString    mContentType;
    PRInt32         mLength;
};
NS_IMPL_ISUPPORTS(nsFTPContext, kIFTPContextIID);


// BEGIN: nsFtpConnectionThread implementation

NS_IMPL_ISUPPORTS2(nsFtpConnectionThread, nsIRunnable, nsIRequest);

nsFtpConnectionThread::nsFtpConnectionThread() {
    NS_INIT_REFCNT();
    mAction = GET;
    mUsePasv = PR_TRUE;
    mState = FTP_S_USER;
    mNextState = FTP_S_USER;
    mBin = PR_TRUE;
    mLength = 0;
    mConnected = PR_FALSE;
    mResetMode = PR_FALSE;
    mList      = PR_FALSE;
    mKeepRunning = PR_TRUE;
    mUseDefaultPath = PR_TRUE;
    mContinueRead = PR_FALSE;
    mAnonymous = PR_TRUE;
    mRetryPass = PR_FALSE;
    mCachedConn = PR_FALSE;
    mInternalError = NS_OK; // start out on the up 'n up.
    mSentStart = PR_FALSE;
    mConn = nsnull;
    mFTPContext = nsnull;
}

nsFtpConnectionThread::~nsFtpConnectionThread() {
    // lose the socket transport
    NS_RELEASE(mSTS);
    NS_RELEASE(mFTPContext);

    nsAllocator::Free(mURLSpec);
}

nsresult
nsFtpConnectionThread::Process() {
    nsresult rv;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFtpConnectionThread::Process() started for %x (spec =%s)\n", mURL, mURLSpec));
 
    while (mKeepRunning) {

    // churn the event pump
    PLEvent *event;
    rv = mFTPEventQueue->GetEvent(&event);
    if (NS_FAILED(rv)) return rv;
    rv = mFTPEventQueue->HandleEvent(event);
    if (NS_FAILED(rv)) return rv;

        switch(mState) {

//////////////////////////////
//// INTERNAL STATES
//////////////////////////////
            case FTP_READ_BUF:
                {
                char buffer[NS_FTP_BUFFER_READ_SIZE];
                PRUint32 read;
                rv = mCInStream->Read(buffer, NS_FTP_BUFFER_READ_SIZE, &read);
                if (NS_FAILED(rv)) {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - READ_BUF - Read() FAILED with rv = %d\n", mURL, rv));
                    mInternalError = NS_ERROR_FAILURE;
                    mState = FTP_ERROR;
                    break;
                }

                if (0 == read) {
                    // EOF
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - READ_BUF received EOF\n", mURL));
                    mState = FTP_COMPLETE;
                    break;
                }
                buffer[read] = '\0';
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - READ_BUF - read \"%s\" (%d bytes)", mURL, buffer, read));

                // get the response code out.
                if (!mContinueRead) {
                    PR_sscanf(buffer, "%d", &mResponseCode);
                    mResponseCode = mResponseCode / 100; // truncate it
                }

                // see if we're handling a multi-line response.
                if (mContinueRead || (buffer[3] == '-')) {
                    // yup, multi-line response, start appending
                    char *tmpBuffer = buffer, *crlf = nsnull;
                    PRBool lastLine = PR_FALSE;
                    while ( (crlf = PL_strstr(tmpBuffer, FTP_CRLF)) ) {
                        if (crlf) {
                            char tmpChar = crlf[2];
                            crlf[2] = '\0';
                            // see if this is the last line
                            lastLine = tmpBuffer[3] != '-';
                            mResponseMsg += tmpBuffer+4; // skip over the code and '-'
                            crlf[2] = tmpChar;
                            tmpBuffer = crlf+2;
                        }
                    }
                    if (*tmpBuffer)
                        mResponseMsg += tmpBuffer+4;

                    // see if this was the last line
                    if (lastLine || (*tmpBuffer && (tmpBuffer[3] != '-')) ) {
                        // yup. last line, let's move on.
                        if (mState == mNextState) {
                            NS_ASSERTION(0, "ftp read state mixup");
                            mInternalError = NS_ERROR_FAILURE;
                            mState = FTP_ERROR;
                        } else {
                            mState = mNextState;
                        }
                        mContinueRead = PR_FALSE;
                    } else {
                        // don't increment state, we need to read more.
                        mContinueRead = PR_TRUE;
                    }
                    break;
                }

                // get the rest of the line
                mResponseMsg = buffer+4;
                if (mState == mNextState) {
                    NS_ASSERTION(0, "ftp read state mixup");
                    mInternalError = NS_ERROR_FAILURE;
                    mState = FTP_ERROR;
                } else {
                    mState = mNextState;
                }
                break;
                }
                // END: FTP_READ_BUF

            case FTP_ERROR:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - ERROR\n", mURL));

                // if something went wrong, delete this connection entry and don't
                // bother putting it in the cache.
                NS_ASSERTION(mConn, "we should have created the conn obj upon Init()");
                delete mConn;

                rv = StopProcessing();
                break;
                }
                // END: FTP_ERROR

            case FTP_COMPLETE:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - COMPLETE\n", mURL));
                // push through all of the pertinent state into the cache entry;
                mConn->mCwd = mCwd;
                mConn->mUseDefaultPath = mUseDefaultPath;
                rv = mConnCache->InsertConn(mCacheKey.GetBuffer(), mConn);
                if (NS_FAILED(rv)) return rv;

                rv = StopProcessing();
                break;
                }
                // END: FTP_COMPLETE


//////////////////////////////
//// CONNECTION SETUP STATES
//////////////////////////////

            case FTP_S_USER:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_USER - ", mURL));
                rv = S_user();
                if (NS_FAILED(rv)) {
                    mInternalError = NS_ERROR_FTP_LOGIN;
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
                    mState = FTP_READ_BUF;
                    mNextState = FTP_R_USER;
                }
                break;
                }
                // END: FTP_S_USER

            case FTP_R_USER:
                {
                mState = R_user();
                if (FTP_ERROR == mState)
                    mInternalError = NS_ERROR_FTP_LOGIN;
                break;
                }
                // END: FTP_R_USER

            case FTP_S_PASS:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_PASS - ", mURL));
                rv = S_pass();
                if (NS_FAILED(rv)) {
                    mInternalError = NS_ERROR_FTP_LOGIN;
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
                    mState = FTP_READ_BUF;
                    mNextState = FTP_R_PASS;
                }
                break;    
                }
                // END: FTP_S_PASS

            case FTP_R_PASS:
                {
                mState = R_pass();
                if (FTP_ERROR == mState)
                    mInternalError = NS_ERROR_FTP_LOGIN;
                break;
                }
                // END: FTP_R_PASS    

            case FTP_S_SYST:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_SYST - ", mURL));
                rv = S_syst();
                if (NS_FAILED(rv)) {
                    mInternalError = rv;
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
                    mState = FTP_READ_BUF;
                    mNextState = FTP_R_SYST;
                }
                break;
                }
                // END: FTP_S_SYST

            case FTP_R_SYST: 
                {
                mState = R_syst();
                if (FTP_ERROR == mState)
                    mInternalError = NS_ERROR_FAILURE;
                break;
                }
                // END: FTP_R_SYST

            case FTP_S_ACCT:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_ACCT - ", mURL));
                rv = S_acct();
                if (NS_FAILED(rv)) {
                    mInternalError = NS_ERROR_FTP_LOGIN;
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
                    mState = FTP_READ_BUF;
                    mNextState = FTP_R_ACCT;
                }
                break;
                }
                // END: FTP_S_ACCT

            case FTP_R_ACCT:
                {
                mState = R_acct();
                if (FTP_ERROR == mState)
                    mInternalError = NS_ERROR_FTP_LOGIN;
                break;
                }
                // END: FTP_R_ACCT

            case FTP_S_MACB:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_MACB - ", mURL));
                rv = S_macb();
                if (NS_FAILED(rv)) {
                    mInternalError = rv;
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
                    mState = FTP_READ_BUF;
                    mNextState = FTP_R_MACB;
                }
                break;
                }
                // END: FTP_S_MACB

            case FTP_R_MACB:
                {
                mState = R_macb();
                if (FTP_ERROR == mState)
                    mInternalError = NS_ERROR_FAILURE;
                break;
                }
                // END: FTP_R_MACB

            case FTP_S_PWD:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_PWD - ", mURL));
                rv = S_pwd();
                if (NS_FAILED(rv)) {
                    mInternalError = rv;
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
                    mState = FTP_READ_BUF;
                    mNextState = FTP_R_PWD;
                }
                break;
                }
                // END: FTP_S_PWD

            case FTP_R_PWD:
                {
                mState = R_pwd();
                if (FTP_ERROR == mState)
                    mInternalError = NS_ERROR_FAILURE;
                break;
                }
                // END: FTP_R_PWD

            case FTP_S_MODE:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_MODE - ", mURL));
                rv = S_mode();
                if (NS_FAILED(rv)) {
                    mInternalError = NS_ERROR_FTP_MODE;
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
                    mState = FTP_READ_BUF;
                    mNextState = FTP_R_MODE;
                }
                break;
                }
                // END: FTP_S_MODE

            case FTP_R_MODE:
                {
                mState = R_mode();
                if (FTP_ERROR == mState)
                    mInternalError = NS_ERROR_FTP_MODE;
                break;
                }
                // END: FTP_R_MODE

            case FTP_S_CWD:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_CWD - ", mURL));
                rv = S_cwd();
                if (NS_FAILED(rv)) {
                    mInternalError = NS_ERROR_FTP_CWD;
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {                
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
                    mState = FTP_READ_BUF;
                    mNextState = FTP_R_CWD;
                }
                break;
                }
                // END: FTP_S_CWD

            case FTP_R_CWD:
                {
                mState = R_cwd();
                if (FTP_ERROR == mState)
                    mInternalError = NS_ERROR_FTP_CWD;
                break;
                }
                // END: FTP_R_CWD

            case FTP_S_SIZE:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_SIZE - ", mURL));
                rv = S_size();
                if (NS_FAILED(rv)) {
                    mInternalError = rv;
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
                    mState = FTP_READ_BUF;
                    mNextState = FTP_R_SIZE;
                }
                break;
                }
                // END: FTP_S_SIZE

            case FTP_R_SIZE: 
                {
                mState = R_size();
                if (FTP_ERROR == mState)
                    mInternalError = NS_ERROR_FAILURE;
                break;
                }
                // END: FTP_R_SIZE

            case FTP_S_MDTM:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_MDTM - ", mURL));
                rv = S_mdtm();
                if (NS_FAILED(rv)) {
                    mInternalError = rv;
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
                    mState = FTP_READ_BUF;
                    mNextState = FTP_R_MDTM;
                }
                break;
                }
                // END: FTP_S_MDTM;

            case FTP_R_MDTM:
                {
                mState = R_mdtm();
                if (FTP_ERROR == mState)
                    mInternalError = NS_ERROR_FAILURE;
                break;
                }
                // END: FTP_R_MDTM

            case FTP_S_LIST:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_LIST - ", mURL));
                rv = S_list();
                if (NS_FAILED(rv)) {
                    mInternalError = rv;
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
                    // get the data channel ready
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_LIST - Opening data stream ", mURL));
                    rv = mDPipe->OpenInputStream(0, -1, getter_AddRefs(mDInStream));
                    if (NS_FAILED(rv)) {
                        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                        return rv;
                    }
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));

                    mState = FTP_READ_BUF;
                    mNextState = FTP_R_LIST;
                }
                break;
                }
                // END: FTP_S_LIST

            case FTP_R_LIST:
                {
                mState = R_list();
                if (FTP_ERROR == mState)
                    mInternalError = NS_ERROR_FAILURE;
                mNextState = FTP_COMPLETE;
                mDInStream->Close();
                mDInStream = 0;
                break;
                }
                // END: FTP_R_LIST

            case FTP_S_RETR:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_RETR - ", mURL));
                rv = S_retr();
                if (NS_FAILED(rv)) {
                    mInternalError = rv;
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
                    // get the data channel ready
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_RETR - Opening data stream ", mURL));
                    rv = mDPipe->OpenInputStream(0, -1, getter_AddRefs(mDInStream));
                    if (NS_FAILED(rv)) {
                        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                        return rv;
                    }
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));

                    mState = FTP_READ_BUF;
                    mNextState = FTP_R_RETR;
                }
                break;
                }
                // END: FTP_S_RETR

            case FTP_R_RETR:
                {
                mState = R_retr();
                if (FTP_ERROR == mState)
                    mInternalError = NS_ERROR_FAILURE;
                mNextState = FTP_COMPLETE;
                mDInStream->Close();
                mDInStream = 0;
                break;
                }
                // END: FTP_R_RETR

                    
//////////////////////////////
//// DATA CONNECTION STATES
//////////////////////////////
            case FTP_S_PASV:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_PASV - ", mURL));
                rv = S_pasv();
                if (NS_FAILED(rv)) {
                    mInternalError = NS_ERROR_FTP_PASV;
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
                    mState = FTP_READ_BUF;
                    mNextState = FTP_R_PASV;
                }
                break;
                }
                // FTP: FTP_S_PASV

            case FTP_R_PASV:
                {
                mState = R_pasv();
                if (FTP_ERROR == mState)
                    mInternalError = NS_ERROR_FTP_PASV;
                break;
                }
                // END: FTP_R_PASV

//////////////////////////////
//// ACTION STATES
//////////////////////////////
            case FTP_S_DEL_FILE:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_DEL_FILE - ", mURL));
                rv = S_del_file();
                if (NS_FAILED(rv)) {
                    mInternalError = rv;
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
                    mState = FTP_READ_BUF;
                    mNextState = FTP_R_DEL_FILE;
                }
                break;
                }
                // END: FTP_S_DEL_FILE

            case FTP_R_DEL_FILE:
                {
                mState = R_del_file();
                if (FTP_ERROR == mState)
                    mInternalError = NS_ERROR_FAILURE;
                break;
                }
                // END: FTP_R_DEL_FILE

            case FTP_S_DEL_DIR:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_DEL_DIR - ", mURL));
                rv = S_del_dir();
                if (NS_FAILED(rv)) {
                    mInternalError = NS_ERROR_FTP_DEL_DIR;
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
                    mState = FTP_READ_BUF;
                    mNextState = FTP_R_DEL_DIR;
                }
                break;
                }
                // END: FTP_S_DEL_DIR

            case FTP_R_DEL_DIR:
                {
                mState = R_del_dir();
                if (FTP_ERROR == mState)
                    mInternalError = NS_ERROR_FTP_DEL_DIR;
                break;
                }
                // END: FTP_R_DEL_DIR

            case FTP_S_MKDIR:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_MKDIR - ", mURL));
                rv = S_mkdir();
                if (NS_FAILED(rv)) {
                    mInternalError = NS_ERROR_FTP_MKDIR;
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
                    mState = FTP_READ_BUF;
                    mNextState = FTP_R_MKDIR;
                }
                break;
                }
                // END: FTP_S_MKDIR

            case FTP_R_MKDIR: 
                {
                mState = R_mkdir();
                if (FTP_ERROR == mState)
                    mInternalError = NS_ERROR_FTP_MKDIR;
                break;
                }
                // END: FTP_R_MKDIR

            default:
                ;

        } // END: switch 
    } // END: while loop

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFtpConnectionThread::Process() ended for %x (spec =%s)\n\n\n", mURL, mURLSpec));

    return NS_OK;
}

///////////////////////////////////
// STATE METHODS
///////////////////////////////////
nsresult
nsFtpConnectionThread::S_user() {
    nsresult rv;
    PRUint32 bytes;
    nsCAutoString usernameStr("USER ");

    if (mAnonymous) {
        usernameStr.Append("anonymous");
    } else {
        if (!mUsername.Length()) {
            NS_WITH_PROXIED_SERVICE(nsIPrompt, authdialog, kNetSupportDialogCID, nsnull, &rv);
            if (NS_FAILED(rv)) return rv;

            PRUnichar *user, *passwd;
            PRBool retval;
            static nsAutoString message;
            if (message.Length() < 1) {
                char *host;
                rv = mURL->GetHost(&host);
                if (NS_FAILED(rv)) return rv;
                message = "Enter username and password for "; //TODO localize it!
                message += host;
                nsAllocator::Free(host);
            }
		    rv = authdialog->PromptUsernameAndPassword(message.GetUnicode(), &user, &passwd, &retval);
            if (retval) {
                mUsername = user;
                mPassword = passwd;
            }
        }
        usernameStr.Append(mUsername);    
    }
    usernameStr.Append(FTP_CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL, usernameStr.GetBuffer()));

    rv = mCOutStream->Write(usernameStr.GetBuffer(), usernameStr.Length(), &bytes);
    return rv;
}

FTP_STATE
nsFtpConnectionThread::R_user() {
    if (mResponseCode == 3) {
        // send off the password
        return FTP_S_PASS;
    } else if (mResponseCode == 2) {
        // no password required, we're already logged in
        return FTP_S_SYST;
    } else {
        // LOGIN FAILED
        if (mAnonymous) {
            // we just tried to login anonymously and failed.
            // kick back out to S_user() and try again after
            // gathering uname/pwd info from the user.
            mAnonymous = PR_FALSE;
            return FTP_S_USER;
        } else {
            return FTP_ERROR;
        }
    }
}


nsresult
nsFtpConnectionThread::S_pass() {
    nsresult rv;
    PRUint32 bytes;
    nsCAutoString passwordStr("PASS ");

    if (mAnonymous) {
        passwordStr.Append("mozilla@");
    } else {
        if (!mPassword.Length() || mRetryPass) {
            // ignore any password we have, it's not working
            NS_WITH_PROXIED_SERVICE(nsIPrompt, authdialog, kNetSupportDialogCID, nsnull, &rv);
            if (NS_FAILED(rv)) return rv;

            PRUnichar *passwd;
            PRBool retval;
            static nsAutoString message;
            if (message.Length() < 1) {
                char *host;
                rv = mURL->GetHost(&host);
                if (NS_FAILED(rv)) return rv;
                message = "Enter password for "; //TODO localize it!
		        message += mUsername;
                message += " on ";
                message += host;
                nsAllocator::Free(host);
            }
		    rv = authdialog->PromptPassword(message.GetUnicode(), &passwd, &retval);
            if (retval) {
                mPassword = passwd;
            }
        }
        passwordStr.Append(mPassword);    
    }
    passwordStr.Append(FTP_CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL, passwordStr.GetBuffer()));

    rv = mCOutStream->Write(passwordStr.GetBuffer(), passwordStr.Length(), &bytes);
    return rv;
}

FTP_STATE
nsFtpConnectionThread::R_pass() {
    if (mResponseCode == 3) {
        // send account info
        return FTP_S_ACCT;
    } else if (mResponseCode == 2) {
        // logged in
        return FTP_S_SYST;
    } else {
        // kick back out to S_pass() and ask the user again.
        mRetryPass = PR_TRUE;
        return FTP_S_PASS;
    }
}

nsresult
nsFtpConnectionThread::S_syst() {
    char *buffer = "SYST" FTP_CRLF;
    PRUint32 bytes;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL, buffer));

    return mCOutStream->Write(buffer, PL_strlen(buffer), &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_syst() {
    FTP_STATE state = FTP_S_PWD;
    if (mResponseCode == 2) {

        SetSystInternals(); // must be called first to setup member vars.
 
        if (!mUseDefaultPath)
            state = FindActionState();

        // setup next state based on server type.
        if (mServerType == FTP_PETER_LEWIS_TYPE || mServerType == FTP_WEBSTAR_TYPE)
            state = FTP_S_MACB;
    }

    return state;
}

nsresult
nsFtpConnectionThread::S_acct() {
    char *buffer = "ACCT noaccount" FTP_CRLF;
    PRUint32 bytes;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL, buffer));

    return mCOutStream->Write(buffer, PL_strlen(buffer), &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_acct() {
    if (mResponseCode == 2)
        return FTP_S_SYST;
    else
        return FTP_ERROR;
}

nsresult
nsFtpConnectionThread::S_macb() {
    char *buffer = "MACB ENABLE" FTP_CRLF;
    PRUint32 bytes;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL, buffer));

    return mCOutStream->Write(buffer, PL_strlen(buffer), &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_macb() {
    if (mResponseCode == 2) {
        // set the mac binary
        if (mServerType == FTP_UNIX_TYPE) {
            // This state is carry over from the old ftp implementation
            // I'm not sure what's really going on here.
            // original comment "we were unsure here"
            mServerType = FTP_NCSA_TYPE;    
        }
    }
    return FindActionState();
}

nsresult
nsFtpConnectionThread::S_pwd() {
    char *buffer = "PWD" FTP_CRLF;
    PRUint32 bytes;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL, buffer));

    return mCOutStream->Write(buffer, PL_strlen(buffer), &bytes);
}


FTP_STATE
nsFtpConnectionThread::R_pwd() {
    nsresult rv;
    FTP_STATE state = FTP_ERROR;
    nsCAutoString lNewMsg(mResponseMsg);
    // fun response interpretation begins :)

    // check for a quoted path
    PRInt32 beginQ, endQ;
    beginQ = lNewMsg.Find("\"");
    if (beginQ > -1) {
        endQ = lNewMsg.RFind("\"");
        if (endQ > beginQ) {
            // quoted path
            nsCAutoString tmpPath;
            lNewMsg.Mid(tmpPath, beginQ+1, (endQ-beginQ-1));
            lNewMsg = tmpPath;
        }
    } 

    // default next state
    state = FindActionState();

    // reset server types if necessary
    if (mServerType == FTP_TCPC_TYPE) {
        if (lNewMsg.CharAt(1) == '/') {
            mServerType = FTP_NCSA_TYPE;
        }
    }
    else if(mServerType == FTP_GENERIC_TYPE) {
        if (lNewMsg.CharAt(1) == '/') {
            // path names ending with '/' imply unix
            mServerType = FTP_UNIX_TYPE;
            mList = PR_TRUE;
        } else if (lNewMsg.Last() == ']') {
            // path names ending with ']' imply vms
            mServerType = FTP_VMS_TYPE;
            mList = PR_TRUE;
        }
    }

    // we only want to use the parent working directory (pwd) when
    // the url supplied provides ambiguous path info (such as /./, or
    // no slash at all.

    if (mUseDefaultPath && mServerType != FTP_VMS_TYPE) {
        mUseDefaultPath = PR_FALSE;
        // we want to use the default path specified by the PWD command.
        nsCAutoString ptr;

        if (lNewMsg.First() != '/') {
            PRInt32 start = lNewMsg.FindChar('/');
            if (start > -1) {
                lNewMsg.Right(ptr, start); // use everything after the first slash (inclusive)
            } else {
                // if we couldn't find a slash, check for back slashes and switch them out.
                start = lNewMsg.FindChar('\\');
                if (start > -1) {
                    lNewMsg.ReplaceChar("\\", '/');
                }
                ptr = lNewMsg;
            }
        } else {
            ptr = lNewMsg;
        }

        // construct the new url by appending
        // the initial path to the new path.
        if (ptr.Length()) {

            char *initialPath = nsnull;
            rv = mURL->GetPath(&initialPath);
            if (NS_FAILED(rv)) return FTP_ERROR;

            if (ptr.Last() == '/') {
                PRUint32 insertionOffset = ptr.Length() - 1;
                ptr.Cut(insertionOffset, 1);
                ptr.Insert(initialPath, insertionOffset);
            } else {
                ptr.Append(initialPath);
            }
            nsAllocator::Free(initialPath);

            const char *p = ptr.GetBuffer();
            rv = mURL->SetPath(p);
            if (NS_FAILED(rv)) return FTP_ERROR;
        }
    }

    // change state for these servers.
    if (mServerType == FTP_GENERIC_TYPE
        || mServerType == FTP_NCSA_TYPE
        || mServerType == FTP_TCPC_TYPE
        || mServerType == FTP_WEBSTAR_TYPE
        || mServerType == FTP_PETER_LEWIS_TYPE)
        state = FTP_S_MACB;

    return state;
}

nsresult
nsFtpConnectionThread::S_mode() {
    char *buffer;
    PRUint32 bytes;
    if (mBin) {
        buffer = "TYPE I" FTP_CRLF;
    } else {
        buffer = "TYPE A" FTP_CRLF;
    }

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL, buffer));

    return mCOutStream->Write(buffer, PL_strlen(buffer), &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_mode() {
    if (mResponseCode != 2) {
        return FTP_ERROR;
    }

    if (mAction == PUT) {
        return FTP_S_CWD;
    } else {
        if (mResetMode) {
            // if this was a mode reset, we know it was from a previous R_size() call
            // don't bother going back into size
            return FTP_S_CWD;
        } else {
            return FTP_S_SIZE;
        }
    }
}

nsresult
nsFtpConnectionThread::S_cwd() {
    nsresult rv;
    char *path;
    PRUint32 bytes;
    rv = mURL->GetPath(&path);
    if (NS_FAILED(rv)) return rv;

    nsCAutoString cwdStr("CWD ");

    if (mServerType == FTP_VMS_TYPE) {
        char *slash = nsnull;
        
        if ( (slash = PL_strrchr(path, '/')) ) {
            *slash = '\0';
            cwdStr.Append("[");
            cwdStr.Append(path);
            cwdStr.Append("]" FTP_CRLF);
            *slash = '/';

            // turn '/'s into '.'s
            //while ( (slash = PL_strchr(buffer, '/')) )
            //    *slash = '.';
        } else {
            cwdStr.Append("[");
            cwdStr.Append(".");
            cwdStr.Append(path);
            cwdStr.Append("]" FTP_CRLF);
        }
    } else {
        // non VMS server
        cwdStr.Append(path);
        cwdStr.Append(FTP_CRLF);
    }
    nsAllocator::Free(path);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL, cwdStr.GetBuffer()));

    cwdStr.Mid(mCwdAttempt, 3, cwdStr.Length() - 4);

    rv = mCOutStream->Write(cwdStr.GetBuffer(), cwdStr.Length(), &bytes);
    return rv;
}

FTP_STATE
nsFtpConnectionThread::R_cwd() {
    FTP_STATE state = FTP_ERROR;
    if (mResponseCode == 2) {
        mCwd = mCwdAttempt;
        // success
        if (mAction == PUT) {
            // we are uploading
            state = FTP_S_PUT;
        }
    
        // we are GETting a file or dir

        if (mServerType == FTP_VMS_TYPE) {
            if (mFilename.Length() > 0) {
                // it's a file
                state = FTP_S_RETR;
            }
        }

        // we're not VMS, and we've already failed a RETR.
        state = FTP_S_LIST;
    } else {
        state = FTP_ERROR;
    }
    return state;
}

nsresult
nsFtpConnectionThread::S_size() {
    nsresult rv;
    char *path;
    PRUint32 bytes;
    rv = mURL->GetPath(&path);
    if (NS_FAILED(rv)) return rv;

    // XXX should the actual file name be parsed out???
    mFilename = path; 

    /*if (mServerType == FTP_VMS_TYPE) {
        mState = FindGetState();
        break;
    }*/

    nsCAutoString sizeBuf("SIZE ");
    sizeBuf.Append(path);
    sizeBuf.Append(FTP_CRLF);

    nsAllocator::Free(path);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL, sizeBuf.GetBuffer()));

    rv = mCOutStream->Write(sizeBuf.GetBuffer(), sizeBuf.Length(), &bytes);
    return rv;
}

FTP_STATE
nsFtpConnectionThread::R_size() {
    if (mResponseCode == 2) {
        PRInt32 conversionError;
        mLength = mResponseMsg.ToInteger(&conversionError);

        mFTPContext->SetContentLength(mLength);
    } if (mResponseCode == 5) {
        // couldn't get the size of what we asked for, must be a dir.
        mBin = PR_FALSE;
        // if it's a dir, we need to reset the mode to ascii (we default to bin)
        mResetMode = PR_TRUE;
        return FTP_S_MODE;
    }
    return FTP_S_MDTM;
}

nsresult
nsFtpConnectionThread::S_mdtm() {
    nsresult rv;
    char *path;
    PRUint32 bytes;
    rv = mURL->GetPath(&path);
    if (NS_FAILED(rv)) return rv;

    /*if (mServerType == FTP_VMS_TYPE) {
        mState = FindGetState();
        break;
    }*/

    nsCAutoString mdtmStr("MDTM ");
    mdtmStr.Append(path);
    mdtmStr.Append(FTP_CRLF);
    nsAllocator::Free(path);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL, mdtmStr.GetBuffer()));

    rv = mCOutStream->Write(mdtmStr.GetBuffer(), mdtmStr.Length(), &bytes);
    return rv;
}

FTP_STATE
nsFtpConnectionThread::R_mdtm() {
    if (mResponseCode == 2) {
         // The time is returned in ISO 3307 "Representation
         // of the Time of Day" format. This format is YYYYMMDDHHmmSS or
         // YYYYMMDDHHmmSS.xxx, where
         //         YYYY    is the year
         //         MM      is the month (01-12)
         //         DD      is the day of the month (01-31)
         //         HH      is the hour of the day (00-23)
         //         mm      is the minute of the hour (00-59)
         //         SS      is the second of the hour (00-59)
         //         xxx     if present, is a fractional second and may be any length
         // Time is expressed in UTC (GMT), not local time.
        PRExplodedTime ts;
        char *timeStr = mResponseMsg.ToNewCString();
        if (!timeStr) return FTP_ERROR;

        ts.tm_year =
         (timeStr[0] - '0') * 1000 +
         (timeStr[1] - '0') *  100 +
         (timeStr[2] - '0') *   10 +
         (timeStr[3] - '0');

        ts.tm_month = ((timeStr[4] - '0') * 10 + (timeStr[5] - '0')) - 1;
        ts.tm_mday = (timeStr[6] - '0') * 10 + (timeStr[7] - '0');
        ts.tm_hour = (timeStr[8] - '0') * 10 + (timeStr[9] - '0');
        ts.tm_min  = (timeStr[10] - '0') * 10 + (timeStr[11] - '0');
        ts.tm_sec  = (timeStr[12] - '0') * 10 + (timeStr[13] - '0');
        ts.tm_usec = 0;

        mLastModified = PR_ImplodeTime(&ts);
    }

    return FindGetState();
}

nsresult
nsFtpConnectionThread::S_list() {
    char *buffer;
    PRUint32 bytes;
    if (mList)
        buffer = "LIST" FTP_CRLF;
    else
        buffer = "NLST" FTP_CRLF;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL, buffer));

    return mCOutStream->Write(buffer, PL_strlen(buffer), &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_list() {
    nsresult rv;

    if ((mResponseCode == 4) 
        ||
        (mResponseCode == 5)) {
        // unable to open the data connection.
        return FTP_ERROR;
    }

    // setup a listener to push the data into. This listener sits inbetween the
    // unconverted data of fromType, and the final listener in the chain (in this case
    // the mListener).
    nsCOMPtr<nsIStreamListener> converterListener;

    NS_WITH_SERVICE(nsIStreamConverterService, StreamConvService, kStreamConverterServiceCID, &rv);
    if (NS_FAILED(rv)) {
        return FTP_ERROR;
    }

    nsAutoString fromStr("text/ftp-dir-");
    SetDirMIMEType(fromStr);

    // all FTP directory listings are converted to http-index
    nsAutoString toStr("application/http-index-format");

    rv = StreamConvService->AsyncConvertData(fromStr.GetUnicode(),
                                             toStr.GetUnicode(),
                                             mSyncListener, mURL, getter_AddRefs(converterListener));
    if (NS_FAILED(rv)) {
        return FTP_ERROR;
    }

    rv = converterListener->OnStartRequest(mChannel, mContext);
    if (NS_FAILED(rv)) return FTP_ERROR;

    mSentStart = PR_TRUE;


    // The only way out of this loop is if the stream read
    // fails or we read the end of the stream.
    while (1) {
        PRUint32 read;
        char *listBuf = (char*)nsAllocator::Alloc(NS_FTP_BUFFER_READ_SIZE + 1);
        if (!listBuf) return FTP_ERROR;

        rv = mDInStream->Read(listBuf, NS_FTP_BUFFER_READ_SIZE, &read);
        if (NS_FAILED(rv)) {
            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x R_list() data pipe read failed w/ rv = %d\n", mURL, rv));
            nsAllocator::Free(listBuf);
            return FTP_ERROR;
        } else if (read < 1) {
            // EOF
            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x R_list() data pipe read hit EOF\n", mURL));
            nsAllocator::Free(listBuf);
            return FTP_READ_BUF;
        }
        listBuf[read] = '\0';

        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x R_list() data pipe read %d bytes:\n%s\n", mURL, read, listBuf));

        nsCOMPtr<nsISupports> stringStreamSup;
        rv = NS_NewCharInputStream(getter_AddRefs(stringStreamSup), listBuf);
        if (NS_FAILED(rv)) return FTP_ERROR;

        nsCOMPtr<nsIInputStream> listStream = do_QueryInterface(stringStreamSup, &rv);

        rv = converterListener->OnDataAvailable(mChannel, mFTPContext, listStream, 0, read);
        if (NS_FAILED(rv)) return FTP_ERROR;
    }


    // NOTE: that we're not firing an OnStopRequest() to the converterListener (the
    //  stream converter). It (at least this implementation of it) doesn't care 
    //  about stops. FTP_COMPLETE will send the stop.

    return FTP_READ_BUF;
}

nsresult
nsFtpConnectionThread::S_retr() {
    nsresult rv;
    char *path;
    PRUint32 bytes;
    rv = mURL->GetPath(&path);
    if (NS_FAILED(rv)) return rv;

    nsCAutoString retrStr("RETR ");

    if (mServerType == FTP_VMS_TYPE) {
        NS_ASSERTION(mFilename.Length() > 0, "ftp filename not present");
        retrStr.Append(mFilename);
    } else {
        retrStr.Append(path);
    }
    nsAllocator::Free(path);
    retrStr.Append(FTP_CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL, retrStr.GetBuffer()));

    rv = mCOutStream->Write(retrStr.GetBuffer(), retrStr.Length(), &bytes);
    return rv;
}

FTP_STATE
nsFtpConnectionThread::R_retr() {
    nsresult rv;
    // The only way we can get here is if we knew we were dealing with
    // a file and not a dir listing. This state assumes we're retrieving
    // a file!
    if (mResponseCode == 1) {
        // success.

        rv = mListener->OnStartRequest(mChannel, mContext);
        if (NS_FAILED(rv)) return FTP_ERROR;

        mSentStart = PR_TRUE;

        // build up context info for the nsFTPChannel
        NS_WITH_SERVICE(nsIMIMEService, MIMEService, kMIMEServiceCID, &rv);
        if (NS_FAILED(rv)) return FTP_ERROR;

        char *contentType;
        rv = MIMEService->GetTypeFromURI(mURL, &contentType);

        // if we fail, we want to push the data on up anyway. let the app figure
        // out what to do.
        if (NS_SUCCEEDED(rv)) {
            mFTPContext->SetContentType(contentType);
        }

        nsIBufferInputStream *bufInStrm = nsnull;
        nsIBufferOutputStream *bufOutStrm = nsnull;
        rv = NS_NewPipe(&bufInStrm, &bufOutStrm);
        if (NS_FAILED(rv)) return FTP_ERROR;

        nsIInputStream *inStream = nsnull;
        rv = bufInStrm->QueryInterface(NS_GET_IID(nsIInputStream), (void**)&inStream);
        if (NS_FAILED(rv)) return FTP_ERROR;

        PRUint32 read = 0, readSoFar = 0;
        while (readSoFar < NS_FTP_BUFFER_READ_SIZE) {
            // suck in the data from the server
            PRUint32 avail;
            rv = inStream->Available(&avail);
            if (NS_FAILED(rv)) return FTP_ERROR;

            rv = bufOutStrm->WriteFrom(mDInStream, NS_FTP_BUFFER_READ_SIZE-readSoFar, &read);
            if (NS_FAILED(rv)) return FTP_ERROR;
            readSoFar += read;
            if (read == 0) {
                // we've exhausted the stream, send any data we have left and get out of dodge.
                rv = mListener->OnDataAvailable(mChannel, mFTPContext, inStream, avail, readSoFar);
                if (NS_FAILED(rv)) return FTP_ERROR;

                break; // this terminates the loop
            }

            if (readSoFar == NS_FTP_BUFFER_READ_SIZE) {
                // we've filled our buffer, send the data off
                readSoFar = 0;
                rv = mListener->OnDataAvailable(mChannel, mContext, inStream, avail, NS_FTP_BUFFER_READ_SIZE);
                if (NS_FAILED(rv)) return FTP_ERROR;
            }
        }

        // we're done filling this end of the pipe. close it.
        bufOutStrm->Close();
        NS_RELEASE(inStream);

        return FTP_READ_BUF;

    } else {
        if (mServerType == FTP_VMS_TYPE) {
            return FTP_ERROR;
        } else {
            return FTP_S_CWD;
        }
    }
}


nsresult
nsFtpConnectionThread::S_pasv() {
    char *buffer = "PASV" FTP_CRLF;
    PRUint32 bytes;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL, buffer));

    return mCOutStream->Write(buffer, PL_strlen(buffer), &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_pasv() {
    nsresult rv;
    PRInt32 h0, h1, h2, h3, p0, p1;
    PRInt32 port;

    if (mResponseCode != 2)  {
        // failed. increment to port.
        // mState = FTP_S_PORT;

        mUsePasv = PR_FALSE;
        return FTP_ERROR;
    }

    mUsePasv = PR_TRUE;

    char *ptr = nsnull;
    char *response = mResponseMsg.ToNewCString();
    if (!response) return FTP_ERROR;

    // The returned address string can be of the form
    // (xxx,xxx,xxx,xxx,ppp,ppp) or
    //  xxx,xxx,xxx,xxx,ppp,ppp (without parens)
    ptr = response;
    while (*ptr++) {
        if (*ptr == '(') {
            // move just beyond the open paren
            ptr++;
            break;
        }
        if (*ptr == ',') {
            ptr--;
            // backup to the start of the digits
            while ( (*ptr >= '0') && (*ptr <= '9'))
                ptr--;
            ptr++; // get back onto the numbers
            break;
        }
    } // end while

    PRInt32 fields = PR_sscanf(ptr, 
#ifdef __alpha
        "%d,%d,%d,%d,%d,%d",
#else
        "%ld,%ld,%ld,%ld,%ld,%ld",
#endif
        &h0, &h1, &h2, &h3, &p0, &p1);

    if (fields < 6) {
        // bad format. we'll try PORT, but it's probably over.
        
        mUsePasv = PR_FALSE;
        return FTP_ERROR;
    }

    port = ((PRInt32) (p0<<8)) + p1;
    nsCString host;
    host.Append(h0);
    host.Append(".");
    host.Append(h1);
    host.Append(".");
    host.Append(h2);
    host.Append(".");
    host.Append(h3);

    nsAllocator::Free(response);

    // now we know where to connect our data channel
    rv = mSTS->CreateTransport(host.GetBuffer(), port, nsnull, getter_AddRefs(mDPipe)); // the data channel
    if (NS_FAILED(rv)) return FTP_ERROR;

    if (mAction == GET) {
        // Setup the data channel for file reception
    } else {
        // get the output stream so we can write to the server
        rv = mDPipe->OpenOutputStream(0, getter_AddRefs(mDOutStream));
        if (NS_FAILED(rv)) return FTP_ERROR;
    }

    // we're connected figure out what type of transfer we're doing (ascii or binary)
    char *type = nsnull;
    rv = mChannel->GetContentType(&type);
    nsCAutoString typeStr;
    if (NS_FAILED(rv) || !type) 
        typeStr = "bin";
    else
        typeStr = type;

    mContentType = typeStr;

    nsAllocator::Free(type);
    PRInt32 textType = typeStr.Find("text");
    if (textType == 0)
        // only send ascii for text type files
        mBin = PR_FALSE;
    else
        // all others are bin
        mBin = PR_TRUE;
    return FTP_S_MODE;
}

nsresult
nsFtpConnectionThread::S_del_file() {
    nsresult rv;
    char *filename;
    PRUint32 bytes;
    rv = mURL->GetPath(&filename); // XXX we should probably check to 
                                   // XXX make sure we have an actual filename.
    if (NS_FAILED(rv)) return rv;

    nsCAutoString delStr("DELE ");
    delStr.Append(filename);
    delStr.Append(FTP_CRLF);
    nsAllocator::Free(filename);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL, delStr.GetBuffer()));

    rv = mCOutStream->Write(delStr.GetBuffer(), delStr.Length(), &bytes);
    return rv;
}

FTP_STATE
nsFtpConnectionThread::R_del_file() {
    if (mResponseCode != 2) {
        return FTP_S_DEL_DIR;
    }

    return FTP_COMPLETE;
}

nsresult
nsFtpConnectionThread::S_del_dir() {
    nsresult rv;
    char *dir;
    PRUint32 bytes;
    rv = mURL->GetPath(&dir);
    if (NS_FAILED(rv)) return rv;

    nsCAutoString delDirStr("RMD ");
    delDirStr.Append(dir);
    delDirStr.Append(FTP_CRLF);
    nsAllocator::Free(dir);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL, delDirStr.GetBuffer()));

    rv = mCOutStream->Write(delDirStr.GetBuffer(), delDirStr.Length(), &bytes);
    return rv;
}

FTP_STATE
nsFtpConnectionThread::R_del_dir() {
    if (mResponseCode != 2) {
        return FTP_ERROR;
    }
    return FTP_COMPLETE;
}

nsresult
nsFtpConnectionThread::S_mkdir() {
    nsresult rv;
    char *dir;
    PRUint32 bytes;
    rv = mURL->GetPath(&dir);
    if (NS_FAILED(rv)) return rv;

    nsCAutoString mkdirStr("MKD ");
    mkdirStr.Append(dir);
    mkdirStr.Append(FTP_CRLF);
    nsAllocator::Free(dir);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL, mkdirStr.GetBuffer()));

    rv = mCOutStream->Write(mkdirStr.GetBuffer(), mkdirStr.Length(), &bytes);
    return rv;
}

FTP_STATE
nsFtpConnectionThread::R_mkdir() {
    if (mResponseCode != 2) {
        return FTP_ERROR;
    }
    return FTP_COMPLETE;
}

///////////////////////////////////
// END: STATE METHODS
///////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
// nsIRunnable method:

NS_IMETHODIMP
nsFtpConnectionThread::Run() {
    nsresult rv;
    
    rv = nsServiceManager::GetService(kSocketTransportServiceCID,
                                      NS_GET_IID(nsISocketTransportService), 
                                      (nsISupports **)&mSTS);
    if(NS_FAILED(rv)) return rv;

    /////////////////////////
    // COMMAND CHANNEL SETUP
    /////////////////////////
    char *host;
    rv = mURL->GetHost(&host);
    if (NS_FAILED(rv)) return rv;
    PRInt32 port;
    rv = mURL->GetPort(&port);
    if (NS_FAILED(rv)) return rv;

    // use a cached connection if there is one.
    rv = mConnCache->RemoveConn(mCacheKey.GetBuffer(), &mConn);
    if (NS_FAILED(rv)) return rv;

    if (mConn) {
        mCachedConn = PR_TRUE;
        // we were passed in a connection to use
        mCPipe = mConn->mSocketTransport;

        mCInStream  = mConn->mInputStream;
        mCOutStream = mConn->mOutputStream;

        mServerType = mConn->mServerType;
        mCwd        = mConn->mCwd;
        mList       = mConn->mList;
        mUseDefaultPath = mConn->mUseDefaultPath;
    } else {
        // build our own
        rv = mSTS->CreateTransport(host, port, nsnull, getter_AddRefs(mCPipe)); // the command channel
        nsAllocator::Free(host);
        if (NS_FAILED(rv)) return rv;

        // get the output stream so we can write to the server
        rv = mCPipe->OpenOutputStream(0, getter_AddRefs(mCOutStream));
        if (NS_FAILED(rv))  return rv;

        rv = mCPipe->OpenInputStream(0, -1, getter_AddRefs(mCInStream));
        if (NS_FAILED(rv)) return rv;

        // cache this stuff.
        mConn = new nsConnectionCacheObj(mCPipe, mCInStream, mCOutStream);
        if (!mConn) return NS_ERROR_OUT_OF_MEMORY;
    }

    ///////////////////////////////
    // END - COMMAND CHANNEL SETUP
    ///////////////////////////////

    if (!mCachedConn) {
        // digest any server greeting.
        char greetBuf[NS_FTP_BUFFER_READ_SIZE];
        for (PRInt32 i=0; i < NS_FTP_BUFFER_READ_SIZE; i++)
            greetBuf[i] = '\0';
        PRUint32 read;
        rv = mCInStream->Read(greetBuf, NS_FTP_BUFFER_READ_SIZE, &read);
        if (NS_FAILED(rv)) {
            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x nsFTPConnTrd::Run() greeting read failed with rv = %d\n", mURL, rv));
            return rv;
        }

        if (read > 0) {
            // we got something.
            // look for a response code
            switch (greetBuf[0]) {
            case '2':
                PR_sscanf(greetBuf, "%d", &mResponseCode);
                mResponseCode = mResponseCode / 100; // truncate it

                // we're receiving some data
                // see if it's multiline
                if (greetBuf[3] == '-') {
                    // yup, multi-line. be sure to digest the rest of it later.
                    char *tmpBuffer = greetBuf, *crlf = nsnull;
                    PRBool lastLine = PR_FALSE;
                    while ( (crlf = PL_strstr(tmpBuffer, FTP_CRLF)) ) {
                        if (crlf) {
                            char tmpChar = crlf[2];
                            crlf[2] = '\0';
                            // see if this is the last line
                            lastLine = tmpBuffer[3] != '-';
                            mResponseMsg += tmpBuffer+4; // skip over the code and '-'
                            crlf[2] = tmpChar;
                            tmpBuffer = crlf+2;
                        }
                    }
                    if (*tmpBuffer)
                        mResponseMsg += tmpBuffer+4;

                    // see if this was the last line
                    if (lastLine || (*tmpBuffer && (tmpBuffer[3] != '-')) ) {
                        // yup. last line, let's move on.
                        mContinueRead = PR_FALSE;
                        mState = mNextState;
                    } else {
                        //we need to read more.
                        mContinueRead = PR_TRUE;
                        mState = FTP_READ_BUF;
                    }
                }
                break;
            default:
                break;
            }
        }
    } else {
        // we're already connected to this server.
        // skip login.
        mState = FTP_S_PWD;
    }

    mConnected = PR_TRUE;

    rv = Process();

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsFtpConnectionThread::IsPending(PRBool *result)
{
    nsresult rv = NS_OK;
    *result = PR_FALSE;
    if (mCPipe) {
        rv = mCPipe->IsPending(result);
        if (NS_FAILED(rv)) return rv;
    }
    if (mDPipe) {
        rv = mDPipe->IsPending(result);
        if (NS_FAILED(rv)) return rv;
    }
    return rv;
}

NS_IMETHODIMP
nsFtpConnectionThread::Cancel(void)
{
    nsresult rv = NS_OK;
    if (mCPipe) {
        rv = mCPipe->Cancel();
        if (NS_FAILED(rv)) return rv;
    }
    if (mDPipe) {
        rv = mDPipe->Cancel();
        if (NS_FAILED(rv)) return rv;
    }
    return rv;
}

NS_IMETHODIMP
nsFtpConnectionThread::Suspend(void)
{
    nsresult rv = NS_OK;
    if (mCPipe) {
        rv = mCPipe->Suspend();
        if (NS_FAILED(rv)) return rv;
    }
    if (mDPipe) {
        rv = mDPipe->Suspend();
        if (NS_FAILED(rv)) return rv;
    }
    return rv;
}

NS_IMETHODIMP
nsFtpConnectionThread::Resume(void)
{
    nsresult rv = NS_OK;
    if (mCPipe) {
        rv = mCPipe->Resume();
        if (NS_FAILED(rv)) return rv;
    }
    if (mDPipe) {
        rv = mDPipe->Resume();
        if (NS_FAILED(rv)) return rv;
    }
    return rv;
}

nsresult
nsFtpConnectionThread::Init(nsIEventQueue* aFTPEventQ,
                            nsIURI* aUrl,
                            nsIEventQueue* aEventQ,
                            nsIProtocolHandler* aHandler,
                            nsIChannel* aChannel,
                            nsISupports* aContext) {
    nsresult rv;

    NS_ASSERTION(aFTPEventQ, "FTP: thread needs an event queue to process");
    mFTPEventQueue = aFTPEventQ;

    NS_ASSERTION(aChannel, "FTP: thread needs a channel");

    NS_WITH_SERVICE(nsIProxyObjectManager, pIProxyObjectManager, kProxyObjectManagerCID, &rv);
    if(NS_FAILED(rv)) return rv;

    rv = pIProxyObjectManager->GetProxyObject(aEventQ, 
                                              NS_GET_IID(nsIStreamListener), 
                                              aChannel,
                                              PROXY_ASYNC | PROXY_ALWAYS,
                                              getter_AddRefs(mListener));
    if (NS_FAILED(rv)) return rv;

    rv = pIProxyObjectManager->GetProxyObject(aEventQ, 
                                              NS_GET_IID(nsIStreamListener), 
                                              aChannel,
                                              PROXY_SYNC | PROXY_ALWAYS,
                                              getter_AddRefs(mSyncListener));
    if (NS_FAILED(rv)) return rv;

    rv = pIProxyObjectManager->GetProxyObject(aEventQ, 
                                              NS_GET_IID(nsIChannel), 
                                              aChannel,
                                              PROXY_SYNC | PROXY_ALWAYS,
                                              getter_AddRefs(mChannel));
    if (NS_FAILED(rv)) return rv;

    mContext = aContext;
    mURL = aUrl;

    mURL->GetSpec(&mURLSpec);

    // pull any username and/or password out of the uri
    char *preHost = nsnull;
    rv = mURL->GetPreHost(&preHost);
    if (NS_FAILED(rv)) return rv;
    
    if (preHost) {
        char *colon = PL_strchr(preHost, ':');
        if (colon) {
            *colon = '\0';
            mPassword = colon+1;
        }
        mUsername = preHost;
        nsAllocator::Free(preHost);
    }

    char *host;
    rv = mURL->GetHost(&host);
    if (NS_FAILED(rv)) return rv;
    PRInt32 port;
    rv = mURL->GetPort(&port);
    if (NS_FAILED(rv)) return rv;

    mCacheKey.SetString(host);
    mCacheKey.Append(port);
    nsAllocator::Free(host);

    // this context is used to get channel specific info back into the FTP channel
    nsFTPContext *dataCtxt = new nsFTPContext();
    if (!dataCtxt) return NS_ERROR_OUT_OF_MEMORY;
    rv = dataCtxt->QueryInterface(NS_GET_IID(nsIFTPContext), (void**)&mFTPContext);
    //mFTPContext = NS_STATIC_CAST(nsIFTPContext*, dataCtxt);
    if (NS_FAILED(rv)) return rv;

    // get a proxied ptr to the FTP protocol handler service so we can control
    // the connection cache from here.
    rv = pIProxyObjectManager->GetProxyObject(nsnull, 
                                              NS_GET_IID(nsIConnectionCache), 
                                              aHandler,
                                              PROXY_SYNC | PROXY_ALWAYS,
                                              getter_AddRefs(mConnCache));
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

nsresult
nsFtpConnectionThread::SetAction(FTP_ACTION aAction) {
    if (mConnected)
        return NS_ERROR_ALREADY_CONNECTED;
    mAction = aAction;
    return NS_OK;
}

nsresult
nsFtpConnectionThread::StopProcessing() {
    PRUnichar* errorMsg = nsnull;
    nsresult rv;

    mCOutStream = 0;
    mCInStream  = 0;

    // if we haven't sent an OnStartRequest() yet, fire one now. We don't want
    // to blidly send an OnStop if we haven't "started" anything.
    if (!mSentStart) {
        rv = mListener->OnStartRequest(mChannel, mContext);
        if (NS_FAILED(rv)) return rv;
    }

    if (NS_FAILED(mInternalError)) {
        // generate a FTP specific error msg.
        rv = MapResultCodeToString(mInternalError, &errorMsg);
        if (NS_FAILED(rv)) return rv;
    }

    rv = mListener->OnStopRequest(mChannel, mContext, mInternalError, errorMsg);
    if (NS_FAILED(rv)) return rv;

    // after notifying the listener (the FTP channel) of the error, stop processing.
    mKeepRunning = PR_FALSE;
    return NS_OK;
}

// Here's where we do all the string whacking/parsing magic to determine
// what type of server it is we're dealing with.
void
nsFtpConnectionThread::SetSystInternals(void) {
    if (mResponseMsg.Equals("UNIX Type: L8 MAC-OS MachTen", 28)) {
        mServerType = FTP_MACHTEN_TYPE;
        mList = PR_TRUE;
    }
    else if (mResponseMsg.Find("UNIX") > -1) {
        mServerType = FTP_UNIX_TYPE;
        mList = PR_TRUE;
    }
    else if (mResponseMsg.Find("windows", PR_TRUE) > -1) {
        // treat any server w/ "windows" in it as a dos based NT server.
        mServerType = FTP_NT_TYPE;
        mList = PR_TRUE;
    }
    else if (mResponseMsg.Equals("VMS", 3)) {
        mServerType = FTP_VMS_TYPE;
        mList = PR_TRUE;
    }
    else if (mResponseMsg.Equals("VMS/CMS", 6) || mResponseMsg.Equals("VM ", 3)) {
        mServerType = FTP_CMS_TYPE;
    }
    else if (mResponseMsg.Equals("DCTS", 4)) {
        mServerType = FTP_DCTS_TYPE;
    }
    else if (mResponseMsg.Find("MAC-OS TCP/Connect II") > -1) {
        mServerType = FTP_TCPC_TYPE;
        mList = PR_TRUE;
    }
    else if (mResponseMsg.Equals("MACOS Peter's Server", 20)) {
        mServerType = FTP_PETER_LEWIS_TYPE;
        mList = PR_TRUE;
    }
    else if (mResponseMsg.Equals("MACOS WebSTAR FTP", 17)) {
        mServerType = FTP_WEBSTAR_TYPE;
        mList = PR_TRUE;
    }

    mConn->mServerType = mServerType;
    mConn->mList = mList;
}

FTP_STATE
nsFtpConnectionThread::FindActionState(void) {

    // These operations require the separate data channel.
    if (mAction == GET || mAction == PUT) {
        // we're doing an operation that requies the data channel.
        // figure out what kind of data channel we want to setup,
        // and do it.
        if (mUsePasv)
            return FTP_S_PASV;
        else
            // until we have PORT support, we'll just fail.
            // return FTP_S_PORT;
            return FTP_ERROR;
    }

    // These operations use the command channel response as the
    // data to return to the user.
    if (mAction == DEL)
        return FTP_S_DEL_FILE; // we assume it's a file

    if (mAction == MKDIR)
        return FTP_S_MKDIR;

    return FTP_ERROR;
}

FTP_STATE
nsFtpConnectionThread::FindGetState(void) {
    char *path = nsnull;
    nsresult rv;
    FTP_STATE result = FTP_ERROR;

    rv = mURL->GetPath(&path);
    if (NS_FAILED(rv)) return FTP_ERROR;

    if (mServerType == FTP_VMS_TYPE) {
        // check for directory
        if (!path[0] || (path[0] == '/' && !path[1]) ) {
            mDirectory = PR_TRUE;
            result = FTP_S_LIST; 
        }
        else if (!PL_strchr(path, '/')) {
            result = FTP_S_RETR;
        }
        else {
            result = FTP_S_CWD;
        }
    } else {
        // XXX I've removed the check for "aleady tried RETR"
        if (path[PL_strlen(path) -1] == '/') {
            result = FTP_S_CWD;
        } else {
            result = FTP_S_RETR;
        }
    }
    nsCRT::free(path);
    return result;
}

nsresult
nsFtpConnectionThread::MapResultCodeToString(nsresult aResultCode, PRUnichar* *aOutMsg) {
    nsCAutoString errorMsg;

    switch (aResultCode) {
    case NS_ERROR_FTP_LOGIN:
        {
            errorMsg = "FTP: Login failed.";
            break;
        }
    case NS_ERROR_FTP_MODE:
        {
            errorMsg = "FTP: MODE command failed.";
            break;
        }
    case NS_ERROR_FTP_CWD:
        {
            errorMsg = "FTP: CWD command failed.";
            break;
        }
    case NS_ERROR_FTP_PASV:
        {
            errorMsg = "FTP: PASV command failed.";
            break;
        }
    case NS_ERROR_FTP_DEL_DIR:
        {
            errorMsg = "FTP: DEL directory command failed.";
            break;
        }
    case NS_ERROR_FTP_MKDIR:
        {
            errorMsg = "FTP: MKDIR command failed";
            break;
        }
    default:
        {
            errorMsg = "Unknown FTP error.";
            break;
        }
    } // END: switch

    *aOutMsg = errorMsg.ToNewUnicode();

    return NS_OK;
}


void
nsFtpConnectionThread::SetDirMIMEType(nsString& aString) {
    // the from content type is a string of the form
    // "text/ftp-dir-SERVER_TYPE" where SERVER_TYPE represents the server we're talking to.
    switch (mServerType) {
    case FTP_UNIX_TYPE:
        aString.Append("unix");
        break;
    case FTP_DCTS_TYPE:
        aString.Append("dcts");
        break;
    case FTP_NCSA_TYPE:
        aString.Append("ncsa");
        break;
    case FTP_PETER_LEWIS_TYPE:
        aString.Append("peter_lewis");
        break;
    case FTP_MACHTEN_TYPE:
        aString.Append("machten");
        break;
    case FTP_CMS_TYPE:
        aString.Append("cms");
        break;
    case FTP_TCPC_TYPE:
        aString.Append("tcpc");
        break;
    case FTP_VMS_TYPE:
        aString.Append("vms");
        break;
    case FTP_NT_TYPE:
        aString.Append("nt");
        break;
    case FTP_WEBSTAR_TYPE:
        aString.Append("webstar");
        break;
    default:
        aString.Append("generic");
    }
}

