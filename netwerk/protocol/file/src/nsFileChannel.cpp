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
#include "nsEscape.h"
#include "nsIMIMEService.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "nsIFileTransportService.h"

static NS_DEFINE_CID(kMIMEServiceCID, NS_MIMESERVICE_CID);
static NS_DEFINE_CID(kEventQueueService, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);

#ifdef STREAM_CONVERTER_HACK
#include "nsIStreamConverter.h"
#include "nsIAllocator.h"
#endif

////////////////////////////////////////////////////////////////////////////////

nsFileChannel::nsFileChannel()
    : mLoadAttributes(LOAD_NORMAL),
      mRealListener(nsnull)
{
    NS_INIT_REFCNT();
}

nsresult
nsFileChannel::Init(nsIFileProtocolHandler* handler, const char* command, nsIURI* uri,
                    nsILoadGroup *aGroup, nsIEventSinkGetter* getter)
{
    nsresult rv;

    mGetter = getter;
    mHandler = handler;
    mURI = uri;
    mCommand = nsCRT::strdup(command);
    if (mCommand == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    mLoadGroup = aGroup;
    if (mLoadGroup) {
        rv = mLoadGroup->GetDefaultLoadAttributes(&mLoadAttributes);
        if (NS_FAILED(rv)) return rv;
    }

    // if we support the nsIURL interface then use it to get just
    // the file path with no other garbage!
    nsCOMPtr<nsIURL> aUrl = do_QueryInterface(mURI, &rv);
    if (NS_SUCCEEDED(rv) && aUrl) { // does it support the url interface?
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
}

NS_IMETHODIMP
nsFileChannel::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    NS_ASSERTION(aInstancePtr, "no instance pointer");
    if (aIID.Equals(NS_GET_IID(nsIFileChannel)) ||
        aIID.Equals(NS_GET_IID(nsIChannel)) ||
        aIID.Equals(NS_GET_IID(nsIRequest)) ||
        aIID.Equals(NS_GET_IID(nsISupports))) {
        *aInstancePtr = NS_STATIC_CAST(nsIFileChannel*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(NS_GET_IID(nsIStreamListener)) ||
        aIID.Equals(NS_GET_IID(nsIStreamObserver))) {
        *aInstancePtr = NS_STATIC_CAST(nsIStreamListener*, this);
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

    rv = fts->CreateTransport(mSpec, mCommand, mGetter,
                              getter_AddRefs(mFileTransport));
    if (NS_FAILED(rv)) return rv;

    return mFileTransport->OpenInputStream(startPosition, readCount, result);
}

NS_IMETHODIMP
nsFileChannel::OpenOutputStream(PRUint32 startPosition, nsIOutputStream **result)
{
    nsresult rv;

    if (mFileTransport)
        return NS_ERROR_IN_PROGRESS;

    NS_WITH_SERVICE(nsIFileTransportService, fts, kFileTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = fts->CreateTransport(mSpec, mCommand, mGetter,
                              getter_AddRefs(mFileTransport));
    if (NS_FAILED(rv)) return rv;

    return mFileTransport->OpenOutputStream(startPosition, result);
}

NS_IMETHODIMP
nsFileChannel::AsyncRead(PRUint32 startPosition, PRInt32 readCount,
                         nsISupports *ctxt,
                         nsIStreamListener *listener)
{
    nsresult rv;

    if (mFileTransport)
        return NS_ERROR_IN_PROGRESS;

    // mscott --  this is just one temporary hack until we have a legit stream converter
    // story going....if the file we are opening is an rfc822 file then we want to 
    // go out and convert the data into html before we try to load it. so I'm inserting
    // code which if we are rfc-822 will cause us to literally insert a converter between
    // the file channel stream of incoming data and the consumer at the other end of the
    // AsyncRead call...
    mRealListener = listener;
    nsCOMPtr<nsIStreamListener> tempListener;
#ifdef STREAM_CONVERTER_HACK
    nsXPIDLCString aContentType;

    rv = GetContentType(getter_Copies(aContentType));
    if (NS_SUCCEEDED(rv) && nsCRT::strcasecmp("message/rfc822", aContentType) == 0)
    {
        // okay we are an rfc822 message...
        // (0) Create an instance of an RFC-822 stream converter...
        // because I need this converter to be around for the lifetime of the channel,
        // I'm making it a member variable.
        // (1) create a proxied stream listener for the caller of this method
        // (2) set this proxied listener as the listener on the output stream
        // (3) create a proxied stream listener for the converter
        // (4) set tempListener to be the stream converter's listener.

        // (0) create a stream converter
        // mscott - we could generalize this hack to work with other stream converters by simply
        // using the content type of the file to generate a progid for a stream converter and use
        // that instead of a class id...
        if (!mStreamConverter) {
            rv = nsComponentManager::CreateInstance(NS_ISTREAMCONVERTER_KEY "?from=message/rfc822?to=text/xul", 
                                                    NULL, NS_GET_IID(nsIStreamConverter), 
                                                    (void **) getter_AddRefs(mStreamConverter)); 
            if (NS_FAILED(rv)) return rv;
        }

        // (3) set the stream converter as the listener on the channel
        tempListener = mStreamConverter;

        mStreamConverter->AsyncConvertData(nsnull, nsnull, this, (nsIChannel *) this);
        mStreamConverterOutType = "text/xul";
    }
    else
        tempListener = this;
#else   
    tempListener = this;
#endif

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

    rv = fts->CreateTransport(mSpec, mCommand, mGetter,
                              getter_AddRefs(mFileTransport));
    if (NS_FAILED(rv)) return rv;

    return mFileTransport->AsyncRead(startPosition, readCount, ctxt, tempListener);
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

    rv = fts->CreateTransport(mSpec, mCommand, mGetter,
                              getter_AddRefs(mFileTransport));
    if (NS_FAILED(rv)) return rv;

    return mFileTransport->AsyncWrite(fromStream, startPosition, writeCount, ctxt, observer);
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
        *aContentType = mStreamConverterOutType.ToNewCString();
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
nsFileChannel::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
    *aLoadGroup = mLoadGroup;
    NS_IF_ADDREF(*aLoadGroup);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::GetOwner(nsISupports * *aOwner)
{
    *aOwner = mOwner.get();
    NS_IF_ADDREF(*aOwner);
    return NS_OK;
}

NS_IMETHODIMP
nsFileChannel::SetOwner(nsISupports * aOwner)
{
    mOwner = aOwner;
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
    return mRealListener->OnDataAvailable(this, context, aIStream,
                                          aSourceOffset, aLength);
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

////////////////////////////////////////////////////////////////////////////////
