/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsNetUtil_h__
#define nsNetUtil_h__

#include "nsIURI.h"
#include "netCore.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIStreamListener.h"
#include "nsIStreamProvider.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIChannel.h"
#include "nsMemory.h"
#include "nsCOMPtr.h"
#include "nsIHTTPProtocolHandler.h"
#include "nsIDownloader.h"
#include "nsIStreamLoader.h"
#include "nsIStreamIO.h"
#include "nsIPipe.h"
#include "nsXPIDLString.h"
#include "prio.h"       // for read/write flags, permissions, etc.

#include "nsNetCID.h"

// Helper, to simplify getting the I/O service.
inline const nsGetServiceByCID
do_GetIOService(nsresult* error = 0)
{
    static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
    return nsGetServiceByCID(kIOServiceCID, 0, error);
}

inline nsresult
NS_NewURI(nsIURI* *result, 
          const char* spec, 
          nsIURI* baseURI = nsnull,
          nsIIOService* ioService = nsnull)     // pass in nsIIOService to optimize callers
{
    nsresult rv;

    nsCOMPtr<nsIIOService> serv;
    if (ioService == nsnull) {
        serv = do_GetIOService(&rv);
        if (NS_FAILED(rv)) return rv;
        ioService = serv.get();
    }

    return ioService->NewURI(spec, baseURI, result);
}

