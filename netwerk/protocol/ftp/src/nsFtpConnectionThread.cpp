/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Contributor(s): Bradley Baetz <bbaetz@cs.mcgill.ca>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsFTPChannel.h"
#include "nsFtpConnectionThread.h"
#include "nsFtpControlConnection.h"
#include "nsFtpProtocolHandler.h"

#include <limits.h>

#include "nsISocketTransport.h"
#include "nsIStreamConverterService.h"
#include "nsReadableUtils.h"
#include "prprf.h"
#include "prlog.h"
#include "prnetdb.h"
#include "netCore.h"
#include "ftpCore.h"
#include "nsProxiedService.h"
#include "nsCRT.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIURL.h"
#include "nsEscape.h"
#include "nsNetUtil.h"
#include "nsIDNSService.h" // for host error code
#include "nsIWalletService.h"
#include "nsIMemory.h"
#include "nsIStringStream.h"
#include "nsIPref.h"
#include "nsMimeTypes.h"
#include "nsIStringBundle.h"

#include "nsICacheEntryDescriptor.h"
#include "nsICacheListener.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kWalletServiceCID, NS_WALLETSERVICE_CID);
static NS_DEFINE_CID(kStreamConverterServiceCID,    NS_STREAMCONVERTERSERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kStreamListenerTeeCID, NS_STREAMLISTENERTEE_CID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);


#if defined(PR_LOGGING)
extern PRLogModuleInfo* gFTPLog;
#endif /* PR_LOGGING */

class DataRequestForwarder : public nsIFTPChannel, 
                             public nsIStreamListener,
                             public nsIInterfaceRequestor,
                             public nsIProgressEventSink
{
public:
    DataRequestForwarder();
    virtual ~DataRequestForwarder();
    nsresult Init(nsIRequest *request);

    nsresult SetStreamListener(nsIStreamListener *listener);
    nsresult SetCacheEntry(nsICacheEntryDescriptor *entry, PRBool writing);

    NS_DECL_ISUPPORTS
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIPROGRESSEVENTSINK

    NS_FORWARD_NSIREQUEST(mRequest->)
    NS_FORWARD_NSICHANNEL(mFTPChannel->)
    NS_FORWARD_NSIFTPCHANNEL(mFTPChannel->)
    
    PRUint32 GetBytesTransfered() {return mBytesTransfered;} ;
    void Uploading(PRBool value);
    void RetryConnection();

protected:

    nsCOMPtr<nsIRequest>              mRequest;
    nsCOMPtr<nsIFTPChannel>           mFTPChannel;
    nsCOMPtr<nsIStreamListener>       mListener;
    nsCOMPtr<nsIProgressEventSink>    mEventSink;
    nsCOMPtr<nsICacheEntryDescriptor> mCacheEntry;

    PRUint32 mBytesTransfered;
    PRPackedBool   mDelayedOnStartFired;
    PRPackedBool   mUploading;
    PRPackedBool   mRetrying;
    
    nsresult DelayedOnStartRequest(nsIRequest *request, nsISupports *ctxt);
};


// The whole purpose of this class is to opaque 
// the socket transport so that clients only see
// the same nsIChannel/nsIRequest that they started.

NS_IMPL_THREADSAFE_ISUPPORTS7(DataRequestForwarder, 
                              nsIStreamListener, 
                              nsIRequestObserver, 
                              nsIFTPChannel,
                              nsIChannel,
                              nsIRequest,
                              nsIInterfaceRequestor,
                              nsIProgressEventSink);


DataRequestForwarder::DataRequestForwarder()
{
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) DataRequestForwarder CREATED\n", this));
        
    mBytesTransfered = 0;
    mRetrying = mUploading = mDelayedOnStartFired = PR_FALSE;
    NS_INIT_ISUPPORTS();
}

DataRequestForwarder::~DataRequestForwarder()
{
}

// nsIInterfaceRequestor method
NS_IMETHODIMP
DataRequestForwarder::GetInterface(const nsIID &anIID, void **aResult ) 
{
    if (anIID.Equals(NS_GET_IID(nsIProgressEventSink))) 
    {
        *aResult = NS_STATIC_CAST(nsIProgressEventSink*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    } 
    return NS_ERROR_NO_INTERFACE;
}


nsresult 
DataRequestForwarder::Init(nsIRequest *request)
{
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) DataRequestForwarder Init [request=%x]\n", this, request));

    NS_ENSURE_ARG(request);

    // for the forwarding declarations.
    mRequest    = request;
    mFTPChannel = do_QueryInterface(request);
    mEventSink  = do_QueryInterface(request);
    mListener   = do_QueryInterface(request);
    
    if (!mRequest || !mFTPChannel)
        return NS_ERROR_FAILURE;
    
    return NS_OK;
}

void 
DataRequestForwarder::Uploading(PRBool value)
{
    mUploading = value;
}

nsresult 
DataRequestForwarder::SetCacheEntry(nsICacheEntryDescriptor *cacheEntry, PRBool writing)
{
    // if there is a cache entry descriptor, send data to it.
    if (!cacheEntry) 
        return NS_ERROR_FAILURE;
    
    mCacheEntry = cacheEntry;
    if (!writing)
        return NS_OK;

    nsCOMPtr<nsITransport> cacheTransport;
    nsresult rv = cacheEntry->GetTransport(getter_AddRefs(cacheTransport));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIOutputStream> out;
    rv = cacheTransport->OpenOutputStream(0, PRUint32(-1), 0, getter_AddRefs(out));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStreamListenerTee> tee =
        do_CreateInstance(kStreamListenerTeeCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = tee->Init(mListener, out);
    if (NS_FAILED(rv)) return rv;

    mListener = do_QueryInterface(tee, &rv);

    return NS_OK;
}


nsresult 
DataRequestForwarder::SetStreamListener(nsIStreamListener *listener)
{
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) DataRequestForwarder SetStreamListener [listener=%x]\n", this, listener)); 
    
    mListener = listener;
    if (!mListener) 
        return NS_ERROR_FAILURE;

    return NS_OK;
}

nsresult 
DataRequestForwarder::DelayedOnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) DataRequestForwarder DelayedOnStartRequest \n", this)); 
    return mListener->OnStartRequest(this, ctxt); 
}

void
DataRequestForwarder::RetryConnection()
{
    // The problem here is that if we send a second PASV, our listener would
    // get an OnStop from the socket transport, and fail. So we temporarily
    // suspend notifications
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) DataRequestForwarder RetryConnection \n", this));

    mRetrying = PR_TRUE;
}

