/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PageIconProtocolHandler.h"

#include "mozilla/ScopeExit.h"
#include "mozilla/Services.h"
#include "nsFaviconService.h"
#include "nsIChannel.h"
#include "nsIFaviconService.h"
#include "nsIIOService.h"
#include "nsILoadInfo.h"
#include "nsIOutputStream.h"
#include "nsIPipe.h"
#include "nsIRequestObserver.h"
#include "nsIURIMutator.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace places {

StaticRefPtr<PageIconProtocolHandler> PageIconProtocolHandler::sSingleton;

namespace {

class DefaultFaviconObserver final : public nsIRequestObserver {
 public:
  explicit DefaultFaviconObserver(nsIOutputStream* aOutputStream)
      : mOutputStream(aOutputStream) {
    MOZ_ASSERT(aOutputStream);
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUESTOBSERVER
 private:
  ~DefaultFaviconObserver() = default;

  nsCOMPtr<nsIOutputStream> mOutputStream;
};

NS_IMPL_ISUPPORTS(DefaultFaviconObserver, nsIRequestObserver);

NS_IMETHODIMP DefaultFaviconObserver::OnStartRequest(nsIRequest*) {
  return NS_OK;
}

NS_IMETHODIMP DefaultFaviconObserver::OnStopRequest(nsIRequest*, nsresult) {
  // We must close the outputStream regardless.
  mOutputStream->Close();
  return NS_OK;
}

}  // namespace

static nsresult MakeDefaultFaviconChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                                          nsIChannel** aOutChannel) {
  nsCOMPtr<nsIIOService> ios = mozilla::services::GetIOService();
  nsCOMPtr<nsIChannel> chan;
  nsCOMPtr<nsIURI> defaultFaviconURI;

  auto* faviconService = nsFaviconService::GetFaviconService();
  if (MOZ_UNLIKELY(!faviconService)) {
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv =
      faviconService->GetDefaultFavicon(getter_AddRefs(defaultFaviconURI));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = ios->NewChannelFromURIWithLoadInfo(defaultFaviconURI, aLoadInfo,
                                          getter_AddRefs(chan));
  NS_ENSURE_SUCCESS(rv, rv);
  chan->SetOriginalURI(aURI);
  chan->SetContentType(NS_LITERAL_CSTRING(FAVICON_DEFAULT_MIMETYPE));
  chan.forget(aOutChannel);
  return NS_OK;
}

static nsresult StreamDefaultFavicon(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                                     nsIOutputStream* aOutputStream,
                                     nsIChannel* aOriginalChannel) {
  auto closeStreamOnError =
      mozilla::MakeScopeExit([&] { aOutputStream->Close(); });

  auto observer = MakeRefPtr<DefaultFaviconObserver>(aOutputStream);

  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv = NS_NewSimpleStreamListener(getter_AddRefs(listener),
                                           aOutputStream, observer);
  NS_ENSURE_SUCCESS(rv, rv);

  aOriginalChannel->SetContentType(
      NS_LITERAL_CSTRING(FAVICON_DEFAULT_MIMETYPE));
  nsCOMPtr<nsIChannel> defaultIconChannel;
  rv = MakeDefaultFaviconChannel(aURI, aLoadInfo,
                                 getter_AddRefs(defaultIconChannel));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = defaultIconChannel->AsyncOpen(listener);
  NS_ENSURE_SUCCESS(rv, rv);

  closeStreamOnError.release();
  return NS_OK;
}

namespace {

class FaviconDataCallback final : public nsIFaviconDataCallback {
 public:
  FaviconDataCallback(nsIURI* aURI, nsIOutputStream* aOutputStream,
                      nsIChannel* aChannel, nsILoadInfo* aLoadInfo)
      : mURI(aURI),
        mOutputStream(aOutputStream),
        mChannel(aChannel),
        mLoadInfo(aLoadInfo) {
    MOZ_ASSERT(aOutputStream);
    MOZ_ASSERT(aLoadInfo);
    MOZ_ASSERT(aChannel);
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIFAVICONDATACALLBACK
 private:
  ~FaviconDataCallback() = default;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIOutputStream> mOutputStream;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsILoadInfo> mLoadInfo;
};

NS_IMPL_ISUPPORTS(FaviconDataCallback, nsIFaviconDataCallback);

NS_IMETHODIMP FaviconDataCallback::OnComplete(nsIURI* aURI, uint32_t aDataLen,
                                              const uint8_t* aData,
                                              const nsACString& aMimeType,
                                              uint16_t aWidth) {
  if (!aDataLen) {
    return StreamDefaultFavicon(mURI, mLoadInfo, mOutputStream, mChannel);
  }
  mChannel->SetContentLength(aDataLen);
  mChannel->SetContentType(aMimeType);
  uint32_t wroteLength = 0;
  mOutputStream->Write(reinterpret_cast<const char*>(aData), aDataLen,
                       &wroteLength);
  return mOutputStream->Close();
}

}  // namespace

NS_IMPL_ISUPPORTS(PageIconProtocolHandler, nsIProtocolHandler,
                  nsISupportsWeakReference);

NS_IMETHODIMP PageIconProtocolHandler::GetScheme(nsACString& aScheme) {
  aScheme.Assign(NS_LITERAL_CSTRING("page-icon"));
  return NS_OK;
}

NS_IMETHODIMP PageIconProtocolHandler::GetDefaultPort(int32_t* aPort) {
  *aPort = -1;
  return NS_OK;
}

NS_IMETHODIMP PageIconProtocolHandler::GetProtocolFlags(uint32_t* aFlags) {
  *aFlags = URI_NORELATIVE | URI_NOAUTH | URI_DANGEROUS_TO_LOAD |
            URI_IS_LOCAL_RESOURCE;
  return NS_OK;
}

NS_IMETHODIMP PageIconProtocolHandler::AllowPort(int32_t, const char*,
                                                 bool* aAllow) {
  *aAllow = false;
  return NS_OK;
}

NS_IMETHODIMP PageIconProtocolHandler::NewChannel(nsIURI* aURI,
                                                  nsILoadInfo* aLoadInfo,
                                                  nsIChannel** aOutChannel) {
  nsresult rv = NewChannelInternal(aURI, aLoadInfo, aOutChannel);
  if (NS_SUCCEEDED(rv)) {
    return rv;
  }
  return MakeDefaultFaviconChannel(aURI, aLoadInfo, aOutChannel);
}

nsresult PageIconProtocolHandler::NewChannelInternal(nsIURI* aURI,
                                                     nsILoadInfo* aLoadInfo,
                                                     nsIChannel** aOutChannel) {
  // Create a pipe that will give us an output stream that we can use once
  // we got all the favicon data.
  nsCOMPtr<nsIInputStream> pipeIn;
  nsCOMPtr<nsIOutputStream> pipeOut;
  nsresult rv =
      NS_NewPipe(getter_AddRefs(pipeIn), getter_AddRefs(pipeOut), 0,
                 nsIFaviconService::MAX_FAVICON_BUFFER_SIZE, true, true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create our channel.
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewInputStreamChannel(
      getter_AddRefs(channel), aURI, pipeIn.forget(),
      aLoadInfo->GetLoadingPrincipal(),
      nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED,
      nsIContentPolicy::TYPE_INTERNAL_IMAGE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = channel->SetLoadInfo(aLoadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  auto faviconCallback =
      MakeRefPtr<FaviconDataCallback>(aURI, pipeOut, channel, aLoadInfo);
  auto* faviconService = nsFaviconService::GetFaviconService();
  if (MOZ_UNLIKELY(!faviconService)) {
    return NS_ERROR_UNEXPECTED;
  }

  uint16_t preferredSize = 0;
  faviconService->PreferredSizeFromURI(aURI, &preferredSize);

  nsCOMPtr<nsIURI> pageURI;
  {
    // NOTE: We don't need to strip #size= fragments because
    // GetFaviconDataForPage strips them when doing the database lookup.
    nsAutoCString pageQuery;
    aURI->GetPathQueryRef(pageQuery);
    rv = NS_NewURI(getter_AddRefs(pageURI), pageQuery);
    if (NS_FAILED(rv)) {
      return rv;
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = faviconService->GetFaviconDataForPage(pageURI, faviconCallback,
                                             preferredSize);
  NS_ENSURE_SUCCESS(rv, rv);

  channel.forget(aOutChannel);
  return NS_OK;
}

}  // namespace places
}  // namespace mozilla
