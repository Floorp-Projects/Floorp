/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsFtpConnectionThread.h"
#include "nsISocketTransport.h"
#include "nsIStreamConverterService.h"
#include "prprf.h"
#include "prlog.h"
#include "prnetdb.h"
#include "netCore.h"
#include "ftpCore.h"
#include "nsProxiedService.h"
#include "nsCRT.h"
#include "nsIInterfaceRequestor.h"
#include "nsAsyncEvent.h"
#include "nsIURL.h"
#include "nsEscape.h"
#include "nsNetUtil.h"
#include "nsIDNSService.h" // for host error code
#include "nsIWalletService.h"
#include "nsIProxy.h"
#include "nsIMemory.h"

static NS_DEFINE_CID(kWalletServiceCID, NS_WALLETSERVICE_CID);
static NS_DEFINE_CID(kStreamConverterServiceCID,    NS_STREAMCONVERTERSERVICE_CID);
static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

#define FTP_COMMAND_CHANNEL_SEG_SIZE 64
#define FTP_COMMAND_CHANNEL_MAX_SIZE 512

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gFTPLog;
#endif /* PR_LOGGING */

NS_IMPL_THREADSAFE_ADDREF(nsFtpConnectionThread);
NS_IMPL_THREADSAFE_RELEASE(nsFtpConnectionThread); 
NS_IMPL_QUERY_INTERFACE2(nsFtpConnectionThread, nsIRunnable, nsIRequest);

nsFtpConnectionThread::nsFtpConnectionThread() {
    NS_INIT_REFCNT();
    // bool init
    mConnected = mList = mRetryPass = mCachedConn = mSentStart = PR_FALSE;
    mFireCallbacks = mBin = mKeepRunning = mAnonymous = PR_TRUE;

    mAction = GET;
    mState = FTP_COMMAND_CONNECT;
    mNextState = FTP_S_USER;

    mInternalError = NS_OK;
    mConn = nsnull;
    mSuspendCount = 0;
    mPort = 21;

    mLock = nsnull;
    mLastModified = LL_ZERO;
    mWriteCount = 0;
    mBufferSegmentSize = 0;
    mBufferMaxSize = 0;

    mIPv6Checked = PR_FALSE;
    mIPv6ServerAddress = nsnull;
}

nsFtpConnectionThread::~nsFtpConnectionThread() {
    nsXPIDLCString spec;
    mURL->GetSpec(getter_Copies(spec));
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("~nsFtpConnectionThread() for %s", (const char*)spec));
    if (mLock) PR_DestroyLock(mLock);
    if (mIPv6ServerAddress) nsMemory::Free(mIPv6ServerAddress);
}

nsresult
nsFtpConnectionThread::Process() {
    nsresult    rv = NS_OK;
    PRBool      continueRead = PR_FALSE, brokenLine = PR_FALSE;
    nsCAutoString carryOverBuf;

#ifdef DEBUG
    nsXPIDLCString spec;
    (void)mURL->GetSpec(getter_Copies(spec));

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFtpConnectionThread::Process() started for %x (spec =%s)\n",
        mURL.get(), NS_STATIC_CAST(const char*, spec)));