NS_IMETHODIMP 
DataRequestForwarder::OnStartRequest(nsIRequest *request, nsISupports *ctxt)
{
    NS_ASSERTION(mListener, "No Listener Set.");
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) DataRequestForwarder OnStartRequest \n[mRetrying=%d]", this, mRetrying)); 

    if (mRetrying) {
        mRetrying = PR_FALSE;
        return NS_OK;
    }

    if (!mListener)
        return NS_ERROR_NOT_INITIALIZED;

    if (mUploading)
        return mListener->OnStartRequest(this, ctxt);     

    return NS_OK;
}

NS_IMETHODIMP
DataRequestForwarder::OnStopRequest(nsIRequest *request, nsISupports *ctxt, nsresult statusCode)
{
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) DataRequestForwarder OnStopRequest [status=%x, mRetrying=%d]\n", this, statusCode, mRetrying)); 

    if (mRetrying)
        return NS_OK;
    
    nsCOMPtr<nsITransportRequest> trequest(do_QueryInterface(request));
    if (trequest)
    {
        nsCOMPtr<nsITransport> trans;
        trequest->GetTransport(getter_AddRefs(trans));
        nsCOMPtr<nsISocketTransport> sTrans (do_QueryInterface(trans));
        if (sTrans)
            sTrans->SetReuseConnection(PR_FALSE);
    }
    if (!mListener)
        return NS_ERROR_NOT_INITIALIZED;

    return mListener->OnStopRequest(this, ctxt, statusCode);
}

NS_IMETHODIMP
DataRequestForwarder::OnDataAvailable(nsIRequest *request, nsISupports *ctxt, nsIInputStream *input, PRUint32 offset, PRUint32 count)
{ 
    nsresult rv;
    NS_ASSERTION(mListener, "No Listener Set.");
    NS_ASSERTION(!mUploading, "Since we are uploading, we should never get a ODA");
 
    if (!mListener)
        return NS_ERROR_NOT_INITIALIZED;

    // we want to delay firing the onStartRequest until we know that there is data
    if (!mDelayedOnStartFired) { 
        mDelayedOnStartFired = PR_TRUE;
        rv = DelayedOnStartRequest(request, ctxt);
        if (NS_FAILED(rv)) return rv;
    }

    rv = mListener->OnDataAvailable(this, ctxt, input, mBytesTransfered, count); 
    mBytesTransfered += count;
    return rv;
} 


// nsIProgressEventSink methods
NS_IMETHODIMP
DataRequestForwarder::OnStatus(nsIRequest *request, nsISupports *aContext,
                       nsresult aStatus, const PRUnichar* aStatusArg)
{
    if (!mEventSink)
        return NS_OK;

    return mEventSink->OnStatus(nsnull, nsnull, aStatus, nsnull);
}

NS_IMETHODIMP
DataRequestForwarder::OnProgress(nsIRequest *request, nsISupports* aContext,
                                  PRUint32 aProgress, PRUint32 aProgressMax) {
    if (!mEventSink)
        return NS_OK;
    PRUint32 count = mUploading ? aProgress : mBytesTransfered;
    PRUint32 max   = mUploading ? aProgressMax : 0;
    return mEventSink->OnProgress(this, nsnull, count, max);
}



NS_IMPL_THREADSAFE_ISUPPORTS3(nsFtpState, 
                              nsIStreamListener, 
                              nsIRequestObserver, 
                              nsIRequest);

nsFtpState::nsFtpState() {
    NS_INIT_REFCNT();

    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("(%x) nsFtpState created", this));
    // bool init
    mRETRFailed = PR_FALSE;
    mWaitingForDConn = mTryingCachedControl = mRetryPass = PR_FALSE;
    mFireCallbacks = mKeepRunning = mAnonymous = PR_TRUE;

    mAction = GET;
    mState = FTP_COMMAND_CONNECT;
    mNextState = FTP_S_USER;

    mInternalError = NS_OK;
    mSuspendCount = 0;
    mPort = 21;

    mReceivedControlData = PR_FALSE;
    mControlStatus = NS_OK;            
    mControlReadContinue = PR_FALSE;
    mControlReadBrokenLine = PR_FALSE;

    mIPv6Checked = PR_FALSE;
    mIPv6ServerAddress = nsnull;
    
    mControlConnection = nsnull;
    mDRequestForwarder = nsnull;

    mGenerateRawContent = mGenerateHTMLContent = PR_FALSE;
    nsresult rv;
    nsCOMPtr<nsIPref> pPref(do_GetService(kPrefCID, &rv)); 
    if (NS_SUCCEEDED(rv) || pPref) { 
        pPref->GetBoolPref("network.dir.generate_html", &mGenerateHTMLContent);
        pPref->GetBoolPref("network.ftp.raw_output", &mGenerateRawContent);
    }
}

nsFtpState::~nsFtpState() 
{
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("(%x) nsFtpState destroyed", this));
    
    if (mIPv6ServerAddress) nsMemory::Free(mIPv6ServerAddress);
    NS_IF_RELEASE(mDRequestForwarder);
}


// nsIStreamListener implementation
NS_IMETHODIMP
nsFtpState::OnDataAvailable(nsIRequest *request,
                            nsISupports *aContext,
                            nsIInputStream *aInStream,
                            PRUint32 aOffset, 
                            PRUint32 aCount)
{    
    if (aCount == 0)
        return NS_OK; /*** should this be an error?? */
    
    if (!mReceivedControlData) {
        nsCOMPtr<nsIProgressEventSink> sink(do_QueryInterface(mChannel));
        if (sink)
            // parameter can be null cause the channel fills them in.
            sink->OnStatus(nsnull, nsnull, 
                           NS_NET_STATUS_BEGIN_FTP_TRANSACTION, nsnull);
        
        mReceivedControlData = PR_TRUE;
    }

    char* buffer = (char*)nsMemory::Alloc(aCount + 1);
    nsresult rv = aInStream->Read(buffer, aCount, &aCount);
    if (NS_FAILED(rv)) 
    {
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) nsFtpState - error reading %x\n", this, rv));
        return NS_ERROR_FAILURE;
    }
    buffer[aCount] = '\0';
        

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) reading %d bytes: \"%s\"", this, aCount, buffer));

    // get the response code out.
    if (!mControlReadContinue && !mControlReadBrokenLine) {
        PR_sscanf(buffer, "%d", &mResponseCode);
        if (buffer[3] == '-') {
            mControlReadContinue = PR_TRUE;
        } else {
            mControlReadBrokenLine = !PL_strstr(buffer, CRLF);
        }
    }

    // see if we're handling a multi-line response.
    if (mControlReadContinue) {
        // yup, multi-line response, start appending
        char *tmpBuffer = nsnull, *crlf = nsnull;
        
        // if we have data from our last round be sure
        // to prepend it.
        char *cleanup = nsnull;
        if (!mControlReadCarryOverBuf.IsEmpty() ) {
            mControlReadCarryOverBuf += buffer;
            cleanup = tmpBuffer = ToNewCString(mControlReadCarryOverBuf);
            mControlReadCarryOverBuf.Truncate(0);
            if (!tmpBuffer) return NS_ERROR_OUT_OF_MEMORY;
        } else {
            cleanup = tmpBuffer = buffer;
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
            mControlReadCarryOverBuf = tmpBuffer;
        
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
            mControlReadContinue = PR_FALSE;
        } else {
            // don't increment state, we need to read more.
            mControlReadContinue = PR_TRUE;
        }
    }
    else
    {
        NS_ASSERTION(mState != mNextState, "ftp read state mixup");
    
        // get the rest of the line
        if (mControlReadBrokenLine) {
            mResponseMsg += buffer;
            mControlReadBrokenLine = !PL_strstr(buffer, CRLF);
            if (!mControlReadBrokenLine)
                mState = mNextState;
        } else {
            mResponseMsg = buffer+4;
            mState = mNextState;
        }
    }
    
    if (!mControlReadContinue)
        {
            if (mFTPEventSink)
                mFTPEventSink->OnFTPControlLog(PR_TRUE, mResponseMsg);

        return Process();
        }
    return NS_OK;
}


