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
#include "nsIEventQueueService.h"
#include "nsIStringStream.h"
#include "nsIPipe.h"
#include "nsIMIMEService.h"
#include "nsIStreamConverterService.h"
#include "nsIIOService.h"
#include "prprf.h"
#include "prlog.h"
#include "netCore.h"
#include "ftpCore.h"
#include "nsProxiedService.h"
#include "nsAutoLock.h"
#include "nsCRT.h"
#include "nsIInterfaceRequestor.h"

#include "nsAppShellCIDs.h" // TODO remove later
#include "nsIAppShellService.h" // TODO remove later
#include "nsIWebShellWindow.h" // TODO remove later
#include "nsINetPrompt.h"

#include "nsFtpProtocolHandler.h"
#include "nsPIFTPChannel.h"

static NS_DEFINE_CID(kIOServiceCID,                 NS_IOSERVICE_CID);
static NS_DEFINE_CID(kStreamConverterServiceCID,    NS_STREAMCONVERTERSERVICE_CID);
static NS_DEFINE_CID(kMIMEServiceCID,               NS_MIMESERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID,         NS_EVENTQUEUESERVICE_CID);

static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kFTPHandlerCID, NS_FTPPROTOCOLHANDLER_CID);

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gFTPLog;
#endif /* PR_LOGGING */

NS_IMPL_ISUPPORTS2(nsFtpConnectionThread, nsIRunnable, nsIRequest);

nsFtpConnectionThread::nsFtpConnectionThread() {
    NS_INIT_REFCNT();
    mSTS = nsnull;
    mAction = GET;
    mUsePasv = PR_TRUE;
    mState = FTP_S_USER;
    mNextState = FTP_S_USER;
    mBin = PR_TRUE;
    mLength = -1;
    mConnected = PR_FALSE;
    mResetMode = PR_FALSE;
    mList      = PR_FALSE;
    mKeepRunning = PR_TRUE;
    mContinueRead = PR_FALSE;
    mAnonymous = PR_TRUE;
    mRetryPass = PR_FALSE;
    mCachedConn = PR_FALSE;
    mInternalError = NS_OK; // start out on the up 'n up.
    mSentStart = PR_FALSE;
    mConn = nsnull;
    mSuspendCount = 0;
    mPort = 21; // default FTP port
}

nsFtpConnectionThread::~nsFtpConnectionThread() {
    // lose the socket transport
    NS_IF_RELEASE(mSTS);
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("~nsFtpConnectionThread() called"));
}

