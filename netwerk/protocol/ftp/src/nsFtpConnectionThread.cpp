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

#include "nsFtpConnectionThread.h"
#include "nsFtpStreamListenerEvent.h" // the various events we fire off to the
                                      // owning thread.
#include "nsIChannel.h"
#include "nsISocketTransportService.h"
#include "nsIURI.h"
#include "nsIBufferInputStream.h" // for our internal stream state
#include "nsIInputStream.h"
#include "nsIEventQueueService.h"
#include "nsIFTPContext.h" // for the context passed between OnDataAvail calls
#include "nsIStringStream.h"
#include "nsIPipe.h"
#include "nsIBufferOutputStream.h"
#include "nsIMIMEService.h"
#include "nsXPIDLString.h" 

static NS_DEFINE_CID(kMIMEServiceCID, NS_MIMESERVICE_CID);
static NS_DEFINE_CID(kEventQueueServiceCID,      NS_EVENTQUEUESERVICE_CID);

#define FTP_CRLF "\r\n" 

#if defined(PR_LOGGING)
extern PRLogModuleInfo* gFTPLog;
#endif /* PR_LOGGING */


#include "prprf.h"
#include "netCore.h"
#include "ftpCore.h"
static NS_DEFINE_IID(kIFTPContextIID, NS_IFTPCONTEXT_IID);
class nsFTPContext : nsIFTPContext {
public:
    // nsISupports methods
    NS_DECL_ISUPPORTS

    // nsIFTPContext methods
    NS_IMETHOD IsCmdResponse(PRBool *_retval) {
        *_retval = mCmdResponse;
        return NS_OK;
    };

    NS_IMETHOD SetContentType(char *aContentType) {
        mContentType = aContentType;
        return NS_OK;
    };

    NS_IMETHOD GetContentType(char * *aContentType) {
        *aContentType = mContentType.ToNewCString();
        return NS_OK;
    };

    // nsFTPContext methods
    nsFTPContext() {
        NS_INIT_REFCNT();
        mCmdResponse = PR_TRUE;
    };

    virtual ~nsFTPContext() {};

    PRBool mCmdResponse;
    nsString2 mContentType;
};
NS_IMPL_ISUPPORTS(nsFTPContext, kIFTPContextIID);

static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);


NS_IMPL_ADDREF(nsFtpConnectionThread);
NS_IMPL_RELEASE(nsFtpConnectionThread);