// nsIRequestObserver implementation
NS_IMETHODIMP
nsFtpState::OnStartRequest(nsIRequest *request, nsISupports *aContext)
{
#if defined(PR_LOGGING)
    nsXPIDLCString spec;
    (void)mURL->GetSpec(getter_Copies(spec));

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) nsFtpState::OnStartRequest() (spec =%s)\n",
        this, NS_STATIC_CAST(const char*, spec)));
#endif
    return NS_OK;
}

NS_IMETHODIMP
nsFtpState::OnStopRequest(nsIRequest *request, nsISupports *aContext,
                            nsresult aStatus)
{
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) nsFtpState::OnStopRequest() rv=%x\n", this, aStatus));
    mControlStatus = aStatus;

    // HACK.  This may not always work.  I am assuming that I can mask the OnStopRequest
    // notification since I created it via the control connection.
    if (mTryingCachedControl && NS_FAILED(aStatus) && NS_SUCCEEDED(mInternalError)) {
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) nsFtpState::OnStopRequest() cache connect failed.  Reconnecting...\n", this, aStatus));
        mTryingCachedControl = PR_FALSE;
        Connect();
        return NS_OK;
    }        

    if (NS_FAILED(aStatus)) // aStatus will be NS_OK if we are sucessfully disconnecing the control connection. 
        StopProcessing();

    return NS_OK;
}

nsresult
nsFtpState::EstablishControlConnection()
{
    nsresult rv;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) trying cached control\n", this));
        
    nsISupports* connection;
    (void) nsFtpProtocolHandler::RemoveConnection(mURL, &connection);
    
    if (connection) {
        mControlConnection = NS_STATIC_CAST(nsFtpControlConnection*, (nsIStreamListener*)connection);
        if (mControlConnection->IsAlive())
        {
            // set stream listener of the control connection to be us.        
            (void) mControlConnection->SetStreamListener(NS_STATIC_CAST(nsIStreamListener*, this));
            
            // read cached variables into us. 
            mServerType = mControlConnection->mServerType;           
            mPassword   = mControlConnection->mPassword;

            mTryingCachedControl = PR_TRUE;
            
            // we're already connected to this server, skip login.
            mState = FTP_S_PASV;
            mResponseCode = 530;  //assume the control connection was dropped.
            mControlStatus = NS_OK;
            mReceivedControlData = PR_FALSE;  // For this request, we have not.

            // if we succeed, return.  Otherwise, we need to 
            // create a transport
            rv = mControlConnection->Connect(mProxyInfo);
            if (NS_SUCCEEDED(rv))
                return rv;
        }
#if defined(PR_LOGGING)
        else 
        {
            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) isAlive return false\n", this));
        }
#endif
    }

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) creating control\n", this));
        
    mState = FTP_READ_BUF;
    mNextState = FTP_S_USER;
    
    nsXPIDLCString host;
    rv = mURL->GetHost(getter_Copies(host));
    if (NS_FAILED(rv)) return rv;

    mControlConnection = new nsFtpControlConnection(host, mPort);
    if (!mControlConnection) return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(mControlConnection);
        
    // Must do it this way 'cuz the channel intercepts the progress notifications.
    (void) mControlConnection->SetStreamListener(NS_STATIC_CAST(nsIStreamListener*, this));

    return mControlConnection->Connect(mProxyInfo);
}

void 
nsFtpState::MoveToNextState(FTP_STATE nextState)
{
    if (NS_FAILED(mInternalError)) 
    {
        mState = FTP_ERROR;
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) FAILED (%x)\n", this, mInternalError));
    }
    else
    {
        mState = FTP_READ_BUF;
        mNextState = nextState;
    }  
}

