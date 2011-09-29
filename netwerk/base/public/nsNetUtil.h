/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
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
 *   Malcolm Smith <malsmith@cs.rmit.edu.au>
 *   Taras Glek <tglek@mozilla.com>
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
#include "nsStringGlue.h"
#include "nsMemory.h"
#include "nsCOMPtr.h"
#include "prio.h" // for read/write flags, permissions, etc.

#include "nsCRT.h"
#include "nsIURI.h"
#include "nsIStandardURL.h"
#include "nsIURLParser.h"
#include "nsIUUIDGenerator.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsISafeOutputStream.h"
#include "nsIStreamListener.h"
#include "nsIRequestObserverProxy.h"
#include "nsISimpleStreamListener.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIIOService.h"
#include "nsIServiceManager.h"
#include "nsIChannel.h"
#include "nsChannelProperties.h"
#include "nsIInputStreamChannel.h"
#include "nsITransport.h"
#include "nsIStreamTransportService.h"
#include "nsIHttpChannel.h"
#include "nsIDownloader.h"
#include "nsIStreamLoader.h"
#include "nsIUnicharStreamLoader.h"
#include "nsIPipe.h"
#include "nsIProtocolHandler.h"
#include "nsIFileProtocolHandler.h"
#include "nsIStringStream.h"
#include "nsILocalFile.h"
#include "nsIFileStreams.h"
#include "nsIFileURL.h"
#include "nsIProtocolProxyService.h"
#include "nsIProxyInfo.h"
#include "nsIFileStreams.h"
#include "nsIBufferedStreams.h"
#include "nsIInputStreamPump.h"
#include "nsIAsyncStreamCopier.h"
#include "nsIPersistentProperties2.h"
#include "nsISyncStreamListener.h"
#include "nsInterfaceRequestorAgg.h"
#include "nsINetUtil.h"
#include "nsIURIWithPrincipal.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPrompt2.h"
#include "nsIAuthPromptAdapterFactory.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsINestedURI.h"
#include "nsIMutable.h"
#include "nsIPropertyBag2.h"
#include "nsIWritablePropertyBag2.h"
#include "nsIIDNService.h"
#include "nsIChannelEventSink.h"
#include "nsIChannelPolicy.h"
#include "nsISocketProviderService.h"
#include "nsISocketProvider.h"
#include "nsIMIMEHeaderParam.h"
#include "mozilla/Services.h"

#include "nsIRedirectChannelRegistrar.h"

#ifdef MOZILLA_INTERNAL_API

inline already_AddRefed<nsIIOService>
do_GetIOService(nsresult* error = 0)
{
    already_AddRefed<nsIIOService> ret = mozilla::services::GetIOService();
    if (error)
        *error = ret.get() ? NS_OK : NS_ERROR_FAILURE;
    return ret;
}

inline already_AddRefed<nsINetUtil>
do_GetNetUtil(nsresult *error = 0) 
{
    nsCOMPtr<nsIIOService> io = mozilla::services::GetIOService();
    already_AddRefed<nsINetUtil> ret = nsnull;
    if (io)
        CallQueryInterface(io, &ret.mRawPtr);

    if (error)
        *error = ret.get() ? NS_OK : NS_ERROR_FAILURE;
    return ret;
}
#else
// Helper, to simplify getting the I/O service.
inline const nsGetServiceByContractIDWithError
do_GetIOService(nsresult* error = 0)
{
    return nsGetServiceByContractIDWithError(NS_IOSERVICE_CONTRACTID, error);
}

