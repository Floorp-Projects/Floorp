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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

#include "nsFileTransport.h"
#include "nsFileTransportService.h"
#include "nsIInterfaceRequestor.h"
#include "nsAutoLock.h"
#include "netCore.h"
#include "nsIFileStreams.h"
#include "nsCOMPtr.h"
#include "nsIProxyObjectManager.h"
#include "nsNetUtil.h"

static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);

#define NS_OUTPUT_STREAM_BUFFER_SIZE    (64 * 1024)

#ifdef PR_LOGGING
//
// Log module for nsFileTransport logging...
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
    : mContentType(nsnull),
      mBufferSegmentSize(NS_FILE_TRANSPORT_DEFAULT_SEGMENT_SIZE),
      mBufferMaxSize(NS_FILE_TRANSPORT_DEFAULT_BUFFER_SIZE),
      mXferState(CLOSED),
      mRunState(RUNNING),
      mCancelStatus(NS_OK),
      mMonitor(nsnull),
      mStatus(NS_OK),
      mLoadAttributes(LOAD_NORMAL),
      mOffset(0),
      mTotalAmount(-1),
      mTransferAmount(-1),
      mBuffer(nsnull),
      mService(nsnull)
{
    NS_INIT_REFCNT();

#ifdef PR_LOGGING
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
nsFileTransport::Init(nsFileTransportService *aService, nsIFile* file, PRInt32 ioFlags, PRInt32 perm)
{
    nsresult rv;
    nsCOMPtr<nsIFileIO> io;
    rv = NS_NewFileIO(getter_AddRefs(io), file, ioFlags, perm);
    if (NS_FAILED(rv)) return rv;

    return Init(aService, io);
}

nsresult
nsFileTransport::Init(nsFileTransportService *aService, const char* name, nsIInputStream* inStr,
                      const char* contentType, PRInt32 contentLength)
{
    nsresult rv;
    nsCOMPtr<nsIInputStreamIO> io;
    rv = NS_NewInputStreamIO(getter_AddRefs(io),
                             name, inStr, contentType, contentLength);
    if (NS_FAILED(rv)) return rv;
    return Init(aService, io);
}

nsresult
nsFileTransport::Init(nsFileTransportService *aService, nsIStreamIO* io)
{
    nsresult rv = NS_OK;
    if (mMonitor == nsnull) {
        mMonitor = nsAutoMonitor::NewMonitor("nsFileTransport");
        if (mMonitor == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    mStreamIO = io;
    nsXPIDLCString name;
    rv = mStreamIO->GetName(getter_Copies(name));
    mStreamName = NS_STATIC_CAST(const char*, name);
    NS_ASSERTION(NS_SUCCEEDED(rv), "GetName failed");

    mService = aService;
    PR_AtomicIncrement(&mService->mTotalTransports);

    return rv;
}

nsFileTransport::~nsFileTransport()
{
    if (mXferState != CLOSED) {
        DoClose();
    }
    NS_ASSERTION(mSource == nsnull, "transport not closed");
    NS_ASSERTION(mInputStream == nsnull, "transport not closed");
    NS_ASSERTION(mOutputStream == nsnull, "transport not closed");
    NS_ASSERTION(mSink == nsnull, "transport not closed");
    NS_ASSERTION(mBuffer == nsnull, "transport not closed");
    if (mMonitor)
        nsAutoMonitor::DestroyMonitor(mMonitor);
    if (mContentType)
        nsCRT::free(mContentType);

    PR_AtomicDecrement(&mService->mTotalTransports);

}

NS_IMPL_THREADSAFE_ISUPPORTS5(nsFileTransport, 
                              nsIChannel, 
                              nsIRequest, 
                              nsIRunnable, 
                              nsIInputStreamObserver,
                              nsIOutputStreamObserver);

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
nsFileTransport::GetName(PRUnichar* *result)
{
    nsString name;
    name.AppendWithConversion(mStreamName);
    *result = name.ToNewUnicode();
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsFileTransport::IsPending(PRBool *result)
{
    *result = mXferState != CLOSED;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::GetStatus(nsresult *status)
{
    *status = mRunState == CANCELED ? mCancelStatus : mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::Cancel(nsresult status)
{
    nsAutoMonitor mon(mMonitor);
    NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");

    nsresult rv = NS_OK;
    if (mRunState == SUSPENDED) {
        rv = Resume();
    }
    if (NS_SUCCEEDED(rv)) {
        // if there's no other error pending, say that we aborted
        mRunState = CANCELED;
        mCancelStatus = status;
    }
    PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
           ("nsFileTransport: Cancel [this=%x %s]",
            this, mStreamName.GetBuffer()));
    return rv;
}

NS_IMETHODIMP
nsFileTransport::Suspend()
{
    nsAutoMonitor mon(mMonitor);
    nsresult rv = NS_OK;
    if (mRunState != SUSPENDED) {
        // XXX close the stream here?
        mRunState = SUSPENDED;
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: Suspend [this=%x %s]",
                this, mStreamName.GetBuffer()));
    }
    return rv;
}

NS_IMETHODIMP
nsFileTransport::Resume()
{
    nsAutoMonitor mon(mMonitor);
    nsresult rv = NS_OK;
    if (mRunState == SUSPENDED) {
        // XXX re-open the stream and seek here?
        mRunState = RUNNING;  // set this first before resuming!
        mStatus = mService->DispatchRequest(this);
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: Resume [this=%x %s] status=%x",
                this, mStreamName.GetBuffer(), mStatus));
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// From nsITransport
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileTransport::OpenInputStream(nsIInputStream **result)
{
    nsresult rv;
    nsCOMPtr<nsIInputStream> in;
    rv = mStreamIO->GetInputStream(getter_AddRefs(in));
    if (NS_FAILED(rv)) return rv;
    NS_ASSERTION(mTransferAmount == -1, "need to wrap input stream in one that truncates");
    if (mOffset > 0) {
        nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(in, &rv);
        if (NS_FAILED(rv)) return rv;
        rv = seekable->Seek(nsISeekableStream::NS_SEEK_SET, mOffset);
        if (NS_FAILED(rv)) return rv;
    }
    *result = in;
    NS_ADDREF(*result);
    return rv;
}

NS_IMETHODIMP
nsFileTransport::OpenOutputStream(nsIOutputStream **result)
{
    return mStreamIO->GetOutputStream(result);
}

NS_IMETHODIMP
nsFileTransport::AsyncRead(nsIStreamListener *listener, nsISupports *ctxt)
{
    nsresult rv = NS_OK;

    if (mXferState != CLOSED)
        return NS_ERROR_IN_PROGRESS;

    NS_ASSERTION(listener, "need to supply an nsIStreamListener");
    rv = NS_NewAsyncStreamListener(getter_AddRefs(mListener), 
                                   listener, nsnull);
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewPipe(getter_AddRefs(mInputStream),
                    getter_AddRefs(mOutputStream),
                    mBufferSegmentSize, mBufferMaxSize,
                    PR_TRUE, PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    rv = mInputStream->SetObserver(this);
    if (NS_FAILED(rv)) return rv;
    rv = mOutputStream->SetObserver(this);
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(mContext == nsnull, "context not released");
    mContext = ctxt;
    mXferState = OPEN_FOR_READ;

    PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
           ("nsFileTransport: AsyncRead [this=%x %s] mOffset=%d mTransferAmount=%d",
            this, mStreamName.GetBuffer(), mOffset, mTransferAmount));

    rv = mService->DispatchRequest(this);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::AsyncWrite(nsIInputStream *fromStream,
                            nsIStreamObserver *observer,
                            nsISupports *ctxt)
{
    nsresult rv = NS_OK;

    if (mXferState != CLOSED)
        return NS_ERROR_IN_PROGRESS;

    if (observer) {
        rv = NS_NewAsyncStreamObserver(getter_AddRefs(mObserver),
                                       observer, NS_CURRENT_EVENTQ);
        if (NS_FAILED(rv)) return rv;
    }

    NS_ASSERTION(mContext == nsnull, "context not released");
    mContext = ctxt;
    mXferState = OPEN_FOR_WRITE;
    mSource = fromStream;

    PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
           ("nsFileTransport: AsyncWrite [this=%x %s]",
            this, mStreamName.GetBuffer()));

    rv = mService->DispatchRequest(this);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRunnable methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileTransport::Run(void)
{
    while (mXferState != CLOSED && mRunState != SUSPENDED) {
        if (mRunState == CANCELED) {
            if (mXferState == READING)
                mXferState = END_READ;
            else if (mXferState == WRITING)
                mXferState = END_WRITE;
            else
                mXferState = CLOSING;
            mStatus = mCancelStatus;
        }
        Process();
    }
    return NS_OK;
}

static NS_METHOD
nsWriteToFile(nsIInputStream* in,
              void* closure,
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
    switch (mXferState) {
      case OPEN_FOR_READ: { 
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: OPEN_FOR_READ [this=%x %s]",
                this, mStreamName.GetBuffer()));
        mStatus = mStreamIO->Open(&mContentType, &mTotalAmount);
        if (mListener) {
            nsresult rv = mListener->OnStartRequest(this, mContext);  // always send the start notification
            if (NS_SUCCEEDED(mStatus))
                mStatus = rv;
        }

        mXferState = NS_FAILED(mStatus) ? END_READ : START_READ;
        break;
      }

      case START_READ: {
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: START_READ [this=%x %s]",
                this, mStreamName.GetBuffer()));

        PR_AtomicIncrement(&mService->mInUseTransports);

        mStatus = mStreamIO->GetInputStream(getter_AddRefs(mSource));
        if (NS_FAILED(mStatus)) {
            mXferState = END_READ;
            return;
        }

        if (mOffset > 0) {
            // if we need to set a starting offset, QI for the nsISeekableStream and set it
            nsCOMPtr<nsISeekableStream> ras = do_QueryInterface(mSource, &mStatus);
            if (NS_FAILED(mStatus)) {
                mXferState = END_READ;
                return;
            }
            // for now, assume the offset is always relative to the start of the file (position 0)
            // so use PR_SEEK_SET
            mStatus = ras->Seek(PR_SEEK_SET, mOffset);
            if (NS_FAILED(mStatus)) {
                mXferState = END_READ;
                return;
            }
        }

        // capture the total amount for progress information
        if (mTransferAmount < 0) {
            mTransferAmount = mTotalAmount;
        }
        mTotalAmount = mTransferAmount;

        mXferState = READING;
        break;
      }

      case READING: {
        PRUint32 writeAmt;
        // and feed the buffer to the application via the buffer stream:
        mStatus = mOutputStream->WriteFrom(mSource, mTransferAmount, &writeAmt);
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: READING [this=%x %s] amt=%d status=%x",
                this, mStreamName.GetBuffer(), writeAmt, mStatus));
        if (mStatus == NS_BASE_STREAM_WOULD_BLOCK) {
            mStatus = NS_OK;
            return;
        }
        if (NS_FAILED(mStatus) || writeAmt == 0) {
            PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
                   ("nsFileTransport: READING [this=%x %s] %s writing to buffered output stream",
                    this, mStreamName.GetBuffer(), NS_SUCCEEDED(mStatus) ? "done" : "error"));
            mXferState = END_READ;
            return;
        }
        if (mTransferAmount > 0) {
            mTransferAmount -= writeAmt;
            PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
                   ("nsFileTransport: READING [this=%x %s] %d bytes left to transfer",
                    this, mStreamName.GetBuffer(), mTransferAmount));
        }
        PRUint32 offset = mOffset;
        mOffset += writeAmt;
        if (mListener) {
            mStatus = mListener->OnDataAvailable(this, mContext,
                                                 mInputStream,
                                                 offset, writeAmt);
            if (NS_FAILED(mStatus)) {
                PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
                       ("nsFileTransport: READING [this=%x %s] error notifying stream listener",
                        this, mStreamName.GetBuffer()));

                mXferState = END_READ;
                return;
            }
        }

        if (mProgress && mTransferAmount >= 0 && 
            !(mLoadAttributes & LOAD_BACKGROUND)) {
            nsresult rv = mProgress->OnProgress(this, mContext,
                                    mTotalAmount - mTransferAmount,
                                    mTotalAmount);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unexpected OnProgress failure");
        }

        if (mTransferAmount == 0) {
            mXferState = END_READ;
            return;
        }

        // stay in the READING state
        break;
      }

      case END_READ: {
        
        PR_AtomicDecrement(&mService->mInUseTransports);

        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: END_READ [this=%x %s] status=%x",
                this, mStreamName.GetBuffer(), mStatus));

