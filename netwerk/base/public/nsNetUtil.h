/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Contributor(s):
 *   Bradley Baetz <bbaetz@student.usyd.edu.au>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
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
#include "nsISafeOutputStream.h"
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
#include "nsIStreamTransportService.h"
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
#include "nsIPersistentProperties2.h"
#include "nsISyncStreamListener.h"

// Helper, to simplify getting the I/O service.
inline const nsGetServiceByCID
do_GetIOService(nsresult* error = 0)
{
    static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
    return nsGetServiceByCID(kIOServiceCID, 0, error);
}

// private little helper function... don't call this directly!
inline nsresult
net_EnsureIOService(nsIIOService **ios, nsCOMPtr<nsIIOService> &grip)
{
    nsresult rv = NS_OK;
    if (!*ios) {
        grip = do_GetIOService(&rv);
        *ios = grip;
    }
    return rv;
}

inline nsresult
NS_NewURI(nsIURI **result, 
          const nsACString &spec, 
          const char *charset = nsnull,
          nsIURI *baseURI = nsnull,
          nsIIOService *ioService = nsnull)     // pass in nsIIOService to optimize callers
{
    nsresult rv;
    nsCOMPtr<nsIIOService> grip;
    rv = net_EnsureIOService(&ioService, grip);
    if (ioService)
        rv = ioService->NewURI(spec, charset, baseURI, result);
    return rv; 
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
    nsCOMPtr<nsIIOService> grip;
    rv = net_EnsureIOService(&ioService, grip);
    if (ioService)
        rv = ioService->NewFileURI(spec, result);
    return rv;
}

inline nsresult
NS_NewChannel(nsIChannel           **result, 
              nsIURI                *uri,
              nsIIOService          *ioService = nsnull,    // pass in nsIIOService to optimize callers
              nsILoadGroup          *loadGroup = nsnull,
              nsIInterfaceRequestor *callbacks = nsnull,
              PRUint32               loadFlags = nsIRequest::LOAD_NORMAL)
{
    nsresult rv;
    nsCOMPtr<nsIIOService> grip;
    rv = net_EnsureIOService(&ioService, grip);
    if (ioService) {
        nsIChannel *chan;
        rv = ioService->NewChannelFromURI(uri, &chan);
        if (NS_SUCCEEDED(rv)) {
            if (loadGroup)
                rv |= chan->SetLoadGroup(loadGroup);
            if (callbacks)
                rv |= chan->SetNotificationCallbacks(callbacks);
            if (loadFlags != nsIRequest::LOAD_NORMAL)
                rv |= chan->SetLoadFlags(loadFlags);
            if (NS_SUCCEEDED(rv))
                *result = chan;
            else
                NS_RELEASE(chan);
        }
    }
    return rv;
}

// Use this function with CAUTION. It creates a stream that blocks when you
// Read() from it and blocking the UI thread is a bad idea. If you don't want
// to implement a full blown asynchronous consumer (via nsIStreamListener) look
// at nsIStreamLoader instead.
inline nsresult
NS_OpenURI(nsIInputStream       **result,
           nsIURI                *uri,
           nsIIOService          *ioService = nsnull,     // pass in nsIIOService to optimize callers
           nsILoadGroup          *loadGroup = nsnull,
           nsIInterfaceRequestor *callbacks = nsnull,
           PRUint32               loadFlags = nsIRequest::LOAD_NORMAL)
{
    nsresult rv;
    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewChannel(getter_AddRefs(channel), uri, ioService,
                       loadGroup, callbacks, loadFlags);
    if (NS_SUCCEEDED(rv)) {
        nsIInputStream *stream;
        rv = channel->Open(&stream);
        if (NS_SUCCEEDED(rv))
            *result = stream;
    }
    return rv;
}