// An alias to do_GetIOService
inline const nsGetServiceByContractIDWithError
do_GetNetUtil(nsresult* error = 0)
{
    return do_GetIOService(error);
}
#endif

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
    return NS_NewURI(result, NS_ConvertUTF16toUTF8(spec), charset, baseURI, ioService);
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
              PRUint32               loadFlags = nsIRequest::LOAD_NORMAL,
              nsIChannelPolicy      *channelPolicy = nsnull)
{
    nsresult rv;
    nsCOMPtr<nsIIOService> grip;
    rv = net_EnsureIOService(&ioService, grip);
    if (ioService) {
        nsCOMPtr<nsIChannel> chan;
        rv = ioService->NewChannelFromURI(uri, getter_AddRefs(chan));
        if (NS_SUCCEEDED(rv)) {
            if (loadGroup)
                rv |= chan->SetLoadGroup(loadGroup);
            if (callbacks)
                rv |= chan->SetNotificationCallbacks(callbacks);
            if (loadFlags != nsIRequest::LOAD_NORMAL)
                rv |= chan->SetLoadFlags(loadFlags);
            if (channelPolicy) {
                nsCOMPtr<nsIWritablePropertyBag2> props = do_QueryInterface(chan, &rv);
                if (props) {
                    props->SetPropertyAsInterface(NS_CHANNEL_PROP_CHANNEL_POLICY,
                                                  channelPolicy);
                }
            }
            if (NS_SUCCEEDED(rv))
                chan.forget(result);
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
           PRUint32               loadFlags = nsIRequest::LOAD_NORMAL,
           nsIChannel           **channelOut = nsnull)
{
    nsresult rv;
    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewChannel(getter_AddRefs(channel), uri, ioService,
                       loadGroup, callbacks, loadFlags);
    if (NS_SUCCEEDED(rv)) {
        nsIInputStream *stream;
        rv = channel->Open(&stream);
        if (NS_SUCCEEDED(rv)) {
            *result = stream;
            if (channelOut) {
                *channelOut = nsnull;
                channel.swap(*channelOut);
            }
        }
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
            rv = baseURI->Resolve(NS_ConvertUTF16toUTF8(spec), resultBuf);
        if (NS_SUCCEEDED(rv))
            CopyUTF8toUTF16(resultBuf, result);
    }
    return rv;
}

/**
 * This function is a helper function to get a scheme's default port.
 */
inline PRInt32
NS_GetDefaultPort(const char *scheme,
                  nsIIOService* ioService = nsnull)
{
  nsresult rv;

  nsCOMPtr<nsIIOService> grip;
  net_EnsureIOService(&ioService, grip);
  if (!ioService)
      return -1;
 
  nsCOMPtr<nsIProtocolHandler> handler;
  rv = ioService->GetProtocolHandler(scheme, getter_AddRefs(handler));
  if (NS_FAILED(rv))
    return -1;
  PRInt32 port;
  rv = handler->GetDefaultPort(&port);
  return NS_SUCCEEDED(rv) ? port : -1;
}

/**
 * This function is a helper function to apply the ToAscii conversion
 * to a string
 */
inline bool
NS_StringToACE(const nsACString &idn, nsACString &result)
{
  nsCOMPtr<nsIIDNService> idnSrv = do_GetService(NS_IDNSERVICE_CONTRACTID);
  if (!idnSrv)
    return PR_FALSE;
  nsresult rv = idnSrv->ConvertUTF8toACE(idn, result);
  if (NS_FAILED(rv))
    return PR_FALSE;
  
  return PR_TRUE;
}

/**
 * This function is a helper function to get a protocol's default port if the
 * URI does not specify a port explicitly. Returns -1 if this protocol has no
 * concept of ports or if there was an error getting the port.
 */
inline PRInt32
NS_GetRealPort(nsIURI* aURI,
               nsIIOService* ioService = nsnull)     // pass in nsIIOService to optimize callers
{
    PRInt32 port;
    nsresult rv = aURI->GetPort(&port);
    if (NS_FAILED(rv))
        return -1;

    if (port != -1)
        return port; // explicitly specified

    // Otherwise, we have to get the default port from the protocol handler

    // Need the scheme first
    nsCAutoString scheme;
    rv = aURI->GetScheme(scheme);
    if (NS_FAILED(rv))
        return -1;

    return NS_GetDefaultPort(scheme.get());
}

inline nsresult
NS_NewInputStreamChannel(nsIChannel      **result,
                         nsIURI           *uri,
                         nsIInputStream   *stream,
                         const nsACString &contentType,
                         const nsACString *contentCharset)
{
    nsresult rv;
    nsCOMPtr<nsIInputStreamChannel> isc =
        do_CreateInstance(NS_INPUTSTREAMCHANNEL_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return rv;
    rv |= isc->SetURI(uri);
    rv |= isc->SetContentStream(stream);
    if (NS_FAILED(rv))
        return rv;
    nsCOMPtr<nsIChannel> chan = do_QueryInterface(isc, &rv);
    if (NS_FAILED(rv))
        return rv;
    if (!contentType.IsEmpty())
        rv |= chan->SetContentType(contentType);
    if (contentCharset && !contentCharset->IsEmpty())
        rv |= chan->SetContentCharset(*contentCharset);
    if (NS_SUCCEEDED(rv)) {
        *result = nsnull;
        chan.swap(*result);
    }
    return rv;
}

inline nsresult
NS_NewInputStreamChannel(nsIChannel      **result,
                         nsIURI           *uri,
                         nsIInputStream   *stream,
                         const nsACString &contentType    = EmptyCString())
{
    return NS_NewInputStreamChannel(result, uri, stream, contentType, nsnull);
}

inline nsresult
NS_NewInputStreamChannel(nsIChannel      **result,
                         nsIURI           *uri,
                         nsIInputStream   *stream,
                         const nsACString &contentType,
                         const nsACString &contentCharset)
{
    return NS_NewInputStreamChannel(result, uri, stream, contentType,
                                    &contentCharset);
}

inline nsresult
NS_NewInputStreamPump(nsIInputStreamPump **result,
                      nsIInputStream      *stream,
                      PRInt64              streamPos = PRInt64(-1),
                      PRInt64              streamLen = PRInt64(-1),
                      PRUint32             segsize = 0,
                      PRUint32             segcount = 0,
                      bool                 closeWhenDone = false)
{
    nsresult rv;
    nsCOMPtr<nsIInputStreamPump> pump =
        do_CreateInstance(NS_INPUTSTREAMPUMP_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = pump->Init(stream, streamPos, streamLen,
                        segsize, segcount, closeWhenDone);
        if (NS_SUCCEEDED(rv)) {
            *result = nsnull;
            pump.swap(*result);
        }
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
                        bool                   sourceBuffered = true,
                        bool                   sinkBuffered = true,
                        PRUint32               chunkSize = 0,
                        bool                   closeSource = true,
                        bool                   closeSink = true)
{
    nsresult rv;
    nsCOMPtr<nsIAsyncStreamCopier> copier =
        do_CreateInstance(NS_ASYNCSTREAMCOPIER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = copier->Init(source, sink, target, sourceBuffered, sinkBuffered,
                          chunkSize, closeSource, closeSink);
        if (NS_SUCCEEDED(rv)) {
            *result = nsnull;
            copier.swap(*result);
        }
    }
    return rv;
}

inline nsresult
NS_NewLoadGroup(nsILoadGroup      **result,
                nsIRequestObserver *obs)
{
    nsresult rv;
    nsCOMPtr<nsILoadGroup> group =
        do_CreateInstance(NS_LOADGROUP_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = group->SetGroupObserver(obs);
        if (NS_SUCCEEDED(rv)) {
            *result = nsnull;
            group.swap(*result);
        }
    }
    return rv;
}

inline nsresult
NS_NewDownloader(nsIStreamListener   **result,
                 nsIDownloadObserver  *observer,
                 nsIFile              *downloadLocation = nsnull)
{
    nsresult rv;
    nsCOMPtr<nsIDownloader> downloader =
        do_CreateInstance(NS_DOWNLOADER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = downloader->Init(observer, downloadLocation);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*result = downloader);
    }
    return rv;
}

inline nsresult
NS_NewStreamLoader(nsIStreamLoader        **result,
                   nsIStreamLoaderObserver *observer)
{
    nsresult rv;
    nsCOMPtr<nsIStreamLoader> loader =
        do_CreateInstance(NS_STREAMLOADER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = loader->Init(observer);
        if (NS_SUCCEEDED(rv)) {
            *result = nsnull;
            loader.swap(*result);
        }
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
        rv = NS_NewStreamLoader(result, observer);
        if (NS_SUCCEEDED(rv))
          rv = channel->AsyncOpen(*result, context);
    }
    return rv;
}

inline nsresult
NS_NewUnicharStreamLoader(nsIUnicharStreamLoader        **result,
                          nsIUnicharStreamLoaderObserver *observer)
{
    nsresult rv;
    nsCOMPtr<nsIUnicharStreamLoader> loader =
        do_CreateInstance(NS_UNICHARSTREAMLOADER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = loader->Init(observer);
        if (NS_SUCCEEDED(rv)) {
            *result = nsnull;
            loader.swap(*result);
        }
    }
    return rv;
}

inline nsresult
NS_NewSyncStreamListener(nsIStreamListener **result,
                         nsIInputStream    **stream)
{
    nsresult rv;
    nsCOMPtr<nsISyncStreamListener> listener =
        do_CreateInstance(NS_SYNCSTREAMLISTENER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = listener->GetInputStream(stream);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*result = listener);  // cannot use nsCOMPtr::swap
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
NS_ImplementChannelOpen(nsIChannel      *channel,
                        nsIInputStream **result)
{
    nsCOMPtr<nsIStreamListener> listener;
    nsCOMPtr<nsIInputStream> stream;
    nsresult rv = NS_NewSyncStreamListener(getter_AddRefs(listener),
                                           getter_AddRefs(stream));
    if (NS_SUCCEEDED(rv)) {
        rv = channel->AsyncOpen(listener, nsnull);
        if (NS_SUCCEEDED(rv)) {
            PRUint32 n;
            // block until the initial response is received or an error occurs.
            rv = stream->Available(&n);
            if (NS_SUCCEEDED(rv)) {
                *result = nsnull;
                stream.swap(*result);
            }
        }
    }
    return rv;
}

inline nsresult
NS_NewRequestObserverProxy(nsIRequestObserver **result,
                           nsIRequestObserver  *observer,
                           nsIEventTarget      *target = nsnull)
{
    nsresult rv;
    nsCOMPtr<nsIRequestObserverProxy> proxy =
        do_CreateInstance(NS_REQUESTOBSERVERPROXY_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = proxy->Init(observer, target);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*result = proxy);  // cannot use nsCOMPtr::swap
    }
    return rv;
}

inline nsresult
NS_NewSimpleStreamListener(nsIStreamListener **result,
                           nsIOutputStream    *sink,
                           nsIRequestObserver *observer = nsnull)
{
    nsresult rv;
    nsCOMPtr<nsISimpleStreamListener> listener = 
        do_CreateInstance(NS_SIMPLESTREAMLISTENER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = listener->Init(sink, observer);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*result = listener);  // cannot use nsCOMPtr::swap
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
        bool allow;
        rv = ioService->AllowPort(port, scheme, &allow);
        if (NS_SUCCEEDED(rv) && !allow) {
            NS_WARNING("port blocked");
            rv = NS_ERROR_PORT_ACCESS_NOT_ALLOWED;
        }
    }
    return rv;
}

// Determine if this URI is using a safe port.
inline nsresult
NS_CheckPortSafety(nsIURI *uri) {
    PRInt32 port;
    nsresult rv = uri->GetPort(&port);
    if (NS_FAILED(rv) || port == -1)  // port undefined or default-valued
        return NS_OK;
    nsCAutoString scheme;
    uri->GetScheme(scheme);
    return NS_CheckPortSafety(port, scheme.get());
}

inline nsresult
NS_NewProxyInfo(const nsACString &type,
                const nsACString &host,
                PRInt32           port,
                PRUint32          flags,
                nsIProxyInfo    **result)
{
    nsresult rv;
    nsCOMPtr<nsIProtocolProxyService> pps =
            do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
        rv = pps->NewProxyInfo(type, host, port, flags, PR_UINT32_MAX, nsnull,
                               result);
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
NS_GetURLSpecFromFile(nsIFile      *file,
                      nsACString   &url,
                      nsIIOService *ioService = nsnull)
{
    nsresult rv;
    nsCOMPtr<nsIFileProtocolHandler> fileHandler;
    rv = NS_GetFileProtocolHandler(getter_AddRefs(fileHandler), ioService);
    if (NS_SUCCEEDED(rv))
        rv = fileHandler->GetURLSpecFromFile(file, url);
    return rv;
}

/**
 * Converts the nsIFile to the corresponding URL string.
 * Should only be called on files which are not directories,
 * is otherwise identical to NS_GetURLSpecFromFile, but is
 * usually more efficient.
 * Warning: this restriction may not be enforced at runtime!
 */
inline nsresult
NS_GetURLSpecFromActualFile(nsIFile      *file,
                            nsACString   &url,
                            nsIIOService *ioService = nsnull)
{
    nsresult rv;
    nsCOMPtr<nsIFileProtocolHandler> fileHandler;
    rv = NS_GetFileProtocolHandler(getter_AddRefs(fileHandler), ioService);
    if (NS_SUCCEEDED(rv))
        rv = fileHandler->GetURLSpecFromActualFile(file, url);
    return rv;
}

/**
 * Converts the nsIFile to the corresponding URL string.
 * Should only be called on files which are directories,
 * is otherwise identical to NS_GetURLSpecFromFile, but is
 * usually more efficient.
 * Warning: this restriction may not be enforced at runtime!
 */
inline nsresult
NS_GetURLSpecFromDir(nsIFile      *file,
                     nsACString   &url,
                     nsIIOService *ioService = nsnull)
{
    nsresult rv;
    nsCOMPtr<nsIFileProtocolHandler> fileHandler;
    rv = NS_GetFileProtocolHandler(getter_AddRefs(fileHandler), ioService);
    if (NS_SUCCEEDED(rv))
        rv = fileHandler->GetURLSpecFromDir(file, url);
    return rv;
}

/**
 * Obtains the referrer for a given channel.  This first tries to obtain the
 * referrer from the property docshell.internalReferrer, and if that doesn't
 * work and the channel is an nsIHTTPChannel, we check it's referrer property.
 *
 * @returns NS_ERROR_NOT_AVAILABLE if no referrer is available.
 */
inline nsresult
NS_GetReferrerFromChannel(nsIChannel *channel,
                          nsIURI **referrer)
{
    nsresult rv = NS_ERROR_NOT_AVAILABLE;
    *referrer = nsnull;

    nsCOMPtr<nsIPropertyBag2> props(do_QueryInterface(channel));
    if (props) {
      // We have to check for a property on a property bag because the
      // referrer may be empty for security reasons (for example, when loading
      // an http page with an https referrer).
      rv = props->GetPropertyAsInterface(NS_LITERAL_STRING("docshell.internalReferrer"),
                                         NS_GET_IID(nsIURI),
                                         reinterpret_cast<void **>(referrer));
      if (NS_FAILED(rv))
        *referrer = nsnull;
    }

    // if that didn't work, we can still try to get the referrer from the
    // nsIHttpChannel (if we can QI to it)
    if (!(*referrer)) {
      nsCOMPtr<nsIHttpChannel> chan(do_QueryInterface(channel));
      if (chan) {
        rv = chan->GetReferrer(referrer);
        if (NS_FAILED(rv))
          *referrer = nsnull;
      }
    }
    return rv;
}

#ifdef MOZILLA_INTERNAL_API
inline nsresult
NS_ExamineForProxy(const char    *scheme,
                   const char    *host,
                   PRInt32        port, 
                   nsIProxyInfo **proxyInfo)
{
    nsresult rv;
    nsCOMPtr<nsIProtocolProxyService> pps =
            do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &rv);
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
        nsCOMPtr<nsIURI> uri =
                do_CreateInstance(NS_STANDARDURL_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv)) {
            rv = uri->SetSpec(spec);
            if (NS_SUCCEEDED(rv))
                rv = pps->Resolve(uri, 0, proxyInfo);
        }
    }
    return rv;
}
#endif

