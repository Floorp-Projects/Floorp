/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#if defined(XP_PC)
#include <windows.h> // Interlocked increment...
#endif

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
#include "nsString2.h"
#include "nsISocketProvider.h"
#include "nsISocketProviderService.h"

static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kSocketProviderService, NS_SOCKETPROVIDERSERVICE_CID);


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
    eSocketState_Connected,     // WaitConenct    -> Connected
    eSocketState_WaitReadWrite, // Connected      -> WaitReadWrite
    eSocketState_Done,          // WaitReadWrite  -> Done
    eSocketState_Connected,     // Done           -> Connected
    eSocketState_Error,         // Timeout        -> Error
    eSocketState_Closed         // Error          -> Closed
  },
};

//
// This is the timeout value (in milliseconds) for calls to PR_Connect(...).
//
// The gConnectTimeout gets initialized the first time a nsSocketTransport
// is created...  This interval is then passed to all PR_Connect() calls...
//
#define CONNECT_TIMEOUT_IN_MS 20

static int gTimeoutIsInitialized = 0;
static PRIntervalTime gConnectTimeout = PR_INTERVAL_NO_TIMEOUT;

//
// This is the global buffer used by all nsSocketTransport instances when
// reading from or writing to the network.
//
static char gIOBuffer[MAX_IO_BUFFER_SIZE];

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

nsSocketTransport::nsSocketTransport()
{
  NS_ASSERTION(MAX_IO_TRANSFER_SIZE <= MAX_IO_BUFFER_SIZE, "bad MAX_IO_TRANSFER_SIZE");
  NS_INIT_REFCNT();

  PR_INIT_CLIST(&mListLink);

  mHostName     = nsnull;
  mPort         = 0;
  mSocketType   = nsnull;
  mSocketFD     = nsnull;
  mLock         = nsnull;

  mSuspendCount = 0;
  mCancelOperation = PR_FALSE;

  mReadWriteState = 0;  
  SetReadType (eSocketRead_None);
  SetWriteType(eSocketWrite_None);

  mCurrentState = eSocketState_Created;
  mOperation    = eSocketOperation_None;
  mSelectFlags  = 0;

  mWriteCount     = 0;
  mSourceOffset   = 0;
  mService        = nsnull;
  mLoadAttributes = LOAD_NORMAL;

  //
  // Set up Internet defaults...
  //
  memset(&mNetAddress, 0, sizeof(mNetAddress));
    PR_InitializeNetAddr(PR_IpAddrNull, 0, &mNetAddress);

  //
  // Initialize the global connect timeout value if necessary...
  //
  if (0 == gTimeoutIsInitialized) {
    gConnectTimeout = PR_MillisecondsToInterval(CONNECT_TIMEOUT_IN_MS);
    gTimeoutIsInitialized = 1;
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
         ("Creating nsSocketTransport [this=%x].\n", this));

}


nsSocketTransport::~nsSocketTransport()
{
  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("Deleting nsSocketTransport [this=%x].\n", this));

  // Release the nsCOMPtrs...
  //
  // It is easier to debug problems if these are released before the 
  // nsSocketTransport context is lost...
  //
  mReadListener = null_nsCOMPtr();
  mReadContext  = null_nsCOMPtr();
#ifndef NSPIPE2
  mReadStream   = null_nsCOMPtr();
  mReadBuffer   = null_nsCOMPtr();
#else
  mReadPipeIn   = null_nsCOMPtr();
  mReadPipeOut  = null_nsCOMPtr();
#endif

  mWriteObserver = null_nsCOMPtr();
  mWriteContext  = null_nsCOMPtr();
#ifndef NSPIPE2
  mWriteStream   = null_nsCOMPtr();
  mWriteBuffer   = null_nsCOMPtr();
#else
  mWritePipeIn   = null_nsCOMPtr();
  mWritePipeOut  = null_nsCOMPtr();
#endif
    
  NS_IF_RELEASE(mService);

  if (mHostName) {
    nsCRT::free(mHostName);
  }

  if (mSocketType) {
    nsCRT::free(mSocketType);
  }

  if (mSocketFD) {
    PR_Close(mSocketFD);
    mSocketFD = nsnull;
  }

  if (mLock) {
    PR_DestroyLock(mLock);
    mLock = nsnull;
  }
}


