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

#define PAGE_THUMB_HOST "thumbnails"
#define PAGE_THUMB_SCHEME "moz-page-thumb"

namespace mozilla {
namespace net {

LazyLogModule gPageThumbProtocolLog("PageThumbProtocol");

#undef LOG
#define LOG(level, ...) \
  MOZ_LOG(gPageThumbProtocolLog, LogLevel::level, (__VA_ARGS__))

StaticRefPtr<PageThumbProtocolHandler> PageThumbProtocolHandler::sSingleton;

/**
 * Helper class used with SimpleChannel to asynchronously obtain an input
 * stream from the parent for a remote moz-page-thumb load from the child.
 */
class PageThumbStreamGetter final : public nsICancelable {
  NS_DECL_ISUPPORTS
  NS_DECL_NSICANCELABLE

 public:
  PageThumbStreamGetter(nsIURI* aURI, nsILoadInfo* aLoadInfo)
      : mURI(aURI), mLoadInfo(aLoadInfo) {
    MOZ_ASSERT(aURI);
    MOZ_ASSERT(aLoadInfo);

    SetupEventTarget();
  }

  void SetupEventTarget() {
    mMainThreadEventTarget = nsContentUtils::GetEventTargetByLoadInfo(
        mLoadInfo, TaskCategory::Other);
    if (!mMainThreadEventTarget) {
      mMainThreadEventTarget = GetMainThreadSerialEventTarget();
    }
  }

  // Get an input stream from the parent asynchronously.
  RequestOrReason GetAsync(nsIStreamListener* aListener, nsIChannel* aChannel);

  // Handle an input stream being returned from the parent
  void OnStream(already_AddRefed<nsIInputStream> aStream);

  static void CancelRequest(nsIStreamListener* aListener, nsIChannel* aChannel,
                            nsresult aResult);

