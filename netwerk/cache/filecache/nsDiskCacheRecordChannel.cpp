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
#include "nsIMIMEService.h"
#include "nsISupportsUtils.h"
#include "prio.h"

//static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kMIMEServiceCID, NS_MIMESERVICE_CID);

// This is copied from nsMemCacheChannel, We should consolidate these two.
class WriteStreamWrapper : public nsIOutputStream 
{
  public:
  WriteStreamWrapper(nsDiskCacheRecordChannel* aChannel,
                     nsIOutputStream *aBaseStream);

  virtual ~WriteStreamWrapper();

  static nsresult
  Create(nsDiskCacheRecordChannel* aChannel, nsIOutputStream *aBaseStream, nsIOutputStream* *aWrapper);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM

  private:
  nsDiskCacheRecordChannel*         mChannel;
  nsCOMPtr<nsIOutputStream>         mBaseStream;
  PRUint32                          mTotalSize;
};

// implement nsISupports
NS_IMPL_THREADSAFE_ISUPPORTS(WriteStreamWrapper, NS_GET_IID(nsIOutputStream))

WriteStreamWrapper::WriteStreamWrapper(nsDiskCacheRecordChannel* aChannel, 
                                       nsIOutputStream *aBaseStream) 
  : mChannel(aChannel), mBaseStream(aBaseStream), mTotalSize(0)
{ 
  NS_INIT_REFCNT(); 
  NS_ADDREF(mChannel);
}

WriteStreamWrapper::~WriteStreamWrapper()
{
  NS_RELEASE(mChannel);
}

nsresult 
WriteStreamWrapper::Create(nsDiskCacheRecordChannel*aChannel, nsIOutputStream *aBaseStream, nsIOutputStream* * aWrapper) 
{
  WriteStreamWrapper *wrapper = new WriteStreamWrapper(aChannel, aBaseStream);
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
  mChannel->NotifyStorageInUse(*aNumWritten);
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
  mChannel->mRecord->WriteComplete();

  // Truncate the file if we have to. It should have been already but that
  // would be too easy wouldn't it!!!
  mChannel->mRecord->SetStoredContentLength(mTotalSize);

  return rv;
}

nsDiskCacheRecordChannel::nsDiskCacheRecordChannel(nsDiskCacheRecord *aRecord, 
                                                   nsILoadGroup *aLoadGroup)
  : mRecord(aRecord),
    mLoadGroup(aLoadGroup),
    mLoadAttributes(nsIChannel::LOAD_NORMAL),
    mStatus(NS_OK) 
{
  NS_INIT_REFCNT();
  NS_ADDREF(mRecord);
  mRecord->mNumChannels++;
}

