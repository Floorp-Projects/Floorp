/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsProxyObjectManager.h"
#include "nsXPIDLString.h"
#include "nsNetUtil.h"

static NS_DEFINE_CID(kSocketProviderService, NS_SOCKETPROVIDERSERVICE_CID);
static NS_DEFINE_CID(kDNSService, NS_DNSSERVICE_CID);
static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
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
static PRIntervalTime gTimeoutInterval = PR_INTERVAL_NO_WAIT;

#if defined(PR_LOGGING)
//
// Log module for SocketTransport logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsSocketTransport:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
//
PRLogModuleInfo* gSocketLog = nsnull;

#endif /* PR_LOGGING */

nsSocketTransport::nsSocketTransport():
    mCancelOperation(PR_FALSE),
    mCloseConnectionOnceDone(PR_FALSE),
    mCurrentState(eSocketState_Created),
    mHostName(nsnull),
    mLoadAttributes(LOAD_NORMAL),
    mLock(nsnull),
    mOperation(eSocketOperation_None),
    mPort(0),
    mPrintHost(nsnull),
    mReadWriteState(0),
    mSelectFlags(0),
    mService(nsnull),
    mSocketFD(nsnull),
    mSocketType(nsnull),
    mReadOffset(0),
    mWriteOffset(0),
    mStatus(NS_OK),
    mSuspendCount(0),
    mWriteCount(0),
    mWriteBuffer(nsnull),
    mWriteBufferIndex(0),
    mWriteBufferLength(0),
    mBytesExpected(-1),
    mReuseCount(0),
    mLastReuseCount(0)
{
  NS_INIT_REFCNT();

  PR_INIT_CLIST(&mListLink);

  mLastActiveTime  =  PR_INTERVAL_NO_WAIT;

  SetReadType (eSocketRead_None);
  SetWriteType(eSocketWrite_None);

  //
  // Set up Internet defaults...
  //
  memset(&mNetAddress, 0, sizeof(mNetAddress));
  PR_InitializeNetAddr(PR_IpAddrNull, 0, &mNetAddress);

  //
  // Initialize the global connect timeout value if necessary...
  //
  if (PR_INTERVAL_NO_WAIT == gConnectTimeout) {
    gConnectTimeout  = PR_MillisecondsToInterval(CONNECT_TIMEOUT_IN_MS);
  }

#if defined(PR_LOGGING)
  //
  // Initialize the global PRLogModule for socket transport logging 
  // if necessary...
  //
  if (nsnull == gSocketLog) {
    gSocketLog = PR_NewLogModule("nsSocketTransport");
  }
#endif /* PR_LOGGING */

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("Creating nsSocketTransport [%x].\n", this));
}


nsSocketTransport::~nsSocketTransport()
{
  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("Deleting nsSocketTransport [%s:%d %x].\n", 
          mHostName, mPort, this));

  // Release the nsCOMPtrs...
  //
  // It is easier to debug problems if these are released before the 
  // nsSocketTransport context is lost...
  //
  mReadListener = null_nsCOMPtr();
  mReadContext  = null_nsCOMPtr();
  mReadPipeIn   = null_nsCOMPtr();
  mReadPipeOut  = null_nsCOMPtr();

  mWriteObserver = null_nsCOMPtr();
  mWriteContext  = null_nsCOMPtr();
  mWritePipeIn   = null_nsCOMPtr();
  mWritePipeOut  = null_nsCOMPtr();
  //
  // Cancel any pending DNS request...
  //
  if (mDNSRequest) {
    mDNSRequest->Cancel();
  }
  mDNSRequest = null_nsCOMPtr();

  NS_IF_RELEASE(mService);

  CRTFREEIF(mPrintHost);
  CRTFREEIF(mHostName);
  CRTFREEIF(mSocketType);

  if (mSocketFD) {
    PR_Close(mSocketFD);
    mSocketFD = nsnull;
  }

  if (mLock) {
    PR_DestroyLock(mLock);
    mLock = nsnull;
  }

  if (mWriteBuffer) {
    PR_Free(mWriteBuffer);
    mWriteBuffer = nsnull;
  }
}


nsresult nsSocketTransport::Init(nsSocketTransportService* aService,
                                 const char* aHost, 
                                 PRInt32 aPort,
                                 const char* aSocketType,
                                 const char* aPrintHost,
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

  mPort = aPort;
  if (aHost && *aHost) {
    mHostName = nsCRT::strdup(aHost);
    if (!mHostName) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  } 
  // hostname was nsnull or empty...
  else {
    rv = NS_ERROR_FAILURE;
  }

  if (aPrintHost)
  {
      mPrintHost = nsCRT::strdup(aPrintHost);
      if (!mPrintHost)
          rv = NS_ERROR_OUT_OF_MEMORY;
  }

  if (NS_SUCCEEDED(rv) && aSocketType) {
    mSocketType = nsCRT::strdup(aSocketType);
    if (!mSocketType) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  } 

  //
  // Create the lock used for synchronizing access to the transport instance.
  //
  if (NS_SUCCEEDED(rv)) {
    mLock = PR_NewLock();
    if (!mLock) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // Update the active time for timeout purposes...
  mLastActiveTime  = PR_IntervalNow();

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("Initializing nsSocketTransport [%s:%d %x].  rv = %x",
          mHostName, mPort, this, rv));

  return rv;
}


