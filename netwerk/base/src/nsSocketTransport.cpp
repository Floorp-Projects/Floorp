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

#include "nspr.h"

#include "nsCRT.h"
#include "nsIServiceManager.h"
#include "nscore.h"
#include "netCore.h"
#include "nsIStreamListener.h"
#include "nsILoadGroup.h"
#include "nsSocketTransport.h"
#include "nsSocketTransportService.h"
#include "nsAutoLock.h"
#include "nsIDNSService.h"
#include "nsISocketProvider.h"
#include "nsISocketProviderService.h"
#include "nsStdURL.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProxyObjectManager.h"
#include "nsXPIDLString.h"
#include "nsNetUtil.h"
#include "nsISSLSocketControl.h"
#include "nsIChannelSecurityInfo.h"
#include "nsMemory.h"

#if defined(PR_LOGGING)
static PRLogModuleInfo *gSocketTransportLog = nsnull;
#endif

#define LOG(args) PR_LOG(gSocketTransportLog, PR_LOG_DEBUG, args)

static NS_DEFINE_CID(kSocketProviderService, NS_SOCKETPROVIDERSERVICE_CID);
static NS_DEFINE_CID(kDNSService, NS_DNSSERVICE_CID);
static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);

//
//----------------------------------------------------------------------------
// nsSocketTransport
//----------------------------------------------------------------------------
//

//
// This is the State table which maps current state to next state
// for each socket operation...
//
nsSocketState gStateTable[eSocketOperation_Max][eSocketState_Max] = {
    // eSocketOperation_None:
    {
        eSocketState_Error,         // Created        -> Error
        eSocketState_Error,         // WaitDNS        -> Error
        eSocketState_Error,         // Closed         -> Error
        eSocketState_Error,         // WaitConnect    -> Error
        eSocketState_Error,         // Connected      -> Error
        eSocketState_Error,         // WaitReadWrite  -> Error
        eSocketState_Error,         // Done           -> Error
        eSocketState_Error,         // Timeout        -> Error
        eSocketState_Error          // Error          -> Error
    },
    // eSocketOperation_Connect:
    {
        eSocketState_WaitDNS,       // Created        -> WaitDNS
        eSocketState_Closed,        // WaitDNS        -> Closed
        eSocketState_WaitConnect,   // Closed         -> WaitConnect
        eSocketState_Connected,     // WaitConnect    -> Connected
        eSocketState_Connected,     // Connected      -> Done
        eSocketState_Error,         // WaitReadWrite  -> Error
        eSocketState_Connected,     // Done           -> Connected
        eSocketState_Error,         // Timeout        -> Error
        eSocketState_Closed         // Error          -> Closed
    },
    // eSocketOperation_ReadWrite:
    {
        eSocketState_WaitDNS,       // Created        -> WaitDNS
        eSocketState_Closed,        // WaitDNS        -> Closed
        eSocketState_WaitConnect,   // Closed         -> WaitConnect
        eSocketState_Connected,     // WaitConnect    -> Connected
        eSocketState_WaitReadWrite, // Connected      -> WaitReadWrite
        eSocketState_Done,          // WaitReadWrite  -> Done
        eSocketState_Connected,     // Done           -> Connected
        eSocketState_Error,         // Timeout        -> Error
        eSocketState_Closed         // Error          -> Closed
    },
};

//
// This is the timeout value (in milliseconds) for calls to PR_Connect(...).
// The socket transport thread will block for this amount of time waiting
// for the PR_Connect(...) to complete...
//
// The gConnectTimeout gets initialized the first time a nsSocketTransport
// is created...  This interval is then passed to all PR_Connect() calls...
//
#define CONNECT_TIMEOUT_IN_MS 20

static PRIntervalTime gConnectTimeout  = PR_INTERVAL_NO_WAIT;

#if defined(PR_LOGGING)
static PRUint32 sTotalTransportsCreated = 0;
static PRUint32 sTotalTransportsDeleted = 0;
#endif

nsSocketTransport::nsSocketTransport():
    mSocketTimeout(PR_INTERVAL_NO_TIMEOUT),
    mSocketConnectTimeout(PR_MillisecondsToInterval(DEFAULT_SOCKET_CONNECT_TIMEOUT_IN_MS)),
    mCurrentState(eSocketState_Created),
    mHostName(nsnull),
    mPort(0),
    mMonitor(nsnull),
    mOperation(eSocketOperation_None),
    mProxyPort(0),
    mProxyHost(nsnull),
    mProxyTransparent(PR_FALSE),
    mSSLProxy(PR_FALSE),
    mService(nsnull),
    mReadWriteState(0),
    mSelectFlags(0),
    mStatus(NS_OK),
    mSocketFD(nsnull),
    mSocketTypeCount(0),
    mSocketTypes(nsnull),
    mBytesExpected(-1),
    mReuseCount(0),
    mLastReuseCount(0),
    mBufferSegmentSize(0),
    mBufferMaxSize(0),
    mIdleTimeoutInSeconds(0),
    mBIS(nsnull),
    mBOS(nsnull),
    mReadRequest(nsnull),
    mWriteRequest(nsnull),
    mCloseConnectionOnceDone(PR_FALSE),
    mWasConnected(PR_FALSE)
{
    NS_INIT_REFCNT();

#if defined(PR_LOGGING)
    if (!gSocketTransportLog)
        gSocketTransportLog = PR_NewLogModule("nsSocketTransport");
#endif
    
    PR_INIT_CLIST(&mListLink);
    
    mLastActiveTime  =  PR_INTERVAL_NO_WAIT;
    
    SetReadType (eSocketRead_None);
    SetWriteType(eSocketWrite_None);
    
    //
    // Set up Internet defaults...
    //
    memset(&mNetAddress, 0, sizeof(mNetAddress));
    PR_SetNetAddr(PR_IpAddrAny, PR_AF_INET6, 0, &mNetAddress);
    
    //
    // Initialize the global connect timeout value if necessary...
    //
    if (PR_INTERVAL_NO_WAIT == gConnectTimeout)
        gConnectTimeout  = PR_MillisecondsToInterval(CONNECT_TIMEOUT_IN_MS);
    
    LOG(("nsSocketTransport: Creating [%x], TotalCreated=%d, TotalDeleted=%d\n",
        this, ++sTotalTransportsCreated, sTotalTransportsDeleted));
}


nsSocketTransport::~nsSocketTransport()
{
    LOG(("nsSocketTransport: Deleting [%s:%d %x], TotalCreated=%d, TotalDeleted=%d\n", 
        mHostName, mPort, this, sTotalTransportsCreated, ++sTotalTransportsDeleted));
    
    //
    // Cancel any pending DNS request...
    //
    if (mDNSRequest) {
        mDNSRequest->Cancel(NS_BINDING_ABORTED);
        mDNSRequest = 0;
    }
    
    CloseConnection();
    
    if (mService) {
        PR_AtomicDecrement(&mService->mTotalTransports);
        NS_RELEASE(mService);
    }
    
    CRTFREEIF(mProxyHost);
    CRTFREEIF(mHostName);
    while (mSocketTypeCount) {
        mSocketTypeCount--;
        CRTFREEIF(mSocketTypes[mSocketTypeCount]);
    }
    if (mSocketTypes)
        nsMemory::Free(mSocketTypes);
    
    if (mMonitor) {
        nsAutoMonitor::DestroyMonitor(mMonitor);
        mMonitor = nsnull;
    }
}


nsresult nsSocketTransport::Init(nsSocketTransportService* aService,
                                 const char* aHost, 
                                 PRInt32 aPort,
                                 PRUint32 aSocketTypeCount,
                                 const char* *aSocketTypes,
                                 const char* aProxyHost,
                                 PRInt32 aProxyPort,
                                 PRUint32 bufferSegmentSize,
                                 PRUint32 bufferMaxSize)
{
   nsresult rv = NS_OK;
    
    mBufferSegmentSize = bufferSegmentSize != 0
        ? bufferSegmentSize : NS_SOCKET_TRANSPORT_SEGMENT_SIZE;
    mBufferMaxSize = bufferMaxSize != 0
        ? bufferMaxSize : NS_SOCKET_TRANSPORT_BUFFER_SIZE;
    
    mService = aService;
    NS_ADDREF(mService);
    
    mPort      = aPort;
    mProxyPort = aProxyPort;
    
    if (aHost && *aHost) {
        mHostName = nsCRT::strdup(aHost);
        if (!mHostName)
            rv = NS_ERROR_OUT_OF_MEMORY;
    }
    else // hostname was nsnull or empty...
        rv = NS_ERROR_FAILURE;
    
    if (aProxyHost && *aProxyHost) {
        mProxyHost = nsCRT::strdup(aProxyHost);
        if (!mProxyHost)
            rv = NS_ERROR_OUT_OF_MEMORY;
    }
    
    if (NS_SUCCEEDED(rv) && aSocketTypeCount) {
        mSocketTypes = (char**) nsMemory::Alloc(aSocketTypeCount * sizeof(char*));
        if (!mSocketTypes)
            rv = NS_ERROR_OUT_OF_MEMORY;
        else {
            mSocketTypeCount = 0;
            for (PRUint32 type = 0; type < aSocketTypeCount; type++) {
                const char * socketType = aSocketTypes[type];
                if (socketType == nsnull)
                    continue;
#ifdef DEBUG
                LOG(("nsSocketTransport: pushing io layer: %s\n", socketType));
#endif
                mSocketTypes[mSocketTypeCount] = nsCRT::strdup(socketType);
                if (!mSocketTypes[mSocketTypeCount]) {
                    rv = NS_ERROR_OUT_OF_MEMORY;
                    break;
                }
                
                // increase the count
                mSocketTypeCount++;
                
                if (nsCRT::strcmp(socketType, "socks") == 0) {
                    // for SOCKS proxys, we want to switch some of
                    // the default proxy behavior
                    mProxyTransparent = PR_TRUE;
                }
                if (mProxyHost && (nsCRT::strcmp(socketType, "ssl") == 0))
                    mSSLProxy = PR_TRUE;
            }
        }
    } 
    
    //
    // Create the lock used for synchronizing access to the transport instance.
    //
    if (NS_SUCCEEDED(rv)) {
        mMonitor = nsAutoMonitor::NewMonitor("nsSocketTransport");
        if (!mMonitor)
            rv = NS_ERROR_OUT_OF_MEMORY;
    }
    
    // Update the active time for timeout purposes...
    mLastActiveTime = PR_IntervalNow();
    PR_AtomicIncrement(&mService->mTotalTransports);
    
    LOG(("nsSocketTransport: Initializing [%s:%d %x].  rv = %x",
        mHostName, mPort, this, rv));

    return rv;
}


