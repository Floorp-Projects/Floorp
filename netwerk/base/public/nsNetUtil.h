/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Bradley Baetz <bbaetz@student.usyd.edu.au>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsNetUtil_h__
#define nsNetUtil_h__

#include "nsNetError.h"
#include "nsNetCID.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsMemory.h"
#include "nsCOMPtr.h"
#include "prio.h" // for read/write flags, permissions, etc.

#include "nsIURI.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIStreamListener.h"
#include "nsIRequestObserverProxy.h"
#include "nsIStreamListenerProxy.h" // XXX for nsIAsyncStreamListener
#include "nsISimpleStreamListener.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIChannel.h"
#include "nsIInputStreamChannel.h"
#include "nsITransport.h"
#include "nsIHttpChannel.h"
#include "nsIDownloader.h"
#include "nsIResumableEntityID.h"
#include "nsIStreamLoader.h"
#include "nsIUnicharStreamLoader.h"
#include "nsIPipe.h"
#include "nsIProtocolHandler.h"
#include "nsIFileProtocolHandler.h"
#include "nsIStringStream.h"
#include "nsILocalFile.h"
#include "nsIFileStreams.h"
#include "nsIProtocolProxyService.h"
#include "nsIProxyInfo.h"
#include "nsIFileStreams.h"
#include "nsIBufferedStreams.h"
#include "nsIInputStreamPump.h"
#include "nsIAsyncStreamCopier.h"

// Helper, to simplify getting the I/O service.
inline const nsGetServiceByCID
do_GetIOService(nsresult* error = 0)
{
    static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
    return nsGetServiceByCID(kIOServiceCID, 0, error);
}

inline nsresult
NS_NewURI(nsIURI **result, 
          const nsACString &spec, 
          const char *charset = nsnull,
          nsIURI *baseURI = nsnull,
          nsIIOService *ioService = nsnull)     // pass in nsIIOService to optimize callers
{
    nsresult rv;

    nsCOMPtr<nsIIOService> serv;
    if (ioService == nsnull) {
        serv = do_GetIOService(&rv);
        if (NS_FAILED(rv)) return rv;
        ioService = serv.get();
    }

    return ioService->NewURI(spec, charset, baseURI, result);
}

inline nsresult
NS_NewURI(nsIURI* *result, 
          const nsAString& spec, 
          const char *charset = nsnull,
          nsIURI* baseURI = nsnull,
          nsIIOService* ioService = nsnull)     // pass in nsIIOService to optimize callers
{
    return NS_NewURI(result, NS_ConvertUCS2toUTF8(spec), charset, baseURI, ioService);
}

inline nsresult
NS_NewURI(nsIURI* *result, 
          const char *spec,
          nsIURI* baseURI = nsnull,
          nsIIOService* ioService = nsnull)     // pass in nsIIOService to optimize callers
{
    return NS_NewURI(result, nsDependentCString(spec), nsnull, baseURI, ioService);
}

inline nsresult
NS_NewFileURI(nsIURI* *result, 
              nsIFile* spec, 
              nsIIOService* ioService = nsnull)     // pass in nsIIOService to optimize callers
{
    nsresult rv;

    nsCOMPtr<nsIIOService> serv;
    if (ioService == nsnull) {
        serv = do_GetIOService(&rv);
        if (NS_FAILED(rv)) return rv;
        ioService = serv.get();
    }

    return ioService->NewFileURI(spec, result);
}

inline nsresult
NS_NewChannel(nsIChannel* *result, 
              nsIURI* uri,
              nsIIOService* ioService = nsnull,    // pass in nsIIOService to optimize callers
              nsILoadGroup* loadGroup = nsnull,
              nsIInterfaceRequestor* notificationCallbacks = nsnull,
              nsLoadFlags loadAttributes = NS_STATIC_CAST(nsLoadFlags, nsIRequest::LOAD_NORMAL))
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
    if (loadAttributes != nsIRequest::LOAD_NORMAL) {
        rv = channel->SetLoadFlags(loadAttributes);
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
           nsLoadFlags loadAttributes = NS_STATIC_CAST(nsLoadFlags, nsIRequest::LOAD_NORMAL))
{
    nsresult rv;
    nsCOMPtr<nsIChannel> channel;

    rv = NS_NewChannel(getter_AddRefs(channel), uri, ioService,
                       loadGroup, notificationCallbacks, loadAttributes);
    if (NS_FAILED(rv)) return rv;

    nsIInputStream* inStr;
    rv = channel->Open(&inStr);
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
           nsLoadFlags loadAttributes = NS_STATIC_CAST(nsLoadFlags, nsIRequest::LOAD_NORMAL))
{
    nsresult rv;
    nsCOMPtr<nsIChannel> channel;

    rv = NS_NewChannel(getter_AddRefs(channel), uri, ioService,
                       loadGroup, notificationCallbacks, loadAttributes);
    if (NS_FAILED(rv)) return rv;

    rv = channel->AsyncOpen(aConsumer, context);
    return rv;
}

