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
#include "nsIStreamListener.h"
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
#include "nsIStreamLoader.h"
#include "nsIStreamIO.h"
#include "nsXPIDLString.h"
#include "prio.h"       // for read/write flags, permissions, etc.

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
           nsLoadFlags loadAttributes = nsIChannel::LOAD_NORMAL,
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
           nsLoadFlags loadAttributes = nsIChannel::LOAD_NORMAL,
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
           nsLoadFlags loadAttributes = nsIChannel::LOAD_NORMAL,
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
NS_NewStreamLoader(nsIStreamLoader* *result,
                   nsIURI* uri,
                   nsIStreamLoaderObserver* observer,
                   nsISupports* context = nsnull,
                   nsILoadGroup* loadGroup = nsnull,
                   nsIInterfaceRequestor* notificationCallbacks = nsnull,
                   nsLoadFlags loadAttributes = nsIChannel::LOAD_NORMAL,
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

inline nsresult
NS_NewSyncStreamListener(nsIInputStream **inStream, 
                         nsIOutputStream **outStream,
                         nsIStreamListener **listener)
{
    nsresult rv;
    nsCOMPtr<nsISyncStreamListener> lsnr;
    static NS_DEFINE_CID(kSyncStreamListenerCID, NS_SYNCSTREAMLISTENER_CID);
    rv = nsComponentManager::CreateInstance(kSyncStreamListenerCID,
                                            nsnull, 
                                            NS_GET_IID(nsISyncStreamListener),
                                            getter_AddRefs(lsnr));
    if (NS_FAILED(rv)) return rv;
    rv = lsnr->Init(inStream, outStream);
    if (NS_FAILED(rv)) return rv;

    *listener = lsnr;
    NS_ADDREF(*listener);
    return NS_OK;
}

#endif // nsNetUtil_h__