#if defined (DEBUG_dougt) || defined (DEBUG_warren)
        NS_ASSERTION(mTransferAmount <= 0 || NS_FAILED(mStatus), "didn't transfer all the data");
#endif 
        if (mTransferAmount > 0 && NS_SUCCEEDED(mStatus)) {
            // This happens when the requested read amount is more than the amount
            // of the data in the stream/file.
            mStatus = NS_BASE_STREAM_CLOSED;
        }
        mOutputStream->Flush();
        mOutputStream = null_nsCOMPtr();
        mInputStream = null_nsCOMPtr();

        mSource = null_nsCOMPtr();

        nsresult rv;
        if (mListener) {
            rv = mListener->OnStopRequest(this, mContext, mStatus, nsnull);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unexpected OnStopRequest failure");
            mListener = null_nsCOMPtr();
        }
        if (mProgress && !(mLoadAttributes & LOAD_BACKGROUND)) {
            nsAutoString fileName; fileName.AssignWithConversion(mStreamName);
            rv = mProgress->OnStatus(this, mContext, 
                                     NS_NET_STATUS_READ_FROM, 
                                     fileName.GetUnicode());
            NS_ASSERTION(NS_SUCCEEDED(rv), "unexpected OnStopRequest failure");
        }
        mContext = null_nsCOMPtr();

        mXferState = CLOSING;
        break;
      }

      case OPEN_FOR_WRITE: {
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: OPEN_FOR_WRITE [this=%x %s]",
                this, mStreamName.GetBuffer()));
        mStatus = mStreamIO->Open(&mContentType, &mTotalAmount);
        if (mObserver) {
            nsresult rv = mObserver->OnStartRequest(this, mContext);  // always send the start notification
            if (NS_SUCCEEDED(mStatus))
                mStatus = rv;
        }

        mXferState = NS_FAILED(mStatus) ? END_WRITE : START_WRITE;
        break;
      }

      case START_WRITE: {
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: START_WRITE [this=%x %s]",
                this, mStreamName.GetBuffer()));

        PR_AtomicIncrement(&mService->mInUseTransports);

        mStatus = mStreamIO->GetOutputStream(getter_AddRefs(mSink));
        if (NS_FAILED(mStatus)) {
            mXferState = END_WRITE;
            return;
        }

        if (mOffset > 0) {
            // if we need to set a starting offset, QI for the nsISeekableStream and set it
            nsCOMPtr<nsISeekableStream> ras = do_QueryInterface(mSink, &mStatus);
            if (NS_FAILED(mStatus)) {
                mXferState = END_WRITE;
                return;
            }
            // for now, assume the offset is always relative to the start of the file (position 0)
            // so use PR_SEEK_SET
            mStatus = ras->Seek(PR_SEEK_SET, mOffset);
            if (NS_FAILED(mStatus)) {
                mXferState = END_WRITE;
                return;
            }
            mOffset = 0;
        }

        mInputStream = do_QueryInterface(mSource, &mStatus);
        if (NS_FAILED(mStatus)) {
            // if the given input stream isn't a buffered input
            // stream, then we need to have our own buffer to do the
            // transfer

            mStatus = NS_OK;
            mBuffer = new char[mBufferSegmentSize];
            if (mBuffer == nsnull) {
                mStatus = NS_ERROR_OUT_OF_MEMORY;
                mXferState = END_WRITE;
                return;
            }
        }

        mXferState = WRITING;
        break;
      }

      case WRITING: {
        PRUint32 transferAmt = mBufferSegmentSize;
        if (mTransferAmount >= 0)
            transferAmt = PR_MIN(mBufferSegmentSize, (PRUint32)mTransferAmount);
        PRUint32 writeAmt;
        if (mInputStream) {
            mStatus = mInputStream->ReadSegments(nsWriteToFile, mSink,
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
                mXferState = END_WRITE;
                return;
            }
            mStatus = mSink->Write(mBuffer, readAmt, &writeAmt);
        }
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: WRITING [this=%x %s] amt=%d status=%x",
                this, mStreamName.GetBuffer(), writeAmt, mStatus));
        if (mStatus == NS_BASE_STREAM_WOULD_BLOCK) {
            mStatus = NS_OK;
            return;
        }
        if (NS_FAILED(mStatus) || writeAmt == 0) {
            mXferState = END_WRITE;
            return;
        }

        mTransferAmount -= writeAmt;
        mOffset += writeAmt;
        if (mProgress && !(mLoadAttributes & LOAD_BACKGROUND)) {
            (void)mProgress->OnProgress(this, mContext,
                                        mTotalAmount - mTransferAmount,
                                        mTotalAmount);
        }

        // stay in the WRITING state
        break;
      }

      case END_WRITE: {
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: END_WRITE [this=%x %s] status=%x",
                this, mStreamName.GetBuffer(), mStatus));

        PR_AtomicDecrement(&mService->mInUseTransports);