nsresult
nsFtpConnectionThread::Process() {
    nsresult rv;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFtpConnectionThread::Process() started for %x (spec =%s)\n",
        mURL.get(), (const char *) mURLSpec));
 
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
                if (!mContinueRead) {
                    PR_sscanf(buffer, "%d", &mResponseCode);
                    mResponseCode = mResponseCode / 100; // truncate it
                }

                // see if we're handling a multi-line response.
                if (mContinueRead || (buffer[3] == '-')) {
                    // yup, multi-line response, start appending
                    char *tmpBuffer = buffer, *crlf = nsnull;
                    PRBool lastLine = PR_FALSE;
                    while ( (crlf = PL_strstr(tmpBuffer, CRLF)) ) {
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

            case FTP_READ_DATA_BUF:
                {
                PRUint32 read = 0, readSoFar = 0;

                if (mCallerOutputStream) {
                    // we have an output stream, write data to it
                    // rather than sending OnDataAvailable() calls.

                    while (readSoFar < NS_FTP_BUFFER_READ_SIZE) {
                        rv = mCallerOutputStream->WriteFrom(mDInStream, 
                                                            NS_FTP_BUFFER_READ_SIZE-readSoFar, 
                                                            &read);
                        if (NS_FAILED(rv)) {
                            mInternalError = NS_ERROR_FAILURE;
                            mState = FTP_ERROR;
                            break;
                        }
                        readSoFar += read;

                        if (!read) {
                            // EOF
                            mCallerOutputStream->Close();

                            mState = FTP_READ_BUF;
                            mNextState = FTP_COMPLETE;
                            break;
                        }
                    }
                } else {
                    NS_ASSERTION(mListener, "FTP: conn thread listener DNE");
                    // no output stream. shuttle data over using the listener
                    while (readSoFar < NS_FTP_BUFFER_READ_SIZE) {
                        PRUint32 avail = 0;
                        rv = mBufInStream->Available(&avail);
                        if (NS_FAILED(rv)) return rv;

                        // suck in the data from the server
                        rv = mBufOutStream->WriteFrom(mDInStream, NS_FTP_BUFFER_READ_SIZE-readSoFar, &read);
                        if (NS_FAILED(rv)) {
                            mInternalError = NS_ERROR_FAILURE;
                            mState = FTP_ERROR;
                            break;
                        }
                        readSoFar += read;
                        if (read == 0) {
                            // we've exhausted the stream, send any data we have left and get out of dodge.
                            rv = mListener->OnDataAvailable(mChannel, mContext, mBufInStream, avail, readSoFar);
                            if (NS_FAILED(rv)) {
                                mInternalError = NS_ERROR_FAILURE;
                                mState = FTP_ERROR;
                                break;
                            }

                            // we're done filling this end of the pipe. close it.
                            mBufOutStream->Close();

                            mState = FTP_READ_BUF;
                            mNextState = FTP_COMPLETE;
                            break; // this terminates the loop
                        }

                        if (readSoFar == NS_FTP_BUFFER_READ_SIZE) {
                            // we've filled our buffer, send the data off
                            rv = mListener->OnDataAvailable(mChannel, mContext, mBufInStream, avail, NS_FTP_BUFFER_READ_SIZE);
                            if (NS_FAILED(rv)) {
                                mInternalError = NS_ERROR_FAILURE;
                                mState = FTP_ERROR;
                                break;
                            }
                            mState = FTP_READ_DATA_BUF;
                        }
                    }
                } // end mCallerOutputStream
                break;
                }
                // END: FTP_READ_DATA_BUF

            case FTP_ERROR:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - ERROR\n", mURL.get()));

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

            case FTP_S_MACB:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_MACB - ", mURL.get()));
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
                    // get the data channel ready
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_LIST - Opening data stream ", mURL.get()));
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
                    // get the data channel ready
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_RETR - Opening data stream ", mURL.get()));
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

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFtpConnectionThread::Process() ended for %x (spec =%s)\n\n\n",
        mURL.get(), (const char *) mURLSpec));

    return NS_OK;
}

nsresult
nsFtpConnectionThread::SetOutputStream(nsIBufferOutputStream* aOutputStream) {
    if (mCallerOutputStream) return NS_ERROR_FAILURE;
    mCallerOutputStream = aOutputStream;
    return NS_OK;
}

nsresult
nsFtpConnectionThread::SetStreamObserver(nsIStreamObserver* aObserver, nsISupports *aContext) {
    nsresult rv;
    static PRBool streamObserverSet = PR_FALSE;
    if (streamObserverSet) return NS_ERROR_FAILURE;
    NS_WITH_SERVICE(nsIProxyObjectManager, pIProxyObjectManager, kProxyObjectManagerCID, &rv);
    if(NS_FAILED(rv)) return rv;

    rv = pIProxyObjectManager->GetProxyObject(nsnull, /* use the main thread's eventQ */
                                              NS_GET_IID(nsIStreamObserver), 
                                              aObserver,
                                              PROXY_ASYNC | PROXY_ALWAYS,
                                              getter_AddRefs(mObserver));
    mObserverContext = aContext;
    streamObserverSet = PR_TRUE;
    return rv;
}