nsresult
nsFtpState::Process() 
{
    nsresult    rv = NS_OK;
    PRBool      processingRead = PR_TRUE;
    
    while (mKeepRunning && processingRead) 
    {
        switch(mState) 
        {
          case FTP_COMMAND_CONNECT:
              KillControlConnection();
              PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) Establishing control connection...", this));
              mInternalError = EstablishControlConnection();  // sets mState
              if (NS_FAILED(mInternalError)) {
                  mState = FTP_ERROR;
                  PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) FAILED\n", this));
              }
              else
                  PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) SUCCEEDED\n", this));
              break;
    
          case FTP_READ_BUF:
              PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) Waiting for control data (%x)...", this, rv));
              processingRead = PR_FALSE;
              break;
          
          case FTP_ERROR: // xx needs more work to handle dropped control connection cases
              if (mTryingCachedControl && mResponseCode == 530 &&
                  mInternalError == NS_ERROR_FTP_PASV) {
                  // The user was logged out during an pasv operation
                  // we want to restart this request with a new control
                  // channel.
                  mState = FTP_COMMAND_CONNECT;
              }
              else if (mResponseCode == 421 && 
                       mInternalError != NS_ERROR_FTP_LOGIN) {
                  // The command channel dropped for some reason.
                  // Fire it back up, unless we were trying to login
                  // in which case the server might just be telling us
                  // that the max number of users has been reached...
                  mState = FTP_COMMAND_CONNECT;
              } else {
                  PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) FTP_ERROR - Calling StopProcessing\n", this));
                  rv = StopProcessing();
                  NS_ASSERTION(NS_SUCCEEDED(rv), "StopProcessing failed.");
                  processingRead = PR_FALSE;
              }
              break;
          
          case FTP_COMPLETE:
              PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) COMPLETE\n", this));
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
            
            if (NS_FAILED(rv))
                mInternalError = NS_ERROR_FTP_CWD;
            
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
    // some servers on connect send us a 421.  (84525)
    if (mResponseCode == 421)
        return NS_ERROR_FAILURE;

    nsresult rv;
    nsCAutoString usernameStr("USER ");

    if (mAnonymous) {
        usernameStr.Append("anonymous");
    } else {
        if (!mUsername.Length()) {
            if (!mAuthPrompter) return NS_ERROR_NOT_INITIALIZED;
            PRUnichar *user = nsnull, *passwd = nsnull;
            PRBool retval;
            nsXPIDLCString host;
            rv = mURL->GetHost(getter_Copies(host));
            if (NS_FAILED(rv)) return rv;
            nsAutoString hostU; hostU.AppendWithConversion(host);

            nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIStringBundle> bundle;
            rv = bundleService->CreateBundle(NECKO_MSGS_URL, getter_AddRefs(bundle));

            
            nsXPIDLString formatedString;
            const PRUnichar *formatStrings[1] = { hostU.get()};
            rv = bundle->FormatStringFromName(NS_LITERAL_STRING("EnterUserPasswordFor").get(),
                                              formatStrings, 1,
                                              getter_Copies(formatedString));                   

            rv = mAuthPrompter->PromptUsernameAndPassword(nsnull,
                                                          formatedString,
                                                          NS_ConvertASCIItoUCS2(host).get(), // XXX i18n
                                                          nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                                                          &user, 
                                                          &passwd, 
                                                          &retval);

            // if the user canceled or didn't supply a username we want to fail
            if (!retval || (user && !*user) )
                return NS_ERROR_FAILURE;
            mUsername = user;
            mPassword = passwd;
        }
        usernameStr.AppendWithConversion(mUsername);    
    }
    usernameStr.Append(CRLF);

    return SendFTPCommand(usernameStr);
}

