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

#include "nsFileChannel.h"
#include "nscore.h"
#include "nsIEventSinkGetter.h"
#include "nsIURI.h"
#include "nsIEventQueue.h"
#include "nsIStreamListener.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsFileProtocolHandler.h"
#include "nsIBufferInputStream.h"
#include "nsIBufferOutputStream.h"
#include "nsAutoLock.h"
#include "netCore.h"
#include "nsFileStream.h"
#include "nsIFileStream.h"
#include "nsISimpleEnumerator.h"
#include "nsIURL.h"
#include "prio.h"
#include "prmem.h" // XXX can be removed when we start doing real content-type discovery
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsSpecialSystemDirectory.h"
#include "nsDirectoryIndexStream.h"
#include "nsEscape.h"
#include "nsIMIMEService.h"
#include "prlog.h"
#include "nsIPrincipal.h"

static NS_DEFINE_CID(kMIMEServiceCID, NS_MIMESERVICE_CID);

static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);
NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

#ifdef STREAM_CONVERTER_HACK
#include "nsIAllocator.h"
static NS_DEFINE_CID(kStreamConverterCID,    NS_STREAM_CONVERTER_CID);
#endif

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

nsFileChannel::nsFileChannel()
    : mURI(nsnull), mGetter(nsnull), mListener(nsnull), mEventQueue(nsnull),
      mContext(nsnull), mState(QUIESCENT),
      mSuspended(PR_FALSE), mFileStream(nsnull),
      mBufferInputStream(nsnull), mBufferOutputStream(nsnull),
      mStatus(NS_OK), mHandler(nsnull), mSourceOffset(0),
      mLoadAttributes(LOAD_NORMAL),
      mReadFixedAmount(PR_FALSE), mLoadGroup(nsnull), mRealListener(nsnull),
      mPrincipal(nsnull)
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
nsFileChannel::Init(nsFileProtocolHandler* handler,
                    const char* verb, nsIURI* uri, nsILoadGroup *aGroup,
                    nsIEventSinkGetter* getter)
{
    nsresult rv;

    mHandler = handler;
    NS_ADDREF(mHandler);

    mGetter = getter;
    NS_IF_ADDREF(mGetter);

    mMonitor = PR_NewMonitor();
    if (mMonitor == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    if (getter) {
        (void)getter->GetEventSink(verb, nsCOMTypeInfo<nsIStreamListener>::GetIID(), (nsISupports**)&mListener);
        // ignore the failure -- we can live without having an event sink
    }

    mURI = uri;
    NS_ADDREF(mURI);

    mLoadGroup = aGroup;
    NS_IF_ADDREF(mLoadGroup);
    if (mLoadGroup) {
      mLoadGroup->GetDefaultLoadAttributes(&mLoadAttributes);
    }

    // if we support the nsIURL interface then use it to get just
	// the file path with no other garbage!
	nsCOMPtr<nsIURL> aUrl = do_QueryInterface(mURI, &rv);
	if (NS_SUCCEEDED(rv) && aUrl) // does it support the url interface?
	{
		nsXPIDLCString fileString;
		aUrl->DirFile(getter_Copies(fileString));
		// to be mac friendly you need to convert a file path to a nsFilePath before
		// passing it to a nsFileSpec...
#ifdef XP_MAC
		nsFilePath filePath(nsUnescape((char*)(const char*)fileString));
		mSpec = filePath;
		
		// Don't assume we actually created a good file spec
		FSSpec theSpec = mSpec.GetFSSpec();
		if (!theSpec.name[0])
		{
			NS_ERROR("failed to create a file spec");

			// Since we didn't actually create the file spec 
			// we return an error
			return NS_ERROR_MALFORMED_URI;
		}
#else
		nsFilePath filePath(nsUnescape((char*)(const char*)fileString));
		mSpec = filePath;
#endif
	}
	else
	{
		// otherwise do the best we can by using the spec for the uri....
		// XXX temporary, until we integrate more thoroughly with nsFileSpec
		char* url;
		rv = mURI->GetSpec(&url);
		if (NS_FAILED(rv)) return rv;
		nsFileURL fileURL(url);
		nsCRT::free(url);
		mSpec = fileURL;
	}

    return NS_OK;
}

nsFileChannel::~nsFileChannel()
{
    NS_IF_RELEASE(mURI);
    NS_IF_RELEASE(mGetter);
    NS_IF_RELEASE(mListener);
    NS_IF_RELEASE(mEventQueue);
    NS_IF_RELEASE(mContext);
    NS_IF_RELEASE(mHandler);
    NS_ASSERTION(mFileStream == nsnull, "channel not closed");
    NS_ASSERTION(mBufferInputStream == nsnull, "channel not closed");
    NS_ASSERTION(mBufferOutputStream == nsnull, "channel not closed");
    if (mMonitor) 
        PR_DestroyMonitor(mMonitor);
    NS_IF_RELEASE(mLoadGroup);
    NS_IF_RELEASE(mPrincipal);
}

NS_IMETHODIMP
nsFileChannel::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "no instance pointer");
    if (aIID.Equals(nsCOMTypeInfo<nsIFileChannel>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsIChannel>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsIFileChannel*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE; 
}

NS_IMPL_ADDREF(nsFileChannel);
NS_IMPL_RELEASE(nsFileChannel);

NS_METHOD
nsFileChannel::Create(nsISupports* aOuter, const nsIID& aIID, void* *aResult)
{
    nsFileChannel* fc = new nsFileChannel();
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
nsFileChannel::IsPending(PRBool *result)
{
    *result = mState != QUIESCENT;
    return NS_OK; 
}

NS_IMETHODIMP
nsFileChannel::Cancel()
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv = NS_OK;
    mStatus = NS_BINDING_ABORTED;
    if (mSuspended) {
        Resume();
    }
    mState = ENDING;
    PR_LOG(gFileTransportLog, PR_LOG_DEBUG, 
           ("nsFileTransport: Cancel [this=%x %s]", 
            this, (const char*)mSpec));
    return rv;
}

NS_IMETHODIMP
nsFileChannel::Suspend()
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv = NS_OK;
    if (!mSuspended) {
        // XXX close the stream here?
        mStatus = mHandler->Suspend(this);
        mSuspended = PR_TRUE;
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG, 
               ("nsFileTransport: Suspend [this=%x %s]", 
                this, (const char*)mSpec));
    }
    return rv;
}