NS_IMETHODIMP
nsFtpConnectionThread::QueryInterface(const nsIID& aIID, void** aInstancePtr) {
    NS_ASSERTION(aInstancePtr, "no instance pointer");
    if (aIID.Equals(nsCOMTypeInfo<nsIRunnable>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()) ) {
        *aInstancePtr = NS_STATIC_CAST(nsFtpConnectionThread*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    /*if (aIID.Equals(nsIStreamListener::GetIID()) ||
        aIID.Equals(nsIStreamObserver::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsIStreamListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }*/
    return NS_NOINTERFACE; 
}

nsFtpConnectionThread::nsFtpConnectionThread(nsIEventQueue* aEventQ, nsIStreamListener* aListener,
                                             nsIChannel* channel, nsISupports* context) {
    NS_INIT_REFCNT();
    
    mEventQueue = aEventQ; // whoever creates us must provide an event queue
                           // so we can post events back to them.
    NS_IF_ADDREF(mEventQueue);
    mListener = aListener;
    NS_IF_ADDREF(mListener);
    mAction = GET;
    mUsePasv = PR_TRUE;
    mState = FTP_S_USER;
    mNextState = FTP_S_USER;
    mBin = PR_TRUE;
    mLength = 0;
    mConnected = PR_FALSE;
    mChannel = channel;
    NS_ADDREF(channel);
    mContext = context;
    NS_IF_ADDREF(context);
    mDPipe = nsnull;

    mCBufInStream = nsnull;

    mKeepRunning = PR_TRUE;
    mUseDefaultPath = PR_TRUE;
}

nsFtpConnectionThread::~nsFtpConnectionThread() {
    NS_RELEASE(mListener);
    NS_RELEASE(mChannel);
    NS_IF_RELEASE(mContext);
    NS_IF_RELEASE(mEventQueue);

    // Close the data channel
    NS_IF_RELEASE(mDPipe);

    // Close the command channel
    NS_RELEASE(mCPipe);

    // lose the socket transport
    NS_RELEASE(mSTS);

    nsAllocator::Free(mURLSpec);
}

nsresult
nsFtpConnectionThread::Process() {
    nsresult rv;
    PRBool continueRead = PR_FALSE;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFtpConnectionThread::Process() started for %x (spec =%s)\n", mUrl, mURLSpec));
    
    while (mKeepRunning) {

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
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - READ_BUF - Read() FAILED\n", mUrl));
                    return rv;
                }
                buffer[read] = '\0';
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - READ_BUF - read \"%s\" (%d bytes)\n", mUrl, buffer, read));

                // get the response code out.
                if (!continueRead) {
                    PR_sscanf(buffer, "%d", &mResponseCode);
                    mResponseCode = mResponseCode / 100; // truncate it
                }

                // see if we're handling a multi-line response.
                if (continueRead || (buffer[3] == '-')) {
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

                // get the rest of the line
                mResponseMsg = buffer+4;
                if (mState == mNextState) {
                    NS_ASSERTION(0, "ftp read state mixup");
                    mState = FTP_ERROR;
                } else {
                    mState = mNextState;
                }
                break;
                }
                // END: FTP_READ_BUF

            case FTP_ERROR:
                {
                // We have error'd out. Stop binding and pass the error back to the user.
                PRUnichar* errorMsg = nsnull;
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - ERROR\n", mUrl));
                nsFtpOnStopRequestEvent* event =
                    new nsFtpOnStopRequestEvent(mListener, mChannel, mContext);
                if (!event)
                    return NS_ERROR_OUT_OF_MEMORY;

                rv = MapResultCodeToString(rv, &errorMsg);
                if (NS_FAILED(rv)) {
                    delete event;
                    return rv;
                }

                rv = event->Init(rv, errorMsg);
                if (NS_FAILED(rv)) {
                    delete event;
                    return rv;
                }

                rv = event->Fire(mEventQueue);
                if (NS_FAILED(rv)) {
                    delete event;
                    return rv;
                }

                // we're done reading and writing, close the streams.
                mCOutStream->Close();
                mCInStream->Close();

                // after notifying the user of the error, stop processing.
                mKeepRunning = PR_FALSE;
                break;
                }
                // END: FTP_ERROR

            case FTP_COMPLETE:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - COMPLETE\n",mUrl));
                nsFtpOnStopRequestEvent* event =
                    new nsFtpOnStopRequestEvent(mListener, mChannel, mContext);
                if (event == nsnull)
                    return NS_ERROR_OUT_OF_MEMORY;

                rv = event->Fire(mEventQueue);
                if (NS_FAILED(rv)) {
                    delete event;
                    return rv;
                }
                
                // we're done reading and writing, close the streams.
                mCOutStream->Close();
                mCInStream->Close();

                // fall out of the while loop
                mKeepRunning = PR_FALSE;
                break;
                }
                // END: FTP_COMPLETE


//////////////////////////////
//// CONNECTION SETUP STATES
//////////////////////////////

            case FTP_S_USER:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_USER - ", mUrl));
                rv = S_user();
                if (NS_FAILED(rv)) {
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
                break;
                }
                // END: FTP_R_USER

            case FTP_S_PASS:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_PASS - ", mUrl));
                rv = S_pass();
                if (NS_FAILED(rv)) {
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
                break;
                }
                // END: FTP_R_PASS    

            case FTP_S_SYST:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_SYST - ", mUrl));
                rv = S_syst();
                if (NS_FAILED(rv)) {
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
                break;
                }
                // END: FTP_R_SYST

            case FTP_S_ACCT:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_ACCT - ", mUrl));
                rv = S_acct();
                if (NS_FAILED(rv)) {
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
                break;
                }
                // END: FTP_R_ACCT

            case FTP_S_MACB:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_MACB - ", mUrl));
                rv = S_macb();
                if (NS_FAILED(rv)) {
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
                break;
                }
                // END: FTP_R_MACB

            case FTP_S_PWD:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_PWD - ", mUrl));
                rv = S_pwd();
                if (NS_FAILED(rv)) {
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
                break;
                }
                // END: FTP_R_PWD

            case FTP_S_MODE:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_MODE - ", mUrl));
                rv = S_mode();
                if (NS_FAILED(rv)) {
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
                break;
                }
                // END: FTP_R_MODE

            case FTP_S_CWD:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_CWD - ", mUrl));
                rv = S_cwd();
                if (NS_FAILED(rv)) {
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
                break;
                }
                // END: FTP_R_CWD

            case FTP_S_SIZE:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_SIZE - ", mUrl));
                rv = S_size();
                if (NS_FAILED(rv)) {
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
                break;
                }
                // END: FTP_R_SIZE

            case FTP_S_MDTM:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_MDTM - ", mUrl));
                rv = S_mdtm();
                if (NS_FAILED(rv)) {
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
                break;
                }
                // END: FTP_R_MDTM

            case FTP_S_LIST:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_LIST - ", mUrl));
                rv = S_list();
                if (NS_FAILED(rv)) {
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
                    // get the data channel ready
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_LIST - Opening data stream ", mUrl));
                    rv = mDPipe->OpenInputStream(0, -1, &mDInStream);
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
                mDInStream->Close();
                break;
                }
                // END: FTP_R_LIST

            case FTP_S_RETR:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_RETR - ", mUrl));
                rv = S_retr();
                if (NS_FAILED(rv)) {
                    mState = FTP_ERROR;
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("FAILED\n"));
                } else {
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("SUCCEEDED\n"));
                    // get the data channel ready
                    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_RETR - Opening data stream ", mUrl));
                    rv = mDPipe->OpenInputStream(0, -1, &mDInStream);
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
                mDInStream->Close();
                break;
                }
                // END: FTP_R_RETR

                    