nsresult nsSocketTransport::CheckForTimeout(PRIntervalTime aCurrentTime)
{
  nsresult rv = NS_OK;
  PRIntervalTime idleInterval;

  // Enter the socket transport lock...
  nsAutoLock aLock(mLock);

  idleInterval = aCurrentTime - mLastActiveTime;

  //
  // Only timeout if the transport is waiting to connect to the server
  //
  if ((eSocketState_WaitConnect == mCurrentState) && 
      (idleInterval >= gTimeoutInterval)) {
    PR_LOG(gSocketLog, PR_LOG_ERROR, 
           ("nsSocketTransport::CheckForTimeout() [%s:%d %x].\t"
            "TIMED OUT... Idle interval: %d\n",
            mHostName, mPort, this, idleInterval));

    // Move the transport into the Timeout state...  
    mCurrentState = eSocketState_Timeout;
    rv = NS_ERROR_NET_TIMEOUT;
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
  PR_Lock(mLock);

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::Process() [%s:%d %x].\t"
          "aSelectFlags = %x.\t"
          "CurrentState = %d\n",
          mHostName, mPort, this, aSelectFlags, mCurrentState));

  //
  // Check for an error during PR_Poll(...)
  //
  if (PR_POLL_EXCEPT & aSelectFlags) {
    PR_LOG(gSocketLog, PR_LOG_ERROR, 
           ("Operation failed via PR_POLL_EXCEPT. [%s:%d %x].\n", 
            mHostName, mPort, this));
    // An error has occurred, so cancel the read and/or write operation...
    if (mCurrentState == eSocketState_WaitConnect)
        mStatus = NS_ERROR_CONNECTION_REFUSED;
    else
        mStatus = NS_BINDING_FAILED;
  }

  if (PR_POLL_HUP & aSelectFlags) {
    PR_LOG(gSocketLog, PR_LOG_ERROR, 
           ("Operation failed via PR_POLL_HUP. [%s:%d %x].\n", 
            mHostName, mPort, this));
    mStatus = NS_OK;
    mCurrentState = eSocketState_Error;
  }

  while (!done)
  {
    //
    // If the transport has been suspended, then return NS_OK immediately...
    // This removes the transport from the select list...
    //
    if (mSuspendCount) {
      PR_LOG(gSocketLog, PR_LOG_DEBUG, 
             ("Transport [%s:%d %x] is suspended.\n", 
              mHostName, mPort, this));
  
      done = PR_TRUE;
      mStatus = NS_OK;
      continue;
    }

    //
    // If the transport has been canceled the set the status code to
    // NS_BINDING_ABORTED which is a failure code...  This will cause the
    // transport to move into the error state and end the request...
    //
    if (mCancelOperation) {
      PR_LOG(gSocketLog, PR_LOG_DEBUG, 
             ("Transport [%s:%d %x] has been cancelled.\n", 
              mHostName, mPort, this));

      mCancelOperation = PR_FALSE;
      mStatus = NS_BINDING_ABORTED;
    }

    //
    // If an error has occurred then move into the error state...
    //
    if (NS_FAILED(mStatus) && (NS_BASE_STREAM_WOULD_BLOCK != mStatus)) {
      mCurrentState = eSocketState_Error;
    }

    switch (mCurrentState) {
      case eSocketState_Created:
      case eSocketState_Closed:
        break;

      case eSocketState_Connected:
        //
        // A connection has been established with the server
        //
        
        mSelectFlags = PR_POLL_EXCEPT;

        // XXX/ruslan: in case of keel-alive we need to check whether the connection is still alive;
        //  this is ugly, but so as this state machine. It's too late in doWrite to do anything; let
        //  me know if someone sees a better solution

        if (mReuseCount > 0 && mReuseCount > mLastReuseCount)
        {
            PRBool isalive = PR_FALSE;
            if (NS_SUCCEEDED (IsAlive (0, &isalive)) && !isalive)
            {
                CloseConnection ();

                mCurrentState = eSocketState_WaitConnect;
                mLastReuseCount = mReuseCount;
                continue;
            }
        }

        if (GetReadType() != eSocketRead_None) {
          // Set the select flags for non-blocking reads...
          mSelectFlags |= PR_POLL_READ;

          // Fire a notification that the read has started...
          if (mReadListener) {
            mReadListener->OnStartRequest(this, mReadContext);
          }
        }
        if (GetWriteType() != eSocketWrite_None) {
          // Set the select flags for non-blocking writes...
          mSelectFlags |= PR_POLL_WRITE;

          // Fire a notification that the write has started...
          if (mWriteObserver) {
            mWriteObserver->OnStartRequest(this, mWriteContext);
          }
        }
        break;

      case eSocketState_Error:
        PR_LOG(gSocketLog, PR_LOG_DEBUG, 
               ("Transport [%s:%d %x] is in error state.\n", 
                mHostName, mPort, this));

        // Cancel any DNS requests...
        if (mDNSRequest) {
          mDNSRequest->Cancel();
          mDNSRequest = null_nsCOMPtr();
        }

        // Cancel any read and/or write requests...
        SetFlag(eSocketRead_Done);
        SetFlag(eSocketWrite_Done);

        CloseConnection();
        //
        // Fall into the Done state...
        //
      case eSocketState_Done:
        PR_LOG(gSocketLog, PR_LOG_DEBUG, 
               ("Transport [%s:%d %x] is in done state.\n", 
                mHostName, mPort, this));

        mBytesExpected = -1;

        if (GetFlag(eSocketRead_Done)) {
          // Fire a notification that the read has finished...
          if (mReadListener) {
            mReadListener->OnStopRequest(this, mReadContext, mStatus, nsnull);
            mReadListener = null_nsCOMPtr();
            mReadContext  = null_nsCOMPtr();
          }
          // Close the socket transport end of the pipe...
          if (mReadPipeOut) {
            mReadPipeOut->Close();
          }
          mReadPipeIn  = null_nsCOMPtr();
          mReadPipeOut = null_nsCOMPtr();
          SetReadType(eSocketRead_None);
          ClearFlag(eSocketRead_Done);

          // When we have finished reading from the server
          // close connection if required to do so...
          if (mCloseConnectionOnceDone)
            CloseConnection();
        }

        if (GetFlag(eSocketWrite_Done)) {
          // Fire a notification that the write has finished...
          if (mWriteObserver) {
            mWriteObserver->OnStopRequest(this, mWriteContext, mStatus, nsnull);
            mWriteObserver = null_nsCOMPtr();
            mWriteContext  = null_nsCOMPtr();
          }
          // Close the socket transport end of the pipe...
          if (mWritePipeIn) {
            mWritePipeIn->Close();
          }
          mWritePipeIn  = null_nsCOMPtr();
          mWritePipeOut = null_nsCOMPtr();
          SetWriteType(eSocketWrite_None);
          ClearFlag(eSocketWrite_Done);

          if (mCloseConnectionOnceDone)
            CloseConnection();

        }

        //
        // Are all read and write requests done?
        //
        if ((GetReadType()  == eSocketRead_None) && 
            (GetWriteType() == eSocketWrite_None)) 
        {
          mCurrentState = gStateTable[mOperation][mCurrentState];
          mOperation    = eSocketOperation_None;
          mStatus = NS_OK;
          done = PR_TRUE;
        } else {
          // Still reading or writing...
          mCurrentState = eSocketState_WaitReadWrite;
        }
        continue;

      case eSocketState_WaitDNS:
        mStatus = doResolveHost();
        break;

      case eSocketState_WaitConnect:
        mStatus = doConnection(aSelectFlags);
        if (NS_SUCCEEDED(mStatus) && mOpenObserver) {
          mOpenObserver->OnStartRequest(this, mOpenContext);
          NS_ASSERTION(mOperation == eSocketOperation_Connect, "bad state");
          mCurrentState = gStateTable[mOperation][mCurrentState];
          mOperation    = eSocketOperation_None;
          done = PR_TRUE;
          continue;
        }
        break;

      case eSocketState_WaitReadWrite:
        // Process the read request...
        if (GetReadType() != eSocketRead_None)
        {
            if (mBytesExpected == 0)
            {
                mStatus = NS_OK;
                mSelectFlags &= (~PR_POLL_READ);

            }
            else
                mStatus = doRead (aSelectFlags);

            if (NS_SUCCEEDED(mStatus)) 
            {
                SetFlag(eSocketRead_Done);
                break;
            }
        }
        // Process the write request...
        if ((NS_SUCCEEDED(mStatus) || mStatus == NS_BASE_STREAM_WOULD_BLOCK)
            && (GetWriteType() != eSocketWrite_None)) {
          mStatus = doWrite(aSelectFlags);
          if (NS_SUCCEEDED(mStatus)) {
            SetFlag(eSocketWrite_Done);
            break;
          }
        }
        break;

      case eSocketState_Timeout:
        mStatus = NS_ERROR_NET_TIMEOUT;
        break;

      default:
        NS_ASSERTION(0, "Unexpected state...");
        mStatus = NS_ERROR_FAILURE;
        break;
    }

    fireStatus(mCurrentState);
    //
    // If the current state has successfully completed, then move to the
    // next state for the current operation...
    //
    if (NS_SUCCEEDED(mStatus)) {
      mCurrentState = gStateTable[mOperation][mCurrentState];
    } 
    else if (NS_BASE_STREAM_WOULD_BLOCK == mStatus) {
      done = PR_TRUE;
    }

    // 
    // Any select flags are *only* valid the first time through the loop...
    // 
    aSelectFlags = 0;
  }

  // Update the active time for timeout purposes...
  mLastActiveTime  = PR_IntervalNow();

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::Process() [%s:%d %x]. mStatus = %x.\t"
          "CurrentState = %d\n\n",
          mHostName, mPort, this, mStatus, mCurrentState));

  PR_Unlock(mLock);
  return mStatus;
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

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::doResolveHost() [%s:%d %x].\n", 
          mHostName, mPort, this));

  //
  // The hostname has not been resolved yet...
  //
  if (! mNetAddress.inet.ip) {
    //
    // Initialize the port used for the connection...
    //
    // XXX: The list of ports must be restricted - see net_bad_ports_table[] in 
    //      mozilla/network/main/mkconect.c
    //
    mNetAddress.inet.port = PR_htons(mPort);

    NS_WITH_SERVICE(nsIDNSService,
                    pDNSService,
                    kDNSService,
                    &rv);
    if (NS_FAILED(rv)) return rv;

    //
    // Give up the SocketTransport lock.  This allows the DNS thread to call the
    // nsIDNSListener notifications without blocking...
    //
    PR_Unlock(mLock);

    rv = pDNSService->Lookup(nsnull, mHostName, this, 
                             getter_AddRefs(mDNSRequest));
    //
    // Aquire the SocketTransport lock again...
    //
    PR_Lock(mLock);

    if (NS_SUCCEEDED(rv)) {
      //
      // The DNS lookup has finished...  It has either failed or succeeded.
      //
      if (NS_FAILED(mStatus) || mNetAddress.inet.ip) {
        mDNSRequest = null_nsCOMPtr();
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

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::doResolveHost() [%s:%d %x].\t"
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

  NS_ASSERTION(eSocketState_WaitConnect == mCurrentState, "Wrong state.");

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::doConnection() [%s:%d %x].\t"
          "aSelectFlags = %x.\n",
          mHostName, mPort, this, aSelectFlags));

  if (!mSocketFD) {
    //
    // Step 1:
    //    Create a new TCP socket structure...
    //
    if (!mSocketType) 
    {
      mSocketFD = PR_NewTCPSocket();
    }
    else 
    {
      NS_WITH_SERVICE(nsISocketProviderService,
                      pProviderService,
                      kSocketProviderService,
                      &rv);

      nsCOMPtr<nsISocketProvider> pProvider;

      if (NS_SUCCEEDED(rv))
        rv = pProviderService->GetSocketProvider(mSocketType, getter_AddRefs(pProvider));

      if (NS_SUCCEEDED(rv))
        rv = pProvider->NewSocket(mHostName, &mSocketFD, getter_AddRefs(mSecurityInfo));
    }

    if (mSocketFD) {
      PRSocketOptionData opt;

      // Make the socket non-blocking...
      opt.option = PR_SockOpt_Nonblocking;
      opt.value.non_blocking = PR_TRUE;
      status = PR_SetSocketOption(mSocketFD, &opt);
      if (PR_SUCCESS != status) {
        rv = NS_ERROR_FAILURE;
      }

      // XXX: Is this still necessary?
#if defined(XP_WIN16) || (defined(XP_OS2) && !defined(XP_OS2_DOUGSOCK))
      opt.option = PR_SockOpt_Linger;
      opt.value.linger.polarity = PR_TRUE;
      opt.value.linger.linger = PR_INTERVAL_NO_WAIT;
      PR_SetSocketOption(*sock, &opt);
#endif /* XP_WIN16 || XP_OS2*/
    } 
    else {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }

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
        else if (PR_IS_CONNECTED_ERROR == code) {
          rv = NS_OK;
        }
        //
        // The connection was refused...
        //
        else {
          // Connection refused...
          PR_LOG(gSocketLog, PR_LOG_ERROR, 
                 ("Connection Refused [%s:%d %x].  PRErrorCode = %x\n",
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
      PR_LOG(gSocketLog, PR_LOG_ERROR, 
             ("Connection Refused via PR_POLL_EXCEPT. [%s:%d %x].\n", 
              mHostName, mPort, this));

      rv = NS_ERROR_CONNECTION_REFUSED;
    }
    //
    // The connection was successful...  
    //
    // PR_Poll(...) returns PR_POLL_WRITE to indicate that the connection is
    // established...
    //
    else if (PR_POLL_WRITE & aSelectFlags) {
      rv = NS_OK;
    }
  } else {
    rv = NS_BASE_STREAM_WOULD_BLOCK;
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::doConnection() [%s:%d %x].\t"
          "rv = %x.\n\n",
          mHostName, mPort, this, rv));

  return rv;
}


typedef struct {
  PRFileDesc *fd;
  PRBool      bEOF;
} nsReadFromSocketClosure;


NS_METHOD
nsReadFromSocket(void* closure,
                 char* toRawSegment,
                 PRUint32 offset,
                 PRUint32 count,
                 PRUint32 *readCount)
{
  nsresult rv = NS_OK;
  PRInt32 len;
  nsReadFromSocketClosure *info = (nsReadFromSocketClosure*)closure;

  info->bEOF = PR_FALSE;
  *readCount = 0;
  if (count > 0)
  {
        len = PR_Read (info -> fd, toRawSegment, count);

    if (len >= 0)
    {
      *readCount = (PRUint32)len;
      info -> bEOF = (0 == len);
    } 
    //
    // Error...
    //
    else
    {
        PRErrorCode code = PR_GetError   ();

    if (PR_WOULD_BLOCK_ERROR == code)
    {
        rv = NS_BASE_STREAM_WOULD_BLOCK;
    }
    else
    {
        PRInt32 osCode = PR_GetOSError ();
        PR_LOG (gSocketLog, PR_LOG_ERROR, ("PR_Read() failed. PRErrorCode = %x, os_error=%d\n", code, osCode));

        info -> bEOF = PR_TRUE;

        // XXX: What should this error code be?
        rv = NS_ERROR_FAILURE;
      }
    }
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("nsReadFromSocket [fd=%x].  rv = %x. Buffer space = %d.  Bytes read =%d\n",
          info->fd, rv, count, *readCount));

  return rv;
}

NS_METHOD
nsWriteToSocket(void* closure,
                const char* fromRawSegment,
                PRUint32 toOffset,
                PRUint32 count,
                PRUint32 *writeCount)
{
  nsresult rv = NS_OK;
  PRInt32 len;
  PRFileDesc* fd = (PRFileDesc*)closure;

  *writeCount = 0;
  if (count > 0) {
    len = PR_Write(fd, fromRawSegment, count);
    if (len > 0) {
      *writeCount = (PRUint32)len;
    }
    //
    // Error...
    //
    else {
      PRErrorCode code = PR_GetError();

      if (PR_WOULD_BLOCK_ERROR == code) {
        rv = NS_BASE_STREAM_WOULD_BLOCK;
      } 
      else {
        PR_LOG(gSocketLog, PR_LOG_ERROR, 
               ("PR_Write() failed. PRErrorCode = %x\n", code));

        // XXX: What should this error code be?
        rv = NS_ERROR_FAILURE;
      }
    }
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("nsWriteToSocket [fd=%x].  rv = %x. Buffer space = %d.  Bytes written =%d\n",
          fd, rv, count, *writeCount));

  return rv;

}

//-----
//
//  doRead:
//
//  This method is called while holding the SocketTransport lock.  It is
//  always called on the socket transport thread...
//
//  Return values:
//    NS_OK
//    NS_BASE_STREAM_WOULD_BLOCK
//
//    NS_ERROR_FAILURE
//
//-----
nsresult nsSocketTransport::doRead(PRInt16 aSelectFlags)
{
  nsReadFromSocketClosure info;
  PRUint32 totalBytesWritten;
  nsresult rv = NS_OK;

  NS_ASSERTION(eSocketState_WaitReadWrite == mCurrentState, "Wrong state.");
  NS_ASSERTION(GetReadType() != eSocketRead_None, "Bad Read Type!");

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::doRead() [%s:%d %x].\t"
          "aSelectFlags = %x.\t",
          mHostName, mPort, this, aSelectFlags));

  //
  // Fill the stream with as much data from the network as possible...
  //
  //
  totalBytesWritten = 0;
  info.fd = mSocketFD;

  // Release the transport lock...  WriteSegments(...) aquires the nsBuffer
  // lock which could cause a deadlock by blocking the socket transport 
  // thread
  //
  PR_Unlock(mLock);
  rv = mReadPipeOut->WriteSegments(nsReadFromSocket, (void*)&info, 
                                   MAX_IO_TRANSFER_SIZE, &totalBytesWritten);
  PR_Lock(mLock);

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("WriteSegments [fd=%x].  rv = %x. Bytes read =%d\n",
         mSocketFD, rv, totalBytesWritten));

  //
  // Fire a single OnDataAvaliable(...) notification once as much data has
  // been filled into the stream as possible...
  //
  if (totalBytesWritten)
  {
    if (mReadListener)
    {
      nsresult rv1;

      rv1 = mReadListener->OnDataAvailable(this, mReadContext, mReadPipeIn, 
                                           mReadOffset, 
                                           totalBytesWritten);

      //
      // If the consumer returns failure, then cancel the operation...
      //
      if (NS_FAILED(rv1))
            rv = rv1;

    }
    mReadOffset += totalBytesWritten;
  }

  //
  // Deal with the possible return values...
  //
  if (NS_SUCCEEDED(rv)) {
    if (info.bEOF || mBytesExpected == 0)
    {
      // EOF condition
      mSelectFlags &= (~PR_POLL_READ);
      rv = NS_OK;
    } 
    else {    // continue to return WOULD_BLOCK until we've completely finished this read
      rv = NS_BASE_STREAM_WOULD_BLOCK;
    }
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::doRead() [%s:%d %x]. rv = %x.\t"
          "Total bytes read: %d\n\n",
          mHostName, mPort, this, rv, totalBytesWritten));
  if (mEventSink)
      // we don't have content length info at the socket level
      // just pass 0 through.
      (void)mEventSink->OnProgress(this, mReadContext, mReadOffset, 0);

  return rv;
}