nsresult nsSocketTransport::Init(nsSocketTransportService* aService,
                                 const char* aHost, 
                                 PRInt32 aPort,
                                 const char* aSocketType)
{
  nsresult rv = NS_OK;

  mService = aService;
  NS_ADDREF(mService);

  mPort = aPort;
  if (aHost) {
    mHostName = nsCRT::strdup(aHost);
    if (!mHostName) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  } 
  // aHost was nsnull...
  else {
    rv = NS_ERROR_NULL_POINTER;
  }

  if (aSocketType) {
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

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("Initializing nsSocketTransport [this=%x].  rv = %x. \t"
          "aHost = %s.\t"
          "aPort = %d.\n",
          this, rv, aHost, aPort));

  return rv;
}


nsresult nsSocketTransport::Process(PRInt16 aSelectFlags)
{
  nsresult rv = NS_OK;
  PRBool done = PR_FALSE;

  //
  // Enter the socket transport lock...  
  // This lock protects access to socket transport member data...
  //
  nsAutoLock aLock(mLock);

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::Process() [this=%x].\t"
          "aSelectFlags = %x.\t"
          "CurrentState = %d\n",
          this, aSelectFlags, mCurrentState));

  //
  // Check for an error during PR_Poll(...)
  //
  if (PR_POLL_EXCEPT & aSelectFlags) {
    PR_LOG(gSocketLog, PR_LOG_ERROR, 
           ("Operation failed via PR_POLL_EXCEPT. [this=%x].\n", this));
    // An error has occurred, so cancel the read and/or write operation...
    mCurrentState = eSocketState_Error;
    rv = NS_BINDING_FAILED;
  }

  while (!done)
  {
    //
    // If the transport has been suspended, then return NS_OK immediately...
    // This removes the transport from the select list...
    //
    if (mSuspendCount) {
      PR_LOG(gSocketLog, PR_LOG_DEBUG, 
             ("Transport [this=%x] is suspended.\n", this));
  
      done = PR_TRUE;
      rv = NS_OK;
      continue;
    }

    if (mCancelOperation) {
      PR_LOG(gSocketLog, PR_LOG_DEBUG, 
             ("Transport [this=%x] has been cancelled.\n", this));

      // Cancel any read and/or write requests...
      SetFlag(eSocketRead_Done);
      SetFlag(eSocketWrite_Done);
      mCurrentState = eSocketState_Done;

      rv = NS_BINDING_ABORTED;
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
               ("Transport [this=%x] is in error state.\n", this));

        // Cancel any read and/or write requests...
        SetFlag(eSocketRead_Done);
        SetFlag(eSocketWrite_Done);

        CloseConnection();
        //
        // Fall into the Done state...
        //
      case eSocketState_Done:
        PR_LOG(gSocketLog, PR_LOG_DEBUG, 
               ("Transport [this=%x] is in done state.\n", this));

        if (GetFlag(eSocketRead_Done)) {
          // Fire a notification that the read has finished...
          if (mReadListener) {
            mReadListener->OnStopRequest(this, mReadContext, rv, nsnull);
            mReadListener = null_nsCOMPtr();
            mReadContext  = null_nsCOMPtr();
          }
#ifndef NSPIPE2
          mReadStream = null_nsCOMPtr();
          // XXX: The buffer should be reused...
          mReadBuffer = null_nsCOMPtr();
#else
          mReadPipeIn  = null_nsCOMPtr();
          mReadPipeOut = null_nsCOMPtr();
#endif
          SetReadType(eSocketRead_None);
          ClearFlag(eSocketRead_Done);
        }

        if (GetFlag(eSocketWrite_Done)) {
          // Fire a notification that the write has finished...
          if (mWriteObserver) {
            mWriteObserver->OnStopRequest(this, mWriteContext, rv, nsnull);
            mWriteObserver = null_nsCOMPtr();
            mWriteContext  = null_nsCOMPtr();
          }
#ifndef NSPIPE2
          mWriteStream = null_nsCOMPtr();
          mWriteBuffer = null_nsCOMPtr();
#else
          mWritePipeIn  = null_nsCOMPtr();
          mWritePipeOut = null_nsCOMPtr();
#endif
          SetWriteType(eSocketWrite_None);
          ClearFlag(eSocketWrite_Done);
        }

        //
        // Are all read and write requests done?
        //
        if ((GetReadType()  == eSocketRead_None) && 
            (GetWriteType() == eSocketWrite_None)) 
        {
          mCurrentState = gStateTable[mOperation][mCurrentState];
          mOperation    = eSocketOperation_None;
          done = PR_TRUE;
        } else {
          // Still reading or writing...
          mCurrentState = eSocketState_WaitReadWrite;
        }
        continue;

      case eSocketState_WaitDNS:
        rv = doResolveHost();
        break;

      case eSocketState_WaitConnect:
        rv = doConnection(aSelectFlags);
        break;

      case eSocketState_WaitReadWrite:
        // Process the read request...
        if (GetReadType() != eSocketRead_None) {
          rv = doRead(aSelectFlags);
          if (NS_OK == rv) {
            SetFlag(eSocketRead_Done);
            break;
          }
        }
        // Process the write request...
        if ((NS_SUCCEEDED(rv) || rv == NS_BASE_STREAM_WOULD_BLOCK)
            && (GetWriteType() != eSocketWrite_None)) {
          rv = doWrite(aSelectFlags);
          if (NS_OK == rv) {
            SetFlag(eSocketWrite_Done);
            break;
          }
        }
        break;

      case eSocketState_Timeout:
        NS_ASSERTION(0, "Unexpected state...");
        rv = NS_ERROR_FAILURE;
        break;

      default:
        NS_ASSERTION(0, "Unexpected state...");
        rv = NS_ERROR_FAILURE;
        break;
    }
    //
    // If the current state has successfully completed, then move to the
    // next state for the current operation...
    //
    if (NS_OK == rv) {
      mCurrentState = gStateTable[mOperation][mCurrentState];
    } 
    else if (NS_BASE_STREAM_WOULD_BLOCK == rv) {
      done = PR_TRUE;
    }
    else if (NS_FAILED(rv)) {
      mCurrentState = eSocketState_Error;
    }
    // 
    // Any select flags are *only* valid the first time through the loop...
    // 
    aSelectFlags = 0;
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::Process() [this=%x]. rv = %x.\t"
          "CurrentState = %d\n\n",
          this, rv, mCurrentState));

  return rv;
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
  PRStatus status;
  nsresult rv = NS_OK;

  NS_ASSERTION(eSocketState_WaitDNS == mCurrentState, "Wrong state.");

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::doResolveHost() [this=%x].\n", 
          this));

  //
  // Initialize the port used for the connection...
  //
  // XXX: The list of ports must be restricted - see net_bad_ports_table[] in 
  //      mozilla/network/main/mkconect.c
  //
  mNetAddress.inet.port = PR_htons(mPort);

  //
  // Resolve the address of the given host...
  //
  char dbbuf[PR_NETDB_BUF_SIZE];
  PRHostEnt hostEnt;

  if (nsString2::IsDigit(mHostName[0])) {
      PRNetAddr *netAddr = (PRNetAddr*)nsAllocator::Alloc(sizeof(PRNetAddr));
      status = PR_StringToNetAddr(mHostName, netAddr);
      if (PR_SUCCESS != status) {
          rv = NS_ERROR_UNKNOWN_HOST; // check this!
      }
      status = PR_GetHostByAddr(netAddr, dbbuf, sizeof(dbbuf), &hostEnt);
      nsAllocator::Free(netAddr);
  } else {
      status = PR_GetHostByName(mHostName, dbbuf, sizeof(dbbuf), &hostEnt);
  }

  if (PR_SUCCESS == status) {
    if (hostEnt.h_addr_list) {
      memcpy(&mNetAddress.inet.ip, hostEnt.h_addr_list[0], 
                                   sizeof(mNetAddress.inet.ip));
    } else {
      // XXX: What should happen here?  The GetHostByName(...) succeeded but 
      //      there are *no* A records...
    }

    PR_LOG(gSocketLog, PR_LOG_DEBUG, 
           ("Host name resolved [this=%x].  Name is %s\n", this, mHostName));
  } 
  // DNS lookup failed...
  else {
    PR_LOG(gSocketLog, PR_LOG_ERROR, 
           ("Host name resolution FAILURE [this=%x].\n", this));

    rv = NS_ERROR_UNKNOWN_HOST;
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::doResolveHost() [this=%x].\t"
          "rv = %x.\n\n",
          this, rv));

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
         ("+++ Entering nsSocketTransport::doConnection() [this=%x].\t"
          "aSelectFlags = %x.\n",
          this, aSelectFlags));

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
        rv = pProvider->NewSocket(&mSocketFD);
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
                 ("Connection Refused [this=%x].  PRErrorCode = %x\n",
                  this, code));

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
             ("Connection Refused via PR_POLL_EXCEPT. [this=%x].\n", this));

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
         ("--- Leaving nsSocketTransport::doConnection() [this=%x].\t"
          "rv = %x.\n\n",
          this, rv));

  return rv;
}