FTP_STATE
nsFtpState::R_user() {
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
nsFtpState::S_pass() {
    nsresult rv;
    nsCAutoString passwordStr("PASS ");

    mResponseMsg = "";

    if (mAnonymous) {
        char* anonPassword = nsnull;
        PRBool useRealEmail = PR_FALSE;
        nsCOMPtr<nsIPref> pPref(do_GetService(kPrefCID, &rv));
        if (NS_SUCCEEDED(rv) && pPref) {
            rv = pPref->GetBoolPref("advanced.mailftp", &useRealEmail);
            if (NS_SUCCEEDED(rv) && useRealEmail)
                rv = pPref->CopyCharPref("network.ftp.anonymous_password", &anonPassword);
        }
        if (NS_SUCCEEDED(rv) && useRealEmail && anonPassword && *anonPassword != '\0') {
            passwordStr.Append(anonPassword);
            nsMemory::Free(anonPassword);
        }
        else {
            // We need to default to a valid email address - bug 101027
            // example.com is reserved (rfc2606), so use that
            passwordStr.Append("mozilla@example.com");
        }
    } else {
        if (!mPassword.Length() || mRetryPass) {
            if (!mAuthPrompter) return NS_ERROR_NOT_INITIALIZED;

            PRUnichar *passwd = nsnull;
            PRBool retval;
            
            nsXPIDLCString host;
            rv = mURL->GetHost(getter_Copies(host));
            if (NS_FAILED(rv)) return rv;
            nsAutoString hostU; hostU.AppendWithConversion(host);
            
            nsCOMPtr<nsIStringBundleService> bundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIStringBundle> bundle;
            rv = bundleService->CreateBundle(NECKO_MSGS_URL, getter_AddRefs(bundle));

            nsXPIDLString formatedString;
            const PRUnichar *formatStrings[2] = { mUsername.get(), hostU.get() };
            rv = bundle->FormatStringFromName(NS_LITERAL_STRING("EnterPasswordFor").get(),
                                              formatStrings, 2,
                                              getter_Copies(formatedString)); 

            nsXPIDLCString prePath;
            rv = mURL->GetPrePath(getter_Copies(prePath));
            if (NS_FAILED(rv)) return rv;
            rv = mAuthPrompter->PromptPassword(nsnull,
                                               formatedString,
                                               NS_ConvertASCIItoUCS2(prePath).get(), 
                                               nsIAuthPrompt::SAVE_PASSWORD_PERMANENTLY,
                                               &passwd, &retval);

            // we want to fail if the user canceled or didn't enter a password.
            if (!retval || (passwd && !*passwd) )
                return NS_ERROR_FAILURE;
            mPassword = passwd;
        }
        passwordStr.AppendWithConversion(mPassword);    
    }
    passwordStr.Append(CRLF);

    return SendFTPCommand(passwordStr);
}

FTP_STATE
nsFtpState::R_pass() {
    if (mResponseCode/100 == 3) {
        // send account info
        return FTP_S_ACCT;
    } else if (mResponseCode/100 == 2) {
        // logged in
        return FTP_S_SYST;
    } else if (mResponseCode == 503) {
        // start over w/ the user command.
        // note: the password was successful, and it's stored in mPassword
        mRetryPass = PR_FALSE;
        return FTP_S_USER;
    } else if (mResponseCode == 530) {
        // user limit reached
        return FTP_ERROR;
    } else {
        // kick back out to S_pass() and ask the user again.
        nsresult rv = NS_OK;

        nsCOMPtr<nsIWalletService> walletService = 
                 do_GetService(kWalletServiceCID, &rv);
        if (NS_FAILED(rv)) return FTP_ERROR;

        nsXPIDLCString uri;
        rv = mURL->GetSpec(getter_Copies(uri));
        if (NS_FAILED(rv)) return FTP_ERROR;

        rv = walletService->SI_RemoveUser(uri, nsnull);
        if (NS_FAILED(rv)) return FTP_ERROR;

        mRetryPass = PR_TRUE;
        return FTP_S_PASS;
    }
}

nsresult
nsFtpState::S_syst() {
    nsCString systString("SYST" CRLF);
    return SendFTPCommand( systString );
}

FTP_STATE
nsFtpState::R_syst() {
    if (mResponseCode/100 == 2) {
        if (( mResponseMsg.Find("L8") > -1) || 
            ( mResponseMsg.Find("UNIX") > -1) || 
            ( mResponseMsg.Find("BSD") > -1) )  // non standard response (91019)
        {
            mServerType = FTP_UNIX_TYPE;
        }
        else if ( ( mResponseMsg.Find("WIN32", PR_TRUE) > -1) ||
                  ( mResponseMsg.Find("windows", PR_TRUE) > -1) )
        {
            mServerType = FTP_NT_TYPE;
        }
        else if ( mResponseMsg.Find("OS/2", PR_TRUE) > -1)
        {
            mServerType = FTP_OS2_TYPE;
        }
        else
        {
            NS_ASSERTION(0, "Server type list format unrecognized.");
            // Guessing causes crashes.  
#if DEBUG
            printf("Server listing unrecognized: %s \n", mResponseMsg.get());
#endif
            return FTP_ERROR;
        }

        return FTP_S_TYPE;
    }

    if (mResponseCode/100 == 5) {   
        // server didn't like the SYST command.  Probably (500, 501, 502)
        // No clue.  We will just hope it is UNIX type server.
        mServerType = FTP_UNIX_TYPE;

        return FTP_S_TYPE;
    }
    return FTP_ERROR;
}

nsresult
nsFtpState::S_acct() {
    nsCString acctString("ACCT noaccount" CRLF);
    return SendFTPCommand(acctString);
}

FTP_STATE
nsFtpState::R_acct() {
    if (mResponseCode/100 == 2)
        return FTP_S_SYST;
    else
        return FTP_ERROR;
}

nsresult
nsFtpState::S_type() {
    nsCString typeString("TYPE I" CRLF);
    return SendFTPCommand(typeString);
}

FTP_STATE
nsFtpState::R_type() {
    if (mResponseCode/100 != 2) 
        return FTP_ERROR;
    
    return FTP_S_PASV;
}

nsresult
nsFtpState::S_cwd() {
    nsCAutoString cwdStr("CWD ");
    cwdStr.Append(mPath);
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
    nsCAutoString sizeBuf("SIZE ");
    sizeBuf.Append(mPath);
    sizeBuf.Append(CRLF);

    return SendFTPCommand(sizeBuf);
}

FTP_STATE
nsFtpState::R_size() {
    if (mResponseCode/100 == 2) {
        PRInt32 conversionError;
        PRInt32 length = mResponseMsg.ToInteger(&conversionError);
        if (NS_FAILED(mChannel->SetContentLength(length))) return FTP_ERROR;
    }

    if (mResponseCode == 550) // File unavailable (e.g., file not found, no access).
        return FTP_S_RETR;  // Even if the file reports zero size, lets try retr (91292)

    // if we tried downloading this, lets try restarting it...
    if (mDRequestForwarder && mDRequestForwarder->GetBytesTransfered() > 0)
        return FTP_S_REST;
    
    return FTP_S_RETR;
}

nsresult 
nsFtpState::SetContentType()
{
    if (mGenerateRawContent) {
        nsAutoString fromStr(NS_LITERAL_STRING("text/ftp-dir-"));
        SetDirMIMEType(fromStr);

        nsCAutoString contentType;contentType.AssignWithConversion(fromStr);
        return mChannel->SetContentType(contentType);
    }

    if (mGenerateHTMLContent)
        return mChannel->SetContentType("text/html");
    
    return mChannel->SetContentType("application/http-index-format");
}

nsresult
nsFtpState::S_list() {
    nsresult rv;

    if (!mDRequestForwarder) 
        return NS_ERROR_FAILURE;

    rv = SetContentType();
    
    if (NS_FAILED(rv)) 
        return FTP_ERROR;
    
    // save off the server type if we are caching.
    if(mCacheEntry) {
        nsCAutoString serverType;
        serverType.AppendInt(mServerType);
        (void) mCacheEntry->SetMetaDataElement("servertype", serverType);
    }

    nsCOMPtr<nsIStreamListener> converter;
    
    rv = BuildStreamConverter(getter_AddRefs(converter));
    if (NS_FAILED(rv)) {
        // clear mResponseMsg which is displayed to the user.
        // TODO: we should probably set this to something 
        // meaningful.
        mResponseMsg = "";
        return rv;
    }
    
    // the data forwarder defaults to sending notifications
    // to the channel.  Lets hijack and send the notifications
    // to the stream converter.
    mDRequestForwarder->SetStreamListener(converter);
    mDRequestForwarder->SetCacheEntry(mCacheEntry, PR_TRUE);

    nsCAutoString listString("LIST" CRLF);

    return SendFTPCommand(listString);
}

FTP_STATE
nsFtpState::R_list() {
    if (mResponseCode/100 == 1) 
        return FTP_READ_BUF;

    if (mResponseCode/100 == 2) {
        //(DONE)
        mNextState = FTP_COMPLETE;
        return FTP_COMPLETE;
    }

    mFireCallbacks = PR_TRUE;
    return FTP_ERROR;
}

nsresult
nsFtpState::S_retr() {
    nsresult rv = NS_OK;
    nsCAutoString retrStr("RETR ");
    retrStr.Append(mPath);
    retrStr.Append(CRLF);
    
    if (!mDRequestForwarder)
        return NS_ERROR_FAILURE;
    
    rv = SendFTPCommand(retrStr);
    return rv;
}

FTP_STATE
nsFtpState::R_retr() {

    if (mResponseCode/100 == 2) {
        //(DONE)
        mNextState = FTP_COMPLETE;
        return FTP_COMPLETE;
    }

    if (mResponseCode/100 == 1) 
         return FTP_READ_BUF;
    
    // These error codes are related to problems with the connection.  
    // If we encounter any at this point, do not try CWD and abort.
    if (mResponseCode == 421 || mResponseCode == 425 || mResponseCode == 426)
        return FTP_ERROR;

    if (mResponseCode/100 == 5) {
        mRETRFailed = PR_TRUE;
        
        // We need to kill off the existing connection - see bug 101128
        mDRequestForwarder->RetryConnection();
        nsCOMPtr<nsISocketTransport> st = do_QueryInterface(mDPipe);
        if (st)
            st->SetReuseConnection(PR_FALSE);
        mDPipe = 0;

        return FTP_S_PASV;
    }

    return FTP_S_CWD;
}


nsresult
nsFtpState::S_rest() {
    
    nsCAutoString restString("REST ");
    restString.AppendInt(mDRequestForwarder->GetBytesTransfered(), 10);
    restString.Append(CRLF);

    return SendFTPCommand(restString);
}

FTP_STATE
nsFtpState::R_rest() {
    // only if something terrible happens do we want to error out in this state.
    if (mResponseCode/100 == 4)
        return FTP_ERROR;
   
    return FTP_S_RETR; 
}

nsresult
nsFtpState::S_stor() {
    nsresult rv = NS_OK;
    nsCAutoString storStr("STOR ");
    storStr.Append(mPath.get());
    storStr.Append(CRLF);

    rv = SendFTPCommand(storStr);
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(mWriteStream, "we're trying to upload without any data");
    if (!mWriteStream)
        return NS_ERROR_FAILURE;

    PRUint32 len;
    mWriteStream->Available(&len);
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) writing on Data Transport\n", this));
    return NS_AsyncWriteFromStream(getter_AddRefs(mDPipeRequest), 
                                   mDPipe, 
                                   mWriteStream, 0, len, 0, mDRequestForwarder);
}