inline nsresult
NS_MakeAbsoluteURI(nsACString &result,
                   const nsACString &spec, 
                   nsIURI *baseURI, 
                   nsIIOService *ioService = nsnull)     // pass in nsIIOService to optimize callers
{
    if (!baseURI) {
        NS_WARNING("It doesn't make sense to not supply a base URI");
        result = spec;
        return NS_OK;
    }

    if (spec.IsEmpty())
        return baseURI->GetSpec(result);

    return baseURI->Resolve(spec, result);
}

inline nsresult
NS_MakeAbsoluteURI(char **result,
                   const char *spec, 
                   nsIURI *baseURI, 
                   nsIIOService *ioService = nsnull)     // pass in nsIIOService to optimize callers
{
    nsCAutoString resultBuf;

    nsresult rv = NS_MakeAbsoluteURI(resultBuf, nsDependentCString(spec), baseURI, ioService);
    if (NS_FAILED(rv)) return rv;

    *result = ToNewCString(resultBuf);
    return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

inline nsresult
NS_MakeAbsoluteURI(nsAString &result,
                   const nsAString &spec, 
                   nsIURI *baseURI,
                   nsIIOService *ioService = nsnull)     // pass in nsIIOService to optimize callers
{
    if (!baseURI) {
        NS_WARNING("It doesn't make sense to not supply a base URI");
        result = spec;
        return NS_OK;
    }

    nsCAutoString resultBuf;
    nsresult rv;

    if (spec.IsEmpty())
        rv = baseURI->GetSpec(resultBuf);
    else
        rv = baseURI->Resolve(NS_ConvertUCS2toUTF8(spec), resultBuf);
    if (NS_FAILED(rv)) return rv;

    result = NS_ConvertUTF8toUCS2(resultBuf); // XXX CopyUTF8toUCS2
    return NS_OK;
}

inline nsresult
NS_NewInputStreamChannel(nsIChannel **result,
                         nsIURI *uri,
                         nsIInputStream *stream,
                         const nsACString &contentType = NS_LITERAL_CSTRING(""),
                         const nsACString &contentCharset = NS_LITERAL_CSTRING(""))
{
    nsresult rv;

    static NS_DEFINE_CID(kInputStreamChannelCID, NS_INPUTSTREAMCHANNEL_CID);
    nsCOMPtr<nsIInputStreamChannel> channel =
        do_CreateInstance(kInputStreamChannelCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = channel->SetURI(uri);
    if (NS_FAILED(rv)) return rv;
    rv = channel->SetContentStream(stream);
    if (NS_FAILED(rv)) return rv;
    rv = channel->SetContentType(contentType);
    if (NS_FAILED(rv)) return rv;
    rv = channel->SetContentCharset(contentCharset);
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*result = channel);
    return NS_OK;
}

inline nsresult
NS_NewInputStreamPump(nsIInputStreamPump **result,
                      nsIInputStream *stream,
                      PRInt32 streamPos = -1,
                      PRInt32 streamLen = -1,
                      PRUint32 segsize = 0,
                      PRUint32 segcount = 0,
                      PRBool closeWhenDone = PR_FALSE)
{
    nsresult rv;

    static NS_DEFINE_CID(kInputStreamPumpCID, NS_INPUTSTREAMPUMP_CID);
    nsCOMPtr<nsIInputStreamPump> pump =
        do_CreateInstance(kInputStreamPumpCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = pump->Init(stream, streamPos, streamLen,
                    segsize, segcount, closeWhenDone);
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*result = pump);
    return NS_OK;
}

// NOTE: you can optimize the copy by specifying whether or not your streams
// are buffered (i.e., do they implement ReadSegments/WriteSegments).  the
// default assumption of FALSE for both streams is OK, but the copy is much
// more efficient if one of the streams is buffered.
inline nsresult
NS_NewAsyncStreamCopier(nsIAsyncStreamCopier **result,
                        nsIInputStream *source,
                        nsIOutputStream *sink,
                        PRBool sourceBuffered = PR_FALSE,
                        PRBool sinkBuffered = PR_FALSE,
                        PRUint32 chunkSize = 0)
{
    nsresult rv;

    static NS_DEFINE_CID(kAsyncStreamCopierCID, NS_ASYNCSTREAMCOPIER_CID);
    nsCOMPtr<nsIAsyncStreamCopier> copier =
        do_CreateInstance(kAsyncStreamCopierCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = copier->Init(source, sink, sourceBuffered, sinkBuffered, chunkSize);
    if (NS_FAILED(rv)) return rv; 
    
    NS_ADDREF(*result = copier);
    return NS_OK;
}

inline nsresult
NS_NewLoadGroup(nsILoadGroup* *result, nsIRequestObserver* obs)
{
    nsresult rv;
    nsCOMPtr<nsILoadGroup> group;
    static NS_DEFINE_CID(kLoadGroupCID, NS_LOADGROUP_CID);
    rv = nsComponentManager::CreateInstance(kLoadGroupCID, nsnull, 
                                            NS_GET_IID(nsILoadGroup), 
                                            getter_AddRefs(group));
    if (NS_FAILED(rv)) return rv;
    rv = group->SetGroupObserver(obs);
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
                   nsLoadFlags loadAttributes = NS_STATIC_CAST(nsLoadFlags, nsIRequest::LOAD_NORMAL))
{
    nsresult rv;
    nsCOMPtr<nsIDownloader> downloader;
    static NS_DEFINE_CID(kDownloaderCID, NS_DOWNLOADER_CID);
    rv = nsComponentManager::CreateInstance(kDownloaderCID,
                                            nsnull,
                                            NS_GET_IID(nsIDownloader),
                                            getter_AddRefs(downloader));
    if (NS_FAILED(rv)) return rv;
    rv = downloader->Init(uri, observer, context, synchronous, loadGroup,
                          notificationCallbacks, loadAttributes);
    if (NS_FAILED(rv)) return rv;
    *result = downloader;
    NS_ADDREF(*result);
    return rv;
}

inline nsresult
NS_NewStreamLoader(nsIStreamLoader **aResult,
                   nsIChannel *aChannel,
                   nsIStreamLoaderObserver *aObserver,
                   nsISupports *aContext)
{
    nsresult rv;
    nsCOMPtr<nsIStreamLoader> loader;
    static NS_DEFINE_CID(kStreamLoaderCID, NS_STREAMLOADER_CID);
    rv = nsComponentManager::CreateInstance(kStreamLoaderCID,
                                            nsnull,
                                            NS_GET_IID(nsIStreamLoader),
                                            getter_AddRefs(loader));
    if (NS_FAILED(rv)) return rv;

    rv = loader->Init(aChannel, aObserver, aContext);
    if (NS_FAILED(rv)) return rv;

    *aResult = loader;
    NS_ADDREF(*aResult);
    return rv;
}

inline nsresult
NS_NewStreamLoader(nsIStreamLoader* *result,
                   nsIURI* uri,
                   nsIStreamLoaderObserver* observer,
                   nsISupports* context = nsnull,
                   nsILoadGroup* loadGroup = nsnull,
                   nsIInterfaceRequestor* notificationCallbacks = nsnull,
                   nsLoadFlags loadAttributes = NS_STATIC_CAST(nsLoadFlags, nsIRequest::LOAD_NORMAL),
                   nsIURI* referrer = nsnull)
{
    nsresult rv;

    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewChannel(getter_AddRefs(channel),
                       uri,
                       nsnull,
                       loadGroup,
                       notificationCallbacks,
                       loadAttributes);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
    if (httpChannel)
        httpChannel->SetReferrer(referrer);

    return NS_NewStreamLoader(result, channel, observer, context);
}

inline nsresult
NS_NewUnicharStreamLoader(nsIUnicharStreamLoader **aResult,
                          nsIChannel *aChannel,
                          nsIUnicharStreamLoaderObserver *aObserver,
                          nsISupports *aContext = nsnull,
                          PRUint32 aSegmentSize = nsIUnicharStreamLoader::DEFAULT_SEGMENT_SIZE)
{
    nsresult rv;
    nsCOMPtr<nsIUnicharStreamLoader> loader;
    static NS_DEFINE_CID(kUnicharStreamLoaderCID, NS_UNICHARSTREAMLOADER_CID);
    rv = nsComponentManager::CreateInstance(kUnicharStreamLoaderCID,
                                            nsnull,
                                            NS_GET_IID(nsIUnicharStreamLoader),
                                            getter_AddRefs(loader));
    if (NS_FAILED(rv)) return rv;
    rv = loader->Init(aChannel, aObserver, aContext, aSegmentSize);
                      
    if (NS_FAILED(rv)) return rv;
    *aResult = loader;
    NS_ADDREF(*aResult);
    return rv;
}

inline nsresult
NS_NewRequestObserverProxy(nsIRequestObserver **aResult,
                           nsIRequestObserver *aObserver,
                           nsIEventQueue *aEventQ=nsnull)
{
    NS_ENSURE_ARG_POINTER(aResult);

    nsresult rv;
    nsCOMPtr<nsIRequestObserverProxy> proxy;
    static NS_DEFINE_CID(kRequestObserverProxyCID, NS_REQUESTOBSERVERPROXY_CID);

    rv = nsComponentManager::CreateInstance(kRequestObserverProxyCID,
                                            nsnull,
                                            NS_GET_IID(nsIRequestObserverProxy),
                                            getter_AddRefs(proxy));
    if (NS_FAILED(rv)) return rv;

    rv = proxy->Init(aObserver, aEventQ);
    if (NS_FAILED(rv)) return rv;

    return CallQueryInterface(proxy, aResult);
}

inline nsresult
NS_NewSimpleStreamListener(nsIStreamListener **aResult,
                           nsIOutputStream *aSink,
                           nsIRequestObserver *aObserver=nsnull)
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

    NS_ADDREF(*aResult = listener);
    return NS_OK;
}

// Deprecated, prefer NS_NewStreamListenerProxy
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

    NS_ADDREF(*result = lsnr);
    return NS_OK;
}

