/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "mozilla/LoadContext.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Telemetry.h"
#include "nsNetUtil.h"
#include "nsNetUtilInlines.h"
#include "nsCategoryCache.h"
#include "nsContentUtils.h"
#include "nsHashKeys.h"
#include "nsHttp.h"
#include "nsIAsyncStreamCopier.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPrompt2.h"
#include "nsIAuthPromptAdapterFactory.h"
#include "nsIBufferedStreams.h"
#include "nsIChannelEventSink.h"
#include "nsIContentSniffer.h"
#include "nsIDocument.h"
#include "nsIDownloader.h"
#include "nsIFileProtocolHandler.h"
#include "nsIFileStreams.h"
#include "nsIFileURL.h"
#include "nsIIDNService.h"
#include "nsIInputStreamChannel.h"
#include "nsIInputStreamPump.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadContext.h"
#include "nsIMIMEHeaderParam.h"
#include "nsIMutable.h"
#include "nsINode.h"
#include "nsIOfflineCacheUpdate.h"
#include "nsIPersistentProperties2.h"
#include "nsIPrivateBrowsingChannel.h"
#include "nsIPropertyBag2.h"
#include "nsIProtocolProxyService.h"
#include "nsIRedirectChannelRegistrar.h"
#include "nsIRequestObserverProxy.h"
#include "nsIScriptSecurityManager.h"
#include "nsISimpleStreamListener.h"
#include "nsISocketProvider.h"
#include "nsISocketProviderService.h"
#include "nsIStandardURL.h"
#include "nsIStreamLoader.h"
#include "nsIIncrementalStreamLoader.h"
#include "nsIStreamTransportService.h"
#include "nsStringStream.h"
#include "nsISyncStreamListener.h"
#include "nsITransport.h"
#include "nsIUnicharStreamLoader.h"
#include "nsIURIWithPrincipal.h"
#include "nsIURLParser.h"
#include "nsIUUIDGenerator.h"
#include "nsIViewSourceChannel.h"
#include "nsInterfaceRequestorAgg.h"
#include "plstr.h"
#include "nsINestedURI.h"
#include "mozilla/dom/nsCSPUtils.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "nsIScriptError.h"
#include "nsISiteSecurityService.h"
#include "nsHttpHandler.h"
#include "nsNSSComponent.h"

#ifdef MOZ_WIDGET_GONK
#include "nsINetworkManager.h"
#include "nsThreadUtils.h" // for NS_IsMainThread
#endif

#include <limits>

using namespace mozilla;
using namespace mozilla::net;

nsresult /*NS_NewChannelWithNodeAndTriggeringPrincipal */
NS_NewChannelWithTriggeringPrincipal(nsIChannel           **outChannel,
                                     nsIURI                *aUri,
                                     nsINode               *aLoadingNode,
                                     nsIPrincipal          *aTriggeringPrincipal,
                                     nsSecurityFlags        aSecurityFlags,
                                     nsContentPolicyType    aContentPolicyType,
                                     nsILoadGroup          *aLoadGroup /* = nullptr */,
                                     nsIInterfaceRequestor *aCallbacks /* = nullptr */,
                                     nsLoadFlags            aLoadFlags /* = nsIRequest::LOAD_NORMAL */,
                                     nsIIOService          *aIoService /* = nullptr */)
{
  MOZ_ASSERT(aLoadingNode);
  NS_ASSERTION(aTriggeringPrincipal, "Can not create channel without a triggering Principal!");
  return NS_NewChannelInternal(outChannel,
                               aUri,
                               aLoadingNode,
                               aLoadingNode->NodePrincipal(),
                               aTriggeringPrincipal,
                               aSecurityFlags,
                               aContentPolicyType,
                               aLoadGroup,
                               aCallbacks,
                               aLoadFlags,
                               aIoService);
}

// See NS_NewChannelInternal for usage and argument description
nsresult /*NS_NewChannelWithPrincipalAndTriggeringPrincipal */
NS_NewChannelWithTriggeringPrincipal(nsIChannel           **outChannel,
                                     nsIURI                *aUri,
                                     nsIPrincipal          *aLoadingPrincipal,
                                     nsIPrincipal          *aTriggeringPrincipal,
                                     nsSecurityFlags        aSecurityFlags,
                                     nsContentPolicyType    aContentPolicyType,
                                     nsILoadGroup          *aLoadGroup /* = nullptr */,
                                     nsIInterfaceRequestor *aCallbacks /* = nullptr */,
                                     nsLoadFlags            aLoadFlags /* = nsIRequest::LOAD_NORMAL */,
                                     nsIIOService          *aIoService /* = nullptr */)
{
  NS_ASSERTION(aLoadingPrincipal, "Can not create channel without a loading Principal!");
  return NS_NewChannelInternal(outChannel,
                               aUri,
                               nullptr, // aLoadingNode
                               aLoadingPrincipal,
                               aTriggeringPrincipal,
                               aSecurityFlags,
                               aContentPolicyType,
                               aLoadGroup,
                               aCallbacks,
                               aLoadFlags,
                               aIoService);
}

nsresult /* NS_NewChannelNode */
NS_NewChannel(nsIChannel           **outChannel,
              nsIURI                *aUri,
              nsINode               *aLoadingNode,
              nsSecurityFlags        aSecurityFlags,
              nsContentPolicyType    aContentPolicyType,
              nsILoadGroup          *aLoadGroup /* = nullptr */,
              nsIInterfaceRequestor *aCallbacks /* = nullptr */,
              nsLoadFlags            aLoadFlags /* = nsIRequest::LOAD_NORMAL */,
              nsIIOService          *aIoService /* = nullptr */)
{
  NS_ASSERTION(aLoadingNode, "Can not create channel without a loading Node!");
  return NS_NewChannelInternal(outChannel,
                               aUri,
                               aLoadingNode,
                               aLoadingNode->NodePrincipal(),
                               nullptr, // aTriggeringPrincipal
                               aSecurityFlags,
                               aContentPolicyType,
                               aLoadGroup,
                               aCallbacks,
                               aLoadFlags,
                               aIoService);
}

nsresult
NS_MakeAbsoluteURI(nsACString       &result,
                   const nsACString &spec,
                   nsIURI           *baseURI)
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

nsresult
NS_MakeAbsoluteURI(char        **result,
                   const char   *spec,
                   nsIURI       *baseURI)
{
    nsresult rv;
    nsAutoCString resultBuf;
    rv = NS_MakeAbsoluteURI(resultBuf, nsDependentCString(spec), baseURI);
    if (NS_SUCCEEDED(rv)) {
        *result = ToNewCString(resultBuf);
        if (!*result)
            rv = NS_ERROR_OUT_OF_MEMORY;
    }
    return rv;
}

nsresult
NS_MakeAbsoluteURI(nsAString       &result,
                   const nsAString &spec,
                   nsIURI          *baseURI)
{
    nsresult rv;
    if (!baseURI) {
        NS_WARNING("It doesn't make sense to not supply a base URI");
        result = spec;
        rv = NS_OK;
    }
    else {
        nsAutoCString resultBuf;
        if (spec.IsEmpty())
            rv = baseURI->GetSpec(resultBuf);
        else
            rv = baseURI->Resolve(NS_ConvertUTF16toUTF8(spec), resultBuf);
        if (NS_SUCCEEDED(rv))
            CopyUTF8toUTF16(resultBuf, result);
    }
    return rv;
}

int32_t
NS_GetDefaultPort(const char *scheme,
                  nsIIOService *ioService /* = nullptr */)
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
  int32_t port;
  rv = handler->GetDefaultPort(&port);
  return NS_SUCCEEDED(rv) ? port : -1;
}

/**
 * This function is a helper function to apply the ToAscii conversion
 * to a string
 */
bool
NS_StringToACE(const nsACString &idn, nsACString &result)
{
  nsCOMPtr<nsIIDNService> idnSrv = do_GetService(NS_IDNSERVICE_CONTRACTID);
  if (!idnSrv)
    return false;
  nsresult rv = idnSrv->ConvertUTF8toACE(idn, result);
  if (NS_FAILED(rv))
    return false;

  return true;
}

int32_t
NS_GetRealPort(nsIURI *aURI)
{
    int32_t port;
    nsresult rv = aURI->GetPort(&port);
    if (NS_FAILED(rv))
        return -1;

    if (port != -1)
        return port; // explicitly specified

    // Otherwise, we have to get the default port from the protocol handler

    // Need the scheme first
    nsAutoCString scheme;
    rv = aURI->GetScheme(scheme);
    if (NS_FAILED(rv))
        return -1;

    return NS_GetDefaultPort(scheme.get());
}

