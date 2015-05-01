/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNetUtil_h__
#define nsNetUtil_h__

#include "nsError.h"
#include "nsNetCID.h"
#include "nsStringGlue.h"
#include "nsMemory.h"
#include "nsCOMPtr.h"
#include "prio.h" // for read/write flags, permissions, etc.
#include "nsHashKeys.h"

#include "plstr.h"
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
#include "nsIFile.h"
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
#include "nsISocketProviderService.h"
#include "nsISocketProvider.h"
#include "nsIRedirectChannelRegistrar.h"
#include "nsIMIMEHeaderParam.h"
#include "nsILoadContext.h"
#include "nsIScriptSecurityManager.h"
#include "mozilla/Services.h"
#include "nsIPrivateBrowsingChannel.h"
#include "mozIApplicationClearPrivateDataParams.h"
#include "nsIOfflineCacheUpdate.h"
#include "nsIContentSniffer.h"
#include "nsCategoryCache.h"
#include "nsStringStream.h"
#include "nsIViewSourceChannel.h"
#include "mozilla/LoadInfo.h"
#include "nsINode.h"

#include <limits>

#ifdef MOZILLA_INTERNAL_API

#include "nsReadableUtils.h"

inline already_AddRefed<nsIIOService>
do_GetIOService(nsresult* error = 0)
{
    nsCOMPtr<nsIIOService> io = mozilla::services::GetIOService();
    if (error)
        *error = io ? NS_OK : NS_ERROR_FAILURE;
    return io.forget();
}

inline already_AddRefed<nsINetUtil>
do_GetNetUtil(nsresult *error = 0) 
{
    nsCOMPtr<nsIIOService> io = mozilla::services::GetIOService();
    nsCOMPtr<nsINetUtil> util;
    if (io)
        util = do_QueryInterface(io);

    if (error)
        *error = !!util ? NS_OK : NS_ERROR_FAILURE;
    return util.forget();
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
          const char *charset = nullptr,
          nsIURI *baseURI = nullptr,
          nsIIOService *ioService = nullptr)     // pass in nsIIOService to optimize callers
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
          const char *charset = nullptr,
          nsIURI* baseURI = nullptr,
          nsIIOService* ioService = nullptr)     // pass in nsIIOService to optimize callers
{
    return NS_NewURI(result, NS_ConvertUTF16toUTF8(spec), charset, baseURI, ioService);
}

inline nsresult
NS_NewURI(nsIURI* *result, 
          const char *spec,
          nsIURI* baseURI = nullptr,
          nsIIOService* ioService = nullptr)     // pass in nsIIOService to optimize callers
{
    return NS_NewURI(result, nsDependentCString(spec), nullptr, baseURI, ioService);
}

inline nsresult
NS_NewFileURI(nsIURI* *result, 
              nsIFile* spec, 
              nsIIOService* ioService = nullptr)     // pass in nsIIOService to optimize callers
{
    nsresult rv;
    nsCOMPtr<nsIIOService> grip;
    rv = net_EnsureIOService(&ioService, grip);
    if (ioService)
        rv = ioService->NewFileURI(spec, result);
    return rv;
}

