/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator.
 * 
 * The Initial Developer of the Original Code is Intel Corp.
 * Portions created by Intel Corp. are
 * Copyright (C) 1999, 1999 Intel Corp.  All
 * Rights Reserved.
 * 
 * Contributor(s): Yixiong Zou <yixiong.zou@intel.com>
 *                 Carl Wong <carl.wong@intel.com>
 */

/*
 * Most of the code are taken from nsFileChannel.  
 */

#include "nsDiskCacheRecordChannel.h"
#include "nsIFileTransportService.h"
//#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIOutputStream.h"
#include "netCore.h"
#include "nsCExternalHandlerService.h"
#include "nsIMIMEService.h"
#include "nsISupportsUtils.h"
#include "prio.h"

//static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);

// This is copied from nsMemCacheChannel, We should consolidate these two.
class WriteStreamWrapper : public nsIOutputStream 
{
  public:
  WriteStreamWrapper(nsDiskCacheRecordTransport* aTransport,
                     nsIOutputStream *aBaseStream);

  virtual ~WriteStreamWrapper();

  static nsresult
  Create(nsDiskCacheRecordTransport* aTransport, nsIOutputStream *aBaseStream, nsIOutputStream* *aWrapper);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM

  private:
  nsDiskCacheRecordTransport*       mTransport;
  nsCOMPtr<nsIOutputStream>         mBaseStream;
  PRUint32                          mTotalSize;
  PRUint32                          mOldLength;
};

// implement nsISupports
NS_IMPL_THREADSAFE_ISUPPORTS1(WriteStreamWrapper, nsIOutputStream)

WriteStreamWrapper::WriteStreamWrapper(nsDiskCacheRecordTransport* aTransport, 
                                       nsIOutputStream *aBaseStream) 
  : mTransport(aTransport), mBaseStream(aBaseStream), mTotalSize(0), mOldLength(0)
{ 
  NS_INIT_REFCNT(); 
  NS_ADDREF(mTransport);
  mTransport->mRecord->GetStoredContentLength(&mOldLength);
}

WriteStreamWrapper::~WriteStreamWrapper()
{
  NS_RELEASE(mTransport);
}

nsresult 
WriteStreamWrapper::Create(nsDiskCacheRecordTransport*aTransport, nsIOutputStream *aBaseStream, nsIOutputStream* * aWrapper) 
{
  WriteStreamWrapper *wrapper = new WriteStreamWrapper(aTransport, aBaseStream);
  if (!wrapper) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(wrapper);
  *aWrapper = wrapper;
  return NS_OK;
}

NS_IMETHODIMP
WriteStreamWrapper::Write(const char *aBuffer, PRUint32 aCount, PRUint32 *aNumWritten) 
{
  *aNumWritten = 0;
  nsresult rv = mBaseStream->Write(aBuffer, aCount, aNumWritten);
  mTotalSize += *aNumWritten;
  return rv;
}
    
