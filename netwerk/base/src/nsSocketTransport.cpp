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

#include "nspr.h"
#include "nsCRT.h"
#include "nscore.h"
#include "nsIStreamListener.h"
#include "nsSocketTransport.h"
#include "nsSocketTransportService.h"

//
// This is the State table which maps current state to next state
// for each socket operation...
//
nsSocketState gStateTable[eSocketOperation_Max][eSocketState_Max] = {
  // eSocketOperation_None:
  {
    eSocketState_Error,         // Created     -> Error
    eSocketState_Error,         // WaitDNS     -> Error
    eSocketState_Error,         // Closed      -> Error
    eSocketState_Error,         // WaitConnect -> Error
    eSocketState_Error,         // Connected   -> Error
    eSocketState_Error,         // WaitRead    -> Error
    eSocketState_Error,         // WaitWrite   -> Error
    eSocketState_Error,         // Done        -> Error
    eSocketState_Error,         // Timeout     -> Error
    eSocketState_Error          // Error       -> Error
  },
  // eSocketOperation_Connect:
  {
    eSocketState_WaitDNS,       // Created     -> WaitDNS
    eSocketState_Closed,        // WaitDNS     -> Closed
    eSocketState_WaitConnect,   // Closed      -> WaitConnect
    eSocketState_Connected,     // WaitConnect -> Connected
    eSocketState_Connected,     // Connected   -> Done
    eSocketState_Error,         // WaitRead    -> Error
    eSocketState_Error,         // WaitWrite   -> Error
    eSocketState_Connected,     // Done        -> Connected
    eSocketState_Error,         // Timeout     -> Error
    eSocketState_Closed         // Error       -> Closed
  },
  // eSocketOperation_Read:
  {
    eSocketState_WaitDNS,       // Created     -> WaitDNS
    eSocketState_Closed,        // WaitDNS     -> Closed
    eSocketState_WaitConnect,   // Closed      -> WaitConnect
    eSocketState_Connected,     // WaitConenct -> Connected
    eSocketState_WaitRead,      // Connected   -> WaitRead
    eSocketState_Done,          // WaitRead    -> Done
    eSocketState_Error,         // WaitWrite   -> Error
    eSocketState_Connected,     // Done        -> Connected
    eSocketState_Error,         // Timeout     -> Error
    eSocketState_Connected      // Error       -> Connected
  },
    // eSocketOperation_Write:
  {
    eSocketState_WaitDNS,       // Created     -> WaitDNS
    eSocketState_Closed,        // WaitDNS     -> Closed
    eSocketState_WaitConnect,   // Closed      -> WaitConnect
    eSocketState_Connected,     // WaitConenct -> Connected
    eSocketState_WaitWrite,     // Connected   -> WaitWrite
    eSocketState_Error,         // WaitRead    -> Error
    eSocketState_Done,          // WaitWrite   -> Done
    eSocketState_Connected,     // Done        -> Connected
    eSocketState_Error,         // Timeout     -> Error
    eSocketState_Connected          // Error       -> Connected
  }
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
#define MAX_IO_BUFFER_SIZE   8192

static char gIOBuffer[MAX_IO_BUFFER_SIZE];



static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);


nsSocketTransport::nsSocketTransport()
{
  NS_INIT_REFCNT();

  PR_INIT_CLIST(this);

  mHostName     = nsnull;
  mPort         = 0;
  mSocketFD     = nsnull;

  mCurrentState = eSocketState_Created;
  mOperation    = eSocketOperation_None;
  mSelectFlags  = 0;

  mReadStream   = nsnull;
  mWriteStream  = nsnull;
  mListener     = nsnull;
  mContext      = nsnull;
  mService      = nsnull;

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
}


nsSocketTransport::~nsSocketTransport()
{
  NS_IF_RELEASE(mContext);
  NS_IF_RELEASE(mListener);
  NS_IF_RELEASE(mReadStream);
  NS_IF_RELEASE(mWriteStream);
  NS_IF_RELEASE(mService);

  if (mHostName) {
    PR_Free(mHostName);
  }

  if (mSocketFD) {
    PR_Close(mSocketFD);
    mSocketFD = nsnull;
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
    // Copy the host name...
    //
    // XXX:  This is so evil!  Since this is a char* it must be freed with
    //       PR_Free(...) NOT delete[] like PRUnichar* buffers...
    //
    mHostName = nsCRT::strdup(aHost);
    if (!mHostName) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  } 
  // aHost was nsnull...
  else {
    rv = NS_ERROR_NULL_POINTER;
  }

  return rv;
}