nsresult
nsFtpConnectionThread::SetStreamListener(nsIStreamListener* aListener) {
    nsresult rv;
    // stream listeners can be reset at any time.
    mListener = 0;
    mSyncListener = 0;

    NS_WITH_SERVICE(nsIProxyObjectManager, pIProxyObjectManager, kProxyObjectManagerCID, &rv);
    if(NS_FAILED(rv)) return rv;

    rv = pIProxyObjectManager->GetProxyObject(nsnull, 
                                              NS_GET_IID(nsIStreamListener), 
                                              aListener,
                                              PROXY_ASYNC | PROXY_ALWAYS,
                                              getter_AddRefs(mListener));
    if (NS_FAILED(rv)) return rv;

    rv = pIProxyObjectManager->GetProxyObject(nsnull, 
                                              NS_GET_IID(nsIStreamListener), 
                                              aListener,
                                              PROXY_SYNC | PROXY_ALWAYS,
                                              getter_AddRefs(mSyncListener));
    if (NS_FAILED(rv)) return rv;

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

          NS_WITH_SERVICE(nsIAppShellService, appshellservice, kAppShellServiceCID, &rv);
          if (NS_FAILED(rv)) return rv;

          nsCOMPtr<nsIWebShellWindow> webshellwindow;
          appshellservice->GetHiddenWindow(getter_AddRefs( webshellwindow ) );
          nsCOMPtr<nsINetPrompt> prompter( do_QueryInterface( webshellwindow ) );

          NS_WITH_SERVICE(nsIProxyObjectManager, pIProxyObjectManager, kProxyObjectManagerCID, &rv);
          if(NS_FAILED(rv)) {
            return rv;
          }
          nsINetPrompt* proxyprompter = NULL;
          rv = pIProxyObjectManager->GetProxyObject
            (NS_UI_THREAD_EVENTQ, nsINetPrompt::GetIID(), prompter, PROXY_SYNC, (void**)&proxyprompter);
            PRUnichar *user = nsnull, *passwd = nsnull;
            PRBool retval;
            static nsAutoString message;
            nsXPIDLCString host;
            rv = mURL->GetHost(getter_Copies(host));
            if (NS_FAILED(rv)) return rv;
            if (message.Length() < 1) {
                message = "Enter username and password for "; //TODO localize it!
                message += host;
            }

            rv = proxyprompter->PromptUsernameAndPassword(host, NULL, message.GetUnicode(), &user, &passwd, &retval);
            proxyprompter->Release();  // Must be done as not managed for you.
            // if the user canceled or didn't supply a username we want to fail
            if (!retval || (user && !*user) )
                return NS_ERROR_FAILURE;
            mUsername = user;
            mPassword = passwd;
        }
        usernameStr.Append(mUsername);    
    }
    usernameStr.Append(CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), usernameStr.GetBuffer()));

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
          NS_WITH_SERVICE(nsIAppShellService, appshellservice, kAppShellServiceCID, &rv);
          if (NS_FAILED(rv)) return rv;

          nsCOMPtr<nsIWebShellWindow> webshellwindow;
          appshellservice->GetHiddenWindow(getter_AddRefs( webshellwindow ) );
          nsCOMPtr<nsINetPrompt> prompter( do_QueryInterface( webshellwindow ) );

          NS_WITH_SERVICE(nsIProxyObjectManager, pIProxyObjectManager, kProxyObjectManagerCID, &rv);
          if(NS_FAILED(rv)) {
            return rv;
          }
          nsINetPrompt* proxyprompter = NULL;
          rv = pIProxyObjectManager->GetProxyObject
            (NS_UI_THREAD_EVENTQ, nsINetPrompt::GetIID(), prompter, PROXY_SYNC, (void**)&proxyprompter);
            PRUnichar *passwd = nsnull;
            PRBool retval;
            static nsAutoString message;
            nsXPIDLCString host2;
            rv = mURL->GetHost(getter_Copies(host2));
            if (NS_FAILED(rv)) {
              return rv;
            }
            nsAutoString userAtHost;
            userAtHost = mUsername;
            userAtHost += "@";
            userAtHost += host2;
            char * userAtHostC = userAtHost.ToNewCString();
            if (message.Length() < 1) {
                nsXPIDLCString host;
                rv = mURL->GetHost(getter_Copies(host));
                if (NS_FAILED(rv)) return rv;
                message = "Enter password for "; //TODO localize it!
		        message += mUsername;
                message += " on ";
                message += host;
            }
            rv = proxyprompter->PromptPassword(userAtHostC, NULL, message.GetUnicode(), &passwd, &retval);
            nsCRT::free(userAtHostC);
            proxyprompter->Release();  // Must be done as not managed for you.

            // we want to fail if the user canceled or didn't enter a password.
            if (!retval || (passwd && !*passwd) )
                return NS_ERROR_FAILURE;
            mPassword = passwd;
        }
        passwordStr.Append(mPassword);    
    }
    passwordStr.Append(CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), passwordStr.GetBuffer()));

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
    char *buffer = "SYST" CRLF;
    PRUint32 bytes;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), buffer));

    return mCOutStream->Write(buffer, PL_strlen(buffer), &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_syst() {
    FTP_STATE state = FTP_S_PWD;
    if (mResponseCode == 2) {

        SetSystInternals(); // must be called first to setup member vars.
 
        // setup next state based on server type.
        if (mServerType == FTP_PETER_LEWIS_TYPE || mServerType == FTP_WEBSTAR_TYPE)
            state = FTP_S_MACB;
        else
            state = FindActionState();
    }

    return state;
}

nsresult
nsFtpConnectionThread::S_acct() {
    char *buffer = "ACCT noaccount" CRLF;
    PRUint32 bytes;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), buffer));

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
    char *buffer = "MACB ENABLE" CRLF;
    PRUint32 bytes;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), buffer));

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
    char *buffer = "PWD" CRLF;
    PRUint32 bytes;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), buffer));

    return mCOutStream->Write(buffer, PL_strlen(buffer), &bytes);
}