FTP_STATE
nsFtpState::R_stor() {
   if (mResponseCode/100 == 2) {
        //(DONE)
        mNextState = FTP_COMPLETE;
        return FTP_COMPLETE;
    }

   if (mResponseCode/100 == 1) {
        return FTP_READ_BUF;
   }  
 
   return FTP_ERROR;
}


nsresult
nsFtpState::S_pasv() {
    nsresult rv;

    if (mIPv6Checked == PR_FALSE) {
        // Find IPv6 socket address, if server is IPv6
        mIPv6Checked = PR_TRUE;
        PR_ASSERT(mIPv6ServerAddress == 0);
        nsCOMPtr<nsITransport> controlSocket;
        mControlConnection->GetTransport(getter_AddRefs(controlSocket));
        if (!controlSocket) return FTP_ERROR;
        nsCOMPtr<nsISocketTransport> sTrans = do_QueryInterface(controlSocket, &rv);
        
        if (sTrans) {
            rv = sTrans->GetIPStr(100, &mIPv6ServerAddress);
        }

        if (NS_SUCCEEDED(rv)) {
            PRNetAddr addr;
            if (PR_StringToNetAddr(mIPv6ServerAddress, &addr) != PR_SUCCESS ||
                PR_IsNetAddrType(&addr, PR_IpAddrV4Mapped)) {
                nsMemory::Free(mIPv6ServerAddress);
                mIPv6ServerAddress = 0;
            }
            PR_ASSERT(!mIPv6ServerAddress || addr.raw.family == PR_AF_INET6);
        }
    }


    char * string;
    if (mIPv6ServerAddress)
        string = "EPSV" CRLF;
    else
        string = "PASV" CRLF;

    nsCString pasvString(string);
    return SendFTPCommand(pasvString);
    
}

FTP_STATE
nsFtpState::R_pasv() {
    nsresult rv;
    PRInt32 port;

    if (mResponseCode/100 != 2)  {
        return FTP_ERROR;
    }

    char *response = ToNewCString(mResponseMsg);
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

    const char* hostStr = mIPv6ServerAddress ? mIPv6ServerAddress : host.get();

    // now we know where to connect our data channel
    nsCOMPtr<nsISocketTransportService> sts = do_GetService(kSocketTransportServiceCID, &rv);

    rv =  sts->CreateTransport(hostStr, 
                               port, 
                               mProxyInfo,
                               FTP_DATA_CHANNEL_SEG_SIZE, 
                               FTP_DATA_CHANNEL_MAX_SIZE, 
                               getter_AddRefs(mDPipe)); // the data channel
    if (NS_FAILED(rv)) return FTP_ERROR;

    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) Created Data Transport (%s:%x)\n", this, hostStr, port));

    nsCOMPtr<nsISocketTransport> sTrans = do_QueryInterface(mDPipe, &rv);
    if (NS_FAILED(rv)) return FTP_ERROR;

    if (NS_FAILED(sTrans->SetReuseConnection(PR_TRUE))) return FTP_ERROR;

    if (!mDRequestForwarder) {
        mDRequestForwarder = new DataRequestForwarder;
        if (!mDRequestForwarder) return FTP_ERROR;
        NS_ADDREF(mDRequestForwarder);
    
        rv = mDRequestForwarder->Init(mChannel);
        if (NS_FAILED(rv)){
            PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) forwarder->Init failed (rv=%x)\n", this, rv));
            return FTP_ERROR;
        }
    }

    // hook ourself up as a proxy for progress notifications
    mWaitingForDConn = PR_TRUE;
    rv = mDPipe->SetNotificationCallbacks(NS_STATIC_CAST(nsIInterfaceRequestor*, mDRequestForwarder), PR_FALSE);
    if (NS_FAILED(rv)){
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) forwarder->SetNotificationCallbacks failed (rv=%x)\n", this, rv));
        return FTP_ERROR;
    }

    if (mAction == PUT) {
        NS_ASSERTION(!mRETRFailed, "Failed before uploading");
        mDRequestForwarder->Uploading(PR_TRUE);
        return FTP_S_STOR;
    }
   
    rv = mDPipe->AsyncRead(mDRequestForwarder, nsnull, 0, PRUint32(-1), 0, getter_AddRefs(mDPipeRequest));    
    if (NS_FAILED(rv)){
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) forwarder->AsyncRead failed (rv=%x)\n", this, rv));
        return FTP_ERROR;
    }

    if (mRETRFailed)
        return FTP_S_CWD;

    return FTP_S_SIZE;
}


////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsFtpState::GetName(PRUnichar* *result)
{
    nsresult rv;
    nsXPIDLCString urlStr;
    rv = mURL->GetSpec(getter_Copies(urlStr));
    if (NS_FAILED(rv)) return rv;
    nsString name;
    name.AppendWithConversion(urlStr);
    *result = ToNewUnicode(name);
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsFtpState::IsPending(PRBool *result)
{
    nsresult rv = NS_OK;
    *result = PR_FALSE;
    
    nsCOMPtr<nsIRequest> request;
    mControlConnection->GetReadRequest(getter_AddRefs(request));

    if (request) {
        rv = request->IsPending(result);
        if (NS_FAILED(rv)) return rv;
    }
    return rv;
}

NS_IMETHODIMP
nsFtpState::GetStatus(nsresult *status)
{
    *status = mInternalError;
    return NS_OK;
}

NS_IMETHODIMP
nsFtpState::Cancel(nsresult status)
{
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) nsFtpState::Cancel() rv=%x\n", this, status));

    // we should try to recover the control connection....
    if (NS_SUCCEEDED(mControlStatus))
        mControlStatus = status;

    // kill the data connection immediately. 
    NS_IF_RELEASE(mDRequestForwarder);
    if (mDPipeRequest) {
        mDPipeRequest->Cancel(status);
        mDPipeRequest = 0;
    }

    (void) StopProcessing();   
    return NS_OK;
}