nsresult nsSocketTransport::Process(PRInt16 aSelectFlags)
{
  nsresult rv = NS_OK;
  PRBool done = PR_FALSE;

  while (!done)
  {
    switch (mCurrentState) {
      case eSocketState_Created:
      case eSocketState_Closed:
        break;

      case eSocketState_Connected:
        if (mListener) {
          mListener->OnStartBinding(mContext);
        }
        break;

      case eSocketState_Done:
      case eSocketState_Error:
        if (mListener) {
          mListener->OnStopBinding(mContext, rv, nsnull);
        }
        NS_IF_RELEASE(mContext);
        //
        // Set up the connection for the next operation...
        //
        mCurrentState = gStateTable[mOperation][mCurrentState];
        mOperation    = eSocketOperation_None;
        done = PR_TRUE;
        continue;

      case eSocketState_WaitDNS:
        rv = doResolveHost();
        break;

      case eSocketState_WaitConnect:
        rv = doConnection(aSelectFlags);
        break;

      case eSocketState_WaitRead:
        rv = doRead(aSelectFlags);
        break;

      case eSocketState_WaitWrite:
        rv = doWrite(aSelectFlags);
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
  
  return rv;
}


//-----
//
// doResolveHost:
//
// Return Codes:
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
  } 
  // DNS lookup failed...
  else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

//-----
//
// doConnection:
//
// Return values:
//    NS_OK
//    NS_BASE_STREAM_WOULD_BLOCK
//
//    NS_ERROR_CONNECTION_REFUSED (XXX: Not yet defined)
//    NS_ERROR_FAILURE
//    NS_ERROR_OUT_OF_MEMORY
//
//-----
nsresult nsSocketTransport::doConnection(PRInt16 aSelectFlags)
{
  PRStatus status;
  nsresult rv = NS_OK;

  NS_ASSERTION(eSocketState_WaitConnect == mCurrentState, "Wrong state.");
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
        // XXX: Should be NS_ERROR_CONNECTION_REFUSED
        rv = NS_ERROR_FAILURE;
      }
    }
  }
  //
  // Step 3:
  //    Process the flags returned by PR_Poll() if any...
  //
  else if (NS_SUCCEEDED(rv) && aSelectFlags) {
    if (PR_POLL_EXCEPT & aSelectFlags) {
      // XXX: Should be NS_ERROR_CONNECTION_REFUSED
      rv = NS_ERROR_FAILURE;
    }
    //
    // The connection was successful...
    //
    else if (PR_POLL_WRITE & aSelectFlags) {
      rv = NS_OK;
    }
  }

  return rv;
}


//-----
//
// doRead:
//
// Return values:
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

  NS_ASSERTION(eSocketState_WaitRead == mCurrentState, "Wrong state.");

  //
  // Check for an error during PR_Poll(...)
  //
  if (PR_POLL_EXCEPT & aSelectFlags) {
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
          // XXX: What should this error code be?
          rv = NS_ERROR_FAILURE;
        }
      }
    } 
    //
    // There is no room in the input stream for more data...  Give the 
    // consumer more time to empty the stream...
    //
    // XXX: Until there is a mechanism to remove this entry from the 
    //      select list until the consumer has read some data, this will
    //      cause the transport thread to spin !!
    //
    else {
      rv = NS_BASE_STREAM_WOULD_BLOCK;
    }
  }

  //
  // Fire a single OnDataAvaliable(...) notification once as much data has
  // been filled into the stream as possible...
  //
  if (totalBytesWritten) {
    mListener->OnDataAvailable(mContext, mReadStream, totalBytesWritten);
  }

  //
  // Set up the select flags for connect...
  //
  if (NS_BASE_STREAM_WOULD_BLOCK == rv) {
    mSelectFlags = (PR_POLL_READ | PR_POLL_EXCEPT);
  }

  return rv;
}