//-----
//
//  doWrite:
//
//  This method is called while holding the SocketTransport lock.  It is
//  always called on the socket transport thread...
//
//  Return values:
//    NS_OK
//    NS_BASE_STREAM_WOULD_BLOCK
//
//    NS_ERROR_FAILURE
//
//-----
nsresult nsSocketTransport::doWrite(PRInt16 aSelectFlags)
{
  PRUint32 totalBytesWritten = 0;
  nsresult rv = NS_OK;

  NS_ASSERTION(eSocketState_WaitReadWrite == mCurrentState, "Wrong state.");
  NS_ASSERTION(GetWriteType() != eSocketWrite_None, "Bad Write Type!");

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::doWrite() [%s:%d %x].\t"
          "aSelectFlags = %x.\t"
          "mWriteCount = %d\n",
          mHostName, mPort, this, aSelectFlags, mWriteCount));

  if (mWritePipeIn) {
    // Writing from a nsIBufferInputStream...
    rv = doWriteFromBuffer(&totalBytesWritten);
  }
  else {
    // Writing from a generic nsIInputStream...
    rv = doWriteFromStream(&totalBytesWritten);
  }

  // Update the counters...
  if (mWriteCount > 0) {
    NS_ASSERTION(mWriteCount >= (PRInt32)totalBytesWritten, 
                 "wrote more than humanly possible");
    mWriteCount -= totalBytesWritten;
  }

  mWriteOffset += totalBytesWritten;

  //
  // The write operation has completed...
  //
  if ((NS_SUCCEEDED(rv) && (0 == totalBytesWritten)) ||         // eof, or
      (GetFlag(eSocketWrite_Async) && (0 == mWriteCount))) {    // wrote everything
    mSelectFlags &= (~PR_POLL_WRITE);
    rv = NS_OK;
  }
  else if (NS_SUCCEEDED(rv)) {
    // We wrote something, so loop and try again.
    // return WOULD_BLOCK to keep the transport on the select list...
    rv = NS_BASE_STREAM_WOULD_BLOCK;
  }
  else if (NS_BASE_STREAM_WOULD_BLOCK == rv) {
    //
    // If the buffer is empty, then notify the reader and stop polling
    // for write until there is data in the buffer.  See the OnWrite()
    // notification...
    //
    NS_ASSERTION((0 == totalBytesWritten), "returned NS_BASE_STREAM_WOULD_BLOCK and a writeCount");
    if (GetFlag(eSocketWrite_Sync)) {
      // We can only wait if we created the stream (a pipe -- created in the synchronous 
      // OpenOutputStream case). If we didn't create it, we couldn't have been the buffer
      // observer, so we won't get any notification when more data becomes available. 
      SetFlag(eSocketWrite_Wait);
      mSelectFlags &= (~PR_POLL_WRITE);
    }
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::doWrite() [%s:%d %x]. rv = %x.\t"
          "Total bytes written: %d\n\n",
          mHostName, mPort, this, rv, totalBytesWritten));

  if (mEventSink)
      // we don't have content length info at the socket level
      // just pass 0 through.
      (void)mEventSink->OnProgress(this, mWriteContext, mWriteOffset, 0);

  return rv;
}