NS_METHOD
nsReadFromSocket(void* closure,
                 char* toRawSegment,
                 PRUint32 offset,
                 PRUint32 count,
                 PRUint32 *readCount)
{
  nsresult rv = NS_OK;
  PRInt32 len;
  PRErrorCode code;

  PRFileDesc* fd = (PRFileDesc*)closure;

  *readCount = 0;

  len = PR_Read(fd, toRawSegment, count);
  if (len > 0) {
    *readCount = (PRUint32)len;
  } 
  //
  // The read operation has completed...
  //
  else if (len == 0) {
    rv = NS_BASE_STREAM_EOF;
  }
  //
  // Error...
  //
  else {
    code = PR_GetError();

    if (PR_WOULD_BLOCK_ERROR == code) {
      rv = NS_BASE_STREAM_WOULD_BLOCK;
    } 
    else {
      PR_LOG(gSocketLog, PR_LOG_ERROR, 
             ("PR_Read() failed. PRErrorCode = %x\n", code));

      // XXX: What should this error code be?
      rv = NS_ERROR_FAILURE;
    }
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("nsReadFromSocket [fd=%x].  rv = %x. Buffer space = %d.  Bytes read =%d\n",
          fd, rv, count, *readCount));

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
  PRErrorCode code;

  PRFileDesc* fd = (PRFileDesc*)closure;

  *writeCount = 0;

  len = PR_Write(fd, fromRawSegment, count);
  if (len > 0) {
    *writeCount = (PRUint32)len;
  }
  //
  // Error...
  //
  else {
    code = PR_GetError();

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
  PRUint32 totalBytesWritten;
  nsresult rv = NS_OK;

  NS_ASSERTION(eSocketState_WaitReadWrite == mCurrentState, "Wrong state.");
  NS_ASSERTION(GetReadType() != eSocketRead_None, "Bad Read Type!");

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::doRead() [this=%x].\t"
          "aSelectFlags = %x.\t",
          this, aSelectFlags));

  //
  // Fill the stream with as much data from the network as possible...
  //
  //
  totalBytesWritten = 0;
  //
  // Release the transport lock...  WriteSegments(...) aquires the nsBuffer
  // lock which could cause a deadlock by blocking the socket transport 
  // thread
  //
  PR_Unlock(mLock);
