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

#include "nsIBuffer.h"

//
// --------------------------------------------------------------------------
// Private implementation of a nsIBufferInputStream used by the
// nsSocketTransport implementation
// --------------------------------------------------------------------------
//

#if defined(XP_PC)
#include <windows.h> // Interlocked increment...
#endif

#include "nspr.h"
#include "nscore.h"
#include "nsSocketTransportStreams.h"
#include "nsSocketTransport.h"



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
// gSocketLog is defined in nsSocketTransport.cpp
//
extern PRLogModuleInfo* gSocketLog;

#endif /* PR_LOGGING */



nsSocketTransportStream::nsSocketTransportStream()
{
  NS_INIT_REFCNT();

  mIsTransportSuspended = PR_FALSE;
  mIsStreamBlocking     = PR_FALSE;

  mMonitor   = nsnull;
  mTransport = nsnull;
  mStream    = nsnull;
}


nsSocketTransportStream::~nsSocketTransportStream()
{
  NS_IF_RELEASE(mTransport);
  NS_IF_RELEASE(mStream);

  if (mMonitor) {
    PR_DestroyMonitor(mMonitor);
    mMonitor = nsnull;
  }
}


nsresult nsSocketTransportStream::Init(nsSocketTransport* aTransport, 
                                       PRBool aBlockingFlag)
{
  nsresult rv = NS_OK;

  NS_ASSERTION(aTransport, "Null transport supplied.");

  mIsStreamBlocking = aBlockingFlag;

  mMonitor = PR_NewMonitor();
  if (!mMonitor) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

  if (NS_SUCCEEDED(rv) && aTransport) {
    mTransport = aTransport;
    NS_ADDREF(mTransport);
  } else {
    rv = NS_ERROR_NULL_POINTER;
  }

  if (NS_SUCCEEDED(rv)) {
    nsIBuffer* buf;
    rv = NS_NewBuffer(&buf, MAX_IO_BUFFER_SIZE, MAX_IO_BUFFER_SIZE);
    if (NS_FAILED(rv)) return rv;
    rv = NS_NewBufferInputStream(&mStream, buf);
  }

  return rv;
}


nsresult nsSocketTransportStream::BlockTransport(void)
{
  Lock();

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("nsSocketTransportStream::BlockTransport() [this=%x].\n", this));

  //
  // XXX: It would be nice to call mTransport->Suspend() here...  
  //      Unfortunately, BlockTransport() is called within the transport lock.
  //      So, since NSPR locks are not re-enterent we must rely on the caller
  //      (ie. the transport) to suspend itself...
  //
  mIsTransportSuspended = PR_TRUE;

  Unlock();

  return NS_OK;
}

//
// --------------------------------------------------------------------------
// nsISupports implementation...
// --------------------------------------------------------------------------
//

NS_IMPL_THREADSAFE_ISUPPORTS(nsSocketTransportStream, 
                             nsIBufferInputStream::GetIID());


//
// --------------------------------------------------------------------------
// nsIBaseStream implementation...
// --------------------------------------------------------------------------
//

NS_IMETHODIMP
nsSocketTransportStream::Close()
{
  return mStream->Close();
}


//
// --------------------------------------------------------------------------
// nsIInputStream implementation...
// --------------------------------------------------------------------------
//
NS_IMETHODIMP
nsSocketTransportStream::GetLength(PRUint32 *aResult)
{
  return mStream->GetLength(aResult);
}


NS_IMETHODIMP
nsSocketTransportStream::Read(char * aBuf, PRUint32 aCount, 
                              PRUint32 *aReadCount)
{
  nsresult rv;

  // Enter the stream lock...
  Lock();

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransportStream::Read() [this=%x].\t"
          "aCount=%d\n",
          this, aCount));

  do {
    rv = mStream->Read(aBuf, aCount, aReadCount);

    if (mIsStreamBlocking && (NS_BASE_STREAM_WOULD_BLOCK == rv)) {
      Wait();
    } else {
      break;
    }
  } while (1);

  //
  // If the transport is blocked waiting for room in the input stream, then 
  // resume it...
  //
  if (mIsTransportSuspended && (*aReadCount) ) {
    PR_LOG(gSocketLog, PR_LOG_DEBUG, 
           ("nsSocketTransportStream::Read() [this=%x].  "
           "Resuming transport [%x].\n",
            this, mTransport));

    mTransport->Resume();
    mIsTransportSuspended = PR_FALSE;
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransportStream::Read() [this=%x].\t"
          "rv = %x.  aReadCount=%d\n",
          this, rv, *aReadCount));

  // Leave the stream lock...
  Unlock();

  return rv;
}


//
// --------------------------------------------------------------------------
// nsIBufferInputStream implementation...
// --------------------------------------------------------------------------
//

NS_IMETHODIMP
nsSocketTransportStream::GetBuffer(nsIBuffer* *result)
{
  return mStream->GetBuffer(result);
}

NS_IMETHODIMP
nsSocketTransportStream::FillFrom(nsIInputStream* aStream, 
                                  PRUint32 aCount, 
                                  PRUint32 *aWriteCount)
{
  nsresult rv;

  // Enter the stream lock...
  Lock();

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransportStream::Fill() [this=%x].\t"
          "aCount=%d\n",
          this, aCount));

  rv = mStream->FillFrom(aStream, aCount, aWriteCount);

  if (mIsStreamBlocking && (NS_BASE_STREAM_WOULD_BLOCK != rv)) {
    Notify();
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransportStream::Fill() [this=%x].\t"
          "rv = %x.  aWriteCount=%d\n",
          this, rv, *aWriteCount));

  // Leave the stream lock...
  Unlock();

  return rv;
}

NS_IMETHODIMP
nsSocketTransportStream::Fill(const char* aBuf, 
                              PRUint32 aCount, 
                              PRUint32 *aWriteCount)
{
  nsresult rv;

  // Enter the stream lock...
  Lock();

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("+++ Entering nsSocketTransportStream::Fill() [this=%x].\t"
          "aCount=%d\n",
          this, aCount));


  rv = mStream->Fill(aBuf, aCount, aWriteCount);

  if (mIsStreamBlocking && (NS_BASE_STREAM_WOULD_BLOCK != rv)) {
    Notify();
  }

  PR_LOG(gSocketLog, PR_LOG_DEBUG, 
         ("--- Leaving nsSocketTransportStream::Fill() [this=%x].\t"
          "rv = %x.  aWriteCount=%d\n",
          this, rv, *aWriteCount));

  // Leave the stream lock...
  Unlock();

  return rv;
}


