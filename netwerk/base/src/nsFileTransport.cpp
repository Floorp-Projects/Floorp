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
#include "nscore.h"
#include "nsIInterfaceRequestor.h"
#include "nsIURI.h"
#include "nsIEventQueue.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIBufferInputStream.h"
#include "nsIBufferOutputStream.h"
#include "nsAutoLock.h"
#include "netCore.h"
#include "nsIFileStreams.h"
#include "nsISimpleEnumerator.h"
#include "nsIURL.h"
#include "prio.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsSpecialSystemDirectory.h"
#include "nsDirectoryIndexStream.h"
#include "nsEscape.h"
#include "nsIMIMEService.h"
#include "nsProxyObjectManager.h"
#include "nsNetUtil.h"
#include "nsInt64.h"

#define NS_INPUT_STREAM_BUFFER_SIZE     (16 * 1024)
#define NS_OUTPUT_STREAM_BUFFER_SIZE    (64 * 1024)

//#define NO_BUFFERING 1

static NS_DEFINE_CID(kMIMEServiceCID, NS_MIMESERVICE_CID);
static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);

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

#define DEFAULT_TYPE "text/html"

class nsLocalFileSystem : public nsIFileSystem
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Open(char **contentType, PRInt32 *contentLength) {
        // don't actually open the file here -- we'll do it on demand in the
        // GetInputStream/GetOutputStream methods
        nsresult rv = NS_OK;

        // We'll try to use the file's length, if it has one. If not,
        // assume the file to be special, and set the content length
        // to -1, which means "read the stream until exhausted".
        PRInt64 size;
        rv = mFile->GetFileSize(&size);
        if (NS_SUCCEEDED(rv)) {
            *contentLength = nsInt64(size);
            if (! *contentLength)
                *contentLength = -1;
        }
        else 
            *contentLength = -1;

        PRBool isDir;
        rv = mFile->IsDirectory(&isDir);
        if (NS_SUCCEEDED(rv) && isDir) {
            // Directories turn into an HTTP-index stream, with
            // unbounded (i.e., read 'til the stream says it's done)
            // length.
            *contentType = nsCRT::strdup("application/http-index-format");
            *contentLength = -1;
        }
        else {
            char* fileName;
            rv = mFile->GetLeafName(&fileName);
            if (NS_FAILED(rv)) return rv;
            if (fileName != nsnull) {
	            PRInt32 len = nsCRT::strlen(fileName);
	            const char* ext = nsnull;
	            for (PRInt32 i = len; i >= 0; i--) {
	                if (fileName[i] == '.') {
	                    ext = &fileName[i + 1];
	                    break;
	                }
	            }

	            if (ext) {
	                NS_WITH_SERVICE(nsIMIMEService, mimeServ, kMIMEServiceCID, &rv);
	                if (NS_SUCCEEDED(rv)) {
	                    rv = mimeServ->GetTypeFromExtension(ext, contentType);
	                }
	            }
	            else
	                rv = NS_ERROR_FAILURE;

				nsCRT::free(fileName);
			} else {
				rv = NS_ERROR_FAILURE;
			}

            if (NS_FAILED(rv)) {
                // if all else fails treat it as text/html?
                *contentType = nsCRT::strdup(DEFAULT_TYPE);
                if (*contentType == nsnull)
                    rv = NS_ERROR_OUT_OF_MEMORY;
                else
                    rv = NS_OK;
            }
        }
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: logically opening %s: type=%s len=%d",
                (const char*)mSpec, *contentType, *contentLength));
        return rv;
    }

    NS_IMETHOD Close(nsresult status) {
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: logically closing %s: status=%x",
                (const char*)mSpec, status));
        return NS_OK;
    }

    NS_IMETHOD GetInputStream(nsIInputStream * *aInputStream) {
        nsresult rv;
        PRBool isDir;
        rv = mFile->IsDirectory(&isDir);
        if (NS_SUCCEEDED(rv) && isDir) {
            rv = nsDirectoryIndexStream::Create(mFile, aInputStream);
            PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
                   ("nsFileTransport: opening local dir %s for input (%x)",
                    (const char*)mSpec, rv));
            return rv;
        }

        nsCOMPtr<nsIInputStream> fileIn;
        rv = NS_NewFileInputStream(mFile, getter_AddRefs(fileIn));
        if (NS_FAILED(rv))
        {
#if DEBUG
          char* filePath = nsnull;
          mFile->GetPath(&filePath);
          if (filePath)
          {
            printf("Opening %s failed\n", filePath);
            nsAllocator::Free(filePath);
          }
#endif        
          return rv;
        }
        
