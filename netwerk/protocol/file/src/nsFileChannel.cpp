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
#include "nsIFileStream.h"
#include "nsISimpleEnumerator.h"
#include "nsIURL.h"
#include "prio.h"
#include "prmem.h" // XXX can be removed when we start doing real content-type discovery
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsSpecialSystemDirectory.h"
#include "nsEscape.h"
#include "nsIMIMEService.h"

static NS_DEFINE_CID(kMIMEServiceCID, NS_MIMESERVICE_CID);

static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);
NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

#ifdef STREAM_CONVERTER_HACK
#include "nsIAllocator.h"
static NS_DEFINE_CID(kStreamConverterCID,    NS_STREAM_CONVERTER_CID);
#endif

////////////////////////////////////////////////////////////////////////////////

nsFileChannel::nsFileChannel()
    : mURI(nsnull), mGetter(nsnull), mListener(nsnull), mEventQueue(nsnull),
      mContext(nsnull), mState(QUIESCENT),
      mSuspended(PR_FALSE), mFileStream(nsnull),
      mBufferInputStream(nsnull), mBufferOutputStream(nsnull),
      mStatus(NS_OK), mHandler(nsnull), mSourceOffset(0),
      mLoadAttributes(LOAD_NORMAL),
	  mReadFixedAmount(PR_FALSE)
{
    NS_INIT_REFCNT();
}

nsresult
nsFileChannel::Init(nsFileProtocolHandler* handler,
                    const char* verb, nsIURI* uri, nsIEventSinkGetter* getter,
                    nsIEventQueue* queue)
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
        rv = getter->GetEventSink(verb, nsCOMTypeInfo<nsIStreamListener>::GetIID(), (nsISupports**)&mListener);
        // ignore the failure -- we can live without having an event sink
    }

    mURI = uri;
    NS_ADDREF(mURI);

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

    mEventQueue = queue;
    NS_IF_ADDREF(mEventQueue);

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
    }
    return rv;
}

NS_IMETHODIMP
nsFileChannel::Resume()
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv = NS_OK;
    if (!mSuspended) {
        // XXX re-open the stream and seek here?
        mStatus = mHandler->Resume(this);
        mSuspended = PR_FALSE;
    }
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
#if 0
class nsAsyncOutputStream : public nsIBufferOutputStream {
public:
    NS_DECL_ISUPPORTS

    // nsIBaseStream methods:

    NS_IMETHOD Close() {
        return mOutputStream->Close();
    }

    // nsIOutputStream methods:

    NS_IMETHOD Write(const char *buf, PRUint32 count, PRUint32 *writeCount) {
        nsresult rv;
        rv = mOutputStream->Write(buf, count, writeCount);
        if (NS_FAILED(rv)) return rv;
        rv = mListener->OnDataAvailable(mChannel, mContext, mInputStream, mOffset, *writeCount);
        mOffset += *writeCount;
        return rv;
    }

    NS_IMETHOD Flush() {
        nsresult rv;
        rv = mOutputStream->Flush();
        if (NS_FAILED(rv)) return rv;
        rv = mListener->OnStopRequest(mChannel, mContext, /*mStatus*/rv, nsnull);
        return rv;
    }

    // nsIBufferOutputStream methods:

    NS_IMETHOD GetBuffer(nsIBuffer * *aBuffer) {
        return mOutputStream->GetBuffer(aBuffer);
    }

