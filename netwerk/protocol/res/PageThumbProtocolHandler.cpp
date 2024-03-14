/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PageThumbProtocolHandler.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ipc/URIParams.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Try.h"

#include "LoadInfo.h"
#include "nsContentUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIFile.h"
#include "nsIFileChannel.h"
#include "nsIFileStreams.h"
#include "nsIMIMEService.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsIPageThumbsStorageService.h"
#include "nsIInputStreamPump.h"
#include "nsIStreamListener.h"
#include "nsIInputStream.h"
#include "nsNetUtil.h"
#include "nsURLHelper.h"
#include "prio.h"
#include "SimpleChannel.h"
#include "nsICancelable.h"

#ifdef MOZ_PLACES
#  include "nsIPlacesPreviewsHelperService.h"
#endif

#define PAGE_THUMB_HOST "thumbnails"
#define PLACES_PREVIEWS_HOST "places-previews"
#define PAGE_THUMB_SCHEME "moz-page-thumb"

namespace mozilla {
namespace net {

LazyLogModule gPageThumbProtocolLog("PageThumbProtocol");

#undef LOG
#define LOG(level, ...) \
  MOZ_LOG(gPageThumbProtocolLog, LogLevel::level, (__VA_ARGS__))

StaticRefPtr<PageThumbProtocolHandler> PageThumbProtocolHandler::sSingleton;

NS_IMPL_QUERY_INTERFACE(PageThumbProtocolHandler,
                        nsISubstitutingProtocolHandler, nsIProtocolHandler,
                        nsISupportsWeakReference)
NS_IMPL_ADDREF_INHERITED(PageThumbProtocolHandler, SubstitutingProtocolHandler)
NS_IMPL_RELEASE_INHERITED(PageThumbProtocolHandler, SubstitutingProtocolHandler)

already_AddRefed<PageThumbProtocolHandler>
PageThumbProtocolHandler::GetSingleton() {
  if (!sSingleton) {
    sSingleton = new PageThumbProtocolHandler();
    ClearOnShutdown(&sSingleton);
  }

  return do_AddRef(sSingleton);
}

// A moz-page-thumb URI is only loadable by chrome pages in the parent process,
// or privileged content running in the privileged about content process.
PageThumbProtocolHandler::PageThumbProtocolHandler()
    : SubstitutingProtocolHandler(PAGE_THUMB_SCHEME) {}

RefPtr<RemoteStreamPromise> PageThumbProtocolHandler::NewStream(
    nsIURI* aChildURI, bool* aTerminateSender) {
  MOZ_ASSERT(!IsNeckoChild());
  MOZ_ASSERT(NS_IsMainThread());

  if (!aChildURI || !aTerminateSender) {
    return RemoteStreamPromise::CreateAndReject(NS_ERROR_INVALID_ARG, __func__);
  }

  *aTerminateSender = true;
  nsresult rv;

  // We should never receive a URI that isn't for a moz-page-thumb because
  // these requests ordinarily come from the child's PageThumbProtocolHandler.
  // Ensure this request is for a moz-page-thumb URI. A compromised child
  // process could send us any URI.
  bool isPageThumbScheme = false;
  if (NS_FAILED(aChildURI->SchemeIs(PAGE_THUMB_SCHEME, &isPageThumbScheme)) ||
      !isPageThumbScheme) {
    return RemoteStreamPromise::CreateAndReject(NS_ERROR_UNKNOWN_PROTOCOL,
                                                __func__);
  }

  // We should never receive a URI that does not have "thumbnails" as the host.
  nsAutoCString host;
  if (NS_FAILED(aChildURI->GetAsciiHost(host)) ||
      !(host.EqualsLiteral(PAGE_THUMB_HOST) ||
        host.EqualsLiteral(PLACES_PREVIEWS_HOST))) {
    return RemoteStreamPromise::CreateAndReject(NS_ERROR_UNEXPECTED, __func__);
  }

  // For errors after this point, we want to propagate the error to
  // the child, but we don't force the child process to be terminated.
  *aTerminateSender = false;

  // Make sure the child URI resolves to a file URI. We will then get a file
  // channel for the request. The resultant channel should be a file channel
  // because we only request remote streams for resource loads where the URI
  // resolves to a file.
  nsAutoCString resolvedSpec;
  rv = ResolveURI(aChildURI, resolvedSpec);
  if (NS_FAILED(rv)) {
    return RemoteStreamPromise::CreateAndReject(rv, __func__);
  }

  nsAutoCString resolvedScheme;
  rv = net_ExtractURLScheme(resolvedSpec, resolvedScheme);
  if (NS_FAILED(rv) || !resolvedScheme.EqualsLiteral("file")) {
    return RemoteStreamPromise::CreateAndReject(NS_ERROR_UNEXPECTED, __func__);
  }

  nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
  if (NS_FAILED(rv)) {
    return RemoteStreamPromise::CreateAndReject(rv, __func__);
  }

  nsCOMPtr<nsIURI> resolvedURI;
  rv = ioService->NewURI(resolvedSpec, nullptr, nullptr,
                         getter_AddRefs(resolvedURI));
  if (NS_FAILED(rv)) {
    return RemoteStreamPromise::CreateAndReject(rv, __func__);
  }

  // We use the system principal to get a file channel for the request,
  // but only after we've checked (above) that the child URI is of
  // moz-page-thumb scheme and that the URI host matches PAGE_THUMB_HOST.
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), resolvedURI,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER);
  if (NS_FAILED(rv)) {
    return RemoteStreamPromise::CreateAndReject(rv, __func__);
  }