//////////////////////////////
//// DATA CONNECTION STATES
//////////////////////////////
            case FTP_S_PASV:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_PASV - ", mUrl));
                rv = S_pasv();
                if (NS_FAILED(rv)) {
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
                break;
                }
                // END: FTP_R_PASV

//////////////////////////////
//// ACTION STATES
//////////////////////////////
            case FTP_S_DEL_FILE:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_DEL_FILE - ", mUrl));
                rv = S_del_file();
                if (NS_FAILED(rv)) {
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
                break;
                }
                // END: FTP_R_DEL_FILE

            case FTP_S_DEL_DIR:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_DEL_DIR - ", mUrl));
                rv = S_del_dir();
                if (NS_FAILED(rv)) {
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
                break;
                }
                // END: FTP_R_DEL_DIR

            case FTP_S_MKDIR:
                {
                PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Process() - S_MKDIR - ", mUrl));
                rv = S_mkdir();
                if (NS_FAILED(rv)) {
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
                break;
                }
                // END: FTP_R_MKDIR

            default:
                ;

        } // END: switch 
    } // END: while loop

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("nsFtpConnectionThread::Process() ended for %x (spec =%s)\n\n\n", mUrl, mURLSpec));

    return NS_OK;
}

///////////////////////////////////
// STATE METHODS
///////////////////////////////////
nsresult
nsFtpConnectionThread::S_user() {
    nsresult rv;
    char *buffer;
    PRUint32 bufLen, bytes;
    nsString2 usernameStr("USER ");

    if (!mUsername.Length()) {
        // XXX we need to prompt the user to enter a password.

        // sendEventToUIThreadToPostADialog(&mPassword);
        usernameStr.Append("anonymous");
    } else {
        usernameStr.Append(mUsername);    
    }
    usernameStr.Append(FTP_CRLF);
    buffer = usernameStr.ToNewCString();
    if (!buffer) return NS_ERROR_OUT_OF_MEMORY;
    bufLen = PL_strlen(buffer);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mUrl, buffer));

    rv = mCOutStream->Write(buffer, bufLen, &bytes);
    nsAllocator::Free(buffer);
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
        return FTP_ERROR;
    }
}


nsresult
nsFtpConnectionThread::S_pass() {
    nsresult rv;
    char *buffer;
    PRUint32 bufLen, bytes;
    nsString2 passwordStr("PASS ");

    if (!mPassword.Length()) {
        // XXX we need to prompt the user to enter a password.

        // sendEventToUIThreadToPostADialog(&mPassword);
        passwordStr.Append("mozilla@");
    } else {
        passwordStr.Append(mPassword);    
    }
    passwordStr.Append(FTP_CRLF);
    buffer = passwordStr.ToNewCString();
    if (!buffer) return NS_ERROR_OUT_OF_MEMORY;
    bufLen = PL_strlen(buffer);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mUrl, buffer));

    rv = mCOutStream->Write(buffer, bufLen, &bytes);
    nsAllocator::Free(buffer);
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
        // LOGIN FAILED
        return FTP_ERROR;
    }
}

