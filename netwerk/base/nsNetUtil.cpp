/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "nsNetUtil.h"

#include "mozilla/AntiTrackingCommon.h"
#include "mozilla/Atomics.h"
#include "mozilla/Encoding.h"
#include "mozilla/LoadContext.h"
#include "mozilla/LoadInfo.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Monitor.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/Telemetry.h"
#include "nsCategoryCache.h"
#include "nsContentUtils.h"
#include "nsFileStreams.h"
#include "nsHashKeys.h"
#include "nsHttp.h"
#include "nsIAsyncStreamCopier.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPrompt2.h"
#include "nsIAuthPromptAdapterFactory.h"
#include "nsIBufferedStreams.h"
#include "nsIChannelEventSink.h"
#include "nsIContentSniffer.h"
#include "mozilla/dom/Document.h"
#include "nsICookieService.h"
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
#include "nsIObjectLoadingContent.h"
#include "nsIOfflineCacheUpdate.h"
#include "nsPersistentProperties.h"
#include "nsIPrivateBrowsingChannel.h"
#include "nsIPropertyBag2.h"
#include "nsIProtocolProxyService.h"
#include "mozilla/net/RedirectChannelRegistrar.h"
#include "nsRequestObserverProxy.h"
#include "nsIScriptSecurityManager.h"
#include "nsISensitiveInfoHiddenURI.h"
#include "nsISimpleStreamListener.h"
#include "nsISocketProvider.h"
#include "nsISocketProviderService.h"
#include "nsIStandardURL.h"
#include "nsIStreamLoader.h"
#include "nsIIncrementalStreamLoader.h"
#include "nsIStreamTransportService.h"
#include "nsStringStream.h"
#include "nsSyncStreamListener.h"
#include "nsITransport.h"
#include "nsIURIWithSpecialOrigin.h"
#include "nsIURLParser.h"
#include "nsIUUIDGenerator.h"
#include "nsIViewSourceChannel.h"
#include "nsInterfaceRequestorAgg.h"
#include "plstr.h"
#include "nsINestedURI.h"
#include "mozilla/dom/nsCSPUtils.h"
#include "mozilla/dom/nsMixedContentBlocker.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "nsIScriptError.h"
#include "nsISiteSecurityService.h"
#include "nsHttpHandler.h"
#include "nsNSSComponent.h"
#include "nsIRedirectHistoryEntry.h"
#ifdef MOZ_NEW_CERT_STORAGE
#  include "nsICertStorage.h"
#else
#  include "nsICertBlocklist.h"
#endif
#include "nsICertOverrideService.h"
#include "nsQueryObject.h"
#include "mozIThirdPartyUtil.h"
#include "../mime/nsMIMEHeaderParamImpl.h"
#include "nsStandardURL.h"
#include "nsChromeProtocolHandler.h"
#include "nsJSProtocolHandler.h"
#include "nsDataHandler.h"
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "nsStreamUtils.h"
#include "nsSocketTransportService2.h"
#include "nsViewSourceHandler.h"
#include "nsJARURI.h"
#include "nsIconURI.h"
#include "nsAboutProtocolHandler.h"
#include "nsResProtocolHandler.h"
#include "mozilla/net/ExtensionProtocolHandler.h"
#include <limits>

#if defined(MOZ_THUNDERBIRD) || defined(MOZ_SUITE)
#  include "nsNewMailnewsURI.h"
#endif

using namespace mozilla;
using namespace mozilla::net;
using mozilla::dom::BlobURLProtocolHandler;
using mozilla::dom::ClientInfo;
using mozilla::dom::PerformanceStorage;
using mozilla::dom::ServiceWorkerDescriptor;

#define MAX_RECURSION_COUNT 50

already_AddRefed<nsIIOService> do_GetIOService(nsresult* error /* = 0 */) {
  nsCOMPtr<nsIIOService> io = mozilla::services::GetIOService();
  if (error) *error = io ? NS_OK : NS_ERROR_FAILURE;
  return io.forget();
}

nsresult NS_NewLocalFileInputStream(nsIInputStream** result, nsIFile* file,
                                    int32_t ioFlags /* = -1 */,
                                    int32_t perm /* = -1 */,
                                    int32_t behaviorFlags /* = 0 */) {
  nsresult rv;
  nsCOMPtr<nsIFileInputStream> in =
      do_CreateInstance(NS_LOCALFILEINPUTSTREAM_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = in->Init(file, ioFlags, perm, behaviorFlags);
    if (NS_SUCCEEDED(rv)) in.forget(result);
  }
  return rv;
}

nsresult NS_NewLocalFileOutputStream(nsIOutputStream** result, nsIFile* file,
                                     int32_t ioFlags /* = -1 */,
                                     int32_t perm /* = -1 */,
                                     int32_t behaviorFlags /* = 0 */) {
  nsresult rv;
  nsCOMPtr<nsIFileOutputStream> out =
      do_CreateInstance(NS_LOCALFILEOUTPUTSTREAM_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = out->Init(file, ioFlags, perm, behaviorFlags);
    if (NS_SUCCEEDED(rv)) out.forget(result);
  }
  return rv;
}

nsresult net_EnsureIOService(nsIIOService** ios, nsCOMPtr<nsIIOService>& grip) {
  nsresult rv = NS_OK;
  if (!*ios) {
    grip = do_GetIOService(&rv);
    *ios = grip;
  }
  return rv;
}

nsresult NS_NewFileURI(
    nsIURI** result, nsIFile* spec,
    nsIIOService*
        ioService /* = nullptr */)  // pass in nsIIOService to optimize callers
{
  nsresult rv;
  nsCOMPtr<nsIIOService> grip;
  rv = net_EnsureIOService(&ioService, grip);
  if (ioService) rv = ioService->NewFileURI(spec, result);
  return rv;
}