  auto promiseHolder = MakeUnique<MozPromiseHolder<RemoteStreamPromise>>();
  RefPtr<RemoteStreamPromise> promise = promiseHolder->Ensure(__func__);

  nsCOMPtr<nsIMIMEService> mime = do_GetService("@mozilla.org/mime;1", &rv);
  if (NS_FAILED(rv)) {
    return RemoteStreamPromise::CreateAndReject(rv, __func__);
  }

  nsAutoCString contentType;
  rv = mime->GetTypeFromURI(aChildURI, contentType);
  if (NS_FAILED(rv)) {
    return RemoteStreamPromise::CreateAndReject(rv, __func__);
  }

  rv = NS_DispatchBackgroundTask(
      NS_NewRunnableFunction(
          "PageThumbProtocolHandler::NewStream",
          [contentType, channel, holder = std::move(promiseHolder)]() {
            nsresult rv;

            nsCOMPtr<nsIFileChannel> fileChannel =
                do_QueryInterface(channel, &rv);
            if (NS_FAILED(rv)) {
              holder->Reject(rv, __func__);
              return;
            }

            nsCOMPtr<nsIFile> requestedFile;
            rv = fileChannel->GetFile(getter_AddRefs(requestedFile));
            if (NS_FAILED(rv)) {
              holder->Reject(rv, __func__);
              return;
            }

            nsCOMPtr<nsIInputStream> inputStream;
            rv = NS_NewLocalFileInputStream(getter_AddRefs(inputStream),
                                            requestedFile, PR_RDONLY, -1);
            if (NS_FAILED(rv)) {
              holder->Reject(rv, __func__);
              return;
            }

            RemoteStreamInfo info(inputStream, contentType, -1);

            holder->Resolve(std::move(info), __func__);
          }),
      NS_DISPATCH_EVENT_MAY_BLOCK);

  if (NS_FAILED(rv)) {
    return RemoteStreamPromise::CreateAndReject(rv, __func__);
  }

  return promise;
}

bool PageThumbProtocolHandler::ResolveSpecialCases(const nsACString& aHost,
                                                   const nsACString& aPath,
                                                   const nsACString& aPathname,
                                                   nsACString& aResult) {
  // This should match the scheme in PageThumbs.jsm. We will only resolve
  // URIs for thumbnails generated by PageThumbs here.
  if (!aHost.EqualsLiteral(PAGE_THUMB_HOST) &&
      !aHost.EqualsLiteral(PLACES_PREVIEWS_HOST)) {
    // moz-page-thumb should always have a "thumbnails" host. We do not intend
    // to allow substitution rules to be created for moz-page-thumb.
    return false;
  }

  // Regardless of the outcome, the scheme will be resolved to file://.
  aResult.Assign("file://");

  if (IsNeckoChild()) {
    // We will resolve the URI in the parent if load is performed in the child
    // because the child does not have access to the profile directory path.
    // Technically we could retrieve the path from dom::ContentChild, but I
    // would prefer to obtain the path from PageThumbsStorageService (which
    // depends on OS.Path). Here, we resolve to the same URI, with the file://
    // scheme. This won't ever be accessed directly by the content process,
    // and is mainly used to keep the substitution protocol handler mechanism
    // happy.
    aResult.Append(aHost);
    aResult.Append(aPath);
  } else {
    // Resolve the URI in the parent to the thumbnail file URI since we will
    // attempt to open the channel to load the file after this.
    nsAutoString thumbnailUrl;
    nsresult rv = GetThumbnailPath(aPath, aHost, thumbnailUrl);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    aResult.Append(NS_ConvertUTF16toUTF8(thumbnailUrl));
  }

  return true;
}