nsresult
nsFtpConnectionThread::S_syst() {
    char *buffer = "SYST" FTP_CRLF;
    PRUint32 bufLen, bytes;
    bufLen = PL_strlen(buffer);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mUrl, buffer));

    return mCOutStream->Write(buffer, bufLen, &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_syst() {
    FTP_STATE state = FTP_ERROR;
    if (mResponseCode == 2) {
        if (mUseDefaultPath) {
            state = FTP_S_PWD;
        } else {
            state = FindActionState();
        }

        SetSystInternals(); // must be called first to setup member vars.

        // setup next state based on server type.
        if (mServerType == FTP_PETER_LEWIS_TYPE || mServerType == FTP_WEBSTAR_TYPE) {
            state = FTP_S_MACB;
        } else if (mServerType == FTP_TCPC_TYPE || mServerType == FTP_GENERIC_TYPE) {
            state = FTP_S_PWD;
        } 
    } else {
        state = FTP_S_PWD;        
    }
    return state;
}

nsresult
nsFtpConnectionThread::S_acct() {
    char *buffer = "ACCT noaccount" FTP_CRLF;
    PRUint32 bufLen = PL_strlen(buffer), bytes;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mUrl, buffer));

    return mCOutStream->Write(buffer, bufLen, &bytes);
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
    PRUint32 bufLen = PL_strlen(buffer), bytes;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mUrl, buffer));

    return mCOutStream->Write(buffer, bufLen, &bytes);
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
    PRUint32 bufLen = PL_strlen(buffer), bytes;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mUrl, buffer));

    return mCOutStream->Write(buffer, bufLen, &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_pwd() {
    nsresult rv;
    FTP_STATE state = FTP_ERROR;
    nsString2 lNewMsg;
    // fun response interpretation begins :)
    /*PRInt32 start = mResponseMsg.Find("\"");
    nsString2 lNewMsg;
    if (start > -1) {
        mResponseMsg.Left(lNewMsg, start);
    } else {
        lNewMsg = mResponseMsg;                
    }*/
    lNewMsg = mResponseMsg;

    // check for a quoted path
    PRInt32 beginQ, endQ;
    beginQ = lNewMsg.Find("\"");
    if (beginQ > -1) {
        endQ = lNewMsg.RFind("\"");
        if (endQ > beginQ) {
            // quoted path
            nsString2 tmpPath;
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

    if (mUseDefaultPath && mServerType != FTP_VMS_TYPE) {
        // we want to use the default path specified by the PWD command.
        /*PRInt32 start = lNewMsg.Find('"', 1, PR_FALSE);
        nsString2 path, ptr;
        lNewMsg.Mid(path, start-1, lNewMsg.Length() - start);*/
        nsString2 path = lNewMsg;
        nsString2 ptr;
        PRInt32 start;

        if (path.First() != '/') {
            start = path.Find('/');
            if (start > -1) {
                path.Right(ptr, start);
            } else {
                // if we couldn't find a slash, check for back slashes and switch them out.
                PRInt32 start = path.Find("\\");
                if (start > -1) {
                    path.ReplaceChar("\\", '/');
                }
            }
        } else {
            ptr = path;
        }

        // construct the new url
        if (ptr.Length()) {
            nsString2 newPath;

            newPath = ptr;


            char *initialPath = nsnull;
            rv = mUrl->GetPath(&initialPath);
            if (NS_FAILED(rv)) return FTP_ERROR;
            
            if (initialPath && *initialPath) {
                if (newPath.Last() == '/')
                    newPath.Cut(newPath.Length()-1, 1);
                newPath.Append(initialPath);
            }
            nsAllocator::Free(initialPath);

            char *p = newPath.ToNewCString();
            mUrl->SetPath(p);
            nsAllocator::Free(p);
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
    PRUint32 bufLen, bytes;
    if (mBin) {
        buffer = "TYPE I" FTP_CRLF;
    } else {
        buffer = "TYPE A" FTP_CRLF;
    }
    bufLen = PL_strlen(buffer);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mUrl, buffer));

    return mCOutStream->Write(buffer, bufLen, &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_mode() {
    if (mResponseCode != 2) {
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
    nsresult rv;
    char *path, *buffer;
    PRUint32 bufLen, bytes;
    rv = mUrl->GetPath(&path);
    if (NS_FAILED(rv)) return rv;

    nsString2 cwdStr("CWD ");

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
    buffer = cwdStr.ToNewCString();
    bufLen = PL_strlen(buffer);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mUrl, buffer));

    rv = mCOutStream->Write(buffer, bufLen, &bytes);
    nsAllocator::Free(buffer);
    return rv;
}

FTP_STATE
nsFtpConnectionThread::R_cwd() {
    FTP_STATE state = FTP_ERROR;
    if (mResponseCode == 2) {
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
    char *path, *buffer;
    PRUint32 bufLen, bytes;
    rv = mUrl->GetPath(&path);
    if (NS_FAILED(rv)) return rv;

    // XXX should the actual file name be parsed out???
    mFilename = path; 

    /*if (mServerType == FTP_VMS_TYPE) {
        mState = FindGetState();
        break;
    }*/

    nsString2 sizeBuf("SIZE ");
    sizeBuf.Append(path);
    sizeBuf.Append(FTP_CRLF);
    buffer = sizeBuf.ToNewCString();
    if (!buffer) return NS_ERROR_OUT_OF_MEMORY;

    nsAllocator::Free(path);
    bufLen = PL_strlen(buffer);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mUrl, buffer));

    rv = mCOutStream->Write(buffer, bufLen, &bytes);
    nsAllocator::Free(buffer);
    return rv;
}

FTP_STATE
nsFtpConnectionThread::R_size() {
    if (mResponseCode == 2) {
        PRInt32 conversionError;
        mLength = mResponseMsg.ToInteger(&conversionError);
    } if (mResponseCode == 5) {
        // couldn't get the size of what we asked for, must be a dir.
        return FTP_S_CWD;
    }
    return FTP_S_MDTM;
}

nsresult
nsFtpConnectionThread::S_mdtm() {
    nsresult rv;
    char *path, *buffer;
    PRUint32 bufLen, bytes;
    rv = mUrl->GetPath(&path);
    if (NS_FAILED(rv)) return rv;

    /*if (mServerType == FTP_VMS_TYPE) {
        mState = FindGetState();
        break;
    }*/

    nsString2 mdtmStr("MDTM ");
    mdtmStr.Append(path);
    mdtmStr.Append(FTP_CRLF);
    nsAllocator::Free(path);

    buffer = mdtmStr.ToNewCString();
    if (!buffer) return NS_ERROR_OUT_OF_MEMORY;
    bufLen = PL_strlen(buffer);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mUrl, buffer));

    rv = mCOutStream->Write(buffer, bufLen, &bytes);
    nsAllocator::Free(buffer);
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
        PRInt64 t1, t2;
        PRTime t;
        time_t tt;
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

        t = PR_ImplodeTime(&ts);
        LL_I2L(t1, PR_USEC_PER_SEC);
        LL_DIV(t2, t, t1);
        LL_L2I(tt, t2);

        mLastModified = tt;                
    }

    return FindGetState();
}

