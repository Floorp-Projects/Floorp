/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */

#include "nsFileTransport.h"
#include "nsFileTransportService.h"
#include "nscore.h"
#include "nsIEventSinkGetter.h"
#include "nsIURI.h"
#include "nsIEventQueue.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIBufferInputStream.h"
#include "nsIBufferOutputStream.h"
#include "nsAutoLock.h"
#include "netCore.h"
#include "nsFileStream.h"
#include "nsIFileStream.h"
#include "nsISimpleEnumerator.h"
#include "nsIURL.h"
#include "prio.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsSpecialSystemDirectory.h"
#include "nsDirectoryIndexStream.h"
#include "nsEscape.h"
#include "nsIMIMEService.h"
#include "prlog.h"

static NS_DEFINE_CID(kMIMEServiceCID, NS_MIMESERVICE_CID);
static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

#if defined(PR_LOGGING)
//
// Log module for SocketTransport logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsFileTransport:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_DEBUG level information and places all output in
// the file nspr.log
//
PRLogModuleInfo* gFileTransportLog = nsnull;

#endif /* PR_LOGGING */

////////////////////////////////////////////////////////////////////////////////

nsFileTransport::nsFileTransport()
    : mState(QUIESCENT),
      mSuspended(PR_FALSE), 
      mStatus(NS_OK), 
      mOffset(0),
      mMonitor(nsnull),
      mBuffer(nsnull)
{
    NS_INIT_REFCNT();

#if defined(PR_LOGGING)
    //
    // Initialize the global PRLogModule for socket transport logging
    // if necessary...
    //
    if (nsnull == gFileTransportLog) {
        gFileTransportLog = PR_NewLogModule("nsFileTransport");
    }
#endif /* PR_LOGGING */
}