inline nsresult
NS_OpenURI(nsIStreamListener     *listener, 
           nsISupports           *context, 
           nsIURI                *uri,
           nsIIOService          *ioService = nsnull,     // pass in nsIIOService to optimize callers
           nsILoadGroup          *loadGroup = nsnull,
           nsIInterfaceRequestor *callbacks = nsnull,
           PRUint32               loadFlags = nsIRequest::LOAD_NORMAL)
{
    nsresult rv;
    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewChannel(getter_AddRefs(channel), uri, ioService,
                       loadGroup, callbacks, loadFlags);
    if (NS_SUCCEEDED(rv))
        rv = channel->AsyncOpen(listener, context);
    return rv;
}

inline nsresult
NS_MakeAbsoluteURI(nsACString       &result,
                   const nsACString &spec, 
                   nsIURI           *baseURI, 
                   nsIIOService     *unused = nsnull)
{
    nsresult rv;
    if (!baseURI) {
        NS_WARNING("It doesn't make sense to not supply a base URI");
        result = spec;
        rv = NS_OK;
    }
    else if (spec.IsEmpty())
        rv = baseURI->GetSpec(result);
    else
        rv = baseURI->Resolve(spec, result);
    return rv;
}

inline nsresult
NS_MakeAbsoluteURI(char        **result,
                   const char   *spec, 
                   nsIURI       *baseURI, 
                   nsIIOService *unused = nsnull)
{
    nsresult rv;
    nsCAutoString resultBuf;
    rv = NS_MakeAbsoluteURI(resultBuf, nsDependentCString(spec), baseURI);
    if (NS_SUCCEEDED(rv)) {
        *result = ToNewCString(resultBuf);
        if (!*result)
            rv = NS_ERROR_OUT_OF_MEMORY;
    }
    return rv;
}

inline nsresult
NS_MakeAbsoluteURI(nsAString       &result,
                   const nsAString &spec, 
                   nsIURI          *baseURI,
                   nsIIOService    *unused = nsnull)
{
    nsresult rv;
    if (!baseURI) {
        NS_WARNING("It doesn't make sense to not supply a base URI");
        result = spec;
        rv = NS_OK;
    }
    else {
        nsCAutoString resultBuf;
        if (spec.IsEmpty())
            rv = baseURI->GetSpec(resultBuf);
        else
            rv = baseURI->Resolve(NS_ConvertUCS2toUTF8(spec), resultBuf);
        if (NS_SUCCEEDED(rv))
            CopyUTF8toUTF16(resultBuf, result);
    }
    return rv;
}

inline nsresult
NS_NewInputStreamChannel(nsIChannel      **result,
                         nsIURI           *uri,
                         nsIInputStream   *stream,
                         const nsACString &contentType    = EmptyCString(),
                         const nsACString &contentCharset = EmptyCString())
{
    nsresult rv;
    static NS_DEFINE_CID(kInputStreamChannelCID, NS_INPUTSTREAMCHANNEL_CID);
    nsCOMPtr<nsIInputStreamChannel> channel =
        do_CreateInstance(kInputStreamChannelCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv |= channel->SetURI(uri);
        rv |= channel->SetContentStream(stream);
        rv |= channel->SetContentType(contentType);
        rv |= channel->SetContentCharset(contentCharset);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*result = channel);
    }
    return rv;
}

inline nsresult
NS_NewInputStreamPump(nsIInputStreamPump **result,
                      nsIInputStream      *stream,
                      PRInt32              streamPos = -1,
                      PRInt32              streamLen = -1,
                      PRUint32             segsize = 0,
                      PRUint32             segcount = 0,
                      PRBool               closeWhenDone = PR_FALSE)
{
    nsresult rv;
    static NS_DEFINE_CID(kInputStreamPumpCID, NS_INPUTSTREAMPUMP_CID);
    nsCOMPtr<nsIInputStreamPump> pump =
        do_CreateInstance(kInputStreamPumpCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = pump->Init(stream, streamPos, streamLen,
                        segsize, segcount, closeWhenDone);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*result = pump);
    }
    return rv;
}