FTP_STATE
nsFtpConnectionThread::R_pwd() {
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
        buffer = "TYPE I" CRLF;
    } else {
        buffer = "TYPE A" CRLF;
    }

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), buffer));

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
    nsXPIDLCString path;
    PRUint32 bytes;
    rv = mURL->GetPath(getter_Copies(path));
    if (NS_FAILED(rv)) return rv;

    nsCAutoString cwdStr("CWD ");

    if (mServerType == FTP_VMS_TYPE) {
        char *slash = nsnull;
        
        if ( (slash = PL_strrchr(path, '/')) ) {
            *slash = '\0';
            cwdStr.Append("[");
            cwdStr.Append(path);
            cwdStr.Append("]" CRLF);
            *slash = '/';

            // turn '/'s into '.'s
            //while ( (slash = PL_strchr(buffer, '/')) )
            //    *slash = '.';
        } else {
            cwdStr.Append("[");
            cwdStr.Append(".");
            cwdStr.Append(path);
            cwdStr.Append("]" CRLF);
        }
    } else {
        // non VMS server
        cwdStr.Append(path);
        cwdStr.Append(CRLF);
    }

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), cwdStr.GetBuffer()));

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
    nsXPIDLCString path;
    PRUint32 bytes;
    rv = mURL->GetPath(getter_Copies(path));
    if (NS_FAILED(rv)) return rv;

    // XXX should the actual file name be parsed out???
    mFilename = path; 

    nsCAutoString sizeBuf("SIZE ");
    sizeBuf.Append(path);
    sizeBuf.Append(CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), sizeBuf.GetBuffer()));

    rv = mCOutStream->Write(sizeBuf.GetBuffer(), sizeBuf.Length(), &bytes);
    return rv;
}

FTP_STATE
nsFtpConnectionThread::R_size() {
    FTP_STATE retState = FTP_S_MDTM;
    nsresult rv;
    if (mResponseCode == 2) {
        PRInt32 conversionError;
        mLength = mResponseMsg.ToInteger(&conversionError);

        rv = mFTPChannel->SetContentLength(mLength);
        if (NS_FAILED(rv)) return FTP_ERROR;

    } if (mResponseCode == 5) {
        // couldn't get the size of what we asked for, must be a dir.
        mResetMode = PR_TRUE;

        rv = mFTPChannel->SetContentType("application/http-index-format");
        if (NS_FAILED(rv)) return FTP_ERROR;
        retState = FTP_S_MODE;
    }

    if (mObserver) {
        // this was an AsyncOpen call. Notify the AsyncOpen observer
        // then wait for further instructions.
        rv = mObserver->OnStartRequest(mChannel, mObserverContext);
        if (NS_FAILED(rv)) return FTP_ERROR;
        nsAutoCMonitor mon(this);
        mon.Wait();
    }

    return retState;
}