inline nsresult
NS_CheckPortSafety(PRInt32 port, const char* scheme, nsIIOService* ioService = nsnull)
{
    nsresult rv;

    nsCOMPtr<nsIIOService> serv;
    if (ioService == nsnull) {
        serv = do_GetIOService(&rv);
        if (NS_FAILED(rv)) return rv;
        ioService = serv.get();
    }

    PRBool allow;
    
    rv = ioService->AllowPort(port, scheme, &allow);
    if (NS_FAILED(rv)) {
        NS_ERROR("NS_CheckPortSafety: ioService->AllowPort failed\n");
        return rv;
    }
    
    if (!allow)
        return NS_ERROR_PORT_ACCESS_NOT_ALLOWED;

    return NS_OK;
}

inline nsresult
NS_NewProxyInfo(const char* type, const char* host, PRInt32 port, nsIProxyInfo* *result)
{
    nsresult rv;

    static NS_DEFINE_CID(kPPSServiceCID, NS_PROTOCOLPROXYSERVICE_CID);
    nsCOMPtr<nsIProtocolProxyService> pps = do_GetService(kPPSServiceCID,&rv);

    if (NS_FAILED(rv)) return rv;

    return pps->NewProxyInfo(type, host, port, result);
}

inline nsresult
NS_GetFileProtocolHandler(nsIFileProtocolHandler **result,
                          nsIIOService *ioService=nsnull)
{
    nsresult rv;

    nsCOMPtr<nsIIOService> serv;
    if (ioService == nsnull) {
        serv = do_GetIOService(&rv);
        if (NS_FAILED(rv)) return rv;
        ioService = serv.get();
    }

    nsCOMPtr<nsIProtocolHandler> handler;
    rv = ioService->GetProtocolHandler("file", getter_AddRefs(handler));
    if (NS_FAILED(rv)) return rv;

    return CallQueryInterface(handler, result);
}