/*
* How to create a new Channel, using NS_NewChannel,
* NS_NewChannelWithTriggeringPrincipal,
* NS_NewInputStreamChannel, NS_NewChannelInternal
* and it's variations:
*
* What specific API function to use:
* * The NS_NewChannelInternal functions should almost never be directly
*   called outside of necko code.
* * If possible, use NS_NewChannel() providing a loading *nsINode*
* * If no loading *nsINode* is avaialable, call NS_NewChannel() providing
*   a loading *nsIPrincipal*.
* * Call NS_NewChannelWithTriggeringPrincipal if the triggeringPrincipal
*   is different from the loadingPrincipal.
* * Call NS_NewChannelInternal() providing aLoadInfo object in cases where
*   you already have loadInfo object, e.g in case of a channel redirect.
*
* @param aURI
*        nsIURI from which to make a channel
* @param aLoadingNode
*        The loadingDocument of the channel.
*        The element or document where the result of this request will be
*        used. This is the document/element that will get access to the
*        result of this request. For example for an image load, it's the
*        document in which the image will be loaded. And for a CSS
*        stylesheet it's the document whose rendering will be affected by
*        the stylesheet.
*        If possible, pass in the element which is performing the load. But
*        if the load is coming from a JS API (such as XMLHttpRequest) or if
*        the load might be coalesced across multiple elements (such as
*        for <img>) then pass in the Document node instead.
*        For loads that are not related to any document, such as loads coming
*        from addons or internal browser features, use null here.
* @param aLoadingPrincipal
*        The loadingPrincipal of the channel.
*        The principal of the document where the result of this request will
*        be used.
*        This defaults to the principal of aLoadingNode, so when aLoadingNode
*        is passed this can be left as null. However for loads where
*        aLoadingNode is null this argument must be passed.
*        For example for loads from a WebWorker, pass the principal
*        of that worker. For loads from an addon or from internal browser
*        features, pass the system principal.
*        This principal should almost always be the system principal if
*        aLoadingNode is null. The only exception to this is for loads
*        from WebWorkers since they don't have any nodes to be passed as
*        aLoadingNode.
*        Please note, aLoadingPrincipal is *not* the principal of the
*        resource being loaded. But rather the principal of the context
*        where the resource will be used.
* @param aTriggeringPrincipal
*        The triggeringPrincipal of the load.
*        The triggeringPrincipal is the principal of the resource that caused
*        this particular URL to be loaded.
*        Most likely the triggeringPrincipal and the loadingPrincipal are
*        identical, in which case the triggeringPrincipal can be left out.
*        In some cases the loadingPrincipal and the triggeringPrincipal are
*        different however, e.g. a stylesheet may import a subresource. In
*        that case the principal of the stylesheet which contains the
*        import command is the triggeringPrincipal, and the principal of
*        the document whose rendering is affected is the loadingPrincipal.
* @param aSecurityFlags
*        The securityFlags of the channel.
*        Any of the securityflags defined in nsILoadInfo.idl
* @param aContentPolicyType
*        The contentPolicyType of the channel.
*        Any of the content types defined in nsIContentPolicy.idl
*
* Please note, if you provide both a loadingNode and a loadingPrincipal,
* then loadingPrincipal must be equal to loadingNode->NodePrincipal().
* But less error prone is to just supply a loadingNode.
*
* Keep in mind that URIs coming from a webpage should *never* use the
* systemPrincipal as the loadingPrincipal.
*/
inline nsresult
NS_NewChannelInternal(nsIChannel**           outChannel,
                      nsIURI*                aUri,
                      nsINode*               aLoadingNode,
                      nsIPrincipal*          aLoadingPrincipal,
                      nsIPrincipal*          aTriggeringPrincipal,
                      nsSecurityFlags        aSecurityFlags,
                      nsContentPolicyType    aContentPolicyType,
                      nsILoadGroup*          aLoadGroup = nullptr,
                      nsIInterfaceRequestor* aCallbacks = nullptr,
                      nsLoadFlags            aLoadFlags = nsIRequest::LOAD_NORMAL,
                      nsIIOService*          aIoService = nullptr)
{
  NS_ENSURE_ARG_POINTER(outChannel);

  nsCOMPtr<nsIIOService> grip;
  nsresult rv = net_EnsureIOService(&aIoService, grip);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  rv = aIoService->NewChannelFromURI2(
         aUri,
         aLoadingNode ?
           aLoadingNode->AsDOMNode() : nullptr,
         aLoadingPrincipal,
         aTriggeringPrincipal,
         aSecurityFlags,
         aContentPolicyType,
         getter_AddRefs(channel));
  NS_ENSURE_SUCCESS(rv, rv);

  if (aLoadGroup) {
    rv = channel->SetLoadGroup(aLoadGroup);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aCallbacks) {
    rv = channel->SetNotificationCallbacks(aCallbacks);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aLoadFlags != nsIRequest::LOAD_NORMAL) {
    // Retain the LOAD_REPLACE load flag if set.
    nsLoadFlags normalLoadFlags = 0;
    channel->GetLoadFlags(&normalLoadFlags);
    rv = channel->SetLoadFlags(aLoadFlags | (normalLoadFlags & nsIChannel::LOAD_REPLACE));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  channel.forget(outChannel);
  return NS_OK;
}

// See NS_NewChannelInternal for usage and argument description
inline nsresult
NS_NewChannelInternal(nsIChannel**           outChannel,
                      nsIURI*                aUri,
                      nsILoadInfo*           aLoadInfo,
                      nsILoadGroup*          aLoadGroup = nullptr,
                      nsIInterfaceRequestor* aCallbacks = nullptr,
                      nsLoadFlags            aLoadFlags = nsIRequest::LOAD_NORMAL,
                      nsIIOService*          aIoService = nullptr)
{
  // NS_NewChannelInternal is mostly called for channel redirects. We should allow
  // the creation of a channel even if the original channel did not have a loadinfo
  // attached.
  if (!aLoadInfo) {
    return NS_NewChannelInternal(outChannel,
                                 aUri,
                                 nullptr, // aLoadingNode
                                 nullptr, // aLoadingPrincipal
                                 nullptr, // aTriggeringPrincipal
                                 nsILoadInfo::SEC_NORMAL,
                                 nsIContentPolicy::TYPE_OTHER,
                                 aLoadGroup,
                                 aCallbacks,
                                 aLoadFlags,
                                 aIoService);
  }
  nsresult rv = NS_NewChannelInternal(outChannel,
                                      aUri,
                                      aLoadInfo->LoadingNode(),
                                      aLoadInfo->LoadingPrincipal(),
                                      aLoadInfo->TriggeringPrincipal(),
                                      aLoadInfo->GetSecurityFlags(),
                                      aLoadInfo->GetContentPolicyType(),
                                      aLoadGroup,
                                      aCallbacks,
                                      aLoadFlags,
                                      aIoService);
  NS_ENSURE_SUCCESS(rv, rv);
  // Please note that we still call SetLoadInfo on the channel because
  // we want the same instance of the loadInfo to be set on the channel.
  (*outChannel)->SetLoadInfo(aLoadInfo);
  return NS_OK;
}

// See NS_NewChannelInternal for usage and argument description
inline nsresult /*NS_NewChannelWithNodeAndTriggeringPrincipal */
NS_NewChannelWithTriggeringPrincipal(nsIChannel**           outChannel,
                                     nsIURI*                aUri,
                                     nsINode*               aLoadingNode,
                                     nsIPrincipal*          aTriggeringPrincipal,
                                     nsSecurityFlags        aSecurityFlags,
                                     nsContentPolicyType    aContentPolicyType,
                                     nsILoadGroup*          aLoadGroup = nullptr,
                                     nsIInterfaceRequestor* aCallbacks = nullptr,
                                     nsLoadFlags            aLoadFlags = nsIRequest::LOAD_NORMAL,
                                     nsIIOService*          aIoService = nullptr)
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
inline nsresult /*NS_NewChannelWithPrincipalAndTriggeringPrincipal */
NS_NewChannelWithTriggeringPrincipal(nsIChannel**           outChannel,
                                     nsIURI*                aUri,
                                     nsIPrincipal*          aLoadingPrincipal,
                                     nsIPrincipal*          aTriggeringPrincipal,
                                     nsSecurityFlags        aSecurityFlags,
                                     nsContentPolicyType    aContentPolicyType,
                                     nsILoadGroup*          aLoadGroup = nullptr,
                                     nsIInterfaceRequestor* aCallbacks = nullptr,
                                     nsLoadFlags            aLoadFlags = nsIRequest::LOAD_NORMAL,
                                     nsIIOService*          aIoService = nullptr)
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

// See NS_NewChannelInternal for usage and argument description
inline nsresult /* NS_NewChannelNode */
NS_NewChannel(nsIChannel**           outChannel,
              nsIURI*                aUri,
              nsINode*               aLoadingNode,
              nsSecurityFlags        aSecurityFlags,
              nsContentPolicyType    aContentPolicyType,
              nsILoadGroup*          aLoadGroup = nullptr,
              nsIInterfaceRequestor* aCallbacks = nullptr,
              nsLoadFlags            aLoadFlags = nsIRequest::LOAD_NORMAL,
              nsIIOService*          aIoService = nullptr)
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

// See NS_NewChannelInternal for usage and argument description
inline nsresult /* NS_NewChannelPrincipal */
NS_NewChannel(nsIChannel**           outChannel,
              nsIURI*                aUri,
              nsIPrincipal*          aLoadingPrincipal,
              nsSecurityFlags        aSecurityFlags,
              nsContentPolicyType    aContentPolicyType,
              nsILoadGroup*          aLoadGroup = nullptr,
              nsIInterfaceRequestor* aCallbacks = nullptr,
              nsLoadFlags            aLoadFlags = nsIRequest::LOAD_NORMAL,
              nsIIOService*          aIoService = nullptr)
{
  return NS_NewChannelInternal(outChannel,
                               aUri,
                               nullptr, // aLoadingNode,
                               aLoadingPrincipal,
                               nullptr, // aTriggeringPrincipal
                               aSecurityFlags,
                               aContentPolicyType,
                               aLoadGroup,
                               aCallbacks,
                               aLoadFlags,
                               aIoService);
}

inline nsresult
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

inline nsresult
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

inline nsresult
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

/**
 * This function is a helper function to get a scheme's default port.
 */
inline int32_t
NS_GetDefaultPort(const char *scheme,
                  nsIIOService* ioService = nullptr)
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
inline bool
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

/**
 * This function is a helper function to get a protocol's default port if the
 * URI does not specify a port explicitly. Returns -1 if this protocol has no
 * concept of ports or if there was an error getting the port.
 */
inline int32_t
NS_GetRealPort(nsIURI* aURI)
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

inline nsresult /* NS_NewInputStreamChannelWithLoadInfo */
NS_NewInputStreamChannelInternal(nsIChannel**        outChannel,
                                 nsIURI*             aUri,
                                 nsIInputStream*     aStream,
                                 const nsACString&   aContentType,
                                 const nsACString&   aContentCharset,
                                 nsILoadInfo*        aLoadInfo)
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

inline nsresult
NS_NewInputStreamChannelInternal(nsIChannel**        outChannel,
                                 nsIURI*             aUri,
                                 nsIInputStream*     aStream,
                                 const nsACString&   aContentType,
                                 const nsACString&   aContentCharset,
                                 nsINode*            aLoadingNode,
                                 nsIPrincipal*       aLoadingPrincipal,
                                 nsIPrincipal*       aTriggeringPrincipal,
                                 nsSecurityFlags     aSecurityFlags,
                                 nsContentPolicyType aContentPolicyType,
                                 nsIURI*             aBaseURI = nullptr)
{
  nsCOMPtr<nsILoadInfo> loadInfo =
    new mozilla::LoadInfo(aLoadingPrincipal,
                          aTriggeringPrincipal,
                          aLoadingNode,
                          aSecurityFlags,
                          aContentPolicyType,
                          aBaseURI);
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

inline nsresult /* NS_NewInputStreamChannelPrincipal */
NS_NewInputStreamChannel(nsIChannel**        outChannel,
                         nsIURI*             aUri,
                         nsIInputStream*     aStream,
                         nsIPrincipal*       aLoadingPrincipal,
                         nsSecurityFlags     aSecurityFlags,
                         nsContentPolicyType aContentPolicyType,
                         const nsACString&   aContentType    = EmptyCString(),
                         const nsACString&   aContentCharset = EmptyCString())
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

inline nsresult
NS_NewInputStreamChannelInternal(nsIChannel**        outChannel,
                                 nsIURI*             aUri,
                                 const nsAString&    aData,
                                 const nsACString&   aContentType,
                                 nsINode*            aLoadingNode,
                                 nsIPrincipal*       aLoadingPrincipal,
                                 nsIPrincipal*       aTriggeringPrincipal,
                                 nsSecurityFlags     aSecurityFlags,
                                 nsContentPolicyType aContentPolicyType,
                                 bool                aIsSrcdocChannel = false,
                                 nsIURI*             aBaseURI = nullptr)
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
                                        aLoadingNode,
                                        aLoadingPrincipal,
                                        aTriggeringPrincipal,
                                        aSecurityFlags,
                                        aContentPolicyType,
                                        aBaseURI);

  NS_ENSURE_SUCCESS(rv, rv);

  if (aIsSrcdocChannel) {
    nsCOMPtr<nsIInputStreamChannel> inStrmChan = do_QueryInterface(channel);
    NS_ENSURE_TRUE(inStrmChan, NS_ERROR_FAILURE);
    inStrmChan->SetSrcdocData(aData);
  }
  channel.forget(outChannel);
  return NS_OK;
}