nsresult nsSocketTransport::CheckForTimeout(PRIntervalTime aCurrentTime)
{
    nsresult rv = NS_OK;
    PRIntervalTime idleInterval;
    
    // Enter the socket transport lock...
    nsAutoMonitor mon(mMonitor);
    
    if (aCurrentTime > mLastActiveTime) {
        idleInterval = aCurrentTime - mLastActiveTime;
    
        if ((mSocketConnectTimeout != PR_INTERVAL_NO_TIMEOUT) && 
            (mCurrentState == eSocketState_WaitConnect) &&
            (idleInterval >= mSocketConnectTimeout)
            ||
            (mSocketTimeout != PR_INTERVAL_NO_TIMEOUT) &&
            (mCurrentState == eSocketState_WaitReadWrite) &&
            (idleInterval >= mSocketTimeout)) 
        {
            LOG(("nsSocketTransport: CheckForTimeout() [%s:%d %x].\t"
               "TIMED OUT... Idle interval: %d\n",
                mHostName, mPort, this, idleInterval));
            
            // Move the transport into the Timeout state...  
            mCurrentState = eSocketState_Timeout;
            rv = NS_ERROR_NET_TIMEOUT;
        }
    }
    return rv;
}


nsresult nsSocketTransport::Process(PRInt16 aSelectFlags)
{
    PRBool done = PR_FALSE;

    //
    // Enter the socket transport lock...  
    // This lock protects access to socket transport member data...
    //
    PR_EnterMonitor(mMonitor);

    LOG(("nsSocketTransport: Entering Process() [host=%s:%d this=%x], "
        "aSelectFlags=%x, CurrentState=%d.\n",
        mHostName, mPort, this, aSelectFlags, mCurrentState));

    if (mOperation == eSocketOperation_None)
        done = PR_TRUE; // nothing to process

    while (!done) {
        //
        // If an error has occurred then move into the error state...
        //
        if (NS_FAILED(mStatus) && (NS_BASE_STREAM_WOULD_BLOCK != mStatus))
            mCurrentState = eSocketState_Error;

        switch (mCurrentState) {
        case eSocketState_Created:
            LOG(("nsSocketTransport: Transport [host=%s:%d this=%x] is in Created state.\n",
                mHostName, mPort, this));
            break;
        case eSocketState_Closed:
            LOG(("nsSocketTransport: Transport [host=%s:%d this=%x] is in Closed state.\n",
                mHostName, mPort, this));
            break;

        case eSocketState_Connected:
            LOG(("nsSocketTransport: Transport [host=%s:%d this=%x] is in Connected state.\n",
                mHostName, mPort, this));

            //
            // A connection has been established with the server
            //
            PR_AtomicIncrement(&mService->mConnectedTransports);
            mWasConnected = PR_TRUE;

            // Send status message
            OnStatus(NS_NET_STATUS_CONNECTED_TO);

            // Initialize select flags
            mSelectFlags = PR_POLL_EXCEPT;

            if (mReadRequest) {
                mSelectFlags |= PR_POLL_READ;
                mReadRequest->OnStart();
                mReadRequest->SetSocket(mSocketFD);
            }
            if (mWriteRequest) {
                mSelectFlags |= PR_POLL_WRITE;
                mWriteRequest->OnStart();
                mWriteRequest->SetSocket(mSocketFD);
            }
            break;
            
        case eSocketState_Error:
            LOG(("nsSocketTransport: Transport [host=%s:%d this=%x] is in Error state.\n", 
                mHostName, mPort, this));

            // Cancel any DNS requests...
            if (mDNSRequest) {
                mDNSRequest->Cancel(NS_BINDING_ABORTED);
                mDNSRequest = 0;
            }

            // Set the error status on the request(s)
            if (mReadRequest)
                mReadRequest->SetStatus(mStatus);
            if (mWriteRequest)
                mWriteRequest->SetStatus(mStatus);

            mCloseConnectionOnceDone = PR_TRUE;

            //
            // Fall into the Done state...
            //
        case eSocketState_Done:
            LOG(("nsSocketTransport: Transport [host=%s:%d this=%x] is in Done state.\n", 
                mHostName, mPort, this));

            if (mReadRequest)
                CompleteAsyncRead();

            if (mWriteRequest)
                CompleteAsyncWrite();

            // Close connection if expected to do so...
            if (mCloseConnectionOnceDone)
                CloseConnection();

            mCurrentState = gStateTable[mOperation][mCurrentState];
            mOperation = eSocketOperation_None;
            mStatus = NS_OK;
            done = PR_TRUE;
            continue;

        case eSocketState_WaitDNS:
            LOG(("nsSocketTransport: Transport [host=%s:%d this=%x] is in WaitDNS state.\n",
                mHostName, mPort, this));
            mStatus = doResolveHost();

            // Send status message
            OnStatus(NS_NET_STATUS_RESOLVING_HOST);
            break;

        case eSocketState_WaitConnect:
            LOG(("nsSocketTransport: Transport [host=%s:%d this=%x] is in WaitConnect state.\n",
                mHostName, mPort, this));
            mStatus = doConnection(aSelectFlags);

            // Send status message
            OnStatus(NS_NET_STATUS_CONNECTING_TO);
            break;

        case eSocketState_WaitReadWrite:
            LOG(("nsSocketTransport: Transport [host=%s:%d this=%x] "
                 "is in WaitReadWrite state [readtype=%x writetype=%x status=%x].\n",
                mHostName, mPort, this, GetReadType(), GetWriteType(), mStatus));
            mStatus = doReadWrite(aSelectFlags);
            break;

        case eSocketState_Timeout:
            LOG(("nsSocketTransport: Transport [host=%s:%d this=%x] is in Timeout state.\n",
                mHostName, mPort, this));
            mStatus = NS_ERROR_NET_TIMEOUT;
            break;

        default:
            NS_ASSERTION(0, "Unexpected state...");
            mStatus = NS_ERROR_FAILURE;
            break;
        }

        //
        // If the current state has successfully completed, then move to the
        // next state for the current operation...
        //
        if (NS_SUCCEEDED(mStatus))
            mCurrentState = gStateTable[mOperation][mCurrentState];
        else if (NS_BASE_STREAM_WOULD_BLOCK == mStatus)
            done = PR_TRUE;

        // 
        // Any select flags are *only* valid the first time through the loop...
        // 
        aSelectFlags = 0;
    }

    // Update the active time for timeout purposes...
    mLastActiveTime  = PR_IntervalNow();

    LOG(("nsSocketTransport: Leaving Process() [host=%s:%d this=%x], mStatus = %x, "
        "CurrentState=%d, mSelectFlags=%x\n\n",
        mHostName, mPort, this, mStatus, mCurrentState, mSelectFlags));

    PR_ExitMonitor(mMonitor);
    return mStatus;
}

nsresult
nsSocketTransport::Cancel(nsresult status)
{
    //
    // Cancel any existing requests
    //
    if (mReadRequest)
        mReadRequest->Cancel(status);
    if (mWriteRequest)
        mWriteRequest->Cancel(status);
    return NS_OK;
}

void
nsSocketTransport::CompleteAsyncRead()
{
    LOG(("nsSocketTransport: CompleteAsyncRead [this=%x]\n", this));

    // order here is not important since we are called from within the monitor
 
    SetFlag(eSocketRead_Done);
    SetReadType(eSocketRead_None);

    mSelectFlags &= ~PR_POLL_READ;
    mBytesExpected = -1;

    mReadRequest->OnStop();
    NS_RELEASE(mReadRequest);
    mReadRequest = nsnull;
}

void
nsSocketTransport::CompleteAsyncWrite()
{
    LOG(("nsSocketTransport: CompleteAsyncWrite [this=%x]\n", this));

    // order here is not important since we are called from within the monitor

    SetFlag(eSocketWrite_Done);
    SetWriteType(eSocketWrite_None);

    mSelectFlags &= ~PR_POLL_WRITE;

    mWriteRequest->OnStop();
    NS_RELEASE(mWriteRequest);
    mWriteRequest = nsnull;
}