// NOTE: you will need to specify whether or not your streams are buffered
// (i.e., do they implement ReadSegments/WriteSegments).  the default
// assumption of TRUE for both streams might not be right for you!
inline nsresult
NS_NewAsyncStreamCopier(nsIAsyncStreamCopier **result,
                        nsIInputStream        *source,
                        nsIOutputStream       *sink,
                        nsIEventTarget        *target,
                        PRBool                 sourceBuffered = PR_TRUE,
                        PRBool                 sinkBuffered = PR_TRUE,
                        PRUint32               chunkSize = 0)
{
    nsresult rv;
    static NS_DEFINE_CID(kAsyncStreamCopierCID, NS_ASYNCSTREAMCOPIER_CID);
    nsCOMPtr<nsIAsyncStreamCopier> copier =
        do_CreateInstance(kAsyncStreamCopierCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = copier->Init(source, sink, target, sourceBuffered, sinkBuffered, chunkSize);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*result = copier);
    }
    return rv;
}

inline nsresult
NS_NewLoadGroup(nsILoadGroup      **result,
                nsIRequestObserver *obs)
{
    nsresult rv;
    static NS_DEFINE_CID(kLoadGroupCID, NS_LOADGROUP_CID);
    nsCOMPtr<nsILoadGroup> group =
        do_CreateInstance(kLoadGroupCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = group->SetGroupObserver(obs);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*result = group);
    }
    return rv;
}

inline nsresult
NS_NewDownloader(nsIStreamListener   **result,
                 nsIDownloadObserver  *observer,
                 nsIFile              *downloadLocation = nsnull)
{
    nsresult rv;
    static NS_DEFINE_CID(kDownloaderCID, NS_DOWNLOADER_CID);
    nsCOMPtr<nsIDownloader> downloader =
        do_CreateInstance(kDownloaderCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = downloader->Init(observer, downloadLocation);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*result = downloader);
    }
    return rv;
}

inline nsresult
NS_NewStreamLoader(nsIStreamLoader        **aResult,
                   nsIChannel              *aChannel,
                   nsIStreamLoaderObserver *aObserver,
                   nsISupports             *aContext)
{
    nsresult rv;
    static NS_DEFINE_CID(kStreamLoaderCID, NS_STREAMLOADER_CID);
    nsCOMPtr<nsIStreamLoader> loader =
        do_CreateInstance(kStreamLoaderCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = loader->Init(aChannel, aObserver, aContext);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*aResult = loader);
    }
    return rv;
}

inline nsresult
NS_NewStreamLoader(nsIStreamLoader        **result,
                   nsIURI                  *uri,
                   nsIStreamLoaderObserver *observer,
                   nsISupports             *context   = nsnull,
                   nsILoadGroup            *loadGroup = nsnull,
                   nsIInterfaceRequestor   *callbacks = nsnull,
                   PRUint32                 loadFlags = nsIRequest::LOAD_NORMAL,
                   nsIURI                  *referrer  = nsnull)
{
    nsresult rv;
    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewChannel(getter_AddRefs(channel),
                       uri,
                       nsnull,
                       loadGroup,
                       callbacks,
                       loadFlags);
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
        if (httpChannel)
            httpChannel->SetReferrer(referrer);
        rv = NS_NewStreamLoader(result, channel, observer, context);
    }
    return rv;
}

inline nsresult
NS_NewUnicharStreamLoader(nsIUnicharStreamLoader        **aResult,
                          nsIChannel                     *aChannel,
                          nsIUnicharStreamLoaderObserver *aObserver,
                          nsISupports                    *aContext     = nsnull,
                          PRUint32                        aSegmentSize = nsIUnicharStreamLoader::DEFAULT_SEGMENT_SIZE)
{
    nsresult rv;
    static NS_DEFINE_CID(kUnicharStreamLoaderCID, NS_UNICHARSTREAMLOADER_CID);
    nsCOMPtr<nsIUnicharStreamLoader> loader =
        do_CreateInstance(kUnicharStreamLoaderCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = loader->Init(aChannel, aObserver, aContext, aSegmentSize);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*aResult = loader);
    }
    return rv;
}