inline nsresult
NS_NewInputStreamChannel(nsIChannel**        outChannel,
                         nsIURI*             aUri,
                         const nsAString&    aData,
                         const nsACString&   aContentType,
                         nsIPrincipal*       aLoadingPrincipal,
                         nsSecurityFlags     aSecurityFlags,
                         nsContentPolicyType aContentPolicyType,
                         bool                aIsSrcdocChannel = false,
                         nsIURI*             aBaseURI = nullptr)
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
                                          aIsSrcdocChannel,
                                          aBaseURI);
}

inline nsresult
NS_NewInputStreamPump(nsIInputStreamPump **result,
                      nsIInputStream      *stream,
                      int64_t              streamPos = int64_t(-1),
                      int64_t              streamLen = int64_t(-1),
                      uint32_t             segsize = 0,
                      uint32_t             segcount = 0,
                      bool                 closeWhenDone = false)
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
                        uint32_t               chunkSize = 0,
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
            *result = nullptr;
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
            *result = nullptr;
            group.swap(*result);
        }
    }
    return rv;
}

// Create a new nsILoadGroup that will match the given principal.
nsresult
NS_NewLoadGroup(nsILoadGroup** aResult, nsIPrincipal* aPrincipal);

// Determine if the given loadGroup/principal pair will produce a principal
// with similar permissions when passed to NS_NewChannel().  This checks for
// things like making sure the appId and browser element flags match.  Without
// an appropriate load group these values can be lost when getting the result
// principal back out of the channel.  Null principals are also always allowed
// as they do not have permissions to actually use the load group.
bool
NS_LoadGroupMatchesPrincipal(nsILoadGroup* aLoadGroup,
                             nsIPrincipal* aPrincipal);