inline nsresult
NS_NewURI(nsIURI* *result, 
          const nsAReadableString& spec, 
          nsIURI* baseURI = nsnull,
          nsIIOService* ioService = nsnull)     // pass in nsIIOService to optimize callers
{
    char* specStr = ToNewUTF8String(spec); // this forces a single byte char*
    if (specStr == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    nsresult rv = NS_NewURI(result, specStr, baseURI, ioService);
    nsMemory::Free(specStr);
    return rv;
}

inline nsresult
NS_OpenURI(nsIChannel* *result, 
           nsIURI* uri,
           nsIIOService* ioService = nsnull,    // pass in nsIIOService to optimize callers
           nsILoadGroup* loadGroup = nsnull,
           nsIInterfaceRequestor* notificationCallbacks = nsnull,
           nsLoadFlags loadAttributes = NS_STATIC_CAST(nsLoadFlags, nsIChannel::LOAD_NORMAL),
           PRUint32 bufferSegmentSize = 0, 
           PRUint32 bufferMaxSize = 0)
{
    nsresult rv;

    nsCOMPtr<nsIIOService> serv;
    if (ioService == nsnull) {
        serv = do_GetIOService(&rv);
        if (NS_FAILED(rv)) return rv;
        ioService = serv.get();
    }

    nsIChannel* channel = nsnull;
    rv = ioService->NewChannelFromURI(uri, &channel);
    if (NS_FAILED(rv)) return rv;

    if (loadGroup) {
        rv = channel->SetLoadGroup(loadGroup);
        if (NS_FAILED(rv)) return rv;
    }
    if (notificationCallbacks) {
        rv = channel->SetNotificationCallbacks(notificationCallbacks);
        if (NS_FAILED(rv)) return rv;
    }
    if (loadAttributes != nsIChannel::LOAD_NORMAL) {
        rv = channel->SetLoadAttributes(loadAttributes);
        if (NS_FAILED(rv)) return rv;
    }
    if (bufferSegmentSize != 0) {
        rv = channel->SetBufferSegmentSize(bufferSegmentSize);
        if (NS_FAILED(rv)) return rv;
    }
    if (bufferMaxSize != 0) {
        rv = channel->SetBufferMaxSize(bufferMaxSize);
        if (NS_FAILED(rv)) return rv;
    }

    *result = channel;
    return rv;
}

// Use this function with CAUTION. And do not use it on 
// the UI thread. It creates a stream that blocks when
// you Read() from it and blocking the UI thread is
// illegal. If you don't want to implement a full
// blown asyncrhonous consumer (via nsIStreamListener)
// look at nsIStreamLoader instead.
inline nsresult
NS_OpenURI(nsIInputStream* *result,
           nsIURI* uri,
           nsIIOService* ioService = nsnull,     // pass in nsIIOService to optimize callers
           nsILoadGroup* loadGroup = nsnull,
           nsIInterfaceRequestor* notificationCallbacks = nsnull,
           nsLoadFlags loadAttributes = NS_STATIC_CAST(nsLoadFlags, nsIChannel::LOAD_NORMAL),
           PRUint32 bufferSegmentSize = 0, 
           PRUint32 bufferMaxSize = 0)
{
    nsresult rv;
    nsCOMPtr<nsIChannel> channel;

    rv = NS_OpenURI(getter_AddRefs(channel), uri, ioService,
                    loadGroup, notificationCallbacks, loadAttributes,
                    bufferSegmentSize, bufferMaxSize);
    if (NS_FAILED(rv)) return rv;

    nsIInputStream* inStr;
    rv = channel->OpenInputStream(&inStr);
    if (NS_FAILED(rv)) return rv;

    *result = inStr;
    return rv;
}

inline nsresult
NS_OpenURI(nsIStreamListener* aConsumer, 
           nsISupports* context, 
           nsIURI* uri,
           nsIIOService* ioService = nsnull,     // pass in nsIIOService to optimize callers
           nsILoadGroup* loadGroup = nsnull,
           nsIInterfaceRequestor* notificationCallbacks = nsnull,
           nsLoadFlags loadAttributes = NS_STATIC_CAST(nsLoadFlags, nsIChannel::LOAD_NORMAL),
           PRUint32 bufferSegmentSize = 0, 
           PRUint32 bufferMaxSize = 0)
{
    nsresult rv;
    nsCOMPtr<nsIChannel> channel;

    rv = NS_OpenURI(getter_AddRefs(channel), uri, ioService,
                    loadGroup, notificationCallbacks, loadAttributes,
                    bufferSegmentSize, bufferMaxSize);
    if (NS_FAILED(rv)) return rv;

    rv = channel->AsyncRead(aConsumer, context);
    return rv;
}

inline nsresult
NS_MakeAbsoluteURI(char* *result,
                   const char* spec, 
                   nsIURI* baseURI = nsnull, 
                   nsIIOService* ioService = nsnull)     // pass in nsIIOService to optimize callers
{
    NS_ASSERTION(baseURI, "It doesn't make sense to not supply a base URI");
 
    if (spec == nsnull)
        return baseURI->GetSpec(result);
    
    return baseURI->Resolve(spec, result);
}

inline nsresult
NS_MakeAbsoluteURI(nsAWritableString& result,
                   const nsAReadableString& spec, 
                   nsIURI* baseURI = nsnull,
                   nsIIOService* ioService = nsnull)     // pass in nsIIOService to optimize callers
{
    char* resultStr;
    char* specStr = ToNewUTF8String(spec);
    if (!specStr) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    nsresult rv = NS_MakeAbsoluteURI(&resultStr, specStr, baseURI, ioService);
    nsMemory::Free(specStr);
    if (NS_FAILED(rv)) return rv;

    result.Assign(NS_ConvertUTF8toUCS2(resultStr));
   
    nsMemory::Free(resultStr);
    return rv;
}

inline nsresult
NS_NewPostDataStream(nsIInputStream **result,
                     PRBool isFile,
                     const char *data,
                     PRUint32 encodeFlags,
                     nsIIOService* ioService = nsnull)     // pass in nsIIOService to optimize callers
{
    nsresult rv;
    nsCOMPtr<nsIIOService> serv;

    if (ioService == nsnull) {
        serv = do_GetIOService(&rv);
        if (NS_FAILED(rv)) return rv;
        ioService = serv.get();
    }

    nsCOMPtr<nsIProtocolHandler> handler;
    rv = ioService->GetProtocolHandler("http", getter_AddRefs(handler));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIHTTPProtocolHandler> http = do_QueryInterface(handler, &rv);
    if (NS_FAILED(rv)) return rv;
    
    return http->NewPostDataStream(isFile, data, encodeFlags, result);
}

inline nsresult
NS_NewStreamIOChannel(nsIStreamIOChannel **result,
                      nsIURI* uri,
                      nsIStreamIO* io)
{
    nsresult rv;
    nsCOMPtr<nsIStreamIOChannel> channel;
    static NS_DEFINE_CID(kStreamIOChannelCID, NS_STREAMIOCHANNEL_CID);
    rv = nsComponentManager::CreateInstance(kStreamIOChannelCID,
                                            nsnull, 
                                            NS_GET_IID(nsIStreamIOChannel),
                                            getter_AddRefs(channel));
    if (NS_FAILED(rv)) return rv;
    rv = channel->Init(uri, io);
    if (NS_FAILED(rv)) return rv;

    *result = channel;
    NS_ADDREF(*result);
    return NS_OK;
}

inline nsresult
NS_NewInputStreamChannel(nsIChannel **result,
                         nsIURI* uri,
                         nsIInputStream* inStr,
                         const char* contentType,
                         PRInt32 contentLength)
{
    nsresult rv;
    nsXPIDLCString spec;
    rv = uri->GetSpec(getter_Copies(spec));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIInputStreamIO> io;
    rv = NS_NewInputStreamIO(getter_AddRefs(io), spec, inStr, 
                             contentType, contentLength);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStreamIOChannel> channel;
    rv = NS_NewStreamIOChannel(getter_AddRefs(channel), uri, io);
    if (NS_FAILED(rv)) return rv;

    *result = channel;
    NS_ADDREF(*result);
    return NS_OK;
}

inline nsresult
NS_NewLoadGroup(nsILoadGroup* *result, nsIStreamObserver* obs)
{
    nsresult rv;
    nsCOMPtr<nsILoadGroup> group;
    static NS_DEFINE_CID(kLoadGroupCID, NS_LOADGROUP_CID);
    rv = nsComponentManager::CreateInstance(kLoadGroupCID, nsnull, 
                                            NS_GET_IID(nsILoadGroup), 
                                            getter_AddRefs(group));
    if (NS_FAILED(rv)) return rv;
    rv = group->Init(obs);
    if (NS_FAILED(rv)) return rv;

    *result = group;
    NS_ADDREF(*result);
    return NS_OK;
}


inline nsresult
NS_NewDownloader(nsIDownloader* *result,
                   nsIURI* uri,
                   nsIDownloadObserver* observer,
                   nsISupports* context = nsnull,
                   PRBool synchronous = PR_FALSE,
                   nsILoadGroup* loadGroup = nsnull,
                   nsIInterfaceRequestor* notificationCallbacks = nsnull,
                   nsLoadFlags loadAttributes = NS_STATIC_CAST(nsLoadFlags, nsIChannel::LOAD_NORMAL),
                   PRUint32 bufferSegmentSize = 0, 
                   PRUint32 bufferMaxSize = 0)
{
    nsresult rv;
    nsCOMPtr<nsIDownloader> downloader;
    static NS_DEFINE_CID(kDownloaderCID, NS_DOWNLOADER_CID);
    rv = nsComponentManager::CreateInstance(kDownloaderCID,
                                            nsnull,
                                            NS_GET_IID(nsIDownloader),
                                            getter_AddRefs(downloader));
    if (NS_FAILED(rv)) return rv;
    rv = downloader->Init(uri, observer, context, synchronous, loadGroup, notificationCallbacks,
                          loadAttributes, bufferSegmentSize, bufferMaxSize);
    if (NS_FAILED(rv)) return rv;
    *result = downloader;
    NS_ADDREF(*result);
    return rv;
}

inline nsresult
NS_NewStreamLoader(nsIStreamLoader* *result,
                   nsIURI* uri,
                   nsIStreamLoaderObserver* observer,
                   nsISupports* context = nsnull,
                   nsILoadGroup* loadGroup = nsnull,
                   nsIInterfaceRequestor* notificationCallbacks = nsnull,
                   nsLoadFlags loadAttributes = NS_STATIC_CAST(nsLoadFlags, nsIChannel::LOAD_NORMAL),
                   PRUint32 bufferSegmentSize = 0, 
                   PRUint32 bufferMaxSize = 0)
{
    nsresult rv;
    nsCOMPtr<nsIStreamLoader> loader;
    static NS_DEFINE_CID(kStreamLoaderCID, NS_STREAMLOADER_CID);
    rv = nsComponentManager::CreateInstance(kStreamLoaderCID,
                                            nsnull,
                                            NS_GET_IID(nsIStreamLoader),
                                            getter_AddRefs(loader));
    if (NS_FAILED(rv)) return rv;
    rv = loader->Init(uri, observer, context, loadGroup, notificationCallbacks, loadAttributes,
                      bufferSegmentSize, bufferMaxSize);
    if (NS_FAILED(rv)) return rv;
    *result = loader;
    NS_ADDREF(*result);
    return rv;
}

inline nsresult
NS_NewStreamObserverProxy(nsIStreamObserver **aResult,
                          nsIStreamObserver *aObserver,
                          nsIEventQueue *aEventQ=nsnull)
{
    NS_ENSURE_ARG_POINTER(aResult);

    nsresult rv;
    nsCOMPtr<nsIStreamObserverProxy> proxy;
    static NS_DEFINE_CID(kStreamObserverProxyCID, NS_STREAMOBSERVERPROXY_CID);

    rv = nsComponentManager::CreateInstance(kStreamObserverProxyCID,
                                            nsnull,
                                            NS_GET_IID(nsIStreamObserverProxy),
                                            getter_AddRefs(proxy));
    if (NS_FAILED(rv)) return rv;

    rv = proxy->Init(aObserver, aEventQ);
    if (NS_FAILED(rv)) return rv;

    *aResult = proxy;
    NS_ADDREF(*aResult);

    return NS_OK;
}

inline nsresult
NS_NewStreamListenerProxy(nsIStreamListener **aResult,
                          nsIStreamListener *aListener,
                          nsIEventQueue *aEventQ=nsnull,
                          PRUint32 aBufferSegmentSize=0,
                          PRUint32 aBufferMaxSize=0)
{
    NS_ENSURE_ARG_POINTER(aResult);

    nsresult rv;
    nsCOMPtr<nsIStreamListenerProxy> proxy;
    static NS_DEFINE_CID(kStreamListenerProxyCID, NS_STREAMLISTENERPROXY_CID);

    rv = nsComponentManager::CreateInstance(kStreamListenerProxyCID,
                                            nsnull,
                                            NS_GET_IID(nsIStreamListenerProxy),
                                            getter_AddRefs(proxy));
    if (NS_FAILED(rv)) return rv;

    rv = proxy->Init(aListener, aEventQ, aBufferSegmentSize, aBufferMaxSize);
    if (NS_FAILED(rv)) return rv;

    *aResult = proxy;
    NS_ADDREF(*aResult);

    return NS_OK;
}

inline nsresult
NS_NewStreamProviderProxy(nsIStreamProvider **aResult,
                          nsIStreamProvider *aProvider,
                          nsIEventQueue *aEventQ=nsnull,
                          PRUint32 aBufferSegmentSize=0,
                          PRUint32 aBufferMaxSize=0)
{
    NS_ENSURE_ARG_POINTER(aResult);

    nsresult rv;
    nsCOMPtr<nsIStreamProviderProxy> proxy;
    static NS_DEFINE_CID(kStreamProviderProxyCID, NS_STREAMPROVIDERPROXY_CID);

    rv = nsComponentManager::CreateInstance(kStreamProviderProxyCID,
                                            nsnull,
                                            NS_GET_IID(nsIStreamProviderProxy),
                                            getter_AddRefs(proxy));
    if (NS_FAILED(rv)) return rv;

    rv = proxy->Init(aProvider, aEventQ, aBufferSegmentSize, aBufferMaxSize);
    if (NS_FAILED(rv)) return rv;

    *aResult = proxy;
    NS_ADDREF(*aResult);

    return NS_OK;
}

inline nsresult
NS_NewSimpleStreamListener(nsIStreamListener **aResult,
                           nsIOutputStream *aSink,
                           nsIStreamObserver *aObserver=nsnull)
{
    NS_ENSURE_ARG_POINTER(aResult);

    nsresult rv;
    nsCOMPtr<nsISimpleStreamListener> listener;
    static NS_DEFINE_CID(kSimpleStreamListenerCID, NS_SIMPLESTREAMLISTENER_CID);
    rv = nsComponentManager::CreateInstance(kSimpleStreamListenerCID,
                                            nsnull,
                                            NS_GET_IID(nsISimpleStreamListener),
                                            getter_AddRefs(listener));
    if (NS_FAILED(rv)) return rv;

    rv = listener->Init(aSink, aObserver);
    if (NS_FAILED(rv)) return rv;

    *aResult = listener.get();
    NS_ADDREF(*aResult);

    return NS_OK;
}

inline nsresult
NS_NewSimpleStreamProvider(nsIStreamProvider **aResult,
                           nsIInputStream *aSource,
                           nsIStreamObserver *aObserver=nsnull)
{
    NS_ENSURE_ARG_POINTER(aResult);

    nsresult rv;
    nsCOMPtr<nsISimpleStreamProvider> provider;
    static NS_DEFINE_CID(kSimpleStreamProviderCID, NS_SIMPLESTREAMPROVIDER_CID);
    rv = nsComponentManager::CreateInstance(kSimpleStreamProviderCID,
                                            nsnull,
                                            NS_GET_IID(nsISimpleStreamProvider),
                                            getter_AddRefs(provider));
    if (NS_FAILED(rv)) return rv;

    rv = provider->Init(aSource, aObserver);
    if (NS_FAILED(rv)) return rv;

    *aResult = provider.get();
    NS_ADDREF(*aResult);

    return NS_OK;
}

// Depracated, prefer NS_NewStreamObserverProxy
inline nsresult
NS_NewAsyncStreamObserver(nsIStreamObserver **result,
                          nsIStreamObserver *receiver,
                          nsIEventQueue *eventQueue)
{
    nsresult rv;
    nsCOMPtr<nsIAsyncStreamObserver> obs;
    static NS_DEFINE_CID(kAsyncStreamObserverCID, NS_ASYNCSTREAMOBSERVER_CID);
    rv = nsComponentManager::CreateInstance(kAsyncStreamObserverCID,
                                            nsnull, 
                                            NS_GET_IID(nsIAsyncStreamObserver),
                                            getter_AddRefs(obs));
    if (NS_FAILED(rv)) return rv;
    rv = obs->Init(receiver, eventQueue);
    if (NS_FAILED(rv)) return rv;

    *result = obs;
    NS_ADDREF(*result);
    return NS_OK;
}

// Depracated, prefer NS_NewStreamListenerProxy
inline nsresult
NS_NewAsyncStreamListener(nsIStreamListener **result,
                          nsIStreamListener *receiver,
                          nsIEventQueue *eventQueue)
{
    nsresult rv;
    nsCOMPtr<nsIAsyncStreamListener> lsnr;
    static NS_DEFINE_CID(kAsyncStreamListenerCID, NS_ASYNCSTREAMLISTENER_CID);
    rv = nsComponentManager::CreateInstance(kAsyncStreamListenerCID,
                                            nsnull, 
                                            NS_GET_IID(nsIAsyncStreamListener),
                                            getter_AddRefs(lsnr));
    if (NS_FAILED(rv)) return rv;
    rv = lsnr->Init(receiver, eventQueue);
    if (NS_FAILED(rv)) return rv;

    *result = lsnr;
    NS_ADDREF(*result);
    return NS_OK;
}

// Depracated, prefer a true synchonous implementation
inline nsresult
NS_NewSyncStreamListener(nsIInputStream **aInStream, 
                         nsIOutputStream **aOutStream,
                         nsIStreamListener **aResult)
{
    nsresult rv;

    NS_ENSURE_ARG_POINTER(aInStream);
    NS_ENSURE_ARG_POINTER(aOutStream);

    nsCOMPtr<nsIInputStream> pipeIn;
    nsCOMPtr<nsIOutputStream> pipeOut;

    rv = NS_NewPipe(getter_AddRefs(pipeIn),
                    getter_AddRefs(pipeOut),
                    4*1024,   // NS_SYNC_STREAM_LISTENER_SEGMENT_SIZE
                    32*1024); // NS_SYNC_STREAM_LISTENER_BUFFER_SIZE
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewSimpleStreamListener(aResult, pipeOut);
    if (NS_FAILED(rv)) return rv;

    *aInStream = pipeIn;
    NS_ADDREF(*aInStream);
    *aOutStream = pipeOut;
    NS_ADDREF(*aOutStream);

    return NS_OK;
}

//
// Calls AsyncWrite on the specified channel, with a stream provider that
// reads data from the specified input stream.
//
inline nsresult
NS_AsyncWriteFromStream(nsIChannel *aChannel,
                        nsIInputStream *aSource,
                        nsIStreamObserver *aObserver=NULL,
                        nsISupports *aContext=NULL)
{
    NS_ENSURE_ARG_POINTER(aChannel);

    nsresult rv;
    nsCOMPtr<nsIStreamProvider> provider;
    rv = NS_NewSimpleStreamProvider(getter_AddRefs(provider),
                                    aSource,
                                    aObserver);
    if (NS_FAILED(rv)) return rv;

    return aChannel->AsyncWrite(provider, aContext);
}

//
// Calls AsyncRead on the specified channel, with a stream listener that
// writes data to the specified output stream.
//
inline nsresult
NS_AsyncReadToStream(nsIChannel *aChannel,
                     nsIOutputStream *aSink,
                     nsIStreamObserver *aObserver=NULL,
                     nsISupports *aContext=NULL)
{
    NS_ENSURE_ARG_POINTER(aChannel);

    nsresult rv;
    nsCOMPtr<nsIStreamListener> listener;
    rv = NS_NewSimpleStreamListener(getter_AddRefs(listener),
                                    aSink,
                                    aObserver);
    if (NS_FAILED(rv)) return rv;

    return aChannel->AsyncRead(listener, aContext);
}

#endif // nsNetUtil_h__