nsresult
nsFtpConnectionThread::S_mdtm() {
    nsresult rv;
    nsXPIDLCString path;
    PRUint32 bytes;
    rv = mURL->GetPath(getter_Copies(path));
    if (NS_FAILED(rv)) return rv;

    /*if (mServerType == FTP_VMS_TYPE) {
        mState = FindGetState();
        break;
    }*/

    nsCAutoString mdtmStr("MDTM ");
    mdtmStr.Append(path);
    mdtmStr.Append(CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), mdtmStr.GetBuffer()));

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
        buffer = "LIST" CRLF;
    else
        buffer = "NLST" CRLF;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), buffer));

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

    if (mCallerOutputStream) {
        nsCOMPtr<nsIInputStream> convertedStream;
        rv = StreamConvService->Convert(mDInStream,
                                        fromStr.GetUnicode(),
                                        toStr.GetUnicode(),
                                        mURL,
                                        getter_AddRefs(convertedStream));
        if (NS_FAILED(rv)) return FTP_ERROR;

        mDInStream->Close();
        mDInStream = 0;

        mDInStream = convertedStream;
        return FTP_READ_DATA_BUF;
    } else {
        rv = StreamConvService->AsyncConvertData(fromStr.GetUnicode(),
                                                 toStr.GetUnicode(),
                                                 mSyncListener,
                                                 mURL, getter_AddRefs(converterListener));
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
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x R_list() data pipe read failed w/ rv = %d\n", mURL.get(), rv));
                nsAllocator::Free(listBuf);
                return FTP_ERROR;
            } else if (read < 1) {
                // EOF
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x R_list() data pipe read hit EOF\n", mURL.get()));
                nsAllocator::Free(listBuf);
                return FTP_READ_BUF;
            }
            listBuf[read] = '\0';

            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x R_list() data pipe read %d bytes:\n%s\n", mURL.get(), read, listBuf));

            nsCOMPtr<nsISupports> stringStreamSup;
            rv = NS_NewCharInputStream(getter_AddRefs(stringStreamSup), listBuf);
            if (NS_FAILED(rv)) return FTP_ERROR;

            nsCOMPtr<nsIInputStream> listStream = do_QueryInterface(stringStreamSup, &rv);

            rv = converterListener->OnDataAvailable(mChannel, mContext, listStream, 0, read);
            if (NS_FAILED(rv)) return FTP_ERROR;
        }
    }


    // NOTE: that we're not firing an OnStopRequest() to the converterListener (the
    //  stream converter). It (at least this implementation of it) doesn't care 
    //  about stops. FTP_COMPLETE will send the stop.

    return FTP_READ_BUF;
}

nsresult
nsFtpConnectionThread::S_retr() {
    nsresult rv;
    nsXPIDLCString path;
    PRUint32 bytes;
    rv = mURL->GetPath(getter_Copies(path));
    if (NS_FAILED(rv)) return rv;

    nsCAutoString retrStr("RETR ");

    if (mServerType == FTP_VMS_TYPE) {
        NS_ASSERTION(mFilename.Length() > 0, "ftp filename not present");
        retrStr.Append(mFilename);
    } else {
        retrStr.Append(path);
    }
    retrStr.Append(CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), retrStr.GetBuffer()));

    rv = mCOutStream->Write(retrStr.GetBuffer(), retrStr.Length(), &bytes);
    return rv;
}

#define NS_FTP_SEGMENT_SIZE        (2*1024)
#define NS_FTP_BUFFER_SIZE         (8*1024)