nsresult
nsFileTransport::Init(nsFileSpec& spec, const char* command, nsIEventSinkGetter* getter)
{
    if (mMonitor == nsnull) {
        mMonitor = nsAutoMonitor::NewMonitor("nsFileTransport");
        if (mMonitor == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    mSpec = spec;
    if (getter) {
        nsCOMPtr<nsISupports> sink;
        (void)getter->GetEventSink(command, nsIProgressEventSink::GetIID(), getter_AddRefs(sink));
        mProgress = (nsIProgressEventSink*)sink.get();
    }
    return NS_OK;
}

nsresult
nsFileTransport::Init(nsIInputStream* fromStream, const char* command, nsIEventSinkGetter* getter)
{
    if (mMonitor == nsnull) {
        mMonitor = nsAutoMonitor::NewMonitor("nsFileTransport");
        if (mMonitor == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    mSource = fromStream;
    if (getter) {
        nsCOMPtr<nsISupports> sink;
        (void)getter->GetEventSink(command, nsIProgressEventSink::GetIID(), getter_AddRefs(sink));
        mProgress = (nsIProgressEventSink*)sink.get();
    }
    return NS_OK;
}

nsFileTransport::~nsFileTransport()
{
    NS_ASSERTION(mSource == nsnull, "transport not closed");
    NS_ASSERTION(mBufferInputStream == nsnull, "transport not closed");
    NS_ASSERTION(mBufferOutputStream == nsnull, "transport not closed");
    NS_ASSERTION(mSink == nsnull, "transport not closed");
    NS_ASSERTION(mBuffer == nsnull, "transport not closed");
    if (mMonitor)
        nsAutoMonitor::DestroyMonitor(mMonitor);
}

NS_IMETHODIMP
nsFileTransport::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "no instance pointer");
    if (aIID.Equals(NS_GET_IID(nsIChannel)) ||
        aIID.Equals(NS_GET_IID(nsIRequest)) ||
        aIID.Equals(NS_GET_IID(nsISupports))) {
        *aInstancePtr = NS_STATIC_CAST(nsIChannel*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(NS_GET_IID(nsIRunnable))) {
        *aInstancePtr = NS_STATIC_CAST(nsIRunnable*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(NS_GET_IID(nsIPipeObserver))) {
        *aInstancePtr = NS_STATIC_CAST(nsIPipeObserver*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsFileTransport);
NS_IMPL_RELEASE(nsFileTransport);

NS_METHOD
nsFileTransport::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    nsFileTransport* fc = new nsFileTransport();
    if (fc == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(fc);
    nsresult rv = fc->QueryInterface(aIID, aResult);
    NS_RELEASE(fc);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIRequest
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileTransport::IsPending(PRBool *result)
{
    *result = mState != QUIESCENT;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::Cancel()
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv = NS_OK;
    mStatus = NS_BINDING_ABORTED;
    if (mSuspended) {
        Resume();
    }
    if (mState == READING)
        mState = END_READ;
    else
        mState = END_WRITE;
    PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
           ("nsFileTransport: Cancel [this=%x %s]",
            this, (const char*)mSpec));
    return rv;
}

NS_IMETHODIMP
nsFileTransport::Suspend()
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv = NS_OK;
    if (!mSuspended) {
        // XXX close the stream here?
        NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;
        mStatus = fts->Suspend(this);
        mSuspended = PR_TRUE;
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: Suspend [this=%x %s]",
                this, (const char*)mSpec));
    }
    return rv;
}

NS_IMETHODIMP
nsFileTransport::Resume()
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv = NS_OK;
    if (mSuspended) {
        // XXX re-open the stream and seek here?
        NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;
        mSuspended = PR_FALSE;  // set this first before resuming!
        mStatus = fts->Resume(this);
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: Resume [this=%x %s] status=%x",
                this, (const char*)mSpec, mStatus));
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// From nsITransport
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileTransport::OpenInputStream(PRUint32 startPosition, PRInt32 readCount,
                                 nsIInputStream **result)
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv;

    if (mState != QUIESCENT)
        return NS_ERROR_IN_PROGRESS;

    if (!mSpec.Exists())
        return NS_ERROR_FAILURE;        // XXX probably need NS_BASE_STREAM_FILE_NOT_FOUND or something

    rv = NS_NewPipe(getter_AddRefs(mBufferInputStream), 
                    getter_AddRefs(mBufferOutputStream),
                    this,     // nsIPipeObserver
                    NS_FILE_TRANSPORT_SEGMENT_SIZE,
                    NS_FILE_TRANSPORT_BUFFER_SIZE);
    if (NS_FAILED(rv)) return rv;

    rv = mBufferOutputStream->SetNonBlocking(PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    mState = START_READ;
    mOffset = startPosition;
    mAmount = readCount;
    mListener = null_nsCOMPtr();

    *result = mBufferInputStream.get();
    NS_ADDREF(*result);
    PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
           ("nsFileTransport: OpenInputStream [this=%x %s]",
            this, (const char*)mSpec));

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = fts->DispatchRequest(this);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **result)
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv;

    if (mState != QUIESCENT)
        return NS_ERROR_IN_PROGRESS;

    NS_ASSERTION(startPosition == 0, "implement startPosition");
    nsCOMPtr<nsISupports> str;
    rv = NS_NewTypicalOutputFileStream(getter_AddRefs(str), mSpec);
    if (NS_FAILED(rv)) return rv;
    rv = str->QueryInterface(NS_GET_IID(nsIOutputStream), (void**)result);

    PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
           ("nsFileTransport: OpenOutputStream [this=%x %s]",
            this, (const char*)mSpec));
    return rv;
}