nsresult
nsFtpConnectionThread::S_list() {
    char *buffer;
    PRUint32 bufLen, bytes;
    if (mList)
        buffer = "LIST" FTP_CRLF;
    else
        buffer = "NLST" FTP_CRLF;
    bufLen = PL_strlen(buffer);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mUrl, buffer));

    return mCOutStream->Write(buffer, bufLen, &bytes);
}

FTP_STATE
nsFtpConnectionThread::R_list() {
    nsresult rv;
    char listBuf[NS_FTP_BUFFER_READ_SIZE];
    PRUint32 read;

    // XXX need to loop this read somehow.
    rv = mDInStream->Read(listBuf, NS_FTP_BUFFER_READ_SIZE, &read);
    if (NS_FAILED(rv)) return FTP_ERROR;

    // tell the user about the data.

    nsIInputStream *stringStream = nsnull;
    nsISupports *stringStrmSup = nsnull;
    rv = NS_NewCharInputStream(&stringStrmSup, listBuf);
    if (NS_FAILED(rv)) return FTP_ERROR;

    rv = stringStrmSup->QueryInterface(nsCOMTypeInfo<nsIInputStream>::GetIID(), (void**)&stringStream);
    if (NS_FAILED(rv)) return FTP_ERROR;

    nsFTPContext *dataCtxt = new nsFTPContext();
    if (!dataCtxt) return FTP_ERROR;
    dataCtxt->SetContentType("text/ftp-dirListing");
    nsISupports *ctxtSup = nsnull;
    rv = dataCtxt->QueryInterface(nsCOMTypeInfo<nsISupports>::GetIID(), (void**)&ctxtSup);

    // tell the user that we've begun the transaction.
    nsFtpOnStartRequestEvent* startEvent =
        new nsFtpOnStartRequestEvent(mListener, mChannel, mContext);
    if (!startEvent) return FTP_ERROR;

    rv = startEvent->Fire(mEventQueue);
    if (NS_FAILED(rv)) {
        delete startEvent;
        return FTP_ERROR;
    }

    nsFtpOnDataAvailableEvent* availEvent =
        new nsFtpOnDataAvailableEvent(mListener, mChannel, ctxtSup); // XXX the destroy event method
                                                                      // XXX needs to clean up this new.
    if (!availEvent) return FTP_ERROR;

    rv = availEvent->Init(stringStream, 0, read);
    NS_RELEASE(stringStream);
    if (NS_FAILED(rv)) {
        delete availEvent;
        return FTP_ERROR;
    }

    rv = availEvent->Fire(mEventQueue);
    if (NS_FAILED(rv)) {
        delete availEvent;
        return FTP_ERROR;
    }

    return FTP_COMPLETE;
}