inline nsresult
NS_NewDownloader(nsIStreamListener   **result,
                 nsIDownloadObserver  *observer,
                 nsIFile              *downloadLocation = nullptr)
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
            *result = nullptr;
            loader.swap(*result);
        }
    }
    return rv;
}

inline nsresult
NS_NewStreamLoaderInternal(nsIStreamLoader**        outStream,
                           nsIURI*                  aUri,
                           nsIStreamLoaderObserver* aObserver,
                           nsINode*                 aLoadingNode,
                           nsIPrincipal*            aLoadingPrincipal,
                           nsSecurityFlags          aSecurityFlags,
                           nsContentPolicyType      aContentPolicyType,
                           nsISupports*             aContext = nullptr,
                           nsILoadGroup*            aLoadGroup = nullptr,
                           nsIInterfaceRequestor*   aCallbacks = nullptr,
                           nsLoadFlags              aLoadFlags = nsIRequest::LOAD_NORMAL,
                           nsIURI*                  aReferrer = nullptr)
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
  return channel->AsyncOpen(*outStream, aContext);
}


inline nsresult /* NS_NewStreamLoaderNode */
NS_NewStreamLoader(nsIStreamLoader**        outStream,
                   nsIURI*                  aUri,
                   nsIStreamLoaderObserver* aObserver,
                   nsINode*                 aLoadingNode,
                   nsSecurityFlags          aSecurityFlags,
                   nsContentPolicyType      aContentPolicyType,
                   nsISupports*             aContext = nullptr,
                   nsILoadGroup*            aLoadGroup = nullptr,
                   nsIInterfaceRequestor*   aCallbacks = nullptr,
                   nsLoadFlags              aLoadFlags = nsIRequest::LOAD_NORMAL,
                   nsIURI*                  aReferrer = nullptr)
{
  NS_ASSERTION(aLoadingNode, "Can not create stream loader without a loading Node!");
  return NS_NewStreamLoaderInternal(outStream,
                                    aUri,
                                    aObserver,
                                    aLoadingNode,
                                    aLoadingNode->NodePrincipal(),
                                    aSecurityFlags,
                                    aContentPolicyType,
                                    aContext,
                                    aLoadGroup,
                                    aCallbacks,
                                    aLoadFlags,
                                    aReferrer);
}