    NS_IMETHOD WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *writeCount) {
        nsresult rv;
        rv = mOutputStream->WriteFrom(inStr, count, writeCount);
        if (NS_FAILED(rv)) return rv;
        rv = mListener->OnDataAvailable(mChannel, mContext, mInputStream, mOffset, *writeCount);
        mOffset += *writeCount;
        return rv;
    }

    NS_IMETHOD GetNonBlocking(PRBool *aNonBlocking) {
        return mOutputStream->GetNonBlocking(aNonBlocking);
    }

    NS_IMETHOD SetNonBlocking(PRBool aNonBlocking) {
        return mOutputStream->SetNonBlocking(aNonBlocking);
    }

    nsAsyncOutputStream()
        : mContext(nsnull), mListener(nsnull), mInputStream(nsnull),
          mOutputStream(nsnull), mOffset(0)
    {
        NS_INIT_REFCNT();
    }

    nsresult Init(nsIChannel* channel, nsISupports* context, nsIStreamListener* listener,
                  PRUint32 growBySize, PRUint32 maxSize) {
        nsresult rv;
        rv = NS_NewPipe(&mInputStream, &mOutputStream,
                        growBySize, maxSize, PR_TRUE, nsnull);
        if (NS_FAILED(rv)) return rv;

        mChannel = channel;
        NS_ADDREF(mChannel);
        mContext = context;
        NS_IF_ADDREF(mContext);
        mListener = listener;
        NS_ADDREF(mListener);
        return rv;
    }

    virtual ~nsAsyncOutputStream() {
        NS_IF_RELEASE(mChannel);
        NS_IF_RELEASE(mContext);
        NS_IF_RELEASE(mListener);
        NS_IF_RELEASE(mInputStream);
        NS_IF_RELEASE(mOutputStream);
    }

    static NS_METHOD Create(nsIBufferInputStream* *inStr,
                            nsIBufferOutputStream* *outStr,
                            nsIChannel* channel, nsISupports* context,
                            nsIStreamListener* listener,
                            PRUint32 growBySize, PRUint32 maxSize) {
        nsAsyncOutputStream* str = new nsAsyncOutputStream();
        if (str == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(str);
        nsresult rv = str->Init(channel, context, listener, growBySize, maxSize);
        if (NS_FAILED(rv)) {
            NS_RELEASE(str);
            return rv;
        }
        *inStr = str->mInputStream;
        NS_ADDREF(*inStr);
        *outStr = str;
        return NS_OK;
    }

protected:
    nsIChannel*                 mChannel;
    nsISupports*                mContext;
    nsIStreamListener*          mListener;
    nsIBufferInputStream*       mInputStream;
    nsIBufferOutputStream*      mOutputStream;
    PRUint32                    mOffset;
};

NS_IMPL_ISUPPORTS(nsAsyncOutputStream, nsCOMTypeInfo<nsIBufferOutputStream>::GetIID());
#endif
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

    rv = NS_NewPipe(&mBufferInputStream, &mBufferOutputStream,
                    NS_FILE_TRANSPORT_SEGMENT_SIZE,
                    NS_FILE_TRANSPORT_BUFFER_SIZE, PR_TRUE, this);
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
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **result)
{
    nsAutoMonitor mon(mMonitor);

    nsresult rv;

    if (mState != QUIESCENT)
        return NS_ERROR_IN_PROGRESS;

#if 0
    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsIStreamListener* syncListener;
    nsIBufferInputStream* inStr;
    nsIBufferOutputStream* outStr;
    rv = serv->NewSyncStreamListener(&inStr, &outStr, &syncListener);
    if (NS_FAILED(rv)) return rv;

    mListener = syncListener;
    mOutputStream = outStr;
    mState = START_READ;
    mSourceOffset = startPosition;
    mAmount = readCount;

    rv = mHandler->DispatchRequest(this);
    if (NS_FAILED(rv)) {
        NS_RELEASE(inStr);
        return rv;
    }

    *result = inStr;
#else
    NS_ASSERTION(startPosition == 0, "implement startPosition");
    nsISupports* str;
    rv = NS_NewTypicalOutputFileStream(&str, mSpec);
    if (NS_FAILED(rv)) return rv;
    rv = str->QueryInterface(nsCOMTypeInfo<nsIOutputStream>::GetIID(), (void**)result);
    NS_RELEASE(str);
    return rv;

#endif
    return NS_OK;
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
		rv = serv->NewAsyncStreamListener(listener, mEventQueue, getter_AddRefs(proxiedConsumerListener));
		if (NS_FAILED(rv)) return rv;

		// (3) set the stream converter as the listener on the channel
		mListener = mStreamConverter;
		NS_IF_ADDREF(mListener); // mListener is NOT a com ptr...

		// now set the output stream correctly
		mStreamConverter->Init(mURI, proxiedConsumerListener, this);
		mStreamConverter->GetContentType(getter_Copies(mStreamConverterOutType));
	}
	else
		rv = serv->NewAsyncStreamListener(listener, mEventQueue, &mListener);
#else   
	rv = serv->NewAsyncStreamListener(listener, mEventQueue, &mListener);
#endif
    if (NS_FAILED(rv)) return rv;

#if 0
    rv = nsAsyncOutputStream::Create(&mBufferInputStream,
                                     &mBufferOutputStream,
                                     this, ctxt, mListener, 
                                     NS_FILE_TRANSPORT_SEGMENT_SIZE,
                                     NS_FILE_TRANSPORT_BUFFER_SIZE);
    if (NS_FAILED(rv)) return rv;
#else
    rv = NS_NewPipe(&mBufferInputStream, &mBufferOutputStream,
                    NS_FILE_TRANSPORT_SEGMENT_SIZE,
                    NS_FILE_TRANSPORT_BUFFER_SIZE, PR_TRUE, this);
    if (NS_FAILED(rv)) return rv;
