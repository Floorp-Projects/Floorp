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

#include "nsFileTransport.h"
#include "nsFileTransportService.h"
#include "nsIStreamListener.h"
#include "nsCRT.h"
#include "nscore.h"
#include "nsIFileStream.h"
#include "nsFileSpec.h"
#include "nsIBuffer.h"
#include "nsIBufferInputStream.h"
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"

static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);

////////////////////////////////////////////////////////////////////////////////
// nsFileTransport methods:

nsFileTransport::nsFileTransport()
    : mPath(nsnull), mContext(nsnull), mListener(nsnull), mState(ENDED),
      mSuspended(PR_FALSE), mFileStream(nsnull), mBuffer(nsnull),
      mBufferStream(nsnull), mStatus(NS_OK), mService(nsnull), mSourceOffset(0), mEventQueue(nsnull)
{
    NS_INIT_REFCNT();
    mMonitor = PR_NewMonitor();    
}

nsFileTransport::~nsFileTransport()
{
    if (mPath) nsCRT::free(mPath);
    PR_DestroyMonitor(mMonitor);
    NS_IF_RELEASE(mListener);
    NS_IF_RELEASE(mContext);
    NS_IF_RELEASE(mService);
    NS_IF_RELEASE(mFileStream);
    NS_IF_RELEASE(mBuffer);
    NS_IF_RELEASE(mBufferStream);
    NS_IF_RELEASE(mEventQueue);
}

nsresult
nsFileTransport::Init(const char* path,
                      nsFileTransportService* service)
{
    mPath = nsCRT::strdup(path);
    if (mPath == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    mService = service;
    NS_ADDREF(mService);

    return NS_OK;
}

nsresult
nsFileTransport::Init(nsISupports* context,
                      nsIStreamListener* listener,
                      State state, PRUint32 startPosition, PRInt32 count)
{
    nsresult rv = NS_OK;
    if (!mMonitor)
        return NS_ERROR_OUT_OF_MEMORY;
    PR_EnterMonitor(mMonitor);

    if (mState != ENDED)
        rv = NS_ERROR_FAILURE;
    else {
        mContext = context;
        NS_IF_ADDREF(mContext);

        mListener = listener;
        NS_ADDREF(mListener);

        mState = state;
        mSourceOffset = startPosition;
        mAmount = count;
    }
    PR_ExitMonitor(mMonitor);
    return rv;
}

NS_IMPL_ADDREF(nsFileTransport);
NS_IMPL_RELEASE(nsFileTransport);

NS_IMETHODIMP
nsFileTransport::QueryInterface(const nsIID& aIID, void* *aInstancePtr)
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
    if (aIID.Equals(nsCOMTypeInfo<nsIRunnable>::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsIRunnable*, this); 
        NS_ADDREF_THIS(); 
        return NS_OK; 
    } 
    if (aIID.Equals(nsCOMTypeInfo<nsIBufferObserver>::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsIBufferObserver*, this); 
        NS_ADDREF_THIS(); 
        return NS_OK; 
    } 
    return NS_NOINTERFACE; 
}

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsFileTransport::IsPending(PRBool *result)
{
    *result = mState != ENDED;
    return NS_OK; 
}

NS_IMETHODIMP
nsFileTransport::Cancel(void)
{
    nsresult rv = NS_OK;
    if (!mMonitor)
        return NS_ERROR_OUT_OF_MEMORY;
    PR_EnterMonitor(mMonitor);
    mStatus = NS_BINDING_ABORTED;
    if (mSuspended) {
        Resume();
    }
    mState = ENDING;
    PR_ExitMonitor(mMonitor);
    return rv;
}

NS_IMETHODIMP
nsFileTransport::Suspend(void)
{
    nsresult rv = NS_OK;
    if (!mMonitor)
        return NS_ERROR_OUT_OF_MEMORY;
    PR_EnterMonitor(mMonitor);
    if (!mSuspended) {
        // XXX close the stream here?
        mStatus = mService->Suspend(this);
        mSuspended = PR_TRUE;
    }
    PR_ExitMonitor(mMonitor);
    return rv;
}

