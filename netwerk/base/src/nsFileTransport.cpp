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
#include "nsIByteBufferInputStream.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

////////////////////////////////////////////////////////////////////////////////
// nsFileTransport methods:

nsFileTransport::nsFileTransport()
    : mPath(nsnull), mContext(nsnull), mListener(nsnull), mState(STARTING),
      mFileStream(nsnull), mBufferStream(nsnull), mStatus(NS_OK)
{
    NS_INIT_REFCNT();
}

nsFileTransport::~nsFileTransport()
{
    if (mPath) delete[] mPath;
    NS_IF_RELEASE(mListener);
    NS_IF_RELEASE(mContext);
    NS_IF_RELEASE(mService);
    NS_IF_RELEASE(mFileStream);
    NS_IF_RELEASE(mBufferStream);
}

nsresult
nsFileTransport::Init(const char* path,
                      nsISupports* context,
                      nsIStreamListener* listener,
                      nsFileTransportService* service)
{
    mPath = nsCRT::strdup(path);
    if (mPath == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    
    mContext = context;
    NS_IF_ADDREF(mContext);

    mListener = listener;
    NS_ADDREF(mListener);

    mService = service;
    NS_ADDREF(mService);

    return NS_OK;
}

NS_IMPL_ADDREF(nsFileTransport);
NS_IMPL_RELEASE(nsFileTransport);

NS_IMETHODIMP
nsFileTransport::QueryInterface(const nsIID& aIID, void* *aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER; 
    } 
    if (aIID.Equals(nsITransport::GetIID()) ||
        aIID.Equals(nsICancelable::GetIID()) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = NS_STATIC_CAST(nsITransport*, this); 
        NS_ADDREF_THIS(); 
        return NS_OK; 
    } 
    if (aIID.Equals(nsIRunnable::GetIID())) {
        *aInstancePtr = NS_STATIC_CAST(nsIRunnable*, this); 
        NS_ADDREF_THIS(); 
        return NS_OK; 
    } 
    return NS_NOINTERFACE; 
}

////////////////////////////////////////////////////////////////////////////////
// nsICancelable methods:

NS_IMETHODIMP
nsFileTransport::Cancel(void)
{
    nsresult rv = NS_OK;
    PR_CEnterMonitor(this);
    mStatus = NS_BINDING_ABORTED;
    switch (mState) {
      case RUNNING:
        mState = ENDING;
        break;
      case SUSPENDED:
        rv = Resume();
        break;
      default:
        rv = NS_ERROR_FAILURE;
        break;
    }
    PR_CExitMonitor(this);
    return rv;
}

NS_IMETHODIMP
nsFileTransport::Suspend(void)
{
    nsresult rv = NS_OK;
    PR_CEnterMonitor(this);
    switch (mState) {
      case RUNNING:
        // XXX close the stream here?
        mStatus = mService->Suspend(this);

        mState = SUSPENDED;
        break;
      default:
        rv = NS_ERROR_FAILURE;
        break;
    }
    PR_CExitMonitor(this);
    return rv;
}

NS_IMETHODIMP
nsFileTransport::Resume(void)
{
    nsresult rv = NS_OK;
    PR_CEnterMonitor(this);
    switch (mState) {
      case SUSPENDED:
        // XXX re-open the stream and seek here?
        mStatus = mService->Resume(this);

        mState = RUNNING;
        break;
      default:
        rv = NS_ERROR_FAILURE;
        break;
    }
    PR_CExitMonitor(this);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

void
nsFileTransport::Continue(void)
{
    PR_CEnterMonitor(this);
    switch (mState) {
      case STARTING: {
          nsISupports* fs;
          nsFileSpec spec(mPath);

          mStatus = mListener->OnStartBinding(mContext);  // always send the start notification
          if (NS_FAILED(mStatus)) goto error;

          mStatus = NS_NewTypicalInputFileStream(&fs, spec);
          if (NS_FAILED(mStatus)) goto error;

          mStatus = fs->QueryInterface(nsIInputStream::GetIID(), (void**)&mFileStream);
          NS_RELEASE(fs);
          if (NS_FAILED(mStatus)) goto error;

          mStatus = NS_NewByteBufferInputStream(&mBufferStream, PR_FALSE, NS_FILE_TRANSPORT_BUFFER_SIZE);
          if (NS_FAILED(mStatus)) goto error;

          mState = RUNNING;
          break;
      }
      case RUNNING: {
          if (NS_FAILED(mStatus)) goto error;

          PRUint32 amt;
          mStatus = mBufferStream->Fill(mFileStream, &amt);
          if (mStatus == NS_BASE_STREAM_EOF) goto error; 
          if (NS_FAILED(mStatus)) goto error;

          // and feed the buffer to the application via the byte buffer stream:
          mStatus = mListener->OnDataAvailable(mContext, mBufferStream, amt);      // XXX maybe amt should be bufStr->GetLength()
          if (NS_FAILED(mStatus)) goto error;
          
          // stay in the RUNNING state
          break;
      }
      case SUSPENDED: {
          NS_NOTREACHED("trying to continue a suspended file transfer");
          break;
      }
      case ENDING: {
          NS_IF_RELEASE(mBufferStream);
          mBufferStream = nsnull;
          NS_IF_RELEASE(mFileStream);
          mFileStream = nsnull;

          // XXX where do we get the error message?
          (void)mListener->OnStopBinding(mContext, mStatus, nsnull);

          mState = ENDED;
          break;
      }
      case ENDED: {
          NS_NOTREACHED("trying to continue an ended file transfer");
          break;
      }
    }
    PR_CExitMonitor(this);
    return;

  error:
    mState = ENDING;
    PR_CExitMonitor(this);
    return;
}

////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileTransport::Run(void)
{
    while (mState != ENDED && mState != SUSPENDED) {
        Continue();
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