inline nsresult
NS_GetFileFromURLSpec(const nsACString &inURL, nsIFile **result,
                      nsIIOService *ioService=nsnull)
{
    nsCOMPtr<nsIFileProtocolHandler> fileHandler;
    nsresult rv = NS_GetFileProtocolHandler(getter_AddRefs(fileHandler), ioService);
    if (NS_FAILED(rv)) return rv;

    return fileHandler->GetFileFromURLSpec(inURL, result);
}

inline nsresult
NS_GetURLSpecFromFile(nsIFile* aFile, nsACString &aUrl,
                      nsIIOService *ioService=nsnull)
{
    nsCOMPtr<nsIFileProtocolHandler> fileHandler;
    nsresult rv = NS_GetFileProtocolHandler(getter_AddRefs(fileHandler), ioService);
    if (NS_FAILED(rv)) return rv;

    return fileHandler->GetURLSpecFromFile(aFile, aUrl);
}

inline nsresult
NS_NewResumableEntityID(nsIResumableEntityID** aRes,
                        PRUint32 size,
                        PRTime lastModified)
{
    nsresult rv;
    nsCOMPtr<nsIResumableEntityID> ent =
        do_CreateInstance(NS_RESUMABLEENTITYID_CONTRACTID,&rv);
    if (NS_FAILED(rv)) return rv;

    ent->SetSize(size);
    ent->SetLastModified(lastModified);

    *aRes = ent;
    NS_ADDREF(*aRes);
    return NS_OK;
}