#endif

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

	// we need to try to get the file name from a nsIURL in order to lose the query and ref stuff
	// that may be tacked on....I left the old code which incorrectly did this under a #if 0 in
	// case we need to add it back in later...
	nsXPIDLCString fileName;
	nsCOMPtr<nsIURL> urlForm = do_QueryInterface(mURI, &rv);
	NS_ASSERTION(urlForm, "bad assumption here....");
	urlForm->GetFileName(getter_Copies(fileName));

#if 0
    char *cStrSpec= nsnull;
    rv = mURI->GetSpec(&cStrSpec);
    if (!cStrSpec)
        return NS_ERROR_OUT_OF_MEMORY;
    // find the file extension
    nsString2 specStr(cStrSpec);
#endif
	nsString2 specStr(fileName);
    nsString2 extStr;
    PRInt32 extLoc = specStr.RFindChar('.');
    if (-1 != extLoc) {
        specStr.Right(extStr, specStr.Length() - extLoc - 1);
        char *ext = extStr.ToNewCString();

        NS_WITH_SERVICE(nsIMIMEService, MIMEService, kMIMEServiceCID, &rv);
        if (NS_FAILED(rv)) return rv;

        nsIMIMEInfo *MIMEInfo = nsnull;
        rv = MIMEService->GetFromExtension(ext, &MIMEInfo);
        nsAllocator::Free(ext);
        if (NS_FAILED(rv)) return rv;

        rv = MIMEInfo->GetMIMEType(aContentType);

        NS_RELEASE(MIMEInfo);
        return rv;
        // we should probably set the content-type for this response at this stage too.
    }

    // if all else fails treat it as text/html?
    *aContentType = nsCRT::strdup(DUMMY_TYPE);
    if (!*aContentType) {
        return NS_ERROR_OUT_OF_MEMORY;
    } else {
        return NS_OK;
    }
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

          mStatus = NS_NewTypicalInputFileStream(&fs, mSpec);
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
		  if (mReadFixedAmount)
			  mAmount -= amt;   // subtract off the amount we just read from mAmount.
          if (NS_FAILED(mStatus)) goto error;
          if (mStatus == NS_BASE_STREAM_WOULD_BLOCK || amt == 0) {
              // Our nsIBufferObserver will have been called from WriteFrom
              // which in turn calls Suspend, so we should end up suspending
              // this file channel.
              Suspend();
              return;
          }

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
          // stay in the WRITING state
          break;
      }
      case ENDING: {
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
// nsIBufferObserver methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::OnFull(nsIBuffer* buffer)
{
    return Suspend();
}

NS_IMETHODIMP
nsFileChannel::OnWrite(nsIBuffer* aBuffer, PRUint32 aCount)
{
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::OnEmpty(nsIBuffer* buffer)
{
    return Resume();
}

////////////////////////////////////////////////////////////////////////////////
// From nsIFileChannel
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::GetCreationDate(PRTime *aCreationDate)
{
    // XXX no GetCreationDate in nsFileSpec yet
    return NS_ERROR_NOT_IMPLEMENTED;
}

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
    nsresult rv;

    nsFileSpec parentSpec;
    mSpec.GetParent(parentSpec);
    nsFileURL parentURL(parentSpec);
    const char* urlStr = parentURL.GetURLString();

    NS_WITH_SERVICE(nsIIOService, serv, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsIChannel* channel;
    rv = serv->NewChannel("load",    // XXX what should this be?
                          urlStr, nsnull,
                          mGetter, &channel);
    if (NS_FAILED(rv)) return rv;

    // this cast is safe because nsFileURL::GetURLString aways
    // returns file: strings, and consequently we'll make nsIFileChannel
    // objects from them:
    *aParent = NS_STATIC_CAST(nsIFileChannel*, channel);

    return NS_OK;
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
    // XXX no Create in nsFileSpec -- creates non-existent file
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::Delete()
{
    // XXX no Delete in nsFileSpec -- deletes file or dir
    return NS_ERROR_NOT_IMPLEMENTED;
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
    // XXX no IsLink in nsFileSpec (for alias/shortcut/symlink)
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::ResolveLink(nsIFileChannel **_retval)
{
    // XXX no ResolveLink in nsFileSpec yet -- returns what link points to
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::MakeUniqueFileName(const char* baseName, char **_retval)
{
    // XXX makeUnique needs to return the name or file spec to the newly create
    // file!
    return NS_ERROR_NOT_IMPLEMENTED;
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