nsresult nsSocketTransport::doWriteFromBuffer(PRUint32 *aCount)
{
  nsresult rv;
  PRUint32 transferCount;

  // Figure out how much data to write...
  if (mWriteCount > 0) {
    transferCount = PR_MIN(mWriteCount, MAX_IO_TRANSFER_SIZE);
  } else {
    transferCount = MAX_IO_TRANSFER_SIZE;
  }

  //
  // Release the transport lock...  ReadSegments(...) aquires the nsBuffer
  // lock which could cause a deadlock by blocking the socket transport 
  // thread
  //
  PR_Unlock(mLock);
  
  //
  // Write the data to the network...
  // 
  // return values:
  //   NS_BASE_STREAM_WOULD_BLOCK - The stream is empty.
  //   NS_OK                      - The write succeeded.
  //   FAILURE                    - Something bad happened.
  //
  rv = mWritePipeIn->ReadSegments(nsWriteToSocket, (void*)mSocketFD, 
                                  transferCount, aCount);
  PR_Lock(mLock);

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
        ("ReadSegments [fd=%x].  rv = %x. Bytes written =%d\n",
         mSocketFD, rv, *aCount));

  return rv;
}


nsresult nsSocketTransport::doWriteFromStream(PRUint32 *aCount)
{
  nsresult rv = NS_OK;
  PRUint32 transferCount;

  // Figure out how much data to write...
  if (mWriteCount > 0) {
    transferCount = PR_MIN(mWriteCount, MAX_IO_TRANSFER_SIZE);
  } else {
    transferCount = MAX_IO_TRANSFER_SIZE;
  }

  //
  // Read some data from the stream...
  // 
  // return values:
  //   NS_BASE_STREAM_WOULD_BLOCK - The stream is empty.
  //   NS_OK                      - The write succeeded.
  //   FAILURE                    - Something bad happened.
  //
  if (0 == mWriteBufferLength) {
    // When the buffer is empty, read the next chunk of data into 
    // the buffer...
    rv = mWriteFromStream->Read(mWriteBuffer, transferCount, 
                                &mWriteBufferLength);
    mWriteBufferIndex = 0;
  }

  *aCount = 0;
  if (NS_SUCCEEDED(rv)) {
    // Try to send the data to the network.
    rv = nsWriteToSocket((void*)mSocketFD, mWriteBuffer, mWriteBufferIndex, 
                         mWriteBufferLength, aCount);
    // Update the buffer index and length with the actual amount of data
    // that was sent...
    mWriteBufferIndex  += *aCount;
    mWriteBufferLength -= *aCount;
  }

  return rv;  
}