NS_IMETHODIMP
nsFtpState::Suspend(void)
{
    nsresult rv = NS_OK;
    
    if (!mControlConnection)
        return NS_ERROR_FAILURE;

    // suspending the underlying socket transport will
    // cause the FTP state machine to "suspend" when it
    // tries to use the transport. May not be granular 
    // enough.
    if (mSuspendCount < 1) {
        mSuspendCount++;

        // only worry about the read request.
        nsCOMPtr<nsIRequest> request;
        mControlConnection->GetReadRequest(getter_AddRefs(request));

        if (request) {
            rv = request->Suspend();
            if (NS_FAILED(rv)) return rv;
        }

        if (mDPipeRequest) {
            rv = mDPipeRequest->Suspend();
            if (NS_FAILED(rv)) return rv;
        }
    }

    return rv;
}

NS_IMETHODIMP
nsFtpState::Resume(void)
{
    nsresult rv = NS_ERROR_FAILURE;
    
    // resuming the underlying socket transports will
    // cause the FTP state machine to unblock and 
    // go on about it's business.
    if (mSuspendCount) {

        PRBool dataAlive = PR_FALSE;

        nsCOMPtr<nsISocketTransport> dataSocket = do_QueryInterface(mDPipe);
        if (dataSocket) 
            dataSocket->IsAlive(0, &dataAlive);            
            
        if (dataSocket && dataAlive && mControlConnection->IsAlive())
        {
            nsCOMPtr<nsIRequest> controlRequest;
            mControlConnection->GetReadRequest(getter_AddRefs(controlRequest));
            NS_ASSERTION(controlRequest, "where did my request go!");
            
            controlRequest->Resume();
            rv = mDPipeRequest->Resume();
        }
        else
        {
            // control or data connection went down.  need to reconnect.  
            // if we were downloading, we need to perform REST.
            rv = EstablishControlConnection();
        }
    }
    mSuspendCount--;
    return rv;
}

NS_IMETHODIMP
nsFtpState::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
    *aLoadGroup = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsFtpState::SetLoadGroup(nsILoadGroup *aLoadGroup)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFtpState::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
    *aLoadFlags = nsIRequest::LOAD_NORMAL;
    return NS_OK;
}

NS_IMETHODIMP
nsFtpState::SetLoadFlags(nsLoadFlags aLoadFlags)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsFtpState::Init(nsIFTPChannel* aChannel,
                 nsIPrompt*  aPrompter,
                 nsIAuthPrompt* aAuthPrompter,
                 nsIFTPEventSink* sink,
                 nsICacheEntryDescriptor* cacheEntry,
                 nsIProxyInfo* proxyInfo) 
{
    nsresult rv = NS_OK;

    mKeepRunning = PR_TRUE;
    mPrompter = aPrompter;
    mFTPEventSink = sink;
    mAuthPrompter = aAuthPrompter;
    mCacheEntry = cacheEntry;
    mProxyInfo = proxyInfo;
    
    // parameter validation
    NS_ASSERTION(aChannel, "FTP: needs a channel");

    mChannel = aChannel; // a straight com ptr to the channel

    rv = aChannel->GetURI(getter_AddRefs(mURL));
    if (NS_FAILED(rv)) return rv;
        
    if (mCacheEntry) {
        nsCacheAccessMode access;
        mCacheEntry->GetAccessGranted(&access);
        if (access & nsICache::ACCESS_READ) {
            
            // make sure the channel knows wassup
            SetContentType();
            
            NS_ASSERTION(!mDRequestForwarder, "there should not be a data forwarder");
            mDRequestForwarder = new DataRequestForwarder;
            if (!mDRequestForwarder) return NS_ERROR_OUT_OF_MEMORY;
            NS_ADDREF(mDRequestForwarder);
    
            rv = mDRequestForwarder->Init(mChannel);
            
            nsXPIDLCString serverType;
            (void) mCacheEntry->GetMetaDataElement("servertype", getter_Copies(serverType));
            nsCAutoString serverNum(serverType.get());
            PRInt32 err;
            mServerType = serverNum.ToInteger(&err);
            
            nsCOMPtr<nsIStreamListener> converter;
            rv = BuildStreamConverter(getter_AddRefs(converter));
            if (NS_FAILED(rv)) return rv;
                
            mDRequestForwarder->SetStreamListener(converter);
            mDRequestForwarder->SetCacheEntry(mCacheEntry, PR_FALSE);

            // Get a transport to the cached data...
            nsCOMPtr<nsITransport> transport;
            rv = mCacheEntry->GetTransport(getter_AddRefs(transport));
            if (NS_FAILED(rv)) return rv;

            // Pump the cache data downstream
            return transport->AsyncRead(mDRequestForwarder, 
                                        nsnull,
                                        0, PRUint32(-1), 0,
                                        getter_AddRefs(mDPipeRequest));
        }
    }
    char *path = nsnull;
    nsCOMPtr<nsIURL> aURL(do_QueryInterface(mURL));
    
    if (aURL)
        rv = aURL->GetFilePath(&path);
    else
        rv = mURL->GetPath(&path);
    
    if (NS_FAILED(rv)) return rv;
    mPath.Adopt(nsUnescape(path));

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

    return NS_OK;
}

nsresult
nsFtpState::Connect()
{
    if (mDRequestForwarder)
        return NS_OK;  // we are already connected.
    
    nsresult rv;

    mState = FTP_COMMAND_CONNECT;
    mNextState = FTP_S_USER;

    rv = Process();

    // check for errors.
    if (NS_FAILED(rv)) {
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("-- Connect() on Control Connect FAILED with rv = %x\n", rv));
        mInternalError = NS_ERROR_FAILURE;
        mState = FTP_ERROR;
    }

    return rv;
}
nsresult
nsFtpState::SetWriteStream(nsIInputStream* aInStream) {
    if (!aInStream)
        return NS_OK;

    mAction = PUT;
    mWriteStream = aInStream;
    return NS_OK;
}

void
nsFtpState::KillControlConnection() {
    mControlReadContinue = PR_FALSE;
    mControlReadBrokenLine = PR_FALSE;
    mControlReadCarryOverBuf.Truncate(0);

    if (mDPipe) {
        mDPipe->SetNotificationCallbacks(nsnull, PR_FALSE);
        mDPipe = 0;
    }
    
    NS_IF_RELEASE(mDRequestForwarder);

    mIPv6Checked = PR_FALSE;
    if (mIPv6ServerAddress) {
        nsMemory::Free(mIPv6ServerAddress);
        mIPv6ServerAddress = 0;
    }
    // if everything went okay, save the connection. 
    // FIX: need a better way to determine if we can cache the connections.
    //      there are some errors which do not mean that we need to kill the connection
    //      e.g. fnf.

    if (!mControlConnection)
        return;

    // kill the reference to ourselves in the control connection.
    (void) mControlConnection->SetStreamListener(nsnull);

    if (FTP_CACHE_CONTROL_CONNECTION && 
        NS_SUCCEEDED(mInternalError) &&
        NS_SUCCEEDED(mControlStatus) &&
        mControlConnection->IsAlive()) {

        PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("(%x) nsFtpState caching control connection", this));

        // Store connection persistant data
        mControlConnection->mServerType = mServerType;           
        mControlConnection->mPassword = mPassword;
        nsresult rv = nsFtpProtocolHandler::InsertConnection(mURL, 
                                           NS_STATIC_CAST(nsISupports*, (nsIStreamListener*)mControlConnection));
        // Can't cache it?  Kill it then.  
        mControlConnection->Disconnect(rv);
    } 
    else
        mControlConnection->Disconnect(NS_BINDING_ABORTED);

    NS_RELEASE(mControlConnection);
 
    return;
}