FTP_STATE
nsFtpConnectionThread::R_retr() {
    nsresult rv;
    // The only way we can get here is if we knew we were dealing with
    // a file and not a dir listing. This state assumes we're retrieving
    // a file!
    if (mResponseCode == 1) {
        // success.

        // we may not have a listener if we were OpenInputStream()ed.
        if (mListener) {
            rv = mListener->OnStartRequest(mChannel, mContext);
            if (NS_FAILED(rv)) return FTP_ERROR;
            mSentStart = PR_TRUE;
        }

        rv = NS_NewPipe(getter_AddRefs(mBufInStream), getter_AddRefs(mBufOutStream),
                        nsnull, NS_FTP_SEGMENT_SIZE, NS_FTP_BUFFER_SIZE);
        if (NS_FAILED(rv)) return FTP_ERROR;

        return FTP_READ_DATA_BUF;

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
    char *buffer = "PASV" CRLF;
    PRUint32 bytes;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), buffer));

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
    rv = mSTS->CreateTransport(host.GetBuffer(), port, nsnull, 
                               mBufferSegmentSize, mBufferMaxSize,
                               getter_AddRefs(mDPipe)); // the data channel
    if (NS_FAILED(rv)) return FTP_ERROR;

    // hook ourself up as a proxy for progress notifications
    nsCOMPtr<nsIInterfaceRequestor> progressProxy(do_QueryInterface(mChannel));
    rv = mDPipe->SetNotificationCallbacks(progressProxy);
    if (NS_FAILED(rv)) return FTP_ERROR;


    if (mAction == GET) {
        // Setup the data channel for file reception
    } else {
        // get the output stream so we can write to the server
        rv = mDPipe->OpenOutputStream(0, getter_AddRefs(mDOutStream));
        if (NS_FAILED(rv)) return FTP_ERROR;
    }

    // we're connected figure out what type of transfer we're doing (ascii or binary)
    nsXPIDLCString type;
    rv = mFTPChannel->GetContentType(getter_Copies(type));
    nsCAutoString typeStr;
    if (NS_FAILED(rv) || !type) 
        typeStr = "bin";
    else
        typeStr = type;

    mContentType = typeStr;

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
    nsXPIDLCString filename;
    PRUint32 bytes;
    rv = mURL->GetPath(getter_Copies(filename)); // XXX we should probably check to 
                                   // XXX make sure we have an actual filename.
    if (NS_FAILED(rv)) return rv;

    nsCAutoString delStr("DELE ");
    delStr.Append(filename);
    delStr.Append(CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), delStr.GetBuffer()));

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
    nsXPIDLCString dir;
    PRUint32 bytes;
    rv = mURL->GetPath(getter_Copies(dir));
    if (NS_FAILED(rv)) return rv;

    nsCAutoString delDirStr("RMD ");
    delDirStr.Append(dir);
    delDirStr.Append(CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), delDirStr.GetBuffer()));

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
    nsXPIDLCString dir;
    PRUint32 bytes;
    rv = mURL->GetPath(getter_Copies(dir));
    if (NS_FAILED(rv)) return rv;

    nsCAutoString mkdirStr("MKD ");
    mkdirStr.Append(dir);
    mkdirStr.Append(CRLF);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mURL.get(), mkdirStr.GetBuffer()));

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

    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->CreateThreadEventQueue();
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(mFTPEventQueue));
    if (NS_FAILED(rv)) return rv;

    // we've got to send the event queue for this sucker over to the 
    // channel's thread so he can post event back to us.
    NS_WITH_SERVICE(nsIProxyObjectManager, pIProxyObjectManager, kProxyObjectManagerCID, &rv);
    if(NS_FAILED(rv)) return rv;

    nsCOMPtr<nsPIFTPChannel> ftpChannel;
    rv = pIProxyObjectManager->GetProxyObject(nsnull, 
                                              NS_GET_IID(nsPIFTPChannel), 
                                              mChannel,
                                              PROXY_SYNC | PROXY_ALWAYS,
                                              getter_AddRefs(ftpChannel));
    if (NS_FAILED(rv)) return rv;

    rv = ftpChannel->SetConnectionQueue(mFTPEventQueue);
    if (NS_FAILED(rv)) return rv;
    

    rv = nsServiceManager::GetService(kSocketTransportServiceCID,
                                      NS_GET_IID(nsISocketTransportService), 
                                      (nsISupports **)&mSTS);
    if(NS_FAILED(rv)) return rv;

    /////////////////////////
    // COMMAND CHANNEL SETUP
    /////////////////////////

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
    } else {
        nsXPIDLCString host;
        rv = mURL->GetHost(getter_Copies(host));
        if (NS_FAILED(rv)) return rv;

        // build our own
        rv = mSTS->CreateTransport(host, mPort, host, 
                                   mBufferSegmentSize, mBufferMaxSize,
                                   getter_AddRefs(mCPipe)); // the command channel
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
            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x nsFTPConnTrd::Run() greeting read failed with rv = %d\n", mURL.get(), rv));
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
                    while ( (crlf = PL_strstr(tmpBuffer, CRLF)) ) {
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
        mState = FTP_S_PASV;
    }

    mConnected = PR_TRUE;

    rv = Process();
    mListener = 0;
    mSyncListener = 0;
    mChannel = 0;
    mConnCache = 0;

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
    mInternalError = NS_ERROR_FAILURE; // XXX should be an FTP code.
    mState = FTP_COMPLETE;
    return rv;
}

NS_IMETHODIMP
nsFtpConnectionThread::Suspend(void)
{
    nsresult rv = NS_OK;
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
        nsAutoCMonitor mon(this);
        mon.Wait();
        // If we're notifiec, let everythign resume.
        rv = Resume();
    }

    return rv;
}