#endif // DEBUG
 
    while (mKeepRunning) {
        switch(mState) {

//////////////////////////////
//// INTERNAL STATES
//////////////////////////////
            case FTP_COMMAND_CONNECT:
                {
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

                    mPassword   = mConn->mPassword;
                } else {
                    mCachedConn = PR_FALSE;
                    nsXPIDLCString host;
                    rv = mURL->GetHost(getter_Copies(host));
                    if (NS_FAILED(rv)) return rv;

                    // build our own
                    rv = CreateTransport(host, mPort, 
                                         FTP_COMMAND_CHANNEL_SEG_SIZE,
                                         FTP_COMMAND_CHANNEL_MAX_SIZE,
                                         getter_AddRefs(mCPipe)); // the command channel
                    if (NS_FAILED(rv)) return rv;

                    // get the output stream so we can write to the server
                    rv = mCPipe->OpenOutputStream(getter_AddRefs(mCOutStream));
                    if (NS_FAILED(rv)) {
                        mInternalError = rv;
                        mState = FTP_ERROR;
                        break;
                    }

                    rv = mCPipe->OpenInputStream(getter_AddRefs(mCInStream));
                    if (NS_FAILED(rv)) {
                        mInternalError = rv;
                        mState = FTP_ERROR;
                        break;
                    }
                    // cache this stuff.
                    mConn = new nsConnectionCacheObj(mCPipe, mCInStream, mCOutStream);
                    if (!mConn) return NS_ERROR_OUT_OF_MEMORY;
                }

                if (!mCachedConn) {
                    mState = FTP_READ_BUF;
                } else {
                    // we're already connected to this server, skip login.
                    mState = FTP_S_PASV;
                }

                // PR_ASSERT(mIPv6Checked == PR_FALSE);

                mConnected = PR_TRUE;                

                break;
                } // END: FTP_COMMAND_CONNECT

            case FTP_READ_BUF:
                {
                char buffer[NS_FTP_BUFFER_READ_SIZE];
                PRUint32 read;
                rv = mCInStream->Read(buffer, NS_FTP_BUFFER_READ_SIZE, &read);
                if (NS_FAILED(rv)) {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - READ_BUF - Read() FAILED with rv = %d\n", mURL.get(), rv));
                    mInternalError = NS_ERROR_FAILURE;
                    mState = FTP_ERROR;
                    break;
                }
                

                if (0 == read) {
                    // EOF
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - READ_BUF received EOF\n", mURL.get()));
                    mState = FTP_COMPLETE;
                    mInternalError = NS_ERROR_FAILURE;
                    break;
                }
                buffer[read] = '\0';
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - READ_BUF - read \"%s\" (%d bytes)", mURL.get(), buffer, read));

                // get the response code out.
                if (!continueRead && !brokenLine) {
                    PR_sscanf(buffer, "%d", &mResponseCode);
                    if (buffer[3] == '-') {
                        continueRead = PR_TRUE;
                    } else {
                        brokenLine = !PL_strstr(buffer, CRLF);
                    }
                }

                // see if we're handling a multi-line response.
                if (continueRead) {
                    // yup, multi-line response, start appending
                    char *tmpBuffer = nsnull, *crlf = nsnull;
                    
                    // if we have data from our last round be sure
                    // to prepend it.
                    char *cleanup = nsnull;
                    if (!carryOverBuf.IsEmpty() ) {
                        carryOverBuf += buffer;
                        cleanup = tmpBuffer = carryOverBuf.ToNewCString();
                        carryOverBuf.Truncate(0);
                        if (!tmpBuffer) return NS_ERROR_OUT_OF_MEMORY;
                    } else {
                        tmpBuffer = buffer;
                    }

                    PRBool lastLine = PR_FALSE;
                    while ( (crlf = PL_strstr(tmpBuffer, CRLF)) ) {
                        char tmpChar = crlf[2];
                        crlf[2] = '\0';
                        // see if this is the last line
                        if (tmpBuffer[3] != '-' &&
                            nsCRT::IsAsciiDigit(tmpBuffer[0]) &&
                            nsCRT::IsAsciiDigit(tmpBuffer[1]) &&
                            nsCRT::IsAsciiDigit(tmpBuffer[2]) ) {
                            lastLine = PR_TRUE;
                        }
                        mResponseMsg += tmpBuffer+4; // skip over the code and '-'
                        crlf[2] = tmpChar;
                        tmpBuffer = crlf+2;
                    }
                    if (*tmpBuffer)
                        carryOverBuf = tmpBuffer;

                    if (cleanup) nsMemory::Free(cleanup);

                    // see if this was the last line
                    if (lastLine) {
                        // yup. last line, let's move on.
                        if (mState == mNextState) {
                            NS_ASSERTION(0, "ftp read state mixup");
                            mInternalError = NS_ERROR_FAILURE;
                            mState = FTP_ERROR;
                        } else {
                            mState = mNextState;
                        }
                        continueRead = PR_FALSE;
                    } else {
                        // don't increment state, we need to read more.
                        continueRead = PR_TRUE;
                    }
                    break;
                }


                NS_ASSERTION(mState != mNextState, "ftp read state mixup");

                // get the rest of the line
                if (brokenLine) {
                    mResponseMsg += buffer;
                    brokenLine = !PL_strstr(buffer, CRLF);
                    if (!brokenLine)
                        mState = mNextState;
                } else {
                    mResponseMsg = buffer+4;
                    mState = mNextState;
                }

                break;
                }
                // END: FTP_READ_BUF

            case FTP_ERROR:
                {
                if (mConn) delete mConn;

                if (mResponseCode == 421 && 
                    mInternalError != NS_ERROR_FTP_LOGIN) {
                    // The command channel dropped for some reason.
                    // Fire it back up, unless we were trying to login
                    // in which case the server might just be telling us
                    // that the max number of users has been reached...
                    mState = FTP_COMMAND_CONNECT;
                    mNextState = FTP_S_USER;
		    if (mIPv6Checked) {
			mIPv6Checked = PR_FALSE;
		    }
                } else {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - ERROR\n", mURL.get()));
                    rv = StopProcessing();
                }
                break;
                }
                // END: FTP_ERROR

            case FTP_COMPLETE:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - COMPLETE\n", mURL.get()));

                if (NS_SUCCEEDED(mInternalError)) {
                    // we only want to cache the connection if things are ok.
                    // push through all of the pertinent state into the cache entry;
                    mConn->mCwd = mCwd;
                    rv = mConnCache->InsertConn(mCacheKey.GetBuffer(), mConn);
                    if (NS_FAILED(rv)) return rv;
                }

                rv = StopProcessing();
                break;
                }
                // END: FTP_COMPLETE

//////////////////////////////
//// CONNECTION SETUP STATES
//////////////////////////////

            case FTP_S_USER:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_USER - ", mURL.get()));
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
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_PASS - ", mURL.get()));
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
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_SYST - ", mURL.get()));
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
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_ACCT - ", mURL.get()));
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

            case FTP_S_PWD:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_PWD - ", mURL.get()));
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
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_MODE - ", mURL.get()));
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
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_CWD - ", mURL.get()));
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
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_SIZE - ", mURL.get()));
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
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_MDTM - ", mURL.get()));
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
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_LIST - ", mURL.get()));
                rv = S_list();
                if (NS_FAILED(rv)) {
                    mInternalError = rv;
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
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
                if (FTP_ERROR == mState) {
                    mInternalError = rv;
                    mNextState = FTP_ERROR;
                } else {
                    char *lf = PL_strchr(mResponseMsg.GetBuffer(), LF);
                    if (lf && lf+1 && *(lf+1)) 
                        // we have a double resposne
                        mState = FTP_COMPLETE;

                    mNextState = FTP_COMPLETE;
                }
                break;
                }
                // END: FTP_R_LIST

            case FTP_S_RETR:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_RETR - ", mURL.get()));
                rv = S_retr();
                if (NS_FAILED(rv)) {
                    mInternalError = rv;
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
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
                break;
                }
                // END: FTP_R_RETR


            case FTP_S_STOR:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_STOR - ", mURL.get()));
                rv = S_stor();
                if (NS_FAILED(rv)) {
                    mInternalError = rv;
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
                    mState = FTP_READ_BUF;
                    mNextState = FTP_R_STOR;
                }
                break;
                }
                // END: FTP_S_RETR

            case FTP_R_STOR:
                {
                mState = R_stor();
                if (FTP_ERROR == mState)
                    mInternalError = NS_ERROR_FAILURE;
                mNextState = FTP_COMPLETE;
                break;
                }
                // END: FTP_R_RETR

//////////////////////////////
//// DATA CONNECTION STATES
//////////////////////////////
            case FTP_S_PASV:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_PASV - ", mURL.get()));
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
                if (FTP_ERROR == mState) {
                    mInternalError = NS_ERROR_FTP_PASV;
                    break;
                }
                mNextState = FTP_READ_BUF;
                break;
                }
                // END: FTP_R_PASV

//////////////////////////////
//// ACTION STATES
//////////////////////////////
            case FTP_S_DEL_FILE:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_DEL_FILE - ", mURL.get()));
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
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_DEL_DIR - ", mURL.get()));
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
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_MKDIR - ", mURL.get()));
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