nsresult PageThumbProtocolHandler::SubstituteChannel(nsIURI* aURI,
                                                     nsILoadInfo* aLoadInfo,
                                                     nsIChannel** aRetVal) {
  // Check if URI resolves to a file URI.
  nsAutoCString resolvedSpec;
  MOZ_TRY(ResolveURI(aURI, resolvedSpec));

  nsAutoCString scheme;
  MOZ_TRY(net_ExtractURLScheme(resolvedSpec, scheme));

  if (!scheme.EqualsLiteral("file")) {
    NS_WARNING("moz-page-thumb URIs should only resolve to file URIs.");
    return NS_ERROR_NO_INTERFACE;
  }

  // Load the URI remotely if accessed from a child.
  if (IsNeckoChild()) {
    MOZ_TRY(SubstituteRemoteChannel(aURI, aLoadInfo, aRetVal));
  }

  return NS_OK;
}

Result<Ok, nsresult> PageThumbProtocolHandler::SubstituteRemoteChannel(
    nsIURI* aURI, nsILoadInfo* aLoadInfo, nsIChannel** aRetVal) {
  MOZ_ASSERT(IsNeckoChild());
  MOZ_TRY(aURI ? NS_OK : NS_ERROR_INVALID_ARG);
  MOZ_TRY(aLoadInfo ? NS_OK : NS_ERROR_INVALID_ARG);

#ifdef DEBUG
  nsAutoCString resolvedSpec;
  MOZ_TRY(ResolveURI(aURI, resolvedSpec));

  nsAutoCString scheme;
  MOZ_TRY(net_ExtractURLScheme(resolvedSpec, scheme));

  MOZ_ASSERT(scheme.EqualsLiteral("file"));
#endif /* DEBUG */

  RefPtr<RemoteStreamGetter> streamGetter =
      new RemoteStreamGetter(aURI, aLoadInfo);

  NewSimpleChannel(aURI, aLoadInfo, streamGetter, aRetVal);
  return Ok();
}

nsresult PageThumbProtocolHandler::GetThumbnailPath(const nsACString& aPath,
                                                    const nsACString& aHost,
                                                    nsString& aThumbnailPath) {
  MOZ_ASSERT(!IsNeckoChild());

  // Ensures that the provided path has a query string. We will start parsing
  // from there.
  int32_t queryIndex = aPath.FindChar('?');
  if (queryIndex <= 0) {
    return NS_ERROR_MALFORMED_URI;
  }

  // Extract URL from query string.
  nsAutoString url;
  bool found =
      URLParams::Extract(Substring(aPath, queryIndex + 1), u"url"_ns, url);
  if (!found || url.IsVoid()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsresult rv;
  if (aHost.EqualsLiteral(PAGE_THUMB_HOST)) {
    nsCOMPtr<nsIPageThumbsStorageService> pageThumbsStorage =
        do_GetService("@mozilla.org/thumbnails/pagethumbs-service;1", &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    // Use PageThumbsStorageService to get the local file path of the screenshot
    // for the given URL.
    rv = pageThumbsStorage->GetFilePathForURL(url, aThumbnailPath);
#ifdef MOZ_PLACES
  } else if (aHost.EqualsLiteral(PLACES_PREVIEWS_HOST)) {
    nsCOMPtr<nsIPlacesPreviewsHelperService> helper =
        do_GetService("@mozilla.org/places/previews-helper;1", &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    rv = helper->GetFilePathForURL(url, aThumbnailPath);
#endif
  } else {
    MOZ_ASSERT_UNREACHABLE("Unknown thumbnail host");
    return NS_ERROR_UNEXPECTED;
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

// static
void PageThumbProtocolHandler::NewSimpleChannel(
    nsIURI* aURI, nsILoadInfo* aLoadinfo, RemoteStreamGetter* aStreamGetter,
    nsIChannel** aRetVal) {
  nsCOMPtr<nsIChannel> channel = NS_NewSimpleChannel(
      aURI, aLoadinfo, aStreamGetter,
      [](nsIStreamListener* listener, nsIChannel* simpleChannel,
         RemoteStreamGetter* getter) -> RequestOrReason {
        return getter->GetAsync(listener, simpleChannel,
                                &NeckoChild::SendGetPageThumbStream);
      });

  channel.swap(*aRetVal);
}

#undef LOG

}  // namespace net
}  // namespace mozilla