//-----
//
// doWrite:
//
// Return values:
//    NS_OK
//    NS_BASE_STREAM_WOULD_BLOCK
//
//    NS_ERROR_FAILURE
//
//-----
nsresult nsSocketTransport::doWrite(PRInt16 aSelectFlags)
{
  PRUint32 size, bytesRead;
  PRInt32 len;
  PRErrorCode code;
  nsresult rv = NS_OK;

  NS_ASSERTION(eSocketState_WaitWrite == mCurrentState, "Wrong state.");

  //
  // Check for an error during PR_Poll(...)
  //
  if (PR_POLL_EXCEPT & aSelectFlags) {
    // XXX: What should this error code be?
    rv = NS_ERROR_FAILURE;
  }

  while (NS_OK == rv) {
    rv = mWriteStream->Read(gIOBuffer, sizeof(gIOBuffer), &bytesRead);
    if (NS_SUCCEEDED(rv)) {
      if (bytesRead > 0) {
        len = PR_Write(mSocketFD, gIOBuffer, bytesRead);
        if (len < 0) {
          code = PR_GetError();

          if (PR_WOULD_BLOCK_ERROR == code) {
            rv = NS_BASE_STREAM_WOULD_BLOCK;
          } 
          else {
            rv = NS_ERROR_FAILURE;
          }
        }
      } 
      //
      // The write operation has completed...
      //
      else if (bytesRead == 0) {
        rv = NS_OK;
        break;
      }
    }
  }

  //
  // Set up the select flags for connect...
  //
  if (NS_BASE_STREAM_WOULD_BLOCK == rv) {
    mSelectFlags = (PR_POLL_WRITE | PR_POLL_EXCEPT);
  }

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
NS_IMPL_ISUPPORTS(nsSocketTransport, nsITransport::GetIID());


//
// --------------------------------------------------------------------------
// nsICancelable implementation...
// --------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSocketTransport::Cancel(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketTransport::Suspend(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSocketTransport::Resume(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}



//
// --------------------------------------------------------------------------
// nsITransport implementation...
// --------------------------------------------------------------------------
//

NS_IMETHODIMP
nsSocketTransport::AsyncRead(nsISupports* aContext, 
                             PLEventQueue* aAppEventQueue,
                             nsIStreamListener* aListener)
{
  nsresult rv = NS_OK;

  if (eSocketOperation_None != mOperation) {
    // XXX: This should be NS_ERROR_IN_PROGRESS...
    rv = NS_ERROR_FAILURE;
  }

  if (NS_SUCCEEDED(rv) && !mReadStream) {
    rv = NS_NewByteBufferInputStream(&mReadStream, PR_FALSE, 
                                     MAX_IO_BUFFER_SIZE);
  }

  if (NS_SUCCEEDED(rv)) {
    NS_IF_RELEASE(mContext);
    mContext = aContext;
    NS_IF_ADDREF(mContext);

    NS_IF_RELEASE(mListener);
    rv = NS_NewAsyncStreamListener(&mListener, aAppEventQueue, aListener);
  }

  if (NS_SUCCEEDED(rv)) {
    mOperation = eSocketOperation_Read;

    mService->AddToWorkQ(this);
  }

  return rv;
}


NS_IMETHODIMP
nsSocketTransport::AsyncWrite(nsIInputStream* aFromStream, 
                              nsISupports* aContext,
                              PLEventQueue* aAppEventQueue,
                              nsIStreamObserver* aObserver)
{
  nsresult rv = NS_OK;

  if (eSocketOperation_None != mOperation) {
    // XXX: This should be NS_ERROR_IN_PROGRESS...
    rv = NS_ERROR_FAILURE;
  }

  if (NS_SUCCEEDED(rv)) {
    NS_IF_RELEASE(mWriteStream);
    mWriteStream = aFromStream;
    NS_ADDREF(mWriteStream);

    NS_IF_RELEASE(mContext);
    mContext = aContext;
    NS_IF_ADDREF(mContext);

    NS_IF_RELEASE(mListener);
    nsIStreamObserver* aListener;
    rv = NS_NewAsyncStreamObserver(&aListener, aAppEventQueue, aObserver);
    // XXX:  This is really evil...
    mListener = (nsIStreamListener*)aListener;
  }

  if (NS_SUCCEEDED(rv)) {
    mOperation = eSocketOperation_Write;
    mService->AddToWorkQ(this);
  }

  return rv;
}


NS_IMETHODIMP
nsSocketTransport::OpenInputStream(nsIInputStream* *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsSocketTransport::OpenOutputStream(nsIOutputStream* *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