nsresult
nsFtpState::StopProcessing() {
    nsresult rv;
    PR_LOG(gFTPLog, PR_LOG_ALWAYS, ("(%x) nsFtpState stopping", this));

#ifdef DEBUG_dougt
    printf("FTP Stopped: [response code %d] [response msg follows:]\n%s\n", mResponseCode, mResponseMsg.get());
#endif

    if ( NS_FAILED(mInternalError) && mResponseMsg.Length()) 
    {
        // check to see if the control status is bad.
        // web shell wont throw an alert.  we better:
        nsAutoString    text;
        text.AssignWithConversion(mResponseMsg);
        
        NS_ASSERTION(mPrompter, "no prompter!");
        if (mPrompter)
            (void) mPrompter->Alert(nsnull, text.get());
    }
    
    nsresult broadcastErrorCode = mControlStatus;
    if ( NS_SUCCEEDED(broadcastErrorCode))
        broadcastErrorCode = mInternalError;

    if (mFireCallbacks && mChannel) {
        nsCOMPtr<nsIStreamListener> channelListener = do_QueryInterface(mChannel);
        nsCOMPtr<nsIRequest> channelRequest = do_QueryInterface(mChannel);
        NS_ASSERTION(channelListener && channelRequest, "ftp channel better have these interfaces");        
    
        nsCOMPtr<nsIStreamListener> asyncListener;
        rv = NS_NewAsyncStreamListener(getter_AddRefs(asyncListener), channelListener, NS_UI_THREAD_EVENTQ);
        if(asyncListener) {
            (void) asyncListener->OnStartRequest(channelRequest, nsnull);
            (void) asyncListener->OnStopRequest(channelRequest, nsnull, broadcastErrorCode);
        }
    }

    // Clean up the event loop
    mKeepRunning = PR_FALSE;

    KillControlConnection();

    nsCOMPtr<nsIProgressEventSink> sink(do_QueryInterface(mChannel));
    if (sink)
        sink->OnStatus(nsnull, nsnull, NS_NET_STATUS_END_FTP_TRANSACTION, nsnull);

    // Release the Observers
    mWriteStream = 0;

    mPrompter = 0;
    mAuthPrompter = 0;
    mChannel = 0;
    mProxyInfo = 0;

    return NS_OK;
}

void
nsFtpState::SetDirMIMEType(nsString& aString) {
    // the from content type is a string of the form
    // "text/ftp-dir-SERVER_TYPE" where SERVER_TYPE represents the server we're talking to.
    switch (mServerType) {
    case FTP_UNIX_TYPE:
        aString.Append(NS_LITERAL_STRING("unix"));
        break;
    case FTP_NT_TYPE:
        aString.Append(NS_LITERAL_STRING("nt"));
        break;
    case FTP_OS2_TYPE:
        aString.Append(NS_LITERAL_STRING("os2"));
        break;
    default:
        aString.Append(NS_LITERAL_STRING("generic"));
    }
}

nsresult
nsFtpState::BuildStreamConverter(nsIStreamListener** convertStreamListener)
{
    nsresult rv;
    // setup a listener to push the data into. This listener sits inbetween the
    // unconverted data of fromType, and the final listener in the chain (in this case
    // the mListener).
    nsCOMPtr<nsIStreamListener> converterListener;
    nsCOMPtr<nsIStreamListener> listener = do_QueryInterface(mChannel);

    nsCOMPtr<nsIStreamConverterService> scs = 
             do_GetService(kStreamConverterServiceCID, &rv);

    if (NS_FAILED(rv)) 
        return rv;

    nsAutoString fromStr(NS_LITERAL_STRING("text/ftp-dir-"));
    SetDirMIMEType(fromStr);
    if (mGenerateRawContent) {
        converterListener = listener;
    }
    else if (mGenerateHTMLContent) {
        rv = scs->AsyncConvertData(fromStr.get(), 
                                   NS_LITERAL_STRING("text/html").get(),
                                   listener, 
                                   mURL, 
                                   getter_AddRefs(converterListener));
    } 
    else 
    {
        rv = scs->AsyncConvertData(fromStr.get(), 
                                   NS_LITERAL_STRING(APPLICATION_HTTP_INDEX_FORMAT).get(),
                                   listener, 
                                   mURL, 
                                   getter_AddRefs(converterListener));
    }

    if (NS_FAILED(rv)) {
        PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) scs->AsyncConvertData failed (rv=%x)\n", this, rv));
        return rv;
    }

    NS_ADDREF(*convertStreamListener = converterListener);
    return rv;
}

nsresult
nsFtpState::DataConnectionEstablished()
{
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x) Data Connection established.", this));
    
    mFireCallbacks  = PR_FALSE; // observer callbacks will be handled by the transport.
    mWaitingForDConn= PR_FALSE;

    // sending empty string with (mWaitingForDConn == PR_FALSE) will cause the 
    // control socket to write out its buffer.
    nsCString a("");
    SendFTPCommand(a);
    
    return NS_OK;
}

nsresult 
nsFtpState::SendFTPCommand(nsCString& command)
{
    NS_ASSERTION(mControlConnection, "null control connection");        
    
    // we don't want to log the password:
    nsCAutoString logcmd(command);
    if (command.CompareWithConversion("PASS ", PR_FALSE, 5) == 0) 
        logcmd = "PASS xxxxx";
    
    PR_LOG(gFTPLog, PR_LOG_DEBUG, ("(%x)(dwait=%d) Writing \"%s\"\n", this, mWaitingForDConn, logcmd.get()));
    if (mFTPEventSink)
        mFTPEventSink->OnFTPControlLog(PR_FALSE, logcmd.get());
    
    if (mControlConnection) {
        return mControlConnection->Write(command, mWaitingForDConn);
    }
    return NS_ERROR_FAILURE;
}