nsresult nsSocketTransport::CloseConnection(PRBool bNow)
{
  PRStatus status;
  nsresult rv = NS_OK;

  if (!bNow) // close connection once done. 
  {
    mCloseConnectionOnceDone = PR_TRUE;
    return NS_OK;
  }

  if (!mSocketFD) {
    mCurrentState = eSocketState_Closed;
    return NS_OK;
  }

  status = PR_Close(mSocketFD);
  if (PR_SUCCESS != status) {
    rv = NS_ERROR_FAILURE;
  }
  mSocketFD = nsnull;

  if (NS_SUCCEEDED(rv)) {
    mCurrentState = eSocketState_Closed;
  }
  if (mOpenObserver) {
    nsresult rv2 = mOpenObserver->OnStopRequest(this, mOpenContext,
                                                rv, nsnull);    // XXX need error message
    if (NS_SUCCEEDED(rv))
      rv = rv2;
    mOpenObserver = null_nsCOMPtr();
    mOpenContext = null_nsCOMPtr();
  }
  return rv;
}


void nsSocketTransport::SetSocketTimeout(PRIntervalTime aTimeInterval)
{
  gTimeoutInterval = aTimeInterval;
}


//
// --------------------------------------------------------------------------
// nsISupports implementation...
// --------------------------------------------------------------------------
//

NS_IMPL_THREADSAFE_ISUPPORTS5(nsSocketTransport, 
                              nsIChannel, 
                              nsIRequest, 
                              nsIDNSListener, 
                              nsIPipeObserver,
                              nsISocketTransport);