 private:
  ~PageThumbStreamGetter() = default;

  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsILoadInfo> mLoadInfo;
  nsCOMPtr<nsIStreamListener> mListener;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsISerialEventTarget> mMainThreadEventTarget;
  nsCOMPtr<nsIInputStreamPump> mPump;
  bool mCanceled{false};
  nsresult mStatus{NS_OK};
};

NS_IMPL_ISUPPORTS(PageThumbStreamGetter, nsICancelable)

// Request an input stream from the parent.
RequestOrReason PageThumbStreamGetter::GetAsync(nsIStreamListener* aListener,
                                                nsIChannel* aChannel) {
  MOZ_ASSERT(IsNeckoChild());
  MOZ_ASSERT(mMainThreadEventTarget);

  mListener = aListener;
  mChannel = aChannel;

  nsCOMPtr<nsICancelable> cancelableRequest(this);

  RefPtr<PageThumbStreamGetter> self = this;

  // Request an input stream for this moz-page-thumb URI.
  gNeckoChild->SendGetPageThumbStream(mURI)->Then(
      mMainThreadEventTarget, __func__,
      [self](const RefPtr<nsIInputStream>& stream) {
        self->OnStream(do_AddRef(stream));
      },
      [self](const mozilla::ipc::ResponseRejectReason) {
        self->OnStream(nullptr);
      });
  return RequestOrCancelable(WrapNotNull(cancelableRequest));
}

// Called to cancel the ongoing async request.
NS_IMETHODIMP
PageThumbStreamGetter::Cancel(nsresult aStatus) {
  if (mCanceled) {
    return NS_OK;
  }

  mCanceled = true;
  mStatus = aStatus;

  if (mPump) {
    mPump->Cancel(aStatus);
    mPump = nullptr;
  }

  return NS_OK;
}

// static
void PageThumbStreamGetter::CancelRequest(nsIStreamListener* aListener,
                                          nsIChannel* aChannel,
                                          nsresult aResult) {
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(aChannel);

  aListener->OnStartRequest(aChannel);
  aListener->OnStopRequest(aChannel, aResult);
  aChannel->Cancel(NS_BINDING_ABORTED);
}

// Handle an input stream sent from the parent.
void PageThumbStreamGetter::OnStream(already_AddRefed<nsIInputStream> aStream) {
  MOZ_ASSERT(IsNeckoChild());
  MOZ_ASSERT(mChannel);
  MOZ_ASSERT(mListener);
  MOZ_ASSERT(mMainThreadEventTarget);

  nsCOMPtr<nsIInputStream> stream = std::move(aStream);
  nsCOMPtr<nsIChannel> channel = std::move(mChannel);

  // We must keep an owning reference to the listener until we pass it on
  // to AsyncRead.
  nsCOMPtr<nsIStreamListener> listener = mListener.forget();

  if (mCanceled) {
    // The channel that has created this stream getter has been canceled.
    CancelRequest(listener, channel, mStatus);
    return;
  }

  if (!stream) {
    // The parent didn't send us back a stream.
    CancelRequest(listener, channel, NS_ERROR_FILE_ACCESS_DENIED);
    return;
  }

  nsCOMPtr<nsIInputStreamPump> pump;
  nsresult rv = NS_NewInputStreamPump(getter_AddRefs(pump), stream.forget(), 0,
                                      0, false, mMainThreadEventTarget);
  if (NS_FAILED(rv)) {
    CancelRequest(listener, channel, rv);
    return;
  }

  rv = pump->AsyncRead(listener);
  if (NS_FAILED(rv)) {
    CancelRequest(listener, channel, rv);
    return;
  }

  mPump = pump;
}

NS_IMPL_QUERY_INTERFACE(PageThumbProtocolHandler,
                        nsISubstitutingProtocolHandler, nsIProtocolHandler,
                        nsIProtocolHandlerWithDynamicFlags,
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

PageThumbProtocolHandler::PageThumbProtocolHandler()
    : SubstitutingProtocolHandler(PAGE_THUMB_SCHEME) {}

nsresult PageThumbProtocolHandler::GetFlagsForURI(nsIURI* aURI,
                                                  uint32_t* aFlags) {
  // A moz-page-thumb URI is only loadable by chrome pages in the parent
  // process, or privileged content running in the privileged about content
  // process.
  *aFlags = URI_STD | URI_IS_UI_RESOURCE | URI_IS_LOCAL_RESOURCE |
            URI_NORELATIVE | URI_NOAUTH;

  return NS_OK;
}

RefPtr<PageThumbStreamPromise> PageThumbProtocolHandler::NewStream(
    nsIURI* aChildURI, bool* aTerminateSender) {
  MOZ_ASSERT(!IsNeckoChild());
  MOZ_ASSERT(NS_IsMainThread());

  if (!aChildURI || !aTerminateSender) {
    return PageThumbStreamPromise::CreateAndReject(NS_ERROR_INVALID_ARG,
                                                   __func__);
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
    return PageThumbStreamPromise::CreateAndReject(NS_ERROR_UNKNOWN_PROTOCOL,
                                                   __func__);
  }

  // We should never receive a URI that does not have "thumbnails" as the host.
  nsAutoCString host;
  if (NS_FAILED(aChildURI->GetAsciiHost(host)) ||
      !host.EqualsLiteral(PAGE_THUMB_HOST)) {
    return PageThumbStreamPromise::CreateAndReject(NS_ERROR_UNEXPECTED,
                                                   __func__);
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
    return PageThumbStreamPromise::CreateAndReject(rv, __func__);
  }

  nsAutoCString resolvedScheme;
  rv = net_ExtractURLScheme(resolvedSpec, resolvedScheme);
  if (NS_FAILED(rv) || !resolvedScheme.EqualsLiteral("file")) {
    return PageThumbStreamPromise::CreateAndReject(NS_ERROR_UNEXPECTED,
                                                   __func__);
  }

  nsCOMPtr<nsIIOService> ioService = do_GetIOService(&rv);
  if (NS_FAILED(rv)) {
    return PageThumbStreamPromise::CreateAndReject(rv, __func__);
  }

  nsCOMPtr<nsIURI> resolvedURI;
  rv = ioService->NewURI(resolvedSpec, nullptr, nullptr,
                         getter_AddRefs(resolvedURI));
  if (NS_FAILED(rv)) {
    return PageThumbStreamPromise::CreateAndReject(rv, __func__);
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
    return PageThumbStreamPromise::CreateAndReject(rv, __func__);
  }

  auto promiseHolder = MakeUnique<MozPromiseHolder<PageThumbStreamPromise>>();
  RefPtr<PageThumbStreamPromise> promise = promiseHolder->Ensure(__func__);

  rv = NS_DispatchBackgroundTask(
      NS_NewRunnableFunction(
          "PageThumbProtocolHandler::NewStream",
          [channel, holder = std::move(promiseHolder)]() {
            nsresult rv;

            nsCOMPtr<nsIFileChannel> fileChannel =
                do_QueryInterface(channel, &rv);
            if (NS_FAILED(rv)) {
              holder->Reject(rv, __func__);
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

            holder->Resolve(inputStream, __func__);
          }),
      NS_DISPATCH_EVENT_MAY_BLOCK);

  if (NS_FAILED(rv)) {
    return PageThumbStreamPromise::CreateAndReject(rv, __func__);
  }

  return promise;
}

bool PageThumbProtocolHandler::ResolveSpecialCases(const nsACString& aHost,
                                                   const nsACString& aPath,
                                                   const nsACString& aPathname,
                                                   nsACString& aResult) {
  // This should match the scheme in PageThumbs.jsm. We will only resolve
  // URIs for thumbnails generated by PageThumbs here.
  if (!aHost.EqualsLiteral(PAGE_THUMB_HOST)) {
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
    nsresult rv = GetThumbnailPath(aPath, thumbnailUrl);
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

  RefPtr<PageThumbStreamGetter> streamGetter =
      new PageThumbStreamGetter(aURI, aLoadInfo);

  NewSimpleChannel(aURI, aLoadInfo, streamGetter, aRetVal);
  return Ok();
}

nsresult PageThumbProtocolHandler::GetThumbnailPath(const nsACString& aPath,
                                                    nsString& aThumbnailPath) {
  MOZ_ASSERT(!IsNeckoChild());

  // Ensures that the provided path has a query string. We will start parsing
  // from there.
  int32_t queryIndex = aPath.FindChar('?');
  if (queryIndex <= 0) {
    return NS_ERROR_MALFORMED_URI;
  }

  nsresult rv;

  nsCOMPtr<nsIPageThumbsStorageService> pageThumbsStorage =
      do_GetService("@mozilla.org/thumbnails/pagethumbs-service;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Extract URL from query string.
  nsAutoString url;
  bool found =
      URLParams::Extract(Substring(aPath, queryIndex + 1), u"url"_ns, url);
  if (!found || url.IsVoid()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Use PageThumbsStorageService to get the local file path of the screenshot
  // for the given URL.
  rv = pageThumbsStorage->GetFilePathForURL(url, aThumbnailPath);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

// static
void PageThumbProtocolHandler::SetContentType(nsIURI* aURI,
                                              nsIChannel* aChannel) {
  nsresult rv;
  nsCOMPtr<nsIMIMEService> mime = do_GetService("@mozilla.org/mime;1", &rv);
  if (NS_SUCCEEDED(rv)) {
    nsAutoCString contentType;
    rv = mime->GetTypeFromURI(aURI, contentType);
    if (NS_SUCCEEDED(rv)) {
      Unused << aChannel->SetContentType(contentType);
    }
  }
}

// static
void PageThumbProtocolHandler::NewSimpleChannel(
    nsIURI* aURI, nsILoadInfo* aLoadinfo, PageThumbStreamGetter* aStreamGetter,
    nsIChannel** aRetVal) {
  nsCOMPtr<nsIChannel> channel = NS_NewSimpleChannel(
      aURI, aLoadinfo, aStreamGetter,
      [](nsIStreamListener* listener, nsIChannel* simpleChannel,
         PageThumbStreamGetter* getter) -> RequestOrReason {
        return getter->GetAsync(listener, simpleChannel);
      });

  SetContentType(aURI, channel);
  channel.swap(*aRetVal);
}

#undef LOG

}  // namespace net
}  // namespace mozilla