nsresult
nsFtpConnectionThread::S_retr() {
    nsresult rv;
    char *path, *buffer;
    PRUint32 bufLen, bytes;
    rv = mUrl->GetPath(&path);
    if (NS_FAILED(rv)) return rv;

    nsString2 retrStr("RETR ");

    if (mServerType == FTP_VMS_TYPE) {
        NS_ASSERTION(mFilename.Length() > 0, "ftp filename not present");
        retrStr.Append(mFilename);
    } else {
        retrStr.Append(path);
    }
    nsAllocator::Free(path);
    retrStr.Append(FTP_CRLF);
    buffer = retrStr.ToNewCString();
    if (!buffer) return NS_ERROR_OUT_OF_MEMORY;

    bufLen = PL_strlen(buffer);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mUrl, buffer));

    rv = mCOutStream->Write(buffer, bufLen, &bytes);
    nsAllocator::Free(buffer);
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
        nsIBufferInputStream *bufInStrm = nsnull;
        nsIBufferOutputStream *bufOutStrm = nsnull;
        rv = NS_NewPipe(&bufInStrm, &bufOutStrm);
        if (NS_FAILED(rv)) return FTP_ERROR;

        PRUint32 read = 0, readSoFar = 0;
        while (readSoFar < mLength) {
            // suck in the data.
            bufOutStrm->WriteFrom(mDInStream, mLength-readSoFar, &read);
            readSoFar += read;
        }

        // we're done filling this end of the pipe. close it.
        bufOutStrm->Close();

        nsFTPContext *dataCtxt = new nsFTPContext();
        if (!dataCtxt) return FTP_ERROR;

        NS_WITH_SERVICE(nsIMIMEService, MIMEService, kMIMEServiceCID, &rv);
        if (NS_FAILED(rv)) return FTP_ERROR;


        char *contentType;
        rv = MIMEService->GetTypeFromURI(mUrl, &contentType);
        if (NS_FAILED(rv)) return FTP_ERROR;

        dataCtxt->SetContentType(contentType);
        nsISupports *ctxtSup = nsnull;
        rv = dataCtxt->QueryInterface(nsCOMTypeInfo<nsISupports>::GetIID(), (void**)&ctxtSup);

        // tell the user that we've begun the transaction.
        nsFtpOnStartRequestEvent* startEvent =
            new nsFtpOnStartRequestEvent(mListener, mChannel, mContext);
        if (!startEvent) return FTP_ERROR;

        rv = startEvent->Fire(mEventQueue);
        if (NS_FAILED(rv)) {
            delete startEvent;
            return FTP_ERROR;
        }

        nsFtpOnDataAvailableEvent* event =
            new nsFtpOnDataAvailableEvent(mListener, mChannel, ctxtSup); // XXX the destroy event method
                                                                          // XXX needs to clean up this new.
        if (!event) return FTP_ERROR;

        nsIInputStream *inStream = nsnull;
        rv = bufInStrm->QueryInterface(nsCOMTypeInfo<nsIInputStream>::GetIID(), (void**)&inStream);
        if (NS_FAILED(rv)) return FTP_ERROR;

        rv = event->Init(inStream, 0, mLength);
        NS_RELEASE(inStream);
        if (NS_FAILED(rv)) {
            delete event;
            return FTP_ERROR;
        }

        rv = event->Fire(mEventQueue);
        if (NS_FAILED(rv)) {
            delete event;
            return FTP_ERROR;
        }

        return FTP_COMPLETE;

    } else {
        if (mServerType == FTP_VMS_TYPE) {
            // XXX the old code has some DOUBLE_PASV code in here to 
            // XXX try again. I'm forgoing it for now.
            return FTP_ERROR;
        } else {
            return FTP_S_CWD;
        }
    }
}