nsresult NS_GetURIWithNewRef(nsIURI* aInput, const nsACString& aRef,
                             nsIURI** aOutput) {
  if (NS_WARN_IF(!aInput || !aOutput)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool hasRef;
  nsresult rv = aInput->GetHasRef(&hasRef);

  nsAutoCString ref;
  if (NS_SUCCEEDED(rv)) {
    rv = aInput->GetRef(ref);
  }

  // If the ref is already equal to the new ref, we do not need to do anything.
  // Also, if the GetRef failed (it could return NS_ERROR_NOT_IMPLEMENTED)
  // we can assume SetRef would fail as well, so returning the original
  // URI is OK.
  if (NS_FAILED(rv) || (!hasRef && aRef.IsEmpty()) ||
      (!aRef.IsEmpty() && aRef == ref)) {
    nsCOMPtr<nsIURI> uri = aInput;
    uri.forget(aOutput);
    return NS_OK;
  }

  return NS_MutateURI(aInput).SetRef(aRef).Finalize(aOutput);
}

nsresult NS_GetURIWithoutRef(nsIURI* aInput, nsIURI** aOutput) {
  return NS_GetURIWithNewRef(aInput, EmptyCString(), aOutput);
}

nsresult NS_NewChannelInternal(
    nsIChannel** outChannel, nsIURI* aUri, nsILoadInfo* aLoadInfo,
    PerformanceStorage* aPerformanceStorage /* = nullptr */,
    nsILoadGroup* aLoadGroup /* = nullptr */,
    nsIInterfaceRequestor* aCallbacks /* = nullptr */,
    nsLoadFlags aLoadFlags /* = nsIRequest::LOAD_NORMAL */,
    nsIIOService* aIoService /* = nullptr */) {
  // NS_NewChannelInternal is mostly called for channel redirects. We should
  // allow the creation of a channel even if the original channel did not have a
  // loadinfo attached.
  NS_ENSURE_ARG_POINTER(outChannel);

  nsCOMPtr<nsIIOService> grip;
  nsresult rv = net_EnsureIOService(&aIoService, grip);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  rv = aIoService->NewChannelFromURIWithLoadInfo(aUri, aLoadInfo,
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

#ifdef DEBUG
  nsLoadFlags channelLoadFlags = 0;
  channel->GetLoadFlags(&channelLoadFlags);
  // Will be removed when we remove LOAD_REPLACE altogether
  // This check is trying to catch protocol handlers that still
  // try to set the LOAD_REPLACE flag.
  MOZ_DIAGNOSTIC_ASSERT(!(channelLoadFlags & nsIChannel::LOAD_REPLACE));
#endif

  if (aLoadFlags != nsIRequest::LOAD_NORMAL) {
    rv = channel->SetLoadFlags(aLoadFlags);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aPerformanceStorage) {
    nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
    loadInfo->SetPerformanceStorage(aPerformanceStorage);
  }

  channel.forget(outChannel);
  return NS_OK;
}

namespace {

void AssertLoadingPrincipalAndClientInfoMatch(
    nsIPrincipal* aLoadingPrincipal, const ClientInfo& aLoadingClientInfo,
    nsContentPolicyType aType) {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  // Verify that the provided loading ClientInfo matches the loading
  // principal.  Unfortunately we can't just use nsIPrincipal::Equals() here
  // because of some corner cases:
  //
  //  1. Worker debugger scripts want to use a system loading principal for
  //     worker scripts with a content principal.  We exempt these from this
  //     check.
  //  2. Null principals currently require exact object identity for
  //     nsIPrincipal::Equals() to return true.  This doesn't work here because
  //     ClientInfo::GetPrincipal() uses PrincipalInfoToPrincipal() to allocate
  //     a new object.  To work around this we compare the principal origin
  //     string itself.  If bug 1431771 is fixed then we could switch to
  //     Equals().

  // Allow worker debugger to load with a system principal.
  if (aLoadingPrincipal->IsSystemPrincipal() &&
      (aType == nsIContentPolicy::TYPE_INTERNAL_WORKER ||
       aType == nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER ||
       aType == nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER ||
       aType == nsIContentPolicy::TYPE_INTERNAL_WORKER_IMPORT_SCRIPTS)) {
    return;
  }

  // Perform a fast comparison for most principal checks.
  nsCOMPtr<nsIPrincipal> clientPrincipal(aLoadingClientInfo.GetPrincipal());
  if (aLoadingPrincipal->Equals(clientPrincipal)) {
    return;
  }

  // Fall back to a slower origin equality test to support null principals.
  nsAutoCString loadingOrigin;
  MOZ_ALWAYS_SUCCEEDS(aLoadingPrincipal->GetOrigin(loadingOrigin));

  nsAutoCString clientOrigin;
  MOZ_ALWAYS_SUCCEEDS(clientPrincipal->GetOrigin(clientOrigin));

  MOZ_DIAGNOSTIC_ASSERT(loadingOrigin == clientOrigin);
#endif
}

}  // namespace

nsresult NS_NewChannel(nsIChannel** outChannel, nsIURI* aUri,
                       nsIPrincipal* aLoadingPrincipal,
                       nsSecurityFlags aSecurityFlags,
                       nsContentPolicyType aContentPolicyType,
                       nsICookieSettings* aCookieSettings /* = nullptr */,
                       PerformanceStorage* aPerformanceStorage /* = nullptr */,
                       nsILoadGroup* aLoadGroup /* = nullptr */,
                       nsIInterfaceRequestor* aCallbacks /* = nullptr */,
                       nsLoadFlags aLoadFlags /* = nsIRequest::LOAD_NORMAL */,
                       nsIIOService* aIoService /* = nullptr */) {
  return NS_NewChannelInternal(
      outChannel, aUri,
      nullptr,  // aLoadingNode,
      aLoadingPrincipal,
      nullptr,  // aTriggeringPrincipal
      Maybe<ClientInfo>(), Maybe<ServiceWorkerDescriptor>(), aSecurityFlags,
      aContentPolicyType, aCookieSettings, aPerformanceStorage, aLoadGroup,
      aCallbacks, aLoadFlags, aIoService);
}

nsresult NS_NewChannel(nsIChannel** outChannel, nsIURI* aUri,
                       nsIPrincipal* aLoadingPrincipal,
                       const ClientInfo& aLoadingClientInfo,
                       const Maybe<ServiceWorkerDescriptor>& aController,
                       nsSecurityFlags aSecurityFlags,
                       nsContentPolicyType aContentPolicyType,
                       nsICookieSettings* aCookieSettings /* = nullptr */,
                       PerformanceStorage* aPerformanceStorage /* = nullptr */,
                       nsILoadGroup* aLoadGroup /* = nullptr */,
                       nsIInterfaceRequestor* aCallbacks /* = nullptr */,
                       nsLoadFlags aLoadFlags /* = nsIRequest::LOAD_NORMAL */,
                       nsIIOService* aIoService /* = nullptr */) {
  AssertLoadingPrincipalAndClientInfoMatch(
      aLoadingPrincipal, aLoadingClientInfo, aContentPolicyType);

  Maybe<ClientInfo> loadingClientInfo;
  loadingClientInfo.emplace(aLoadingClientInfo);

  return NS_NewChannelInternal(outChannel, aUri,
                               nullptr,  // aLoadingNode,
                               aLoadingPrincipal,
                               nullptr,  // aTriggeringPrincipal
                               loadingClientInfo, aController, aSecurityFlags,
                               aContentPolicyType, aCookieSettings,
                               aPerformanceStorage, aLoadGroup, aCallbacks,
                               aLoadFlags, aIoService);
}

nsresult NS_NewChannelInternal(
    nsIChannel** outChannel, nsIURI* aUri, nsINode* aLoadingNode,
    nsIPrincipal* aLoadingPrincipal, nsIPrincipal* aTriggeringPrincipal,
    const Maybe<ClientInfo>& aLoadingClientInfo,
    const Maybe<ServiceWorkerDescriptor>& aController,
    nsSecurityFlags aSecurityFlags, nsContentPolicyType aContentPolicyType,
    nsICookieSettings* aCookieSettings /* = nullptr */,
    PerformanceStorage* aPerformanceStorage /* = nullptr */,
    nsILoadGroup* aLoadGroup /* = nullptr */,
    nsIInterfaceRequestor* aCallbacks /* = nullptr */,
    nsLoadFlags aLoadFlags /* = nsIRequest::LOAD_NORMAL */,
    nsIIOService* aIoService /* = nullptr */) {
  NS_ENSURE_ARG_POINTER(outChannel);

  nsCOMPtr<nsIIOService> grip;
  nsresult rv = net_EnsureIOService(&aIoService, grip);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  rv = aIoService->NewChannelFromURIWithClientAndController(
      aUri, aLoadingNode, aLoadingPrincipal, aTriggeringPrincipal,
      aLoadingClientInfo, aController, aSecurityFlags, aContentPolicyType,
      getter_AddRefs(channel));
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aLoadGroup) {
    rv = channel->SetLoadGroup(aLoadGroup);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aCallbacks) {
    rv = channel->SetNotificationCallbacks(aCallbacks);
    NS_ENSURE_SUCCESS(rv, rv);
  }

#ifdef DEBUG
  nsLoadFlags channelLoadFlags = 0;
  channel->GetLoadFlags(&channelLoadFlags);
  // Will be removed when we remove LOAD_REPLACE altogether
  // This check is trying to catch protocol handlers that still
  // try to set the LOAD_REPLACE flag.
  MOZ_DIAGNOSTIC_ASSERT(!(channelLoadFlags & nsIChannel::LOAD_REPLACE));
#endif

  if (aLoadFlags != nsIRequest::LOAD_NORMAL) {
    rv = channel->SetLoadFlags(aLoadFlags);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aPerformanceStorage || aCookieSettings) {
    nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();

    if (aPerformanceStorage) {
      loadInfo->SetPerformanceStorage(aPerformanceStorage);
    }

    if (aCookieSettings) {
      loadInfo->SetCookieSettings(aCookieSettings);
    }
  }

  channel.forget(outChannel);
  return NS_OK;
}

nsresult /*NS_NewChannelWithNodeAndTriggeringPrincipal */
NS_NewChannelWithTriggeringPrincipal(
    nsIChannel** outChannel, nsIURI* aUri, nsINode* aLoadingNode,
    nsIPrincipal* aTriggeringPrincipal, nsSecurityFlags aSecurityFlags,
    nsContentPolicyType aContentPolicyType,
    PerformanceStorage* aPerformanceStorage /* = nullptr */,
    nsILoadGroup* aLoadGroup /* = nullptr */,
    nsIInterfaceRequestor* aCallbacks /* = nullptr */,
    nsLoadFlags aLoadFlags /* = nsIRequest::LOAD_NORMAL */,
    nsIIOService* aIoService /* = nullptr */) {
  MOZ_ASSERT(aLoadingNode);
  NS_ASSERTION(aTriggeringPrincipal,
               "Can not create channel without a triggering Principal!");
  return NS_NewChannelInternal(
      outChannel, aUri, aLoadingNode, aLoadingNode->NodePrincipal(),
      aTriggeringPrincipal, Maybe<ClientInfo>(),
      Maybe<ServiceWorkerDescriptor>(), aSecurityFlags, aContentPolicyType,
      aLoadingNode->OwnerDoc()->CookieSettings(), aPerformanceStorage,
      aLoadGroup, aCallbacks, aLoadFlags, aIoService);
}

// See NS_NewChannelInternal for usage and argument description
nsresult NS_NewChannelWithTriggeringPrincipal(
    nsIChannel** outChannel, nsIURI* aUri, nsIPrincipal* aLoadingPrincipal,
    nsIPrincipal* aTriggeringPrincipal, nsSecurityFlags aSecurityFlags,
    nsContentPolicyType aContentPolicyType,
    nsICookieSettings* aCookieSettings /* = nullptr */,
    PerformanceStorage* aPerformanceStorage /* = nullptr */,
    nsILoadGroup* aLoadGroup /* = nullptr */,
    nsIInterfaceRequestor* aCallbacks /* = nullptr */,
    nsLoadFlags aLoadFlags /* = nsIRequest::LOAD_NORMAL */,
    nsIIOService* aIoService /* = nullptr */) {
  NS_ASSERTION(aLoadingPrincipal,
               "Can not create channel without a loading Principal!");
  return NS_NewChannelInternal(
      outChannel, aUri,
      nullptr,  // aLoadingNode
      aLoadingPrincipal, aTriggeringPrincipal, Maybe<ClientInfo>(),
      Maybe<ServiceWorkerDescriptor>(), aSecurityFlags, aContentPolicyType,
      aCookieSettings, aPerformanceStorage, aLoadGroup, aCallbacks, aLoadFlags,
      aIoService);
}

// See NS_NewChannelInternal for usage and argument description
nsresult NS_NewChannelWithTriggeringPrincipal(
    nsIChannel** outChannel, nsIURI* aUri, nsIPrincipal* aLoadingPrincipal,
    nsIPrincipal* aTriggeringPrincipal, const ClientInfo& aLoadingClientInfo,
    const Maybe<ServiceWorkerDescriptor>& aController,
    nsSecurityFlags aSecurityFlags, nsContentPolicyType aContentPolicyType,
    nsICookieSettings* aCookieSettings /* = nullptr */,
    PerformanceStorage* aPerformanceStorage /* = nullptr */,
    nsILoadGroup* aLoadGroup /* = nullptr */,
    nsIInterfaceRequestor* aCallbacks /* = nullptr */,
    nsLoadFlags aLoadFlags /* = nsIRequest::LOAD_NORMAL */,
    nsIIOService* aIoService /* = nullptr */) {
  AssertLoadingPrincipalAndClientInfoMatch(
      aLoadingPrincipal, aLoadingClientInfo, aContentPolicyType);

  Maybe<ClientInfo> loadingClientInfo;
  loadingClientInfo.emplace(aLoadingClientInfo);

  return NS_NewChannelInternal(
      outChannel, aUri,
      nullptr,  // aLoadingNode
      aLoadingPrincipal, aTriggeringPrincipal, loadingClientInfo, aController,
      aSecurityFlags, aContentPolicyType, aCookieSettings, aPerformanceStorage,
      aLoadGroup, aCallbacks, aLoadFlags, aIoService);
}

nsresult NS_NewChannel(nsIChannel** outChannel, nsIURI* aUri,
                       nsINode* aLoadingNode, nsSecurityFlags aSecurityFlags,
                       nsContentPolicyType aContentPolicyType,
                       PerformanceStorage* aPerformanceStorage /* = nullptr */,
                       nsILoadGroup* aLoadGroup /* = nullptr */,
                       nsIInterfaceRequestor* aCallbacks /* = nullptr */,
                       nsLoadFlags aLoadFlags /* = nsIRequest::LOAD_NORMAL */,
                       nsIIOService* aIoService /* = nullptr */) {
  NS_ASSERTION(aLoadingNode, "Can not create channel without a loading Node!");
  return NS_NewChannelInternal(
      outChannel, aUri, aLoadingNode, aLoadingNode->NodePrincipal(),
      nullptr,  // aTriggeringPrincipal
      Maybe<ClientInfo>(), Maybe<ServiceWorkerDescriptor>(), aSecurityFlags,
      aContentPolicyType, aLoadingNode->OwnerDoc()->CookieSettings(),
      aPerformanceStorage, aLoadGroup, aCallbacks, aLoadFlags, aIoService);
}

nsresult NS_GetIsDocumentChannel(nsIChannel* aChannel, bool* aIsDocument) {
  // Check if this channel is going to be used to create a document. If it has
  // LOAD_DOCUMENT_URI set it is trivially creating a document. If
  // LOAD_HTML_OBJECT_DATA is set it may or may not be used to create a
  // document, depending on its MIME type.

  if (!aChannel || !aIsDocument) {
    return NS_ERROR_NULL_POINTER;
  }
  *aIsDocument = false;
  nsLoadFlags loadFlags;
  nsresult rv = aChannel->GetLoadFlags(&loadFlags);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (loadFlags & nsIChannel::LOAD_DOCUMENT_URI) {
    *aIsDocument = true;
    return NS_OK;
  }
  if (!(loadFlags & nsIRequest::LOAD_HTML_OBJECT_DATA)) {
    *aIsDocument = false;
    return NS_OK;
  }
  nsAutoCString mimeType;
  rv = aChannel->GetContentType(mimeType);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (nsContentUtils::HtmlObjectContentTypeForMIMEType(
          mimeType, false, nullptr) == nsIObjectLoadingContent::TYPE_DOCUMENT) {
    *aIsDocument = true;
    return NS_OK;
  }
  *aIsDocument = false;
  return NS_OK;
}

nsresult NS_MakeAbsoluteURI(nsACString& result, const nsACString& spec,
                            nsIURI* baseURI) {
  nsresult rv;
  if (!baseURI) {
    NS_WARNING("It doesn't make sense to not supply a base URI");
    result = spec;
    rv = NS_OK;
  } else if (spec.IsEmpty())
    rv = baseURI->GetSpec(result);
  else
    rv = baseURI->Resolve(spec, result);
  return rv;
}

nsresult NS_MakeAbsoluteURI(char** result, const char* spec, nsIURI* baseURI) {
  nsresult rv;
  nsAutoCString resultBuf;
  rv = NS_MakeAbsoluteURI(resultBuf, nsDependentCString(spec), baseURI);
  if (NS_SUCCEEDED(rv)) {
    *result = ToNewCString(resultBuf);
    if (!*result) rv = NS_ERROR_OUT_OF_MEMORY;
  }
  return rv;
}

nsresult NS_MakeAbsoluteURI(nsAString& result, const nsAString& spec,
                            nsIURI* baseURI) {
  nsresult rv;
  if (!baseURI) {
    NS_WARNING("It doesn't make sense to not supply a base URI");
    result = spec;
    rv = NS_OK;
  } else {
    nsAutoCString resultBuf;
    if (spec.IsEmpty())
      rv = baseURI->GetSpec(resultBuf);
    else
      rv = baseURI->Resolve(NS_ConvertUTF16toUTF8(spec), resultBuf);
    if (NS_SUCCEEDED(rv)) CopyUTF8toUTF16(resultBuf, result);
  }
  return rv;
}

int32_t NS_GetDefaultPort(const char* scheme,
                          nsIIOService* ioService /* = nullptr */) {
  nsresult rv;

  // Getting the default port through the protocol handler has a lot of XPCOM
  // overhead involved.  We optimize the protocols that matter for Web pages
  // (HTTP and HTTPS) by hardcoding their default ports here.
  if (strncmp(scheme, "http", 4) == 0) {
    if (scheme[4] == 's' && scheme[5] == '\0') {
      return 443;
    }
    if (scheme[4] == '\0') {
      return 80;
    }
  }

  nsCOMPtr<nsIIOService> grip;
  net_EnsureIOService(&ioService, grip);
  if (!ioService) return -1;

  nsCOMPtr<nsIProtocolHandler> handler;
  rv = ioService->GetProtocolHandler(scheme, getter_AddRefs(handler));
  if (NS_FAILED(rv)) return -1;
  int32_t port;
  rv = handler->GetDefaultPort(&port);
  return NS_SUCCEEDED(rv) ? port : -1;
}

/**
 * This function is a helper function to apply the ToAscii conversion
 * to a string
 */
bool NS_StringToACE(const nsACString& idn, nsACString& result) {
  nsCOMPtr<nsIIDNService> idnSrv = do_GetService(NS_IDNSERVICE_CONTRACTID);
  if (!idnSrv) return false;
  nsresult rv = idnSrv->ConvertUTF8toACE(idn, result);
  if (NS_FAILED(rv)) return false;

  return true;
}

int32_t NS_GetRealPort(nsIURI* aURI) {
  int32_t port;
  nsresult rv = aURI->GetPort(&port);
  if (NS_FAILED(rv)) return -1;

  if (port != -1) return port;  // explicitly specified

  // Otherwise, we have to get the default port from the protocol handler

  // Need the scheme first
  nsAutoCString scheme;
  rv = aURI->GetScheme(scheme);
  if (NS_FAILED(rv)) return -1;

  return NS_GetDefaultPort(scheme.get());
}

nsresult NS_NewInputStreamChannelInternal(
    nsIChannel** outChannel, nsIURI* aUri,
    already_AddRefed<nsIInputStream> aStream, const nsACString& aContentType,
    const nsACString& aContentCharset, nsILoadInfo* aLoadInfo) {
  nsresult rv;
  nsCOMPtr<nsIInputStreamChannel> isc =
      do_CreateInstance(NS_INPUTSTREAMCHANNEL_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = isc->SetURI(aUri);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> stream = std::move(aStream);
  rv = isc->SetContentStream(stream);
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

  MOZ_ASSERT(aLoadInfo, "need a loadinfo to create a inputstreamchannel");
  channel->SetLoadInfo(aLoadInfo);

  // If we're sandboxed, make sure to clear any owner the channel
  // might already have.
  if (aLoadInfo && aLoadInfo->GetLoadingSandboxed()) {
    channel->SetOwner(nullptr);
  }

  channel.forget(outChannel);
  return NS_OK;
}

nsresult NS_NewInputStreamChannelInternal(
    nsIChannel** outChannel, nsIURI* aUri,
    already_AddRefed<nsIInputStream> aStream, const nsACString& aContentType,
    const nsACString& aContentCharset, nsINode* aLoadingNode,
    nsIPrincipal* aLoadingPrincipal, nsIPrincipal* aTriggeringPrincipal,
    nsSecurityFlags aSecurityFlags, nsContentPolicyType aContentPolicyType) {
  nsCOMPtr<nsILoadInfo> loadInfo = new mozilla::net::LoadInfo(
      aLoadingPrincipal, aTriggeringPrincipal, aLoadingNode, aSecurityFlags,
      aContentPolicyType);
  if (!loadInfo) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIInputStream> stream = std::move(aStream);

  return NS_NewInputStreamChannelInternal(outChannel, aUri, stream.forget(),
                                          aContentType, aContentCharset,
                                          loadInfo);
}

nsresult NS_NewInputStreamChannel(
    nsIChannel** outChannel, nsIURI* aUri,
    already_AddRefed<nsIInputStream> aStream, nsIPrincipal* aLoadingPrincipal,
    nsSecurityFlags aSecurityFlags, nsContentPolicyType aContentPolicyType,
    const nsACString& aContentType /* = EmptyCString() */,
    const nsACString& aContentCharset /* = EmptyCString() */) {
  nsCOMPtr<nsIInputStream> stream = aStream;
  return NS_NewInputStreamChannelInternal(outChannel, aUri, stream.forget(),
                                          aContentType, aContentCharset,
                                          nullptr,  // aLoadingNode
                                          aLoadingPrincipal,
                                          nullptr,  // aTriggeringPrincipal
                                          aSecurityFlags, aContentPolicyType);
}

nsresult NS_NewInputStreamChannelInternal(nsIChannel** outChannel, nsIURI* aUri,
                                          const nsAString& aData,
                                          const nsACString& aContentType,
                                          nsILoadInfo* aLoadInfo,
                                          bool aIsSrcdocChannel /* = false */) {
  nsresult rv;
  nsCOMPtr<nsIStringInputStream> stream;
  stream = do_CreateInstance(NS_STRINGINPUTSTREAM_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t len;
  char* utf8Bytes = ToNewUTF8String(aData, &len);
  rv = stream->AdoptData(utf8Bytes, len);

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewInputStreamChannelInternal(getter_AddRefs(channel), aUri,
                                        stream.forget(), aContentType,
                                        NS_LITERAL_CSTRING("UTF-8"), aLoadInfo);

  NS_ENSURE_SUCCESS(rv, rv);

  if (aIsSrcdocChannel) {
    nsCOMPtr<nsIInputStreamChannel> inStrmChan = do_QueryInterface(channel);
    NS_ENSURE_TRUE(inStrmChan, NS_ERROR_FAILURE);
    inStrmChan->SetSrcdocData(aData);
  }
  channel.forget(outChannel);
  return NS_OK;
}

nsresult NS_NewInputStreamChannelInternal(
    nsIChannel** outChannel, nsIURI* aUri, const nsAString& aData,
    const nsACString& aContentType, nsINode* aLoadingNode,
    nsIPrincipal* aLoadingPrincipal, nsIPrincipal* aTriggeringPrincipal,
    nsSecurityFlags aSecurityFlags, nsContentPolicyType aContentPolicyType,
    bool aIsSrcdocChannel /* = false */) {
  nsCOMPtr<nsILoadInfo> loadInfo = new mozilla::net::LoadInfo(
      aLoadingPrincipal, aTriggeringPrincipal, aLoadingNode, aSecurityFlags,
      aContentPolicyType);
  return NS_NewInputStreamChannelInternal(outChannel, aUri, aData, aContentType,
                                          loadInfo, aIsSrcdocChannel);
}

nsresult NS_NewInputStreamChannel(nsIChannel** outChannel, nsIURI* aUri,
                                  const nsAString& aData,
                                  const nsACString& aContentType,
                                  nsIPrincipal* aLoadingPrincipal,
                                  nsSecurityFlags aSecurityFlags,
                                  nsContentPolicyType aContentPolicyType,
                                  bool aIsSrcdocChannel /* = false */) {
  return NS_NewInputStreamChannelInternal(outChannel, aUri, aData, aContentType,
                                          nullptr,  // aLoadingNode
                                          aLoadingPrincipal,
                                          nullptr,  // aTriggeringPrincipal
                                          aSecurityFlags, aContentPolicyType,
                                          aIsSrcdocChannel);
}

nsresult NS_NewInputStreamPump(
    nsIInputStreamPump** aResult, already_AddRefed<nsIInputStream> aStream,
    uint32_t aSegsize /* = 0 */, uint32_t aSegcount /* = 0 */,
    bool aCloseWhenDone /* = false */,
    nsIEventTarget* aMainThreadTarget /* = nullptr */) {
  nsCOMPtr<nsIInputStream> stream = std::move(aStream);

  nsresult rv;
  nsCOMPtr<nsIInputStreamPump> pump =
      do_CreateInstance(NS_INPUTSTREAMPUMP_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = pump->Init(stream, aSegsize, aSegcount, aCloseWhenDone,
                    aMainThreadTarget);
    if (NS_SUCCEEDED(rv)) {
      *aResult = nullptr;
      pump.swap(*aResult);
    }
  }
  return rv;
}

nsresult NS_NewLoadGroup(nsILoadGroup** result, nsIRequestObserver* obs) {
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

bool NS_IsReasonableHTTPHeaderValue(const nsACString& aValue) {
  return mozilla::net::nsHttp::IsReasonableHeaderValue(aValue);
}

bool NS_IsValidHTTPToken(const nsACString& aToken) {
  return mozilla::net::nsHttp::IsValidToken(aToken);
}

void NS_TrimHTTPWhitespace(const nsACString& aSource, nsACString& aDest) {
  mozilla::net::nsHttp::TrimHTTPWhitespace(aSource, aDest);
}

nsresult NS_NewLoadGroup(nsILoadGroup** aResult, nsIPrincipal* aPrincipal) {
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

bool NS_LoadGroupMatchesPrincipal(nsILoadGroup* aLoadGroup,
                                  nsIPrincipal* aPrincipal) {
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

  return true;
}

nsresult NS_NewDownloader(nsIStreamListener** result,
                          nsIDownloadObserver* observer,
                          nsIFile* downloadLocation /* = nullptr */) {
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

nsresult NS_NewIncrementalStreamLoader(
    nsIIncrementalStreamLoader** result,
    nsIIncrementalStreamLoaderObserver* observer) {
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

nsresult NS_NewStreamLoader(
    nsIStreamLoader** result, nsIStreamLoaderObserver* observer,
    nsIRequestObserver* requestObserver /* = nullptr */) {
  nsresult rv;
  nsCOMPtr<nsIStreamLoader> loader =
      do_CreateInstance(NS_STREAMLOADER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = loader->Init(observer, requestObserver);
    if (NS_SUCCEEDED(rv)) {
      *result = nullptr;
      loader.swap(*result);
    }
  }
  return rv;
}

nsresult NS_NewStreamLoaderInternal(
    nsIStreamLoader** outStream, nsIURI* aUri,
    nsIStreamLoaderObserver* aObserver, nsINode* aLoadingNode,
    nsIPrincipal* aLoadingPrincipal, nsSecurityFlags aSecurityFlags,
    nsContentPolicyType aContentPolicyType,
    nsILoadGroup* aLoadGroup /* = nullptr */,
    nsIInterfaceRequestor* aCallbacks /* = nullptr */,
    nsLoadFlags aLoadFlags /* = nsIRequest::LOAD_NORMAL */) {
  nsCOMPtr<nsIChannel> channel;
  nsresult rv = NS_NewChannelInternal(
      getter_AddRefs(channel), aUri, aLoadingNode, aLoadingPrincipal,
      nullptr,  // aTriggeringPrincipal
      Maybe<ClientInfo>(), Maybe<ServiceWorkerDescriptor>(), aSecurityFlags,
      aContentPolicyType,
      nullptr,  // nsICookieSettings
      nullptr,  // PerformanceStorage
      aLoadGroup, aCallbacks, aLoadFlags);

  NS_ENSURE_SUCCESS(rv, rv);
  rv = NS_NewStreamLoader(outStream, aObserver);
  NS_ENSURE_SUCCESS(rv, rv);
  return channel->AsyncOpen(*outStream);
}

nsresult NS_NewStreamLoader(
    nsIStreamLoader** outStream, nsIURI* aUri,
    nsIStreamLoaderObserver* aObserver, nsINode* aLoadingNode,
    nsSecurityFlags aSecurityFlags, nsContentPolicyType aContentPolicyType,
    nsILoadGroup* aLoadGroup /* = nullptr */,
    nsIInterfaceRequestor* aCallbacks /* = nullptr */,
    nsLoadFlags aLoadFlags /* = nsIRequest::LOAD_NORMAL */) {
  NS_ASSERTION(aLoadingNode,
               "Can not create stream loader without a loading Node!");
  return NS_NewStreamLoaderInternal(
      outStream, aUri, aObserver, aLoadingNode, aLoadingNode->NodePrincipal(),
      aSecurityFlags, aContentPolicyType, aLoadGroup, aCallbacks, aLoadFlags);
}

nsresult NS_NewStreamLoader(
    nsIStreamLoader** outStream, nsIURI* aUri,
    nsIStreamLoaderObserver* aObserver, nsIPrincipal* aLoadingPrincipal,
    nsSecurityFlags aSecurityFlags, nsContentPolicyType aContentPolicyType,
    nsILoadGroup* aLoadGroup /* = nullptr */,
    nsIInterfaceRequestor* aCallbacks /* = nullptr */,
    nsLoadFlags aLoadFlags /* = nsIRequest::LOAD_NORMAL */) {
  return NS_NewStreamLoaderInternal(outStream, aUri, aObserver,
                                    nullptr,  // aLoadingNode
                                    aLoadingPrincipal, aSecurityFlags,
                                    aContentPolicyType, aLoadGroup, aCallbacks,
                                    aLoadFlags);
}

nsresult NS_NewSyncStreamListener(nsIStreamListener** result,
                                  nsIInputStream** stream) {
  nsCOMPtr<nsISyncStreamListener> listener = nsSyncStreamListener::Create();
  if (listener) {
    nsresult rv = listener->GetInputStream(stream);
    if (NS_SUCCEEDED(rv)) {
      listener.forget(result);
    }
    return rv;
  }
  return NS_ERROR_FAILURE;
}

nsresult NS_ImplementChannelOpen(nsIChannel* channel, nsIInputStream** result) {
  nsCOMPtr<nsIStreamListener> listener;
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewSyncStreamListener(getter_AddRefs(listener),
                                         getter_AddRefs(stream));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_MaybeOpenChannelUsingAsyncOpen(channel, listener);
  NS_ENSURE_SUCCESS(rv, rv);

  uint64_t n;
  // block until the initial response is received or an error occurs.
  rv = stream->Available(&n);
  NS_ENSURE_SUCCESS(rv, rv);

  *result = nullptr;
  stream.swap(*result);

  return NS_OK;
}

nsresult NS_NewRequestObserverProxy(nsIRequestObserver** result,
                                    nsIRequestObserver* observer,
                                    nsISupports* context) {
  nsCOMPtr<nsIRequestObserverProxy> proxy = new nsRequestObserverProxy();
  nsresult rv = proxy->Init(observer, context);
  if (NS_SUCCEEDED(rv)) {
    proxy.forget(result);
  }
  return rv;
}

nsresult NS_NewSimpleStreamListener(
    nsIStreamListener** result, nsIOutputStream* sink,
    nsIRequestObserver* observer /* = nullptr */) {
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

nsresult NS_CheckPortSafety(int32_t port, const char* scheme,
                            nsIIOService* ioService /* = nullptr */) {
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

nsresult NS_CheckPortSafety(nsIURI* uri) {
  int32_t port;
  nsresult rv = uri->GetPort(&port);
  if (NS_FAILED(rv) || port == -1)  // port undefined or default-valued
    return NS_OK;
  nsAutoCString scheme;
  uri->GetScheme(scheme);
  return NS_CheckPortSafety(port, scheme.get());
}

nsresult NS_NewProxyInfo(const nsACString& type, const nsACString& host,
                         int32_t port, uint32_t flags, nsIProxyInfo** result) {
  nsresult rv;
  nsCOMPtr<nsIProtocolProxyService> pps =
      do_GetService(NS_PROTOCOLPROXYSERVICE_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
    rv = pps->NewProxyInfo(type, host, port, EmptyCString(), EmptyCString(),
                           flags, UINT32_MAX, nullptr, result);
  return rv;
}

nsresult NS_GetFileProtocolHandler(nsIFileProtocolHandler** result,
                                   nsIIOService* ioService /* = nullptr */) {
  nsresult rv;
  nsCOMPtr<nsIIOService> grip;
  rv = net_EnsureIOService(&ioService, grip);
  if (ioService) {
    nsCOMPtr<nsIProtocolHandler> handler;
    rv = ioService->GetProtocolHandler("file", getter_AddRefs(handler));
    if (NS_SUCCEEDED(rv)) rv = CallQueryInterface(handler, result);
  }
  return rv;
}

nsresult NS_GetFileFromURLSpec(const nsACString& inURL, nsIFile** result,
                               nsIIOService* ioService /* = nullptr */) {
  nsresult rv;
  nsCOMPtr<nsIFileProtocolHandler> fileHandler;
  rv = NS_GetFileProtocolHandler(getter_AddRefs(fileHandler), ioService);
  if (NS_SUCCEEDED(rv)) rv = fileHandler->GetFileFromURLSpec(inURL, result);
  return rv;
}

nsresult NS_GetURLSpecFromFile(nsIFile* file, nsACString& url,
                               nsIIOService* ioService /* = nullptr */) {
  nsresult rv;
  nsCOMPtr<nsIFileProtocolHandler> fileHandler;
  rv = NS_GetFileProtocolHandler(getter_AddRefs(fileHandler), ioService);
  if (NS_SUCCEEDED(rv)) rv = fileHandler->GetURLSpecFromFile(file, url);
  return rv;
}

nsresult NS_GetURLSpecFromActualFile(nsIFile* file, nsACString& url,
                                     nsIIOService* ioService /* = nullptr */) {
  nsresult rv;
  nsCOMPtr<nsIFileProtocolHandler> fileHandler;
  rv = NS_GetFileProtocolHandler(getter_AddRefs(fileHandler), ioService);
  if (NS_SUCCEEDED(rv)) rv = fileHandler->GetURLSpecFromActualFile(file, url);
  return rv;
}

nsresult NS_GetURLSpecFromDir(nsIFile* file, nsACString& url,
                              nsIIOService* ioService /* = nullptr */) {
  nsresult rv;
  nsCOMPtr<nsIFileProtocolHandler> fileHandler;
  rv = NS_GetFileProtocolHandler(getter_AddRefs(fileHandler), ioService);
  if (NS_SUCCEEDED(rv)) rv = fileHandler->GetURLSpecFromDir(file, url);
  return rv;
}

void NS_GetReferrerFromChannel(nsIChannel* channel, nsIURI** referrer) {
  *referrer = nullptr;

  nsCOMPtr<nsIPropertyBag2> props(do_QueryInterface(channel));
  if (props) {
    // We have to check for a property on a property bag because the
    // referrer may be empty for security reasons (for example, when loading
    // an http page with an https referrer).
    nsresult rv = props->GetPropertyAsInterface(
        NS_LITERAL_STRING("docshell.internalReferrer"), NS_GET_IID(nsIURI),
        reinterpret_cast<void**>(referrer));
    if (NS_FAILED(rv)) *referrer = nullptr;
  }

  if (*referrer) {
    return;
  }

  // if that didn't work, we can still try to get the referrer from the
  // nsIHttpChannel (if we can QI to it)
  nsCOMPtr<nsIHttpChannel> chan(do_QueryInterface(channel));
  if (!chan) {
    return;
  }

  nsCOMPtr<nsIReferrerInfo> referrerInfo = chan->GetReferrerInfo();
  if (!referrerInfo) {
    return;
  }

  referrerInfo->GetOriginalReferrer(referrer);
}

already_AddRefed<nsINetUtil> do_GetNetUtil(nsresult* error /* = 0 */) {
  nsCOMPtr<nsIIOService> io = mozilla::services::GetIOService();
  nsCOMPtr<nsINetUtil> util;
  if (io) util = do_QueryInterface(io);

  if (error) *error = !!util ? NS_OK : NS_ERROR_FAILURE;
  return util.forget();
}

nsresult NS_ParseRequestContentType(const nsACString& rawContentType,
                                    nsCString& contentType,
                                    nsCString& contentCharset) {
  // contentCharset is left untouched if not present in rawContentType
  nsresult rv;
  nsCOMPtr<nsINetUtil> util = do_GetNetUtil(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCString charset;
  bool hadCharset;
  rv = util->ParseRequestContentType(rawContentType, charset, &hadCharset,
                                     contentType);
  if (NS_SUCCEEDED(rv) && hadCharset) contentCharset = charset;
  return rv;
}

nsresult NS_ParseResponseContentType(const nsACString& rawContentType,
                                     nsCString& contentType,
                                     nsCString& contentCharset) {
  // contentCharset is left untouched if not present in rawContentType
  nsresult rv;
  nsCOMPtr<nsINetUtil> util = do_GetNetUtil(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCString charset;
  bool hadCharset;
  rv = util->ParseResponseContentType(rawContentType, charset, &hadCharset,
                                      contentType);
  if (NS_SUCCEEDED(rv) && hadCharset) contentCharset = charset;
  return rv;
}

nsresult NS_ExtractCharsetFromContentType(const nsACString& rawContentType,
                                          nsCString& contentCharset,
                                          bool* hadCharset,
                                          int32_t* charsetStart,
                                          int32_t* charsetEnd) {
  // contentCharset is left untouched if not present in rawContentType
  nsresult rv;
  nsCOMPtr<nsINetUtil> util = do_GetNetUtil(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return util->ExtractCharsetFromContentType(
      rawContentType, contentCharset, charsetStart, charsetEnd, hadCharset);
}

nsresult NS_NewAtomicFileOutputStream(nsIOutputStream** result, nsIFile* file,
                                      int32_t ioFlags /* = -1 */,
                                      int32_t perm /* = -1 */,
                                      int32_t behaviorFlags /* = 0 */) {
  nsresult rv;
  nsCOMPtr<nsIFileOutputStream> out =
      do_CreateInstance(NS_ATOMICLOCALFILEOUTPUTSTREAM_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = out->Init(file, ioFlags, perm, behaviorFlags);
    if (NS_SUCCEEDED(rv)) out.forget(result);
  }
  return rv;
}

nsresult NS_NewSafeLocalFileOutputStream(nsIOutputStream** result,
                                         nsIFile* file,
                                         int32_t ioFlags /* = -1 */,
                                         int32_t perm /* = -1 */,
                                         int32_t behaviorFlags /* = 0 */) {
  nsresult rv;
  nsCOMPtr<nsIFileOutputStream> out =
      do_CreateInstance(NS_SAFELOCALFILEOUTPUTSTREAM_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = out->Init(file, ioFlags, perm, behaviorFlags);
    if (NS_SUCCEEDED(rv)) out.forget(result);
  }
  return rv;
}

nsresult NS_NewLocalFileStream(nsIFileStream** result, nsIFile* file,
                               int32_t ioFlags /* = -1 */,
                               int32_t perm /* = -1 */,
                               int32_t behaviorFlags /* = 0 */) {
  nsCOMPtr<nsIFileStream> stream = new nsFileStream();
  nsresult rv = stream->Init(file, ioFlags, perm, behaviorFlags);
  if (NS_SUCCEEDED(rv)) {
    stream.forget(result);
  }
  return rv;
}

nsresult NS_NewBufferedOutputStream(
    nsIOutputStream** aResult, already_AddRefed<nsIOutputStream> aOutputStream,
    uint32_t aBufferSize) {
  nsCOMPtr<nsIOutputStream> outputStream = std::move(aOutputStream);

  nsresult rv;
  nsCOMPtr<nsIBufferedOutputStream> out =
      do_CreateInstance(NS_BUFFEREDOUTPUTSTREAM_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = out->Init(outputStream, aBufferSize);
    if (NS_SUCCEEDED(rv)) {
      out.forget(aResult);
    }
  }
  return rv;
}

MOZ_MUST_USE nsresult NS_NewBufferedInputStream(
    nsIInputStream** aResult, already_AddRefed<nsIInputStream> aInputStream,
    uint32_t aBufferSize) {
  nsCOMPtr<nsIInputStream> inputStream = std::move(aInputStream);

  nsresult rv;
  nsCOMPtr<nsIBufferedInputStream> in =
      do_CreateInstance(NS_BUFFEREDINPUTSTREAM_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = in->Init(inputStream, aBufferSize);
    if (NS_SUCCEEDED(rv)) {
      in.forget(aResult);
    }
  }
  return rv;
}

namespace {

#define BUFFER_SIZE 8192

class BufferWriter final : public nsIInputStreamCallback {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  BufferWriter(nsIInputStream* aInputStream, void* aBuffer, int64_t aCount)
      : mMonitor("BufferWriter.mMonitor"),
        mInputStream(aInputStream),
        mBuffer(aBuffer),
        mCount(aCount),
        mWrittenData(0),
        mBufferType(aBuffer ? eExternal : eInternal),
        mBufferSize(0) {
    MOZ_ASSERT(aInputStream);
    MOZ_ASSERT(aCount == -1 || aCount > 0);
    MOZ_ASSERT_IF(mBuffer, aCount > 0);
  }

  nsresult Write() {
    NS_ASSERT_OWNINGTHREAD(BufferWriter);

    // Let's make the inputStream buffered if it's not.
    if (!NS_InputStreamIsBuffered(mInputStream)) {
      nsCOMPtr<nsIInputStream> bufferedStream;
      nsresult rv = NS_NewBufferedInputStream(
          getter_AddRefs(bufferedStream), mInputStream.forget(), BUFFER_SIZE);
      NS_ENSURE_SUCCESS(rv, rv);

      mInputStream = bufferedStream;
    }

    mAsyncInputStream = do_QueryInterface(mInputStream);

    if (!mAsyncInputStream) {
      return WriteSync();
    }

    // Let's use mAsyncInputStream only.
    mInputStream = nullptr;

    return WriteAsync();
  }

  uint64_t WrittenData() const {
    NS_ASSERT_OWNINGTHREAD(BufferWriter);
    return mWrittenData;
  }

  void* StealBuffer() {
    NS_ASSERT_OWNINGTHREAD(BufferWriter);
    MOZ_ASSERT(mBufferType == eInternal);

    void* buffer = mBuffer;

    mBuffer = nullptr;
    mBufferSize = 0;

    return buffer;
  }

 private:
  ~BufferWriter() {
    if (mBuffer && mBufferType == eInternal) {
      free(mBuffer);
    }

    if (mTaskQueue) {
      mTaskQueue->BeginShutdown();
    }
  }

  nsresult WriteSync() {
    NS_ASSERT_OWNINGTHREAD(BufferWriter);

    uint64_t length = (uint64_t)mCount;

    if (mCount == -1) {
      nsresult rv = mInputStream->Available(&length);
      NS_ENSURE_SUCCESS(rv, rv);

      if (length == 0) {
        // nothing to read.
        return NS_OK;
      }
    }

    if (mBufferType == eInternal) {
      mBuffer = malloc(length);
      if (NS_WARN_IF(!mBuffer)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    uint32_t writtenData;
    nsresult rv = mInputStream->ReadSegments(NS_CopySegmentToBuffer, mBuffer,
                                             length, &writtenData);
    NS_ENSURE_SUCCESS(rv, rv);

    mWrittenData = writtenData;
    return NS_OK;
  }

  nsresult WriteAsync() {
    NS_ASSERT_OWNINGTHREAD(BufferWriter);

    if (mCount > 0 && mBufferType == eInternal) {
      mBuffer = malloc(mCount);
      if (NS_WARN_IF(!mBuffer)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }

    while (true) {
      if (mCount == -1 && !MaybeExpandBufferSize()) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      uint64_t offset = mWrittenData;
      uint64_t length = mCount == -1 ? BUFFER_SIZE : mCount;

      // Let's try to read data directly.
      uint32_t writtenData;
      nsresult rv = mAsyncInputStream->ReadSegments(
          NS_CopySegmentToBuffer, static_cast<char*>(mBuffer) + offset, length,
          &writtenData);

      // Operation completed. Nothing more to read.
      if (NS_SUCCEEDED(rv) && writtenData == 0) {
        return NS_OK;
      }

      // If we succeeded, let's try to read again.
      if (NS_SUCCEEDED(rv)) {
        mWrittenData += writtenData;
        if (mCount != -1) {
          MOZ_ASSERT(mCount >= writtenData);
          mCount -= writtenData;

          // Is this the end of the reading?
          if (mCount == 0) {
            return NS_OK;
          }
        }

        continue;
      }

      // Async wait...
      if (rv == NS_BASE_STREAM_WOULD_BLOCK) {
        rv = MaybeCreateTaskQueue();
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        MonitorAutoLock lock(mMonitor);

        rv = mAsyncInputStream->AsyncWait(this, 0, length, mTaskQueue);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }

        lock.Wait();
        continue;
      }

      // Otherwise, let's propagate the error.
      return rv;
    }

    MOZ_ASSERT_UNREACHABLE("We should not be here");
    return NS_ERROR_FAILURE;
  }

  nsresult MaybeCreateTaskQueue() {
    NS_ASSERT_OWNINGTHREAD(BufferWriter);

    if (!mTaskQueue) {
      nsCOMPtr<nsIEventTarget> target =
          do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
      if (!target) {
        return NS_ERROR_FAILURE;
      }

      mTaskQueue = new TaskQueue(target.forget());
    }

    return NS_OK;
  }

  NS_IMETHOD
  OnInputStreamReady(nsIAsyncInputStream* aStream) override {
    MOZ_ASSERT(!NS_IsMainThread());

    // We have something to read. Let's unlock the main-thread.
    MonitorAutoLock lock(mMonitor);
    lock.Notify();
    return NS_OK;
  }

  bool MaybeExpandBufferSize() {
    NS_ASSERT_OWNINGTHREAD(BufferWriter);

    MOZ_ASSERT(mCount == -1);

    if (mBufferSize >= mWrittenData + BUFFER_SIZE) {
      // The buffer is big enough.
      return true;
    }

    CheckedUint32 bufferSize =
        std::max<uint32_t>(static_cast<uint32_t>(mWrittenData), BUFFER_SIZE);
    while (bufferSize.isValid() &&
           bufferSize.value() < mWrittenData + BUFFER_SIZE) {
      bufferSize *= 2;
    }

    if (!bufferSize.isValid()) {
      return false;
    }

    void* buffer = realloc(mBuffer, bufferSize.value());
    if (!buffer) {
      return false;
    }

    mBuffer = buffer;
    mBufferSize = bufferSize.value();
    return true;
  }

  // All the members of this class are touched on the owning thread only. The
  // monitor is only used to communicate when there is more data to read.
  Monitor mMonitor;

  nsCOMPtr<nsIInputStream> mInputStream;
  nsCOMPtr<nsIAsyncInputStream> mAsyncInputStream;

  RefPtr<TaskQueue> mTaskQueue;

  void* mBuffer;
  int64_t mCount;
  uint64_t mWrittenData;

  enum {
    // The buffer is allocated internally and this object must release it
    // in the DTOR if not stolen. The buffer can be reallocated.
    eInternal,

    // The buffer is not owned by this object and it cannot be reallocated.
    eExternal,
  } mBufferType;

  // The following set if needed for the async read.
  uint64_t mBufferSize;
};

NS_IMPL_ISUPPORTS(BufferWriter, nsIInputStreamCallback)

}  // anonymous namespace

nsresult NS_ReadInputStreamToBuffer(nsIInputStream* aInputStream, void** aDest,
                                    int64_t aCount, uint64_t* aWritten) {
  MOZ_ASSERT(aInputStream);
  MOZ_ASSERT(aCount >= -1);

  uint64_t dummyWritten;
  if (!aWritten) {
    aWritten = &dummyWritten;
  }

  if (aCount == 0) {
    *aWritten = 0;
    return NS_OK;
  }

  // This will take care of allocating and reallocating aDest.
  RefPtr<BufferWriter> writer = new BufferWriter(aInputStream, *aDest, aCount);

  nsresult rv = writer->Write();
  NS_ENSURE_SUCCESS(rv, rv);

  *aWritten = writer->WrittenData();

  if (!*aDest) {
    *aDest = writer->StealBuffer();
  }

  return NS_OK;
}

nsresult NS_ReadInputStreamToString(nsIInputStream* aInputStream,
                                    nsACString& aDest, int64_t aCount,
                                    uint64_t* aWritten) {
  uint64_t dummyWritten;
  if (!aWritten) {
    aWritten = &dummyWritten;
  }

  // Nothing to do if aCount is 0.
  if (aCount == 0) {
    aDest.Truncate();
    *aWritten = 0;
    return NS_OK;
  }

  // If we have the size, we can pre-allocate the buffer.
  if (aCount > 0) {
    if (NS_WARN_IF(aCount >= INT32_MAX) ||
        NS_WARN_IF(!aDest.SetLength(aCount, mozilla::fallible))) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    void* dest = aDest.BeginWriting();
    nsresult rv =
        NS_ReadInputStreamToBuffer(aInputStream, &dest, aCount, aWritten);
    NS_ENSURE_SUCCESS(rv, rv);

    if ((uint64_t)aCount > *aWritten) {
      aDest.Truncate(*aWritten);
    }

    return NS_OK;
  }

  // If the size is unknown, BufferWriter will allocate the buffer.
  void* dest = nullptr;
  nsresult rv =
      NS_ReadInputStreamToBuffer(aInputStream, &dest, aCount, aWritten);
  MOZ_ASSERT_IF(NS_FAILED(rv), dest == nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!dest) {
    MOZ_ASSERT(*aWritten == 0);
    aDest.Truncate();
    return NS_OK;
  }

  aDest.Adopt(reinterpret_cast<char*>(dest), *aWritten);
  return NS_OK;
}

nsresult NS_NewURI(nsIURI** result, const nsACString& spec,
                   NotNull<const Encoding*> encoding,
                   nsIURI* baseURI /* = nullptr */) {
  nsAutoCString charset;
  encoding->Name(charset);
  return NS_NewURI(result, spec, charset.get(), baseURI);
}

nsresult NS_NewURI(nsIURI** result, const nsAString& aSpec,
                   const char* charset /* = nullptr */,
                   nsIURI* baseURI /* = nullptr */) {
  nsAutoCString spec;
  if (!AppendUTF16toUTF8(aSpec, spec, mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_NewURI(result, spec, charset, baseURI);
}

nsresult NS_NewURI(nsIURI** result, const nsAString& aSpec,
                   NotNull<const Encoding*> encoding,
                   nsIURI* baseURI /* = nullptr */) {
  nsAutoCString spec;
  if (!AppendUTF16toUTF8(aSpec, spec, mozilla::fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_NewURI(result, spec, encoding, baseURI);
}

nsresult NS_NewURI(nsIURI** result, const char* spec,
                   nsIURI* baseURI /* = nullptr */) {
  return NS_NewURI(result, nsDependentCString(spec), nullptr, baseURI);
}

static nsresult NewStandardURI(const nsACString& aSpec, const char* aCharset,
                               nsIURI* aBaseURI, int32_t aDefaultPort,
                               nsIURI** aURI) {
  nsCOMPtr<nsIURI> base(aBaseURI);
  return NS_MutateURI(new nsStandardURL::Mutator())
      .Apply(NS_MutatorMethod(&nsIStandardURLMutator::Init,
                              nsIStandardURL::URLTYPE_AUTHORITY, aDefaultPort,
                              nsCString(aSpec), aCharset, base, nullptr))
      .Finalize(aURI);
}

extern MOZ_THREAD_LOCAL(uint32_t) gTlsURLRecursionCount;

template <typename T>
class TlsAutoIncrement {
 public:
  explicit TlsAutoIncrement(T& var) : mVar(var) {
    mValue = mVar.get();
    mVar.set(mValue + 1);
  }
  ~TlsAutoIncrement() {
    typename T::Type value = mVar.get();
    MOZ_ASSERT(value == mValue + 1);
    mVar.set(value - 1);
  }

  typename T::Type value() { return mValue; }

 private:
  typename T::Type mValue;
  T& mVar;
};

nsresult NS_NewURI(nsIURI** aURI, const nsACString& aSpec,
                   const char* aCharset /* = nullptr */,
                   nsIURI* aBaseURI /* = nullptr */) {
  TlsAutoIncrement<decltype(gTlsURLRecursionCount)> inc(gTlsURLRecursionCount);
  if (inc.value() >= MAX_RECURSION_COUNT) {
    return NS_ERROR_MALFORMED_URI;
  }

  nsAutoCString scheme;
  nsresult rv = net_ExtractURLScheme(aSpec, scheme);
  if (NS_FAILED(rv)) {
    // then aSpec is relative
    if (!aBaseURI) {
      return NS_ERROR_MALFORMED_URI;
    }

    if (!aSpec.IsEmpty() && aSpec[0] == '#') {
      // Looks like a reference instead of a fully-specified URI.
      // --> initialize |uri| as a clone of |aBaseURI|, with ref appended.
      return NS_GetURIWithNewRef(aBaseURI, aSpec, aURI);
    }

    rv = aBaseURI->GetScheme(scheme);
    if (NS_FAILED(rv)) return rv;
  }

  if (scheme.EqualsLiteral("http") || scheme.EqualsLiteral("ws")) {
    return NewStandardURI(aSpec, aCharset, aBaseURI, NS_HTTP_DEFAULT_PORT,
                          aURI);
  }
  if (scheme.EqualsLiteral("https") || scheme.EqualsLiteral("wss")) {
    return NewStandardURI(aSpec, aCharset, aBaseURI, NS_HTTPS_DEFAULT_PORT,
                          aURI);
  }
  if (scheme.EqualsLiteral("ftp")) {
    return NewStandardURI(aSpec, aCharset, aBaseURI, 21, aURI);
  }
  if (scheme.EqualsLiteral("ssh")) {
    return NewStandardURI(aSpec, aCharset, aBaseURI, 22, aURI);
  }

  if (scheme.EqualsLiteral("file")) {
    nsAutoCString buf(aSpec);
#if defined(XP_WIN)
    buf.Truncate();
    if (!net_NormalizeFileURL(aSpec, buf)) {
      buf = aSpec;
    }
#endif

    nsCOMPtr<nsIURI> base(aBaseURI);
    return NS_MutateURI(new nsStandardURL::Mutator())
        .Apply(NS_MutatorMethod(&nsIFileURLMutator::MarkFileURL))
        .Apply(NS_MutatorMethod(&nsIStandardURLMutator::Init,
                                nsIStandardURL::URLTYPE_NO_AUTHORITY, -1, buf,
                                aCharset, base, nullptr))
        .Finalize(aURI);
  }

  if (scheme.EqualsLiteral("data")) {
    return nsDataHandler::CreateNewURI(aSpec, aCharset, aBaseURI, aURI);
  }

  if (scheme.EqualsLiteral("moz-safe-about") ||
      scheme.EqualsLiteral("page-icon") || scheme.EqualsLiteral("moz") ||
      scheme.EqualsLiteral("moz-anno") ||
      scheme.EqualsLiteral("moz-page-thumb") ||
      scheme.EqualsLiteral("moz-fonttable")) {
    return NS_MutateURI(new nsSimpleURI::Mutator())
        .SetSpec(aSpec)
        .Finalize(aURI);
  }

  if (scheme.EqualsLiteral("chrome")) {
    return nsChromeProtocolHandler::CreateNewURI(aSpec, aCharset, aBaseURI,
                                                 aURI);
  }

  if (scheme.EqualsLiteral("javascript")) {
    return nsJSProtocolHandler::CreateNewURI(aSpec, aCharset, aBaseURI, aURI);
  }

  if (scheme.EqualsLiteral("blob")) {
    return BlobURLProtocolHandler::CreateNewURI(aSpec, aCharset, aBaseURI,
                                                aURI);
  }

  if (scheme.EqualsLiteral("view-source")) {
    return nsViewSourceHandler::CreateNewURI(aSpec, aCharset, aBaseURI, aURI);
  }

  if (scheme.EqualsLiteral("resource")) {
    RefPtr<nsResProtocolHandler> handler = nsResProtocolHandler::GetSingleton();
    if (!handler) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    return handler->NewURI(aSpec, aCharset, aBaseURI, aURI);
  }

  if (scheme.EqualsLiteral("indexeddb")) {
    nsCOMPtr<nsIURI> base(aBaseURI);
    return NS_MutateURI(new nsStandardURL::Mutator())
        .Apply(NS_MutatorMethod(&nsIStandardURLMutator::Init,
                                nsIStandardURL::URLTYPE_AUTHORITY, 0,
                                nsCString(aSpec), aCharset, base, nullptr))
        .Finalize(aURI);
  }

  if (scheme.EqualsLiteral("moz-extension")) {
    RefPtr<mozilla::net::ExtensionProtocolHandler> handler =
        mozilla::net::ExtensionProtocolHandler::GetSingleton();
    if (!handler) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    return handler->NewURI(aSpec, aCharset, aBaseURI, aURI);
  }

  if (scheme.EqualsLiteral("about")) {
    return nsAboutProtocolHandler::CreateNewURI(aSpec, aCharset, aBaseURI,
                                                aURI);
  }

  if (scheme.EqualsLiteral("jar")) {
    nsCOMPtr<nsIURI> base(aBaseURI);
    return NS_MutateURI(new nsJARURI::Mutator())
        .Apply(NS_MutatorMethod(&nsIJARURIMutator::SetSpecBaseCharset,
                                nsCString(aSpec), base, aCharset))
        .Finalize(aURI);
  }

  if (scheme.EqualsLiteral("moz-icon")) {
    return NS_MutateURI(new nsMozIconURI::Mutator())
        .SetSpec(aSpec)
        .Finalize(aURI);
  }

#ifdef MOZ_WIDGET_GTK
  if (scheme.EqualsLiteral("smb") || scheme.EqualsLiteral("sftp")) {
    nsCOMPtr<nsIURI> base(aBaseURI);
    return NS_MutateURI(new nsStandardURL::Mutator())
        .Apply(NS_MutatorMethod(&nsIStandardURLMutator::Init,
                                nsIStandardURL::URLTYPE_STANDARD, -1,
                                nsCString(aSpec), aCharset, base, nullptr))
        .Finalize(aURI);
  }
#endif

  if (scheme.EqualsLiteral("android")) {
    nsCOMPtr<nsIURI> base(aBaseURI);
    return NS_MutateURI(NS_STANDARDURLMUTATOR_CONTRACTID)
        .Apply(NS_MutatorMethod(&nsIStandardURLMutator::Init,
                                nsIStandardURL::URLTYPE_STANDARD, -1,
                                nsCString(aSpec), aCharset, base, nullptr))
        .Finalize(aURI);
  }

  // web-extensions can add custom protocol implementations with standard URLs
  // that have notion of hostname, authority and relative URLs. Below we
  // manually check agains set of known protocols schemes until more general
  // solution is in place (See Bug 1569733)
  if (scheme.EqualsLiteral("dweb") || scheme.EqualsLiteral("dat") ||
      scheme.EqualsLiteral("ipfs") || scheme.EqualsLiteral("ipns") ||
      scheme.EqualsLiteral("ssb") || scheme.EqualsLiteral("wtp")) {
    return NewStandardURI(aSpec, aCharset, aBaseURI, -1, aURI);
  }

#if defined(MOZ_THUNDERBIRD) || defined(MOZ_SUITE)
  rv = NS_NewMailnewsURI(aURI, aSpec, aCharset, aBaseURI);
  if (rv != NS_ERROR_UNKNOWN_PROTOCOL) {
    return rv;
  }
#endif

  if (aBaseURI) {
    nsAutoCString newSpec;
    rv = aBaseURI->Resolve(aSpec, newSpec);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString newScheme;
    rv = net_ExtractURLScheme(newSpec, newScheme);
    if (NS_SUCCEEDED(rv)) {
      // The scheme shouldn't really change at this point.
      MOZ_DIAGNOSTIC_ASSERT(newScheme == scheme);
    }

    return NS_MutateURI(new nsSimpleURI::Mutator())
        .SetSpec(newSpec)
        .Finalize(aURI);
  }

  // Falls back to external protocol handler.
  return NS_MutateURI(new nsSimpleURI::Mutator()).SetSpec(aSpec).Finalize(aURI);
}

nsresult NS_GetSanitizedURIStringFromURI(nsIURI* aUri,
                                         nsAString& aSanitizedSpec) {
  aSanitizedSpec.Truncate();

  nsCOMPtr<nsISensitiveInfoHiddenURI> safeUri = do_QueryInterface(aUri);
  nsAutoCString cSpec;
  nsresult rv;
  if (safeUri) {
    rv = safeUri->GetSensitiveInfoHiddenSpec(cSpec);
  } else {
    rv = aUri->GetSpec(cSpec);
  }

  if (NS_SUCCEEDED(rv)) {
    aSanitizedSpec.Assign(NS_ConvertUTF8toUTF16(cSpec));
  }
  return rv;
}

nsresult NS_LoadPersistentPropertiesFromURISpec(
    nsIPersistentProperties** outResult, const nsACString& aSpec) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), uri,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIInputStream> in;
  rv = channel->Open(getter_AddRefs(in));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPersistentProperties> properties = new nsPersistentProperties();
  rv = properties->Load(in);
  NS_ENSURE_SUCCESS(rv, rv);

  properties.swap(*outResult);
  return NS_OK;
}

bool NS_UsePrivateBrowsing(nsIChannel* channel) {
  OriginAttributes attrs;
  bool result = NS_GetOriginAttributes(channel, attrs, false);
  NS_ENSURE_TRUE(result, result);
  return attrs.mPrivateBrowsingId > 0;
}

bool NS_GetOriginAttributes(nsIChannel* aChannel,
                            mozilla::OriginAttributes& aAttributes,
                            bool aUsingStoragePrincipal) {
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  loadInfo->GetOriginAttributes(&aAttributes);

  bool isPrivate = false;
  nsCOMPtr<nsIPrivateBrowsingChannel> pbChannel = do_QueryInterface(aChannel);
  if (pbChannel) {
    nsresult rv = pbChannel->GetIsChannelPrivate(&isPrivate);
    NS_ENSURE_SUCCESS(rv, false);
  } else {
    // Some channels may not implement nsIPrivateBrowsingChannel
    nsCOMPtr<nsILoadContext> loadContext;
    NS_QueryNotificationCallbacks(aChannel, loadContext);
    isPrivate = loadContext && loadContext->UsePrivateBrowsing();
  }
  aAttributes.SyncAttributesWithPrivateBrowsing(isPrivate);

  if (aUsingStoragePrincipal) {
    StoragePrincipalHelper::PrepareOriginAttributes(aChannel, aAttributes);
  }
  return true;
}

bool NS_HasBeenCrossOrigin(nsIChannel* aChannel, bool aReport) {
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  // TYPE_DOCUMENT loads have a null LoadingPrincipal and can not be cross
  // origin.
  if (!loadInfo->LoadingPrincipal()) {
    return false;
  }

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

  for (nsIRedirectHistoryEntry* redirectHistoryEntry :
       loadInfo->RedirectChain()) {
    nsCOMPtr<nsIPrincipal> principal;
    redirectHistoryEntry->GetPrincipal(getter_AddRefs(principal));
    if (!principal) {
      return true;
    }

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

bool NS_IsSafeTopLevelNav(nsIChannel* aChannel) {
  if (!aChannel) {
    return false;
  }
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  if (loadInfo->GetExternalContentPolicyType() !=
      nsIContentPolicy::TYPE_DOCUMENT) {
    return false;
  }
  RefPtr<HttpBaseChannel> baseChan = do_QueryObject(aChannel);
  if (!baseChan) {
    return false;
  }
  nsHttpRequestHead* requestHead = baseChan->GetRequestHead();
  if (!requestHead) {
    return false;
  }
  return requestHead->IsSafeMethod();
}

bool NS_IsSameSiteForeign(nsIChannel* aChannel, nsIURI* aHostURI) {
  if (!aChannel) {
    return false;
  }
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  // Do not treat loads triggered by web extensions as foreign
  nsCOMPtr<nsIURI> channelURI;
  NS_GetFinalChannelURI(aChannel, getter_AddRefs(channelURI));
  if (BasePrincipal::Cast(loadInfo->TriggeringPrincipal())
          ->AddonAllowsLoad(channelURI)) {
    return false;
  }

  nsCOMPtr<nsIURI> uri;
  if (loadInfo->GetExternalContentPolicyType() ==
      nsIContentPolicy::TYPE_DOCUMENT) {
    // for loads of TYPE_DOCUMENT we query the hostURI from the
    // triggeringPricnipal which returns the URI of the document that caused the
    // navigation.
    loadInfo->TriggeringPrincipal()->GetURI(getter_AddRefs(uri));
  } else {
    uri = aHostURI;
  }

  nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
      do_GetService(THIRDPARTYUTIL_CONTRACTID);
  if (!thirdPartyUtil) {
    return false;
  }

  bool isForeign = true;
  nsresult rv = thirdPartyUtil->IsThirdPartyChannel(aChannel, uri, &isForeign);
  // if we are dealing with a cross origin request, we can return here
  // because we already know the request is 'foreign'.
  if (NS_FAILED(rv) || isForeign) {
    return true;
  }

  // for loads of TYPE_SUBDOCUMENT we have to perform an additional test,
  // because a cross-origin iframe might perform a navigation to a same-origin
  // iframe which would send same-site cookies. Hence, if the iframe navigation
  // was triggered by a cross-origin triggeringPrincipal, we treat the load as
  // foreign.
  if (loadInfo->GetExternalContentPolicyType() ==
      nsIContentPolicy::TYPE_SUBDOCUMENT) {
    nsCOMPtr<nsIURI> triggeringPrincipalURI;
    loadInfo->TriggeringPrincipal()->GetURI(
        getter_AddRefs(triggeringPrincipalURI));
    rv = thirdPartyUtil->IsThirdPartyChannel(aChannel, triggeringPrincipalURI,
                                             &isForeign);
    if (NS_FAILED(rv) || isForeign) {
      return true;
    }
  }

  // for the purpose of same-site cookies we have to treat any cross-origin
  // redirects as foreign. E.g. cross-site to same-site redirect is a problem
  // with regards to CSRF.

  nsCOMPtr<nsIPrincipal> redirectPrincipal;
  nsCOMPtr<nsIURI> redirectURI;
  for (nsIRedirectHistoryEntry* entry : loadInfo->RedirectChain()) {
    entry->GetPrincipal(getter_AddRefs(redirectPrincipal));
    if (redirectPrincipal) {
      redirectPrincipal->GetURI(getter_AddRefs(redirectURI));
      rv = thirdPartyUtil->IsThirdPartyChannel(aChannel, redirectURI,
                                               &isForeign);
      // if at any point we encounter a cross-origin redirect we can return.
      if (NS_FAILED(rv) || isForeign) {
        return true;
      }
    }
  }
  return isForeign;
}

bool NS_ShouldCheckAppCache(nsIPrincipal* aPrincipal) {
  uint32_t privateBrowsingId = 0;
  nsresult rv = aPrincipal->GetPrivateBrowsingId(&privateBrowsingId);
  if (NS_SUCCEEDED(rv) && (privateBrowsingId > 0)) {
    return false;
  }

  nsCOMPtr<nsIOfflineCacheUpdateService> offlineService =
      do_GetService("@mozilla.org/offlinecacheupdate-service;1");
  if (!offlineService) {
    return false;
  }

  bool allowed;
  rv = offlineService->OfflineAppAllowed(aPrincipal, nullptr, &allowed);
  return NS_SUCCEEDED(rv) && allowed;
}

void NS_WrapAuthPrompt(nsIAuthPrompt* aAuthPrompt,
                       nsIAuthPrompt2** aAuthPrompt2) {
  nsCOMPtr<nsIAuthPromptAdapterFactory> factory =
      do_GetService(NS_AUTHPROMPT_ADAPTER_FACTORY_CONTRACTID);
  if (!factory) return;

  NS_WARNING("Using deprecated nsIAuthPrompt");
  factory->CreateAdapter(aAuthPrompt, aAuthPrompt2);
}

void NS_QueryAuthPrompt2(nsIInterfaceRequestor* aCallbacks,
                         nsIAuthPrompt2** aAuthPrompt) {
  CallGetInterface(aCallbacks, aAuthPrompt);
  if (*aAuthPrompt) return;

  // Maybe only nsIAuthPrompt is provided and we have to wrap it.
  nsCOMPtr<nsIAuthPrompt> prompt(do_GetInterface(aCallbacks));
  if (!prompt) return;

  NS_WrapAuthPrompt(prompt, aAuthPrompt);
}

void NS_QueryAuthPrompt2(nsIChannel* aChannel, nsIAuthPrompt2** aAuthPrompt) {
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
    if (*aAuthPrompt) return;
  }

  nsCOMPtr<nsILoadGroup> group;
  aChannel->GetLoadGroup(getter_AddRefs(group));
  if (!group) return;

  group->GetNotificationCallbacks(getter_AddRefs(callbacks));
  if (!callbacks) return;
  NS_QueryAuthPrompt2(callbacks, aAuthPrompt);
}

nsresult NS_NewNotificationCallbacksAggregation(
    nsIInterfaceRequestor* callbacks, nsILoadGroup* loadGroup,
    nsIEventTarget* target, nsIInterfaceRequestor** result) {
  nsCOMPtr<nsIInterfaceRequestor> cbs;
  if (loadGroup) loadGroup->GetNotificationCallbacks(getter_AddRefs(cbs));
  return NS_NewInterfaceRequestorAggregation(callbacks, cbs, target, result);
}

nsresult NS_NewNotificationCallbacksAggregation(
    nsIInterfaceRequestor* callbacks, nsILoadGroup* loadGroup,
    nsIInterfaceRequestor** result) {
  return NS_NewNotificationCallbacksAggregation(callbacks, loadGroup, nullptr,
                                                result);
}

nsresult NS_DoImplGetInnermostURI(nsINestedURI* nestedURI, nsIURI** result) {
  MOZ_ASSERT(nestedURI, "Must have a nested URI!");
  MOZ_ASSERT(!*result, "Must have null *result");

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

nsresult NS_ImplGetInnermostURI(nsINestedURI* nestedURI, nsIURI** result) {
  // Make it safe to use swap()
  *result = nullptr;

  return NS_DoImplGetInnermostURI(nestedURI, result);
}

already_AddRefed<nsIURI> NS_GetInnermostURI(nsIURI* aURI) {
  MOZ_ASSERT(aURI, "Must have URI");

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

nsresult NS_GetFinalChannelURI(nsIChannel* channel, nsIURI** uri) {
  *uri = nullptr;

  nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
  nsCOMPtr<nsIURI> resultPrincipalURI;
  loadInfo->GetResultPrincipalURI(getter_AddRefs(resultPrincipalURI));
  if (resultPrincipalURI) {
    resultPrincipalURI.forget(uri);
    return NS_OK;
  }
  return channel->GetOriginalURI(uri);
}

nsresult NS_URIChainHasFlags(nsIURI* uri, uint32_t flags, bool* result) {
  nsresult rv;
  nsCOMPtr<nsINetUtil> util = do_GetNetUtil(&rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return util->URIChainHasFlags(uri, flags, result);
}

uint32_t NS_SecurityHashURI(nsIURI* aURI) {
  nsCOMPtr<nsIURI> baseURI = NS_GetInnermostURI(aURI);

  nsAutoCString scheme;
  uint32_t schemeHash = 0;
  if (NS_SUCCEEDED(baseURI->GetScheme(scheme)))
    schemeHash = mozilla::HashString(scheme);

  // TODO figure out how to hash file:// URIs
  if (scheme.EqualsLiteral("file")) return schemeHash;  // sad face

#if IS_ORIGIN_IS_FULL_SPEC_DEFINED
  bool hasFlag;
  if (NS_FAILED(NS_URIChainHasFlags(
          baseURI, nsIProtocolHandler::ORIGIN_IS_FULL_SPEC, &hasFlag)) ||
      hasFlag) {
    nsAutoCString spec;
    uint32_t specHash;
    nsresult res = baseURI->GetSpec(spec);
    if (NS_SUCCEEDED(res))
      specHash = mozilla::HashString(spec);
    else
      specHash = static_cast<uint32_t>(res);
    return specHash;
  }
#endif

  nsAutoCString host;
  uint32_t hostHash = 0;
  if (NS_SUCCEEDED(baseURI->GetAsciiHost(host)))
    hostHash = mozilla::HashString(host);

  return mozilla::AddToHash(schemeHash, hostHash, NS_GetRealPort(baseURI));
}

bool NS_SecurityCompareURIs(nsIURI* aSourceURI, nsIURI* aTargetURI,
                            bool aStrictFileOriginPolicy) {
  nsresult rv;

  // Note that this is not an Equals() test on purpose -- for URIs that don't
  // support host/port, we want equality to basically be object identity, for
  // security purposes.  Otherwise, for example, two javascript: URIs that
  // are otherwise unrelated could end up "same origin", which would be
  // unfortunate.
  if (aSourceURI && aSourceURI == aTargetURI) {
    return true;
  }

  if (!aTargetURI || !aSourceURI) {
    return false;
  }

  // If either URI is a nested URI, get the base URI
  nsCOMPtr<nsIURI> sourceBaseURI = NS_GetInnermostURI(aSourceURI);
  nsCOMPtr<nsIURI> targetBaseURI = NS_GetInnermostURI(aTargetURI);

#if defined(MOZ_THUNDERBIRD) || defined(MOZ_SUITE)
  // Check if either URI has a special origin.
  nsCOMPtr<nsIURI> origin;
  nsCOMPtr<nsIURIWithSpecialOrigin> uriWithSpecialOrigin =
      do_QueryInterface(sourceBaseURI);
  if (uriWithSpecialOrigin) {
    rv = uriWithSpecialOrigin->GetOrigin(getter_AddRefs(origin));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }
    MOZ_ASSERT(origin);
    sourceBaseURI = origin;
  }
  uriWithSpecialOrigin = do_QueryInterface(targetBaseURI);
  if (uriWithSpecialOrigin) {
    rv = uriWithSpecialOrigin->GetOrigin(getter_AddRefs(origin));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }
    MOZ_ASSERT(origin);
    targetBaseURI = origin;
  }
#endif

  nsCOMPtr<nsIPrincipal> sourceBlobPrincipal;
  if (BlobURLProtocolHandler::GetBlobURLPrincipal(
          sourceBaseURI, getter_AddRefs(sourceBlobPrincipal))) {
    nsCOMPtr<nsIURI> sourceBlobOwnerURI;
    rv = sourceBlobPrincipal->GetURI(getter_AddRefs(sourceBlobOwnerURI));
    if (NS_SUCCEEDED(rv)) {
      sourceBaseURI = sourceBlobOwnerURI;
    }
  }

  nsCOMPtr<nsIPrincipal> targetBlobPrincipal;
  if (BlobURLProtocolHandler::GetBlobURLPrincipal(
          targetBaseURI, getter_AddRefs(targetBlobPrincipal))) {
    nsCOMPtr<nsIURI> targetBlobOwnerURI;
    rv = targetBlobPrincipal->GetURI(getter_AddRefs(targetBlobOwnerURI));
    if (NS_SUCCEEDED(rv)) {
      targetBaseURI = targetBlobOwnerURI;
    }
  }

  if (!sourceBaseURI || !targetBaseURI) return false;

  // Compare schemes
  nsAutoCString targetScheme;
  bool sameScheme = false;
  if (NS_FAILED(targetBaseURI->GetScheme(targetScheme)) ||
      NS_FAILED(sourceBaseURI->SchemeIs(targetScheme.get(), &sameScheme)) ||
      !sameScheme) {
    // Not same-origin if schemes differ
    return false;
  }

  // For file scheme, reject unless the files are identical. See
  // NS_RelaxStrictFileOriginPolicy for enforcing file same-origin checking
  if (targetScheme.EqualsLiteral("file")) {
    // in traditional unsafe behavior all files are the same origin
    if (!aStrictFileOriginPolicy) return true;

    nsCOMPtr<nsIFileURL> sourceFileURL(do_QueryInterface(sourceBaseURI));
    nsCOMPtr<nsIFileURL> targetFileURL(do_QueryInterface(targetBaseURI));

    if (!sourceFileURL || !targetFileURL) return false;

    nsCOMPtr<nsIFile> sourceFile, targetFile;

    sourceFileURL->GetFile(getter_AddRefs(sourceFile));
    targetFileURL->GetFile(getter_AddRefs(targetFile));

    if (!sourceFile || !targetFile) return false;

    // Otherwise they had better match
    bool filesAreEqual = false;
    rv = sourceFile->Equals(targetFile, &filesAreEqual);
    return NS_SUCCEEDED(rv) && filesAreEqual;
  }

#if IS_ORIGIN_IS_FULL_SPEC_DEFINED
  bool hasFlag;
  if (NS_FAILED(NS_URIChainHasFlags(
          targetBaseURI, nsIProtocolHandler::ORIGIN_IS_FULL_SPEC, &hasFlag)) ||
      hasFlag) {
    // URIs with this flag have the whole spec as a distinct trust
    // domain; use the whole spec for comparison
    nsAutoCString targetSpec;
    nsAutoCString sourceSpec;
    return (NS_SUCCEEDED(targetBaseURI->GetSpec(targetSpec)) &&
            NS_SUCCEEDED(sourceBaseURI->GetSpec(sourceSpec)) &&
            targetSpec.Equals(sourceSpec));
  }
#endif

  // Compare hosts
  nsAutoCString targetHost;
  nsAutoCString sourceHost;
  if (NS_FAILED(targetBaseURI->GetAsciiHost(targetHost)) ||
      NS_FAILED(sourceBaseURI->GetAsciiHost(sourceHost))) {
    return false;
  }

  nsCOMPtr<nsIStandardURL> targetURL(do_QueryInterface(targetBaseURI));
  nsCOMPtr<nsIStandardURL> sourceURL(do_QueryInterface(sourceBaseURI));
  if (!targetURL || !sourceURL) {
    return false;
  }

  if (!targetHost.Equals(sourceHost, nsCaseInsensitiveCStringComparator())) {
    return false;
  }

  return NS_GetRealPort(targetBaseURI) == NS_GetRealPort(sourceBaseURI);
}

bool NS_URIIsLocalFile(nsIURI* aURI) {
  nsCOMPtr<nsINetUtil> util = do_GetNetUtil();

  bool isFile;
  return util &&
         NS_SUCCEEDED(util->ProtocolHasFlags(
             aURI, nsIProtocolHandler::URI_IS_LOCAL_FILE, &isFile)) &&
         isFile;
}

bool NS_RelaxStrictFileOriginPolicy(nsIURI* aTargetURI, nsIURI* aSourceURI,
                                    bool aAllowDirectoryTarget /* = false */) {
  if (!NS_URIIsLocalFile(aTargetURI)) {
    // This is probably not what the caller intended
    MOZ_ASSERT_UNREACHABLE(
        "NS_RelaxStrictFileOriginPolicy called with non-file URI");
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
      !targetFile || !sourceFile || NS_FAILED(targetFile->Normalize()) ||
#ifndef MOZ_WIDGET_ANDROID
      NS_FAILED(sourceFile->Normalize()) ||
#endif
      (!aAllowDirectoryTarget &&
       (NS_FAILED(targetFile->IsDirectory(&targetIsDir)) || targetIsDir))) {
    return false;
  }

  if (!StaticPrefs::privacy_file_unique_origin()) {
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
  }

  return false;
}

bool NS_IsInternalSameURIRedirect(nsIChannel* aOldChannel,
                                  nsIChannel* aNewChannel, uint32_t aFlags) {
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

bool NS_IsHSTSUpgradeRedirect(nsIChannel* aOldChannel, nsIChannel* aNewChannel,
                              uint32_t aFlags) {
  if (!(aFlags & nsIChannelEventSink::REDIRECT_STS_UPGRADE)) {
    return false;
  }

  nsCOMPtr<nsIURI> oldURI, newURI;
  aOldChannel->GetURI(getter_AddRefs(oldURI));
  aNewChannel->GetURI(getter_AddRefs(newURI));

  if (!oldURI || !newURI) {
    return false;
  }

  if (!oldURI->SchemeIs("http")) {
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

nsresult NS_LinkRedirectChannels(uint32_t channelId,
                                 nsIParentChannel* parentChannel,
                                 nsIChannel** _result) {
  nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
      RedirectChannelRegistrar::GetOrCreate();
  MOZ_ASSERT(registrar);

  return registrar->LinkChannels(channelId, parentChannel, _result);
}

nsresult NS_MaybeOpenChannelUsingOpen(nsIChannel* aChannel,
                                      nsIInputStream** aStream) {
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  return aChannel->Open(aStream);
}

nsresult NS_MaybeOpenChannelUsingAsyncOpen(nsIChannel* aChannel,
                                           nsIStreamListener* aListener) {
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  return aChannel->AsyncOpen(aListener);
}

/** Given the first (disposition) token from a Content-Disposition header,
 * tell whether it indicates the content is inline or attachment
 * @param aDispToken the disposition token from the content-disposition header
 */
uint32_t NS_GetContentDispositionFromToken(const nsAString& aDispToken) {
  // RFC 2183, section 2.8 says that an unknown disposition
  // value should be treated as "attachment"
  // If all of these tests eval to false, then we have a content-disposition of
  // "attachment" or unknown
  if (aDispToken.IsEmpty() || aDispToken.LowerCaseEqualsLiteral("inline") ||
      // Broken sites just send
      // Content-Disposition: filename="file"
      // without a disposition token... screen those out.
      StringHead(aDispToken, 8).LowerCaseEqualsLiteral("filename"))
    return nsIChannel::DISPOSITION_INLINE;

  return nsIChannel::DISPOSITION_ATTACHMENT;
}

uint32_t NS_GetContentDispositionFromHeader(const nsACString& aHeader,
                                            nsIChannel* aChan /* = nullptr */) {
  nsresult rv;
  nsCOMPtr<nsIMIMEHeaderParam> mimehdrpar =
      do_GetService(NS_MIMEHEADERPARAM_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return nsIChannel::DISPOSITION_ATTACHMENT;

  nsAutoString dispToken;
  rv = mimehdrpar->GetParameterHTTP(aHeader, "", EmptyCString(), true, nullptr,
                                    dispToken);

  if (NS_FAILED(rv)) {
    // special case (see bug 272541): empty disposition type handled as "inline"
    if (rv == NS_ERROR_FIRST_HEADER_FIELD_COMPONENT_EMPTY)
      return nsIChannel::DISPOSITION_INLINE;
    return nsIChannel::DISPOSITION_ATTACHMENT;
  }

  return NS_GetContentDispositionFromToken(dispToken);
}

nsresult NS_GetFilenameFromDisposition(nsAString& aFilename,
                                       const nsACString& aDisposition,
                                       nsIURI* aURI /* = nullptr */) {
  aFilename.Truncate();

  nsresult rv;
  nsCOMPtr<nsIMIMEHeaderParam> mimehdrpar =
      do_GetService(NS_MIMEHEADERPARAM_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  // Get the value of 'filename' parameter
  rv = mimehdrpar->GetParameterHTTP(aDisposition, "filename", EmptyCString(),
                                    true, nullptr, aFilename);

  if (NS_FAILED(rv)) {
    aFilename.Truncate();
    return rv;
  }

  if (aFilename.IsEmpty()) return NS_ERROR_NOT_AVAILABLE;

  return NS_OK;
}

void net_EnsurePSMInit() {
  nsresult rv;
  nsCOMPtr<nsISupports> psm = do_GetService(PSM_COMPONENT_CONTRACTID, &rv);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  nsCOMPtr<nsISupports> sss = do_GetService(NS_SSSERVICE_CONTRACTID);
#ifdef MOZ_NEW_CERT_STORAGE
  nsCOMPtr<nsISupports> cbl = do_GetService(NS_CERTSTORAGE_CONTRACTID);
#else
  nsCOMPtr<nsISupports> cbl = do_GetService(NS_CERTBLOCKLIST_CONTRACTID);
#endif
  nsCOMPtr<nsISupports> cos = do_GetService(NS_CERTOVERRIDE_CONTRACTID);
}

bool NS_IsAboutBlank(nsIURI* uri) {
  // GetSpec can be expensive for some URIs, so check the scheme first.
  if (!uri->SchemeIs("about")) {
    return false;
  }

  return uri->GetSpecOrDefault().EqualsLiteral("about:blank");
}

nsresult NS_GenerateHostPort(const nsCString& host, int32_t port,
                             nsACString& hostLine) {
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
  } else
    hostLine.Assign(host);
  if (port != -1) {
    hostLine.Append(':');
    hostLine.AppendInt(port);
  }
  return NS_OK;
}

void NS_SniffContent(const char* aSnifferType, nsIRequest* aRequest,
                     const uint8_t* aData, uint32_t aLength,
                     nsACString& aSniffedType) {
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
    nsresult rv = sniffers[i]->GetMIMETypeFromContent(aRequest, aData, aLength,
                                                      aSniffedType);
    if (NS_SUCCEEDED(rv) && !aSniffedType.IsEmpty()) {
      return;
    }
  }

  aSniffedType.Truncate();

  // If the Sniffers did not hit and NoSniff is set
  // Check if we have any MIME Type at all or report an
  // Error to the Console
  nsCOMPtr<nsIHttpChannel> channel = do_QueryInterface(aRequest);
  if (channel) {
    nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();

    if (loadInfo->GetSkipContentSniffing()) {
      nsAutoCString type;
      channel->GetContentType(type);

      if (type.Equals(nsCString("application/x-unknown-content-type"))) {
        nsCOMPtr<nsIURI> requestUri;
        channel->GetURI(getter_AddRefs(requestUri));
        nsAutoCString spec;
        requestUri->GetSpec(spec);
        if (spec.Length() > 50) {
          spec.Truncate(50);
          spec.AppendLiteral("...");
        }
        channel->LogMimeTypeMismatch(
            nsCString("XTCOWithMIMEValueMissing"), false,
            NS_ConvertUTF8toUTF16(spec),
            // Type is not used in the Error Message but required
            NS_ConvertUTF8toUTF16(type));
      }
    }
  }
}

bool NS_IsSrcdocChannel(nsIChannel* aChannel) {
  bool isSrcdoc;
  nsCOMPtr<nsIInputStreamChannel> isr = do_QueryInterface(aChannel);
  if (isr) {
    isr->GetIsSrcdocChannel(&isSrcdoc);
    return isSrcdoc;
  }
  nsCOMPtr<nsIViewSourceChannel> vsc = do_QueryInterface(aChannel);
  if (vsc) {
    nsresult rv = vsc->GetIsSrcdocChannel(&isSrcdoc);
    if (NS_SUCCEEDED(rv)) {
      return isSrcdoc;
    }
  }
  return false;
}

nsresult NS_ShouldSecureUpgrade(
    nsIURI* aURI, nsILoadInfo* aLoadInfo, nsIPrincipal* aChannelResultPrincipal,
    bool aPrivateBrowsing, bool aAllowSTS,
    const OriginAttributes& aOriginAttributes, bool& aShouldUpgrade,
    std::function<void(bool, nsresult)>&& aResultCallback,
    bool& aWillCallback) {
  aWillCallback = false;

  // Even if we're in private browsing mode, we still enforce existing STS
  // data (it is read-only).
  // if the connection is not using SSL and either the exact host matches or
  // a superdomain wants to force HTTPS, do it.
  bool isHttps = aURI->SchemeIs("https");

  if (!isHttps &&
      !nsMixedContentBlocker::IsPotentiallyTrustworthyLoopbackURL(aURI)) {
    if (aLoadInfo) {
      // If any of the documents up the chain to the root document makes use of
      // the CSP directive 'upgrade-insecure-requests', then it's time to
      // fulfill the promise to CSP and mixed content blocking to upgrade the
      // channel from http to https.
      if (aLoadInfo->GetUpgradeInsecureRequests() ||
          aLoadInfo->GetBrowserUpgradeInsecureRequests()) {
        // let's log a message to the console that we are upgrading a request
        nsAutoCString scheme;
        aURI->GetScheme(scheme);
        // append the additional 's' for security to the scheme :-)
        scheme.AppendLiteral("s");
        NS_ConvertUTF8toUTF16 reportSpec(aURI->GetSpecOrDefault());
        NS_ConvertUTF8toUTF16 reportScheme(scheme);

        if (aLoadInfo->GetUpgradeInsecureRequests()) {
          AutoTArray<nsString, 2> params = {reportSpec, reportScheme};
          uint32_t innerWindowId = aLoadInfo->GetInnerWindowID();
          CSP_LogLocalizedStr(
              "upgradeInsecureRequest", params,
              EmptyString(),  // aSourceFile
              EmptyString(),  // aScriptSample
              0,              // aLineNumber
              0,              // aColumnNumber
              nsIScriptError::warningFlag,
              NS_LITERAL_CSTRING("upgradeInsecureRequest"), innerWindowId,
              !!aLoadInfo->GetOriginAttributes().mPrivateBrowsingId);
          Telemetry::AccumulateCategorical(
              Telemetry::LABELS_HTTP_SCHEME_UPGRADE_TYPE::CSP);
        } else {
          RefPtr<dom::Document> doc;
          nsINode* node = aLoadInfo->LoadingNode();
          if (node) {
            doc = node->OwnerDoc();
          }

          nsAutoString brandName;
          nsresult rv = nsContentUtils::GetLocalizedString(
              nsContentUtils::eBRAND_PROPERTIES, "brandShortName", brandName);
          if (NS_SUCCEEDED(rv)) {
            AutoTArray<nsString, 3> params = {brandName, reportSpec,
                                              reportScheme};
            nsContentUtils::ReportToConsole(
                nsIScriptError::warningFlag,
                NS_LITERAL_CSTRING("DATA_URI_BLOCKED"), doc,
                nsContentUtils::eSECURITY_PROPERTIES,
                "BrowserUpgradeInsecureDisplayRequest", params);
          }
          Telemetry::AccumulateCategorical(
              Telemetry::LABELS_HTTP_SCHEME_UPGRADE_TYPE::BrowserDisplay);
        }

        aShouldUpgrade = true;
        return NS_OK;
      }
    }

    // enforce Strict-Transport-Security
    nsISiteSecurityService* sss = gHttpHandler->GetSSService();
    NS_ENSURE_TRUE(sss, NS_ERROR_OUT_OF_MEMORY);

    bool isStsHost = false;
    uint32_t hstsSource = 0;
    uint32_t flags =
        aPrivateBrowsing ? nsISocketProvider::NO_PERMANENT_STORAGE : 0;

    auto handleResultFunc = [aAllowSTS](bool aIsStsHost, uint32_t aHstsSource) {
      if (aIsStsHost) {
        LOG(("nsHttpChannel::Connect() STS permissions found\n"));
        if (aAllowSTS) {
          Telemetry::AccumulateCategorical(
              Telemetry::LABELS_HTTP_SCHEME_UPGRADE_TYPE::STS);
          switch (aHstsSource) {
            case nsISiteSecurityService::SOURCE_PRELOAD_LIST:
              Telemetry::Accumulate(Telemetry::HSTS_UPGRADE_SOURCE, 0);
              break;
            case nsISiteSecurityService::SOURCE_ORGANIC_REQUEST:
              Telemetry::Accumulate(Telemetry::HSTS_UPGRADE_SOURCE, 1);
              break;
            case nsISiteSecurityService::SOURCE_UNKNOWN:
            default:
              // record this as an organic request
              Telemetry::Accumulate(Telemetry::HSTS_UPGRADE_SOURCE, 1);
              break;
          }
          return true;
        }
        Telemetry::AccumulateCategorical(
            Telemetry::LABELS_HTTP_SCHEME_UPGRADE_TYPE::PrefBlockedSTS);
      } else {
        Telemetry::AccumulateCategorical(
            Telemetry::LABELS_HTTP_SCHEME_UPGRADE_TYPE::NoReasonToUpgrade);
      }
      return false;
    };

    // Calling |IsSecureURI| before the storage is ready to read will
    // block the main thread. Once the storage is ready, we can call it
    // from main thread.
    static Atomic<bool, Relaxed> storageReady(false);
    if (!storageReady && gSocketTransportService && aResultCallback) {
      nsCOMPtr<nsIURI> uri = aURI;
      nsCOMPtr<nsISiteSecurityService> service = sss;
      nsresult rv = gSocketTransportService->Dispatch(
          NS_NewRunnableFunction(
              "net::NS_ShouldSecureUpgrade",
              [service{std::move(service)}, uri{std::move(uri)}, flags(flags),
               originAttributes(aOriginAttributes),
               handleResultFunc{std::move(handleResultFunc)},
               resultCallback{std::move(aResultCallback)}]() mutable {
                uint32_t hstsSource = 0;
                bool isStsHost = false;
                nsresult rv = service->IsSecureURI(
                    nsISiteSecurityService::HEADER_HSTS, uri, flags,
                    originAttributes, nullptr, &hstsSource, &isStsHost);

                // Successfully get the result from |IsSecureURI| implies that
                // the storage is ready to read.
                storageReady = NS_SUCCEEDED(rv);
                bool shouldUpgrade = handleResultFunc(isStsHost, hstsSource);

                NS_DispatchToMainThread(NS_NewRunnableFunction(
                    "net::NS_ShouldSecureUpgrade::ResultCallback",
                    [rv, shouldUpgrade,
                     resultCallback{std::move(resultCallback)}]() {
                      resultCallback(shouldUpgrade, rv);
                    }));
              }),
          NS_DISPATCH_NORMAL);
      aWillCallback = NS_SUCCEEDED(rv);
      return rv;
    }

    nsresult rv =
        sss->IsSecureURI(nsISiteSecurityService::HEADER_HSTS, aURI, flags,
                         aOriginAttributes, nullptr, &hstsSource, &isStsHost);

    // if the SSS check fails, it's likely because this load is on a
    // malformed URI or something else in the setup is wrong, so any error
    // should be reported.
    NS_ENSURE_SUCCESS(rv, rv);

    aShouldUpgrade = handleResultFunc(isStsHost, hstsSource);
    return NS_OK;
  }

  Telemetry::AccumulateCategorical(
      Telemetry::LABELS_HTTP_SCHEME_UPGRADE_TYPE::AlreadyHTTPS);
  aShouldUpgrade = false;
  return NS_OK;
}

nsresult NS_GetSecureUpgradedURI(nsIURI* aURI, nsIURI** aUpgradedURI) {
  NS_MutateURI mutator(aURI);
  mutator.SetScheme(
      NS_LITERAL_CSTRING("https"));  // Change the scheme to HTTPS:

  // Change the default port to 443:
  nsCOMPtr<nsIStandardURL> stdURL = do_QueryInterface(aURI);
  if (stdURL) {
    mutator.Apply(
        NS_MutatorMethod(&nsIStandardURLMutator::SetDefaultPort, 443, nullptr));
  } else {
    // If we don't have a nsStandardURL, fall back to using GetPort/SetPort.
    // XXXdholbert Is this function even called with a non-nsStandardURL arg,
    // in practice?
    NS_WARNING("Calling NS_GetSecureUpgradedURI for non nsStandardURL");
    int32_t oldPort = -1;
    nsresult rv = aURI->GetPort(&oldPort);
    if (NS_FAILED(rv)) return rv;

    // Keep any nonstandard ports so only the scheme is changed.
    // For example:
    //  http://foo.com:80 -> https://foo.com:443
    //  http://foo.com:81 -> https://foo.com:81

    if (oldPort == 80 || oldPort == -1) {
      mutator.SetPort(-1);
    } else {
      mutator.SetPort(oldPort);
    }
  }

  return mutator.Finalize(aUpgradedURI);
}

nsresult NS_CompareLoadInfoAndLoadContext(nsIChannel* aChannel) {
  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();

  nsCOMPtr<nsILoadContext> loadContext;
  NS_QueryNotificationCallbacks(aChannel, loadContext);
  if (!loadContext) {
    return NS_OK;
  }

  // We try to skip about:newtab.
  // about:newtab will use SystemPrincipal to download thumbnails through
  // https:// and blob URLs.
  bool isAboutPage = false;
  nsINode* node = loadInfo->LoadingNode();
  if (node) {
    nsIURI* uri = node->OwnerDoc()->GetDocumentURI();
    isAboutPage = uri->SchemeIs("about");
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

  OriginAttributes originAttrsLoadInfo = loadInfo->GetOriginAttributes();
  OriginAttributes originAttrsLoadContext;
  loadContext->GetOriginAttributes(originAttrsLoadContext);

  LOG(
      ("NS_CompareLoadInfoAndLoadContext - loadInfo: %d, %d; "
       "loadContext: %d, %d. [channel=%p]",
       originAttrsLoadInfo.mUserContextId,
       originAttrsLoadInfo.mPrivateBrowsingId,
       originAttrsLoadContext.mUserContextId,
       originAttrsLoadContext.mPrivateBrowsingId, aChannel));

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

nsresult NS_SetRequestBlockingReason(nsIChannel* channel, uint32_t reason) {
  NS_ENSURE_ARG(channel);

  nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
  return NS_SetRequestBlockingReason(loadInfo, reason);
}

nsresult NS_SetRequestBlockingReason(nsILoadInfo* loadInfo, uint32_t reason) {
  NS_ENSURE_ARG(loadInfo);

  return loadInfo->SetRequestBlockingReason(reason);
}

nsresult NS_SetRequestBlockingReasonIfNull(nsILoadInfo* loadInfo,
                                           uint32_t reason) {
  NS_ENSURE_ARG(loadInfo);

  uint32_t existingReason;
  if (NS_SUCCEEDED(loadInfo->GetRequestBlockingReason(&existingReason)) &&
      existingReason != nsILoadInfo::BLOCKING_REASON_NONE) {
    return NS_OK;
  }

  return loadInfo->SetRequestBlockingReason(reason);
}

bool NS_IsOffline() {
  bool offline = true;
  bool connectivity = true;
  nsCOMPtr<nsIIOService> ios = do_GetIOService();
  if (ios) {
    ios->GetOffline(&offline);
    ios->GetConnectivity(&connectivity);
  }
  return offline || !connectivity;
}

/**
 * This function returns true if this channel should be classified by
 * the URL Classifier, false otherwise.
 *
 * The idea of the algorithm to determine if a channel should be
 * classified is based on:
 * 1. Channels created by non-privileged code should be classified.
 * 2. Top-level document’s channels, if loaded by privileged code
 *    (system principal), should be classified.
 * 3. Any other channel, created by privileged code, is considered safe.
 *
 * A bad/hacked/corrupted safebrowsing database, plus a mistakenly
 * classified critical channel (this may result from a bug in the exemption
 * rules or incorrect information being passed into) can cause serious
 * problems. For example, if the updater channel is classified and blocked
 * by the Safe Browsing, Firefox can't update itself, and there is no way to
 * recover from that.
 *
 * So two safeguards are added to ensure critical channels are never
 * automatically classified either because there is a bug in the algorithm
 * or the data in loadinfo is wrong.
 * 1. beConservative, this is set by ServiceRequest and we treat
 *    channel created for ServiceRequest as critical channels.
 * 2. nsIChannel::LOAD_BYPASS_URL_CLASSIFIER, channel's opener can use this
 *    flag to enforce bypassing the URL classifier check.
 */
bool NS_ShouldClassifyChannel(nsIChannel* aChannel) {
  nsLoadFlags loadFlags;
  Unused << aChannel->GetLoadFlags(&loadFlags);
  //  If our load flags dictate that we must let this channel through without
  //  URL classification, obey that here without performing more checks.
  if (loadFlags & nsIChannel::LOAD_BYPASS_URL_CLASSIFIER) {
    return false;
  }

  nsCOMPtr<nsIHttpChannelInternal> httpChannel(do_QueryInterface(aChannel));
  if (httpChannel) {
    bool beConservative;
    nsresult rv = httpChannel->GetBeConservative(&beConservative);

    // beConservative flag, set by ServiceRequest to ensure channels that
    // fetch update use conservative TLS setting, are used here to identify
    // channels are critical to bypass classification. for channels don't
    // support beConservative, continue to apply the exemption rules.
    if (NS_SUCCEEDED(rv) && beConservative) {
      return false;
    }
  }

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  if (loadInfo) {
    nsContentPolicyType type = loadInfo->GetExternalContentPolicyType();

    // Skip classifying channel triggered by system unless it is a top-level
    // load.
    if (nsContentUtils::IsSystemPrincipal(loadInfo->TriggeringPrincipal()) &&
        nsIContentPolicy::TYPE_DOCUMENT != type) {
      return false;
    }
  }

  return true;
}

namespace mozilla {
namespace net {

bool InScriptableRange(int64_t val) {
  return (val <= kJS_MAX_SAFE_INTEGER) && (val >= kJS_MIN_SAFE_INTEGER);
}

bool InScriptableRange(uint64_t val) { return val <= kJS_MAX_SAFE_UINTEGER; }

nsresult GetParameterHTTP(const nsACString& aHeaderVal, const char* aParamName,
                          nsAString& aResult) {
  return nsMIMEHeaderParamImpl::GetParameterHTTP(aHeaderVal, aParamName,
                                                 aResult);
}

bool SchemeIsHTTP(nsIURI* aURI) {
  MOZ_ASSERT(aURI);
  return aURI->SchemeIs("http");
}

bool SchemeIsHTTPS(nsIURI* aURI) {
  MOZ_ASSERT(aURI);
  return aURI->SchemeIs("https");
}

bool SchemeIsJavascript(nsIURI* aURI) {
  MOZ_ASSERT(aURI);
  return aURI->SchemeIs("javascript");
}

bool SchemeIsChrome(nsIURI* aURI) {
  MOZ_ASSERT(aURI);
  return aURI->SchemeIs("chrome");
}

bool SchemeIsAbout(nsIURI* aURI) {
  MOZ_ASSERT(aURI);
  return aURI->SchemeIs("about");
}

bool SchemeIsBlob(nsIURI* aURI) {
  MOZ_ASSERT(aURI);
  return aURI->SchemeIs("blob");
}

bool SchemeIsFile(nsIURI* aURI) {
  MOZ_ASSERT(aURI);
  return aURI->SchemeIs("file");
}

bool SchemeIsData(nsIURI* aURI) {
  MOZ_ASSERT(aURI);
  return aURI->SchemeIs("data");
}

bool SchemeIsViewSource(nsIURI* aURI) {
  MOZ_ASSERT(aURI);
  return aURI->SchemeIs("view-source");
}

bool SchemeIsResource(nsIURI* aURI) {
  MOZ_ASSERT(aURI);
  return aURI->SchemeIs("resource");
}

bool SchemeIsFTP(nsIURI* aURI) {
  MOZ_ASSERT(aURI);
  return aURI->SchemeIs("ftp");
}

}  // namespace net
}  // namespace mozilla
