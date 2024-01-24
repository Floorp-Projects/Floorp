//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Implementation of cached-favicon: URLs for accessing favicons. The urls are
 * sent to the favicon service. If the favicon service doesn't have the data,
 * a stream containing the default favicon will be returned.
 */

#include "nsCachedFaviconProtocolHandler.h"
#include "nsFaviconService.h"
#include "nsICancelable.h"
#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsISupportsUtils.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsInputStreamPump.h"
#include "nsContentUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsStringStream.h"
#include "SimpleChannel.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/storage.h"
#include "mozIStorageResultSet.h"
#include "mozIStorageRow.h"
#include "Helpers.h"
#include "FaviconHelpers.h"

using namespace mozilla;
using namespace mozilla::places;

////////////////////////////////////////////////////////////////////////////////
//// Global Functions

/**
 * Creates a channel to obtain the default favicon.
 */
static nsresult GetDefaultIcon(nsIChannel* aOriginalChannel,
                               nsIChannel** aChannel) {
  nsCOMPtr<nsIURI> defaultIconURI;
  nsresult rv = NS_NewURI(getter_AddRefs(defaultIconURI),
                          nsLiteralCString(FAVICON_DEFAULT_URL));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsILoadInfo> loadInfo = aOriginalChannel->LoadInfo();
  rv = NS_NewChannelInternal(aChannel, defaultIconURI, loadInfo);
  NS_ENSURE_SUCCESS(rv, rv);
  Unused << (*aChannel)->SetContentType(
      nsLiteralCString(FAVICON_DEFAULT_MIMETYPE));
  Unused << aOriginalChannel->SetContentType(
      nsLiteralCString(FAVICON_DEFAULT_MIMETYPE));
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// faviconAsyncLoader

namespace {

/**
 * An instance of this class is passed to the favicon service as the callback
 * for getting favicon data from the database.  We'll get this data back in
 * HandleResult, and on HandleCompletion, we'll close our output stream which
 * will close the original channel for the favicon request.
 *
 * However, if an error occurs at any point and we don't have mData, we will
 * just fallback to the default favicon.  If anything happens at that point, the
 * world must be against us, so we can do nothing.
 */
class faviconAsyncLoader : public AsyncStatementCallback, public nsICancelable {
  NS_DECL_NSICANCELABLE
  NS_DECL_ISUPPORTS_INHERITED

 public:
  faviconAsyncLoader(nsIChannel* aChannel, nsIStreamListener* aListener,
                     uint16_t aPreferredSize)
      : mChannel(aChannel),
        mListener(aListener),
        mPreferredSize(aPreferredSize) {
    MOZ_ASSERT(aChannel, "Not providing a channel will result in crashes!");
    MOZ_ASSERT(aListener,
               "Not providing a stream listener will result in crashes!");
    MOZ_ASSERT(aChannel, "Not providing a channel!");
  }

  //////////////////////////////////////////////////////////////////////////////
  //// mozIStorageStatementCallback

  NS_IMETHOD HandleResult(mozIStorageResultSet* aResultSet) override {
    nsCOMPtr<mozIStorageRow> row;
    while (NS_SUCCEEDED(aResultSet->GetNextRow(getter_AddRefs(row))) && row) {
      int32_t width;
      nsresult rv = row->GetInt32(1, &width);
      NS_ENSURE_SUCCESS(rv, rv);

      // Check if we already found an image >= than the preferred size,
      // otherwise keep examining the next results.
      if (width < mPreferredSize && !mData.IsEmpty()) {
        return NS_OK;
      }

      // Eventually override the default mimeType for svg.
      if (width == UINT16_MAX) {
        rv = mChannel->SetContentType(nsLiteralCString(SVG_MIME_TYPE));
      } else {
        rv = mChannel->SetContentType(nsLiteralCString(PNG_MIME_TYPE));
      }
      NS_ENSURE_SUCCESS(rv, rv);

      // Obtain the binary blob that contains our favicon data.
      uint8_t* data;
      uint32_t dataLen;
      rv = row->GetBlob(0, &dataLen, &data);
      NS_ENSURE_SUCCESS(rv, rv);
      mData.Adopt(TO_CHARBUFFER(data), dataLen);
    }

    return NS_OK;
  }

  static void CancelRequest(nsIStreamListener* aListener, nsIChannel* aChannel,
                            nsresult aResult) {
    MOZ_ASSERT(aListener);
    MOZ_ASSERT(aChannel);

    aListener->OnStartRequest(aChannel);
    aListener->OnStopRequest(aChannel, aResult);
    aChannel->CancelWithReason(NS_BINDING_ABORTED,
                               "faviconAsyncLoader::CancelRequest"_ns);
  }

  NS_IMETHOD HandleCompletion(uint16_t aReason) override {
    MOZ_DIAGNOSTIC_ASSERT(mListener);
    MOZ_ASSERT(mChannel);
    NS_ENSURE_TRUE(mListener, NS_ERROR_UNEXPECTED);
    NS_ENSURE_TRUE(mChannel, NS_ERROR_UNEXPECTED);

    // Ensure we'll break possible cycles with the listener.
    auto cleanup = MakeScopeExit([&]() {
      mListener = nullptr;
      mChannel = nullptr;
    });

    if (mCanceled) {
      // The channel that has created this faviconAsyncLoader has been canceled.
      CancelRequest(mListener, mChannel, mStatus);
      return NS_OK;
    }

    nsresult rv;

    nsCOMPtr<nsILoadInfo> loadInfo = mChannel->LoadInfo();
    nsISerialEventTarget* target = GetMainThreadSerialEventTarget();
    if (!mData.IsEmpty()) {
      nsCOMPtr<nsIInputStream> stream;
      rv = NS_NewCStringInputStream(getter_AddRefs(stream), mData);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
      if (NS_SUCCEEDED(rv)) {
        RefPtr<nsInputStreamPump> pump;
        rv = nsInputStreamPump::Create(getter_AddRefs(pump), stream, 0, 0, true,
                                       target);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
        if (NS_SUCCEEDED(rv)) {
          rv = pump->AsyncRead(mListener);
          if (NS_FAILED(rv)) {
            CancelRequest(mListener, mChannel, rv);
            return rv;
          }

          mPump = pump;
          return NS_OK;
        }
      }
    }

    // Fallback to the default favicon.
    // we should pass the loadInfo of the original channel along
    // to the new channel. Note that mChannel can not be null,
    // constructor checks that.
    rv = GetDefaultIcon(mChannel, getter_AddRefs(mDefaultIconChannel));
    if (NS_FAILED(rv)) {
      CancelRequest(mListener, mChannel, rv);
      return rv;
    }

    rv = mDefaultIconChannel->AsyncOpen(mListener);
    if (NS_FAILED(rv)) {
      mDefaultIconChannel = nullptr;
      CancelRequest(mListener, mChannel, rv);
      return rv;
    }

    return NS_OK;
  }

 protected:
  virtual ~faviconAsyncLoader() = default;

 private:
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIChannel> mDefaultIconChannel;
  nsCOMPtr<nsIStreamListener> mListener;
  nsCOMPtr<nsIInputStreamPump> mPump;
  nsCString mData;
  uint16_t mPreferredSize;
  bool mCanceled{false};
  nsresult mStatus{NS_OK};
};

NS_IMPL_ISUPPORTS_INHERITED(faviconAsyncLoader, AsyncStatementCallback,
                            nsICancelable)

NS_IMETHODIMP
faviconAsyncLoader::Cancel(nsresult aStatus) {
  if (mCanceled) {
    return NS_OK;
  }

  mCanceled = true;
  mStatus = aStatus;

  if (mPump) {
    mPump->Cancel(aStatus);
    mPump = nullptr;
  }

  if (mDefaultIconChannel) {
    mDefaultIconChannel->Cancel(aStatus);
    mDefaultIconChannel = nullptr;
  }

  return NS_OK;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//// nsCachedFaviconProtocolHandler

NS_IMPL_ISUPPORTS(nsCachedFaviconProtocolHandler, nsIProtocolHandler)

// nsCachedFaviconProtocolHandler::GetScheme

NS_IMETHODIMP
nsCachedFaviconProtocolHandler::GetScheme(nsACString& aScheme) {
  aScheme.AssignLiteral("cached-favicon");
  return NS_OK;
}

// nsCachedFaviconProtocolHandler::NewChannel
//

NS_IMETHODIMP
nsCachedFaviconProtocolHandler::NewChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                                           nsIChannel** _retval) {
  NS_ENSURE_ARG_POINTER(aURI);

  nsCOMPtr<nsIURI> cachedFaviconURI;
  nsresult rv = ParseCachedFaviconURI(aURI, getter_AddRefs(cachedFaviconURI));
  NS_ENSURE_SUCCESS(rv, rv);

  return NewFaviconChannel(aURI, cachedFaviconURI, aLoadInfo, _retval);
}

// nsCachedFaviconProtocolHandler::AllowPort
//
//    Don't override any bans on bad ports.

NS_IMETHODIMP
nsCachedFaviconProtocolHandler::AllowPort(int32_t port, const char* scheme,
                                          bool* _retval) {
  *_retval = false;
  return NS_OK;
}

// nsCachedFaviconProtocolHandler::ParseCachedFaviconURI
//
//    Get actual URI from cached-favicon URL

nsresult nsCachedFaviconProtocolHandler::ParseCachedFaviconURI(
    nsIURI* aURI, nsIURI** aResultURI) {
  nsresult rv;
  nsAutoCString path;
  rv = aURI->GetPathQueryRef(path);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = NS_NewURI(aResultURI, path);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

nsresult nsCachedFaviconProtocolHandler::NewFaviconChannel(
    nsIURI* aURI, nsIURI* aCachedFaviconURI, nsILoadInfo* aLoadInfo,
    nsIChannel** _channel) {
  // Create our channel.  We'll call SetContentType with the right type when
  // we know what it actually is.
  nsCOMPtr<nsIChannel> channel = NS_NewSimpleChannel(
      aURI, aLoadInfo, aCachedFaviconURI,
      [](nsIStreamListener* listener, nsIChannel* channel,
         nsIURI* cachedFaviconURI) -> RequestOrReason {
        auto fallback = [&]() -> RequestOrReason {
          nsCOMPtr<nsIChannel> chan;
          nsresult rv = GetDefaultIcon(channel, getter_AddRefs(chan));
          NS_ENSURE_SUCCESS(rv, Err(rv));

          rv = chan->AsyncOpen(listener);
          NS_ENSURE_SUCCESS(rv, Err(rv));

          nsCOMPtr<nsIRequest> request(chan);
          return RequestOrCancelable(WrapNotNull(request));
        };

        // Now we go ahead and get our data asynchronously for the favicon.
        // Ignore the ref part of the URI before querying the database because
        // we may have added a size fragment for rendering purposes.
        nsFaviconService* faviconService =
            nsFaviconService::GetFaviconService();
        nsAutoCString faviconSpec;
        nsresult rv = cachedFaviconURI->GetSpecIgnoringRef(faviconSpec);
        // Any failures fallback to the default icon channel.
        if (NS_FAILED(rv) || !faviconService) return fallback();

        uint16_t preferredSize = UINT16_MAX;
        MOZ_ALWAYS_SUCCEEDS(faviconService->PreferredSizeFromURI(
            cachedFaviconURI, &preferredSize));
        nsCOMPtr<mozIStorageStatementCallback> callback =
            new faviconAsyncLoader(channel, listener, preferredSize);
        if (!callback) return fallback();

        rv = faviconService->GetFaviconDataAsync(faviconSpec, callback);
        if (NS_FAILED(rv)) return fallback();

        nsCOMPtr<nsICancelable> cancelable = do_QueryInterface(callback);
        return RequestOrCancelable(WrapNotNull(cancelable));
      });
  NS_ENSURE_TRUE(channel, NS_ERROR_OUT_OF_MEMORY);

  channel.forget(_channel);
  return NS_OK;
}