inline nsresult
NS_NewSyncStreamListener(nsIStreamListener **aResult,
                         nsIInputStream    **aStream)
{
    nsresult rv;
    static NS_DEFINE_CID(kSyncStreamListenerCID, NS_SYNCSTREAMLISTENER_CID);
    nsCOMPtr<nsISyncStreamListener> listener =
        do_CreateInstance(kSyncStreamListenerCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = listener->GetInputStream(aStream);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*aResult = listener);
    }
    return rv;
}

/**
 * Implement the nsIChannel::Open(nsIInputStream**) method using the channel's
 * AsyncOpen method.
 *
 * NOTE: Reading from the returned nsIInputStream may spin the current
 * thread's event queue, which could result in any event being processed.
 */
inline nsresult
NS_ImplementChannelOpen(nsIChannel      *aChannel,
                        nsIInputStream **aResult)
{
    nsCOMPtr<nsIStreamListener> listener;
    nsCOMPtr<nsIInputStream> stream;
    nsresult rv = NS_NewSyncStreamListener(getter_AddRefs(listener),
                                           getter_AddRefs(stream));
    if (NS_SUCCEEDED(rv)) {
        rv = aChannel->AsyncOpen(listener, nsnull);
        if (NS_SUCCEEDED(rv)) {
            PRUint32 n;
            // block until the initial response is received or an error occurs.
            rv = stream->Available(&n);
            if (NS_SUCCEEDED(rv))
                NS_ADDREF(*aResult = stream);
        }
    }
    return rv;
}

inline nsresult
NS_NewRequestObserverProxy(nsIRequestObserver **aResult,
                           nsIRequestObserver  *aObserver,
                           nsIEventQueue       *aEventQ = nsnull)
{
    nsresult rv;
    static NS_DEFINE_CID(kRequestObserverProxyCID, NS_REQUESTOBSERVERPROXY_CID);
    nsCOMPtr<nsIRequestObserverProxy> proxy =
        do_CreateInstance(kRequestObserverProxyCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = proxy->Init(aObserver, aEventQ);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*aResult = proxy);
    }
    return rv;
}

inline nsresult
NS_NewSimpleStreamListener(nsIStreamListener **aResult,
                           nsIOutputStream    *aSink,
                           nsIRequestObserver *aObserver = nsnull)
{
    nsresult rv;
    static NS_DEFINE_CID(kSimpleStreamListenerCID, NS_SIMPLESTREAMLISTENER_CID);
    nsCOMPtr<nsISimpleStreamListener> listener = 
        do_CreateInstance(kSimpleStreamListenerCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = listener->Init(aSink, aObserver);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*aResult = listener);
    }
    return rv;
}

inline nsresult
NS_NewAsyncStreamListener(nsIStreamListener **result,
                          nsIStreamListener  *receiver,
                          nsIEventQueue      *eventQueue)
{
    nsresult rv;
    static NS_DEFINE_CID(kAsyncStreamListenerCID, NS_ASYNCSTREAMLISTENER_CID);
    nsCOMPtr<nsIAsyncStreamListener> lsnr =
        do_CreateInstance(kAsyncStreamListenerCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = lsnr->Init(receiver, eventQueue);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*result = lsnr);
    }
    return rv;
}

inline nsresult
NS_CheckPortSafety(PRInt32       port,
                   const char   *scheme,
                   nsIIOService *ioService = nsnull)
{
    nsresult rv;
    nsCOMPtr<nsIIOService> grip;
    rv = net_EnsureIOService(&ioService, grip);
    if (ioService) {
        PRBool allow;
        rv = ioService->AllowPort(port, scheme, &allow);
        if (NS_SUCCEEDED(rv) && !allow)
            rv = NS_ERROR_PORT_ACCESS_NOT_ALLOWED;
    }
    return rv;
}