#ifdef DEBUG
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFtpConnectionThread::Process() ended for %x (spec =%s)\n\n\n",
        mURL.get(), NS_STATIC_CAST(const char*, spec)));
#endif // DEBUG

    return NS_OK;
}

nsresult
nsFtpConnectionThread::SetStreamObserver(nsIStreamObserver* aObserver, nsISupports *aContext) {
    nsresult rv = NS_OK;
    if (mConnected) return NS_ERROR_ALREADY_CONNECTED;
    NS_ASSERTION(aObserver, "null observer");
    mObserver = aObserver;
    mObserverContext = aContext;
    return rv;
}

nsresult
nsFtpConnectionThread::SetStreamListener(nsIStreamListener* aListener, nsISupports *aContext) {
    mListener = aListener;
    mListenerContext = aContext;
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
            if (!mPrompter) return NS_ERROR_NOT_INITIALIZED;

            NS_WITH_SERVICE(nsIProxyObjectManager, pIProxyObjectManager, 
                            kProxyObjectManagerCID, &rv);
            if (NS_FAILED(rv)) return rv;


            nsCOMPtr<nsIPrompt> proxyprompter;
            rv = pIProxyObjectManager->GetProxyForObject(NS_UI_THREAD_EVENTQ, 
                        NS_GET_IID(nsIPrompt), mPrompter, 
                        PROXY_SYNC, getter_AddRefs(proxyprompter));
            PRUnichar *user = nsnull, *passwd = nsnull;
            PRBool retval;
            nsAutoString message;
            nsXPIDLCString host;
            rv = mURL->GetHost(getter_Copies(host));
            if (NS_FAILED(rv)) return rv;
            message.AssignWithConversion("Enter username and password for "); //TODO localize it!
            message.AppendWithConversion(host);

            nsAutoString realm; // XXX i18n
            CopyASCIItoUCS2(nsLiteralCString(NS_STATIC_CAST(const char*, host)), realm);
            rv = proxyprompter->PromptUsernameAndPassword(nsnull,
                                                          message.GetUnicode(),
                                                          realm.GetUnicode(), nsIPrompt::SAVE_PASSWORD_PERMANENTLY,
                                                          &user, &passwd, &retval);

            // if the user canceled or didn't supply a username we want to fail
            if (!retval || (user && !*user) )
                return NS_ERROR_FAILURE;
            mUsername = user;
            mPassword = passwd;
        }
        usernameStr.AppendWithConversion(mUsername);    
    }
    usernameStr.Append(CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), usernameStr.GetBuffer()));

    return mCOutStream->Write(usernameStr.GetBuffer(), usernameStr.Length(), &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_user() {
    if (mResponseCode/100 == 3) {
        // send off the password
        return FTP_S_PASS;
    } else if (mResponseCode/100 == 2) {
        // no password required, we're already logged in
        return FTP_S_SYST;
    } else if (mResponseCode/100 == 5) {
        // problem logging in. typically this means the server
        // has reached it's user limit.
        return FTP_ERROR;
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

    mResponseMsg = "";

    if (mAnonymous) {
        passwordStr.Append("mozilla@");
    } else {
        if (!mPassword.Length() || mRetryPass) {
            if (!mPrompter) return NS_ERROR_NOT_INITIALIZED;

            NS_WITH_SERVICE(nsIProxyObjectManager, pIProxyObjectManager, 
                            kProxyObjectManagerCID, &rv);
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIPrompt> proxyprompter;
            rv = pIProxyObjectManager->GetProxyForObject(NS_UI_THREAD_EVENTQ, 
                        NS_GET_IID(nsIPrompt), mPrompter,
                        PROXY_SYNC, getter_AddRefs(proxyprompter));
            PRUnichar *passwd = nsnull;
            PRBool retval;
            nsAutoString message;
            nsAutoString title;
            title.AssignWithConversion("Password");
            
            nsXPIDLCString host;
            rv = mURL->GetHost(getter_Copies(host));
            if (NS_FAILED(rv)) return rv;
            message.AssignWithConversion("Enter password for "); //TODO localize it!
            message += mUsername;
            message.AppendWithConversion(" on ");
            message.AppendWithConversion(host);

            nsXPIDLCString prePath;
            rv = mURL->GetPrePath(getter_Copies(prePath));
            if (NS_FAILED(rv)) return rv;
            rv = proxyprompter->PromptPassword(title.GetUnicode(),
                                               message.GetUnicode(),
                                               NS_ConvertASCIItoUCS2(prePath).GetUnicode(), 
                                               nsIPrompt::SAVE_PASSWORD_PERMANENTLY,
                                               &passwd, &retval);

            // we want to fail if the user canceled or didn't enter a password.
            if (!retval || (passwd && !*passwd) )
                return NS_ERROR_FAILURE;
            mPassword = passwd;
        }
        passwordStr.AppendWithConversion(mPassword);    
    }
    passwordStr.Append(CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), passwordStr.GetBuffer()));

    return mCOutStream->Write(passwordStr.GetBuffer(), passwordStr.Length(), &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_pass() {
    if (mResponseCode/100 == 3) {
        // send account info
        return FTP_S_ACCT;
    } else if (mResponseCode/100 == 2) {
        // logged in
        mConn->mPassword = mPassword;
        return FTP_S_SYST;
    } else if (mResponseCode == 503) {
        // start over w/ the user command.
        // note: the password was successful, and it's stored in mPassword
        mRetryPass = PR_FALSE;
        return FTP_S_USER;
    } else if (mResponseCode == 530) {
        // user limit reached
        if (!mPrompter) return FTP_ERROR;
        nsresult rv;
        NS_WITH_SERVICE(nsIProxyObjectManager, pIProxyObjectManager, 
                        kProxyObjectManagerCID, &rv);
        if (NS_FAILED(rv)) return FTP_ERROR;

        nsCOMPtr<nsIPrompt> proxyprompter;
        rv = pIProxyObjectManager->GetProxyForObject(NS_UI_THREAD_EVENTQ, 
                    NS_GET_IID(nsIPrompt), mPrompter,
                    PROXY_SYNC, getter_AddRefs(proxyprompter));
        if (NS_FAILED(rv)) return FTP_ERROR;

        nsAutoString    title;
        title.AssignWithConversion("Error");        // localization
        nsAutoString    text;
        text.AssignWithConversion(mResponseMsg);

        rv = proxyprompter->Alert(title.GetUnicode(), text.GetUnicode());
        return FTP_ERROR;
    } else {
        // kick back out to S_pass() and ask the user again.
        nsresult rv = NS_OK;

        NS_WITH_SERVICE(nsIWalletService, walletService, kWalletServiceCID, &rv);
        if (NS_FAILED(rv)) return FTP_ERROR;

        NS_WITH_SERVICE(nsIProxyObjectManager, pIProxyObjectManager, 
                        kProxyObjectManagerCID, &rv);
        if (NS_FAILED(rv)) return FTP_ERROR;

        nsCOMPtr<nsIWalletService> pWalletService;
        rv = pIProxyObjectManager->GetProxyForObject(NS_UI_THREAD_EVENTQ, 
                        NS_GET_IID(nsIWalletService), walletService, 
                        PROXY_SYNC, getter_AddRefs(pWalletService));
        if (NS_FAILED(rv)) return FTP_ERROR;

        nsXPIDLCString uri;
        rv = mURL->GetSpec(getter_Copies(uri));
        if (NS_FAILED(rv)) return FTP_ERROR;

        rv = pWalletService->SI_RemoveUser(uri, nsnull);
        if (NS_FAILED(rv)) return FTP_ERROR;

        mRetryPass = PR_TRUE;
        return FTP_S_PASS;
    }
}

#define SYST_COMM ("SYST" CRLF)
nsresult
nsFtpConnectionThread::S_syst() {
    PRUint32 bytes;
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), SYST_COMM));
    return mCOutStream->Write(SYST_COMM, PL_strlen(SYST_COMM), &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_syst() {
    if (mResponseCode/100 == 2) {
        if (mResponseMsg.Find("UNIX") > -1) {
            mServerType = FTP_UNIX_TYPE;
            mList = PR_TRUE;
        }
        else if (mResponseMsg.Find("windows", PR_TRUE) > -1) {
            // treat any server w/ "windows" in it as a dos based NT server.
            mServerType = FTP_NT_TYPE;
            mList = PR_TRUE;
        }

        mConn->mServerType = mServerType;
        mConn->mList = mList;
    }
    return FTP_S_PWD;
}

#define ACCT_COMM ("ACCT noaccount" CRLF)
nsresult
nsFtpConnectionThread::S_acct() {
    PRUint32 bytes;
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), ACCT_COMM));
    return mCOutStream->Write(ACCT_COMM, PL_strlen(ACCT_COMM), &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_acct() {
    if (mResponseCode/100 == 2)
        return FTP_S_SYST;
    else
        return FTP_ERROR;
}

#define PWD_COMM ("PWD" CRLF)
nsresult
nsFtpConnectionThread::S_pwd() {
    PRUint32 bytes;
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), PWD_COMM));
    return mCOutStream->Write(PWD_COMM, PL_strlen(PWD_COMM), &bytes);
}