#ifdef NO_BUFFERING
        *aInputStream = fileIn;
        NS_ADDREF(*aInputStream);
#else
        rv = NS_NewBufferedInputStream(fileIn, NS_OUTPUT_STREAM_BUFFER_SIZE,
                                       aInputStream);
#endif

        // printf("opening %s for reading\n", (const char*)mSpec);
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: opening local file %s for input (%x)",
                (const char*)mSpec, rv));
        return rv;
    }

    NS_IMETHOD GetOutputStream(nsIOutputStream * *aOutputStream) {
        nsresult rv;
        PRBool isDir;
        rv = mFile->IsDirectory(&isDir);
        if (NS_SUCCEEDED(rv) && isDir) {
            return NS_ERROR_FAILURE;
        }

        nsCOMPtr<nsIOutputStream> fileOut;
        rv = NS_NewFileOutputStream(mFile, 
                                    PR_CREATE_FILE | PR_WRONLY,
                                    0664,
                                    getter_AddRefs(fileOut));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIOutputStream> bufStr;
#ifdef NO_BUFFERING
        bufStr = fileOut;
#else
        rv = NS_NewBufferedOutputStream(fileOut, NS_OUTPUT_STREAM_BUFFER_SIZE,
                                        getter_AddRefs(bufStr));
        if (NS_FAILED(rv)) return rv;
#endif

        *aOutputStream = bufStr;
        NS_ADDREF(*aOutputStream);

        // printf("opening %s for writing\n", (const char*)mSpec);
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: opening local file %s for output (%x)",
                (const char*)mSpec, rv));
        return rv;
    }

    nsLocalFileSystem(nsIFile* file) : mFile(file) {
        NS_INIT_REFCNT();
//#ifdef PR_LOGGING
        (void)mFile->GetPath(&mSpec);
//#endif
    }

    virtual ~nsLocalFileSystem() {
//#ifdef PR_LOGGING
        if (mSpec) nsCRT::free(mSpec);
//#endif
    }

    static nsresult Create(nsIFile* file, nsIFileSystem* *result) {
        nsLocalFileSystem* fs = new nsLocalFileSystem(file);
        if (fs == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(fs);
        *result = fs;
        return NS_OK;
    }

protected:
    nsCOMPtr<nsIFile>   mFile;
//#ifdef PR_LOGGING
    char*               mSpec;
//#endif
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsLocalFileSystem, nsIFileSystem);

////////////////////////////////////////////////////////////////////////////////