inline nsresult
NS_NewProxyInfo(const nsACString &type,
                const nsACString &host,
                PRInt32           port,
                nsIProxyInfo    **result)
{
    nsresult rv;
    static NS_DEFINE_CID(kPPSServiceCID, NS_PROTOCOLPROXYSERVICE_CID);
    nsCOMPtr<nsIProtocolProxyService> pps = do_GetService(kPPSServiceCID, &rv);
    if (NS_SUCCEEDED(rv))
        rv = pps->NewProxyInfo(type, host, port, result);
    return rv; 
}

inline nsresult
NS_GetFileProtocolHandler(nsIFileProtocolHandler **result,
                          nsIIOService            *ioService = nsnull)
{
    nsresult rv;
    nsCOMPtr<nsIIOService> grip;
    rv = net_EnsureIOService(&ioService, grip);
    if (ioService) {
        nsCOMPtr<nsIProtocolHandler> handler;
        rv = ioService->GetProtocolHandler("file", getter_AddRefs(handler));
        if (NS_SUCCEEDED(rv))
            rv = CallQueryInterface(handler, result);
    }
    return rv;
}

inline nsresult
NS_GetFileFromURLSpec(const nsACString  &inURL,
                      nsIFile          **result,
                      nsIIOService      *ioService = nsnull)
{
    nsresult rv;
    nsCOMPtr<nsIFileProtocolHandler> fileHandler;
    rv = NS_GetFileProtocolHandler(getter_AddRefs(fileHandler), ioService);
    if (NS_SUCCEEDED(rv))
        rv = fileHandler->GetFileFromURLSpec(inURL, result);
    return rv;
}

inline nsresult
NS_GetURLSpecFromFile(nsIFile      *aFile,
                      nsACString   &aUrl,
                      nsIIOService *ioService = nsnull)
{
    nsresult rv;
    nsCOMPtr<nsIFileProtocolHandler> fileHandler;
    rv = NS_GetFileProtocolHandler(getter_AddRefs(fileHandler), ioService);
    if (NS_SUCCEEDED(rv))
        rv = fileHandler->GetURLSpecFromFile(aFile, aUrl);
    return rv;
}

inline nsresult
NS_NewResumableEntityID(nsIResumableEntityID **aRes,
                        PRUint64               size,
                        const nsACString      &lastModified,
                        const nsACString      &entityTag)
{
    nsresult rv;
    nsCOMPtr<nsIResumableEntityID> ent =
        do_CreateInstance(NS_RESUMABLEENTITYID_CONTRACTID,&rv);
    if (NS_SUCCEEDED(rv)) {
        ent->SetSize(size);
        ent->SetLastModified(lastModified);
        ent->SetEntityTag(entityTag);
        NS_ADDREF(*aRes = ent);
    }
    return rv;
}

inline nsresult
NS_ExamineForProxy(const char    *scheme,
                   const char    *host,
                   PRInt32        port, 
                   nsIProxyInfo **proxyInfo)
{
    nsresult rv;
    static NS_DEFINE_CID(kPPSServiceCID, NS_PROTOCOLPROXYSERVICE_CID);
    nsCOMPtr<nsIProtocolProxyService> pps = do_GetService(kPPSServiceCID, &rv);
    if (NS_SUCCEEDED(rv)) {
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
        static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);    
        nsCOMPtr<nsIURI> uri = do_CreateInstance(kStandardURLCID, &rv);
        if (NS_SUCCEEDED(rv)) {
            rv = uri->SetSpec(spec);
            if (NS_SUCCEEDED(rv))
                rv = pps->ExamineForProxy(uri, proxyInfo);
        }
    }
    return rv;
}