NS_IMETHODIMP
nsFileTransport::Resume(void)
{
    nsresult rv = NS_OK;
    if (!mMonitor)
        return NS_ERROR_OUT_OF_MEMORY;
    PR_EnterMonitor(mMonitor);
    if (!mSuspended) {
        // XXX re-open the stream and seek here?
        mStatus = mService->Resume(this);
        mSuspended = PR_FALSE;
    }
    PR_ExitMonitor(mMonitor);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsIChannel methods:

NS_IMETHODIMP
nsFileTransport::GetURI(nsIURI* *aURL)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                           nsISupports *ctxt,
                           nsIStreamListener *listener)
{
    nsresult rv;

    if (!mEventQueue) {
        NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueService, &rv);
        if (NS_FAILED(rv)) return rv;
        rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), &mEventQueue);
        if (NS_FAILED(rv)) return rv;
    }

    nsIStreamListener* asyncListener;
    rv = NS_NewAsyncStreamListener(&asyncListener, mEventQueue, listener);
    if (NS_FAILED(rv)) return rv;

    rv = Init(ctxt, asyncListener, START_READ, startPosition, readCount);
    NS_RELEASE(asyncListener);

    rv = mService->DispatchRequest(this);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::AsyncWrite(nsIInputStream* fromStream,
                            PRUint32 startPosition,
                            PRInt32 writeCount,
                            nsISupports* context,
                            nsIStreamObserver* observer)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsFileTransport::GetLoadAttributes(nsLoadFlags *aLoadAttributes) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::SetLoadAttributes(nsLoadFlags aLoadAttributes) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileTransport::GetContentType(char * *aContentType) {
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsFileTransport::OpenInputStream(PRUint32 startPosition, PRInt32 readCount, 
                                 nsIInputStream* *result)
{
    nsresult rv;
    NS_ASSERTION(startPosition == 0, "fix me");

    nsIStreamListener* syncListener;
    nsIInputStream* inStr;
    nsIBufferOutputStream* outStream;

    rv = NS_NewSyncStreamListener(&inStr, &outStream, &syncListener);
    if (NS_FAILED(rv)) return rv;

    rv = Init(nsnull, syncListener, START_READ, 0, -1);
    NS_RELEASE(syncListener);
    if (NS_FAILED(rv)) {
        NS_RELEASE(inStr);
        return rv;
    }

    rv = mService->DispatchRequest(this);
    if (NS_FAILED(rv)) {
        NS_RELEASE(inStr);
        return rv;
    }

    *result = inStr;
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::OpenOutputStream(PRUint32 startPosition, nsIOutputStream* *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////

void
nsFileTransport::Process(void)
{
    if (!mMonitor) {
        mState = ENDING;
        return;
    }
    PR_EnterMonitor(mMonitor);
    switch (mState) {
      case START_READ: {
          nsISupports* fs;
          nsFileSpec spec(mPath);

          mStatus = mListener->OnStartRequest(this, mContext);  // always send the start notification
          if (NS_FAILED(mStatus)) goto error;

          mStatus = NS_NewTypicalInputFileStream(&fs, spec);
          if (NS_FAILED(mStatus)) goto error;

          mStatus = fs->QueryInterface(nsCOMTypeInfo<nsIInputStream>::GetIID(), (void**)&mFileStream);
          NS_RELEASE(fs);
          if (NS_FAILED(mStatus)) goto error;

          mStatus = NS_NewBuffer(&mBuffer, NS_FILE_TRANSPORT_BUFFER_SIZE,
                                 NS_FILE_TRANSPORT_BUFFER_SIZE, this);
          if (NS_FAILED(mStatus)) goto error;

          mStatus = NS_NewBufferInputStream(&mBufferStream, mBuffer, PR_FALSE);
          if (NS_FAILED(mStatus)) goto error;

          mState = READING;
          break;
      }
      case READING: {
          if (NS_FAILED(mStatus)) goto error;

          PRUint32 amt;
          nsIInputStream* inStr = NS_STATIC_CAST(nsIInputStream*, mFileStream);
          PRUint32 inLen;
          mStatus = inStr->GetLength(&inLen);
          if (NS_FAILED(mStatus)) goto error;

          mStatus = mBuffer->WriteFrom(inStr, inLen, &amt);
          if (mStatus == NS_BASE_STREAM_EOF) goto error; 

          // XXX we could be catching a legit error code here that we should spit back
          // XXX not just the one we're expecting to come from the buffer.
          if (NS_FAILED(mStatus)) {
              PR_Wait(mMonitor, PR_INTERVAL_NO_TIMEOUT);
              mStatus = NS_OK;
              break;
          }

          // and feed the buffer to the application via the byte buffer stream:
          // XXX maybe amt should be mBufferStream->GetLength():
          mStatus = mListener->OnDataAvailable(this, mContext, mBufferStream, mSourceOffset, amt);
          if (NS_FAILED(mStatus)) goto error;
          
          mSourceOffset += amt;

          // stay in the READING state
          break;
      }
      case START_WRITE: {
          nsISupports* fs;
          nsFileSpec spec(mPath);

          mStatus = mListener->OnStartRequest(this, mContext);  // always send the start notification
          if (NS_FAILED(mStatus)) goto error;

          mStatus = NS_NewTypicalOutputFileStream(&fs, spec);
          if (NS_FAILED(mStatus)) goto error;

          mStatus = fs->QueryInterface(nsCOMTypeInfo<nsIOutputStream>::GetIID(), (void**)&mFileStream);
          NS_RELEASE(fs);
          if (NS_FAILED(mStatus)) goto error;

          mStatus = NS_NewBuffer(&mBuffer, NS_FILE_TRANSPORT_BUFFER_SIZE,
                                 NS_FILE_TRANSPORT_BUFFER_SIZE, this);
          if (NS_FAILED(mStatus)) goto error;

          mStatus = NS_NewBufferInputStream(&mBufferStream, mBuffer, PR_FALSE);
          if (NS_FAILED(mStatus)) goto error;

          mState = WRITING;
          break;
      }
      case WRITING: {
          break;
      }
      case ENDING: {
          NS_IF_RELEASE(mBufferStream);
          mBufferStream = nsnull;
          NS_IF_RELEASE(mFileStream);
          mFileStream = nsnull;

          // XXX where do we get the error message?
          (void)mListener->OnStopRequest(this, mContext, mStatus, nsnull);

          mState = ENDED;
          break;
      }
      case ENDED: {
          NS_NOTREACHED("trying to continue an ended file transfer");
          break;
      }
    }
    PR_ExitMonitor(mMonitor);
    return;

  error:
    // we'll map EOF to NS_OK, otherwise external users are going to choke on NS_FAILED(NS_BASE_STREAM_EOF)
    if (mStatus = NS_BASE_STREAM_EOF) mStatus = NS_OK;
    mState = ENDING;
    PR_ExitMonitor(mMonitor);
    return;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileTransport::Run(void)
{
    while (mState != ENDED && !mSuspended) {
        Process();
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileTransport::OnFull(nsIBuffer *buffer) {
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::OnWrite(nsIBuffer* aBuffer, PRUint32 aCount)
{
    return NS_OK;
}

NS_IMETHODIMP
nsFileTransport::OnEmpty(nsIBuffer *buffer) {
    PR_EnterMonitor(mMonitor);
    PR_Notify(mMonitor);
    PR_ExitMonitor(mMonitor);
    return NS_OK;
}