#ifndef NSPIPE2
  rv = mReadBuffer->WriteSegments(nsReadFromSocket, (void*)mSocketFD, 
                                  MAX_IO_TRANSFER_SIZE, &totalBytesWritten);
#else
  rv = mReadPipeOut->WriteSegments(nsReadFromSocket, (void*)mSocketFD, 
                                   MAX_IO_TRANSFER_SIZE, &totalBytesWritten);
#endif
  PR_Lock(mLock);

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("WriteSegments [fd=%x].  rv = %x. Bytes read =%d\n",
         mSocketFD, rv, totalBytesWritten));

  //
  // Deal with the possible return values...
  //
  if (NS_BASE_STREAM_EOF == rv) {
    mSelectFlags &= (~PR_POLL_READ);
    rv = NS_OK;
  } 
  else if (NS_SUCCEEDED(rv)) {
    // continue to return WOULD_BLOCK until we've completely finished this read
    rv = NS_BASE_STREAM_WOULD_BLOCK;
  }

  //
  // Fire a single OnDataAvaliable(...) notification once as much data has
  // been filled into the stream as possible...
  //
  if (totalBytesWritten) {
    if (mReadListener) {
      nsresult rv1;

#ifndef NSPIPE2
      rv1 = mReadListener->OnDataAvailable(this, mReadContext, mReadStream, 
                                           mSourceOffset, 
                                           totalBytesWritten);
#else
      rv1 = mReadListener->OnDataAvailable(this, mReadContext, mReadPipeIn, 
                                           mSourceOffset, 
                                           totalBytesWritten);
#endif
      //
      // If the consumer returns failure, then cancel the operation...
      //
      if (NS_FAILED(rv1)) {
        rv = rv1;
      }
    }
#ifndef NSPIPE2
    else if (GetReadType() == eSocketRead_Sync) {
      nsAutoCMonitor mon(mReadBuffer);
      mon.Notify();
    }
#endif
    mSourceOffset += totalBytesWritten;
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::doRead() [this=%x]. rv = %x.\t"
          "Total bytes read: %d\n\n",
          this, rv, totalBytesWritten));

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
  PRUint32 totalBytesRead;
  nsresult rv = NS_OK;

  NS_ASSERTION(eSocketState_WaitReadWrite == mCurrentState, "Wrong state.");
  NS_ASSERTION(GetWriteType() != eSocketWrite_None, "Bad Write Type!");

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::doWrite() [this=%x].\t"
          "aSelectFlags = %x.\t"
          "mWriteCount = %d\n",
          this, aSelectFlags, mWriteCount));

  totalBytesRead = 0;