//-----
//
//  doResolveHost:
//
//  This method is called while holding the SocketTransport lock.  It is
//  always called on the socket transport thread...
//
//  Return Codes:
//    NS_OK
//    NS_ERROR_HOST_NOT_FOUND
//    NS_ERROR_FAILURE
//
//-----
nsresult nsSocketTransport::doResolveHost(void)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(eSocketState_WaitDNS == mCurrentState, "Wrong state.");

    LOG(("nsSocketTransport: Entering doResolveHost() [host=%s:%d this=%x].\n", 
        mHostName, mPort, this));

    //
    // The hostname has not been resolved yet...
    //
    if (PR_IsNetAddrType(&mNetAddress, PR_IpAddrAny)) {
        //
        // Initialize the port used for the connection...
        //
        // XXX: The list of ports must be restricted - see net_bad_ports_table[] in 
        //      mozilla/network/main/mkconect.c
        //
        mNetAddress.ipv6.port = PR_htons(((mProxyPort != -1 && !mProxyTransparent) ? mProxyPort : mPort));

        NS_WITH_SERVICE(nsIDNSService,
                        pDNSService,
                        kDNSService,
                        &rv);
        if (NS_FAILED(rv)) return rv;

        //
        // Give up the SocketTransport lock.  This allows the DNS thread to call the
        // nsIDNSListener notifications without blocking...
        //
        PR_ExitMonitor(mMonitor);

        rv = pDNSService->Lookup((mProxyHost && !mProxyTransparent) ? mProxyHost : mHostName, 
                                 this, 
                                 nsnull, 
                                 getter_AddRefs(mDNSRequest));
        //
        // Aquire the SocketTransport lock again...
        //
        PR_EnterMonitor(mMonitor);

        if (NS_SUCCEEDED(rv)) {
            //
            // The DNS lookup has finished...  It has either failed or succeeded.
            //
            if (NS_FAILED(mStatus) || !PR_IsNetAddrType(&mNetAddress, PR_IpAddrAny)) {
                mDNSRequest = 0;
                rv = mStatus;
            } 
            //
            // The DNS lookup is being processed...  Mark the transport as waiting
            // until the result is available...
            //
            else {
                SetFlag(eSocketDNS_Wait);
                rv = NS_BASE_STREAM_WOULD_BLOCK;
            }
        }
    }

    LOG(("nsSocketTransport: Leaving doResolveHost() [%s:%d %x].\t"
        "rv = %x.\n\n",
        mHostName, mPort, this, rv));

    return rv;
}

//-----
//
//  doConnection:
//
//  This method is called while holding the SocketTransport lock.  It is
//  always called on the socket transport thread...
//
//  Return values:
//    NS_OK
//    NS_BASE_STREAM_WOULD_BLOCK
//
//    NS_ERROR_CONNECTION_REFUSED
//    NS_ERROR_FAILURE
//    NS_ERROR_OUT_OF_MEMORY
//
//-----
nsresult nsSocketTransport::doConnection(PRInt16 aSelectFlags)
{
    PRStatus status;
    nsresult rv = NS_OK;
    PRBool proxyTransparent = PR_FALSE;

    NS_ASSERTION(eSocketState_WaitConnect == mCurrentState, "Wrong state.");

    LOG(("nsSocketTransport: Entering doConnection() [host=%s:%d this=%x], "
        "aSelectFlags=%x.\n",
        mHostName, mPort, this, aSelectFlags));

    if (!mSocketFD) {
        //
        // Step 1:
        //    Create a new TCP socket structure...
        //
        if (!mSocketTypeCount) 
            mSocketFD = PR_OpenTCPSocket(PR_AF_INET6);
        else {
            NS_WITH_SERVICE(nsISocketProviderService,
                            pProviderService,
                            kSocketProviderService,
                            &rv);

            char * destHost = mHostName;
            PRInt32 destPort = mPort;
            char * proxyHost = mProxyHost;
            PRInt32 proxyPort = mProxyPort;

            for (PRUint32 type = 0; type < mSocketTypeCount; type++) {
                nsCOMPtr<nsISocketProvider> pProvider;
              
                if (NS_SUCCEEDED(rv))
                    rv = pProviderService->GetSocketProvider(mSocketTypes[type], 
                                                           getter_AddRefs(pProvider));
              
                if (!NS_SUCCEEDED(rv)) break;
              
                nsCOMPtr<nsISupports> socketInfo;
              
                if (!type) {
                    // if this is the first type, we'll want the 
                    // service to allocate a new socket
                    rv = pProvider->NewSocket(destHost,
                                              destPort,
                                              proxyHost,
                                              proxyPort,
                                              &mSocketFD, 
                                              getter_AddRefs(socketInfo));
                }
                else {
                    // the socket has already been allocated, 
                    // so we just want the service to add itself
                    // to the stack (such as pushing an io layer)
                    rv = pProvider->AddToSocket(destHost,
                                                destPort,
                                                proxyHost,
                                                proxyPort,
                                                mSocketFD,
                                                getter_AddRefs(socketInfo));
                }
              
                if (!NS_SUCCEEDED(rv) || !mSocketFD) break;

                // if the service was ssl or tls, we want to hold onto the socket info
                if (nsCRT::strcmp(mSocketTypes[type], "ssl") == 0 ||
                    nsCRT::strcmp(mSocketTypes[type], "tls") == 0) {
                    mSecurityInfo = socketInfo;
                }
                else if (nsCRT::strcmp(mSocketTypes[type], "ssl-forcehandshake") == 0) {
                    mSecurityInfo = socketInfo;
                    nsCOMPtr<nsIChannelSecurityInfo> securityInfo = do_QueryInterface(mSecurityInfo, &rv);
                    if (NS_SUCCEEDED(rv) && securityInfo)
                        securityInfo->SetForceHandshake(PR_TRUE);
                }
                else if (nsCRT::strcmp(mSocketTypes[type], "socks") == 0) {
                    // since socks is transparent, any layers above
                    // it do not have to worry about proxy stuff
                    proxyHost = nsnull;
                    proxyPort = -1;
                    proxyTransparent = PR_TRUE;
                }
            }
        }

        if (mSocketFD) {
            PRSocketOptionData opt;

            // Make the socket non-blocking...
            opt.option = PR_SockOpt_Nonblocking;
            opt.value.non_blocking = PR_TRUE;
            status = PR_SetSocketOption(mSocketFD, &opt);
            if (PR_SUCCESS != status)
                rv = NS_ERROR_FAILURE;

            // XXX: Is this still necessary?
#if defined(XP_WIN16) // || (defined(XP_OS2) && !defined(XP_OS2_DOUGSOCK))
            opt.option = PR_SockOpt_Linger;
            opt.value.linger.polarity = PR_TRUE;
            opt.value.linger.linger = PR_INTERVAL_NO_WAIT;
#ifdef XP_OS2
            PR_SetSocketOption(mSocketFD, &opt);
#else
            PR_SetSocketOption(*sock, &opt);
#endif
#endif /* XP_WIN16 || XP_OS2*/
        } 
        else
            rv = NS_ERROR_OUT_OF_MEMORY;

        //
        // Step 2:
        //    Initiate the connect() to the host...  
        //
        //    This is only done the first time doConnection(...) is called.
        //
        if (NS_SUCCEEDED(rv)) {
            status = PR_Connect(mSocketFD, &mNetAddress, gConnectTimeout);
            if (PR_SUCCESS != status) {
                PRErrorCode code = PR_GetError();
                //
                // If the PR_Connect(...) would block, then return WOULD_BLOCK...
                // It is the callers responsibility to place the transport on the
                // select list of the transport thread...
                //
                if ((PR_WOULD_BLOCK_ERROR == code) || 
                    (PR_IN_PROGRESS_ERROR == code)) {
              
                    // Set up the select flags for connect...
                    mSelectFlags = (PR_POLL_READ | PR_POLL_EXCEPT | PR_POLL_WRITE);
                    rv = NS_BASE_STREAM_WOULD_BLOCK;
                } 
                //
                // If the socket is already connected, then return success...
                //
                else if (PR_IS_CONNECTED_ERROR == code)
                    rv = NS_OK;
                //
                // The connection was refused...
                //
                else {
                    // Connection refused...
                    LOG(("nsSocketTransport: Connection Refused [%s:%d %x].  PRErrorCode = %x\n",
                        mHostName, mPort, this, code));
                    rv = NS_ERROR_CONNECTION_REFUSED;
                }
            }
        }
    }
    //
    // Step 3:
    //    Process the flags returned by PR_Poll() if any...
    //
    else if (aSelectFlags) {
        if (PR_POLL_EXCEPT & aSelectFlags) {
            LOG(("nsSocketTransport: Connection Refused via PR_POLL_EXCEPT. [%s:%d %x].\n", 
                mHostName, mPort, this));
            rv = NS_ERROR_CONNECTION_REFUSED;
        }
        else if (PR_POLL_HUP & aSelectFlags) {
            LOG(("nsSocketTransport: Connection Refused via PR_POLL_HUP. [%s:%d %x].\n", 
                mHostName, mPort, this));
            rv = NS_ERROR_CONNECTION_REFUSED;
        }
        //
        // The connection was successful...  
        //
        // PR_Poll(...) returns PR_POLL_WRITE to indicate that the connection is
        // established...
        //
        else if (PR_POLL_WRITE & aSelectFlags)
            rv = NS_OK;
    } else
        rv = NS_BASE_STREAM_WOULD_BLOCK;

    LOG(("nsSocketTransport: Leaving doConnection() [%s:%d %x].\t"
        "rv = %x.\n\n",
        mHostName, mPort, this, rv));

    if (rv == NS_OK && mSecurityInfo && mProxyHost && mProxyHost && proxyTransparent) {
        // if the connection phase is finished, and the ssl layer
        // has been pushed, and we were proxying (transparently; ie. nothing
        // has to happen in the protocol layer above us), it's time
        // for the ssl to "step up" and start doing it's thing.
        nsCOMPtr<nsISSLSocketControl> sslControl = do_QueryInterface(mSecurityInfo, &rv);
        if (NS_SUCCEEDED(rv) && sslControl)
            sslControl->ProxyStepUp();
    }
    return rv;
}