inline nsresult /* NS_NewStreamLoaderPrincipal */
NS_NewStreamLoader(nsIStreamLoader**        outStream,
                   nsIURI*                  aUri,
                   nsIStreamLoaderObserver* aObserver,
                   nsIPrincipal*            aLoadingPrincipal,
                   nsSecurityFlags          aSecurityFlags,
                   nsContentPolicyType      aContentPolicyType,
                   nsISupports*             aContext = nullptr,
                   nsILoadGroup*            aLoadGroup = nullptr,
                   nsIInterfaceRequestor*   aCallbacks = nullptr,
                   nsLoadFlags              aLoadFlags = nsIRequest::LOAD_NORMAL,
                   nsIURI*                  aReferrer = nullptr)
{
  return NS_NewStreamLoaderInternal(outStream,
                                    aUri,
                                    aObserver,
                                    nullptr, // aLoadingNode
                                    aLoadingPrincipal,
                                    aSecurityFlags,
                                    aContentPolicyType,
                                    aContext,
                                    aLoadGroup,
                                    aCallbacks,
                                    aLoadFlags,
                                    aReferrer);
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
            *result = nullptr;
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
        if (NS_SUCCEEDED(rv)) {
            listener.forget(result);
        }
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
        rv = channel->AsyncOpen(listener, nullptr);
        if (NS_SUCCEEDED(rv)) {
            uint64_t n;
            // block until the initial response is received or an error occurs.
            rv = stream->Available(&n);
            if (NS_SUCCEEDED(rv)) {
                *result = nullptr;
                stream.swap(*result);
            }
        }
    }
    return rv;
}

inline nsresult
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

inline nsresult
NS_NewSimpleStreamListener(nsIStreamListener **result,
                           nsIOutputStream    *sink,
                           nsIRequestObserver *observer = nullptr)
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

inline nsresult
NS_CheckPortSafety(int32_t       port,
                   const char   *scheme,
                   nsIIOService *ioService = nullptr)
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
    int32_t port;
    nsresult rv = uri->GetPort(&port);
    if (NS_FAILED(rv) || port == -1)  // port undefined or default-valued
        return NS_OK;
    nsAutoCString scheme;
    uri->GetScheme(scheme);
    return NS_CheckPortSafety(port, scheme.get());
}

inline nsresult
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

inline nsresult
NS_GetFileProtocolHandler(nsIFileProtocolHandler **result,
                          nsIIOService            *ioService = nullptr)
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
                      nsIIOService      *ioService = nullptr)
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
                      nsIIOService *ioService = nullptr)
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
                            nsIIOService *ioService = nullptr)
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
                     nsIIOService *ioService = nullptr)
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

inline nsresult
NS_NewLocalFileInputStream(nsIInputStream **result,
                           nsIFile         *file,
                           int32_t          ioFlags       = -1,
                           int32_t          perm          = -1,
                           int32_t          behaviorFlags = 0)
{
    nsresult rv;
    nsCOMPtr<nsIFileInputStream> in =
        do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = in->Init(file, ioFlags, perm, behaviorFlags);
        if (NS_SUCCEEDED(rv))
            in.forget(result);
    }
    return rv;
}

inline nsresult
NS_NewPartialLocalFileInputStream(nsIInputStream **result,
                                  nsIFile         *file,
                                  uint64_t         offset,
                                  uint64_t         length,
                                  int32_t          ioFlags       = -1,
                                  int32_t          perm          = -1,
                                  int32_t          behaviorFlags = 0)
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
                            int32_t           ioFlags       = -1,
                            int32_t           perm          = -1,
                            int32_t           behaviorFlags = 0)
{
    nsresult rv;
    nsCOMPtr<nsIFileOutputStream> out =
        do_CreateInstance(NS_LOCALFILEOUTPUTSTREAM_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = out->Init(file, ioFlags, perm, behaviorFlags);
        if (NS_SUCCEEDED(rv))
            out.forget(result);
    }
    return rv;
}

// returns a file output stream which can be QI'ed to nsISafeOutputStream.
inline nsresult
NS_NewAtomicFileOutputStream(nsIOutputStream **result,
                                nsIFile          *file,
                                int32_t           ioFlags       = -1,
                                int32_t           perm          = -1,
                                int32_t           behaviorFlags = 0)
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

// returns a file output stream which can be QI'ed to nsISafeOutputStream.
inline nsresult
NS_NewSafeLocalFileOutputStream(nsIOutputStream **result,
                                nsIFile          *file,
                                int32_t           ioFlags       = -1,
                                int32_t           perm          = -1,
                                int32_t           behaviorFlags = 0)
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

inline nsresult
NS_NewLocalFileStream(nsIFileStream **result,
                      nsIFile        *file,
                      int32_t         ioFlags       = -1,
                      int32_t         perm          = -1,
                      int32_t         behaviorFlags = 0)
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

// returns the input end of a pipe.  the output end of the pipe
// is attached to the original stream.  data from the original
// stream is read into the pipe on a background thread.
inline nsresult
NS_BackgroundInputStream(nsIInputStream **result,
                         nsIInputStream  *stream,
                         uint32_t         segmentSize  = 0,
                         uint32_t         segmentCount = 0)
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

// returns the output end of a pipe.  the input end of the pipe
// is attached to the original stream.  data written to the pipe
// is copied to the original stream on a background thread.
inline nsresult
NS_BackgroundOutputStream(nsIOutputStream **result,
                          nsIOutputStream  *stream,
                          uint32_t          segmentSize  = 0,
                          uint32_t          segmentCount = 0)
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