class nsInputStreamFileSystem : public nsIFileSystem
{
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD Open(char **contentType, PRInt32 *contentLength) {
        *contentType = nsCRT::strdup(mContentType);
        if (*contentType == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        *contentLength = mContentLength;
        return NS_OK;
    }

    NS_IMETHOD Close(nsresult status) {
        return NS_OK;
    }

    NS_IMETHOD GetInputStream(nsIInputStream * *aInputStream) {
        *aInputStream = mInput;
        NS_ADDREF(*aInputStream);
        return NS_OK;
    }

    NS_IMETHOD GetOutputStream(nsIOutputStream * *aOutputStream) {
        return NS_ERROR_FAILURE;        // we only do input here
    }

    nsInputStreamFileSystem() {
        NS_INIT_REFCNT();
    }

    nsresult Init(nsIInputStream* in, const char* contentType,
                  PRInt32 contentLength) {
        mInput = in;
        NS_ASSERTION(contentType, "content type not supplied");
        mContentType = nsCRT::strdup(contentType);
        if (mContentType == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        mContentLength = contentLength;
        return NS_OK;
    }

    virtual ~nsInputStreamFileSystem() {
        if (mContentType) nsCRT::free(mContentType);
    }

    static nsresult Create(nsIInputStream* in, const char* contentType,
                           PRInt32 contentLength, nsIFileSystem* *result) {
        nsInputStreamFileSystem* fs = new nsInputStreamFileSystem();
        if (fs == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(fs);
        nsresult rv = fs->Init(in, contentType, contentLength);
        if (NS_FAILED(rv)) {
            NS_RELEASE(fs);
            return rv;
        }
        *result = fs;
        return NS_OK;
    }

protected:
    nsCOMPtr<nsIInputStream>    mInput;
    char*                       mContentType;
    PRInt32                     mContentLength;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsInputStreamFileSystem, nsIFileSystem);

////////////////////////////////////////////////////////////////////////////////

nsFileTransport::nsFileTransport()
    : mContentType(nsnull),
      mState(CLOSED),
      mCommand(NONE),
      mSuspended(PR_FALSE),
      mMonitor(nsnull),
      mStatus(NS_OK),
      mOffset(0),
      mTotalAmount(-1),
      mTransferAmount(0),
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

    mSpec = nsnull;
#endif /* PR_LOGGING */
}

nsresult
nsFileTransport::Init(nsIFile* file, PRInt32 mode, const char* command,
                      PRUint32 bufferSegmentSize, PRUint32 bufferMaxSize)
{
    nsresult rv;
    mFile = file;
#if 0
    nsCOMPtr<nsIFileChannel> channel;
    rv = NS_NewFileChannel(file,
                           mode, 
                           nsnull,      // contentType -- infer
                           0,           // contentLength -- infer
                           nsnull,      // loadGroup
                           nsnull,      // notificationCallbacks
                           nsIChannel::LOAD_NORMAL,
                           nsnull,      // originalURI
                           bufferSegmentSize, 
                           bufferMaxSize,
                           getter_AddRefs(channel));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIFileSystem> fsObj = do_QueryInterface(channel, &rv);
#else
    nsCOMPtr<nsIFileSystem> fsObj;
    rv = nsLocalFileSystem::Create(file, getter_AddRefs(fsObj));
#endif
    if (NS_FAILED(rv)) return rv;
    return Init(fsObj, command, bufferSegmentSize, bufferMaxSize);
}

nsresult
nsFileTransport::Init(nsIInputStream* fromStream, const char* contentType,
                      PRInt32 contentLength, const char* command,
                      PRUint32 bufferSegmentSize, PRUint32 bufferMaxSize)
{
    nsresult rv;
    nsCOMPtr<nsIFileSystem> fsObj;
    rv = nsInputStreamFileSystem::Create(fromStream, contentType, contentLength,
                                         getter_AddRefs(fsObj));
    if (NS_FAILED(rv)) return rv;
    return Init(fsObj, command, bufferSegmentSize, bufferMaxSize);
}

nsresult
nsFileTransport::Init(nsIFileSystem* fsObj, const char* command,
                      PRUint32 bufferSegmentSize, PRUint32 bufferMaxSize)
{
    nsresult rv = NS_OK;
    if (mMonitor == nsnull) {
        mMonitor = nsAutoMonitor::NewMonitor("nsFileTransport");
        if (mMonitor == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    mFileObject = fsObj;
    mBufferSegmentSize = bufferSegmentSize != 0
        ? bufferSegmentSize : NS_FILE_TRANSPORT_DEFAULT_SEGMENT_SIZE;
    mBufferMaxSize = bufferMaxSize != 0
        ? bufferMaxSize : NS_FILE_TRANSPORT_DEFAULT_BUFFER_SIZE;
#ifdef PR_LOGGING
    if (mFile)
        (void)mFile->GetPath(&mSpec);
    else
        mSpec = nsCRT::strdup("<unknown>");
#endif
    return rv;
}

nsFileTransport::~nsFileTransport()
{
    if (mState != CLOSED) {
        DoClose();
    }
    NS_ASSERTION(mSource == nsnull, "transport not closed");
    NS_ASSERTION(mBufferInputStream == nsnull, "transport not closed");
    NS_ASSERTION(mBufferOutputStream == nsnull, "transport not closed");
    NS_ASSERTION(mSink == nsnull, "transport not closed");
    NS_ASSERTION(mBuffer == nsnull, "transport not closed");
    if (mMonitor)
        nsAutoMonitor::DestroyMonitor(mMonitor);
    if (mContentType)
        nsCRT::free(mContentType);
#ifdef PR_LOGGING
    if (mSpec)
        nsCRT::free(mSpec);
#endif
}

NS_IMPL_THREADSAFE_ISUPPORTS4(nsFileTransport, 
                              nsIChannel, 
                              nsIRequest, 
                              nsIRunnable, 
                              nsIPipeObserver);

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
    *result = mState != CLOSED;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::Cancel()
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv = NS_OK;
    if (mSuspended) {
        rv = Resume();
    }
    if (NS_SUCCEEDED(rv)) {
        // if there's no other error pending, say that we aborted
        mStatus = NS_BINDING_ABORTED;
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

    if (mState != CLOSED)
        return NS_ERROR_IN_PROGRESS;

    PRBool exists;
    rv = mFile->Exists(&exists);
    if (NS_FAILED(rv)) return rv;

    if (!exists)
        return NS_ERROR_FAILURE;        // XXX probably need NS_BASE_STREAM_FILE_NOT_FOUND or something

    rv = NS_NewPipe(getter_AddRefs(mBufferInputStream),
                    getter_AddRefs(mBufferOutputStream),
                    this,     // nsIPipeObserver
                    mBufferSegmentSize, mBufferMaxSize);
    if (NS_FAILED(rv)) return rv;

    rv = mBufferOutputStream->SetNonBlocking(PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    mState = OPENING;
    mCommand = INITIATE_READ;
    mOffset = startPosition;
    mTransferAmount = readCount;
    mListener = null_nsCOMPtr();

    *result = mBufferInputStream.get();
    NS_ADDREF(*result);
    PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
           ("nsFileTransport: OpenInputStream [this=%x %s] startPosition=%d readCount=%d",
            this, (const char*)mSpec, startPosition, readCount));

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

    if (mState != CLOSED)
        return NS_ERROR_IN_PROGRESS;

    nsCOMPtr<nsIOutputStream> fileOut;
    rv = NS_NewFileOutputStream(mFile, 
                                PR_CREATE_FILE | PR_WRONLY,
                                0664,
                                getter_AddRefs(fileOut));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIOutputStream> bufStr;
#ifdef NO_BUFFERING
    bufStr = fileOut;
#else
    rv = NS_NewBufferedOutputStream(fileOut, NS_OUTPUT_STREAM_BUFFER_SIZE,
                                    getter_AddRefs(bufStr));
    if (NS_FAILED(rv)) return rv;
#endif

    *result = bufStr;
    NS_ADDREF(*result);

    mOffset = startPosition;
    if (mOffset > 0) {
        // if we need to set a starting offset, QI for nsISeekableStream
        nsCOMPtr<nsISeekableStream> ras =
            do_QueryInterface(*result, &mStatus);
        if (NS_FAILED(mStatus))
            return NS_ERROR_IN_PROGRESS;

        // For now, assume the offset is always relative to the
        // start of the file (position 0), so use PR_SEEK_SET
        mStatus = ras->Seek(PR_SEEK_SET, mOffset);
        if (NS_FAILED(mStatus))
            return NS_ERROR_IN_PROGRESS;
    }

    PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
           ("nsFileTransport: OpenOutputStream [this=%x %s]",
            this, (const char*)mSpec));
    return rv;
}

NS_IMETHODIMP
nsFileTransport::AsyncOpen(nsIStreamObserver *observer, nsISupports* ctxt)
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv = NS_OK;

    if (mState != CLOSED)
        return NS_ERROR_IN_PROGRESS;

    NS_ASSERTION(observer, "need to supply an nsIStreamObserver");
    rv = NS_NewAsyncStreamObserver(observer, NS_CURRENT_EVENTQ, getter_AddRefs(mOpenObserver));
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(mOpenContext == nsnull, "context not released");
    mOpenContext = ctxt;

    mState = OPENING;
    NS_ASSERTION(mCommand == NONE, "out of sync");

    PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
           ("nsFileTransport: AsyncOpen [this=%x %s]",
            this, (const char*)mSpec));

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = fts->DispatchRequest(this);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                           nsISupports *ctxt,
                           nsIStreamListener *listener)
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv = NS_OK;