//
// --------------------------------------------------------------------------
// nsISocketTransport implementation...
// --------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSocketTransport::GetReuseConnection(PRBool *_retval)
{
    if (_retval == NULL)
        return NS_ERROR_FAILURE;

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
nsSocketTransport::GetBytesExpected (PRInt32 * bytes)
{
    if (bytes != NULL)
    {
        *bytes = mBytesExpected;
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSocketTransport::SetBytesExpected (PRInt32 bytes)
{
    nsAutoLock alock (mLock);

    if (mCurrentState == eSocketState_WaitReadWrite)
    {
        mBytesExpected = bytes;

        if (mBytesExpected == 0)
            mService -> Wakeup (this);
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


//
// --------------------------------------------------------------------------
// nsIRequest implementation...
// --------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSocketTransport::IsPending(PRBool *result)
{
  *result = mCurrentState != eSocketState_Created
    && mCurrentState != eSocketState_Closed;
  return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::Cancel(void)
{
  nsresult rv = NS_OK;

  // Enter the socket transport lock...
  nsAutoLock aLock(mLock);

  mCancelOperation = PR_TRUE;
  //
  // Wake up the transport on the socket transport thread so it can
  // be removed from the select list...  
  //
  rv = mService->AddToWorkQ(this);

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("Canceling nsSocketTransport [%s:%d %x].  rv = %x\n",
          mHostName, mPort, this, rv));

  return rv;
}

NS_IMETHODIMP
nsSocketTransport::Suspend(void)
{
  nsresult rv = NS_OK;

  // Enter the socket transport lock...
  nsAutoLock aLock(mLock);

  mSuspendCount += 1;
  //
  // Wake up the transport on the socket transport thread so it can
  // be removed from the select list...  
  //
  // Only do this the first time a transport is suspended...
  //
  if (1 == mSuspendCount) {
    rv = mService->AddToWorkQ(this);
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("Suspending nsSocketTransport [%s:%d %x].  rv = %x\t"
          "mSuspendCount = %d.\n",
          mHostName, mPort, this, rv, mSuspendCount));

  return rv;
}


NS_IMETHODIMP
nsSocketTransport::Resume(void)
{
  nsresult rv = NS_OK;

  // Enter the socket transport lock...
  nsAutoLock aLock(mLock);

  if (mSuspendCount) {
    mSuspendCount -= 1;
    //
    // Wake up the transport on the socket transport thread so it can
    // be resumed...
    //
    if (0 == mSuspendCount) {
      rv = mService->AddToWorkQ(this);
    }
  } else {
    // Only a suspended transport can be resumed...
    rv = NS_ERROR_FAILURE;
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("Resuming nsSocketTransport [%s:%d %x].  rv = %x\t"
          "mSuspendCount = %d.\n",
          mHostName, mPort, this, rv, mSuspendCount));

  return rv;
}

// 
// -------------------------------------------------------------------------- 
// nsIPipeObserver implementation... 
// -------------------------------------------------------------------------- 
// 
// The pipe observer is used by the following methods: 
//    AsyncRead(...) 
//    OpenInputStream(...) 
//    OpenOutputStream(...). 
// 
NS_IMETHODIMP 
nsSocketTransport::OnFull(nsIPipe* aPipe) 
{ 
  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("nsSocketTransport::OnFull() [%s:%d %x] nsIPipe=%x.\n", 
          mHostName, mPort, this, aPipe));

  // 
  // The socket transport has filled up the pipe.  Remove the 
  // transport from the select list until the consumer can 
  // make room... 
  // 
  nsCOMPtr<nsIBufferInputStream> in;
  nsresult rv = aPipe->GetInputStream(getter_AddRefs(in));
  if (NS_SUCCEEDED(rv) && in == mReadPipeIn) {
    // Enter the socket transport lock...
    nsAutoLock aLock(mLock);

    NS_ASSERTION(!GetFlag(eSocketRead_Wait), "Already waiting!"); 

    SetFlag(eSocketRead_Wait);
    mSelectFlags &= (~PR_POLL_READ);
    return NS_OK;
  }

  // Else, since we might get an OnFull without an intervening OnWrite
  // try the OnWrite case to see if we need to resume the blocking write operation:
  return OnWrite(aPipe, 0);
}


NS_IMETHODIMP
nsSocketTransport::OnWrite(nsIPipe* aPipe, PRUint32 aCount)
{
  nsresult rv = NS_OK;

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("nsSocketTransport::OnWrite() [%s:%d %x]. nsIPipe=%x Count=%d\n", 
         mHostName, mPort, this, aPipe, aCount));

  // 
  // The consumer has written some data into the pipe... If the transport 
  // was waiting to write some data to the network, then add it to the 
  // select list... 
  // 
  nsCOMPtr<nsIBufferInputStream> in;
  rv = aPipe->GetInputStream(getter_AddRefs(in));
  if (NS_SUCCEEDED(rv) && in == mWritePipeIn) {
    // Enter the socket transport lock...
    nsAutoLock aLock(mLock);

    if (GetFlag(eSocketWrite_Wait)) { 
      ClearFlag(eSocketWrite_Wait); 
      mSelectFlags |= PR_POLL_WRITE; 

      // Start the crank.
      mOperation = eSocketOperation_ReadWrite;
      rv = mService->AddToWorkQ(this);
    }
  }

  return NS_OK;
}


NS_IMETHODIMP 
nsSocketTransport::OnEmpty(nsIPipe* aPipe)
{
  nsresult rv = NS_OK;

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("nsSocketTransport::OnEmpty() [%s:%d %x] nsIPipe=%x.\n", 
         mHostName, mPort, this, aPipe));

  // 
  // The consumer has emptied the pipe...  If the transport was waiting 
  // for room in the pipe, then put it back on the select list... 
  // 
  nsCOMPtr<nsIBufferInputStream> in;
  rv = aPipe->GetInputStream(getter_AddRefs(in));
  if (NS_SUCCEEDED(rv) && in == mReadPipeIn) {
    // Enter the socket transport lock... 
    nsAutoLock aLock(mLock); 
  
    if (GetFlag(eSocketRead_Wait)) {
      ClearFlag(eSocketRead_Wait);
      mSelectFlags |= PR_POLL_READ;
      rv = mService->AddToWorkQ(this);
    }
  }

  return rv;
}

NS_IMETHODIMP 
nsSocketTransport::OnClose(nsIPipe* aPipe) 
{ 
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
  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("nsSocketTransport::OnStartLookup(...) [%s:%d %x].  Host is %s\n", 
         mHostName, mPort, this, aHostName));

  return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::OnFound(nsISupports *aContext, 
                           const char* aHostName,
                           nsHostEnt *aHostEnt) 
{
  // Enter the socket transport lock...
  nsAutoLock lock(mLock);
  nsresult rv = NS_OK;

  if (aHostEnt->hostEnt.h_addr_list
      && aHostEnt->hostEnt.h_addr_list[0]) {
    memcpy(&mNetAddress.inet.ip, aHostEnt->hostEnt.h_addr_list[0], 
           sizeof(mNetAddress.inet.ip));

    PR_LOG(gSocketLog, PR_LOG_DEBUG, 
           ("nsSocketTransport::OnFound(...) [%s:%d %x]."
            "  DNS lookup succeeded => %s (%d.%d.%d.%d)\n", 
            mHostName, mPort, this,
            aHostEnt->hostEnt.h_name,
            mNetAddress.inet.ip & 0xff,
            (mNetAddress.inet.ip >> 8) & 0xff,
            (mNetAddress.inet.ip >> 16) & 0xff,
            (mNetAddress.inet.ip >> 24) & 0xff));
  } else {
    // XXX: What should happen here?  The GetHostByName(...) succeeded but 
    //      there are *no* A records...
    rv = NS_ERROR_FAILURE;

    PR_LOG(gSocketLog, PR_LOG_DEBUG, 
           ("nsSocketTransport::OnFound(...) [%s:%d %x]."
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
  nsAutoLock lock(mLock);

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("nsSocketTransport::OnStopLookup(...) [%s:%d %x]."
          "  Status = %x Host is %s\n", 
         mHostName, mPort, this, aStatus, aHostName));

  // Release our reference to the DNS Request...
  mDNSRequest = null_nsCOMPtr();

  // If the lookup failed, set the status...
  if (NS_FAILED(aStatus)) {
    mStatus = aStatus;
  }

  // Start processing the transport again - if necessary...
  if (GetFlag(eSocketDNS_Wait)) {
    ClearFlag(eSocketDNS_Wait);
    mService->AddToWorkQ(this);
  }

  return NS_OK;
}


//
// --------------------------------------------------------------------------
// nsIChannel implementation...
// --------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSocketTransport::GetOriginalURI(nsIURI * *aURL)
{
  nsStdURL *url;
  url = new nsStdURL(nsnull);
  url->SetHost(mHostName);
  url->SetPort(mPort);

  nsresult rv;
  rv = CallQueryInterface(url, aURL);

  return rv;
}

NS_IMETHODIMP
nsSocketTransport::GetURI(nsIURI * *aURL)
{
  return GetOriginalURI(aURL);
}

NS_IMETHODIMP
nsSocketTransport::AsyncOpen(nsIStreamObserver *observer, nsISupports* ctxt)
{
  nsresult rv = NS_OK;

  // Enter the socket transport lock...
  nsAutoLock aLock(mLock);

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::AsyncOpen() [%s:%d %x]\n", 
          mHostName, mPort, this));

  // If a read is already in progress then fail...
  if (GetReadType() != eSocketRead_None) {
    rv = NS_ERROR_IN_PROGRESS;
  }

  // Create a marshalling open observer to receive notifications...
  if (NS_SUCCEEDED(rv)) {
    rv = NS_NewAsyncStreamObserver(observer, NS_CURRENT_EVENTQ,
                                   getter_AddRefs(mOpenObserver));
  }

  if (NS_SUCCEEDED(rv)) {
    // Store the context used for this read...
    mOpenContext = ctxt;

    mOperation = eSocketOperation_Connect;
    SetReadType(eSocketRead_None);

    rv = mService->AddToWorkQ(this);
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::AsyncOpen() [%s:%d %x]. rv = %x.\n",
          mHostName, mPort, this, rv));

  return rv;
}

NS_IMETHODIMP
nsSocketTransport::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                             nsISupports* aContext,
                             nsIStreamListener* aListener)
{
  // XXX deal with startPosition and readCount parameters
  NS_ASSERTION(startPosition == 0, "can't deal with offsets in socket transport");
  nsresult rv = NS_OK;
  
  // Enter the socket transport lock...
  nsAutoLock aLock(mLock);

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::AsyncRead() [%s:%d %x]\t"
          "readCount = %d.\n", 
          mHostName, mPort, this, readCount));

  // If a read is already in progress then fail...
  if (GetReadType() != eSocketRead_None) {
    rv = NS_ERROR_IN_PROGRESS;
  }

  // Create a new non-blocking input stream for reading data into...
  if (NS_SUCCEEDED(rv) && !mReadPipeIn) {
    // XXXbe calling out of module with a lock held...
    rv = NS_NewPipe(getter_AddRefs(mReadPipeIn),
                    getter_AddRefs(mReadPipeOut),
                    this,       // nsIPipeObserver
                    mBufferSegmentSize, mBufferMaxSize);
    if (NS_SUCCEEDED(rv)) {
      rv = mReadPipeIn->SetNonBlocking(PR_TRUE);
    }
    if (NS_SUCCEEDED(rv)) {
      rv = mReadPipeOut->SetNonBlocking(PR_TRUE);
    }
  }

  // Create a marshalling stream listener to receive notifications...
  if (NS_SUCCEEDED(rv)) {
    rv = NS_NewAsyncStreamListener(aListener, NS_CURRENT_EVENTQ,
                                   getter_AddRefs(mReadListener));
  }

  if (NS_SUCCEEDED(rv)) {
    // Store the context used for this read...
    mReadContext = aContext;

    mOperation = eSocketOperation_ReadWrite;
    SetReadType(eSocketRead_Async);

    rv = mService->AddToWorkQ(this);
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::AsyncRead() [%s:%d %x]. rv = %x.\n",
          mHostName, mPort, this, rv));

  return rv;
}