inline nsresult
NS_ParseContentType(const nsACString &rawContentType,
                    nsCString        &contentType,
                    nsCString        &contentCharset)
{
    // contentCharset is left untouched if not present in rawContentType
    nsresult rv;
    nsCOMPtr<nsINetUtil> util = do_GetNetUtil(&rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCString charset;
    bool hadCharset;
    rv = util->ParseContentType(rawContentType, charset, &hadCharset,
                                contentType);
    if (NS_SUCCEEDED(rv) && hadCharset)
        contentCharset = charset;
    return rv;
}

inline nsresult
NS_ExtractCharsetFromContentType(const nsACString &rawContentType,
                                 nsCString        &contentCharset,
                                 bool             *hadCharset,
                                 PRInt32          *charsetStart,
                                 PRInt32          *charsetEnd)
{
    // contentCharset is left untouched if not present in rawContentType
    nsresult rv;
    nsCOMPtr<nsINetUtil> util = do_GetNetUtil(&rv);
    NS_ENSURE_SUCCESS(rv, rv);

    return util->ExtractCharsetFromContentType(rawContentType,
                                               contentCharset,
                                               charsetStart,
                                               charsetEnd,
                                               hadCharset);
}

inline nsresult
NS_NewLocalFileInputStream(nsIInputStream **result,
                           nsIFile         *file,
                           PRInt32          ioFlags       = -1,
                           PRInt32          perm          = -1,
                           PRInt32          behaviorFlags = 0)
{
    nsresult rv;
    nsCOMPtr<nsIFileInputStream> in =
        do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = in->Init(file, ioFlags, perm, behaviorFlags);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*result = in);  // cannot use nsCOMPtr::swap
    }
    return rv;
}