    if ((mState != CLOSED && mState != OPENING && mState != OPENED)
        || mCommand != NONE)
        return NS_ERROR_IN_PROGRESS;

    NS_ASSERTION(listener, "need to supply an nsIStreamListener");
    rv = NS_NewAsyncStreamListener(listener, nsnull, getter_AddRefs(mListener));
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewPipe(getter_AddRefs(mBufferInputStream),
                    getter_AddRefs(mBufferOutputStream),
                    this,       // nsIPipeObserver
                    mBufferSegmentSize, mBufferMaxSize);
    if (NS_FAILED(rv)) return rv;

    rv = mBufferInputStream->SetNonBlocking(PR_TRUE);
    if (NS_FAILED(rv)) return rv;
    rv = mBufferOutputStream->SetNonBlocking(PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(mContext == nsnull, "context not released");
    mContext = ctxt;

    if (mState == CLOSED)
        mState = OPENING;
    mCommand = INITIATE_READ;
    mOffset = startPosition;
    mTransferAmount = readCount;

    PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
           ("nsFileTransport: AsyncRead [this=%x %s] startPosition=%d readCount=%d",
            this, (const char*)mSpec, startPosition, readCount));

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

    if ((mState != CLOSED && mState != OPENING && mState != OPENED)
        || mCommand != NONE)
        return NS_ERROR_IN_PROGRESS;