nsresult
nsFtpConnectionThread::S_pasv() {
    char *buffer = "PASV" FTP_CRLF;
    PRUint32 bufLen = PL_strlen(buffer), bytes;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mUrl, buffer));

    return mCOutStream->Write(buffer, bufLen, &bytes);
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

    // now we know where to connect our data channel
    rv = mSTS->CreateTransport(host.GetBuffer(), port, &mDPipe); // the data channel
    if (NS_FAILED(rv)) return FTP_ERROR;

    if (mAction == GET) {
        // Setup the data channel for file reception

        nsFTPContext *dataCtxt = new nsFTPContext();
        if (!dataCtxt) return FTP_ERROR;
        dataCtxt->mCmdResponse = PR_FALSE;
        nsISupports *ctxtSup = nsnull;
        rv = dataCtxt->QueryInterface(nsCOMTypeInfo<nsISupports>::GetIID(), (void**)&ctxtSup);

    } else {
        // get the output stream so we can write to the server
        rv = mDPipe->OpenOutputStream(0, &mDOutStream);
        if (NS_FAILED(rv)) return FTP_ERROR;
    }

    // we're connected figure out what type of transfer we're doing (ascii or binary)
    char *type = nsnull;
    rv = mChannel->GetContentType(&type);
    nsString2 typeStr;
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
    char *filename, *buffer;
    PRUint32 bufLen, bytes;
    rv = mUrl->GetPath(&filename); // XXX we should probably check to 
                                   // XXX make sure we have an actual filename.
    if (NS_FAILED(rv)) return rv;

    nsString2 delStr("DELE ");
    delStr.Append(filename);
    delStr.Append(FTP_CRLF);
    nsAllocator::Free(filename);

    buffer = delStr.ToNewCString();
    if (!buffer) return NS_ERROR_OUT_OF_MEMORY;

    bufLen = PL_strlen(buffer);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mUrl, buffer));

    rv = mCOutStream->Write(buffer, bufLen, &bytes);
    nsAllocator::Free(buffer);
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
    char *dir, *buffer;
    PRUint32 bufLen, bytes;
    rv = mUrl->GetPath(&dir);
    if (NS_FAILED(rv)) return rv;

    nsString2 delDirStr("RMD ");
    delDirStr.Append(dir);
    delDirStr.Append(FTP_CRLF);
    nsAllocator::Free(dir);

    buffer = delDirStr.ToNewCString();
    if (!buffer) return NS_ERROR_OUT_OF_MEMORY;

    bufLen = PL_strlen(buffer);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mUrl, buffer));

    rv = mCOutStream->Write(buffer, bufLen, &bytes);
    nsAllocator::Free(buffer);
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
    char *dir, *buffer;
    PRUint32 bufLen, bytes;
    rv = mUrl->GetPath(&dir);
    if (NS_FAILED(rv)) return rv;

    nsString2 mkdirStr("MKD ");
    mkdirStr.Append(dir);
    mkdirStr.Append(FTP_CRLF);
    nsAllocator::Free(dir);

    buffer = mkdirStr.ToNewCString();
    if (!buffer) return NS_ERROR_OUT_OF_MEMORY;

    bufLen = PL_strlen(buffer);

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("%x Writing \"%s\"\n", mUrl, buffer));

    rv = mCOutStream->Write(buffer, bufLen, &bytes);
    nsAllocator::Free(buffer);
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
    
    // Create the Event Queue for this thread...
    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = eventQService->CreateThreadEventQueue();
    if (NS_FAILED(rv)) return rv;

    rv = nsServiceManager::GetService(kSocketTransportServiceCID,
                                      nsCOMTypeInfo<nsISocketTransportService>::GetIID(), 
                                      (nsISupports **)&mSTS);
    if(NS_FAILED(rv)) return rv;

    // Create the command channel transport
    nsXPIDLCString host;
    PRInt32 port = 0;
    rv = mUrl->GetHost(getter_Copies(host));
    if (NS_FAILED(rv)) return rv;
    rv = mUrl->GetPort(&port);
    if (NS_FAILED(rv)) return rv;

    ///////////////////////////////
    // COMMAND CHANNEL SETUP
    ///////////////////////////////
    rv = mSTS->CreateTransport(host, port, &mCPipe); // the command channel
    if (NS_FAILED(rv)) return rv;

    // get the output stream so we can write to the server
    rv = mCPipe->OpenOutputStream(0, &mCOutStream);
    if (NS_FAILED(rv)) return rv;

    rv = mCPipe->OpenInputStream(0, -1, &mCInStream);
    if (NS_FAILED(rv)) return rv;

    // digest any server greeting.
    char greetBuf[NS_FTP_BUFFER_READ_SIZE];
    PRUint32 read;
    rv = mCInStream->Read(greetBuf, NS_FTP_BUFFER_READ_SIZE, &read);

    ///////////////////////////////
    // END - COMMAND CHANNEL SETUP
    ///////////////////////////////

    mConnected = PR_TRUE;

    rv = Process();

    NS_RELEASE(mCPipe);
    NS_IF_RELEASE(mDPipe);

    NS_RELEASE(mSTS);

    return rv;
}