inline nsresult
NS_NewPartialLocalFileInputStream(nsIInputStream **result,
                                  nsIFile         *file,
                                  PRUint64         offset,
                                  PRUint64         length,
                                  PRInt32          ioFlags       = -1,
                                  PRInt32          perm          = -1,
                                  PRInt32          behaviorFlags = 0)
{
    nsresult rv;
    nsCOMPtr<nsIPartialFileInputStream> in =
        do_CreateInstance(NS_PARTIALLOCALFILEINPUTSTREAM_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = in->Init(file, offset, length, ioFlags, perm, behaviorFlags);
        if (NS_SUCCEEDED(rv))
            rv = CallQueryInterface(in, result);
    }
    return rv;
}

inline nsresult
NS_NewLocalFileOutputStream(nsIOutputStream **result,
                            nsIFile          *file,
                            PRInt32           ioFlags       = -1,
                            PRInt32           perm          = -1,
                            PRInt32           behaviorFlags = 0)
{
    nsresult rv;
    nsCOMPtr<nsIFileOutputStream> out =
        do_CreateInstance(NS_LOCALFILEOUTPUTSTREAM_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = out->Init(file, ioFlags, perm, behaviorFlags);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*result = out);  // cannot use nsCOMPtr::swap
    }
    return rv;
}

// returns a file output stream which can be QI'ed to nsISafeOutputStream.
inline nsresult
NS_NewSafeLocalFileOutputStream(nsIOutputStream **result,
                                nsIFile          *file,
                                PRInt32           ioFlags       = -1,
                                PRInt32           perm          = -1,
                                PRInt32           behaviorFlags = 0)
{
    nsresult rv;
    nsCOMPtr<nsIFileOutputStream> out =
        do_CreateInstance(NS_SAFELOCALFILEOUTPUTSTREAM_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = out->Init(file, ioFlags, perm, behaviorFlags);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*result = out);  // cannot use nsCOMPtr::swap
    }
    return rv;
}

// returns the input end of a pipe.  the output end of the pipe
// is attached to the original stream.  data from the original
// stream is read into the pipe on a background thread.
inline nsresult
NS_BackgroundInputStream(nsIInputStream **result,
                         nsIInputStream  *stream,
                         PRUint32         segmentSize  = 0,
                         PRUint32         segmentCount = 0)
{
    nsresult rv;
    nsCOMPtr<nsIStreamTransportService> sts =
        do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsITransport> inTransport;
        rv = sts->CreateInputTransport(stream, PRInt64(-1), PRInt64(-1),
                                       PR_TRUE, getter_AddRefs(inTransport));
        if (NS_SUCCEEDED(rv))
            rv = inTransport->OpenInputStream(nsITransport::OPEN_BLOCKING,
                                              segmentSize, segmentCount,
                                              result);
    }
    return rv;
}

// returns the output end of a pipe.  the input end of the pipe
// is attached to the original stream.  data written to the pipe
// is copied to the original stream on a background thread.
inline nsresult
NS_BackgroundOutputStream(nsIOutputStream **result,
                          nsIOutputStream  *stream,
                          PRUint32          segmentSize  = 0,
                          PRUint32          segmentCount = 0)
{
    nsresult rv;
    nsCOMPtr<nsIStreamTransportService> sts =
        do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsITransport> inTransport;
        rv = sts->CreateOutputTransport(stream, PRInt64(-1), PRInt64(-1),
                                        PR_TRUE, getter_AddRefs(inTransport));
        if (NS_SUCCEEDED(rv))
            rv = inTransport->OpenOutputStream(nsITransport::OPEN_BLOCKING,
                                               segmentSize, segmentCount,
                                               result);
    }
    return rv;
}

inline nsresult
NS_NewBufferedInputStream(nsIInputStream **result,
                          nsIInputStream  *str,
                          PRUint32         bufferSize)
{
    nsresult rv;
    nsCOMPtr<nsIBufferedInputStream> in =
        do_CreateInstance(NS_BUFFEREDINPUTSTREAM_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = in->Init(str, bufferSize);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*result = in);  // cannot use nsCOMPtr::swap
    }
    return rv;
}

// note: the resulting stream can be QI'ed to nsISafeOutputStream iff the
// provided stream supports it.
inline nsresult
NS_NewBufferedOutputStream(nsIOutputStream **result,
                           nsIOutputStream  *str,
                           PRUint32          bufferSize)
{
    nsresult rv;
    nsCOMPtr<nsIBufferedOutputStream> out =
        do_CreateInstance(NS_BUFFEREDOUTPUTSTREAM_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = out->Init(str, bufferSize);
        if (NS_SUCCEEDED(rv))
            NS_ADDREF(*result = out);  // cannot use nsCOMPtr::swap
    }
    return rv;
}

/**
 * Attempts to buffer a given output stream.  If this fails, it returns the
 * passed-in output stream.
 *
 * @param aOutputStream
 *        The output stream we want to buffer.  This cannot be null.
 * @param aBufferSize
 *        The size of the buffer for the buffered output stream.
 * @returns an nsIOutputStream that is buffered with the specified buffer size,
 *          or is aOutputStream if creating the new buffered stream failed.
 */
inline already_AddRefed<nsIOutputStream>
NS_BufferOutputStream(nsIOutputStream *aOutputStream,
                      PRUint32 aBufferSize)
{
    NS_ASSERTION(aOutputStream, "No output stream given!");

    nsCOMPtr<nsIOutputStream> bos;
    nsresult rv = NS_NewBufferedOutputStream(getter_AddRefs(bos), aOutputStream,
                                             aBufferSize);
    if (NS_SUCCEEDED(rv))
        return bos.forget();

    NS_ADDREF(aOutputStream);
    return aOutputStream;
}

// returns an input stream compatible with nsIUploadChannel::SetUploadStream()
inline nsresult
NS_NewPostDataStream(nsIInputStream  **result,
                     bool              isFile,
                     const nsACString &data,
                     PRUint32          encodeFlags,
                     nsIIOService     *unused = nsnull)
{
    nsresult rv;

    if (isFile) {
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
    nsCOMPtr<nsIStringInputStream> stream
        (do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv));
    if (NS_FAILED(rv))
        return rv;

    rv = stream->SetData(data.BeginReading(), data.Length());
    if (NS_FAILED(rv))
        return rv;

    NS_ADDREF(*result = stream);
    return NS_OK;
}