#if defined (DEBUG_dougt) || defined (DEBUG_warren)
        NS_ASSERTION(mTransferAmount <= 0 || NS_FAILED(mStatus), "didn't transfer all the data");
#endif 
        if (mTransferAmount > 0 && NS_SUCCEEDED(mStatus)) {
            // This happens when the requested write amount is more than the amount
            // of the data in the stream/file.
            mStatus = NS_BASE_STREAM_CLOSED;
        }

        if (mSink) {
            mSink->Flush();
            mSink = null_nsCOMPtr();
        }
        if (mInputStream) {
            mInputStream = null_nsCOMPtr();
        }
        else if (mBuffer) {
            delete mBuffer;
            mBuffer = nsnull;
        }
        if (mSource) {
            (void)mSource->Close();
            mSource = null_nsCOMPtr();
        }

        nsresult rv;
        if (mObserver) {
            rv = mObserver->OnStopRequest(this, mContext, mStatus, nsnull);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unexpected OnStopRequest failure");
            mObserver = null_nsCOMPtr();
        }
        if (mProgress && !(mLoadAttributes & LOAD_BACKGROUND)) {
            nsAutoString fileName; fileName.AssignWithConversion(mStreamName);
            rv = mProgress->OnStatus(this, mContext,
                                     NS_NET_STATUS_WROTE_TO, 
                                     fileName.GetUnicode());
            NS_ASSERTION(NS_SUCCEEDED(rv), "unexpected OnStopRequest failure");
        }
        mContext = null_nsCOMPtr();

        mXferState = CLOSING;
        break;
      }

      case CLOSING: {
        DoClose();
        break;
      }

      case CLOSED: {
        NS_NOTREACHED("trying to continue a quiescent file transfer");
        break;
      }
    }
}