NS_IMETHODIMP
nsFileTransport::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                           nsISupports *ctxt,
                           nsIStreamListener *listener)
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv = NS_OK;

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIEventQueue> eventQueue;
    rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), getter_AddRefs(eventQueue));
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(listener, "need to supply an nsIStreamListener");
    rv = serv->NewAsyncStreamListener(listener, eventQueue, getter_AddRefs(mListener));
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewPipe(getter_AddRefs(mBufferInputStream),
                    getter_AddRefs(mBufferOutputStream),
                    this,       // nsIPipeObserver
                    NS_FILE_TRANSPORT_SEGMENT_SIZE,
                    NS_FILE_TRANSPORT_BUFFER_SIZE);
    if (NS_FAILED(rv)) return rv;

    rv = mBufferOutputStream->SetNonBlocking(PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(mContext == nsnull, "context not released");
    mContext = ctxt;

    mState = START_READ;
    mOffset = startPosition;
    mAmount = readCount;
    mTotalAmount = 0;

    PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
           ("nsFileTransport: AsyncRead [this=%x %s]",
            this, (const char*)mSpec));

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = fts->DispatchRequest(this);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::AsyncWrite(nsIInputStream *fromStream,
                            PRUint32 startPosition, PRInt32 writeCount,
                            nsISupports *ctxt,
                            nsIStreamObserver *observer)
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv = NS_OK;

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIEventQueue> eventQueue;
    rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), getter_AddRefs(eventQueue));
    if (NS_FAILED(rv)) return rv;

    if (observer) {
        rv = serv->NewAsyncStreamObserver(observer, eventQueue, getter_AddRefs(mObserver));
        if (NS_FAILED(rv)) return rv;
    }

    NS_ASSERTION(mContext == nsnull, "context not released");
    mContext = ctxt;

    mState = START_WRITE;
    mOffset = startPosition;
    mAmount = writeCount;
    mSource = fromStream;
    mTotalAmount = 0;

    PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
           ("nsFileTransport: AsyncWrite [this=%x %s]",
            this, (const char*)mSpec));

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = fts->DispatchRequest(this);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRunnable methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileTransport::Run(void)
{
    while (mState != QUIESCENT && !mSuspended) {
        Process();
    }
    return NS_OK;
}

static NS_METHOD
nsWriteToFile(void* closure,
              const char* fromRawSegment,
              PRUint32 toOffset,
              PRUint32 count,
              PRUint32 *writeCount)
{
    nsIOutputStream* outStr = (nsIOutputStream*)closure;
    nsresult rv = outStr->Write(fromRawSegment, count, writeCount);
    return rv;
}