NS_IMETHODIMP
nsFileChannel::Resume()
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv = NS_OK;
    if (mSuspended) {
        // XXX re-open the stream and seek here?
        mSuspended = PR_FALSE;  // set this first before resuming!
        mStatus = mHandler->Resume(this);
        PR_LOG(gFileTransportLog, PR_LOG_DEBUG, 
               ("nsFileTransport: Resume [this=%x %s] status=%x", 
                this, (const char*)mSpec, mStatus));
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIChannel
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::GetURI(nsIURI * *aURI)
{
    *aURI = mURI;
    NS_ADDREF(mURI);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::OpenInputStream(PRUint32 startPosition, PRInt32 readCount,
                               nsIInputStream **result)
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv;

    if (mState != QUIESCENT)
        return NS_ERROR_IN_PROGRESS;

    PRBool exists;
    rv = Exists(&exists);
    if (NS_FAILED(rv)) return rv;
    if (!exists)
        return NS_ERROR_FAILURE;        // XXX probably need NS_BASE_STREAM_FILE_NOT_FOUND or something

    rv = NS_NewPipe(&mBufferInputStream, &mBufferOutputStream,
                    this,     // nsIPipeObserver
                    NS_FILE_TRANSPORT_SEGMENT_SIZE,
                    NS_FILE_TRANSPORT_BUFFER_SIZE);
    if (NS_FAILED(rv)) return rv;
#if 0
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = serv->NewSyncStreamListener(&mBufferInputStream, &mBufferOutputStream, &mListener);
    if (NS_FAILED(rv)) return rv;
#endif

    rv = mBufferOutputStream->SetNonBlocking(PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    mState = START_READ;
    mSourceOffset = startPosition;
    mAmount = readCount;
    mListener = nsnull;

    rv = mHandler->DispatchRequest(this);
    if (NS_FAILED(rv)) return rv;

    *result = mBufferInputStream;
    NS_ADDREF(*result);
    PR_LOG(gFileTransportLog, PR_LOG_DEBUG, 
           ("nsFileTransport: OpenInputStream [this=%x %s]", 
            this, (const char*)mSpec));
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **result)
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv;

    if (mState != QUIESCENT)
        return NS_ERROR_IN_PROGRESS;

    NS_ASSERTION(startPosition == 0, "implement startPosition");
    nsISupports* str;
    rv = NS_NewTypicalOutputFileStream(&str, mSpec);
    if (NS_FAILED(rv)) return rv;
    rv = str->QueryInterface(nsCOMTypeInfo<nsIOutputStream>::GetIID(), (void**)result);
    NS_RELEASE(str);
    PR_LOG(gFileTransportLog, PR_LOG_DEBUG, 
           ("nsFileTransport: OpenOutputStream [this=%x %s]", 
            this, (const char*)mSpec));
    return rv;
}

NS_IMETHODIMP
nsFileChannel::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                         nsISupports *ctxt,
                         nsIStreamListener *listener)
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv = NS_OK;

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    if (!mEventQueue) {
        NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
        if (NS_FAILED(rv)) return rv;
        rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), &mEventQueue);
        if (NS_FAILED(rv)) return rv;
    }

	// mscott --  this is just one temporary hack until we have a legit stream converter
	// story going....if the file we are opening is an rfc822 file then we want to 
	// go out and convert the data into html before we try to load it. so I'm inserting
	// code which if we are rfc-822 will cause us to literally insert a converter between
	// the file channel stream of incoming data and the consumer at the other end of the
	// AsyncRead call...