NS_IMETHODIMP
WriteStreamWrapper::WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval)
{
    NS_NOTREACHED("WriteFrom");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WriteStreamWrapper::WriteSegments(nsReadSegmentFun reader, void * closure, PRUint32 count, PRUint32 *_retval)
{
    NS_NOTREACHED("WriteSegments");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WriteStreamWrapper::GetNonBlocking(PRBool *aNonBlocking)
{
    NS_NOTREACHED("GetNonBlocking");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WriteStreamWrapper::SetNonBlocking(PRBool aNonBlocking)
{
    NS_NOTREACHED("SetNonBlocking");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WriteStreamWrapper::GetObserver(nsIOutputStreamObserver * *aObserver)
{
    NS_NOTREACHED("GetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WriteStreamWrapper::SetObserver(nsIOutputStreamObserver * aObserver)
{
    NS_NOTREACHED("SetObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WriteStreamWrapper::Flush() 
{ 
  return mBaseStream->Flush(); 
}

NS_IMETHODIMP
WriteStreamWrapper::Close() 
{
  nsresult rv = mBaseStream->Close(); 

  // Tell the record we finished write to the file
  mTransport->mRecord->WriteComplete();

  if (mTotalSize < mOldLength) {

      // Truncate the file if we have to. It should have been already but that
      // would be too easy wouldn't it!!!
      mTransport->mRecord->SetStoredContentLength(mTotalSize);
  } else if (mTotalSize > mOldLength) {

      mTransport->NotifyStorageInUse(mTotalSize - mOldLength);
  }

  return rv;
}

nsDiskCacheRecordTransport::nsDiskCacheRecordTransport(nsDiskCacheRecord *aRecord, 
                                                   nsILoadGroup *aLoadGroup)
  : mRecord(aRecord),
    mLoadGroup(aLoadGroup),
    mStatus(NS_OK) 
{
  NS_INIT_REFCNT();
  NS_ADDREF(mRecord);
  mRecord->mNumTransports++;
}

nsDiskCacheRecordTransport::~nsDiskCacheRecordTransport()
{
  mRecord->mNumTransports--;
  NS_RELEASE(mRecord);
}

// FUR!!
//
//  I know that I gave conflicting advice on the issue of file
//  transport versus file protocol handler, but I thought that the
//  last word was that we would use the raw transport, when I wrote:
//
//   > I just thought of an argument for the other side of the coin, i.e. the
//   > benefit of *not* reusing the file protocol handler: On the Mac, it's
//   > expensive to convert from a string URL to an nsFileSpec, because the Mac
//   > is brain-dead and scans every directory on the path to the file.  It's
//   > cheaper to create a nsFileSpec for a cache file by combining a single,
//   > static nsFileSpec that corresponds to the cache directory with the
//   > relative path to the cache file (using nsFileSpec's operator +).  This
//   > operation is optimized on the Mac to avoid the scanning operation.
//
//  The Mac guys will eat us alive if we do path string to nsFileSpec
//  conversions for every cache file we open.

nsresult 
nsDiskCacheRecordTransport::Init(void) 
{
  nsresult rv = mRecord->mFile->Clone(getter_AddRefs(mSpec)) ;
#if 0  
#ifdef XP_MAC

  // Don't assume we actually created a good file spec
  FSSpec theSpec = mSpec.GetFSSpec();
  if (!theSpec.name[0]) {
    NS_ERROR("failed to create a file spec");

    // Since we didn't actually create the file spec
    // we return an error
    return NS_ERROR_MALFORMED_URI;
  }
#endif
 #endif 
  return rv ;
}

nsresult 
nsDiskCacheRecordTransport::NotifyStorageInUse(PRInt32 aBytesUsed)
{
  return mRecord->mDiskCache->mStorageInUse += aBytesUsed;
}

// implement nsISupports
NS_IMPL_THREADSAFE_ISUPPORTS5(nsDiskCacheRecordTransport, 
                              nsITransport,
                              nsITransportRequest,
                              nsIRequest,
                              nsIStreamListener,
                              nsIStreamObserver)

// implement nsIRequest
NS_IMETHODIMP
nsDiskCacheRecordTransport::GetName(PRUnichar* *result)
{
  NS_NOTREACHED("nsDiskCacheRecordTransport::GetName");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDiskCacheRecordTransport::IsPending(PRBool *aIsPending) 
{
  *aIsPending = PR_FALSE;
  if(!mCurrentReadRequest)
    return NS_OK;

  return mCurrentReadRequest->IsPending(aIsPending);
}

NS_IMETHODIMP
nsDiskCacheRecordTransport::GetStatus(nsresult *status)
{
    *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsDiskCacheRecordTransport::Cancel(nsresult status)
{
  NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
  mStatus = status;
  if(!mCurrentReadRequest)
    return NS_ERROR_FAILURE;

  return mCurrentReadRequest->Cancel(status);
}

NS_IMETHODIMP
nsDiskCacheRecordTransport::Suspend(void)
{
  if(!mCurrentReadRequest)
    return NS_ERROR_FAILURE;

  return mCurrentReadRequest->Suspend();
}

NS_IMETHODIMP
nsDiskCacheRecordTransport::Resume(void)
{
  if(!mCurrentReadRequest)
    return NS_ERROR_FAILURE;

  return mCurrentReadRequest->Resume();
}

NS_IMETHODIMP
nsDiskCacheRecordTransport::GetTransport(nsITransport **result)
{
    NS_ENSURE_ARG_POINTER(result);
    NS_ADDREF(*result = this);
    return NS_OK;
}


#if 0
// implement nsIChannel

NS_IMETHODIMP
nsDiskCacheRecordTransport::GetOriginalURI(nsIURI* *aURI)
{
  // FUR - might need to implement this - not sure
  NS_NOTREACHED("nsDiskCacheRecordTransport::GetOriginalURI");
  return NS_ERROR_NOT_IMPLEMENTED ;
}
  
NS_IMETHODIMP
nsDiskCacheRecordTransport::SetOriginalURI(nsIURI* aURI)
{
  // FUR - might need to implement this - not sure
  NS_NOTREACHED("nsDiskCacheRecordTransport::SetOriginalURI");
  return NS_ERROR_NOT_IMPLEMENTED ;
}
  
NS_IMETHODIMP
nsDiskCacheRecordTransport::GetURI(nsIURI* *aURI)
{
  if(!mFileTransport)
    return NS_ERROR_FAILURE;

  return mFileTransport->GetURI(aURI); // no-op
}

NS_IMETHODIMP
nsDiskCacheRecordTransport::SetURI(nsIURI* aURI)
{
  if(!mFileTransport)
    return NS_ERROR_FAILURE;

  return mFileTransport->SetURI(aURI); // no-op
}
#endif

NS_IMETHODIMP
nsDiskCacheRecordTransport::OpenInputStream(PRUint32 transferOffset,
                                            PRUint32 transferCount, 
                                            PRUint32 transferFlags, 
                                            nsIInputStream* *aResult)
{
  nsresult rv;

  if(mFileTransport)
    return NS_ERROR_IN_PROGRESS;

  NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
  if(NS_FAILED(rv)) return rv;
  
  rv = fts->CreateTransport(mSpec,
                            PR_RDONLY,
                            PR_IRUSR | PR_IWUSR,
                            getter_AddRefs(mFileTransport));
  if(NS_FAILED(rv))
    return rv;
  
  // we don't need to worry about progress notification
  
  rv = mFileTransport->OpenInputStream(transferOffset,
                                       transferCount,
                                       transferFlags,
                                       aResult);
  if(NS_FAILED(rv)) 
    mFileTransport = nsnull;

  return rv;
}

NS_IMETHODIMP
nsDiskCacheRecordTransport::OpenOutputStream(PRUint32 transferOffset,
                                             PRUint32 transferCount, 
                                             PRUint32 transferFlags, 
                                             nsIOutputStream* *aResult)
{
  nsresult rv;
  NS_ENSURE_ARG(aResult);
  
  if(mFileTransport)
    return NS_ERROR_IN_PROGRESS;

  nsCOMPtr<nsIOutputStream> outputStream;

  NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
  if(NS_FAILED(rv)) return rv;
  
  rv = fts->CreateTransport(mSpec,
                            PR_WRONLY | PR_CREATE_FILE,
                            PR_IRUSR | PR_IWUSR,
                            getter_AddRefs(mFileTransport));
  if(NS_FAILED(rv))
    return rv;
 
  // we don't need to worry about notification callbacks
  
  rv = mFileTransport->OpenOutputStream(transferOffset,
                                        transferCount, 
                                        transferFlags, 
                                        getter_AddRefs(outputStream));
  if(NS_FAILED(rv)) {
    mFileTransport = nsnull;
    return rv;
  }
 
  return WriteStreamWrapper::Create(this, outputStream, aResult);
}

NS_IMETHODIMP
nsDiskCacheRecordTransport::AsyncRead(nsIStreamListener *aListener,
                                      nsISupports *aContext,
                                      PRUint32 transferOffset, 
                                      PRUint32 transferCount, 
                                      PRUint32 transferFlags, 
                                      nsIRequest **_retval)
{
  nsresult rv;

  if(mFileTransport)
    return NS_ERROR_IN_PROGRESS;
  
  mRealListener = aListener;
  nsCOMPtr<nsIStreamListener> tempListener = this;

// XXX Only channels are added to load groups
#if 0
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

    rv = mLoadGroup->AddRequest(this, nsnull);
    if (NS_FAILED(rv)) return rv;
  }
#endif


  NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
 
  rv = fts->CreateTransport(mSpec,
                            PR_RDONLY,
                            PR_IRUSR | PR_IWUSR,
                            getter_AddRefs(mFileTransport));
  if (NS_FAILED(rv)) return rv;

  // no callbacks

  rv = mFileTransport->AsyncRead(tempListener, aContext,
                                 transferOffset, 
                                 transferCount, 
                                 transferFlags,
                                 getter_AddRefs(mCurrentReadRequest));

  if (NS_FAILED(rv)) {
    // release the transport so that we don't think we're in progress
    mFileTransport = nsnull;
  }

  NS_ADDREF(*_retval=this);
  return rv;
}

NS_IMETHODIMP
nsDiskCacheRecordTransport::AsyncWrite(nsIStreamProvider *provider, 
                                     nsISupports *ctxt,
                                     PRUint32 transferOffset, 
                                     PRUint32 transferCount, 
                                     PRUint32 transferFlags, 
                                     nsIRequest **_retval)
{
  /*
  if(!mFileTransport)
    return NS_ERROR_FAILURE;

  return mFileTransport->AsyncWrite(fromStream, 
                                    startPosition,
                                    writeCount,
                                    ctxt,
                                    observer);
  */

  // I can't do this since the write is not monitored, and I won't be
  // able to updata the storage. 
  NS_NOTREACHED("nsDiskCacheRecordTransport::AsyncWrite");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsDiskCacheRecordTransport::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
nsDiskCacheRecordTransport::GetProgressEventSink(nsIProgressEventSink **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDiskCacheRecordTransport::SetProgressEventSink(nsIProgressEventSink *aProgress)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsDiskCacheRecordTransport::OnStartRequest(nsIRequest *request, nsISupports* context)
{
  NS_ASSERTION(mRealListener, "No listener...");
  return mRealListener->OnStartRequest(this, context);
}

NS_IMETHODIMP
nsDiskCacheRecordTransport::OnStopRequest(nsIRequest *request, nsISupports* context,
                                        nsresult aStatus, const PRUnichar* aStatusArg)
{
  nsresult rv;

  rv = mRealListener->OnStopRequest(this, context, aStatus, aStatusArg);

// XXX Only channels are added to load groups
#if 0
  if (mLoadGroup) {
    if (NS_SUCCEEDED(rv)) {
      mLoadGroup->RemoveRequest(this, context, aStatus, aStatusArg);
    }
  }
#endif

  // Release the reference to the consumer stream listener...
  mRealListener = 0;
  mFileTransport = 0;
  mCurrentReadRequest = 0;
  return rv;
}

NS_IMETHODIMP
nsDiskCacheRecordTransport::OnDataAvailable(nsIRequest *request, nsISupports* context,
                               nsIInputStream *aIStream, PRUint32 aSourceOffset,
                               PRUint32 aLength)
{
  nsresult rv;

  rv = mRealListener->OnDataAvailable(this, context, aIStream,
                                      aSourceOffset, aLength);

  //
  // If the connection is being aborted cancel the transport.  This will
  // insure that the transport will go away even if it is blocked waiting
  // for the consumer to empty the pipe...
  //
  if (NS_FAILED(rv) && mCurrentReadRequest) {
    mCurrentReadRequest->Cancel(rv);
  }
  return rv;
}

// XXX No reason to implement nsIFileChannel
#if 0
/* void init (in nsIFile file, in long ioFlags, in long perm); */
NS_IMETHODIMP nsDiskCacheRecordTransport::Init(nsIFile *file, PRInt32 ioFlags, PRInt32 perm)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIFile file; */
NS_IMETHODIMP nsDiskCacheRecordTransport::GetFile(nsIFile * *result)
{
    NS_ADDREF(*result = mSpec);
    return NS_OK;
}

/* attribute long ioFlags; */
NS_IMETHODIMP nsDiskCacheRecordTransport::GetIoFlags(PRInt32 *aIoFlags)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsDiskCacheRecordTransport::SetIoFlags(PRInt32 aIoFlags)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute long permissions; */
NS_IMETHODIMP nsDiskCacheRecordTransport::GetPermissions(PRInt32 *aPermissions)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsDiskCacheRecordTransport::SetPermissions(PRInt32 aPermissions)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
#endif