inline nsresult
NS_ParseContentType(const nsACString &rawContentType,
                    nsCString        &contentType,
                    nsCString        &contentCharset)
{
    // contentCharset is left untouched if not present in rawContentType
    nsACString::const_iterator begin, it, end;
    it = rawContentType.BeginReading(begin);
    rawContentType.EndReading(end);
    if (FindCharInReadable(';', it, end)) {
        contentType = Substring(begin, it);
        // now look for "charset=FOO" and extract "FOO"
        begin = ++it;
        if (FindInReadable(NS_LITERAL_CSTRING("charset="), begin, it = end,
                           nsCaseInsensitiveCStringComparator())) {
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
NS_NewLocalFileInputStream(nsIInputStream **aResult,
                           nsIFile         *aFile,
                           PRInt32          aIOFlags       = -1,
                           PRInt32          aPerm          = -1,
                           PRInt32          aBehaviorFlags = 0)
{
    nsresult rv;
    static NS_DEFINE_CID(kLocalFileInputStreamCID, NS_LOCALFILEINPUTSTREAM_CID);
    nsCOMPtr<nsIFileInputStream> in =
        do_CreateInstance(kLocalFileInputStreamCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = in->Init(aFile, aIOFlags, aPerm, aBehaviorFlags);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*aResult = in);
    }
    return rv;
}

inline nsresult
NS_NewLocalFileOutputStream(nsIOutputStream **aResult,
                            nsIFile          *aFile,
                            PRInt32           aIOFlags       = -1,
                            PRInt32           aPerm          = -1,
                            PRInt32           aBehaviorFlags = 0)
{
    nsresult rv;
    static NS_DEFINE_CID(kLocalFileOutputStreamCID, NS_LOCALFILEOUTPUTSTREAM_CID);
    nsCOMPtr<nsIFileOutputStream> out =
        do_CreateInstance(kLocalFileOutputStreamCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = out->Init(aFile, aIOFlags, aPerm, aBehaviorFlags);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*aResult = out);
    }
    return rv;
}

// returns a file output stream which can be QI'ed to nsISafeOutputStream.
inline nsresult
NS_NewSafeLocalFileOutputStream(nsIOutputStream **aResult,
                                nsIFile          *aFile,
                                PRInt32           aIOFlags       = -1,
                                PRInt32           aPerm          = -1,
                                PRInt32           aBehaviorFlags = 0)
{
    nsresult rv;
    static NS_DEFINE_CID(kSafeLocalFileOutputStreamCID, NS_SAFELOCALFILEOUTPUTSTREAM_CID);
    nsCOMPtr<nsIFileOutputStream> out =
        do_CreateInstance(kSafeLocalFileOutputStreamCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = out->Init(aFile, aIOFlags, aPerm, aBehaviorFlags);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*aResult = out);
    }
    return rv;
}

// returns the input end of a pipe.  the output end of the pipe
// is attached to the original stream.  data from the original
// stream is read into the pipe on a background thread.
inline nsresult
NS_BackgroundInputStream(nsIInputStream **aResult,
                         nsIInputStream  *aStream,
                         PRUint32         aSegmentSize  = 0,
                         PRUint32         aSegmentCount = 0)
{
    nsresult rv;
    nsCOMPtr<nsIStreamTransportService> sts =
        do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsITransport> inTransport;
        rv = sts->CreateInputTransport(aStream, -1, -1, PR_TRUE,
                                       getter_AddRefs(inTransport));
        if (NS_SUCCEEDED(rv))
            rv = inTransport->OpenInputStream(nsITransport::OPEN_BLOCKING,
                                              aSegmentSize, aSegmentCount,
                                              aResult);
    }
    return rv;
}

// returns the output end of a pipe.  the input end of the pipe
// is attached to the original stream.  data written to the pipe
// is copied to the original stream on a background thread.
inline nsresult
NS_BackgroundOutputStream(nsIOutputStream **aResult,
                          nsIOutputStream  *aStream,
                          PRUint32          aSegmentSize  = 0,
                          PRUint32          aSegmentCount = 0)
{
    nsresult rv;
    nsCOMPtr<nsIStreamTransportService> sts =
        do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsITransport> inTransport;
        rv = sts->CreateOutputTransport(aStream, -1, -1, PR_TRUE,
                                        getter_AddRefs(inTransport));
        if (NS_SUCCEEDED(rv))
            rv = inTransport->OpenOutputStream(nsITransport::OPEN_BLOCKING,
                                               aSegmentSize, aSegmentCount,
                                               aResult);
    }
    return rv;
}