nsresult /* NS_NewInputStreamChannelWithLoadInfo */
NS_NewInputStreamChannelInternal(nsIChannel        **outChannel,
                                 nsIURI             *aUri,
                                 nsIInputStream     *aStream,
                                 const nsACString   &aContentType,
                                 const nsACString   &aContentCharset,
                                 nsILoadInfo        *aLoadInfo)
{
  nsresult rv;
  nsCOMPtr<nsIInputStreamChannel> isc =
    do_CreateInstance(NS_INPUTSTREAMCHANNEL_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = isc->SetURI(aUri);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = isc->SetContentStream(aStream);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(isc, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aContentType.IsEmpty()) {
    rv = channel->SetContentType(aContentType);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (!aContentCharset.IsEmpty()) {
    rv = channel->SetContentCharset(aContentCharset);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  channel->SetLoadInfo(aLoadInfo);

  // If we're sandboxed, make sure to clear any owner the channel
  // might already have.
  if (aLoadInfo && aLoadInfo->GetLoadingSandboxed()) {
    channel->SetOwner(nullptr);
  }

  channel.forget(outChannel);
  return NS_OK;
}

nsresult
NS_NewInputStreamChannelInternal(nsIChannel        **outChannel,
                                 nsIURI             *aUri,
                                 nsIInputStream     *aStream,
                                 const nsACString   &aContentType,
                                 const nsACString   &aContentCharset,
                                 nsINode            *aLoadingNode,
                                 nsIPrincipal       *aLoadingPrincipal,
                                 nsIPrincipal       *aTriggeringPrincipal,
                                 nsSecurityFlags     aSecurityFlags,
                                 nsContentPolicyType aContentPolicyType)
{
  nsCOMPtr<nsILoadInfo> loadInfo =
    new mozilla::LoadInfo(aLoadingPrincipal,
                          aTriggeringPrincipal,
                          aLoadingNode,
                          aSecurityFlags,
                          aContentPolicyType);
  if (!loadInfo) {
    return NS_ERROR_UNEXPECTED;
  }
  return NS_NewInputStreamChannelInternal(outChannel,
                                          aUri,
                                          aStream,
                                          aContentType,
                                          aContentCharset,
                                          loadInfo);
}

nsresult /* NS_NewInputStreamChannelPrincipal */
NS_NewInputStreamChannel(nsIChannel        **outChannel,
                         nsIURI             *aUri,
                         nsIInputStream     *aStream,
                         nsIPrincipal       *aLoadingPrincipal,
                         nsSecurityFlags     aSecurityFlags,
                         nsContentPolicyType aContentPolicyType,
                         const nsACString   &aContentType    /* = EmptyCString() */,
                         const nsACString   &aContentCharset /* = EmptyCString() */)
{
  return NS_NewInputStreamChannelInternal(outChannel,
                                          aUri,
                                          aStream,
                                          aContentType,
                                          aContentCharset,
                                          nullptr, // aLoadingNode
                                          aLoadingPrincipal,
                                          nullptr, // aTriggeringPrincipal
                                          aSecurityFlags,
                                          aContentPolicyType);
}

nsresult
NS_NewInputStreamChannelInternal(nsIChannel        **outChannel,
                                 nsIURI             *aUri,
                                 const nsAString    &aData,
                                 const nsACString   &aContentType,
                                 nsILoadInfo        *aLoadInfo,
                                 bool                aIsSrcdocChannel /* = false */)
{
  nsresult rv;
  nsCOMPtr<nsIStringInputStream> stream;
  stream = do_CreateInstance(NS_STRINGINPUTSTREAM_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef MOZILLA_INTERNAL_API
    uint32_t len;
    char* utf8Bytes = ToNewUTF8String(aData, &len);
    rv = stream->AdoptData(utf8Bytes, len);
#else
    char* utf8Bytes = ToNewUTF8String(aData);
    rv = stream->AdoptData(utf8Bytes, strlen(utf8Bytes));
#endif

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewInputStreamChannelInternal(getter_AddRefs(channel),
                                        aUri,
                                        stream,
                                        aContentType,
                                        NS_LITERAL_CSTRING("UTF-8"),
                                        aLoadInfo);

  NS_ENSURE_SUCCESS(rv, rv);

  if (aIsSrcdocChannel) {
    nsCOMPtr<nsIInputStreamChannel> inStrmChan = do_QueryInterface(channel);
    NS_ENSURE_TRUE(inStrmChan, NS_ERROR_FAILURE);
    inStrmChan->SetSrcdocData(aData);
  }
  channel.forget(outChannel);
  return NS_OK;
}

nsresult
NS_NewInputStreamChannelInternal(nsIChannel        **outChannel,
                                 nsIURI             *aUri,
                                 const nsAString    &aData,
                                 const nsACString   &aContentType,
                                 nsINode            *aLoadingNode,
                                 nsIPrincipal       *aLoadingPrincipal,
                                 nsIPrincipal       *aTriggeringPrincipal,
                                 nsSecurityFlags     aSecurityFlags,
                                 nsContentPolicyType aContentPolicyType,
                                 bool                aIsSrcdocChannel /* = false */)
{
  nsCOMPtr<nsILoadInfo> loadInfo =
      new mozilla::LoadInfo(aLoadingPrincipal, aTriggeringPrincipal,
                            aLoadingNode, aSecurityFlags, aContentPolicyType);
  return NS_NewInputStreamChannelInternal(outChannel, aUri, aData, aContentType,
                                          loadInfo, aIsSrcdocChannel);
}

nsresult
NS_NewInputStreamChannel(nsIChannel        **outChannel,
                         nsIURI             *aUri,
                         const nsAString    &aData,
                         const nsACString   &aContentType,
                         nsIPrincipal       *aLoadingPrincipal,
                         nsSecurityFlags     aSecurityFlags,
                         nsContentPolicyType aContentPolicyType,
                         bool                aIsSrcdocChannel /* = false */)
{
  return NS_NewInputStreamChannelInternal(outChannel,
                                          aUri,
                                          aData,
                                          aContentType,
                                          nullptr, // aLoadingNode
                                          aLoadingPrincipal,
                                          nullptr, // aTriggeringPrincipal
                                          aSecurityFlags,
                                          aContentPolicyType,
                                          aIsSrcdocChannel);
}

nsresult
NS_NewInputStreamPump(nsIInputStreamPump **result,
                      nsIInputStream      *stream,
                      int64_t              streamPos /* = int64_t(-1) */,
                      int64_t              streamLen /* = int64_t(-1) */,
                      uint32_t             segsize /* = 0 */,
                      uint32_t             segcount /* = 0 */,
                      bool                 closeWhenDone /* = false */)
{
    nsresult rv;
    nsCOMPtr<nsIInputStreamPump> pump =
        do_CreateInstance(NS_INPUTSTREAMPUMP_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = pump->Init(stream, streamPos, streamLen,
                        segsize, segcount, closeWhenDone);
        if (NS_SUCCEEDED(rv)) {
            *result = nullptr;
            pump.swap(*result);
        }
    }
    return rv;
}

nsresult
NS_NewAsyncStreamCopier(nsIAsyncStreamCopier **result,
                        nsIInputStream        *source,
                        nsIOutputStream       *sink,
                        nsIEventTarget        *target,
                        bool                   sourceBuffered /* = true */,
                        bool                   sinkBuffered /* = true */,
                        uint32_t               chunkSize /* = 0 */,
                        bool                   closeSource /* = true */,
                        bool                   closeSink /* = true */)
{
    nsresult rv;
    nsCOMPtr<nsIAsyncStreamCopier> copier =
        do_CreateInstance(NS_ASYNCSTREAMCOPIER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = copier->Init(source, sink, target, sourceBuffered, sinkBuffered,
                          chunkSize, closeSource, closeSink);
        if (NS_SUCCEEDED(rv)) {
            *result = nullptr;
            copier.swap(*result);
        }
    }
    return rv;
}

nsresult
NS_NewLoadGroup(nsILoadGroup      **result,
                nsIRequestObserver *obs)
{
    nsresult rv;
    nsCOMPtr<nsILoadGroup> group =
        do_CreateInstance(NS_LOADGROUP_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = group->SetGroupObserver(obs);
        if (NS_SUCCEEDED(rv)) {
            *result = nullptr;
            group.swap(*result);
        }
    }
    return rv;
}

bool NS_IsReasonableHTTPHeaderValue(const nsACString &aValue)
{
  return mozilla::net::nsHttp::IsReasonableHeaderValue(aValue);
}

bool NS_IsValidHTTPToken(const nsACString &aToken)
{
  return mozilla::net::nsHttp::IsValidToken(aToken);
}

nsresult
NS_NewLoadGroup(nsILoadGroup **aResult, nsIPrincipal *aPrincipal)
{
    using mozilla::LoadContext;
    nsresult rv;

    nsCOMPtr<nsILoadGroup> group =
        do_CreateInstance(NS_LOADGROUP_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    RefPtr<LoadContext> loadContext = new LoadContext(aPrincipal);
    rv = group->SetNotificationCallbacks(loadContext);
    NS_ENSURE_SUCCESS(rv, rv);

    group.forget(aResult);
    return rv;
}

bool
NS_LoadGroupMatchesPrincipal(nsILoadGroup *aLoadGroup,
                             nsIPrincipal *aPrincipal)
{
    if (!aPrincipal) {
      return false;
    }

    // If this is a null principal then the load group doesn't really matter.
    // The principal will not be allowed to perform any actions that actually
    // use the load group.  Unconditionally treat null principals as a match.
    if (aPrincipal->GetIsNullPrincipal()) {
      return true;
    }

    if (!aLoadGroup) {
        return false;
    }

    nsCOMPtr<nsILoadContext> loadContext;
    NS_QueryNotificationCallbacks(nullptr, aLoadGroup, NS_GET_IID(nsILoadContext),
                                  getter_AddRefs(loadContext));
    NS_ENSURE_TRUE(loadContext, false);

    // Verify load context browser flag match the principal
    bool contextInIsolatedBrowser;
    nsresult rv = loadContext->GetIsInIsolatedMozBrowserElement(&contextInIsolatedBrowser);
    NS_ENSURE_SUCCESS(rv, false);

    return nsIScriptSecurityManager::NO_APP_ID == aPrincipal->GetAppId() &&
           contextInIsolatedBrowser == aPrincipal->GetIsInIsolatedMozBrowserElement();
}

nsresult
NS_NewDownloader(nsIStreamListener   **result,
                 nsIDownloadObserver  *observer,
                 nsIFile              *downloadLocation /* = nullptr */)
{
    nsresult rv;
    nsCOMPtr<nsIDownloader> downloader =
        do_CreateInstance(NS_DOWNLOADER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = downloader->Init(observer, downloadLocation);
        if (NS_SUCCEEDED(rv)) {
            downloader.forget(result);
        }
    }
    return rv;
}

nsresult
NS_NewIncrementalStreamLoader(nsIIncrementalStreamLoader        **result,
                              nsIIncrementalStreamLoaderObserver *observer)
{
    nsresult rv;
    nsCOMPtr<nsIIncrementalStreamLoader> loader =
        do_CreateInstance(NS_INCREMENTALSTREAMLOADER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = loader->Init(observer);
        if (NS_SUCCEEDED(rv)) {
            *result = nullptr;
            loader.swap(*result);
        }
    }
    return rv;
}

nsresult
NS_NewStreamLoaderInternal(nsIStreamLoader        **outStream,
                           nsIURI                  *aUri,
                           nsIStreamLoaderObserver *aObserver,
                           nsINode                 *aLoadingNode,
                           nsIPrincipal            *aLoadingPrincipal,
                           nsSecurityFlags          aSecurityFlags,
                           nsContentPolicyType      aContentPolicyType,
                           nsILoadGroup            *aLoadGroup /* = nullptr */,
                           nsIInterfaceRequestor   *aCallbacks /* = nullptr */,
                           nsLoadFlags              aLoadFlags /* = nsIRequest::LOAD_NORMAL */,
                           nsIURI                  *aReferrer /* = nullptr */)
{
   nsCOMPtr<nsIChannel> channel;
   nsresult rv = NS_NewChannelInternal(getter_AddRefs(channel),
                                       aUri,
                                       aLoadingNode,
                                       aLoadingPrincipal,
                                       nullptr, // aTriggeringPrincipal
                                       aSecurityFlags,
                                       aContentPolicyType,
                                       aLoadGroup,
                                       aCallbacks,
                                       aLoadFlags);

  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    httpChannel->SetReferrer(aReferrer);
  }
  rv = NS_NewStreamLoader(outStream, aObserver);
  NS_ENSURE_SUCCESS(rv, rv);
  return channel->AsyncOpen2(*outStream);
}


nsresult /* NS_NewStreamLoaderNode */
NS_NewStreamLoader(nsIStreamLoader        **outStream,
                   nsIURI                  *aUri,
                   nsIStreamLoaderObserver *aObserver,
                   nsINode                 *aLoadingNode,
                   nsSecurityFlags          aSecurityFlags,
                   nsContentPolicyType      aContentPolicyType,
                   nsILoadGroup            *aLoadGroup /* = nullptr */,
                   nsIInterfaceRequestor   *aCallbacks /* = nullptr */,
                   nsLoadFlags              aLoadFlags /* = nsIRequest::LOAD_NORMAL */,
                   nsIURI                  *aReferrer /* = nullptr */)
{
  NS_ASSERTION(aLoadingNode, "Can not create stream loader without a loading Node!");
  return NS_NewStreamLoaderInternal(outStream,
                                    aUri,
                                    aObserver,
                                    aLoadingNode,
                                    aLoadingNode->NodePrincipal(),
                                    aSecurityFlags,
                                    aContentPolicyType,
                                    aLoadGroup,
                                    aCallbacks,
                                    aLoadFlags,
                                    aReferrer);
}

nsresult /* NS_NewStreamLoaderPrincipal */
NS_NewStreamLoader(nsIStreamLoader        **outStream,
                   nsIURI                  *aUri,
                   nsIStreamLoaderObserver *aObserver,
                   nsIPrincipal            *aLoadingPrincipal,
                   nsSecurityFlags          aSecurityFlags,
                   nsContentPolicyType      aContentPolicyType,
                   nsILoadGroup            *aLoadGroup /* = nullptr */,
                   nsIInterfaceRequestor   *aCallbacks /* = nullptr */,
                   nsLoadFlags              aLoadFlags /* = nsIRequest::LOAD_NORMAL */,
                   nsIURI                  *aReferrer /* = nullptr */)
{
  return NS_NewStreamLoaderInternal(outStream,
                                    aUri,
                                    aObserver,
                                    nullptr, // aLoadingNode
                                    aLoadingPrincipal,
                                    aSecurityFlags,
                                    aContentPolicyType,
                                    aLoadGroup,
                                    aCallbacks,
                                    aLoadFlags,
                                    aReferrer);
}

nsresult
NS_NewUnicharStreamLoader(nsIUnicharStreamLoader        **result,
                          nsIUnicharStreamLoaderObserver *observer)
{
    nsresult rv;
    nsCOMPtr<nsIUnicharStreamLoader> loader =
        do_CreateInstance(NS_UNICHARSTREAMLOADER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = loader->Init(observer);
        if (NS_SUCCEEDED(rv)) {
            *result = nullptr;
            loader.swap(*result);
        }
    }
    return rv;
}

nsresult
NS_NewSyncStreamListener(nsIStreamListener **result,
                         nsIInputStream    **stream)
{
    nsresult rv;
    nsCOMPtr<nsISyncStreamListener> listener =
        do_CreateInstance(NS_SYNCSTREAMLISTENER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = listener->GetInputStream(stream);
        if (NS_SUCCEEDED(rv)) {
            listener.forget(result);
        }
    }
    return rv;
}

nsresult
NS_ImplementChannelOpen(nsIChannel      *channel,
                        nsIInputStream **result)
{
    nsCOMPtr<nsIStreamListener> listener;
    nsCOMPtr<nsIInputStream> stream;
    nsresult rv = NS_NewSyncStreamListener(getter_AddRefs(listener),
                                           getter_AddRefs(stream));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_MaybeOpenChannelUsingAsyncOpen2(channel, listener);
    NS_ENSURE_SUCCESS(rv, rv);

    uint64_t n;
    // block until the initial response is received or an error occurs.
    rv = stream->Available(&n);
    NS_ENSURE_SUCCESS(rv, rv);

    *result = nullptr;
    stream.swap(*result);

    return NS_OK;
 }

nsresult
NS_NewRequestObserverProxy(nsIRequestObserver **result,
                           nsIRequestObserver  *observer,
                           nsISupports         *context)
{
    nsresult rv;
    nsCOMPtr<nsIRequestObserverProxy> proxy =
        do_CreateInstance(NS_REQUESTOBSERVERPROXY_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = proxy->Init(observer, context);
        if (NS_SUCCEEDED(rv)) {
            proxy.forget(result);
        }
    }
    return rv;
}

nsresult
NS_NewSimpleStreamListener(nsIStreamListener **result,
                           nsIOutputStream    *sink,
                           nsIRequestObserver *observer /* = nullptr */)
{
    nsresult rv;
    nsCOMPtr<nsISimpleStreamListener> listener =
        do_CreateInstance(NS_SIMPLESTREAMLISTENER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = listener->Init(sink, observer);
        if (NS_SUCCEEDED(rv)) {
            listener.forget(result);
        }
    }
    return rv;
}

nsresult
NS_CheckPortSafety(int32_t       port,
                   const char   *scheme,
                   nsIIOService *ioService /* = nullptr */)
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

nsresult
NS_CheckPortSafety(nsIURI *uri)
{
    int32_t port;
    nsresult rv = uri->GetPort(&port);
    if (NS_FAILED(rv) || port == -1)  // port undefined or default-valued
        return NS_OK;
    nsAutoCString scheme;
    uri->GetScheme(scheme);
    return NS_CheckPortSafety(port, scheme.get());
}

nsresult
NS_NewProxyInfo(const nsACString &type,
                const nsACString &host,
                int32_t           port,
                uint32_t          flags,
                nsIProxyInfo    **result)
{
    nsresult rv;
    nsCOMPtr<nsIProtocolProxyService> pps =
            do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
        rv = pps->NewProxyInfo(type, host, port, flags, UINT32_MAX, nullptr,
                               result);
    return rv;
}

nsresult
NS_GetFileProtocolHandler(nsIFileProtocolHandler **result,
                          nsIIOService            *ioService /* = nullptr */)
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

nsresult
NS_GetFileFromURLSpec(const nsACString  &inURL,
                      nsIFile          **result,
                      nsIIOService      *ioService /* = nullptr */)
{
    nsresult rv;
    nsCOMPtr<nsIFileProtocolHandler> fileHandler;
    rv = NS_GetFileProtocolHandler(getter_AddRefs(fileHandler), ioService);
    if (NS_SUCCEEDED(rv))
        rv = fileHandler->GetFileFromURLSpec(inURL, result);
    return rv;
}

nsresult
NS_GetURLSpecFromFile(nsIFile      *file,
                      nsACString   &url,
                      nsIIOService *ioService /* = nullptr */)
{
    nsresult rv;
    nsCOMPtr<nsIFileProtocolHandler> fileHandler;
    rv = NS_GetFileProtocolHandler(getter_AddRefs(fileHandler), ioService);
    if (NS_SUCCEEDED(rv))
        rv = fileHandler->GetURLSpecFromFile(file, url);
    return rv;
}

nsresult
NS_GetURLSpecFromActualFile(nsIFile      *file,
                            nsACString   &url,
                            nsIIOService *ioService /* = nullptr */)
{
    nsresult rv;
    nsCOMPtr<nsIFileProtocolHandler> fileHandler;
    rv = NS_GetFileProtocolHandler(getter_AddRefs(fileHandler), ioService);
    if (NS_SUCCEEDED(rv))
        rv = fileHandler->GetURLSpecFromActualFile(file, url);
    return rv;
}

nsresult
NS_GetURLSpecFromDir(nsIFile      *file,
                     nsACString   &url,
                     nsIIOService *ioService /* = nullptr */)
{
    nsresult rv;
    nsCOMPtr<nsIFileProtocolHandler> fileHandler;
    rv = NS_GetFileProtocolHandler(getter_AddRefs(fileHandler), ioService);
    if (NS_SUCCEEDED(rv))
        rv = fileHandler->GetURLSpecFromDir(file, url);
    return rv;
}

nsresult
NS_GetReferrerFromChannel(nsIChannel *channel,
                          nsIURI **referrer)
{
    nsresult rv = NS_ERROR_NOT_AVAILABLE;
    *referrer = nullptr;

    nsCOMPtr<nsIPropertyBag2> props(do_QueryInterface(channel));
    if (props) {
      // We have to check for a property on a property bag because the
      // referrer may be empty for security reasons (for example, when loading
      // an http page with an https referrer).
      rv = props->GetPropertyAsInterface(NS_LITERAL_STRING("docshell.internalReferrer"),
                                         NS_GET_IID(nsIURI),
                                         reinterpret_cast<void **>(referrer));
      if (NS_FAILED(rv))
        *referrer = nullptr;
    }

    // if that didn't work, we can still try to get the referrer from the
    // nsIHttpChannel (if we can QI to it)
    if (!(*referrer)) {
      nsCOMPtr<nsIHttpChannel> chan(do_QueryInterface(channel));
      if (chan) {
        rv = chan->GetReferrer(referrer);
        if (NS_FAILED(rv))
          *referrer = nullptr;
      }
    }
    return rv;
}

nsresult
NS_ParseRequestContentType(const nsACString &rawContentType,
                           nsCString        &contentType,
                           nsCString        &contentCharset)
{
    // contentCharset is left untouched if not present in rawContentType
    nsresult rv;
    nsCOMPtr<nsINetUtil> util = do_GetNetUtil(&rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCString charset;
    bool hadCharset;
    rv = util->ParseRequestContentType(rawContentType, charset, &hadCharset,
                                       contentType);
    if (NS_SUCCEEDED(rv) && hadCharset)
        contentCharset = charset;
    return rv;
}

nsresult
NS_ParseResponseContentType(const nsACString &rawContentType,
                            nsCString        &contentType,
                            nsCString        &contentCharset)
{
    // contentCharset is left untouched if not present in rawContentType
    nsresult rv;
    nsCOMPtr<nsINetUtil> util = do_GetNetUtil(&rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCString charset;
    bool hadCharset;
    rv = util->ParseResponseContentType(rawContentType, charset, &hadCharset,
                                        contentType);
    if (NS_SUCCEEDED(rv) && hadCharset)
        contentCharset = charset;
    return rv;
}

nsresult
NS_ExtractCharsetFromContentType(const nsACString &rawContentType,
                                 nsCString        &contentCharset,
                                 bool             *hadCharset,
                                 int32_t          *charsetStart,
                                 int32_t          *charsetEnd)
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

nsresult
NS_NewPartialLocalFileInputStream(nsIInputStream **result,
                                  nsIFile         *file,
                                  uint64_t         offset,
                                  uint64_t         length,
                                  int32_t          ioFlags       /* = -1 */,
                                  int32_t          perm          /* = -1 */,
                                  int32_t          behaviorFlags /* = 0 */)
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

nsresult
NS_NewAtomicFileOutputStream(nsIOutputStream **result,
                                nsIFile       *file,
                                int32_t        ioFlags       /* = -1 */,
                                int32_t        perm          /* = -1 */,
                                int32_t        behaviorFlags /* = 0 */)
{
    nsresult rv;
    nsCOMPtr<nsIFileOutputStream> out =
        do_CreateInstance(NS_ATOMICLOCALFILEOUTPUTSTREAM_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = out->Init(file, ioFlags, perm, behaviorFlags);
        if (NS_SUCCEEDED(rv))
            out.forget(result);
    }
    return rv;
}

nsresult
NS_NewSafeLocalFileOutputStream(nsIOutputStream **result,
                                nsIFile          *file,
                                int32_t           ioFlags       /* = -1 */,
                                int32_t           perm          /* = -1 */,
                                int32_t           behaviorFlags /* = 0 */)
{
    nsresult rv;
    nsCOMPtr<nsIFileOutputStream> out =
        do_CreateInstance(NS_SAFELOCALFILEOUTPUTSTREAM_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = out->Init(file, ioFlags, perm, behaviorFlags);
        if (NS_SUCCEEDED(rv))
            out.forget(result);
    }
    return rv;
}

nsresult
NS_NewLocalFileStream(nsIFileStream **result,
                      nsIFile        *file,
                      int32_t         ioFlags       /* = -1 */,
                      int32_t         perm          /* = -1 */,
                      int32_t         behaviorFlags /* = 0 */)
{
    nsresult rv;
    nsCOMPtr<nsIFileStream> stream =
        do_CreateInstance(NS_LOCALFILESTREAM_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = stream->Init(file, ioFlags, perm, behaviorFlags);
        if (NS_SUCCEEDED(rv))
            stream.forget(result);
    }
    return rv;
}

nsresult
NS_BackgroundInputStream(nsIInputStream **result,
                         nsIInputStream  *stream,
                         uint32_t         segmentSize /* = 0 */,
                         uint32_t         segmentCount /* = 0 */)
{
    nsresult rv;
    nsCOMPtr<nsIStreamTransportService> sts =
        do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsITransport> inTransport;
        rv = sts->CreateInputTransport(stream, int64_t(-1), int64_t(-1),
                                       true, getter_AddRefs(inTransport));
        if (NS_SUCCEEDED(rv))
            rv = inTransport->OpenInputStream(nsITransport::OPEN_BLOCKING,
                                              segmentSize, segmentCount,
                                              result);
    }
    return rv;
}

nsresult
NS_BackgroundOutputStream(nsIOutputStream **result,
                          nsIOutputStream  *stream,
                          uint32_t          segmentSize  /* = 0 */,
                          uint32_t          segmentCount /* = 0 */)
{
    nsresult rv;
    nsCOMPtr<nsIStreamTransportService> sts =
        do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsITransport> inTransport;
        rv = sts->CreateOutputTransport(stream, int64_t(-1), int64_t(-1),
                                        true, getter_AddRefs(inTransport));
        if (NS_SUCCEEDED(rv))
            rv = inTransport->OpenOutputStream(nsITransport::OPEN_BLOCKING,
                                               segmentSize, segmentCount,
                                               result);
    }
    return rv;
}

nsresult
NS_NewBufferedOutputStream(nsIOutputStream **result,
                           nsIOutputStream  *str,
                           uint32_t          bufferSize)
{
    nsresult rv;
    nsCOMPtr<nsIBufferedOutputStream> out =
        do_CreateInstance(NS_BUFFEREDOUTPUTSTREAM_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = out->Init(str, bufferSize);
        if (NS_SUCCEEDED(rv)) {
            out.forget(result);
        }
    }
    return rv;
}

already_AddRefed<nsIOutputStream>
NS_BufferOutputStream(nsIOutputStream *aOutputStream,
                      uint32_t aBufferSize)
{
    NS_ASSERTION(aOutputStream, "No output stream given!");

    nsCOMPtr<nsIOutputStream> bos;
    nsresult rv = NS_NewBufferedOutputStream(getter_AddRefs(bos), aOutputStream,
                                             aBufferSize);
    if (NS_SUCCEEDED(rv))
        return bos.forget();

    bos = aOutputStream;
    return bos.forget();
}

already_AddRefed<nsIInputStream>
NS_BufferInputStream(nsIInputStream *aInputStream,
                      uint32_t aBufferSize)
{
    NS_ASSERTION(aInputStream, "No input stream given!");

    nsCOMPtr<nsIInputStream> bis;
    nsresult rv = NS_NewBufferedInputStream(getter_AddRefs(bis), aInputStream,
                                            aBufferSize);
    if (NS_SUCCEEDED(rv))
        return bis.forget();

    bis = aInputStream;
    return bis.forget();
}

nsresult
NS_ReadInputStreamToBuffer(nsIInputStream *aInputStream,
                           void **aDest,
                           uint32_t aCount)
{
    nsresult rv;

    if (!*aDest) {
        *aDest = malloc(aCount);
        if (!*aDest)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    char * p = reinterpret_cast<char*>(*aDest);
    uint32_t bytesRead;
    uint32_t totalRead = 0;
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

#ifdef MOZILLA_INTERNAL_API

nsresult
NS_ReadInputStreamToString(nsIInputStream *aInputStream,
                           nsACString &aDest,
                           uint32_t aCount)
{
    if (!aDest.SetLength(aCount, mozilla::fallible))
        return NS_ERROR_OUT_OF_MEMORY;
    void* dest = aDest.BeginWriting();
    return NS_ReadInputStreamToBuffer(aInputStream, &dest, aCount);
}

#endif

nsresult
NS_LoadPersistentPropertiesFromURISpec(nsIPersistentProperties **outResult,
                                       const nsACString         &aSpec)
{
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri), aSpec);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewChannel(getter_AddRefs(channel),
                       uri,
                       nsContentUtils::GetSystemPrincipal(),
                       nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                       nsIContentPolicy::TYPE_OTHER);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIInputStream> in;
    rv = channel->Open2(getter_AddRefs(in));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPersistentProperties> properties =
      do_CreateInstance(NS_PERSISTENTPROPERTIES_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = properties->Load(in);
    NS_ENSURE_SUCCESS(rv, rv);

    properties.swap(*outResult);
    return NS_OK;
}

bool
NS_UsePrivateBrowsing(nsIChannel *channel)
{
    bool isPrivate = false;
    nsCOMPtr<nsIPrivateBrowsingChannel> pbChannel = do_QueryInterface(channel);
    if (pbChannel && NS_SUCCEEDED(pbChannel->GetIsChannelPrivate(&isPrivate))) {
        return isPrivate;
    }

    // Some channels may not implement nsIPrivateBrowsingChannel
    nsCOMPtr<nsILoadContext> loadContext;
    NS_QueryNotificationCallbacks(channel, loadContext);
    return loadContext && loadContext->UsePrivateBrowsing();
}

bool
NS_GetOriginAttributes(nsIChannel *aChannel,
                       mozilla::NeckoOriginAttributes &aAttributes)
{
    nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
    if (!loadInfo) {
        return false;
    }

    loadInfo->GetOriginAttributes(&aAttributes);
    aAttributes.SyncAttributesWithPrivateBrowsing(NS_UsePrivateBrowsing(aChannel));
    return true;
}

bool
NS_HasBeenCrossOrigin(nsIChannel* aChannel, bool aReport)
{
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  MOZ_RELEASE_ASSERT(loadInfo, "Origin tracking only works for channels created with a loadinfo");

#ifdef DEBUG
  // Don't enforce TYPE_DOCUMENT assertions for loads
  // initiated by javascript tests.
  bool skipContentTypeCheck = false;
  skipContentTypeCheck = Preferences::GetBool("network.loadinfo.skip_type_assertion");
#endif

  MOZ_ASSERT(skipContentTypeCheck ||
             loadInfo->GetExternalContentPolicyType() != nsIContentPolicy::TYPE_DOCUMENT,
             "calling NS_HasBeenCrossOrigin on a top level load");

  // Always treat tainted channels as cross-origin.
  if (loadInfo->GetTainting() != LoadTainting::Basic) {
    return true;
  }

  nsCOMPtr<nsIPrincipal> loadingPrincipal = loadInfo->LoadingPrincipal();
  uint32_t mode = loadInfo->GetSecurityMode();
  bool dataInherits =
    mode == nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_INHERITS ||
    mode == nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_INHERITS ||
    mode == nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS;

  bool aboutBlankInherits = dataInherits && loadInfo->GetAboutBlankInherits();

  for (nsIPrincipal* principal : loadInfo->RedirectChain()) {
    nsCOMPtr<nsIURI> uri;
    principal->GetURI(getter_AddRefs(uri));
    if (!uri) {
      return true;
    }

    if (aboutBlankInherits && NS_IsAboutBlank(uri)) {
      continue;
    }

    if (NS_FAILED(loadingPrincipal->CheckMayLoad(uri, aReport, dataInherits))) {
      return true;
    }
  }

  nsCOMPtr<nsIURI> uri;
  NS_GetFinalChannelURI(aChannel, getter_AddRefs(uri));
  if (!uri) {
    return true;
  }

  if (aboutBlankInherits && NS_IsAboutBlank(uri)) {
    return false;
  }

  return NS_FAILED(loadingPrincipal->CheckMayLoad(uri, aReport, dataInherits));
}

bool
NS_ShouldCheckAppCache(nsIURI *aURI, bool usePrivateBrowsing)
{
    if (usePrivateBrowsing) {
        return false;
    }

    nsCOMPtr<nsIOfflineCacheUpdateService> offlineService =
        do_GetService("@mozilla.org/offlinecacheupdate-service;1");
    if (!offlineService) {
        return false;
    }

    bool allowed;
    nsresult rv = offlineService->OfflineAppAllowedForURI(aURI,
                                                          nullptr,
                                                          &allowed);
    return NS_SUCCEEDED(rv) && allowed;
}

bool
NS_ShouldCheckAppCache(nsIPrincipal *aPrincipal, bool usePrivateBrowsing)
{
    if (usePrivateBrowsing) {
        return false;
    }

    nsCOMPtr<nsIOfflineCacheUpdateService> offlineService =
        do_GetService("@mozilla.org/offlinecacheupdate-service;1");
    if (!offlineService) {
        return false;
    }

    bool allowed;
    nsresult rv = offlineService->OfflineAppAllowed(aPrincipal,
                                                    nullptr,
                                                    &allowed);
    return NS_SUCCEEDED(rv) && allowed;
}

void
NS_WrapAuthPrompt(nsIAuthPrompt   *aAuthPrompt,
                  nsIAuthPrompt2 **aAuthPrompt2)
{
    nsCOMPtr<nsIAuthPromptAdapterFactory> factory =
        do_GetService(NS_AUTHPROMPT_ADAPTER_FACTORY_CONTRACTID);
    if (!factory)
        return;

    NS_WARNING("Using deprecated nsIAuthPrompt");
    factory->CreateAdapter(aAuthPrompt, aAuthPrompt2);
}

void
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

void
NS_QueryAuthPrompt2(nsIChannel      *aChannel,
                    nsIAuthPrompt2 **aAuthPrompt)
{
    *aAuthPrompt = nullptr;

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

nsresult
NS_NewNotificationCallbacksAggregation(nsIInterfaceRequestor  *callbacks,
                                       nsILoadGroup           *loadGroup,
                                       nsIEventTarget         *target,
                                       nsIInterfaceRequestor **result)
{
    nsCOMPtr<nsIInterfaceRequestor> cbs;
    if (loadGroup)
        loadGroup->GetNotificationCallbacks(getter_AddRefs(cbs));
    return NS_NewInterfaceRequestorAggregation(callbacks, cbs, target, result);
}

nsresult
NS_NewNotificationCallbacksAggregation(nsIInterfaceRequestor  *callbacks,
                                       nsILoadGroup           *loadGroup,
                                       nsIInterfaceRequestor **result)
{
    return NS_NewNotificationCallbacksAggregation(callbacks, loadGroup, nullptr, result);
}

nsresult
NS_DoImplGetInnermostURI(nsINestedURI *nestedURI, nsIURI **result)
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

nsresult
NS_ImplGetInnermostURI(nsINestedURI *nestedURI, nsIURI **result)
{
    // Make it safe to use swap()
    *result = nullptr;

    return NS_DoImplGetInnermostURI(nestedURI, result);
}

nsresult
NS_EnsureSafeToReturn(nsIURI *uri, nsIURI **result)
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

void
NS_TryToSetImmutable(nsIURI *uri)
{
    nsCOMPtr<nsIMutable> mutableObj(do_QueryInterface(uri));
    if (mutableObj) {
        mutableObj->SetMutable(false);
    }
}

already_AddRefed<nsIURI>
NS_TryToMakeImmutable(nsIURI *uri,
                      nsresult *outRv /* = nullptr */)
{
    nsresult rv;
    nsCOMPtr<nsINetUtil> util = do_GetNetUtil(&rv);

    nsCOMPtr<nsIURI> result;
    if (NS_SUCCEEDED(rv)) {
        NS_ASSERTION(util, "do_GetNetUtil lied");
        rv = util->ToImmutableURI(uri, getter_AddRefs(result));
    }

    if (NS_FAILED(rv)) {
        result = uri;
    }

    if (outRv) {
        *outRv = rv;
    }

    return result.forget();
}

already_AddRefed<nsIURI>
NS_GetInnermostURI(nsIURI *aURI)
{
    NS_PRECONDITION(aURI, "Must have URI");

    nsCOMPtr<nsIURI> uri = aURI;

    nsCOMPtr<nsINestedURI> nestedURI(do_QueryInterface(uri));
    if (!nestedURI) {
        return uri.forget();
    }

    nsresult rv = nestedURI->GetInnermostURI(getter_AddRefs(uri));
    if (NS_FAILED(rv)) {
        return nullptr;
    }

    return uri.forget();
}

nsresult
NS_GetFinalChannelURI(nsIChannel *channel, nsIURI **uri)
{
    *uri = nullptr;
    nsLoadFlags loadFlags = 0;
    nsresult rv = channel->GetLoadFlags(&loadFlags);
    NS_ENSURE_SUCCESS(rv, rv);

    if (loadFlags & nsIChannel::LOAD_REPLACE) {
        return channel->GetURI(uri);
    }

    return channel->GetOriginalURI(uri);
}

uint32_t
NS_SecurityHashURI(nsIURI *aURI)
{
    nsCOMPtr<nsIURI> baseURI = NS_GetInnermostURI(aURI);

    nsAutoCString scheme;
    uint32_t schemeHash = 0;
    if (NS_SUCCEEDED(baseURI->GetScheme(scheme)))
        schemeHash = mozilla::HashString(scheme);

    // TODO figure out how to hash file:// URIs
    if (scheme.EqualsLiteral("file"))
        return schemeHash; // sad face

    bool hasFlag;
    if (NS_FAILED(NS_URIChainHasFlags(baseURI,
        nsIProtocolHandler::ORIGIN_IS_FULL_SPEC, &hasFlag)) ||
        hasFlag)
    {
        nsAutoCString spec;
        uint32_t specHash;
        nsresult res = baseURI->GetSpec(spec);
        if (NS_SUCCEEDED(res))
            specHash = mozilla::HashString(spec);
        else
            specHash = static_cast<uint32_t>(res);
        return specHash;
    }

    nsAutoCString host;
    uint32_t hostHash = 0;
    if (NS_SUCCEEDED(baseURI->GetAsciiHost(host)))
        hostHash = mozilla::HashString(host);

    return mozilla::AddToHash(schemeHash, hostHash, NS_GetRealPort(baseURI));
}

bool
NS_SecurityCompareURIs(nsIURI *aSourceURI,
                       nsIURI *aTargetURI,
                       bool    aStrictFileOriginPolicy)
{
    // Note that this is not an Equals() test on purpose -- for URIs that don't
    // support host/port, we want equality to basically be object identity, for
    // security purposes.  Otherwise, for example, two javascript: URIs that
    // are otherwise unrelated could end up "same origin", which would be
    // unfortunate.
    if (aSourceURI && aSourceURI == aTargetURI)
    {
        return true;
    }

    if (!aTargetURI || !aSourceURI)
    {
        return false;
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
        return false;

    // Compare schemes
    nsAutoCString targetScheme;
    bool sameScheme = false;
    if (NS_FAILED( targetBaseURI->GetScheme(targetScheme) ) ||
        NS_FAILED( sourceBaseURI->SchemeIs(targetScheme.get(), &sameScheme) ) ||
        !sameScheme)
    {
        // Not same-origin if schemes differ
        return false;
    }

    // For file scheme, reject unless the files are identical. See
    // NS_RelaxStrictFileOriginPolicy for enforcing file same-origin checking
    if (targetScheme.EqualsLiteral("file"))
    {
        // in traditional unsafe behavior all files are the same origin
        if (!aStrictFileOriginPolicy)
            return true;

        nsCOMPtr<nsIFileURL> sourceFileURL(do_QueryInterface(sourceBaseURI));
        nsCOMPtr<nsIFileURL> targetFileURL(do_QueryInterface(targetBaseURI));

        if (!sourceFileURL || !targetFileURL)
            return false;

        nsCOMPtr<nsIFile> sourceFile, targetFile;

        sourceFileURL->GetFile(getter_AddRefs(sourceFile));
        targetFileURL->GetFile(getter_AddRefs(targetFile));

        if (!sourceFile || !targetFile)
            return false;

        // Otherwise they had better match
        bool filesAreEqual = false;
        nsresult rv = sourceFile->Equals(targetFile, &filesAreEqual);
        return NS_SUCCEEDED(rv) && filesAreEqual;
    }

    bool hasFlag;
    if (NS_FAILED(NS_URIChainHasFlags(targetBaseURI,
        nsIProtocolHandler::ORIGIN_IS_FULL_SPEC, &hasFlag)) ||
        hasFlag)
    {
        // URIs with this flag have the whole spec as a distinct trust
        // domain; use the whole spec for comparison
        nsAutoCString targetSpec;
        nsAutoCString sourceSpec;
        return ( NS_SUCCEEDED( targetBaseURI->GetSpec(targetSpec) ) &&
                 NS_SUCCEEDED( sourceBaseURI->GetSpec(sourceSpec) ) &&
                 targetSpec.Equals(sourceSpec) );
    }

    // Compare hosts
    nsAutoCString targetHost;
    nsAutoCString sourceHost;
    if (NS_FAILED( targetBaseURI->GetAsciiHost(targetHost) ) ||
        NS_FAILED( sourceBaseURI->GetAsciiHost(sourceHost) ))
    {
        return false;
    }

    nsCOMPtr<nsIStandardURL> targetURL(do_QueryInterface(targetBaseURI));
    nsCOMPtr<nsIStandardURL> sourceURL(do_QueryInterface(sourceBaseURI));
    if (!targetURL || !sourceURL)
    {
        return false;
    }

#ifdef MOZILLA_INTERNAL_API
    if (!targetHost.Equals(sourceHost, nsCaseInsensitiveCStringComparator() ))
#else
    if (!targetHost.Equals(sourceHost, CaseInsensitiveCompare))
#endif
    {
        return false;
    }

    return NS_GetRealPort(targetBaseURI) == NS_GetRealPort(sourceBaseURI);
}

bool
NS_URIIsLocalFile(nsIURI *aURI)
{
  nsCOMPtr<nsINetUtil> util = do_GetNetUtil();

  bool isFile;
  return util && NS_SUCCEEDED(util->ProtocolHasFlags(aURI,
                                nsIProtocolHandler::URI_IS_LOCAL_FILE,
                                &isFile)) &&
         isFile;
}

bool
NS_RelaxStrictFileOriginPolicy(nsIURI *aTargetURI,
                               nsIURI *aSourceURI,
                               bool aAllowDirectoryTarget /* = false */)
{
  if (!NS_URIIsLocalFile(aTargetURI)) {
    // This is probably not what the caller intended
    NS_NOTREACHED("NS_RelaxStrictFileOriginPolicy called with non-file URI");
    return false;
  }

  if (!NS_URIIsLocalFile(aSourceURI)) {
    // If the source is not also a file: uri then forget it
    // (don't want resource: principals in a file: doc)
    //
    // note: we're not de-nesting jar: uris here, we want to
    // keep archive content bottled up in its own little island
    return false;
  }

  //
  // pull out the internal files
  //
  nsCOMPtr<nsIFileURL> targetFileURL(do_QueryInterface(aTargetURI));
  nsCOMPtr<nsIFileURL> sourceFileURL(do_QueryInterface(aSourceURI));
  nsCOMPtr<nsIFile> targetFile;
  nsCOMPtr<nsIFile> sourceFile;
  bool targetIsDir;

  // Make sure targetFile is not a directory (bug 209234)
  // and that it exists w/out unescaping (bug 395343)
  if (!sourceFileURL || !targetFileURL ||
      NS_FAILED(targetFileURL->GetFile(getter_AddRefs(targetFile))) ||
      NS_FAILED(sourceFileURL->GetFile(getter_AddRefs(sourceFile))) ||
      !targetFile || !sourceFile ||
      NS_FAILED(targetFile->Normalize()) ||
#ifndef MOZ_WIDGET_ANDROID
      NS_FAILED(sourceFile->Normalize()) ||
#endif
      (!aAllowDirectoryTarget &&
       (NS_FAILED(targetFile->IsDirectory(&targetIsDir)) || targetIsDir))) {
    return false;
  }

  //
  // If the file to be loaded is in a subdirectory of the source
  // (or same-dir if source is not a directory) then it will
  // inherit its source principal and be scriptable by that source.
  //
  bool sourceIsDir;
  bool allowed = false;
  nsresult rv = sourceFile->IsDirectory(&sourceIsDir);
  if (NS_SUCCEEDED(rv) && sourceIsDir) {
    rv = sourceFile->Contains(targetFile, &allowed);
  } else {
    nsCOMPtr<nsIFile> sourceParent;
    rv = sourceFile->GetParent(getter_AddRefs(sourceParent));
    if (NS_SUCCEEDED(rv) && sourceParent) {
      rv = sourceParent->Equals(targetFile, &allowed);
      if (NS_FAILED(rv) || !allowed) {
        rv = sourceParent->Contains(targetFile, &allowed);
      } else {
        MOZ_ASSERT(aAllowDirectoryTarget,
                   "sourceFile->Parent == targetFile, but targetFile "
                   "should've been disallowed if it is a directory");
      }
    }
  }

  if (NS_SUCCEEDED(rv) && allowed) {
    return true;
  }

  return false;
}

bool
NS_IsInternalSameURIRedirect(nsIChannel *aOldChannel,
                             nsIChannel *aNewChannel,
                             uint32_t aFlags)
{
  if (!(aFlags & nsIChannelEventSink::REDIRECT_INTERNAL)) {
    return false;
  }

  nsCOMPtr<nsIURI> oldURI, newURI;
  aOldChannel->GetURI(getter_AddRefs(oldURI));
  aNewChannel->GetURI(getter_AddRefs(newURI));

  if (!oldURI || !newURI) {
    return false;
  }

  bool res;
  return NS_SUCCEEDED(oldURI->Equals(newURI, &res)) && res;
}

bool
NS_IsHSTSUpgradeRedirect(nsIChannel *aOldChannel,
                         nsIChannel *aNewChannel,
                         uint32_t aFlags)
{
  if (!(aFlags & nsIChannelEventSink::REDIRECT_STS_UPGRADE)) {
    return false;
  }

  nsCOMPtr<nsIURI> oldURI, newURI;
  aOldChannel->GetURI(getter_AddRefs(oldURI));
  aNewChannel->GetURI(getter_AddRefs(newURI));

  if (!oldURI || !newURI) {
    return false;
  }

  bool isHttp;
  if (NS_FAILED(oldURI->SchemeIs("http", &isHttp)) || !isHttp) {
    return false;
  }

  nsCOMPtr<nsIURI> upgradedURI;
  nsresult rv = NS_GetSecureUpgradedURI(oldURI, getter_AddRefs(upgradedURI));
  if (NS_FAILED(rv)) {
    return false;
  }

  bool res;
  return NS_SUCCEEDED(upgradedURI->Equals(newURI, &res)) && res;
}

nsresult
NS_LinkRedirectChannels(uint32_t channelId,
                        nsIParentChannel *parentChannel,
                        nsIChannel **_result)
{
  nsresult rv;

  nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
      do_GetService("@mozilla.org/redirectchannelregistrar;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return registrar->LinkChannels(channelId,
                                 parentChannel,
                                 _result);
}

#define NS_FAKE_SCHEME "http://"
#define NS_FAKE_TLD ".invalid"
nsresult NS_MakeRandomInvalidURLString(nsCString &result)
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

nsresult NS_MaybeOpenChannelUsingOpen2(nsIChannel* aChannel,
                                       nsIInputStream **aStream)
{
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  if (loadInfo && loadInfo->GetSecurityMode() != 0) {
    return aChannel->Open2(aStream);
  }
  return aChannel->Open(aStream);
}

nsresult NS_MaybeOpenChannelUsingAsyncOpen2(nsIChannel* aChannel,
                                            nsIStreamListener *aListener)
{
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->GetLoadInfo();
  if (loadInfo && loadInfo->GetSecurityMode() != 0) {
    return aChannel->AsyncOpen2(aListener);
  }
  return aChannel->AsyncOpen(aListener, nullptr);
}

nsresult
NS_CheckIsJavaCompatibleURLString(nsCString &urlString, bool *result)
{
  *result = false; // Default to "no"

  nsresult rv = NS_OK;
  nsCOMPtr<nsIURLParser> urlParser =
    do_GetService(NS_STDURLPARSER_CONTRACTID, &rv);
  if (NS_FAILED(rv) || !urlParser)
    return NS_ERROR_FAILURE;

  bool compatible = true;
  uint32_t schemePos = 0;
  int32_t schemeLen = 0;
  urlParser->ParseURL(urlString.get(), -1, &schemePos, &schemeLen,
                      nullptr, nullptr, nullptr, nullptr);
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
      compatible = false;
  } else {
    compatible = false;
  }

  *result = compatible;

  return NS_OK;
}

/** Given the first (disposition) token from a Content-Disposition header,
 * tell whether it indicates the content is inline or attachment
 * @param aDispToken the disposition token from the content-disposition header
 */
uint32_t
NS_GetContentDispositionFromToken(const nsAString &aDispToken)
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
      StringHead(aDispToken, 8).LowerCaseEqualsLiteral("filename"))
    return nsIChannel::DISPOSITION_INLINE;

  return nsIChannel::DISPOSITION_ATTACHMENT;
}

uint32_t
NS_GetContentDispositionFromHeader(const nsACString &aHeader,
                                   nsIChannel *aChan /* = nullptr */)
{
  nsresult rv;
  nsCOMPtr<nsIMIMEHeaderParam> mimehdrpar = do_GetService(NS_MIMEHEADERPARAM_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return nsIChannel::DISPOSITION_ATTACHMENT;

  nsAutoCString fallbackCharset;
  if (aChan) {
    nsCOMPtr<nsIURI> uri;
    aChan->GetURI(getter_AddRefs(uri));
    if (uri)
      uri->GetOriginCharset(fallbackCharset);
  }

  nsAutoString dispToken;
  rv = mimehdrpar->GetParameterHTTP(aHeader, "", fallbackCharset, true, nullptr,
                                    dispToken);

  if (NS_FAILED(rv)) {
    // special case (see bug 272541): empty disposition type handled as "inline"
    if (rv == NS_ERROR_FIRST_HEADER_FIELD_COMPONENT_EMPTY)
        return nsIChannel::DISPOSITION_INLINE;
    return nsIChannel::DISPOSITION_ATTACHMENT;
  }

  return NS_GetContentDispositionFromToken(dispToken);
}

nsresult
NS_GetFilenameFromDisposition(nsAString &aFilename,
                              const nsACString &aDisposition,
                              nsIURI *aURI /* = nullptr */)
{
  aFilename.Truncate();

  nsresult rv;
  nsCOMPtr<nsIMIMEHeaderParam> mimehdrpar =
      do_GetService(NS_MIMEHEADERPARAM_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);

  nsAutoCString fallbackCharset;
  if (url)
    url->GetOriginCharset(fallbackCharset);
  // Get the value of 'filename' parameter
  rv = mimehdrpar->GetParameterHTTP(aDisposition, "filename",
                                    fallbackCharset, true, nullptr,
                                    aFilename);

  if (NS_FAILED(rv)) {
    aFilename.Truncate();
    return rv;
  }

  if (aFilename.IsEmpty())
    return NS_ERROR_NOT_AVAILABLE;

  return NS_OK;
}

void net_EnsurePSMInit()
{
    nsresult rv;
    nsCOMPtr<nsISupports> psm = do_GetService(PSM_COMPONENT_CONTRACTID, &rv);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
}

bool NS_IsAboutBlank(nsIURI *uri)
{
    // GetSpec can be expensive for some URIs, so check the scheme first.
    bool isAbout = false;
    if (NS_FAILED(uri->SchemeIs("about", &isAbout)) || !isAbout) {
        return false;
    }

    return uri->GetSpecOrDefault().EqualsLiteral("about:blank");
}

nsresult
NS_GenerateHostPort(const nsCString& host, int32_t port,
                    nsACString &hostLine)
{
    if (strchr(host.get(), ':')) {
        // host is an IPv6 address literal and must be encapsulated in []'s
        hostLine.Assign('[');
        // scope id is not needed for Host header.
        int scopeIdPos = host.FindChar('%');
        if (scopeIdPos == -1)
            hostLine.Append(host);
        else if (scopeIdPos > 0)
            hostLine.Append(Substring(host, 0, scopeIdPos));
        else
          return NS_ERROR_MALFORMED_URI;
        hostLine.Append(']');
    }
    else
        hostLine.Assign(host);
    if (port != -1) {
        hostLine.Append(':');
        hostLine.AppendInt(port);
    }
    return NS_OK;
}

void
NS_SniffContent(const char *aSnifferType, nsIRequest *aRequest,
                const uint8_t *aData, uint32_t aLength,
                nsACString &aSniffedType)
{
  typedef nsCategoryCache<nsIContentSniffer> ContentSnifferCache;
  extern ContentSnifferCache* gNetSniffers;
  extern ContentSnifferCache* gDataSniffers;
  ContentSnifferCache* cache = nullptr;
  if (!strcmp(aSnifferType, NS_CONTENT_SNIFFER_CATEGORY)) {
    if (!gNetSniffers) {
      gNetSniffers = new ContentSnifferCache(NS_CONTENT_SNIFFER_CATEGORY);
    }
    cache = gNetSniffers;
  } else if (!strcmp(aSnifferType, NS_DATA_SNIFFER_CATEGORY)) {
    if (!gDataSniffers) {
      gDataSniffers = new ContentSnifferCache(NS_DATA_SNIFFER_CATEGORY);
    }
    cache = gDataSniffers;
  } else {
    // Invalid content sniffer type was requested
    MOZ_ASSERT(false);
    return;
  }

  nsCOMArray<nsIContentSniffer> sniffers;
  cache->GetEntries(sniffers);
  for (int32_t i = 0; i < sniffers.Count(); ++i) {
    nsresult rv = sniffers[i]->GetMIMETypeFromContent(aRequest, aData, aLength, aSniffedType);
    if (NS_SUCCEEDED(rv) && !aSniffedType.IsEmpty()) {
      return;
    }
  }

  aSniffedType.Truncate();
}

bool
NS_IsSrcdocChannel(nsIChannel *aChannel)
{
  bool isSrcdoc;
  nsCOMPtr<nsIInputStreamChannel> isr = do_QueryInterface(aChannel);
  if (isr) {
    isr->GetIsSrcdocChannel(&isSrcdoc);
    return isSrcdoc;
  }
  nsCOMPtr<nsIViewSourceChannel> vsc = do_QueryInterface(aChannel);
  if (vsc) {
    vsc->GetIsSrcdocChannel(&isSrcdoc);
    return isSrcdoc;
  }
  return false;
}

nsresult
NS_ShouldSecureUpgrade(nsIURI* aURI,
                       nsILoadInfo* aLoadInfo,
                       nsIPrincipal* aChannelResultPrincipal,
                       bool aPrivateBrowsing,
                       bool aAllowSTS,
                       bool& aShouldUpgrade)
{
  // Even if we're in private browsing mode, we still enforce existing STS
  // data (it is read-only).
  // if the connection is not using SSL and either the exact host matches or
  // a superdomain wants to force HTTPS, do it.
  bool isHttps = false;
  nsresult rv = aURI->SchemeIs("https", &isHttps);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!isHttps) {
    // If any of the documents up the chain to the root doucment makes use of
    // the CSP directive 'upgrade-insecure-requests', then it's time to fulfill
    // the promise to CSP and mixed content blocking to upgrade the channel
    // from http to https.
    if (aLoadInfo) {
      // Please note that cross origin top level navigations are not subject
      // to upgrade-insecure-requests, see:
      // http://www.w3.org/TR/upgrade-insecure-requests/#examples
      // Compare the principal we are navigating to (aChannelResultPrincipal)
      // with the referring/triggering Principal.
      bool crossOriginNavigation =
        (aLoadInfo->GetExternalContentPolicyType() == nsIContentPolicy::TYPE_DOCUMENT) &&
        (!aChannelResultPrincipal->Equals(aLoadInfo->TriggeringPrincipal()));

      if (aLoadInfo->GetUpgradeInsecureRequests() && !crossOriginNavigation) {
        // let's log a message to the console that we are upgrading a request
        nsAutoCString scheme;
        aURI->GetScheme(scheme);
        // append the additional 's' for security to the scheme :-)
        scheme.AppendASCII("s");
        NS_ConvertUTF8toUTF16 reportSpec(aURI->GetSpecOrDefault());
        NS_ConvertUTF8toUTF16 reportScheme(scheme);

        const char16_t* params[] = { reportSpec.get(), reportScheme.get() };
        uint32_t innerWindowId = aLoadInfo->GetInnerWindowID();
        CSP_LogLocalizedStr(u"upgradeInsecureRequest",
                            params, ArrayLength(params),
                            EmptyString(), // aSourceFile
                            EmptyString(), // aScriptSample
                            0, // aLineNumber
                            0, // aColumnNumber
                            nsIScriptError::warningFlag, "CSP",
                            innerWindowId);

        Telemetry::Accumulate(Telemetry::HTTP_SCHEME_UPGRADE, 4);
        aShouldUpgrade = true;
        return NS_OK;
      }
    }

    // enforce Strict-Transport-Security
    nsISiteSecurityService* sss = gHttpHandler->GetSSService();
    NS_ENSURE_TRUE(sss, NS_ERROR_OUT_OF_MEMORY);

    bool isStsHost = false;
    uint32_t flags = aPrivateBrowsing ? nsISocketProvider::NO_PERMANENT_STORAGE : 0;
    rv = sss->IsSecureURI(nsISiteSecurityService::HEADER_HSTS, aURI, flags,
                          nullptr, &isStsHost);

    // if the SSS check fails, it's likely because this load is on a
    // malformed URI or something else in the setup is wrong, so any error
    // should be reported.
    NS_ENSURE_SUCCESS(rv, rv);

    if (isStsHost) {
      LOG(("nsHttpChannel::Connect() STS permissions found\n"));
      if (aAllowSTS) {
        Telemetry::Accumulate(Telemetry::HTTP_SCHEME_UPGRADE, 3);
        aShouldUpgrade = true;
        return NS_OK;
      } else {
        Telemetry::Accumulate(Telemetry::HTTP_SCHEME_UPGRADE, 2);
      }
    } else {
      Telemetry::Accumulate(Telemetry::HTTP_SCHEME_UPGRADE, 1);
    }
  } else {
    Telemetry::Accumulate(Telemetry::HTTP_SCHEME_UPGRADE, 0);
  }
  aShouldUpgrade = false;
  return NS_OK;
}

nsresult
NS_GetSecureUpgradedURI(nsIURI* aURI, nsIURI** aUpgradedURI)
{
  nsCOMPtr<nsIURI> upgradedURI;

  nsresult rv = aURI->Clone(getter_AddRefs(upgradedURI));
  NS_ENSURE_SUCCESS(rv,rv);

  // Change the scheme to HTTPS:
  upgradedURI->SetScheme(NS_LITERAL_CSTRING("https"));

  // Change the default port to 443:
  nsCOMPtr<nsIStandardURL> upgradedStandardURL = do_QueryInterface(upgradedURI);
  if (upgradedStandardURL) {
    upgradedStandardURL->SetDefaultPort(443);
  } else {
    // If we don't have a nsStandardURL, fall back to using GetPort/SetPort.
    // XXXdholbert Is this function even called with a non-nsStandardURL arg,
    // in practice?
    int32_t oldPort = -1;
    rv = aURI->GetPort(&oldPort);
    if (NS_FAILED(rv)) return rv;

    // Keep any nonstandard ports so only the scheme is changed.
    // For example:
    //  http://foo.com:80 -> https://foo.com:443
    //  http://foo.com:81 -> https://foo.com:81

    if (oldPort == 80 || oldPort == -1) {
        upgradedURI->SetPort(-1);
    } else {
        upgradedURI->SetPort(oldPort);
    }
  }

  upgradedURI.forget(aUpgradedURI);
  return NS_OK;
}

nsresult
NS_CompareLoadInfoAndLoadContext(nsIChannel *aChannel)
{
  nsCOMPtr<nsILoadInfo> loadInfo;
  aChannel->GetLoadInfo(getter_AddRefs(loadInfo));

  nsCOMPtr<nsILoadContext> loadContext;
  NS_QueryNotificationCallbacks(aChannel, loadContext);
  if (!loadInfo || !loadContext) {
    return NS_OK;
  }

  // We try to skip about:newtab and about:sync-tabs.
  // about:newtab will use SystemPrincipal to download thumbnails through
  // https:// and blob URLs.
  // about:sync-tabs will fetch icons through moz-icon://.
  bool isAboutPage = false;
  nsINode* node = loadInfo->LoadingNode();
  if (node) {
    nsIDocument* doc = node->OwnerDoc();
    if (doc) {
      nsIURI* uri = doc->GetDocumentURI();
      nsresult rv = uri->SchemeIs("about", &isAboutPage);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  if (isAboutPage) {
    return NS_OK;
  }

  // We skip the favicon loading here. The favicon loading might be
  // triggered by the XUL image. For that case, the loadContext will have
  // default originAttributes since the XUL image uses SystemPrincipal, but
  // the loadInfo will use originAttributes from the content. Thus, the
  // originAttributes between loadInfo and loadContext will be different.
  // That's why we have to skip the comparison for the favicon loading.
  if (nsContentUtils::IsSystemPrincipal(loadInfo->LoadingPrincipal()) &&
      loadInfo->InternalContentPolicyType() ==
        nsIContentPolicy::TYPE_INTERNAL_IMAGE_FAVICON) {
    return NS_OK;
  }

  bool loadContextIsInBE = false;
  nsresult rv = loadContext->GetIsInIsolatedMozBrowserElement(&loadContextIsInBE);
  if (NS_FAILED(rv)) {
    return NS_ERROR_UNEXPECTED;
  }

  OriginAttributes originAttrsLoadInfo = loadInfo->GetOriginAttributes();
  DocShellOriginAttributes originAttrsLoadContext;
  loadContext->GetOriginAttributes(originAttrsLoadContext);

  LOG(("NS_CompareLoadInfoAndLoadContext - loadInfo: %d, %d, %d, %d; "
       "loadContext: %d %d, %d. [channel=%p]",
       originAttrsLoadInfo.mAppId, originAttrsLoadInfo.mInIsolatedMozBrowser,
       originAttrsLoadInfo.mUserContextId, originAttrsLoadInfo.mPrivateBrowsingId,
       loadContextIsInBE,
       originAttrsLoadContext.mUserContextId, originAttrsLoadContext.mPrivateBrowsingId,
       aChannel));

  MOZ_ASSERT(originAttrsLoadInfo.mInIsolatedMozBrowser ==
             loadContextIsInBE,
             "The value of InIsolatedMozBrowser in the loadContext and in "
             "the loadInfo are not the same!");

  MOZ_ASSERT(originAttrsLoadInfo.mUserContextId ==
             originAttrsLoadContext.mUserContextId,
             "The value of mUserContextId in the loadContext and in the "
             "loadInfo are not the same!");

  MOZ_ASSERT(originAttrsLoadInfo.mPrivateBrowsingId ==
             originAttrsLoadContext.mPrivateBrowsingId,
             "The value of mPrivateBrowsingId in the loadContext and in the "
             "loadInfo are not the same!");

  return NS_OK;
}

namespace mozilla {
namespace net {

bool
InScriptableRange(int64_t val)
{
    return (val <= kJS_MAX_SAFE_INTEGER) && (val >= kJS_MIN_SAFE_INTEGER);
}

bool
InScriptableRange(uint64_t val)
{
    return val <= kJS_MAX_SAFE_UINTEGER;
}

} // namespace net
} // namespace mozilla