NS_IMETHODIMP
nsFtpConnectionThread::Resume(void)
{
    nsresult rv = NS_ERROR_FAILURE;
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
                            nsIChannel* aChannel,
                            nsISupports* aContext,
                            PRUint32 bufferSegmentSize,
                            PRUint32 bufferMaxSize) {
    nsresult rv;

    mBufferSegmentSize = bufferSegmentSize;
    mBufferMaxSize = bufferMaxSize;

    // parameter validation
    NS_ASSERTION(aChannel, "FTP: thread needs a channel");

    // setup internal member variables
    mChannel = aChannel; // a straight com ptr to the channel
    mContext = aContext; // this is the original caller's context

    rv = aChannel->GetURI(getter_AddRefs(mURL));
    if (NS_FAILED(rv)) return rv;

    mURL->GetSpec(getter_Copies(mURLSpec));

    // pull any username and/or password out of the uri
    nsXPIDLCString preHost;
    rv = mURL->GetPreHost(getter_Copies(preHost));
    if (NS_FAILED(rv)) return rv;
    
    if (preHost) {
        // we found some username (and maybe password info) in the URL. 
        // we know we're not going anonymously now.
        mAnonymous = PR_FALSE;
        char *colon = PL_strchr(preHost, ':');
        if (colon) {
            *colon = '\0';
            mPassword = colon+1;
        }
        mUsername = preHost;
    }

    // setup the connection cache key
    nsXPIDLCString host;
    rv = mURL->GetHost(getter_Copies(host));
    if (NS_FAILED(rv)) return rv;

    PRInt32 port;
    rv = mURL->GetPort(&port);
    if (NS_FAILED(rv)) return rv;

    if (port > 0)
        mPort = port;

    mCacheKey.SetString(host);
    mCacheKey.Append(port);

    NS_WITH_SERVICE(nsIProxyObjectManager, pIProxyObjectManager, kProxyObjectManagerCID, &rv);
    if(NS_FAILED(rv)) return rv;

    // This proxied channel is used to set channel related
    // state on the *real* channel back in the main thread.
    rv = pIProxyObjectManager->GetProxyObject(nsnull, 
                                              NS_GET_IID(nsPIFTPChannel), 
                                              aChannel,
                                              PROXY_SYNC | PROXY_ALWAYS,
                                              getter_AddRefs(mFTPChannel));
    if (NS_FAILED(rv)) return rv;

    // get a proxied ptr to the FTP protocol handler service so we can control
    // the connection cache from here.
    rv = pIProxyObjectManager->GetProxyObject(nsnull, 
                                              NS_GET_IID(nsIConnectionCache), 
                                              aHandler,
                                              PROXY_SYNC | PROXY_ALWAYS,
                                              getter_AddRefs(mConnCache));
    return rv;
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
    nsresult rv;
    PRUnichar* errorMsg = nsnull;

    // Release the transports
    mCPipe = 0;
    mDPipe = 0;

    // kill the event loop
    mKeepRunning = PR_FALSE;

    // setup any internal error message to propegate
    if (NS_FAILED(mInternalError)) {
        // generate a FTP specific error msg.
        rv = MapResultCodeToString(mInternalError, &errorMsg);
        if (NS_FAILED(rv)) return rv;
    }


    rv = mFTPChannel->Stopped(mInternalError, errorMsg);
    if (NS_FAILED(rv)) return rv;

    // if we have a listener, end the transaction.
    if (mListener) {
        // if we haven't sent an OnStartRequest() yet, fire one now. We don't want
        // to blidly send an OnStop if we haven't "started" anything.
        if (!mSentStart) {
            rv = mListener->OnStartRequest(mChannel, mContext);
            if (NS_FAILED(rv)) return rv;
        }

        rv = mListener->OnStopRequest(mChannel, mContext, mInternalError, errorMsg);
        if (NS_FAILED(rv)) return rv;
    }

    // if we have an observer, end hte transaction
    if (mObserver) {
        rv = mObserver->OnStopRequest(mChannel, mObserverContext, mInternalError, errorMsg);
    }

    return rv;
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
    nsXPIDLCString path;
    nsresult rv;
    FTP_STATE result = FTP_ERROR;

    rv = mURL->GetPath(getter_Copies(path));
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