void
nsFileTransport::Process(void)
{
    nsAutoMonitor mon(mMonitor);

    switch (mState) {
      case START_READ: {
          if (mListener) {
              mStatus = mListener->OnStartRequest(this, mContext);  // always send the start notification
              if (NS_FAILED(mStatus)) {
                  mState = END_READ;
                  return;
              }
          }

          if (mSource == nsnull) {
              nsCOMPtr<nsISupports> fs;

              if (mSpec.IsDirectory()) {
                  mStatus = nsDirectoryIndexStream::Create(mSpec, getter_AddRefs(fs));
              }
              else {
                  mStatus = NS_NewTypicalInputFileStream(getter_AddRefs(fs), mSpec);
              }
              if (NS_FAILED(mStatus)) {
                  mState = END_READ;
                  return;
              }
              mSource = do_QueryInterface(fs, &mStatus);
              if (NS_FAILED(mStatus)) {
                  mState = END_READ;
                  return;
              }
          }

          if (mOffset > 0) {
              // if we need to set a starting offset, QI for the nsIRandomAccessStore and set it
              nsCOMPtr<nsIRandomAccessStore> ras = do_QueryInterface(mSource, &mStatus);
              if (NS_FAILED(mStatus)) {
                  mState = END_READ;
                  return;
              }
              // for now, assume the offset is always relative to the start of the file (position 0)
              // so use PR_SEEK_SET
              mStatus = ras->Seek(PR_SEEK_SET, mOffset);
              if (NS_FAILED(mStatus)) {
                  mState = END_READ;
                  return;
              }
          }

          // capture the total amount for progress information
          if (mProgress) {
              (void)mSource->Available(&mTotalAmount);  // XXX should be content length
          }

          PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
                 ("nsFileTransport: START_READ [this=%x %s]",
                  this, (const char*)mSpec));
          mState = READING;
          break;
      }
      case READING: {
          if (NS_FAILED(mStatus)) {
              mState = END_READ;
              return;
          }

          PRUint32 inLen;
          mStatus = mSource->Available(&inLen);
          if (NS_FAILED(mStatus)) {
              mState = END_READ;
              return;
          }

          // if the user wanted to only read a fixed number of bytes
          // we need to honor that...
          if (mAmount >= 0)
              inLen = PR_MIN(inLen, (PRUint32)mAmount);

          PRUint32 amt;
          mStatus = mBufferOutputStream->WriteFrom(mSource, inLen, &amt);
          PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
                 ("nsFileTransport: READING [this=%x %s] amt=%d status=%x",
                  this, (const char*)mSpec, amt, mStatus));
          if (mStatus == NS_BASE_STREAM_WOULD_BLOCK) {
              mStatus = NS_OK;
              return;
          }
          if (NS_FAILED(mStatus) || amt == 0) {
              mState = END_READ;
              return;
          }

          // and feed the buffer to the application via the buffer stream:
          if (mListener) {
              mStatus = mListener->OnDataAvailable(this, mContext, 
                                                   mBufferInputStream,
                                                   mOffset, amt);
              if (NS_FAILED(mStatus)) {
                  mState = END_READ;
                  return;
              }
          }

          mOffset += amt;
          if (mProgress) {
              (void)mProgress->OnProgress(this, mContext, mOffset, mTotalAmount);
          }

          if (mAmount >= 0) {
              mAmount -= amt;
              if (mAmount == 0) {
                  mState = END_READ;
                  return;
              }
          }

          // stay in the READING state
          break;
      }
      case END_READ: {
          PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
                 ("nsFileTransport: ENDING [this=%x %s] status=%x",
                  this, (const char*)mSpec, mStatus));
          mBufferOutputStream->Flush();
          mBufferOutputStream = null_nsCOMPtr();
          mBufferInputStream = null_nsCOMPtr();
          mSource = null_nsCOMPtr();

          mState = QUIESCENT;

          if (mListener) {
              // XXX where do we get the done message?
              (void)mListener->OnStopRequest(this, mContext, mStatus, nsnull);
              mListener = null_nsCOMPtr();
          }
          if (mProgress) {
              nsAutoString msg = "Read ";
              msg += (const char*)mSpec;
              (void)mProgress->OnStatus(this, mContext, msg.mUStr);
          }
          mContext = null_nsCOMPtr();

          break;
      }
      case START_WRITE: {
          if (mObserver) {
              mStatus = mObserver->OnStartRequest(this, mContext);  // always send the start notification
              if (NS_FAILED(mStatus)) {
                  mState = END_WRITE;
                  return;
              }
          }

          if (mSink == nsnull) {
              nsCOMPtr<nsISupports> fs;

              mStatus = NS_NewTypicalOutputFileStream(getter_AddRefs(fs), mSpec);
              if (NS_FAILED(mStatus)) {
                  mState = END_WRITE;
                  return;
              }

              mSink = do_QueryInterface(fs, &mStatus);
              if (NS_FAILED(mStatus)) {
                  mState = END_WRITE;
                  return;
              }
          }

          if (mOffset > 0) {
              // if we need to set a starting offset, QI for the nsIRandomAccessStore and set it
              nsCOMPtr<nsIRandomAccessStore> ras = do_QueryInterface(mSink, &mStatus);
              if (NS_FAILED(mStatus)) {
                  mState = END_WRITE;
                  return;
              }
              // for now, assume the offset is always relative to the start of the file (position 0)
              // so use PR_SEEK_SET
              mStatus = ras->Seek(PR_SEEK_SET, mOffset);
              if (NS_FAILED(mStatus)) {
                  mState = END_WRITE;
                  return;
              }
              mOffset = 0;
          }

          mBufferInputStream = do_QueryInterface(mSource, &mStatus);
          if (NS_FAILED(mStatus)) {
              // if the given input stream isn't a buffered input
              // stream, then we need to have our own buffer to do the
              // transfer

              mStatus = NS_OK;
              mBuffer = new char[NS_FILE_TRANSPORT_SEGMENT_SIZE];
              if (mBuffer == nsnull) {
                  mStatus = NS_ERROR_OUT_OF_MEMORY;
                  mState = END_WRITE;
                  return;
              }
          }

          // capture the total amount for progress information
          if (mProgress) {
              (void)mSource->Available(&mTotalAmount);  // XXX should be content length
          }

          PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
                 ("nsFileTransport: START_WRITE [this=%x %s]",
                  this, (const char*)mSpec));
          mState = WRITING;
          break;
      }
      case WRITING: {
          if (NS_FAILED(mStatus)) {
              mState = END_WRITE;
              return;
          }

          PRUint32 transferAmt = NS_FILE_TRANSPORT_SEGMENT_SIZE;
          if (mAmount >= 0)
              transferAmt = PR_MIN(NS_FILE_TRANSPORT_SEGMENT_SIZE, mAmount);
          PRUint32 writeAmt;
          if (mBufferInputStream) {
              mStatus = mBufferInputStream->ReadSegments(nsWriteToFile, mSink,
                                                         transferAmt, &writeAmt);
          }
          else {
              PRUint32 readAmt;
              mStatus = mSource->Read(mBuffer, transferAmt, &readAmt);
              if (mStatus == NS_BASE_STREAM_WOULD_BLOCK) {
                  mStatus = NS_OK;
                  return;
              }
              if (NS_FAILED(mStatus) || readAmt == 0) {
                  mState = END_WRITE;
                  return;
              }
              mStatus = mSink->Write(mBuffer, readAmt, &writeAmt);
          }
          PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
                 ("nsFileTransport: WRITING [this=%x %s] amt=%d status=%x",
                  this, (const char*)mSpec, writeAmt, mStatus));
          if (mStatus == NS_BASE_STREAM_WOULD_BLOCK) {
              mStatus = NS_OK;
              return;
          }
          if (NS_FAILED(mStatus) || writeAmt == 0) {
              mState = END_WRITE;
              return;
          }

          mOffset += writeAmt;
          if (mProgress) {
              (void)mProgress->OnProgress(this, mContext, mOffset, mTotalAmount);
          }

          // stay in the WRITING state
          break;
      }
      case END_WRITE: {
          PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
                 ("nsFileTransport: ENDING [this=%x %s] status=%x",
                  this, (const char*)mSpec, mStatus));
          mSink->Flush();
          if (mBufferInputStream)
              mBufferInputStream = null_nsCOMPtr();
          else {
              delete mBuffer;
              mBuffer = nsnull;
          }
          mSink = null_nsCOMPtr();
          mSource = null_nsCOMPtr();

          mState = QUIESCENT;

          if (mObserver) {
              // XXX where do we get the done message?
              (void)mObserver->OnStopRequest(this, mContext, mStatus, nsnull);
              mObserver = null_nsCOMPtr();
          }
          if (mProgress) {
              nsAutoString msg = "Wrote ";
              msg += (const char*)mSpec;
              (void)mProgress->OnStatus(this, mContext, msg.mUStr);
          }
          mContext = null_nsCOMPtr();

          break;
      }
      case QUIESCENT: {
          NS_NOTREACHED("trying to continue a quiescent file transfer");
          break;
      }
    }
}

////////////////////////////////////////////////////////////////////////////////
// nsIPipeObserver methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileTransport::OnFull(nsIPipe* pipe)
{
    return Suspend();
}

NS_IMETHODIMP
nsFileTransport::OnWrite(nsIPipe* pipe, PRUint32 aCount)
{
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::OnEmpty(nsIPipe* pipe)
{
    return Resume();
}

////////////////////////////////////////////////////////////////////////////////
// other nsIChannel methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileTransport::GetURI(nsIURI * *aURI)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::GetLoadAttributes(nsLoadFlags *aLoadAttributes)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::SetLoadAttributes(nsLoadFlags aLoadAttributes)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::GetContentType(char * *aContentType)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::GetContentLength(PRInt32 *aContentLength)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::GetOwner(nsISupports * *aOwner)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::SetOwner(nsISupports * aOwner)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