NS_IMETHODIMP
nsSocketTransport::AsyncWrite(nsIInputStream* aFromStream, 
                              PRUint32 startPosition, PRInt32 writeCount,
                              nsISupports* aContext,
                              nsIStreamObserver* aObserver)
{
  // XXX deal with startPosition and writeCount parameters
  nsresult rv = NS_OK;

  // Enter the socket transport lock...
  nsAutoLock aLock(mLock);

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::AsyncWrite() [%s:%d %x]\t"
          "writeCount = %d.\n", 
          mHostName, mPort, this, writeCount));

  // If a write is already in progress then fail...
  if (GetWriteType() != eSocketWrite_None) {
    rv = NS_ERROR_IN_PROGRESS;
  }

  if (NS_SUCCEEDED(rv)) {
    mWritePipeIn = do_QueryInterface(aFromStream, &rv);
    if (NS_FAILED(rv)) {
      // If the input stream does not support nsIBufferInputStream, then
      // an intermediate buffer is necessary to move the data from the
      // stream to the network...
      mWriteFromStream = aFromStream;

      rv = NS_OK;
      // Create the intermediate buffer if necessary...
      if (!mWriteBuffer) {
        mWriteBuffer = (char *)PR_Malloc(MAX_IO_TRANSFER_SIZE+1);
        if (!mWriteBuffer) {
          rv = NS_ERROR_OUT_OF_MEMORY;
        }
      }
      // Reset to buffer index and length.
      mWriteBufferLength = 0;
      mWriteBufferIndex  = 0;
    }
  }

  if (NS_SUCCEEDED(rv)) {
    mWriteContext = aContext;

    // Create a marshalling stream observer to receive notifications...
    if (aObserver) {
      rv = NS_NewAsyncStreamObserver(aObserver, NS_CURRENT_EVENTQ,
                                     getter_AddRefs(mWriteObserver));
    }

    mWriteCount = writeCount;
  }

  if (NS_SUCCEEDED(rv)) {
    mOperation = eSocketOperation_ReadWrite;
    SetWriteType(eSocketWrite_Async);

    rv = mService->AddToWorkQ(this);
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::AsyncWrite() [%s:%d %x]. rv = %x.\n",
          mHostName, mPort, this, rv));

  return rv;
}


NS_IMETHODIMP
nsSocketTransport::OpenInputStream(PRUint32 startPosition, PRInt32 readCount, 
                                   nsIInputStream* *result)
{
  nsresult rv = NS_OK;
  NS_ASSERTION(startPosition == 0, "fix me");

  // Enter the socket transport lock...
  nsAutoLock aLock(mLock);

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::OpenInputStream() [%s:%d %x].\n", 
         mHostName, mPort, this));

  // If a read is already in progress then fail...
  if (GetReadType() != eSocketRead_None) {
    rv = NS_ERROR_IN_PROGRESS;
  }

  // Create a new blocking input stream for reading data into...
  if (NS_SUCCEEDED(rv)) {
    mReadListener = null_nsCOMPtr();
    mReadContext  = null_nsCOMPtr();

    // XXXbe calling out of module with a lock held...
    rv = NS_NewPipe(getter_AddRefs(mReadPipeIn),
                    getter_AddRefs(mReadPipeOut),
                    this,       // nsIPipeObserver
                    mBufferSegmentSize, mBufferMaxSize);
    if (NS_SUCCEEDED(rv)) {
      rv = mReadPipeOut->SetNonBlocking(PR_TRUE);
      *result = mReadPipeIn;
      NS_IF_ADDREF(*result);
    }
  }

  if (NS_SUCCEEDED(rv)) {
    mOperation = eSocketOperation_ReadWrite;
    SetReadType(eSocketRead_Sync);

    rv = mService->AddToWorkQ(this);
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::OpenInputStream() [%s:%d %x].\t"
          "rv = %x.\n",
          mHostName, mPort, this, rv));

  return rv;
}