inline nsresult
NS_ExamineForProxy(const char* scheme, const char* host, PRInt32 port, 
                   nsIProxyInfo* *proxyInfo)
{
    nsresult rv;

    static NS_DEFINE_CID(kPPSServiceCID, NS_PROTOCOLPROXYSERVICE_CID);
    nsCOMPtr<nsIProtocolProxyService> pps = do_GetService(kPPSServiceCID,&rv);

    if (NS_FAILED(rv)) return rv;

    nsCAutoString spec(scheme);
    spec.Append("://");
    spec.Append(host);
    spec.Append(':');
    spec.AppendInt(port);
    
    // XXXXX - Under no circumstances whatsoever should any code which
    // wants a uri do this. I do this here because I do not, in fact,
    // actually want a uri (the dummy uris created here may not be 
    // syntactically valid for the specific protocol), and all we need
    // is something which has a valid scheme, hostname, and a string
    // to pass to PAC if needed - bbaetz
    static NS_DEFINE_CID(kSTDURLCID, NS_STANDARDURL_CID);    
    nsCOMPtr<nsIURI> uri = do_CreateInstance(kSTDURLCID, &rv);
    if (NS_FAILED(rv)) return rv;
    rv = uri->SetSpec(spec);
    if (NS_FAILED(rv)) return rv;

    return pps->ExamineForProxy(uri, proxyInfo);
}

inline nsresult
NS_ParseContentType(const nsACString &rawContentType,
                    nsCString &contentType,
                    nsCString &contentCharset)
{
    // contentCharset is left untouched if not present in rawContentType
    nsACString::const_iterator begin, it, end;
    it = rawContentType.BeginReading(begin);
    rawContentType.EndReading(end);
    if (FindCharInReadable(';', it, end)) {
        contentType = Substring(begin, it);
        // now look for "charset=FOO" and extract "FOO"
        begin = ++it;
        if (FindInReadable(NS_LITERAL_CSTRING("charset="), begin, it = end)) {
            contentCharset = Substring(it, end);
            contentCharset.StripWhitespace();
        }
    }
    else
        contentType = rawContentType;
    ToLowerCase(contentType);
    contentType.StripWhitespace();
    return NS_OK;
}

