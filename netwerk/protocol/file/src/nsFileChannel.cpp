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

#include "nsFileChannel.h"
#include "nscore.h"
#include "nsIInterfaceRequestor.h"
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
#include "nsEscape.h"
#include "nsIMIMEService.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "nsIFileTransportService.h"

static NS_DEFINE_CID(kMIMEServiceCID, NS_MIMESERVICE_CID);
static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

nsFileChannel::nsFileChannel()
    : mLoadAttributes(LOAD_NORMAL)
{
    NS_INIT_REFCNT();
}

nsresult
nsFileChannel::Init(nsIFileProtocolHandler* handler, 
                    const char* command,
                    nsIURI* uri,
                    nsILoadGroup* aLoadGroup,
                    nsIInterfaceRequestor* notificationCallbacks,
                    nsLoadFlags loadAttributes,
                    nsIURI* originalURI)
{
    nsresult rv;

    mHandler = handler;
    mOriginalURI = originalURI ? originalURI : uri;
    mURI = uri;
    mCommand = nsCRT::strdup(command);
    if (mCommand == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    rv = SetLoadAttributes(loadAttributes);
    if (NS_FAILED(rv)) return rv;

    rv = SetLoadGroup(aLoadGroup);
    if (NS_FAILED(rv)) return rv;

    rv = SetNotificationCallbacks(notificationCallbacks);
    if (NS_FAILED(rv)) return rv;

    // if we support the nsIURL interface then use it to get just
    // the file path with no other garbage!
    nsCOMPtr<nsIURL> aUrl = do_QueryInterface(mURI, &rv);
    if (NS_SUCCEEDED(rv) && aUrl) { // does it support the url interface?
        nsXPIDLCString fileString;
        aUrl->GetFilePath(getter_Copies(fileString));
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
    else {
        // otherwise do the best we can by using the spec for the uri....
        // XXX temporary, until we integrate more thoroughly with nsFileSpec
        char* url;
        rv = mURI->GetSpec(&url);
        if (NS_FAILED(rv)) return rv;
        nsFileURL fileURL(url);
        nsCRT::free(url);
        mSpec = fileURL;
    }

    return rv;
}

nsFileChannel::~nsFileChannel()
{
    if (mCommand) nsCRT::free(mCommand);
}

NS_IMPL_ISUPPORTS5(nsFileChannel,
                   nsIFileChannel,
                   nsIChannel,
                   nsIRequest,
                   nsIStreamListener,
                   nsIStreamObserver)

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
    if (mFileTransport)
        return mFileTransport->IsPending(result);
    *result = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::Cancel()
{
    if (mFileTransport)
        return mFileTransport->Cancel();
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::Suspend()
{
    if (mFileTransport)
        return mFileTransport->Suspend();
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::Resume()
{
    if (mFileTransport)
        return mFileTransport->Resume();
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIChannel
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::GetOriginalURI(nsIURI * *aURI)
{
    *aURI = mOriginalURI;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetURI(nsIURI * *aURI)
{
    *aURI = mURI;
    NS_ADDREF(*aURI);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::OpenInputStream(PRUint32 startPosition, PRInt32 readCount,
                               nsIInputStream **result)
{
    nsresult rv;

    if (mFileTransport)
        return NS_ERROR_IN_PROGRESS;

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = fts->CreateTransport(mSpec, mCommand, getter_AddRefs(mFileTransport));
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->SetNotificationCallbacks(mCallbacks);
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->OpenInputStream(startPosition, readCount, result);
  done:
    if (NS_FAILED(rv)) {
        // release the transport so that we don't think we're in progress
        mFileTransport = nsnull;
    }
    return rv;
}

NS_IMETHODIMP
nsFileChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **result)
{
    nsresult rv;

    if (mFileTransport)
        return NS_ERROR_IN_PROGRESS;

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = fts->CreateTransport(mSpec, mCommand, getter_AddRefs(mFileTransport));
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->SetNotificationCallbacks(mCallbacks);
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->OpenOutputStream(startPosition, result);
  done:
    if (NS_FAILED(rv)) {
        // release the transport so that we don't think we're in progress
        mFileTransport = nsnull;
    }
    return rv;
}

NS_IMETHODIMP
nsFileChannel::AsyncOpen(nsIStreamObserver *observer, nsISupports* ctxt)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsFileChannel::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                         nsISupports *ctxt,
                         nsIStreamListener *listener)
{
    nsresult rv;

    if (mFileTransport)
        return NS_ERROR_IN_PROGRESS;

    mRealListener = listener;
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

    rv = fts->CreateTransport(mSpec, mCommand, getter_AddRefs(mFileTransport));
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->SetNotificationCallbacks(mCallbacks);
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->AsyncRead(startPosition, readCount, ctxt, tempListener);
  done:
    if (NS_FAILED(rv)) {
        // release the transport so that we don't think we're in progress
        mFileTransport = nsnull;
    }
    return rv;
}

NS_IMETHODIMP
nsFileChannel::AsyncWrite(nsIInputStream *fromStream,
                          PRUint32 startPosition, PRInt32 writeCount,
                          nsISupports *ctxt,
                          nsIStreamObserver *observer)
{
    nsresult rv;

    if (mFileTransport)
        return NS_ERROR_IN_PROGRESS;

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = fts->CreateTransport(mSpec, mCommand, getter_AddRefs(mFileTransport));
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->SetNotificationCallbacks(mCallbacks);
    if (NS_FAILED(rv)) goto done;

    rv = mFileTransport->AsyncWrite(fromStream, startPosition, writeCount, ctxt, observer);
  done:
    if (NS_FAILED(rv)) {
        // release the transport so that we don't think we're in progress
        mFileTransport = nsnull;
    }
    return rv;
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
nsFileChannel::GetContentLength(PRInt32 *aContentLength)
{
    nsresult rv;
    PRUint32 length;

    rv = GetFileSize(&length);
    if (NS_SUCCEEDED(rv)) {
        *aContentLength = (PRInt32)length;
    } else {
        *aContentLength = -1;
    }
    return rv;
}

NS_IMETHODIMP
nsFileChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
    mLoadGroup = aLoadGroup;
    if (mLoadGroup) {
        nsresult rv = mLoadGroup->GetDefaultLoadAttributes(&mLoadAttributes);
        if (NS_FAILED(rv)) return rv;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetOwner(nsISupports* *aOwner)
{
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetOwner(nsISupports* aOwner)
{
    mOwner = aOwner;
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
  *aNotificationCallbacks = mCallbacks.get();
  NS_IF_ADDREF(*aNotificationCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
  mCallbacks = aNotificationCallbacks;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIStreamListener methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsFileChannel::OnStartRequest(nsIChannel* transportChannel, nsISupports* context)
{
    NS_ASSERTION(mRealListener, "No listener...");
    return mRealListener->OnStartRequest(this, context);
}

NS_IMETHODIMP
nsFileChannel::OnStopRequest(nsIChannel* transportChannel, nsISupports* context,
                             nsresult aStatus, const PRUnichar* aMsg)
{
    nsresult rv;

    rv = mRealListener->OnStopRequest(this, context, aStatus, aMsg);

    if (mLoadGroup) {
        if (NS_SUCCEEDED(rv)) {
            mLoadGroup->RemoveChannel(this, context, aStatus, aMsg);
        }
    }

    // Release the reference to the consumer stream listener...
    mRealListener = null_nsCOMPtr();
    mFileTransport = null_nsCOMPtr();
    return rv;
}

NS_IMETHODIMP
nsFileChannel::OnDataAvailable(nsIChannel* transportChannel, nsISupports* context,
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
    if (NS_FAILED(rv)) {
        mFileTransport->Cancel();
    }
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

    nsDirEnumerator() : mDir(nsnull) {
        NS_INIT_REFCNT();
    }

    nsresult Init(nsIFileProtocolHandler* handler, nsFileSpec& spec) {
        const char* path = spec.GetNativePathCString();
        mDir = PR_OpenDir(path);
        if (mDir == nsnull)    // not a directory?
            return NS_ERROR_FAILURE;
        mHandler = handler;
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
            rv = mHandler->NewChannelFromNativePath(path, getter_AddRefs(mNext));
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
        mNext = null_nsCOMPtr();
        return NS_OK;
    }

    virtual ~nsDirEnumerator() {
        if (mDir) {
            PRStatus status = PR_CloseDir(mDir);
            NS_ASSERTION(status == PR_SUCCESS, "close failed");
        }
    }

protected:
    nsCOMPtr<nsIFileProtocolHandler>    mHandler;
    PRDir*                              mDir;
    nsCOMPtr<nsIFileChannel>            mNext;
};

NS_IMPL_ISUPPORTS(nsDirEnumerator, NS_GET_IID(nsISimpleEnumerator));

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
    rv = src->QueryInterface(NS_GET_IID(nsIFileChannel), (void**)&fc);
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
    rv = src->QueryInterface(NS_GET_IID(nsIFileChannel), (void**)&fc);
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

    if (NS_SUCCEEDED(rv)) {
        return CreateFileChannelFromFileSpec(tempSpec, _retval);
    }

    return rv;
}

NS_IMETHODIMP
nsFileChannel::MakeUnique(const char* baseName, nsIFileChannel **_retval)
{
    if (mSpec.IsDirectory()) {
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
        rv = mURI->QueryInterface(NS_GET_IID(nsIURL), (void**)&url);
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

NS_IMETHODIMP
nsFileChannel::GetFileSpec(nsFileSpec *spec)
{
    *spec = mSpec;
    return NS_OK;
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
                          mCallbacks,
                          nsIChannel::LOAD_NORMAL,
                          nsnull,
                          &channel);
    if (NS_FAILED(rv)) return rv;

    // this cast is safe because nsFileURL::GetURLString aways
    // returns file: strings, and consequently we'll make nsIFileChannel
    // objects from them:
    *result = NS_STATIC_CAST(nsIFileChannel*, channel);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