nsresult
nsSocketTransport::doBlockingConnection()
{
    nsresult rv = NS_OK;

    //
    // The hostname has not been resolved yet...
    //
    if (PR_IsNetAddrType(&mNetAddress, PR_IpAddrAny)) {
        NS_WITH_SERVICE(nsIDNSService,
                        pDNSService,
                        kDNSService,
                        &rv);
        if (NS_FAILED(rv)) return rv;

        nsXPIDLCString result;
        const char *host = (mProxyHost && !mProxyTransparent) ? mProxyHost : mHostName;
        rv = pDNSService->Resolve(host, getter_Copies(result));
        if (NS_FAILED(rv)) return rv;

        LOG(("nsSocketTransport: doBlockingConnection [this=%x] resolved host=%s ==> %s\n",
            this, host, result.get()));

        PRNetAddr addr;
        PRStatus st = PR_StringToNetAddr(result, &addr);
        if (st != PR_SUCCESS) {
            LOG(("nsSocketTransport: doBlockingConnection [this=%x] host resolution failed\n", this));
            return NS_ERROR_FAILURE;
        }

        if (addr.raw.family == PR_AF_INET)
            PR_ConvertIPv4AddrToIPv6(addr.inet.ip, &mNetAddress.ipv6.ip);
        else
            memcpy(&mNetAddress.ipv6.ip, &addr.ipv6.ip, sizeof(mNetAddress.ipv6.ip));

        mNetAddress.ipv6.port
            = PR_htons(((mProxyPort != -1 && !mProxyTransparent) ? mProxyPort : mPort));

        LOG(("address { family=%hu, port=%hu }\n",
            mNetAddress.ipv6.family, PR_ntohs(mNetAddress.ipv6.port)));
    }

    //
    // The connection has not been established yet...
    //
    if (!mSocketFD) {
        mCurrentState = eSocketState_WaitConnect;
        rv = doConnection(0);
        if (NS_FAILED(rv)) {
            if (rv != NS_BASE_STREAM_WOULD_BLOCK)
                return rv;
            PRPollDesc pd;
            PRIntervalTime pollTimeout = PR_MillisecondsToInterval(DEFAULT_POLL_TIMEOUT_IN_MS);
            PRInt32 result;
            do {
                pd.fd = mSocketFD;
                pd.in_flags = mSelectFlags;
                pd.out_flags = 0;

                result = PR_Poll(&pd, 1, pollTimeout);
                switch (result) {
                case 0:
                    rv = NS_ERROR_NET_TIMEOUT;
                    break;
                case 1:
                    rv = doConnection(pd.out_flags);
                    break;
                default:
                    rv = NS_ERROR_FAILURE;
                }
            }
            while (rv == NS_BASE_STREAM_WOULD_BLOCK);
        }
        mCurrentState = eSocketState_Connected;
    }

    return rv;
}

nsresult
nsSocketTransport::doReadWrite(PRInt16 aSelectFlags)
{
    nsresult readStatus = NS_OK, writeStatus = NS_OK;

    LOG(("nsSocketTransport: doReadWrite [this=%x, aSelectFlags=%hx, mReadRequest=%x, mWriteRequest=%x",
        this, aSelectFlags, mReadRequest, mWriteRequest));

    if (PR_POLL_EXCEPT & aSelectFlags) {
        LOG(("nsSocketTransport: [this=%x] received PR_POLL_EXCEPT\n", this));
        return NS_BINDING_FAILED;
    }
    if (PR_POLL_ERR & aSelectFlags) {
        LOG(("nsSocketTransport: [this=%x] received PR_POLL_ERR\n", this));
        return NS_BINDING_FAILED;
    }
    if (PR_POLL_HUP & aSelectFlags) {
        LOG(("nsSocketTransport: [this=%x] received PR_POLL_HUP\n", this));
        return NS_OK;
    }

    // Process the read request...
    if (mReadRequest) {
        if (mReadRequest->IsCanceled() || (mBytesExpected == 0))
            mSelectFlags &= ~PR_POLL_READ;
        else if (aSelectFlags & PR_POLL_READ)
            readStatus = mReadRequest->OnRead();
        else
            readStatus = NS_BASE_STREAM_WOULD_BLOCK;

        if (mReadRequest->IsSuspended()) {
            mSelectFlags &= ~PR_POLL_READ;
            readStatus = NS_BASE_STREAM_WOULD_BLOCK;
        }
        else if (NS_SUCCEEDED(readStatus))
            CompleteAsyncRead();
        else if (NS_FAILED(readStatus) && (readStatus != NS_BASE_STREAM_WOULD_BLOCK))
            return readStatus;
    }

    // Process the write request...
    if (mWriteRequest) {
        if (mWriteRequest->IsCanceled())
            mSelectFlags &= ~PR_POLL_WRITE;
        else if (aSelectFlags & PR_POLL_WRITE)
            writeStatus = mWriteRequest->OnWrite();
        else
            writeStatus = NS_BASE_STREAM_WOULD_BLOCK;

        if (mWriteRequest->IsSuspended()) {
            mSelectFlags &= ~PR_POLL_WRITE;
            writeStatus = NS_BASE_STREAM_WOULD_BLOCK;
        }
        else if (NS_SUCCEEDED(writeStatus))
            CompleteAsyncWrite();
        else if (NS_FAILED(writeStatus) && (writeStatus != NS_BASE_STREAM_WOULD_BLOCK))
            return writeStatus;
    }

    LOG(("nsSocketTransport: doReadWrite [readstatus=%x writestatus=%x readsuspend=%d writesuspend=%d mSelectFlags=%hx]\n",
        readStatus, writeStatus,
        mReadRequest ? mReadRequest->IsSuspended() : 0,
        mWriteRequest ? mWriteRequest->IsSuspended() : 0,
        mSelectFlags));

    //
    // If both requests successfully completed then move to the "done" state.
    //
    if (NS_SUCCEEDED(readStatus) && NS_SUCCEEDED(writeStatus))
        return NS_OK;

    // Else remain in the same state
    return NS_BASE_STREAM_WOULD_BLOCK;
}

nsresult nsSocketTransport::CloseConnection(PRBool bNow)
{
    PRStatus status;
    nsresult rv = NS_OK;
    
    if (!bNow) { // close connection once done. 
        mCloseConnectionOnceDone = PR_TRUE;
        return NS_OK;
    }
    
    if (!mSocketFD) {
        mCurrentState = eSocketState_Closed;
        return NS_OK;
    }
    
    status = PR_Close(mSocketFD);
    if (PR_SUCCESS != status)
        rv = NS_ERROR_FAILURE;

    mSocketFD = nsnull;

    if (mWasConnected) {
        if (mService)
            PR_AtomicDecrement(&mService->mConnectedTransports);
        mWasConnected = PR_FALSE;
    }
    
    if (NS_SUCCEEDED(rv))
        mCurrentState = eSocketState_Closed;

    return rv;
}


//
// --------------------------------------------------------------------------
// nsISupports implementation...
// --------------------------------------------------------------------------
//

NS_IMPL_THREADSAFE_ISUPPORTS3(nsSocketTransport, 
                              nsITransport,
                              nsIDNSListener, 
                              nsISocketTransport);

//
// --------------------------------------------------------------------------
// nsISocketTransport implementation...
// --------------------------------------------------------------------------
//