#ifndef NSPIPE2
  if (mWriteBuffer)
#else
  if (mWritePipeIn)
#endif
  {
    rv = doWriteFromBuffer(&totalBytesRead);
  }

  else {
    rv = doWriteFromStream(&totalBytesRead);
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::doWrite() [this=%x]. rv = %x.\t"
          "Total bytes written: %d\n\n",
          this, rv, totalBytesRead));

  return rv;
}


nsresult nsSocketTransport::doWriteFromBuffer(PRUint32 *aCount)
{
  nsresult rv;

  *aCount = 0;
  //
  // Release the transport lock...  ReadSegments(...) aquires the nsBuffer
  // lock which could cause a deadlock by blocking the socket transport 
  // thread
  //
  PR_Unlock(mLock);
#ifndef NSPIPE2
  rv = mWriteBuffer->ReadSegments(nsWriteToSocket, (void*)mSocketFD, 
                                  MAX_IO_TRANSFER_SIZE, aCount);
#else
  PRUint32 transferCount = mWriteCount >= 0 ? PR_MIN(mWriteCount, MAX_IO_TRANSFER_SIZE) : MAX_IO_TRANSFER_SIZE;
  rv = mWritePipeIn->ReadSegments(nsWriteToSocket, (void*)mSocketFD, 
                                  transferCount, aCount);
#endif
  PR_Lock(mLock);

  if (mWriteCount > 0) {
    NS_ASSERTION(mWriteCount >= (PRInt32)*aCount, "wrote more than humanly possible");
    mWriteCount -= *aCount;
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
        ("ReadSegments [fd=%x].  rv = %x. Bytes written =%d\n",
         mSocketFD, rv, *aCount));

  if (NS_BASE_STREAM_EOF == rv) {
    //
    // The write operation has completed...
    //
    mSelectFlags &= (~PR_POLL_WRITE);
    rv = NS_OK;
  }
  else if (NS_SUCCEEDED(rv) || rv == NS_BASE_STREAM_WOULD_BLOCK) {
    //
    // If the buffer is empty, then notify the reader and stop polling
    // for write until there is data in the buffer.  See the OnWrite()
    // notification...
    //
    if (mWriteCount == 0) {
      SetFlag(eSocketWrite_Wait);
      mSelectFlags &= (~PR_POLL_WRITE);

#ifndef NSPIPE2
      PR_Unlock(mLock);
      {
        nsAutoCMonitor mon(mWriteBuffer);
        mon.Notify();
      }
      PR_Lock(mLock);
#endif
    }

    // continue to return WOULD_BLOCK until we've completely finished 
    // this write...
    rv = NS_BASE_STREAM_WOULD_BLOCK;
  }

  return rv;
}


nsresult nsSocketTransport::doWriteFromStream(PRUint32 *aCount)
{
  PRUint32 bytesRead;
  PRInt32 maxBytesToRead, len;
  PRErrorCode code;
  nsresult rv = NS_OK;

  *aCount = 0;
  while (NS_OK == rv) {
    // Determine the amount of data to read from the input stream...
    if ((mWriteCount > 0) && (mWriteCount < MAX_IO_TRANSFER_SIZE)) {
      maxBytesToRead = mWriteCount;
    } else {
      maxBytesToRead = sizeof(gIOBuffer);
    }

    bytesRead = 0;
#ifndef NSPIPE2
    rv = mWriteStream->Read(gIOBuffer, maxBytesToRead, &bytesRead);
#else
    rv = mWriteFromStream->Read(gIOBuffer, maxBytesToRead, &bytesRead);
#endif
    if ((NS_SUCCEEDED(rv) || rv == NS_BASE_STREAM_WOULD_BLOCK) && bytesRead) {
      // Update the counters...
      *aCount += bytesRead;
      if (mWriteCount > 0) {
        mWriteCount -= bytesRead;
      }
      // Write the data to the socket...
      len = PR_Write(mSocketFD, gIOBuffer, bytesRead);

      if (len < 0) {
        code = PR_GetError();

        if (PR_WOULD_BLOCK_ERROR == code) {
          rv = NS_BASE_STREAM_WOULD_BLOCK;
        } 
        else {
          PR_LOG(gSocketLog, PR_LOG_ERROR, 
                 ("PR_Write() failed. [this=%x].  PRErrorCode = %x\n",
                 this, code));

          // XXX: What should this error code be?
          rv = NS_ERROR_FAILURE;
        }
      }
    }
    //
    // The write operation has completed...
    //
    if ((mWriteCount == 0) || (NS_BASE_STREAM_EOF == rv) ) {
      mSelectFlags &= (~PR_POLL_WRITE);
      rv = NS_OK;
      break;
    }
  }
  return rv;  
}