MOZ_WARN_UNUSED_RESULT inline nsresult
NS_NewBufferedInputStream(nsIInputStream **result,
                          nsIInputStream  *str,
                          uint32_t         bufferSize)
{
    nsresult rv;
    nsCOMPtr<nsIBufferedInputStream> in =
        do_CreateInstance(NS_BUFFEREDINPUTSTREAM_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
        rv = in->Init(str, bufferSize);
        if (NS_SUCCEEDED(rv)) {
            in.forget(result);
        }
    }
    return rv;
}

// note: the resulting stream can be QI'ed to nsISafeOutputStream iff the
// provided stream supports it.
inline nsresult
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

// returns an input stream compatible with nsIUploadChannel::SetUploadStream()
inline nsresult
NS_NewPostDataStream(nsIInputStream  **result,
                     bool              isFile,
                     const nsACString &data)
{
    nsresult rv;

    if (isFile) {
        nsCOMPtr<nsIFile> file;
        nsCOMPtr<nsIInputStream> fileStream;

        rv = NS_NewNativeLocalFile(data, false, getter_AddRefs(file));
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

    stream.forget(result);
    return NS_OK;
}

inline nsresult
NS_ReadInputStreamToBuffer(nsIInputStream *aInputStream, 
                           void** aDest,
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

// external code can't see fallible_t
#ifdef MOZILLA_INTERNAL_API

inline nsresult
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

inline nsresult
NS_LoadPersistentPropertiesFromURI(nsIPersistentProperties** outResult,
                                   nsIURI*                   aUri,
                                   nsIPrincipal*             aLoadingPrincipal,
                                   nsContentPolicyType       aContentPolicyType,
                                   nsIIOService*             aIoService = nullptr)
{
    nsCOMPtr<nsIChannel> channel;
    nsresult rv = NS_NewChannel(getter_AddRefs(channel),
                                aUri,
                                aLoadingPrincipal,
                                nsILoadInfo::SEC_NORMAL,
                                aContentPolicyType,
                                nullptr,     // aLoadGroup
                                nullptr,     // aCallbacks
                                nsIRequest::LOAD_NORMAL,
                                aIoService);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIInputStream> in;
    rv = channel->Open(getter_AddRefs(in));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIPersistentProperties> properties =
      do_CreateInstance(NS_PERSISTENTPROPERTIES_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = properties->Load(in);
    NS_ENSURE_SUCCESS(rv, rv);

    properties.swap(*outResult);
    return NS_OK;
 }

inline nsresult
NS_LoadPersistentPropertiesFromURISpec(nsIPersistentProperties** outResult,
                                       const nsACString&         aSpec,
                                       nsIPrincipal*             aLoadingPrincipal,
                                       nsContentPolicyType       aContentPolicyType,
                                       const char*               aCharset = nullptr,
                                       nsIURI*                   aBaseURI = nullptr,
                                       nsIIOService*             aIoService = nullptr)
{
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(getter_AddRefs(uri),
                            aSpec,
                            aCharset,
                            aBaseURI,
                            aIoService);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_LoadPersistentPropertiesFromURI(outResult,
                                              uri,
                                              aLoadingPrincipal,
                                              aContentPolicyType,
                                              aIoService);
}

/**
 * NS_QueryNotificationCallbacks implements the canonical algorithm for
 * querying interfaces from a channel's notification callbacks.  It first
 * searches the channel's notificationCallbacks attribute, and if the interface
 * is not found there, then it inspects the notificationCallbacks attribute of
 * the channel's loadGroup.
 *
 * Note: templatized only because nsIWebSocketChannel is currently not an
 * nsIChannel.
 */
template <class T> inline void
NS_QueryNotificationCallbacks(T            *channel,
                              const nsIID  &iid,
                              void        **result)
{
    NS_PRECONDITION(channel, "null channel");
    *result = nullptr;

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

// template helper:
// Note: "class C" templatized only because nsIWebSocketChannel is currently not
// an nsIChannel.

template <class C, class T> inline void
NS_QueryNotificationCallbacks(C           *channel,
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
    *result = nullptr;

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
 * Returns true if channel is using Private Browsing, or false if not.
 * Returns false if channel's callbacks don't implement nsILoadContext.
 */
inline bool
NS_UsePrivateBrowsing(nsIChannel *channel)
{
    bool isPrivate = false;
    bool isOverriden = false;
    nsCOMPtr<nsIPrivateBrowsingChannel> pbChannel = do_QueryInterface(channel);
    if (pbChannel &&
        NS_SUCCEEDED(pbChannel->IsPrivateModeOverriden(&isPrivate, &isOverriden)) &&
        isOverriden) {
        return isPrivate;
    }
    nsCOMPtr<nsILoadContext> loadContext;
    NS_QueryNotificationCallbacks(channel, loadContext);
    return loadContext && loadContext->UsePrivateBrowsing();
}

// Constants duplicated from nsIScriptSecurityManager so we avoid having necko
// know about script security manager.
#define NECKO_NO_APP_ID 0
#define NECKO_UNKNOWN_APP_ID UINT32_MAX
// special app id reserved for separating the safebrowsing cookie
#define NECKO_SAFEBROWSING_APP_ID UINT32_MAX - 1

/**
 * Gets AppId and isInBrowserElement from channel's nsILoadContext.
 * Returns false if error or channel's callbacks don't implement nsILoadContext.
 */
inline bool
NS_GetAppInfo(nsIChannel *aChannel, uint32_t *aAppID, bool *aIsInBrowserElement)
{
    nsCOMPtr<nsILoadContext> loadContext;
    NS_QueryNotificationCallbacks(aChannel, loadContext);
    if (!loadContext) {
        return false;
    }

    nsresult rv = loadContext->GetAppId(aAppID);
    NS_ENSURE_SUCCESS(rv, false);

    rv = loadContext->GetIsInBrowserElement(aIsInBrowserElement);
    NS_ENSURE_SUCCESS(rv, false);

    return true;
}

/**
 *  Gets appId and browserOnly parameters from the TOPIC_WEB_APP_CLEAR_DATA
 *  nsIObserverService notification.  Used when clearing user data or
 *  uninstalling web apps.
 */
inline nsresult
NS_GetAppInfoFromClearDataNotification(nsISupports *aSubject,
                                       uint32_t *aAppID, bool* aBrowserOnly)
{
    nsresult rv;

    nsCOMPtr<mozIApplicationClearPrivateDataParams>
        clearParams(do_QueryInterface(aSubject));
    MOZ_ASSERT(clearParams);
    if (!clearParams) {
        return NS_ERROR_UNEXPECTED;
    }

    uint32_t appId;
    rv = clearParams->GetAppId(&appId);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    MOZ_ASSERT(appId != NECKO_UNKNOWN_APP_ID);
    NS_ENSURE_SUCCESS(rv, rv);
    if (appId == NECKO_UNKNOWN_APP_ID) {
        return NS_ERROR_UNEXPECTED;
    }

    bool browserOnly = false;
    rv = clearParams->GetBrowserOnly(&browserOnly);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    NS_ENSURE_SUCCESS(rv, rv);

    *aAppID = appId;
    *aBrowserOnly = browserOnly;
    return NS_OK;
}

/**
 * Determines whether appcache should be checked for a given URI.
 */
inline bool
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

inline bool
NS_ShouldCheckAppCache(nsIPrincipal * aPrincipal, bool usePrivateBrowsing)
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
                                       nsIEventTarget         *target,
                                       nsIInterfaceRequestor **result)
{
    nsCOMPtr<nsIInterfaceRequestor> cbs;
    if (loadGroup)
        loadGroup->GetNotificationCallbacks(getter_AddRefs(cbs));
    return NS_NewInterfaceRequestorAggregation(callbacks, cbs, target, result);
}

inline nsresult
NS_NewNotificationCallbacksAggregation(nsIInterfaceRequestor  *callbacks,
                                       nsILoadGroup           *loadGroup,
                                       nsIInterfaceRequestor **result)
{
    return NS_NewNotificationCallbacksAggregation(callbacks, loadGroup, nullptr, result);
}

/**
 * Helper function for testing online/offline state of the browser.
 */
inline bool
NS_IsOffline()
{
    bool offline = true;
    bool connectivity = true;
    nsCOMPtr<nsIIOService> ios = do_GetIOService();
    if (ios) {
        ios->GetOffline(&offline);
        ios->GetConnectivity(&connectivity);
    }
    return offline || !connectivity;
}

inline bool
NS_IsAppOffline(uint32_t appId)
{
    bool appOffline = false;
    nsCOMPtr<nsIIOService> io(
        do_GetService("@mozilla.org/network/io-service;1"));
    if (io) {
        io->IsAppOffline(appId, &appOffline);
    }
    return appOffline;
}

inline bool
NS_IsAppOffline(nsIPrincipal * principal)
{
    if (!principal) {
        return NS_IsOffline();
    }
    uint32_t appId = nsIScriptSecurityManager::UNKNOWN_APP_ID;
    principal->GetAppId(&appId);

    return NS_IsAppOffline(appId);
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
    *result = nullptr;

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
        mutableObj->SetMutable(false);
    }
}

/**
 * Helper function for calling ToImmutableURI.  If all else fails, returns
 * the input URI.  The optional second arg indicates whether we had to fall
 * back to the input URI.  Passing in a null URI is ok.
 */
inline already_AddRefed<nsIURI>
NS_TryToMakeImmutable(nsIURI* uri,
                      nsresult* outRv = nullptr)
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

/**
 * Helper function for testing whether the given URI, or any of its
 * inner URIs, has all the given protocol flags.
 */
inline nsresult
NS_URIChainHasFlags(nsIURI   *uri,
                    uint32_t  flags,
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
NS_GetInnermostURI(nsIURI* aURI)
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
    *uri = nullptr;
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
inline uint32_t
NS_SecurityHashURI(nsIURI* aURI)
{
    nsCOMPtr<nsIURI> baseURI = NS_GetInnermostURI(aURI);

    nsAutoCString scheme;
    uint32_t schemeHash = 0;
    if (NS_SUCCEEDED(baseURI->GetScheme(scheme)))
        schemeHash = mozilla::HashString(scheme);

    // TODO figure out how to hash file:// URIs
    if (scheme.EqualsLiteral("file"))
        return schemeHash; // sad face

    if (scheme.EqualsLiteral("imap") ||
        scheme.EqualsLiteral("mailbox") ||
        scheme.EqualsLiteral("news"))
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

    // Special handling for mailnews schemes
    if (targetScheme.EqualsLiteral("imap") ||
        targetScheme.EqualsLiteral("mailbox") ||
        targetScheme.EqualsLiteral("news"))
    {
        // Each message is a distinct trust domain; use the
        // whole spec for comparison
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

inline bool
NS_URIIsLocalFile(nsIURI *aURI)
{
  nsCOMPtr<nsINetUtil> util = do_GetNetUtil();

  bool isFile;
  return util && NS_SUCCEEDED(util->ProtocolHasFlags(aURI,
                                nsIProtocolHandler::URI_IS_LOCAL_FILE,
                                &isFile)) &&
         isFile;
}

// When strict file origin policy is enabled, SecurityCompareURIs will fail for
// file URIs that do not point to the same local file. This call provides an
// alternate file-specific origin check that allows target files that are
// contained in the same directory as the source.
//
// https://developer.mozilla.org/en-US/docs/Same-origin_policy_for_file:_URIs
inline bool
NS_RelaxStrictFileOriginPolicy(nsIURI *aTargetURI,
                               nsIURI *aSourceURI,
                               bool aAllowDirectoryTarget = false)
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

inline bool
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

inline bool
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

  bool isHttps;
  if (NS_FAILED(newURI->SchemeIs("https", &isHttps)) || !isHttps) {
    return false;
  }

  nsCOMPtr<nsIURI> upgradedURI;
  if (NS_FAILED(oldURI->Clone(getter_AddRefs(upgradedURI)))) {
    return false;
  }

  if (NS_FAILED(upgradedURI->SetScheme(NS_LITERAL_CSTRING("https")))) {
    return false;
  }

  int32_t oldPort = -1;
  if (NS_FAILED(oldURI->GetPort(&oldPort))) {
    return false;
  }
  if (oldPort == 80 || oldPort == -1) {
    upgradedURI->SetPort(-1);
  } else {
    upgradedURI->SetPort(oldPort);
  }

  bool res;
  return NS_SUCCEEDED(upgradedURI->Equals(newURI, &res)) && res;
}

inline nsresult
NS_LinkRedirectChannels(uint32_t channelId,
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
inline uint32_t
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
      StringHead(aDispToken, 8).LowerCaseEqualsLiteral("filename"))
    return nsIChannel::DISPOSITION_INLINE;

  return nsIChannel::DISPOSITION_ATTACHMENT;
}

/** Determine the disposition (inline/attachment) of the content based on the
 * Content-Disposition header
 * @param aHeader the content-disposition header (full value)
 * @param aChan the channel the header came from
 */
inline uint32_t
NS_GetContentDispositionFromHeader(const nsACString& aHeader, nsIChannel *aChan = nullptr)
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

/** Extracts the filename out of a content-disposition header
 * @param aFilename [out] The filename. Can be empty on error.
 * @param aDisposition Value of a Content-Disposition header
 * @param aURI Optional. Will be used to get a fallback charset for the
 *        filename, if it is QI'able to nsIURL
 */
inline nsresult
NS_GetFilenameFromDisposition(nsAString& aFilename,
                              const nsACString& aDisposition,
                              nsIURI* aURI = nullptr)
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

    nsAutoCString str;
    uri->GetSpec(str);
    return str.EqualsLiteral("about:blank");
}


inline nsresult
NS_GenerateHostPort(const nsCString& host, int32_t port,
                    nsACString& hostLine)
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

/**
 * Sniff the content type for a given request or a given buffer.
 *
 * aSnifferType can be either NS_CONTENT_SNIFFER_CATEGORY or
 * NS_DATA_SNIFFER_CATEGORY.  The function returns the sniffed content type
 * in the aSniffedType argument.  This argument will not be modified if the
 * content type could not be sniffed.
 */
inline void
NS_SniffContent(const char* aSnifferType, nsIRequest* aRequest,
                const uint8_t* aData, uint32_t aLength,
                nsACString& aSniffedType)
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

/**
 * Whether the channel was created to load a srcdoc document.
 * Note that view-source:about:srcdoc is classified as a srcdoc document by 
 * this function, which may not be applicable everywhere.
 */
inline bool
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

/**
 * Return true if the given string is a reasonable HTTP header value given the
 * definition in RFC 2616 section 4.2.  Currently we don't pay the cost to do
 * full, sctrict validation here since it would require fulling parsing the
 * value.
 */
bool NS_IsReasonableHTTPHeaderValue(const nsACString& aValue);

/**
 * Return true if the given string is a valid HTTP token per RFC 2616 section
 * 2.2.
 */
bool NS_IsValidHTTPToken(const nsACString& aToken);

namespace mozilla {
namespace net {

const static uint64_t kJS_MAX_SAFE_UINTEGER = +9007199254740991ULL;
const static  int64_t kJS_MIN_SAFE_INTEGER  = -9007199254740991LL;
const static  int64_t kJS_MAX_SAFE_INTEGER  = +9007199254740991LL;

// Make sure a 64bit value can be captured by JS MAX_SAFE_INTEGER
inline bool
InScriptableRange(int64_t val)
{
    return (val <= kJS_MAX_SAFE_INTEGER) && (val >= kJS_MIN_SAFE_INTEGER);
}

// Make sure a 64bit value can be captured by JS MAX_SAFE_INTEGER
inline bool
InScriptableRange(uint64_t val)
{
    return val <= kJS_MAX_SAFE_UINTEGER;
}

} // namespace mozilla
} // namespace mozilla::net

#endif // !nsNetUtil_h__