inline nsresult
NS_ReadInputStreamToBuffer(nsIInputStream *aInputStream, 
                           void** aDest,
                           PRUint32 aCount)
{
    nsresult rv;

    if (!*aDest) {
        *aDest = malloc(aCount);
        if (!*aDest)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    char * p = reinterpret_cast<char*>(*aDest);
    PRUint32 bytesRead;
    PRUint32 totalRead = 0;
    while (1) {
        rv = aInputStream->Read(p + totalRead, aCount - totalRead, &bytesRead);
        if (!NS_SUCCEEDED(rv)) 
            return rv;
        totalRead += bytesRead;
        if (totalRead == aCount)
            break;
        // if Read reads 0 bytes, we've hit EOF 
        if (bytesRead == 0)
            return NS_ERROR_UNEXPECTED;
    }
    return rv; 
}

inline nsresult
NS_ReadInputStreamToString(nsIInputStream *aInputStream, 
                           nsACString &aDest,
                           PRUint32 aCount)
{
    aDest.SetLength(aCount);
    if (aDest.Length() != aCount)
        return NS_ERROR_OUT_OF_MEMORY;
    void* dest = aDest.BeginWriting();
    return NS_ReadInputStreamToBuffer(aInputStream, &dest, aCount);
}

inline nsresult
NS_LoadPersistentPropertiesFromURI(nsIPersistentProperties **result,
                                   nsIURI                   *uri,
                                   nsIIOService             *ioService = nsnull)
{
    nsCOMPtr<nsIInputStream> in;
    nsresult rv = NS_OpenURI(getter_AddRefs(in), uri, ioService);
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsIPersistentProperties> properties = 
            do_CreateInstance(NS_PERSISTENTPROPERTIES_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv)) {
            rv = properties->Load(in);
            if (NS_SUCCEEDED(rv)) {
                *result = nsnull;
                properties.swap(*result);
            }
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

/**
 * NS_QueryNotificationCallbacks implements the canonical algorithm for
 * querying interfaces from a channel's notification callbacks.  It first
 * searches the channel's notificationCallbacks attribute, and if the interface
 * is not found there, then it inspects the notificationCallbacks attribute of
 * the channel's loadGroup.
 */
inline void
NS_QueryNotificationCallbacks(nsIChannel   *channel,
                              const nsIID  &iid,
                              void        **result)
{
    NS_PRECONDITION(channel, "null channel");
    *result = nsnull;

    nsCOMPtr<nsIInterfaceRequestor> cbs;
    channel->GetNotificationCallbacks(getter_AddRefs(cbs));
    if (cbs)
        cbs->GetInterface(iid, result);
    if (!*result) {
        // try load group's notification callbacks...
        nsCOMPtr<nsILoadGroup> loadGroup;
        channel->GetLoadGroup(getter_AddRefs(loadGroup));
        if (loadGroup) {
            loadGroup->GetNotificationCallbacks(getter_AddRefs(cbs));
            if (cbs)
                cbs->GetInterface(iid, result);
        }
    }
}

/* template helper */
template <class T> inline void
NS_QueryNotificationCallbacks(nsIChannel  *channel,
                              nsCOMPtr<T> &result)
{
    NS_QueryNotificationCallbacks(channel, NS_GET_TEMPLATE_IID(T),
                                  getter_AddRefs(result));
}

/**
 * Alternate form of NS_QueryNotificationCallbacks designed for use by
 * nsIChannel implementations.
 */
inline void
NS_QueryNotificationCallbacks(nsIInterfaceRequestor  *callbacks,
                              nsILoadGroup           *loadGroup,
                              const nsIID            &iid,
                              void                  **result)
{
    *result = nsnull;

    if (callbacks)
        callbacks->GetInterface(iid, result);
    if (!*result) {
        // try load group's notification callbacks...
        if (loadGroup) {
            nsCOMPtr<nsIInterfaceRequestor> cbs;
            loadGroup->GetNotificationCallbacks(getter_AddRefs(cbs));
            if (cbs)
                cbs->GetInterface(iid, result);
        }
    }
}

/**
 * Wraps an nsIAuthPrompt so that it can be used as an nsIAuthPrompt2. This
 * method is provided mainly for use by other methods in this file.
 *
 * *aAuthPrompt2 should be set to null before calling this function.
 */
inline void
NS_WrapAuthPrompt(nsIAuthPrompt *aAuthPrompt, nsIAuthPrompt2** aAuthPrompt2)
{
    nsCOMPtr<nsIAuthPromptAdapterFactory> factory =
        do_GetService(NS_AUTHPROMPT_ADAPTER_FACTORY_CONTRACTID);
    if (!factory)
        return;

    NS_WARNING("Using deprecated nsIAuthPrompt");
    factory->CreateAdapter(aAuthPrompt, aAuthPrompt2);
}

/**
 * Gets an auth prompt from an interface requestor. This takes care of wrapping
 * an nsIAuthPrompt so that it can be used as an nsIAuthPrompt2.
 */
inline void
NS_QueryAuthPrompt2(nsIInterfaceRequestor  *aCallbacks,
                    nsIAuthPrompt2        **aAuthPrompt)
{
    CallGetInterface(aCallbacks, aAuthPrompt);
    if (*aAuthPrompt)
        return;

    // Maybe only nsIAuthPrompt is provided and we have to wrap it.
    nsCOMPtr<nsIAuthPrompt> prompt(do_GetInterface(aCallbacks));
    if (!prompt)
        return;

    NS_WrapAuthPrompt(prompt, aAuthPrompt);
}

/**
 * Gets an nsIAuthPrompt2 from a channel. Use this instead of
 * NS_QueryNotificationCallbacks for better backwards compatibility.
 */
inline void
NS_QueryAuthPrompt2(nsIChannel      *aChannel,
                    nsIAuthPrompt2 **aAuthPrompt)
{
    *aAuthPrompt = nsnull;

    // We want to use any auth prompt we can find on the channel's callbacks,
    // and if that fails use the loadgroup's prompt (if any)
    // Therefore, we can't just use NS_QueryNotificationCallbacks, because
    // that would prefer a loadgroup's nsIAuthPrompt2 over a channel's
    // nsIAuthPrompt.
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    aChannel->GetNotificationCallbacks(getter_AddRefs(callbacks));
    if (callbacks) {
        NS_QueryAuthPrompt2(callbacks, aAuthPrompt);
        if (*aAuthPrompt)
            return;
    }

    nsCOMPtr<nsILoadGroup> group;
    aChannel->GetLoadGroup(getter_AddRefs(group));
    if (!group)
        return;

    group->GetNotificationCallbacks(getter_AddRefs(callbacks));
    if (!callbacks)
        return;
    NS_QueryAuthPrompt2(callbacks, aAuthPrompt);
}

/* template helper */
template <class T> inline void
NS_QueryNotificationCallbacks(nsIInterfaceRequestor *callbacks,
                              nsILoadGroup          *loadGroup,
                              nsCOMPtr<T>           &result)
{
    NS_QueryNotificationCallbacks(callbacks, loadGroup,
                                  NS_GET_TEMPLATE_IID(T),
                                  getter_AddRefs(result));
}

/* template helper */
template <class T> inline void
NS_QueryNotificationCallbacks(const nsCOMPtr<nsIInterfaceRequestor> &aCallbacks,
                              const nsCOMPtr<nsILoadGroup>          &aLoadGroup,
                              nsCOMPtr<T>                           &aResult)
{
    NS_QueryNotificationCallbacks(aCallbacks.get(), aLoadGroup.get(), aResult);
}

/* template helper */
template <class T> inline void
NS_QueryNotificationCallbacks(const nsCOMPtr<nsIChannel> &aChannel,
                              nsCOMPtr<T>                &aResult)
{
    NS_QueryNotificationCallbacks(aChannel.get(), aResult);
}

/**
 * This function returns a nsIInterfaceRequestor instance that returns the
 * same result as NS_QueryNotificationCallbacks when queried.  It is useful
 * as the value for nsISocketTransport::securityCallbacks.
 */
inline nsresult
NS_NewNotificationCallbacksAggregation(nsIInterfaceRequestor  *callbacks,
                                       nsILoadGroup           *loadGroup,
                                       nsIInterfaceRequestor **result)
{
    nsCOMPtr<nsIInterfaceRequestor> cbs;
    if (loadGroup)
        loadGroup->GetNotificationCallbacks(getter_AddRefs(cbs));
    return NS_NewInterfaceRequestorAggregation(callbacks, cbs, result);
}

/**
 * Helper function for testing online/offline state of the browser.
 */
inline bool
NS_IsOffline()
{
    bool offline = true;
    nsCOMPtr<nsIIOService> ios = do_GetIOService();
    if (ios)
        ios->GetOffline(&offline);
    return offline;
}

/**
 * Helper functions for implementing nsINestedURI::innermostURI.
 *
 * Note that NS_DoImplGetInnermostURI is "private" -- call
 * NS_ImplGetInnermostURI instead.
 */
inline nsresult
NS_DoImplGetInnermostURI(nsINestedURI* nestedURI, nsIURI** result)
{
    NS_PRECONDITION(nestedURI, "Must have a nested URI!");
    NS_PRECONDITION(!*result, "Must have null *result");
    
    nsCOMPtr<nsIURI> inner;
    nsresult rv = nestedURI->GetInnerURI(getter_AddRefs(inner));
    NS_ENSURE_SUCCESS(rv, rv);

    // We may need to loop here until we reach the innermost
    // URI.
    nsCOMPtr<nsINestedURI> nestedInner(do_QueryInterface(inner));
    while (nestedInner) {
        rv = nestedInner->GetInnerURI(getter_AddRefs(inner));
        NS_ENSURE_SUCCESS(rv, rv);
        nestedInner = do_QueryInterface(inner);
    }

    // Found the innermost one if we reach here.
    inner.swap(*result);

    return rv;
}

inline nsresult
NS_ImplGetInnermostURI(nsINestedURI* nestedURI, nsIURI** result)
{
    // Make it safe to use swap()
    *result = nsnull;

    return NS_DoImplGetInnermostURI(nestedURI, result);
}

/**
 * Helper function that ensures that |result| is a URI that's safe to
 * return.  If |uri| is immutable, just returns it, otherwise returns
 * a clone.  |uri| must not be null.
 */
inline nsresult
NS_EnsureSafeToReturn(nsIURI* uri, nsIURI** result)
{
    NS_PRECONDITION(uri, "Must have a URI");
    
    // Assume mutable until told otherwise
    bool isMutable = true;
    nsCOMPtr<nsIMutable> mutableObj(do_QueryInterface(uri));
    if (mutableObj) {
        nsresult rv = mutableObj->GetMutable(&isMutable);
        isMutable = NS_FAILED(rv) || isMutable;
    }

    if (!isMutable) {
        NS_ADDREF(*result = uri);
        return NS_OK;
    }

    nsresult rv = uri->Clone(result);
    if (NS_SUCCEEDED(rv) && !*result) {
        NS_ERROR("nsIURI.clone contract was violated");
        return NS_ERROR_UNEXPECTED;
    }

    return rv;
}

/**
 * Helper function that tries to set the argument URI to be immutable
 */  
inline void
NS_TryToSetImmutable(nsIURI* uri)
{
    nsCOMPtr<nsIMutable> mutableObj(do_QueryInterface(uri));
    if (mutableObj) {
        mutableObj->SetMutable(PR_FALSE);
    }
}

/**
 * Helper function for calling ToImmutableURI.  If all else fails, returns
 * the input URI.  The optional second arg indicates whether we had to fall
 * back to the input URI.  Passing in a null URI is ok.
 */
inline already_AddRefed<nsIURI>
NS_TryToMakeImmutable(nsIURI* uri,
                      nsresult* outRv = nsnull)
{
    nsresult rv;
    nsCOMPtr<nsINetUtil> util = do_GetNetUtil(&rv);

    nsIURI* result = nsnull;
    if (NS_SUCCEEDED(rv)) {
        NS_ASSERTION(util, "do_GetNetUtil lied");
        rv = util->ToImmutableURI(uri, &result);
    }

    if (NS_FAILED(rv)) {
        NS_IF_ADDREF(result = uri);
    }

    if (outRv) {
        *outRv = rv;
    }

    return result;
}

/**
 * Helper function for testing whether the given URI, or any of its
 * inner URIs, has all the given protocol flags.
 */
inline nsresult
NS_URIChainHasFlags(nsIURI   *uri,
                    PRUint32  flags,
                    bool     *result)
{
    nsresult rv;
    nsCOMPtr<nsINetUtil> util = do_GetNetUtil(&rv);
    NS_ENSURE_SUCCESS(rv, rv);

    return util->URIChainHasFlags(uri, flags, result);
}

/**
 * Helper function for getting the innermost URI for a given URI.  The return
 * value could be just the object passed in if it's not a nested URI.
 */
inline already_AddRefed<nsIURI>
NS_GetInnermostURI(nsIURI *uri)
{
    NS_PRECONDITION(uri, "Must have URI");
    
    nsCOMPtr<nsINestedURI> nestedURI(do_QueryInterface(uri));
    if (!nestedURI) {
        NS_ADDREF(uri);
        return uri;
    }

    nsresult rv = nestedURI->GetInnermostURI(&uri);
    if (NS_FAILED(rv)) {
        return nsnull;
    }

    return uri;
}

/**
 * Get the "final" URI for a channel.  This is either the same as GetURI or
 * GetOriginalURI, depending on whether this channel has
 * nsIChanel::LOAD_REPLACE set.  For channels without that flag set, the final
 * URI is the original URI, while for ones with the flag the final URI is the
 * channel URI.
 */
inline nsresult
NS_GetFinalChannelURI(nsIChannel* channel, nsIURI** uri)
{
    *uri = nsnull;
    nsLoadFlags loadFlags = 0;
    nsresult rv = channel->GetLoadFlags(&loadFlags);
    NS_ENSURE_SUCCESS(rv, rv);
    
    if (loadFlags & nsIChannel::LOAD_REPLACE) {
        return channel->GetURI(uri);
    }
    
    return channel->GetOriginalURI(uri);
}

// NS_SecurityHashURI must return the same hash value for any two URIs that
// compare equal according to NS_SecurityCompareURIs.  Unfortunately, in the
// case of files, it's not clear we can do anything better than returning
// the schemeHash, so hashing files degenerates to storing them in a list.
inline PRUint32
NS_SecurityHashURI(nsIURI* aURI)
{
    nsCOMPtr<nsIURI> baseURI = NS_GetInnermostURI(aURI);

    nsCAutoString scheme;
    PRUint32 schemeHash = 0;
    if (NS_SUCCEEDED(baseURI->GetScheme(scheme)))
        schemeHash = nsCRT::HashCode(scheme.get());

    // TODO figure out how to hash file:// URIs
    if (scheme.EqualsLiteral("file"))
        return schemeHash; // sad face

    if (scheme.EqualsLiteral("imap") ||
        scheme.EqualsLiteral("mailbox") ||
        scheme.EqualsLiteral("news"))
    {
        nsCAutoString spec;
        PRUint32 specHash = baseURI->GetSpec(spec);
        if (NS_SUCCEEDED(specHash))
            specHash = nsCRT::HashCode(spec.get());
        return specHash;
    }

    nsCAutoString host;
    PRUint32 hostHash = 0;
    if (NS_SUCCEEDED(baseURI->GetAsciiHost(host)))
        hostHash = nsCRT::HashCode(host.get());

    // XOR to combine hash values
    return schemeHash ^ hostHash ^ NS_GetRealPort(baseURI);
}

inline bool
NS_SecurityCompareURIs(nsIURI* aSourceURI,
                       nsIURI* aTargetURI,
                       bool aStrictFileOriginPolicy)
{
    // Note that this is not an Equals() test on purpose -- for URIs that don't
    // support host/port, we want equality to basically be object identity, for
    // security purposes.  Otherwise, for example, two javascript: URIs that
    // are otherwise unrelated could end up "same origin", which would be
    // unfortunate.
    if (aSourceURI && aSourceURI == aTargetURI)
    {
        return PR_TRUE;
    }

    if (!aTargetURI || !aSourceURI)
    {
        return PR_FALSE;
    }

    // If either URI is a nested URI, get the base URI
    nsCOMPtr<nsIURI> sourceBaseURI = NS_GetInnermostURI(aSourceURI);
    nsCOMPtr<nsIURI> targetBaseURI = NS_GetInnermostURI(aTargetURI);

    // If either uri is an nsIURIWithPrincipal
    nsCOMPtr<nsIURIWithPrincipal> uriPrinc = do_QueryInterface(sourceBaseURI);
    if (uriPrinc) {
        uriPrinc->GetPrincipalUri(getter_AddRefs(sourceBaseURI));
    }

    uriPrinc = do_QueryInterface(targetBaseURI);
    if (uriPrinc) {
        uriPrinc->GetPrincipalUri(getter_AddRefs(targetBaseURI));
    }

    if (!sourceBaseURI || !targetBaseURI)
        return PR_FALSE;

    // Compare schemes
    nsCAutoString targetScheme;
    bool sameScheme = false;
    if (NS_FAILED( targetBaseURI->GetScheme(targetScheme) ) ||
        NS_FAILED( sourceBaseURI->SchemeIs(targetScheme.get(), &sameScheme) ) ||
        !sameScheme)
    {
        // Not same-origin if schemes differ
        return PR_FALSE;
    }

    // special handling for file: URIs
    if (targetScheme.EqualsLiteral("file"))
    {
        // in traditional unsafe behavior all files are the same origin
        if (!aStrictFileOriginPolicy)
            return PR_TRUE;

        nsCOMPtr<nsIFileURL> sourceFileURL(do_QueryInterface(sourceBaseURI));
        nsCOMPtr<nsIFileURL> targetFileURL(do_QueryInterface(targetBaseURI));

        if (!sourceFileURL || !targetFileURL)
            return PR_FALSE;

        nsCOMPtr<nsIFile> sourceFile, targetFile;

        sourceFileURL->GetFile(getter_AddRefs(sourceFile));
        targetFileURL->GetFile(getter_AddRefs(targetFile));

        if (!sourceFile || !targetFile)
            return PR_FALSE;

        // Otherwise they had better match
        bool filesAreEqual = false;
        nsresult rv = sourceFile->Equals(targetFile, &filesAreEqual);
        return NS_SUCCEEDED(rv) && filesAreEqual;
    }

    // Special handling for mailnews schemes
    if (targetScheme.EqualsLiteral("imap") ||
        targetScheme.EqualsLiteral("mailbox") ||
        targetScheme.EqualsLiteral("news"))
    {
        // Each message is a distinct trust domain; use the
        // whole spec for comparison
        nsCAutoString targetSpec;
        nsCAutoString sourceSpec;
        return ( NS_SUCCEEDED( targetBaseURI->GetSpec(targetSpec) ) &&
                 NS_SUCCEEDED( sourceBaseURI->GetSpec(sourceSpec) ) &&
                 targetSpec.Equals(sourceSpec) );
    }

    // Compare hosts
    nsCAutoString targetHost;
    nsCAutoString sourceHost;
    if (NS_FAILED( targetBaseURI->GetAsciiHost(targetHost) ) ||
        NS_FAILED( sourceBaseURI->GetAsciiHost(sourceHost) ))
    {
        return PR_FALSE;
    }

    nsCOMPtr<nsIStandardURL> targetURL(do_QueryInterface(targetBaseURI));
    nsCOMPtr<nsIStandardURL> sourceURL(do_QueryInterface(sourceBaseURI));
    if (!targetURL || !sourceURL)
    {
        return PR_FALSE;
    }

#ifdef MOZILLA_INTERNAL_API
    if (!targetHost.Equals(sourceHost, nsCaseInsensitiveCStringComparator() ))
#else
    if (!targetHost.Equals(sourceHost, CaseInsensitiveCompare))
#endif
    {
        return PR_FALSE;
    }

    return NS_GetRealPort(targetBaseURI) == NS_GetRealPort(sourceBaseURI);
}

inline bool
NS_IsInternalSameURIRedirect(nsIChannel *aOldChannel,
                             nsIChannel *aNewChannel,
                             PRUint32 aFlags)
{
  if (!(aFlags & nsIChannelEventSink::REDIRECT_INTERNAL)) {
    return PR_FALSE;
  }

  nsCOMPtr<nsIURI> oldURI, newURI;
  aOldChannel->GetURI(getter_AddRefs(oldURI));
  aNewChannel->GetURI(getter_AddRefs(newURI));

  if (!oldURI || !newURI) {
    return PR_FALSE;
  }

  bool res;
  return NS_SUCCEEDED(oldURI->Equals(newURI, &res)) && res;
}

inline nsresult
NS_LinkRedirectChannels(PRUint32 channelId,
                        nsIParentChannel *parentChannel,
                        nsIChannel** _result)
{
  nsresult rv;

  nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
      do_GetService("@mozilla.org/redirectchannelregistrar;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return registrar->LinkChannels(channelId,
                                 parentChannel,
                                 _result);
}

/**
 * Helper function to create a random URL string that's properly formed
 * but guaranteed to be invalid.
 */  
#define NS_FAKE_SCHEME "http://"
#define NS_FAKE_TLD ".invalid"
inline nsresult
NS_MakeRandomInvalidURLString(nsCString& result)
{
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsID idee;
  rv = uuidgen->GenerateUUIDInPlace(&idee);
  NS_ENSURE_SUCCESS(rv, rv);

  char chars[NSID_LENGTH];
  idee.ToProvidedString(chars);

  result.AssignLiteral(NS_FAKE_SCHEME);
  // Strip off the '{' and '}' at the beginning and end of the UUID
  result.Append(chars + 1, NSID_LENGTH - 3);
  result.AppendLiteral(NS_FAKE_TLD);

  return NS_OK;
}
#undef NS_FAKE_SCHEME
#undef NS_FAKE_TLD

/**
 * Helper function to determine whether urlString is Java-compatible --
 * whether it can be passed to the Java URL(String) constructor without the
 * latter throwing a MalformedURLException, or without Java otherwise
 * mishandling it.  This function (in effect) implements a scheme whitelist
 * for Java.
 */  
inline nsresult
NS_CheckIsJavaCompatibleURLString(nsCString& urlString, bool *result)
{
  *result = PR_FALSE; // Default to "no"

  nsresult rv = NS_OK;
  nsCOMPtr<nsIURLParser> urlParser =
    do_GetService(NS_STDURLPARSER_CONTRACTID, &rv);
  if (NS_FAILED(rv) || !urlParser)
    return NS_ERROR_FAILURE;

  bool compatible = true;
  PRUint32 schemePos = 0;
  PRInt32 schemeLen = 0;
  urlParser->ParseURL(urlString.get(), -1, &schemePos, &schemeLen,
                      nsnull, nsnull, nsnull, nsnull);
  if (schemeLen != -1) {
    nsCString scheme;
    scheme.Assign(urlString.get() + schemePos, schemeLen);
    // By default Java only understands a small number of URL schemes, and of
    // these only some can legitimately represent a browser page's "origin"
    // (and be something we can legitimately expect Java to handle ... or not
    // to mishandle).
    //
    // Besides those listed below, the OJI plugin understands the "jar",
    // "mailto", "netdoc", "javascript" and "rmi" schemes, and Java Plugin2
    // also understands the "about" scheme.  We actually pass "about" URLs
    // to Java ("about:blank" when processing a javascript: URL (one that
    // calls Java) from the location bar of a blank page, and (in FF4 and up)
    // "about:home" when processing a javascript: URL from the home page).
    // And Java doesn't appear to mishandle them (for example it doesn't allow
    // connections to "about" URLs).  But it doesn't make any sense to do
    // same-origin checks on "about" URLs, so we don't include them in our
    // scheme whitelist.
    //
    // The OJI plugin doesn't understand "chrome" URLs (only Java Plugin2
    // does) -- so we mustn't pass them to the OJI plugin.  But we do need to
    // pass "chrome" URLs to Java Plugin2:  Java Plugin2 grants additional
    // privileges to chrome "origins", and some extensions take advantage of
    // this.  For more information see bug 620773.
    //
    // As of FF4, we no longer support the OJI plugin.
    if (PL_strcasecmp(scheme.get(), "http") &&
        PL_strcasecmp(scheme.get(), "https") &&
        PL_strcasecmp(scheme.get(), "file") &&
        PL_strcasecmp(scheme.get(), "ftp") &&
        PL_strcasecmp(scheme.get(), "gopher") &&
        PL_strcasecmp(scheme.get(), "chrome"))
      compatible = PR_FALSE;
  } else {
    compatible = PR_FALSE;
  }

  *result = compatible;

  return NS_OK;
}

/** Given the first (disposition) token from a Content-Disposition header,
 * tell whether it indicates the content is inline or attachment
 * @param aDispToken the disposition token from the content-disposition header
 */
inline PRUint32
NS_GetContentDispositionFromToken(const nsAString& aDispToken)
{
  // RFC 2183, section 2.8 says that an unknown disposition
  // value should be treated as "attachment"
  // If all of these tests eval to false, then we have a content-disposition of
  // "attachment" or unknown
  if (aDispToken.IsEmpty() ||
      aDispToken.LowerCaseEqualsLiteral("inline") ||
      // Broken sites just send
      // Content-Disposition: filename="file"
      // without a disposition token... screen those out.
      StringHead(aDispToken, 8).LowerCaseEqualsLiteral("filename") ||
      // Also in use is Content-Disposition: name="file"
      StringHead(aDispToken, 4).LowerCaseEqualsLiteral("name"))
    return nsIChannel::DISPOSITION_INLINE;

  return nsIChannel::DISPOSITION_ATTACHMENT;
}

/** Determine the disposition (inline/attachment) of the content based on the
 * Content-Disposition header
 * @param aHeader the content-disposition header (full value)
 * @param aChan the channel the header came from
 */
inline PRUint32
NS_GetContentDispositionFromHeader(const nsACString& aHeader, nsIChannel *aChan = nsnull)
{
  nsresult rv;
  nsCOMPtr<nsIMIMEHeaderParam> mimehdrpar = do_GetService(NS_MIMEHEADERPARAM_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return nsIChannel::DISPOSITION_ATTACHMENT;

  nsCAutoString fallbackCharset;
  if (aChan) {
    nsCOMPtr<nsIURI> uri;
    aChan->GetURI(getter_AddRefs(uri));
    if (uri)
      uri->GetOriginCharset(fallbackCharset);
  }

  nsAutoString dispToken;
  rv = mimehdrpar->GetParameter(aHeader, "", fallbackCharset, PR_TRUE, nsnull,
                                dispToken);

  if (NS_FAILED(rv)) {
    // special case (see bug 272541): empty disposition type handled as "inline"
    if (rv == NS_ERROR_FIRST_HEADER_FIELD_COMPONENT_EMPTY)
        return nsIChannel::DISPOSITION_INLINE;
    return nsIChannel::DISPOSITION_ATTACHMENT;
  }

  return NS_GetContentDispositionFromToken(dispToken);
}

/** Extracts the filename out of a content-disposition header
 * @param aFilename [out] The filename. Can be empty on error.
 * @param aDisposition Value of a Content-Disposition header
 * @param aURI Optional. Will be used to get a fallback charset for the
 *        filename, if it is QI'able to nsIURL
 */
inline nsresult
NS_GetFilenameFromDisposition(nsAString& aFilename,
                              const nsACString& aDisposition,
                              nsIURI* aURI = nsnull)
{
  aFilename.Truncate();

  nsresult rv;
  nsCOMPtr<nsIMIMEHeaderParam> mimehdrpar =
      do_GetService(NS_MIMEHEADERPARAM_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);

  nsCAutoString fallbackCharset;
  if (url)
    url->GetOriginCharset(fallbackCharset);
  // Get the value of 'filename' parameter
  rv = mimehdrpar->GetParameter(aDisposition, "filename",
                                fallbackCharset, PR_TRUE, nsnull,
                                aFilename);
  if (NS_FAILED(rv) || aFilename.IsEmpty()) {
    // Try 'name' parameter, instead.
    rv = mimehdrpar->GetParameter(aDisposition, "name", fallbackCharset,
                                  PR_TRUE, nsnull, aFilename);
  }

  if (NS_FAILED(rv)) {
    aFilename.Truncate();
    return rv;
  }

  if (aFilename.IsEmpty())
    return NS_ERROR_NOT_AVAILABLE;

  return NS_OK;
}

/**
 * Make sure Personal Security Manager is initialized
 */
inline void
net_EnsurePSMInit()
{
    nsCOMPtr<nsISocketProviderService> spserv =
            do_GetService(NS_SOCKETPROVIDERSERVICE_CONTRACTID);
    if (spserv) {
        nsCOMPtr<nsISocketProvider> provider;
        spserv->GetSocketProvider("ssl", getter_AddRefs(provider));
    }
}

/**
 * Test whether a URI is "about:blank".  |uri| must not be null
 */
inline bool
NS_IsAboutBlank(nsIURI *uri)
{
    // GetSpec can be expensive for some URIs, so check the scheme first.
    bool isAbout = false;
    if (NS_FAILED(uri->SchemeIs("about", &isAbout)) || !isAbout) {
        return false;
    }

    nsCAutoString str;
    uri->GetSpec(str);
    return str.EqualsLiteral("about:blank");
}

#endif // !nsNetUtil_h__