nsDiskCacheRecordChannel::~nsDiskCacheRecordChannel()
{
  mRecord->mNumChannels--;
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
nsDiskCacheRecordChannel::Init(void) 
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
nsDiskCacheRecordChannel::NotifyStorageInUse(PRInt32 aBytesUsed)
{
  return mRecord->mDiskCache->mStorageInUse += aBytesUsed;
}

// implement nsISupports
NS_IMPL_THREADSAFE_ISUPPORTS4(nsDiskCacheRecordChannel, 
                   nsIChannel, 
                   nsIRequest,
                   nsIStreamListener,
                   nsIStreamObserver)

// implement nsIRequest
NS_IMETHODIMP
nsDiskCacheRecordChannel::GetName(PRUnichar* *result)
{
  NS_NOTREACHED("nsDiskCacheRecordChannel::GetName");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::IsPending(PRBool *aIsPending) 
{
  *aIsPending = PR_FALSE;
  if(!mFileTransport)
    return NS_OK;

  return mFileTransport->IsPending(aIsPending);
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::GetStatus(nsresult *status)
{
    *status = mStatus;
    return NS_OK;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::Cancel(nsresult status)
{
  NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
  mStatus = status;
  if(!mFileTransport)
    return NS_ERROR_FAILURE;

  return mFileTransport->Cancel(status);
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::Suspend(void)
{
  if(!mFileTransport)
    return NS_ERROR_FAILURE;

  return mFileTransport->Suspend();
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::Resume(void)
{
  if(!mFileTransport)
    return NS_ERROR_FAILURE;

  return mFileTransport->Resume();
}

// implement nsIChannel

NS_IMETHODIMP
nsDiskCacheRecordChannel::GetOriginalURI(nsIURI* *aURI)
{
  // FUR - might need to implement this - not sure
  NS_NOTREACHED("nsDiskCacheRecordChannel::GetOriginalURI");
  return NS_ERROR_NOT_IMPLEMENTED ;
}
  
NS_IMETHODIMP
nsDiskCacheRecordChannel::SetOriginalURI(nsIURI* aURI)
{
  // FUR - might need to implement this - not sure
  NS_NOTREACHED("nsDiskCacheRecordChannel::SetOriginalURI");
  return NS_ERROR_NOT_IMPLEMENTED ;
}
  
NS_IMETHODIMP
nsDiskCacheRecordChannel::GetURI(nsIURI* *aURI)
{
  if(!mFileTransport)
    return NS_ERROR_FAILURE;

  return mFileTransport->GetURI(aURI);
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::SetURI(nsIURI* aURI)
{
  if(!mFileTransport)
    return NS_ERROR_FAILURE;

  return mFileTransport->SetURI(aURI);
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::OpenInputStream(nsIInputStream* *aResult)
{
  nsresult rv;

  if(mFileTransport)
    return NS_ERROR_IN_PROGRESS;

  NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
  if(NS_FAILED(rv)) return rv;
  
  rv = fts->CreateTransport(mSpec, PR_RDONLY, PR_IRUSR | PR_IWUSR,
                            getter_AddRefs(mFileTransport));
  if(NS_FAILED(rv))
    return rv;
  
  // we don't need to worry about notification callbacks
  
  rv = mFileTransport->OpenInputStream(aResult);
  if(NS_FAILED(rv)) 
    mFileTransport = nsnull;

  return rv;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::OpenOutputStream(nsIOutputStream* *aResult)
{
  nsresult rv;
  NS_ENSURE_ARG(aResult);
  
  if(mFileTransport)
    return NS_ERROR_IN_PROGRESS;

  nsCOMPtr<nsIOutputStream> outputStream;

  NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
  if(NS_FAILED(rv)) return rv;
  
  rv = fts->CreateTransport(mSpec, PR_WRONLY | PR_CREATE_FILE, PR_IRUSR | PR_IWUSR,
                            getter_AddRefs(mFileTransport));
  if(NS_FAILED(rv))
    return rv;
 
  // we don't need to worry about notification callbacks
  
  rv = mFileTransport->OpenOutputStream(getter_AddRefs(outputStream));
  if(NS_FAILED(rv)) {
    mFileTransport = nsnull;
    return rv;
  }
 
  return WriteStreamWrapper::Create(this, outputStream, aResult);
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::AsyncRead(nsIStreamListener *aListener,
                                    nsISupports *aContext)
{
  nsresult rv;

  if(mFileTransport)
    return NS_ERROR_IN_PROGRESS;
  
  mRealListener = aListener;
  nsCOMPtr<nsIStreamListener> tempListener = this;

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


  NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
 
  rv = fts->CreateTransport(mSpec, PR_RDONLY, PR_IRUSR | PR_IWUSR,
                            getter_AddRefs(mFileTransport));
  if (NS_FAILED(rv)) return rv;

  // no callbacks

  rv = mFileTransport->AsyncRead(tempListener, aContext);

  if (NS_FAILED(rv)) {
    // release the transport so that we don't think we're in progress
    mFileTransport = nsnull;
  }
  return rv;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::AsyncWrite(nsIInputStream *fromStream, 
                                     nsIStreamObserver *observer, 
                                     nsISupports *ctxt)

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
  NS_NOTREACHED("nsDiskCacheRecordChannel::AsyncWrite");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::GetLoadAttributes(nsLoadFlags *aLoadAttributes)
{
    *aLoadAttributes = mLoadAttributes;
    return NS_OK;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::SetLoadAttributes(nsLoadFlags aLoadAttributes)
{
    mLoadAttributes = aLoadAttributes;
    return NS_OK;
}

#define DUMMY_TYPE "application/x-unknown-content-type"

NS_IMETHODIMP
nsDiskCacheRecordChannel::GetContentType(char * *aContentType)
{
// Not required to be implemented, since it is implemented by cache manager
    NS_NOTREACHED("nsDiskCacheRecordChannel::GetContentType");
    return NS_ERROR_NOT_IMPLEMENTED;
// This was the pre nsIFile stuff. Not sure if I have to implement this routines since
// the memory cache doesn't
#if 0
  nsresult rv; 
  PRBool isDirectory;
  if ( NS_SUCCEEDED(mSpec->IsDirectory(&isDirectory)) && isDirectory) {
    *aContentType = nsCRT::strdup("application/http-index-format");
    return *aContentType ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    // I wish I can make this simplier


    char* urlStr;
    mRecord->mFile->GetURLString(&urlStr);

    // file: URLs (currently) have no additional structure beyond that provided by standard
    // URLs, so there is no "outer" given to CreateInstance 

    nsCOMPtr<nsIURI> url;
    rv = nsComponentManager::CreateInstance(kStandardURLCID, nsnull,
                                            NS_GET_IID(nsIURI),
                                            //(void**)&url);
                                            getter_AddRefs(url));
    if (NS_FAILED(rv)) return rv;

    rv = url->SetSpec((char*)urlStr);
    if (NS_FAILED(rv)) 
        return rv;

    NS_WITH_SERVICE(nsIMIMEService, MIMEService, kMIMEServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = MIMEService->GetTypeFromURI(url, aContentType);
    if (NS_SUCCEEDED(rv)) return rv;
   
  }

  *aContentType = nsCRT::strdup(DUMMY_TYPE);
  if (!*aContentType) {
    return NS_ERROR_OUT_OF_MEMORY;
  } else {
    return NS_OK;
  }
#endif
}

NS_IMETHODIMP nsDiskCacheRecordChannel::SetContentType(const char * aContentType) 
{
	NS_NOTREACHED("nsDiskCacheRecordChannel::SetContentType");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::GetContentLength(PRInt32 *aContentLength)
{
  nsresult rv;
  PRUint32 length;

  PRInt64 fileSize; 
  rv = mRecord->mFile->GetFileSize( &fileSize);
  LL_L2UI( length, fileSize );	
  
  
  if (NS_SUCCEEDED(rv)) {
    *aContentLength = (PRInt32)length;
  } else {
    *aContentLength = -1;
  }
  return rv;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::SetContentLength(PRInt32 aContentLength)
{
  NS_NOTREACHED("nsDiskCacheRecordChannel::SetContentLength");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::GetTransferOffset(PRUint32 *aTransferOffset)
{
 nsresult rv = NS_ERROR_FAILURE;
 if ( mFileTransport )
 	 rv = mFileTransport->GetTransferOffset( aTransferOffset );
 return rv;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::SetTransferOffset(PRUint32 aTransferOffset)
{
   nsresult rv = NS_OK;
 if ( mFileTransport )
 	 rv = mFileTransport->SetTransferOffset( aTransferOffset );
 return rv;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::GetTransferCount(PRInt32 *aTransferCount)
{
  NS_NOTREACHED("nsDiskCacheRecordChannel::GetTransferCount");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::SetTransferCount(PRInt32 aTransferCount)
{
  NS_NOTREACHED("nsDiskCacheRecordChannel::SetTransferCount");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::GetBufferSegmentSize(PRUint32 *aBufferSegmentSize)
{
  NS_NOTREACHED("nsDiskCacheRecordChannel::GetBufferSegmentSize");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::SetBufferSegmentSize(PRUint32 aBufferSegmentSize)
{
  NS_NOTREACHED("nsDiskCacheRecordChannel::SetBufferSegmentSize");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::GetBufferMaxSize(PRUint32 *aBufferMaxSize)
{
  NS_NOTREACHED("nsDiskCacheRecordChannel::GetBufferMaxSize");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::SetBufferMaxSize(PRUint32 aBufferMaxSize)
{
  NS_NOTREACHED("nsDiskCacheRecordChannel::SetBufferMaxSize");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::GetLocalFile(nsIFile* *file)
{
    *file = mSpec;
    NS_ADDREF(*file);
    return NS_OK;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::GetPipeliningAllowed(PRBool *aPipeliningAllowed)
{
    *aPipeliningAllowed = PR_FALSE;
    return NS_OK;
}
 
NS_IMETHODIMP
nsDiskCacheRecordChannel::SetPipeliningAllowed(PRBool aPipeliningAllowed)
{
    NS_NOTREACHED("SetPipeliningAllowed");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::GetOwner(nsISupports* *aOwner)
{
  *aOwner = mOwner.get();
  NS_IF_ADDREF(*aOwner);
  return NS_OK;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::SetOwner(nsISupports* aOwner) 
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
  // Not required to be implemented, since it is implemented by cache manager
  NS_ASSERTION(0, "nsDiskCacheRecordChannel method unexpectedly called");
  return NS_OK;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
  // Not required to be implemented, since it is implemented by cache manager
  NS_ASSERTION(0, "nsDiskCacheRecordChannel method unexpectedly called");
  return NS_OK;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
  // Not required to be implemented, since it is implemented by cache manager
  NS_NOTREACHED("nsDiskCacheRecordChannel::GetNotificationCallbacks");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
    // Not required to be implemented, since it is implemented by cache manager
    NS_NOTREACHED("nsDiskCacheRecordChannel::SetNotificationCallbacks");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsDiskCacheRecordChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
    *aSecurityInfo = nsnull;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsDiskCacheRecordChannel::OnStartRequest(nsIChannel* transportChannel, nsISupports* context)
{
  NS_ASSERTION(mRealListener, "No listener...");
  return mRealListener->OnStartRequest(this, context);
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::OnStopRequest(nsIChannel* transportChannel, nsISupports* context,
                                        nsresult aStatus, const PRUnichar* aStatusArg)
{
  nsresult rv;

  rv = mRealListener->OnStopRequest(this, context, aStatus, aStatusArg);

  if (mLoadGroup) {
    if (NS_SUCCEEDED(rv)) {
      mLoadGroup->RemoveChannel(this, context, aStatus, aStatusArg);
    }
  }

  // Release the reference to the consumer stream listener...
  mRealListener = null_nsCOMPtr();
  mFileTransport = null_nsCOMPtr();
  return rv;
}

NS_IMETHODIMP
nsDiskCacheRecordChannel::OnDataAvailable(nsIChannel* transportChannel, nsISupports* context,
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
  if (NS_FAILED(rv) && mFileTransport) {
    mFileTransport->Cancel(rv);
  }
  return rv;
}