#ifdef STREAM_CONVERTER_HACK
	nsXPIDLCString aContentType;


    mRealListener = listener;

	rv = GetContentType(getter_Copies(aContentType));
	if (NS_SUCCEEDED(rv) && PL_strcasecmp("message/rfc822", aContentType) == 0)
	{
		// okay we are an rfc822 message...
		// (0) Create an instance of an RFC-822 stream converter...
		// because I need this converter to be around for the lifetime of the channel,
		// I'm making it a member variable.
		// (1) create a proxied stream listener for the caller of this method
		// (2) set this proxied listener as the listener on the output stream
		// (3) create a proxied stream listener for the converter
		// (4) set mListener to be the stream converter's listener.

		// (0) create a stream converter
		nsCOMPtr<nsIStreamConverter2> mimeParser;
		// mscott - we could generalize this hack to work with other stream converters by simply
		// using the content type of the file to generate a progid for a stream converter and use
		// that instead of a class id...
		if (!mStreamConverter)
			rv = nsComponentManager::CreateInstance(kStreamConverterCID, 
													NULL, nsCOMTypeInfo<nsIStreamConverter2>::GetIID(), 
													(void **) getter_AddRefs(mStreamConverter)); 
		if (NS_FAILED(rv)) return rv;

		// (1) and (2)
		nsCOMPtr<nsIStreamListener> proxiedConsumerListener;
		rv = serv->NewAsyncStreamListener(this, mEventQueue, getter_AddRefs(proxiedConsumerListener));
		if (NS_FAILED(rv)) return rv;

		// (3) set the stream converter as the listener on the channel
		mListener = mStreamConverter;
		NS_IF_ADDREF(mListener); // mListener is NOT a com ptr...

		// now set the output stream correctly
		mStreamConverter->Init(mURI, proxiedConsumerListener, this);
		mStreamConverter->GetContentType(getter_Copies(mStreamConverterOutType));
	}
	else
		rv = serv->NewAsyncStreamListener(this, mEventQueue, &mListener);
