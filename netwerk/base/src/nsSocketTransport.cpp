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
#include "nscore.h"
#include "netCore.h"
#include "nsIStreamListener.h"
#include "nsSocketTransport.h"
#include "nsSocketTransportService.h"
#include "nsSocketTransportStreams.h"

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
    eSocketState_Error,         // DoneRead       -> Error
    eSocketState_Error,         // DoneWrite      -> Error
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
    eSocketState_Error,         // DoneRead       -> Error
    eSocketState_Error,         // DoneWrite      -> Error
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
    eSocketState_Connected,     // DoneRead       -> Connected
    eSocketState_Connected,     // DoneWrite      -> Connected
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
static PRIntervalTime gConnectTimeout = -1;

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

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);


nsSocketTransport::nsSocketTransport()
{
  NS_INIT_REFCNT();

  PR_INIT_CLIST(&mListLink);

  mHostName     = nsnull;
  mPort         = 0;
  mSocketFD     = nsnull;
  mLock         = nsnull;

  mSuspendCount = 0;

  mCurrentState = eSocketState_Created;
  mOperation    = eSocketOperation_None;
  mSelectFlags  = 0;

  mReadStream   = nsnull;
  mReadContext  = nsnull;
  mReadListener = nsnull;

  mWriteStream    = nsnull;
  mWriteContext   = nsnull;
  mWriteObserver  = nsnull;

  mSourceOffset   = 0;
  mService        = nsnull;

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

  NS_IF_RELEASE(mReadListener);
  NS_IF_RELEASE(mReadStream);
  NS_IF_RELEASE(mReadContext);

  NS_IF_RELEASE(mWriteObserver);
  NS_IF_RELEASE(mWriteStream);
  NS_IF_RELEASE(mWriteContext);
  
  NS_IF_RELEASE(mService);

  if (mHostName) {
    nsCRT::free(mHostName);
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
                                 PRInt32 aPort)
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
  Lock();

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::Process() [this=%x].\t"
          "aSelectFlags = %x.\t"
          "CurrentState = %d\n",
          this, aSelectFlags, mCurrentState));

  //
  // If the transport has been suspended, then return NS_OK immediately...
  // This removes the transport from the select list...
  //
  if (mSuspendCount) {
    PR_LOG(gSocketLog, PR_LOG_DEBUG, 
           ("Transport [this=%x] is suspended.\n", this));

    done = PR_TRUE;
    rv = NS_OK;
  }

  while (!done)
  {
    switch (mCurrentState) {
      case eSocketState_Created:
      case eSocketState_Closed:
        break;

      case eSocketState_Connected:
        // Fire a notification for read...
        if (mReadListener) {
          mReadListener->OnStartBinding(mReadContext);
        }
        // Fire a notification for write...
        if (mWriteObserver) {
          mWriteObserver->OnStartBinding(mWriteContext);
        }
        break;

      case eSocketState_Done:
      case eSocketState_Error:
      case eSocketState_DoneRead:
      case eSocketState_DoneWrite:
        PR_LOG(gSocketLog, PR_LOG_DEBUG, 
               ("Transport [this=%x] is in done state %d.\n", this, mCurrentState));

        // Fire a notification that the read has finished...
        if (eSocketState_DoneWrite != mCurrentState) {
          if (mReadListener) {
            mReadListener->OnStopBinding(mReadContext, rv, nsnull);
            NS_RELEASE(mReadListener);
            NS_IF_RELEASE(mReadContext);
          }
          NS_IF_RELEASE(mReadStream);
        }

        // Fire a notification that the write has finished...
        if (eSocketState_DoneRead != mCurrentState) {
          if (mWriteObserver) {
            mWriteObserver->OnStopBinding(mWriteContext, rv, nsnull);
            NS_RELEASE(mWriteObserver);
            NS_IF_RELEASE(mWriteContext);
          }
          NS_IF_RELEASE(mWriteStream);
        }

        //
        // Set up the connection for the next operation...
        //
        if (mReadStream || mWriteStream) {
          mCurrentState = eSocketState_WaitReadWrite;
        } else {
          mCurrentState = gStateTable[mOperation][mCurrentState];
          mOperation    = eSocketOperation_None;
          done = PR_TRUE;
        }
        continue;

      case eSocketState_WaitDNS:
        rv = doResolveHost();
        break;

      case eSocketState_WaitConnect:
        rv = doConnection(aSelectFlags);
        break;

      case eSocketState_WaitReadWrite:
        if (mReadStream) {
          rv = doRead(aSelectFlags);
          if (NS_OK == rv) {
            mCurrentState = eSocketState_DoneRead;
            continue;
          }
        }

        if (NS_SUCCEEDED(rv) && mWriteStream) {
          rv = doWrite(aSelectFlags);
          if (NS_OK == rv) {
            mCurrentState = eSocketState_DoneWrite;
            continue;
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
    else if (NS_FAILED(rv)) {
      mCurrentState = eSocketState_Error;
    }
    else if (NS_BASE_STREAM_WOULD_BLOCK == rv) {
      done = PR_TRUE;
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

  // Leave the socket transport lock...
  Unlock();

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

  status = PR_GetHostByName(mHostName, dbbuf, sizeof(dbbuf), &hostEnt);
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

    rv = NS_ERROR_FAILURE;
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

  //
  // Step 1:
  //    Create a new TCP socket structure (if necessary)...
  //
  if (!mSocketFD) {
    mSocketFD = PR_NewTCPSocket();
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
  }

  //
  // Step 2:
  //    Initiate the connect() to the host...  
  //
  //    This is only done the first time doConnection(...) is called.
  //    If aSelectFlags == 0 then this is the first time... Otherwise,
  //    PR_Poll(...) would have set the flags non-zero.
  //
  if (NS_SUCCEEDED(rv) && (0 == aSelectFlags)) {
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
  //
  // Step 3:
  //    Process the flags returned by PR_Poll() if any...
  //
  else if (NS_SUCCEEDED(rv) && aSelectFlags) {
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
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::doConnection() [this=%x].\t"
          "rv = %x.\n\n",
          this, rv));

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
  PRUint32 size, bytesWritten, totalBytesWritten;
  PRInt32 len;
  PRErrorCode code;
  nsresult rv = NS_OK;

  NS_ASSERTION(eSocketState_WaitReadWrite == mCurrentState, "Wrong state.");

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::doRead() [this=%x].\t"
          "aSelectFlags = %x.\n",
          this, aSelectFlags));

  //
  // Check for an error during PR_Poll(...)
  //
  if (PR_POLL_EXCEPT & aSelectFlags) {
    PR_LOG(gSocketLog, PR_LOG_ERROR, 
           ("PR_Read() failed via PR_POLL_EXCEPT. [this=%x].\n", this));

    // XXX: What should this error code be?
    rv = NS_ERROR_FAILURE;
  }

  totalBytesWritten = 0;
  while (NS_OK == rv) {
    //
    // Determine how much space is available in the input stream...
    //
    // Since, the data is being read into a global buffer from the net, unless
    // it can all be pushed into the stream it will be lost!
    //
    mReadStream->GetLength(&size);
    size = MAX_IO_BUFFER_SIZE - size;
    if (size > MAX_IO_BUFFER_SIZE) {
      size = MAX_IO_BUFFER_SIZE;
    }

    if (size > 0) {
      len = PR_Read(mSocketFD, gIOBuffer, size);
      if (len > 0) {
        rv = mReadStream->Fill(gIOBuffer, len, &bytesWritten);
        NS_ASSERTION(bytesWritten == (PRUint32)len, "Data was lost during read.");

        totalBytesWritten += bytesWritten;
      } 
      //
      // The read operation has completed...
      //
      else if (len == 0) {
        rv = NS_OK;
        break;
      } 
      // Error...
      else {
        code = PR_GetError();

        if (PR_WOULD_BLOCK_ERROR == code) {
          rv = NS_BASE_STREAM_WOULD_BLOCK;
        } 
        else {
          PR_LOG(gSocketLog, PR_LOG_ERROR, 
                 ("PR_Read() failed. [this=%x].  PRErrorCode = %x\n",
                  this, code));

          // XXX: What should this error code be?
          rv = NS_ERROR_FAILURE;
        }
      }
    } 
    //
    // There is no room in the input stream for more data...  
    // 
    // Remove this entry from the select list until the consumer has read 
    // some data...  Since we are already holding the transport lock, just
    // increment the suspend count...
    //
    // When the consumer makes some room in the stream, the transport will be
    // resumed...
    //
    else {
      mSuspendCount += 1;
      mReadStream->BlockTransport();
      rv = NS_BASE_STREAM_WOULD_BLOCK;
    }
  }

  //
  // Fire a single OnDataAvaliable(...) notification once as much data has
  // been filled into the stream as possible...
  //
  if (totalBytesWritten && mReadListener) {
    mReadListener->OnDataAvailable(mReadContext, mReadStream, mSourceOffset, totalBytesWritten);
    mSourceOffset += totalBytesWritten;
  }

  //
  // Set up the select flags for connect...
  //
  if (NS_BASE_STREAM_WOULD_BLOCK == rv) {
    mSelectFlags |= (PR_POLL_READ | PR_POLL_EXCEPT);
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
  PRUint32 bytesRead, totalBytesRead;
  PRInt32 len;
  PRErrorCode code;
  nsresult rv = NS_OK;

  NS_ASSERTION(eSocketState_WaitReadWrite == mCurrentState, "Wrong state.");

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::doWrite() [this=%x].\t"
          "aSelectFlags = %x.\n",
          this, aSelectFlags));

  //
  // Check for an error during PR_Poll(...)
  //
  if (PR_POLL_EXCEPT & aSelectFlags) {
    PR_LOG(gSocketLog, PR_LOG_ERROR, 
           ("PR_Write() failed via PR_POLL_EXCEPT. [this=%x].\n", this));

    // XXX: What should this error code be?
    rv = NS_ERROR_FAILURE;
  }

  totalBytesRead = 0;
  while (NS_OK == rv) {
    rv = mWriteStream->Read(gIOBuffer, sizeof(gIOBuffer), &bytesRead);
    if (NS_SUCCEEDED(rv)) {
      if (bytesRead > 0) {
        totalBytesRead += bytesRead;
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
      else if (bytesRead == 0) {
        mSelectFlags &= (~PR_POLL_WRITE);
        rv = NS_OK;
        break;
      }
    }
  }

  //
  // Set up the select flags for connect...
  //
  if (NS_BASE_STREAM_WOULD_BLOCK == rv) {
    mSelectFlags |= (PR_POLL_WRITE | PR_POLL_EXCEPT);
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::doWrite() [this=%x]. rv = %x.\t"
          "Total bytes written: %d\n\n",
          this, rv, totalBytesRead));

  return rv;
}


nsresult nsSocketTransport::CloseConnection(void)
{
  PRStatus status;
  nsresult rv = NS_OK;

  NS_ASSERTION(mSocketFD, "Socket does not exist");

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

NS_IMPL_THREADSAFE_ISUPPORTS(nsSocketTransport, nsITransport::GetIID());


//
// --------------------------------------------------------------------------
// nsICancelable implementation...
// --------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSocketTransport::Cancel(void)
{
  nsresult rv = NS_OK;

  rv = NS_ERROR_NOT_IMPLEMENTED;

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
  Lock();

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

  // Leave the socket transport lock...
  Unlock();

  return rv;
}


NS_IMETHODIMP
nsSocketTransport::Resume(void)
{
  nsresult rv = NS_OK;

  // Enter the socket transport lock...
  Lock();

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

  // Leave the socket transport lock...
  Unlock();

  return rv;
}



//
// --------------------------------------------------------------------------
// nsITransport implementation...
// --------------------------------------------------------------------------
//

NS_IMETHODIMP
nsSocketTransport::AsyncRead(nsISupports* aContext, 
                             nsIEventQueue* aAppEventQueue,
                             nsIStreamListener* aListener)
{
  nsresult rv = NS_OK;

  // Enter the socket transport lock...
  Lock();

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::AsyncRead() [this=%x].\n", this));

  // If a read is already in progress then fail...
  if (mReadListener) {
    rv = NS_ERROR_IN_PROGRESS;
  }

  // Create a new non-blocking input stream for reading data into...
  if (NS_SUCCEEDED(rv) && !mReadStream) {
    mReadStream = new nsSocketTransportStream();
    if (mReadStream) {
      NS_ADDREF(mReadStream);
      rv = mReadStream->Init(this, PR_FALSE);
    } else {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (NS_SUCCEEDED(rv)) {
    // Store the context used for this read...
    NS_IF_RELEASE(mReadContext);
    mReadContext = aContext;
    NS_IF_ADDREF(mReadContext);

    // Create a marshalling stream listener to receive notifications...
    rv = NS_NewAsyncStreamListener(&mReadListener, aAppEventQueue, aListener);
  }

  if (NS_SUCCEEDED(rv)) {
    mOperation = eSocketOperation_ReadWrite;

    rv = mService->AddToWorkQ(this);
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::AsyncRead() [this=%x]. rv = %x.\n",
          this, rv));

  // Leave the socket transport lock...
  Unlock();

  return rv;
}


NS_IMETHODIMP
nsSocketTransport::AsyncWrite(nsIInputStream* aFromStream, 
                              nsISupports* aContext,
                              nsIEventQueue* aAppEventQueue,
                              nsIStreamObserver* aObserver)
{
  nsresult rv = NS_OK;

  // Enter the socket transport lock...
  Lock();

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::AsyncWrite() [this=%x].\n", this));

  // If a write is already in progress then fail...
  if (mWriteStream) {
    rv = NS_ERROR_IN_PROGRESS;
  }

  if (NS_SUCCEEDED(rv)) {
    mWriteStream = aFromStream;
    NS_ADDREF(mWriteStream);

    NS_IF_RELEASE(mWriteContext);
    mWriteContext = aContext;
    NS_IF_ADDREF(mWriteContext);

    // Create a marshalling stream observer to receive notifications...
    NS_IF_RELEASE(mWriteObserver);
    if (aObserver) {
      rv = NS_NewAsyncStreamObserver(&mWriteObserver, aAppEventQueue, aObserver);
    }
  }

  if (NS_SUCCEEDED(rv)) {
    mOperation = eSocketOperation_ReadWrite;
    rv = mService->AddToWorkQ(this);
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::AsyncWrite() [this=%x]. rv = %x.\n",
          this, rv));

  // Leave the socket transport lock...
  Unlock();

  return rv;
}


NS_IMETHODIMP
nsSocketTransport::OpenInputStream(nsIInputStream* *result)
{
  nsresult rv = NS_OK;

  // Enter the socket transport lock...
  Lock();

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::OpenInputStream() [this=%x].\n", 
         this));

  // If a read is already in progress then fail...
  if (mReadListener) {
    rv = NS_ERROR_IN_PROGRESS;
  }

  // Create a new blocking input stream for reading data into...
  if (NS_SUCCEEDED(rv)) {
    NS_IF_RELEASE(mReadStream);
    NS_IF_RELEASE(mReadContext);

    mReadStream = new nsSocketTransportStream();
    if (mReadStream) {
      NS_ADDREF(mReadStream);
      rv = mReadStream->Init(this, PR_TRUE);
    } else {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (NS_SUCCEEDED(rv)) {
    mOperation = eSocketOperation_ReadWrite;

    rv = mService->AddToWorkQ(this);
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::OpenInputStream() [this=%x].\t"
          "rv = %x.\n",
          this, rv));

  // Leave the socket transport lock...
  Unlock();

  return rv;
}


NS_IMETHODIMP
nsSocketTransport::OpenOutputStream(nsIOutputStream* *result)
{
  nsresult rv = NS_OK;

  // Enter the socket transport lock...
  Lock();

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransport::OpenOutputStream() [this=%x].\n", 
         this));

  // If a write is already in progress then fail...
  if (mWriteStream) {
    rv = NS_ERROR_IN_PROGRESS;
  }

  if (NS_SUCCEEDED(rv)) {
    NS_IF_RELEASE(mWriteObserver);
    NS_IF_RELEASE(mWriteContext);

  	// We want a pipe here so the caller can "write" into one end
  	// and the other end (aWriteStream) gets the data. This data
  	// is then written to the underlying socket when nsSocketTransport::doWrite()
  	// is called.

    // XXX not sure if this should be blocking (PR_TRUE) or non-blocking.
    rv = NS_NewPipe(&mWriteStream,
           result,
           PR_FALSE, MAX_IO_BUFFER_SIZE);
  }

  if (NS_SUCCEEDED(rv)) {
    mOperation = eSocketOperation_ReadWrite;
    // Start the crank.
    rv = mService->AddToWorkQ(this);
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransport::OpenOutputStream() [this=%x].\t"
          "rv = %x.\n",
          this, rv));

  // Leave the socket transport lock...
  Unlock();

  return rv;
}