nsresult nsSocketTransport::CloseConnection(void)
{
  PRStatus status;
  nsresult rv = NS_OK;

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
  
  return rv;
}


//
// --------------------------------------------------------------------------
// nsISupports implementation...
// --------------------------------------------------------------------------
//

NS_IMPL_THREADSAFE_ADDREF(nsSocketTransport);
NS_IMPL_THREADSAFE_RELEASE(nsSocketTransport);

NS_IMETHODIMP
nsSocketTransport::QueryInterface(const nsIID& aIID, void* *aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER; 
  } 
  if (aIID.Equals(nsCOMTypeInfo<nsIChannel>::GetIID()) ||
      aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsIChannel*, this); 
    NS_ADDREF_THIS(); 
    return NS_OK; 
  } 
#ifndef NSPIPE2
  if (aIID.Equals(nsCOMTypeInfo<nsIBufferObserver>::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsIBufferObserver*, this); 
    NS_ADDREF_THIS(); 
    return NS_OK; 
  } 
#else
  if (aIID.Equals(nsCOMTypeInfo<nsIPipeObserver>::GetIID())) {
    *aInstancePtr = NS_STATIC_CAST(nsIPipeObserver*, this); 
    NS_ADDREF_THIS(); 
    return NS_OK; 
  }
#endif
  return NS_NOINTERFACE; 
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
         ("Canceling nsSocketTransport [this=%x].  rv = %x\n",
          this, rv));

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
         ("Suspending nsSocketTransport [this=%x].  rv = %x\t"
          "mSuspendCount = %d.\n",
          this, rv, mSuspendCount));

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
         ("Resuming nsSocketTransport [this=%x].  rv = %x\t"
          "mSuspendCount = %d.\n",
          this, rv, mSuspendCount));

  return rv;
}

//
// --------------------------------------------------------------------------
// nsIPipeObserver implementation...
// --------------------------------------------------------------------------
//
NS_IMETHODIMP
#ifndef NSPIPE2
nsSocketTransport::OnFull(nsIBuffer* aBuffer)
#else
nsSocketTransport::OnFull(nsIPipe* aPipe)
#endif
{
#ifndef NSPIPE2
  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("nsSocketTransport::OnFull() [this=%x] nsIBuffer=%x.\n", 
         this, aBuffer));
#else
  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("nsSocketTransport::OnFull() [this=%x] nsIPipe=%x.\n", 
         this, aPipe));
#endif

#ifndef NSPIPE2
  if (aBuffer == mReadBuffer.get())
#else
  nsCOMPtr<nsIBufferInputStream> in;
  nsresult rv = aPipe->GetInputStream(getter_AddRefs(in));
  if (NS_SUCCEEDED(rv) && in == mReadPipeIn)
#endif
  {
    NS_ASSERTION(!GetFlag(eSocketRead_Wait), "Already waiting!");

    SetFlag(eSocketRead_Wait);
    mSelectFlags &= (~PR_POLL_READ);
  }

  return NS_OK;
}


NS_IMETHODIMP
#ifndef NSPIPE2
nsSocketTransport::OnWrite(nsIBuffer* aBuffer, PRUint32 aCount)
#else
nsSocketTransport::OnWrite(nsIPipe* aPipe, PRUint32 aCount)
#endif
{
  nsresult rv = NS_OK;

#ifndef NSPIPE2
  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("nsSocketTransport::OnWrite() [this=%x]. nsIBuffer=%x Count=%d\n", 
         this, aBuffer, aCount));
#else
  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("nsSocketTransport::OnWrite() [this=%x]. nsIPipe=%x Count=%d\n", 
         this, aPipe, aCount));
#endif

#ifndef NSPIPE2
  if (aBuffer == mWriteBuffer.get())
#else
  nsCOMPtr<nsIBufferOutputStream> out;
  rv = aPipe->GetOutputStream(getter_AddRefs(out));
  if (NS_SUCCEEDED(rv) && out == mWritePipeOut)