nsresult
nsFtpConnectionThread::Init(nsIURI* aUrl) {
    nsresult rv;
    mUrl = aUrl;
    NS_ADDREF(mUrl);

    mUrl->GetSpec(&mURLSpec);

    // pull any username and/or password out of the uri
    char *preHost = nsnull;
    rv = mUrl->GetPreHost(&preHost);
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
nsFtpConnectionThread::SetUsePasv(PRBool aUsePasv) {
    if (mConnected)
        return NS_ERROR_ALREADY_CONNECTED;
    mUsePasv = aUsePasv;
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
    else if (mResponseMsg.Find("Windows_NT") > -1) {
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

    rv = mUrl->GetPath(&path);
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
#if 0  // XXX waiting on NS_NewString() to make it's way into the builds (raptorbase.dll)
    nsresult rv;
    nsStr* string = nsnull;

    rv = NS_NewString(aOutMsg);
    if (NS_FAILED(rv)) return rv;

    string = new nsStr;
    if (!string) return NS_ERROR_OUT_OF_MEMORY;

    switch (aResultCode) {
    case NS_ERROR_FTP_LOGIN:
        {
            string->mStr = "FTP: Login failed.";
            rv = (*aOutMsg)->SetStr(string);
            if (NS_FAILED(rv)) {
                delete string;
                return rv;
            }
            break;
        }
    case NS_ERROR_FTP_MODE:
        {
            string->mStr = "FTP: MODE command failed.";
            rv = (*aOutMsg)->SetStr(string);
            if (NS_FAILED(rv)) {
                delete string;
                return rv;
            }
            break;
        }
    case NS_ERROR_FTP_CWD:
        {
            string->mStr = "FTP: CWD command failed.";
            rv = (*aOutMsg)->SetStr(string);
            if (NS_FAILED(rv)) {
                delete string;
                return rv;
            }
            break;
        }
    case NS_ERROR_FTP_PASV:
        {
            string->mStr = "FTP: PASV command failed.";
            rv = (*aOutMsg)->SetStr(string);
            if (NS_FAILED(rv)) {
                delete string;
                return rv;
            }
            break;
        }
    case NS_ERROR_FTP_DEL_DIR:
        {
            string->mStr = "FTP: DEL directory command failed.";
            rv = (*aOutMsg)->SetStr(string);
            if (NS_FAILED(rv)) {
                delete string;
                return rv;
            }
            break;
        }
    case NS_ERROR_FTP_MKDIR:
        {
            string->mStr = "FTP: MKDIR command failed";
            rv = (*aOutMsg)->SetStr(string);
            if (NS_FAILED(rv)) {
                delete string;
                return rv;
            }
            break;
        }
    default:
        {
            string->mStr = "Unknown FTP error.";
            rv = (*aOutMsg)->SetStr(string);
            if (NS_FAILED(rv)) {
                delete string;
                return rv;
            }
            break;
        }
    } // END: switch

    delete string;

#endif // if 0
    return NS_OK;
}