FTP_STATE
nsFtpConnectionThread::R_pwd() {
    FTP_STATE state = FTP_ERROR;

    // check for a quoted path
    PRInt32 beginQ, endQ;
    beginQ = mResponseMsg.Find("\"");
    if (beginQ > -1) {
        endQ = mResponseMsg.RFind("\"");
        if (endQ > beginQ) {
            // quoted path
            nsCAutoString tmpPath;
            mResponseMsg.Mid(tmpPath, beginQ+1, (endQ-beginQ-1));

            if (tmpPath.Length() <= 1)
            {
                tmpPath = "";
            }

            nsCAutoString   modPath(mPath);
            if ((modPath.Length() > 0) && (!modPath.Equals("/")))
            {
                tmpPath = modPath;
            }

            mResponseMsg = tmpPath;
            mURL->SetPath(tmpPath);
            mPath = tmpPath;            
        }
    } 

    // default next state
    state = FindActionState();

    if(mServerType == FTP_GENERIC_TYPE) {
        if (mResponseMsg.CharAt(1) == '/') {
            mServerType = FTP_UNIX_TYPE; // path names ending with '/' imply unix
            mList = PR_TRUE;
        } 
    }

    return state;
}

nsresult
nsFtpConnectionThread::S_mode() {
    char *buffer;
    PRUint32 bytes;
    if (mBin) {
        buffer = "TYPE I" CRLF;
    } else {
        buffer = "TYPE A" CRLF;
    }
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), buffer));
    return mCOutStream->Write(buffer, PL_strlen(buffer), &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_mode() {
    if (mResponseCode/100 != 2) {
        return FTP_ERROR;
    }

    if (mAction == PUT) {
        return FTP_S_CWD;
    } else {
        return FTP_S_SIZE;
    }
}

nsresult
nsFtpConnectionThread::S_cwd() {
    PRUint32 bytes;

    nsCAutoString cwdStr("CWD ");
    cwdStr.Append(mPath);
    cwdStr.Append(CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), cwdStr.GetBuffer()));

    cwdStr.Mid(mCwdAttempt, 4, cwdStr.Length() - 6);

    return mCOutStream->Write(cwdStr.GetBuffer(), cwdStr.Length(), &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_cwd() {
    FTP_STATE state = FTP_ERROR;
    if (mResponseCode/100 == 2) {
        mCwd = mCwdAttempt;

        // update
        mURL->SetPath(mCwd);

        nsresult rv = mChannel->SetContentType("application/http-index-format");
        if (NS_FAILED(rv)) return FTP_ERROR;

        // success
        if (mAction == PUT) {
            // we are uploading
            state = FTP_S_STOR;
        } else {
            // we are GETting a file or dir
            state = FTP_S_LIST;
        }
    } else {
        state = FTP_S_RETR;
    }
    return state;
}

nsresult
nsFtpConnectionThread::S_size() {
    PRUint32 bytes;

    nsCAutoString sizeBuf("SIZE ");
    sizeBuf.Append(mPath);
    sizeBuf.Append(CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), sizeBuf.GetBuffer()));

    return mCOutStream->Write(sizeBuf.GetBuffer(), sizeBuf.Length(), &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_size() {
    if (mResponseCode/100 == 2) {
        PRInt32 conversionError;
        PRInt32 length = mResponseMsg.ToInteger(&conversionError);
		if (NS_FAILED(mChannel->SetContentLength(length))) return FTP_ERROR;
    }

    return FTP_S_MDTM;
}

nsresult
nsFtpConnectionThread::S_mdtm() {
    PRUint32 bytes;

    nsCAutoString mdtmStr("MDTM ");
    mdtmStr.Append(mPath);
    mdtmStr.Append(CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), mdtmStr.GetBuffer()));
    return mCOutStream->Write(mdtmStr.GetBuffer(), mdtmStr.Length(), &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_mdtm() {
    if (mResponseCode/100 == 2) {
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
        nsMemory::Free(timeStr);
        mLastModified = PR_ImplodeTime(&ts);
    }

    return FTP_S_CWD;
}

nsresult
nsFtpConnectionThread::S_list() {
    nsresult rv;
    char *buffer;
    PRUint32 bytes;
    if (mList)
        buffer = "LIST" CRLF;
    else
        buffer = "NLST" CRLF;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), buffer));

    rv = mCOutStream->Write(buffer, PL_strlen(buffer), &bytes);
    if (NS_FAILED(rv)) return rv;


    // setup a listener to push the data into. This listener sits inbetween the
    // unconverted data of fromType, and the final listener in the chain (in this case
    // the mListener).
    nsCOMPtr<nsIStreamListener> converterListener;

    NS_WITH_SERVICE(nsIStreamConverterService, StreamConvService, kStreamConverterServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsAutoString fromStr; fromStr.AssignWithConversion("text/ftp-dir-");
    SetDirMIMEType(fromStr);
    nsAutoString toStr; toStr.AssignWithConversion("application/http-index-format");

    rv = StreamConvService->AsyncConvertData(fromStr.GetUnicode(), toStr.GetUnicode(),
                                             mListener, mURL, getter_AddRefs(converterListener));
    if (NS_FAILED(rv)) return rv;
    
    nsFTPAsyncReadEvent *event = new nsFTPAsyncReadEvent(converterListener,
                                                         mDPipe,
                                                         mListenerContext);
    if (!event) return NS_ERROR_OUT_OF_MEMORY;
    mFireCallbacks = PR_FALSE; // listener callbacks will be handled by the transport.
    return event->Fire(mEventQueue);
}