void
nsFileTransport::DoClose(void)
{
    PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
           ("nsFileTransport: CLOSING [this=%x %s] status=%x",
            this, mStreamName.GetBuffer(), mStatus));

    if (mStreamIO) {
        nsresult rv = mStreamIO->Close(mStatus);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unexpected Close failure");
        mStreamIO = null_nsCOMPtr();
    }
    mXferState = CLOSED;

    PR_AtomicDecrement(&mService->mConnectedTransports);
}

////////////////////////////////////////////////////////////////////////////////
// nsIInputStreamObserver/nsIOutputStreamObserver methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileTransport::OnFull(nsIOutputStream* out)
{
    return Suspend();
}

NS_IMETHODIMP
nsFileTransport::OnWrite(nsIOutputStream* out, PRUint32 aCount)
{
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::OnEmpty(nsIInputStream* in)
{
    return Resume();
}

NS_IMETHODIMP
nsFileTransport::OnClose(nsIInputStream* in)
{
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// other nsIChannel methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileTransport::GetOriginalURI(nsIURI* *aURI)
{
    NS_NOTREACHED("GetOriginalURI");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::SetOriginalURI(nsIURI* aURI)
{
    NS_NOTREACHED("SetOriginalURI");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::GetURI(nsIURI* *aURI)
{
//    NS_NOTREACHED("GetURI");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::SetURI(nsIURI* aURI)
{
    NS_NOTREACHED("SetURI");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::GetLoadAttributes(nsLoadFlags *aLoadAttributes)
{
    *aLoadAttributes = mLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::SetLoadAttributes(nsLoadFlags aLoadAttributes)
{
    mLoadAttributes = aLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::GetContentType(char * *aContentType)
{
    *aContentType = nsCRT::strdup(mContentType);
    if (*aContentType == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::SetContentType(const char *aContentType)
{
    if (mContentType) {
      nsCRT::free(mContentType);
    }
    mContentType = nsCRT::strdup(aContentType);
    if (!mContentType) return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::GetContentLength(PRInt32 *aContentLength)
{
    *aContentLength = mTotalAmount;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::SetContentLength(PRInt32 aContentLength)
{
    NS_NOTREACHED("SetContentLength");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::GetTransferOffset(PRUint32 *aTransferOffset)
{
    *aTransferOffset = mOffset;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::SetTransferOffset(PRUint32 aTransferOffset)
{
    mOffset = aTransferOffset;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::GetTransferCount(PRInt32 *aTransferCount)
{
    *aTransferCount = mTransferAmount;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::SetTransferCount(PRInt32 aTransferCount)
{
    mTransferAmount = aTransferCount;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::GetBufferSegmentSize(PRUint32 *aBufferSegmentSize)
{
    *aBufferSegmentSize = mBufferSegmentSize;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::SetBufferSegmentSize(PRUint32 aBufferSegmentSize)
{
    mBufferSegmentSize = aBufferSegmentSize;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::GetBufferMaxSize(PRUint32 *aBufferMaxSize)
{
    *aBufferMaxSize = mBufferMaxSize;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::SetBufferMaxSize(PRUint32 aBufferMaxSize)
{
    mBufferMaxSize = aBufferMaxSize;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::GetLocalFile(nsIFile* *file)
{
    nsresult rv;
    nsCOMPtr<nsIFileIO> fileIO = do_QueryInterface(mStreamIO, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = fileIO->GetFile(file);
    if (NS_FAILED(rv)) {
        *file = nsnull;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::GetPipeliningAllowed(PRBool *aPipeliningAllowed)
{
    *aPipeliningAllowed = PR_FALSE;
    return NS_OK;
}
 
NS_IMETHODIMP
nsFileTransport::SetPipeliningAllowed(PRBool aPipeliningAllowed)
{
    NS_NOTREACHED("SetPipeliningAllowed");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::GetOwner(nsISupports * *aOwner)
{
    NS_NOTREACHED("GetOwner");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::SetOwner(nsISupports * aOwner)
{
    NS_NOTREACHED("SetOwner");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
    NS_NOTREACHED("GetLoadGroup");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    NS_NOTREACHED("SetLoadGroup");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
    *aNotificationCallbacks = mCallbacks.get();
    NS_IF_ADDREF(*aNotificationCallbacks);
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
    mCallbacks = aNotificationCallbacks;

    // Get a nsIProgressEventSink so that we can fire status/progress on it-
    if (mCallbacks) {
        nsCOMPtr<nsISupports> sink;
        nsresult rv = mCallbacks->GetInterface(NS_GET_IID(nsIProgressEventSink),
                                               getter_AddRefs(sink));
        if (NS_FAILED(rv)) return NS_OK;        // don't need a progress event sink

        // Now generate a proxied event sink
        NS_WITH_SERVICE(nsIProxyObjectManager,
                        proxyMgr, kProxyObjectManagerCID, &rv);
        if (NS_FAILED(rv)) return rv;
        
        rv = proxyMgr->GetProxyForObject(NS_UI_THREAD_EVENTQ, // primordial thread - should change?
                                      NS_GET_IID(nsIProgressEventSink),
                                      sink,
                                      PROXY_ASYNC | PROXY_ALWAYS,
                                      getter_AddRefs(mProgress));
    }
    return NS_OK;
}


NS_IMETHODIMP 
nsFileTransport::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}
////////////////////////////////////////////////////////////////////////////////