    if (observer) {
        rv = NS_NewAsyncStreamObserver(observer, NS_CURRENT_EVENTQ, getter_AddRefs(mObserver));
        if (NS_FAILED(rv)) return rv;
    }

    NS_ASSERTION(mContext == nsnull, "context not released");
    mContext = ctxt;

    if (mState == CLOSED)
        mState = OPENING;
    mCommand = INITIATE_WRITE;
    mOffset = startPosition;
    mTransferAmount = writeCount;
    mSource = fromStream;

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
    while (mState != CLOSED && mState != OPENED && !mSuspended) {
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
      case OPENING: {
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: OPENING [this=%x %s]",
                this, (const char*)mSpec));
        mStatus = mFileObject->Open(&mContentType, &mTotalAmount);
        if (NS_SUCCEEDED(mStatus) && mOpenObserver) {
            mStatus = mOpenObserver->OnStartRequest(this, mOpenContext);
        }
        switch (mCommand) {
          case INITIATE_READ:
            mState = NS_FAILED(mStatus) ? END_READ : START_READ;
            break;
          case INITIATE_WRITE:
            mState = NS_FAILED(mStatus) ? END_WRITE : START_WRITE;
            break;
          default:
            mState = NS_FAILED(mStatus) ? CLOSING : OPENED;
            break;
        }
        break;
      }

      case OPENED: {
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: OPENED [this=%x %s]",
                this, (const char*)mSpec));
        break;
      }

      case START_READ: {
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: START_READ [this=%x %s]",
                this, (const char*)mSpec));

        mStatus = mFileObject->GetInputStream(getter_AddRefs(mSource));
        if (NS_FAILED(mStatus)) {
            mState = END_READ;
            return;
        }

        if (mOffset > 0) {
            // if we need to set a starting offset, QI for the nsISeekableStream and set it
            nsCOMPtr<nsISeekableStream> ras = do_QueryInterface(mSource, &mStatus);
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
        if (mTransferAmount < 0) {
            mTransferAmount = mTotalAmount;
        }
        mTotalAmount = mTransferAmount;

        if (mListener) {
            mStatus = mListener->OnStartRequest(this, mContext);  // always send the start notification
            if (NS_FAILED(mStatus)) {
                mState = END_READ;
                return;
            }
        }
        mState = READING;
        break;
      }

      case READING: {
        PRUint32 writeAmt;
        // and feed the buffer to the application via the buffer stream:
        mStatus = mBufferOutputStream->WriteFrom(mSource, mTransferAmount, &writeAmt);
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: READING [this=%x %s] amt=%d status=%x",
                this, (const char*)mSpec, writeAmt, mStatus));
        if (mStatus == NS_BASE_STREAM_WOULD_BLOCK) {
            mStatus = NS_OK;
            return;
        }
        if (NS_FAILED(mStatus) || writeAmt == 0) {
            PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
                   ("nsFileTransport: READING [this=%x %s] %s writing to buffered output stream",
                    this, (const char*)mSpec, NS_SUCCEEDED(mStatus) ? "done" : "error"));
            mState = END_READ;
            return;
        }
        if (mTransferAmount > 0) {
            mTransferAmount -= writeAmt;
            PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
                   ("nsFileTransport: READING [this=%x %s] %d bytes left to transfer",
                    this, (const char*)mSpec, mTransferAmount));
        }
        PRUint32 offset = mOffset;
        mOffset += writeAmt;
        if (mListener) {
            mStatus = mListener->OnDataAvailable(this, mContext,
                                                 mBufferInputStream,
                                                 offset, writeAmt);
            if (NS_FAILED(mStatus)) {
                PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
                       ("nsFileTransport: READING [this=%x %s] error notifying stream listener",
                        this, (const char*)mSpec));

                mState = END_READ;
                return;
            }
        }

        if (mProgress && mTransferAmount >= 0) {
            nsresult rv = mProgress->OnProgress(this, mContext,
                                    mTotalAmount - mTransferAmount,
                                    mTotalAmount);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unexpected OnProgress failure");
        }

        if (mTransferAmount == 0) {
            mState = END_READ;
            return;
        }

        // stay in the READING state
        break;
      }

      case END_READ: {
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: END_READ [this=%x %s] status=%x",
                this, (const char*)mSpec, mStatus));