#endif
  {
    // Enter the socket transport lock...
    nsAutoLock aLock(mLock);

    if (mWriteCount >= 0) {
      mWriteCount += aCount;
    }
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
#ifndef NSPIPE2
nsSocketTransport::OnEmpty(nsIBuffer* aBuffer)
#else
nsSocketTransport::OnEmpty(nsIPipe* aPipe)
#endif
{
  nsresult rv = NS_OK;

#ifndef NSPIPE2
  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("nsSocketTransport::OnEmpty() [this=%x] nsIBuffer=%x.\n", 
         this, aBuffer));
#else
  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("nsSocketTransport::OnEmpty() [this=%x] nsIPipe=%x.\n", 
         this, aPipe));
#endif

#ifndef NSPIPE2
  if (aBuffer == mReadBuffer.get())
#else
  nsCOMPtr<nsIBufferInputStream> in;
  rv = aPipe->GetInputStream(getter_AddRefs(in));
  if (NS_SUCCEEDED(rv) && in == mReadPipeIn)
#endif
  {
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


//
// --------------------------------------------------------------------------
// nsIChannel implementation...
// --------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSocketTransport::GetURI(nsIURI * *aURL)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketTransport::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                             nsISupports* aContext,
                             nsIStreamListener* aListener)
{
  // XXX deal with startPosition and readCount parameters
  nsresult rv = NS_OK;
  
  // Enter the socket transport lock...
  nsAutoLock aLock(mLock);

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::AsyncRead() [this=%x]\t"
          "readCount = %d.\n", 
          this, readCount));

  // If a read is already in progress then fail...
  if (GetReadType() != eSocketRead_None) {
    rv = NS_ERROR_IN_PROGRESS;
  }

  // Create a new non-blocking input stream for reading data into...
#ifndef NSPIPE2
  if (NS_SUCCEEDED(rv) && !mReadStream) {
    rv = NS_NewBuffer(getter_AddRefs(mReadBuffer), MAX_IO_BUFFER_SIZE/2, 
                      2*MAX_IO_BUFFER_SIZE, this);

    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIBufferInputStream> newStream;

      rv = NS_NewBufferInputStream(getter_AddRefs(newStream), 
                                   mReadBuffer, PR_FALSE);
      mReadStream = newStream;
    }
  }
#else
  if (NS_SUCCEEDED(rv) && !mReadPipeIn) {
    rv = NS_NewPipe(getter_AddRefs(mReadPipeIn),
                    getter_AddRefs(mReadPipeOut),
                    this,       // nsIPipeObserver
                    NS_SOCKET_TRANSPORT_SEGMENT_SIZE,
                    NS_SOCKET_TRANSPORT_BUFFER_SIZE);
    if (NS_SUCCEEDED(rv)) {
      rv = mReadPipeIn->SetNonBlocking(PR_TRUE);
    }
    if (NS_SUCCEEDED(rv)) {
      rv = mReadPipeOut->SetNonBlocking(PR_TRUE);
    }
  }