NS_IMETHODIMP
nsSocketTransport::OpenOutputStream(PRUint32 startPosition, nsIOutputStream* *result)
{
  nsresult rv = NS_OK;
  NS_ASSERTION(startPosition == 0, "fix me");

  // Enter the socket transport lock...
  nsAutoLock aLock(mLock);

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::OpenOutputStream() [%s:%d %x].\n", 
         mHostName, mPort, this));

  // If a write is already in progress then fail...
  if (GetWriteType() != eSocketWrite_None) {
    rv = NS_ERROR_IN_PROGRESS;
  }

  if (NS_SUCCEEDED(rv)) {
    // No observer or write context is available...
    mWriteCount    = 0;
    mWriteObserver = null_nsCOMPtr();
    mWriteContext  = null_nsCOMPtr();

    // We want a pipe here so the caller can "write" into one end
    // and the other end (aWriteStream) gets the data. This data
    // is then written to the underlying socket when nsSocketTransport::doWrite()
    // is called.

    nsCOMPtr<nsIBufferOutputStream> out;
    nsCOMPtr<nsIBufferInputStream>  in;
    // XXXbe calling out of module with a lock held...
    rv = NS_NewPipe(getter_AddRefs(in), getter_AddRefs(out),
                    this,       // nsIPipeObserver
                    mBufferSegmentSize, mBufferMaxSize);
    if (NS_SUCCEEDED(rv)) {
      rv = in->SetNonBlocking(PR_TRUE);
    }

    if (NS_SUCCEEDED(rv)) {
      mWritePipeIn = in;
      *result = out;
      NS_IF_ADDREF(*result);

      mWritePipeOut = out;
    }

    SetWriteType(eSocketWrite_Sync);
  }
/*
  if (NS_SUCCEEDED(rv)) {
    mOperation = eSocketOperation_ReadWrite;
    // Start the crank.
    rv = mService->AddToWorkQ(this);
  }
*/
  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::OpenOutputStream() [%s:%d %x].\t"
          "rv = %x.\n",
          mHostName, mPort, this, rv));

  return rv;
}

NS_IMETHODIMP
nsSocketTransport::GetLoadAttributes(PRUint32 *aLoadAttributes)
{
  *aLoadAttributes = mLoadAttributes;
  return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::SetLoadAttributes(PRUint32 aLoadAttributes)
{
  mLoadAttributes = aLoadAttributes;
  return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::GetContentType(char * *aContentType)
{
  *aContentType = nsnull;
  return NS_ERROR_FAILURE;    // XXX doesn't make sense for transports
}

NS_IMETHODIMP
nsSocketTransport::SetContentType(const char *aContentType)
{
  return NS_ERROR_FAILURE;    // XXX doesn't make sense for transports
}

NS_IMETHODIMP
nsSocketTransport::GetContentLength(PRInt32 *aContentLength)
{
  // The content length is always unknown for transports...
  *aContentLength = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
  NS_ASSERTION(0, "transports shouldn't end up in groups");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSocketTransport::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketTransport::GetOwner(nsISupports * *aOwner)
{
  *aOwner = mOwner.get();
  NS_IF_ADDREF(*aOwner);
  return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::SetOwner(nsISupports * aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
  *aNotificationCallbacks = mCallbacks.get();
  NS_IF_ADDREF(*aNotificationCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
  mCallbacks = aNotificationCallbacks;
  mEventSink = 0;

  // Get a nsIProgressEventSink so that we can fire status/progress on it-
  if (mCallbacks) {
    nsCOMPtr<nsIProgressEventSink> sink;
    nsresult rv = mCallbacks->GetInterface(NS_GET_IID(nsIProgressEventSink),
                                           getter_AddRefs(sink));
    if (NS_SUCCEEDED(rv)) {
      // Now generate a proxied event sink-
      NS_WITH_SERVICE(nsIProxyObjectManager, 
                      proxyMgr, kProxyObjectManagerCID, &rv);
      if (NS_SUCCEEDED(rv)) {
        rv = proxyMgr->GetProxyObject(NS_UI_THREAD_EVENTQ, // primordial thread - should change?
                                      NS_GET_IID(nsIProgressEventSink),
                                      sink,
                                      PROXY_ASYNC | PROXY_ALWAYS,
                                      getter_AddRefs(mEventSink));
      }
    }
  }

  return NS_OK;
}

nsresult
nsSocketTransport::fireStatus(PRUint32 aCode)
{
  // need to optimize this - TODO
  nsXPIDLString tempmesg;
  nsresult rv = GetSocketErrorString(aCode, getter_Copies(tempmesg));

  nsAutoString mesg(tempmesg);
  if (mPrintHost)
    mesg.Append(mPrintHost);
  else
    mesg.Append(mHostName);

  if (NS_FAILED(rv)) return rv;

  return mEventSink ? mEventSink->OnStatus(this,
                                           mReadContext, 
                                           mesg.mUStr) // this gets freed elsewhere.
                    : NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsSocketTransport::IsAlive (PRUint32 seconds, PRBool *alive)
{
    *alive = PR_TRUE;

    if (mSocketFD)
    {
        if (seconds > 0 && mLastActiveTime != PR_INTERVAL_NO_WAIT)
        {
            PRUint32  now = PR_IntervalToSeconds (PR_IntervalNow ());
            PRUint32 last = PR_IntervalToSeconds ( mLastActiveTime );

            if (now - last > seconds)
                *alive = PR_FALSE;
        }

        static char c;
        PRInt32 rval = PR_Read (mSocketFD, &c, 0);
        
        if (rval <= 0)
        {
            PRErrorCode code = PR_GetError ();

            if (rval == 0 || code != PR_WOULD_BLOCK_ERROR)
                *alive = PR_FALSE;
        }
    }
    else
        *alive = PR_FALSE;

    return NS_OK;
}

//TODO l10n and i18n stuff here!
nsresult
nsSocketTransport::GetSocketErrorString(PRUint32 iCode, 
        PRUnichar** oString) const
{
    nsresult rv = NS_ERROR_FAILURE;
    if (!oString)
        return NS_ERROR_NULL_POINTER;

    *oString = nsnull;

    switch (iCode) /* these are currently just nsSocketState 
                      (as in nsSocketTransport.h) */
    {
        case eSocketState_Created: 
        case eSocketState_WaitDNS:
            {
                static nsAutoString mesg("Resolving host ");
                *oString = mesg.ToNewUnicode();
                if (!*oString) return NS_ERROR_OUT_OF_MEMORY;
                rv = NS_OK;
            }
            break;
        case eSocketState_Connected:
            {
                static nsAutoString mesg("Connected to ");
                *oString = mesg.ToNewUnicode();
                if (!*oString) return NS_ERROR_OUT_OF_MEMORY;
                rv = NS_OK;
            }
            break;
        case eSocketState_WaitReadWrite:
            {
                static nsAutoString frommesg("Transferring data from ");
                static nsAutoString tomesg("Sending request to ");
                *oString = (mWriteContext == nsnull) ? 
                    frommesg.ToNewUnicode() : tomesg.ToNewUnicode();
                if (!*oString) return NS_ERROR_OUT_OF_MEMORY;
                rv = NS_OK;
            }
            break;
        case eSocketState_WaitConnect:
            {
                static nsAutoString mesg("Connecting to ");
                *oString = mesg.ToNewUnicode();
                if (!*oString) return NS_ERROR_OUT_OF_MEMORY;
                rv = NS_OK;
            }
            break;
        case eSocketState_Closed:
        case eSocketState_Done:
        case eSocketState_Timeout:
        case eSocketState_Error:
        case eSocketState_Max:
        default:
            return rv; // just return error, ie no status strings for this case
            break;
    }
    return rv;
}