#if defined (DEBUG_dougt) || defined (DEBUG_warren)
        NS_ASSERTION(mTransferAmount <= 0 || NS_FAILED(mStatus), "didn't transfer all the data");
#endif 
        mBufferOutputStream->Flush();
        mBufferOutputStream = null_nsCOMPtr();
        mBufferInputStream = null_nsCOMPtr();

        mSource = null_nsCOMPtr();

        if (mListener) {
            // XXX where do we get the done message?
            nsresult rv = mListener->OnStopRequest(this, mContext, mStatus, nsnull);
            NS_ASSERTION(NS_SUCCEEDED(rv), "unexpected OnStopRequest failure");
            mListener = null_nsCOMPtr();
        }
        if (mProgress) {
            // XXX fix up this message for i18n
            nsAutoString msg = "Read ";
#ifdef PR_LOGGING
            msg += (const char*)mSpec;
#endif
            (void)mProgress->OnStatus(this, mContext, msg.GetUnicode());
        }
        mContext = null_nsCOMPtr();

        mState = NS_FAILED(mStatus) ? CLOSING : OPENED;      // stay in the opened state for the next read/write request
        mCommand = NONE;
        break;
      }

      case START_WRITE: {
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG,
               ("nsFileTransport: START_WRITE [this=%x %s]",
                this, (const char*)mSpec));

        mStatus = mFileObject->GetOutputStream(getter_AddRefs(mSink));
        if (NS_FAILED(mStatus)) {
            mState = END_WRITE;
            return;
        }

        if (mOffset > 0) {
            // if we need to set a starting offset, QI for the nsISeekableStream and set it
            nsCOMPtr<nsISeekableStream> ras = do_QueryInterface(mSink, &mStatus);
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
            mBuffer = new char[mBufferSegmentSize];
            if (mBuffer == nsnull) {
                mStatus = NS_ERROR_OUT_OF_MEMORY;
                mState = END_WRITE;
                return;
            }
        }

        if (mObserver) {
            mStatus = mObserver->OnStartRequest(this, mContext);  // always send the start notification
            if (NS_FAILED(mStatus)) {
                mState = END_WRITE;
                return;
            }
        }

        mState = WRITING;
        break;
      }

      case WRITING: {
        PRUint32 transferAmt = mBufferSegmentSize;
        if (mTransferAmount >= 0)
            transferAmt = PR_MIN(mBufferSegmentSize, (PRUint32)mTransferAmount);
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

        mTransferAmount -= writeAmt;
        mOffset += writeAmt;
        if (mProgress) {
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
                this, (const char*)mSpec, mStatus));

        if (mSink) {
            mSink->Flush();
            mSink = null_nsCOMPtr();
        }
        if (mBufferInputStream) {
            mBufferInputStream = null_nsCOMPtr();
        }
        else if (mBuffer) {
            delete mBuffer;
            mBuffer = nsnull;
        }
        if (mSource) {
            (void)mSource->Close();
            mSource = null_nsCOMPtr();
        }

        mState = OPENED;

        if (mObserver) {
            // XXX where do we get the done message?
            (void)mObserver->OnStopRequest(this, mContext, mStatus, nsnull);
            mObserver = null_nsCOMPtr();
        }
        if (mProgress) {
            // XXX fix up this message for i18n
            nsAutoString msg = "Wrote ";
#ifdef PR_LOGGING
            msg += (const char*)mSpec;
#endif
            (void)mProgress->OnStatus(this, mContext, msg.GetUnicode());
        }
        mContext = null_nsCOMPtr();

        mState = NS_FAILED(mStatus) ? CLOSING : OPENED;      // stay in the opened state for the next read/write request
        mCommand = NONE;
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
            this, (const char*)mSpec, mStatus));

    if (mOpenObserver) {
        (void)mOpenObserver->OnStopRequest(this, mOpenContext, 
                                           mStatus, nsnull);  // XXX fix error message
        mOpenObserver = null_nsCOMPtr();
        mOpenContext = null_nsCOMPtr();
    }
    if (mFileObject) {
        nsresult rv = mFileObject->Close(mStatus);
        NS_ASSERTION(NS_SUCCEEDED(rv), "unexpected Close failure");
        mFileObject = null_nsCOMPtr();
    }
    mState = CLOSED;
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

NS_IMETHODIMP
nsFileTransport::OnClose(nsIPipe* pipe)
{
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// other nsIChannel methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileTransport::GetOriginalURI(nsIURI * *aURI)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

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

NS_IMETHODIMP
nsFileTransport::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
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
        
        rv = proxyMgr->GetProxyObject(NS_UI_THREAD_EVENTQ, // primordial thread - should change?
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