FTP_STATE
nsFtpConnectionThread::R_list() {
    if ((mResponseCode/100 == 4) || (mResponseCode/100 == 5)) {
        mFireCallbacks = PR_TRUE;
        return FTP_ERROR;
    }
    return FTP_READ_BUF;
}

nsresult
nsFtpConnectionThread::S_retr() {
    nsresult rv = NS_OK;
    PRUint32 bytes;

    nsCAutoString retrStr("RETR ");
    retrStr.Append(mPath);
    retrStr.Append(CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), retrStr.GetBuffer()));

    rv = mCOutStream->Write(retrStr.GetBuffer(), retrStr.Length(), &bytes);

    nsFTPAsyncReadEvent *event = new nsFTPAsyncReadEvent(mListener,
                                                         mDPipe,
                                                         mListenerContext);
    if (!event) return NS_ERROR_OUT_OF_MEMORY;
    mFireCallbacks = PR_FALSE; // listener callbacks will be handled by the transport.
    return event->Fire(mEventQueue);
}

FTP_STATE
nsFtpConnectionThread::R_retr() {
    if (mResponseCode/100 == 1) {
        // see if there's another response in this read.
        // this can happen if the server sends back two
        // responses before we've processed the first one.
        PRInt32 loc = -1;
        loc = mResponseMsg.FindChar(LF);
        if (loc > -1) {
            PRInt32 err;
            nsCAutoString response;
            mResponseMsg.Mid(response, loc, mResponseMsg.Length() - (loc+1));
            if (response.Length()) {
                PRInt32 code = response.ToInteger(&err);
                if (code/100 != 2)
                    return FTP_ERROR;
                else
                    return FTP_COMPLETE;
            }
        }
        return FTP_READ_BUF;
    }
    mFireCallbacks = PR_TRUE;
    return FTP_ERROR;
}


nsresult
nsFtpConnectionThread::S_stor() {
    nsresult rv = NS_OK;
    PRUint32 bytes;

    nsCAutoString retrStr("STOR ");
    retrStr.Append(mPath);
    retrStr.Append(CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), retrStr.GetBuffer()));

    rv = mCOutStream->Write(retrStr.GetBuffer(), retrStr.Length(), &bytes);

    NS_ASSERTION(mWriteStream, "we're trying to upload without any data");
    nsFTPAsyncWriteEvent *event = new nsFTPAsyncWriteEvent(mWriteStream,
                                                           mWriteCount,
                                                           mObserver,
                                                           mDPipe,
                                                           mObserverContext);
    if (!event) return NS_ERROR_OUT_OF_MEMORY;
    mFireCallbacks = PR_FALSE; // observer callbacks will be handled by the transport.
    return event->Fire(mEventQueue);
}

FTP_STATE
nsFtpConnectionThread::R_stor() {
    if (mResponseCode/100 == 1) {
        return FTP_READ_BUF;
    }
    return FTP_ERROR;
}