#endif

  // Create a marshalling stream listener to receive notifications...
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIEventQueue> eventQ;

    // Get the event queue of the current thread...
    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
    if (NS_SUCCEEDED(rv)) {
      rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), 
                                              getter_AddRefs(eventQ));
    }
    if (NS_SUCCEEDED(rv)) {
      rv = NS_NewAsyncStreamListener(getter_AddRefs(mReadListener), 
                                     eventQ, aListener);
    }
  }

  if (NS_SUCCEEDED(rv)) {
    // Store the context used for this read...
    mReadContext = aContext;

    mOperation = eSocketOperation_ReadWrite;
    SetReadType(eSocketRead_Async);

    rv = mService->AddToWorkQ(this);
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::AsyncRead() [this=%x]. rv = %x.\n",
          this, rv));

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
         ("+++ Entering nsSocketTransport::AsyncWrite() [this=%x]\t"
          "writeCount = %d.\n", 
          this, writeCount));

  // If a write is already in progress then fail...
  if (GetWriteType() != eSocketWrite_None) {
    rv = NS_ERROR_IN_PROGRESS;
  }

  if (NS_SUCCEEDED(rv)) {
#ifndef NSPIPE2
    mWriteStream  = aFromStream;
#else
    mWriteFromStream = aFromStream;
#if 0
    // Note that the following assignment can fail, but we'll deal with 
    // it in doWrite:
    mWritePipeIn = do_QueryInterface(aFromStream);
    if (mWritePipeIn) {
      rv = mWritePipeIn->SetNonBlocking(PR_TRUE);
    }
#endif
#endif
    mWriteContext = aContext;

    // Create a marshalling stream observer to receive notifications...
    if (aObserver) {
      nsCOMPtr<nsIEventQueue> eventQ;

      // Get the event queue of the current thread...
      NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
      if (NS_SUCCEEDED(rv)) {
        rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), 
                                                getter_AddRefs(eventQ));
      }
      if (NS_SUCCEEDED(rv)) {
        rv = NS_NewAsyncStreamObserver(getter_AddRefs(mWriteObserver), 
                                       eventQ, aObserver);
      }
    }

    mWriteCount = writeCount;
  }

  if (NS_SUCCEEDED(rv)) {
    mOperation = eSocketOperation_ReadWrite;
    SetWriteType(eSocketWrite_Async);

    rv = mService->AddToWorkQ(this);
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::AsyncWrite() [this=%x]. rv = %x.\n",
          this, rv));

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
         ("+++ Entering nsSocketTransport::OpenInputStream() [this=%x].\n", 
         this));

  // If a read is already in progress then fail...
  if (GetReadType() != eSocketRead_None) {
    rv = NS_ERROR_IN_PROGRESS;
  }

  // Create a new blocking input stream for reading data into...
  if (NS_SUCCEEDED(rv)) {
    mReadListener = null_nsCOMPtr();
    mReadContext  = null_nsCOMPtr();

#ifndef NSPIPE2
    rv = NS_NewBuffer(getter_AddRefs(mReadBuffer), MAX_IO_BUFFER_SIZE/2, 
                      2*MAX_IO_BUFFER_SIZE, this);

    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIBufferInputStream> newStream;

      rv = NS_NewBufferInputStream(getter_AddRefs(newStream), 
                                   mReadBuffer, PR_TRUE);
      mReadStream = newStream;
      *result = newStream;
      NS_IF_ADDREF(*result);
    }
#else
    rv = NS_NewPipe(getter_AddRefs(mReadPipeIn),
                    getter_AddRefs(mReadPipeOut),
                    this,       // nsIPipeObserver
                    NS_SOCKET_TRANSPORT_SEGMENT_SIZE,
                    NS_SOCKET_TRANSPORT_BUFFER_SIZE);
    if (NS_SUCCEEDED(rv)) {
      rv = mReadPipeOut->SetNonBlocking(PR_TRUE);
      *result = mReadPipeIn;
      NS_IF_ADDREF(*result);
    }
#endif
  }

  if (NS_SUCCEEDED(rv)) {
    mOperation = eSocketOperation_ReadWrite;
    SetReadType(eSocketRead_Sync);

    rv = mService->AddToWorkQ(this);
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::OpenInputStream() [this=%x].\t"
          "rv = %x.\n",
          this, rv));

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
         ("+++ Entering nsSocketTransport::OpenOutputStream() [this=%x].\n", 
         this));

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
#ifndef NSPIPE2
    rv = NS_NewPipe(getter_AddRefs(in), getter_AddRefs(out),
                    MAX_IO_BUFFER_SIZE, MAX_IO_BUFFER_SIZE, PR_TRUE, this);
#else
    rv = NS_NewPipe(getter_AddRefs(in), getter_AddRefs(out),
                    this,       // nsIPipeObserver
                    NS_SOCKET_TRANSPORT_SEGMENT_SIZE,
                    NS_SOCKET_TRANSPORT_BUFFER_SIZE);
    if (NS_SUCCEEDED(rv)) {
      rv = in->SetNonBlocking(PR_TRUE);
    }
#endif

    if (NS_SUCCEEDED(rv)) {
#ifndef NSPIPE2
      mWriteStream = in;
#else
      mWritePipeIn = in;
#endif
      *result = out;
      NS_IF_ADDREF(*result);

#ifndef NSPIPE2
      out->GetBuffer(getter_AddRefs(mWriteBuffer));
#else
      mWritePipeOut = out;
#endif
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
         ("--- Leaving nsSocketTransport::OpenOutputStream() [this=%x].\t"
          "rv = %x.\n",
          this, rv));

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
nsSocketTransport::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
  NS_ASSERTION(0, "transports shouldn't end up in groups");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSocketTransport::GetPrincipal(nsIPrincipal * *aPrincipal)
{
  *aPrincipal = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsSocketTransport::SetPrincipal(nsIPrincipal * aPrincipal)
{
  return NS_OK;
}