inline nsresult
NS_NewBufferedInputStream(nsIInputStream **aResult,
                          nsIInputStream *aStr,
                          PRUint32        aBufferSize)
{
    nsresult rv;
    static NS_DEFINE_CID(kBufferedInputStreamCID, NS_BUFFEREDINPUTSTREAM_CID);
    nsCOMPtr<nsIBufferedInputStream> in =
        do_CreateInstance(kBufferedInputStreamCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = in->Init(aStr, aBufferSize);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*aResult = in);
    }
    return rv;
}

// note: the resulting stream can be QI'ed to nsISafeOutputStream iff the
// provided stream supports it.
inline nsresult
NS_NewBufferedOutputStream(nsIOutputStream **aResult,
                           nsIOutputStream  *aStr,
                           PRUint32          aBufferSize)
{
    nsresult rv;
    static NS_DEFINE_CID(kBufferedOutputStreamCID, NS_BUFFEREDOUTPUTSTREAM_CID);
    nsCOMPtr<nsIBufferedOutputStream> out =
        do_CreateInstance(kBufferedOutputStreamCID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = out->Init(aStr, aBufferSize);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*aResult = out);
    }
    return rv;
}

// returns an input stream compatible with nsIUploadChannel::SetUploadStream()
inline nsresult
NS_NewPostDataStream(nsIInputStream  **result,
                     PRBool            isFile,
                     const nsACString &data,
                     PRUint32          encodeFlags,
                     nsIIOService     *unused = nsnull)
{
    if (isFile) {
        nsresult rv;
        nsCOMPtr<nsILocalFile> file;
        nsCOMPtr<nsIInputStream> fileStream;

        rv = NS_NewNativeLocalFile(data, PR_FALSE, getter_AddRefs(file));
        if (NS_SUCCEEDED(rv)) {
            rv = NS_NewLocalFileInputStream(getter_AddRefs(fileStream), file);
            if (NS_SUCCEEDED(rv)) {
                // wrap the file stream with a buffered input stream
                rv = NS_NewBufferedInputStream(result, fileStream, 8192);
            }
        }
        return rv;
    }

    // otherwise, create a string stream for the data (copies)
    return NS_NewCStringInputStream(result, data);
}

inline nsresult
NS_LoadPersistentPropertiesFromURI(nsIPersistentProperties **result,
                                   nsIURI                  *uri,
                                   nsIIOService            *ioService = nsnull)
{
    nsCOMPtr<nsIInputStream> in;
    nsresult rv = NS_OpenURI(getter_AddRefs(in), uri, ioService);
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIPersistentProperties> properties = 
            do_CreateInstance(NS_PERSISTENTPROPERTIES_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv)) {
            rv = properties->Load(in);
            if (NS_SUCCEEDED(rv))
                NS_ADDREF(*result = properties);
        }
    }
    return rv;
}

inline nsresult
NS_LoadPersistentPropertiesFromURISpec(nsIPersistentProperties **result,
                                       const nsACString        &spec,
                                       const char              *charset = nsnull,
                                       nsIURI                  *baseURI = nsnull,
                                       nsIIOService            *ioService = nsnull)     
{
    nsCOMPtr<nsIURI> uri;
    nsresult rv = 
        NS_NewURI(getter_AddRefs(uri), spec, charset, baseURI, ioService);

    if (NS_SUCCEEDED(rv))
        rv = NS_LoadPersistentPropertiesFromURI(result, uri, ioService);

    return rv;
}

#endif // !nsNetUtil_h__