inline nsresult
NS_NewLocalFileInputStream(nsIInputStream** aResult,
                           nsIFile* aFile,
                           PRInt32 aIOFlags = -1,
                           PRInt32 aPerm = -1,
                           PRInt32 aBehaviorFlags = 0)
{
    nsresult rv;
    nsCOMPtr<nsIFileInputStream> in;
    static NS_DEFINE_CID(kLocalFileInputStreamCID, NS_LOCALFILEINPUTSTREAM_CID);
    rv = nsComponentManager::CreateInstance(kLocalFileInputStreamCID,
                                            nsnull, 
                                            NS_GET_IID(nsIFileInputStream),
                                            getter_AddRefs(in));
    if (NS_FAILED(rv)) return rv;
    rv = in->Init(aFile, aIOFlags, aPerm, aBehaviorFlags);
    if (NS_FAILED(rv)) return rv;

    *aResult = in;
    NS_ADDREF(*aResult);
    return NS_OK;
}

inline nsresult
NS_NewLocalFileOutputStream(nsIOutputStream** aResult,
                            nsIFile* aFile,
                            PRInt32 aIOFlags = -1,
                            PRInt32 aPerm = -1,
                            PRInt32 aBehaviorFlags = 0)
{
    nsresult rv;
    nsCOMPtr<nsIFileOutputStream> out;
    static NS_DEFINE_CID(kLocalFileOutputStreamCID, NS_LOCALFILEOUTPUTSTREAM_CID);
    rv = nsComponentManager::CreateInstance(kLocalFileOutputStreamCID,
                                            nsnull, 
                                            NS_GET_IID(nsIFileOutputStream),
                                            getter_AddRefs(out));
    if (NS_FAILED(rv)) return rv;
    rv = out->Init(aFile, aIOFlags, aPerm, aBehaviorFlags);
    if (NS_FAILED(rv)) return rv;

    *aResult = out;
    NS_ADDREF(*aResult);
    return NS_OK;
}

inline nsresult
NS_NewBufferedInputStream(nsIInputStream** aResult,
                          nsIInputStream* aStr,
                          PRUint32 aBufferSize)
{
    nsresult rv;
    nsCOMPtr<nsIBufferedInputStream> in;
    static NS_DEFINE_CID(kBufferedInputStreamCID, NS_BUFFEREDINPUTSTREAM_CID);
    rv = nsComponentManager::CreateInstance(kBufferedInputStreamCID,
                                            nsnull, 
                                            NS_GET_IID(nsIBufferedInputStream),
                                            getter_AddRefs(in));
    if (NS_FAILED(rv)) return rv;
    rv = in->Init(aStr, aBufferSize);
    if (NS_FAILED(rv)) return rv;

    *aResult = in;
    NS_ADDREF(*aResult);
    return NS_OK;
}

inline nsresult
NS_NewBufferedOutputStream(nsIOutputStream** aResult,
                           nsIOutputStream* aStr,
                           PRUint32 aBufferSize)
{
    nsresult rv;
    nsCOMPtr<nsIBufferedOutputStream> out;
    static NS_DEFINE_CID(kBufferedOutputStreamCID, NS_BUFFEREDOUTPUTSTREAM_CID);
    rv = nsComponentManager::CreateInstance(kBufferedOutputStreamCID,
                                            nsnull, 
                                            NS_GET_IID(nsIBufferedOutputStream),
                                            getter_AddRefs(out));
    if (NS_FAILED(rv)) return rv;
    rv = out->Init(aStr, aBufferSize);
    if (NS_FAILED(rv)) return rv;

    *aResult = out;
    NS_ADDREF(*aResult);
    return NS_OK;
}

inline nsresult
NS_NewPostDataStream(nsIInputStream **result,
                     PRBool isFile,
                     const nsACString &data,
                     PRUint32 encodeFlags,
                     nsIIOService* ioService = nsnull)     // pass in nsIIOService to optimize callers
{
    nsresult rv;

    if (isFile) {
        nsCOMPtr<nsILocalFile> file;
        nsCOMPtr<nsIInputStream> fileStream;

        rv = NS_NewNativeLocalFile(data, PR_FALSE, getter_AddRefs(file));
        if (NS_FAILED(rv)) return rv;

        rv = NS_NewLocalFileInputStream(getter_AddRefs(fileStream), file);
        if (NS_FAILED(rv)) return rv;

        // wrap the file stream with a buffered input stream
        return NS_NewBufferedInputStream(result, fileStream, 8192);
    }

    // otherwise, create a string stream for the data
    return NS_NewCStringInputStream(result, data);
}

#endif // nsNetUtil_h__