NS_IMETHODIMP
nsSocketTransport::GetHost(char **aHost)
{
    NS_ENSURE_ARG_POINTER(aHost);
    *aHost = nsCRT::strdup(mHostName);
    if (!*aHost)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::GetPort(PRInt32 *aPort)
{
    NS_ENSURE_ARG_POINTER(aPort);
    *aPort = mPort;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::GetReuseConnection(PRBool *_retval)
{
    if (_retval == NULL)
        return NS_ERROR_NULL_POINTER;

    *_retval = !mCloseConnectionOnceDone;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::SetReuseConnection(PRBool aReuse)
{
    mCloseConnectionOnceDone = !aReuse;

    if (aReuse)
        mReuseCount++;

    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::GetReuseCount (PRUint32 * count)
{
    if (count == NULL)
        return NS_ERROR_NULL_POINTER;

    *count = mReuseCount;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::SetReuseCount (PRUint32   count)
{
    mReuseCount = count;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::GetBytesExpected (PRInt32 * bytes)
{
    if (bytes != NULL) {
        *bytes = mBytesExpected;
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSocketTransport::SetBytesExpected (PRInt32 bytes)
{
    nsAutoMonitor mon(mMonitor);

    if (mCurrentState == eSocketState_WaitReadWrite) {
        mBytesExpected = bytes;
        if (mBytesExpected == 0)
            mService->Wakeup(this);
    }
    return NS_OK;
}
       
NS_IMETHODIMP
nsSocketTransport::GetSecurityInfo(nsISupports **info)
{
  *info = mSecurityInfo.get();
  NS_IF_ADDREF(*info);
  return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::GetProgressEventSink(nsIProgressEventSink **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ADDREF(*aResult = mEventSink);
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::SetProgressEventSink(nsIProgressEventSink *aEventSink)
{
    mEventSink = nsnull;
  
    if (aEventSink) {
        nsresult rv;
        // Now generate a proxied event sink-
        NS_WITH_SERVICE(nsIProxyObjectManager, 
                        proxyMgr, kProxyObjectManagerCID, &rv);
        if (NS_SUCCEEDED(rv)) {
            rv = proxyMgr->GetProxyForObject(NS_UI_THREAD_EVENTQ, // primordial thread - should change?
                                             NS_GET_IID(nsIProgressEventSink),
                                             aEventSink,
                                             PROXY_ASYNC | PROXY_ALWAYS,
                                             getter_AddRefs(mEventSink));
        }
    }
    return NS_OK;
}


//
// --------------------------------------------------------------------------
// request helpers...
// --------------------------------------------------------------------------
//

nsresult
nsSocketTransport::GetName(PRUnichar **result)
{
    nsString name;
    name.AppendWithConversion(mHostName);
    name.AppendWithConversion(":");
    name.AppendInt(mPort);
    *result = name.ToNewUnicode();
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult
nsSocketTransport::Dispatch(nsSocketRequest *req)
{
    nsresult rv = NS_OK;

    // Enter the socket transport lock...
    nsAutoMonitor mon(mMonitor);

    if (req == mReadRequest)
        mSelectFlags |= PR_POLL_READ;
    else
        mSelectFlags |= PR_POLL_WRITE;

    //
    // Wake up the transport on the socket transport thread so it can
    // be resumed...
    //
    mLastActiveTime  = PR_IntervalNow();
    rv = mService->AddToWorkQ(this);

    LOG(("nsSocketTransport: Resuming [%s:%d %x].  rv = %x\t",
        mHostName, mPort, this, rv));

    return rv;
}

nsresult
nsSocketTransport::OnProgress(nsSocketRequest *req, nsISupports *ctx, PRUint32 offset)
{
    if (mEventSink)
        // we don't have content length info at the socket level
        // just pass 0 through.
        mEventSink->OnProgress(req, ctx, offset, 0);
    return NS_OK;
}

//
// --------------------------------------------------------------------------
// nsIDNSListener implementation...
// --------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSocketTransport::OnStartLookup(nsISupports *aContext, const char *aHostName)
{
    LOG(("nsSocketTransport: OnStartLookup(...) [%s:%d %x].  Host is %s\n", 
        mHostName, mPort, this, aHostName));

    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::OnFound(nsISupports *aContext, 
                           const char* aHostName,
                           nsHostEnt *aHostEnt) 
{
    // Enter the socket transport lock...
    nsAutoMonitor mon(mMonitor);
    nsresult rv = NS_OK;

    if (aHostEnt->hostEnt.h_addr_list && aHostEnt->hostEnt.h_addr_list[0]) {
        if (aHostEnt->hostEnt.h_addrtype == PR_AF_INET6)
            memcpy(&mNetAddress.ipv6.ip, aHostEnt->hostEnt.h_addr_list[0], sizeof(mNetAddress.ipv6.ip));
        else
            PR_ConvertIPv4AddrToIPv6(*(PRUint32*)aHostEnt->hostEnt.h_addr_list[0], &mNetAddress.ipv6.ip);
#if defined(PR_LOGGING)
        char addrbuf[50];
        PR_NetAddrToString(&mNetAddress, addrbuf, sizeof(addrbuf));
        LOG(("nsSocketTransport: OnFound(...) [%s:%d %x]."
            "  DNS lookup succeeded => %s (%s)\n",
            mHostName, mPort, this,
            aHostEnt->hostEnt.h_name,
            addrbuf));
#endif
    } else {
        // XXX: What should happen here?  The GetHostByName(...) succeeded but 
        //      there are *no* A records...
        rv = NS_ERROR_FAILURE;

        LOG(("nsSocketTransport: OnFound(...) [%s:%d %x]."
            "  DNS lookup succeeded (%s) but no address returned!",
            mHostName, mPort, this,
            aHostEnt->hostEnt.h_name));
    }

    return rv;
}

NS_IMETHODIMP
nsSocketTransport::OnStopLookup(nsISupports *aContext,
                                const char *aHostName,
                                nsresult aStatus)
{
    // Enter the socket transport lock...
    nsAutoMonitor mon(mMonitor);

    LOG(("nsSocketTransport: OnStopLookup(...) [%s:%d %x]."
        "  Status = %x Host is %s\n", 
        mHostName, mPort, this, aStatus, aHostName));

    // Release our reference to the DNS Request...
    mDNSRequest = 0;

    // If the lookup failed, set the status...
    if (NS_FAILED(aStatus))
        mStatus = aStatus;

    // Start processing the transport again - if necessary...
    if (GetFlag(eSocketDNS_Wait)) {
        ClearFlag(eSocketDNS_Wait);
        mLastActiveTime = PR_IntervalNow();
        mService->AddToWorkQ(this);
    }

    return NS_OK;
}


//
// --------------------------------------------------------------------------
// nsITransport implementation...
// --------------------------------------------------------------------------
//

NS_IMETHODIMP
nsSocketTransport::AsyncRead(nsIStreamListener* aListener,
                             nsISupports* aContext,
                             PRUint32 aOffset,
                             PRUint32 aCount,
                             PRUint32 aFlags,
                             nsIRequest **aRequest)
{
    NS_ENSURE_ARG_POINTER(aRequest);
    nsresult rv = NS_OK;
    
    // Enter the socket transport lock...
    nsAutoMonitor mon(mMonitor);
    
    LOG(("nsSocketTransport: Entering AsyncRead() [host=%s:%d this=%x]\n", 
        mHostName, mPort, this));
    
    if (GetReadType() != eSocketRead_None)
        rv = NS_ERROR_IN_PROGRESS;
    
    nsCOMPtr<nsIStreamListener> listener;

    if (NS_SUCCEEDED(rv))
        rv = NS_NewStreamListenerProxy(getter_AddRefs(listener),
                                       aListener, nsnull,
                                       mBufferSegmentSize,
                                       mBufferMaxSize);

    if (NS_SUCCEEDED(rv)) {
        NS_NEWXPCOM(mReadRequest, nsSocketReadRequest);
        if (mReadRequest) {
            NS_ADDREF(mReadRequest);
            mReadRequest->SetTransport(this);
            mReadRequest->SetListener(listener);
            mReadRequest->SetListenerContext(aContext);
            //mReadRequest->SetTransferCount(aCount);
        }
        else
            rv = NS_ERROR_OUT_OF_MEMORY;
    }
    
    if (NS_SUCCEEDED(rv)) {
        mOperation = eSocketOperation_ReadWrite;
        SetReadType(eSocketRead_Async);
        
        mLastActiveTime = PR_IntervalNow();
        rv = mService->AddToWorkQ(this);
    }
    
    LOG(("nsSocketTransport: Leaving AsyncRead() [host=%s:%d this=%x], rv=%x.\n",
        mHostName, mPort, this, rv));
    
    NS_IF_ADDREF(*aRequest = mReadRequest);
    return rv;
}


NS_IMETHODIMP
nsSocketTransport::AsyncWrite(nsIStreamProvider* aProvider,
                              nsISupports* aContext,
                              PRUint32 aOffset,
                              PRUint32 aCount,
                              PRUint32 aFlags,
                              nsIRequest **aRequest)
{
    NS_ENSURE_ARG_POINTER(aRequest);
    nsresult rv = NS_OK;

    // Enter the socket transport lock...
    nsAutoMonitor mon(mMonitor);

    LOG(("nsSocketTransport: Entering AsyncWrite() [host=%s:%d this=%x]\n", 
        mHostName, mPort, this));
    
    // If a write is already in progress then fail...
    if (GetWriteType() != eSocketWrite_None)
        rv = NS_ERROR_IN_PROGRESS;

    nsCOMPtr<nsIStreamProvider> provider;
    
    if (NS_SUCCEEDED(rv))
        rv = NS_NewStreamProviderProxy(getter_AddRefs(provider),
                                       aProvider, nsnull,
                                       mBufferSegmentSize,
                                       mBufferMaxSize);

    if (NS_SUCCEEDED(rv)) {
        NS_NEWXPCOM(mWriteRequest, nsSocketWriteRequest);
        if (mWriteRequest) {
            NS_ADDREF(mWriteRequest);
            mWriteRequest->SetTransport(this);
            mWriteRequest->SetProvider(provider);
            mWriteRequest->SetProviderContext(aContext);
            //mWriteRequest->SetTransferCount(aCount);
        }
        else
            rv = NS_ERROR_OUT_OF_MEMORY;
    }
    
    if (NS_SUCCEEDED(rv)) {
        mOperation = eSocketOperation_ReadWrite;
        SetWriteType(eSocketWrite_Async);
        
        mLastActiveTime = PR_IntervalNow ();
        rv = mService->AddToWorkQ(this);
    }
    
    LOG(("nsSocketTransport: Leaving AsyncWrite() [host=%s:%d this=%x], rv=%x.\n",
        mHostName, mPort, this, rv));
    
    NS_IF_ADDREF(*aRequest = mWriteRequest);
    return rv;
}


NS_IMETHODIMP
nsSocketTransport::OpenInputStream(PRUint32 aOffset,
                                   PRUint32 aCount,
                                   PRUint32 aFlags,
                                   nsIInputStream **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    nsresult rv = NS_OK;

    // Enter the socket transport lock...
    nsAutoMonitor mon(mMonitor);

    LOG(("nsSocketTransport: Entering OpenInputStream() [host=%s:%d this=%x].\n", 
        mHostName, mPort, this));

    // If a read is already in progress then fail...
    if (GetReadType() != eSocketRead_None)
        rv = NS_ERROR_IN_PROGRESS;

    if (NS_SUCCEEDED(rv))
        rv = doBlockingConnection();

    if (NS_SUCCEEDED(rv)) {
        NS_NEWXPCOM(mBIS, nsSocketBIS);
        if (!mBIS)
            rv = NS_ERROR_OUT_OF_MEMORY;
        else {
            NS_ADDREF(mBIS);
            mBIS->SetTransport(this);
            mBIS->SetSocket(mSocketFD);
        }
    }

    if (NS_SUCCEEDED(rv)) {
        mOperation = eSocketOperation_ReadWrite;
        SetReadType(eSocketRead_Sync);

        mLastActiveTime = PR_IntervalNow();
    }

    LOG(("nsSocketTransport: Leaving OpenInputStream() [host=%s:%d this=%x], "
        "rv=%x.\n",
        mHostName, mPort, this, rv));

    NS_IF_ADDREF(*aResult = mBIS);
    return rv;
}


NS_IMETHODIMP
nsSocketTransport::OpenOutputStream(PRUint32 aOffset,
                                    PRUint32 aCount,
                                    PRUint32 aFlags,
                                    nsIOutputStream **aResult)
{
    nsresult rv = NS_OK;

    // Enter the socket transport lock...
    nsAutoMonitor mon(mMonitor);

    LOG(("nsSocketTransport: Entering OpenOutputStream() [host=%s:%d this=%x].\n", 
        mHostName, mPort, this));

    // If a write is already in progress then fail...
    if (GetWriteType() != eSocketWrite_None)
        rv = NS_ERROR_IN_PROGRESS;

    if (NS_SUCCEEDED(rv))
        rv = doBlockingConnection();

    if (NS_SUCCEEDED(rv)) {
        NS_NEWXPCOM(mBOS, nsSocketBOS);
        if (!mBOS)
            rv = NS_ERROR_OUT_OF_MEMORY;
        else {
            NS_ADDREF(mBOS);
            mBOS->SetTransport(this);
            mBOS->SetSocket(mSocketFD);
        }
    }

    if (NS_SUCCEEDED(rv)) {
        mOperation = eSocketOperation_ReadWrite;
        SetWriteType(eSocketWrite_Sync);

        mLastActiveTime = PR_IntervalNow();
    }

    LOG(("nsSocketTransport: Leaving OpenOutputStream() [host=%s:%d this=%x], "
        "rv = %x.\n",
        mHostName, mPort, this, rv));

    NS_IF_ADDREF(*aResult = mBOS);
    return rv;
}

NS_IMETHODIMP
nsSocketTransport::IsAlive (PRUint32 seconds, PRBool *alive)
{
    *alive = PR_TRUE;

    if (mSocketFD) {
        if (mLastActiveTime != PR_INTERVAL_NO_WAIT) {
            PRUint32  now = PR_IntervalToSeconds (PR_IntervalNow ());
            PRUint32 last = PR_IntervalToSeconds ( mLastActiveTime );
            PRUint32 diff = now - last;

            if (seconds && diff > seconds || mIdleTimeoutInSeconds && diff > mIdleTimeoutInSeconds)
                *alive = PR_FALSE;
        }

        static char c;
        PRInt32 rval = PR_Read (mSocketFD, &c, 0);
        
        if (rval < 0) {
            PRErrorCode code = PR_GetError ();

            if (code != PR_WOULD_BLOCK_ERROR)
                *alive = PR_FALSE;
        }
    }
    else
        *alive = PR_FALSE;

    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::GetIdleTimeout (PRUint32* seconds)
{
    if (seconds == NULL)
        return NS_ERROR_NULL_POINTER;

    *seconds = mIdleTimeoutInSeconds;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::SetIdleTimeout (PRUint32  seconds)
{
    mIdleTimeoutInSeconds = seconds;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::GetIPStr(PRUint32 aLen, char **_retval)
{
    NS_ASSERTION(aLen > 0, "caller must pass in str len");
    *_retval = (char*)nsMemory::Alloc(aLen);
    if (!*_retval) return NS_ERROR_FAILURE;

    PRStatus status = PR_NetAddrToString(&mNetAddress, *_retval, aLen);

    if (PR_FAILURE == status) {
        nsMemory::Free(*_retval);
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::GetSocketTimeout (PRUint32 * o_Seconds)
{
    if (o_Seconds == NULL)
        return NS_ERROR_NULL_POINTER;

    if (mSocketTimeout == PR_INTERVAL_NO_TIMEOUT)
        *o_Seconds = 0;
    else
        *o_Seconds = PR_IntervalToSeconds (mSocketTimeout);

    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::SetSocketTimeout (PRUint32  a_Seconds)
{
    if (a_Seconds == 0)
        mSocketTimeout = PR_INTERVAL_NO_TIMEOUT;
    else
        mSocketTimeout = PR_SecondsToInterval (a_Seconds);

    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::GetSocketConnectTimeout (PRUint32 * o_Seconds)
{
    if (o_Seconds == NULL)
        return NS_ERROR_NULL_POINTER;

    if (mSocketConnectTimeout == PR_INTERVAL_NO_TIMEOUT)
        *o_Seconds = 0;
    else
        *o_Seconds = PR_IntervalToSeconds (mSocketConnectTimeout);
    return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::SetSocketConnectTimeout (PRUint32   a_Seconds)
{
    if (a_Seconds == 0)
        mSocketConnectTimeout = PR_INTERVAL_NO_TIMEOUT;
    else
        mSocketConnectTimeout = PR_SecondsToInterval (a_Seconds);
    return NS_OK;
}

nsresult
nsSocketTransport::OnStatus(nsSocketRequest *req, nsISupports *ctxt, nsresult message)
{
    if (!mEventSink)
        return NS_ERROR_FAILURE;

    nsAutoString host; host.AssignWithConversion(mHostName);
    return mEventSink->OnStatus(req, ctxt, message, host.GetUnicode());
}

nsresult
nsSocketTransport::OnStatus(nsresult message)
{
    nsSocketRequest *req;

    if (mReadRequest)
        req = mReadRequest;
    else
        req = mWriteRequest;

    NS_ENSURE_TRUE(req, NS_ERROR_NOT_INITIALIZED);
    return OnStatus(req, req->GetContext(), message);
}

//
//----------------------------------------------------------------------------
// nsSocketBS
//----------------------------------------------------------------------------
//

nsSocketBS::nsSocketBS()
    : mTransport(nsnull)
    , mSock(nsnull)
{ }

nsSocketBS::~nsSocketBS()
{
    NS_IF_RELEASE(mTransport);
}

void
nsSocketBS::SetTransport(nsSocketTransport *aTransport)
{
    NS_IF_RELEASE(mTransport);
    NS_IF_ADDREF(mTransport = aTransport);
}

nsresult
nsSocketBS::Poll(PRInt16 event)
{
    nsresult rv = NS_OK;

    PRIntervalTime pollTimeout = PR_MillisecondsToInterval(DEFAULT_POLL_TIMEOUT_IN_MS);
    PRPollDesc pd;

    pd.fd = mSock;
    pd.in_flags = event | PR_POLL_EXCEPT;
    pd.out_flags = 0;

    PRInt32 result = PR_Poll(&pd, 1, pollTimeout);
    switch (result) {
    case 0:
        rv = NS_ERROR_NET_TIMEOUT;
    case 1:
        rv = NS_OK;
    default:
        LOG(("nsSocketBS::Poll [this=%x] PR_Poll returned %d [error=%d]\n",
            this, result, PR_GetError()));
        rv = NS_ERROR_FAILURE;
    }

    return rv;
}

//
//----------------------------------------------------------------------------
// nsSocketBIS
//----------------------------------------------------------------------------
//

NS_IMPL_ISUPPORTS1(nsSocketBIS, nsIInputStream)

nsSocketBIS::nsSocketBIS()
{
    NS_INIT_ISUPPORTS();
}

nsSocketBIS::~nsSocketBIS()
{ }

NS_IMETHODIMP
nsSocketBIS::Close()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketBIS::Available(PRUint32 *aCount)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketBIS::Read(char *aBuf, PRUint32 aCount, PRUint32 *aBytesRead)
{
    nsresult rv;
    PRInt32 total;

    LOG(("nsSocketBIS::Read [this=%x count=%u]\n", this, aCount));

tryRead:
    total = PR_Read(mSock, aBuf, aCount);
    if (total < 0) {
        if (PR_GetError() == PR_WOULD_BLOCK_ERROR) {
            //
            // Block until the socket is readable and then try again.
            //
            rv = Poll(PR_POLL_READ);
            if (NS_FAILED(rv)) {
                LOG(("nsSocketBIS::Read [this=%x] Poll failed [rv=%x]\n", this, rv));
                return rv;
            }
            LOG(("nsSocketBIS::Read [this=%x] Poll succeeded; looping back to PR_Read\n", this));
            goto tryRead;
        }
        return NS_ERROR_FAILURE;
    }
    /*
    if (mTransport && total) {
        mTransport->OnStatus(this, mListenerContext, NS_NET_STATUS_RECEIVING_FROM);
        mTransport->OnProgress(this, mListenerContext, mOffset);
    }
    */
    *aBytesRead = (PRUint32) total;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketBIS::ReadSegments(nsWriteSegmentFun aWriter, void *aClosure,
                                  PRUint32 aCount, PRUint32 *aBytesRead)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketBIS::GetNonBlocking(PRBool *aValue)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketBIS::GetObserver(nsIInputStreamObserver **aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketBIS::SetObserver(nsIInputStreamObserver *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//
//----------------------------------------------------------------------------
// nsSocketBOS
//----------------------------------------------------------------------------
//

NS_IMPL_ISUPPORTS1(nsSocketBOS, nsIOutputStream)

nsSocketBOS::nsSocketBOS()
{
    NS_INIT_ISUPPORTS();
}

nsSocketBOS::~nsSocketBOS()
{ }

NS_IMETHODIMP
nsSocketBOS::Close()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketBOS::Flush()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketBOS::Write(const char *aBuf, PRUint32 aCount, PRUint32 *aBytesWritten)
{
    nsresult rv;
    PRInt32 total;

    LOG(("nsSocketBOS::Write [this=%x count=%u]\n", this, aCount));

tryWrite:
    total = PR_Write(mSock, aBuf, aCount);
    if (total < 0) {
        if (PR_GetError() == PR_WOULD_BLOCK_ERROR) {
            //
            // Block until the socket is writable and then try again.
            //
            rv = Poll(PR_POLL_WRITE);
            if (NS_FAILED(rv)) {
                LOG(("nsSocketBOS::Write [this=%x] Poll failed [rv=%x]\n", this, rv));
                return rv;
            }
            LOG(("nsSocketBOS::Write [this=%x] Poll succeeded; looping back to PR_Write\n", this));
            goto tryWrite;
        }
        return NS_ERROR_FAILURE;
    }
    /*
    if (mTransport && total) {
        mTransport->OnStatus(this, mListenerContext, NS_NET_STATUS_SENDING_TO);
        mTransport->OnProgress(this, mListenerContext, mOffset);
    }
    */
    *aBytesWritten = (PRUint32) total;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketBOS::WriteFrom(nsIInputStream *aIS, PRUint32 aCount, PRUint32 *aBytesWritten)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketBOS::WriteSegments(nsReadSegmentFun aReader, void *aClosure,
                         PRUint32 aCount, PRUint32 *aBytesWritten)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketBOS::GetNonBlocking(PRBool *aValue)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketBOS::SetNonBlocking(PRBool aValue)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketBOS::GetObserver(nsIOutputStreamObserver **aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketBOS::SetObserver(nsIOutputStreamObserver *aObserver)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//
//----------------------------------------------------------------------------
// nsSocketIS
//----------------------------------------------------------------------------
//

NS_IMPL_THREADSAFE_ISUPPORTS1(nsSocketIS, nsIInputStream)

nsSocketIS::nsSocketIS()
    : mOffset(0)
    , mSock(nsnull)
    , mError(0)
{
    NS_INIT_ISUPPORTS();
}

NS_IMETHODIMP
nsSocketIS::Close()
{
    mSock = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketIS::Available(PRUint32 *aCount)
{
    NS_ENSURE_ARG_POINTER(aCount);
    NS_ENSURE_TRUE(mSock, NS_ERROR_NOT_INITIALIZED);
    PRInt32 avail = PR_Available(mSock);
    if (avail < 0)
        return NS_ERROR_FAILURE;
    *aCount = avail;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketIS::Read(char *aBuf, PRUint32 aCount, PRUint32 *aBytesRead)
{
    NS_ENSURE_ARG_POINTER(aBytesRead);
    NS_ENSURE_TRUE(mSock, NS_ERROR_NOT_INITIALIZED);
    PRInt32 result = PR_Read(mSock, aBuf, aCount);
    LOG(("nsSocketTransport: PR_Read(count=%u) returned %d\n", aCount, result));
    mError = 0;
    nsresult rv = NS_OK;
    if (result < 0) {
        mError = PR_GetError();
        if (PR_WOULD_BLOCK_ERROR == mError) {
            LOG(("nsSocketTransport: PR_Read() failed with PR_WOULD_BLOCK_ERROR\n"));
            rv = NS_BASE_STREAM_WOULD_BLOCK;
        }
        else {
            LOG(("nsSocketTransport: PR_Read() failed [error=%x, os_error=%x]\n",
                mError, PR_GetOSError()));
            rv = NS_ERROR_FAILURE;
        }
        *aBytesRead = 0;
    }
    else {
        *aBytesRead = (PRUint32) result;
        mOffset += (PRUint32) result;
    }
    return rv;
}

NS_IMETHODIMP
nsSocketIS::ReadSegments(nsWriteSegmentFun aWriter, void *aClosure,
                                  PRUint32 aCount, PRUint32 *aBytesRead)
{
    NS_NOTREACHED("nsSocketIS::ReadSegments");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketIS::GetNonBlocking(PRBool *aValue)
{
    NS_ENSURE_ARG_POINTER(aValue);
    *aValue = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketIS::GetObserver(nsIInputStreamObserver **aObserver)
{
    NS_NOTREACHED("nsSocketIS::GetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketIS::SetObserver(nsIInputStreamObserver *aObserver)
{
    NS_NOTREACHED("nsSocketIS::SetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

//
//----------------------------------------------------------------------------
// nsSocketOS
//----------------------------------------------------------------------------
//

NS_IMPL_THREADSAFE_ISUPPORTS1(nsSocketOS, nsIOutputStream)

nsSocketOS::nsSocketOS()
    : mOffset(0)
    , mSock(nsnull)
    , mError(0)
{
    NS_INIT_ISUPPORTS();
}

NS_IMETHODIMP
nsSocketOS::Close()
{
    mSock = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketOS::Flush()
{
    return NS_OK;
}

NS_IMETHODIMP
nsSocketOS::Write(const char *aBuf, PRUint32 aCount, PRUint32 *aBytesWritten)
{
    NS_ENSURE_TRUE(mSock, NS_ERROR_NOT_INITIALIZED);
    PRInt32 result = PR_Write(mSock, aBuf, aCount);
    LOG(("nsSocketTransport: PR_Write(count=%u) returned %d\n", aCount, result));
    mError = 0;
    nsresult rv = NS_OK;
    if (result < 0) {
        mError = PR_GetError();
        if (PR_WOULD_BLOCK_ERROR == mError) {
            LOG(("nsSocketTransport: PR_Write() failed with PR_WOULD_BLOCK_ERROR\n"));
            rv = NS_BASE_STREAM_WOULD_BLOCK;
        } else {
            LOG(("nsSocketTransport: PR_Write() failed [error=%x, os_error=%x]\n",
                mError, PR_GetOSError()));
            rv = NS_ERROR_FAILURE;
        }
        *aBytesWritten = 0;
    }
    else {
        *aBytesWritten = (PRUint32) result;
        mOffset += (PRUint32) result;
    }
    return rv;
}

NS_IMETHODIMP
nsSocketOS::WriteFrom(nsIInputStream *aIS, PRUint32 aCount, PRUint32 *aBytesWritten)
{
    NS_NOTREACHED("nsSocketOS::WriteFrom");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketOS::WriteSegments(nsReadSegmentFun aReader, void *aClosure,
                         PRUint32 aCount, PRUint32 *aBytesWritten)
{
    NS_NOTREACHED("nsSocketOS::WriteSegments");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketOS::GetNonBlocking(PRBool *aValue)
{
    NS_ENSURE_ARG_POINTER(aValue);
    *aValue = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketOS::SetNonBlocking(PRBool aValue)
{
    NS_NOTREACHED("nsSocketOS::SetNonBlocking");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketOS::GetObserver(nsIOutputStreamObserver **aObserver)
{
    NS_NOTREACHED("nsSocketOS::GetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketOS::SetObserver(nsIOutputStreamObserver *aObserver)
{
    NS_NOTREACHED("nsSocketOS::SetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

//
//----------------------------------------------------------------------------
// nsSocketRequest
//----------------------------------------------------------------------------
//

NS_IMPL_THREADSAFE_ISUPPORTS2(nsSocketRequest,
                              nsIRequest,
                              nsITransportRequest)

nsSocketRequest::nsSocketRequest()
    : mTransport(nsnull)
    , mStatus(NS_OK)
    , mSuspendCount(PR_FALSE)
    , mCanceled(PR_FALSE)
    , mStartFired(0)
{
    NS_INIT_ISUPPORTS();
}

nsSocketRequest::~nsSocketRequest()
{
    NS_IF_RELEASE(mTransport);
}

void
nsSocketRequest::SetTransport(nsSocketTransport *aTransport)
{
    //
    // Keep an owning reference back to the transport
    //
    NS_IF_RELEASE(mTransport);
    NS_IF_ADDREF(mTransport = aTransport);
}

nsresult
nsSocketRequest::OnStart(nsIStreamObserver *aObserver, nsISupports *aContext)
{
    if (aObserver) {
        aObserver->OnStartRequest(this, aContext);
        mStartFired = PR_TRUE;
    }
    return NS_OK;
}

nsresult
nsSocketRequest::OnStop(nsIStreamObserver *aObserver, nsISupports *aContext)
{
    if (aObserver) {
        if (!mStartFired) {
            aObserver->OnStartRequest(this, aContext);
            mStartFired = PR_TRUE;
        }
        aObserver->OnStopRequest(this, aContext, mStatus, nsnull);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsSocketRequest::GetName(PRUnichar **aResult)
{
    return mTransport ?
        mTransport->GetName(aResult) : NS_ERROR_NOT_INITIALIZED;
}

NS_IMETHODIMP
nsSocketRequest::IsPending(PRBool *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult =
        (mTransport ? (mTransport->GetSocket() != nsnull) : nsnull);
    return NS_OK;
}

NS_IMETHODIMP
nsSocketRequest::GetStatus(nsresult *aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsSocketRequest::Cancel(nsresult status)
{
    if (!mTransport)
        return NS_ERROR_NOT_INITIALIZED;
    LOG(("nsSocketRequest: Cancel [this=%x status=%x]\n", this, status));
    mStatus = status;
    mCanceled = PR_TRUE;
    // Will be canceled the next time PR_Poll returns
    return NS_OK;
}

NS_IMETHODIMP
nsSocketRequest::Suspend()
{
    if (IsCanceled())
        return NS_ERROR_FAILURE;
    if (!mTransport)
        return NS_ERROR_NOT_INITIALIZED;
    LOG(("nsSocketRequest: Suspend [this=%x]\n", this));
    mSuspendCount++;
    // Will be suspended the next time PR_Poll returns
    return NS_OK;
}

NS_IMETHODIMP
nsSocketRequest::Resume()
{
    if (IsCanceled())
        return NS_ERROR_FAILURE;
    if (!mTransport)
        return NS_ERROR_NOT_INITIALIZED;
    LOG(("nsSocketRequest: Resume [this=%x]\n", this));
    if (--mSuspendCount == 0)
        return mTransport->Dispatch(this);
    return NS_OK;
}

NS_IMETHODIMP
nsSocketRequest::GetTransport(nsITransport **result)
{
    NS_ENSURE_ARG_POINTER(result);
    NS_IF_ADDREF(*result = mTransport);
    return NS_OK;
}

//
//----------------------------------------------------------------------------
// nsSocketReadRequest
//----------------------------------------------------------------------------
//

nsSocketReadRequest::nsSocketReadRequest()
    : mInputStream(nsnull)
{ }

nsSocketReadRequest::~nsSocketReadRequest()
{
    NS_IF_RELEASE(mInputStream);
}

void
nsSocketReadRequest::SetSocket(PRFileDesc *aSock)
{
    if (!mInputStream) {
        NS_NEWXPCOM(mInputStream, nsSocketIS);
        if (mInputStream) {
            NS_ADDREF(mInputStream);
            mInputStream->SetSocket(aSock);
        }
    }
}

//
// return:
//   NS_OK                       to advance to next state
//   NS_ERROR_...                to advance to error state
//   NS_BASE_STREAM_WOULD_BLOCK  to stay on select list
//
nsresult
nsSocketReadRequest::OnRead()
{
    LOG(("nsSocketReadRequest: [this=%x] inside OnRead.\n", this));

    NS_ENSURE_TRUE(mInputStream, NS_ERROR_NOT_INITIALIZED);
    NS_ENSURE_TRUE(mListener, NS_ERROR_NOT_INITIALIZED);

    nsresult rv;
    PRUint32 amount, offset = mInputStream->GetOffset();

    rv = mInputStream->Available(&amount);
    if (NS_FAILED(rv)) return rv;

    amount = PR_MIN(amount, MAX_IO_TRANSFER_SIZE);

    LOG(("nsSocketReadRequest: [this=%x] calling listener [offset=%u, count=%u]\n",
        this, offset, amount));

    rv = mListener->OnDataAvailable(this,
                                    mListenerContext,
                                    mInputStream,
                                    offset,
                                    amount);

    //
    // Handle the error conditions
    //
    if (NS_BASE_STREAM_WOULD_BLOCK == rv) {
        LOG(("nsSocketReadRequest: [this=%x] listener would block; suspending self.\n", this));
        mSuspendCount++;
    }
    else if (NS_BASE_STREAM_CLOSED == rv) {
        LOG(("nsSocketReadRequest: [this=%x] done reading socket.\n", this));
        rv = NS_OK; // go to done state
    }
    else if (NS_FAILED(rv)) {
        LOG(("nsSocketReadRequest: [this=%x] error reading socket.\n", this));
    }
    else {
        PRUint32 total = offset - mInputStream->GetOffset();
        offset += total;

        if ((0 == total) && !mInputStream->GotWouldBlock())
            LOG(("nsSocketReadRequest: [this=%x] done reading socket.\n", this));
        else {
            LOG(("nsSocketReadRequest: [this=%x] read %u bytes [offset=%u]\n",
                this, total, offset));
            //
            // Stay in the read state
            //
            rv = NS_BASE_STREAM_WOULD_BLOCK;
        }

        if (mTransport && total) {
            mTransport->OnStatus(this, mListenerContext, NS_NET_STATUS_RECEIVING_FROM);
            mTransport->OnProgress(this, mListenerContext, offset);
        }
    }
    return rv;
}

//
//----------------------------------------------------------------------------
// nsSocketWriteRequest
//----------------------------------------------------------------------------
//

nsSocketWriteRequest::nsSocketWriteRequest()
    : mOutputStream(nsnull)
{ }

nsSocketWriteRequest::~nsSocketWriteRequest()
{
    NS_IF_RELEASE(mOutputStream);
}

void
nsSocketWriteRequest::SetSocket(PRFileDesc *aSock)
{
    if (!mOutputStream) {
        NS_NEWXPCOM(mOutputStream, nsSocketOS);
        if (mOutputStream) {
            NS_ADDREF(mOutputStream);
            mOutputStream->SetSocket(aSock);
        }
    }
}

//
// return:
//   NS_OK                       to advance to next state
//   NS_ERROR_...                to advance to error state
//   NS_BASE_STREAM_WOULD_BLOCK  to stay on select list
//
nsresult
nsSocketWriteRequest::OnWrite()
{
    LOG(("nsSocketWriteRequest: [this=%x] inside OnWrite.\n", this));

    NS_ENSURE_TRUE(mOutputStream, NS_ERROR_NOT_INITIALIZED);
    NS_ENSURE_TRUE(mProvider, NS_ERROR_NOT_INITIALIZED);

    nsresult rv;
    PRUint32 offset = mOutputStream->GetOffset();

    rv = mProvider->OnDataWritable(this,
                                   mProviderContext,
                                   mOutputStream,
                                   offset,
                                   MAX_IO_TRANSFER_SIZE);

    //
    // Handle the error conditions
    //
    if (NS_BASE_STREAM_WOULD_BLOCK == rv) {
        LOG(("nsSocketWriteRequest: [this=%x] provider would block; suspending self.\n", this));
        mSuspendCount++;
    }
    else if (NS_BASE_STREAM_CLOSED == rv) {
        LOG(("nsSocketWriteRequest: [this=%x] provider has finished.\n", this));
        rv = NS_OK; // go to done state
    }
    else if (NS_FAILED(rv)) {
        LOG(("nsSocketWriteRequest: [this=%x] provider failed, rv=%x.\n", this, rv));
    }
    else {
        PRUint32 total = offset - mOutputStream->GetOffset();
        offset += total;

        if ((0 == total) && !mOutputStream->GotWouldBlock())
            LOG(("nsSocketWriteRequest: [this=%x] done writing to socket.\n", this));
        else {
            LOG(("nsSocketWriteRequest: [this=%x] wrote %u bytes [offset=%u]\n",
                this, total, offset));
            //
            // Stay in the write state
            //
            rv = NS_BASE_STREAM_WOULD_BLOCK;
        }

        if (mTransport && total) {
            mTransport->OnStatus(this, mProviderContext, NS_NET_STATUS_SENDING_TO);
            mTransport->OnProgress(this, mProviderContext, offset);
        }
    }
    return rv;
}