#else   
	rv = serv->NewAsyncStreamListener(this, mEventQueue, &mListener);
#endif
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewPipe(&mBufferInputStream, &mBufferOutputStream,
                    this,       // nsIPipeObserver
                    NS_FILE_TRANSPORT_SEGMENT_SIZE,
                    NS_FILE_TRANSPORT_BUFFER_SIZE);
    if (NS_FAILED(rv)) return rv;

    rv = mBufferOutputStream->SetNonBlocking(PR_TRUE);
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(mContext == nsnull, "context not released");
    mContext = ctxt;
    NS_IF_ADDREF(mContext);

    mState = START_READ;
    mSourceOffset = startPosition;

	// did the user request a specific number of bytes to read?
	// if they passed in -1 then they want all bytes to be read.f
	if (readCount > 0) // did the user pass in
	{
		mReadFixedAmount = PR_TRUE;
		mAmount = (PRUint32) readCount; // mscott - this is a safe cast!
	}
	else
		mAmount = 0; // don't worry we'll ignore this parameter from here on out because mReadFixedAmount is false

    if (mLoadGroup) {
        nsCOMPtr<nsILoadGroupListenerFactory> factory;
        //
        // Create a load group "proxy" listener...
        //
        rv = mLoadGroup->GetGroupListenerFactory(getter_AddRefs(factory));
        if (factory) {
          nsIStreamListener *newListener;
          rv = factory->CreateLoadGroupListener(mRealListener, &newListener);
          if (NS_SUCCEEDED(rv)) {
            mRealListener = newListener;
            NS_RELEASE(newListener);
          }
        }

        rv = mLoadGroup->AddChannel(this, nsnull);
        if (NS_FAILED(rv)) return rv;
    }

    PR_LOG(gFileTransportLog, PR_LOG_DEBUG, 
           ("nsFileTransport: AsyncRead [this=%x %s]", 
            this, (const char*)mSpec));
    rv = mHandler->DispatchRequest(this);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::AsyncWrite(nsIInputStream *fromStream,
                          PRUint32 startPosition, PRInt32 writeCount,
                          nsISupports *ctxt,
                          nsIStreamObserver *observer)
{
    nsAutoMonitor mon(mMonitor);

    PR_LOG(gFileTransportLog, PR_LOG_DEBUG, 
           ("nsFileTransport: AsyncWrite [this=%x %s]", 
            this, (const char*)mSpec));
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::GetLoadAttributes(PRUint32 *aLoadAttributes)
{
    *aLoadAttributes = mLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetLoadAttributes(PRUint32 aLoadAttributes)
{
    mLoadAttributes = aLoadAttributes;
    return NS_OK;
}

#define DUMMY_TYPE "text/html"

NS_IMETHODIMP
nsFileChannel::GetContentType(char * *aContentType)
{
    nsresult rv = NS_OK;
#ifdef STREAM_CONVERTER_HACK
	// okay, if we already have a stream converter hooked up to the channel
	// then we want to LIE about the content type...the content type is really
	// the stream converter out type...
	if (mStreamConverter) 
	{
		*aContentType = (char *) nsAllocator::Clone(mStreamConverterOutType, nsCRT::strlen(mStreamConverterOutType) + 1);
		return rv;
	}
#endif

    if (mSpec.IsDirectory()) {
        *aContentType = nsCRT::strdup("application/http-index-format");
        return *aContentType ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }
    else {
        NS_WITH_SERVICE(nsIMIMEService, MIMEService, kMIMEServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        rv = MIMEService->GetTypeFromURI(mURI, aContentType);
        if (NS_SUCCEEDED(rv)) return rv;
    }

    // if all else fails treat it as text/html?
    *aContentType = nsCRT::strdup(DUMMY_TYPE);
    if (!*aContentType) {
        return NS_ERROR_OUT_OF_MEMORY;
    } else {
        return NS_OK;
    }
}

NS_IMETHODIMP
nsFileChannel::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetPrincipal(nsIPrincipal * *aPrincipal)
{
    *aPrincipal = mPrincipal;
    NS_IF_ADDREF(*aPrincipal);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetPrincipal(nsIPrincipal * aPrincipal)
{
    NS_IF_RELEASE(mPrincipal);
    mPrincipal = aPrincipal;
    NS_IF_ADDREF(mPrincipal);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIRunnable methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::Run(void)
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
nsFileChannel::Process(void)
{
    nsAutoMonitor mon(mMonitor);

    switch (mState) {
      case START_READ: {
          nsISupports* fs;

          if (mListener) {
              mStatus = mListener->OnStartRequest(this, mContext);  // always send the start notification
              if (NS_FAILED(mStatus)) goto error;
          }

          if (mSpec.IsDirectory()) {
              mStatus = nsDirectoryIndexStream::Create(mSpec, &fs);
          }
          else {
              mStatus = NS_NewTypicalInputFileStream(&fs, mSpec);
          }
          if (NS_FAILED(mStatus)) goto error;

		  if (mSourceOffset > 0) // if we need to set a starting offset, QI for the nsIRandomAccessStore and set it
		  {
			nsCOMPtr<nsIRandomAccessStore> inputStream;
			inputStream = do_QueryInterface(fs, &mStatus);
			if (NS_FAILED(mStatus)) goto error;
			// for now, assume the offset is always relative to the start of the file (position 0)
			// so use PR_SEEK_SET
			inputStream->Seek(PR_SEEK_SET, mSourceOffset);
		  }

		  
          mStatus = fs->QueryInterface(nsCOMTypeInfo<nsIInputStream>::GetIID(), (void**)&mFileStream);
          NS_RELEASE(fs);
          if (NS_FAILED(mStatus)) goto error;

          PR_LOG(gFileTransportLog, PR_LOG_DEBUG, 
                 ("nsFileTransport: START_READ [this=%x %s]", 
                  this, (const char*)mSpec));
          mState = READING;
          break;
      }
      case READING: {
          if (NS_FAILED(mStatus)) goto error;

          nsIInputStream* fileStr = NS_STATIC_CAST(nsIInputStream*, mFileStream);

          PRUint32 inLen;
          mStatus = fileStr->GetLength(&inLen);
          if (NS_FAILED(mStatus)) goto error;

		  // mscott --> if the user wanted to only read a fixed number of bytes
		  // we need to honor that...
		  if (mReadFixedAmount) 
			  inLen = inLen < mAmount ? inLen : mAmount; // take the min(inLen, mAmount)

          PRUint32 amt;
          mStatus = mBufferOutputStream->WriteFrom(fileStr, inLen, &amt);
          PR_LOG(gFileTransportLog, PR_LOG_DEBUG, 
                 ("nsFileTransport: READING [this=%x %s] amt=%d status=%x", 
                  this, (const char*)mSpec, amt, mStatus));
          if (mStatus == NS_BASE_STREAM_WOULD_BLOCK) {
              mStatus = NS_OK;
              return;
          }
          if (NS_FAILED(mStatus)) goto error;
          if (mReadFixedAmount)
              mAmount -= amt;   // subtract off the amount we just read from mAmount.

          // and feed the buffer to the application via the buffer stream:
          if (mListener) {
              mStatus = mListener->OnDataAvailable(this, mContext, mBufferInputStream, mSourceOffset, amt);
              if (NS_FAILED(mStatus)) goto error;
          }
          
		  if (mReadFixedAmount && mAmount == 0)
		  {
			  Cancel(); // stop reading data...we are done
			  return;
		  }

          mSourceOffset += amt;

          // stay in the READING state
          break;
      }
      case START_WRITE: {
          nsISupports* fs;

          if (mListener) {
              mStatus = mListener->OnStartRequest(this, mContext);  // always send the start notification
              if (NS_FAILED(mStatus)) goto error;
          }

          mStatus = NS_NewTypicalOutputFileStream(&fs, mSpec);
          if (NS_FAILED(mStatus)) goto error;

          mStatus = fs->QueryInterface(nsCOMTypeInfo<nsIOutputStream>::GetIID(), (void**)&mFileStream);
          NS_RELEASE(fs);
          if (NS_FAILED(mStatus)) goto error;

          PR_LOG(gFileTransportLog, PR_LOG_DEBUG, 
                 ("nsFileTransport: START_WRITE [this=%x %s]", 
                  this, (const char*)mSpec));
          mState = WRITING;
          break;
      }
      case WRITING: {
          if (NS_FAILED(mStatus)) goto error;
#if 0
          PRUint32 amt;
          mStatus = mBuffer->ReadSegments(nsWriteToFile, mFileStream, (PRUint32)-1, &amt);
          if (mStatus == NS_BASE_STREAM_EOF) goto error; 
          if (NS_FAILED(mStatus)) goto error;

          nsAutoCMonitor mon(mBuffer);
          mon.Notify();

          mSourceOffset += amt;
#endif
          PR_LOG(gFileTransportLog, PR_LOG_DEBUG, 
                 ("nsFileTransport: WRITING [this=%x %s]", 
                  this, (const char*)mSpec));
          // stay in the WRITING state
          break;
      }
      case ENDING: {
          PR_LOG(gFileTransportLog, PR_LOG_DEBUG, 
                 ("nsFileTransport: ENDING [this=%x %s] status=%x", 
                  this, (const char*)mSpec, mStatus));
          mBufferOutputStream->Flush();
          if (mListener) {
              // XXX where do we get the error message?
              (void)mListener->OnStopRequest(this, mContext, mStatus, nsnull);
              NS_RELEASE(mListener);
          }

          NS_IF_RELEASE(mBufferOutputStream);
          mBufferOutputStream = nsnull;
          NS_IF_RELEASE(mBufferInputStream);
          mBufferInputStream = nsnull;
          NS_IF_RELEASE(mFileStream);
          mFileStream = nsnull;
          NS_IF_RELEASE(mContext);
          mContext = nsnull;

          mState = QUIESCENT;
          break;
      }
      case QUIESCENT: {
          NS_NOTREACHED("trying to continue a quiescent file transfer");
          break;
      }
    }
    return;

  error:
    // Map EOF to NS_OK for the OnStopBinding(...) notification...
    if (NS_BASE_STREAM_EOF == mStatus) {
      mStatus = NS_OK;
    }
    mState = ENDING;
    return;
}

////////////////////////////////////////////////////////////////////////////////
// nsIPipeObserver methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::OnFull(nsIPipe* pipe)
{
    return Suspend();
}

NS_IMETHODIMP
nsFileChannel::OnWrite(nsIPipe* pipe, PRUint32 aCount)
{
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::OnEmpty(nsIPipe* pipe)
{
    return Resume();
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::OnDataAvailable(nsIChannel* channel, nsISupports* context,
                               nsIInputStream *aIStream, 
                               PRUint32 aSourceOffset,
                               PRUint32 aLength)
{
    return mRealListener->OnDataAvailable(channel, context, aIStream, 
                                          aSourceOffset, aLength);
}

NS_IMETHODIMP
nsFileChannel::OnStartRequest(nsIChannel* channel, nsISupports* context)
{
    NS_ASSERTION(mRealListener, "No listener...");
    return mRealListener->OnStartRequest(channel, context);
}

NS_IMETHODIMP
nsFileChannel::OnStopRequest(nsIChannel* channel, nsISupports* context,
                             nsresult aStatus,
                             const PRUnichar* aMsg)
{
    nsresult rv;

    rv = mRealListener->OnStopRequest(channel, context, aStatus, aMsg);
 
    if (mLoadGroup) {
        mLoadGroup->RemoveChannel(channel, context, aStatus, aMsg);
    }

    // Release the reference to the consumer stream listener...
    mRealListener = null_nsCOMPtr();
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIFileChannel
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::GetModDate(PRTime *aModDate)
{
    nsFileSpec::TimeStamp date;
    mSpec.GetModDate(date);
    LL_I2L(*aModDate, date);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetFileSize(PRUint32 *aFileSize)
{
    *aFileSize = mSpec.GetFileSize();
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetParent(nsIFileChannel * *aParent)
{
    nsFileSpec parentSpec;
    mSpec.GetParent(parentSpec);
    return CreateFileChannelFromFileSpec(parentSpec, aParent);
}

class nsDirEnumerator : public nsISimpleEnumerator
{
public: 
    NS_DECL_ISUPPORTS
 
    nsDirEnumerator() : mHandler(nsnull), mDir(nsnull), mNext(nsnull) {
        NS_INIT_REFCNT();
    }

    nsresult Init(nsFileProtocolHandler* handler, nsFileSpec& spec) {
        const char* path = spec.GetNativePathCString();
        mDir = PR_OpenDir(path);
        if (mDir == nsnull)    // not a directory?
            return NS_ERROR_FAILURE;

        mHandler = handler;
        NS_ADDREF(mHandler);
        return NS_OK;
    }

    NS_IMETHOD HasMoreElements(PRBool *result) {
        nsresult rv;
        if (mNext == nsnull && mDir) {
            PRDirEntry* entry = PR_ReadDir(mDir, PR_SKIP_BOTH);
            if (entry == nsnull) {
                // end of dir entries

                PRStatus status = PR_CloseDir(mDir);
                if (status != PR_SUCCESS) 
                    return NS_ERROR_FAILURE;
                mDir = nsnull;

                *result = PR_FALSE;
                return NS_OK;
            }

            const char* path = entry->name;
            rv = mHandler->NewChannelFromNativePath(path, &mNext);
            if (NS_FAILED(rv)) return rv;

            NS_ASSERTION(mNext, "NewChannel failed");
        }            
        *result = mNext != nsnull;
        return NS_OK;
    }

    NS_IMETHOD GetNext(nsISupports **result) {
        nsresult rv;
        PRBool hasMore;
        rv = HasMoreElements(&hasMore);
        if (NS_FAILED(rv)) return rv;

        *result = mNext;        // might return nsnull
        mNext = nsnull;
        return NS_OK;
    }

    virtual ~nsDirEnumerator() {
        if (mDir) {
            PRStatus status = PR_CloseDir(mDir);
            NS_ASSERTION(status == PR_SUCCESS, "close failed");
        }
        NS_IF_RELEASE(mHandler);
        NS_IF_RELEASE(mNext);
    }

protected:
    nsFileProtocolHandler*      mHandler;
    PRDir*                      mDir;
    nsIFileChannel*             mNext;
};

NS_IMPL_ISUPPORTS(nsDirEnumerator, nsCOMTypeInfo<nsISimpleEnumerator>::GetIID());

NS_IMETHODIMP
nsFileChannel::GetChildren(nsISimpleEnumerator * *aChildren)
{
    nsresult rv;

    PRBool isDir;    
    rv = IsDirectory(&isDir);
    if (NS_FAILED(rv)) return rv;
    if (!isDir)
        return NS_ERROR_FAILURE;

    nsDirEnumerator* dirEnum = new nsDirEnumerator();
    if (dirEnum == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(dirEnum);
    rv = dirEnum->Init(mHandler, mSpec);
    if (NS_FAILED(rv)) {
        NS_RELEASE(dirEnum);
        return rv;
    }
    *aChildren = dirEnum;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetNativePath(char * *aNativePath)
{
    char* nativePath = nsCRT::strdup(mSpec.GetNativePathCString());
    if (nativePath == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    *aNativePath = nativePath;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::Exists(PRBool *result)
{
    *result = mSpec.Exists();
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::Create()
{
    nsFileSpec mySpec(mSpec); // relative path.
    {
        nsIOFileStream testStream(mySpec); // creates the file
        // Scope ends here, file gets closed
    }
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::Delete()
{
    mSpec.Delete(PR_TRUE); // RECURSIVE DELETE!
    if (mSpec.Exists())
        return NS_ERROR_FAILURE;
    
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::MoveFrom(nsIURI *src)
{
#if 0
    nsresult rv;
    nsIFileChannel* fc;
    rv = src->QueryInterface(nsCOMTypeInfo<nsIFileChannel>::GetIID(), (void**)&fc);
    if (NS_SUCCEEDED(rv)) {
        rv = fc->moveToDir(this);
        NS_RELEASE(fc);
        return rv;
    }
    else {
        // Do it the hard way -- fetch the URL and store the bits locally.
        // Delete the src when done.
        return NS_ERROR_NOT_IMPLEMENTED;
    }
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsFileChannel::CopyFrom(nsIURI *src)
{
#if 0
    nsresult rv;
    nsIFileChannel* fc;
    rv = src->QueryInterface(nsCOMTypeInfo<nsIFileChannel>::GetIID(), (void**)&fc);
    if (NS_SUCCEEDED(rv)) {
        rv = fc->copyToDir(this);
        NS_RELEASE(fc);
        return rv;
    }
    else {
        // Do it the hard way -- fetch the URL and store the bits locally.
        return NS_ERROR_NOT_IMPLEMENTED;
    }
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsFileChannel::IsDirectory(PRBool *result)
{
    *result = mSpec.IsDirectory();
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::IsFile(PRBool *result)
{
    *result = mSpec.IsFile();
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::IsLink(PRBool *_retval)
{
    *_retval = mSpec.IsSymlink();
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::ResolveLink(nsIFileChannel **_retval)
{
    PRBool ignore;
    nsFileSpec tempSpec = mSpec;
    nsresult rv = tempSpec.ResolveSymlink(ignore);

    if(NS_SUCCEEDED(rv))
    {
        return CreateFileChannelFromFileSpec(tempSpec, _retval);
    }
    
    return rv;
}

NS_IMETHODIMP
nsFileChannel::MakeUnique(const char* baseName, nsIFileChannel **_retval)
{
    if (mSpec.IsDirectory())
    {
        nsFileSpec tempSpec = mSpec;
        tempSpec.MakeUnique(baseName);

        return CreateFileChannelFromFileSpec(tempSpec, _retval);
    }
    return NS_ERROR_FAILURE;        // XXX probably need NS_BASE_STREAM_NOT_DIRECTORY or something
}
    

NS_IMETHODIMP
nsFileChannel::Execute(const char *args)
{
    nsresult rv;
    char* queryArgs = nsnull;
    
    if (args == nsnull) {
        nsIURL* url;
        rv = mURI->QueryInterface(nsCOMTypeInfo<nsIURL>::GetIID(), (void**)&url);
        if (NS_SUCCEEDED(rv)) {
            rv = url->GetQuery(&queryArgs);
            NS_RELEASE(url);
            if (NS_FAILED(rv)) return rv;
            args = queryArgs;
        }
    }
    
    rv = mSpec.Execute(args);
    if (queryArgs)
        nsCRT::free(queryArgs);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

nsresult
nsFileChannel::CreateFileChannelFromFileSpec(nsFileSpec& spec, nsIFileChannel **result)
{
    nsresult rv;

    nsFileURL aURL(spec);
    const char* urlStr = aURL.GetURLString();

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsIChannel* channel;
    rv = serv->NewChannel("load",    // XXX what should this be?
                          urlStr, 
                          nsnull,
                          mLoadGroup,
                          mGetter, 
                          &channel);

    if (NS_FAILED(rv)) return rv;

    // this cast is safe because nsFileURL::GetURLString aways
    // returns file: strings, and consequently we'll make nsIFileChannel
    // objects from them:
    *result = NS_STATIC_CAST(nsIFileChannel*, channel);
    return NS_OK;
}