#define PASV_COMM ("PASV" CRLF)
#define EPSV_COMM ("EPSV" CRLF)
nsresult
nsFtpConnectionThread::S_pasv() {
    if (mIPv6Checked == PR_FALSE) {
        // Find IPv6 socket address, if server is IPv6
        nsresult rv;
        
        mIPv6Checked = PR_TRUE;
        PR_ASSERT(mIPv6ServerAddress == 0);
        nsCOMPtr<nsISocketTransport> sTrans = do_QueryInterface(mCPipe, &rv);
        
        if (!NS_FAILED(rv)) {
            rv = sTrans->GetIPStr(100, &mIPv6ServerAddress);
        }

        if (!NS_FAILED(rv)) {
            PRNetAddr addr;
            if (PR_StringToNetAddr(mIPv6ServerAddress, &addr) != PR_SUCCESS ||
                PR_IsNetAddrType(&addr, PR_IpAddrV4Mapped)) {
                nsMemory::Free(mIPv6ServerAddress);
                mIPv6ServerAddress = 0;
            }
            PR_ASSERT(!mIPv6ServerAddress || addr.raw.family == PR_AF_INET6);
        }
    }

    PRUint32 bytes;
    const char *comm = mIPv6ServerAddress ? EPSV_COMM : PASV_COMM;
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\" (addr is %s)\n", mURL.get(), comm, mIPv6ServerAddress ? mIPv6ServerAddress : "v4"));
    return mCOutStream->Write(comm, PL_strlen(comm), &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_pasv() {
    nsresult rv;
    PRInt32 port;

    if (mResponseCode/100 != 2)  {
        return FTP_ERROR;
    }

    char *response = mResponseMsg.ToNewCString();
    if (!response) return FTP_ERROR;
    char *ptr = response;

    nsCAutoString host;
    if (mIPv6ServerAddress) {
        // The returned string is of the form
        // text (|||ppp|)
        // Where '|' can be any single character
        char delim;
        while (*ptr && *ptr != '(') ptr++;
        if (*ptr++ != '(') {
            return FTP_ERROR;
        }
        delim = *ptr++;
        if (!delim || *ptr++ != delim || *ptr++ != delim || 
            *ptr < '0' || *ptr > '9') {
            return FTP_ERROR;
        }
        port = 0;
        do {
            port = port * 10 + *ptr++ - '0';
        } while (*ptr >= '0' && *ptr <= '9');
        if (*ptr++ != delim || *ptr != ')') {
            return FTP_ERROR;
        }
    } else {
        // The returned address string can be of the form
        // (xxx,xxx,xxx,xxx,ppp,ppp) or
        //  xxx,xxx,xxx,xxx,ppp,ppp (without parens)
        PRInt32 h0, h1, h2, h3, p0, p1;
        while (*ptr) {
            if (*ptr == '(') {
                // move just beyond the open paren
                ptr++;
                break;
            }
            if (*ptr == ',') {
                // backup to the start of the digits
                do {
                    ptr--;
                } while ((ptr >=response) && (*ptr >= '0') && (*ptr <= '9'));
                ptr++; // get back onto the numbers
                break;
            }
            ptr++;
        } // end while

        PRInt32 fields = PR_sscanf(ptr, 
            "%ld,%ld,%ld,%ld,%ld,%ld",
            &h0, &h1, &h2, &h3, &p0, &p1);

        if (fields < 6) {
            return FTP_ERROR;
        }

        port = ((PRInt32) (p0<<8)) + p1;
        host.AppendInt(h0);
        host.Append('.');
        host.AppendInt(h1);
        host.Append('.');
        host.AppendInt(h2);
        host.Append('.');
        host.AppendInt(h3);
    }

    nsMemory::Free(response);

    // now we know where to connect our data channel
    rv = CreateTransport(mIPv6ServerAddress ? mIPv6ServerAddress : host.GetBuffer(),
                         port, mBufferSegmentSize, mBufferMaxSize,
                         getter_AddRefs(mDPipe)); // the data channel
    if (NS_FAILED(rv)) return FTP_ERROR;
    
    nsCOMPtr<nsISocketTransport> sTrans = do_QueryInterface(mDPipe, &rv);
    if (NS_FAILED(rv)) return FTP_ERROR;

    if (NS_FAILED(sTrans->SetReuseConnection(PR_FALSE))) return FTP_ERROR;

    // hook ourself up as a proxy for progress notifications
    nsCOMPtr<nsIInterfaceRequestor> progressProxy(do_QueryInterface(mChannel));
    rv = mDPipe->SetNotificationCallbacks(progressProxy);
    if (NS_FAILED(rv)) return FTP_ERROR;

    // we're connected figure out what type of transfer we're doing (ascii or binary)
    nsXPIDLCString type;
    rv = mChannel->GetContentType(getter_Copies(type));
    nsCAutoString typeStr;
    if (NS_FAILED(rv) || !type) 
        typeStr = "bin";
    else
        typeStr.Assign(type);

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
    PRUint32 bytes;
    nsCAutoString delStr("DELE ");
    delStr.Append(mPath);
    delStr.Append(CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), delStr.GetBuffer()));

    return mCOutStream->Write(delStr.GetBuffer(), delStr.Length(), &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_del_file() {
    if (mResponseCode/100 != 2)
        return FTP_S_DEL_DIR;
    return FTP_COMPLETE;
}

nsresult
nsFtpConnectionThread::S_del_dir() {
    PRUint32 bytes;
    nsCAutoString delDirStr("RMD ");
    delDirStr.Append(mPath);
    delDirStr.Append(CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), delDirStr.GetBuffer()));

    return mCOutStream->Write(delDirStr.GetBuffer(), delDirStr.Length(), &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_del_dir() {
    if (mResponseCode/100 != 2)
        return FTP_ERROR;
    return FTP_COMPLETE;
}

nsresult
nsFtpConnectionThread::S_mkdir() {
    PRUint32 bytes;
    nsCAutoString mkdirStr("MKD ");
    mkdirStr.Append(mPath);
    mkdirStr.Append(CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), mkdirStr.GetBuffer()));

    return mCOutStream->Write(mkdirStr.GetBuffer(), mkdirStr.Length(), &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_mkdir() {
    if (mResponseCode/100 != 2)
        return FTP_ERROR;
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
                                      getter_AddRefs(mSTS));
    if(NS_FAILED(rv)) return rv;

    rv = Process(); // turn the crank.
    if (NS_FAILED(rv)) return rv;

    // before releaseing the refs to these cached members,
    // marshall over a ref so we're sure the final release
    // of these members doesn't occur on our thread.
    NS_ASSERTION(mListener, "no listener implies no-one is handling data");
    nsFTPReleaseEvent *event = 
        new nsFTPReleaseEvent(NS_STATIC_CAST(nsISupports*, mListener));
    if (!event) return NS_ERROR_OUT_OF_MEMORY;
    rv = event->Fire(mEventQueue);
    if (NS_FAILED(rv)) return rv;
    mListener = 0; // ditch our ref.

    if (mListenerContext) {
        event = new nsFTPReleaseEvent(NS_STATIC_CAST(nsISupports*, mListenerContext));
        if (!event) return NS_ERROR_OUT_OF_MEMORY;
        rv = event->Fire(mEventQueue);
        if (NS_FAILED(rv)) return rv;
        mListenerContext = 0;
    }

    if (mObserver) {
        event = new nsFTPReleaseEvent(NS_STATIC_CAST(nsISupports*, mObserver));
        if (!event) return NS_ERROR_OUT_OF_MEMORY;
        rv = event->Fire(mEventQueue);
        if (NS_FAILED(rv)) return rv;
        mObserver = 0;
    }

    if (mObserverContext) {
        event = new nsFTPReleaseEvent(NS_STATIC_CAST(nsISupports*, mObserverContext));
        if (!event) return NS_ERROR_OUT_OF_MEMORY;
        rv = event->Fire(mEventQueue);
        if (NS_FAILED(rv)) return rv;
        mObserverContext = 0;
    }

    event = new nsFTPReleaseEvent(NS_STATIC_CAST(nsISupports*, mChannel));
    if (!event) return NS_ERROR_OUT_OF_MEMORY;
    rv = event->Fire(mEventQueue);
    if (NS_FAILED(rv)) return rv;
    mChannel = 0; // ditch our ref

    event = new nsFTPReleaseEvent(NS_STATIC_CAST(nsISupports*, mConnCache));
    if (!event) return NS_ERROR_OUT_OF_MEMORY;
    rv = event->Fire(mEventQueue);
    if (NS_FAILED(rv)) return rv;
    mConnCache = 0;

    if (mPrompter) {
        event = new nsFTPReleaseEvent(NS_STATIC_CAST(nsISupports*, mPrompter));
        if (!event) return NS_ERROR_OUT_OF_MEMORY;
        rv = event->Fire(mEventQueue);
        if (NS_FAILED(rv)) return rv;
        mPrompter = 0;
    }

    if (mWriteStream) {
        event = new nsFTPReleaseEvent(NS_STATIC_CAST(nsISupports*, mWriteStream));
        if (!event) return NS_ERROR_OUT_OF_MEMORY;
        rv = event->Fire(mEventQueue);
        if (NS_FAILED(rv)) return rv;
        mWriteStream = 0;
    }

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsFtpConnectionThread::GetName(PRUnichar* *result)
{
    nsresult rv;
    nsXPIDLCString urlStr;
    rv = mURL->GetSpec(getter_Copies(urlStr));
    if (NS_FAILED(rv)) return rv;
    nsString name;
    name.AppendWithConversion(urlStr);
    *result = name.ToNewUnicode();
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsFtpConnectionThread::IsPending(PRBool *result)
{
    nsresult rv = NS_OK;
    *result = PR_FALSE;
    nsAutoLock aLock(mLock);

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
nsFtpConnectionThread::GetStatus(nsresult *status)
{
    *status = mInternalError;
    return NS_OK;
}

NS_IMETHODIMP
nsFtpConnectionThread::Cancel(nsresult status)
{
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
    nsresult rv = NS_OK;
    nsAutoLock aLock(mLock);

    if (mCPipe) {
        rv = mCPipe->Cancel(status);
        if (NS_FAILED(rv)) return rv;
    }
    if (mDPipe) {
        rv = mDPipe->Cancel(status);
        if (NS_FAILED(rv)) return rv;
    }
    mInternalError = status;
    mState = FTP_COMPLETE;
    return rv;
}

NS_IMETHODIMP
nsFtpConnectionThread::Suspend(void)
{
    nsresult rv = NS_OK;
    nsAutoLock aLock(mLock);

    // suspending the underlying socket transport will
    // cause the FTP state machine to "suspend" when it
    // tries to use the transport. May not be granular 
    // enough.
    if (mSuspendCount < 1) {
        mSuspendCount++;
        if (mCPipe) {
            rv = mCPipe->Suspend();
            if (NS_FAILED(rv)) return rv;
        }
        if (mDPipe) {
            rv = mDPipe->Suspend();
            if (NS_FAILED(rv)) return rv;
        }
    }

    return rv;
}

NS_IMETHODIMP
nsFtpConnectionThread::Resume(void)
{
    nsresult rv = NS_ERROR_FAILURE;
    nsAutoLock aLock(mLock);

    // resuming the underlying socket transports will
    // cause the FTP state machine to unblock and 
    // go on about it's business.
    if (mSuspendCount) {
        // only a suspended thread can be resumed
        if (mCPipe) {
            rv = mCPipe->Resume();
            if (NS_FAILED(rv)) return rv;
        }
        if (mDPipe) {
            rv = mDPipe->Resume();
            if (NS_FAILED(rv)) return rv;
        }
        rv = NS_OK;
    }
    mSuspendCount--;
    return rv;
}

nsresult
nsFtpConnectionThread::Init(nsIProtocolHandler* aHandler,
                            nsIFTPChannel* aChannel,
                            nsIPrompt*  aPrompter,
                            PRUint32 bufferSegmentSize,
                            PRUint32 bufferMaxSize) {
    nsresult rv = NS_OK;

    if (mConnected) return NS_ERROR_ALREADY_CONNECTED;

    mPrompter = aPrompter;

    mBufferSegmentSize = bufferSegmentSize;
    mBufferMaxSize = bufferMaxSize;

    mLock = PR_NewLock();
    if (!mLock) return NS_ERROR_OUT_OF_MEMORY;

    // parameter validation
    NS_ASSERTION(aChannel, "FTP: thread needs a channel");

    mChannel = aChannel; // a straight com ptr to the channel

    rv = aChannel->GetURI(getter_AddRefs(mURL));
    if (NS_FAILED(rv)) return rv;

    char *path = nsnull;
    rv = mURL->GetPath(&path);
    if (NS_FAILED(rv)) return rv;
    mPath = nsUnescape(path);
    nsMemory::Free(path);

    // pull any username and/or password out of the uri
    nsXPIDLCString uname;
    rv = mURL->GetUsername(getter_Copies(uname));
    if (NS_FAILED(rv)) {
        return rv;
    } else {
        if ((const char*)uname && *(const char*)uname) {
            mAnonymous = PR_FALSE;
            mUsername.AssignWithConversion(uname);
        }
    }

    nsXPIDLCString password;
    rv = mURL->GetPassword(getter_Copies(password));
    if (NS_FAILED(rv))
        return rv;
    else
        mPassword.AssignWithConversion(password);
    
    // setup the connection cache key
    nsXPIDLCString host;
    rv = mURL->GetHost(getter_Copies(host));
    if (NS_FAILED(rv)) return rv;

    PRInt32 port;
    rv = mURL->GetPort(&port);
    if (NS_FAILED(rv)) return rv;

    if (port > 0)
        mPort = port;

    mCacheKey.Assign(host);
    mCacheKey.AppendInt(port);

    NS_WITH_SERVICE(nsIEventQueueService, eqs, kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = eqs->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(mEventQueue));
    if (NS_FAILED(rv)) return rv;

    mConnCache = do_QueryInterface(aHandler, &rv);
    return rv;
}

nsresult
nsFtpConnectionThread::SetWriteStream(nsIInputStream* aInStream, PRUint32 aWriteCount) {
    if (mConnected) return NS_ERROR_ALREADY_CONNECTED;
    mAction = PUT;
    mWriteStream = aInStream;
    mWriteCount  = aWriteCount; // The is the amount of data to write.
    return NS_OK;
}

nsresult
nsFtpConnectionThread::StopProcessing() {
    nsresult rv;

    // kill the event loop
    mKeepRunning = PR_FALSE;

#if 0
    if (NS_FAILED(mInternalError)) {
        if (mCPipe) (void)mCPipe->Cancel();
        if (mDPipe) (void)mDPipe->Cancel();
    }
#endif

    // Release the transports
    mCPipe = 0;
    mDPipe = 0;
    mIPv6Checked = PR_FALSE;
    if (mIPv6ServerAddress) {
        nsMemory::Free(mIPv6ServerAddress);
        mIPv6ServerAddress = 0;
    }

    if (mFireCallbacks) {
        // we never got to the point that the transport would be
        // taking over notifications. we'll handle them our selves.

        nsCOMPtr<nsIIOService> serv = do_GetService(kIOServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        if (mObserver) {
            nsCOMPtr<nsIStreamObserver> asyncObserver;
            rv = NS_NewAsyncStreamObserver(getter_AddRefs(asyncObserver), mObserver, mEventQueue);
            if(NS_FAILED(rv)) return rv;

            // we only want to fire OnStop. No OnStart has been fired, and
            // we only want to propagate an error.
            rv = asyncObserver->OnStopRequest(mChannel, mObserverContext, mInternalError, nsnull);
            if (NS_FAILED(rv)) return rv;
        }

        if (mListener) {
            nsCOMPtr<nsIStreamListener> asyncListener;
            rv = NS_NewAsyncStreamListener(getter_AddRefs(asyncListener), mListener, mEventQueue);
            if(NS_FAILED(rv)) return rv;

            // we only want to fire OnStop. No OnStart has been fired, and
            // we only want to propagate an error.
            rv = asyncListener->OnStopRequest(mChannel, mListenerContext, mInternalError, nsnull);
            if (NS_FAILED(rv)) return rv;
        }
    }

    return rv;
}

FTP_STATE
nsFtpConnectionThread::FindActionState(void) {

    // These operations require the separate data channel.
    if (mAction == GET || mAction == PUT) {
        // we're doing an operation that requies the data channel.
        // figure out what kind of data channel we want to setup,
        // and do it.
        return FTP_S_PASV;
    }

    // These operations use the command channel response as the
    // data to return to the user.
    if (mAction == DEL)
        return FTP_S_DEL_FILE; // we assume it's a file

    if (mAction == MKDIR)
        return FTP_S_MKDIR;

    return FTP_ERROR;
}

void
nsFtpConnectionThread::SetDirMIMEType(nsString& aString) {
    // the from content type is a string of the form
    // "text/ftp-dir-SERVER_TYPE" where SERVER_TYPE represents the server we're talking to.
    switch (mServerType) {
    case FTP_UNIX_TYPE:
        aString.AppendWithConversion("unix");
        break;
    case FTP_NT_TYPE:
        aString.AppendWithConversion("nt");
        break;
    default:
        aString.AppendWithConversion("generic");
    }
}

nsresult
nsFtpConnectionThread::CreateTransport(const char * host, PRInt32 port,
                                       PRUint32 bufferSegmentSize,
                                       PRUint32 bufferMaxSize,
                                       nsIChannel** o_pTrans)
{
    PRBool usingProxy;
    if (NS_SUCCEEDED(mChannel->GetUsingTransparentProxy(&usingProxy)) && usingProxy) {
        
        nsresult rv;
        
        nsCOMPtr<nsIProxy> channelProxy = do_QueryInterface(mChannel, &rv);
        
        if (NS_SUCCEEDED(rv)) {
            
            nsXPIDLCString proxyHost;
            nsXPIDLCString proxyType;
            PRInt32 proxyPort;
            
            rv = channelProxy->GetProxyHost(getter_Copies(proxyHost));
            if (NS_FAILED(rv)) return rv;
            
            rv = channelProxy->GetProxyPort(&proxyPort);
            if (NS_FAILED(rv)) return rv;
            
            rv = channelProxy->GetProxyType(getter_Copies(proxyType));
            if (NS_SUCCEEDED(rv) && nsCRT::strcasecmp(proxyType, "socks") == 0) {
                
                return mSTS->CreateTransportOfType("socks", host, port, proxyHost, proxyPort, bufferSegmentSize,
                                                   bufferMaxSize, o_pTrans);
                
            }
                
            return mSTS->CreateTransport(host, port, proxyHost, proxyPort, bufferSegmentSize,
                                         bufferMaxSize, o_pTrans);
                
        }
    }
    
    return mSTS->CreateTransport(host, port, nsnull, -1, bufferSegmentSize,
                                 bufferMaxSize, o_pTrans);
}
